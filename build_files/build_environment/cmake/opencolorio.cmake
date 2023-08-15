# SPDX-FileCopyrightText: 2017-2022 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(OPENCOLORIO_EXTRA_ARGS
  -DOCIO_BUILD_APPS=OFF
  -DOCIO_BUILD_PYTHON=ON
  -DOCIO_BUILD_NUKE=OFF
  -DOCIO_BUILD_JAVA=OFF
  -DBUILD_SHARED_LIBS=ON
  -DOCIO_BUILD_DOCS=OFF
  -DOCIO_BUILD_TESTS=OFF
  -DOCIO_BUILD_GPU_TESTS=OFF
  -DOCIO_USE_SSE=ON

  -DOCIO_INSTALL_EXT_PACKAGES=NONE

  -Dexpat_ROOT=${LIBDIR}/expat
  -Dyaml-cpp_ROOT=${LIBDIR}/yamlcpp
  -Dyaml-cpp_VERSION=${YAMLCPP_VERSION}
  -Dpystring_ROOT=${LIBDIR}/pystring
  -DImath_ROOT=${LIBDIR}/imath
  -Dminizip-ng_ROOT=${LIBDIR}/minizipng
  -Dminizip-ng_INCLUDE_DIR=${LIBDIR}/minizipng/include
  -Dminizip-ng_LIBRARY=${LIBDIR}/minizipng/lib/libminizip${LIBEXT}
  -DZLIB_LIBRARY=${LIBDIR}/zlib/lib/${ZLIB_LIBRARY}
  -DZLIB_INCLUDE_DIR=${LIBDIR}/zlib/include/
  -DPython_EXECUTABLE=${PYTHON_BINARY}
  -Dpybind11_ROOT=${LIBDIR}/pybind11
)

if(APPLE)
  set(OPENCOLORIO_EXTRA_ARGS
    ${OPENCOLORIO_EXTRA_ARGS}
    "-DCMAKE_SHARED_LINKER_FLAGS=-liconv ${LIBDIR}/bzip2/lib/${LIBPREFIX}bz2${LIBEXT}"
  )
elseif(UNIX)
  set(OPENCOLORIO_EXTRA_ARGS
    ${OPENCOLORIO_EXTRA_ARGS}
    "-DCMAKE_SHARED_LINKER_FLAGS=${LIBDIR}/bzip2/lib/${LIBPREFIX}bz2${LIBEXT}"
  )
endif()

if(BLENDER_PLATFORM_ARM)
  set(OPENCOLORIO_EXTRA_ARGS
    ${OPENCOLORIO_EXTRA_ARGS}
    -DOCIO_USE_SSE=OFF
  )
endif()

if(WIN32)
  set(OPENCOLORIO_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DIMATH_DLL")
  if(BUILD_MODE STREQUAL Debug)
    set(OPENCOLORIO_CXX_FLAGS "${OPENCOLORIO_CXX_FLAGS} -DPy_DEBUG")
  endif()
  set(OPENCOLORIO_EXTRA_ARGS
    ${OPENCOLORIO_EXTRA_ARGS}
    -DCMAKE_DEBUG_POSTFIX=_d
    -Dexpat_LIBRARY=${LIBDIR}/expat/lib/libexpat$<$<STREQUAL:${BUILD_MODE},Debug>:d>MD${LIBEXT}
    -DImath_LIBRARY=${LIBDIR}/imath/lib/imath${OPENEXR_VERSION_POSTFIX}${LIBEXT}
    -DCMAKE_CXX_FLAGS=${OPENCOLORIO_CXX_FLAGS}
  )
else()
  set(OPENCOLORIO_EXTRA_ARGS
    ${OPENCOLORIO_EXTRA_ARGS}
  )
endif()

ExternalProject_Add(external_opencolorio
  URL file://${PACKAGE_DIR}/${OPENCOLORIO_FILE}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  URL_HASH ${OPENCOLORIO_HASH_TYPE}=${OPENCOLORIO_HASH}
  CMAKE_GENERATOR ${PLATFORM_ALT_GENERATOR}
  PREFIX ${BUILD_DIR}/opencolorio
  PATCH_COMMAND ${PATCH_CMD} -p 1 -N -d ${BUILD_DIR}/opencolorio/src/external_opencolorio < ${PATCH_DIR}/opencolorio.diff
  CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${LIBDIR}/opencolorio ${DEFAULT_CMAKE_FLAGS} ${OPENCOLORIO_EXTRA_ARGS}
  INSTALL_DIR ${LIBDIR}/opencolorio
)

add_dependencies(
  external_opencolorio
  external_yamlcpp
  external_expat
  external_imath
  external_pystring
  external_zlib
  external_minizipng
  external_python
  external_pybind11
)

if(WIN32)
  if(BUILD_MODE STREQUAL Release)
    ExternalProject_Add_Step(external_opencolorio after_install
      COMMAND ${CMAKE_COMMAND} -E copy_directory ${LIBDIR}/opencolorio/include ${HARVEST_TARGET}/opencolorio/include
      COMMAND ${CMAKE_COMMAND} -E copy ${LIBDIR}/opencolorio/bin/OpenColorIO_2_2.dll ${HARVEST_TARGET}/opencolorio/bin/OpenColorIO_2_2.dll
      COMMAND ${CMAKE_COMMAND} -E copy_directory ${LIBDIR}/opencolorio/lib ${HARVEST_TARGET}/opencolorio/lib
      DEPENDEES install
    )
  endif()
  if(BUILD_MODE STREQUAL Debug)
    ExternalProject_Add_Step(external_opencolorio after_install
      COMMAND ${CMAKE_COMMAND} -E copy ${LIBDIR}/opencolorio/bin/OpenColorIO_d_2_2.dll ${HARVEST_TARGET}/opencolorio/bin/OpenColorIO_d_2_2.dll
      COMMAND ${CMAKE_COMMAND} -E copy ${LIBDIR}/opencolorio/lib/Opencolorio_d.lib ${HARVEST_TARGET}/opencolorio/lib/OpenColorIO_d.lib
      COMMAND ${CMAKE_COMMAND} -E copy_directory ${LIBDIR}/opencolorio/lib/site-packages ${HARVEST_TARGET}/opencolorio/lib/site-packages-debug
      DEPENDEES install
    )
  endif()
else()
  ExternalProject_Add_Step(external_opencolorio after_install
    COMMAND cp ${LIBDIR}/yamlcpp/lib/libyaml-cpp.a ${LIBDIR}/opencolorio/lib/
    COMMAND cp ${LIBDIR}/expat/lib/libexpat.a ${LIBDIR}/opencolorio/lib/
    COMMAND cp ${LIBDIR}/pystring/lib/libpystring.a ${LIBDIR}/opencolorio/lib/
    DEPENDEES install
  )
endif()
