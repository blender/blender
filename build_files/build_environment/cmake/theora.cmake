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

if(UNIX)
  set(THEORA_CONFIGURE_ENV ${CONFIGURE_ENV} && export HAVE_PDFLATEX=no)
else()
  set(THEORA_CONFIGURE_ENV ${CONFIGURE_ENV})
endif()

ExternalProject_Add(external_theora
  URL ${THEORA_URI}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  URL_HASH SHA256=${THEORA_HASH}
  PREFIX ${BUILD_DIR}/theora
  PATCH_COMMAND ${PATCH_CMD} -p 0 -d ${BUILD_DIR}/theora/src/external_theora < ${PATCH_DIR}/theora.diff
  CONFIGURE_COMMAND ${THEORA_CONFIGURE_ENV} && cd ${BUILD_DIR}/theora/src/external_theora/ && ${CONFIGURE_COMMAND} --prefix=${LIBDIR}/theora
    --disable-shared
    --enable-static
    --with-pic
    --with-ogg=${LIBDIR}/ogg
    --with-vorbis=${LIBDIR}/vorbis
    --disable-examples
  BUILD_COMMAND ${THEORA_CONFIGURE_ENV} && cd ${BUILD_DIR}/theora/src/external_theora/ && make -j${MAKE_THREADS}
  INSTALL_COMMAND ${THEORA_CONFIGURE_ENV} && cd ${BUILD_DIR}/theora/src/external_theora/ && make install
  INSTALL_DIR ${LIBDIR}/theora
)

add_dependencies(
  external_theora
  external_vorbis
  external_ogg
)

if(MSVC)
  set_target_properties(external_theora PROPERTIES FOLDER Mingw)
endif()
