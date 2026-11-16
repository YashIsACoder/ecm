cmake_minimum_required(VERSION 3.20)
project(BwscSimulation LANGUAGES CXX)

find_package(ECM 0.2 CONFIG REQUIRED)
add_executable(bwsc-simulation main.cpp)
target_link_libraries(bwsc-simulation PRIVATE ECM::lab)
