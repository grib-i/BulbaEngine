if(UNIX AND NOT APPLE AND BLB_BUILD_APP)
  set_target_properties(${BLB_APP_NAME} PROPERTIES
    INSTALL_RPATH "$ORIGIN/../${CMAKE_INSTALL_LIBDIR}"
  )
endif()

if(UNIX AND NOT APPLE AND BLB_BUILD_SHARED)
  set_target_properties(bulba_engine PROPERTIES
    INSTALL_RPATH "$ORIGIN"
  )
endif()

if(WIN32)
  install(TARGETS bulba_engine
    EXPORT BulbaEngineTargets
    RUNTIME_DEPENDENCY_SET BulbaRuntimeDependencies
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    LIBRARY DESTINATION "${CMAKE_INSTALL_BINDIR}"
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
  )

  if(BLB_BUILD_APP)
    install(TARGETS ${BLB_APP_NAME}
      RUNTIME_DEPENDENCY_SET BulbaRuntimeDependencies
      RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    )
  endif()

  install(RUNTIME_DEPENDENCY_SET BulbaRuntimeDependencies
    DIRECTORIES
      "$ENV{VULKAN_SDK}/Bin"
      "$ENV{VULKAN_SDK}/Bin32"
    PRE_EXCLUDE_REGEXES
      "api-ms-win-.*"
      "ext-ms-win-.*"
      "^vulkan-1\\.dll$"
    POST_EXCLUDE_REGEXES
      ".*[Ww]indows[/\\\\][Ss]ystem32[/\\\\].*"
      ".*[Ww]indows[/\\\\][Ss]ys[Ww][Oo][Ww]64[/\\\\].*"
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
  )
else()
  install(TARGETS bulba_engine
    EXPORT BulbaEngineTargets
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
  )

  if(BLB_BUILD_APP)
    install(TARGETS ${BLB_APP_NAME}
      RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    )
  endif()
endif()

install(DIRECTORY "${BLB_SOURCE_DIR}/include/"
  DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)

if(EXISTS "${BLB_SOURCE_DIR}/src/bulba/core/utils/miniaudio.h")
  install(FILES
    "${BLB_SOURCE_DIR}/src/bulba/core/utils/miniaudio.h"
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/bulba/core/utils"
  )
endif()

if(BLB_BUILD_SHARED)
  configure_package_config_file(
    "${BLB_SOURCE_DIR}/cmake/BulbaEngineConfig.cmake.in"
    "${BLB_BINARY_DIR}/BulbaEngineConfig.cmake"
    INSTALL_DESTINATION "${BLB_CMAKE_INSTALL_DIR}"
  )

  write_basic_package_version_file(
    "${BLB_BINARY_DIR}/BulbaEngineConfigVersion.cmake"
    VERSION "${PROJECT_VERSION}"
    COMPATIBILITY SameMajorVersion
  )

  install(EXPORT BulbaEngineTargets
    FILE BulbaEngineTargets.cmake
    NAMESPACE BulbaEngine::
    DESTINATION "${BLB_CMAKE_INSTALL_DIR}"
  )

  install(FILES
    "${BLB_BINARY_DIR}/BulbaEngineConfig.cmake"
    "${BLB_BINARY_DIR}/BulbaEngineConfigVersion.cmake"
    DESTINATION "${BLB_CMAKE_INSTALL_DIR}"
  )
else()
  message(STATUS "Static archive installation has no package export; use BLB_BUILD_SHARED=ON for find_package(BulbaEngine)")
endif()

set(CPACK_PACKAGE_NAME "bulba-engine")
set(CPACK_PACKAGE_VENDOR "BulbaEngine")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "BulbaEngine and its shared library")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "BulbaEngine")
set(CPACK_PACKAGE_DIRECTORY "${BLB_BINARY_DIR}/packages")

if(WIN32)
  set(CPACK_GENERATOR "NSIS")
  set(CPACK_PACKAGE_FILE_NAME "BulbaEngine-${PROJECT_VERSION}-Windows-x64")
  set(CPACK_NSIS_DISPLAY_NAME "BulbaEngine ${PROJECT_VERSION}")
  set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
  set(CPACK_NSIS_MODIFY_PATH OFF)
elseif(UNIX AND NOT APPLE)
  set(CPACK_GENERATOR "DEB")
  set(CPACK_PACKAGE_FILE_NAME "bulba-engine-${PROJECT_VERSION}-Linux-x86_64")
  set(CPACK_PACKAGING_INSTALL_PREFIX "/usr")
  set(CPACK_DEBIAN_PACKAGE_MAINTAINER "BulbaEngine maintainers")
  set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
  set(CPACK_DEBIAN_PACKAGE_GENERATE_SHLIBS ON)
  set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA
    "${BLB_SOURCE_DIR}/packaging/linux/postinst;${BLB_SOURCE_DIR}/packaging/linux/postrm"
  )
endif()

include(CPack)
