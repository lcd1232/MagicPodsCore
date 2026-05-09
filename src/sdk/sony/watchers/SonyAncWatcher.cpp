// MagicPodsCore: https://github.com/steam3d/MagicPodsCore
// Copyright: 2020-2026 Aleksandr Maslov <https://magicpods.app> & Andrei Litvintsev <a.a.litvintsev@gmail.com>
// License: GPL-3.0

#include "SonyAncWatcher.h"

#include "Logger.h"

namespace MagicPodsCore
{
    void SonyAncWatcher::ProcessResponse(const SonyResponseData &data)
    {
        if (data.Type != SonyMsgType::Command)
            return;

        // Both the explicit get reply (AncRet) and the unsolicited push (AncNotify) carry the same body shape.
        if (data.Id != SonyMsgIds::AncRet && data.Id != SonyMsgIds::AncNotify)
            return;

        // Body layout (offsets relative to start of body, i.e. without the 0x3e prefix):
        //   [0]=type [1]=prefix [2..5]=size [6]=cmd [7]=0x02 [8]=switch [9]=0x02
        //   [10]=filter [11]=0x01 [12]=ambient-voice [13]=volume
        if (data.Body.size() < 14)
            return;

        SonyAncState state{};
        state.AncSwitch = data.Body[8] == 0 ? SonyAncSwitch::Off : SonyAncSwitch::On;
        state.AncFilter = static_cast<SonyAncFilter>(data.Body[10]);
        state.AmbientVoice = static_cast<SonyAncFilterAmbientVoice>(data.Body[12]);
        state.Volume = data.Body[13];

        Logger::Debug("Sony ANC: switch=%d filter=%d voice=%d volume=%d",
                      static_cast<int>(state.AncSwitch),
                      static_cast<int>(state.AncFilter),
                      static_cast<int>(state.AmbientVoice),
                      state.Volume);

        _ancChanged.FireEvent(state);
    }

    bool SonyAncWatcher::IsSupport(SonyModelIds model)
    {
        switch (model)
        {
        case SonyModelIds::Wh1000xm6:
            return true;
        default:
            return false;
        }
    }

    std::vector<SonyAncFilter> SonyAncWatcher::GetAncFiltersFor(SonyModelIds model)
    {
        if (IsSupport(model))
            return {SonyAncFilter::Anc, SonyAncFilter::Ambient, SonyAncFilter::Wind};
        return {};
    }
}
