# SPDX-FileCopyrightText: 2011-2022 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

string(TIMESTAMP CURRENT_YEAR "%Y")

set(PROJECT_DESCRIPTION  "Blender is the free and open source 3D creation suite software.")
set(PROJECT_COPYRIGHT    "Copyright (C) 2001-${CURRENT_YEAR} Blender Authors")
set(PROJECT_CONTACT      "foundation@blender.org")
set(PROJECT_VENDOR       "Blender Foundation")

set(MAJOR_VERSION ${BLENDER_VERSION_MAJOR})
set(MINOR_VERSION ${BLENDER_VERSION_MINOR})
set(PATCH_VERSION ${BLENDER_VERSION_PATCH})

set(CPACK_SYSTEM_NAME ${CMAKE_SYSTEM_NAME})
set(CPACK_PACKAGE_DESCRIPTION ${PROJECT_DESCRIPTION})
set(CPACK_PACKAGE_VENDOR ${PROJECT_VENDOR})
set(CPACK_PACKAGE_CONTACT ${PROJECT_CONTACT})
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/COPYING")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
set(CPACK_PACKAGE_VERSION_MAJOR "${MAJOR_VERSION}")
set(CPACK_PACKAGE_VERSION_MINOR "${MINOR_VERSION}")
set(CPACK_PACKAGE_VERSION_PATCH "${PATCH_VERSION}")


# Get the build revision, note that this can get out-of-sync, so for packaging run cmake first.
set(MY_WC_HASH "unknown")
if(EXISTS ${CMAKE_SOURCE_DIR}/.git/)
  find_package(Git)
  if(GIT_FOUND)
    # message(STATUS "Found Git: ${GIT_EXECUTABLE}")
    execute_process(
      COMMAND git rev-parse --short=12 HEAD
      WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
      OUTPUT_VARIABLE MY_WC_HASH
      OUTPUT_STRIP_TRAILING_WHITESPACE
      ERROR_QUIET
    )
  endif()
endif()
set(BUILD_REV ${MY_WC_HASH})
unset(MY_WC_HASH)


# Force Package Name
execute_process(COMMAND date "+%Y%m%d" OUTPUT_VARIABLE CPACK_DATE OUTPUT_STRIP_TRAILING_WHITESPACE)
string(TOLOWER ${PROJECT_NAME} PROJECT_NAME_LOWER)
if(MSVC)
  if("${CMAKE_SIZEOF_VOID_P}" EQUAL "8")
    set(PACKAGE_ARCH windows64)
  else()
    set(PACKAGE_ARCH windows32)
  endif()
else()
  set(PACKAGE_ARCH ${CMAKE_SYSTEM_PROCESSOR})
endif()

if(CPACK_OVERRIDE_PACKAGENAME)
  set(CPACK_PACKAGE_FILE_NAME ${CPACK_OVERRIDE_PACKAGENAME}-${PACKAGE_ARCH})
else()
  set(CPACK_PACKAGE_FILE_NAME ${PROJECT_NAME_LOWER}-${MAJOR_VERSION}.${MINOR_VERSION}.${PATCH_VERSION}-git${CPACK_DATE}.${BUILD_REV}-${PACKAGE_ARCH})
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  # RPM packages
  include(build_files/cmake/RpmBuild.cmake)
  if(RPMBUILD_FOUND)
    set(CPACK_GENERATOR "RPM")
    set(CPACK_RPM_PACKAGE_RELEASE "git${CPACK_DATE}.${BUILD_REV}")
    set(CPACK_SET_DESTDIR "true")
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${PROJECT_DESCRIPTION}")
    set(CPACK_PACKAGE_RELOCATABLE "false")
    set(CPACK_RPM_PACKAGE_LICENSE "GPLv2+ and Apache 2.0")
    set(CPACK_RPM_PACKAGE_GROUP "Amusements/Multimedia")
    set(CPACK_RPM_USER_BINARY_SPECFILE "${CMAKE_SOURCE_DIR}/build_files/package_spec/rpm/blender.spec.in")
  endif()
endif()

# Mac Bundle
if(APPLE)
  set(CPACK_GENERATOR "DragNDrop")

  # Libraries are bundled directly
  set(CPACK_COMPONENT_LIBRARIES_HIDDEN TRUE)
endif()

if(WIN32)
  set(CPACK_PACKAGE_INSTALL_DIRECTORY "Blender Foundation/Blender ${MAJOR_VERSION}.${MINOR_VERSION}")
  set(CPACK_PACKAGE_INSTALL_REGISTRY_KEY "Blender Foundation/Blender ${MAJOR_VERSION}.${MINOR_VERSION}")

  set(CPACK_NSIS_MUI_ICON ${CMAKE_SOURCE_DIR}/release/windows/icons/winblender.ico)
  set(CPACK_NSIS_COMPRESSOR "/SOLID lzma")

  # Even though we no longer display this, we still need to set it otherwise it'll throw an error
  # during the msi build.
  set(CPACK_RESOURCE_FILE_LICENSE ${CMAKE_SOURCE_DIR}/release/license/spdx/GPL-3.0-or-later.txt)
  set(CPACK_WIX_PRODUCT_ICON ${CMAKE_SOURCE_DIR}/release/windows/icons/winblender.ico)

  set(BLENDER_NAMESPACE_GUID "507F933F-5898-404A-9A05-18282FD491A6")

  string(UUID CPACK_WIX_UPGRADE_GUID
    NAMESPACE ${BLENDER_NAMESPACE_GUID}
    NAME ${CPACK_PACKAGE_INSTALL_DIRECTORY}
    TYPE SHA1 UPPER
  )

  set(CPACK_WIX_TEMPLATE ${CMAKE_SOURCE_DIR}/release/windows/installer_wix/WIX.template)
  set(CPACK_WIX_UI_BANNER ${CMAKE_SOURCE_DIR}/release/windows/installer_wix/WIX_UI_BANNER.bmp)
  set(CPACK_WIX_UI_DIALOG ${CMAKE_SOURCE_DIR}/release/windows/installer_wix/WIX_UI_DIALOG.png)
  set(CPACK_WIX_EXTRA_SOURCES ${CMAKE_SOURCE_DIR}/release/windows/installer_wix/WixUI_Blender.wxs)
  set(CPACK_WIX_UI_REF "WixUI_Blender")
  set(CPACK_WIX_LIGHT_EXTRA_FLAGS -dcl:medium)
endif()

set(CPACK_PACKAGE_EXECUTABLES "blender-launcher" "Blender ${MAJOR_VERSION}.${MINOR_VERSION}")
set(CPACK_CREATE_DESKTOP_LINKS "blender-launcher" "Blender ${MAJOR_VERSION}.${MINOR_VERSION}")

include(CPack)

# Target for build_archive.py script, to automatically pass along
# version, revision, platform, build directory
macro(add_package_archive packagename extension)
  set(build_archive python ${CMAKE_SOURCE_DIR}/build_files/package_spec/build_archive.py)
  set(package_output ${CMAKE_BINARY_DIR}/release/${packagename}.${extension})

  add_custom_target(package_archive DEPENDS ${package_output})

  add_custom_command(
    OUTPUT ${package_output}
    COMMAND ${build_archive} ${packagename} ${extension} bin release
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
  )
  unset(build_archive)
  unset(package_output)
endmacro()

if(APPLE)
  add_package_archive(
    "${PROJECT_NAME}-${BLENDER_VERSION}-${BUILD_REV}-OSX-${CMAKE_OSX_ARCHITECTURES}"
    "zip"
  )
elseif(UNIX)
  # platform name could be tweaked, to include glibc, and ensure processor is correct (i386 vs i686)
  string(TOLOWER ${CMAKE_SYSTEM_NAME} PACKAGE_SYSTEM_NAME)

  add_package_archive(
    "${PROJECT_NAME}-${BLENDER_VERSION}-${BUILD_REV}-${PACKAGE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}"
    "tar.xz"
  )
endif()

unset(MAJOR_VERSION)
unset(MINOR_VERSION)
unset(PATCH_VERSION)

unset(BUILD_REV)
