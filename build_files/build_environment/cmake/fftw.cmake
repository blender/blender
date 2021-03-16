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

set(FFTW_EXTRA_ARGS)

if(WIN32)
  set(FFTW3_PATCH_COMMAND ${PATCH_CMD} --verbose -p 0 -N -d ${BUILD_DIR}/fftw3/src/external_fftw3 < ${PATCH_DIR}/fftw3.diff)
  set(FFTW_EXTRA_ARGS --disable-static --enable-shared)
  set(FFTW_INSTALL install-strip)
else()
  set(FFTW_EXTRA_ARGS --enable-static)
  set(FFTW_INSTALL install)
endif()

ExternalProject_Add(external_fftw3
  URL file://${PACKAGE_DIR}/${FFTW_FILE}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  URL_HASH ${FFTW_HASH_TYPE}=${FFTW_HASH}
  PREFIX ${BUILD_DIR}/fftw3
  CONFIGURE_COMMAND ${CONFIGURE_ENV} && cd ${BUILD_DIR}/fftw3/src/external_fftw3/ && ${CONFIGURE_COMMAND} ${FFTW_EXTRA_ARGS} --prefix=${mingw_LIBDIR}/fftw3
  PATCH_COMMAND ${FFTW3_PATCH_COMMAND}
  BUILD_COMMAND ${CONFIGURE_ENV} && cd ${BUILD_DIR}/fftw3/src/external_fftw3/ && make -j${MAKE_THREADS}
  INSTALL_COMMAND ${CONFIGURE_ENV} && cd ${BUILD_DIR}/fftw3/src/external_fftw3/ && make ${FFTW_INSTALL}
  INSTALL_DIR ${LIBDIR}/fftw3
)

if(MSVC)
  set_target_properties(external_fftw3 PROPERTIES FOLDER Mingw)
  if(BUILD_MODE STREQUAL Release)
    ExternalProject_Add_Step(external_fftw3 after_install
      COMMAND ${CMAKE_COMMAND} -E copy ${LIBDIR}/fftw3/lib/libfftw3.dll.a ${HARVEST_TARGET}/fftw3/lib/libfftw.lib
      COMMAND ${CMAKE_COMMAND} -E copy ${LIBDIR}/fftw3/bin/libfftw3-3.dll ${HARVEST_TARGET}/fftw3/lib/libfftw3-3.dll
      COMMAND ${CMAKE_COMMAND} -E copy ${LIBDIR}/fftw3/include/fftw3.h ${HARVEST_TARGET}/fftw3/include/fftw3.h
      DEPENDEES install
    )
  endif()
endif()
