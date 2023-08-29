# SPDX-FileCopyrightText: 2019-2022 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later


set(OIDN_EXTRA_ARGS
  -DOIDN_APPS=OFF
  -DTBB_ROOT=${LIBDIR}/tbb
  -DTBB_STATIC_LIB=${TBB_STATIC_LIBRARY}
  -DOIDN_STATIC_LIB=ON
  -DOIDN_STATIC_RUNTIME=OFF
  -DISPC_EXECUTABLE=${LIBDIR}/ispc/bin/ispc
  -DOIDN_FILTER_RTLIGHTMAP=OFF
  -DPYTHON_EXECUTABLE=${PYTHON_BINARY}
)

if(WIN32)
  set(OIDN_EXTRA_ARGS
    ${OIDN_EXTRA_ARGS}
    -DTBB_DEBUG_LIBRARY=${LIBDIR}/tbb/lib/tbb.lib
    -DTBB_DEBUG_LIBRARY_MALLOC=${LIBDIR}/tbb/lib/tbbmalloc.lib
  )
else()
  set(OIDN_EXTRA_ARGS
    ${OIDN_EXTRA_ARGS}
    -Dtbb_LIBRARY_RELEASE=${LIBDIR}/tbb/lib/tbb_static.a
    -Dtbbmalloc_LIBRARY_RELEASE=${LIBDIR}/tbb/lib/tbbmalloc_static.a
  )
endif()

ExternalProject_Add(external_openimagedenoise
  URL file://${PACKAGE_DIR}/${OIDN_FILE}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  URL_HASH ${OIDN_HASH_TYPE}=${OIDN_HASH}
  PREFIX ${BUILD_DIR}/openimagedenoise
  CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${LIBDIR}/openimagedenoise ${DEFAULT_CMAKE_FLAGS} ${OIDN_EXTRA_ARGS}
  INSTALL_DIR ${LIBDIR}/openimagedenoise
)

add_dependencies(
  external_openimagedenoise
  external_tbb
  external_ispc
  external_python
)

if(WIN32)
  if(BUILD_MODE STREQUAL Release)
    ExternalProject_Add_Step(external_openimagedenoise after_install
      COMMAND ${CMAKE_COMMAND} -E copy_directory ${LIBDIR}/openimagedenoise/include ${HARVEST_TARGET}/openimagedenoise/include
      COMMAND ${CMAKE_COMMAND} -E copy ${LIBDIR}/openimagedenoise/lib/openimagedenoise.lib ${HARVEST_TARGET}/openimagedenoise/lib/openimagedenoise.lib
      COMMAND ${CMAKE_COMMAND} -E copy ${LIBDIR}/openimagedenoise/lib/common.lib ${HARVEST_TARGET}/openimagedenoise/lib/common.lib
      COMMAND ${CMAKE_COMMAND} -E copy ${LIBDIR}/openimagedenoise/lib/dnnl.lib ${HARVEST_TARGET}/openimagedenoise/lib/dnnl.lib
      DEPENDEES install
    )
  endif()
  if(BUILD_MODE STREQUAL Debug)
    ExternalProject_Add_Step(external_openimagedenoise after_install
      COMMAND ${CMAKE_COMMAND} -E copy ${LIBDIR}/openimagedenoise/lib/openimagedenoise.lib ${HARVEST_TARGET}/openimagedenoise/lib/openimagedenoise_d.lib
      COMMAND ${CMAKE_COMMAND} -E copy ${LIBDIR}/openimagedenoise/lib/common.lib ${HARVEST_TARGET}/openimagedenoise/lib/common_d.lib
      COMMAND ${CMAKE_COMMAND} -E copy ${LIBDIR}/openimagedenoise/lib/dnnl.lib ${HARVEST_TARGET}/openimagedenoise/lib/dnnl_d.lib
      DEPENDEES install
    )
  endif()
endif()
