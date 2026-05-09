// MagicPodsCore: https://github.com/steam3d/MagicPodsCore
// Copyright: 2020-2026 Aleksandr Maslov <https://magicpods.app> & Andrei Litvintsev <a.a.litvintsev@gmail.com>
// License: GPL-3.0

#pragma once

namespace MagicPodsCore
{
    // Modalias: bluetooth:v054c{ProductId}
    enum class SonyModelIds : unsigned short
    {
        Unknown = 0x0000,
        Wh1000xm6 = 0x0f8a,
    };

    static const SonyModelIds AllSonyModelIds[] = {
        SonyModelIds::Wh1000xm6,
    };
}
