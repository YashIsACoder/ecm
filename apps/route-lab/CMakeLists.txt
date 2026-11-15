add_library(ecm_route_lab STATIC src/race.cpp src/route.cpp src/scenario.cpp)
target_include_directories(ecm_route_lab PUBLIC include)
target_link_libraries(ecm_route_lab PUBLIC ECM::ecm_cont)
set_target_properties(ecm_route_lab PROPERTIES CXX_EXTENSIONS OFF)

set(default_route "${CMAKE_CURRENT_SOURCE_DIR}/data/outback-2025-style.csv")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${default_route}")
file(READ "${default_route}" ECM_DEFAULT_ROUTE_CSV)
configure_file(cmake/default_route.hpp.in generated/default_route.hpp @ONLY)

add_executable(solar-race src/main.cpp)
target_include_directories(solar-race PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")
target_link_libraries(solar-race PRIVATE ecm_route_lab)
set_target_properties(solar-race PROPERTIES
    CXX_EXTENSIONS OFF
    RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin"
)

install(TARGETS solar-race
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    COMPONENT route-lab
)
install(FILES "${default_route}" data/rolling-1150.csv
    DESTINATION "${CMAKE_INSTALL_DATADIR}/ecm/routes"
    COMPONENT route-lab
)

if(ECM_BUILD_TESTS)
    add_subdirectory(tests)
endif()
