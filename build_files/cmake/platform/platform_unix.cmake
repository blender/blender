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
# The Original Code is Copyright (C) 2016, Blender Foundation
# All rights reserved.
#
# Contributor(s): Sergey Sharybin.
#
# ***** END GPL LICENSE BLOCK *****

# Libraries configuration for any *nix system including Linux and Unix.

# Detect precompiled library directory
set(LIBDIR_NAME ${CMAKE_SYSTEM_NAME}_${CMAKE_SYSTEM_PROCESSOR})
string(TOLOWER ${LIBDIR_NAME} LIBDIR_NAME)
set(LIBDIR ${CMAKE_SOURCE_DIR}/../lib/${LIBDIR_NAME})

if(EXISTS ${LIBDIR})
	file(GLOB LIB_SUBDIRS ${LIBDIR}/*)
	set(CMAKE_PREFIX_PATH ${LIB_SUBDIRS})
	set(WITH_STATIC_LIBS ON)
	set(WITH_OPENMP_STATIC ON)
endif()

# Wrapper to prefer static libraries
macro(find_package_wrapper)
	if(WITH_STATIC_LIBS)
		find_package_static(${ARGV})
	else()
		find_package(${ARGV})
	endif()
endmacro()

find_package_wrapper(JPEG REQUIRED)
find_package_wrapper(PNG REQUIRED)
find_package_wrapper(ZLIB REQUIRED)
find_package_wrapper(Freetype REQUIRED)

if(WITH_LZO AND WITH_SYSTEM_LZO)
	find_package_wrapper(LZO)
	if(NOT LZO_FOUND)
		message(FATAL_ERROR "Failed finding system LZO version!")
	endif()
endif()

if(WITH_SYSTEM_EIGEN3)
	find_package_wrapper(Eigen3)
	if(NOT EIGEN3_FOUND)
		message(FATAL_ERROR "Failed finding system Eigen3 version!")
	endif()
endif()
# else values are set below for all platforms

if(WITH_PYTHON)
	# No way to set py35, remove for now.
	# find_package(PythonLibs)

	# Use our own instead, since without py is such a rare case,
	# require this package
	# XXX Linking errors with debian static python :/
#		find_package_wrapper(PythonLibsUnix REQUIRED)
	find_package(PythonLibsUnix REQUIRED)
endif()

if(WITH_IMAGE_OPENEXR)
	find_package_wrapper(OpenEXR)  # our own module
	if(NOT OPENEXR_FOUND)
		set(WITH_IMAGE_OPENEXR OFF)
	endif()
endif()

if(WITH_IMAGE_OPENJPEG)
	find_package_wrapper(OpenJPEG)
	if(NOT OPENJPEG_FOUND)
		set(WITH_IMAGE_OPENJPEG OFF)
	endif()
endif()

if(WITH_IMAGE_TIFF)
	# XXX Linking errors with debian static tiff :/
#		find_package_wrapper(TIFF)
	find_package(TIFF)
	if(NOT TIFF_FOUND)
		set(WITH_IMAGE_TIFF OFF)
	endif()
endif()

# Audio IO
if(WITH_SYSTEM_AUDASPACE)
	find_package_wrapper(Audaspace)
	if(NOT AUDASPACE_FOUND OR NOT AUDASPACE_C_FOUND)
		message(FATAL_ERROR "Audaspace external library not found!")
	endif()
endif()

if(WITH_OPENAL)
	find_package_wrapper(OpenAL)
	if(NOT OPENAL_FOUND)
		set(WITH_OPENAL OFF)
	endif()
endif()

if(WITH_SDL)
	if(WITH_SDL_DYNLOAD)
		set(SDL_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/extern/sdlew/include/SDL2")
		set(SDL_LIBRARY)
	else()
		find_package_wrapper(SDL2)
		if(SDL2_FOUND)
			# Use same names for both versions of SDL until we move to 2.x.
			set(SDL_INCLUDE_DIR "${SDL2_INCLUDE_DIR}")
			set(SDL_LIBRARY "${SDL2_LIBRARY}")
			set(SDL_FOUND "${SDL2_FOUND}")
		else()
			find_package_wrapper(SDL)
		endif()
		mark_as_advanced(
			SDL_INCLUDE_DIR
			SDL_LIBRARY
		)
		# unset(SDLMAIN_LIBRARY CACHE)
		if(NOT SDL_FOUND)
			set(WITH_SDL OFF)
		endif()
	endif()
endif()

if(WITH_JACK)
	find_package_wrapper(Jack)
	if(NOT JACK_FOUND)
		set(WITH_JACK OFF)
	endif()
endif()

# Codecs
if(WITH_CODEC_SNDFILE)
	find_package_wrapper(SndFile)
	if(NOT LIBSNDFILE_FOUND)
		set(WITH_CODEC_SNDFILE OFF)
	endif()
endif()

if(WITH_CODEC_FFMPEG)
	if(EXISTS ${LIBDIR})
		# For precompiled lib directory, all ffmpeg dependencies are in the same folder
		file(GLOB ffmpeg_libs ${LIBDIR}/ffmpeg/lib/*.a ${LIBDIR}/sndfile/lib/*.a)
		set(FFMPEG ${LIBDIR}/ffmpeg CACHE PATH "FFMPEG Directory")
		set(FFMPEG_LIBRARIES ${ffmpeg_libs} ${ffmpeg_libs} CACHE STRING "FFMPEG Libraries")
	else()
		set(FFMPEG /usr CACHE PATH "FFMPEG Directory")
		set(FFMPEG_LIBRARIES avformat avcodec avutil avdevice swscale CACHE STRING "FFMPEG Libraries")
	endif()

	mark_as_advanced(FFMPEG)

	# lame, but until we have proper find module for ffmpeg
	set(FFMPEG_INCLUDE_DIRS ${FFMPEG}/include)
	if(EXISTS "${FFMPEG}/include/ffmpeg/")
		list(APPEND FFMPEG_INCLUDE_DIRS "${FFMPEG}/include/ffmpeg")
	endif()
	# end lameness

	mark_as_advanced(FFMPEG_LIBRARIES)
	set(FFMPEG_LIBPATH ${FFMPEG}/lib)
endif()

if(WITH_FFTW3)
	find_package_wrapper(Fftw3)
	if(NOT FFTW3_FOUND)
		set(WITH_FFTW3 OFF)
	endif()
endif()

if(WITH_OPENCOLLADA)
	find_package_wrapper(OpenCOLLADA)
	if(OPENCOLLADA_FOUND)
		find_package_wrapper(XML2)
		find_package_wrapper(PCRE)
	else()
		set(WITH_OPENCOLLADA OFF)
	endif()
endif()

if(WITH_MEM_JEMALLOC)
	find_package_wrapper(JeMalloc)
	if(NOT JEMALLOC_FOUND)
		set(WITH_MEM_JEMALLOC OFF)
	endif()
endif()

if(WITH_INPUT_NDOF)
	find_package_wrapper(Spacenav)
	if(SPACENAV_FOUND)
		# use generic names within blenders buildsystem.
		set(NDOF_INCLUDE_DIRS ${SPACENAV_INCLUDE_DIRS})
		set(NDOF_LIBRARIES ${SPACENAV_LIBRARIES})
	else()
		set(WITH_INPUT_NDOF OFF)
	endif()
endif()

if(WITH_CYCLES_OSL)
	set(CYCLES_OSL ${LIBDIR}/osl CACHE PATH "Path to OpenShadingLanguage installation")
	if(NOT OSL_ROOT)
		set(OSL_ROOT ${CYCLES_OSL})
	endif()
	find_package_wrapper(OpenShadingLanguage)
	if(OSL_FOUND)
		if(${OSL_LIBRARY_VERSION_MAJOR} EQUAL "1" AND ${OSL_LIBRARY_VERSION_MINOR} LESS "6")
			# Note: --whole-archive is needed to force loading of all symbols in liboslexec,
			# otherwise LLVM is missing the osl_allocate_closure_component function
			set(OSL_LIBRARIES
				${OSL_OSLCOMP_LIBRARY}
				-Wl,--whole-archive ${OSL_OSLEXEC_LIBRARY}
				-Wl,--no-whole-archive ${OSL_OSLQUERY_LIBRARY}
			)
		endif()
	else()
		message(STATUS "OSL not found, disabling it from Cycles")
		set(WITH_CYCLES_OSL OFF)
	endif()
endif()

if(WITH_OPENVDB)
	find_package_wrapper(OpenVDB)
	find_package_wrapper(TBB)
	find_package_wrapper(Blosc)
	if(NOT OPENVDB_FOUND OR NOT TBB_FOUND)
		set(WITH_OPENVDB OFF)
		set(WITH_OPENVDB_BLOSC OFF)
		message(STATUS "OpenVDB not found, disabling it")
	elseif(NOT BLOSC_FOUND)
		set(WITH_OPENVDB_BLOSC OFF)
		message(STATUS "Blosc not found, disabling it")
	endif()
endif()

if(WITH_ALEMBIC)
	find_package_wrapper(Alembic)

	if(WITH_ALEMBIC_HDF5)
		set(HDF5_ROOT_DIR ${LIBDIR}/hdf5)
		find_package_wrapper(HDF5)
	endif()

	if(NOT ALEMBIC_FOUND OR (WITH_ALEMBIC_HDF5 AND NOT HDF5_FOUND))
		set(WITH_ALEMBIC OFF)
		set(WITH_ALEMBIC_HDF5 OFF)
	endif()
endif()

if(WITH_BOOST)
	# uses in build instructions to override include and library variables
	if(NOT BOOST_CUSTOM)
		if(WITH_STATIC_LIBS)
			set(Boost_USE_STATIC_LIBS ON)
		endif()
		set(Boost_USE_MULTITHREADED ON)
		set(__boost_packages filesystem regex thread date_time)
		if(WITH_CYCLES_OSL)
			if(NOT (${OSL_LIBRARY_VERSION_MAJOR} EQUAL "1" AND ${OSL_LIBRARY_VERSION_MINOR} LESS "6"))
				list(APPEND __boost_packages wave)
			else()
			endif()
		endif()
		if(WITH_INTERNATIONAL)
			list(APPEND __boost_packages locale)
		endif()
		if(WITH_CYCLES_NETWORK)
			list(APPEND __boost_packages serialization)
		endif()
		if(WITH_OPENVDB)
			list(APPEND __boost_packages iostreams)
		endif()
		list(APPEND __boost_packages system)
		find_package(Boost 1.48 COMPONENTS ${__boost_packages})
		if(NOT Boost_FOUND)
			# try to find non-multithreaded if -mt not found, this flag
			# doesn't matter for us, it has nothing to do with thread
			# safety, but keep it to not disturb build setups
			set(Boost_USE_MULTITHREADED OFF)
			find_package(Boost 1.48 COMPONENTS ${__boost_packages})
		endif()
		unset(__boost_packages)
		if(Boost_USE_STATIC_LIBS AND WITH_BOOST_ICU)
			find_package(IcuLinux)
		endif()
		mark_as_advanced(Boost_DIR)  # why doesnt boost do this?
	endif()

	set(BOOST_INCLUDE_DIR ${Boost_INCLUDE_DIRS})
	set(BOOST_LIBRARIES ${Boost_LIBRARIES})
	set(BOOST_LIBPATH ${Boost_LIBRARY_DIRS})
	set(BOOST_DEFINITIONS "-DBOOST_ALL_NO_LIB")
endif()

if(WITH_OPENIMAGEIO)
	find_package_wrapper(OpenImageIO)
	if(NOT OPENIMAGEIO_PUGIXML_FOUND AND WITH_CYCLES_STANDALONE)
		find_package_wrapper(PugiXML)
	else()
		set(PUGIXML_INCLUDE_DIR "${OPENIMAGEIO_INCLUDE_DIR/OpenImageIO}")
		set(PUGIXML_LIBRARIES "")
	endif()

	set(OPENIMAGEIO_LIBRARIES
		${OPENIMAGEIO_LIBRARIES}
		${PNG_LIBRARIES}
		${JPEG_LIBRARIES}
		${ZLIB_LIBRARIES}
		${BOOST_LIBRARIES}
	)
	set(OPENIMAGEIO_LIBPATH)  # TODO, remove and reference the absolute path everywhere
	set(OPENIMAGEIO_DEFINITIONS "")

	if(WITH_IMAGE_TIFF)
		list(APPEND OPENIMAGEIO_LIBRARIES "${TIFF_LIBRARY}")
	endif()
	if(WITH_IMAGE_OPENEXR)
		list(APPEND OPENIMAGEIO_LIBRARIES "${OPENEXR_LIBRARIES}")
	endif()

	if(NOT OPENIMAGEIO_FOUND)
		set(WITH_OPENIMAGEIO OFF)
		message(STATUS "OpenImageIO not found, disabling WITH_CYCLES")
	endif()
endif()

if(WITH_OPENCOLORIO)
	find_package_wrapper(OpenColorIO)

	set(OPENCOLORIO_LIBRARIES ${OPENCOLORIO_LIBRARIES})
	set(OPENCOLORIO_LIBPATH)  # TODO, remove and reference the absolute path everywhere
	set(OPENCOLORIO_DEFINITIONS)

	if(NOT OPENCOLORIO_FOUND)
		set(WITH_OPENCOLORIO OFF)
		message(STATUS "OpenColorIO not found")
	endif()
endif()

if(WITH_LLVM)
	if(EXISTS ${LIBDIR})
		set(LLVM_STATIC ON)
	endif()

	find_package_wrapper(LLVM)

	# Symbol conflicts with same UTF library used by OpenCollada
	if(EXISTS ${LIBDIR})
		if(WITH_OPENCOLLADA AND (${LLVM_VERSION} VERSION_LESS "4.0.0"))
			list(REMOVE_ITEM OPENCOLLADA_LIBRARIES ${OPENCOLLADA_UTF_LIBRARY})
		endif()
	endif()

	if(NOT LLVM_FOUND)
		set(WITH_LLVM OFF)
		message(STATUS "LLVM not found")
	endif()
endif()

if(WITH_LLVM OR WITH_SDL_DYNLOAD)
	# Fix for conflict with Mesa llvmpipe
	set(PLATFORM_LINKFLAGS
		"${PLATFORM_LINKFLAGS} -Wl,--version-script='${CMAKE_SOURCE_DIR}/source/creator/blender.map'"
	)
endif()

if(WITH_OPENSUBDIV OR WITH_CYCLES_OPENSUBDIV)
	find_package_wrapper(OpenSubdiv)

	set(OPENSUBDIV_LIBRARIES ${OPENSUBDIV_LIBRARIES})
	set(OPENSUBDIV_LIBPATH)  # TODO, remove and reference the absolute path everywhere

	if(NOT OPENSUBDIV_FOUND)
		set(WITH_OPENSUBDIV OFF)
		set(WITH_CYCLES_OPENSUBDIV OFF)
		message(STATUS "OpenSubdiv not found")
	endif()
endif()

# OpenSuse needs lutil, ArchLinux not, for now keep, can avoid by using --as-needed
if(HAIKU)
	list(APPEND PLATFORM_LINKLIBS -lnetwork)
else()
	list(APPEND PLATFORM_LINKLIBS -lutil -lc -lm)
endif()

find_package(Threads REQUIRED)
list(APPEND PLATFORM_LINKLIBS ${CMAKE_THREAD_LIBS_INIT})
# used by other platforms
set(PTHREADS_LIBRARIES ${CMAKE_THREAD_LIBS_INIT})

if(CMAKE_DL_LIBS)
	list(APPEND PLATFORM_LINKLIBS ${CMAKE_DL_LIBS})
endif()

if(CMAKE_SYSTEM_NAME MATCHES "Linux")
	if(NOT WITH_PYTHON_MODULE)
		# binreloc is linux only
		set(BINRELOC_INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/extern/binreloc/include)
		set(WITH_BINRELOC ON)
	endif()
endif()

# lfs on glibc, all compilers should use
add_definitions(-D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64 -D_LARGEFILE64_SOURCE)

# GNU Compiler
if(CMAKE_COMPILER_IS_GNUCC)
	set(PLATFORM_CFLAGS "-pipe -fPIC -funsigned-char -fno-strict-aliasing")

	if(WITH_LINKER_GOLD)
		execute_process(
			COMMAND ${CMAKE_C_COMPILER} -fuse-ld=gold -Wl,--version
			ERROR_QUIET OUTPUT_VARIABLE LD_VERSION)
		if("${LD_VERSION}" MATCHES "GNU gold")
			set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fuse-ld=gold")
			set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fuse-ld=gold")
		else()
			message(STATUS "GNU gold linker isn't available, using the default system linker.")
		endif()
		unset(LD_VERSION)
	endif()

# CLang is the same as GCC for now.
elseif(CMAKE_C_COMPILER_ID MATCHES "Clang")
	set(PLATFORM_CFLAGS "-pipe -fPIC -funsigned-char -fno-strict-aliasing")
# Intel C++ Compiler
elseif(CMAKE_C_COMPILER_ID MATCHES "Intel")
	# think these next two are broken
	find_program(XIAR xiar)
	if(XIAR)
		set(CMAKE_AR "${XIAR}")
	endif()
	mark_as_advanced(XIAR)

	find_program(XILD xild)
	if(XILD)
		set(CMAKE_LINKER "${XILD}")
	endif()
	mark_as_advanced(XILD)

	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fp-model precise -prec_div -parallel")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fp-model precise -prec_div -parallel")

	# set(PLATFORM_CFLAGS "${PLATFORM_CFLAGS} -diag-enable sc3")
	set(PLATFORM_CFLAGS "-pipe -fPIC -funsigned-char -fno-strict-aliasing")
	set(PLATFORM_LINKFLAGS "${PLATFORM_LINKFLAGS} -static-intel")
endif()
