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

            DriveInitStateMachine(f);

            _responseDataReceived.FireEvent(f);
        }
    }

    void SonyDevice::DriveInitStateMachine(const SonyResponseData &frame)
    {
        const auto previousStep = _initStep;
        const auto trans = ComputeSonyInitTransition(previousStep, frame);

        _initStep = trans.newStep;
        for (const auto &payload : trans.commandsToSend)
            SendCommand(payload);

        if (previousStep != trans.newStep)
        {
            switch (trans.newStep)
            {
            case SonyInitStep::AwaitingCapabilityInfo:
                Logger::Info("Sony init: got ProtocolInfo");
                break;
            case SonyInitStep::AwaitingDeviceInfoFw:
                Logger::Info("Sony init: got CapabilityInfo");
                break;
            case SonyInitStep::AwaitingDeviceInfoModel:
                Logger::Info("Sony init: got DeviceInfo(FW)");
                break;
            case SonyInitStep::AwaitingDeviceInfoSeries:
                Logger::Info("Sony init: got DeviceInfo(Model)");
                break;
            case SonyInitStep::AwaitingSupportFunction:
                Logger::Info("Sony init: got DeviceInfo(Series)");
                break;
            case SonyInitStep::Complete:
                Logger::Info("Sony init: got SupportFunction. Sent LogSetStatus + initial battery/ANC state.");

                // The first PowerGetStatus reply occasionally goes missing
                // (the XM6 is not always ready to answer the moment we
                // send the initial query). Schedule a couple of follow-up
                // battery queries so the battery is visible promptly even
                // when the very first one went unanswered. Detached; self-
                // exits if the client was torn down in the meantime.
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
    }

    void SonyDevice::OnConnectedChanged(bool isConnected)
    {
        if (!isConnected)
        {
            // Reset so the next reconnect re-runs the handshake.
            _outboundSeq = 0;
            _initStep = SonyInitStep::NotStarted;
            return;
        }

        StartHandshakeWhenClientReady();
    }

    void SonyDevice::StartHandshakeWhenClientReady()
    {
        // Whichever event triggers the handshake (Device::Init synchronously
        // starting the client at construction, vs Device's BlueZ
        // connected-status handler firing _onConnectedPropertyChangedEvent
        // *before* it calls _client->Start() on a reconnect), we want the
        // handshake to fire as soon as the RFCOMM socket is actually up. If
        // it's already up, kick off immediately; otherwise poll briefly on a
        // detached thread so a reconnect doesn't leave the V2 init missing.
        if (_client && _client->IsStarted())
        {
            if (_initStep == SonyInitStep::NotStarted)
            {
                Logger::Info("Sony init: starting handshake");
                SendCommand(SonyConnectGetProtocolInfo::Build());
                _initStep = SonyInitStep::AwaitingProtocolInfo;
            }
            return;
        }

        std::thread([this]() {
            for (int i = 0; i < 20; ++i)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
                if (!_client)
                    return;
                if (!_client->IsStarted())
                    continue;
                if (_initStep != SonyInitStep::NotStarted)
                    return;
                Logger::Info("Sony init: starting handshake (after %dms wait)", (i + 1) * 500);
                SendCommand(SonyConnectGetProtocolInfo::Build());
                _initStep = SonyInitStep::AwaitingProtocolInfo;
                return;
            }
            Logger::Warn("Sony init: client never started after ~10s, giving up handshake");
        }).detach();
    }

    void SonyDevice::SendCommand(const std::vector<unsigned char> &payload)
    {
        const unsigned char seq = _outboundSeq;
        _outboundSeq ^= 1;

        const auto frame = _packet.Encode(SonyDataType::DataMdr, seq, payload);
        Logger::Info("Sony TX type=0c seq=%u payload=%s",
                     static_cast<unsigned int>(seq),
                     BodyHex(payload).c_str());
        _client->SendData(frame);
    }

    void SonyDevice::SendAck(unsigned char receivedSeq)
    {
        // ACKs are addressed to the frame we just received; their seq is the
        // bitwise complement of the inbound seq and they don't disturb our
        // outbound counter.
        const unsigned char ackSeq = 1 - receivedSeq;
        Logger::Info("Sony TX type=01 seq=%u (ack)", static_cast<unsigned int>(ackSeq));
        _client->SendData(_packet.Encode(SonyDataType::Ack, ackSeq, {}));
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
