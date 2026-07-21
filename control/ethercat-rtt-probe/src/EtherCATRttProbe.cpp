#include "EtherCATRttProbe.hpp"

/// Registers EtherCATRttProbe with Xentara.
const xentara::ControlExporter<EtherCATRttProbe> kEtherCATRttProbe;

void EtherCATRttProbe::initialize(xentara::InitContext &context)
{
    rttLastMs = context.config().getDataPoint("RttLastMs");
    rttMinMsOut = context.config().getDataPoint("RttMinMs");
    rttMaxMsOut = context.config().getDataPoint("RttMaxMs");
    rttAvgMsOut = context.config().getDataPoint("RttAvgMs");
    rttSampleCountOut = context.config().getDataPoint("RttSampleCount");
}

void EtherCATRttProbe::step(xentara::RunContext &context)
{
    const auto now = std::chrono::steady_clock::now();

    // First step has no previous timestamp to diff against, so it only seeds
    // lastStepTime. Every step after that publishes a real interval.
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
}
