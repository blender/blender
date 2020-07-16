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

if(WIN32)
  set(X264_EXTRA_ARGS --enable-win32thread --cross-prefix=${MINGW_HOST}- --host=${MINGW_HOST})
endif()


if(APPLE)
  if("${CMAKE_OSX_ARCHITECTURES}" STREQUAL "arm64")
    set(X264_EXTRA_ARGS ${X264_EXTRA_ARGS} "--disable-asm")
    set(X264_CONFIGURE_ENV echo .)
  else()
    set(X264_CONFIGURE_ENV
      export AS=${LIBDIR}/nasm/bin/nasm
    )
  endif()
else()
  set(X264_CONFIGURE_ENV echo .)
endif()

ExternalProject_Add(external_x264
  URL ${X264_URI}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  URL_HASH SHA256=${X264_HASH}
  PREFIX ${BUILD_DIR}/x264
  CONFIGURE_COMMAND ${CONFIGURE_ENV} && ${X264_CONFIGURE_ENV} && cd ${BUILD_DIR}/x264/src/external_x264/ &&
    ${CONFIGURE_COMMAND} --prefix=${LIBDIR}/x264
    --enable-static
    --enable-pic
    --disable-lavf
    ${X264_EXTRA_ARGS}
  BUILD_COMMAND ${CONFIGURE_ENV} && cd ${BUILD_DIR}/x264/src/external_x264/ && make -j${MAKE_THREADS}
  INSTALL_COMMAND ${CONFIGURE_ENV} && cd ${BUILD_DIR}/x264/src/external_x264/ && make install
  INSTALL_DIR ${LIBDIR}/x264
)

if(MSVC)
  set_target_properties(external_x264 PROPERTIES FOLDER Mingw)
endif()

if(APPLE)
  add_dependencies(
    external_x264
    external_nasm
  )
endif()
