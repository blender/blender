# SPDX-FileCopyrightText: 2017-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

if(WIN32)
  set(OPENEXR_CMAKE_CXX_STANDARD_LIBRARIES "kernel32${LIBEXT} user32${LIBEXT} gdi32${LIBEXT} winspool${LIBEXT} shell32${LIBEXT} ole32${LIBEXT} oleaut32${LIBEXT} uuid${LIBEXT} comdlg32${LIBEXT} advapi32${LIBEXT} psapi${LIBEXT}")
  set(OPENEXR_EXTRA_ARGS
    -DCMAKE_CXX_STANDARD_LIBRARIES=${OPENEXR_CMAKE_CXX_STANDARD_LIBRARIES}
  )
else()
  set(OPENEXR_EXTRA_ARGS
  )
endif()

set(OPENEXR_EXTRA_ARGS
  ${OPENEXR_EXTRA_ARGS}
  -DBUILD_TESTING=OFF
  -DOPENEXR_BUILD_BOTH_STATIC_SHARED=OFF
  -DBUILD_SHARED_LIBS=ON
  -DOPENEXR_INSTALL_TOOLS=OFF
  -DImath_DIR=${LIBDIR}/imath/lib/cmake/Imath
  -DOPENEXR_LIB_SUFFIX=${OPENEXR_VERSION_BUILD_POSTFIX}
  -Dlibdeflate_DIR=${LIBDIR}/deflate/lib/cmake/libdeflate
)

ExternalProject_Add(external_openexr
  URL file://${PACKAGE_DIR}/${OPENEXR_FILE}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  URL_HASH ${OPENEXR_HASH_TYPE}=${OPENEXR_HASH}

  CMAKE_GENERATOR ${PLATFORM_ALT_GENERATOR}
  PREFIX ${BUILD_DIR}/openexr

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

    DEPENDEES install
  )
else()
  harvest(external_openexr openexr/include openexr/include "*.h")
  harvest_rpath_lib(external_openexr openexr/lib openexr/lib "*${SHAREDLIBEXT}*")
endif()

add_dependencies(
  external_openexr
  external_imath
  external_deflate
)
