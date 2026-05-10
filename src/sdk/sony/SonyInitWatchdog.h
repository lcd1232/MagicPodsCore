// MagicPodsCore: https://github.com/steam3d/MagicPodsCore
// Copyright: 2020-2026 Aleksandr Maslov <https://magicpods.app> & Andrei Litvintsev <a.a.litvintsev@gmail.com>
// License: GPL-3.0

#pragma once

#include "sdk/sony/SonyInitStateMachine.h"

#include <chrono>
#include <optional>
#include <vector>

namespace MagicPodsCore
{
    // The XM6 occasionally drops the very first GetProtocolInfo (or any other
    // mid-handshake query) when the headphones have just reconnected after
    // a Steam Deck reboot - the RFCOMM channel is up but the device's MDR
    // firmware isn't yet ready to talk. The state machine's
    // ComputeSonyInitTransition is event-driven and silent in that case, so
    // we layer a watchdog on top: if a step has been awaiting its reply for
    // longer than this threshold, hand the caller the payload it should
    // re-send. When `current` is NotStarted or Complete the watchdog is
    // intentionally quiet. Pure function; the SonyDevice runtime is
    // responsible for ticking it on a worker thread.
    std::optional<std::vector<unsigned char>> ComputeSonyInitWatchdogTick(
        SonyInitStep current,
        std::chrono::milliseconds sinceLastProgress);
}
