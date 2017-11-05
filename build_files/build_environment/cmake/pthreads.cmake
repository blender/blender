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
		set(PTHREAD_XCFLAGS /MD)

		if(MSVC14) # vs2015 has timespec
			set(PTHREAD_CPPFLAGS "/I. /DHAVE_PTW32_CONFIG_H /D_TIMESPEC_DEFINED ")
		else() # everything before doesn't
			set(PTHREAD_CPPFLAGS "/I. /DHAVE_PTW32_CONFIG_H ")
		endif()

		set(PTHREADS_BUILD cd ${BUILD_DIR}/pthreads/src/external_pthreads/ && cd && nmake VC /e CPPFLAGS=${PTHREAD_CPPFLAGS} /e XCFLAGS=${PTHREAD_XCFLAGS} /e XLIBS=/NODEFAULTLIB:msvcr)

		ExternalProject_Add(external_pthreads
			URL ${PTHREADS_URI}
			DOWNLOAD_DIR ${DOWNLOAD_DIR}
			URL_HASH SHA512=${PTHREADS_SHA512}
			PREFIX ${BUILD_DIR}/pthreads
			CONFIGURE_COMMAND echo .
			PATCH_COMMAND ${PATCH_CMD} --verbose -p 0 -N -d ${BUILD_DIR}/pthreads/src/external_pthreads < ${PATCH_DIR}/pthreads.diff
			BUILD_COMMAND ${PTHREADS_BUILD}
			INSTALL_COMMAND COMMAND
				${CMAKE_COMMAND} -E copy ${BUILD_DIR}/pthreads/src/external_pthreads/pthreadVC2.dll ${LIBDIR}/pthreads/lib/pthreadVC2.dll &&
				${CMAKE_COMMAND} -E copy ${BUILD_DIR}/pthreads/src/external_pthreads/pthreadVC2${LIBEXT} ${LIBDIR}/pthreads/lib/pthreadVC2${LIBEXT} &&
				${CMAKE_COMMAND} -E copy ${BUILD_DIR}/pthreads/src/external_pthreads/pthread.h ${LIBDIR}/pthreads/inc/pthread.h &&
				${CMAKE_COMMAND} -E copy ${BUILD_DIR}/pthreads/src/external_pthreads/sched.h ${LIBDIR}/pthreads/inc/sched.h &&
				${CMAKE_COMMAND} -E copy ${BUILD_DIR}/pthreads/src/external_pthreads/semaphore.h ${LIBDIR}/pthreads/inc/semaphore.h
			INSTALL_DIR ${LIBDIR}/pthreads
		)
endif()
