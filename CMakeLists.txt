cmake_minimum_required(VERSION 3.20)
project(ECM VERSION 0.2.0 LANGUAGES CXX)

include(GNUInstallDirs)

set(ECM_STANDALONE OFF)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(ECM_STANDALONE ON)
endif()

option(ECM_BUILD_PYTHON "Build Python bindings" ${ECM_STANDALONE})
option(ECM_BUILD_ROUTE_LAB "Build the Sunswift route lab CLI" ${ECM_STANDALONE})
option(ECM_BUILD_TESTS "Build and register tests" ${ECM_STANDALONE})

if(ECM_BUILD_TESTS)
    enable_testing()
endif()

add_subdirectory(src)

if(ECM_BUILD_PYTHON)
    add_subdirectory(bindings/python)
endif()

if(ECM_BUILD_ROUTE_LAB)
    add_subdirectory(apps/route-lab)
endif()

if(ECM_BUILD_TESTS)
    add_subdirectory(tests)
endif()

include(cmake/InstallECM.cmake)
