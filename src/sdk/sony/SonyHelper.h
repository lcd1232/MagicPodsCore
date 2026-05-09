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
        // Sony "DATA_MDR_v2" Bluetooth SPP service UUID (Sony Headphones Connect protocol).
        inline static const std::string SONY_SPP_UUID = "96cc203e-5068-46ad-b32d-e316f5e069ba";

        static bool IsSonyDevice(unsigned short vendorId, unsigned short productId);
        static std::string GetServiceGuid(SonyModelIds model);
    };
}
