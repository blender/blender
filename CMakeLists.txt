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
# Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
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

#-----------------------------------------------------------------------------
# Set default config options
OPTION(WITH_PLAYER		"Build Player"						OFF)
OPTION(WITH_GAMEENGINE		"Enable Game Engine"					ON)
OPTION(WITH_BULLET		"Enable Bullet (Physics Engine)"			ON)
OPTION(WITH_INTERNATIONAL	"Enable I18N   (International fonts and text)"		ON)
OPTION(WITH_VERSE		"Enable Verse  (http://verse.blender.org)"		OFF)
OPTION(WITH_ELBEEM		"Enable Elbeem (Fluid Simulation)"			ON)
OPTION(WITH_QUICKTIME		"Enable Quicktime Support"				OFF)
OPTION(WITH_OPENEXR		"Enable OpenEXR Support (http://www.openexr.com)"	ON)
OPTION(WITH_FFMPEG		"Enable FFMPeg Support (http://ffmpeg.mplayerhq.hu/)"	OFF)
OPTION(WITH_OPENAL		"Enable OpenAL Support (http://www.openal.org)"		ON)
OPTION(YESIAMSTUPID		"Enable execution on 64-bit platforms"			OFF)
OPTION(WITH_OPENMP		"Enable OpenMP (has to be supported by the compiler)"	OFF)

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

IF(UNIX)
  INCLUDE(${CMAKE_ROOT}/Modules/FindOpenAL.cmake)
  IF(OPENAL_FOUND)
    SET(WITH_OPENAL ON)
    SET(OPENAL_LIB ${OPENAL_LIBRARY})
    SET(OPENAL_INC ${OPENAL_INCLUDE_DIR})
  ELSE(OPENAL_FOUND)
    SET(WITH_OPENAL OFF)
  ENDIF(OPENAL_FOUND)

  FIND_LIBRARY(ALUT_LIBRARY
    NAMES alut
    PATHS
    /usr/local/lib
    /usr/lib
    /sw/lib
    /opt/local/lib
    /opt/csw/lib
    /opt/lib
  )
  IF(ALUT_LIBRARY)
    SET(OPENAL_LIB ${OPENAL_LIB} ${ALUT_LIBRARY})
  ENDIF(ALUT_LIBRARY)

  FIND_LIBRARY(INTL_LIBRARY
    NAMES intl
    PATHS
    /usr/local/lib
    /usr/lib
    /sw/lib
    /opt/local/lib
    /opt/csw/lib
    /opt/lib
  )
  FIND_LIBRARY(ICONV_LIBRARY
    NAMES iconv
    PATHS
    /usr/local/lib
    /usr/lib
    /sw/lib
    /opt/local/lib
    /opt/csw/lib
    /opt/lib
  )
  IF(INTL_LIBRARY AND ICONV_LIBRARY)
    SET(GETTEXT_LIB ${INTL_LIBRARY} ${ICONV_LIBRARY})
  ENDIF(INTL_LIBRARY AND ICONV_LIBRARY)
  
  FIND_PATH(FREETYPE_INC
    freetype
    PATHS
    /usr/local/include/freetype2
    /usr/include/freetype2
    /sw/include/freetype2
    /opt/local/include/freetype2
    /opt/csw/include/freetype2
    /opt/include/freetype2
    NO_DEFAULT_PATH
  )
  SET(FREETYPE_LIB freetype)

  INCLUDE(${CMAKE_ROOT}/Modules/FindPythonLibs.cmake)
  SET(PYTHON_INC "${PYTHON_INCLUDE_PATH}" CACHE STRING "")
  SET(PYTHON_LIB "${PYTHON_LIBRARIES}" CACHE STRING "")
  INCLUDE(${CMAKE_ROOT}/Modules/FindPythonInterp.cmake)
  SET(PYTHON_BINARY ${PYTHON_EXECUTABLE} CACHE STRING "")
  SET(PYTHON_LINKFLAGS "-Xlinker -export-dynamic")

  INCLUDE(${CMAKE_ROOT}/Modules/FindSDL.cmake)
  SET(SDL_INC ${SDL_INCLUDE_DIR})
  SET(SDL_LIB ${SDL_LIBRARY})

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
  SET(FFMPEG_LIB avformat avcodec avutil)
  SET(FFMPEG_LIBPATH ${FFMPEG}/lib)

  SET(JPEG_LIB jpeg)

  SET(PNG_LIB png)

  SET(ZLIB_LIB z)

  SET(LLIBS "-lXi -lutil -lc -lm -lpthread -lstdc++")

  IF(WITH_OPENMP)
    SET(LLIBS "${LLIBS} -lgomp")
    SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fopenmp")
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fopenmp")
  ENDIF(WITH_OPENMP)


  SET(PLATFORM_CFLAGS "-pipe -fPIC -funsigned-char -fno-strict-aliasing -DXP_UNIX -Wno-char-subscripts")

  SET(PLATFORM_LINKFLAGS "-pthread")

  INCLUDE_DIRECTORIES(/usr/include /usr/local/include)
ENDIF(UNIX)

IF(WIN32)

  INCLUDE(${CMAKE_ROOT}/Modules/Platform/Windows-cl.cmake)
  
  SET(LIBDIR ${CMAKE_SOURCE_DIR}/../lib/windows)

  # Setup 64bit and 64bit windows systems
  IF(CMAKE_CL_64)
    message("64 bit compiler detected.")
    SET(LIBDIR ${CMAKE_SOURCE_DIR}/../lib/win64)
  ENDIF(CMAKE_CL_64)

  SET(PYTHON ${LIBDIR}/python)
  SET(PYTHON_VERSION 2.5)
  SET(PYTHON_INC "${PYTHON}/include/python${PYTHON_VERSION}")
  SET(PYTHON_BINARY python)
  SET(PYTHON_LIB python25)
  SET(PYTHON_LIBPATH ${PYTHON}/lib)

  #SET(WITH_OPENAL ON)
  SET(OPENAL ${LIBDIR}/openal)
  SET(OPENAL_INC ${OPENAL}/include ${OPENAL}/include/AL)
  SET(OPENAL_LIB openal_static)
  SET(OPENAL_LIBPATH ${OPENAL}/lib)

  IF(CMAKE_CL_64)
  	SET(PNG_LIB libpng)
  ELSE(CMAKE_CL_64)
  	SET(PNG_LIB libpng_st)
  ENDIF(CMAKE_CL_64)
  SET(JPEG_LIB libjpeg)

  SET(ZLIB ${LIBDIR}/zlib)
  SET(ZLIB_INC ${ZLIB}/include)
  IF(CMAKE_CL_64)
  	SET(ZLIB_LIB zlib)
  ELSE(CMAKE_CL_64)
  	SET(ZLIB_LIB libz)
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
  
  SET(GETTEXT ${LIBDIR}/gettext)
  SET(GETTEXT_INC ${GETTEXT}/include)
  SET(GETTEXT_LIB gnu_gettext)
  SET(GETTEXT_LIBPATH ${GETTEXT}/lib)

  SET(FREETYPE ${LIBDIR}/freetype)
  SET(FREETYPE_INC ${FREETYPE}/include ${FREETYPE}/include/freetype2)
  SET(FREETYPE_LIBPATH ${FREETYPE}/lib)
  SET(FREETYPE_LIB freetype2ST)
  
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
  SET(FFMPEG_INC ${FFMPEG}/include)
  SET(FFMPEG_LIB avcodec-51 avformat-52 avdevice-52 avutil-49 swscale-0)
  SET(FFMPEG_LIBPATH ${FFMPEG}/lib)

  SET(LLIBS kernel32 user32 gdi32 comdlg32 advapi32 shell32 ole32 oleaut32 uuid ws2_32 vfw32 winmm)
  IF(WITH_OPENAL)
    SET(LLIBS ${LLIBS} dxguid)
  ENDIF(WITH_OPENAL)

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
  SET(SDL_INC ${SDL}/include)
  SET(SDL_LIB SDL)
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

  IF(CMAKE_CL_64)
  SET(PLATFORM_LINKFLAGS "/NODEFAULTLIB:libc.lib;MSVCRT.lib ")
  ELSE(CMAKE_CL_64)
  SET(PLATFORM_LINKFLAGS "/NODEFAULTLIB:libc.lib ")
  ENDIF(CMAKE_CL_64)
  SET(CMAKE_EXE_LINKER_FLAGS_DEBUG "${CMAKE_EXE_LINKER_FLAGS_DEBUG} /NODEFAULTLIB:libcmt.lib;libc.lib ")
ENDIF(WIN32)

IF(APPLE)
  IF(CMAKE_OSX_ARCHITECTURES MATCHES i386)
    SET(LIBDIR ${CMAKE_SOURCE_DIR}/../lib/darwin-8.x.i386)
  ELSE(CMAKE_OSX_ARCHITECTURES MATCHES i386)
    SET(LIBDIR ${CMAKE_SOURCE_DIR}/../lib/darwin-6.1-powerpc)
  ENDIF(CMAKE_OSX_ARCHITECTURES MATCHES i386)

  INCLUDE(${CMAKE_ROOT}/Modules/FindOpenAL.cmake)
  IF(OPENAL_FOUND)
    SET(WITH_OPENAL ON)
    SET(OPENAL_LIB ${OPENAL_LIBRARY})
    SET(OPENAL_INC ${OPENAL_INCLUDE_DIR})
  ELSE(OPENAL_FOUND)
    SET(WITH_OPENAL OFF)
  ENDIF(OPENAL_FOUND)

  SET(PYTHON /System/Library/Frameworks/Python.framework/Versions/)
  SET(PYTHON_VERSION 2.3)
  SET(PYTHON_INC "${PYTHON}${PYTHON_VERSION}/include/python${PYTHON_VERSION}" CACHE STRING "")
  SET(PYTHON_BINARY ${PYTHON}${PYTHON_VERSION}/bin/python${PYTHON_VERSION} CACHE STRING "")
  SET(PYTHON_LIB "")
  SET(PYTHON_LIBPATH ${PYTHON}${PYTHON_VERSION}/lib/python${PYTHON_VERSION}/config CACHE STRING "")
  SET(PYTHON_LINKFLAGS "-u __dummy -u _PyMac_Error -framework System -framework Python")

  SET(GETTEXT ${LIBDIR}/gettext)
  SET(GETTEXT_INC "${GETTEXT}/include")
  SET(GETTEXT_LIB intl iconv)
  SET(GETTEXT_LIBPATH ${GETTEXT}/lib)

  SET(PNG_LIB png)
  SET(JPEG_LIB jpeg)

  SET(ZLIB /usr)
  SET(ZLIB_INC "${ZLIB}/include")
  SET(ZLIB_LIB z)

  SET(FREETYPE ${LIBDIR}/freetype)
  SET(FREETYPE_INC ${FREETYPE}/include ${FREETYPE}/include/freetype2)
  SET(FREETYPE_LIBPATH ${FREETYPE}/lib)
  SET(FREETYPE_LIB freetype)

  SET(OPENEXR ${LIBDIR}/openexr)
  SET(OPENEXR_INC ${OPENEXR}/include/OpenEXR ${OPENEXR}/include)
  IF(CMAKE_OSX_ARCHITECTURES MATCHES i386)
    SET(OPENEXR_LIB Iex Half IlmImf Imath IlmThread)
  ELSE(CMAKE_OSX_ARCHITECTURES MATCHES i386)
    SET(OPENEXR_LIB Iex Half IlmImf Imath)
  ENDIF(CMAKE_OSX_ARCHITECTURES MATCHES i386)
  SET(OPENEXR_LIBPATH ${OPENEXR}/lib)

  SET(LLIBS stdc++ SystemStubs)

  SET(PLATFORM_CFLAGS "-pipe -fPIC -funsigned-char -fno-strict-aliasing")
  SET(PLATFORM_LINKFLAGS "-fexceptions -framework CoreServices -framework Foundation -framework IOKit -framework AppKit -framework Carbon -framework AGL -framework AudioUnit -framework AudioToolbox -framework CoreAudio -framework QuickTime")

  IF(WITH_OPENMP)
    SET(LLIBS "${LLIBS} -lgomp ")
    SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fopenmp ")
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fopenmp ")
  ENDIF(WITH_OPENMP)

  SET(SDL ${LIBDIR}/sdl)
  SET(SDL_INC ${SDL}/include)
  SET(SDL_LIB SDL)
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
ENDIF(APPLE)

IF(CMAKE_SYSTEM_NAME MATCHES "Linux")
  SET(BINRELOC ${CMAKE_SOURCE_DIR}/extern/binreloc)
  SET(BINRELOC_INC ${BINRELOC}/include)
ENDIF(CMAKE_SYSTEM_NAME MATCHES "Linux")

#-----------------------------------------------------------------------------
# Common.
SET(VERSE_INC ${CMAKE_SOURCE_DIR}/extern/verse/dist)

SET(FTGL ${CMAKE_SOURCE_DIR}/extern/bFTGL)
SET(FTGL_INC ${FTGL}/include)
SET(FTGL_LIB extern_ftgl)


#-----------------------------------------------------------------------------
# Configure OpenGL.
INCLUDE(${CMAKE_ROOT}/Modules/FindOpenGL.cmake)
INCLUDE_DIRECTORIES(${OPENGL_INCLUDE_DIR})
#-----------------------------------------------------------------------------
# Extra compile flags
IF(WITH_GAMEENGINE)
  SET(PLATFORM_CFLAGS "${PLATFORM_CFLAGS} -DGAMEBLENDER ")
ENDIF(WITH_GAMEENGINE)
IF(WITH_BULLET)
  SET(PLATFORM_CFLAGS "${PLATFORM_CFLAGS} -DWITH_BULLET ")
ENDIF(WITH_BULLET)
SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${PLATFORM_CFLAGS} ")
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${PLATFORM_CFLAGS} ")

#-----------------------------------------------------------------------------
# Libraries
FILE(WRITE ${CMAKE_BINARY_DIR}/cmake_blender_libs.txt "")
SUBDIRS(
  intern
  extern
  source
)


#-----------------------------------------------------------------------------
# Blender Application
SUBDIRS(source/creator)

#-----------------------------------------------------------------------------
# Blender Player
IF(WITH_PLAYER)
  SUBDIRS(blenderplayer)
ENDIF(WITH_PLAYER)
