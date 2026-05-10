// MagicPodsCore: https://github.com/steam3d/MagicPodsCore
// Copyright: 2020-2026 Aleksandr Maslov <https://magicpods.app> & Andrei Litvintsev <a.a.litvintsev@gmail.com>
// License: GPL-3.0

#pragma once

#include "Event.h"
#include "sdk/sony/enums/SonyModelIds.h"
#include "sdk/sony/structs/SonyAncState.h"
#include "sdk/sony/structs/SonyResponseData.h"

namespace MagicPodsCore
{
    class SonyAncWatcher
    {
    private:
        SonyModelIds model;
        Event<SonyAncState> _ancChanged{};

    public:
        explicit SonyAncWatcher(SonyModelIds model) : model(model) {}

        Event<SonyAncState> &GetAncChangedEvent() { return _ancChanged; }

        void ProcessResponse(const SonyResponseData &data);

        static bool IsSupported(SonyModelIds model);
    };
}
