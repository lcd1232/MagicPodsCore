// MagicPodsCore: https://github.com/steam3d/MagicPodsCore
// Copyright: 2020-2026 Aleksandr Maslov <https://magicpods.app> & Andrei Litvintsev <a.a.litvintsev@gmail.com>
// License: GPL-3.0

#pragma once

#include "Device.h"
#include "Event.h"
#include "sdk/sony/SonyPacket.h"
#include "sdk/sony/enums/SonyMsgIds.h"
#include "sdk/sony/enums/SonyModelIds.h"
#include "sdk/sony/structs/SonyAncState.h"
#include "sdk/sony/structs/SonyResponseData.h"
#include "settings/SettingsService.h"

namespace MagicPodsCore
{
    // Where we are in the V2 init handshake. The device won't reliably answer
    // feature queries (battery, ANC) until we've walked the connect/get-info
    // chain end-to-end and finished with LOG_SET_STATUS.
    enum class SonyInitStep : unsigned char
    {
        NotStarted,
        AwaitingProtocolInfo,
        AwaitingCapabilityInfo,
        AwaitingDeviceInfoFw,
        AwaitingDeviceInfoModel,
        AwaitingDeviceInfoSeries,
        AwaitingSupportFunction,
        AwaitingLogSetStatusAck,
        Complete,
    };

    class SonyDevice : public Device
    {
    private:
        unsigned short _customProductId = 0;
        SonyPacket _packet{};
        Event<SonyResponseData> _responseDataReceived{};

        // Shared seq toggle. The protocol uses a single 1-bit counter for both
        // directions: every received non-ACK frame's seq becomes our outgoing
        // seq for the next non-ACK send, and we reply ACK with `1 - seq`.
        unsigned char _seq{0};
        SonyInitStep _initStep{SonyInitStep::NotStarted};

        void OnResponseDataReceived(const std::vector<unsigned char> &data) override;
        void DriveInitStateMachine(const SonyResponseData &frame);
        void OnConnectedChanged(bool isConnected);

        void SendCommand(const std::vector<unsigned char> &payload);
        void SendAck(unsigned char receivedSeq);

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

        Event<SonyResponseData> &GetResponseDataReceived() { return _responseDataReceived; }

        // Used by SonyAncCapability to push a NCASM_SET_PARAM frame to the device.
        void SendNcAsmSetParam(const SonyAncState &state);

        static std::unique_ptr<SonyDevice> Create(std::shared_ptr<DBusDeviceInfo> deviceInfo,
                                                  std::shared_ptr<PulseAudioClient> audioClient,
                                                  std::shared_ptr<SettingsService> settingsService,
                                                  unsigned short model);
    };
}
