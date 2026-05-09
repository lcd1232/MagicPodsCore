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
        if (data.Type != SonyMsgType::Command)
            return;
        if (data.Id != SonyMsgIds::BatteryRet)
            return;

        // Body layout for battery reply:
        //   [0]=type [1]=prefix [2..5]=size [6]=cmd(0x11) [7]=subcommand
        //   For headphones (subcommand=0x01): [8]=left [9]=left-charging [10]=right [11]=right-charging
        //   For case        (subcommand=0x02): [8]=case [9]=case-charging
        if (data.Body.size() < 9)
            return;

        const auto subcommand = static_cast<SonyBatterySubcommand>(data.Body[7]);

        std::vector<DeviceBatteryData> batteries{};

        if (subcommand == SonyBatterySubcommand::Headphones)
        {
            if (data.Body.size() < 12)
                return;

            unsigned char left = data.Body[8];
            bool leftCharging = data.Body[9] != 0;
            unsigned char right = data.Body[10];
            bool rightCharging = data.Body[11] != 0;

            // WH-* over-ear models report a single battery in the left slot; in-ear WF-* report both.
            if (right == 0 && !rightCharging)
            {
                batteries.emplace_back(
                    DeviceBatteryType::Single,
                    left == 0 ? DeviceBatteryStatus::Disconnected : DeviceBatteryStatus::Connected,
                    static_cast<short>(left),
                    leftCharging);
            }
            else
            {
                batteries.emplace_back(
                    DeviceBatteryType::Left,
                    left == 0 ? DeviceBatteryStatus::Disconnected : DeviceBatteryStatus::Connected,
                    static_cast<short>(left),
                    leftCharging);
                batteries.emplace_back(
                    DeviceBatteryType::Right,
                    right == 0 ? DeviceBatteryStatus::Disconnected : DeviceBatteryStatus::Connected,
                    static_cast<short>(right),
                    rightCharging);
            }
        }
        else if (subcommand == SonyBatterySubcommand::Case)
        {
            unsigned char caseLevel = data.Body[8];
            bool caseCharging = data.Body.size() > 9 ? data.Body[9] != 0 : false;
            batteries.emplace_back(
                DeviceBatteryType::Case,
                caseLevel == 0 ? DeviceBatteryStatus::Disconnected : DeviceBatteryStatus::Connected,
                static_cast<short>(caseLevel),
                caseCharging);
        }
        else
        {
            return;
        }

        Logger::Debug("Sony battery update (subcommand=%d, count=%zu)", static_cast<int>(subcommand), batteries.size());
        _batteryChanged.FireEvent(batteries);
    }
}
