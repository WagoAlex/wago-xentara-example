#pragma once
 
#include <xentara/Input.hpp>
#include <xentara/Output.hpp>
#include <xentara/cpp-control.h>
 
class XentaraBasicControl final : public xentara::Control
{
public:
    void initialize(xentara::InitContext &context) override;
 
    void step(xentara::RunContext &context) override;
 
private:
    /// Xentara-managed input data points for demonstration
    xentara::StringInput payload;
 
    /// Xentara-managed output data point representing the result of adding input1 and input2
    xentara::BooleanOutput head;
    xentara::BooleanOutput white;
    xentara::BooleanOutput blue;
};