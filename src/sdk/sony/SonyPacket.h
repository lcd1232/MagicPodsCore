// MagicPodsCore: https://github.com/steam3d/MagicPodsCore
// Copyright: 2020-2026 Aleksandr Maslov <https://magicpods.app> & Andrei Litvintsev <a.a.litvintsev@gmail.com>
// License: GPL-3.0

#pragma once

#include "sdk/sony/enums/SonyMsgIds.h"
#include "sdk/sony/structs/SonyResponseData.h"

#include <optional>
#include <vector>

namespace MagicPodsCore
{
    // Sony "MDR_v2" frame:
    //   [0x3e] [type] [seq] [size_be32] [payload...] [crc] [0x3c]
    //   crc = (sum of body bytes) mod 256, where body = type|seq|size|payload
    //   any 0x3c/0x3d/0x3e inside body or crc is escaped: 0x3d 0x{2c,2d,2e}
    class SonyPacket
    {
    public:
        static constexpr unsigned char StartByte = 0x3e;
        static constexpr unsigned char EndByte = 0x3c;

        std::vector<unsigned char> Encode(SonyDataType type, unsigned char seq, const std::vector<unsigned char> &payload) const;

        std::optional<SonyResponseData> Extract(const std::vector<unsigned char> &bytes) const;
    };
}
