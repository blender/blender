# -*- mode: cmake; indent-tabs-mode: t; -*-
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
# The Original Code is Copyright (C) 2006, Blender Foundation
# All rights reserved.
#
# The Original Code is: all of this file.
#
# Contributor(s): Jacques Beaurain.
#
# ***** END GPL LICENSE BLOCK *****

#-----------------------------------------------------------------------------
# We don't allow in-source builds. This causes no end of troubles because
# all out-of-source builds will use the CMakeCache.txt file there and even
# build the libs and objects in it.

if(${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})
	if(NOT DEFINED WITH_IN_SOURCE_BUILD)
		message(FATAL_ERROR
			"CMake generation for blender is not allowed within the source directory!"
			"\n Remove the CMakeCache.txt file and try again from another folder, e.g.:"
			"\n "
			"\n rm CMakeCache.txt"
			"\n cd .."
			"\n mkdir cmake-make"
			"\n cd cmake-make"
			"\n cmake ../blender"
			"\n "
			"\n Alternately define WITH_IN_SOURCE_BUILD to force this option (not recommended!)"
		)
	endif()
endif()

cmake_minimum_required(VERSION 2.8)

if(NOT EXECUTABLE_OUTPUT_PATH)
	set(FIRST_RUN "TRUE")
endif()

# this starts out unset
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/build_files/cmake/Modules")

# avoid having empty buildtype
set(CMAKE_BUILD_TYPE_INIT "Release")

# quiet output for Makefiles, 'make -s' helps too
# set_property(GLOBAL PROPERTY RULE_MESSAGES OFF)

# global compile definitions since add_definitions() adds for all.
set_property(DIRECTORY APPEND PROPERTY COMPILE_DEFINITIONS_DEBUG           DEBUG _DEBUG)
set_property(DIRECTORY APPEND PROPERTY COMPILE_DEFINITIONS_RELEASE         NDEBUG)
set_property(DIRECTORY APPEND PROPERTY COMPILE_DEFINITIONS_MINSIZEREL      NDEBUG)
set_property(DIRECTORY APPEND PROPERTY COMPILE_DEFINITIONS_RELWITHDEBINFO  NDEBUG)


#-----------------------------------------------------------------------------
# Set policy

# see "cmake --help-policy CMP0003"
# So library linking is more sane
cmake_policy(SET CMP0003 NEW)

# So BUILDINFO and BLENDERPATH strings are automatically quoted
cmake_policy(SET CMP0005 NEW)

# So syntax problems are errors
cmake_policy(SET CMP0010 NEW)

# Input directories must have CMakeLists.txt
cmake_policy(SET CMP0014 NEW)

#-----------------------------------------------------------------------------
# Load some macros.
include(build_files/cmake/macros.cmake)


#-----------------------------------------------------------------------------
# Initialize project.

blender_project_hack_pre()

project(Blender)

blender_project_hack_post()

enable_testing()

#-----------------------------------------------------------------------------
# Redirect output files

set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR}/bin CACHE INTERNAL "" FORCE )
set(LIBRARY_OUTPUT_PATH ${CMAKE_BINARY_DIR}/lib CACHE INTERNAL "" FORCE )

#-----------------------------------------------------------------------------
# Set default config options

get_blender_version()


# Blender internal features
option(WITH_BLENDER "Build blender (disable to build only the blender player)" ON)
mark_as_advanced(WITH_BLENDER)

option(WITH_INTERNATIONAL "Enable I18N (International fonts and text)" ON)

option(WITH_PYTHON        "Enable Embedded Python API  (only disable for development)" ON)
option(WITH_PYTHON_SECURITY "Disables execution of scripts within blend files by default (recommend to leave off)" OFF)
mark_as_advanced(WITH_PYTHON)  # dont want people disabling this unless they really know what they are doing.
mark_as_advanced(WITH_PYTHON_SECURITY)  # some distrobutions see this as a security issue, rather than have them patch it, make a build option.

option(WITH_PYTHON_SAFETY "Enable internal API error checking to track invalid data to prevent crash on access (at the expense of some effeciency, only enable for development)." OFF)
option(WITH_PYTHON_MODULE "Enable building as a python module (experemental, only enable for development)" OFF)
option(WITH_BUILDINFO     "Include extra build details (only disable for development & faster builds)" ON)
option(WITH_IK_ITASC      "Enable ITASC IK solver (only disable for development & for incompatible C++ compilers)" ON)
option(WITH_FFTW3         "Enable FFTW3 support (Used for smoke and audio effects)" OFF)
option(WITH_BULLET        "Enable Bullet (Physics Engine)" ON)
option(WITH_GAMEENGINE    "Enable Game Engine" ON)
option(WITH_PLAYER        "Build Player" OFF)

# GHOST Windowing Library Options
option(WITH_GHOST_DEBUG   "Enable debugging output for the GHOST library" OFF)
mark_as_advanced(WITH_GHOST_DEBUG)

option(WITH_GHOST_SDL    "Enable building blender against SDL for windowing rather then the native APIs" OFF)
mark_as_advanced(WITH_GHOST_SDL)

# Misc...
option(WITH_HEADLESS      "Build without graphical support (renderfarm, server mode only)" OFF)
mark_as_advanced(WITH_HEADLESS)

option(WITH_AUDASPACE    "Build with blenders audio library (only disable if you know what you're doing!)" ON)
mark_as_advanced(WITH_AUDASPACE)


# (unix defaults to OpenMP On)
if(UNIX AND NOT APPLE)
	option(WITH_OPENMP        "Enable OpenMP (has to be supported by the compiler)" ON)
else()
	option(WITH_OPENMP        "Enable OpenMP (has to be supported by the compiler)" OFF)
endif()

if(UNIX AND NOT APPLE)
	option(WITH_X11_XINPUT "Enable X11 Xinput (tablet support and unicode input)"	ON)
	option(WITH_X11_XF86VMODE "Enable X11 video mode switching"	OFF)
	option(WITH_BUILTIN_GLEW "Use GLEW OpenGL wrapper library bundled with blender" ON)
	option(WITH_XDG_USER_DIRS    "Build with XDG Base Directory Specification (only config and documents for now)" OFF)
	mark_as_advanced(WITH_XDG_USER_DIRS)
else()
	# not an option for other OS's
	set(WITH_BUILTIN_GLEW ON)
endif()

# Modifiers
option(WITH_MOD_FLUID        	"Enable Elbeem Modifier (Fluid Simulation)" ON)
option(WITH_MOD_SMOKE        	"Enable Smoke Modifier (Smoke Simulation)" ON)
option(WITH_MOD_DECIMATE       	"Enable Decimate Modifier" ON)
option(WITH_MOD_BOOLEAN        	"Enable Boolean Modifier" ON)
option(WITH_MOD_REMESH        	"Enable Remesh Modifier" ON)
option(WITH_MOD_CLOTH_ELTOPO   	"Enable Experemental cloth solver" OFF)
mark_as_advanced(WITH_MOD_CLOTH_ELTOPO)
option(WITH_MOD_OCEANSIM        "Enable Ocean Modifier" OFF)

# Image format support
option(WITH_IMAGE_OPENEXR       "Enable OpenEXR Support (http://www.openexr.com)" ON)
option(WITH_IMAGE_OPENJPEG      "Enable OpenJpeg Support (http://www.openjpeg.org)" ON)
option(WITH_IMAGE_TIFF          "Enable LibTIFF Support" ON)
option(WITH_IMAGE_DDS           "Enable DDS Image Support" ON)
option(WITH_IMAGE_CINEON        "Enable CINEON and DPX Image Support" ON)
option(WITH_IMAGE_HDR           "Enable HDR Image Support" ON)
option(WITH_IMAGE_REDCODE       "Enable RedCode Image Support" OFF)
option(WITH_IMAGE_FRAMESERVER   "Enable image FrameServer Support for rendering" ON)

# Audio/Video format support
option(WITH_CODEC_FFMPEG        "Enable FFMPeg Support (http://ffmpeg.org)" OFF)
option(WITH_CODEC_SNDFILE       "Enable libsndfile Support (http://www.mega-nerd.com/libsndfile)" OFF)
if(APPLE OR (WIN32 AND NOT UNIX))
	option(WITH_CODEC_QUICKTIME     "Enable Quicktime Support" OFF)
endif()

# 3D format support
# disable opencollada on non-apple unix because opencollada has no package for debian
option(WITH_OPENCOLLADA		"Enable OpenCollada Support (http://www.opencollada.org)"	OFF)

# Sound output
option(WITH_SDL           "Enable SDL for sound and joystick support" ON)
option(WITH_OPENAL        "Enable OpenAL Support (http://www.openal.org)" ON)
option(WITH_JACK          "Enable Jack Support (http://www.jackaudio.org)" OFF)

# Compression
option(WITH_LZO           "Enable fast LZO compression (used for pointcache)" ON)
option(WITH_LZMA          "Enable best LZMA compression, (used for pointcache)" ON)

# Camera/motion tracking
option(WITH_LIBMV         "Enable libmv structure from motion library" ON)

# Misc
option(WITH_INPUT_NDOF "Enable NDOF input devices (SpaceNavigator and friends)" ON)
option(WITH_RAYOPTIMIZATION	"Enable use of SIMD (SSE) optimizations for the raytracer" ON)
if(UNIX AND NOT APPLE)
	option(WITH_INSTALL_PORTABLE "Install redistributeable runtime, otherwise install into CMAKE_INSTALL_PREFIX" ON)
endif()
option(WITH_PYTHON_INSTALL       "Copy system python into the blender install folder" ON)

# Cycles
option(WITH_CYCLES					"Enable cycles Render Engine" ON)
option(WITH_CYCLES_TEST				"Build cycles test application" OFF)
option(WITH_CYCLES_CUDA_BINARIES	"Build cycles CUDA binaries" OFF)
set(CYCLES_CUDA_BINARIES_ARCH		sm_13 sm_20 sm_21 CACHE STRING "CUDA architectures to build binaries for")
mark_as_advanced(CYCLES_CUDA_BINARIES_ARCH)

# disable for now, but plan to support on all platforms eventually
option(WITH_MEM_JEMALLOC   "Enable malloc replacement (http://www.canonware.com/jemalloc)" OFF)
mark_as_advanced(WITH_MEM_JEMALLOC)

# Debug
option(WITH_CXX_GUARDEDALLOC "Enable GuardedAlloc for C++ memory allocation tracking (only enable for development)" OFF)
mark_as_advanced(WITH_CXX_GUARDEDALLOC)

option(WITH_ASSERT_ABORT "Call abort() when raising an assertion through BLI_assert()" OFF)
mark_as_advanced(WITH_ASSERT_ABORT)


if(APPLE)
	if(NOT CMAKE_OSX_ARCHITECTURES)
		set(CMAKE_OSX_ARCHITECTURES x86_64 CACHE STRING
		"Choose the architecture you want to build Blender for: i386, x86_64 or ppc"
		FORCE)
	endif()
	if(NOT CMAKE_OSX_DEPLOYMENT_TARGET)
		set(CMAKE_OSX_DEPLOYMENT_TARGET 10.5 CACHE STRING
		"Choose the minimum OSX version required: 10.4 or 10.5"
		FORCE)
	endif()
	option(WITH_COCOA	  "Use Cocoa framework instead of deprecated Carbon" ON)
	option(USE_QTKIT	  "Use QtKit instead of Carbon quicktime (needed for having partial quicktime for 64bit)" OFF)
	option(WITH_LIBS10.5  "Use 10.5 libs (needed for 64bit builds)" OFF)
endif()

#-----------------------------------------------------------------------------
# Check for conflicting/unsupported configurations

if(NOT WITH_BLENDER AND NOT WITH_PLAYER)
	message(FATAL_ERROR "At least one of WITH_BLENDER or WITH_PLAYER must be enabled, nothing to do!")
endif()

if(NOT WITH_GAMEENGINE AND WITH_PLAYER)
	message(FATAL_ERROR "WITH_PLAYER requires WITH_GAMEENGINE")
endif()

if(NOT WITH_AUDASPACE AND (WITH_OPENAL OR WITH_SDL OR WITH_JACK))
	message(FATAL_ERROR "WITH_OPENAL/WITH_SDL/WITH_JACK/WITH_CODEC_FFMPEG require WITH_AUDASPACE")
endif()

if(NOT WITH_SDL AND WITH_GHOST_SDL)
	message(FATAL_ERROR "WITH_GHOST_SDL requires WITH_SDL to be ON")
endif()

if(NOT WITH_IMAGE_OPENJPEG AND WITH_IMAGE_REDCODE)
	message(FATAL_ERROR "WITH_IMAGE_REDCODE requires WITH_IMAGE_OPENJPEG")
endif()

# python module, needs some different options
if(WITH_PYTHON_MODULE AND WITH_PLAYER)
	message(FATAL_ERROR "WITH_PYTHON_MODULE requires WITH_PLAYER to be OFF")
endif()

if(WITH_PYTHON_MODULE AND WITH_PYTHON_INSTALL)
	message(FATAL_ERROR "WITH_PYTHON_MODULE requires WITH_PYTHON_INSTALL to be OFF")
endif()

if(MINGW)
	if(WITH_CODEC_QUICKTIME)
		message(FATAL_ERROR "MINGW requires WITH_CODEC_QUICKTIME to be OFF "
							"because it is currently unsupported, remove this "
							"line if youre a developer who wants to add support.")
	endif()
endif()

if(NOT WITH_FFTW3 AND WITH_MOD_OCEANSIM)
	message(FATAL_ERROR "WITH_MOD_OCEANSIM requires WITH_FFTW3 to be ON")
endif()

# may as well build python module without a UI
if(WITH_PYTHON_MODULE)
	set(WITH_HEADLESS ON)
endif()

# auto enable openimageio and boost for cycles
if(WITH_CYCLES)
	set(WITH_OPENIMAGEIO ON)
	set(WITH_BOOST ON)
endif()

TEST_SSE_SUPPORT(COMPILER_SSE_FLAG COMPILER_SSE2_FLAG)

# don't store paths to libs for portable distrobution
if(WITH_INSTALL_PORTABLE)
	set(CMAKE_SKIP_BUILD_RPATH TRUE)
endif()

#-----------------------------------------------------------------------------
# Initialize un-cached vars, avoid unused warning

# linux only, not cached
set(WITH_BINRELOC OFF)

# MAXOSX only, set to avoid uninitialized
set(EXETYPE "")

# C/C++ flags
set(PLATFORM_CFLAGS)

# these are added to later on.
set(C_WARNINGS)
set(CXX_WARNINGS)

# for gcc -Wno-blah-blah
set(CC_REMOVE_STRICT_FLAGS)

# libraries to link the binary with passed to target_link_libraries()
# known as LLIBS to scons
set(PLATFORM_LINKLIBS "")

# Added to linker flags in setup_liblinks
# - CMAKE_EXE_LINKER_FLAGS
# - CMAKE_EXE_LINKER_FLAGS_DEBUG
set(PLATFORM_LINKFLAGS "")
set(PLATFORM_LINKFLAGS_DEBUG "")


# disabled for now, not supported
# option(WITH_WEBPLUGIN     "Enable Web Plugin (Unix only)" OFF)

# For alternate Python locations the commandline can be used to override detected/default cache settings, e.g:
# On Unix:
#   cmake ../blender \
#         -D PYTHON_VERSION=3.2 \
#         -D PYTHON_INCLUDE_DIR=/opt/py32/include/python3.2d \
#         -D PYTHON_LIBRARY=/opt/py32/lib/libpython3.2d.so
#
# On Macs:
#   cmake ../blender \
#         -D PYTHON_INCLUDE_DIR=/System/Library/Frameworks/Python.framework/Versions/3.2/include/python3.2 \
#         -D PYTHON_LIBPATH=/System/Library/Frameworks/Python.framework/Versions/3.2/lib/python3.2/config \
#         -G Xcode
#
# When changing any of this remember to update the notes in doc/build_systems/cmake.txt

#-----------------------------------------------------------------------------
#Platform specifics

if(UNIX AND NOT APPLE)

	# needed for ubuntu 11.04
	if(EXISTS "/usr/lib/x86_64-linux-gnu")
		set(CMAKE_LIBRARY_PATH "/usr/lib/x86_64-linux-gnu;${CMAKE_LIBRARY_PATH}")
	endif()

	# set lib directory if it exists
	if(CMAKE_SYSTEM_NAME MATCHES "Linux")
		if("${CMAKE_SIZEOF_VOID_P}" EQUAL "8")
			set(LIBDIR ${CMAKE_SOURCE_DIR}/../lib/linux64)
		else()
			set(LIBDIR ${CMAKE_SOURCE_DIR}/../lib/linux)
		endif()

		if(NOT EXISTS ${LIBDIR})
			unset(LIBDIR)
		endif()
	endif()

	find_package(JPEG REQUIRED)
	find_package(PNG REQUIRED)
	find_package(ZLIB REQUIRED)
	find_package(Freetype REQUIRED)

	if(WITH_PYTHON)
		# No way to set py32. remove for now.
		# find_package(PythonLibs)

		# Use our own instead, since wothout py is such a rare case,
		# require this package
		find_package(PythonLibsUnix REQUIRED)
	endif()


	if(WITH_IMAGE_OPENEXR)
		find_package(OpenEXR)  # our own module
		if(NOT OPENEXR_FOUND)
			set(WITH_IMAGE_OPENEXR OFF)
		endif()
	endif()

	if(WITH_IMAGE_OPENJPEG)
		find_package(OpenJPEG)
		if(NOT OPENJPEG_FOUND)
			set(WITH_IMAGE_OPENJPEG OFF)
		endif()
	endif()

	if(WITH_IMAGE_TIFF)
		find_package(TIFF)
		if(NOT TIFF_FOUND)
			set(WITH_IMAGE_TIFF OFF)
		endif()
	endif()

	# Audio IO
	if(WITH_OPENAL)
		find_package(OpenAL)
		if(NOT OPENAL_FOUND)
			set(WITH_OPENAL OFF)
		endif()
	endif()

	if(WITH_SDL)
		find_package(SDL)
		mark_as_advanced(
			SDLMAIN_LIBRARY
			SDL_INCLUDE_DIR
			SDL_LIBRARY
			SDL_LIBRARY_TEMP
		)
		# unset(SDLMAIN_LIBRARY CACHE)
		if(NOT SDL_FOUND)
			set(WITH_SDL OFF)
		endif()
	endif()

	if(WITH_JACK)
		find_package(Jack)
		if(NOT JACK_FOUND)
			set(WITH_JACK OFF)
		endif()
	endif()

	# Codecs
	if(WITH_CODEC_SNDFILE)
		find_package(SndFile)
		if(NOT SNDFILE_FOUND)
			set(WITH_CODEC_SNDFILE OFF)
		endif()
	endif()

	if(WITH_CODEC_FFMPEG)
		# use lib dir if available and nothing else specified
		if(LIBDIR AND NOT FFMPEG)
			set(FFMPEG ${LIBDIR}/ffmpeg CACHE PATH "FFMPEG Directory")
			set(FFMPEG_LIBRARIES avformat avcodec avutil avdevice swscale dirac_encoder mp3lame ogg orc-0.4 schroedinger-1.0 theora theoraenc theoradec vorbis vorbisenc vpx x264 xvidcore faad asound CACHE STRING "FFMPEG Libraries")
		else()
			set(FFMPEG /usr CACHE PATH "FFMPEG Directory")
			set(FFMPEG_LIBRARIES avformat avcodec avutil avdevice swscale CACHE STRING "FFMPEG Libraries")
		endif()

		mark_as_advanced(FFMPEG)
		set(FFMPEG_INCLUDE_DIRS ${FFMPEG}/include)
		mark_as_advanced(FFMPEG_LIBRARIES)
		set(FFMPEG_LIBPATH ${FFMPEG}/lib)
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D__STDC_CONSTANT_MACROS")
	endif()

	if(WITH_INTERNATIONAL)
		find_library(INTL_LIBRARY
			NAMES intl
			PATHS
			/sw/lib
		)

		find_library(ICONV_LIBRARY
			NAMES iconv
			PATHS
			/sw/lib
		)
		mark_as_advanced(
			ICONV_LIBRARY
			INTL_LIBRARY
		)

		if(INTL_LIBRARY AND ICONV_LIBRARY)
			set(GETTEXT_LIBRARIES ${INTL_LIBRARY} ${ICONV_LIBRARY})
		endif()
	endif()

	if(WITH_FFTW3)
		find_package(Fftw3)
		if(NOT FFTW3_FOUND)
			set(WITH_FFTW3 OFF)
		endif()
	endif()

	if(WITH_OPENCOLLADA)
		find_package(OpenCOLLADA)
		if(OPENCOLLADA_FOUND)
			find_package(XML2)
			find_package(PCRE)
		else()
			set(WITH_OPENCOLLADA OFF)
		endif()
	endif()

	if(WITH_MEM_JEMALLOC)
		find_package(JeMalloc)
		if(NOT JEMALLOC_FOUND)
			set(WITH_MEM_JEMALLOC OFF)
		endif()
	endif()

	if (WITH_INPUT_NDOF)
		find_package(Spacenav)
		if(NOT SPACENAV_FOUND)
			set(WITH_INPUT_NDOF OFF)
		endif()

		# use generic names within blenders buildsystem.
		if(SPACENAV_FOUND)
			set(NDOF_INCLUDE_DIRS ${SPACENAV_INCLUDE_DIRS})
			set(NDOF_LIBRARIES ${SPACENAV_LIBRARIES})
		endif()
	endif()

	if(WITH_BOOST)
		# uses in build instructions to override include and library variables
		if(NOT BOOST_CUSTOM)
			# use lib dir if available and nothing else specified
			if(LIBDIR AND NOT BOOST_ROOT)
				set(BOOST_ROOT ${LIBDIR}/boost)
				set(Boost_USE_MULTITHREADED OFF)
			else()
				set(Boost_USE_MULTITHREADED ON)
			endif()
			find_package(Boost 1.34 COMPONENTS filesystem regex system thread)
			mark_as_advanced(Boost_DIR)  # why doesnt boost do this?
		endif()

		set(BOOST_INCLUDE_DIR ${Boost_INCLUDE_DIRS})
		set(BOOST_LIBRARIES ${Boost_LIBRARIES})
		set(BOOST_LIBPATH ${Boost_LIBRARY_DIRS})
		set(BOOST_DEFINITIONS "-DBOOST_ALL_NO_LIB")
	endif()

	if(WITH_OPENIMAGEIO)
		# use lib dir if available and nothing else specified
		if(LIBDIR AND NOT OPENIMAGEIO_ROOT_DIR)
			set(OPENIMAGEIO_ROOT_DIR ${LIBDIR}/oiio)
		endif()

		find_package(OpenImageIO)

		set(OPENIMAGEIO_LIBRARIES ${OPENIMAGEIO_LIBRARIES} ${PNG_LIBRARIES} ${JPEG_LIBRARIES} ${TIFF_LIBRARY} ${OPENEXR_LIBRARIES} ${ZLIB_LIBRARIES} ${BOOST_LIBRARIES})
		set(OPENIMAGEIO_LIBPATH)  # TODO, remove and reference the absolute path everywhere
		set(OPENIMAGEIO_DEFINITIONS)

		if(NOT OPENIMAGEIO_FOUND)
			set(WITH_OPENIMAGEIO OFF)
			set(WITH_CYCLES OFF)
			message(STATUS "OpenImageIO not found, disabling WITH_CYCLES")
		endif()
	endif()

	# OpenSuse needs lutil, ArchLinux not, for now keep, can avoid by using --as-needed
	set(PLATFORM_LINKLIBS "-lutil -lc -lm -lpthread -lstdc++")

	if((NOT WITH_HEADLESS) AND (NOT WITH_GHOST_SDL))
		find_package(X11 REQUIRED)
		find_path(X11_XF86keysym_INCLUDE_PATH X11/XF86keysym.h ${X11_INC_SEARCH_PATH})
		mark_as_advanced(X11_XF86keysym_INCLUDE_PATH)

		set(PLATFORM_LINKLIBS "${PLATFORM_LINKLIBS} ${X11_X11_LIB}")

		if(WITH_X11_XINPUT)
			set(PLATFORM_LINKLIBS "${PLATFORM_LINKLIBS} ${X11_Xinput_LIB}")
		endif()

		if(WITH_X11_XF86VMODE)
			# XXX, why dont cmake make this available?
			FIND_LIBRARY(X11_Xxf86vmode_LIB Xxf86vm   ${X11_LIB_SEARCH_PATH})
			mark_as_advanced(X11_Xxf86vmode_LIB)
			if(X11_Xxf86vmode_LIB)
				set(PLATFORM_LINKLIBS "${PLATFORM_LINKLIBS} ${X11_Xxf86vmode_LIB}")
			else()
				set(WITH_X11_XF86VMODE OFF)
			endif()
		endif()
	endif()

	if(CMAKE_SYSTEM_NAME MATCHES "Linux")
		if(NOT WITH_PYTHON_MODULE)
			# BSD's dont use libdl.so
		set(PLATFORM_LINKLIBS "${PLATFORM_LINKLIBS} -ldl")
			# binreloc is linux only
			set(BINRELOC_INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/extern/binreloc/include)
			set(WITH_BINRELOC ON)
		endif()
	endif()

	set(PLATFORM_LINKFLAGS "-pthread")

	# lfs on glibc, all compilers should use
	add_definitions(-D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64 -D_LARGEFILE64_SOURCE)

	# GNU Compiler
	if(CMAKE_COMPILER_IS_GNUCC)
		set(PLATFORM_CFLAGS "-pipe -fPIC -funsigned-char -fno-strict-aliasing")
	# CLang is the same as GCC for now.
	elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
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

elseif(WIN32)

	# this file is included anyway when building under Windows with cl.exe
	#  include(${CMAKE_ROOT}/Modules/Platform/Windows-cl.cmake)

	set(LIBDIR ${CMAKE_SOURCE_DIR}/../lib/windows)

	# Setup 64bit and 64bit windows systems
	if(CMAKE_CL_64)
		message("64 bit compiler detected.")
		set(LIBDIR ${CMAKE_SOURCE_DIR}/../lib/win64)
	endif()

	add_definitions(-DWIN32)

	if(WITH_INTERNATIONAL)
		set(ICONV ${LIBDIR}/iconv)
		set(ICONV_INCLUDE_DIRS ${ICONV}/include)
		set(ICONV_LIBRARIES iconv)
		set(ICONV_LIBPATH ${ICONV}/lib)
	endif()

	set(JPEG "${LIBDIR}/jpeg")
	set(JPEG_INCLUDE_DIR "${JPEG}/include")
	set(JPEG_LIBPATH ${JPEG}/lib) # not cmake defined

	set(WINTAB_INC ${LIBDIR}/wintab/include)

	if(WITH_OPENAL)
		set(OPENAL ${LIBDIR}/openal)
		set(OPENAL_INCLUDE_DIR ${OPENAL}/include)
		set(OPENAL_LIBRARY OpenAL32)
		set(OPENAL_LIBPATH ${OPENAL}/lib)
	endif()

	if(WITH_CODEC_SNDFILE)
		set(SNDFILE ${LIBDIR}/sndfile)
		set(SNDFILE_INCLUDE_DIRS ${SNDFILE}/include)
		set(SNDFILE_LIBRARIES libsndfile-1)
		set(SNDFILE_LIBPATH ${SNDFILE}/lib) # TODO, deprecate
	endif()

	if(WITH_SDL)
		set(SDL ${LIBDIR}/sdl)
		set(SDL_INCLUDE_DIR ${SDL}/include)
		set(SDL_LIBRARY SDL)
		set(SDL_LIBPATH ${SDL}/lib)
	endif()

	if(WITH_CODEC_QUICKTIME)
		set(QUICKTIME ${LIBDIR}/QTDevWin)
		set(QUICKTIME_INCLUDE_DIRS ${QUICKTIME}/CIncludes)
		set(QUICKTIME_LIBRARIES qtmlClient)
		set(QUICKTIME_LIBPATH ${QUICKTIME}/Libraries)
	endif()

	if(WITH_RAYOPTIMIZATION AND SUPPORT_SSE_BUILD)
		add_definitions(-D__SSE__ -D__MMX__)
	endif()

	if(MSVC)
		if(CMAKE_CL_64)
			set(PLATFORM_LINKLIBS ws2_32 vfw32 winmm kernel32 user32 gdi32 comdlg32 advapi32 shfolder shell32 ole32 oleaut32 uuid)
		else()
			set(PLATFORM_LINKLIBS ws2_32 vfw32 winmm kernel32 user32 gdi32 comdlg32 advapi32 shfolder shell32 ole32 oleaut32 uuid)
		endif()

		add_definitions(/D_CRT_NONSTDC_NO_DEPRECATE /D_CRT_SECURE_NO_DEPRECATE /D_SCL_SECURE_NO_DEPRECATE /D_CONSOLE /D_LIB)

		set(CMAKE_CXX_FLAGS "/nologo /J /W0 /Gd /wd4018 /wd4244 /wd4305 /wd4800 /wd4065 /wd4267 /we4013" CACHE STRING "MSVC MT C++ flags " FORCE)
		set(CMAKE_C_FLAGS   "/nologo /J /W0 /Gd /wd4018 /wd4244 /wd4305 /wd4800 /wd4065 /wd4267 /we4013 /EHsc" CACHE STRING "MSVC MT C++ flags " FORCE)

		if(CMAKE_CL_64)
			set(CMAKE_CXX_FLAGS_DEBUG "/Od /Gm /EHsc /RTC1 /MTd /W3 /nologo /Zi /J" CACHE STRING "MSVC MT flags " FORCE)
		else()
			set(CMAKE_CXX_FLAGS_DEBUG "/Od /Gm /EHsc /RTC1 /MTd /W3 /nologo /ZI /J" CACHE STRING "MSVC MT flags " FORCE)
		endif()
		set(CMAKE_CXX_FLAGS_RELEASE "/O2 /Ob2 /EHsc /MT /W3 /nologo /J" CACHE STRING "MSVC MT flags " FORCE)
		set(CMAKE_CXX_FLAGS_MINSIZEREL "/O1 /Ob1 /EHsc /MT /W3 /nologo /J" CACHE STRING "MSVC MT flags " FORCE)
		set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "/O2 /Ob1 /EHsc /MT /W3 /nologo /Zi /J" CACHE STRING "MSVC MT flags " FORCE)
		if(CMAKE_CL_64)
			set(CMAKE_C_FLAGS_DEBUG "/Od /Gm /EHsc /RTC1 /MTd /W3 /nologo /Zi /J" CACHE STRING "MSVC MT flags " FORCE)
		else()
			set(CMAKE_C_FLAGS_DEBUG "/Od /Gm /EHsc /RTC1 /MTd /W3 /nologo /ZI /J" CACHE STRING "MSVC MT flags " FORCE)
		endif()
		set(CMAKE_C_FLAGS_RELEASE "/O2 /Ob2 /EHsc /MT /W3 /nologo /J" CACHE STRING "MSVC MT flags " FORCE)
		set(CMAKE_C_FLAGS_MINSIZEREL "/O1 /Ob1 /EHsc /MT /W3 /nologo /J" CACHE STRING "MSVC MT flags " FORCE)
		set(CMAKE_C_FLAGS_RELWITHDEBINFO "/O2 /Ob1 /EHsc /MT /W3 /nologo /Zi /J" CACHE STRING "MSVC MT flags " FORCE)

		if(WITH_INTERNATIONAL)
			set(GETTEXT ${LIBDIR}/gettext)
			set(GETTEXT_INCLUDE_DIRS ${GETTEXT}/include)
			set(GETTEXT_LIBPATH ${GETTEXT}/lib)
			set(GETTEXT_LIBRARIES gnu_gettext)
		endif()

		set(PNG_LIBRARIES libpng)
		set(JPEG_LIBRARIES libjpeg)
		
		set(PNG "${LIBDIR}/png")
		set(PNG_INCLUDE_DIR "${PNG}/include")
		set(PNG_LIBPATH ${PNG}/lib) # not cmake defined

		set(ZLIB_INCLUDE_DIRS ${LIBDIR}/zlib/include)
		if(CMAKE_CL_64)
			set(ZLIB_LIBRARIES ${LIBDIR}/zlib/lib/libz.lib)
		else()
			set(ZLIB_LIBRARIES ${LIBDIR}/zlib/lib/zlib.lib)
		endif()

		set(PTHREADS_INCLUDE_DIRS ${LIBDIR}/pthreads/include)
		set(PTHREADS_LIBRARIES ${LIBDIR}/pthreads/lib/pthreadVC2.lib)

		set(FREETYPE ${LIBDIR}/freetype)
		set(FREETYPE_INCLUDE_DIRS
			${LIBDIR}/freetype/include
			${LIBDIR}/freetype/include/freetype2
		)
		set(FREETYPE_LIBRARY ${LIBDIR}/freetype/lib/freetype2ST.lib)

		if(WITH_FFTW3)
			set(FFTW3 ${LIBDIR}/fftw3)
			set(FFTW3_LIBRARIES libfftw)
			set(FFTW3_INCLUDE_DIRS ${FFTW3}/include)
			set(FFTW3_LIBPATH ${FFTW3}/lib)
		endif()

		if(WITH_OPENCOLLADA)

			set(OPENCOLLADA_INCLUDE_DIRS
				${LIBDIR}/opencollada/include/COLLADAStreamWriter/include
				${LIBDIR}/opencollada/include/COLLADABaseUtils/include
				${LIBDIR}/opencollada/include/COLLADAFramework/include
				${LIBDIR}/opencollada/include/COLLADASaxFrameworkLoader/include
				${LIBDIR}/opencollada/include/GeneratedSaxParser/include
			)
			
			set_lib_path(OPENCOLLADA_LIBPATH "opencollada/lib")

			set(OPENCOLLADA_LIBRARIES
				${OPENCOLLADA_LIBPATH}/OpenCOLLADASaxFrameworkLoader.lib
				${OPENCOLLADA_LIBPATH}/OpenCOLLADAFramework.lib
				${OPENCOLLADA_LIBPATH}/OpenCOLLADABaseUtils.lib
				${OPENCOLLADA_LIBPATH}/OpenCOLLADAStreamWriter.lib
				${OPENCOLLADA_LIBPATH}/MathMLSolver.lib
				${OPENCOLLADA_LIBPATH}/GeneratedSaxParser.lib
				${OPENCOLLADA_LIBPATH}/xml2.lib
				${OPENCOLLADA_LIBPATH}/buffer.lib
				${OPENCOLLADA_LIBPATH}/ftoa.lib
				${OPENCOLLADA_LIBPATH}/UTF.lib
			)
			set(PCRE_LIBRARIES
				${OPENCOLLADA_LIBPATH}/pcre.lib
			)
			
			unset(OPENCOLLADA_LIBPATH)
			
		endif()

		if(WITH_CODEC_FFMPEG)
			set(FFMPEG_INCLUDE_DIRS
				${LIBDIR}/ffmpeg/include
				${LIBDIR}/ffmpeg/include/msvc
			)
			set(FFMPEG_LIBRARIES
				${LIBDIR}/ffmpeg/lib/avcodec-53.lib
				${LIBDIR}/ffmpeg/lib/avformat-53.lib
				${LIBDIR}/ffmpeg/lib/avdevice-53.lib
				${LIBDIR}/ffmpeg/lib/avutil-51.lib
				${LIBDIR}/ffmpeg/lib/swscale-2.lib
			)
		endif()

		if(WITH_IMAGE_OPENEXR)
			set_lib_path(OPENEXR "openexr")
			set_lib_path(OPENEXR_LIBPATH "openexr/lib")
			set(OPENEXR_LIBRARIES
				${OPENEXR_LIBPATH}/Iex.lib
				${OPENEXR_LIBPATH}/Half.lib
				${OPENEXR_LIBPATH}/IlmImf.lib
				${OPENEXR_LIBPATH}/Imath.lib
				${OPENEXR_LIBPATH}/IlmThread.lib
			)
			set_lib_path(OPENEXR_INCUDE "openexr/include")
			set(OPENEXR_INCLUDE_DIRS
				${OPENEXR_INCUDE}
				${OPENEXR_INCUDE}/IlmImf
				${OPENEXR_INCUDE}/Iex
				${OPENEXR_INCUDE}/Imath
			)
			unset(OPENEXR_INCUDE)
			unset(OPENEXR_LIBPATH)
		endif()

		if(WITH_IMAGE_TIFF)
			set(TIFF_LIBRARY ${LIBDIR}/tiff/lib/libtiff.lib)
			set(TIFF_INCLUDE_DIR ${LIBDIR}/tiff/include)
		endif()

		if(WITH_JACK)
			set(JACK_INCLUDE_DIRS
				${LIBDIR}/jack/include/jack
				${LIBDIR}/jack/include
			)
			set(JACK_LIBRARIES ${LIBDIR}/jack/lib/libjack.lib)
		endif()

		if(WITH_PYTHON)
			# normally cached but not since we include them with blender
			set(PYTHON_VERSION 3.2) # CACHE STRING)
			set_lib_path(PYTHON_INCLUDE_DIR "python/include/python${PYTHON_VERSION}")
			set_lib_path(PYTHON_LIBRARY "python/lib/python32.lib") #CACHE FILEPATH)
			
			# uncached vars
			set(PYTHON_INCLUDE_DIRS "${PYTHON_INCLUDE_DIR}")
			set(PYTHON_LIBRARIES  "${PYTHON_LIBRARY}")
		endif()

		if(WITH_BOOST)
			set(BOOST ${LIBDIR}/boost)
			set(BOOST_INCLUDE_DIR ${BOOST}/include)
			if(MSVC10)
				set(BOOST_POSTFIX "vc100-mt-s-1_47.lib")
				set(BOOST_DEBUG_POSTFIX "vc100-mt-sgd-1_47.lib")
			else()
				set(BOOST_POSTFIX "vc90-mt-s-1_47.lib")
				set(BOOST_DEBUG_POSTFIX "vc90-mt-sgd-1_47.lib")					
			endif()
			set(BOOST_LIBRARIES
				optimized libboost_date_time-${BOOST_POSTFIX} optimized libboost_filesystem-${BOOST_POSTFIX}
				optimized libboost_regex-${BOOST_POSTFIX} optimized libboost_system-${BOOST_POSTFIX} optimized libboost_thread-${BOOST_POSTFIX}
				debug libboost_date_time-${BOOST_DEBUG_POSTFIX} debug libboost_filesystem-${BOOST_DEBUG_POSTFIX}
				debug libboost_regex-${BOOST_DEBUG_POSTFIX} debug libboost_system-${BOOST_DEBUG_POSTFIX} debug libboost_thread-${BOOST_DEBUG_POSTFIX})
			if(MSVC10)
				set(BOOST_LIBPATH ${BOOST}/lib/vc_10)
			else()
				set(BOOST_LIBPATH ${BOOST}/lib)
			endif()
			set(BOOST_DEFINITIONS "-DBOOST_ALL_NO_LIB")
		endif()
			
		if(WITH_OPENIMAGEIO)
			set(OPENIMAGEIO ${LIBDIR}/openimageio)
			set(OPENIMAGEIO_INCLUDE_DIRS ${OPENIMAGEIO}/include)
			set(OPENIMAGEIO_LIBRARIES OpenImageIO)
			set_lib_path(OPENIMAGEIO_LIBPATH "openimageio/lib")
			set(OPENIMAGEIO_DEFINITIONS)
		endif()

		set(PLATFORM_LINKFLAGS "/SUBSYSTEM:CONSOLE /STACK:2097152 /INCREMENTAL:NO /NODEFAULTLIB:msvcrt.lib /NODEFAULTLIB:msvcmrt.lib /NODEFAULTLIB:msvcurt.lib /NODEFAULTLIB:msvcrtd.lib")

		# MSVC only, Mingw doesnt need
		if(CMAKE_CL_64)
			set(PLATFORM_LINKFLAGS "/MACHINE:X64 /OPT:NOREF ${PLATFORM_LINKFLAGS}")
		else()
			set(PLATFORM_LINKFLAGS "/MACHINE:IX86 /LARGEADDRESSAWARE ${PLATFORM_LINKFLAGS}")
		endif()

		set(PLATFORM_LINKFLAGS_DEBUG "/NODEFAULTLIB:libcmt.lib /NODEFAULTLIB:libc.lib")

	else()
		# keep GCC specific stuff here
		if(CMAKE_COMPILER_IS_GNUCC)
			set(PLATFORM_LINKLIBS "-lshell32 -lshfolder -lgdi32 -lmsvcrt -lwinmm -lmingw32 -lm -lws2_32 -lz -lstdc++ -lole32 -luuid -lwsock32 -lpsapi")
			set(PLATFORM_CFLAGS "-pipe -funsigned-char -fno-strict-aliasing")

			add_definitions(-D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64 -D_LARGEFILE64_SOURCE)
		endif()

		add_definitions(-DFREE_WINDOWS)

		if(WITH_INTERNATIONAL)
			set(GETTEXT ${LIBDIR}/gcc/gettext)
			set(GETTEXT_INCLUDE_DIRS ${GETTEXT}/include)
			set(GETTEXT_LIBPATH ${GETTEXT}/lib)
			set(GETTEXT_LIBRARIES intl)
		endif()
		
		set(PNG "${LIBDIR}/gcc/png")
		set(PNG_INCLUDE_DIR "${PNG}/include")
		set(PNG_LIBPATH ${PNG}/lib) # not cmake defined

		set(JPEG_LIBRARIES libjpeg)
		set(PNG_LIBRARIES png)

		set(ZLIB ${LIBDIR}/zlib)
		set(ZLIB_INCLUDE_DIRS ${ZLIB}/include)
		set(ZLIB_LIBPATH ${ZLIB}/lib)
		set(ZLIB_LIBRARIES z)

		set(PTHREADS ${LIBDIR}/pthreads)
		set(PTHREADS_INCLUDE_DIRS ${PTHREADS}/include)
		set(PTHREADS_LIBPATH ${PTHREADS}/lib)
		set(PTHREADS_LIBRARIES pthreadGC2)

		set(FREETYPE ${LIBDIR}/gcc/freetype)
		set(FREETYPE_INCLUDE_DIRS ${FREETYPE}/include ${FREETYPE}/include/freetype2)
		set(FREETYPE_LIBPATH ${FREETYPE}/lib)
		set(FREETYPE_LIBRARY freetype)

		if(WITH_FFTW3)
			set(FFTW3 ${LIBDIR}/gcc/fftw3)
			set(FFTW3_LIBRARIES fftw3)
			set(FFTW3_INCLUDE_DIRS ${FFTW3}/include)
			set(FFTW3_LIBPATH ${FFTW3}/lib)
		endif()

		if(WITH_OPENCOLLADA)
			set(OPENCOLLADA ${LIBDIR}/gcc/opencollada)
			set(OPENCOLLADA_INCLUDE_DIRS
				${LIBDIR}/gcc/opencollada/include/COLLADAStreamWriter/include
				${LIBDIR}/gcc/opencollada/include/COLLADABaseUtils/include
				${LIBDIR}/gcc/opencollada/include/COLLADAFramework/include
				${LIBDIR}/gcc/opencollada/include/COLLADASaxFrameworkLoader/include
				${LIBDIR}/gcc/opencollada/include/GeneratedSaxParser/include
			)
			set(OPENCOLLADA_LIBPATH ${OPENCOLLADA}/lib ${OPENCOLLADA}/lib)
			set(OPENCOLLADA_LIBRARIES OpenCOLLADAStreamWriter OpenCOLLADASaxFrameworkLoader OpenCOLLADAFramework OpenCOLLADABaseUtils GeneratedSaxParser UTF MathMLSolver expat pcre buffer ftoa)
			set(PCRE_LIBRARIES pcre)
		endif()

		if(WITH_CODEC_FFMPEG)
			set(FFMPEG ${LIBDIR}/ffmpeg)
			set(FFMPEG_INCLUDE_DIRS ${FFMPEG}/include ${FFMPEG}/include)
			set(FFMPEG_LIBRARIES avcodec-53 avformat-53 avdevice-53 avutil-51 swscale-2)
			set(FFMPEG_LIBPATH ${FFMPEG}/lib)
		endif()

		if(WITH_IMAGE_OPENEXR)
			set(OPENEXR ${LIBDIR}/gcc/openexr)
			set(OPENEXR_INCLUDE_DIRS ${OPENEXR}/include/OpenEXR)
			set(OPENEXR_LIBRARIES Half IlmImf Imath IlmThread Iex)
			set(OPENEXR_LIBPATH ${OPENEXR}/lib)
		endif()

		if(WITH_IMAGE_TIFF)
			set(TIFF ${LIBDIR}/gcc/tiff)
			set(TIFF_LIBRARY tiff)
			set(TIFF_INCLUDE_DIR ${TIFF}/include)
			set(TIFF_LIBPATH ${TIFF}/lib)
		endif()

		if(WITH_JACK)
			set(JACK ${LIBDIR}/jack)
			set(JACK_INCLUDE_DIRS ${JACK}/include/jack ${JACK}/include)
			set(JACK_LIBRARIES jack)
			set(JACK_LIBPATH ${JACK}/lib)

			# TODO, gives linking errors, force off
			set(WITH_JACK OFF)
		endif()

		if(WITH_PYTHON)
			# normally cached but not since we include them with blender
			set(PYTHON_VERSION 3.2) #  CACHE STRING)
			set(PYTHON_INCLUDE_DIR "${LIBDIR}/python/include/python${PYTHON_VERSION}")  # CACHE PATH)
			set(PYTHON_LIBRARY "${LIBDIR}/python/lib/python32mw.lib")  # CACHE FILEPATH)

			# uncached vars
			set(PYTHON_INCLUDE_DIRS "${PYTHON_INCLUDE_DIR}")
			set(PYTHON_LIBRARIES  "${PYTHON_LIBRARY}")
		endif()

		if(WITH_BOOST)
			set(BOOST ${LIBDIR}/boost)
			set(BOOST_INCLUDE_DIR ${BOOST}/include)
			set(BOOST_POSTFIX "mgw45-mt-s-1_47")
			set(BOOST_DEBUG_POSTFIX "mgw45-mt-sd-1_47")
			set(BOOST_LIBRARIES
				optimized boost_date_time-${BOOST_POSTFIX} boost_filesystem-${BOOST_POSTFIX}
				boost_regex-${BOOST_POSTFIX} boost_system-${BOOST_POSTFIX} boost_thread-${BOOST_POSTFIX}
				debug boost_date_time-${BOOST_DEBUG_POSTFIX} boost_filesystem-${BOOST_DEBUG_POSTFIX}
				boost_regex-${BOOST_DEBUG_POSTFIX} boost_system-${BOOST_DEBUG_POSTFIX} boost_thread-${BOOST_DEBUG_POSTFIX})
			set(BOOST_LIBPATH ${BOOST}/lib/gcc)
			set(BOOST_DEFINITIONS "-DBOOST_ALL_NO_LIB -DBOOST_THREAD_USE_LIB ")
		endif()
			
		if(WITH_OPENIMAGEIO)
			set(OPENIMAGEIO ${LIBDIR}/gcc/openimageio)
			set(OPENIMAGEIO_INCLUDE_DIRS ${OPENIMAGEIO}/include)
			set(OPENIMAGEIO_LIBRARIES OpenImageIO)
			set(OPENIMAGEIO_LIBPATH ${OPENIMAGEIO}/lib)
			set(OPENIMAGEIO_DEFINITIONS)
		endif()
		
		set(PLATFORM_LINKFLAGS "-Xlinker --stack=2097152")

		## DISABLE - causes linking errors 
		## for re-distrobution, so users dont need mingw installed
		# set(PLATFORM_LINKFLAGS "${PLATFORM_LINKFLAGS} -static-libgcc -static-libstdc++")

	endif()

	# used in many places so include globally, like OpenGL
	blender_include_dirs("${PTHREADS_INCLUDE_DIRS}")

elseif(APPLE)

	if (${CMAKE_OSX_DEPLOYMENT_TARGET} STREQUAL "10.5")
		set(WITH_LIBS10.5 ON CACHE BOOL "Use 10.5 libs" FORCE)
	endif()

	if(WITH_LIBS10.5)
		set(LIBDIR ${CMAKE_SOURCE_DIR}/../lib/darwin-9.x.universal)
	else()
		if(CMAKE_OSX_ARCHITECTURES MATCHES i386)
			set(LIBDIR ${CMAKE_SOURCE_DIR}/../lib/darwin-8.x.i386)
		else()
			set(LIBDIR ${CMAKE_SOURCE_DIR}/../lib/darwin-8.0.0-powerpc)
		endif()
	endif()


	if(WITH_OPENAL)
		find_package(OpenAL)
		if(OPENAL_FOUND)
			set(WITH_OPENAL ON)
			set(OPENAL_INCLUDE_DIR "${LIBDIR}/openal/include")
		else()
			set(WITH_OPENAL OFF)
		endif()
	endif()

	if(WITH_JACK)
		set(JACK /usr)
		set(JACK_INCLUDE_DIRS ${JACK}/include/jack)
		set(JACK_LIBRARIES jack)
		set(JACK_LIBPATH ${JACK}/lib)
	endif()

	if(WITH_CODEC_SNDFILE)
		set(SNDFILE ${LIBDIR}/sndfile)
		set(SNDFILE_INCLUDE_DIRS ${SNDFILE}/include)
		set(SNDFILE_LIBRARIES sndfile FLAC ogg vorbis vorbisenc)
		set(SNDFILE_LIBPATH ${SNDFILE}/lib ${FFMPEG}/lib)  # TODO, deprecate
	endif()

	if(WITH_PYTHON)
		# we use precompiled libraries for py 3.2 and up by default

		# normally cached but not since we include them with blender
		set(PYTHON_VERSION 3.2)
		set(PYTHON_INCLUDE_DIR "${LIBDIR}/python/include/python${PYTHON_VERSION}")
		# set(PYTHON_BINARY "${LIBDIR}/python/bin/python${PYTHON_VERSION}") # not used yet
		set(PYTHON_LIBRARY python${PYTHON_VERSION})
		set(PYTHON_LIBPATH "${LIBDIR}/python/lib/python${PYTHON_VERSION}")
		# set(PYTHON_LINKFLAGS "-u _PyMac_Error")  # won't  build with this enabled

		# uncached vars
		set(PYTHON_INCLUDE_DIRS "${PYTHON_INCLUDE_DIR}")
		set(PYTHON_LIBRARIES  "${PYTHON_LIBRARY}")
	endif()

	if(WITH_INTERNATIONAL)
		set(GETTEXT ${LIBDIR}/gettext)
		set(GETTEXT_INCLUDE_DIRS "${GETTEXT}/include")
		set(GETTEXT_LIBRARIES intl iconv)
		set(GETTEXT_LIBPATH ${GETTEXT}/lib)
	endif()

	if(WITH_FFTW3)
		set(FFTW3 ${LIBDIR}/fftw3)
		set(FFTW3_INCLUDE_DIRS ${FFTW3}/include)
		set(FFTW3_LIBRARIES fftw3)
		set(FFTW3_LIBPATH ${FFTW3}/lib)
	endif()

	set(PNG_LIBRARIES png)
	set(JPEG_LIBRARIES jpeg)

	set(ZLIB /usr)
	set(ZLIB_INCLUDE_DIRS "${ZLIB}/include")
	set(ZLIB_LIBRARIES z bz2)

	set(FREETYPE ${LIBDIR}/freetype)
	set(FREETYPE_INCLUDE_DIRS ${FREETYPE}/include ${FREETYPE}/include/freetype2)
	set(FREETYPE_LIBPATH ${FREETYPE}/lib)
	set(FREETYPE_LIBRARY freetype)

	if(WITH_IMAGE_OPENEXR)
		set(OPENEXR ${LIBDIR}/openexr)
		set(OPENEXR_INCLUDE_DIRS ${OPENEXR}/include/OpenEXR)
		set(OPENEXR_LIBRARIES Iex Half IlmImf Imath IlmThread)
		set(OPENEXR_LIBPATH ${OPENEXR}/lib)
	endif()

	if(WITH_CODEC_FFMPEG)
		set(FFMPEG ${LIBDIR}/ffmpeg)
		set(FFMPEG_INCLUDE_DIRS ${FFMPEG}/include)
		set(FFMPEG_LIBRARIES avcodec avdevice avformat avutil mp3lame swscale x264 xvidcore theora theoradec theoraenc vorbis vorbisenc vorbisfile ogg)
		set(FFMPEG_LIBPATH ${FFMPEG}/lib)
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D__STDC_CONSTANT_MACROS")
	endif()

	find_library(SYSTEMSTUBS_LIBRARY
		NAMES
		SystemStubs
		PATHS
	)
	mark_as_advanced(SYSTEMSTUBS_LIBRARY)
	if(SYSTEMSTUBS_LIBRARY)
		set(PLATFORM_LINKLIBS stdc++ SystemStubs)
	else()
		set(PLATFORM_LINKLIBS stdc++)
	endif()

	if(WITH_COCOA)
		set(PLATFORM_CFLAGS "-pipe -funsigned-char -DGHOST_COCOA")
		set(PLATFORM_LINKFLAGS "-fexceptions -framework CoreServices -framework Foundation -framework IOKit -framework AppKit -framework Cocoa -framework Carbon -framework AudioUnit -framework AudioToolbox -framework CoreAudio")
		if(USE_QTKIT)
			set(PLATFORM_CFLAGS "${PLATFORM_CFLAGS} -DUSE_QTKIT")
			set(PLATFORM_LINKFLAGS "${PLATFORM_LINKFLAGS} -framework QTKit")
			if(CMAKE_OSX_ARCHITECTURES MATCHES i386)
				set(PLATFORM_LINKFLAGS "${PLATFORM_LINKFLAGS} -framework QuickTime")
				#libSDL still needs 32bit carbon quicktime
			endif()
		elseif(WITH_CODEC_QUICKTIME)
			set(PLATFORM_LINKFLAGS "${PLATFORM_LINKFLAGS} -framework QuickTime")
		endif()

		# XXX - SOME MAC DEV PLEASE TEST WITH THE SDK INSTALLED!
		# ALSO SHOULD BE MOVED INTO OWN MODULE WHEN FUNCTIONAL
		if(WITH_INPUT_NDOF)
			# This thread it *should* work and check the framework - campbell
			# http://www.cmake.org/pipermail/cmake/2005-December/007740.html
			find_library(3D_CONNEXION_CLIENT_LIBRARY
				NAMES 3DconnexionClient
			)
			if(NOT 3D_CONNEXION_CLIENT_LIBRARY)
				set(WITH_INPUT_NDOF OFF)
			endif()

			if(WITH_INPUT_NDOF)
				set(PLATFORM_LINKFLAGS "${PLATFORM_LINKFLAGS} -weak_framework 3DconnexionClient")
			endif()
		endif()

	else()
		set(PLATFORM_CFLAGS "-pipe -funsigned-char")
		set(PLATFORM_LINKFLAGS "-fexceptions -framework CoreServices -framework Foundation -framework IOKit -framework AppKit -framework Carbon -framework AGL -framework AudioUnit -framework AudioToolbox -framework CoreAudio -framework QuickTime")
		set(WITH_INPUT_NDOF OFF)  # unsupported
	endif()

	if(WITH_OPENCOLLADA)
		set(OPENCOLLADA ${LIBDIR}/opencollada)

		set(OPENCOLLADA_INCLUDE_DIRS
			${LIBDIR}/opencollada/include/COLLADAStreamWriter
			${LIBDIR}/opencollada/include/COLLADABaseUtils
			${LIBDIR}/opencollada/include/COLLADAFramework
			${LIBDIR}/opencollada/include/COLLADASaxFrameworkLoader
			${LIBDIR}/opencollada/include/GeneratedSaxParser
		)

		set(OPENCOLLADA_LIBPATH ${OPENCOLLADA}/lib)
		set(OPENCOLLADA_LIBRARIES "OpenCOLLADASaxFrameworkLoader -lOpenCOLLADAFramework -lOpenCOLLADABaseUtils -lOpenCOLLADAStreamWriter -lMathMLSolver -lGeneratedSaxParser -lUTF -lxml2 -lbuffer -lftoa" )
		#pcre is bundled with openCollada
		#set(PCRE ${LIBDIR}/pcre)
		#set(PCRE_LIBPATH ${PCRE}/lib)
		set(PCRE_LIBRARIES pcre)
		#libxml2 is used
		#set(EXPAT ${LIBDIR}/expat)
		#set(EXPAT_LIBPATH ${EXPAT}/lib)
		set(EXPAT_LIB)
	endif()

	if(WITH_SDL)
		set(SDL ${LIBDIR}/sdl)
		set(SDL_INCLUDE_DIR ${SDL}/include)
		set(SDL_LIBRARY SDL)
		set(SDL_LIBPATH ${SDL}/lib)
	endif()

	set(PNG "${LIBDIR}/png")
	set(PNG_INCLUDE_DIR "${PNG}/include")
	set(PNG_LIBPATH ${PNG}/lib)

	set(JPEG "${LIBDIR}/jpeg")
	set(JPEG_INCLUDE_DIR "${JPEG}/include")
	set(JPEG_LIBPATH ${JPEG}/lib)

	if(WITH_IMAGE_TIFF)
		set(TIFF ${LIBDIR}/tiff)
		set(TIFF_INCLUDE_DIR ${TIFF}/include)
		set(TIFF_LIBRARY tiff)
		set(TIFF_LIBPATH ${TIFF}/lib)
	endif()

	if (WITH_INPUT_NDOF)
		# linker needs "-weak_framework 3DconnexionClient"
	endif()

	if(WITH_BOOST)
		set(BOOST ${LIBDIR}/boost)
		set(BOOST_INCLUDE_DIR ${BOOST}/include)
		set(BOOST_LIBRARIES boost_date_time-mt boost_filesystem-mt boost_regex-mt boost_system-mt boost_thread-mt)
		set(BOOST_LIBPATH ${BOOST}/lib)
		set(BOOST_DEFINITIONS)
	endif()

	if(WITH_OPENIMAGEIO)
		set(OPENIMAGEIO ${LIBDIR}/openimageio)
		set(OPENIMAGEIO_INCLUDE_DIRS ${OPENIMAGEIO}/include)
		set(OPENIMAGEIO_LIBRARIES OpenImageIO ${PNG_LIBRARIES} ${JPEG_LIBRARIES} ${TIFF_LIBRARY} ${OPENEXR_LIBRARIES} ${ZLIB_LIBRARIES})
		set(OPENIMAGEIO_LIBPATH ${OPENIMAGEIO}/lib ${JPEG_LIBPATH} ${PNG_LIBPATH} ${TIFF_LIBPATH} ${OPENEXR_LIBPATH} ${ZLIB_LIBPATH})
		set(OPENIMAGEIO_DEFINITIONS "-DOIIO_STATIC_BUILD")
	endif()

	set(EXETYPE MACOSX_BUNDLE)

	set(CMAKE_C_FLAGS_DEBUG "-fno-strict-aliasing -g")
	set(CMAKE_CXX_FLAGS_DEBUG "-fno-strict-aliasing -g")
	if(CMAKE_OSX_ARCHITECTURES MATCHES "i386")
		set(CMAKE_CXX_FLAGS_RELEASE "-O2 -mdynamic-no-pic -ftree-vectorize -msse -msse2 -fvariable-expansion-in-unroller")
		set(CMAKE_C_FLAGS_RELEASE "-O2 -mdynamic-no-pic -ftree-vectorize -msse -msse2 -fvariable-expansion-in-unroller")
	elseif(CMAKE_OSX_ARCHITECTURES MATCHES "x86_64")
		set(CMAKE_CXX_FLAGS_RELEASE "-O2 -mdynamic-no-pic -ftree-vectorize -msse -msse2 -msse3 -mssse3 -fvariable-expansion-in-unroller")
		set(CMAKE_C_FLAGS_RELEASE "-O2 -mdynamic-no-pic -ftree-vectorize -msse -msse2 -msse3 -mssse3 -fvariable-expansion-in-unroller")
	else()
		set(CMAKE_C_FLAGS_RELEASE "-mdynamic-no-pic -fno-strict-aliasing")
		set(CMAKE_CXX_FLAGS_RELEASE "-mdynamic-no-pic -fno-strict-aliasing")
	endif()
endif()

#-----------------------------------------------------------------------------
# Common.

if(APPLE OR WIN32)
	if(NOT EXISTS "${LIBDIR}/")
		message(FATAL_ERROR "Apple and Windows require pre-compiled libs at: '${LIBDIR}'")
	endif()
endif()

# See TEST_SSE_SUPPORT() for how this is defined.

if(WITH_RAYOPTIMIZATION)
	if(SUPPORT_SSE_BUILD)
		set(PLATFORM_CFLAGS " ${COMPILER_SSE_FLAG} ${PLATFORM_CFLAGS}")
		add_definitions(-D__SSE__ -D__MMX__)
	endif()
	if(SUPPORT_SSE2_BUILD)
		set(PLATFORM_CFLAGS " ${COMPILER_SSE2_FLAG} ${PLATFORM_CFLAGS}")
		add_definitions(-D__SSE2__)
		if(NOT SUPPORT_SSE_BUILD) # dont double up
			add_definitions(-D__MMX__)
		endif()
	endif()
endif()


# set the endian define
if(MSVC)
	# for some reason this fails on msvc
	add_definitions(-D__LITTLE_ENDIAN__)
else()
	include(TestBigEndian)
	test_big_endian(_SYSTEM_BIG_ENDIAN)
	if(_SYSTEM_BIG_ENDIAN)
		add_definitions(-D__BIG_ENDIAN__)
	else()
		add_definitions(-D__LITTLE_ENDIAN__)
	endif()
	unset(_SYSTEM_BIG_ENDIAN)
endif()


if(WITH_IMAGE_OPENJPEG)
	if(UNIX AND NOT APPLE)
		# dealt with above
	else()
		set(OPENJPEG_INCLUDE_DIRS "${CMAKE_SOURCE_DIR}/extern/libopenjpeg")
	endif()
endif()

if(WITH_IMAGE_REDCODE)
	set(REDCODE ${CMAKE_SOURCE_DIR}/extern)
	set(REDCODE_INC ${REDCODE})
endif()

#-----------------------------------------------------------------------------
# Blender WebPlugin

if(WITH_WEBPLUGIN)
	set(GECKO_DIR "${CMAKE_SOURCE_DIR}/../gecko-sdk/" CACHE PATH "Gecko SDK path")
	set(WEBPLUGIN_SANDBOX_MODE "apparmor" CACHE STRING "WEB Plugin sandbox mode, can be apparmor, privsep, none")

	set(WITH_PLAYER ON)
endif()


#-----------------------------------------------------------------------------
# Configure OpenGL.
find_package(OpenGL)
blender_include_dirs_sys("${OPENGL_INCLUDE_DIR}")
# unset(OPENGL_LIBRARIES CACHE) # not compat with older cmake
# unset(OPENGL_xmesa_INCLUDE_DIR CACHE) # not compat with older cmake

#-----------------------------------------------------------------------------
# Configure OpenMP.
if(WITH_OPENMP)
	find_package(OpenMP)
	if(OPENMP_FOUND)
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")

		if(APPLE AND ${CMAKE_GENERATOR} MATCHES "Xcode")
			set(CMAKE_XCODE_ATTRIBUTE_ENABLE_OPENMP_SUPPORT "YES")
		endif()
	else()
		set(WITH_OPENMP OFF)
	endif()

	mark_as_advanced(
		OpenMP_C_FLAGS
		OpenMP_CXX_FLAGS
	)
endif()

#-----------------------------------------------------------------------------
# Configure GLEW

if(WITH_BUILTIN_GLEW)
	# set(GLEW_LIBRARY "")  # unused
	set(GLEW_INCLUDE_PATH "${CMAKE_SOURCE_DIR}/extern/glew/include")
else()
	find_package(GLEW)

	if(NOT GLEW_FOUND)
		message(FATAL_ERROR "GLEW is required to build blender, install it or use WITH_BUILTIN_GLEW")
	endif()

	mark_as_advanced(
		GLEW_LIBRARY
		GLEW_INCLUDE_PATH
	)
endif()

#-----------------------------------------------------------------------------
# Configure Python.

if(WITH_PYTHON_MODULE)
	add_definitions(-DPy_ENABLE_SHARED)
endif()

#-----------------------------------------------------------------------------
# Extra compile flags

if(CMAKE_COMPILER_IS_GNUCC)

	ADD_CHECK_C_COMPILER_FLAG(C_WARNINGS C_WARN_ALL -Wall)
	ADD_CHECK_C_COMPILER_FLAG(C_WARNINGS C_WARN_CAST_ALIGN -Wcast-align)
	ADD_CHECK_C_COMPILER_FLAG(C_WARNINGS C_WARN_ERROR_DECLARATION_AFTER_STATEMENT -Werror=declaration-after-statement)
	ADD_CHECK_C_COMPILER_FLAG(C_WARNINGS C_WARN_ERROR_IMPLICIT_FUNCTION_DECLARATION -Werror=implicit-function-declaration)
	ADD_CHECK_C_COMPILER_FLAG(C_WARNINGS C_WARN_ERROR_RETURN_TYPE -Werror=return-type)
	# system headers sometimes do this, disable for now, was: -Werror=strict-prototypes
	ADD_CHECK_C_COMPILER_FLAG(C_WARNINGS C_WARN_STRICT_PROTOTYPES -Wstrict-prototypes)
	ADD_CHECK_C_COMPILER_FLAG(C_WARNINGS C_WARN_NO_CHAR_SUBSCRIPTS -Wno-char-subscripts)
	ADD_CHECK_C_COMPILER_FLAG(C_WARNINGS C_WARN_NO_UNKNOWN_PRAGMAS -Wno-unknown-pragmas)
	ADD_CHECK_C_COMPILER_FLAG(C_WARNINGS C_WARN_POINTER_ARITH -Wpointer-arith)
	ADD_CHECK_C_COMPILER_FLAG(C_WARNINGS C_WARN_UNUSED_PARAMETER -Wunused-parameter)
	ADD_CHECK_C_COMPILER_FLAG(C_WARNINGS C_WARN_WRITE_STRINGS -Wwrite-strings)

	# # this causes too many warnings, disable
	# ADD_CHECK_C_COMPILER_FLAG(C_WARNINGS C_WARN_UNDEFINED -Wundef)

	# disable because it gives warnings for printf() & friends.
	# ADD_CHECK_C_COMPILER_FLAG(C_WARNINGS C_WARN_DOUBLE_PROMOTION -Wdouble-promotion -Wno-error=double-promotion)

	if(NOT APPLE)
		ADD_CHECK_C_COMPILER_FLAG(C_WARNINGS C_WARN_NO_ERROR_UNUSED_BUT_SET_VARIABLE -Wno-error=unused-but-set-variable)
	endif()

	ADD_CHECK_CXX_COMPILER_FLAG(CXX_WARNINGS CXX_WARN_ALL -Wall)
	ADD_CHECK_CXX_COMPILER_FLAG(CXX_WARNINGS CXX_WARN_NO_INVALID_OFFSETOF -Wno-invalid-offsetof)
	ADD_CHECK_CXX_COMPILER_FLAG(CXX_WARNINGS CXX_WARN_NO_SIGN_COMPARE -Wno-sign-compare)

	# # this causes too many warnings, disable
	# ADD_CHECK_CXX_COMPILER_FLAG(CXX_WARNINGS CXX_WARN_UNDEFINED -Wundef)

	# flags to undo strict flags
	ADD_CHECK_C_COMPILER_FLAG(CC_REMOVE_STRICT_FLAGS C_WARN_NO_DEPRECATED_DECLARATIONS -Wno-deprecated-declarations)
	ADD_CHECK_C_COMPILER_FLAG(CC_REMOVE_STRICT_FLAGS C_WARN_NO_UNUSED_PARAMETER        -Wno-unused-parameter)

	if(NOT APPLE)
		ADD_CHECK_C_COMPILER_FLAG(CC_REMOVE_STRICT_FLAGS C_WARN_NO_ERROR_UNUSED_BUT_SET_VARIABLE -Wno-error=unused-but-set-variable)
	endif()

elseif(CMAKE_C_COMPILER_ID MATCHES "Intel")

	ADD_CHECK_C_COMPILER_FLAG(C_WARNINGS C_WARN_ALL -Wall)
	ADD_CHECK_C_COMPILER_FLAG(C_WARNINGS C_WARN_POINTER_ARITH -Wpointer-arith)
	ADD_CHECK_C_COMPILER_FLAG(C_WARNINGS C_WARN_NO_UNKNOWN_PRAGMAS -Wno-unknown-pragmas)

	ADD_CHECK_CXX_COMPILER_FLAG(CXX_WARNINGS CXX_WARN_ALL -Wall)
	ADD_CHECK_CXX_COMPILER_FLAG(CXX_WARNINGS CXX_WARN_NO_INVALID_OFFSETOF -Wno-invalid-offsetof)
	ADD_CHECK_CXX_COMPILER_FLAG(CXX_WARNINGS CXX_WARN_NO_SIGN_COMPARE -Wno-sign-compare)

	# disable numbered, false positives
	set(C_WARNINGS "${C_WARNINGS} -wd188,186,144,913,556")
	set(CXX_WARNINGS "${CXX_WARNINGS} -wd188,186,144,913,556")

endif()

# MSVC2010 fails to links C++ libs right
if(MSVC10)
	if(WITH_IMAGE_OPENEXR)
		message(WARNING "MSVC 2010 does not support OpenEXR, disabling WITH_IMAGE_OPENEXR. To enable support use Use MSVC 2008")
	endif()
	if(WITH_OPENCOLLADA)
		message(WARNING "MSVC 2010 does not support OpenCollada, disabling WITH_OPENCOLLADA. To enable support use Use MSVC 2008")
	endif()
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
	if(WITH_IK_ITASC OR WITH_MOD_FLUID)
		message(WARNING "Using Clang as CXX compiler: disabling WITH_IK_ITASC and WITH_MOD_FLUID, these features will be missing.")
		set(WITH_IK_ITASC OFF)
		set(WITH_MOD_FLUID OFF)
	endif()
endif()


# ensure python header is found since detection can fail, this could happen
# with _any_ library but since we used a fixed python version this tends to
# be most problematic.
if(WITH_PYTHON)
	if(NOT EXISTS "${PYTHON_INCLUDE_DIR}/Python.h")
		message(FATAL_ERROR "Missing: \"${PYTHON_INCLUDE_DIR}/Python.h\",\n"
							"Set the cache entry 'PYTHON_INCLUDE_DIR' to point "
							"to a valid python include path. Containing "
							"Python.h for python version \"${PYTHON_VERSION}\"")
	endif()
endif()


set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${PLATFORM_CFLAGS} ${C_WARNINGS}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${PLATFORM_CFLAGS} ${CXX_WARNINGS}")

#-------------------------------------------------------------------------------
# Global Defines

# better not set includes here but this debugging option is off by default.
if(WITH_CXX_GUARDEDALLOC)
	include_directories(${CMAKE_SOURCE_DIR}/intern/guardedalloc)
endif()

if(WITH_ASSERT_ABORT)
	add_definitions(-DWITH_ASSERT_ABORT)
endif()

# message(STATUS "Using CFLAGS: ${CMAKE_C_FLAGS}")
# message(STATUS "Using CXXFLAGS: ${CMAKE_CXX_FLAGS}")

#-----------------------------------------------------------------------------
# Libraries

add_subdirectory(source)
add_subdirectory(intern)
add_subdirectory(extern)


#-----------------------------------------------------------------------------
# Blender Application
if(WITH_BLENDER)
	add_subdirectory(source/creator)
endif()


#-----------------------------------------------------------------------------
# Blender Player
if(WITH_PLAYER)
	add_subdirectory(source/blenderplayer)
endif()

#-----------------------------------------------------------------------------
# CPack for generating packages
include(build_files/cmake/packaging.cmake)


#-----------------------------------------------------------------------------
# Print Final Configuration

if(FIRST_RUN)
	set(_config_msg "\n* Blender Configuration *")
	macro(info_cfg_option
		_setting)
		set(_msg "   * ${_setting}")
		string(LENGTH "${_msg}" _len)
		while("28" GREATER "${_len}")
			set(_msg "${_msg} ")
			 math(EXPR _len "${_len} + 1")
		endwhile()

		set(_config_msg "${_config_msg}\n${_msg}${${_setting}}")
	endmacro()

	macro(info_cfg_text
		_text)
		set(_config_msg "${_config_msg}\n\n  ${_text}")


	endmacro()

	info_cfg_text("Build Options:")
	info_cfg_option(WITH_GAMEENGINE)
	info_cfg_option(WITH_PLAYER)
	info_cfg_option(WITH_BULLET)
	info_cfg_option(WITH_IK_ITASC)
	info_cfg_option(WITH_OPENCOLLADA)
	info_cfg_option(WITH_FFTW3)
	info_cfg_option(WITH_INTERNATIONAL)
	info_cfg_option(WITH_INPUT_NDOF)
	info_cfg_option(WITH_CYCLES)

	info_cfg_text("Compiler Options:")
	info_cfg_option(WITH_BUILDINFO)
	info_cfg_option(WITH_OPENMP)
	info_cfg_option(WITH_RAYOPTIMIZATION)

	info_cfg_text("System Options:")
	info_cfg_option(WITH_INSTALL_PORTABLE)
	info_cfg_option(WITH_X11_XF86VMODE)
	info_cfg_option(WITH_X11_XINPUT)
	info_cfg_option(WITH_BUILTIN_GLEW)

	info_cfg_text("Image Formats:")
	info_cfg_option(WITH_IMAGE_CINEON)
	info_cfg_option(WITH_IMAGE_DDS)
	info_cfg_option(WITH_IMAGE_HDR)
	info_cfg_option(WITH_IMAGE_OPENEXR)
	info_cfg_option(WITH_IMAGE_OPENJPEG)
	info_cfg_option(WITH_IMAGE_REDCODE)
	info_cfg_option(WITH_IMAGE_TIFF)

	info_cfg_text("Audio:")
	info_cfg_option(WITH_OPENAL)
	info_cfg_option(WITH_SDL)
	info_cfg_option(WITH_JACK)
	info_cfg_option(WITH_CODEC_FFMPEG)
	info_cfg_option(WITH_CODEC_SNDFILE)

	info_cfg_text("Compression:")
	info_cfg_option(WITH_LZMA)
	info_cfg_option(WITH_LZO)

	info_cfg_text("Python:")
	info_cfg_option(WITH_PYTHON_INSTALL)
	info_cfg_option(WITH_PYTHON_MODULE)
	info_cfg_option(WITH_PYTHON_SAFETY)

	info_cfg_text("Modifiers:")
	info_cfg_option(WITH_MOD_BOOLEAN)
	info_cfg_option(WITH_MOD_REMESH)
	info_cfg_option(WITH_MOD_DECIMATE)
	info_cfg_option(WITH_MOD_FLUID)
	info_cfg_option(WITH_MOD_OCEANSIM)

	info_cfg_text("")

	message("${_config_msg}")
endif()
