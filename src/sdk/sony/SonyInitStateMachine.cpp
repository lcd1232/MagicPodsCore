// MagicPodsCore: https://github.com/steam3d/MagicPodsCore
// Copyright: 2020-2026 Aleksandr Maslov <https://magicpods.app> & Andrei Litvintsev <a.a.litvintsev@gmail.com>
// License: GPL-3.0

#include "SonyInitStateMachine.h"

#include "sdk/sony/enums/SonyAnc.h"
#include "sdk/sony/enums/SonyMsgIds.h"
#include "sdk/sony/setters/SonySetAnc.h"

namespace MagicPodsCore
{
    namespace
    {
        // The "inquired type" byte sits at body[7] for command frames whose
        // payload uses the [cmd][type][...] convention (PROTOCOL_INFO,
        // CAPABILITY_INFO, DEVICE_INFO, SUPPORT_FUNCTION, POWER_*, NCASM_*).
        bool BodyHasInquiredType(const SonyResponseData &frame)
        {
            return frame.Body.size() >= 8;
        }

        unsigned char InquiredType(const SonyResponseData &frame)
        {
            return frame.Body[7];
        }
    }

    SonyInitTransition ComputeSonyInitTransition(SonyInitStep current, const SonyResponseData &frame)
    {
        SonyInitTransition trans{current, {}};

        if (current == SonyInitStep::Complete)
            return trans;
        if (frame.Type != SonyDataType::DataMdr)
            return trans;

        switch (current)
        {
        case SonyInitStep::NotStarted:
            // Caller is responsible for kicking off ConnectGetProtocolInfo
            // (it happens when the RFCOMM client is up, not on a received
            // frame). Stay put until our caller advances us.
            break;

        case SonyInitStep::AwaitingProtocolInfo:
            if (frame.Cmd != SonyT1Command::ConnectRetProtocolInfo)
                break;
            trans.newStep = SonyInitStep::AwaitingCapabilityInfo;
            trans.commandsToSend.push_back(SonyConnectGetCapabilityInfo::Build());
            break;

        case SonyInitStep::AwaitingCapabilityInfo:
            if (frame.Cmd != SonyT1Command::ConnectRetCapabilityInfo)
                break;
            trans.newStep = SonyInitStep::AwaitingDeviceInfoFw;
            trans.commandsToSend.push_back(SonyConnectGetDeviceInfo::Build(SonyDeviceInfoType::FwVersion));
            break;

        case SonyInitStep::AwaitingDeviceInfoFw:
            if (frame.Cmd != SonyT1Command::ConnectRetDeviceInfo)
                break;
            if (!BodyHasInquiredType(frame) || InquiredType(frame) != static_cast<unsigned char>(SonyDeviceInfoType::FwVersion))
                break;
            trans.newStep = SonyInitStep::AwaitingDeviceInfoModel;
            trans.commandsToSend.push_back(SonyConnectGetDeviceInfo::Build(SonyDeviceInfoType::ModelName));
            break;

        case SonyInitStep::AwaitingDeviceInfoModel:
            if (frame.Cmd != SonyT1Command::ConnectRetDeviceInfo)
                break;
            if (!BodyHasInquiredType(frame) || InquiredType(frame) != static_cast<unsigned char>(SonyDeviceInfoType::ModelName))
                break;
            trans.newStep = SonyInitStep::AwaitingDeviceInfoSeries;
            trans.commandsToSend.push_back(SonyConnectGetDeviceInfo::Build(SonyDeviceInfoType::SeriesAndColorInfo));
            break;

        case SonyInitStep::AwaitingDeviceInfoSeries:
            if (frame.Cmd != SonyT1Command::ConnectRetDeviceInfo)
                break;
            if (!BodyHasInquiredType(frame) || InquiredType(frame) != static_cast<unsigned char>(SonyDeviceInfoType::SeriesAndColorInfo))
                break;
            trans.newStep = SonyInitStep::AwaitingSupportFunction;
            trans.commandsToSend.push_back(SonyConnectGetSupportFunction::Build());
            break;

        case SonyInitStep::AwaitingSupportFunction:
            if (frame.Cmd != SonyT1Command::ConnectRetSupportFunction)
                break;
            trans.newStep = SonyInitStep::Complete;
            trans.commandsToSend.push_back(SonyLogSetStatus::Build());
            trans.commandsToSend.push_back(SonyPowerGetStatus::Build(SonyPowerInquiredType::Battery));
            trans.commandsToSend.push_back(SonyNcAsmGetParam::Build(SonyNcAsmInquiredType::ModeNcAsmDualNcModeSwitchAndAsmSeamlessNa));
            break;

        case SonyInitStep::Complete:
            break;
        }

        return trans;
    }
}
