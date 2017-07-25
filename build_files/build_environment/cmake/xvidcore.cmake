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
	set(XVIDCORE_EXTRA_ARGS --host=${MINGW_HOST})
endif()

ExternalProject_Add(external_xvidcore
	URL ${XVIDCORE_URI}
	DOWNLOAD_DIR ${DOWNLOAD_DIR}
	URL_HASH SHA256=${XVIDCORE_HASH}
	PREFIX ${BUILD_DIR}/xvidcore
	CONFIGURE_COMMAND ${CONFIGURE_ENV} && cd ${BUILD_DIR}/xvidcore/src/external_xvidcore/build/generic && ${CONFIGURE_COMMAND} --prefix=${LIBDIR}/xvidcore ${XVIDCORE_EXTRA_ARGS}
	BUILD_COMMAND ${CONFIGURE_ENV} && cd ${BUILD_DIR}/xvidcore/src/external_xvidcore/build/generic && make -j${MAKE_THREADS}
	INSTALL_COMMAND ${CONFIGURE_ENV} &&
	${CMAKE_COMMAND} -E remove ${LIBDIR}/xvidcore/lib/* && # clean because re-installing fails otherwise
	cd ${BUILD_DIR}/xvidcore/src/external_xvidcore/build/generic && make install
	INSTALL_DIR ${LIBDIR}/xvidcore
)

ExternalProject_Add_Step(external_xvidcore after_install
	COMMAND ${CMAKE_COMMAND} -E rename ${LIBDIR}/xvidcore/lib/xvidcore.a ${LIBDIR}/xvidcore/lib/libxvidcore.a || true
	COMMAND ${CMAKE_COMMAND} -E remove ${LIBDIR}/xvidcore/lib/xvidcore.dll.a
	DEPENDEES install
)

if(MSVC)
	set_target_properties(external_xvidcore PROPERTIES FOLDER Mingw)
endif()
