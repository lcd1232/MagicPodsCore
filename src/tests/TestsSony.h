// MagicPodsCore: https://github.com/steam3d/MagicPodsCore
// Copyright: 2020-2026 Aleksandr Maslov <https://magicpods.app> & Andrei Litvintsev <a.a.litvintsev@gmail.com>
// License: GPL-3.0

#pragma once

namespace MagicPodsCore
{
    class TestsSony
    {
    private:
        // Frame layer
        bool TestPacketEncodeNoEscape();
        bool TestPacketEncodeWithEscape();
        bool TestPacketExtractNoEscape();
        bool TestPacketExtractWithEscape();
        bool TestPacketRoundTrip();
        bool TestPacketRejectsBadCrc();
        bool TestPacketRejectsTruncated();
        bool TestPacketAckRoundTrip();

        // Watchers
        bool TestBatteryWatcherSingle();
        bool TestBatteryWatcherIgnoresWrongCmd();
        bool TestBatteryWatcherIgnoresShortBody();
        bool TestAncWatcherTransparency();
        bool TestAncWatcherNoiseCancellation();
        bool TestAncWatcherOff();
        bool TestAncWatcherIgnoresWrongInquiredType();

        // Setters
        bool TestSetAncBuildsExpectedPayload();
        bool TestPowerGetStatusBuildsExpectedPayload();
        bool TestNcAsmGetParamBuildsExpectedPayload();

        // Helper
        bool TestIsSonyDeviceMatchesWh1000xm6();
        bool TestIsSonyDeviceRejectsOtherVendor();
        bool TestIsSonyDeviceRejectsUnknownProductId();

        // V2 init state machine (pure)
        bool TestInitMachineProtocolInfoToCapabilityInfo();
        bool TestInitMachineCapabilityInfoToDeviceInfoFw();
        bool TestInitMachineDeviceInfoFwToModel();
        bool TestInitMachineRejectsFwRetransmitInModelState();
        bool TestInitMachineRejectsModelRetransmitInSeriesState();
        bool TestInitMachineSeriesToSupportFunction();
        bool TestInitMachineSupportFunctionToCompleteSendsThreeCommands();
        bool TestInitMachineIgnoresUnrelatedFrameInProtocolInfoState();
        bool TestInitMachineCompleteIsTerminal();
        bool TestInitMachineIgnoresAckFrames();

    public:
        TestsSony();
    };
}
