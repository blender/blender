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

CMAKE_MINIMUM_REQUIRED(VERSION 2.4)
PROJECT(Blender)

#-----------------------------------------------------------------------------
# Redirect output files

SET(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR}/bin)
SET(LIBRARY_OUTPUT_PATH  ${CMAKE_BINARY_DIR}/lib)

# Note! - Could create this from the blender version string
# ...but thats quite involved, make sure this matches the blender version.
SET(BLENDER_VERSION  2.5)

#-----------------------------------------------------------------------------
# Set default config options
OPTION(WITH_PLAYER        "Build Player" OFF)
OPTION(WITH_GAMEENGINE    "Enable Game Engine" ON)
OPTION(WITH_BULLET        "Enable Bullet (Physics Engine)" ON)
OPTION(WITH_INTERNATIONAL "Enable I18N   (International fonts and text)" ON)
OPTION(WITH_ELBEEM        "Enable Elbeem (Fluid Simulation)" ON)
OPTION(WITH_QUICKTIME     "Enable Quicktime Support" OFF)
OPTION(WITH_OPENEXR       "Enable OpenEXR Support (http://www.openexr.com)" ON)
OPTION(WITH_DDS           "Enable DDS Support" ON)
OPTION(WITH_FFMPEG        "Enable FFMPeg Support (http://ffmpeg.mplayerhq.hu/)" OFF)
OPTION(WITH_PYTHON        "Enable Embedded Python API" ON)
OPTION(WITH_SDL           "Enable SDL for sound and joystick support" ON)
OPTION(WITH_OPENJPEG      "Enable OpenJpeg Support (http://www.openjpeg.org/)" OFF)
OPTION(WITH_OPENAL        "Enable OpenAL Support (http://www.openal.org)" ON)
OPTION(WITH_WEBPLUGIN     "Enable Web Plugin (Unix only)" OFF)
OPTION(WITH_FFTW3         "Enable FFTW3 support" OFF)
OPTION(WITH_JACK          "Enable Jack Support (http://www.jackaudio.org)" OFF)
OPTION(WITH_SNDFILE       "Enable libsndfile Support (http://www.mega-nerd.com/libsndfile)" OFF)
OPTION(WITH_LZO           "Enable fast LZO compression, used for pointcache" ON)
OPTION(WITH_LZMA          "Enable best LZMA compression, used for pointcache" ON)
OPTION(WITH_CXX_GUARDEDALLOC "Enable GuardedAlloc for C++ memory allocation" OFF)
OPTION(WITH_BUILDINFO     "Include extra build details" ON)
OPTION(WITH_INSTALL       "Install accompanying scripts and language files needed to run blender" ON)
OPTION(WITH_OPENCOLLADA		"Enable OpenCollada Support (http://www.opencollada.org/)"	ON)

# Unix defaults to OpenMP On
IF(UNIX AND NOT APPLE)
	OPTION(WITH_OPENMP        "Enable OpenMP (has to be supported by the compiler)" ON)
ELSE()
	OPTION(WITH_OPENMP        "Enable OpenMP (has to be supported by the compiler)" OFF)
ENDIF()

OPTION(WITH_RAYOPTIMIZATION	"Enable use of SIMD (SSE) optimizations for the raytracer" OFF) 

IF (APPLE)
	OPTION(WITH_COCOA	  "Use Cocoa framework instead of deprecated Carbon" ON)
	OPTION(USE_QTKIT	  "Use QtKit instead of Carbon quicktime (needed for having partial quicktime for 64bit)" OFF)
	OPTION(WITH_LIBS10.5  "Use 10.5 libs (needed for 64bit builds)" OFF)
ENDIF (APPLE)

IF(NOT WITH_GAMEENGINE AND WITH_PLAYER)
	MESSAGE("WARNING: WITH_PLAYER needs WITH_GAMEENGINE")
ENDIF(NOT WITH_GAMEENGINE AND WITH_PLAYER)

# For alternate Python locations the commandline can be used to override detected/default cache settings, e.g:
# On Unix: 
#   cmake -D PYTHON_LIB=/usr/local/lib/python2.3/config/libpython2.3.so -D PYTHON_INC=/usr/local/include/python2.3 -D PYTHON_BINARY=/usr/local/bin/python2.3 -G "Unix Makefiles" ../blender
# On Macs: 
#   cmake -D PYTHON_INC=/System/Library/Frameworks/Python.framework/Versions/2.5/include/python2.5 -D PYTHON_LIBPATH=/System/Library/Frameworks/Python.framework/Versions/2.5/lib/python2.5/config -D PYTHON_BINARY=/System/Library/Frameworks/Python.framework/Versions/2.5/bin/python2.5 -G Xcode ../blender
#
# When changing any of this remember to update the notes in doc/blender-cmake.txt

#-----------------------------------------------------------------------------
# Load some macros.
INCLUDE(CMake/macros.cmake)

#-----------------------------------------------------------------------------
#Platform specifics

IF(UNIX AND NOT APPLE)


	IF(WITH_OPENAL)
		FIND_PACKAGE(OpenAL)
		IF(OPENAL_FOUND)
			SET(WITH_OPENAL ON)
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

	IF(WITH_SNDFILE)
	SET(SNDFILE /usr)
	SET(SNDFILE_INC ${SNDFILE}/include)
	SET(SNDFILE_LIB sndfile)
	SET(SNDFILE_LIBPATH ${SNDFILE}/lib)
	ENDIF(WITH_SNDFILE)

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

	FIND_PACKAGE(Freetype)
	# UNSET(FREETYPE_INCLUDE_DIRS CACHE) # cant use

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

	IF(WITH_SDL)
		FIND_PACKAGE(SDL)
		# UNSET(SDLMAIN_LIBRARY CACHE)
		IF(NOT SDL_FOUND)
			SET(WITH_SDL OFF)
		ENDIF(NOT SDL_FOUND)
	ENDIF(WITH_SDL)

	FIND_PATH(OPENEXR_INC
		ImfXdr.h
		PATHS
		/usr/local/include/OpenEXR
		/usr/include/OpenEXR
		/sw/include/OpenEXR
		/opt/local/include/OpenEXR
		/opt/csw/include/OpenEXR
		/opt/include/OpenEXR
	)
	SET(OPENEXR_LIB Half IlmImf Iex Imath)

	SET(FFMPEG /usr)
	SET(FFMPEG_INC ${FFMPEG}/include)
	SET(FFMPEG_LIB avformat avcodec avutil avdevice swscale)
	SET(FFMPEG_LIBPATH ${FFMPEG}/lib)
  
	IF(WITH_FFTW3)
		SET(FFTW3 /usr)
		SET(FFTW3_INC ${FFTW3}/include)
		SET(FFTW3_LIB fftw3)
		SET(FFTW3_LIBPATH ${FFTW3}/lib)
	ENDIF(WITH_FFTW3)

	SET(LIBSAMPLERATE /usr)
	SET(LIBSAMPLERATE_INC ${LIBSAMPLERATE}/include)
	SET(LIBSAMPLERATE_LIB samplerate)
	SET(LIBSAMPLERATE_LIBPATH ${LIBSAMPLERATE}/lib)

	FIND_PACKAGE(JPEG REQUIRED)

	FIND_PACKAGE(PNG REQUIRED)

	FIND_PACKAGE(ZLIB REQUIRED)

	IF (WITH_OPENCOLLADA)
		SET(OPENCOLLADA /usr/local/opencollada CACHE FILEPATH "OpenCollada Directory")
		SET(OPENCOLLADA_LIBPATH ${OPENCOLLADA}/lib)
		SET(OPENCOLLADA_LIB OpenCOLLADAStreamWriter OpenCOLLADASaxFrameworkLoader OpenCOLLADAFramework OpenCOLLADABaseUtils GeneratedSaxParser UTF MathMLSolver pcre ftoa buffer)
		SET(OPENCOLLADA_INC ${OPENCOLLADA})
		SET(PCRE /usr CACHE FILEPATH "PCRE Directory")
		SET(PCRE_LIBPATH ${PCRE}/lib)
		SET(PCRE_LIB pcre)
		SET(EXPAT /usr CACHE FILEPATH "Expat Directory")
		SET(EXPAT_LIBPATH ${EXPAT}/lib)
		SET(EXPAT_LIB expat)
	ENDIF (WITH_OPENCOLLADA)


	# Could use ${X11_Xinput_LIB} ${X11_X11_LIB} too
	SET(LLIBS "-lXi -lutil -lc -lm -lpthread -lstdc++ -lX11")

	IF(CMAKE_SYSTEM_NAME MATCHES "Linux")
		# BSD's dont use libdl.so
		SET(LLIBS "${LLIBS} -ldl")
	ENDIF(CMAKE_SYSTEM_NAME MATCHES "Linux")

	IF(WITH_OPENMP)
		SET(LLIBS "${LLIBS} -lgomp")
		SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fopenmp")
		SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fopenmp")
	ENDIF(WITH_OPENMP)


	SET(PLATFORM_CFLAGS "-pipe -fPIC -funsigned-char -fno-strict-aliasing -Wno-char-subscripts")

	SET(PLATFORM_LINKFLAGS "-pthread")

	# Better warnings
	SET(C_WARNINGS "-Wall -Wno-char-subscripts -Wpointer-arith -Wcast-align -Wdeclaration-after-statement -Wno-unknown-pragmas")
	SET(CXX_WARNINGS "-Wall -Wno-invalid-offsetof -Wno-sign-compare")

	INCLUDE_DIRECTORIES(${JPEG_INCLUDE_DIR} ${PNG_INCLUDE_DIR} ${ZLIB_INCLUDE_DIR} )
ENDIF(UNIX AND NOT APPLE)

IF(WIN32)

	# this file is included anyway when building under Windows with cl.exe
	#  INCLUDE(${CMAKE_ROOT}/Modules/Platform/Windows-cl.cmake)
  
	SET(LIBDIR ${CMAKE_SOURCE_DIR}/../lib/windows)

	# Setup 64bit and 64bit windows systems
	IF(CMAKE_CL_64)
		message("64 bit compiler detected.")
		SET(LIBDIR ${CMAKE_SOURCE_DIR}/../lib/win64)
	ENDIF(CMAKE_CL_64)

	SET(PYTHON ${LIBDIR}/python)
	SET(PYTHON_VERSION 3.1)
	SET(PYTHON_INC "${PYTHON}/include/python${PYTHON_VERSION}")
	# SET(PYTHON_BINARY python) # not used yet
	SET(PYTHON_LIB python31)
	SET(PYTHON_LIBPATH ${PYTHON}/lib)

	IF(CMAKE_CL_64)
		SET(WITH_OPENAL OFF)
	ELSE(CMAKE_CL_64)
		#SET(WITH_OPENAL ON)
		SET(OPENAL ${LIBDIR}/openal)
		SET(OPENAL_INCLUDE_DIR ${OPENAL}/include)
		SET(OPENAL_LIBRARY wrap_oal)
		SET(OPENAL_LIBPATH ${OPENAL}/lib)
	ENDIF(CMAKE_CL_64)

	IF(WITH_JACK)
		SET(JACK ${LIBDIR}/jack)
		SET(JACK_INC ${JACK}/include/jack ${JACK}/include)
		SET(JACK_LIB libjack)
		SET(JACK_LIBPATH ${JACK}/lib)
	ENDIF(WITH_JACK)

	IF(WITH_SNDFILE)
		SET(SNDFILE ${LIBDIR}/sndfile)
		SET(SNDFILE_INC ${SNDFILE}/include)
		SET(SNDFILE_LIB libsndfile-1)
		SET(SNDFILE_LIBPATH ${SNDFILE}/lib)
	ENDIF(WITH_SNDFILE)

	IF(CMAKE_CL_64)
		SET(PNG_LIBRARIES libpng)
	ELSE(CMAKE_CL_64)
		SET(PNG_LIBRARIES libpng_st)
	ENDIF(CMAKE_CL_64)
	SET(JPEG_LIBRARY libjpeg)

	SET(ZLIB ${LIBDIR}/zlib)
	SET(ZLIB_INC ${ZLIB}/include)
	IF(CMAKE_CL_64)
		SET(ZLIB_LIBRARIES libz)
	ELSE(CMAKE_CL_64)
		SET(ZLIB_LIBRARIES zlib)
	ENDIF(CMAKE_CL_64)
	SET(ZLIB_LIBPATH ${ZLIB}/lib)
  
	SET(PTHREADS ${LIBDIR}/pthreads)
	SET(PTHREADS_INC ${PTHREADS}/include)
	SET(PTHREADS_LIB pthreadVC2)
	SET(PTHREADS_LIBPATH ${PTHREADS}/lib)
  
	SET(ICONV ${LIBDIR}/iconv)
	SET(ICONV_INC ${ICONV}/include)
	SET(ICONV_LIB iconv)
	SET(ICONV_LIBPATH ${ICONV}/lib)
  
	IF(WITH_FFTW3)
		SET(FFTW3 ${LIBDIR}/fftw3)
		SET(FFTW3_INC ${FFTW3}/include)
		SET(FFTW3_LIB libfftw)
		SET(FFTW3_LIBPATH ${FFTW3}/lib)
	ENDIF(WITH_FFTW3)
  
	SET(GETTEXT ${LIBDIR}/gettext)
	SET(GETTEXT_INC ${GETTEXT}/include)
	IF(CMAKE_CL_64)
		SET(GETTEXT_LIB gettext)
	ELSE(CMAKE_CL_64)
		SET(GETTEXT_LIB gnu_gettext)
	ENDIF(CMAKE_CL_64)
	SET(GETTEXT_LIBPATH ${GETTEXT}/lib)

	SET(FREETYPE ${LIBDIR}/freetype)
	SET(FREETYPE_INCLUDE_DIRS ${FREETYPE}/include ${FREETYPE}/include/freetype2)
	SET(FREETYPE_LIBPATH ${FREETYPE}/lib)
	SET(FREETYPE_LIBRARY freetype2ST)
  
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

  
	SET(QUICKTIME ${LIBDIR}/QTDevWin)
	SET(QUICKTIME_INC ${QUICKTIME}/CIncludes)
	SET(QUICKTIME_LIB qtmlClient)
	SET(QUICKTIME_LIBPATH ${QUICKTIME}/Libraries)

	SET(FFMPEG ${LIBDIR}/ffmpeg)
	SET(FFMPEG_INC ${FFMPEG}/include ${FFMPEG}/include/msvc)
	SET(FFMPEG_LIB avcodec-52 avformat-52 avdevice-52 avutil-50 swscale-0)
	SET(FFMPEG_LIBPATH ${FFMPEG}/lib)

	SET(LIBSAMPLERATE ${LIBDIR}/samplerate)
	SET(LIBSAMPLERATE_INC ${LIBSAMPLERATE}/include)
	SET(LIBSAMPLERATE_LIB libsamplerate)
	SET(LIBSAMPLERATE_LIBPATH ${LIBSAMPLERATE}/lib)

	IF(CMAKE_CL_64)
		SET(LLIBS kernel32 user32 vfw32 winmm ws2_32 )
	ELSE(CMAKE_CL_64)
		SET(LLIBS kernel32 user32 gdi32 comdlg32 advapi32 shell32 ole32 oleaut32 uuid ws2_32 vfw32 winmm)
	ENDIF(CMAKE_CL_64)

	SET(CMAKE_CXX_FLAGS_DEBUG "/D_CRT_NONSTDC_NO_DEPRECATE /D_CRT_SECURE_NO_DEPRECATE /D_SCL_SECURE_NO_DEPRECATE /wd4800 /wd4244 /wd4305 /D_DEBUG /Od /Gm /EHsc /RTC1 /MTd /W3 /nologo /ZI /J" CACHE STRING "MSVC MT flags " FORCE)
	SET(CMAKE_CXX_FLAGS_RELEASE "/D_CRT_NONSTDC_NO_DEPRECATE /D_CRT_SECURE_NO_DEPRECATE /D_SCL_SECURE_NO_DEPRECATE /wd4800 /wd4244 /wd4305 /O2 /Ob2 /DNDEBUG /EHsc /MT /W3 /nologo /J" CACHE STRING "MSVC MT flags " FORCE)
	SET(CMAKE_CXX_FLAGS_MINSIZEREL "/D_CRT_NONSTDC_NO_DEPRECATE /D_CRT_SECURE_NO_DEPRECATE /D_SCL_SECURE_NO_DEPRECATE /wd4800 /wd4244 /wd4305 /O1 /Ob1 /DNDEBUG /EHsc /MT /W3 /nologo /J" CACHE STRING "MSVC MT flags " FORCE)
	SET(CMAKE_CXX_FLAGS_RELWITHDEBINFO "/D_CRT_NONSTDC_NO_DEPRECATE /D_CRT_SECURE_NO_DEPRECATE /D_SCL_SECURE_NO_DEPRECATE /wd4800 /wd4244 /wd4305 /O2 /Ob1 /DNDEBUG /EHsc /MT /W3 /nologo /Zi /J" CACHE STRING "MSVC MT flags " FORCE)
	SET(CMAKE_C_FLAGS_DEBUG "/D_CRT_NONSTDC_NO_DEPRECATE /D_CRT_SECURE_NO_DEPRECATE /D_SCL_SECURE_NO_DEPRECATE /wd4800 /wd4244 /wd4305 /D_DEBUG /Od /Gm /EHsc /RTC1 /MTd /W3 /nologo /ZI /J" CACHE STRING "MSVC MT flags " FORCE)
	SET(CMAKE_C_FLAGS_RELEASE "/D_CRT_NONSTDC_NO_DEPRECATE /D_CRT_SECURE_NO_DEPRECATE /D_SCL_SECURE_NO_DEPRECATE /wd4800 /wd4244 /wd4305 /O2 /Ob2 /DNDEBUG /EHsc /MT /W3 /nologo /J" CACHE STRING "MSVC MT flags " FORCE)
	SET(CMAKE_C_FLAGS_MINSIZEREL "/D_CRT_NONSTDC_NO_DEPRECATE /D_CRT_SECURE_NO_DEPRECATE /D_SCL_SECURE_NO_DEPRECATE /wd4800 /wd4244 /wd4305 /O1 /Ob1 /DNDEBUG /EHsc /MT /W3 /nologo /J" CACHE STRING "MSVC MT flags " FORCE)
	SET(CMAKE_C_FLAGS_RELWITHDEBINFO "/D_CRT_NONSTDC_NO_DEPRECATE /D_CRT_SECURE_NO_DEPRECATE /D_SCL_SECURE_NO_DEPRECATE /wd4800 /wd4244 /wd4305 /O2 /Ob1 /DNDEBUG /EHsc /MT /W3 /nologo /Zi /J" CACHE STRING "MSVC MT flags " FORCE)

	IF(WITH_OPENMP)
		SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /openmp ")
		SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /openmp ")
	ENDIF(WITH_OPENMP)

	SET(SDL ${LIBDIR}/sdl)
	SET(SDL_INCLUDE_DIR ${SDL}/include)
	SET(SDL_LIBRARY SDL)
	SET(SDL_LIBPATH ${SDL}/lib)

	SET(PNG "${LIBDIR}/png")
	SET(PNG_INC "${PNG}/include")
	SET(PNG_LIBPATH ${PNG}/lib)

	SET(JPEG "${LIBDIR}/jpeg")
	SET(JPEG_INC "${JPEG}/include")
	SET(JPEG_LIBPATH ${JPEG}/lib)

	SET(TIFF ${LIBDIR}/tiff)
	SET(TIFF_INC ${TIFF}/include)
  
	SET(WINTAB_INC ${LIBDIR}/wintab/include) 

	SET(OPENCOLLADA ${LIBDIR}/opencollada)
	SET(OPENCOLLADA_INC ${OPENCOLLADA}/include)
	SET(OPENCOLLADA_LIBPATH ${OPENCOLLADA}/lib)
	SET(OPENCOLLADA_LIB OpenCOLLADASaxFrameworkLoader OpenCOLLADAFramework OpenCOLLADABaseUtils OpenCOLLADAStreamWriter MathMLSolver GeneratedSaxParser UTF xml2 buffer ftoa)
	#pcre is bundled with openCollada
	#SET(PCRE ${LIBDIR}/pcre)
	#SET(PCRE_LIBPATH ${PCRE}/lib)
	SET(PCRE_LIB pcre)


	IF(CMAKE_CL_64)
		SET(PLATFORM_LINKFLAGS "/MACHINE:X64 /NODEFAULTLIB:libc.lib;MSVCRT.lib ")
	ELSE(CMAKE_CL_64)
		SET(PLATFORM_LINKFLAGS "/NODEFAULTLIB:libc.lib ")
	ENDIF(CMAKE_CL_64)
  
	SET(CMAKE_EXE_LINKER_FLAGS_DEBUG "${CMAKE_EXE_LINKER_FLAGS_DEBUG} /NODEFAULTLIB:libcmt.lib;libc.lib ")
ENDIF(WIN32)

IF(APPLE)
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

	IF(WITH_SNDFILE)
		SET(SNDFILE ${LIBDIR}/sndfile)
		SET(SNDFILE_INC ${SNDFILE}/include)
		SET(SNDFILE_LIB sndfile)
		SET(SNDFILE_LIBPATH ${SNDFILE}/lib)
	ENDIF(WITH_SNDFILE)

	SET(PYTHON_VERSION 3.1)

	IF(PYTHON_VERSION MATCHES 3.1)
		# we use precompiled libraries for py 3.1 and up by default

		SET(PYTHON ${LIBDIR}/python)
		SET(PYTHON_INC "${PYTHON}/include/python${PYTHON_VERSION}")
		# SET(PYTHON_BINARY "${PYTHON}/bin/python${PYTHON_VERSION}") # not used yet
		SET(PYTHON_LIB python${PYTHON_VERSION})
		SET(PYTHON_LIBPATH "${PYTHON}/lib/python${PYTHON_VERSION}")
		#    SET(PYTHON_LINKFLAGS "-u _PyMac_Error")  # won't  build with this enabled
	ELSE(PYTHON_VERSION MATCHES 3.1)
		# otherwise, use custom system framework

		SET(PYTHON /System/Library/Frameworks/Python.framework/Versions/)
		SET(PYTHON_VERSION 2.5)
		SET(PYTHON_INC "${PYTHON}${PYTHON_VERSION}/include/python${PYTHON_VERSION}")
		# SET(PYTHON_BINARY ${PYTHON}${PYTHON_VERSION}/bin/python${PYTHON_VERSION}) # not used yet
		SET(PYTHON_LIB "")
		SET(PYTHON_LIBPATH ${PYTHON}${PYTHON_VERSION}/lib/python${PYTHON_VERSION}/config)
		SET(PYTHON_LINKFLAGS "-u _PyMac_Error -framework System -framework Python")
	ENDIF(PYTHON_VERSION MATCHES 3.1)

	SET(GETTEXT ${LIBDIR}/gettext)
	SET(GETTEXT_INC "${GETTEXT}/include")
	SET(GETTEXT_LIB intl iconv)
	SET(GETTEXT_LIBPATH ${GETTEXT}/lib)
  
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
	SET(ZLIB_LIBRARIES z)

	SET(FREETYPE ${LIBDIR}/freetype)
	SET(FREETYPE_INCLUDE_DIRS ${FREETYPE}/include ${FREETYPE}/include/freetype2)
	SET(FREETYPE_LIBPATH ${FREETYPE}/lib)
	SET(FREETYPE_LIBRARY freetype)

	SET(OPENEXR ${LIBDIR}/openexr)
	SET(OPENEXR_INC ${OPENEXR}/include/OpenEXR ${OPENEXR}/include)
	SET(OPENEXR_LIB Iex Half IlmImf Imath IlmThread)
	SET(OPENEXR_LIBPATH ${OPENEXR}/lib)

	SET(FFMPEG ${LIBDIR}/ffmpeg)
	SET(FFMPEG_INC ${CMAKE_SOURCE_DIR}/extern/ffmpeg)
	SET(FFMPEG_LIB avcodec avdevice avformat avutil mp3lame swscale x264 xvidcore)
	SET(FFMPEG_LIBPATH ${FFMPEG}/lib)

	SET(LIBSAMPLERATE ${LIBDIR}/samplerate)
	SET(LIBSAMPLERATE_INC ${LIBSAMPLERATE}/include)
	SET(LIBSAMPLERATE_LIB samplerate)
	SET(LIBSAMPLERATE_LIBPATH ${LIBSAMPLERATE}/lib)

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
		ELSEIF(WITH_QUICKTIME)
			SET(PLATFORM_LINKFLAGS "${PLATFORM_LINKFLAGS} -framework QuickTime")
		ENDIF(USE_QTKIT)
	ELSE (WITH_COCOA)
		SET(PLATFORM_CFLAGS "-pipe -funsigned-char")
		SET(PLATFORM_LINKFLAGS "-fexceptions -framework CoreServices -framework Foundation -framework IOKit -framework AppKit -framework Carbon -framework AGL -framework AudioUnit -framework AudioToolbox -framework CoreAudio -framework QuickTime")
	ENDIF (WITH_COCOA)

	IF(WITH_OPENMP)
		SET(LLIBS "${LLIBS} -lgomp")
		SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fopenmp")
		SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fopenmp")
	ENDIF(WITH_OPENMP)

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

	SET(SDL ${LIBDIR}/sdl)
	SET(SDL_INCLUDE_DIR ${SDL}/include)
	SET(SDL_LIBRARY SDL)
	SET(SDL_LIBPATH ${SDL}/lib)

	SET(PNG "${LIBDIR}/png")
	SET(PNG_INC "${PNG}/include")
	SET(PNG_LIBPATH ${PNG}/lib)

	SET(JPEG "${LIBDIR}/jpeg")
	SET(JPEG_INC "${JPEG}/include")
	SET(JPEG_LIBPATH ${JPEG}/lib)

	SET(TIFF ${LIBDIR}/tiff)
	SET(TIFF_INC ${TIFF}/include)

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

ENDIF(APPLE)

IF(CMAKE_SYSTEM_NAME MATCHES "Linux")
	SET(BINRELOC ${CMAKE_SOURCE_DIR}/extern/binreloc)
	SET(BINRELOC_INC ${BINRELOC}/include)
ENDIF(CMAKE_SYSTEM_NAME MATCHES "Linux")


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

set(OPENJPEG ${CMAKE_SOURCE_DIR}/extern/libopenjpeg)
set(OPENJPEG_INC ${OPENJPEG})
set(OPENJPEG_LIb extern_libopenjpeg)

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
# Extra compile flags
SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${PLATFORM_CFLAGS} ${C_WARNINGS}")
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${PLATFORM_CFLAGS} ${CXX_WARNINGS}")

# better not define flags here but this is a debugging option thats off by default.
IF(WITH_CXX_GUARDEDALLOC)
	SET(CMAKE_CXX_FLAGS " -DWITH_CXX_GUARDEDALLOC -I${CMAKE_SOURCE_DIR}/intern/guardedalloc ${CMAKE_CXX_FLAGS}")
ENDIF(WITH_CXX_GUARDEDALLOC)

#-----------------------------------------------------------------------------
# Libraries
FILE(WRITE ${CMAKE_BINARY_DIR}/cmake_blender_libs.txt "")
ADD_SUBDIRECTORY(intern)
ADD_SUBDIRECTORY(extern)
ADD_SUBDIRECTORY(source)


#-----------------------------------------------------------------------------
# Blender Application
ADD_SUBDIRECTORY(source/creator)


#-----------------------------------------------------------------------------
# Blender Player
IF(WITH_PLAYER)
	ADD_SUBDIRECTORY(source/blenderplayer)
ENDIF(WITH_PLAYER)

