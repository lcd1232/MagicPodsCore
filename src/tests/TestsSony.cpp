// MagicPodsCore: https://github.com/steam3d/MagicPodsCore
// Copyright: 2020-2026 Aleksandr Maslov <https://magicpods.app> & Andrei Litvintsev <a.a.litvintsev@gmail.com>
// License: GPL-3.0

#include "TestsSony.h"

#include "Logger.h"
#include "sdk/sony/SonyDeferredAncSet.h"
#include "sdk/sony/SonyHelper.h"
#include "sdk/sony/SonyInitStateMachine.h"
#include "sdk/sony/SonyInitWatchdog.h"
#include "sdk/sony/SonyPacket.h"
#include "sdk/sony/enums/SonyAnc.h"
#include "sdk/sony/enums/SonyMsgIds.h"
#include "sdk/sony/enums/SonyModelIds.h"
#include "sdk/sony/setters/SonySetAnc.h"
#include "sdk/sony/structs/SonyAncState.h"
#include "sdk/sony/watchers/SonyAncWatcher.h"
#include "sdk/sony/watchers/SonyBatteryWatcher.h"

#include <future>
#include <optional>
#include <string>
#include <vector>

namespace MagicPodsCore
{
    namespace
    {
        void Test(const char *name, bool result)
        {
            const std::string padded = std::string(name) + std::string(std::max<int>(0, 70 - static_cast<int>(std::string(name).length())), ' ');
            if (result)
                Logger::Debug("%s: PASS", padded.c_str());
            else
                Logger::Debug("%s: FAIL", padded.c_str());
        }

        // Convenience: feed a raw frame to a watcher and capture whatever the
        // watcher's event fires (or nothing). Times out if the watcher chose
        // not to fire so a passing-by-silence path can be distinguished from a
        // hang.
        template <typename WatcherT, typename EventDataT>
        std::optional<EventDataT> RunWatcher(WatcherT &watcher,
                                             std::function<Event<EventDataT> &(WatcherT &)> getEvent,
                                             const std::vector<unsigned char> &rawFrame)
        {
            SonyPacket packet{};
            auto parsed = packet.Extract(rawFrame);
            if (!parsed.has_value())
                return std::nullopt;

            std::promise<EventDataT> promise;
            auto future = promise.get_future();
            std::atomic<bool> fired{false};
            size_t listenerId = getEvent(watcher).Subscribe([&promise, &fired](size_t, const EventDataT &state) {
                if (!fired.exchange(true))
                    promise.set_value(state);
            });

            watcher.ProcessResponse(parsed.value());

            getEvent(watcher).Unsubscribe(listenerId);

            if (future.wait_for(std::chrono::milliseconds(1)) == std::future_status::ready)
                return future.get();
            return std::nullopt;
        }
    }

    // ---- Frame layer ----

    bool TestsSony::TestPacketEncodeNoEscape()
    {
        // PowerGetStatus(BATTERY) at seq=0 — payload [0x22, 0x00].
        // Body: type + seq + size32 + payload = [0x0c, 0x00, 0,0,0,2, 0x22, 0x00]
        // CRC = 0x0c + 0x02 + 0x22 + 0x00 = 0x30
        // Frame: [0x3e, 0x0c, 0x00, 0,0,0,2, 0x22, 0x00, 0x30, 0x3c]
        SonyPacket packet{};
        const auto frame = packet.Encode(SonyDataType::DataMdr, 0, {0x22, 0x00});
        const std::vector<unsigned char> expected = {0x3e, 0x0c, 0x00, 0x00, 0x00, 0x00, 0x02, 0x22, 0x00, 0x30, 0x3c};
        return frame == expected;
    }

    bool TestsSony::TestPacketEncodeWithEscape()
    {
        // Force a 0x3c into the payload so the encoder must escape it. The
        // escape rule: 0x3c -> 0x3d 0x2c.
        SonyPacket packet{};
        const auto frame = packet.Encode(SonyDataType::DataMdr, 0, {0x3c});
        // Body: [0x0c, 0x00, 0,0,0,1, 0x3c]
        // CRC: 0x0c + 0x01 + 0x3c = 0x49
        // Pre-escape: [0x0c, 0x00, 0x00, 0x00, 0x00, 0x01, 0x3c, 0x49]
        // Post-escape: [0x0c, 0x00, 0x00, 0x00, 0x00, 0x01, 0x3d, 0x2c, 0x49]
        // Frame: [0x3e, ...escaped..., 0x3c]
        const std::vector<unsigned char> expected = {0x3e, 0x0c, 0x00, 0x00, 0x00, 0x00, 0x01, 0x3d, 0x2c, 0x49, 0x3c};
        return frame == expected;
    }

    bool TestsSony::TestPacketExtractNoEscape()
    {
        // Real on-the-wire PowerRetStatus from the WH-1000XM6 (level 82,
        // not charging). body[7]=0x00=Battery, body[8]=0x52=82, body[9]=0x00=NotCharging.
        const std::vector<unsigned char> raw = {0x3e, 0x0c, 0x01, 0x00, 0x00, 0x00, 0x04, 0x23, 0x00, 0x52, 0x00, 0x86, 0x3c};
        SonyPacket packet{};
        auto parsed = packet.Extract(raw);
        if (!parsed.has_value())
            return false;
        const auto &f = parsed.value();
        return f.Type == SonyDataType::DataMdr
            && f.Seq == 1
            && f.Cmd == SonyT1Command::PowerRetStatus
            && f.Body.size() == 10
            && f.Body[7] == 0x00
            && f.Body[8] == 0x52
            && f.Body[9] == 0x00;
    }

    bool TestsSony::TestPacketExtractWithEscape()
    {
        // Round-trip through encode/extract using a payload containing every
        // problematic byte: 0x3c, 0x3d, 0x3e. After escaping in the wire frame
        // none of those should appear unescaped except as the start/end markers.
        SonyPacket packet{};
        const std::vector<unsigned char> payload = {0x3c, 0x3d, 0x3e, 0x42};
        const auto frame = packet.Encode(SonyDataType::DataMdr, 1, payload);

        // No 0x3c/0x3d/0x3e between the markers.
        for (size_t i = 1; i + 1 < frame.size(); ++i)
        {
            if (frame[i] == 0x3c || frame[i] == 0x3e)
                return false;
        }

        auto parsed = packet.Extract(frame);
        if (!parsed.has_value())
            return false;
        const auto &f = parsed.value();
        // Body = type + seq + size32 + payload. Last 4 bytes of body are the payload.
        if (f.Body.size() != 6 + payload.size())
            return false;
        for (size_t i = 0; i < payload.size(); ++i)
        {
            if (f.Body[6 + i] != payload[i])
                return false;
        }
        return true;
    }

    bool TestsSony::TestPacketRoundTrip()
    {
        // Payloads of various sizes round-trip through encode -> extract.
        SonyPacket packet{};
        const std::vector<std::vector<unsigned char>> payloads = {
            {0x00, 0x00},                                                 // GetProtocolInfo
            {0x22, 0x00},                                                 // PowerGetStatus(BATTERY)
            {0x66, 0x19},                                                 // NcAsmGetParam(0x19)
            {0x68, 0x19, 0x01, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00},        // NcAsmSetParam(ANC on)
            {0xc4, 0x01, 0x00},                                            // LogSetStatus
        };
        for (unsigned char seq : {static_cast<unsigned char>(0), static_cast<unsigned char>(1)})
        {
            for (const auto &payload : payloads)
            {
                const auto frame = packet.Encode(SonyDataType::DataMdr, seq, payload);
                auto parsed = packet.Extract(frame);
                if (!parsed.has_value())
                    return false;
                const auto &f = parsed.value();
                if (f.Type != SonyDataType::DataMdr || f.Seq != seq)
                    return false;
                if (f.Body.size() != 6 + payload.size())
                    return false;
                for (size_t i = 0; i < payload.size(); ++i)
                {
                    if (f.Body[6 + i] != payload[i])
                        return false;
                }
            }
        }
        return true;
    }

    bool TestsSony::TestPacketRejectsBadCrc()
    {
        // Same frame as TestPacketExtractNoEscape but with a corrupted CRC byte.
        SonyPacket packet{};
        const std::vector<unsigned char> raw = {0x3e, 0x0c, 0x01, 0x00, 0x00, 0x00, 0x04, 0x23, 0x00, 0x52, 0x00, 0x87 /* was 0x86 */, 0x3c};
        return !packet.Extract(raw).has_value();
    }

    bool TestsSony::TestPacketRejectsTruncated()
    {
        SonyPacket packet{};
        // Far too short to even contain a header.
        return !packet.Extract({0x3e, 0x3c}).has_value()
            && !packet.Extract({0x3e}).has_value()
            && !packet.Extract({}).has_value();
    }

    bool TestsSony::TestPacketAckRoundTrip()
    {
        // ACK of a frame received at seq=0: ACK seq is 1-0=1, payload is empty.
        // Body: [0x01, 0x01, 0,0,0,0]; CRC = 0x01+0x01 = 0x02.
        SonyPacket packet{};
        const auto frame = packet.Encode(SonyDataType::Ack, 1, {});
        const std::vector<unsigned char> expected = {0x3e, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x02, 0x3c};
        if (frame != expected)
            return false;
        auto parsed = packet.Extract(frame);
        return parsed.has_value()
            && parsed.value().Type == SonyDataType::Ack
            && parsed.value().Seq == 1;
    }

    // ---- Watchers ----

    bool TestsSony::TestBatteryWatcherSingle()
    {
        // Real WH-1000XM6 PowerRetStatus reply: cmd=0x23, type=0x00 (Battery),
        // level=0x52 (82), chargingStatus=0x00 (NotCharging).
        const std::vector<unsigned char> raw = {0x3e, 0x0c, 0x01, 0x00, 0x00, 0x00, 0x04, 0x23, 0x00, 0x52, 0x00, 0x86, 0x3c};
        SonyBatteryWatcher watcher(SonyModelIds::Wh1000xm6);
        auto state = RunWatcher<SonyBatteryWatcher, std::vector<DeviceBatteryData>>(
            watcher, [](SonyBatteryWatcher &w) -> Event<std::vector<DeviceBatteryData>> & { return w.GetBatteryChangedEvent(); }, raw);
        if (!state.has_value())
            return false;
        if (state->size() != 1)
            return false;
        const auto &b = state->front();
        return b.Type == DeviceBatteryType::Single
            && b.Battery == 82
            && b.IsCharging == false
            && b.Status == DeviceBatteryStatus::Connected;
    }

    bool TestsSony::TestBatteryWatcherIgnoresWrongCmd()
    {
        // A frame with cmd=0x21 (POWER_RET_CAPABILITY) instead of 0x23 should
        // not produce a battery update.
        const std::vector<unsigned char> raw = {0x3e, 0x0c, 0x00, 0x00, 0x00, 0x00, 0x04, 0x21, 0x00, 0x52, 0x00, 0x83, 0x3c};
        SonyBatteryWatcher watcher(SonyModelIds::Wh1000xm6);
        auto state = RunWatcher<SonyBatteryWatcher, std::vector<DeviceBatteryData>>(
            watcher, [](SonyBatteryWatcher &w) -> Event<std::vector<DeviceBatteryData>> & { return w.GetBatteryChangedEvent(); }, raw);
        return !state.has_value();
    }

    bool TestsSony::TestBatteryWatcherIgnoresShortBody()
    {
        // Truncated body — only 8 bytes after the header instead of 10. Watcher
        // should bail rather than read out of bounds.
        const std::vector<unsigned char> raw = {0x3e, 0x0c, 0x00, 0x00, 0x00, 0x00, 0x02, 0x23, 0x00, 0x2f, 0x3c};
        SonyBatteryWatcher watcher(SonyModelIds::Wh1000xm6);
        auto state = RunWatcher<SonyBatteryWatcher, std::vector<DeviceBatteryData>>(
            watcher, [](SonyBatteryWatcher &w) -> Event<std::vector<DeviceBatteryData>> & { return w.GetBatteryChangedEvent(); }, raw);
        return !state.has_value();
    }

    bool TestsSony::TestAncWatcherTransparency()
    {
        // WH-1000XM6 NcAsmRetParam shape:
        //   payload = [0x67, 0x19, changeStatus, master, mode, ambientMode, level, autoAsm, sensitivity]
        // master=On, mode=ASM (transparency), ambientMode=Normal,
        // ambientLevel=1, autoAsm=Off, sensitivity=Standard.
        SonyPacket packet{};
        const std::vector<unsigned char> payload = {0x67, 0x19, 0x01, 0x01, 0x01, 0x00, 0x01, 0x00, 0x00};
        const auto raw = packet.Encode(SonyDataType::DataMdr, 0, payload);
        SonyAncWatcher watcher(SonyModelIds::Wh1000xm6);
        auto state = RunWatcher<SonyAncWatcher, SonyAncState>(
            watcher, [](SonyAncWatcher &w) -> Event<SonyAncState> & { return w.GetAncChangedEvent(); }, raw);
        if (!state.has_value())
            return false;
        return state->MasterEnabled == SonyNcAsmOnOff::On
            && state->Mode == SonyNcAsmMode::Asm
            && state->AmbientMode == SonyAmbientSoundMode::Normal
            && state->AmbientLevel == 1
            && state->NoiseAdaptiveEnabled == SonyNcAsmOnOff::Off
            && state->NoiseAdaptiveSensitivity == SonyNoiseAdaptiveSensitivity::Standard;
    }

    bool TestsSony::TestAncWatcherNoiseCancellation()
    {
        // Same payload shape but master=On, mode=NC, ambientLevel=0, autoAsm=On.
        SonyPacket packet{};
        const std::vector<unsigned char> payload = {0x67, 0x19, 0x01, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00};
        const auto frame = packet.Encode(SonyDataType::DataMdr, 0, payload);
        SonyAncWatcher watcher(SonyModelIds::Wh1000xm6);
        auto state = RunWatcher<SonyAncWatcher, SonyAncState>(
            watcher, [](SonyAncWatcher &w) -> Event<SonyAncState> & { return w.GetAncChangedEvent(); }, frame);
        if (!state.has_value())
            return false;
        return state->MasterEnabled == SonyNcAsmOnOff::On
            && state->Mode == SonyNcAsmMode::Nc
            && state->AmbientLevel == 0
            && state->NoiseAdaptiveEnabled == SonyNcAsmOnOff::On;
    }

    bool TestsSony::TestAncWatcherOff()
    {
        SonyPacket packet{};
        const std::vector<unsigned char> payload = {0x67, 0x19, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
        const auto frame = packet.Encode(SonyDataType::DataMdr, 0, payload);
        SonyAncWatcher watcher(SonyModelIds::Wh1000xm6);
        auto state = RunWatcher<SonyAncWatcher, SonyAncState>(
            watcher, [](SonyAncWatcher &w) -> Event<SonyAncState> & { return w.GetAncChangedEvent(); }, frame);
        return state.has_value() && state->MasterEnabled == SonyNcAsmOnOff::Off;
    }

    bool TestsSony::TestAncWatcherIgnoresWrongInquiredType()
    {
        // Inquired-type 0x17 (the non-NA seamless variant). The XM6 watcher is
        // wired only for 0x19; a 0x17 frame must not fire an update.
        SonyPacket packet{};
        const std::vector<unsigned char> payload = {0x67, 0x17, 0x01, 0x01, 0x00, 0x00, 0x00};
        const auto frame = packet.Encode(SonyDataType::DataMdr, 0, payload);
        SonyAncWatcher watcher(SonyModelIds::Wh1000xm6);
        auto state = RunWatcher<SonyAncWatcher, SonyAncState>(
            watcher, [](SonyAncWatcher &w) -> Event<SonyAncState> & { return w.GetAncChangedEvent(); }, frame);
        return !state.has_value();
    }

    // ---- Setters ----

    bool TestsSony::TestSetAncBuildsExpectedPayload()
    {
        SonyAncState state{};
        state.ChangeStatus = SonyValueChangeStatus::Changed;
        state.MasterEnabled = SonyNcAsmOnOff::On;
        state.Mode = SonyNcAsmMode::Nc;
        state.AmbientMode = SonyAmbientSoundMode::Normal;
        state.AmbientLevel = 0;
        state.NoiseAdaptiveEnabled = SonyNcAsmOnOff::On;
        state.NoiseAdaptiveSensitivity = SonyNoiseAdaptiveSensitivity::Standard;

        const auto payload = SonyNcAsmSetParam::Build(state);
        const std::vector<unsigned char> expected = {
            0x68, 0x19, 0x01, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00,
        };
        return payload == expected;
    }

    bool TestsSony::TestPowerGetStatusBuildsExpectedPayload()
    {
        const auto payload = SonyPowerGetStatus::Build(SonyPowerInquiredType::Battery);
        return payload == std::vector<unsigned char>{0x22, 0x00};
    }

    bool TestsSony::TestNcAsmGetParamBuildsExpectedPayload()
    {
        const auto payload = SonyNcAsmGetParam::Build(SonyNcAsmInquiredType::ModeNcAsmDualNcModeSwitchAndAsmSeamlessNa);
        return payload == std::vector<unsigned char>{0x66, 0x19};
    }

    // ---- Helper / detection ----

    bool TestsSony::TestIsSonyDeviceMatchesWh1000xm6()
    {
        return SonyHelper::IsSonyDevice(0x054c, 0x0f8a);
    }

    bool TestsSony::TestIsSonyDeviceRejectsOtherVendor()
    {
        // Apple vendor + an unrelated PID must not be detected as Sony.
        return !SonyHelper::IsSonyDevice(0x004c, 0x0f8a);
    }

    bool TestsSony::TestIsSonyDeviceRejectsUnknownProductId()
    {
        // Sony vendor but a PID we haven't registered (e.g. the legacy XM4) -
        // still rejected for now.
        return !SonyHelper::IsSonyDevice(0x054c, 0x0d58);
    }

    // ---- V2 init state machine ----
    //
    // The state machine is a pure function of (currentStep, receivedFrame),
    // so we can test it without a real Bluetooth client. Each test builds
    // a frame that mirrors what the WH-1000XM6 actually sends on the wire.

    namespace
    {
        // Build a SonyResponseData as the SonyDevice receive path would, by
        // round-tripping through SonyPacket so the body is consistent with
        // a real frame's layout (type/seq/size32/payload).
        SonyResponseData MakeFrame(unsigned char seq, const std::vector<unsigned char> &payload)
        {
            SonyPacket packet{};
            const auto frame = packet.Encode(SonyDataType::DataMdr, seq, payload);
            return packet.Extract(frame).value();
        }
    }

    // ---- ANC set deferral ----
    //
    // Repro: after a plugin-driven Disconnect/Connect cycle the V2 handshake
    // restarts. If the user touches the ANC slider during the few seconds
    // it takes init to finish, the SetParam frame goes out interleaved
    // with init queries and the WH-1000XM6 silently drops it - their click
    // appears to do nothing, even though a second click after init
    // completes works. SonyDeferredAncSet buffers the request while init
    // is mid-flight and dispatches it once init reaches Complete.

    bool TestsSony::TestDeferredAncSendsImmediatelyIfReady()
    {
        SonyDeferredAncSet d;
        SonyAncState s;
        s.MasterEnabled = SonyNcAsmOnOff::On;
        s.Mode = SonyNcAsmMode::Asm;
        const auto out = d.Submit(s, /*initComplete=*/true);
        return out.has_value()
            && out->MasterEnabled == SonyNcAsmOnOff::On
            && out->Mode == SonyNcAsmMode::Asm;
    }

    bool TestsSony::TestDeferredAncBuffersWhenNotReady()
    {
        SonyDeferredAncSet d;
        SonyAncState s;
        s.MasterEnabled = SonyNcAsmOnOff::On;
        const auto out = d.Submit(s, /*initComplete=*/false);
        return !out.has_value();
    }

    bool TestsSony::TestDeferredAncFlushesOnInitComplete()
    {
        SonyDeferredAncSet d;
        SonyAncState s;
        s.MasterEnabled = SonyNcAsmOnOff::On;
        s.Mode = SonyNcAsmMode::Nc;
        d.Submit(s, /*initComplete=*/false);
        const auto flushed = d.Flush();
        return flushed.has_value()
            && flushed->MasterEnabled == SonyNcAsmOnOff::On
            && flushed->Mode == SonyNcAsmMode::Nc;
    }

    bool TestsSony::TestDeferredAncFlushReturnsNothingIfNothingBuffered()
    {
        SonyDeferredAncSet d;
        const auto flushed = d.Flush();
        return !flushed.has_value();
    }

    bool TestsSony::TestDeferredAncOnlyKeepsLatestRequest()
    {
        // Rapid slider drags should produce only one final dispatch with
        // the user's most-recent intent, not a flurry of stale Set frames.
        SonyDeferredAncSet d;
        SonyAncState s1;
        s1.MasterEnabled = SonyNcAsmOnOff::Off;
        SonyAncState s2;
        s2.MasterEnabled = SonyNcAsmOnOff::On;
        s2.Mode = SonyNcAsmMode::Asm;
        d.Submit(s1, /*initComplete=*/false);
        d.Submit(s2, /*initComplete=*/false);
        const auto flushed = d.Flush();
        return flushed.has_value()
            && flushed->MasterEnabled == SonyNcAsmOnOff::On
            && flushed->Mode == SonyNcAsmMode::Asm;
    }

    bool TestsSony::TestDeferredAncSubmitImmediateClearsAnyBufferedRequest()
    {
        // If we buffered something while init was running and then init
        // happens to complete *before* a follow-up user click, the
        // already-flushed pending state shouldn't fire a second time when
        // the next click arrives with initComplete=true.
        SonyDeferredAncSet d;
        SonyAncState s1;
        s1.MasterEnabled = SonyNcAsmOnOff::Off;
        SonyAncState s2;
        s2.MasterEnabled = SonyNcAsmOnOff::On;

        d.Submit(s1, /*initComplete=*/false);
        // ... then init completes and the caller flushes:
        d.Flush();
        // ... and a fresh user click arrives with init now done:
        const auto out = d.Submit(s2, /*initComplete=*/true);
        // ... and the next Flush should be empty (nothing to re-send):
        const auto residual = d.Flush();
        return out.has_value()
            && out->MasterEnabled == SonyNcAsmOnOff::On
            && !residual.has_value();
    }

    // ---- V2 init watchdog ----
    //
    // Repro for the bug observed after a Steam Deck reboot at 15:06:33 on
    // 2026-05-10: the WH-1000XM6 reconnected (15:06:57), our RFCOMM client
    // came up, the handshake fired its first GetProtocolInfo at 15:06:58
    // ... and the device sent absolutely nothing back, ever. There was no
    // watchdog to re-send the query, so the state machine sat at
    // AwaitingProtocolInfo forever and battery + ANC stayed empty.
    //
    // These tests describe the watchdog behavior we expect. They live up
    // here (above the rest of the state-machine tests) so it's clear they
    // were the failing tests that drove the fix.

    bool TestsSony::TestWatchdogRetriesProtocolInfoAfterTimeout()
    {
        // 4 seconds in AwaitingProtocolInfo with no progress. Watchdog must
        // hand back the GetProtocolInfo payload so the device gets another
        // chance to reply.
        const auto retry = ComputeSonyInitWatchdogTick(
            SonyInitStep::AwaitingProtocolInfo,
            std::chrono::seconds(4));
        return retry.has_value()
            && retry.value() == std::vector<unsigned char>{0x00, 0x00};
    }

    bool TestsSony::TestWatchdogRetriesEachAwaitingStepAfterTimeout()
    {
        // Every "Awaiting*" step must have a defined retry payload that
        // matches the original outbound for that step. If a step is silently
        // un-retried, a stalled handshake would never recover.
        const std::vector<std::pair<SonyInitStep, std::vector<unsigned char>>> expected = {
            {SonyInitStep::AwaitingProtocolInfo,    {0x00, 0x00}},
            {SonyInitStep::AwaitingCapabilityInfo,  {0x02, 0x00}},
            {SonyInitStep::AwaitingDeviceInfoFw,    {0x04, 0x02}},
            {SonyInitStep::AwaitingDeviceInfoModel, {0x04, 0x01}},
            {SonyInitStep::AwaitingDeviceInfoSeries,{0x04, 0x03}},
            {SonyInitStep::AwaitingSupportFunction, {0x06, 0x00}},
        };
        for (const auto &[step, payload] : expected)
        {
            const auto retry = ComputeSonyInitWatchdogTick(step, std::chrono::seconds(5));
            if (!retry.has_value() || retry.value() != payload)
                return false;
        }
        return true;
    }

    bool TestsSony::TestWatchdogQuietWhileWaiting()
    {
        // Only 500ms in - way too soon to retry. The XM6 is sometimes slow
        // to reply but should not see a duplicate query in a tight loop.
        const auto retry = ComputeSonyInitWatchdogTick(
            SonyInitStep::AwaitingProtocolInfo,
            std::chrono::milliseconds(500));
        return !retry.has_value();
    }

    bool TestsSony::TestWatchdogQuietWhenComplete()
    {
        // Once init has completed, the watchdog must never fire - any
        // resends after that would be unsolicited noise.
        const auto retry = ComputeSonyInitWatchdogTick(
            SonyInitStep::Complete,
            std::chrono::seconds(60));
        return !retry.has_value();
    }

    bool TestsSony::TestWatchdogQuietBeforeStart()
    {
        // Before the handshake has even started there is nothing to retry.
        const auto retry = ComputeSonyInitWatchdogTick(
            SonyInitStep::NotStarted,
            std::chrono::seconds(60));
        return !retry.has_value();
    }

    // ---- V2 init state machine ----
    bool TestsSony::TestInitMachineProtocolInfoToCapabilityInfo()
    {
        // CONNECT_RET_PROTOCOL_INFO: cmd=0x01, inquiredType=0x00, then
        // protocol version + table-1/table-2 supported flags.
        const auto frame = MakeFrame(0, {0x01, 0x00, 0x00, 0x00, 0x03, 0x00, 0x30, 0x17, 0x00, 0x00});
        const auto trans = ComputeSonyInitTransition(SonyInitStep::AwaitingProtocolInfo, frame);
        return trans.newStep == SonyInitStep::AwaitingCapabilityInfo
            && trans.commandsToSend.size() == 1
            && trans.commandsToSend[0] == std::vector<unsigned char>{0x02, 0x00};
    }

    bool TestsSony::TestInitMachineCapabilityInfoToDeviceInfoFw()
    {
        // CONNECT_RET_CAPABILITY_INFO: cmd=0x03, inquiredType=0x00, counter, then UID string.
        const auto frame = MakeFrame(1, {0x03, 0x00, 0x04, 0x11, '5', '8', ':', '1', '8', ':', '6', '2', ':', '2', '0', ':', '3', 'C', ':', '0', '8'});
        const auto trans = ComputeSonyInitTransition(SonyInitStep::AwaitingCapabilityInfo, frame);
        return trans.newStep == SonyInitStep::AwaitingDeviceInfoFw
            && trans.commandsToSend.size() == 1
            && trans.commandsToSend[0] == std::vector<unsigned char>{0x04, 0x02};
    }

    bool TestsSony::TestInitMachineDeviceInfoFwToModel()
    {
        // CONNECT_RET_DEVICE_INFO with type=FwVersion (0x02): we expect to
        // advance and request the Model name next.
        const auto frame = MakeFrame(0, {0x05, 0x02, 0x05, '3', '.', '0', '.', '0'});
        const auto trans = ComputeSonyInitTransition(SonyInitStep::AwaitingDeviceInfoFw, frame);
        return trans.newStep == SonyInitStep::AwaitingDeviceInfoModel
            && trans.commandsToSend.size() == 1
            && trans.commandsToSend[0] == std::vector<unsigned char>{0x04, 0x01};
    }

    bool TestsSony::TestInitMachineRejectsFwRetransmitInModelState()
    {
        // We're waiting for DeviceInfo(Model). The XM6 sometimes retransmits
        // its DeviceInfo(FW) reply (cmd=0x05 type=0x02) before getting around
        // to actually answering our Model query. The state machine MUST stay
        // in AwaitingDeviceInfoModel and not fire any outbound command -
        // otherwise we'd ask for Series next, then SupportFunction, all
        // while the device is still on FW, and the chain falls apart.
        const auto frame = MakeFrame(0, {0x05, 0x02, 0x05, '3', '.', '0', '.', '0'});
        const auto trans = ComputeSonyInitTransition(SonyInitStep::AwaitingDeviceInfoModel, frame);
        return trans.newStep == SonyInitStep::AwaitingDeviceInfoModel
            && trans.commandsToSend.empty();
    }

    bool TestsSony::TestInitMachineRejectsModelRetransmitInSeriesState()
    {
        // Same shape as the FW-retransmit test, but in the Series-awaiting
        // state and with a ModelName retransmit (type=0x01). Also must stay.
        const auto frame = MakeFrame(0, {0x05, 0x01, 0x0a, 'W', 'H', '-', '1', '0', '0', '0', 'X', 'M', '6'});
        const auto trans = ComputeSonyInitTransition(SonyInitStep::AwaitingDeviceInfoSeries, frame);
        return trans.newStep == SonyInitStep::AwaitingDeviceInfoSeries
            && trans.commandsToSend.empty();
    }

    bool TestsSony::TestInitMachineSeriesToSupportFunction()
    {
        // Real Series-and-color reply: cmd=0x05 type=0x03 + 2 bytes (series, color).
        const auto frame = MakeFrame(1, {0x05, 0x03, 0x30, 0x05});
        const auto trans = ComputeSonyInitTransition(SonyInitStep::AwaitingDeviceInfoSeries, frame);
        return trans.newStep == SonyInitStep::AwaitingSupportFunction
            && trans.commandsToSend.size() == 1
            && trans.commandsToSend[0] == std::vector<unsigned char>{0x06, 0x00};
    }

    bool TestsSony::TestInitMachineSupportFunctionToCompleteSendsThreeCommands()
    {
        // CONNECT_RET_SUPPORT_FUNCTION; payload after cmd+inquiredType is
        // a count + (functionType, priority) pairs. We don't parse the list
        // here, just check we transition to Complete and emit LogSetStatus +
        // the initial PowerGetStatus + NcAsmGetParam.
        const auto frame = MakeFrame(0, {0x07, 0x00, 0x02, 0x10, 0xff, 0x12, 0xff});
        const auto trans = ComputeSonyInitTransition(SonyInitStep::AwaitingSupportFunction, frame);
        if (trans.newStep != SonyInitStep::Complete)
            return false;
        if (trans.commandsToSend.size() != 3)
            return false;
        return trans.commandsToSend[0] == std::vector<unsigned char>{0xc4, 0x01, 0x00}
            && trans.commandsToSend[1] == std::vector<unsigned char>{0x22, 0x00}
            && trans.commandsToSend[2] == std::vector<unsigned char>{0x66, 0x19};
    }

    bool TestsSony::TestInitMachineIgnoresUnrelatedFrameInProtocolInfoState()
    {
        // Device pushes a NcAsmNtfy (cmd=0x69) while we're still awaiting
        // ProtocolInfo. Must not advance.
        const auto frame = MakeFrame(0, {0x69, 0x19, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00});
        const auto trans = ComputeSonyInitTransition(SonyInitStep::AwaitingProtocolInfo, frame);
        return trans.newStep == SonyInitStep::AwaitingProtocolInfo
            && trans.commandsToSend.empty();
    }

    bool TestsSony::TestInitMachineCompleteIsTerminal()
    {
        // Once we're Complete, no incoming frame moves us. Even a fresh
        // ProtocolInfo wouldn't reset us; that's a job for OnConnectedChanged
        // on a reconnect, which sets _initStep = NotStarted before we run
        // through the chain again.
        const auto frame = MakeFrame(0, {0x01, 0x00, 0x00, 0x00, 0x03, 0x00, 0x30, 0x17, 0x00, 0x00});
        const auto trans = ComputeSonyInitTransition(SonyInitStep::Complete, frame);
        return trans.newStep == SonyInitStep::Complete
            && trans.commandsToSend.empty();
    }

    bool TestsSony::TestInitMachineIgnoresAckFrames()
    {
        // ACK frames carry no payload and should never advance the init.
        SonyResponseData ack{SonyDataType::Ack, 1, SonyT1Command::ConnectGetProtocolInfo, {0x01, 0x01, 0, 0, 0, 0}};
        const auto trans = ComputeSonyInitTransition(SonyInitStep::AwaitingProtocolInfo, ack);
        return trans.newStep == SonyInitStep::AwaitingProtocolInfo
            && trans.commandsToSend.empty();
    }

    TestsSony::TestsSony()
    {
        Test("SonyPacket.EncodeNoEscape", TestPacketEncodeNoEscape());
        Test("SonyPacket.EncodeWithEscape", TestPacketEncodeWithEscape());
        Test("SonyPacket.ExtractNoEscape", TestPacketExtractNoEscape());
        Test("SonyPacket.ExtractWithEscape", TestPacketExtractWithEscape());
        Test("SonyPacket.RoundTrip", TestPacketRoundTrip());
        Test("SonyPacket.RejectsBadCrc", TestPacketRejectsBadCrc());
        Test("SonyPacket.RejectsTruncated", TestPacketRejectsTruncated());
        Test("SonyPacket.AckRoundTrip", TestPacketAckRoundTrip());

        Test("SonyBatteryWatcher.Single", TestBatteryWatcherSingle());
        Test("SonyBatteryWatcher.IgnoresWrongCmd", TestBatteryWatcherIgnoresWrongCmd());
        Test("SonyBatteryWatcher.IgnoresShortBody", TestBatteryWatcherIgnoresShortBody());

        Test("SonyAncWatcher.Transparency", TestAncWatcherTransparency());
        Test("SonyAncWatcher.NoiseCancellation", TestAncWatcherNoiseCancellation());
        Test("SonyAncWatcher.Off", TestAncWatcherOff());
        Test("SonyAncWatcher.IgnoresWrongInquiredType", TestAncWatcherIgnoresWrongInquiredType());

        Test("SonyNcAsmSetParam.Build", TestSetAncBuildsExpectedPayload());
        Test("SonyPowerGetStatus.Build", TestPowerGetStatusBuildsExpectedPayload());
        Test("SonyNcAsmGetParam.Build", TestNcAsmGetParamBuildsExpectedPayload());

        Test("SonyHelper.IsSonyDeviceMatchesWh1000xm6", TestIsSonyDeviceMatchesWh1000xm6());
        Test("SonyHelper.IsSonyDeviceRejectsOtherVendor", TestIsSonyDeviceRejectsOtherVendor());
        Test("SonyHelper.IsSonyDeviceRejectsUnknownProductId", TestIsSonyDeviceRejectsUnknownProductId());

        Test("SonyDeferredAncSet.SendsImmediatelyIfReady", TestDeferredAncSendsImmediatelyIfReady());
        Test("SonyDeferredAncSet.BuffersWhenNotReady", TestDeferredAncBuffersWhenNotReady());
        Test("SonyDeferredAncSet.FlushesOnInitComplete", TestDeferredAncFlushesOnInitComplete());
        Test("SonyDeferredAncSet.FlushReturnsNothingIfNothingBuffered", TestDeferredAncFlushReturnsNothingIfNothingBuffered());
        Test("SonyDeferredAncSet.OnlyKeepsLatestRequest", TestDeferredAncOnlyKeepsLatestRequest());
        Test("SonyDeferredAncSet.SubmitImmediateClearsAnyBufferedRequest", TestDeferredAncSubmitImmediateClearsAnyBufferedRequest());

        Test("SonyInitWatchdog.RetriesProtocolInfoAfterTimeout", TestWatchdogRetriesProtocolInfoAfterTimeout());
        Test("SonyInitWatchdog.RetriesEachAwaitingStepAfterTimeout", TestWatchdogRetriesEachAwaitingStepAfterTimeout());
        Test("SonyInitWatchdog.QuietWhileWaiting", TestWatchdogQuietWhileWaiting());
        Test("SonyInitWatchdog.QuietWhenComplete", TestWatchdogQuietWhenComplete());
        Test("SonyInitWatchdog.QuietBeforeStart", TestWatchdogQuietBeforeStart());

        Test("SonyInitMachine.ProtocolInfoToCapabilityInfo", TestInitMachineProtocolInfoToCapabilityInfo());
        Test("SonyInitMachine.CapabilityInfoToDeviceInfoFw", TestInitMachineCapabilityInfoToDeviceInfoFw());
        Test("SonyInitMachine.DeviceInfoFwToModel", TestInitMachineDeviceInfoFwToModel());
        Test("SonyInitMachine.RejectsFwRetransmitInModelState", TestInitMachineRejectsFwRetransmitInModelState());
        Test("SonyInitMachine.RejectsModelRetransmitInSeriesState", TestInitMachineRejectsModelRetransmitInSeriesState());
        Test("SonyInitMachine.SeriesToSupportFunction", TestInitMachineSeriesToSupportFunction());
        Test("SonyInitMachine.SupportFunctionToCompleteSendsThreeCommands", TestInitMachineSupportFunctionToCompleteSendsThreeCommands());
        Test("SonyInitMachine.IgnoresUnrelatedFrameInProtocolInfoState", TestInitMachineIgnoresUnrelatedFrameInProtocolInfoState());
        Test("SonyInitMachine.CompleteIsTerminal", TestInitMachineCompleteIsTerminal());
        Test("SonyInitMachine.IgnoresAckFrames", TestInitMachineIgnoresAckFrames());
    }
}
