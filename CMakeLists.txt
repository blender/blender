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
# build the libs and objects in it. It will also conflict with the current 
# Makefile system for Blender

IF(${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})
MESSAGE(FATAL_ERROR "CMake generation for blender is not allowed within the source directory! 
Remove the CMakeCache.txt file and try again from another folder, e.g.: 

	rm CMakeCache.txt 
	cd ..
	mkdir cmake-make 
	cd cmake-make
	cmake -G \"Unix Makefiles\" ../blender
")
ENDIF(${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})

CMAKE_MINIMUM_REQUIRED(VERSION 2.6)

# quiet output for Makefiles, 'make -s' helps too
# SET_PROPERTY(GLOBAL PROPERTY RULE_MESSAGES OFF)

PROJECT(Blender)

#-----------------------------------------------------------------------------
# Redirect output files

SET(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR}/bin)
SET(LIBRARY_OUTPUT_PATH  ${CMAKE_BINARY_DIR}/lib)

#-----------------------------------------------------------------------------
# Load some macros.
INCLUDE(build_files/cmake/macros.cmake)

#-----------------------------------------------------------------------------
# Set default config options

GET_BLENDER_VERSION()

# Blender internal features
OPTION(WITH_INTERNATIONAL "Enable I18N   (International fonts and text)" ON)
OPTION(WITH_PYTHON        "Enable Embedded Python API" ON)
OPTION(WITH_BUILDINFO     "Include extra build details" ON)
OPTION(WITH_IK_ITASC      "Enable ITASC IK solver" ON)
OPTION(WITH_FFTW3         "Enable FFTW3 support (Used for smoke and audio effects)" OFF)
OPTION(WITH_BULLET        "Enable Bullet (Physics Engine)" ON)
OPTION(WITH_GAMEENGINE    "Enable Game Engine" ON)
OPTION(WITH_PLAYER        "Build Player" OFF)
# (unix defaults to OpenMP On)
IF(UNIX AND NOT APPLE)
	OPTION(WITH_OPENMP        "Enable OpenMP (has to be supported by the compiler)" ON)
ELSE()
	OPTION(WITH_OPENMP        "Enable OpenMP (has to be supported by the compiler)" OFF)
ENDIF()

# Modifiers
OPTION(WITH_MOD_FLUID        	"Enable Elbeem Modifier (Fluid Simulation)" ON)
OPTION(WITH_MOD_DECIMATE       	"Enable Decimate Modifier" ON)
OPTION(WITH_MOD_BOOLEAN        	"Enable Boolean Modifier" ON)

# Image format support
OPTION(WITH_IMAGE_OPENEXR       "Enable OpenEXR Support (http://www.openexr.com)" ON)
OPTION(WITH_IMAGE_OPENJPEG      "Enable OpenJpeg Support (http://www.openjpeg.org)" ON)
OPTION(WITH_IMAGE_TIFF          "Enable LibTIFF Support" ON)
OPTION(WITH_IMAGE_DDS           "Enable DDS Image Support" ON)
OPTION(WITH_IMAGE_CINEON        "Enable CINEON and DPX Image Support" ON)
OPTION(WITH_IMAGE_HDR           "Enable HDR Image Support" ON)
OPTION(WITH_IMAGE_REDCODE       "Enable RedCode Image Support" OFF)

# Audio/Video format support
OPTION(WITH_CODEC_FFMPEG        "Enable FFMPeg Support (http://ffmpeg.mplayerhq.hu)" OFF)
OPTION(WITH_CODEC_SNDFILE       "Enable libsndfile Support (http://www.mega-nerd.com/libsndfile)" OFF)
IF(APPLE OR (WIN32 AND NOT UNIX))
	OPTION(WITH_CODEC_QUICKTIME     "Enable Quicktime Support" OFF)
ENDIF(APPLE OR (WIN32 AND NOT UNIX))

# 3D format support
# disable opencollada on non-apple unix because opencollada has no package for debian
IF(UNIX AND NOT APPLE)
	OPTION(WITH_OPENCOLLADA		"Enable OpenCollada Support (http://www.opencollada.org)"	OFF)
ELSE()
	OPTION(WITH_OPENCOLLADA		"Enable OpenCollada Support (http://www.opencollada.org)"	OFF)
ENDIF()

# Sound output
OPTION(WITH_SDL           "Enable SDL for sound and joystick support" ON)
OPTION(WITH_OPENAL        "Enable OpenAL Support (http://www.openal.org)" ON)
OPTION(WITH_JACK          "Enable Jack Support (http://www.jackaudio.org)" OFF)
OPTION(WITH_SAMPLERATE    "Enable samplerate conversion" ON)

# Compression
OPTION(WITH_LZO           "Enable fast LZO compression (used for pointcache)" ON)
OPTION(WITH_LZMA          "Enable best LZMA compression, (used for pointcache)" ON)

# Misc
OPTION(WITH_RAYOPTIMIZATION	"Enable use of SIMD (SSE) optimizations for the raytracer" ON) 
OPTION(WITH_CXX_GUARDEDALLOC "Enable GuardedAlloc for C++ memory allocation tracking" OFF)
OPTION(WITH_INSTALL       "Install accompanying scripts and language files needed to run blender" ON)
OPTION(WITH_PYTHON_INSTALL       "Copy system python into the blender install folder" ON)

IF(APPLE)
	OPTION(WITH_COCOA	  "Use Cocoa framework instead of deprecated Carbon" ON)
	OPTION(USE_QTKIT	  "Use QtKit instead of Carbon quicktime (needed for having partial quicktime for 64bit)" OFF)
	OPTION(WITH_LIBS10.5  "Use 10.5 libs (needed for 64bit builds)" OFF)
ENDIF(APPLE)

# only for developers who want to make this functional
# OPTION(WITH_LCMS          "Enable color correction with lcms" OFF)

IF(NOT WITH_GAMEENGINE AND WITH_PLAYER)
	MESSAGE(FATAL_ERROR "WITH_PLAYER requires WITH_GAMEENGINE")
ENDIF(NOT WITH_GAMEENGINE AND WITH_PLAYER)

IF(NOT WITH_INSTALL AND WITH_PYTHON_INSTALL)
	MESSAGE("WARNING: WITH_PYTHON_INSTALL requires WITH_INSTALL")
ENDIF(NOT WITH_INSTALL AND WITH_PYTHON_INSTALL)

IF(NOT WITH_SAMPLERATE AND (WITH_OPENAL OR WITH_SDL OR WITH_JACK))
	MESSAGE(FATAL_ERROR "WITH_OPENAL/WITH_SDL/WITH_JACK require WITH_SAMPLERATE")
ENDIF(NOT WITH_SAMPLERATE AND (WITH_OPENAL OR WITH_SDL OR WITH_JACK))

IF(NOT WITH_IMAGE_OPENJPEG AND WITH_IMAGE_REDCODE)
	MESSAGE(FATAL_ERROR "WITH_IMAGE_REDCODE requires WITH_IMAGE_OPENJPEG")
ENDIF(NOT WITH_IMAGE_OPENJPEG AND WITH_IMAGE_REDCODE)

TEST_SSE_SUPPORT()

# disabled for now, not supported
# OPTION(WITH_WEBPLUGIN     "Enable Web Plugin (Unix only)" OFF)

# For alternate Python locations the commandline can be used to override detected/default cache settings, e.g:
# On Unix: 
#   cmake -D PYTHON_LIB=/usr/local/lib/python3.1/config/libpython3.1.so -D PYTHON_INC=/usr/local/include/python3.1 -G "Unix Makefiles" ../blender
# On Macs: 
#   cmake -D PYTHON_INC=/System/Library/Frameworks/Python.framework/Versions/3.1/include/python3.1 -D PYTHON_LIBPATH=/System/Library/Frameworks/Python.framework/Versions/3.1/lib/python3.1/config -G Xcode ../blender
#
# When changing any of this remember to update the notes in doc/build_systems/cmake.txt

#-----------------------------------------------------------------------------
#Platform specifics

IF(UNIX AND NOT APPLE)

	IF(WITH_OPENAL)
		FIND_PACKAGE(OpenAL)
		IF(NOT OPENAL_FOUND)
			SET(WITH_OPENAL OFF)
		ENDIF(NOT OPENAL_FOUND)
	ENDIF(WITH_OPENAL)

	IF(WITH_JACK)
		SET(JACK /usr)
		SET(JACK_INC ${JACK}/include/jack)
		SET(JACK_LIB jack)
		SET(JACK_LIBPATH ${JACK}/lib)
	ENDIF(WITH_JACK)

	IF(WITH_CODEC_SNDFILE)
		SET(SNDFILE /usr)
		SET(SNDFILE_INC ${SNDFILE}/include)
		SET(SNDFILE_LIB sndfile)
		SET(SNDFILE_LIBPATH ${SNDFILE}/lib)
	ENDIF(WITH_CODEC_SNDFILE)

	IF(WITH_INTERNATIONAL)
		FIND_LIBRARY(INTL_LIBRARY
			NAMES intl
			PATHS
			/sw/lib
		)

		FIND_LIBRARY(ICONV_LIBRARY
			NAMES iconv
			PATHS
			/sw/lib
		)

		IF(INTL_LIBRARY AND ICONV_LIBRARY)
			SET(GETTEXT_LIB ${INTL_LIBRARY} ${ICONV_LIBRARY})
		ENDIF(INTL_LIBRARY AND ICONV_LIBRARY)
	ENDIF(WITH_INTERNATIONAL)

	FIND_PACKAGE(Freetype)
	# UNSET(FREETYPE_INCLUDE_DIRS CACHE) # cant use


	IF(WITH_PYTHON)
		# No way to set py31. remove for now.
		# FIND_PACKAGE(PythonLibs)
		SET(PYTHON /usr)
		SET(PYTHON_VERSION 3.1)
		SET(PYTHON_INC "${PYTHON}/include/python${PYTHON_VERSION}" CACHE STRING "")
		# SET(PYTHON_BINARY python) # not used yet
		SET(PYTHON_LIB python${PYTHON_VERSION} CACHE STRING "")
		SET(PYTHON_LIBPATH ${PYTHON}/lib CACHE STRING "")
	
		# FIND_PACKAGE(PythonInterp) # not used yet
		# SET(PYTHON_BINARY ${PYTHON_EXECUTABLE} CACHE STRING "")

		SET(PYTHON_LINKFLAGS "-Xlinker -export-dynamic")
	ENDIF(WITH_PYTHON)

	IF(WITH_SDL)
		FIND_PACKAGE(SDL)
		# UNSET(SDLMAIN_LIBRARY CACHE)
		IF(NOT SDL_FOUND)
			SET(WITH_SDL OFF)
		ENDIF(NOT SDL_FOUND)
	ENDIF(WITH_SDL)

	IF(WITH_IMAGE_OPENEXR)
		SET(OPENEXR /usr CACHE FILEPATH "OPENEXR Directory")
		FIND_PATH(OPENEXR_INC
			ImfXdr.h
			PATHS
			${OPENEXR}/include/OpenEXR
			/usr/local/include/OpenEXR
			/sw/include/OpenEXR
			/opt/local/include/OpenEXR
			/opt/csw/include/OpenEXR
			/opt/include/OpenEXR
		)
		SET(OPENEXR_LIB Half IlmImf Iex Imath)
		
		IF(NOT OPENEXR_INC)
			SET(WITH_IMAGE_OPENEXR OFF)
		ENDIF(NOT OPENEXR_INC)
	ENDIF(WITH_IMAGE_OPENEXR)

	IF(WITH_IMAGE_TIFF)
		FIND_PACKAGE(TIFF)
		IF(NOT TIFF_FOUND)
			SET(WITH_IMAGE_TIFF OFF)
		ENDIF(NOT TIFF_FOUND)
	ENDIF(WITH_IMAGE_TIFF)

	FIND_PACKAGE(JPEG REQUIRED)

	FIND_PACKAGE(PNG REQUIRED)

	FIND_PACKAGE(ZLIB REQUIRED)

	IF(WITH_LCMS)
		SET(LCMS /usr CACHE FILEPATH "LCMS directory")
		SET(LCMS_INCLUDE_DIR ${LCMS}/include)
		SET(LCMS_LIBRARY lcms)
		SET(LCMS_LIBPATH ${LCMS}/lib)
	ENDIF(WITH_LCMS)

	IF(WITH_CODEC_FFMPEG)
		SET(FFMPEG /usr CACHE FILEPATH "FFMPEG Directory")
		SET(FFMPEG_INC ${FFMPEG}/include)
		SET(FFMPEG_LIB avformat avcodec avutil avdevice swscale CACHE STRING "FFMPEG Libraries")
		SET(FFMPEG_LIBPATH ${FFMPEG}/lib)
		SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D__STDC_CONSTANT_MACROS")
	ENDIF(WITH_CODEC_FFMPEG)

	IF(WITH_FFTW3)
		SET(FFTW3 /usr)
		SET(FFTW3_INC ${FFTW3}/include)
		SET(FFTW3_LIB fftw3)
		SET(FFTW3_LIBPATH ${FFTW3}/lib)
	ENDIF(WITH_FFTW3)

	IF(WITH_SAMPLERATE)
		SET(LIBSAMPLERATE /usr)
		SET(LIBSAMPLERATE_INC ${LIBSAMPLERATE}/include)
		SET(LIBSAMPLERATE_LIB samplerate)
		SET(LIBSAMPLERATE_LIBPATH ${LIBSAMPLERATE}/lib)
	ENDIF(WITH_SAMPLERATE)

	IF (WITH_OPENCOLLADA)
		SET(OPENCOLLADA /usr/local/opencollada CACHE FILEPATH "OpenCollada Directory")
		SET(OPENCOLLADA_LIBPATH ${OPENCOLLADA}/lib)
		SET(OPENCOLLADA_LIB OpenCOLLADAStreamWriter OpenCOLLADASaxFrameworkLoader OpenCOLLADAFramework OpenCOLLADABaseUtils GeneratedSaxParser UTF MathMLSolver pcre ftoa buffer xml2)
		SET(OPENCOLLADA_INC ${OPENCOLLADA})
		SET(PCRE /usr CACHE FILEPATH "PCRE Directory")
		SET(PCRE_LIBPATH ${PCRE}/lib)
		SET(PCRE_LIB pcre)
		SET(EXPAT /usr CACHE FILEPATH "Expat Directory")
		SET(EXPAT_LIBPATH ${EXPAT}/lib)
		SET(EXPAT_LIB expat)
	ENDIF (WITH_OPENCOLLADA)

	FIND_PACKAGE(X11 REQUIRED)

	# OpenSuse needs lutil, ArchLinux not, for now keep, can avoid by using --as-needed
	SET(LLIBS "-lutil -lc -lm -lpthread -lstdc++ ${X11_X11_LIB} ${X11_Xinput_LIB}")

	IF(CMAKE_SYSTEM_NAME MATCHES "Linux")
		# BSD's dont use libdl.so
		LIST(APPEND LLIBS -ldl)

		# binreloc is linux only
		SET(BINRELOC ${CMAKE_SOURCE_DIR}/extern/binreloc)
		SET(BINRELOC_INC ${BINRELOC}/include)
	ENDIF(CMAKE_SYSTEM_NAME MATCHES "Linux")

	SET(PLATFORM_LINKFLAGS "-pthread")

	SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64 -D_LARGEFILE64_SOURCE")
	SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64 -D_LARGEFILE64_SOURCE")

	# GNU Compiler
	IF(CMAKE_COMPILER_IS_GNUCC)
		SET(PLATFORM_CFLAGS "-pipe -fPIC -funsigned-char -fno-strict-aliasing")
	
		# Better warnings
		# note: -Wunused-parameter is added below for all GCC compilers
		SET(C_WARNINGS "-Wall -Wno-char-subscripts -Wpointer-arith -Wcast-align -Wdeclaration-after-statement -Wno-unknown-pragmas")
		SET(CXX_WARNINGS "-Wall -Wno-invalid-offsetof -Wno-sign-compare")

	# Intel C++ Compiler
	ELSEIF(CMAKE_C_COMPILER_ID MATCHES "Intel")
		# think these next two are broken
		FIND_PROGRAM(XIAR xiar) 
		IF(XIAR) 
			SET(CMAKE_AR "${XIAR}")
		ENDIF(XIAR) 
		MARK_AS_ADVANCED(XIAR)

		FIND_PROGRAM(XILD xild) 
		IF(XILD) 
			SET(CMAKE_LINKER "${XILD}")
		ENDIF(XILD) 
		MARK_AS_ADVANCED(XILD)

		SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fp-model precise -prec_div -parallel")
		SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fp-model precise -prec_div -parallel")

		SET(PLATFORM_CFLAGS "-pipe -fPIC -funsigned-char -fno-strict-aliasing")

		# SET(PLATFORM_CFLAGS "${PLATFORM_CFLAGS} -diag-enable sc3")

		SET(C_WARNINGS "-Wall -Wpointer-arith -Wno-unknown-pragmas")
		SET(CXX_WARNINGS "-Wall -Wno-invalid-offsetof -Wno-sign-compare")

		SET(PLATFORM_LINKFLAGS "${PLATFORM_LINKFLAGS} -static-intel")
	ENDIF(CMAKE_COMPILER_IS_GNUCC)

ELSEIF(WIN32)

	# this file is included anyway when building under Windows with cl.exe
	#  INCLUDE(${CMAKE_ROOT}/Modules/Platform/Windows-cl.cmake)

	SET(LIBDIR ${CMAKE_SOURCE_DIR}/../lib/windows)

	# Setup 64bit and 64bit windows systems
	IF(CMAKE_CL_64)
		MESSAGE("64 bit compiler detected.")
		SET(LIBDIR ${CMAKE_SOURCE_DIR}/../lib/win64)
	ENDIF(CMAKE_CL_64)
	
	ADD_DEFINITIONS(-DWIN32)

	IF(WITH_INTERNATIONAL)
		SET(ICONV ${LIBDIR}/iconv)
		SET(ICONV_INC ${ICONV}/include)
		SET(ICONV_LIB iconv)
		SET(ICONV_LIBPATH ${ICONV}/lib)
	ENDIF(WITH_INTERNATIONAL)

	IF(WITH_SAMPLERATE)
		SET(LIBSAMPLERATE ${LIBDIR}/samplerate)
		SET(LIBSAMPLERATE_INC ${LIBSAMPLERATE}/include)
		SET(LIBSAMPLERATE_LIB libsamplerate)
		SET(LIBSAMPLERATE_LIBPATH ${LIBSAMPLERATE}/lib)
	ENDIF(WITH_SAMPLERATE)

	SET(PNG "${LIBDIR}/png")
	SET(PNG_INC "${PNG}/include")
	SET(PNG_LIBPATH ${PNG}/lib)

	SET(JPEG "${LIBDIR}/jpeg")
	SET(JPEG_INC "${JPEG}/include")
	SET(JPEG_LIBPATH ${JPEG}/lib)

	SET(WINTAB_INC ${LIBDIR}/wintab/include) 

	IF(WITH_OPENAL)
		SET(OPENAL ${LIBDIR}/openal)
		SET(OPENAL_INCLUDE_DIR ${OPENAL}/include)
		SET(OPENAL_LIBRARY wrap_oal)
		SET(OPENAL_LIBPATH ${OPENAL}/lib)
	ENDIF(WITH_OPENAL)

	IF(WITH_CODEC_SNDFILE)
		SET(SNDFILE ${LIBDIR}/sndfile)
		SET(SNDFILE_INC ${SNDFILE}/include)
		SET(SNDFILE_LIB libsndfile-1)
		SET(SNDFILE_LIBPATH ${SNDFILE}/lib)
	ENDIF(WITH_CODEC_SNDFILE)

	IF(WITH_SDL)
		SET(SDL ${LIBDIR}/sdl)
		SET(SDL_INCLUDE_DIR ${SDL}/include)
		SET(SDL_LIBRARY SDL)
		SET(SDL_LIBPATH ${SDL}/lib)
	ENDIF(WITH_SDL)

	IF(WITH_CODEC_QUICKTIME)
		SET(QUICKTIME ${LIBDIR}/QTDevWin)
		SET(QUICKTIME_INC ${QUICKTIME}/CIncludes)
		SET(QUICKTIME_LIB qtmlClient)
		SET(QUICKTIME_LIBPATH ${QUICKTIME}/Libraries)
	ENDIF(WITH_CODEC_QUICKTIME)

	IF(WITH_RAYOPTIMIZATION AND SUPPORT_SSE_BUILD)
		ADD_DEFINITIONS(-D__SSE__ -D__MMX__)
	ENDIF(WITH_RAYOPTIMIZATION AND SUPPORT_SSE_BUILD)

	IF(MSVC)
		IF(CMAKE_CL_64)
			SET(LLIBS kernel32 user32 vfw32 winmm ws2_32 )
		ELSE(CMAKE_CL_64)
			SET(LLIBS kernel32 user32 gdi32 comdlg32 advapi32 shell32 ole32 oleaut32 uuid ws2_32 vfw32 winmm)
		ENDIF(CMAKE_CL_64)
		
		SET(CMAKE_CXX_FLAGS "/D_CRT_NONSTDC_NO_DEPRECATE /D_CRT_SECURE_NO_DEPRECATE /D_SCL_SECURE_NO_DEPRECATE /we4013 /wd4018 /wd4800 /wd4244 /wd4305 /wd4065 /wd4267" CACHE STRING "MSVC MT C++ flags " FORCE)
		SET(CMAKE_C_FLAGS   "/D_CRT_NONSTDC_NO_DEPRECATE /D_CRT_SECURE_NO_DEPRECATE /D_SCL_SECURE_NO_DEPRECATE /we4013 /wd4018 /wd4800 /wd4244 /wd4305 /wd4065 /wd4267" CACHE STRING "MSVC MT C++ flags " FORCE)

		IF(CMAKE_CL_64)
			SET(CMAKE_CXX_FLAGS_DEBUG "/D_DEBUG /Od /Gm /EHsc /RTC1 /MTd /W3 /nologo /Zi /J" CACHE STRING "MSVC MT flags " FORCE)
		ELSE(CMAKE_CL_64)
			SET(CMAKE_CXX_FLAGS_DEBUG "/D_DEBUG /Od /Gm /EHsc /RTC1 /MTd /W3 /nologo /ZI /J" CACHE STRING "MSVC MT flags " FORCE)
		ENDIF(CMAKE_CL_64)
		SET(CMAKE_CXX_FLAGS_RELEASE "/O2 /Ob2 /DNDEBUG /EHsc /MT /W3 /nologo /J" CACHE STRING "MSVC MT flags " FORCE)
		SET(CMAKE_CXX_FLAGS_MINSIZEREL "/O1 /Ob1 /DNDEBUG /EHsc /MT /W3 /nologo /J" CACHE STRING "MSVC MT flags " FORCE)
		SET(CMAKE_CXX_FLAGS_RELWITHDEBINFO "/O2 /Ob1 /DNDEBUG /EHsc /MT /W3 /nologo /Zi /J" CACHE STRING "MSVC MT flags " FORCE)
		IF(CMAKE_CL_64)
			SET(CMAKE_C_FLAGS_DEBUG "/D_DEBUG /Od /Gm /EHsc /RTC1 /MTd /W3 /nologo /Zi /J" CACHE STRING "MSVC MT flags " FORCE)
		ELSE(CMAKE_CL_64)
			SET(CMAKE_C_FLAGS_DEBUG "/D_DEBUG /Od /Gm /EHsc /RTC1 /MTd /W3 /nologo /ZI /J" CACHE STRING "MSVC MT flags " FORCE)
		ENDIF(CMAKE_CL_64)
		SET(CMAKE_C_FLAGS_RELEASE "/O2 /Ob2 /DNDEBUG /EHsc /MT /W3 /nologo /J" CACHE STRING "MSVC MT flags " FORCE)
		SET(CMAKE_C_FLAGS_MINSIZEREL "/O1 /Ob1 /DNDEBUG /EHsc /MT /W3 /nologo /J" CACHE STRING "MSVC MT flags " FORCE)
		SET(CMAKE_C_FLAGS_RELWITHDEBINFO "/O2 /Ob1 /DNDEBUG /EHsc /MT /W3 /nologo /Zi /J" CACHE STRING "MSVC MT flags " FORCE)

		IF(WITH_INTERNATIONAL)
			SET(GETTEXT ${LIBDIR}/gettext)
			SET(GETTEXT_INC ${GETTEXT}/include)
			SET(GETTEXT_LIBPATH ${GETTEXT}/lib)
			IF(CMAKE_CL_64)
				SET(GETTEXT_LIB gettext)
			ELSE(CMAKE_CL_64)
				SET(GETTEXT_LIB gnu_gettext)
			ENDIF(CMAKE_CL_64)
		ENDIF(WITH_INTERNATIONAL)

		IF(CMAKE_CL_64)
			SET(PNG_LIBRARIES libpng)
		ELSE(CMAKE_CL_64)
			SET(PNG_LIBRARIES libpng_st)
		ENDIF(CMAKE_CL_64)
		SET(JPEG_LIBRARY libjpeg)

		SET(ZLIB ${LIBDIR}/zlib)
		SET(ZLIB_INC ${ZLIB}/include)
		SET(ZLIB_LIBPATH ${ZLIB}/lib)
		IF(CMAKE_CL_64)
			SET(ZLIB_LIBRARIES libz)
		ELSE(CMAKE_CL_64)
			SET(ZLIB_LIBRARIES zlib)
		ENDIF(CMAKE_CL_64)

		SET(PTHREADS ${LIBDIR}/pthreads)
		SET(PTHREADS_INC ${PTHREADS}/include)
		SET(PTHREADS_LIBPATH ${PTHREADS}/lib)
		SET(PTHREADS_LIB pthreadVC2)
		
		SET(FREETYPE ${LIBDIR}/freetype)
		SET(FREETYPE_INCLUDE_DIRS ${FREETYPE}/include ${FREETYPE}/include/freetype2)
		SET(FREETYPE_LIBPATH ${FREETYPE}/lib)
		SET(FREETYPE_LIBRARY freetype2ST)

		IF(WITH_FFTW3)
			SET(FFTW3 ${LIBDIR}/fftw3)
			SET(FFTW3_LIB libfftw)
			SET(FFTW3_INC ${FFTW3}/include)
			SET(FFTW3_LIBPATH ${FFTW3}/lib)
		ENDIF(WITH_FFTW3)

		IF(WITH_OPENCOLLADA)
			SET(OPENCOLLADA ${LIBDIR}/opencollada)
			SET(OPENCOLLADA_INC ${OPENCOLLADA}/include)
			SET(OPENCOLLADA_LIBPATH ${OPENCOLLADA}/lib)
			SET(OPENCOLLADA_LIB OpenCOLLADASaxFrameworkLoader OpenCOLLADAFramework OpenCOLLADABaseUtils OpenCOLLADAStreamWriter MathMLSolver GeneratedSaxParser xml2 buffer ftoa)
			SET(PCRE_LIB pcre)
		ENDIF(WITH_OPENCOLLADA)
		
		IF(WITH_LCMS)
			SET(LCMS ${LIBDIR}/lcms)
			SET(LCMS_INCLUDE_DIR ${LCMS}/include)
			SET(LCMS_LIBPATH ${LCMS}/lib)
			SET(LCMS_LIB lcms)
		ENDIF(WITH_LCMS)

		IF(WITH_CODEC_FFMPEG)
			SET(FFMPEG ${LIBDIR}/ffmpeg)
			SET(FFMPEG_INC ${FFMPEG}/include ${FFMPEG}/include/msvc)
			SET(FFMPEG_LIB avcodec-52 avformat-52 avdevice-52 avutil-50 swscale-0)
			SET(FFMPEG_LIBPATH ${FFMPEG}/lib)
		ENDIF(WITH_CODEC_FFMPEG)

		IF(WITH_IMAGE_OPENEXR)
			SET(OPENEXR ${LIBDIR}/openexr)
			SET(OPENEXR_INC ${OPENEXR}/include ${OPENEXR}/include/IlmImf ${OPENEXR}/include/Iex ${OPENEXR}/include/Imath)
			SET(OPENEXR_LIB Iex Half IlmImf Imath IlmThread)
			IF (MSVC80)
				SET(OPENEXR_LIBPATH ${OPENEXR}/lib_vs2005)
			ELSE (MSVC80)
				SET(OPENEXR_LIBPATH ${OPENEXR}/lib_msvc)
			ENDIF(MSVC80)
			IF (MSVC90)
				SET(OPENEXR_LIBPATH ${OPENEXR}/lib_vs2008)
			ENDIF(MSVC90)
		ENDIF(WITH_IMAGE_OPENEXR)

		IF(WITH_IMAGE_TIFF)
			SET(TIFF ${LIBDIR}/tiff)
			SET(TIFF_LIBRARY libtiff)
			SET(TIFF_INCLUDE_DIR ${TIFF}/include)
			SET(TIFF_LIBPATH ${TIFF}/lib)
		ENDIF(WITH_IMAGE_TIFF)

		IF(WITH_JACK)
			SET(JACK ${LIBDIR}/jack)
			SET(JACK_INC ${JACK}/include/jack ${JACK}/include)
			SET(JACK_LIB libjack)
			SET(JACK_LIBPATH ${JACK}/lib)
		ENDIF(WITH_JACK)

		IF(WITH_PYTHON)
			SET(PYTHON ${LIBDIR}/python)
			SET(PYTHON_VERSION 3.1)
			SET(PYTHON_INC "${PYTHON}/include/python${PYTHON_VERSION}")
			# SET(PYTHON_BINARY python) # not used yet
			SET(PYTHON_LIB python31)
			SET(PYTHON_LIBPATH ${PYTHON}/lib)
		ENDIF(WITH_PYTHON)

		# MSVC only, Mingw doesnt need
		IF(CMAKE_CL_64)
			SET(PLATFORM_LINKFLAGS "/MACHINE:X64 /NODEFAULTLIB:libc.lib ")
		ELSE(CMAKE_CL_64)
			SET(PLATFORM_LINKFLAGS "/NODEFAULTLIB:libc.lib ")
		ENDIF(CMAKE_CL_64)

		SET(CMAKE_EXE_LINKER_FLAGS_DEBUG "${CMAKE_EXE_LINKER_FLAGS_DEBUG} /NODEFAULTLIB:libcmt.lib;libc.lib ")

	ELSE(MSVC)
		# keep GCC spesific stuff here
		IF(CMAKE_COMPILER_IS_GNUCC)
			SET(LLIBS "-lshell32 -lshfolder -lgdi32 -lmsvcrt -lwinmm -lmingw32 -lm -lws2_32 -lz -lstdc++ -lole32 -luuid")
			SET(PLATFORM_CFLAGS "-pipe -funsigned-char -fno-strict-aliasing")

			# Better warnings
			SET(C_WARNINGS "-Wall -Wno-char-subscripts -Wpointer-arith -Wcast-align -Wdeclaration-after-statement -Wno-unknown-pragmas")
			SET(CXX_WARNINGS "-Wall -Wno-invalid-offsetof -Wno-sign-compare")

			SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64 -D_LARGEFILE64_SOURCE")
			SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64 -D_LARGEFILE64_SOURCE")
		ENDIF(CMAKE_COMPILER_IS_GNUCC)

		ADD_DEFINITIONS(-DFREE_WINDOWS)

		SET(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -D_DEBUG")
		SET(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -D_DEBUG")

		IF(WITH_INTERNATIONAL)
			SET(GETTEXT ${LIBDIR}/gcc/gettext)
			SET(GETTEXT_INC ${GETTEXT}/include)
			SET(GETTEXT_LIBPATH ${GETTEXT}/lib)
			SET(GETTEXT_LIB intl)
		ENDIF(WITH_INTERNATIONAL)

		SET(JPEG_LIBRARY libjpeg)
		SET(PNG_LIBRARIES png)

		SET(ZLIB ${LIBDIR}/zlib)
		SET(ZLIB_INC ${ZLIB}/include)
		SET(ZLIB_LIBPATH ${ZLIB}/lib)
		SET(ZLIB_LIBRARIES z)

		SET(PTHREADS ${LIBDIR}/pthreads)
		SET(PTHREADS_INC ${PTHREADS}/include)
		SET(PTHREADS_LIBPATH ${PTHREADS}/lib)
		SET(PTHREADS_LIB pthreadGC2)
		
		SET(FREETYPE ${LIBDIR}/gcc/freetype)
		SET(FREETYPE_INCLUDE_DIRS ${FREETYPE}/include ${FREETYPE}/include/freetype2)
		SET(FREETYPE_LIBPATH ${FREETYPE}/lib)
		SET(FREETYPE_LIBRARY freetype)

		IF(WITH_FFTW3)
			SET(FFTW3 ${LIBDIR}/gcc/fftw3)
			SET(FFTW3_LIB fftw3)
			SET(FFTW3_INC ${FFTW3}/include)
			SET(FFTW3_LIBPATH ${FFTW3}/lib)
		ENDIF(WITH_FFTW3)

		IF(WITH_OPENCOLLADA)
			SET(OPENCOLLADA ${LIBDIR}/gcc/opencollada)
			SET(OPENCOLLADA_INC ${OPENCOLLADA}/include)
			SET(OPENCOLLADA_LIBPATH ${OPENCOLLADA}/lib ${OPENCOLLADA}/lib)
			SET(OPENCOLLADA_LIB OpenCOLLADAStreamWriter OpenCOLLADASaxFrameworkLoader OpenCOLLADAFramework OpenCOLLADABaseUtils GeneratedSaxParser UTF MathMLSolver expat pcre buffer ftoa)
			SET(PCRE_LIB pcre)
		ENDIF(WITH_OPENCOLLADA)
		
		IF(WITH_CODEC_FFMPEG)
			SET(FFMPEG ${LIBDIR}/ffmpeg)
			SET(FFMPEG_INC ${FFMPEG}/include ${FFMPEG}/include)
			SET(FFMPEG_LIB avcodec-52 avformat-52 avdevice-52 avutil-50 swscale-0)
			SET(FFMPEG_LIBPATH ${FFMPEG}/lib)
		ENDIF(WITH_CODEC_FFMPEG)

		IF(WITH_IMAGE_OPENEXR)
			SET(OPENEXR ${LIBDIR}/gcc/openexr)
			SET(OPENEXR_INC ${OPENEXR}/include ${OPENEXR}/include/OpenEXR)
			SET(OPENEXR_LIB Half IlmImf Imath IlmThread)
			SET(OPENEXR_LIBPATH ${OPENEXR}/lib)

			# TODO, gives linking errors, force off
			SET(WITH_IMAGE_OPENEXR OFF)
		ENDIF(WITH_IMAGE_OPENEXR)

		IF(WITH_IMAGE_TIFF)
			SET(TIFF ${LIBDIR}/gcc/tiff)
			SET(TIFF_LIBRARY tiff)
			SET(TIFF_INCLUDE_DIR ${TIFF}/include)
			SET(TIFF_LIBPATH ${TIFF}/lib)
		ENDIF(WITH_IMAGE_TIFF)

		IF(WITH_JACK)
			SET(JACK ${LIBDIR}/jack)
			SET(JACK_INC ${JACK}/include/jack ${JACK}/include)
			SET(JACK_LIB jack)
			SET(JACK_LIBPATH ${JACK}/lib)
			
			# TODO, gives linking errors, force off
			SET(WITH_JACK OFF)
		ENDIF(WITH_JACK)

		IF(WITH_PYTHON)
			SET(PYTHON ${LIBDIR}/python)
			SET(PYTHON_VERSION 3.1)
			SET(PYTHON_INC "${PYTHON}/include/python${PYTHON_VERSION}")
			# SET(PYTHON_BINARY python) # not used yet
			SET(PYTHON_LIB python31mw)
			SET(PYTHON_LIBPATH ${PYTHON}/lib)
		ENDIF(WITH_PYTHON)

	ENDIF(MSVC)

	# used in many places so include globally, like OpenGL
	INCLUDE_DIRECTORIES(${PTHREADS_INC})

ELSEIF(APPLE)

	IF(WITH_LIBS10.5)
		SET(LIBDIR ${CMAKE_SOURCE_DIR}/../lib/darwin-9.x.universal)
	ELSE(WITH_LIBS10.5)
		IF(CMAKE_OSX_ARCHITECTURES MATCHES i386)
			SET(LIBDIR ${CMAKE_SOURCE_DIR}/../lib/darwin-8.x.i386)
		ELSE(CMAKE_OSX_ARCHITECTURES MATCHES i386)
			SET(LIBDIR ${CMAKE_SOURCE_DIR}/../lib/darwin-8.0.0-powerpc)
		ENDIF(CMAKE_OSX_ARCHITECTURES MATCHES i386)
	ENDIF(WITH_LIBS10.5)
	

	IF(WITH_OPENAL)
		FIND_PACKAGE(OpenAL)
		IF(OPENAL_FOUND)
			SET(WITH_OPENAL ON)
			SET(OPENAL_INCLUDE_DIR "${LIBDIR}/openal/include")
		ELSE(OPENAL_FOUND)
			SET(WITH_OPENAL OFF)
		ENDIF(OPENAL_FOUND)
	ENDIF(WITH_OPENAL)

	IF(WITH_JACK)
		SET(JACK /usr)
		SET(JACK_INC ${JACK}/include/jack)
		SET(JACK_LIB jack)
		SET(JACK_LIBPATH ${JACK}/lib)
	ENDIF(WITH_JACK)

	IF(WITH_CODEC_SNDFILE)
		SET(SNDFILE ${LIBDIR}/sndfile)
		SET(SNDFILE_INC ${SNDFILE}/include)
		SET(SNDFILE_LIB sndfile FLAC ogg vorbis vorbisenc)
		SET(SNDFILE_LIBPATH ${SNDFILE}/lib ${FFMPEG}/lib)
	ENDIF(WITH_CODEC_SNDFILE)

	SET(PYTHON_VERSION 3.1)

	IF(PYTHON_VERSION MATCHES 3.1)
		# we use precompiled libraries for py 3.1 and up by default

		SET(PYTHON ${LIBDIR}/python)
		SET(PYTHON_INC "${PYTHON}/include/python${PYTHON_VERSION}")
		# SET(PYTHON_BINARY "${PYTHON}/bin/python${PYTHON_VERSION}") # not used yet
		SET(PYTHON_LIB python${PYTHON_VERSION})
		SET(PYTHON_LIBPATH "${PYTHON}/lib/python${PYTHON_VERSION}")
		# SET(PYTHON_LINKFLAGS "-u _PyMac_Error")  # won't  build with this enabled
	ELSE(PYTHON_VERSION MATCHES 3.1)
		# otherwise, use custom system framework

		SET(PYTHON /System/Library/Frameworks/Python.framework/Versions/)
		SET(PYTHON_VERSION 3.1)
		SET(PYTHON_INC "${PYTHON}${PYTHON_VERSION}/include/python${PYTHON_VERSION}")
		# SET(PYTHON_BINARY ${PYTHON}${PYTHON_VERSION}/bin/python${PYTHON_VERSION}) # not used yet
		SET(PYTHON_LIB "")
		SET(PYTHON_LIBPATH ${PYTHON}${PYTHON_VERSION}/lib/python${PYTHON_VERSION}/config)
		SET(PYTHON_LINKFLAGS "-u _PyMac_Error -framework System -framework Python")
	ENDIF(PYTHON_VERSION MATCHES 3.1)

	IF(WITH_INTERNATIONAL)
		SET(GETTEXT ${LIBDIR}/gettext)
		SET(GETTEXT_INC "${GETTEXT}/include")
		SET(GETTEXT_LIB intl iconv)
		SET(GETTEXT_LIBPATH ${GETTEXT}/lib)
	ENDIF(WITH_INTERNATIONAL)
  
	IF(WITH_FFTW3)
		SET(FFTW3 ${LIBDIR}/fftw3)
		SET(FFTW3_INC ${FFTW3}/include)
		SET(FFTW3_LIB fftw3)
		SET(FFTW3_LIBPATH ${FFTW3}/lib)
	ENDIF(WITH_FFTW3)

	SET(PNG_LIBRARIES png)
	SET(JPEG_LIBRARY jpeg)

	SET(ZLIB /usr)
	SET(ZLIB_INC "${ZLIB}/include")
	SET(ZLIB_LIBRARIES z bz2)

	SET(FREETYPE ${LIBDIR}/freetype)
	SET(FREETYPE_INCLUDE_DIRS ${FREETYPE}/include ${FREETYPE}/include/freetype2)
	SET(FREETYPE_LIBPATH ${FREETYPE}/lib)
	SET(FREETYPE_LIBRARY freetype)

	IF(WITH_IMAGE_OPENEXR)
		SET(OPENEXR ${LIBDIR}/openexr)
		SET(OPENEXR_INC ${OPENEXR}/include/OpenEXR ${OPENEXR}/include)
		SET(OPENEXR_LIB Iex Half IlmImf Imath IlmThread)
		SET(OPENEXR_LIBPATH ${OPENEXR}/lib)
	ENDIF(WITH_IMAGE_OPENEXR)

	IF(WITH_LCMS)
		SET(LCMS ${LIBDIR}/lcms)
		SET(LCMS_INCLUDE_DIR ${LCMS}/include)
		SET(LCMS_LIBRARY lcms)
		SET(LCMS_LIBPATH ${LCMS}/lib)
	ENDIF(WITH_LCMS)

	IF(WITH_CODEC_FFMPEG)
		SET(FFMPEG ${LIBDIR}/ffmpeg)
		SET(FFMPEG_INC ${FFMPEG}/include)
		SET(FFMPEG_LIB avcodec avdevice avformat avutil mp3lame swscale x264 xvidcore theora theoradec theoraenc vorbis vorbisenc vorbisfile ogg)
		SET(FFMPEG_LIBPATH ${FFMPEG}/lib)
		SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D__STDC_CONSTANT_MACROS")
	ENDIF(WITH_CODEC_FFMPEG)

	IF(WITH_SAMPLERATE)
		SET(LIBSAMPLERATE ${LIBDIR}/samplerate)
		SET(LIBSAMPLERATE_INC ${LIBSAMPLERATE}/include)
		SET(LIBSAMPLERATE_LIB samplerate)
		SET(LIBSAMPLERATE_LIBPATH ${LIBSAMPLERATE}/lib)
	ENDIF(WITH_SAMPLERATE)

	SET(LLIBS stdc++ SystemStubs)

	IF (WITH_COCOA)
		SET(PLATFORM_CFLAGS "-pipe -funsigned-char -DGHOST_COCOA")
		SET(PLATFORM_LINKFLAGS "-fexceptions -framework CoreServices -framework Foundation -framework IOKit -framework AppKit -framework Cocoa -framework Carbon -framework AudioUnit -framework AudioToolbox -framework CoreAudio")
		IF(USE_QTKIT)
			SET(PLATFORM_CFLAGS "${PLATFORM_CFLAGS} -DUSE_QTKIT")
			SET(PLATFORM_LINKFLAGS "${PLATFORM_LINKFLAGS} -framework QTKit")
			IF(CMAKE_OSX_ARCHITECTURES MATCHES i386)
				SET(PLATFORM_LINKFLAGS "${PLATFORM_LINKFLAGS} -framework QuickTime")
				#libSDL still needs 32bit carbon quicktime 
			ENDIF(CMAKE_OSX_ARCHITECTURES MATCHES i386)
		ELSEIF(WITH_CODEC_QUICKTIME)
			SET(PLATFORM_LINKFLAGS "${PLATFORM_LINKFLAGS} -framework QuickTime")
		ENDIF(USE_QTKIT)
	ELSE (WITH_COCOA)
		SET(PLATFORM_CFLAGS "-pipe -funsigned-char")
		SET(PLATFORM_LINKFLAGS "-fexceptions -framework CoreServices -framework Foundation -framework IOKit -framework AppKit -framework Carbon -framework AGL -framework AudioUnit -framework AudioToolbox -framework CoreAudio -framework QuickTime")
	ENDIF (WITH_COCOA)

	IF (WITH_OPENCOLLADA)
		SET(OPENCOLLADA ${LIBDIR}/opencollada)
		SET(OPENCOLLADA_INC ${OPENCOLLADA}/include)
		SET(OPENCOLLADA_LIBPATH ${OPENCOLLADA}/lib)
		SET(OPENCOLLADA_LIB "OpenCOLLADASaxFrameworkLoader -lOpenCOLLADAFramework -lOpenCOLLADABaseUtils -lOpenCOLLADAStreamWriter -lMathMLSolver -lGeneratedSaxParser -lUTF -lxml2 -lbuffer -lftoa" )
		#pcre is bundled with openCollada
		#SET(PCRE ${LIBDIR}/pcre)
		#SET(PCRE_LIBPATH ${PCRE}/lib)
		SET(PCRE_LIB pcre)
		#libxml2 is used
		#SET(EXPAT ${LIBDIR}/expat)
		#SET(EXPAT_LIBPATH ${EXPAT}/lib)
		SET(EXPAT_LIB)
	ENDIF (WITH_OPENCOLLADA)

	IF(WITH_SDL)
		SET(SDL ${LIBDIR}/sdl)
		SET(SDL_INCLUDE_DIR ${SDL}/include)
		SET(SDL_LIBRARY SDL)
		SET(SDL_LIBPATH ${SDL}/lib)
	ENDIF(WITH_SDL)

	SET(PNG "${LIBDIR}/png")
	SET(PNG_INC "${PNG}/include")
	SET(PNG_LIBPATH ${PNG}/lib)

	SET(JPEG "${LIBDIR}/jpeg")
	SET(JPEG_INC "${JPEG}/include")
	SET(JPEG_LIBPATH ${JPEG}/lib)

	IF(WITH_IMAGE_TIFF)
		SET(TIFF ${LIBDIR}/tiff)
		SET(TIFF_INCLUDE_DIR ${TIFF}/include)
		SET(TIFF_LIBRARY tiff)
		SET(TIFF_LIBPATH ${TIFF}/lib)
	ENDIF(WITH_IMAGE_TIFF)

	SET(EXETYPE MACOSX_BUNDLE)

	SET(CMAKE_C_FLAGS_DEBUG "-fno-strict-aliasing -g")
	SET(CMAKE_CXX_FLAGS_DEBUG "-fno-strict-aliasing -g")
	IF(CMAKE_OSX_ARCHITECTURES MATCHES "i386")
		SET(CMAKE_CXX_FLAGS_RELEASE "-O3 -mdynamic-no-pic -ftree-vectorize -msse -msse2 -fvariable-expansion-in-unroller")
		SET(CMAKE_C_FLAGS_RELEASE "-O3 -mdynamic-no-pic -ftree-vectorize -msse -msse2 -fvariable-expansion-in-unroller")
	ELSEIF(CMAKE_OSX_ARCHITECTURES MATCHES "x86_64")
		SET(CMAKE_CXX_FLAGS_RELEASE "-O3 -mdynamic-no-pic -ftree-vectorize -msse -msse2 -msse3 -mssse3 -fvariable-expansion-in-unroller")
		SET(CMAKE_C_FLAGS_RELEASE "-O3 -mdynamic-no-pic -ftree-vectorize -msse -msse2 -msse3 -mssse3 -fvariable-expansion-in-unroller")
	ELSE(CMAKE_OSX_ARCHITECTURES MATCHES "x86_64")
		SET(CMAKE_C_FLAGS_RELEASE "-mdynamic-no-pic -fno-strict-aliasing")
		SET(CMAKE_CXX_FLAGS_RELEASE "-mdynamic-no-pic -fno-strict-aliasing")
	ENDIF(CMAKE_OSX_ARCHITECTURES MATCHES "i386")

	# Better warnings
	SET(C_WARNINGS "-Wall -Wno-char-subscripts -Wpointer-arith -Wcast-align -Wdeclaration-after-statement -Wno-unknown-pragmas")
	SET(CXX_WARNINGS "-Wall -Wno-invalid-offsetof -Wno-sign-compare")

ENDIF(UNIX AND NOT APPLE)


# buildinfo
IF(WITH_BUILDINFO)
	# BUILD_PLATFORM and BUILD_PLATFORM are taken from CMake
	IF(UNIX)
		EXEC_PROGRAM("date \"+%Y-%m-%d\"" OUTPUT_VARIABLE BUILD_DATE)
		EXEC_PROGRAM("date \"+%H:%M:%S\"" OUTPUT_VARIABLE BUILD_TIME)  
		EXEC_PROGRAM("svnversion ${CMAKE_SOURCE_DIR}" OUTPUT_VARIABLE BUILD_REV RETURN_VALUE BUILD_REV_RETURN)
		IF(BUILD_REV_RETURN)
			SET(BUILD_REV "unknown")
		ENDIF(BUILD_REV_RETURN)
	ENDIF(UNIX)
	
	IF(WIN32)
		EXEC_PROGRAM("cmd /c date /t" OUTPUT_VARIABLE BUILD_DATE)
		EXEC_PROGRAM("cmd /c time /t" OUTPUT_VARIABLE BUILD_TIME)  
		EXEC_PROGRAM("svnversion ${CMAKE_SOURCE_DIR}" OUTPUT_VARIABLE BUILD_REV RETURN_VALUE BUILD_REV_RETURN)
		IF(BUILD_REV_RETURN)
			SET(BUILD_REV "unknown")
		ENDIF(BUILD_REV_RETURN)
	ENDIF(WIN32)
ENDIF(WITH_BUILDINFO)
#-----------------------------------------------------------------------------
# Common.

IF(WITH_RAYOPTIMIZATION)
	IF(CMAKE_COMPILER_IS_GNUCC)
		IF(SUPPORT_SSE_BUILD)
			SET(PLATFORM_CFLAGS " -msse ${PLATFORM_CFLAGS}")
			ADD_DEFINITIONS(-D__SSE__ -D__MMX__)
		ENDIF(SUPPORT_SSE_BUILD)
		IF(SUPPORT_SSE2_BUILD)
			SET(PLATFORM_CFLAGS " -msse2 ${PLATFORM_CFLAGS}")
			ADD_DEFINITIONS(-D__SSE2__)
			IF(NOT SUPPORT_SSE_BUILD) # dont double up
				ADD_DEFINITIONS(-D__MMX__)
			ENDIF(NOT SUPPORT_SSE_BUILD)
		ENDIF(SUPPORT_SSE2_BUILD)
	ENDIF(CMAKE_COMPILER_IS_GNUCC)
ENDIF(WITH_RAYOPTIMIZATION)

IF(WITH_IMAGE_OPENJPEG)
	set(OPENJPEG ${CMAKE_SOURCE_DIR}/extern/libopenjpeg)
	set(OPENJPEG_INC ${OPENJPEG})
ENDIF(WITH_IMAGE_OPENJPEG)

IF(WITH_IMAGE_REDCODE)
	set(REDCODE ${CMAKE_SOURCE_DIR}/extern)
	set(REDCODE_INC ${REDCODE})
ENDIF(WITH_IMAGE_REDCODE)

#-----------------------------------------------------------------------------
# Blender WebPlugin

IF(WITH_WEBPLUGIN) 
	SET(GECKO_DIR "${CMAKE_SOURCE_DIR}/../gecko-sdk/" CACHE PATH "Gecko SDK path")
	SET(WEBPLUGIN_SANDBOX_MODE "apparmor" CACHE STRING "WEB Plugin sandbox mode, can be apparmor, privsep, none")

	SET(WITH_PLAYER ON)
ENDIF(WITH_WEBPLUGIN)


#-----------------------------------------------------------------------------
# Configure OpenGL.
FIND_PACKAGE(OpenGL)
INCLUDE_DIRECTORIES(${OPENGL_INCLUDE_DIR})
# UNSET(OPENGL_LIBRARIES CACHE) # not compat with older cmake 
# UNSET(OPENGL_xmesa_INCLUDE_DIR CACHE) # not compat with older cmake 

#-----------------------------------------------------------------------------
# Configure OpenMP.
IF(WITH_OPENMP)
	FIND_PACKAGE(OpenMP)	
	IF(OPENMP_FOUND)
		SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
		SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")

		IF(APPLE AND ${CMAKE_GENERATOR} MATCHES "Xcode")
			SET(CMAKE_XCODE_ATTRIBUTE_ENABLE_OPENMP_SUPPORT "YES")
		ENDIF(APPLE AND ${CMAKE_GENERATOR} MATCHES "Xcode")
	ELSE(OPENMP_FOUND)
		SET(WITH_OPENMP OFF)
	ENDIF(OPENMP_FOUND)
ENDIF(WITH_OPENMP) 

#-----------------------------------------------------------------------------
# Extra compile flags

IF((NOT WIN32) AND (NOT MSVC))
	# used for internal debug checks
	SET(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DDEBUG")
	SET(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -DDEBUG")
	
	# assert() checks for this.
	SET(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -DNDEBUG")
	SET(CMAKE_CXX_FLAGS_MINSIZEREL "${CMAKE_CXX_FLAGS_MINSIZEREL} -DNDEBUG")
	SET(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -DNDEBUG")

	SET(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -DNDEBUG")
	SET(CMAKE_C_FLAGS_MINSIZEREL "${CMAKE_C_FLAGS_MINSIZEREL} -DNDEBUG")
	SET(CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_RELWITHDEBINFO} -DNDEBUG")
ENDIF((NOT WIN32) AND (NOT MSVC))

IF(CMAKE_COMPILER_IS_GNUCC)
	SET(C_WARNINGS "${C_WARNINGS} -Wunused-parameter -Werror=strict-prototypes -Werror=declaration-after-statement -Werror=implicit-function-declaration -Werror=return-type")
ENDIF(CMAKE_COMPILER_IS_GNUCC)

SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${PLATFORM_CFLAGS} ${C_WARNINGS}")
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${PLATFORM_CFLAGS} ${CXX_WARNINGS}")

# better not define flags here but this is a debugging option thats off by default.
IF(WITH_CXX_GUARDEDALLOC)
	SET(CMAKE_CXX_FLAGS " -DWITH_CXX_GUARDEDALLOC -I${CMAKE_SOURCE_DIR}/intern/guardedalloc ${CMAKE_CXX_FLAGS}")
ENDIF(WITH_CXX_GUARDEDALLOC)

#-----------------------------------------------------------------------------
# Libraries
FILE(WRITE ${CMAKE_BINARY_DIR}/cmake_blender_libs.txt "")
ADD_SUBDIRECTORY(source)
ADD_SUBDIRECTORY(intern)
ADD_SUBDIRECTORY(extern)


#-----------------------------------------------------------------------------
# Blender Application
ADD_SUBDIRECTORY(source/creator)


#-----------------------------------------------------------------------------
# Blender Player
IF(WITH_PLAYER)
	ADD_SUBDIRECTORY(source/blenderplayer)
ENDIF(WITH_PLAYER)
