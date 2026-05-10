// MagicPodsCore: https://github.com/steam3d/MagicPodsCore
// Copyright: 2020-2026 Aleksandr Maslov <https://magicpods.app> & Andrei Litvintsev <a.a.litvintsev@gmail.com>
// License: GPL-3.0

#include "SonyBatteryWatcher.h"

#include "Logger.h"
#include "device/enums/DeviceBatteryStatus.h"
#include "device/enums/DeviceBatteryType.h"

namespace MagicPodsCore
{
    void SonyBatteryWatcher::ProcessResponse(const SonyResponseData &data)
    {
        if (data.Type != SonyDataType::DataMdr)
            return;
        if (data.Cmd != SonyT1Command::PowerRetStatus && data.Cmd != SonyT1Command::PowerNtfyStatus)
            return;

        // Body layout for the over-ear "BATTERY" reply (10 bytes total):
        //   [0]=type [1]=seq [2..5]=size [6]=cmd [7]=inquiredType
        //   [8]=batteryLevel(0-100) [9]=chargingStatus
        if (data.Body.size() < 10)
            return;
        if (static_cast<SonyPowerInquiredType>(data.Body[7]) != SonyPowerInquiredType::Battery)
            return;

        const unsigned char level = data.Body[8];
        const auto chargingStatus = static_cast<SonyBatteryChargingStatus>(data.Body[9]);
        const bool charging = chargingStatus == SonyBatteryChargingStatus::Charging;

        Logger::Debug("Sony battery: level=%d charging=%d", static_cast<int>(level), static_cast<int>(chargingStatus));

        std::vector<DeviceBatteryData> batteries{};
        batteries.emplace_back(
            DeviceBatteryType::Single,
            DeviceBatteryStatus::Connected,
            static_cast<short>(level),
            charging);

        _batteryChanged.FireEvent(batteries);
    }
}
