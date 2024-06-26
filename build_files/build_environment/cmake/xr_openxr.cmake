# SPDX-FileCopyrightText: 2020-2022 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later


# Keep flags in sync with install_deps.sh ones in compile_XR_OpenXR_SDK()
set(XR_OPENXR_SDK_EXTRA_ARGS
  -DBUILD_FORCE_GENERATION=OFF
  -DBUILD_LOADER=ON
  -DDYNAMIC_LOADER=OFF
)

if(UNIX AND NOT APPLE)
  list(APPEND XR_OPENXR_SDK_EXTRA_ARGS
    -DBUILD_WITH_WAYLAND_HEADERS=OFF
    -DBUILD_WITH_XCB_HEADERS=OFF
    -DBUILD_WITH_XLIB_HEADERS=ON
    -DBUILD_WITH_SYSTEM_JSONCPP=OFF
    -DCMAKE_CXX_FLAGS=-DDISABLE_STD_FILESYSTEM=1
  )
endif()

ExternalProject_Add(external_xr_openxr_sdk
  URL file://${PACKAGE_DIR}/${XR_OPENXR_SDK_FILE}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  URL_HASH ${XR_OPENXR_SDK_HASH_TYPE}=${XR_OPENXR_SDK_HASH}
  PREFIX ${BUILD_DIR}/xr_openxr_sdk

  CMAKE_ARGS
    -DCMAKE_INSTALL_PREFIX=${LIBDIR}/xr_openxr_sdk
    ${DEFAULT_CMAKE_FLAGS}
    ${XR_OPENXR_SDK_EXTRA_ARGS}

  INSTALL_DIR ${LIBDIR}/xr_openxr_sdk
)

if(WIN32)
  if(BUILD_MODE STREQUAL Release)
    ExternalProject_Add_Step(external_xr_openxr_sdk after_install
      COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${LIBDIR}/xr_openxr_sdk/include/openxr
        ${HARVEST_TARGET}/xr_openxr_sdk/include/openxr
      COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${LIBDIR}/xr_openxr_sdk/lib
        ${HARVEST_TARGET}/xr_openxr_sdk/lib

      DEPENDEES install
    )
  endif()
  if(BUILD_MODE STREQUAL Debug)
    ExternalProject_Add_Step(external_xr_openxr_sdk after_install
      COMMAND ${CMAKE_COMMAND} -E copy
        ${LIBDIR}/xr_openxr_sdk/lib/openxr_loaderd.lib
        ${HARVEST_TARGET}/xr_openxr_sdk/lib/openxr_loaderd.lib

      DEPENDEES install
    )
  endif()
else()
  harvest(external_xr_openxr_sdk xr_openxr_sdk/include/openxr xr_openxr_sdk/include/openxr "*.h")
  harvest(external_xr_openxr_sdk xr_openxr_sdk/lib xr_openxr_sdk/lib "*.a")
endif()
