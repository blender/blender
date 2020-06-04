# ***** BEGIN GPL LICENSE BLOCK *****
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software Foundation,
# Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# ***** END GPL LICENSE BLOCK *****

if(ALEMBIC_HDF5)
  set(ALEMBIC_HDF5_HL)
  # in debug mode we do not build HDF5_hdf5_hl_LIBRARY which makes cmake really
  # unhappy, stub it with the debug mode lib. it's not linking it in at this
  # point in time anyhow
  if(BUILD_MODE STREQUAL Debug)
    set(ALEMBIC_HDF5_HL -DHDF5_hdf5_hl_LIBRARY=${LIBDIR}/hdf5/lib/libhdf5_hl_D.${LIBEXT})
  endif()
endif()

set(ALEMBIC_EXTRA_ARGS
  -DBUILDSTATIC=ON
  -DLINKSTATIC=ON
  -DALEMBIC_LIB_USES_BOOST=ON
  -DBoost_COMPILER:STRING=${BOOST_COMPILER_STRING}
  -DBoost_USE_MULTITHREADED=ON
  -DUSE_STATIC_BOOST=On
  -DBoost_USE_STATIC_LIBS=ON
  -DBoost_USE_STATIC_RUNTIME=OFF
  -DBoost_DEBUG=ON
  -DBOOST_ROOT=${LIBDIR}/boost
  -DBoost_NO_SYSTEM_PATHS=ON
  -DBoost_NO_BOOST_CMAKE=ON
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
  -DHDF5_ROOT=${LIBDIR}/hdf5
  -DUSE_TESTS=Off
  -DALEMBIC_NO_OPENGL=1
  -DUSE_BINARIES=ON
  -DALEMBIC_ILMBASE_LINK_STATIC=On
  -DALEMBIC_SHARED_LIBS=OFF
  -DGLUT_INCLUDE_DIR=""
  -DZLIB_LIBRARY=${LIBDIR}/zlib/lib/${ZLIB_LIBRARY}
  -DZLIB_INCLUDE_DIR=${LIBDIR}/zlib/include/
  ${ALEMBIC_HDF5_HL}
)

ExternalProject_Add(external_alembic
  URL ${ALEMBIC_URI}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  URL_HASH MD5=${ALEMBIC_MD5}
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
  external_boost
  external_zlib
  external_openexr
)
