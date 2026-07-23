cmake_minimum_required(VERSION 3.25)

project(BlueprintExample LANGUAGES CXX)

# Find the required packages
find_package(XentaraCPPControl REQUIRED)

# Add the control source file as a module
add_library(
    BlueprintExample MODULE

    "src/BlueprintExample.cpp"
)

# Link to the Xentara library
target_link_libraries(
    BlueprintExample

    PRIVATE
        Xentara::cpp-control
)
