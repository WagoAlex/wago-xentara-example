#pragma once

#include <chrono>
#include <cstdint>
#include <limits>

#include <xentara/Output.hpp>
#include <xentara/cpp-control.h>

// Generic EtherCAT cycle-time (RTT) probe.
//
// Deliberately touches no I/O. Its only job each step() is to measure the
// interval since the previous step() and publish live statistics, so an
// operator can hand-edit every digital output and watch every input in the
// Xentara TUI without the control fighting them for those channels. Wire the
// probe's step() before the EtherCAT bus loop task in the pipeline; the loop
// still flushes whatever the TUI wrote to the outputs each cycle.
//
// What RTT means here: the step()-to-step() interval IS the real achieved
// control-cycle time of the pipeline - the rate at which the master exchanges
// process data with the coupler. It is NOT wire propagation delay (that is
// sub-microsecond on the K-Bus and not observable from here). Read these
// numbers as "how close is my actual cycle to the configured Timer period,
// and how much does it jitter", which is the honest thing this can measure.
class EtherCATRttProbe final : public xentara::Control
{
public:
    void initialize(xentara::InitContext &context) override;

    void step(xentara::RunContext &context) override;

private:
    /// Cycle-time measurement state: interval between successive step() calls.
    bool haveLastStepTime{false};
    std::chrono::steady_clock::time_point lastStepTime{};
    double rttMinMs{std::numeric_limits<double>::infinity()};
    double rttMaxMs{0.0};
    double rttSumMs{0.0};
    std::uint64_t rttSampleCount{0};

    /// Published statistics (bound to @Skill.SignalFlow.Register outputs, so
    /// they show up as live values in the TUI).
    xentara::DoubleOutput rttLastMs;
    xentara::DoubleOutput rttMinMsOut;
    xentara::DoubleOutput rttMaxMsOut;
    xentara::DoubleOutput rttAvgMsOut;
    xentara::UInt64Output rttSampleCountOut;
};
