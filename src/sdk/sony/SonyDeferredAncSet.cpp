// MagicPodsCore: https://github.com/steam3d/MagicPodsCore
// Copyright: 2020-2026 Aleksandr Maslov <https://magicpods.app> & Andrei Litvintsev <a.a.litvintsev@gmail.com>
// License: GPL-3.0

#include "SonyDeferredAncSet.h"

namespace MagicPodsCore
{
    std::optional<SonyAncState> SonyDeferredAncSet::Submit(const SonyAncState &state, bool initComplete)
    {
        std::lock_guard lock(_mutex);
        if (initComplete)
        {
            // Init is done; the request can ride straight through. Drop any
            // earlier buffered request - the user's most-recent intent
            // (this one) is the only one we care about.
            _pending.reset();
            return state;
        }
        _pending = state;
        return std::nullopt;
    }

    std::optional<SonyAncState> SonyDeferredAncSet::Flush()
    {
        std::lock_guard lock(_mutex);
        std::optional<SonyAncState> out;
        std::swap(out, _pending);
        return out;
    }
}
