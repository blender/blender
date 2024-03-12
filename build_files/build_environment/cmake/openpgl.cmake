# SPDX-FileCopyrightText: 2022 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

# Note the utility apps may use png/tiff/gif system libraries, but the
# library itself does not depend on them, so should give no problems.

set(OPENPGL_EXTRA_ARGS
  -DOPENPGL_BUILD_STATIC=ON
  -DOPENPGL_TBB_ROOT=${LIBDIR}/tbb
  -DTBB_ROOT=${LIBDIR}/tbb
  -DCMAKE_DEBUG_POSTFIX=_d
)

if(TBB_STATIC_LIBRARY)
  set(OPENPGL_EXTRA_ARGS
    ${OPENPGL_EXTRA_ARGS}
    -DOPENPGL_TBB_COMPONENT=tbb_static
  )
endif()

ExternalProject_Add(external_openpgl
  URL file://${PACKAGE_DIR}/${OPENPGL_FILE}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  URL_HASH ${OPENPGL_HASH_TYPE}=${OPENPGL_HASH}
  PREFIX ${BUILD_DIR}/openpgl

  CMAKE_ARGS
    -DCMAKE_INSTALL_PREFIX=${LIBDIR}/openpgl
    ${DEFAULT_CMAKE_FLAGS}
    ${OPENPGL_EXTRA_ARGS}

  INSTALL_DIR ${LIBDIR}/openpgl
)

add_dependencies(
  external_openpgl
  external_tbb
)

if(WIN32)
  if(BUILD_MODE STREQUAL Release)
    ExternalProject_Add_Step(external_openpgl after_install
      COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${LIBDIR}/openpgl
        ${HARVEST_TARGET}/openpgl

      DEPENDEES install
    )
  else()
  ExternalProject_Add_Step(external_openpgl after_install
      COMMAND ${CMAKE_COMMAND} -E copy
        ${LIBDIR}/openpgl/lib/openpgl_d.lib
        ${HARVEST_TARGET}/openpgl/lib/openpgl_d.lib
      COMMAND ${CMAKE_COMMAND} -E copy
        ${LIBDIR}/openpgl/lib/cmake/openpgl-${OPENPGL_SHORT_VERSION}/openpgl_Exports-debug.cmake
        ${HARVEST_TARGET}/openpgl/lib/cmake/openpgl-${OPENPGL_SHORT_VERSION}/openpgl_Exports-debug.cmake

      DEPENDEES install
    )
  endif()
endif()
