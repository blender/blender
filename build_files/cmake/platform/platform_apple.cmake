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

# Libraries configuration for Apple.

macro(find_package_wrapper)
# do nothing, just satisfy the macro
endmacro()

if(NOT DEFINED LIBDIR)
	if(WITH_CXX11)
		set(LIBDIR ${CMAKE_SOURCE_DIR}/../lib/darwin)
	else()
		set(LIBDIR ${CMAKE_SOURCE_DIR}/../lib/darwin-9.x.universal)
	endif()
else()
	message(STATUS "Using pre-compiled LIBDIR: ${LIBDIR}")
endif()
if(NOT EXISTS "${LIBDIR}/")
	message(FATAL_ERROR "Mac OSX requires pre-compiled libs at: '${LIBDIR}'")
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

if(WITH_ALEMBIC)
	set(ALEMBIC ${LIBDIR}/alembic)
	set(ALEMBIC_INCLUDE_DIR ${ALEMBIC}/include)
	set(ALEMBIC_INCLUDE_DIRS ${ALEMBIC_INCLUDE_DIR})
	set(ALEMBIC_LIBPATH ${ALEMBIC}/lib)
	set(ALEMBIC_LIBRARIES Alembic)
	set(ALEMBIC_FOUND ON)
endif()

if(WITH_OPENSUBDIV OR WITH_CYCLES_OPENSUBDIV)
	set(OPENSUBDIV ${LIBDIR}/opensubdiv)
	set(OPENSUBDIV_LIBPATH ${OPENSUBDIV}/lib)
	find_library(OSD_LIB_CPU NAMES osdCPU PATHS ${OPENSUBDIV_LIBPATH})
	find_library(OSD_LIB_GPU NAMES osdGPU PATHS ${OPENSUBDIV_LIBPATH})
	set(OPENSUBDIV_INCLUDE_DIR ${OPENSUBDIV}/include)
	set(OPENSUBDIV_INCLUDE_DIRS ${OPENSUBDIV_INCLUDE_DIR})
	list(APPEND OPENSUBDIV_LIBRARIES ${OSD_LIB_CPU} ${OSD_LIB_GPU})
endif()

if(WITH_JACK)
	find_library(JACK_FRAMEWORK
		NAMES jackmp
	)
	set(JACK_INCLUDE_DIRS ${JACK_FRAMEWORK}/headers)
	if(NOT JACK_FRAMEWORK)
		set(WITH_JACK OFF)
	endif()
endif()

if(WITH_CODEC_SNDFILE)
	set(SNDFILE ${LIBDIR}/sndfile)
	set(SNDFILE_INCLUDE_DIRS ${SNDFILE}/include)
	set(SNDFILE_LIBRARIES sndfile FLAC ogg vorbis vorbisenc)
	set(SNDFILE_LIBPATH ${SNDFILE}/lib ${LIBDIR}/ffmpeg/lib)  # TODO, deprecate
endif()

if(WITH_PYTHON)
	# we use precompiled libraries for py 3.5 and up by default
	set(PYTHON_VERSION 3.5)
	if(NOT WITH_PYTHON_MODULE AND NOT WITH_PYTHON_FRAMEWORK)
		# normally cached but not since we include them with blender
		set(PYTHON_INCLUDE_DIR "${LIBDIR}/python/include/python${PYTHON_VERSION}m")
		set(PYTHON_EXECUTABLE "${LIBDIR}/python/bin/python${PYTHON_VERSION}m")
		set(PYTHON_LIBRARY python${PYTHON_VERSION}m)
		set(PYTHON_LIBPATH "${LIBDIR}/python/lib/python${PYTHON_VERSION}")
		# set(PYTHON_LINKFLAGS "-u _PyMac_Error")  # won't  build with this enabled
	else()
		# module must be compiled against Python framework
		set(_py_framework "/Library/Frameworks/Python.framework/Versions/${PYTHON_VERSION}")

		set(PYTHON_INCLUDE_DIR "${_py_framework}/include/python${PYTHON_VERSION}m")
		set(PYTHON_EXECUTABLE "${_py_framework}/bin/python${PYTHON_VERSION}m")
		set(PYTHON_LIBPATH "${_py_framework}/lib/python${PYTHON_VERSION}/config-${PYTHON_VERSION}m")
		#set(PYTHON_LIBRARY python${PYTHON_VERSION})
		#set(PYTHON_LINKFLAGS "-u _PyMac_Error -framework Python")  # won't  build with this enabled

		unset(_py_framework)
	endif()

	# uncached vars
	set(PYTHON_INCLUDE_DIRS "${PYTHON_INCLUDE_DIR}")
	set(PYTHON_LIBRARIES  "${PYTHON_LIBRARY}")

	if(NOT EXISTS "${PYTHON_EXECUTABLE}")
		message(FATAL_ERROR "Python executable missing: ${PYTHON_EXECUTABLE}")
	endif()
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
	set(OPENEXR_INCLUDE_DIR ${OPENEXR}/include)
	set(OPENEXR_INCLUDE_DIRS ${OPENEXR_INCLUDE_DIR} ${OPENEXR}/include/OpenEXR)
	if(WITH_CXX11)
		set(OPENEXR_POSTFIX -2_2)
	else()
		set(OPENEXR_POSTFIX)
	endif()
	set(OPENEXR_LIBRARIES
		Iex${OPENEXR_POSTFIX}
		Half
		IlmImf${OPENEXR_POSTFIX}
		Imath${OPENEXR_POSTFIX}
		IlmThread${OPENEXR_POSTFIX})
	set(OPENEXR_LIBPATH ${OPENEXR}/lib)
endif()

if(WITH_CODEC_FFMPEG)
	set(FFMPEG ${LIBDIR}/ffmpeg)
	set(FFMPEG_INCLUDE_DIRS ${FFMPEG}/include)
	set(FFMPEG_LIBRARIES
		avcodec avdevice avformat avutil
		mp3lame swscale x264 xvidcore theora theoradec theoraenc vorbis vorbisenc vorbisfile ogg
	)
	if(WITH_CXX11)
		set(FFMPEG_LIBRARIES ${FFMPEG_LIBRARIES} schroedinger orc vpx webp swresample)
	endif()
	set(FFMPEG_LIBPATH ${FFMPEG}/lib)
endif()

if(WITH_OPENJPEG OR WITH_CODEC_FFMPEG)
	# use openjpeg from libdir that is linked into ffmpeg
	if(WITH_CXX11)
		set(OPENJPEG ${LIBDIR}/openjpeg)
		set(WITH_SYSTEM_OPENJPEG ON)
		set(OPENJPEG_INCLUDE_DIRS ${OPENJPEG}/include)
		set(OPENJPEG_LIBRARIES ${OPENJPEG}/lib/libopenjpeg.a)
	endif()
endif()

find_library(SYSTEMSTUBS_LIBRARY
	NAMES
	SystemStubs
	PATHS
)
mark_as_advanced(SYSTEMSTUBS_LIBRARY)
if(SYSTEMSTUBS_LIBRARY)
	list(APPEND PLATFORM_LINKLIBS SystemStubs)
endif()

set(PLATFORM_CFLAGS "-pipe -funsigned-char")
set(PLATFORM_LINKFLAGS
	"-fexceptions -framework CoreServices -framework Foundation -framework IOKit -framework AppKit -framework Cocoa -framework Carbon -framework AudioUnit -framework AudioToolbox -framework CoreAudio"
)
if(WITH_CODEC_QUICKTIME)
	set(PLATFORM_LINKFLAGS "${PLATFORM_LINKFLAGS} -framework QTKit")
	if(CMAKE_OSX_ARCHITECTURES MATCHES i386)
		set(PLATFORM_LINKFLAGS "${PLATFORM_LINKFLAGS} -framework QuickTime")
		# libSDL still needs 32bit carbon quicktime
	endif()
endif()

if(WITH_CXX11)
	list(APPEND PLATFORM_LINKLIBS c++)
else()
	list(APPEND PLATFORM_LINKLIBS stdc++)
endif()

if(WITH_JACK)
	set(PLATFORM_LINKFLAGS "${PLATFORM_LINKFLAGS} -F/Library/Frameworks -weak_framework jackmp")
endif()

if(WITH_PYTHON_MODULE OR WITH_PYTHON_FRAMEWORK)
	# force cmake to link right framework
	set(PLATFORM_LINKFLAGS "${PLATFORM_LINKFLAGS} /Library/Frameworks/Python.framework/Versions/${PYTHON_VERSION}/Python")
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
	set(OPENCOLLADA_LIBRARIES
		OpenCOLLADASaxFrameworkLoader
		-lOpenCOLLADAFramework
		-lOpenCOLLADABaseUtils
		-lOpenCOLLADAStreamWriter
		-lMathMLSolver
		-lGeneratedSaxParser
		-lxml2 -lbuffer -lftoa
	)
	# Use UTF functions from collada if LLVM is not enabled
	if(NOT WITH_LLVM)
		list(APPEND OPENCOLLADA_LIBRARIES -lUTF)
	endif()
	# pcre is bundled with openCollada
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
	set(SDL_LIBRARY SDL2)
	set(SDL_LIBPATH ${SDL}/lib)
	if(WITH_CXX11)
		set(PLATFORM_LINKFLAGS "${PLATFORM_LINKFLAGS} -framework ForceFeedback")
	else()
		set(PLATFORM_LINKFLAGS "${PLATFORM_LINKFLAGS} -lazy_framework ForceFeedback")
	endif()
endif()

set(PNG "${LIBDIR}/png")
set(PNG_INCLUDE_DIRS "${PNG}/include")
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

if(WITH_BOOST)
	set(BOOST ${LIBDIR}/boost)
	set(BOOST_INCLUDE_DIR ${BOOST}/include)
	if(WITH_CXX11)
		set(BOOST_POSTFIX)
	else()
		set(BOOST_POSTFIX -mt)
	endif()
	set(BOOST_LIBRARIES
		boost_date_time${BOOST_POSTFIX}
		boost_filesystem${BOOST_POSTFIX}
		boost_regex${BOOST_POSTFIX}
		boost_system${BOOST_POSTFIX}
		boost_thread${BOOST_POSTFIX}
		boost_wave${BOOST_POSTFIX}
	)
	if(WITH_INTERNATIONAL)
		list(APPEND BOOST_LIBRARIES boost_locale${BOOST_POSTFIX})
	endif()
	if(WITH_CYCLES_NETWORK)
		list(APPEND BOOST_LIBRARIES boost_serialization${BOOST_POSTFIX})
	endif()
	if(WITH_OPENVDB)
		list(APPEND BOOST_LIBRARIES boost_iostreams${BOOST_POSTFIX})
	endif()
	set(BOOST_LIBPATH ${BOOST}/lib)
	set(BOOST_DEFINITIONS)
endif()

if(WITH_INTERNATIONAL OR WITH_CODEC_FFMPEG)
	set(PLATFORM_LINKFLAGS "${PLATFORM_LINKFLAGS} -liconv") # boost_locale and ffmpeg needs it !
endif()

if(WITH_OPENIMAGEIO)
	set(OPENIMAGEIO ${LIBDIR}/openimageio)
	set(OPENIMAGEIO_INCLUDE_DIRS ${OPENIMAGEIO}/include)
	set(OPENIMAGEIO_LIBRARIES
		${OPENIMAGEIO}/lib/libOpenImageIO.a
		${PNG_LIBRARIES}
		${JPEG_LIBRARIES}
		${TIFF_LIBRARY}
		${OPENEXR_LIBRARIES}
		${ZLIB_LIBRARIES}
	)
	if(WITH_CXX11)
		set(OPENIMAGEIO_LIBRARIES ${OPENIMAGEIO_LIBRARIES} ${LIBDIR}/ffmpeg/lib/libwebp.a)
	endif()
	set(OPENIMAGEIO_LIBPATH
		${OPENIMAGEIO}/lib
		${JPEG_LIBPATH}
		${PNG_LIBPATH}
		${TIFF_LIBPATH}
		${OPENEXR_LIBPATH}
		${ZLIB_LIBPATH}
	)
	set(OPENIMAGEIO_DEFINITIONS "-DOIIO_STATIC_BUILD")
	set(OPENIMAGEIO_IDIFF "${LIBDIR}/openimageio/bin/idiff")
endif()

if(WITH_OPENCOLORIO)
	set(OPENCOLORIO ${LIBDIR}/opencolorio)
	set(OPENCOLORIO_INCLUDE_DIRS ${OPENCOLORIO}/include)
	set(OPENCOLORIO_LIBRARIES OpenColorIO tinyxml yaml-cpp)
	set(OPENCOLORIO_LIBPATH ${OPENCOLORIO}/lib)
endif()

if(WITH_OPENVDB)
	set(OPENVDB ${LIBDIR}/openvdb)
	set(OPENVDB_INCLUDE_DIRS ${OPENVDB}/include)
	set(TBB_INCLUDE_DIRS ${LIBDIR}/tbb/include)
	set(TBB_LIBRARIES ${LIBDIR}/tbb/lib/libtbb.a)
	set(OPENVDB_LIBRARIES openvdb blosc ${TBB_LIBRARIES})
	set(OPENVDB_LIBPATH ${LIBDIR}/openvdb/lib)
	set(OPENVDB_DEFINITIONS)
endif()

if(WITH_LLVM)
	set(LLVM_ROOT_DIR ${LIBDIR}/llvm CACHE PATH	"Path to the LLVM installation")
	set(LLVM_VERSION "3.4" CACHE STRING	"Version of LLVM to use")
	if(EXISTS "${LLVM_ROOT_DIR}/bin/llvm-config")
		set(LLVM_CONFIG "${LLVM_ROOT_DIR}/bin/llvm-config")
	else()
		set(LLVM_CONFIG llvm-config)
	endif()
	execute_process(COMMAND ${LLVM_CONFIG} --version
			OUTPUT_VARIABLE LLVM_VERSION
			OUTPUT_STRIP_TRAILING_WHITESPACE)
	execute_process(COMMAND ${LLVM_CONFIG} --prefix
			OUTPUT_VARIABLE LLVM_ROOT_DIR
			OUTPUT_STRIP_TRAILING_WHITESPACE)
	execute_process(COMMAND ${LLVM_CONFIG} --libdir
			OUTPUT_VARIABLE LLVM_LIBPATH
			OUTPUT_STRIP_TRAILING_WHITESPACE)
	find_library(LLVM_LIBRARY
		      NAMES LLVMAnalysis # first of a whole bunch of libs to get
		      PATHS ${LLVM_LIBPATH})

	if(LLVM_LIBRARY AND LLVM_ROOT_DIR AND LLVM_LIBPATH)
		if(LLVM_STATIC)
			# if static LLVM libraries were requested, use llvm-config to generate
			# the list of what libraries we need, and substitute that in the right
			# way for LLVM_LIBRARY.
			execute_process(COMMAND ${LLVM_CONFIG} --libfiles
					OUTPUT_VARIABLE LLVM_LIBRARY
					OUTPUT_STRIP_TRAILING_WHITESPACE)
			string(REPLACE " " ";" LLVM_LIBRARY ${LLVM_LIBRARY})
		else()
			set(PLATFORM_LINKFLAGS "${PLATFORM_LINKFLAGS} -lLLVM-3.4")
		endif()
	else()
		message(FATAL_ERROR "LLVM not found.")
	endif()
endif()

if(WITH_CYCLES_OSL)
	set(CYCLES_OSL ${LIBDIR}/osl CACHE PATH "Path to OpenShadingLanguage installation")

	find_library(OSL_LIB_EXEC NAMES oslexec PATHS ${CYCLES_OSL}/lib)
	find_library(OSL_LIB_COMP NAMES oslcomp PATHS ${CYCLES_OSL}/lib)
	find_library(OSL_LIB_QUERY NAMES oslquery PATHS ${CYCLES_OSL}/lib)
	# WARNING! depends on correct order of OSL libs linking
	list(APPEND OSL_LIBRARIES ${OSL_LIB_COMP} -force_load ${OSL_LIB_EXEC} ${OSL_LIB_QUERY})
	find_path(OSL_INCLUDE_DIR OSL/oslclosure.h PATHS ${CYCLES_OSL}/include)
	find_program(OSL_COMPILER NAMES oslc PATHS ${CYCLES_OSL}/bin)

	if(OSL_INCLUDE_DIR AND OSL_LIBRARIES AND OSL_COMPILER)
		set(OSL_FOUND TRUE)
	else()
		message(STATUS "OSL not found")
		set(WITH_CYCLES_OSL OFF)
	endif()
endif()

if(WITH_OPENMP)
	execute_process(COMMAND ${CMAKE_C_COMPILER} --version OUTPUT_VARIABLE COMPILER_VENDOR)
	string(SUBSTRING "${COMPILER_VENDOR}" 0 5 VENDOR_NAME) # truncate output
	if(${VENDOR_NAME} MATCHES "Apple") # Apple does not support OpenMP reliable with gcc and not with clang
		set(WITH_OPENMP OFF)
	else() # vanilla gcc or clang_omp support OpenMP
		message(STATUS "Using special OpenMP enabled compiler !") # letting find_package(OpenMP) module work for gcc
		if(CMAKE_C_COMPILER_ID MATCHES "Clang") # clang-omp in darwin libs
			set(OPENMP_FOUND ON)
			set(OpenMP_C_FLAGS "-fopenmp" CACHE STRING "C compiler flags for OpenMP parallization" FORCE)
			set(OpenMP_CXX_FLAGS "-fopenmp" CACHE STRING "C++ compiler flags for OpenMP parallization" FORCE)
			include_directories(${LIBDIR}/openmp/include)
			link_directories(${LIBDIR}/openmp/lib)
			# This is a workaround for our helperbinaries ( datatoc, masgfmt, ... ),
			# They are linked also to omp lib, so we need it in builddir for runtime exexcution,
			# TODO: remove all unneeded dependencies from these

			# for intermediate binaries, in respect to lib ID
			execute_process(
				COMMAND ditto -arch ${CMAKE_OSX_ARCHITECTURES}
				${LIBDIR}/openmp/lib/libiomp5.dylib
				${CMAKE_BINARY_DIR}/Resources/lib/libiomp5.dylib)
		endif()
	endif()
endif()

set(EXETYPE MACOSX_BUNDLE)

set(CMAKE_C_FLAGS_DEBUG "-fno-strict-aliasing -g")
set(CMAKE_CXX_FLAGS_DEBUG "-fno-strict-aliasing -g")
if(CMAKE_OSX_ARCHITECTURES MATCHES "x86_64" OR CMAKE_OSX_ARCHITECTURES MATCHES "i386")
	set(CMAKE_CXX_FLAGS_RELEASE "-O2 -mdynamic-no-pic -msse -msse2 -msse3 -mssse3")
	set(CMAKE_C_FLAGS_RELEASE "-O2 -mdynamic-no-pic  -msse -msse2 -msse3 -mssse3")
	if(NOT CMAKE_C_COMPILER_ID MATCHES "Clang")
		set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -ftree-vectorize  -fvariable-expansion-in-unroller")
		set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -ftree-vectorize  -fvariable-expansion-in-unroller")
	endif()
else()
	set(CMAKE_C_FLAGS_RELEASE "-mdynamic-no-pic -fno-strict-aliasing")
	set(CMAKE_CXX_FLAGS_RELEASE "-mdynamic-no-pic -fno-strict-aliasing")
endif()

if(${XCODE_VERSION} VERSION_EQUAL 5 OR ${XCODE_VERSION} VERSION_GREATER 5)
	# Xcode 5 is always using CLANG, which has too low template depth of 128 for libmv
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ftemplate-depth=1024")
endif()
# Get rid of eventually clashes, we export some symbols explicite as local
set(PLATFORM_LINKFLAGS
	"${PLATFORM_LINKFLAGS} -Xlinker -unexported_symbols_list -Xlinker ${CMAKE_SOURCE_DIR}/source/creator/osx_locals.map"
)

if(WITH_CXX11)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
	set(PLATFORM_LINKFLAGS "${PLATFORM_LINKFLAGS} -stdlib=libc++")
endif()

# Suppress ranlib "has no symbols" warnings (workaround for T48250)
set(CMAKE_C_ARCHIVE_CREATE   "<CMAKE_AR> Scr <TARGET> <LINK_FLAGS> <OBJECTS>")
set(CMAKE_CXX_ARCHIVE_CREATE "<CMAKE_AR> Scr <TARGET> <LINK_FLAGS> <OBJECTS>")
set(CMAKE_C_ARCHIVE_FINISH   "<CMAKE_RANLIB> -no_warning_for_no_symbols -c <TARGET>")
set(CMAKE_CXX_ARCHIVE_FINISH "<CMAKE_RANLIB> -no_warning_for_no_symbols -c <TARGET>")
