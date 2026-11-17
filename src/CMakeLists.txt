add_library(ecm STATIC
    core/ecm.cpp
    core/efficiency.cpp
    core/efficiency_map.cpp
    core/forces.cpp
    core/segment.cpp
)
add_library(ECM::ecm ALIAS ecm)

add_library(ecm_cont STATIC
    continuous/drive_cycle.cpp
    continuous/dynamics.cpp
    continuous/ecm_continuous.cpp
    continuous/integrator.cpp
    continuous/route_energy.cpp
)
add_library(ECM::ecm_cont ALIAS ecm_cont)
target_link_libraries(ecm_cont PUBLIC ECM::ecm)

set(default_route "${PROJECT_SOURCE_DIR}/data/routes/outback-2025-style.csv")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${default_route}")
file(READ "${default_route}" ECM_DEFAULT_ROUTE_CSV)
configure_file(lab/default_route.hpp.in generated/default_route.hpp @ONLY)

add_library(ecm_lab STATIC
    lab/bwsc.cpp
    lab/experiment.cpp
    lab/fleet.cpp
    lab/forecast.cpp
    lab/race.cpp
    lab/route.cpp
    lab/scenario.cpp
    lab/solar.cpp
    lab/statistics.cpp
)
add_library(ECM::lab ALIAS ecm_lab)
set_target_properties(ecm_lab PROPERTIES EXPORT_NAME lab)
target_include_directories(ecm_lab PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")
target_link_libraries(ecm_lab PUBLIC ECM::ecm_cont)

foreach(target IN ITEMS ecm ecm_cont ecm_lab)
    target_include_directories(${target}
        PUBLIC
            $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
        PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}"
    )
    target_compile_features(${target} PUBLIC cxx_std_20)
    set_target_properties(${target} PROPERTIES CXX_EXTENSIONS OFF)
    target_compile_options(${target} PRIVATE
        "$<$<CXX_COMPILER_ID:MSVC>:/W4>"
        "$<$<CXX_COMPILER_ID:AppleClang,Clang,GNU>:-Wall;-Wextra;-Wpedantic>"
    )
endforeach()
