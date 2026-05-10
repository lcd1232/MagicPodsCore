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

namespace MagicPodsCore
{
    void SonyDevice::OnResponseDataReceived(const std::vector<unsigned char> &data)
    {
        auto parsed = _packet.Extract(data);
        if (!parsed.has_value())
            return;

        const auto &frame = parsed.value();

        // Device-to-host ACKs carry no payload and need no further action.
        if (frame.Type == SonyDataType::Ack)
            return;

        // Every incoming command/notify must be ACKed within ~3 s or the
        // device will retransmit and eventually drop the channel.
        SendAck(frame.Seq);
        _seq = frame.Seq;

        DriveInitStateMachine(frame);

        // Hand to capability watchers (battery, ANC, future ones).
        _responseDataReceived.FireEvent(frame);
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

        // The base Device::Init handler already started the RFCOMM client. Now
        // that the socket is up and the read/write threads are spinning, kick
        // off the V2 handshake.
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

        // Bridge the connected-property change into our state machine. The
        // base Device::Init also subscribes to this event to start/stop the
        // client; both subscribers fire on each transition.
        auto *raw = device.get();
        device->GetConnectedPropertyChangedEvent().Subscribe([raw](size_t, bool isConnected)
                                                             { raw->OnConnectedChanged(isConnected); });

        device->Init();

        // If we were already connected at construction, Init started the
        // client synchronously - kick off the handshake too.
        if (device->GetConnected())
            device->OnConnectedChanged(true);

        return device;
    }
}
