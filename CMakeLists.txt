cmake_minimum_required(VERSION 3.20)
project(ECM_module LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

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
        ${CMAKE_CURRENT_SOURCE_DIR}/include
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
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(ecm_cont
    PUBLIC
        ecm
)

# =========================
# Python bindings
# =========================
option(ECM_BUILD_PYTHON "Build Python bindings" ON)

if(ECM_BUILD_PYTHON)
    find_package(Python3 REQUIRED COMPONENTS Interpreter Development)

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
            Python3::Python
            pybind11::module
    )

    set_target_properties(ecm_py PROPERTIES OUTPUT_NAME ecm)
endif()

# =========================
# C++ Tests
# =========================
add_executable(ecm_test
    tests/test_ecm.cpp
)

target_link_libraries(ecm_test
    PRIVATE
        ecm
        ecm_cont
)

# =========================
# Install (optional)
# =========================
install(TARGETS ecm ARCHIVE DESTINATION lib)
install(TARGETS ecm_cont ARCHIVE DESTINATION lib)

if(ECM_BUILD_PYTHON)
    install(TARGETS ecm_py LIBRARY DESTINATION ecm_python)
endif()

