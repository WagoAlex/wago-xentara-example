cmake_minimum_required(VERSION 3.25)

project(EtherCATKbusRttProbe LANGUAGES CXX)

find_package(XentaraCPPControl REQUIRED)

add_library(
    EtherCATKbusRttProbe MODULE

    "src/EtherCATKbusRttProbe.cpp"
)

target_link_libraries(
    EtherCATKbusRttProbe

    PRIVATE
        Xentara::cpp-control
)
