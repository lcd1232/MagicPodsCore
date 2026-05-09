// MagicPodsCore: https://github.com/steam3d/MagicPodsCore
// Copyright: 2020-2026 Aleksandr Maslov <https://magicpods.app> & Andrei Litvintsev <a.a.litvintsev@gmail.com>
// License: GPL-3.0

#include "SonyDevice.h"

#include "capabilities/cmn/CmnBluetoothCodecCapability.h"
#include "capabilities/sony/SonyAncCapability.h"
#include "capabilities/sony/SonyBatteryCapability.h"
#include "sdk/sony/SonyHelper.h"
#include "sdk/sony/enums/SonyMsgIds.h"
#include "sdk/sony/setters/SonySetAnc.h"

namespace MagicPodsCore
{
    void SonyDevice::OnResponseDataReceived(const std::vector<unsigned char> &data)
    {
        auto parsed = _packet.Extract(data);
        if (parsed.has_value())
            _responseDataReceived.FireEvent(parsed.value());
    }

    SonyDevice::SonyDevice(std::shared_ptr<DBusDeviceInfo> deviceInfo,
                           std::shared_ptr<PulseAudioClient> audioClient,
                           std::shared_ptr<SettingsService> settingsService,
                           unsigned short model)
        : Device(deviceInfo, audioClient, settingsService),
          _customProductId(model) {}

    void SonyDevice::SendData(const SonySetAnc &setter)
    {
        _client->SendData(_packet.Encode(setter.Type, _outgoingPrefix, setter.Payload));
        _outgoingPrefix ^= 1;
    }

    void SonyDevice::SendData(const SonyGetAncRequest &request)
    {
        _client->SendData(_packet.Encode(request.Type, _outgoingPrefix, request.Payload));
        _outgoingPrefix ^= 1;
    }

    void SonyDevice::SendData(const SonyGetBatteryRequest &request)
    {
        _client->SendData(_packet.Encode(request.Type, _outgoingPrefix, request.Payload));
        _outgoingPrefix ^= 1;
    }

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

        // After connect, kick off polling the device for its current battery and ANC state.
        // The SonyDevice will toggle the prefix automatically on each send.
        device->_clientStartData.push_back(device->_packet.Encode(SonyMsgType::Command, 0,
                                                                  SonyGetBatteryRequest(SonyBatterySubcommand::Headphones).Payload));
        device->_clientStartData.push_back(device->_packet.Encode(SonyMsgType::Command, 1,
                                                                  SonyGetAncRequest{}.Payload));

        device->Init();
        return device;
    }
}
