// MagicPodsCore: https://github.com/steam3d/MagicPodsCore
// Copyright: 2020-2026 Aleksandr Maslov <https://magicpods.app> & Andrei Litvintsev <a.a.litvintsev@gmail.com>
// License: GPL-3.0

#include "SonyDevice.h"

#include "Logger.h"
#include "capabilities/cmn/CmnBluetoothCodecCapability.h"
#include "capabilities/sony/SonyAncCapability.h"
#include "capabilities/sony/SonyBatteryCapability.h"
#include "sdk/sony/SonyHelper.h"
#include "sdk/sony/enums/SonyAnc.h"
#include "sdk/sony/setters/SonySetAnc.h"

#include <chrono>
#include <thread>

namespace MagicPodsCore
{
    namespace
    {
        std::string BodyHex(const std::vector<unsigned char> &body)
        {
            static const char *const hexDigits = "0123456789abcdef";
            std::string s;
            s.reserve(body.size() * 2);
            for (auto b : body)
            {
                s.push_back(hexDigits[(b >> 4) & 0xf]);
                s.push_back(hexDigits[b & 0xf]);
            }
            return s;
        }
    }

    void SonyDevice::OnResponseDataReceived(const std::vector<unsigned char> &data)
    {
        // The RFCOMM read can hand us any number of frames concatenated in a
        // single recv() buffer. Walk start (0x3E) -> end (0x3C) boundaries
        // and extract each frame independently. Plain 0x3C / 0x3E never appear
        // inside a properly-formed frame body (the framing layer escapes them
        // to 0x3D 0x{2C,2E}), so a naive scan for the markers is safe.
        size_t i = 0;
        while (i < data.size())
        {
            while (i < data.size() && data[i] != SonyPacket::StartByte)
                ++i;
            if (i >= data.size())
                break;

            size_t end = i + 1;
            while (end < data.size() && data[end] != SonyPacket::EndByte)
                ++end;
            if (end >= data.size())
                break;

            std::vector<unsigned char> frame(data.begin() + i, data.begin() + end + 1);
            i = end + 1;

            auto parsed = _packet.Extract(frame);
            if (!parsed.has_value())
            {
                Logger::Warn("Sony RX: dropped %zu-byte frame (parse failed)", frame.size());
                continue;
            }

            const auto &f = parsed.value();
            Logger::Info("Sony RX type=%02x seq=%u cmd=%02x body=%s",
                         static_cast<unsigned int>(f.Type),
                         static_cast<unsigned int>(f.Seq),
                         static_cast<unsigned int>(f.Cmd),
                         BodyHex(f.Body).c_str());

            if (f.Type == SonyDataType::Ack)
                continue;

            // Every incoming command/notify must be ACKed within ~3 s or the
            // device will retransmit and eventually drop the channel.
            SendAck(f.Seq);
            _seq = f.Seq;

            DriveInitStateMachine(f);

            _responseDataReceived.FireEvent(f);
        }
    }

    void SonyDevice::DriveInitStateMachine(const SonyResponseData &frame)
    {
        if (_initStep == SonyInitStep::Complete)
            return;
        if (frame.Type != SonyDataType::DataMdr)
            return;

        // Expected response cmd byte for each init step.
        const auto cmd = frame.Cmd;
        switch (_initStep)
        {
        case SonyInitStep::AwaitingProtocolInfo:
            if (cmd != SonyT1Command::ConnectRetProtocolInfo)
                return;
            Logger::Info("Sony init: got ProtocolInfo");
            SendCommand(SonyConnectGetCapabilityInfo::Build());
            _initStep = SonyInitStep::AwaitingCapabilityInfo;
            break;

        case SonyInitStep::AwaitingCapabilityInfo:
            if (cmd != SonyT1Command::ConnectRetCapabilityInfo)
                return;
            Logger::Info("Sony init: got CapabilityInfo");
            SendCommand(SonyConnectGetDeviceInfo::Build(SonyDeviceInfoType::FwVersion));
            _initStep = SonyInitStep::AwaitingDeviceInfoFw;
            break;

        case SonyInitStep::AwaitingDeviceInfoFw:
            if (cmd != SonyT1Command::ConnectRetDeviceInfo)
                return;
            Logger::Info("Sony init: got DeviceInfo(FW)");
            SendCommand(SonyConnectGetDeviceInfo::Build(SonyDeviceInfoType::ModelName));
            _initStep = SonyInitStep::AwaitingDeviceInfoModel;
            break;

        case SonyInitStep::AwaitingDeviceInfoModel:
            if (cmd != SonyT1Command::ConnectRetDeviceInfo)
                return;
            Logger::Info("Sony init: got DeviceInfo(Model)");
            SendCommand(SonyConnectGetDeviceInfo::Build(SonyDeviceInfoType::SeriesAndColorInfo));
            _initStep = SonyInitStep::AwaitingDeviceInfoSeries;
            break;

        case SonyInitStep::AwaitingDeviceInfoSeries:
            if (cmd != SonyT1Command::ConnectRetDeviceInfo)
                return;
            Logger::Info("Sony init: got DeviceInfo(Series)");
            SendCommand(SonyConnectGetSupportFunction::Build());
            _initStep = SonyInitStep::AwaitingSupportFunction;
            break;

        case SonyInitStep::AwaitingSupportFunction:
            if (cmd != SonyT1Command::ConnectRetSupportFunction)
                return;
            Logger::Info("Sony init: got SupportFunction. Sending LogSetStatus.");
            SendCommand(SonyLogSetStatus::Build());
            _initStep = SonyInitStep::AwaitingLogSetStatusAck;

            // LogSetStatus has no data response - the device only ACKs it.
            // Fire the initial feature queries right away; the device queues
            // them and will answer once it processes our log-set.
            Logger::Info("Sony init: requesting initial battery + ANC state");
            SendCommand(SonyPowerGetStatus::Build(SonyPowerInquiredType::Battery));
            SendCommand(SonyNcAsmGetParam::Build(SonyNcAsmInquiredType::ModeNcAsmDualNcModeSwitchAndAsmSeamlessNa));
            _initStep = SonyInitStep::Complete;

            // The first PowerGetStatus reply sometimes goes missing (the XM6
            // occasionally isn't ready to answer the moment LogSetStatus is
            // ACKed). Schedule a couple of follow-up battery queries so the
            // battery is visible promptly even when the very first one went
            // unanswered. Detached and self-stops as soon as the client is
            // torn down or the device was already updated.
            std::thread([this]() {
                for (auto delay : {std::chrono::seconds(3), std::chrono::seconds(7)})
                {
                    std::this_thread::sleep_for(delay);
                    if (!_client || !_client->IsStarted())
                        return;
                    SendCommand(SonyPowerGetStatus::Build(SonyPowerInquiredType::Battery));
                }
            }).detach();
            break;

        default:
            break;
        }
    }

    void SonyDevice::OnConnectedChanged(bool isConnected)
    {
        if (!isConnected)
        {
            // Reset so the next reconnect re-runs the handshake.
            _seq = 0;
            _initStep = SonyInitStep::NotStarted;
            return;
        }

        // Device::Init's handler is also subscribed to this event and is
        // responsible for calling _client->Start() (which is what opens the
        // RFCOMM socket and spawns the read/write threads). Since handlers
        // fire in subscription order, our caller in Create() has to make sure
        // we subscribe *after* Init() so we run *after* Start. Even so, Start
        // can fail (BlueZ SDP misses, channel busy, etc.) - in that case
        // sending into the dead client would just leak frames into a queue
        // nothing reads, so bail.
        if (!_client || !_client->IsStarted())
        {
            Logger::Warn("Sony init: client not started, skipping handshake");
            return;
        }

        if (_initStep == SonyInitStep::NotStarted)
        {
            Logger::Info("Sony init: starting handshake");
            SendCommand(SonyConnectGetProtocolInfo::Build());
            _initStep = SonyInitStep::AwaitingProtocolInfo;
        }
    }

    void SonyDevice::SendCommand(const std::vector<unsigned char> &payload)
    {
        _client->SendData(_packet.Encode(SonyDataType::DataMdr, _seq, payload));
    }

    void SonyDevice::SendAck(unsigned char receivedSeq)
    {
        _client->SendData(_packet.Encode(SonyDataType::Ack, 1 - receivedSeq, {}));
    }

    void SonyDevice::SendNcAsmSetParam(const SonyAncState &state)
    {
        SendCommand(SonyNcAsmSetParam::Build(state));
    }

    SonyDevice::SonyDevice(std::shared_ptr<DBusDeviceInfo> deviceInfo,
                           std::shared_ptr<PulseAudioClient> audioClient,
                           std::shared_ptr<SettingsService> settingsService,
                           unsigned short model)
        : Device(deviceInfo, audioClient, settingsService),
          _customProductId(model) {}

    std::unique_ptr<SonyDevice> SonyDevice::Create(std::shared_ptr<DBusDeviceInfo> deviceInfo,
                                                   std::shared_ptr<PulseAudioClient> audioClient,
                                                   std::shared_ptr<SettingsService> settingsService,
                                                   unsigned short model)
    {
        auto device = std::make_unique<SonyDevice>(deviceInfo, audioClient, settingsService, model);

        device->capabilities.push_back(std::make_unique<CmnBluetoothCodecCapability>(*device));
        device->capabilities.push_back(std::make_unique<SonyBatteryCapability>(*device));
        device->capabilities.push_back(std::make_unique<SonyAncCapability>(*device));

        device->_client = Client::CreateRFCOMM(deviceInfo->GetAddress(), SonyHelper::GetServiceGuid(static_cast<SonyModelIds>(model)));

        // Init() must run *before* we subscribe our connected-changed handler.
        // Init's own subscription is what calls _client->Start() to open the
        // RFCOMM socket; our handler then runs after it (event listeners fire
        // in subscription order) and pushes the first handshake frame onto an
        // already-open client.
        device->Init();

        auto *raw = device.get();
        device->GetConnectedPropertyChangedEvent().Subscribe([raw](size_t, bool isConnected)
                                                             { raw->OnConnectedChanged(isConnected); });

        // If we were already connected at construction, Init started the
        // client synchronously above; kick off the handshake.
        if (device->GetConnected())
            device->OnConnectedChanged(true);

        return device;
    }
}
