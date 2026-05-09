// MagicPodsCore: https://github.com/steam3d/MagicPodsCore
// Copyright: 2020-2026 Aleksandr Maslov <https://magicpods.app> & Andrei Litvintsev <a.a.litvintsev@gmail.com>
// License: GPL-3.0

#include "SonyAncCapability.h"

namespace MagicPodsCore
{
    DeviceAncModes SonyAncCapability::SonyStateToDeviceAncModes(const SonyAncState &state)
    {
        if (state.AncSwitch == SonyAncSwitch::Off)
            return DeviceAncModes::Off;

        switch (state.AncFilter)
        {
        case SonyAncFilter::Anc:
            return DeviceAncModes::NoiseCancellation;
        case SonyAncFilter::Wind:
            return DeviceAncModes::WindCancellation;
        case SonyAncFilter::Ambient:
            return DeviceAncModes::Transparency;
        default:
            return DeviceAncModes::Off;
        }
    }

    SonyAncState SonyAncCapability::DeviceAncModesToSonyState(DeviceAncModes mode, const SonyAncState &previous)
    {
        SonyAncState next = previous;
        switch (mode)
        {
        case DeviceAncModes::Off:
            next.AncSwitch = SonyAncSwitch::Off;
            break;
        case DeviceAncModes::NoiseCancellation:
            next.AncSwitch = SonyAncSwitch::On;
            next.AncFilter = SonyAncFilter::Anc;
            break;
        case DeviceAncModes::WindCancellation:
            next.AncSwitch = SonyAncSwitch::On;
            next.AncFilter = SonyAncFilter::Wind;
            break;
        case DeviceAncModes::Transparency:
            next.AncSwitch = SonyAncSwitch::On;
            next.AncFilter = SonyAncFilter::Ambient;
            break;
        default:
            next.AncSwitch = SonyAncSwitch::Off;
            break;
        }
        return next;
    }

    nlohmann::json SonyAncCapability::CreateJsonBody()
    {
        const auto filters = SonyAncWatcher::GetAncFiltersFor(static_cast<SonyModelIds>(device.GetProductId()));

        unsigned char optionsFlag = static_cast<unsigned char>(DeviceAncModes::Off);
        for (auto filter : filters)
        {
            switch (filter)
            {
            case SonyAncFilter::Anc:
                optionsFlag |= static_cast<unsigned char>(DeviceAncModes::NoiseCancellation);
                break;
            case SonyAncFilter::Wind:
                optionsFlag |= static_cast<unsigned char>(DeviceAncModes::WindCancellation);
                break;
            case SonyAncFilter::Ambient:
                optionsFlag |= static_cast<unsigned char>(DeviceAncModes::Transparency);
                break;
            }
        }

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
        watcherAncChangedEventId = watcher.GetAncChangedEvent().Subscribe([this](size_t id, const SonyAncState &state) {
            lastState = state;
            DeviceAncModes newOption = SonyStateToDeviceAncModes(state);
            if (!isAvailable)
            {
                isAvailable = true;
                option = newOption;
                Logger::Info("ANC updated: %s", DeviceAncModesToString(option).c_str());
                _onChanged.FireEvent(*this);
            }
            else if (option != newOption)
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
        SonyAncState nextState = DeviceAncModesToSonyState(mode, lastState);
        SendData(SonySetAnc(nextState));
        Logger::Debug("SonyAncCapability::SetFromJson set option to %s", DeviceAncModesToString(mode).c_str());
    }
}
