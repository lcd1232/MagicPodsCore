// MagicPodsCore: https://github.com/steam3d/MagicPodsCore
// Copyright: 2020-2026 Aleksandr Maslov <https://magicpods.app> & Andrei Litvintsev <a.a.litvintsev@gmail.com>
// License: GPL-3.0

#include "SonyAncCapability.h"

#include "Logger.h"

namespace MagicPodsCore
{
    DeviceAncModes SonyAncCapability::SonyStateToDeviceAncMode(const SonyAncState &state)
    {
        if (state.MasterEnabled == SonyNcAsmOnOff::Off)
            return DeviceAncModes::Off;

        switch (state.Mode)
        {
        case SonyNcAsmMode::Nc:
            return DeviceAncModes::NoiseCancellation;
        case SonyNcAsmMode::Asm:
            return DeviceAncModes::Transparency;
        }
        return DeviceAncModes::Off;
    }

    SonyAncState SonyAncCapability::DeviceAncModeToSonyState(DeviceAncModes mode, const SonyAncState &previous)
    {
        SonyAncState next = previous;
        next.ChangeStatus = SonyValueChangeStatus::Changed;

        switch (mode)
        {
        case DeviceAncModes::Off:
            next.MasterEnabled = SonyNcAsmOnOff::Off;
            break;
        case DeviceAncModes::NoiseCancellation:
            next.MasterEnabled = SonyNcAsmOnOff::On;
            next.Mode = SonyNcAsmMode::Nc;
            break;
        case DeviceAncModes::Transparency:
            next.MasterEnabled = SonyNcAsmOnOff::On;
            next.Mode = SonyNcAsmMode::Asm;
            break;
        default:
            // Adaptive / WindCancellation aren't represented in this NA payload;
            // fall back to Off rather than send a malformed state.
            next.MasterEnabled = SonyNcAsmOnOff::Off;
            break;
        }
        return next;
    }

    nlohmann::json SonyAncCapability::CreateJsonBody()
    {
        // The NA payload only directly maps to NoiseCancellation / Transparency
        // (plus Off). Don't advertise modes the headphone won't actually accept.
        unsigned char optionsFlag =
            static_cast<unsigned char>(DeviceAncModes::Off) |
            static_cast<unsigned char>(DeviceAncModes::NoiseCancellation) |
            static_cast<unsigned char>(DeviceAncModes::Transparency);

        auto bodyJson = nlohmann::json::object();
        bodyJson["selected"] = option;
        bodyJson["options"] = optionsFlag;
        return bodyJson;
    }

    void SonyAncCapability::OnReceivedData(const SonyResponseData &data)
    {
        watcher.ProcessResponse(data);
    }

    SonyAncCapability::SonyAncCapability(SonyDevice &device)
        : SonyCapability("anc", false, device),
          watcher(SonyAncWatcher(static_cast<SonyModelIds>(device.GetProductId())))
    {
        // Mark the capability available from the start with a default Off
        // state. The WH-1000XM6 doesn't reply to the initial NcAsmGetParam at
        // the end of the V2 handshake; it only emits NcAsmNtfyParam frames
        // when state actually changes (or in response to a SetParam). Without
        // this, the ANC controls would stay hidden in the UI until the user
        // toggled the headphones' physical button - and worse, some
        // frontends snapshot capabilities at first GetAll and never refresh
        // them from later broadcasts. Once a real notify arrives the option
        // updates to match the device.
        isAvailable = true;

        watcherAncChangedEventId = watcher.GetAncChangedEvent().Subscribe([this](size_t, const SonyAncState &state) {
            lastState = state;
            DeviceAncModes newOption = SonyStateToDeviceAncMode(state);
            if (option != newOption)
            {
                option = newOption;
                Logger::Info("ANC updated: %s", DeviceAncModesToString(option).c_str());
                _onChanged.FireEvent(*this);
            }
        });
    }

    SonyAncCapability::~SonyAncCapability()
    {
        watcher.GetAncChangedEvent().Unsubscribe(watcherAncChangedEventId);
    }

    void SonyAncCapability::SetFromJson(const nlohmann::json &json)
    {
        if (!json.contains(name))
            return;

        const auto &capability = json.at(name);

        if (!capability.contains("selected") || !capability["selected"].is_number_integer())
        {
            Logger::Info("Error: SonyAncCapability::SetFromJson got no value or value is not an integer");
            return;
        }

        unsigned char selected = static_cast<unsigned char>(capability["selected"].get<int>());
        if (!isValidDeviceAncModesType(selected))
        {
            Logger::Info("Error: SonyAncCapability::SetFromJson got unexpected option: %d", selected);
            return;
        }

        DeviceAncModes mode = static_cast<DeviceAncModes>(selected);
        SonyAncState nextState = DeviceAncModeToSonyState(mode, lastState);
        device.SendNcAsmSetParam(nextState);
        Logger::Debug("SonyAncCapability::SetFromJson sent NcAsmSetParam for %s", DeviceAncModesToString(mode).c_str());
    }
}
