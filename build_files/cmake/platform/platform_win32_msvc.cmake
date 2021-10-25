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

# Libraries configuration for Windows when compiling with MSVC.

macro(warn_hardcoded_paths package_name
	)
	if(WITH_WINDOWS_FIND_MODULES)
		message(WARNING "Using HARDCODED ${package_name} locations")
	endif(WITH_WINDOWS_FIND_MODULES)
endmacro()

macro(windows_find_package package_name
	)
	if(WITH_WINDOWS_FIND_MODULES)
		find_package(${package_name})
	endif(WITH_WINDOWS_FIND_MODULES)
endmacro()

macro(find_package_wrapper)
	if(WITH_WINDOWS_FIND_MODULES)
		find_package(${ARGV})
	endif()
endmacro()

add_definitions(-DWIN32)
# Minimum MSVC Version
if(CMAKE_CXX_COMPILER_ID MATCHES MSVC)
	if(MSVC_VERSION EQUAL 1800)
		set(_min_ver "18.0.31101")
		if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS ${_min_ver})
			message(FATAL_ERROR
				"Visual Studio 2013 (Update 4, ${_min_ver}) required, "
				"found (${CMAKE_CXX_COMPILER_VERSION})")
		endif()
	endif()
	if(MSVC_VERSION EQUAL 1900)
		set(_min_ver "19.0.24210")
		if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS ${_min_ver})
			message(FATAL_ERROR
				"Visual Studio 2015 (Update 3, ${_min_ver}) required, "
				"found (${CMAKE_CXX_COMPILER_VERSION})")
		endif()
	endif()
endif()
unset(_min_ver)

# needed for some MSVC installations
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SAFESEH:NO")
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /SAFESEH:NO")
set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} /SAFESEH:NO")

list(APPEND PLATFORM_LINKLIBS
	ws2_32 vfw32 winmm kernel32 user32 gdi32 comdlg32
	advapi32 shfolder shell32 ole32 oleaut32 uuid psapi Dbghelp
)

if(WITH_INPUT_IME)
	list(APPEND PLATFORM_LINKLIBS imm32)
endif()

add_definitions(
	-D_CRT_NONSTDC_NO_DEPRECATE
	-D_CRT_SECURE_NO_DEPRECATE
	-D_SCL_SECURE_NO_DEPRECATE
	-D_CONSOLE
	-D_LIB
)

# MSVC11 needs _ALLOW_KEYWORD_MACROS to build
add_definitions(-D_ALLOW_KEYWORD_MACROS)

# We want to support Vista level ABI
add_definitions(-D_WIN32_WINNT=0x600)

# Make cmake find the msvc redistributables
set(CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS_SKIP TRUE)
include(InstallRequiredSystemLibraries)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /nologo /J /Gd /MP /EHsc")
set(CMAKE_C_FLAGS     "${CMAKE_C_FLAGS} /nologo /J /Gd /MP")

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MTd")
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} /MTd")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MT")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} /MT")
set(CMAKE_CXX_FLAGS_MINSIZEREL "${CMAKE_CXX_FLAGS_MINSIZEREL} /MT")
set(CMAKE_C_FLAGS_MINSIZEREL "${CMAKE_C_FLAGS_MINSIZEREL} /MT")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} /MT")
set(CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_RELWITHDEBINFO} /MT")

set(PLATFORM_LINKFLAGS "/SUBSYSTEM:CONSOLE /STACK:2097152 /INCREMENTAL:NO ")
set(PLATFORM_LINKFLAGS "${PLATFORM_LINKFLAGS} /NODEFAULTLIB:msvcrt.lib /NODEFAULTLIB:msvcmrt.lib /NODEFAULTLIB:msvcurt.lib /NODEFAULTLIB:msvcrtd.lib ")

# Ignore meaningless for us linker warnings.
set(PLATFORM_LINKFLAGS "${PLATFORM_LINKFLAGS} /ignore:4049 /ignore:4217 /ignore:4221")
set(CMAKE_STATIC_LINKER_FLAGS "${CMAKE_STATIC_LINKER_FLAGS} /ignore:4221")

if(CMAKE_CL_64)
	set(PLATFORM_LINKFLAGS "/MACHINE:X64 ${PLATFORM_LINKFLAGS}")
else()
	set(PLATFORM_LINKFLAGS "/MACHINE:IX86 /LARGEADDRESSAWARE ${PLATFORM_LINKFLAGS}")
endif()

set(PLATFORM_LINKFLAGS_DEBUG "/IGNORE:4099 /NODEFAULTLIB:libcmt.lib /NODEFAULTLIB:libc.lib")

if(NOT DEFINED LIBDIR)

	# Setup 64bit and 64bit windows systems
	if(CMAKE_CL_64)
		message(STATUS "64 bit compiler detected.")
		set(LIBDIR_BASE "win64")
	else()
		message(STATUS "32 bit compiler detected.")
		set(LIBDIR_BASE "windows")
	endif()
	if(MSVC_VERSION EQUAL 1910)
		message(STATUS "Visual Studio 2017 detected.")
		set(LIBDIR ${CMAKE_SOURCE_DIR}/../lib/${LIBDIR_BASE}_vc14)
	elseif(MSVC_VERSION EQUAL 1900)
		message(STATUS "Visual Studio 2015 detected.")
		set(LIBDIR ${CMAKE_SOURCE_DIR}/../lib/${LIBDIR_BASE}_vc14)
	else()
		message(STATUS "Visual Studio 2013 detected.")
		set(LIBDIR ${CMAKE_SOURCE_DIR}/../lib/${LIBDIR_BASE}_vc12)
	endif()
else()
	message(STATUS "Using pre-compiled LIBDIR: ${LIBDIR}")
endif()
if(NOT EXISTS "${LIBDIR}/")
	message(FATAL_ERROR "Windows requires pre-compiled libs at: '${LIBDIR}'")
endif()

# Add each of our libraries to our cmake_prefix_path so find_package() could work
file(GLOB children RELATIVE ${LIBDIR} ${LIBDIR}/*)
foreach(child ${children})
	if(IS_DIRECTORY ${LIBDIR}/${child})
		list(APPEND CMAKE_PREFIX_PATH  ${LIBDIR}/${child})
	endif()
endforeach()

set(ZLIB_INCLUDE_DIRS ${LIBDIR}/zlib/include)
set(ZLIB_LIBRARIES ${LIBDIR}/zlib/lib/libz_st.lib)
set(ZLIB_INCLUDE_DIR ${LIBDIR}/zlib/include)
set(ZLIB_LIBRARY ${LIBDIR}/zlib/lib/libz_st.lib)
set(ZLIB_DIR ${LIBDIR}/zlib)

windows_find_package(zlib) # we want to find before finding things that depend on it like png
windows_find_package(png)

if(NOT PNG_FOUND)
	warn_hardcoded_paths(libpng)
	set(PNG_PNG_INCLUDE_DIR ${LIBDIR}/png/include)
	set(PNG_LIBRARIES libpng)
	set(PNG "${LIBDIR}/png")
	set(PNG_INCLUDE_DIRS "${PNG}/include")
	set(PNG_LIBPATH ${PNG}/lib) # not cmake defined
endif()

set(JPEG_NAMES ${JPEG_NAMES} libjpeg)
windows_find_package(jpeg REQUIRED)
if(NOT JPEG_FOUND)
	warn_hardcoded_paths(jpeg)
	set(JPEG_INCLUDE_DIR ${LIBDIR}/jpeg/include)
	set(JPEG_LIBRARIES ${LIBDIR}/jpeg/lib/libjpeg.lib)
endif()

set(PTHREADS_INCLUDE_DIRS ${LIBDIR}/pthreads/include)
set(PTHREADS_LIBRARIES ${LIBDIR}/pthreads/lib/pthreadVC2.lib)

set(FREETYPE ${LIBDIR}/freetype)
set(FREETYPE_INCLUDE_DIRS
	${LIBDIR}/freetype/include
	${LIBDIR}/freetype/include/freetype2
)
set(FREETYPE_LIBRARY ${LIBDIR}/freetype/lib/freetype2ST.lib)
windows_find_package(freetype REQUIRED)

if(WITH_FFTW3)
	set(FFTW3 ${LIBDIR}/fftw3)
	set(FFTW3_LIBRARIES libfftw)
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

	set(OPENCOLLADA_LIBRARIES
		${OPENCOLLADA}/lib/opencollada/OpenCOLLADASaxFrameworkLoader.lib
		${OPENCOLLADA}/lib/opencollada/OpenCOLLADAFramework.lib
		${OPENCOLLADA}/lib/opencollada/OpenCOLLADABaseUtils.lib
		${OPENCOLLADA}/lib/opencollada/OpenCOLLADAStreamWriter.lib
		${OPENCOLLADA}/lib/opencollada/MathMLSolver.lib
		${OPENCOLLADA}/lib/opencollada/GeneratedSaxParser.lib
		${OPENCOLLADA}/lib/opencollada/xml.lib
		${OPENCOLLADA}/lib/opencollada/buffer.lib
		${OPENCOLLADA}/lib/opencollada/ftoa.lib
	)

	if(NOT WITH_LLVM)
		list(APPEND OPENCOLLADA_LIBRARIES ${OPENCOLLADA}/lib/opencollada/UTF.lib)
	endif()

	set(PCRE_LIBRARIES
		${OPENCOLLADA}/lib/opencollada/pcre.lib
	)
endif()

if(WITH_CODEC_FFMPEG)
	set(FFMPEG_INCLUDE_DIRS
		${LIBDIR}/ffmpeg/include
		${LIBDIR}/ffmpeg/include/msvc
	)
	windows_find_package(FFMPEG)
	if(NOT FFMPEG_FOUND)
		warn_hardcoded_paths(ffmpeg)
		set(FFMPEG_LIBRARY_VERSION 57)
		set(FFMPEG_LIBRARY_VERSION_AVU 55)
		set(FFMPEG_LIBRARIES
			${LIBDIR}/ffmpeg/lib/avcodec.lib
			${LIBDIR}/ffmpeg/lib/avformat.lib
			${LIBDIR}/ffmpeg/lib/avdevice.lib
			${LIBDIR}/ffmpeg/lib/avutil.lib
			${LIBDIR}/ffmpeg/lib/swscale.lib
			)
	endif()
endif()

if(WITH_IMAGE_OPENEXR)
	set(OPENEXR_ROOT_DIR ${LIBDIR}/openexr)
	set(OPENEXR_VERSION "2.1")
	windows_find_package(OPENEXR REQUIRED)
	if(NOT OPENEXR_FOUND)
		warn_hardcoded_paths(OpenEXR)
		set(OPENEXR ${LIBDIR}/openexr)
		set(OPENEXR_INCLUDE_DIR ${OPENEXR}/include)
		set(OPENEXR_INCLUDE_DIRS ${OPENEXR_INCLUDE_DIR} ${OPENEXR}/include/OpenEXR)
		set(OPENEXR_LIBPATH ${OPENEXR}/lib)
		set(OPENEXR_LIBRARIES
			optimized ${OPENEXR_LIBPATH}/Iex-2_2.lib
			optimized ${OPENEXR_LIBPATH}/Half.lib
			optimized ${OPENEXR_LIBPATH}/IlmImf-2_2.lib
			optimized ${OPENEXR_LIBPATH}/Imath-2_2.lib
			optimized ${OPENEXR_LIBPATH}/IlmThread-2_2.lib
			debug ${OPENEXR_LIBPATH}/Iex-2_2_d.lib
			debug ${OPENEXR_LIBPATH}/Half_d.lib
			debug ${OPENEXR_LIBPATH}/IlmImf-2_2_d.lib
			debug ${OPENEXR_LIBPATH}/Imath-2_2_d.lib
			debug ${OPENEXR_LIBPATH}/IlmThread-2_2_d.lib
		)
	endif()
endif()

if(WITH_IMAGE_TIFF)
	# Try to find tiff first then complain and set static and maybe wrong paths
	windows_find_package(TIFF)
	if(NOT TIFF_FOUND)
		warn_hardcoded_paths(libtiff)
		set(TIFF_LIBRARY ${LIBDIR}/tiff/lib/libtiff.lib)
		set(TIFF_INCLUDE_DIR ${LIBDIR}/tiff/include)
	endif()
endif()

if(WITH_JACK)
	set(JACK_INCLUDE_DIRS
		${LIBDIR}/jack/include/jack
		${LIBDIR}/jack/include
	)
	set(JACK_LIBRARIES optimized ${LIBDIR}/jack/lib/libjack.lib debug ${LIBDIR}/jack/lib/libjack_d.lib)
endif()

if(WITH_PYTHON)
	set(PYTHON_VERSION 3.5) # CACHE STRING)

	string(REPLACE "." "" _PYTHON_VERSION_NO_DOTS ${PYTHON_VERSION})
	# Use shared libs for vc2008 and vc2010 until we actually have vc2010 libs
	set(PYTHON_LIBRARY ${LIBDIR}/python/lib/python${_PYTHON_VERSION_NO_DOTS}.lib)
	unset(_PYTHON_VERSION_NO_DOTS)

	# Shared includes for both vc2008 and vc2010
	set(PYTHON_INCLUDE_DIR ${LIBDIR}/python/include/python${PYTHON_VERSION})

	# uncached vars
	set(PYTHON_INCLUDE_DIRS "${PYTHON_INCLUDE_DIR}")
	set(PYTHON_LIBRARIES  "${PYTHON_LIBRARY}")
endif()

if(WITH_BOOST)
	if(WITH_CYCLES_OSL)
		set(boost_extra_libs wave)
	endif()
	if(WITH_INTERNATIONAL)
		list(APPEND boost_extra_libs locale)
	endif()
	if(WITH_OPENVDB)
		list(APPEND boost_extra_libs iostreams)
	endif()
	set(Boost_USE_STATIC_RUNTIME ON) # prefix lib
	set(Boost_USE_MULTITHREADED ON) # suffix -mt
	set(Boost_USE_STATIC_LIBS ON) # suffix -s
	if (WITH_WINDOWS_FIND_MODULES)
		find_package(Boost COMPONENTS date_time filesystem thread regex system ${boost_extra_libs})
	endif (WITH_WINDOWS_FIND_MODULES)
	if(NOT Boost_FOUND)
		warn_hardcoded_paths(BOOST)
		set(BOOST ${LIBDIR}/boost)
		set(BOOST_INCLUDE_DIR ${BOOST}/include)
		if(MSVC12)
			set(BOOST_LIBPATH ${BOOST}/lib)
			set(BOOST_POSTFIX "vc120-mt-s-1_60.lib")
			set(BOOST_DEBUG_POSTFIX "vc120-mt-sgd-1_60.lib")
		else()
			set(BOOST_LIBPATH ${BOOST}/lib)
			set(BOOST_POSTFIX "vc140-mt-s-1_60.lib")
			set(BOOST_DEBUG_POSTFIX "vc140-mt-sgd-1_60.lib")
		endif()
		set(BOOST_LIBRARIES
			optimized libboost_date_time-${BOOST_POSTFIX}
			optimized libboost_filesystem-${BOOST_POSTFIX}
			optimized libboost_regex-${BOOST_POSTFIX}
			optimized libboost_system-${BOOST_POSTFIX}
			optimized libboost_thread-${BOOST_POSTFIX}
			debug libboost_date_time-${BOOST_DEBUG_POSTFIX}
			debug libboost_filesystem-${BOOST_DEBUG_POSTFIX}
			debug libboost_regex-${BOOST_DEBUG_POSTFIX}
			debug libboost_system-${BOOST_DEBUG_POSTFIX}
			debug libboost_thread-${BOOST_DEBUG_POSTFIX}
		)
		if(WITH_CYCLES_OSL)
			set(BOOST_LIBRARIES ${BOOST_LIBRARIES}
				optimized libboost_wave-${BOOST_POSTFIX}
				debug libboost_wave-${BOOST_DEBUG_POSTFIX})
		endif()
		if(WITH_INTERNATIONAL)
			set(BOOST_LIBRARIES ${BOOST_LIBRARIES}
				optimized libboost_locale-${BOOST_POSTFIX}
				debug libboost_locale-${BOOST_DEBUG_POSTFIX})
		endif()
	else() # we found boost using find_package
		set(BOOST_INCLUDE_DIR ${Boost_INCLUDE_DIRS})
		set(BOOST_LIBRARIES ${Boost_LIBRARIES})
		set(BOOST_LIBPATH ${Boost_LIBRARY_DIRS})
	endif()
	set(BOOST_DEFINITIONS "-DBOOST_ALL_NO_LIB")
endif()

if(WITH_OPENIMAGEIO)
	windows_find_package(OpenImageIO)
	set(OPENIMAGEIO ${LIBDIR}/openimageio)
	set(OPENIMAGEIO_INCLUDE_DIRS ${OPENIMAGEIO}/include)
	set(OIIO_OPTIMIZED optimized OpenImageIO optimized OpenImageIO_Util)
	set(OIIO_DEBUG debug OpenImageIO_d debug OpenImageIO_Util_d)
	set(OPENIMAGEIO_LIBRARIES ${OIIO_OPTIMIZED} ${OIIO_DEBUG})
	set(OPENIMAGEIO_LIBPATH ${OPENIMAGEIO}/lib)
	set(OPENIMAGEIO_DEFINITIONS "-DUSE_TBB=0")
	set(OPENCOLORIO_DEFINITIONS "-DOCIO_STATIC_BUILD")
	set(OPENIMAGEIO_IDIFF "${OPENIMAGEIO}/bin/idiff.exe")
	add_definitions(-DOIIO_STATIC_BUILD)
	add_definitions(-DOIIO_NO_SSE=1)
endif()

if(WITH_LLVM)
	set(LLVM_ROOT_DIR ${LIBDIR}/llvm CACHE PATH	"Path to the LLVM installation")
	file(GLOB LLVM_LIBRARY_OPTIMIZED ${LLVM_ROOT_DIR}/lib/*.lib)

	if(EXISTS ${LLVM_ROOT_DIR}/debug/lib)
		foreach(LLVM_OPTIMIZED_LIB ${LLVM_LIBRARY_OPTIMIZED})
			get_filename_component(LIBNAME ${LLVM_OPTIMIZED_LIB} ABSOLUTE)
			list(APPEND LLVM_LIBS optimized ${LIBNAME})
		endforeach(LLVM_OPTIMIZED_LIB)

		file(GLOB LLVM_LIBRARY_DEBUG ${LLVM_ROOT_DIR}/debug/lib/*.lib)

		foreach(LLVM_DEBUG_LIB ${LLVM_LIBRARY_DEBUG})
			get_filename_component(LIBNAME ${LLVM_DEBUG_LIB} ABSOLUTE)
			list(APPEND LLVM_LIBS debug ${LIBNAME})
		endforeach(LLVM_DEBUG_LIB)

		set(LLVM_LIBRARY ${LLVM_LIBS})
	else()
		message(WARNING "LLVM debug libs not present on this system. Using release libs for debug builds.")
		set(LLVM_LIBRARY ${LLVM_LIBRARY_OPTIMIZED})
	endif()

endif()

if(WITH_OPENCOLORIO)
	set(OPENCOLORIO ${LIBDIR}/opencolorio)
	set(OPENCOLORIO_INCLUDE_DIRS ${OPENCOLORIO}/include)
	set(OPENCOLORIO_LIBRARIES OpenColorIO)
	set(OPENCOLORIO_LIBPATH ${LIBDIR}/opencolorio/lib)
	set(OPENCOLORIO_DEFINITIONS)
endif()

if(WITH_OPENVDB)
	set(BLOSC_LIBRARIES optimized ${LIBDIR}/blosc/lib/libblosc.lib debug ${LIBDIR}/blosc/lib/libblosc_d.lib)
	set(TBB_LIBRARIES optimized ${LIBDIR}/tbb/lib/tbb.lib debug ${LIBDIR}/tbb/lib/tbb_debug.lib)
	set(TBB_INCLUDE_DIR ${LIBDIR}/tbb/include)
	set(OPENVDB ${LIBDIR}/openvdb)
	set(OPENVDB_INCLUDE_DIRS ${OPENVDB}/include ${TBB_INCLUDE_DIR})
	set(OPENVDB_LIBRARIES optimized openvdb debug openvdb_d ${TBB_LIBRARIES} ${BLOSC_LIBRARIES})
	set(OPENVDB_LIBPATH ${LIBDIR}/openvdb/lib)
endif()

if(WITH_ALEMBIC)
	set(ALEMBIC ${LIBDIR}/alembic)
	set(ALEMBIC_INCLUDE_DIR ${ALEMBIC}/include)
	set(ALEMBIC_INCLUDE_DIRS ${ALEMBIC_INCLUDE_DIR})
	set(ALEMBIC_LIBPATH ${ALEMBIC}/lib)
	set(ALEMBIC_LIBRARIES optimized alembic debug alembic_d)
	set(ALEMBIC_FOUND 1)
endif()

if(WITH_MOD_CLOTH_ELTOPO)
	set(LAPACK ${LIBDIR}/lapack)
	# set(LAPACK_INCLUDE_DIR ${LAPACK}/include)
	set(LAPACK_LIBPATH ${LAPACK}/lib)
	set(LAPACK_LIBRARIES
		${LIBDIR}/lapack/lib/libf2c.lib
		${LIBDIR}/lapack/lib/clapack_nowrap.lib
		${LIBDIR}/lapack/lib/BLAS_nowrap.lib
	)
endif()

if(WITH_OPENSUBDIV OR WITH_CYCLES_OPENSUBDIV)
	set(OPENSUBDIV_INCLUDE_DIR ${LIBDIR}/opensubdiv/include)
	set(OPENSUBDIV_LIBPATH ${LIBDIR}/opensubdiv/lib)
	set(OPENSUBDIV_LIBRARIES
		optimized ${OPENSUBDIV_LIBPATH}/osdCPU.lib
		optimized ${OPENSUBDIV_LIBPATH}/osdGPU.lib
		debug ${OPENSUBDIV_LIBPATH}/osdCPU_d.lib
		debug ${OPENSUBDIV_LIBPATH}/osdGPU_d.lib
	)
	set(OPENSUBDIV_HAS_OPENMP TRUE)
	set(OPENSUBDIV_HAS_TBB FALSE)
	set(OPENSUBDIV_HAS_OPENCL TRUE)
	set(OPENSUBDIV_HAS_CUDA FALSE)
	set(OPENSUBDIV_HAS_GLSL_TRANSFORM_FEEDBACK TRUE)
	set(OPENSUBDIV_HAS_GLSL_COMPUTE TRUE)
	windows_find_package(OpenSubdiv)
endif()

if(WITH_SDL)
	set(SDL ${LIBDIR}/sdl)
	set(SDL_INCLUDE_DIR ${SDL}/include)
	set(SDL_LIBPATH ${SDL}/lib)
	set(SDL_LIBRARY SDL2)
endif()

# Audio IO
if(WITH_SYSTEM_AUDASPACE)
	set(AUDASPACE_INCLUDE_DIRS ${LIBDIR}/audaspace/include/audaspace)
	set(AUDASPACE_LIBRARIES ${LIBDIR}/audaspace/lib/audaspace.lib)
	set(AUDASPACE_C_INCLUDE_DIRS ${LIBDIR}/audaspace/include/audaspace)
	set(AUDASPACE_C_LIBRARIES ${LIBDIR}/audaspace/lib/audaspace-c.lib)
	set(AUDASPACE_PY_INCLUDE_DIRS ${LIBDIR}/audaspace/include/audaspace)
	set(AUDASPACE_PY_LIBRARIES ${LIBDIR}/audaspace/lib/audaspace-py.lib)
endif()

# used in many places so include globally, like OpenGL
blender_include_dirs_sys("${PTHREADS_INCLUDE_DIRS}")

#find signtool
set(ProgramFilesX86_NAME "ProgramFiles(x86)") #env dislikes the ( )
find_program(SIGNTOOL_EXE signtool
	HINTS
		"$ENV{${ProgramFilesX86_NAME}}/Windows Kits/10/bin/x86/"
		"$ENV{ProgramFiles}/Windows Kits/10/bin/x86/"
		"$ENV{${ProgramFilesX86_NAME}}/Windows Kits/8.1/bin/x86/"
		"$ENV{ProgramFiles}/Windows Kits/8.1/bin/x86/"
		"$ENV{${ProgramFilesX86_NAME}}/Windows Kits/8.0/bin/x86/"
		"$ENV{ProgramFiles}/Windows Kits/8.0/bin/x86/"
)
