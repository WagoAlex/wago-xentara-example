#include "MQTTPayloadControl.hpp"

/// Registers the XentaraBasicControl with Xentara.
const xentara::ControlExporter<XentaraBasicControl> kXentaraControl;
 
void XentaraBasicControl::initialize(xentara::InitContext &context)
{
    // Initialize input and output data points using the context configuration
    head = context.config().getDataPoint("Head");
    white = context.config().getDataPoint("White");
    blue = context.config().getDataPoint("Blue");
    payload = context.config().getDataPoint("Payload");
}
 
void XentaraBasicControl::step(xentara::RunContext &context)
{
    // Read values from the Xentara-managed inputs
    auto payloadstring = payload.read(context);
    auto headFound = (payloadstring.find("head") != std::string::npos);
    head.write(context, headFound);
    auto whiteFound = (payloadstring.find("white helmet") != std::string::npos);
    white.write(context, whiteFound);
    auto blueFound = (payloadstring.find("blue helmet") != std::string::npos);
    blue.write(context, blueFound);
}
