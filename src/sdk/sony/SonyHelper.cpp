// MagicPodsCore: https://github.com/steam3d/MagicPodsCore
// Copyright: 2020-2026 Aleksandr Maslov <https://magicpods.app> & Andrei Litvintsev <a.a.litvintsev@gmail.com>
// License: GPL-3.0

#include "SonyHelper.h"

#include "BtVendorIds.h"
#include "StringUtils.h"

#include <algorithm>

namespace MagicPodsCore
{
    bool SonyHelper::IsSonyDevice(unsigned short vendorId, const std::vector<std::string> &uuids)
    {
        if (vendorId != static_cast<unsigned short>(BtVendorIds::Sony))
            return false;

        std::vector<std::string> lowered;
        lowered.reserve(uuids.size());
        std::transform(uuids.begin(), uuids.end(), std::back_inserter(lowered), [](const std::string &g)
                       { return StringUtils::ToLowerCase(g); });

        return std::find(lowered.begin(), lowered.end(), SONY_SPP_UUID) != lowered.end();
    }

    SonyModelIds SonyHelper::GetModelFromName(const std::string &name)
    {
        if (name.empty())
            return SonyModelIds::Unknown;

        std::string n = StringUtils::ToLowerCase(name);

        if (n.find("wh-1000xm6") != std::string::npos)
            return SonyModelIds::Wh1000xm6;
        if (n.find("wh-1000xm5") != std::string::npos)
            return SonyModelIds::Wh1000xm5;
        if (n.find("wh-1000xm4") != std::string::npos)
            return SonyModelIds::Wh1000xm4;

        return SonyModelIds::Unknown;
    }

    std::string SonyHelper::GetServiceGuid(SonyModelIds /*model*/)
    {
        return SONY_SPP_UUID;
    }
}
