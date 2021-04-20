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

set(BOOST_ADDRESS_MODEL 64)

if(BLENDER_PLATFORM_ARM)
  set(BOOST_ARCHITECTURE arm)
else()
  set(BOOST_ARCHITECTURE x86)
endif()

if(WIN32)
  set(BOOST_TOOLSET toolset=msvc-14.1)
  set(BOOST_COMPILER_STRING -vc141)

  set(BOOST_CONFIGURE_COMMAND bootstrap.bat)
  set(BOOST_BUILD_COMMAND b2)
  set(BOOST_BUILD_OPTIONS runtime-link=shared )
  set(BOOST_HARVEST_CMD   ${CMAKE_COMMAND} -E copy_directory ${LIBDIR}/boost/lib/ ${HARVEST_TARGET}/boost/lib/ )
  if(BUILD_MODE STREQUAL Release)
    set(BOOST_HARVEST_CMD ${BOOST_HARVEST_CMD} && ${CMAKE_COMMAND} -E copy_directory ${LIBDIR}/boost/include/boost-${BOOST_VERSION_NODOTS_SHORT}/ ${HARVEST_TARGET}/boost/include/)
  endif()
elseif(APPLE)
  set(BOOST_CONFIGURE_COMMAND ./bootstrap.sh)
  set(BOOST_BUILD_COMMAND ./b2)
  set(BOOST_BUILD_OPTIONS toolset=clang-darwin cxxflags=${PLATFORM_CXXFLAGS} linkflags=${PLATFORM_LDFLAGS} visibility=global --disable-icu boost.locale.icu=off)
  set(BOOST_HARVEST_CMD echo .)
  set(BOOST_PATCH_COMMAND echo .)
else()
  set(BOOST_HARVEST_CMD echo .)
  set(BOOST_CONFIGURE_COMMAND ./bootstrap.sh)
  set(BOOST_BUILD_COMMAND ./b2)
  set(BOOST_BUILD_OPTIONS cxxflags=${PLATFORM_CXXFLAGS} --disable-icu boost.locale.icu=off)
  set(BOOST_PATCH_COMMAND echo .)
  if("${CMAKE_SIZEOF_VOID_P}" EQUAL "8")
    set(BOOST_ADDRESS_MODEL 64)
  else()
    set(BOOST_ADDRESS_MODEL 32)
  endif()
endif()

if(WITH_BOOST_PYTHON)
  set(JAM_FILE ${BUILD_DIR}/boost.user-config.jam)
  configure_file(${PATCH_DIR}/boost.user.jam.in ${JAM_FILE})
  set(BOOST_PYTHON_OPTIONS
    --with-python
    --user-config=${JAM_FILE}
  )
  if(WIN32 AND BUILD_MODE STREQUAL Debug)
    set(BOOST_PYTHON_OPTIONS
      ${BOOST_PYTHON_OPTIONS}
      define=BOOST_DEBUG_PYTHON
    )
  endif()
endif()

set(BOOST_OPTIONS
  --with-filesystem
  --with-locale
  --with-thread
  --with-regex
  --with-system
  --with-date_time
  --with-wave
  --with-atomic
  --with-serialization
  --with-program_options
  --with-iostreams
  -sNO_BZIP2=1
  -sNO_LZMA=1
  -sNO_ZSTD=1
  ${BOOST_TOOLSET}
  ${BOOST_PYTHON_OPTIONS}
)

string(TOLOWER ${BUILD_MODE} BOOST_BUILD_TYPE)

ExternalProject_Add(external_boost
  URL file://${PACKAGE_DIR}/${BOOST_FILE}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  URL_HASH ${BOOST_HASH_TYPE}=${BOOST_HASH}
  PREFIX ${BUILD_DIR}/boost
  UPDATE_COMMAND  ""
  PATCH_COMMAND ${BOOST_PATCH_COMMAND}
  CONFIGURE_COMMAND ${BOOST_CONFIGURE_COMMAND}
  BUILD_COMMAND ${BOOST_BUILD_COMMAND} ${BOOST_BUILD_OPTIONS} -j${MAKE_THREADS} architecture=${BOOST_ARCHITECTURE} address-model=${BOOST_ADDRESS_MODEL} link=static threading=multi ${BOOST_OPTIONS}    --prefix=${LIBDIR}/boost install
  BUILD_IN_SOURCE 1
  INSTALL_COMMAND "${BOOST_HARVEST_CMD}"
)

if(WITH_BOOST_PYTHON)
  add_dependencies(
    external_boost
    external_python
    external_numpy
  )
endif()
