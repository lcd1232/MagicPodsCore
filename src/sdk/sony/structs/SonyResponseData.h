// MagicPodsCore: https://github.com/steam3d/MagicPodsCore
// Copyright: 2020-2026 Aleksandr Maslov <https://magicpods.app> & Andrei Litvintsev <a.a.litvintsev@gmail.com>
// License: GPL-3.0

#pragma once

#include "sdk/sony/enums/SonyMsgIds.h"
#include <vector>

namespace MagicPodsCore
{
    // Decoded Sony packet: start (0x3e) and end (0x3c) bytes are stripped, CRC is validated.
    // Body is everything between them; Id is the command byte at offset 6 of the body.
    struct SonyResponseData
    {
        SonyMsgType Type;
        unsigned char Prefix;
        SonyMsgIds Id;
        std::vector<unsigned char> Body;

        SonyResponseData(SonyMsgType type, unsigned char prefix, SonyMsgIds id, const std::vector<unsigned char> &body)
            : Type(type), Prefix(prefix), Id(id), Body(body) {}
    };
}
