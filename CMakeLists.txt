cmake_minimum_required(VERSION 3.20)
project(ECM_module LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---------------- Core C++ library ----------------
add_library(ecm STATIC
    src/forces.cpp
    src/efficiency.cpp
    src/ecm.cpp
)

target_include_directories(ecm
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# ---------------- Python bindings ----------------
option(ECM_BUILD_PYTHON "Build Python bindings" ON)

if(ECM_BUILD_PYTHON)
    # Find active Python
    find_package(Python3 REQUIRED COMPONENTS Interpreter Development)

    # Fetch pybind11 if not installed system-wide
    include(FetchContent)
    FetchContent_Declare(
        pybind11
        GIT_REPOSITORY https://github.com/pybind/pybind11.git
        GIT_TAG v2.11.1
    )
    FetchContent_MakeAvailable(pybind11)

    # Pybind module target
    pybind11_add_module(ecm_py python/bindings.cpp)

    target_link_libraries(ecm_py
        PRIVATE
            ecm
            Python3::Python
            pybind11::module
    )

    # Rename module to 'ecm' for Python import
    set_target_properties(ecm_py PROPERTIES OUTPUT_NAME ecm)
endif()

# ---------------- C++ Tests ----------------
add_executable(ecm_test
    tests/test_ecm.cpp
)

target_link_libraries(ecm_test
    PRIVATE
        ecm
)

# ---------------- Optional: install rules ----------------
install(TARGETS ecm_py LIBRARY DESTINATION ecm_python) # optional
install(TARGETS ecm ARCHIVE DESTINATION lib)           # optional

