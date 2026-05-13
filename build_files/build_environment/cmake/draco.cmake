# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(DRACO_EXTRA_ARGS
  -DBUILD_SHARED_LIBS=ON
)

if(WIN32)
  set(DRACO_EXTRA_ARGS
    ${DRACO_EXTRA_ARGS}
    -DCMAKE_DEBUG_POSTFIX=_d
  )
endif()

ExternalProject_Add(external_draco
  URL file://${PACKAGE_DIR}/${DRACO_FILE}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  URL_HASH ${DRACO_HASH_TYPE}=${DRACO_HASH}
  CMAKE_GENERATOR ${PLATFORM_ALT_GENERATOR}
  PREFIX ${BUILD_DIR}/draco

  CMAKE_ARGS
  -DCMAKE_INSTALL_PREFIX=${LIBDIR}/draco
  ${DEFAULT_CMAKE_FLAGS}
  ${DRACO_EXTRA_ARGS}

  INSTALL_DIR ${LIBDIR}/draco
)

if(WIN32)
  ExternalProject_Add_Step(external_draco after_install
    COMMAND ${CMAKE_COMMAND} -E copy_directory
      ${LIBDIR}/draco/
      ${HARVEST_TARGET}/draco

    DEPENDEES install
  )
else()
  harvest(external_draco draco/include draco/include "*.h")
  # CMake files first because harvest_rpath_lib edits them. Move from share/cmake to lib/cmake for set_rpath.py to find them.
  harvest(external_draco draco/share/cmake/draco draco/lib/cmake/draco "*.cmake")
  harvest_rpath_lib(external_draco draco/lib draco/lib "*${SHAREDLIBEXT}*")
  # Draco unconditionally builds as a static library, harvest it to satisfy the CMake config target.
  harvest(external_draco draco/lib draco/lib "*.a")
endif()
