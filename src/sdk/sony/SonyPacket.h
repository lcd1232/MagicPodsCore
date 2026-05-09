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
    // Sony "DATA_MDR" frame:
    //   [0x3e] [type] [prefix] [size3] [size2] [size1] [size0] [payload...] [crc] [0x3c]
    //   crc = sum of body bytes (everything except the leading 0x3e, trailing 0x3c, and crc itself)
    //   prefix toggles 0/1 between successive client commands; the headphones echo it in their ack.
    class SonyPacket
    {
    public:
        static constexpr unsigned char StartByte = 0x3e;
        static constexpr unsigned char EndByte = 0x3c;

        std::vector<unsigned char> Encode(SonyMsgType type, unsigned char prefix, const std::vector<unsigned char> &payload) const;

        std::optional<SonyResponseData> Extract(const std::vector<unsigned char> &bytes) const;
    };
}
