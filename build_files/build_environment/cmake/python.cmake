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

set(PYTHON_POSTFIX)
if(BUILD_MODE STREQUAL Debug)
	set(PYTHON_POSTFIX _d)
endif()

if(WIN32)
	if("${CMAKE_SIZEOF_VOID_P}" EQUAL "8")
		set(SSL_POSTFIX -x64)
	else()
		set(SSL_POSTFIX)
	endif()

	set(PYTHON_BINARY ${BUILD_DIR}/python/src/external_python/run/python${PYTHON_POSTFIX}.exe)

	macro(cmake_to_dos_path MsysPath ResultingPath)
		string(REPLACE "/" "\\" ${ResultingPath} "${MsysPath}")
	endmacro()

	set(PYTHON_EXTERNALS_FOLDER ${BUILD_DIR}/python/src/external_python/externals)
	set(DOWNLOADS_EXTERNALS_FOLDER ${DOWNLOAD_DIR}/externals)

	cmake_to_dos_path(${PYTHON_EXTERNALS_FOLDER} PYTHON_EXTERNALS_FOLDER_DOS)
	cmake_to_dos_path(${DOWNLOADS_EXTERNALS_FOLDER} DOWNLOADS_EXTERNALS_FOLDER_DOS)

	message("Python externals = ${PYTHON_EXTERNALS_FOLDER}")
	message("Python externals_dos = ${PYTHON_EXTERNALS_FOLDER_DOS}")
	message("Python DOWNLOADS_EXTERNALS_FOLDER = ${DOWNLOADS_EXTERNALS_FOLDER}")
	message("Python DOWNLOADS_EXTERNALS_FOLDER_DOS = ${DOWNLOADS_EXTERNALS_FOLDER_DOS}")

	ExternalProject_Add(external_python
		URL ${PYTHON_URI}
		DOWNLOAD_DIR ${DOWNLOAD_DIR}
		URL_HASH MD5=${PYTHON_HASH}
		PREFIX ${BUILD_DIR}/python
		PATCH_COMMAND
			echo mklink /D "${PYTHON_EXTERNALS_FOLDER_DOS}" "${DOWNLOADS_EXTERNALS_FOLDER_DOS}" &&
			mklink /D "${PYTHON_EXTERNALS_FOLDER_DOS}" "${DOWNLOADS_EXTERNALS_FOLDER_DOS}"
		CONFIGURE_COMMAND ""
		BUILD_COMMAND cd ${BUILD_DIR}/python/src/external_python/pcbuild/ && set IncludeTkinter=false && call build.bat -e -p ${PYTHON_ARCH} -c ${BUILD_MODE}
		INSTALL_COMMAND COMMAND
			${CMAKE_COMMAND} -E copy ${PYTHON_OUTPUTDIR}/python${PYTHON_SHORT_VERSION_NO_DOTS}${PYTHON_POSTFIX}.dll ${LIBDIR}/python/lib/python${PYTHON_SHORT_VERSION_NO_DOTS}${PYTHON_POSTFIX}.dll &&
			${CMAKE_COMMAND} -E copy ${PYTHON_OUTPUTDIR}/python${PYTHON_SHORT_VERSION_NO_DOTS}${PYTHON_POSTFIX}.pdb ${LIBDIR}/python/lib/python${PYTHON_SHORT_VERSION_NO_DOTS}${PYTHON_POSTFIX}.pdb &&
			${CMAKE_COMMAND} -E copy ${PYTHON_OUTPUTDIR}/python${PYTHON_SHORT_VERSION_NO_DOTS}${PYTHON_POSTFIX}.lib ${LIBDIR}/python/lib/python${PYTHON_SHORT_VERSION_NO_DOTS}${PYTHON_POSTFIX}.lib &&
			${CMAKE_COMMAND} -E copy ${PYTHON_OUTPUTDIR}/python${PYTHON_SHORT_VERSION_NO_DOTS}${PYTHON_POSTFIX}.exp ${LIBDIR}/python/lib/python${PYTHON_SHORT_VERSION_NO_DOTS}${PYTHON_POSTFIX}.exp &&
			${CMAKE_COMMAND} -E copy_directory ${BUILD_DIR}/python/src/external_python/include ${LIBDIR}/python/include/Python${PYTHON_SHORT_VERSION} &&
			${CMAKE_COMMAND} -E copy "${BUILD_DIR}/python/src/external_python/PC/pyconfig.h" ${LIBDIR}/python/include/Python${PYTHON_SHORT_VERSION}/pyconfig.h
	)
	message("PythinRedist = ${BUILD_DIR}/python/src/external_python/redist")
	message("POutput = ${PYTHON_OUTPUTDIR}")
else()
	if(APPLE)
		# we need to add homebrew pkgconfig directories to get ssl, xz

                set(BREW_PKG_CONFIG "/usr/local/opt/openssl/lib/pkgconfig:/usr/local/opt/xz/lib/pkgconfig")
		# disable functions that can be in 10.13 sdk but aren't available on 10.9 target
		set(PYTHON_FUNC_CONFIGS
		  export ac_cv_func_futimens=no &&
		  export ac_cv_func_utimensat=no &&
		  export ac_cv_func_basename_r=no &&
		  export ac_cv_func_clock_getres=no &&
		  export ac_cv_func_clock_gettime=no &&
		  export ac_cv_func_clock_settime=no &&
		  export ac_cv_func_dirname_r=no &&
		  export ac_cv_func_getentropy=no &&
		  export ac_cv_func_mkostemp=no &&
		  export ac_cv_func_mkostemps=no &&
		  export ac_cv_func_timingsafe_bcmp=no)

		set(PYTHON_CONFIGURE_ENV ${CONFIGURE_ENV} && export PKG_CONFIG_PATH=${BREW_PKG_CONFIG} && ${PYTHON_FUNC_CONFIGS})
		set(PYTHON_BINARY ${BUILD_DIR}/python/src/external_python/python.exe)
		#set(PYTHON_PATCH ${PATCH_CMD} --verbose -p1 -d ${BUILD_DIR}/python/src/external_python < ${PATCH_DIR}/python_apple.diff)
		set(PYTHON_PATCH echo .)
	else()
		set(PYTHON_CONFIGURE_ENV ${CONFIGURE_ENV})
		set(PYTHON_BINARY ${BUILD_DIR}/python/src/external_python/python)
	endif()

	ExternalProject_Add(external_python
		URL ${PYTHON_URI}
		DOWNLOAD_DIR ${DOWNLOAD_DIR}
		URL_HASH MD5=${PYTHON_HASH}
		PREFIX ${BUILD_DIR}/python
		PATCH_COMMAND ${PYTHON_PATCH}
		CONFIGURE_COMMAND ${PYTHON_CONFIGURE_ENV} && cd ${BUILD_DIR}/python/src/external_python/ && ${CONFIGURE_COMMAND} --prefix=${LIBDIR}/python
		BUILD_COMMAND ${PYTHON_CONFIGURE_ENV} && cd ${BUILD_DIR}/python/src/external_python/ && make -j${MAKE_THREADS}
		INSTALL_COMMAND ${PYTHON_CONFIGURE_ENV} && cd ${BUILD_DIR}/python/src/external_python/ && make install
		INSTALL_DIR ${LIBDIR}/python)

	add_custom_target(Make_Python_Environment ALL DEPENDS external_python)
endif()

if(MSVC)
	add_custom_command(
		OUTPUT ${LIBDIR}/python${PYTHON_SHORT_VERSION_NO_DOTS}${PYTHON_POSTFIX}.tar.gz
		OUTPUT ${BUILD_DIR}/python/src/external_python/redist/bin/python${PYTHON_POSTFIX}.exe
		COMMAND ${CMAKE_COMMAND} -E copy_directory ${BUILD_DIR}/python/src/external_python/lib ${BUILD_DIR}/python/src/external_python/redist/lib
		COMMAND ${CMAKE_COMMAND} -E copy "${PYTHON_OUTPUTDIR}/_asyncio${PYTHON_POSTFIX}.pyd" ${BUILD_DIR}/python/src/external_python/redist/lib/_asyncio${PYTHON_POSTFIX}.pyd
		COMMAND ${CMAKE_COMMAND} -E copy "${PYTHON_OUTPUTDIR}/_bz2${PYTHON_POSTFIX}.pyd" ${BUILD_DIR}/python/src/external_python/redist/lib/_bz2${PYTHON_POSTFIX}.pyd
		COMMAND ${CMAKE_COMMAND} -E copy "${PYTHON_OUTPUTDIR}/_contextvars${PYTHON_POSTFIX}.pyd" ${BUILD_DIR}/python/src/external_python/redist/lib/_contextvars${PYTHON_POSTFIX}.pyd
		COMMAND ${CMAKE_COMMAND} -E copy "${PYTHON_OUTPUTDIR}/_ctypes${PYTHON_POSTFIX}.pyd" ${BUILD_DIR}/python/src/external_python/redist/lib/_ctypes${PYTHON_POSTFIX}.pyd
		COMMAND ${CMAKE_COMMAND} -E copy "${PYTHON_OUTPUTDIR}/_ctypes_test${PYTHON_POSTFIX}.pyd" ${BUILD_DIR}/python/src/external_python/redist/lib/_ctypes_test${PYTHON_POSTFIX}.pyd
		COMMAND ${CMAKE_COMMAND} -E copy "${PYTHON_OUTPUTDIR}/_decimal${PYTHON_POSTFIX}.pyd" ${BUILD_DIR}/python/src/external_python/redist/lib/_decimal${PYTHON_POSTFIX}.pyd
		COMMAND ${CMAKE_COMMAND} -E copy "${PYTHON_OUTPUTDIR}/_distutils_findvs${PYTHON_POSTFIX}.pyd" ${BUILD_DIR}/python/src/external_python/redist/lib/_distutils_findvs${PYTHON_POSTFIX}.pyd
		COMMAND ${CMAKE_COMMAND} -E copy "${PYTHON_OUTPUTDIR}/_elementtree${PYTHON_POSTFIX}.pyd" ${BUILD_DIR}/python/src/external_python/redist/lib/_elementtree${PYTHON_POSTFIX}.pyd
		COMMAND ${CMAKE_COMMAND} -E copy "${PYTHON_OUTPUTDIR}/_hashlib${PYTHON_POSTFIX}.pyd" ${BUILD_DIR}/python/src/external_python/redist/lib/_hashlib${PYTHON_POSTFIX}.pyd
		COMMAND ${CMAKE_COMMAND} -E copy "${PYTHON_OUTPUTDIR}/_lzma${PYTHON_POSTFIX}.pyd" ${BUILD_DIR}/python/src/external_python/redist/lib/_lzma${PYTHON_POSTFIX}.pyd
		COMMAND ${CMAKE_COMMAND} -E copy "${PYTHON_OUTPUTDIR}/_msi${PYTHON_POSTFIX}.pyd" ${BUILD_DIR}/python/src/external_python/redist/lib/_msi${PYTHON_POSTFIX}.pyd
		COMMAND ${CMAKE_COMMAND} -E copy "${PYTHON_OUTPUTDIR}/_multiprocessing${PYTHON_POSTFIX}.pyd" ${BUILD_DIR}/python/src/external_python/redist/lib/_multiprocessing${PYTHON_POSTFIX}.pyd
		COMMAND ${CMAKE_COMMAND} -E copy "${PYTHON_OUTPUTDIR}/_overlapped${PYTHON_POSTFIX}.pyd" ${BUILD_DIR}/python/src/external_python/redist/lib/_overlapped${PYTHON_POSTFIX}.pyd
		COMMAND ${CMAKE_COMMAND} -E copy "${PYTHON_OUTPUTDIR}/_queue${PYTHON_POSTFIX}.pyd" ${BUILD_DIR}/python/src/external_python/redist/lib/_queue${PYTHON_POSTFIX}.pyd
		COMMAND ${CMAKE_COMMAND} -E copy "${PYTHON_OUTPUTDIR}/_socket${PYTHON_POSTFIX}.pyd" ${BUILD_DIR}/python/src/external_python/redist/lib/_socket${PYTHON_POSTFIX}.pyd
		COMMAND ${CMAKE_COMMAND} -E copy "${PYTHON_OUTPUTDIR}/_sqlite3${PYTHON_POSTFIX}.pyd" ${BUILD_DIR}/python/src/external_python/redist/lib/_sqlite3${PYTHON_POSTFIX}.pyd
		COMMAND ${CMAKE_COMMAND} -E copy "${PYTHON_OUTPUTDIR}/_ssl${PYTHON_POSTFIX}.pyd" ${BUILD_DIR}/python/src/external_python/redist/lib/_ssl${PYTHON_POSTFIX}.pyd
		COMMAND ${CMAKE_COMMAND} -E copy "${PYTHON_OUTPUTDIR}/_testbuffer${PYTHON_POSTFIX}.pyd" ${BUILD_DIR}/python/src/external_python/redist/lib/_testbuffer${PYTHON_POSTFIX}.pyd
		COMMAND ${CMAKE_COMMAND} -E copy "${PYTHON_OUTPUTDIR}/_testcapi${PYTHON_POSTFIX}.pyd" ${BUILD_DIR}/python/src/external_python/redist/lib/_testcapi${PYTHON_POSTFIX}.pyd
		COMMAND ${CMAKE_COMMAND} -E copy "${PYTHON_OUTPUTDIR}/_testimportmultiple${PYTHON_POSTFIX}.pyd" ${BUILD_DIR}/python/src/external_python/redist/lib/_testimportmultiple${PYTHON_POSTFIX}.pyd
		COMMAND ${CMAKE_COMMAND} -E copy "${PYTHON_OUTPUTDIR}/_testmultiphase${PYTHON_POSTFIX}.pyd" ${BUILD_DIR}/python/src/external_python/redist/lib/_testmultiphase${PYTHON_POSTFIX}.pyd
		COMMAND ${CMAKE_COMMAND} -E copy "${PYTHON_OUTPUTDIR}/pyexpat${PYTHON_POSTFIX}.pyd" ${BUILD_DIR}/python/src/external_python/redist/lib/pyexpat${PYTHON_POSTFIX}.pyd
		COMMAND ${CMAKE_COMMAND} -E copy "${PYTHON_OUTPUTDIR}/python${PYTHON_POSTFIX}.exe" ${BUILD_DIR}/python/src/external_python/redist/bin/python${PYTHON_POSTFIX}.exe
		COMMAND ${CMAKE_COMMAND} -E copy "${PYTHON_OUTPUTDIR}/select${PYTHON_POSTFIX}.pyd" ${BUILD_DIR}/python/src/external_python/redist/lib/select${PYTHON_POSTFIX}.pyd
		COMMAND ${CMAKE_COMMAND} -E copy "${PYTHON_OUTPUTDIR}/unicodedata${PYTHON_POSTFIX}.pyd" ${BUILD_DIR}/python/src/external_python/redist/lib/unicodedata${PYTHON_POSTFIX}.pyd
		COMMAND ${CMAKE_COMMAND} -E copy "${PYTHON_OUTPUTDIR}/winsound${PYTHON_POSTFIX}.pyd" ${BUILD_DIR}/python/src/external_python/redist/lib/winsound${PYTHON_POSTFIX}.pyd
		#xxlimited is an example extention module, we don't need to ship it and debug doesn't build it
		#leaving it commented out, so I won't get confused again with the next update.
		#COMMAND ${CMAKE_COMMAND} -E copy "${PYTHON_OUTPUTDIR}/xxlimited${PYTHON_POSTFIX}.pyd" ${BUILD_DIR}/python/src/external_python/redist/lib/xxlimited${PYTHON_POSTFIX}.pyd
		COMMAND ${CMAKE_COMMAND} -E copy "${PYTHON_OUTPUTDIR}/libssl-1_1${SSL_POSTFIX}.dll" ${BUILD_DIR}/python/src/external_python/redist/lib/libssl-1_1${SSL_POSTFIX}.dll
		COMMAND ${CMAKE_COMMAND} -E copy "${PYTHON_OUTPUTDIR}/libcrypto-1_1${SSL_POSTFIX}.dll" ${BUILD_DIR}/python/src/external_python/redist/lib/libcrypto-1_1${SSL_POSTFIX}.dll
		COMMAND ${CMAKE_COMMAND} -E chdir "${BUILD_DIR}/python/src/external_python/redist" ${CMAKE_COMMAND} -E tar "cfvz" "${LIBDIR}/python${PYTHON_SHORT_VERSION_NO_DOTS}${PYTHON_POSTFIX}.tar.gz" "."
		COMMAND ${CMAKE_COMMAND} -E copy_directory ${LIBDIR}/python/ ${HARVEST_TARGET}/python/
		COMMAND ${CMAKE_COMMAND} -E copy ${LIBDIR}/python${PYTHON_SHORT_VERSION_NO_DOTS}${PYTHON_POSTFIX}.tar.gz ${HARVEST_TARGET}/Release/python${PYTHON_SHORT_VERSION_NO_DOTS}${PYTHON_POSTFIX}.tar.gz
	)

	add_custom_target(Package_Python ALL DEPENDS external_python ${LIBDIR}/python${PYTHON_SHORT_VERSION_NO_DOTS}${PYTHON_POSTFIX}.tar.gz ${BUILD_DIR}/python/src/external_python/redist/bin/python${PYTHON_POSTFIX}.exe)

	add_custom_command(OUTPUT ${BUILD_DIR}/python/src/external_python/run/python${PYTHON_POSTFIX}.exe
		COMMAND	${CMAKE_COMMAND} -E copy_directory ${BUILD_DIR}/python/src/external_python/redist ${BUILD_DIR}/python/src/external_python/run
		COMMAND	${CMAKE_COMMAND} -E copy_directory ${BUILD_DIR}/python/src/external_python/include ${BUILD_DIR}/python/src/external_python/run/include
		COMMAND	${CMAKE_COMMAND} -E copy "${BUILD_DIR}/python/src/external_python/PC/pyconfig.h" ${BUILD_DIR}/python/src/external_python/run/include/pyconfig.h
		COMMAND	${CMAKE_COMMAND} -E copy "${PYTHON_OUTPUTDIR}/python${PYTHON_SHORT_VERSION_NO_DOTS}${PYTHON_POSTFIX}.dll" ${BUILD_DIR}/python/src/external_python/run/python${PYTHON_SHORT_VERSION_NO_DOTS}${PYTHON_POSTFIX}.dll
		COMMAND	${CMAKE_COMMAND} -E copy "${PYTHON_OUTPUTDIR}/python${PYTHON_SHORT_VERSION_NO_DOTS}${PYTHON_POSTFIX}.lib" ${BUILD_DIR}/python/src/external_python/run/libs/python${PYTHON_SHORT_VERSION_NO_DOTS}.lib  #missing postfix on purpose, distutils is not expecting it
		COMMAND	${CMAKE_COMMAND} -E copy "${PYTHON_OUTPUTDIR}/python${PYTHON_SHORT_VERSION_NO_DOTS}${PYTHON_POSTFIX}.lib" ${BUILD_DIR}/python/src/external_python/run/libs/python${PYTHON_SHORT_VERSION_NO_DOTS}${PYTHON_POSTFIX}.lib  #other things like numpy still want it though.
		COMMAND	${CMAKE_COMMAND} -E copy "${PYTHON_OUTPUTDIR}/python${PYTHON_POSTFIX}.exe" ${BUILD_DIR}/python/src/external_python/run/python${PYTHON_POSTFIX}.exe
		COMMAND	${BUILD_DIR}/python/src/external_python/run/python${PYTHON_POSTFIX}.exe -m ensurepip --upgrade
	)
	add_custom_target(Make_Python_Environment ALL DEPENDS ${BUILD_DIR}/python/src/external_python/run/python${PYTHON_POSTFIX}.exe Package_Python)
endif()
