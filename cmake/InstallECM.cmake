include(CMakePackageConfigHelpers)

set(ecm_package_dir "${CMAKE_INSTALL_LIBDIR}/cmake/ECM")
configure_package_config_file(
    "${CMAKE_CURRENT_LIST_DIR}/ECMConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/ECMConfig.cmake"
    INSTALL_DESTINATION "${ecm_package_dir}"
)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/ECMConfigVersion.cmake"
    COMPATIBILITY SameMajorVersion
)

install(TARGETS ecm ecm_cont EXPORT ECMTargets
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}" COMPONENT development
)
install(EXPORT ECMTargets
    NAMESPACE ECM::
    DESTINATION "${ecm_package_dir}"
    COMPONENT development
)
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/ECMConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/ECMConfigVersion.cmake"
    DESTINATION "${ecm_package_dir}"
    COMPONENT development
)
install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/include/"
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
    COMPONENT development
)
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE.md"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/ecm"
    COMPONENT development
)
