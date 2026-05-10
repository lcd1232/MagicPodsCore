// MagicPodsCore: https://github.com/steam3d/MagicPodsCore
// Copyright: 2020-2026 Aleksandr Maslov <https://magicpods.app> & Andrei Litvintsev <a.a.litvintsev@gmail.com>
// License: GPL-3.0

#include "SonyDevice.h"

#include "Logger.h"
#include "capabilities/cmn/CmnBluetoothCodecCapability.h"
#include "capabilities/sony/SonyAncCapability.h"
#include "capabilities/sony/SonyBatteryCapability.h"
#include "sdk/sony/SonyHelper.h"
#include "sdk/sony/SonyInitWatchdog.h"
#include "sdk/sony/enums/SonyAnc.h"
#include "sdk/sony/setters/SonySetAnc.h"

#include <chrono>
#include <thread>

namespace MagicPodsCore
{
    namespace
    {
        std::string BodyHex(const std::vector<unsigned char> &body)
        {
            static const char *const hexDigits = "0123456789abcdef";
            std::string s;
            s.reserve(body.size() * 2);
            for (auto b : body)
            {
                s.push_back(hexDigits[(b >> 4) & 0xf]);
                s.push_back(hexDigits[b & 0xf]);
            }
            return s;
        }
    }

    void SonyDevice::OnResponseDataReceived(const std::vector<unsigned char> &data)
    {
        // The RFCOMM read can hand us any number of frames concatenated in a
        // single recv() buffer. Walk start (0x3E) -> end (0x3C) boundaries
        // and extract each frame independently. Plain 0x3C / 0x3E never appear
        // inside a properly-formed frame body (the framing layer escapes them
        // to 0x3D 0x{2C,2E}), so a naive scan for the markers is safe.
        size_t i = 0;
        while (i < data.size())
        {
            while (i < data.size() && data[i] != SonyPacket::StartByte)
                ++i;
            if (i >= data.size())
                break;

            size_t end = i + 1;
            while (end < data.size() && data[end] != SonyPacket::EndByte)
                ++end;
            if (end >= data.size())
                break;

            std::vector<unsigned char> frame(data.begin() + i, data.begin() + end + 1);
            i = end + 1;

            auto parsed = _packet.Extract(frame);
            if (!parsed.has_value())
            {
                Logger::Warn("Sony RX: dropped %zu-byte frame (parse failed)", frame.size());
                continue;
            }

            const auto &f = parsed.value();
            Logger::Info("Sony RX type=%02x seq=%u cmd=%02x body=%s",
                         static_cast<unsigned int>(f.Type),
                         static_cast<unsigned int>(f.Seq),
                         static_cast<unsigned int>(f.Cmd),
                         BodyHex(f.Body).c_str());

            if (f.Type == SonyDataType::Ack)
                continue;

            // Every incoming command/notify must be ACKed within ~3 s or the
            // device will retransmit and eventually drop the channel.
            SendAck(f.Seq);

            DriveInitStateMachine(f);

            _responseDataReceived.FireEvent(f);
        }
    }

    void SonyDevice::DriveInitStateMachine(const SonyResponseData &frame)
    {
        const auto previousStep = _initStep.load();
        const auto trans = ComputeSonyInitTransition(previousStep, frame);

        _initStep.store(trans.newStep);
        for (const auto &payload : trans.commandsToSend)
            SendCommand(payload);

        if (previousStep != trans.newStep)
        {
            MarkInitProgress();
            switch (trans.newStep)
            {
            case SonyInitStep::AwaitingCapabilityInfo:
                Logger::Info("Sony init: got ProtocolInfo");
                break;
            case SonyInitStep::AwaitingDeviceInfoFw:
                Logger::Info("Sony init: got CapabilityInfo");
                break;
            case SonyInitStep::AwaitingDeviceInfoModel:
                Logger::Info("Sony init: got DeviceInfo(FW)");
                break;
            case SonyInitStep::AwaitingDeviceInfoSeries:
                Logger::Info("Sony init: got DeviceInfo(Model)");
                break;
            case SonyInitStep::AwaitingSupportFunction:
                Logger::Info("Sony init: got DeviceInfo(Series)");
                break;
            case SonyInitStep::Complete:
                Logger::Info("Sony init: got SupportFunction. Sent LogSetStatus + initial battery/ANC state.");

                // Flush any ANC SetParam the user fired off during the
                // handshake (e.g. after a plugin Disconnect/Connect cycle).
                // The buffered state is whatever click was most recent.
                if (auto pending = _deferredAncSet.Flush(); pending.has_value())
                {
                    Logger::Info("Sony: dispatching deferred NcAsmSetParam now that init is complete");
                    SendCommand(SonyNcAsmSetParam::Build(pending.value()));
                }

                // The first PowerGetStatus reply occasionally goes missing
                // (the XM6 is not always ready to answer the moment we
                // send the initial query). Schedule a couple of follow-up
                // battery queries so the battery is visible promptly even
                // when the very first one went unanswered. Detached; self-
                // exits if the client was torn down in the meantime.
                std::thread([this]() {
                    for (auto delay : {std::chrono::seconds(3), std::chrono::seconds(7)})
                    {
                        std::this_thread::sleep_for(delay);
                        if (!_client || !_client->IsStarted())
                            return;
                        SendCommand(SonyPowerGetStatus::Build(SonyPowerInquiredType::Battery));
                    }
                }).detach();
                break;
            default:
                break;
            }
        }
    }

    void SonyDevice::OnConnectedChanged(bool isConnected)
    {
        if (!isConnected)
        {
            // Reset so the next reconnect re-runs the handshake. Tell the
            // watchdog to bow out; the next reconnect will spawn a fresh one.
            _watchdogActive.store(false);
            _outboundSeq.store(0);
            _initStep.store(SonyInitStep::NotStarted);
            return;
        }

        StartHandshakeWhenClientReady();
    }

    void SonyDevice::StartHandshakeWhenClientReady()
    {
        // Whichever event triggers the handshake (Device::Init synchronously
        // starting the client at construction, vs Device's BlueZ
        // connected-status handler firing _onConnectedPropertyChangedEvent
        // *before* it calls _client->Start() on a reconnect), we want the
        // handshake to fire as soon as the RFCOMM socket is actually up. If
        // it's already up, kick off immediately; otherwise poll briefly on a
        // detached thread so a reconnect doesn't leave the V2 init missing.
        if (_client && _client->IsStarted())
        {
            if (_initStep.load() == SonyInitStep::NotStarted)
            {
                Logger::Info("Sony init: starting handshake");
                SendCommand(SonyConnectGetProtocolInfo::Build());
                _initStep.store(SonyInitStep::AwaitingProtocolInfo);
                MarkInitProgress();
                StartInitWatchdog();
            }
            return;
        }

        std::thread([this]() {
            for (int i = 0; i < 20; ++i)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
                if (!_client)
                    return;
                if (!_client->IsStarted())
                    continue;
                if (_initStep.load() != SonyInitStep::NotStarted)
                    return;
                Logger::Info("Sony init: starting handshake (after %dms wait)", (i + 1) * 500);
                SendCommand(SonyConnectGetProtocolInfo::Build());
                _initStep.store(SonyInitStep::AwaitingProtocolInfo);
                MarkInitProgress();
                StartInitWatchdog();
                return;
            }
            Logger::Warn("Sony init: client never started after ~10s, giving up handshake");
        }).detach();
    }

    void SonyDevice::MarkInitProgress()
    {
        _lastInitProgressNs.store(std::chrono::steady_clock::now().time_since_epoch().count());
    }

    std::chrono::milliseconds SonyDevice::SinceLastInitProgress() const
    {
        const auto last = _lastInitProgressNs.load();
        if (last == 0)
            return std::chrono::milliseconds(0);
        const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
        return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::nanoseconds(now - last));
    }

    void SonyDevice::StartInitWatchdog()
    {
        if (_watchdogActive.exchange(true))
            return; // already running

        std::thread([this]() { RunInitWatchdog(); }).detach();
    }

    void SonyDevice::RunInitWatchdog()
    {
        // Cap retry attempts so a permanently-unresponsive device doesn't
        // fill the log forever. Six attempts at the 3s threshold gives
        // ~18s of patience, comfortably more than the WH-1000XM6 has
        // ever taken to wake up after a Steam Deck reboot in practice.
        constexpr int kMaxRetries = 6;
        int retries = 0;

        while (_watchdogActive.load() && retries < kMaxRetries)
        {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            if (!_watchdogActive.load())
                break;

            const auto step = _initStep.load();
            if (step == SonyInitStep::Complete || step == SonyInitStep::NotStarted)
                break;

            if (!_client || !_client->IsStarted())
                break;

            const auto retry = ComputeSonyInitWatchdogTick(step, SinceLastInitProgress());
            if (!retry.has_value())
                continue;

            Logger::Info("Sony init: watchdog re-sending step %d (idle for %lldms, attempt %d/%d)",
                         static_cast<int>(step),
                         static_cast<long long>(SinceLastInitProgress().count()),
                         retries + 1,
                         kMaxRetries);
            SendCommand(retry.value());
            MarkInitProgress();
            ++retries;
        }

        if (retries >= kMaxRetries)
        {
            Logger::Warn("Sony init: watchdog gave up after %d retries; init still at step %d",
                         kMaxRetries,
                         static_cast<int>(_initStep.load()));
        }
        _watchdogActive.store(false);
    }

    void SonyDevice::SendCommand(const std::vector<unsigned char> &payload)
    {
        // fetch_xor returns the previous value and atomically toggles the
        // counter. With multiple sender threads (reading thread for ACKs +
        // state-machine sends, battery retry, init watchdog) this keeps the
        // outbound seq from going off the rails.
        const unsigned char seq = _outboundSeq.fetch_xor(1);

        const auto frame = _packet.Encode(SonyDataType::DataMdr, seq, payload);
        Logger::Info("Sony TX type=0c seq=%u payload=%s",
                     static_cast<unsigned int>(seq),
                     BodyHex(payload).c_str());
        _client->SendData(frame);
    }

    void SonyDevice::SendAck(unsigned char receivedSeq)
    {
        // ACKs are addressed to the frame we just received; their seq is the
        // bitwise complement of the inbound seq and they don't disturb our
        // outbound counter.
        const unsigned char ackSeq = 1 - receivedSeq;
        Logger::Info("Sony TX type=01 seq=%u (ack)", static_cast<unsigned int>(ackSeq));
        _client->SendData(_packet.Encode(SonyDataType::Ack, ackSeq, {}));
    }

    void SonyDevice::SendNcAsmSetParam(const SonyAncState &state)
    {
        const bool initComplete = _initStep.load() == SonyInitStep::Complete;
        const auto toSendNow = _deferredAncSet.Submit(state, initComplete);
        if (toSendNow.has_value())
        {
            SendCommand(SonyNcAsmSetParam::Build(toSendNow.value()));
        }
        else
        {
            Logger::Info("Sony: deferring NcAsmSetParam until V2 init completes (current step %d)",
                         static_cast<int>(_initStep.load()));
        }
    }

    SonyDevice::SonyDevice(std::shared_ptr<DBusDeviceInfo> deviceInfo,
                           std::shared_ptr<PulseAudioClient> audioClient,
                           std::shared_ptr<SettingsService> settingsService,
                           unsigned short model)
        : Device(deviceInfo, audioClient, settingsService),
          _customProductId(model) {}

    std::unique_ptr<SonyDevice> SonyDevice::Create(std::shared_ptr<DBusDeviceInfo> deviceInfo,
                                                   std::shared_ptr<PulseAudioClient> audioClient,
                                                   std::shared_ptr<SettingsService> settingsService,
                                                   unsigned short model)
    {
        auto device = std::make_unique<SonyDevice>(deviceInfo, audioClient, settingsService, model);

        device->capabilities.push_back(std::make_unique<CmnBluetoothCodecCapability>(*device));
        device->capabilities.push_back(std::make_unique<SonyBatteryCapability>(*device));
        device->capabilities.push_back(std::make_unique<SonyAncCapability>(*device));

        device->_client = Client::CreateRFCOMM(deviceInfo->GetAddress(), SonyHelper::GetServiceGuid(static_cast<SonyModelIds>(model)));

        // Init() must run *before* we subscribe our connected-changed handler.
        // Init's own subscription is what calls _client->Start() to open the
        // RFCOMM socket; our handler then runs after it (event listeners fire
        // in subscription order) and pushes the first handshake frame onto an
        // already-open client.
        device->Init();

        auto *raw = device.get();
        device->GetConnectedPropertyChangedEvent().Subscribe([raw](size_t, bool isConnected)
                                                             { raw->OnConnectedChanged(isConnected); });

        // If we were already connected at construction, Init started the
        // client synchronously above; kick off the handshake.
        if (device->GetConnected())
            device->OnConnectedChanged(true);

        return device;
    }
}
