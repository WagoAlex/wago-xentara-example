cmake_minimum_required(VERSION 3.25)

project(EtherCATRttProbe LANGUAGES CXX)

# Find the required packages
find_package(XentaraCPPControl REQUIRED)

# Add the control source file as a module
add_library(
    EtherCATRttProbe MODULE

    "src/EtherCATRttProbe.cpp"
)

# Link to the Xentara library
target_link_libraries(
    EtherCATRttProbe

    PRIVATE
        Xentara::cpp-control
)
