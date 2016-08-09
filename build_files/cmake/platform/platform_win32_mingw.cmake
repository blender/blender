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

# Libraries configuration for Windows when compiling with MinGW.

# keep GCC specific stuff here
include(CheckCSourceCompiles)
# Setup 64bit and 64bit windows systems
CHECK_C_SOURCE_COMPILES("
	#ifndef __MINGW64__
	#error
	#endif
	int main(void) { return 0; }
	"
	WITH_MINGW64
)

if(NOT DEFINED LIBDIR)
	if(WITH_MINGW64)
		message(STATUS "Compiling for 64 bit with MinGW-w64.")
		set(LIBDIR ${CMAKE_SOURCE_DIR}/../lib/mingw64)
	else()
		message(STATUS "Compiling for 32 bit with MinGW-w32.")
		set(LIBDIR ${CMAKE_SOURCE_DIR}/../lib/mingw32)

		if(WITH_RAYOPTIMIZATION)
			message(WARNING "MinGW-w32 is known to be unstable with 'WITH_RAYOPTIMIZATION' option enabled.")
		endif()
	endif()
else()
	message(STATUS "Using pre-compiled LIBDIR: ${LIBDIR}")
endif()
if(NOT EXISTS "${LIBDIR}/")
	message(FATAL_ERROR "Windows requires pre-compiled libs at: '${LIBDIR}'")
endif()

list(APPEND PLATFORM_LINKLIBS
	-lshell32 -lshfolder -lgdi32 -lmsvcrt -lwinmm -lmingw32 -lm -lws2_32
	-lz -lstdc++ -lole32 -luuid -lwsock32 -lpsapi -ldbghelp
)

if(WITH_INPUT_IME)
	list(APPEND PLATFORM_LINKLIBS -limm32)
endif()

set(PLATFORM_CFLAGS "-pipe -funsigned-char -fno-strict-aliasing")

if(WITH_MINGW64)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fpermissive")
	list(APPEND PLATFORM_LINKLIBS -lpthread)

	add_definitions(-DFREE_WINDOWS64 -DMS_WIN64)
endif()

add_definitions(-D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64 -D_LARGEFILE64_SOURCE)

add_definitions(-DFREE_WINDOWS)

set(PNG "${LIBDIR}/png")
set(PNG_INCLUDE_DIRS "${PNG}/include")
set(PNG_LIBPATH ${PNG}/lib) # not cmake defined

if(WITH_MINGW64)
	set(JPEG_LIBRARIES jpeg)
else()
	set(JPEG_LIBRARIES libjpeg)
endif()
set(PNG_LIBRARIES png)

set(ZLIB ${LIBDIR}/zlib)
set(ZLIB_INCLUDE_DIRS ${ZLIB}/include)
set(ZLIB_LIBPATH ${ZLIB}/lib)
set(ZLIB_LIBRARIES z)

set(JPEG "${LIBDIR}/jpeg")
set(JPEG_INCLUDE_DIR "${JPEG}/include")
set(JPEG_LIBPATH ${JPEG}/lib) # not cmake defined

# comes with own pthread library
if(NOT WITH_MINGW64)
	set(PTHREADS ${LIBDIR}/pthreads)
	#set(PTHREADS_INCLUDE_DIRS ${PTHREADS}/include)
	set(PTHREADS_LIBPATH ${PTHREADS}/lib)
	set(PTHREADS_LIBRARIES pthreadGC2)
endif()

set(FREETYPE ${LIBDIR}/freetype)
set(FREETYPE_INCLUDE_DIRS ${FREETYPE}/include ${FREETYPE}/include/freetype2)
set(FREETYPE_LIBPATH ${FREETYPE}/lib)
set(FREETYPE_LIBRARY freetype)

if(WITH_FFTW3)
	set(FFTW3 ${LIBDIR}/fftw3)
	set(FFTW3_LIBRARIES fftw3)
	set(FFTW3_INCLUDE_DIRS ${FFTW3}/include)
	set(FFTW3_LIBPATH ${FFTW3}/lib)
endif()

if(WITH_OPENCOLLADA)
	set(OPENCOLLADA ${LIBDIR}/opencollada)
	set(OPENCOLLADA_INCLUDE_DIRS
		${OPENCOLLADA}/include/opencollada/COLLADAStreamWriter
		${OPENCOLLADA}/include/opencollada/COLLADABaseUtils
		${OPENCOLLADA}/include/opencollada/COLLADAFramework
		${OPENCOLLADA}/include/opencollada/COLLADASaxFrameworkLoader
		${OPENCOLLADA}/include/opencollada/GeneratedSaxParser
	)
	set(OPENCOLLADA_LIBPATH ${OPENCOLLADA}/lib/opencollada)
	set(OPENCOLLADA_LIBRARIES
		OpenCOLLADAStreamWriter
		OpenCOLLADASaxFrameworkLoader
		OpenCOLLADAFramework
		OpenCOLLADABaseUtils
		GeneratedSaxParser
		UTF MathMLSolver buffer ftoa xml
	)
	set(PCRE_LIBRARIES pcre)
endif()

if(WITH_CODEC_FFMPEG)
	set(FFMPEG ${LIBDIR}/ffmpeg)
	set(FFMPEG_INCLUDE_DIRS ${FFMPEG}/include)
	if(WITH_MINGW64)
		set(FFMPEG_LIBRARIES avcodec.dll avformat.dll avdevice.dll avutil.dll swscale.dll swresample.dll)
	else()
		set(FFMPEG_LIBRARIES avcodec-55 avformat-55 avdevice-55 avutil-52 swscale-2)
	endif()
	set(FFMPEG_LIBPATH ${FFMPEG}/lib)
endif()

if(WITH_IMAGE_OPENEXR)
	set(OPENEXR ${LIBDIR}/openexr)
	set(OPENEXR_INCLUDE_DIR ${OPENEXR}/include)
	set(OPENEXR_INCLUDE_DIRS ${OPENEXR}/include/OpenEXR)
	set(OPENEXR_LIBRARIES Half IlmImf Imath IlmThread Iex)
	set(OPENEXR_LIBPATH ${OPENEXR}/lib)
endif()

if(WITH_IMAGE_TIFF)
	set(TIFF ${LIBDIR}/tiff)
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
	set(PYTHON_VERSION 3.5) #  CACHE STRING)
	string(REPLACE "." "" _PYTHON_VERSION_NO_DOTS ${PYTHON_VERSION})
	set(PYTHON_INCLUDE_DIR "${LIBDIR}/python/include/python${PYTHON_VERSION}")  # CACHE PATH)
	set(PYTHON_LIBRARY "${LIBDIR}/python/lib/python${_PYTHON_VERSION_NO_DOTS}mw.lib")  # CACHE FILEPATH)
	unset(_PYTHON_VERSION_NO_DOTS)

	# uncached vars
	set(PYTHON_INCLUDE_DIRS "${PYTHON_INCLUDE_DIR}")
	set(PYTHON_LIBRARIES  "${PYTHON_LIBRARY}")
endif()

if(WITH_BOOST)
	set(BOOST ${LIBDIR}/boost)
	set(BOOST_INCLUDE_DIR ${BOOST}/include)
	if(WITH_MINGW64)
		set(BOOST_POSTFIX "mgw47-mt-s-1_49")
		set(BOOST_DEBUG_POSTFIX "mgw47-mt-sd-1_49")
	else()
		set(BOOST_POSTFIX "mgw46-mt-s-1_49")
		set(BOOST_DEBUG_POSTFIX "mgw46-mt-sd-1_49")
	endif()
	set(BOOST_LIBRARIES
		optimized boost_date_time-${BOOST_POSTFIX} boost_filesystem-${BOOST_POSTFIX}
		boost_regex-${BOOST_POSTFIX}
		boost_system-${BOOST_POSTFIX} boost_thread-${BOOST_POSTFIX}
		debug boost_date_time-${BOOST_DEBUG_POSTFIX} boost_filesystem-${BOOST_DEBUG_POSTFIX}
		boost_regex-${BOOST_DEBUG_POSTFIX}
		boost_system-${BOOST_DEBUG_POSTFIX} boost_thread-${BOOST_DEBUG_POSTFIX})
	if(WITH_INTERNATIONAL)
		set(BOOST_LIBRARIES ${BOOST_LIBRARIES}
			optimized boost_locale-${BOOST_POSTFIX}
			debug boost_locale-${BOOST_DEBUG_POSTFIX}
		)
	endif()
	if(WITH_CYCLES_OSL)
		set(BOOST_LIBRARIES ${BOOST_LIBRARIES}
			optimized boost_wave-${BOOST_POSTFIX}
			debug boost_wave-${BOOST_DEBUG_POSTFIX}
		)
	endif()
	set(BOOST_LIBPATH ${BOOST}/lib)
	set(BOOST_DEFINITIONS "-DBOOST_ALL_NO_LIB -DBOOST_THREAD_USE_LIB ")
endif()

if(WITH_OPENIMAGEIO)
	set(OPENIMAGEIO ${LIBDIR}/openimageio)
	set(OPENIMAGEIO_INCLUDE_DIRS ${OPENIMAGEIO}/include)
	set(OPENIMAGEIO_LIBRARIES OpenImageIO)
	set(OPENIMAGEIO_LIBPATH ${OPENIMAGEIO}/lib)
	set(OPENIMAGEIO_DEFINITIONS "")
	set(OPENIMAGEIO_IDIFF "${OPENIMAGEIO}/bin/idiff.exe")
endif()

if(WITH_LLVM)
	set(LLVM_ROOT_DIR ${LIBDIR}/llvm CACHE PATH	"Path to the LLVM installation")
	set(LLVM_LIBPATH ${LLVM_ROOT_DIR}/lib)
	# Explicitly set llvm lib order.
	#---- WARNING ON GCC ORDER OF LIBS IS IMPORTANT, DO NOT CHANGE! ---------
	set(LLVM_LIBRARY LLVMSelectionDAG LLVMCodeGen LLVMScalarOpts LLVMAnalysis LLVMArchive
		LLVMAsmParser LLVMAsmPrinter
		LLVMBitReader LLVMBitWriter
		LLVMDebugInfo LLVMExecutionEngine
		LLVMInstCombine LLVMInstrumentation
		LLVMInterpreter LLVMJIT
		LLVMLinker LLVMMC
		LLVMMCDisassembler LLVMMCJIT
		LLVMMCParser LLVMObject
		LLVMRuntimeDyld
		LLVMSupport
		LLVMTableGen LLVMTarget
		LLVMTransformUtils LLVMVectorize
		LLVMX86AsmParser LLVMX86AsmPrinter
		LLVMX86CodeGen LLVMX86Desc
		LLVMX86Disassembler LLVMX86Info
		LLVMX86Utils LLVMipa
		LLVMipo LLVMCore)
	# imagehelp is needed by LLVM 3.1 on MinGW, check lib\Support\Windows\Signals.inc
	list(APPEND PLATFORM_LINKLIBS -limagehlp)
endif()

if(WITH_OPENCOLORIO)
	set(OPENCOLORIO ${LIBDIR}/opencolorio)
	set(OPENCOLORIO_INCLUDE_DIRS ${OPENCOLORIO}/include)
	set(OPENCOLORIO_LIBRARIES OpenColorIO)
	set(OPENCOLORIO_LIBPATH ${OPENCOLORIO}/lib)
	set(OPENCOLORIO_DEFINITIONS)
endif()

if(WITH_SDL)
	set(SDL ${LIBDIR}/sdl)
	set(SDL_INCLUDE_DIR ${SDL}/include)
	set(SDL_LIBRARY SDL)
	set(SDL_LIBPATH ${SDL}/lib)
endif()

if(WITH_OPENVDB)
	set(OPENVDB ${LIBDIR}/openvdb)
	set(OPENVDB_INCLUDE_DIRS ${OPENVDB}/include)
	set(OPENVDB_LIBRARIES openvdb ${TBB_LIBRARIES})
	set(OPENVDB_LIBPATH ${LIBDIR}/openvdb/lib)
	set(OPENVDB_DEFINITIONS)
endif()

if(WITH_ALEMBIC)
	# TODO(sergey): For until someone drops by and compiles libraries for
	# MinGW we allow users to compile their own Alembic library and use
	# that via find_package(),
	#
	# Once precompiled libraries are there we'll use hardcoded locations.
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

set(PLATFORM_LINKFLAGS "-Xlinker --stack=2097152")

## DISABLE - causes linking errors
## for re-distribution, so users dont need mingw installed
# set(PLATFORM_LINKFLAGS "${PLATFORM_LINKFLAGS} -static-libgcc -static-libstdc++")
