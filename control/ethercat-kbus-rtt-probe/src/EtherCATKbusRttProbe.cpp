#include "EtherCATKbusRttProbe.hpp"

/// Registers EtherCATKbusRttProbe with Xentara.
const xentara::ControlExporter<EtherCATKbusRttProbe> kEtherCATKbusRttProbe;

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

    // Hardware DO->DI loopback round trip. Once the connectivity check has
    // permanently failed, stop touching Do/Di entirely.
    if (kbusConnectionFailed)
    {
        return;
    }

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
        if (cyclesWaited >= kStallCycles)
        {
            if (!kbusConnected)
            {
                // The very first round never completed: Do and Di are not
                // actually wired together (or not to each other). Latch
                // this permanently so KbusCycle* is never mistaken for a
                // real measurement.
                kbusConnectionFailed = true;
                kbusConnectedOut.write(context, false);
            }
        }
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
