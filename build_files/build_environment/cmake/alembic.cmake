# SPDX-License-Identifier: GPL-2.0-or-later

set(ALEMBIC_EXTRA_ARGS
  -DBUILDSTATIC=ON
  -DLINKSTATIC=ON
  -DILMBASE_ROOT=${LIBDIR}/openexr
  -DALEMBIC_ILMBASE_INCLUDE_DIRECTORY=${LIBDIR}/openexr/include/OpenEXR
  -DALEMBIC_ILMBASE_HALF_LIB=${LIBDIR}/openexr/lib/${LIBPREFIX}Half${OPENEXR_VERSION_POSTFIX}${LIBEXT}
  -DALEMBIC_ILMBASE_IMATH_LIB=${LIBDIR}/openexr/lib/${LIBPREFIX}Imath${OPENEXR_VERSION_POSTFIX}${LIBEXT}
  -DALEMBIC_ILMBASE_ILMTHREAD_LIB=${LIBDIR}/openexr/lib/${LIBPREFIX}IlmThread${OPENEXR_VERSION_POSTFIX}${LIBEXT}
  -DALEMBIC_ILMBASE_IEX_LIB=${LIBDIR}/openexr/lib/${LIBPREFIX}Iex${OPENEXR_VERSION_POSTFIX}${LIBEXT}
  -DALEMBIC_ILMBASE_IEXMATH_LIB=${LIBDIR}/openexr/lib/${LIBPREFIX}IexMath${OPENEXR_VERSION_POSTFIX}${LIBEXT}
  -DUSE_PYILMBASE=0
  -DUSE_PYALEMBIC=0
  -DUSE_ARNOLD=0
  -DUSE_MAYA=0
  -DUSE_PRMAN=0
  -DUSE_HDF5=Off
  -DUSE_STATIC_HDF5=Off
  -DUSE_TESTS=Off
  -DALEMBIC_NO_OPENGL=1
  -DUSE_BINARIES=ON
  -DALEMBIC_ILMBASE_LINK_STATIC=On
  -DALEMBIC_SHARED_LIBS=OFF
  -DGLUT_INCLUDE_DIR=""
  -DZLIB_LIBRARY=${LIBDIR}/zlib/lib/${ZLIB_LIBRARY}
  -DZLIB_INCLUDE_DIR=${LIBDIR}/zlib/include/
)

ExternalProject_Add(external_alembic
  URL file://${PACKAGE_DIR}/${ALEMBIC_FILE}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  URL_HASH ${ALEMBIC_HASH_TYPE}=${ALEMBIC_HASH}
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
  external_zlib
  external_openexr
)
