// MagicPodsCore: https://github.com/steam3d/MagicPodsCore
// Copyright: 2020-2026 Aleksandr Maslov <https://magicpods.app> & Andrei Litvintsev <a.a.litvintsev@gmail.com>
// License: GPL-3.0

#pragma once

#include "device/SonyDevice.h"
#include "device/capabilities/Capability.h"
#include "sdk/sony/setters/SonySetAnc.h"
#include "sdk/sony/structs/SonyResponseData.h"

namespace MagicPodsCore
{
    class SonyCapability : public Capability
    {
    private:
        size_t responseDataReceivedId;
        size_t onConnectedPropertyChangedId;

    protected:
        SonyDevice &device;
        virtual void OnReceivedData(const SonyResponseData &data) = 0;
        void SendData(const SonySetAnc &setter);
        void SendData(const SonyGetAncRequest &request);
        void SendData(const SonyGetBatteryRequest &request);
        void Reset() override;

    public:
        explicit SonyCapability(const std::string &name, bool isReadOnly, SonyDevice &device);
        ~SonyCapability() override;
    };
}
