// MagicPodsCore: https://github.com/steam3d/MagicPodsCore
// Copyright: 2020-2026 Aleksandr Maslov <https://magicpods.app> & Andrei Litvintsev <a.a.litvintsev@gmail.com>
// License: GPL-3.0

#pragma once

namespace MagicPodsCore
{
    enum class SonyAncSwitch : unsigned char
    {
        Off = 0x00,
        On = 0x01,
    };

    enum class SonyAncFilter : unsigned char
    {
        Ambient = 0x00,
        Wind = 0x01,
        Anc = 0x02,
    };

    enum class SonyAncFilterAmbientVoice : unsigned char
    {
        Off = 0x00,
        On = 0x01,
    };
}
