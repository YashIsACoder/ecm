cmake_minimum_required(VERSION 3.20)
project(ECMConsumer LANGUAGES CXX)

find_package(ECM 0.2 CONFIG REQUIRED)
add_executable(ecm_consumer main.cpp)
target_link_libraries(ecm_consumer PRIVATE ECM::ecm_cont)

enable_testing()
add_test(NAME Package.RouteEnergy COMMAND ecm_consumer)
