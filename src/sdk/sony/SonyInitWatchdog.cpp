// MagicPodsCore: https://github.com/steam3d/MagicPodsCore
// Copyright: 2020-2026 Aleksandr Maslov <https://magicpods.app> & Andrei Litvintsev <a.a.litvintsev@gmail.com>
// License: GPL-3.0

#include "SonyInitWatchdog.h"

#include "sdk/sony/enums/SonyAnc.h"
#include "sdk/sony/enums/SonyMsgIds.h"
#include "sdk/sony/setters/SonySetAnc.h"

namespace MagicPodsCore
{
    namespace
    {
        // The XM6 has been observed to take a few seconds after RFCOMM
        // connect to be ready to talk MDR. Anything sooner than this and
        // we'd be retrying while the device is still mid-initialisation.
        constexpr auto kRetryThreshold = std::chrono::seconds(3);
    }

    std::optional<std::vector<unsigned char>> ComputeSonyInitWatchdogTick(
        SonyInitStep current,
        std::chrono::milliseconds sinceLastProgress)
    {
        if (sinceLastProgress < kRetryThreshold)
            return std::nullopt;

        switch (current)
        {
        case SonyInitStep::AwaitingProtocolInfo:
            return SonyConnectGetProtocolInfo::Build();
        case SonyInitStep::AwaitingCapabilityInfo:
            return SonyConnectGetCapabilityInfo::Build();
        case SonyInitStep::AwaitingDeviceInfoFw:
            return SonyConnectGetDeviceInfo::Build(SonyDeviceInfoType::FwVersion);
        case SonyInitStep::AwaitingDeviceInfoModel:
            return SonyConnectGetDeviceInfo::Build(SonyDeviceInfoType::ModelName);
        case SonyInitStep::AwaitingDeviceInfoSeries:
            return SonyConnectGetDeviceInfo::Build(SonyDeviceInfoType::SeriesAndColorInfo);
        case SonyInitStep::AwaitingSupportFunction:
            return SonyConnectGetSupportFunction::Build();
        case SonyInitStep::NotStarted:
        case SonyInitStep::Complete:
            return std::nullopt;
        }
        return std::nullopt;
    }
}
