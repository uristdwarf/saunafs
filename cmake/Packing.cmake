set(CPACK_GENERATOR "DEB")
set(CPACK_PACKAGE_NAME "${PROJECT_NAME}"
        CACHE STRING "Name of package to be built"
)

set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "SaunaFS - A distributed filesystem for the cloud"
        CACHE STRING "Summary of package description"
)

set(CPACK_PACKAGE_CONTACT "contact@saunafs.com"
        CACHE STRING "Contact email for package"
)

set(CPACK_PACKAGE_VENDOR "The SaunaFS Team <${CPACK_PACKAGE_CONTACT}>"
        CACHE STRING "Vendor of package"
)

set(CPACK_DEBIAN_PACKAGE_MAINTAINER "${CPACK_PACKAGE_VENDOR}"
        CACHE STRING "Maintainer of package"
)

set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/COPYING")
set(CPACK_RESOURCE_FILE_README "${CMAKE_CURRENT_SOURCE_DIR}/README.md")

set(CPACK_VERBATIM_VARIABLES YES)

set(CPACK_PACKAGE_INSTALL_DIRECTORY ${CPACK_PACKAGE_NAME})
SET(CPACK_OUTPUT_FILE_PREFIX "${CMAKE_SOURCE_DIR}/_packages")
#set(CPACK_STRIP_FILES YES)

set(
    CPACK_INSTALL_DEFAULT_DIRECTORY_PERMISSIONS
    OWNER_READ OWNER_WRITE OWNER_EXECUTE
    GROUP_READ GROUP_EXECUTE
    WORLD_READ WORLD_EXECUTE
)

set(CPACK_PACKAGING_INSTALL_PREFIX ${CMAKE_INSTALL_PREFIX})

set(CPACK_PACKAGE_VERSION_MAJOR ${PACKAGE_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${PACKAGE_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${PACKAGE_VERSION_PATCH})

set(CPACK_COMPONENTS_GROUPING IGNORE)

set(CPACK_DEB_COMPONENT_INSTALL ON)
set(CPACK_DEBIAN_PACKAGE_VERSION "${PACKAGE_VERSION}")
set(CPACK_DEBIAN_FILE_NAME "DEB-DEFAULT")
set(CPACK_DEBIAN_COMPRESSION_TYPE "xz")
set(CPACK_DEBIAN_DEBUGINFO_PACKAGE ON)

set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "amd64")
## TODO(Baldor): Set dependencies for the package
#set(CPACK_DEBIAN_PACKAGE_DEPENDS "asciidoc, debhelper, cmake, libfuse3-dev, pkg-config, zlib1g-dev, libspdlog-dev, libfmt-dev, libboost-system-dev, libboost-program-options-dev, python3")
set(CPACK_DEBIAN_ENABLE_COMPONENT_DEPENDS ON)

# Remove the Unspecified package
get_cmake_property(CPACK_COMPONENTS_ALL COMPONENTS)
list(REMOVE_ITEM CPACK_COMPONENTS_ALL "Unspecified")

include(CPack)
message(STATUS "Components to pack: ${CPACK_COMPONENTS_ALL}")
