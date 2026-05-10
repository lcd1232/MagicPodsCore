// MagicPodsCore: https://github.com/steam3d/MagicPodsCore
// Copyright: 2020-2026 Aleksandr Maslov <https://magicpods.app> & Andrei Litvintsev <a.a.litvintsev@gmail.com>
// License: GPL-3.0

#pragma once

namespace MagicPodsCore
{
    // Frame "type" byte at body offset 0 of a Sony MDR_v2 frame, identifying
    // which dispatch table the device routes the frame to.
    enum class SonyDataType : unsigned char
    {
        Ack        = 0x01,
        DataMdr    = 0x0c,  // Table 1: battery, ANC, device info, etc.
        DataMdrNo2 = 0x0e,  // Table 2: rarer features (not used here)
    };

    // Table-1 command IDs at body offset 6 (the first byte of payload).
    // Values match Sony's official MDR_v2 command numbering as reverse
    // engineered in mos9527/SonyHeadphonesClient libmdr/include/mdr/ProtocolV2T1.hpp.
    enum class SonyT1Command : unsigned char
    {
        ConnectGetProtocolInfo    = 0x00,
        ConnectRetProtocolInfo    = 0x01,
        ConnectGetCapabilityInfo  = 0x02,
        ConnectRetCapabilityInfo  = 0x03,
        ConnectGetDeviceInfo      = 0x04,
        ConnectRetDeviceInfo      = 0x05,
        ConnectGetSupportFunction = 0x06,
        ConnectRetSupportFunction = 0x07,

        PowerGetStatus            = 0x22,
        PowerRetStatus            = 0x23,
        PowerSetStatus            = 0x24,
        PowerNtfyStatus           = 0x25,

        NcAsmGetParam             = 0x66,
        NcAsmRetParam             = 0x67,
        NcAsmSetParam             = 0x68,
        NcAsmNtfyParam            = 0x69,

        LogSetStatus              = 0xc4,
    };

    // Subcommand byte at offset 7 for Connect_*ProtocolInfo / CapabilityInfo /
    // SupportFunction. The protocol fixes it to 0x00.
    enum class SonyConnectInquiredType : unsigned char
    {
        Fixed = 0x00,
    };

    // Subcommand byte at offset 7 for ConnectGetDeviceInfo / ConnectRetDeviceInfo.
    enum class SonyDeviceInfoType : unsigned char
    {
        ModelName          = 0x01,
        FwVersion          = 0x02,
        SeriesAndColorInfo = 0x03,
    };

    // Subcommand byte at offset 7 for PowerGetStatus / PowerRetStatus / PowerNtfyStatus.
    enum class SonyPowerInquiredType : unsigned char
    {
        Battery          = 0x00,
        LeftRightBattery = 0x01,
        CradleBattery    = 0x02,
    };

    enum class SonyBatteryChargingStatus : unsigned char
    {
        NotCharging = 0x00,
        Charging    = 0x01,
        Unknown     = 0x02,
        Charged     = 0x03,
    };
}
