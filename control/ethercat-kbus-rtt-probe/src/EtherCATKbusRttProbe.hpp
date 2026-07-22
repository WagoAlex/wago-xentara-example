#pragma once

#include <chrono>
#include <cstdint>
#include <limits>

#include <xentara/Input.hpp>
#include <xentara/Output.hpp>
#include <xentara/cpp-control.h>

// Same step()-to-step() cycle-time measurement as EtherCATRttProbe (see that
// control's header for what it does and doesn't measure), plus one more
// number: a hardware-verified round-trip time over a physical DO->DI
// loopback (Do bound to DO_8ch_8, Di bound to DI_8ch_1 in this deployment's
// model, wired together on the terminal block).
//
// "Verified" matters here. The very first write (Do = true) is not trusted
// as a real measurement, it is a connectivity check: if Di never reads back
// true within kStallCycles cycles, KbusConnected is latched false and
// KbusCycle* stays at zero forever, so a missing/incorrect wire shows up as
// "not connected", not as a plausible-looking wrong number. Once that first
// round-trip succeeds, KbusConnected latches true and every subsequent
// toggle updates KbusCycleLastMs/MinMs/MaxMs/AvgMs.
//
// The measured round trip is bounded by the coupler's K-Bus scan cycle, not
// by EtherCAT frame rate or wire propagation (the latter is sub-microsecond
// and not observable here) - see the wago-xentara-example README for
// measurements taken on real hardware.
class EtherCATKbusRttProbe final : public xentara::Control
{
public:
    void initialize(xentara::InitContext &context) override;

    void step(xentara::RunContext &context) override;

private:
    static constexpr std::uint64_t kStallCycles{5000};

    /// Cycle-time (step-to-step interval) measurement state, identical in
    /// spirit to EtherCATRttProbe.
    bool haveLastStepTime{false};
    std::chrono::steady_clock::time_point lastStepTime{};
    double rttMinMs{std::numeric_limits<double>::infinity()};
    double rttMaxMs{0.0};
    double rttSumMs{0.0};
    std::uint64_t rttSampleCount{0};

    xentara::DoubleOutput rttLastMs;
    xentara::DoubleOutput rttMinMsOut;
    xentara::DoubleOutput rttMaxMsOut;
    xentara::DoubleOutput rttAvgMsOut;
    xentara::UInt64Output rttSampleCountOut;

    /// Hardware DO->DI loopback round-trip state.
    xentara::BooleanOutput doOut;
    xentara::BooleanInput diIn;

    bool kbusConnected{false};
    bool kbusConnectionFailed{false};
    bool targetValue{true};
    bool haveWaitStart{false};
    std::chrono::steady_clock::time_point waitStart{};
    std::uint64_t cyclesWaited{0};

    double kbusMsMin{std::numeric_limits<double>::infinity()};
    double kbusMsMax{0.0};
    double kbusMsSum{0.0};
    std::uint64_t kbusSampleCount{0};

    xentara::BooleanOutput kbusConnectedOut;
    xentara::DoubleOutput kbusCycleLastMsOut;
    xentara::DoubleOutput kbusCycleMinMsOut;
    xentara::DoubleOutput kbusCycleMaxMsOut;
    xentara::DoubleOutput kbusCycleAvgMsOut;
    xentara::UInt64Output kbusSampleCountOut;
};
