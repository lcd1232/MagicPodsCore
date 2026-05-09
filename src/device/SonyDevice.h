// MagicPodsCore: https://github.com/steam3d/MagicPodsCore
// Copyright: 2020-2026 Aleksandr Maslov <https://magicpods.app> & Andrei Litvintsev <a.a.litvintsev@gmail.com>
// License: GPL-3.0

#pragma once

#include "Device.h"
#include "Event.h"
#include "sdk/sony/SonyPacket.h"
#include "sdk/sony/enums/SonyModelIds.h"
#include "sdk/sony/setters/SonySetAnc.h"
#include "sdk/sony/structs/SonyResponseData.h"
#include "settings/SettingsService.h"

namespace MagicPodsCore
{
    class SonyDevice : public Device
    {
    private:
        unsigned short _customProductId = 0;
        SonyPacket _packet{};
        Event<SonyResponseData> _responseDataReceived{};

        // Sony's command/ack handshake uses a 1-bit prefix that toggles between successive
        // client commands and is echoed back in acks. We bump it on every send.
        unsigned char _outgoingPrefix{1};

        void OnResponseDataReceived(const std::vector<unsigned char> &data) override;

    public:
        explicit SonyDevice(std::shared_ptr<DBusDeviceInfo> deviceInfo,
                            std::shared_ptr<PulseAudioClient> audioClient,
                            std::shared_ptr<SettingsService> settingsService,
                            unsigned short model);

        unsigned short GetProductId() const override
        {
            std::lock_guard lock{_propertyMutex};
            return _customProductId;
        }

        Event<SonyResponseData> &GetResponseDataReceived()
        {
            return _responseDataReceived;
        }

        void SendData(const SonySetAnc &setter);
        void SendData(const SonyGetAncRequest &request);
        void SendData(const SonyGetBatteryRequest &request);

        static std::unique_ptr<SonyDevice> Create(std::shared_ptr<DBusDeviceInfo> deviceInfo,
                                                  std::shared_ptr<PulseAudioClient> audioClient,
                                                  std::shared_ptr<SettingsService> settingsService,
                                                  unsigned short model);
    };
}
