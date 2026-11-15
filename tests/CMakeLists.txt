add_executable(ecm_test cpp/test_ecm.cpp)
target_link_libraries(ecm_test PRIVATE ECM::ecm_cont)
add_test(NAME ECM.Basic COMMAND ecm_test)

add_executable(ecm_segment_test cpp/test_segment.cpp)
target_link_libraries(ecm_segment_test PRIVATE ECM::ecm)
add_test(NAME ECM.Segment COMMAND ecm_segment_test)

add_executable(ecm_drive_cycle_test cpp/test_drive_cycle.cpp)
target_link_libraries(ecm_drive_cycle_test PRIVATE ECM::ecm_cont)
add_test(NAME ECM.DriveCycle COMMAND ecm_drive_cycle_test)

add_executable(ecm_route_energy_test cpp/test_route_energy.cpp)
target_link_libraries(ecm_route_energy_test PRIVATE ECM::ecm_cont)
add_test(NAME ECM.RouteEnergy COMMAND ecm_route_energy_test)
