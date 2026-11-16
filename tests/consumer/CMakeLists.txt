cmake_minimum_required(VERSION 3.20)
project(ECMConsumer LANGUAGES CXX)

find_package(ECM 0.2 CONFIG REQUIRED)
add_executable(ecm_consumer main.cpp)
target_link_libraries(ecm_consumer PRIVATE ECM::lab)

enable_testing()
add_test(NAME Package.RouteEnergy COMMAND ecm_consumer)

add_executable(ecm_bwsc_example ../../examples/bwsc/main.cpp)
target_link_libraries(ecm_bwsc_example PRIVATE ECM::lab)
add_test(NAME Package.BwscExample COMMAND ecm_bwsc_example)
set_tests_properties(Package.BwscExample PROPERTIES PASS_REGULAR_EXPRESSION "Reserve-first minus steady finish rate")
