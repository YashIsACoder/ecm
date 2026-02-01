cmake_minimum_required(VERSION 3.20)

project(
    ECM
    VERSION 0.1.0
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
    src/ecm.cpp
)

target_include_directories(ecm
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_compile_features(ecm PUBLIC cxx_std_20)

target_compile_options(ecm PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)

# =========================
# Continuous ECM (ODE-based)
# =========================
add_library(ecm_cont STATIC
    src/ecm_cont/integrator.cpp
    src/ecm_cont/dynamics.cpp
    src/ecm_cont/ecm_continuous.cpp
)

target_include_directories(ecm_cont
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_compile_features(ecm_cont PUBLIC cxx_std_20)

target_compile_options(ecm_cont PRIVATE
    -Wall
    -Wextra
    -Wpedantic
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

    include(FetchContent)
    FetchContent_Declare(
        pybind11
        GIT_REPOSITORY https://github.com/pybind/pybind11.git
        GIT_TAG v2.11.1
    )
    FetchContent_MakeAvailable(pybind11)

    pybind11_add_module(ecm_py
        python/bindings.cpp
    )

    target_link_libraries(ecm_py
        PRIVATE
            ecm
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
            ecm
            ecm_cont
    )

    add_test(NAME ECM.Basic COMMAND ecm_test)
endif()

# =========================
# Install
# =========================
install(TARGETS ecm ecm_cont
    EXPORT ECMTargets
    ARCHIVE DESTINATION lib
)

install(DIRECTORY include/
    DESTINATION include
)

if(ECM_BUILD_PYTHON)
    install(TARGETS ecm_py
        LIBRARY DESTINATION ecm_python
    )
endif()

