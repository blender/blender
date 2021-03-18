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

if(BUILD_MODE STREQUAL Release)
  set(OPENAL_EXTRA_ARGS
    -DALSOFT_UTILS=OFF
    -DALSOFT_NO_CONFIG_UTIL=ON
    -DALSOFT_EXAMPLES=OFF
    -DALSOFT_TESTS=OFF
    -DALSOFT_CONFIG=OFF
    -DALSOFT_HRTF_DEFS=OFF
    -DALSOFT_INSTALL=ON
    -DALSOFT_BACKEND_SNDIO=OFF
  )

  if(UNIX)
    set(OPENAL_EXTRA_ARGS
      ${OPENAL_EXTRA_ARGS}
      -DLIBTYPE=STATIC
    )
  endif()

  if(UNIX AND NOT APPLE)
    # Ensure we have backends for playback.
    set(OPENAL_EXTRA_ARGS
      ${OPENAL_EXTRA_ARGS}
      -DALSOFT_REQUIRE_ALSA=ON
      -DALSOFT_REQUIRE_OSS=ON
      -DALSOFT_REQUIRE_PULSEAUDIO=ON
    )
  endif()

  ExternalProject_Add(external_openal
    URL file://${PACKAGE_DIR}/${OPENAL_FILE}
    DOWNLOAD_DIR ${DOWNLOAD_DIR}
    URL_HASH ${OPENAL_HASH_TYPE}=${OPENAL_HASH}
    PREFIX ${BUILD_DIR}/openal
    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${LIBDIR}/openal ${DEFAULT_CMAKE_FLAGS} ${OPENAL_EXTRA_ARGS}
    INSTALL_DIR ${LIBDIR}/openal
  )

  if(WIN32)
    ExternalProject_Add_Step(external_openal after_install
      COMMAND ${CMAKE_COMMAND} -E copy ${LIBDIR}/openal/lib/openal32.lib ${HARVEST_TARGET}/openal/lib/openal32.lib
      COMMAND ${CMAKE_COMMAND} -E copy ${LIBDIR}/openal/bin/openal32.dll ${HARVEST_TARGET}/openal/lib/openal32.dll
      COMMAND ${CMAKE_COMMAND} -E copy_directory ${LIBDIR}/openal/include/ ${HARVEST_TARGET}/openal/include/
      DEPENDEES install
    )
  endif()

endif()
