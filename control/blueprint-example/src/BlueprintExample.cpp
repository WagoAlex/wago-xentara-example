#include "BlueprintExample.hpp"

/// Registers BlueprintExample with Xentara.
const xentara::ControlExporter<BlueprintExample> kBlueprintExample;

void BlueprintExample::initialize(xentara::InitContext &context)
{
    enable = context.config().getDataPoint("Enable");
    counterOut = context.config().getDataPoint("Counter");
    activeOut = context.config().getDataPoint("Active");
}

void BlueprintExample::step(xentara::RunContext &context)
{
    const bool isEnabled = enable.read(context);

    if (isEnabled)
    {
        ++counter;
    }

    counterOut.write(context, counter);
    activeOut.write(context, isEnabled);
}
