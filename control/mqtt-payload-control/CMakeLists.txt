cmake_minimum_required(VERSION 3.25)
 
project(MQTTPayloadControl LANGUAGES CXX)
 
# Find the required packages
find_package(XentaraCPPControl REQUIRED)
 
# Add the control source file as a module
add_library(
    MQTTPayloadControl MODULE
 
    "src/MQTTPayloadControl.cpp"
)
 
# Link to the Xentara library
target_link_libraries(
    MQTTPayloadControl
 
    PRIVATE
        Xentara::cpp-control
)