# SPDX-FileCopyrightText: 2017-2024 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

# Note the utility apps may use png/tiff/gif system libraries, but the
# library itself does not depend on them, so should give no problems.

get_filename_component(_hip_path ${HIP_HIPCC_EXECUTABLE} DIRECTORY)
get_filename_component(_hip_path ${_hip_path} DIRECTORY)

set(HIPRT_EXTRA_ARGS
  -DCMAKE_BUILD_TYPE=Release
  -DHIP_PATH=${_hip_path}
  -DBITCODE=ON
  -DGENERATE_BAKE_KERNEL=OFF
  -DNO_UNITTEST=ON
  -DHIPRT_PREFER_HIP_5=ON
)

set(HIPRT_SOURCE_DIR ${BUILD_DIR}/hiprt/src/external_hiprt)
set(HIPRT_BUILD_DIR ${BUILD_DIR}/hiprt/src/external_hiprt-build)

ExternalProject_Add(external_hiprt
  URL file://${PACKAGE_DIR}/${HIPRT_FILE}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  URL_HASH ${HIPRT_HASH_TYPE}=${HIPRT_HASH}
  CMAKE_GENERATOR ${PLATFORM_ALT_GENERATOR}
  PREFIX ${BUILD_DIR}/hiprt

  CMAKE_ARGS
    -DCMAKE_INSTALL_PREFIX=${LIBDIR}/hiprt
    ${HIPRT_EXTRA_ARGS}

  INSTALL_DIR ${LIBDIR}/hiprt
)

if(WIN32)
  # Strip version from shared library name.
  ExternalProject_Add_Step(external_hiprt after_install
    COMMAND ${CMAKE_COMMAND} -E rename
      ${LIBDIR}/hiprt/bin/hiprt${HIPRT_LIBRARY_VERSION}64.dll ${LIBDIR}/hiprt/bin/hiprt64.dll

    COMMAND ${CMAKE_COMMAND} -E copy_directory
      ${LIBDIR}/hiprt
      ${HARVEST_TARGET}/hiprt

    DEPENDEES install
  )
else()
  # Strip version from shared library name.
  ExternalProject_Add_Step(external_hiprt after_install
    COMMAND ${CMAKE_COMMAND} -E rename
      ${LIBDIR}/hiprt/bin/${LIBPREFIX}hiprt${HIPRT_LIBRARY_VERSION}64.so ${LIBDIR}/hiprt/bin/${LIBPREFIX}hiprt64.so


    DEPENDEES install
  )
  harvest(external_hiprt hiprt/include hiprt/include "*.h")
  harvest(external_hiprt hiprt/bin hiprt/lib "*${SHAREDLIBEXT}*")
endif()
