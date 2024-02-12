# SPDX-FileCopyrightText: 2022 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(PYSTRING_EXTRA_ARGS
)

ExternalProject_Add(external_pystring
  URL file://${PACKAGE_DIR}/${PYSTRING_FILE}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  URL_HASH ${PYSTRING_HASH_TYPE}=${PYSTRING_HASH}
  PREFIX ${BUILD_DIR}/pystring

  PATCH_COMMAND ${CMAKE_COMMAND} -E copy
    ${PATCH_DIR}/cmakelists_pystring.txt
    ${BUILD_DIR}/pystring/src/external_pystring/CMakeLists.txt

  CMAKE_ARGS
    -DCMAKE_INSTALL_PREFIX=${LIBDIR}/pystring
    ${DEFAULT_CMAKE_FLAGS}
    ${PYSTRING_EXTRA_ARGS}

  INSTALL_DIR ${LIBDIR}/pystring
)

if(WIN32)
  ExternalProject_Add_Step(external_pystring after_install
    COMMAND ${CMAKE_COMMAND} -E copy_directory
      ${LIBDIR}/pystring/lib
      ${HARVEST_TARGET}/pystring/lib
    COMMAND ${CMAKE_COMMAND} -E copy_directory
      ${LIBDIR}/pystring/include
      ${HARVEST_TARGET}/pystring/include

    DEPENDEES install
  )
endif()
