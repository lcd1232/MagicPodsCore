// MagicPodsCore: https://github.com/steam3d/MagicPodsCore
// Copyright: 2020-2026 Aleksandr Maslov <https://magicpods.app> & Andrei Litvintsev <a.a.litvintsev@gmail.com>
// License: GPL-3.0

#pragma once

#include "sdk/sony/enums/SonyAnc.h"
#include "sdk/sony/enums/SonyMsgIds.h"
#include "sdk/sony/structs/SonyAncState.h"

#include <vector>

namespace MagicPodsCore
{
    // Builders for the V2 Table-1 command payloads (everything after the
    // [type][seq][size_be32] header, before the checksum). Each Build()
    // returns the payload bytes; SonyDevice wraps them with the frame layer.

    struct SonyConnectGetProtocolInfo
    {
        static std::vector<unsigned char> Build()
        {
            return {
                static_cast<unsigned char>(SonyT1Command::ConnectGetProtocolInfo),
                static_cast<unsigned char>(SonyConnectInquiredType::Fixed),
            };
        }
    };

    struct SonyConnectGetCapabilityInfo
    {
        static std::vector<unsigned char> Build()
        {
            return {
                static_cast<unsigned char>(SonyT1Command::ConnectGetCapabilityInfo),
                static_cast<unsigned char>(SonyConnectInquiredType::Fixed),
            };
        }
    };

    struct SonyConnectGetDeviceInfo
    {
        static std::vector<unsigned char> Build(SonyDeviceInfoType which)
        {
            return {
                static_cast<unsigned char>(SonyT1Command::ConnectGetDeviceInfo),
                static_cast<unsigned char>(which),
            };
        }
    };

    struct SonyConnectGetSupportFunction
    {
        static std::vector<unsigned char> Build()
        {
            return {
                static_cast<unsigned char>(SonyT1Command::ConnectGetSupportFunction),
                static_cast<unsigned char>(SonyConnectInquiredType::Fixed),
            };
        }
    };

    struct SonyLogSetStatus
    {
        // Final init step in the upstream reference; payload is fixed.
        static std::vector<unsigned char> Build()
        {
            return {
                static_cast<unsigned char>(SonyT1Command::LogSetStatus),
                0x01,
                0x00,
            };
        }
    };

    struct SonyPowerGetStatus
    {
        static std::vector<unsigned char> Build(SonyPowerInquiredType which)
        {
            return {
                static_cast<unsigned char>(SonyT1Command::PowerGetStatus),
                static_cast<unsigned char>(which),
            };
        }
    };

    struct SonyNcAsmGetParam
    {
        static std::vector<unsigned char> Build(SonyNcAsmInquiredType which)
        {
            return {
                static_cast<unsigned char>(SonyT1Command::NcAsmGetParam),
                static_cast<unsigned char>(which),
            };
        }
    };

    struct SonyNcAsmSetParam
    {
        // Payload for the WH-1000XM6 "MODE_NC_ASM_DUAL_NC_MODE_SWITCH_AND_ASM_SEAMLESS_NA"
        // variant: 9 bytes total ([cmd][type][changeStatus][masterOnOff][mode]
        // [ambientMode][ambientLevel][autoAsmOnOff][adaptiveSensitivity]).
        static std::vector<unsigned char> Build(const SonyAncState &state)
        {
            return {
                static_cast<unsigned char>(SonyT1Command::NcAsmSetParam),
                static_cast<unsigned char>(SonyNcAsmInquiredType::ModeNcAsmDualNcModeSwitchAndAsmSeamlessNa),
                static_cast<unsigned char>(state.ChangeStatus),
                static_cast<unsigned char>(state.MasterEnabled),
                static_cast<unsigned char>(state.Mode),
                static_cast<unsigned char>(state.AmbientMode),
                state.AmbientLevel,
                static_cast<unsigned char>(state.NoiseAdaptiveEnabled),
                static_cast<unsigned char>(state.NoiseAdaptiveSensitivity),
            };
        }
    };
}
