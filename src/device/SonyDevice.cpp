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

        // No startup probes yet: the WH-1000XM6 speaks Sony's "MDR_v2" protocol,
        // which requires a multi-step capability-negotiation handshake before
        // it accepts arbitrary GET/SET commands. That handshake hasn't been
        // implemented here yet, so we just open the socket and observe what
        // the device sends. Battery / ANC will stay empty until the V2 init
        // path lands in a follow-up.

        device->Init();
        return device;
    }
}
