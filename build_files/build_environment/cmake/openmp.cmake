# SPDX-FileCopyrightText: 2002-2022 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

if(APPLE)
  set(OPENMP_PATCH_COMMAND
    ${PATCH_CMD} -p 1 -d
      ${BUILD_DIR}/openmp/src/external_openmp <
      ${PATCH_DIR}/openmp.diff
  )
else()
  set(OPENMP_PATCH_COMMAND)
endif()

ExternalProject_Add(external_openmp
  URL file://${PACKAGE_DIR}/${OPENMP_FILE}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  URL_HASH ${OPENMP_HASH_TYPE}=${OPENMP_HASH}
  PREFIX ${BUILD_DIR}/openmp
  PATCH_COMMAND ${OPENMP_PATCH_COMMAND}

  CMAKE_ARGS
    -DCMAKE_INSTALL_PREFIX=${LIBDIR}/openmp
    ${DEFAULT_CMAKE_FLAGS}

  INSTALL_COMMAND
    cd ${BUILD_DIR}/openmp/src/external_openmp-build &&
    install_name_tool -id @rpath/libomp.dylib runtime/src/libomp.dylib &&
    make install

  INSTALL_DIR ${LIBDIR}/openmp
)

add_dependencies(
  external_openmp
  ll
)
