# SPDX-FileCopyrightText: 2017-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

if(WIN32)
  set(OPENEXR_CMAKE_CXX_STANDARD_LIBRARIES "kernel32${LIBEXT} user32${LIBEXT} gdi32${LIBEXT} winspool${LIBEXT} shell32${LIBEXT} ole32${LIBEXT} oleaut32${LIBEXT} uuid${LIBEXT} comdlg32${LIBEXT} advapi32${LIBEXT} psapi${LIBEXT}")
  set(OPENEXR_EXTRA_ARGS
    -DCMAKE_CXX_STANDARD_LIBRARIES=${OPENEXR_CMAKE_CXX_STANDARD_LIBRARIES}
  )
  if(BUILD_MODE STREQUAL Release)
    list(APPEND OPENEXR_EXTRA_ARGS -DPYTHON_MODULE_EXTENSION=.pyd)
  else()
    list(APPEND OPENEXR_EXTRA_ARGS -DPYTHON_MODULE_EXTENSION=_d.pyd)
  endif()
  list(APPEND OPENEXR_EXTRA_ARGS -DPYTHON_LIBRARIES=${LIBDIR}/python/libs/python${PYTHON_SHORT_VERSION_NO_DOTS}${PYTHON_POSTFIX}.lib)
endif()

set(OPENEXR_EXTRA_ARGS
  ${OPENEXR_EXTRA_ARGS}

  -DBUILD_SHARED_LIBS=ON
  -DOPENEXR_BUILD_BOTH_STATIC_SHARED=OFF

  -DBUILD_TESTING=OFF
  -DOPENEXR_BUILD_PYTHON=ON
  -DOPENEXR_INSTALL_TOOLS=OFF
  -DOPENEXR_LIB_SUFFIX=${OPENEXR_VERSION_BUILD_POSTFIX}

  -DImath_DIR=${LIBDIR}/imath/lib/cmake/Imath
  -Dlibdeflate_DIR=${LIBDIR}/deflate/lib/cmake/libdeflate
  -Dopenjph_DIR=${LIBDIR}/openjph/lib/cmake/openjph
  -Dpybind11_ROOT=${LIBDIR}/pybind11
  -DPython3_EXECUTABLE=${PYTHON_BINARY}
)

ExternalProject_Add(external_openexr
  URL file://${PACKAGE_DIR}/${OPENEXR_FILE}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  URL_HASH ${OPENEXR_HASH_TYPE}=${OPENEXR_HASH}

  CMAKE_GENERATOR ${PLATFORM_ALT_GENERATOR}
  PREFIX ${BUILD_DIR}/openexr

  PATCH_COMMAND ${PATCH_CMD} -p 1 -d
    ${BUILD_DIR}/openexr/src/external_openexr <
    ${PATCH_DIR}/openexr_deflate_cmake.diff && 
    ${PATCH_CMD} -p 1 -d
    ${BUILD_DIR}/openexr/src/external_openexr <
    ${PATCH_DIR}/openexr_2594.diff &&
    ${PATCH_CMD} -p 1 -d 
    ${BUILD_DIR}/openexr/src/external_openexr <
    ${PATCH_DIR}/openexr_python_link.diff

  CMAKE_ARGS
    -DCMAKE_INSTALL_PREFIX=${LIBDIR}/openexr
    ${DEFAULT_CMAKE_FLAGS}
    ${OPENEXR_EXTRA_ARGS}

  INSTALL_DIR ${LIBDIR}/openexr
)

if(WIN32)
  ExternalProject_Add_Step(external_openexr after_install
    COMMAND ${CMAKE_COMMAND} -E copy_directory
      ${LIBDIR}/openexr/lib
      ${HARVEST_TARGET}/openexr/lib
    COMMAND ${CMAKE_COMMAND} -E copy_directory
      ${LIBDIR}/openexr/include
      ${HARVEST_TARGET}/openexr/include
    COMMAND ${CMAKE_COMMAND} -E copy
      ${LIBDIR}/openexr/bin/Iex${OPENEXR_VERSION_POSTFIX}.dll
      ${HARVEST_TARGET}/openexr/bin/Iex${OPENEXR_VERSION_POSTFIX}.dll
    COMMAND ${CMAKE_COMMAND} -E copy
      ${LIBDIR}/openexr/bin/IlmThread${OPENEXR_VERSION_POSTFIX}.dll
      ${HARVEST_TARGET}/openexr/bin/IlmThread${OPENEXR_VERSION_POSTFIX}.dll
    COMMAND ${CMAKE_COMMAND} -E copy
      ${LIBDIR}/openexr/bin/OpenEXRCore${OPENEXR_VERSION_POSTFIX}.dll
      ${HARVEST_TARGET}/openexr/bin/OpenEXRCore${OPENEXR_VERSION_POSTFIX}.dll
    COMMAND ${CMAKE_COMMAND} -E copy
      ${LIBDIR}/openexr/bin/OpenEXRUtil${OPENEXR_VERSION_POSTFIX}.dll
      ${HARVEST_TARGET}/openexr/bin/OpenEXRUtil${OPENEXR_VERSION_POSTFIX}.dll
    COMMAND ${CMAKE_COMMAND} -E copy
      ${LIBDIR}/openexr/bin/OpenEXR${OPENEXR_VERSION_POSTFIX}.dll
      ${HARVEST_TARGET}/openexr/bin/OpenEXR${OPENEXR_VERSION_POSTFIX}.dll
    COMMAND ${CMAKE_COMMAND} -E copy_directory
      ${LIBDIR}/openexr/python
      ${HARVEST_TARGET}/openexr/python
    DEPENDEES install
  )
else()
  harvest(external_openexr openexr/include openexr/include "*.h")
  harvest(external_openexr openexr/lib/cmake/OpenEXR openexr/lib/cmake/OpenEXR "*.cmake")
  harvest_rpath_lib(external_openexr openexr/lib openexr/lib "*${SHAREDLIBEXT}*")
  # The OpenEXR module expects to be installed directly in site-packages, not in a subdir.
  harvest_rpath_python(external_openexr
    openexr/python/OpenEXR
    python/lib/python${PYTHON_SHORT_VERSION}/site-packages
    "*"
  )
endif()

add_dependencies(
  external_openexr
  external_imath
  external_deflate
  external_openjph
  external_pybind11
)
