// MagicPodsCore: https://github.com/steam3d/MagicPodsCore
// Copyright: 2020-2026 Aleksandr Maslov <https://magicpods.app> & Andrei Litvintsev <a.a.litvintsev@gmail.com>
// License: GPL-3.0

#pragma once

#include "SonyCapability.h"
#include "device/DeviceBattery.h"
#include "sdk/sony/watchers/SonyBatteryWatcher.h"

namespace MagicPodsCore
{
    class SonyBatteryCapability : public SonyCapability
    {
    private:
        DeviceBattery battery;
        size_t batteryChangedEventId;

        SonyBatteryWatcher watcher;
        size_t watcherBatteryChangedEventId;

    protected:
        nlohmann::json CreateJsonBody() override;
        void OnReceivedData(const SonyResponseData &data) override;
        void Reset() override;

    public:
        explicit SonyBatteryCapability(SonyDevice &device);
        ~SonyBatteryCapability() override;
        void SetFromJson(const nlohmann::json &json) override {};
    };
}
