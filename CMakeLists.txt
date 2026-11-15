cmake_minimum_required(VERSION 3.20)

project(
    ECM
    VERSION 0.2.0
    LANGUAGES CXX
)

# =========================
# Global configuration
# =========================
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(ECM_BUILD_PYTHON "Build Python bindings" ON)
option(ECM_BUILD_TESTS  "Build C++ tests" ON)

# =========================
# Core ECM (DISCRETE)
# =========================
add_library(ecm STATIC
    src/forces.cpp
    src/efficiency.cpp
    src/efficiency_map.cpp
    src/ecm.cpp
    src/segment.cpp
)
add_library(ECM::ecm ALIAS ecm)

target_include_directories(ecm
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_compile_features(ecm PUBLIC cxx_std_20)

target_compile_options(ecm PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
    $<$<CXX_COMPILER_ID:AppleClang,Clang,GNU>:-Wall;-Wextra;-Wpedantic>
)

# =========================
# Continuous ECM (ODE-based)
# =========================
add_library(ecm_cont STATIC
    src/ecm_cont/integrator.cpp
    src/ecm_cont/dynamics.cpp
    src/ecm_cont/ecm_continuous.cpp
    src/ecm_cont/drive_cycle.cpp
    src/ecm_cont/route_energy.cpp
)
add_library(ECM::ecm_cont ALIAS ecm_cont)

target_include_directories(ecm_cont
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_compile_features(ecm_cont PUBLIC cxx_std_20)

target_compile_options(ecm_cont PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
    $<$<CXX_COMPILER_ID:AppleClang,Clang,GNU>:-Wall;-Wextra;-Wpedantic>
)

target_link_libraries(ecm_cont
    PUBLIC
        ecm
)

# =========================
# Python bindings
# =========================
if(ECM_BUILD_PYTHON)
    find_package(Python3 REQUIRED COMPONENTS Interpreter Development.Module)

    set(PYBIND11_FINDPYTHON ON)
    find_package(pybind11 3.0.1 CONFIG QUIET)
    if(NOT pybind11_FOUND)
        include(FetchContent)
        FetchContent_Declare(
            pybind11
            GIT_REPOSITORY https://github.com/pybind/pybind11.git
            GIT_TAG v3.0.1
            GIT_SHALLOW TRUE
        )
        FetchContent_MakeAvailable(pybind11)
    endif()

    set_target_properties(ecm ecm_cont PROPERTIES POSITION_INDEPENDENT_CODE ON)
    pybind11_add_module(ecm_py
        python/bindings.cpp
    )

    target_link_libraries(ecm_py
        PRIVATE
            ecm_cont
    )

    set_target_properties(ecm_py PROPERTIES
        OUTPUT_NAME ecm
    )
endif()

# =========================
# C++ Tests
# =========================
if(ECM_BUILD_TESTS)
    enable_testing()

    add_executable(ecm_test
        tests/test_ecm.cpp
    )

    target_link_libraries(ecm_test
        PRIVATE
            ecm_cont
    )

    add_test(NAME ECM.Basic COMMAND ecm_test)

    add_executable(ecm_segment_test tests/test_segment.cpp)
    target_link_libraries(ecm_segment_test PRIVATE ecm)
    add_test(NAME ECM.Segment COMMAND ecm_segment_test)

    add_executable(ecm_drive_cycle_test tests/test_drive_cycle.cpp)
    target_link_libraries(ecm_drive_cycle_test PRIVATE ecm_cont)
    add_test(NAME ECM.DriveCycle COMMAND ecm_drive_cycle_test)

    add_executable(ecm_route_energy_test tests/test_route_energy.cpp)
    target_link_libraries(ecm_route_energy_test PRIVATE ecm_cont)
    add_test(NAME ECM.RouteEnergy COMMAND ecm_route_energy_test)

    if(ECM_BUILD_PYTHON)
        foreach(test_name IN ITEMS test_ecm test_cont test_drive_cycle)
            add_test(NAME ECM.Python.${test_name}
                COMMAND "${CMAKE_COMMAND}" -E env "PYTHONPATH=$<TARGET_FILE_DIR:ecm_py>"
                "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/${test_name}.py")
        endforeach()
    endif()
endif()

# =========================
# Install
# =========================
include(CMakePackageConfigHelpers)
write_basic_package_version_file("${CMAKE_CURRENT_BINARY_DIR}/ECMConfigVersion.cmake"
    COMPATIBILITY SameMajorVersion)

install(TARGETS ecm ecm_cont
    EXPORT ECMTargets
    ARCHIVE DESTINATION lib
)

install(EXPORT ECMTargets
    FILE ECMConfig.cmake
    NAMESPACE ECM::
    DESTINATION lib/cmake/ECM
)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/ECMConfigVersion.cmake"
    DESTINATION lib/cmake/ECM
)

install(DIRECTORY include/
    DESTINATION include
)

if(ECM_BUILD_PYTHON)
    install(TARGETS ecm_py
        LIBRARY DESTINATION ecm_python
    )
endif()

