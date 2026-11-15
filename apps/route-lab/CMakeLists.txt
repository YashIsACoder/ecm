add_executable(solar-race src/main.cpp)
target_link_libraries(solar-race PRIVATE ECM::lab)
set_target_properties(solar-race PROPERTIES
    CXX_EXTENSIONS OFF
    RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin"
)

install(TARGETS solar-race
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    COMPONENT route-lab
)
install(FILES "${PROJECT_SOURCE_DIR}/data/routes/outback-2025-style.csv"
              "${PROJECT_SOURCE_DIR}/data/routes/rolling-1150.csv"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/ecm/routes"
    COMPONENT route-lab
)

if(ECM_BUILD_TESTS)
    add_subdirectory(tests)
endif()
