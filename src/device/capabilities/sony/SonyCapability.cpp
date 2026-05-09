// MagicPodsCore: https://github.com/steam3d/MagicPodsCore
// Copyright: 2020-2026 Aleksandr Maslov <https://magicpods.app> & Andrei Litvintsev <a.a.litvintsev@gmail.com>
// License: GPL-3.0

#include "SonyCapability.h"

namespace MagicPodsCore
{
    void SonyCapability::SendData(const SonySetAnc &setter)
    {
        device.SendData(setter);
    }

    void SonyCapability::SendData(const SonyGetAncRequest &request)
    {
        device.SendData(request);
    }

    void SonyCapability::SendData(const SonyGetBatteryRequest &request)
    {
        device.SendData(request);
    }

    void SonyCapability::Reset()
    {
        Capability::Reset();
    }

    SonyCapability::SonyCapability(const std::string &name, bool isReadOnly, SonyDevice &device)
        : Capability(name, isReadOnly), device(device)
    {
        responseDataReceivedId = this->device.GetResponseDataReceived().Subscribe(
            [this](size_t id, const SonyResponseData &data) { OnReceivedData(data); });

        onConnectedPropertyChangedId = this->device.GetConnectedPropertyChangedEvent().Subscribe(
            [this](size_t id, bool isConnected) {
                if (!isConnected)
                    Reset();
            });
    }

    SonyCapability::~SonyCapability()
    {
        // Do not use the device here. All events will be released in ~Device.
    }
}
