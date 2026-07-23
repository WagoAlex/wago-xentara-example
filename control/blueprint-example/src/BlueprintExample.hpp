#pragma once

#include <cstdint>

#include <xentara/Input.hpp>
#include <xentara/Output.hpp>
#include <xentara/cpp-control.h>

// Minimal reference control for the "Blueprint" section of the top-level
// README - the smallest complete example of the initialize()/step() pattern,
// data point binding, and pipeline wiring every custom control follows.
//
// Deliberately touches no physical I/O, so it runs standalone: no EtherCAT
// bus, no coupler, no wiring. Open model/example-blueprint.json directly in
// Xentara Workbench or the TUI to see it - no discovery step needed.
class BlueprintExample final : public xentara::Control
{
public:
    void initialize(xentara::InitContext &context) override;

    void step(xentara::RunContext &context) override;

private:
    /// Counts steps while Enable is true - demonstrates state carried
    /// between step() calls.
    std::uint64_t counter{0};

    /// One input, two outputs: the two directions every control needs,
    /// bound to data points via initialize(). Names here are whatever this
    /// class chooses; the model's @Skill.CPP.Control "parameters" map them
    /// to actual data points (see model/example-blueprint.json).
    xentara::BooleanInput enable;
    xentara::UInt64Output counterOut;
    xentara::BooleanOutput activeOut;
};
