# SPDX-FileCopyrightText: 2017-2022 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(OCIO_PATCH echo .)

set(OPENCOLORIO_EXTRA_ARGS
  -DOCIO_BUILD_APPS=OFF
  -DOCIO_BUILD_PYTHON=ON
  -DOCIO_BUILD_NUKE=OFF
  -DOCIO_BUILD_JAVA=OFF
  -DBUILD_SHARED_LIBS=ON
  -DOCIO_BUILD_DOCS=OFF
  -DOCIO_BUILD_TESTS=OFF
  -DOCIO_BUILD_GPU_TESTS=OFF
  -DOCIO_USE_SIMD=ON

  -DOCIO_INSTALL_EXT_PACKAGES=NONE

  -Dexpat_ROOT=${LIBDIR}/expat
  -Dyaml-cpp_ROOT=${LIBDIR}/yamlcpp
  -Dyaml-cpp_VERSION=${YAMLCPP_VERSION}
  -Dpystring_ROOT=${LIBDIR}/pystring
  -DImath_ROOT=${LIBDIR}/imath
  -Dminizip-ng_ROOT=${LIBDIR}/minizipng
  -Dminizip-ng_INCLUDE_DIR=${LIBDIR}/minizipng/include/minizip-ng
  -Dminizip-ng_LIBRARY=${LIBDIR}/minizipng/lib/libminizip${LIBEXT}
  -DZLIB_LIBRARY=${LIBDIR}/zlib/lib/${ZLIB_LIBRARY}
  -DZLIB_INCLUDE_DIR=${LIBDIR}/zlib/include/
  -DPython_EXECUTABLE=${PYTHON_BINARY}
  -Dpybind11_ROOT=${LIBDIR}/pybind11
)

if(APPLE)
  set(OPENCOLORIO_EXTRA_ARGS
    ${OPENCOLORIO_EXTRA_ARGS}
    # Work around issue where minizip-ng_LIBRARY assumes -ng in file name.
    -Dminizip_LIBRARY=${LIBDIR}/minizipng/lib/libminizip${LIBEXT}
    # Work around issue where homebrew Imath's can be prioritized over our own dependency during linking if installed.
    -DImath_LIBRARY=${LIBDIR}/imath/lib/libImath${SHAREDLIBEXT}
  )
endif()

if(BLENDER_PLATFORM_ARM AND NOT WIN32)
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
  PATCH_COMMAND ${OCIO_PATCH}

  CMAKE_ARGS
    -DCMAKE_INSTALL_PREFIX=${LIBDIR}/opencolorio
    ${DEFAULT_CMAKE_FLAGS}
    ${OPENCOLORIO_EXTRA_ARGS}

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
      COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${LIBDIR}/opencolorio/include
        ${HARVEST_TARGET}/opencolorio/include
      COMMAND ${CMAKE_COMMAND} -E copy
        ${LIBDIR}/opencolorio/bin/OpenColorIO_2_4.dll
        ${HARVEST_TARGET}/opencolorio/bin/OpenColorIO_2_4.dll
      COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${LIBDIR}/opencolorio/lib
        ${HARVEST_TARGET}/opencolorio/lib

      DEPENDEES install
    )
  endif()
  if(BUILD_MODE STREQUAL Debug)
    ExternalProject_Add_Step(external_opencolorio after_install
      COMMAND ${CMAKE_COMMAND} -E copy
        ${LIBDIR}/opencolorio/bin/OpenColorIO_d_2_4.dll
        ${HARVEST_TARGET}/opencolorio/bin/OpenColorIO_d_2_4.dll
      COMMAND ${CMAKE_COMMAND} -E copy
        ${LIBDIR}/opencolorio/lib/Opencolorio_d.lib
        ${HARVEST_TARGET}/opencolorio/lib/OpenColorIO_d.lib
      COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${LIBDIR}/opencolorio/lib/site-packages
        ${HARVEST_TARGET}/opencolorio/lib/site-packages-debug

      DEPENDEES install
    )
  endif()
else()
  ExternalProject_Add_Step(external_opencolorio after_install
    COMMAND cp
      ${LIBDIR}/yamlcpp/lib/libyaml-cpp.a
      ${LIBDIR}/opencolorio/lib/
    COMMAND cp
      ${LIBDIR}/expat/lib/libexpat.a
      ${LIBDIR}/opencolorio/lib/
    COMMAND cp
      ${LIBDIR}/pystring/lib/libpystring.a
      ${LIBDIR}/opencolorio/lib/

    DEPENDEES install
  )

  harvest(external_opencolorio opencolorio/include opencolorio/include "*.h")
  harvest_rpath_lib(external_opencolorio opencolorio/lib opencolorio/lib "*${SHAREDLIBEXT}*")
  harvest_rpath_python(
    external_opencolorio
    opencolorio/lib/python${PYTHON_SHORT_VERSION}
    python/lib/python${PYTHON_SHORT_VERSION}
    "*"
  )
endif()
