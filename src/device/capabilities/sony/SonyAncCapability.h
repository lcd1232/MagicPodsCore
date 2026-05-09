// MagicPodsCore: https://github.com/steam3d/MagicPodsCore
// Copyright: 2020-2026 Aleksandr Maslov <https://magicpods.app> & Andrei Litvintsev <a.a.litvintsev@gmail.com>
// License: GPL-3.0

#pragma once

#include "SonyCapability.h"
#include "device/enums/DeviceAncModes.h"
#include "sdk/sony/structs/SonyAncState.h"
#include "sdk/sony/watchers/SonyAncWatcher.h"

namespace MagicPodsCore
{
    class SonyAncCapability : public SonyCapability
    {
    private:
        DeviceAncModes option{DeviceAncModes::Off};
        SonyAncState lastState{};
        SonyAncWatcher watcher;
        size_t watcherAncChangedEventId;

        static DeviceAncModes SonyStateToDeviceAncModes(const SonyAncState &state);
        static SonyAncState DeviceAncModesToSonyState(DeviceAncModes mode, const SonyAncState &previous);

    protected:
        nlohmann::json CreateJsonBody() override;
        void OnReceivedData(const SonyResponseData &data) override;

    public:
        explicit SonyAncCapability(SonyDevice &device);
        ~SonyAncCapability() override;
        void SetFromJson(const nlohmann::json &json) override;
    };
}
