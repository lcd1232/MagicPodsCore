// MagicPodsCore: https://github.com/steam3d/MagicPodsCore
// Copyright: 2020-2026 Aleksandr Maslov <https://magicpods.app> & Andrei Litvintsev <a.a.litvintsev@gmail.com>
// License: GPL-3.0

#pragma once

#include "sdk/sony/enums/SonyModelIds.h"

#include <string>

namespace MagicPodsCore
{
    class SonyHelper
    {
    public:
        // Sony's "MDR_v2 over RFCOMM" service UUID, used by every XM5-and-newer
        // headphone (including the WH-1000XM6). Earlier XM3/XM4 firmware used
        // a different UUID (96cc203e-...) on a different protocol revision and
        // is not currently supported here. Confirmed against
        // mos9527/SonyHeadphonesClient libmdr/include/mdr-c/Base.h.
        inline static const std::string MDR_V2_SPP_UUID = "956c7b26-d49a-4ba8-b03f-b17d393cb6e2";

        static bool IsSonyDevice(unsigned short vendorId, unsigned short productId);
        static std::string GetServiceGuid(SonyModelIds model);
    };
}
