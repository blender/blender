# SPDX-FileCopyrightText: 2017-2022 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(ALEMBIC_EXTRA_ARGS
  -DImath_ROOT=${LIBDIR}/imath
  -DUSE_PYALEMBIC=OFF
  -DUSE_ARNOLD=OFF
  -DUSE_MAYA=OFF
  -DUSE_PRMAN=OFF
  -DUSE_HDF5=OFF
  -DUSE_TESTS=OFF
  -DUSE_BINARIES=ON
  -DALEMBIC_ILMBASE_LINK_STATIC=OFF
  -DALEMBIC_SHARED_LIBS=OFF
)

ExternalProject_Add(external_alembic
  URL file://${PACKAGE_DIR}/${ALEMBIC_FILE}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  URL_HASH ${ALEMBIC_HASH_TYPE}=${ALEMBIC_HASH}
  CMAKE_GENERATOR ${PLATFORM_ALT_GENERATOR}
  PREFIX ${BUILD_DIR}/alembic
  CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${LIBDIR}/alembic -Wno-dev ${DEFAULT_CMAKE_FLAGS} ${ALEMBIC_EXTRA_ARGS}
  INSTALL_DIR ${LIBDIR}/alembic
)

if(WIN32)
  if(BUILD_MODE STREQUAL Release)
    ExternalProject_Add_Step(external_alembic after_install
      COMMAND ${CMAKE_COMMAND} -E copy_directory ${LIBDIR}/alembic ${HARVEST_TARGET}/alembic
      DEPENDEES install
    )
  endif()
  if(BUILD_MODE STREQUAL Debug)
    ExternalProject_Add_Step(external_alembic after_install
      COMMAND ${CMAKE_COMMAND} -E copy ${LIBDIR}/alembic/lib/alembic.lib ${HARVEST_TARGET}/alembic/lib/alembic_d.lib
      DEPENDEES install
    )
  endif()
endif()



add_dependencies(
  external_alembic
  external_imath
)
