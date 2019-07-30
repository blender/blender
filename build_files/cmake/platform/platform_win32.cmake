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
# ***** END GPL LICENSE BLOCK *****

# Libraries configuration for Windows.

add_definitions(-DWIN32)

if(NOT MSVC)
  message(FATAL_ERROR "Compiler is unsupported")
endif()

if(CMAKE_C_COMPILER_ID MATCHES "Clang")
  set(MSVC_CLANG On)
  set(VC_TOOLS_DIR $ENV{VCToolsRedistDir} CACHE STRING "Location of the msvc redistributables")
  set(MSVC_REDIST_DIR ${VC_TOOLS_DIR})
  if(DEFINED MSVC_REDIST_DIR)
    file(TO_CMAKE_PATH ${MSVC_REDIST_DIR} MSVC_REDIST_DIR)
  else()
    message("Unable to detect the Visual Studio redist directory, copying of the runtime dlls will not work, try running from the visual studio developer prompt.")
  endif()
endif()

set_property(GLOBAL PROPERTY USE_FOLDERS ${WINDOWS_USE_VISUAL_STUDIO_PROJECT_FOLDERS})

if(NOT WITH_PYTHON_MODULE)
  set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT blender)
endif()

macro(warn_hardcoded_paths package_name
  )
  if(WITH_WINDOWS_FIND_MODULES)
    message(WARNING "Using HARDCODED ${package_name} locations")
  endif()
endmacro()

macro(windows_find_package package_name
  )
  if(WITH_WINDOWS_FIND_MODULES)
    find_package(${package_name})
  endif()
endmacro()

macro(find_package_wrapper)
  if(WITH_WINDOWS_FIND_MODULES)
    find_package(${ARGV})
  endif()
endmacro()

add_definitions(-DWIN32)

# Needed, otherwise system encoding causes utf-8 encoding to fail in some cases (C4819)
add_compile_options("$<$<C_COMPILER_ID:MSVC>:/utf-8>")
add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")

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
set(CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS_SKIP FALSE)
set(CMAKE_INSTALL_UCRT_LIBRARIES TRUE)
set(CMAKE_INSTALL_OPENMP_LIBRARIES ${WITH_OPENMP})
set(CMAKE_INSTALL_SYSTEM_RUNTIME_DESTINATION .)
include(InstallRequiredSystemLibraries)

remove_cc_flag("/MDd" "/MD")

if(MSVC_CLANG) # Clangs version of cl doesn't support all flags
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${CXX_WARN_FLAGS} /nologo /J /Gd /EHsc -Wno-unused-command-line-argument -Wno-microsoft-enum-forward-reference ")
  set(CMAKE_C_FLAGS     "${CMAKE_C_FLAGS} /nologo /J /Gd -Wno-unused-command-line-argument -Wno-microsoft-enum-forward-reference")
else()
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /nologo /J /Gd /MP /EHsc")
  set(CMAKE_C_FLAGS     "${CMAKE_C_FLAGS} /nologo /J /Gd /MP")
endif()

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MTd")
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} /MTd")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MT")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} /MT")
set(CMAKE_CXX_FLAGS_MINSIZEREL "${CMAKE_CXX_FLAGS_MINSIZEREL} /MT")
set(CMAKE_C_FLAGS_MINSIZEREL "${CMAKE_C_FLAGS_MINSIZEREL} /MT")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} /MT")
set(CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_RELWITHDEBINFO} /MT")

#JMC is available on msvc 15.8 (1915) and up
if(MSVC_VERSION GREATER 1914 AND NOT MSVC_CLANG)
  set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} /JMC")
endif()

set(PLATFORM_LINKFLAGS "${PLATFORM_LINKFLAGS} /SUBSYSTEM:CONSOLE /STACK:2097152 /INCREMENTAL:NO ")
set(PLATFORM_LINKFLAGS "${PLATFORM_LINKFLAGS} /NODEFAULTLIB:msvcrt.lib /NODEFAULTLIB:msvcmrt.lib /NODEFAULTLIB:msvcurt.lib /NODEFAULTLIB:msvcrtd.lib ")

# Ignore meaningless for us linker warnings.
set(PLATFORM_LINKFLAGS "${PLATFORM_LINKFLAGS} /ignore:4049 /ignore:4217 /ignore:4221")
set(CMAKE_STATIC_LINKER_FLAGS "${CMAKE_STATIC_LINKER_FLAGS} /ignore:4221")

if(CMAKE_CL_64)
  set(PLATFORM_LINKFLAGS "/MACHINE:X64 ${PLATFORM_LINKFLAGS}")
else()
  set(PLATFORM_LINKFLAGS "/MACHINE:IX86 /LARGEADDRESSAWARE ${PLATFORM_LINKFLAGS}")
endif()

set(PLATFORM_LINKFLAGS_DEBUG "${PLATFORM_LINKFLAGS_DEBUG} /IGNORE:4099 /NODEFAULTLIB:libcmt.lib /NODEFAULTLIB:libc.lib")

if(NOT DEFINED LIBDIR)

  # Setup 64bit and 64bit windows systems
  if(CMAKE_CL_64)
    message(STATUS "64 bit compiler detected.")
    set(LIBDIR_BASE "win64")
  else()
    message(STATUS "32 bit compiler detected.")
    set(LIBDIR_BASE "windows")
  endif()
  # Can be 1910..1912
  if(MSVC_VERSION GREATER 1919)
    message(STATUS "Visual Studio 2019 detected.")
    set(LIBDIR ${CMAKE_SOURCE_DIR}/../lib/${LIBDIR_BASE}_vc14)
  elseif(MSVC_VERSION GREATER 1909)
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

# Mark libdir as system headers with a lower warn level, to resolve some warnings
# that we have very little control over
if(MSVC_VERSION GREATER_EQUAL 1914 AND NOT MSVC_CLANG)
  add_compile_options(/experimental:external /external:templates- /external:I "${LIBDIR}" /external:W0)
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
  set(PNG_LIBRARIES ${LIBDIR}/png/lib/libpng.lib)
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
set(PTHREADS_LIBRARIES ${LIBDIR}/pthreads/lib/pthreadVC3.lib)

set(FREETYPE ${LIBDIR}/freetype)
set(FREETYPE_INCLUDE_DIRS
  ${LIBDIR}/freetype/include
  ${LIBDIR}/freetype/include/freetype2
)
set(FREETYPE_LIBRARY ${LIBDIR}/freetype/lib/freetype2ST.lib)
windows_find_package(freetype REQUIRED)

if(WITH_FFTW3)
  set(FFTW3 ${LIBDIR}/fftw3)
  set(FFTW3_LIBRARIES ${FFTW3}/lib/libfftw.lib)
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

  list(APPEND OPENCOLLADA_LIBRARIES ${OPENCOLLADA}/lib/opencollada/UTF.lib)

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
      optimized ${OPENEXR_LIBPATH}/Iex_s.lib
      optimized ${OPENEXR_LIBPATH}/Half_s.lib
      optimized ${OPENEXR_LIBPATH}/IlmImf_s.lib
      optimized ${OPENEXR_LIBPATH}/Imath_s.lib
      optimized ${OPENEXR_LIBPATH}/IlmThread_s.lib
      debug ${OPENEXR_LIBPATH}/Iex_s_d.lib
      debug ${OPENEXR_LIBPATH}/Half_s_d.lib
      debug ${OPENEXR_LIBPATH}/IlmImf_s_d.lib
      debug ${OPENEXR_LIBPATH}/Imath_s_d.lib
      debug ${OPENEXR_LIBPATH}/IlmThread_s_d.lib
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
  set(PYTHON_VERSION 3.7) # CACHE STRING)

  string(REPLACE "." "" _PYTHON_VERSION_NO_DOTS ${PYTHON_VERSION})
  # Use shared libs for vc2008 and vc2010 until we actually have vc2010 libs
  set(PYTHON_LIBRARY ${LIBDIR}/python/lib/python${_PYTHON_VERSION_NO_DOTS}.lib)
  set(PYTHON_LIBRARY_DEBUG ${LIBDIR}/python/lib/python${_PYTHON_VERSION_NO_DOTS}_d.lib)

  unset(_PYTHON_VERSION_NO_DOTS)

  # Shared includes for both vc2008 and vc2010
  set(PYTHON_INCLUDE_DIR ${LIBDIR}/python/include/python${PYTHON_VERSION})

  # uncached vars
  set(PYTHON_INCLUDE_DIRS "${PYTHON_INCLUDE_DIR}")
  set(PYTHON_LIBRARIES debug "${PYTHON_LIBRARY_DEBUG}" optimized "${PYTHON_LIBRARY}" )
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
  if(WITH_WINDOWS_FIND_MODULES)
    find_package(Boost COMPONENTS date_time filesystem thread regex system ${boost_extra_libs})
  endif()
  if(NOT Boost_FOUND)
    warn_hardcoded_paths(BOOST)
    set(BOOST ${LIBDIR}/boost)
    set(BOOST_INCLUDE_DIR ${BOOST}/include)
    set(BOOST_LIBPATH ${BOOST}/lib)
    if(CMAKE_CL_64)
      set(BOOST_POSTFIX "vc140-mt-s-x64-1_68.lib")
      set(BOOST_DEBUG_POSTFIX "vc140-mt-sgd-x64-1_68.lib")
    else()
      set(BOOST_POSTFIX "vc140-mt-s-x32-1_68.lib")
      set(BOOST_DEBUG_POSTFIX "vc140-mt-sgd-x32-1_68.lib")
    endif()
    set(BOOST_LIBRARIES
      optimized ${BOOST_LIBPATH}/libboost_date_time-${BOOST_POSTFIX}
      optimized ${BOOST_LIBPATH}/libboost_filesystem-${BOOST_POSTFIX}
      optimized ${BOOST_LIBPATH}/libboost_regex-${BOOST_POSTFIX}
      optimized ${BOOST_LIBPATH}/libboost_system-${BOOST_POSTFIX}
      optimized ${BOOST_LIBPATH}/libboost_thread-${BOOST_POSTFIX}
      optimized ${BOOST_LIBPATH}/libboost_chrono-${BOOST_POSTFIX}
      debug ${BOOST_LIBPATH}/libboost_date_time-${BOOST_DEBUG_POSTFIX}
      debug ${BOOST_LIBPATH}/libboost_filesystem-${BOOST_DEBUG_POSTFIX}
      debug ${BOOST_LIBPATH}/libboost_regex-${BOOST_DEBUG_POSTFIX}
      debug ${BOOST_LIBPATH}/libboost_system-${BOOST_DEBUG_POSTFIX}
      debug ${BOOST_LIBPATH}/libboost_thread-${BOOST_DEBUG_POSTFIX}
      debug ${BOOST_LIBPATH}/libboost_chrono-${BOOST_DEBUG_POSTFIX}
    )
    if(WITH_CYCLES_OSL)
      set(BOOST_LIBRARIES ${BOOST_LIBRARIES}
        optimized ${BOOST_LIBPATH}/libboost_wave-${BOOST_POSTFIX}
        debug ${BOOST_LIBPATH}/libboost_wave-${BOOST_DEBUG_POSTFIX})
    endif()
    if(WITH_INTERNATIONAL)
      set(BOOST_LIBRARIES ${BOOST_LIBRARIES}
        optimized ${BOOST_LIBPATH}/libboost_locale-${BOOST_POSTFIX}
        debug ${BOOST_LIBPATH}/libboost_locale-${BOOST_DEBUG_POSTFIX})
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
  set(OPENIMAGEIO_LIBPATH ${OPENIMAGEIO}/lib)
  set(OPENIMAGEIO_INCLUDE_DIRS ${OPENIMAGEIO}/include)
  set(OIIO_OPTIMIZED optimized ${OPENIMAGEIO_LIBPATH}/OpenImageIO.lib optimized ${OPENIMAGEIO_LIBPATH}/OpenImageIO_Util.lib)
  set(OIIO_DEBUG debug ${OPENIMAGEIO_LIBPATH}/OpenImageIO_d.lib debug ${OPENIMAGEIO_LIBPATH}/OpenImageIO_Util_d.lib)
  set(OPENIMAGEIO_LIBRARIES ${OIIO_OPTIMIZED} ${OIIO_DEBUG})

  set(OPENIMAGEIO_DEFINITIONS "-DUSE_TBB=0")
  set(OPENCOLORIO_DEFINITIONS "-DOCIO_STATIC_BUILD")
  set(OPENIMAGEIO_IDIFF "${OPENIMAGEIO}/bin/idiff.exe")
  add_definitions(-DOIIO_STATIC_BUILD)
  add_definitions(-DOIIO_NO_SSE=1)
endif()

if(WITH_LLVM)
  set(LLVM_ROOT_DIR ${LIBDIR}/llvm CACHE PATH "Path to the LLVM installation")
  set(LLVM_INCLUDE_DIRS ${LLVM_ROOT_DIR}/$<$<CONFIG:Debug>:Debug>/include CACHE PATH  "Path to the LLVM include directory")
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
  set(OPENCOLORIO_LIBPATH ${LIBDIR}/opencolorio/lib)
  set(OPENCOLORIO_LIBRARIES
    optimized ${OPENCOLORIO_LIBPATH}/OpenColorIO.lib
    optimized ${OPENCOLORIO_LIBPATH}/tinyxml.lib
    optimized ${OPENCOLORIO_LIBPATH}/libyaml-cpp.lib
    debug ${OPENCOLORIO_LIBPATH}/OpenColorIO_d.lib
    debug ${OPENCOLORIO_LIBPATH}/tinyxml_d.lib
    debug ${OPENCOLORIO_LIBPATH}/libyaml-cpp_d.lib
  )
  set(OPENCOLORIO_DEFINITIONS)
endif()

if(WITH_OPENVDB)
  set(BLOSC_LIBRARIES optimized ${LIBDIR}/blosc/lib/libblosc.lib debug ${LIBDIR}/blosc/lib/libblosc_d.lib)
  set(TBB_LIBRARIES optimized ${LIBDIR}/tbb/lib/tbb.lib debug ${LIBDIR}/tbb/lib/tbb_debug.lib)
  set(TBB_INCLUDE_DIR ${LIBDIR}/tbb/include)
  set(OPENVDB ${LIBDIR}/openvdb)
  set(OPENVDB_LIBPATH ${LIBDIR}/openvdb/lib)
  set(OPENVDB_INCLUDE_DIRS ${OPENVDB}/include ${TBB_INCLUDE_DIR})
  set(OPENVDB_LIBRARIES optimized ${OPENVDB_LIBPATH}/openvdb.lib debug ${OPENVDB_LIBPATH}/openvdb_d.lib ${TBB_LIBRARIES} ${BLOSC_LIBRARIES})
  set(OPENVDB_DEFINITIONS -DNOMINMAX)
endif()

if(WITH_ALEMBIC)
  set(ALEMBIC ${LIBDIR}/alembic)
  set(ALEMBIC_INCLUDE_DIR ${ALEMBIC}/include)
  set(ALEMBIC_INCLUDE_DIRS ${ALEMBIC_INCLUDE_DIR})
  set(ALEMBIC_LIBPATH ${ALEMBIC}/lib)
  set(ALEMBIC_LIBRARIES optimized ${ALEMBIC}/lib/alembic.lib debug ${ALEMBIC}/lib/alembic_d.lib)
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

if(WITH_IMAGE_OPENJPEG)
  set(OPENJPEG ${LIBDIR}/openjpeg)
  set(OPENJPEG_INCLUDE_DIRS ${OPENJPEG}/include/openjpeg-2.3)
  set(OPENJPEG_LIBRARIES ${OPENJPEG}/lib/openjp2.lib)
endif()

if(WITH_OPENSUBDIV)
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
  set(SDL_LIBRARY ${SDL_LIBPATH}/SDL2.lib)
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
set(WINTAB_INC ${LIBDIR}/wintab/include)

if(WITH_OPENAL)
  set(OPENAL ${LIBDIR}/openal)
  set(OPENALDIR ${LIBDIR}/openal)
  set(OPENAL_INCLUDE_DIR ${OPENAL}/include/AL)
  set(OPENAL_LIBPATH ${OPENAL}/lib)
  if(MSVC)
    set(OPENAL_LIBRARY ${OPENAL_LIBPATH}/openal32.lib)
  else()
    set(OPENAL_LIBRARY ${OPENAL_LIBPATH}/wrap_oal.lib)
  endif()

endif()

if(WITH_CODEC_SNDFILE)
  set(LIBSNDFILE ${LIBDIR}/sndfile)
  set(LIBSNDFILE_INCLUDE_DIRS ${LIBSNDFILE}/include)
  set(LIBSNDFILE_LIBPATH ${LIBSNDFILE}/lib) # TODO, deprecate
  set(LIBSNDFILE_LIBRARIES ${LIBSNDFILE_LIBPATH}/libsndfile-1.lib)
endif()

if(WITH_RAYOPTIMIZATION AND SUPPORT_SSE_BUILD)
  add_definitions(-D__SSE__ -D__MMX__)
endif()

if(WITH_CYCLES_OSL)
  set(CYCLES_OSL ${LIBDIR}/osl CACHE PATH "Path to OpenShadingLanguage installation")

  find_library(OSL_LIB_EXEC NAMES oslexec PATHS ${CYCLES_OSL}/lib)
  find_library(OSL_LIB_COMP NAMES oslcomp PATHS ${CYCLES_OSL}/lib)
  find_library(OSL_LIB_QUERY NAMES oslquery PATHS ${CYCLES_OSL}/lib)
  find_library(OSL_LIB_EXEC_DEBUG NAMES oslexec_d PATHS ${CYCLES_OSL}/lib)
  find_library(OSL_LIB_COMP_DEBUG NAMES oslcomp_d PATHS ${CYCLES_OSL}/lib)
  find_library(OSL_LIB_QUERY_DEBUG NAMES oslquery_d PATHS ${CYCLES_OSL}/lib)
  list(APPEND OSL_LIBRARIES
    optimized ${OSL_LIB_COMP}
    optimized ${OSL_LIB_EXEC}
    optimized ${OSL_LIB_QUERY}
    optimized ${CYCLES_OSL}/lib/pugixml.lib
    debug ${OSL_LIB_EXEC_DEBUG}
    debug ${OSL_LIB_COMP_DEBUG}
    debug ${OSL_LIB_QUERY_DEBUG}
    debug ${CYCLES_OSL}/lib/pugixml_d.lib
  )
  find_path(OSL_INCLUDE_DIR OSL/oslclosure.h PATHS ${CYCLES_OSL}/include)
  find_program(OSL_COMPILER NAMES oslc PATHS ${CYCLES_OSL}/bin)

  if(OSL_INCLUDE_DIR AND OSL_LIBRARIES AND OSL_COMPILER)
    set(OSL_FOUND TRUE)
  else()
    message(STATUS "OSL not found")
    set(WITH_CYCLES_OSL OFF)
  endif()
endif()

if(WITH_CYCLES_EMBREE)
  windows_find_package(Embree)
  if(NOT EMBREE_FOUND)
    set(EMBREE_INCLUDE_DIRS ${LIBDIR}/embree/include)
    set(EMBREE_LIBRARIES
      optimized ${LIBDIR}/embree/lib/embree3.lib
      optimized ${LIBDIR}/embree/lib/embree_avx2.lib
      optimized ${LIBDIR}/embree/lib/embree_avx.lib
      optimized ${LIBDIR}/embree/lib/embree_sse42.lib
      optimized ${LIBDIR}/embree/lib/lexers.lib
      optimized ${LIBDIR}/embree/lib/math.lib
      optimized ${LIBDIR}/embree/lib/simd.lib
      optimized ${LIBDIR}/embree/lib/sys.lib
      optimized ${LIBDIR}/embree/lib/tasking.lib

      debug ${LIBDIR}/embree/lib/embree3_d.lib
      debug ${LIBDIR}/embree/lib/embree_avx2_d.lib
      debug ${LIBDIR}/embree/lib/embree_avx_d.lib
      debug ${LIBDIR}/embree/lib/embree_sse42_d.lib
      debug ${LIBDIR}/embree/lib/lexers_d.lib
      debug ${LIBDIR}/embree/lib/math_d.lib
      debug ${LIBDIR}/embree/lib/simd_d.lib
      debug ${LIBDIR}/embree/lib/sys_d.lib
      debug ${LIBDIR}/embree/lib/tasking_d.lib)
  endif()
endif()

if(WINDOWS_PYTHON_DEBUG)
  # Include the system scripts in the blender_python_system_scripts project.
  FILE(GLOB_RECURSE inFiles "${CMAKE_SOURCE_DIR}/release/scripts/*.*" )
  ADD_CUSTOM_TARGET(blender_python_system_scripts SOURCES ${inFiles})
  foreach(_source IN ITEMS ${inFiles})
    get_filename_component(_source_path "${_source}" PATH)
    string(REPLACE "${CMAKE_SOURCE_DIR}/release/scripts/" "" _source_path "${_source_path}")
    string(REPLACE "/" "\\" _group_path "${_source_path}")
    source_group("${_group_path}" FILES "${_source}")
  endforeach()
  # Include the user scripts from the profile folder in the blender_python_user_scripts project.
  set(USER_SCRIPTS_ROOT "$ENV{appdata}/blender foundation/blender/${BLENDER_VERSION}")
  file(TO_CMAKE_PATH ${USER_SCRIPTS_ROOT} USER_SCRIPTS_ROOT)
  FILE(GLOB_RECURSE inFiles "${USER_SCRIPTS_ROOT}/scripts/*.*" )
  ADD_CUSTOM_TARGET(blender_python_user_scripts SOURCES ${inFiles})
  foreach(_source IN ITEMS ${inFiles})
    get_filename_component(_source_path "${_source}" PATH)
    string(REPLACE "${USER_SCRIPTS_ROOT}/scripts" "" _source_path "${_source_path}")
    string(REPLACE "/" "\\" _group_path "${_source_path}")
    source_group("${_group_path}" FILES "${_source}")
  endforeach()
  set_target_properties(blender_python_system_scripts PROPERTIES FOLDER "scripts")
  set_target_properties(blender_python_user_scripts PROPERTIES FOLDER "scripts")
  # Set the default debugging options for the project, only write this file once so the user
  # is free to override them at their own perril.
  set(USER_PROPS_FILE "${CMAKE_CURRENT_BINARY_DIR}/source/creator/blender.Cpp.user.props")
  if(NOT EXISTS ${USER_PROPS_FILE})
    # Layout below is messy, because otherwise the generated file will look messy.
    file(WRITE ${USER_PROPS_FILE} "<?xml version=\"1.0\" encoding=\"utf-8\"?>
<Project DefaultTargets=\"Build\" xmlns=\"http://schemas.microsoft.com/developer/msbuild/2003\">
  <PropertyGroup>
    <LocalDebuggerCommandArguments>-con --env-system-scripts \"${CMAKE_SOURCE_DIR}/release/scripts\" </LocalDebuggerCommandArguments>
  </PropertyGroup>
</Project>")
  endif()
endif()
