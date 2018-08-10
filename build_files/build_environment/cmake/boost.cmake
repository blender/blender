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
	if("${CMAKE_SIZEOF_VOID_P}" EQUAL "8")
		set(PYTHON_ARCH x64)
		set(PYTHON_ARCH2 win-AMD64)
		set(PYTHON_OUTPUTDIR ${BUILD_DIR}/python/src/external_python/pcbuild/amd64/)
	else()
		set(PYTHON_ARCH x86)
		set(PYTHON_ARCH2 win32)
		set(PYTHON_OUTPUTDIR ${BUILD_DIR}/python/src/external_python/pcbuild/win32/)
	endif()
	if(MSVC12)
		set(BOOST_TOOLSET toolset=msvc-12.0)
		set(BOOST_COMPILER_STRING -vc120)
		set(PYTHON_COMPILER_STRING v120)
	endif()
	if(MSVC14)
		set(BOOST_TOOLSET toolset=msvc-14.0)
		set(BOOST_COMPILER_STRING -vc140)
		set(PYTHON_COMPILER_STRING v140)
	endif()
	set(JAM_FILE ${BUILD_DIR}/boost/src/external_boost/user-config.jam)
	set(semi_path "${PATCH_DIR}/semi.txt")
	FILE(TO_NATIVE_PATH ${semi_path} semi_path)
	set(BOOST_CONFIGURE_COMMAND bootstrap.bat &&
								echo using python : ${PYTHON_OUTPUTDIR}\\python.exe > "${JAM_FILE}" &&
								echo.   : ${BUILD_DIR}/python/src/external_python/include ${BUILD_DIR}/python/src/external_python/pc >> "${JAM_FILE}" &&
								echo.   : ${BUILD_DIR}/python/src/external_python/pcbuild >> "${JAM_FILE}" &&
								type ${semi_path} >> "${JAM_FILE}"
	)
	set(BOOST_BUILD_COMMAND bjam)
	#--user-config=user-config.jam
	set(BOOST_BUILD_OPTIONS runtime-link=static )
	#set(BOOST_WITH_PYTHON --with-python)
	set(BOOST_HARVEST_CMD 	${CMAKE_COMMAND} -E copy_directory ${LIBDIR}/boost/lib/ ${HARVEST_TARGET}/boost/lib/ )
	if(BUILD_MODE STREQUAL Release)
		set(BOOST_HARVEST_CMD ${BOOST_HARVEST_CMD} && ${CMAKE_COMMAND} -E copy_directory ${LIBDIR}/boost/include/boost-1_60/ ${HARVEST_TARGET}/boost/include/)
	endif()
	set(BOOST_PATCH_COMMAND ${PATCH_CMD} --verbose -p 1 -N -d ${BUILD_DIR}/boost/src/external_boost < ${PATCH_DIR}/boost.diff)

elseif(APPLE)
	set(BOOST_CONFIGURE_COMMAND ./bootstrap.sh)
	set(BOOST_BUILD_COMMAND ./b2)
	set(BOOST_BUILD_OPTIONS toolset=darwin cxxflags=${PLATFORM_CXXFLAGS} linkflags=${PLATFORM_LDFLAGS} --disable-icu boost.locale.icu=off)
	set(BOOST_HARVEST_CMD echo .)
	set(BOOST_PATCH_COMMAND echo .)
else()
	set(BOOST_HARVEST_CMD echo .)
	set(BOOST_CONFIGURE_COMMAND ./bootstrap.sh)
	set(BOOST_BUILD_COMMAND ./b2)
	set(BOOST_BUILD_OPTIONS cxxflags=${PLATFORM_CXXFLAGS} --disable-icu boost.locale.icu=off)
	set(BOOST_PATCH_COMMAND echo .)
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
	${BOOST_WITH_PYTHON}
	${BOOST_TOOLSET}
)

	set(BOOST_ADDRESS_MODEL 64)


string(TOLOWER ${BUILD_MODE} BOOST_BUILD_TYPE)

ExternalProject_Add(external_boost
	URL ${BOOST_URI}
	DOWNLOAD_DIR ${DOWNLOAD_DIR}
	URL_HASH MD5=${BOOST_HASH}
	PREFIX ${BUILD_DIR}/boost
	UPDATE_COMMAND	""
	PATCH_COMMAND ${BOOST_PATCH_COMMAND}
	CONFIGURE_COMMAND ${BOOST_CONFIGURE_COMMAND}
	BUILD_COMMAND ${BOOST_BUILD_COMMAND} ${BOOST_BUILD_OPTIONS} -j${MAKE_THREADS} architecture=x86 address-model=${BOOST_ADDRESS_MODEL} link=static threading=multi ${BOOST_OPTIONS}	--prefix=${LIBDIR}/boost install
	BUILD_IN_SOURCE 1
	INSTALL_COMMAND "${BOOST_HARVEST_CMD}"
)

if(WIN32)
	add_dependencies(
		external_boost
		Make_Python_Environment
	)
endif()
