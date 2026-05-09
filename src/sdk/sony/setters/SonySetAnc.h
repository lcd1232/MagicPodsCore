// MagicPodsCore: https://github.com/steam3d/MagicPodsCore
// Copyright: 2020-2026 Aleksandr Maslov <https://magicpods.app> & Andrei Litvintsev <a.a.litvintsev@gmail.com>
// License: GPL-3.0

#pragma once

#include "sdk/sony/enums/SonyMsgIds.h"
#include "sdk/sony/structs/SonyAncState.h"

#include <vector>

namespace MagicPodsCore
{
    class SonySetAnc
    {
    public:
        const SonyMsgType Type = SonyMsgType::Command;
        const std::vector<unsigned char> Payload;

        explicit SonySetAnc(const SonyAncState &state)
            : Payload{
                static_cast<unsigned char>(SonyMsgIds::AncSet),
                0x02,
                static_cast<unsigned char>(state.AncSwitch),
                0x02,
                static_cast<unsigned char>(state.AncFilter),
                0x01,
                static_cast<unsigned char>(state.AmbientVoice),
                state.Volume,
              }
        {
        }
    };

    class SonyGetAncRequest
    {
    public:
        const SonyMsgType Type = SonyMsgType::Command;
        const std::vector<unsigned char> Payload{
            static_cast<unsigned char>(SonyMsgIds::AncGet),
            0x02,
        };
    };

    class SonyGetBatteryRequest
    {
    public:
        const SonyMsgType Type = SonyMsgType::Command;
        const std::vector<unsigned char> Payload;

        explicit SonyGetBatteryRequest(SonyBatterySubcommand which)
            : Payload{
                static_cast<unsigned char>(SonyMsgIds::BatteryGet),
                static_cast<unsigned char>(which),
              }
        {
        }
    };
}
