// MagicPodsCore: https://github.com/steam3d/MagicPodsCore
// Copyright: 2020-2026 Aleksandr Maslov <https://magicpods.app> & Andrei Litvintsev <a.a.litvintsev@gmail.com>
// License: GPL-3.0

#pragma once

namespace MagicPodsCore
{
    // Type byte at offset 1 of the packet body.
    enum class SonyMsgType : unsigned char
    {
        Ack = 0x01,
        Command = 0x0c,
    };

    // Command byte at offset 6 of the packet body, paired with subcommand at offset 7.
    enum class SonyMsgIds : unsigned char
    {
        Unknown = 0x00,

        BatteryGet = 0x10,
        BatteryRet = 0x11,

        AncGet = 0x66,
        AncRet = 0x67,
        AncSet = 0x68,
        AncNotify = 0x69,
    };

    // Subcommand byte (offset 7) used with battery commands.
    enum class SonyBatterySubcommand : unsigned char
    {
        Headphones = 0x01,
        Case = 0x02,
    };
}
