// MagicPodsCore: https://github.com/steam3d/MagicPodsCore
// Copyright: 2020-2026 Aleksandr Maslov <https://magicpods.app> & Andrei Litvintsev <a.a.litvintsev@gmail.com>
// License: GPL-3.0

#pragma once

#include "sdk/sony/structs/SonyAncState.h"

#include <mutex>
#include <optional>

namespace MagicPodsCore
{
    // A user can drag the ANC slider during the V2 handshake (in particular
    // right after a plugin-driven Disconnect/Connect cycle, when the
    // handshake is restarting). If we forwarded the NcAsmSetParam to the
    // device while init was still in progress, the frame either landed
    // mid-handshake and got silently dropped, or arrived before the device
    // was ready to accept feature commands. Either way the click was lost
    // and the user had to retry. This little buffer holds the most recent
    // user request until init completes; if init is already done it lets
    // the request through unchanged. Thread-safe so it's fine to call from
    // the websocket message handler on one thread and from the V2 reading
    // thread on another.
    class SonyDeferredAncSet
    {
    public:
        // Called when the user issues a SetParam.
        // - If `initComplete` is true: returns the state to dispatch
        //   immediately and clears any previously buffered state.
        // - If `initComplete` is false: stores the state (overwriting any
        //   earlier buffered request - we only need the most recent one)
        //   and returns nullopt. The caller must call Flush() once init
        //   reaches Complete to retrieve and dispatch it.
        std::optional<SonyAncState> Submit(const SonyAncState &state, bool initComplete);

        // Called when the V2 init transitions to Complete. Returns the
        // most recently buffered state, if any, and clears the buffer.
        std::optional<SonyAncState> Flush();

    private:
        mutable std::mutex _mutex;
        std::optional<SonyAncState> _pending;
    };
}
