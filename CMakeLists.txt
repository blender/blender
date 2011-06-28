# -*- mode: cmake; indent-tabs-mode: t; -*-
# $Id$
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

# set default build type to Release
if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE "Release")
endif()

# this starts out unset
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/build_files/cmake/Modules")

# avoid having empty buildtype
set(CMAKE_BUILD_TYPE_INIT "Release")

# quiet output for Makefiles, 'make -s' helps too
# set_property(GLOBAL PROPERTY RULE_MESSAGES OFF)

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

option(WITH_AUDASPACE    "Build with blenders audio library" ON)
mark_as_advanced(WITH_AUDASPACE)

option(WITH_HEADLESS      "Build without graphical support (renderfarm, server mode only)" OFF)
mark_as_advanced(WITH_HEADLESS)

# (unix defaults to OpenMP On)
if(UNIX AND NOT APPLE)
	option(WITH_OPENMP        "Enable OpenMP (has to be supported by the compiler)" ON)
else()
	option(WITH_OPENMP        "Enable OpenMP (has to be supported by the compiler)" OFF)
endif()

if(UNIX AND NOT APPLE)
	option(WITH_X11_XINPUT "Enable X11 Xinput (tablet support)"	ON)
	option(WITH_BUILTIN_GLEW "Use GLEW OpenGL wrapper library bundled with blender" ON)
else()
	# not an option for other OS's
	set(WITH_BUILTIN_GLEW ON)
endif()

# Modifiers
option(WITH_MOD_FLUID        	"Enable Elbeem Modifier (Fluid Simulation)" ON)
option(WITH_MOD_DECIMATE       	"Enable Decimate Modifier" ON)
option(WITH_MOD_BOOLEAN        	"Enable Boolean Modifier" ON)
option(WITH_MOD_CLOTH_ELTOPO   	"Enable Experemental cloth solver" OFF)
mark_as_advanced(WITH_MOD_CLOTH_ELTOPO)

# Image format support
option(WITH_IMAGE_OPENEXR       "Enable OpenEXR Support (http://www.openexr.com)" ON)
option(WITH_IMAGE_OPENJPEG      "Enable OpenJpeg Support (http://www.openjpeg.org)" ON)
option(WITH_IMAGE_TIFF          "Enable LibTIFF Support" ON)
option(WITH_IMAGE_DDS           "Enable DDS Image Support" ON)
option(WITH_IMAGE_CINEON        "Enable CINEON and DPX Image Support" ON)
option(WITH_IMAGE_HDR           "Enable HDR Image Support" ON)
option(WITH_IMAGE_REDCODE       "Enable RedCode Image Support" OFF)

# Audio/Video format support
option(WITH_CODEC_FFMPEG        "Enable FFMPeg Support (http://ffmpeg.mplayerhq.hu)" OFF)
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
option(WITH_SAMPLERATE    "Enable samplerate conversion" ON)

# Compression
option(WITH_LZO           "Enable fast LZO compression (used for pointcache)" ON)
option(WITH_LZMA          "Enable best LZMA compression, (used for pointcache)" ON)

# Misc
option(WITH_RAYOPTIMIZATION	"Enable use of SIMD (SSE) optimizations for the raytracer" ON) 
if(UNIX AND NOT APPLE)
	option(WITH_INSTALL_PORTABLE "Install redistributeable runtime, otherwise install into CMAKE_INSTALL_PREFIX" ON)
endif()
option(WITH_PYTHON_INSTALL       "Copy system python into the blender install folder" ON)

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

if(NOT WITH_GAMEENGINE AND WITH_PLAYER)
	message(FATAL_ERROR "WITH_PLAYER requires WITH_GAMEENGINE")
endif()

if(NOT WITH_AUDASPACE AND (WITH_OPENAL OR WITH_SDL OR WITH_JACK))
	message(FATAL_ERROR "WITH_OPENAL/WITH_SDL/WITH_JACK/WITH_CODEC_FFMPEG require WITH_AUDASPACE")
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

if(WITH_CODEC_QUICKTIME AND MINGW)
	message(FATAL_ERROR "MINGW requires WITH_CODEC_QUICKTIME to be OFF "
						"because it is currently unsupported, remove this "
						"line if youre a developer who wants to add support.")
endif()

# may as well build python module without a UI
if(WITH_PYTHON_MODULE)
	set(WITH_HEADLESS ON)
endif()

TEST_SSE_SUPPORT()

#-----------------------------------------------------------------------------
# Initialize un-cached vars, avoid unused warning

# linux only, not cached
set(WITH_BINRELOC OFF)

# MAXOSX only, set to avoid uninitialized 
set(EXETYPE)

# C/C++ flags
set(PLATFORM_CFLAGS)

# these are added to later on.
set(C_WARNINGS)
set(CXX_WARNINGS)

# libraries to link the binary with passed to target_link_libraries()
# known as LLIBS to scons
set(PLATFORM_LINKLIBS)

# Added to linker flags in setup_liblinks
# - CMAKE_EXE_LINKER_FLAGS
# - CMAKE_EXE_LINKER_FLAGS_DEBUG
set(PLATFORM_LINKFLAGS)
set(PLATFORM_LINKFLAGS_DEBUG)


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
		mark_as_advanced(SDLMAIN_LIBRARY)
		mark_as_advanced(SDL_INCLUDE_DIR)
		mark_as_advanced(SDL_LIBRARY)
		mark_as_advanced(SDL_LIBRARY_TEMP)
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
		set(FFMPEG /usr CACHE FILEPATH "FFMPEG Directory")
		mark_as_advanced(FFMPEG)
		set(FFMPEG_INCLUDE_DIRS ${FFMPEG}/include)
		set(FFMPEG_LIBRARIES avformat avcodec avutil avdevice swscale CACHE STRING "FFMPEG Libraries")
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
		mark_as_advanced(ICONV_LIBRARY)
		mark_as_advanced(INTL_LIBRARY)

		if(INTL_LIBRARY AND ICONV_LIBRARY)
			set(GETTEXT_LIB ${INTL_LIBRARY} ${ICONV_LIBRARY})
		endif()
	endif()

	if(WITH_FFTW3)
		find_package(Fftw3)
		if(NOT FFTW3_FOUND)
			set(WITH_FFTW3 OFF)
		endif()
	endif()

	if(WITH_SAMPLERATE)
		find_package(Samplerate)
		if(NOT SAMPLERATE_FOUND)
			set(WITH_SAMPLERATE OFF)
		endif()
	endif()

	if(WITH_OPENCOLLADA)
		set(OPENCOLLADA /usr/local/opencollada CACHE FILEPATH "OpenCollada Directory")
		mark_as_advanced(OPENCOLLADA)
		set(OPENCOLLADA_LIBPATH ${OPENCOLLADA}/lib)
		set(OPENCOLLADA_LIBRARIES OpenCOLLADAStreamWriter OpenCOLLADASaxFrameworkLoader OpenCOLLADAFramework OpenCOLLADABaseUtils GeneratedSaxParser UTF MathMLSolver pcre ftoa buffer xml2)
		set(OPENCOLLADA_INCLUDE_DIR ${OPENCOLLADA})

		set(PCRE /usr CACHE FILEPATH "PCRE Directory")
		mark_as_advanced(PCRE)
		set(PCRE_LIBPATH ${PCRE}/lib)
		set(PCRE_LIB pcre)

		set(EXPAT /usr CACHE FILEPATH "Expat Directory")
		mark_as_advanced(EXPAT)
		set(EXPAT_LIBPATH ${EXPAT}/lib)
		set(EXPAT_LIB expat)
	endif()

	if(WITH_MEM_JEMALLOC)
		find_package(JeMalloc)
		if(NOT JEMALLOC_FOUND)
			set(WITH_MEM_JEMALLOC OFF)
		endif()
	endif()

	# OpenSuse needs lutil, ArchLinux not, for now keep, can avoid by using --as-needed
	set(PLATFORM_LINKLIBS "-lutil -lc -lm -lpthread -lstdc++")

	if(NOT WITH_HEADLESS)
		find_package(X11 REQUIRED)
		find_path(X11_XF86keysym_INCLUDE_PATH X11/XF86keysym.h ${X11_INC_SEARCH_PATH})
		mark_as_advanced(X11_XF86keysym_INCLUDE_PATH)

		list(APPEND PLATFORM_LINKLIBS ${X11_X11_LIB})

		if(WITH_X11_XINPUT)
			list(APPEND PLATFORM_LINKLIBS ${X11_Xinput_LIB})
		endif()
	endif()

	if(CMAKE_SYSTEM_NAME MATCHES "Linux")
		if(NOT WITH_PYTHON_MODULE)
			# BSD's dont use libdl.so
			list(APPEND PLATFORM_LINKLIBS -ldl)
			# binreloc is linux only
			set(BINRELOC_INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/extern/binreloc/include)
			set(WITH_BINRELOC ON)
		endif()
	endif()

	set(PLATFORM_LINKFLAGS "-pthread")

	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64 -D_LARGEFILE64_SOURCE")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64 -D_LARGEFILE64_SOURCE")

	# GNU Compiler
	if(CMAKE_COMPILER_IS_GNUCC)
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

	if(WITH_SAMPLERATE)
		set(SAMPLERATE ${LIBDIR}/samplerate)
		set(SAMPLERATE_INCLUDE_DIRS ${SAMPLERATE}/include)
		set(SAMPLERATE_LIBRARIES libsamplerate)
		set(SAMPLERATE_LIBPATH ${SAMPLERATE}/lib)
	endif()

	set(PNG "${LIBDIR}/png")
	set(PNG_INCLUDE_DIR "${PNG}/include")
	set(PNG_LIBPATH ${PNG}/lib) # not cmake defined

	set(JPEG "${LIBDIR}/jpeg")
	set(JPEG_INCLUDE_DIR "${JPEG}/include")
	set(JPEG_LIBPATH ${JPEG}/lib) # not cmake defined

	set(WINTAB_INC ${LIBDIR}/wintab/include) 

	if(WITH_OPENAL)
		set(OPENAL ${LIBDIR}/openal)
		set(OPENAL_INCLUDE_DIR ${OPENAL}/include)
		set(OPENAL_LIBRARY wrap_oal)
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
			set(CMAKE_CXX_FLAGS_DEBUG "/D_DEBUG /Od /Gm /EHsc /RTC1 /MTd /W3 /nologo /Zi /J" CACHE STRING "MSVC MT flags " FORCE)
		else()
			set(CMAKE_CXX_FLAGS_DEBUG "/D_DEBUG /Od /Gm /EHsc /RTC1 /MTd /W3 /nologo /ZI /J" CACHE STRING "MSVC MT flags " FORCE)
		endif()
		set(CMAKE_CXX_FLAGS_RELEASE "/O2 /Ob2 /DNDEBUG /EHsc /MT /W3 /nologo /J" CACHE STRING "MSVC MT flags " FORCE)
		set(CMAKE_CXX_FLAGS_MINSIZEREL "/O1 /Ob1 /DNDEBUG /EHsc /MT /W3 /nologo /J" CACHE STRING "MSVC MT flags " FORCE)
		set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "/O2 /Ob1 /DNDEBUG /EHsc /MT /W3 /nologo /Zi /J" CACHE STRING "MSVC MT flags " FORCE)
		if(CMAKE_CL_64)
			set(CMAKE_C_FLAGS_DEBUG "/D_DEBUG /Od /Gm /EHsc /RTC1 /MTd /W3 /nologo /Zi /J" CACHE STRING "MSVC MT flags " FORCE)
		else()
			set(CMAKE_C_FLAGS_DEBUG "/D_DEBUG /Od /Gm /EHsc /RTC1 /MTd /W3 /nologo /ZI /J" CACHE STRING "MSVC MT flags " FORCE)
		endif()
		set(CMAKE_C_FLAGS_RELEASE "/O2 /Ob2 /DNDEBUG /EHsc /MT /W3 /nologo /J" CACHE STRING "MSVC MT flags " FORCE)
		set(CMAKE_C_FLAGS_MINSIZEREL "/O1 /Ob1 /DNDEBUG /EHsc /MT /W3 /nologo /J" CACHE STRING "MSVC MT flags " FORCE)
		set(CMAKE_C_FLAGS_RELWITHDEBINFO "/O2 /Ob1 /DNDEBUG /EHsc /MT /W3 /nologo /Zi /J" CACHE STRING "MSVC MT flags " FORCE)

		if(WITH_INTERNATIONAL)
			set(GETTEXT ${LIBDIR}/gettext)
			set(GETTEXT_INC ${GETTEXT}/include)
			set(GETTEXT_LIBPATH ${GETTEXT}/lib)
			if(CMAKE_CL_64)
				set(GETTEXT_LIB gettext)
			else()
				set(GETTEXT_LIB gnu_gettext)
			endif()
		endif()

		if(CMAKE_CL_64)
			set(PNG_LIBRARIES libpng)
		else()
			set(PNG_LIBRARIES libpng_st)
		endif()
		set(JPEG_LIBRARIES libjpeg)

		set(ZLIB_INCLUDE_DIRS ${LIBDIR}/zlib/include)
		if(CMAKE_CL_64)
			set(ZLIB_LIBRARIES ${LIBDIR}/zlib/lib/libz.lib)
		else()
			set(ZLIB_LIBRARIES ${LIBDIR}/zlib/lib/zlib.lib)
		endif()

		set(PTHREADS_INCLUDE_DIRS ${LIBDIR}/pthreads/include)
		set(PTHREADS_LIBRARIES ${LIBDIR}/pthreads/lib/pthreadVC2.lib)

		set(FREETYPE_INCLUDE_DIRS
			${FREETYPE}/include
			${FREETYPE}/include/freetype2
		)
		set(FREETYPE_LIBRARY ${LIBDIR}/freetype/lib/freetype2ST.lib)

		if(WITH_FFTW3)
			set(FFTW3 ${LIBDIR}/fftw3)
			set(FFTW3_LIBRARIES libfftw)
			set(FFTW3_INCLUDE_DIRS ${FFTW3}/include)
			set(FFTW3_LIBPATH ${FFTW3}/lib)
		endif()

		if(WITH_OPENCOLLADA)
			set(OPENCOLLADA_INCLUDE_DIR
				${LIBDIR}/opencollada/include
			)
			set(OPENCOLLADA_LIBRARIES
				${LIBDIR}/opencollada/lib/OpenCOLLADASaxFrameworkLoader.lib
				${LIBDIR}/opencollada/lib/OpenCOLLADAFramework.lib
				${LIBDIR}/opencollada/lib/OpenCOLLADABaseUtils.lib
				${LIBDIR}/opencollada/lib/OpenCOLLADAStreamWriter.lib
				${LIBDIR}/opencollada/lib/MathMLSolver.lib
				${LIBDIR}/opencollada/lib/GeneratedSaxParser.lib
				${LIBDIR}/opencollada/lib/xml2.lib
				${LIBDIR}/opencollada/lib/buffer.lib
				${LIBDIR}/opencollada/lib/ftoa.lib
				${LIBDIR}/opencollada/lib/UTF.lib
			)
			set(PCRE_LIB
				${LIBDIR}/opencollada/lib/pcre.lib
			)
		endif()

		if(WITH_CODEC_FFMPEG)
			set(FFMPEG_INCLUDE_DIRS
				${LIBDIR}/ffmpeg/include
				${LIBDIR}/ffmpeg/include/msvc
			)
			set(FFMPEG_LIBRARIES
				${LIBDIR}/ffmpeg/lib/avcodec-52.lib
				${LIBDIR}/ffmpeg/lib/avformat-52.lib
				${LIBDIR}/ffmpeg/lib/avdevice-52.lib
				${LIBDIR}/ffmpeg/lib/avutil-50.lib
				${LIBDIR}/ffmpeg/lib/swscale-0.lib
			)
		endif()

		if(WITH_IMAGE_OPENEXR)
			if(MSVC90)
				set(MSVC_LIB _vs2008)
				set(MSVC_INC)
			elseif(MSVC10)
				set(MSVC_LIB _vs2010)
				set(MSVC_INC _vs2010)
			else()
				set(MSVC_LIB msvc)
				set(MSVC_INC)
			endif()
			set(OPENEXR ${LIBDIR}/openexr)
			set(OPENEXR_LIBPATH ${OPENEXR}/lib${MSVC_LIB})
			set(OPENEXR_LIBRARIES
				${OPENEXR_LIBPATH}/Iex.lib
				${OPENEXR_LIBPATH}/Half.lib
				${OPENEXR_LIBPATH}/IlmImf.lib
				${OPENEXR_LIBPATH}/Imath.lib
				${OPENEXR_LIBPATH}/IlmThread.lib
			)
			set(OPENEXR_INCUDE ${OPENEXR}/include${MSVC_INC})
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
			set(PYTHON_VERSION 3.2)
			set(PYTHON_INCLUDE_DIRS "${LIBDIR}/python/include/python${PYTHON_VERSION}")
			# set(PYTHON_BINARY python) # not used yet
			set(PYTHON_LIBRARIES ${LIBDIR}/python/lib/python32.lib)
		endif()

		set(PLATFORM_LINKFLAGS "/SUBSYSTEM:CONSOLE /STACK:2097152 /INCREMENTAL:NO  /NODEFAULTLIB:msvcrt.lib\;msvcmrt.lib\;msvcurt.lib\;msvcrtd.lib ")

		# MSVC only, Mingw doesnt need
		if(CMAKE_CL_64)
			set(PLATFORM_LINKFLAGS "/MACHINE:X64 /OPT:NOREF ${PLATFORM_LINKFLAGS}")
		else()
			set(PLATFORM_LINKFLAGS "/MACHINE:IX86 /LARGEADDRESSAWARE ${PLATFORM_LINKFLAGS}")
		endif()

		set(PLATFORM_LINKFLAGS_DEBUG "/NODEFAULTLIB:libcmt.lib\;libc.lib")

	else()
		# keep GCC spesific stuff here
		if(CMAKE_COMPILER_IS_GNUCC)
			set(PLATFORM_LINKLIBS "-lshell32 -lshfolder -lgdi32 -lmsvcrt -lwinmm -lmingw32 -lm -lws2_32 -lz -lstdc++ -lole32 -luuid")
			set(PLATFORM_CFLAGS "-pipe -funsigned-char -fno-strict-aliasing")

			add_definitions(-D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64 -D_LARGEFILE64_SOURCE)
		endif()

		add_definitions(-DFREE_WINDOWS)

		set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -D_DEBUG")
		set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -D_DEBUG")

		if(WITH_INTERNATIONAL)
			set(GETTEXT ${LIBDIR}/gcc/gettext)
			set(GETTEXT_INC ${GETTEXT}/include)
			set(GETTEXT_LIBPATH ${GETTEXT}/lib)
			set(GETTEXT_LIB intl)
		endif()

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
			set(OPENCOLLADA_INCLUDE_DIR ${OPENCOLLADA}/include)
			set(OPENCOLLADA_LIBPATH ${OPENCOLLADA}/lib ${OPENCOLLADA}/lib)
			set(OPENCOLLADA_LIBRARIES OpenCOLLADAStreamWriter OpenCOLLADASaxFrameworkLoader OpenCOLLADAFramework OpenCOLLADABaseUtils GeneratedSaxParser UTF MathMLSolver expat pcre buffer ftoa)
			set(PCRE_LIB pcre)
		endif()
		
		if(WITH_CODEC_FFMPEG)
			set(FFMPEG ${LIBDIR}/ffmpeg)
			set(FFMPEG_INCLUDE_DIRS ${FFMPEG}/include ${FFMPEG}/include)
			set(FFMPEG_LIBRARIES avcodec-52 avformat-52 avdevice-52 avutil-50 swscale-0)
			set(FFMPEG_LIBPATH ${FFMPEG}/lib)
		endif()

		if(WITH_IMAGE_OPENEXR)
			set(OPENEXR ${LIBDIR}/gcc/openexr)
			set(OPENEXR_INCLUDE_DIRS ${OPENEXR}/include/OpenEXR)
			set(OPENEXR_LIBRARIES Half IlmImf Imath IlmThread)
			set(OPENEXR_LIBPATH ${OPENEXR}/lib)

			# TODO, gives linking errors, force off
			set(WITH_IMAGE_OPENEXR OFF)
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
			set(PYTHON ${LIBDIR}/python)
			set(PYTHON_VERSION 3.2)
			set(PYTHON_INCLUDE_DIRS "${PYTHON}/include/python${PYTHON_VERSION}")
			# set(PYTHON_BINARY python) # not used yet
			set(PYTHON_LIBRARIES ${PYTHON}/lib/python32mw.lib)
			set(PYTHON_LIBPATH ${PYTHON}/lib)
		endif()

		set(PLATFORM_LINKFLAGS "--stack,2097152")

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

	set(PYTHON_VERSION 3.2)

	if(PYTHON_VERSION MATCHES 3.2)
		# we use precompiled libraries for py 3.2 and up by default

		set(PYTHON ${LIBDIR}/python)
		set(PYTHON_INCLUDE_DIRS "${PYTHON}/include/python${PYTHON_VERSION}")
		# set(PYTHON_BINARY "${PYTHON}/bin/python${PYTHON_VERSION}") # not used yet
		set(PYTHON_LIBRARIES python${PYTHON_VERSION})
		set(PYTHON_LIBPATH "${PYTHON}/lib/python${PYTHON_VERSION}")
		# set(PYTHON_LINKFLAGS "-u _PyMac_Error")  # won't  build with this enabled
	else()
		# otherwise, use custom system framework

		set(PYTHON /System/Library/Frameworks/Python.framework/Versions/)
		set(PYTHON_VERSION 3.2)
		set(PYTHON_INCLUDE_DIRS "${PYTHON}${PYTHON_VERSION}/include/python${PYTHON_VERSION}")
		# set(PYTHON_BINARY ${PYTHON}${PYTHON_VERSION}/bin/python${PYTHON_VERSION}) # not used yet
		set(PYTHON_LIBRARIES "")
		set(PYTHON_LIBPATH ${PYTHON}${PYTHON_VERSION}/lib/python${PYTHON_VERSION}/config)
		set(PYTHON_LINKFLAGS "-u _PyMac_Error -framework System -framework Python")
	endif()

	if(WITH_INTERNATIONAL)
		set(GETTEXT ${LIBDIR}/gettext)
		set(GETTEXT_INC "${GETTEXT}/include")
		set(GETTEXT_LIB intl iconv)
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

	if(WITH_SAMPLERATE)
		set(SAMPLERATE ${LIBDIR}/samplerate)
		set(SAMPLERATE_INCLUDE_DIRS ${SAMPLERATE}/include)
		set(SAMPLERATE_LIBRARIES samplerate)
		set(SAMPLERATE_LIBPATH ${SAMPLERATE}/lib)
	endif()

	set(PLATFORM_LINKLIBS stdc++ SystemStubs)

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
	else()
		set(PLATFORM_CFLAGS "-pipe -funsigned-char")
		set(PLATFORM_LINKFLAGS "-fexceptions -framework CoreServices -framework Foundation -framework IOKit -framework AppKit -framework Carbon -framework AGL -framework AudioUnit -framework AudioToolbox -framework CoreAudio -framework QuickTime")
	endif()

	if(WITH_OPENCOLLADA)
		set(OPENCOLLADA ${LIBDIR}/opencollada)
		set(OPENCOLLADA_INCLUDE_DIR ${OPENCOLLADA}/include)
		set(OPENCOLLADA_LIBPATH ${OPENCOLLADA}/lib)
		set(OPENCOLLADA_LIBRARIES "OpenCOLLADASaxFrameworkLoader -lOpenCOLLADAFramework -lOpenCOLLADABaseUtils -lOpenCOLLADAStreamWriter -lMathMLSolver -lGeneratedSaxParser -lUTF -lxml2 -lbuffer -lftoa" )
		#pcre is bundled with openCollada
		#set(PCRE ${LIBDIR}/pcre)
		#set(PCRE_LIBPATH ${PCRE}/lib)
		set(PCRE_LIB pcre)
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

	set(EXETYPE MACOSX_BUNDLE)

	set(CMAKE_C_FLAGS_DEBUG "-fno-strict-aliasing -g")
	set(CMAKE_CXX_FLAGS_DEBUG "-fno-strict-aliasing -g")
	if(CMAKE_OSX_ARCHITECTURES MATCHES "i386")
		set(CMAKE_CXX_FLAGS_RELEASE "-O3 -mdynamic-no-pic -ftree-vectorize -msse -msse2 -fvariable-expansion-in-unroller")
		set(CMAKE_C_FLAGS_RELEASE "-O3 -mdynamic-no-pic -ftree-vectorize -msse -msse2 -fvariable-expansion-in-unroller")
	elseif(CMAKE_OSX_ARCHITECTURES MATCHES "x86_64")
		set(CMAKE_CXX_FLAGS_RELEASE "-O3 -mdynamic-no-pic -ftree-vectorize -msse -msse2 -msse3 -mssse3 -fvariable-expansion-in-unroller")
		set(CMAKE_C_FLAGS_RELEASE "-O3 -mdynamic-no-pic -ftree-vectorize -msse -msse2 -msse3 -mssse3 -fvariable-expansion-in-unroller")
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

if(WITH_RAYOPTIMIZATION)
	if(CMAKE_COMPILER_IS_GNUCC)
		if(SUPPORT_SSE_BUILD)
			set(PLATFORM_CFLAGS " -msse ${PLATFORM_CFLAGS}")
			add_definitions(-D__SSE__ -D__MMX__)
		endif()
		if(SUPPORT_SSE2_BUILD)
			set(PLATFORM_CFLAGS " -msse2 ${PLATFORM_CFLAGS}")
			add_definitions(-D__SSE2__)
			if(NOT SUPPORT_SSE_BUILD) # dont double up
				add_definitions(-D__MMX__)
			endif()
		endif()
	endif()
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

	mark_as_advanced(OpenMP_C_FLAGS)
	mark_as_advanced(OpenMP_CXX_FLAGS)
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

	mark_as_advanced(GLEW_LIBRARY)
	mark_as_advanced(GLEW_INCLUDE_PATH)
endif()

#-----------------------------------------------------------------------------
# Configure Python.

if(WITH_PYTHON_MODULE)
	add_definitions(-DPy_ENABLE_SHARED)
endif() 

#-----------------------------------------------------------------------------
# Extra compile flags

if((NOT WIN32) AND (NOT MSVC))
	# used for internal debug checks
	set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DDEBUG")
	set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -DDEBUG")
	
	# assert() checks for this.
	set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -DNDEBUG")
	set(CMAKE_CXX_FLAGS_MINSIZEREL "${CMAKE_CXX_FLAGS_MINSIZEREL} -DNDEBUG")
	set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -DNDEBUG")

	set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -DNDEBUG")
	set(CMAKE_C_FLAGS_MINSIZEREL "${CMAKE_C_FLAGS_MINSIZEREL} -DNDEBUG")
	set(CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_RELWITHDEBINFO} -DNDEBUG")
endif()

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
	# disable because it gives warnings for printf() & friends.
	# ADD_CHECK_C_COMPILER_FLAG(C_WARNINGS C_WARN_DOUBLE_PROMOTION -Wdouble-promotion -Wno-error=double-promotion)
	ADD_CHECK_C_COMPILER_FLAG(C_WARNINGS C_WARN_NO_ERROR_UNUSED_BUT_SET_VARIABLE -Wno-error=unused-but-set-variable)

	ADD_CHECK_CXX_COMPILER_FLAG(CXX_WARNINGS CXX_WARN_ALL -Wall)
	ADD_CHECK_CXX_COMPILER_FLAG(CXX_WARNINGS CXX_WARN_NO_INVALID_OFFSETOF -Wno-invalid-offsetof)
	ADD_CHECK_CXX_COMPILER_FLAG(CXX_WARNINGS CXX_WARN_NO_SIGN_COMPARE -Wno-sign-compare)

elseif(CMAKE_C_COMPILER_ID MATCHES "Intel")

	ADD_CHECK_C_COMPILER_FLAG(C_WARNINGS C_WARN_ALL -Wall)
	ADD_CHECK_C_COMPILER_FLAG(C_WARNINGS C_WARN_POINTER_ARITH -Wpointer-arith)
	ADD_CHECK_C_COMPILER_FLAG(C_WARNINGS C_WARN_NO_UNKNOWN_PRAGMAS -Wno-unknown-pragmas)

	ADD_CHECK_CXX_COMPILER_FLAG(CXX_WARNINGS CXX_WARN_ALL -Wall)
	ADD_CHECK_CXX_COMPILER_FLAG(CXX_WARNINGS CXX_WARN_NO_INVALID_OFFSETOF -Wno-invalid-offsetof)
	ADD_CHECK_CXX_COMPILER_FLAG(CXX_WARNINGS CXX_WARN_NO_SIGN_COMPARE -Wno-sign-compare)

endif()

# MSVC2010 fails to links C++ libs right
if(MSVC10)
	if(WITH_IMAGE_OPENEXR)
		message(WARNING "MSVC 2010 does not support OpenEXR, disabling WITH_IMAGE_OPENEXR. To enable support use Use MSVC 2008")
		set(WITH_IMAGE_OPENEXR OFF)
	endif()
	if(WITH_OPENCOLLADA)
		message(WARNING "MSVC 2010 does not support OpenCollada, disabling WITH_OPENCOLLADA. To enable support use Use MSVC 2008")
		set(WITH_OPENCOLLADA OFF)
	endif()
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
	if(WITH_IK_ITASC OR WITH_MOD_FLUID)
		message(WARNING "Using Clang as CXX compiler: disabling WITH_IK_ITASC and WITH_MOD_FLUID, these features will be missing.")
		set(WITH_IK_ITASC OFF)
		set(WITH_MOD_FLUID OFF)
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
add_subdirectory(source/creator)


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

	info_cfg_text("Compiler Options:")
	info_cfg_option(WITH_BUILDINFO)
	info_cfg_option(WITH_OPENMP)
	info_cfg_option(WITH_RAYOPTIMIZATION)

	info_cfg_text("System Options:")
	info_cfg_option(WITH_INSTALL_PORTABLE)
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
	info_cfg_option(WITH_SAMPLERATE)

	info_cfg_text("Compression:")
	info_cfg_option(WITH_LZMA)
	info_cfg_option(WITH_LZO)

	info_cfg_text("Python:")
	info_cfg_option(WITH_PYTHON_INSTALL)
	info_cfg_option(WITH_PYTHON_MODULE)
	info_cfg_option(WITH_PYTHON_SAFETY)

	info_cfg_text("Modifiers:")
	info_cfg_option(WITH_MOD_BOOLEAN)
	info_cfg_option(WITH_MOD_DECIMATE)
	info_cfg_option(WITH_MOD_FLUID)

	info_cfg_text("")

	message("${_config_msg}")
endif()
