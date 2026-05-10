// MagicPodsCore: https://github.com/steam3d/MagicPodsCore
// Copyright: 2020-2026 Aleksandr Maslov <https://magicpods.app> & Andrei Litvintsev <a.a.litvintsev@gmail.com>
// License: GPL-3.0

#include "SonyAncWatcher.h"

#include "Logger.h"
#include "sdk/sony/enums/SonyAnc.h"

namespace MagicPodsCore
{
    void SonyAncWatcher::ProcessResponse(const SonyResponseData &data)
    {
        if (data.Type != SonyDataType::DataMdr)
            return;
        if (data.Cmd != SonyT1Command::NcAsmRetParam && data.Cmd != SonyT1Command::NcAsmNtfyParam)
            return;

        // Body layout for the WH-1000XM6 NC/ASM "_NA" payload (15 bytes total):
        //   [0]=type [1]=seq [2..5]=size [6]=cmd [7]=inquiredType
        //   [8]=changeStatus [9]=masterOnOff [10]=ncAsmMode [11]=ambientMode
        //   [12]=ambientLevel [13]=autoAsmOnOff [14]=adaptiveSensitivity
        if (data.Body.size() < 15)
            return;
        if (static_cast<SonyNcAsmInquiredType>(data.Body[7]) != SonyNcAsmInquiredType::ModeNcAsmDualNcModeSwitchAndAsmSeamlessNa)
            return;

        SonyAncState state{};
        state.ChangeStatus            = static_cast<SonyValueChangeStatus>(data.Body[8]);
        state.MasterEnabled           = static_cast<SonyNcAsmOnOff>(data.Body[9]);
        state.Mode                    = static_cast<SonyNcAsmMode>(data.Body[10]);
        state.AmbientMode             = static_cast<SonyAmbientSoundMode>(data.Body[11]);
        state.AmbientLevel            = data.Body[12];
        state.NoiseAdaptiveEnabled    = static_cast<SonyNcAsmOnOff>(data.Body[13]);
        state.NoiseAdaptiveSensitivity = static_cast<SonyNoiseAdaptiveSensitivity>(data.Body[14]);

        Logger::Debug("Sony ANC: master=%d mode=%d ambient=%d level=%d autoAsm=%d sens=%d",
                      static_cast<int>(state.MasterEnabled),
                      static_cast<int>(state.Mode),
                      static_cast<int>(state.AmbientMode),
                      state.AmbientLevel,
                      static_cast<int>(state.NoiseAdaptiveEnabled),
                      static_cast<int>(state.NoiseAdaptiveSensitivity));

        _ancChanged.FireEvent(state);
    }

    bool SonyAncWatcher::IsSupported(SonyModelIds model)
    {
        return model == SonyModelIds::Wh1000xm6;
    }
}
