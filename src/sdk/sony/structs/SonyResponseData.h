// MagicPodsCore: https://github.com/steam3d/MagicPodsCore
// Copyright: 2020-2026 Aleksandr Maslov <https://magicpods.app> & Andrei Litvintsev <a.a.litvintsev@gmail.com>
// License: GPL-3.0

#pragma once

#include "sdk/sony/enums/SonyMsgIds.h"
#include <vector>

namespace MagicPodsCore
{
    // A successfully framed Sony MDR_v2 message. Start (0x3E) and end (0x3C)
    // markers are stripped, byte-stuffing is undone, and the checksum has
    // been validated. `Body` holds the unescaped bytes between the markers
    // *minus* the trailing checksum: that is, [type][seq][size_be32][payload].
    // `Cmd` is body[6] when the frame carries Table-1 data; for ACKs it has no
    // meaning (ACK frames have no payload).
    struct SonyResponseData
    {
        SonyDataType  Type;
        unsigned char Seq;
        SonyT1Command Cmd;
        std::vector<unsigned char> Body;

        SonyResponseData(SonyDataType type, unsigned char seq, SonyT1Command cmd, const std::vector<unsigned char> &body)
            : Type(type), Seq(seq), Cmd(cmd), Body(body) {}
    };
}
