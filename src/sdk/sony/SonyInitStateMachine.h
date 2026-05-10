// MagicPodsCore: https://github.com/steam3d/MagicPodsCore
// Copyright: 2020-2026 Aleksandr Maslov <https://magicpods.app> & Andrei Litvintsev <a.a.litvintsev@gmail.com>
// License: GPL-3.0

#pragma once

#include "sdk/sony/structs/SonyResponseData.h"

#include <vector>

namespace MagicPodsCore
{
    // Where we are in the V2 init handshake.
    //
    // Defined here (not in SonyDevice.h) so the transition function below is
    // testable as a pure function without bringing in the whole SonyDevice
    // dependency tree.
    enum class SonyInitStep : unsigned char
    {
        NotStarted,
        AwaitingProtocolInfo,
        AwaitingCapabilityInfo,
        AwaitingDeviceInfoFw,
        AwaitingDeviceInfoModel,
        AwaitingDeviceInfoSeries,
        AwaitingSupportFunction,
        Complete,
    };

    struct SonyInitTransition
    {
        SonyInitStep newStep;
        // Each entry is the *payload* (the bytes after the [type][seq][size]
        // header, before the checksum) of a DATA_MDR command to dispatch.
        // Empty if no commands need to go out for this transition.
        std::vector<std::vector<unsigned char>> commandsToSend;
    };

    // Pure decision function for the V2 init handshake. Given the step we're
    // currently in and the frame we just received from the device, decide
    // (a) whether to advance the step and (b) which command payload(s) to
    // send next. The state machine validates the inquired-type subbyte for
    // every reply that has one (PROTOCOL_INFO, CAPABILITY_INFO, DEVICE_INFO,
    // SUPPORT_FUNCTION), so a retransmit of an earlier reply doesn't cause
    // a false advance through the chain.
    SonyInitTransition ComputeSonyInitTransition(SonyInitStep currentStep, const SonyResponseData &frame);
}
