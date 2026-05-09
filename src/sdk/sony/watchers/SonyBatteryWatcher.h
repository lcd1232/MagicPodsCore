// MagicPodsCore: https://github.com/steam3d/MagicPodsCore
// Copyright: 2020-2026 Aleksandr Maslov <https://magicpods.app> & Andrei Litvintsev <a.a.litvintsev@gmail.com>
// License: GPL-3.0

#pragma once

#include "Event.h"
#include "device/structs/DeviceBatteryData.h"
#include "sdk/sony/enums/SonyModelIds.h"
#include "sdk/sony/structs/SonyResponseData.h"

#include <vector>

namespace MagicPodsCore
{
    class SonyBatteryWatcher
    {
    private:
        SonyModelIds model;
        Event<std::vector<DeviceBatteryData>> _batteryChanged{};

    public:
        explicit SonyBatteryWatcher(SonyModelIds model) : model(model) {}

        Event<std::vector<DeviceBatteryData>> &GetBatteryChangedEvent()
        {
            return _batteryChanged;
        }

        void ProcessResponse(const SonyResponseData &data);
    };
}
