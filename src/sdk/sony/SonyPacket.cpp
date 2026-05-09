// MagicPodsCore: https://github.com/steam3d/MagicPodsCore
// Copyright: 2020-2026 Aleksandr Maslov <https://magicpods.app> & Andrei Litvintsev <a.a.litvintsev@gmail.com>
// License: GPL-3.0

#include "SonyPacket.h"

namespace MagicPodsCore
{
    std::vector<unsigned char> SonyPacket::Encode(SonyMsgType type, unsigned char prefix, const std::vector<unsigned char> &payload) const
    {
        const auto size = static_cast<unsigned int>(payload.size());

        std::vector<unsigned char> body{};
        body.reserve(7 + payload.size());
        body.push_back(static_cast<unsigned char>(type));
        body.push_back(prefix);
        body.push_back(static_cast<unsigned char>((size >> 24) & 0xff));
        body.push_back(static_cast<unsigned char>((size >> 16) & 0xff));
        body.push_back(static_cast<unsigned char>((size >> 8) & 0xff));
        body.push_back(static_cast<unsigned char>(size & 0xff));
        body.insert(body.end(), payload.begin(), payload.end());

        unsigned char crc = 0;
        for (auto b : body)
            crc += b;

        std::vector<unsigned char> frame{};
        frame.reserve(body.size() + 3);
        frame.push_back(StartByte);
        frame.insert(frame.end(), body.begin(), body.end());
        frame.push_back(crc);
        frame.push_back(EndByte);
        return frame;
    }

    std::optional<SonyResponseData> SonyPacket::Extract(const std::vector<unsigned char> &bytes) const
    {
        // Minimum: start + type + prefix + 4 size bytes + crc + end = 9 bytes
        if (bytes.size() < 9)
            return std::nullopt;
        if (bytes.front() != StartByte || bytes.back() != EndByte)
            return std::nullopt;

        std::vector<unsigned char> body(bytes.begin() + 1, bytes.end() - 2);
        unsigned char receivedCrc = bytes[bytes.size() - 2];

        unsigned char crc = 0;
        for (auto b : body)
            crc += b;
        if (crc != receivedCrc)
            return std::nullopt;

        auto type = static_cast<SonyMsgType>(body[0]);
        unsigned char prefix = body[1];

        // Command id sits at offset 6 of the body for command frames; ack frames carry no command.
        SonyMsgIds id = SonyMsgIds::Unknown;
        if (type == SonyMsgType::Command && body.size() >= 7)
            id = static_cast<SonyMsgIds>(body[6]);

        return SonyResponseData{type, prefix, id, body};
    }
}
