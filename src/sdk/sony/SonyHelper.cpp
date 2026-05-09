// MagicPodsCore: https://github.com/steam3d/MagicPodsCore
// Copyright: 2020-2026 Aleksandr Maslov <https://magicpods.app> & Andrei Litvintsev <a.a.litvintsev@gmail.com>
// License: GPL-3.0

#include "SonyHelper.h"

#include "BtVendorIds.h"

namespace MagicPodsCore
{
    bool SonyHelper::IsSonyDevice(unsigned short vendorId, unsigned short productId)
    {
        if (vendorId != static_cast<unsigned short>(BtVendorIds::Sony))
            return false;

        for (auto &sonyProductId : AllSonyModelIds)
        {
            if (productId == static_cast<unsigned short>(sonyProductId))
                return true;
        }
        return false;
    }

    std::string SonyHelper::GetServiceGuid(SonyModelIds /*model*/)
    {
        return SONY_SPP_UUID;
    }
}
