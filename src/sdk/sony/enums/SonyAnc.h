// MagicPodsCore: https://github.com/steam3d/MagicPodsCore
// Copyright: 2020-2026 Aleksandr Maslov <https://magicpods.app> & Andrei Litvintsev <a.a.litvintsev@gmail.com>
// License: GPL-3.0

#pragma once

namespace MagicPodsCore
{
    // The "inquired type" byte that goes immediately after NCASM_*_PARAM in a
    // V2 NC/ASM frame. Each value selects a different payload shape.
    // The WH-1000XM6 uses the "_NA" (noise-adaptation) variant exclusively.
    enum class SonyNcAsmInquiredType : unsigned char
    {
        ModeNcAsmDualNcModeSwitchAndAsmSeamlessNa = 0x19,
    };

    enum class SonyValueChangeStatus : unsigned char
    {
        UnderChanging = 0x00,
        Changed       = 0x01,
    };

    // Master on/off for both NC and ambient. 0 = nothing active.
    enum class SonyNcAsmOnOff : unsigned char
    {
        Off = 0x00,
        On  = 0x01,
    };

    // Which NC/ASM family is active when the master switch is on.
    enum class SonyNcAsmMode : unsigned char
    {
        Nc  = 0x00,  // noise cancellation
        Asm = 0x01,  // ambient sound mode (transparency)
    };

    enum class SonyAmbientSoundMode : unsigned char
    {
        Normal = 0x00,
        Voice  = 0x01,
    };

    enum class SonyNoiseAdaptiveSensitivity : unsigned char
    {
        Standard = 0x00,
        High     = 0x01,
        Low      = 0x02,
    };
}
