# SPDX-FileCopyrightText: 2012-2022 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(POTRACE_EXTRA_ARGS
)

if((WIN32 AND BUILD_MODE STREQUAL Release) OR UNIX)
  ExternalProject_Add(external_potrace
    URL file://${PACKAGE_DIR}/${POTRACE_FILE}
    DOWNLOAD_DIR ${DOWNLOAD_DIR}
    URL_HASH ${POTRACE_HASH_TYPE}=${POTRACE_HASH}
    PREFIX ${BUILD_DIR}/potrace

    PATCH_COMMAND ${CMAKE_COMMAND} -E copy
      ${PATCH_DIR}/cmakelists_potrace.txt
      ${BUILD_DIR}/potrace/src/external_potrace/CMakeLists.txt

    CMAKE_ARGS
      -DCMAKE_INSTALL_PREFIX=${LIBDIR}/potrace
      ${DEFAULT_CMAKE_FLAGS}
      ${POTRACE_EXTRA_ARGS}

    INSTALL_DIR ${LIBDIR}/potrace
  )
  if(WIN32)
    ExternalProject_Add_Step(external_potrace after_install
      COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${LIBDIR}/potrace
        ${HARVEST_TARGET}/potrace

      DEPENDEES install
    )
  endif()
endif()
