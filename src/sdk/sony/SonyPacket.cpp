// MagicPodsCore: https://github.com/steam3d/MagicPodsCore
// Copyright: 2020-2026 Aleksandr Maslov <https://magicpods.app> & Andrei Litvintsev <a.a.litvintsev@gmail.com>
// License: GPL-3.0

#include "SonyPacket.h"

namespace MagicPodsCore
{
    namespace
    {
        constexpr unsigned char kEscapeSentry = 0x3d;

        // Sony escapes any 0x3c / 0x3d / 0x3e inside the framed bytes so they
        // can't be mistaken for the frame's start or end markers. The escape
        // byte itself (0x3d) also gets escaped.
        std::vector<unsigned char> Escape(const std::vector<unsigned char> &raw)
        {
            std::vector<unsigned char> out{};
            out.reserve(raw.size());
            for (auto b : raw)
            {
                switch (b)
                {
                case 0x3c: out.push_back(kEscapeSentry); out.push_back(0x2c); break;
                case 0x3d: out.push_back(kEscapeSentry); out.push_back(0x2d); break;
                case 0x3e: out.push_back(kEscapeSentry); out.push_back(0x2e); break;
                default: out.push_back(b); break;
                }
            }
            return out;
        }
    }

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
        body.push_back(crc);

        const auto escaped = Escape(body);

        std::vector<unsigned char> frame{};
        frame.reserve(escaped.size() + 2);
        frame.push_back(StartByte);
        frame.insert(frame.end(), escaped.begin(), escaped.end());
        frame.push_back(EndByte);
        return frame;
    }

    std::optional<SonyResponseData> SonyPacket::Extract(const std::vector<unsigned char> &bytes) const
    {
        // Minimum unescaped length: type + prefix + size32 + crc = 7 bytes,
        // wrapped in start/end markers = 9 bytes. Anything shorter is junk.
        if (bytes.size() < 9)
            return std::nullopt;
        if (bytes.front() != StartByte || bytes.back() != EndByte)
            return std::nullopt;

        // Unescape everything between the markers.
        std::vector<unsigned char> body{};
        body.reserve(bytes.size() - 2);
        for (size_t i = 1; i < bytes.size() - 1; ++i)
        {
            unsigned char b = bytes[i];
            if (b == kEscapeSentry)
            {
                if (++i >= bytes.size() - 1)
                    return std::nullopt;
                switch (bytes[i])
                {
                case 0x2c: body.push_back(0x3c); break;
                case 0x2d: body.push_back(0x3d); break;
                case 0x2e: body.push_back(0x3e); break;
                default: return std::nullopt;
                }
            }
            else
            {
                body.push_back(b);
            }
        }

        if (body.size() < 7)
            return std::nullopt;

        unsigned char receivedCrc = body.back();
        body.pop_back();

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
