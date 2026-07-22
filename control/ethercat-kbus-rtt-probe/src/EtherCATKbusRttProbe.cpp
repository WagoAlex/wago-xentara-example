#include "EtherCATKbusRttProbe.hpp"

/// Registers EtherCATKbusRttProbe with Xentara.
const xentara::ControlExporter<EtherCATKbusRttProbe> kEtherCATKbusRttProbe;

void EtherCATKbusRttProbe::RoundTripStats::record(xentara::RunContext &context, double elapsedMs)
{
    msSum += elapsedMs;
    ++sampleCount;

    msLastOut.write(context, elapsedMs);
    msAvgOut.write(context, msSum / static_cast<double>(sampleCount));
    sampleCountOut.write(context, sampleCount);

    if (elapsedMs < msMin)
    {
        msMin = elapsedMs;
        msMinOut.write(context, msMin);
    }
    if (elapsedMs > msMax)
    {
        msMax = elapsedMs;
        msMaxOut.write(context, msMax);
    }
}

void EtherCATKbusRttProbe::initialize(xentara::InitContext &context)
{
    rttLastMs = context.config().getDataPoint("RttLastMs");
    rttMinMsOut = context.config().getDataPoint("RttMinMs");
    rttMaxMsOut = context.config().getDataPoint("RttMaxMs");
    rttAvgMsOut = context.config().getDataPoint("RttAvgMs");
    rttSampleCountOut = context.config().getDataPoint("RttSampleCount");

    doOut = context.config().getDataPoint("Do");
    diIn = context.config().getDataPoint("Di");

    kbusConnectedOut = context.config().getDataPoint("KbusConnected");
    kbusCycleLastMsOut = context.config().getDataPoint("KbusCycleLastMs");
    kbusCycleMinMsOut = context.config().getDataPoint("KbusCycleMinMs");
    kbusCycleMaxMsOut = context.config().getDataPoint("KbusCycleMaxMs");
    kbusCycleAvgMsOut = context.config().getDataPoint("KbusCycleAvgMs");
    kbusSampleCountOut = context.config().getDataPoint("KbusSampleCount");

    aoOut = context.config().getDataPoint("Ao");
    aiIn = context.config().getDataPoint("Ai");

    analogConnectedOut = context.config().getDataPoint("AnalogConnected");
    analogStep.msLastOut = context.config().getDataPoint("AnalogStepLastMs");
    analogStep.msMinOut = context.config().getDataPoint("AnalogStepMinMs");
    analogStep.msMaxOut = context.config().getDataPoint("AnalogStepMaxMs");
    analogStep.msAvgOut = context.config().getDataPoint("AnalogStepAvgMs");
    analogStep.sampleCountOut = context.config().getDataPoint("AnalogStepSampleCount");
    analogGradual.msLastOut = context.config().getDataPoint("AnalogGradualLastMs");
    analogGradual.msMinOut = context.config().getDataPoint("AnalogGradualMinMs");
    analogGradual.msMaxOut = context.config().getDataPoint("AnalogGradualMaxMs");
    analogGradual.msAvgOut = context.config().getDataPoint("AnalogGradualAvgMs");
    analogGradual.sampleCountOut = context.config().getDataPoint("AnalogGradualSampleCount");
}

void EtherCATKbusRttProbe::step(xentara::RunContext &context)
{
    const auto now = std::chrono::steady_clock::now();

    // Cycle-time (step-to-step interval) measurement - identical to
    // EtherCATRttProbe. First step only seeds lastStepTime.
    if (haveLastStepTime)
    {
        const auto elapsedMs = std::chrono::duration<double, std::milli>(now - lastStepTime).count();

        rttLastMs.write(context, elapsedMs);

        rttSumMs += elapsedMs;
        ++rttSampleCount;
        rttAvgMsOut.write(context, rttSumMs / static_cast<double>(rttSampleCount));
        rttSampleCountOut.write(context, rttSampleCount);

        if (elapsedMs < rttMinMs)
        {
            rttMinMs = elapsedMs;
            rttMinMsOut.write(context, rttMinMs);
        }
        if (elapsedMs > rttMaxMs)
        {
            rttMaxMs = elapsedMs;
            rttMaxMsOut.write(context, rttMaxMs);
        }
    }
    lastStepTime = now;
    haveLastStepTime = true;

    stepDigitalKbus(context, now);
    stepAnalog(context, now);
}

void EtherCATKbusRttProbe::stepDigitalKbus(xentara::RunContext &context, std::chrono::steady_clock::time_point now)
{
    doOut.write(context, targetValue);

    if (!haveWaitStart)
    {
        waitStart = now;
        haveWaitStart = true;
        cyclesWaited = 0;
        return;
    }

    ++cyclesWaited;

    const auto observed = diIn.read(context);
    if (observed != targetValue)
    {
        // Keep waiting - do not give up. A quiet startup transient (K-Bus
        // resync after a restart) can legitimately take several seconds,
        // longer than any fixed cycle threshold would tolerate without
        // risking a false "not connected" on wiring that is actually fine.
        // Connected only ever goes from false to true, on the first real
        // match, never the other way based on a timeout.
        return;
    }

    if (!kbusConnected)
    {
        kbusConnected = true;
        kbusConnectedOut.write(context, true);
    }

    const auto elapsedMs = std::chrono::duration<double, std::milli>(now - waitStart).count();

    kbusMsSum += elapsedMs;
    ++kbusSampleCount;

    kbusCycleLastMsOut.write(context, elapsedMs);
    kbusCycleAvgMsOut.write(context, kbusMsSum / static_cast<double>(kbusSampleCount));
    kbusSampleCountOut.write(context, kbusSampleCount);

    if (elapsedMs < kbusMsMin)
    {
        kbusMsMin = elapsedMs;
        kbusCycleMinMsOut.write(context, kbusMsMin);
    }
    if (elapsedMs > kbusMsMax)
    {
        kbusMsMax = elapsedMs;
        kbusCycleMaxMsOut.write(context, kbusMsMax);
    }

    // Flip target and start the next round.
    targetValue = !targetValue;
    waitStart = now;
    cyclesWaited = 0;
}

void EtherCATKbusRttProbe::stepAnalog(xentara::RunContext &context, std::chrono::steady_clock::time_point now)
{
    if (analogPhase == AnalogPhase::StepUp)
    {
        // Instant step: command the high value every cycle in this phase.
        analogCommand = kAnalogHigh;
        aoOut.write(context, analogCommand);

        if (analogPhaseJustEntered)
        {
            analogWaitStart = now;
            analogCyclesWaited = 0;
            analogPhaseJustEntered = false;
            return;
        }

        ++analogCyclesWaited;

        const auto observed = aiIn.read(context);
        const auto diff = observed > analogCommand ? observed - analogCommand : analogCommand - observed;
        if (diff > kAnalogTolerance)
        {
            // Keep waiting - see the comment in stepDigitalKbus for why
            // this never gives up on a fixed cycle threshold.
            return;
        }

        if (!analogConnected)
        {
            analogConnected = true;
            analogConnectedOut.write(context, true);
        }

        const auto elapsedMs = std::chrono::duration<double, std::milli>(now - analogWaitStart).count();
        analogStep.record(context, elapsedMs);

        // Step measurement done - start the gradual ramp down.
        analogPhase = AnalogPhase::GradualDown;
        analogPhaseJustEntered = true;
        return;
    }

    // AnalogPhase::GradualDown
    if (analogCommand > kAnalogLow)
    {
        analogCommand = (analogCommand > kAnalogRampStepPerCycle)
                            ? static_cast<std::uint16_t>(analogCommand - kAnalogRampStepPerCycle)
                            : kAnalogLow;
        aoOut.write(context, analogCommand);
        return;
    }

    // The ramp's commanded value has reached kAnalogLow; measure settling
    // time from here, the same quantity as the step case, just preceded by
    // a gradual approach instead of an instant jump.
    aoOut.write(context, analogCommand);

    if (analogPhaseJustEntered)
    {
        analogWaitStart = now;
        analogCyclesWaited = 0;
        analogPhaseJustEntered = false;
        return;
    }

    ++analogCyclesWaited;

    const auto observed = aiIn.read(context);
    const auto diff = observed > analogCommand ? observed - analogCommand : analogCommand - observed;
    if (diff > kAnalogTolerance)
    {
        return;
    }

    const auto elapsedMs = std::chrono::duration<double, std::milli>(now - analogWaitStart).count();
    analogGradual.record(context, elapsedMs);

    // Gradual measurement done - start the next step-up round.
    analogPhase = AnalogPhase::StepUp;
    analogPhaseJustEntered = true;
}
