// MagicPodsCore: https://github.com/steam3d/MagicPodsCore
// Copyright: 2020-2026 Aleksandr Maslov <https://magicpods.app> & Andrei Litvintsev <a.a.litvintsev@gmail.com>
// License: GPL-3.0

#pragma once

#include "Device.h"
#include "Event.h"
#include "sdk/sony/SonyInitStateMachine.h"
#include "sdk/sony/SonyPacket.h"
#include "sdk/sony/enums/SonyMsgIds.h"
#include "sdk/sony/enums/SonyModelIds.h"
#include "sdk/sony/structs/SonyAncState.h"
#include "sdk/sony/structs/SonyResponseData.h"
#include "settings/SettingsService.h"

#include <atomic>
#include <chrono>

namespace MagicPodsCore
{
    class SonyDevice : public Device
    {
    private:
        unsigned short _customProductId = 0;
        SonyPacket _packet{};
        Event<SonyResponseData> _responseDataReceived{};

        // The protocol's seq carries two roles. For ACKs it's `1 - received`,
        // identifying the frame we're acknowledging. For our outbound non-ACK
        // commands we maintain our *own* 1-bit toggle and flip it after each
        // send: that's what the XM6 actually expects. Tying outbound seq to
        // the last received seq (as a literal reading of the upstream
        // mSeqNumber comment suggests) caused the device to silently drop
        // some commands as duplicates when an unrelated DataMdrNo2 push from
        // the device perturbed the shared counter mid-handshake.
        //
        // Atomic so the V2 init watchdog can call SendCommand from its own
        // worker thread without racing the reading thread's ACK / state-
        // machine sends.
        std::atomic<unsigned char> _outboundSeq{0};
        std::atomic<SonyInitStep> _initStep{SonyInitStep::NotStarted};

        // Nanoseconds since steady_clock epoch at the last *progress* event
        // (a state-machine transition or a watchdog re-send). The watchdog
        // checks (now - this) against its retry threshold to decide whether
        // the current step has been silent too long.
        std::atomic<int64_t> _lastInitProgressNs{0};
        std::atomic<bool> _watchdogActive{false};

        void OnResponseDataReceived(const std::vector<unsigned char> &data) override;
        void DriveInitStateMachine(const SonyResponseData &frame);
        void OnConnectedChanged(bool isConnected);
        void StartHandshakeWhenClientReady();
        void StartInitWatchdog();
        void RunInitWatchdog();
        void MarkInitProgress();
        std::chrono::milliseconds SinceLastInitProgress() const;

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
