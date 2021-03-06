set(CPACK_PACKAGE_NAME "${CMAKE_PROJECT_NAME}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "OpenRW 'Open ReWrite' is an un-official open source recreation of the classic Grand Theft Auto III game executable")
set(CPACK_PACKAGE_VENDOR "openrw")

# FIXME: better description of the project
set(CPACK_PACKAGE_DESCRIPTION_FILE "${CMAKE_CURRENT_SOURCE_DIR}/COPYING")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/COPYING")
string(SUBSTRING "${GIT_SHA1}" 0 8 GIT_SHA1_SHORT)
set(CPACK_PACKAGE_VERSION "${GIT_SHA1_SHORT}")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "${CPACK_PACKAGE_NAME}")
set(CPACK_SOURCE_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${GIT_SHA1_SHORT}")

set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${GIT_SHA1_SHORT}")

# set(CPACK_PROJECT_CONFIG_FILE "${CMAKE_CURRENT_SOURCE_DIR}/CMakeCPackOptions.cmake")

include(CPack)
