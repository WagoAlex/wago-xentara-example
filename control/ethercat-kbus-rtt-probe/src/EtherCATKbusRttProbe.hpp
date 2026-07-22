#pragma once

#include <chrono>
#include <cstdint>
#include <limits>

#include <xentara/Input.hpp>
#include <xentara/Output.hpp>
#include <xentara/cpp-control.h>

// Same step()-to-step() cycle-time measurement as EtherCATRttProbe (see that
// control's header for what it does and doesn't measure), plus two more
// hardware-verified round-trip measurements over physical loopbacks:
//
//   Digital: Do -> Di          (DO_8ch_8 -> DI_8ch_1 in this deployment)
//   Analog:  Ao -> Ai          (AO_1 -> AI_1 in this deployment)
//
// "Verified" matters for both. The very first write is not trusted as a
// real measurement, it is a connectivity check: KbusConnected/
// AnalogConnected only ever flips from false to true, on the first cycle
// where the corresponding input actually reads back the written value.
// There is deliberately no timeout that gives up and latches it false -
// a K-Bus resync after a restart can legitimately take several seconds,
// and a fixed cutoff would risk reporting "not connected" on wiring that
// is actually fine, just slow to come up. Until the first real match,
// the *Cycle*/Analog* registers simply stay at zero.
//
// The analog measurement runs two alternating round trips on the same
// Ao/Ai pair, to see whether an instantaneous step and a gradual ramp reach
// the far end differently:
//   Step:     Ao jumps 0 -> 32760 in one cycle. Timer starts the instant
//             that write happens.
//   Gradual:  Ao then ramps 32760 -> 0 linearly, kAnalogRampStepPerCycle
//             per cycle. The timer starts only once the ramp's commanded
//             value reaches 0, i.e. this measures settling time after the
//             ramp ends, the same quantity as the step case, just preceded
//             by a gradual approach instead of an instant jump.
// Both timers stop once Ai is within kAnalogTolerance counts of the target,
// which exists because a real DAC->wire->ADC loopback essentially never
// reads back bit-exact, unlike a digital loopback.
//
// All of this is bounded by the coupler's K-Bus scan cycle, not by
// EtherCAT frame rate or wire propagation (sub-microsecond, not observable
// here) - see the wago-xentara-example README for measurements taken on
// real hardware.
class EtherCATKbusRttProbe final : public xentara::Control
{
public:
    void initialize(xentara::InitContext &context) override;

    void step(xentara::RunContext &context) override;

private:
    static constexpr std::uint16_t kAnalogHigh{32760};
    static constexpr std::uint16_t kAnalogLow{0};
    // Calibrated against this hardware: a real DAC->wire->ADC loopback has
    // a steady-state noise/offset floor around 20-25 counts, so the
    // tolerance must clear that, and the ramp step must clear the
    // tolerance so a gradual change can't trivially satisfy it mid-ramp.
    static constexpr std::uint16_t kAnalogTolerance{40};
    static constexpr std::uint16_t kAnalogRampStepPerCycle{100};

    /// Bundles the min/max/avg/sample-count bookkeeping and the matching
    /// output data points for one round-trip measurement. Used for the
    /// analog step and gradual measurements; the digital Kbus measurement
    /// predates this and keeps its own hand-written fields below.
    struct RoundTripStats
    {
        double msMin{std::numeric_limits<double>::infinity()};
        double msMax{0.0};
        double msSum{0.0};
        std::uint64_t sampleCount{0};

        xentara::DoubleOutput msLastOut;
        xentara::DoubleOutput msMinOut;
        xentara::DoubleOutput msMaxOut;
        xentara::DoubleOutput msAvgOut;
        xentara::UInt64Output sampleCountOut;

        void record(xentara::RunContext &context, double elapsedMs);
    };

    void stepDigitalKbus(xentara::RunContext &context, std::chrono::steady_clock::time_point now);
    void stepAnalog(xentara::RunContext &context, std::chrono::steady_clock::time_point now);

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

    /// Digital DO->DI loopback round-trip state.
    xentara::BooleanOutput doOut;
    xentara::BooleanInput diIn;

    bool kbusConnected{false};
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

    /// Analog AO->AI loopback round-trip state.
    xentara::UInt16Output aoOut;
    xentara::UInt16Input aiIn;

    enum class AnalogPhase
    {
        StepUp,
        GradualDown,
    };

    bool analogConnected{false};
    AnalogPhase analogPhase{AnalogPhase::StepUp};
    bool analogPhaseJustEntered{true};
    std::uint16_t analogCommand{kAnalogLow};
    std::chrono::steady_clock::time_point analogWaitStart{};
    std::uint64_t analogCyclesWaited{0};

    xentara::BooleanOutput analogConnectedOut;
    RoundTripStats analogStep;
    RoundTripStats analogGradual;
};
