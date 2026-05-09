// MagicPodsCore: https://github.com/steam3d/MagicPodsCore
// Copyright: 2020-2026 Aleksandr Maslov <https://magicpods.app> & Andrei Litvintsev <a.a.litvintsev@gmail.com>
// License: GPL-3.0

#pragma once

#include "sdk/sony/enums/SonyAnc.h"

namespace MagicPodsCore
{
    struct SonyAncState
    {
        SonyAncSwitch AncSwitch{SonyAncSwitch::Off};
        SonyAncFilter AncFilter{SonyAncFilter::Anc};
        SonyAncFilterAmbientVoice AmbientVoice{SonyAncFilterAmbientVoice::Off};
        unsigned char Volume{0};
    };
}
