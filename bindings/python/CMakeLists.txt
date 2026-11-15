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
pybind11_add_module(ecm_py module.cpp)
target_link_libraries(ecm_py PRIVATE ECM::ecm_cont)
set_target_properties(ecm_py PROPERTIES OUTPUT_NAME ecm CXX_EXTENSIONS OFF)

set(ECM_PYTHON_INSTALL_DIR "ecm_python" CACHE STRING "Install directory for the Python extension")
install(TARGETS ecm_py
    LIBRARY DESTINATION "${ECM_PYTHON_INSTALL_DIR}" COMPONENT python
    RUNTIME DESTINATION "${ECM_PYTHON_INSTALL_DIR}" COMPONENT python
)

if(ECM_BUILD_TESTS)
    foreach(test IN ITEMS test_ecm test_cont test_drive_cycle)
        add_test(NAME ECM.Python.${test}
            COMMAND "${CMAKE_COMMAND}" -E env "PYTHONPATH=$<TARGET_FILE_DIR:ecm_py>"
                    "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tests/python/${test}.py")
    endforeach()
endif()
