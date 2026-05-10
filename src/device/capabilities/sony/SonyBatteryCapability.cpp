// MagicPodsCore: https://github.com/steam3d/MagicPodsCore
// Copyright: 2020-2026 Aleksandr Maslov <https://magicpods.app> & Andrei Litvintsev <a.a.litvintsev@gmail.com>
// License: GPL-3.0

#include "SonyBatteryCapability.h"

namespace MagicPodsCore
{
    nlohmann::json SonyBatteryCapability::CreateJsonBody()
    {
        return battery.CreateJsonBody();
    }

    void SonyBatteryCapability::OnReceivedData(const SonyResponseData &data)
    {
        watcher.ProcessResponse(data);
    }

    void SonyBatteryCapability::Reset()
    {
        // Keep the last known battery value visible across BlueZ
        // disconnect/reconnect cycles - dropping it forces the frontend
        // to remove the battery widget entirely until the next handshake
        // repopulates it, which has been confusing in practice. The next
        // PowerRetStatus arriving from the device will overwrite the
        // values with truth.
        Logger::Debug("SonyBatteryCapability::Reset (no-op for visibility)");
    }

    SonyBatteryCapability::SonyBatteryCapability(SonyDevice &device)
        : SonyCapability("battery", true, device),
          battery(true),
          watcher(SonyBatteryWatcher(static_cast<SonyModelIds>(device.GetProductId())))
    {
        batteryChangedEventId = battery.GetBatteryChangedEvent().Subscribe(
            [this](size_t id, const std::vector<DeviceBatteryData> &b) {
                if (!isAvailable)
                    isAvailable = true;
                _onChanged.FireEvent(*this);
                Logger::Debug("SonyBatteryCapability::GetBatteryChangedEvent");
            });

        watcherBatteryChangedEventId = watcher.GetBatteryChangedEvent().Subscribe(
            [this](size_t id, const std::vector<DeviceBatteryData> &b) { battery.UpdateBattery(b); });
    }

    SonyBatteryCapability::~SonyBatteryCapability()
    {
        battery.GetBatteryChangedEvent().Unsubscribe(batteryChangedEventId);
        watcher.GetBatteryChangedEvent().Unsubscribe(watcherBatteryChangedEventId);
    }
}
