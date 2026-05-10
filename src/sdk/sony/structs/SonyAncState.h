// MagicPodsCore: https://github.com/steam3d/MagicPodsCore
// Copyright: 2020-2026 Aleksandr Maslov <https://magicpods.app> & Andrei Litvintsev <a.a.litvintsev@gmail.com>
// License: GPL-3.0

#pragma once

#include "sdk/sony/enums/SonyAnc.h"

namespace MagicPodsCore
{
    // Mirror of the WH-1000XM6's NC/ASM "_NA" payload (NCASM_RET_PARAM /
    // NCASM_NTFY_PARAM with inquired-type = 0x19). 7 bytes after the cmd+type
    // header.
    struct SonyAncState
    {
        SonyValueChangeStatus       ChangeStatus{SonyValueChangeStatus::Changed};
        SonyNcAsmOnOff              MasterEnabled{SonyNcAsmOnOff::Off};
        SonyNcAsmMode               Mode{SonyNcAsmMode::Nc};
        SonyAmbientSoundMode        AmbientMode{SonyAmbientSoundMode::Normal};
        unsigned char               AmbientLevel{0};
        SonyNcAsmOnOff              NoiseAdaptiveEnabled{SonyNcAsmOnOff::Off};
        SonyNoiseAdaptiveSensitivity NoiseAdaptiveSensitivity{SonyNoiseAdaptiveSensitivity::Standard};
    };
}
