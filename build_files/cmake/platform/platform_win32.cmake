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
  set(MSVC_CLANG ON)
  set(VC_TOOLS_DIR $ENV{VCToolsRedistDir} CACHE STRING "Location of the msvc redistributables")
  set(MSVC_REDIST_DIR ${VC_TOOLS_DIR})
  if(DEFINED MSVC_REDIST_DIR)
    file(TO_CMAKE_PATH ${MSVC_REDIST_DIR} MSVC_REDIST_DIR)
  else()
    message("Unable to detect the Visual Studio redist directory, copying of the runtime dlls will not work, try running from the visual studio developer prompt.")
  endif()
  # 1) CMake has issues detecting openmp support in clang-cl so we have to provide
  #    the right switches here.
  # 2) While the /openmp switch *should* work, it currently doesn't as for clang 9.0.0
  if(WITH_OPENMP)
    set(OPENMP_CUSTOM ON)
    set(OPENMP_FOUND ON)
    set(OpenMP_C_FLAGS "/clang:-fopenmp")
    set(OpenMP_CXX_FLAGS "/clang:-fopenmp")
    GET_FILENAME_COMPONENT(LLVMROOT "[HKEY_LOCAL_MACHINE\\SOFTWARE\\WOW6432Node\\LLVM\\LLVM;]" ABSOLUTE CACHE)
    set(CLANG_OPENMP_DLL "${LLVMROOT}/bin/libomp.dll")
    set(CLANG_OPENMP_LIB "${LLVMROOT}/lib/libomp.lib")
    if(NOT EXISTS "${CLANG_OPENMP_DLL}")
      message(FATAL_ERROR "Clang OpenMP library (${CLANG_OPENMP_DLL}) not found.")
    endif()
    set(OpenMP_LINKER_FLAGS "\"${CLANG_OPENMP_LIB}\"")
  endif()
  if(WITH_WINDOWS_STRIPPED_PDB)
    message(WARNING "stripped pdb not supported with clang, disabling..")
    set(WITH_WINDOWS_STRIPPED_PDB OFF)
  endif()
else()
  if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 19.28.29921) # MSVC 2019 16.9.16
    message(FATAL_ERROR "Compiler is unsupported, MSVC 2019 16.9.16 or newer is required for building blender.")
  endif()
endif()

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
# 4099 : PDB 'filename' was not found with 'object/library'
string(APPEND CMAKE_EXE_LINKER_FLAGS " /SAFESEH:NO /ignore:4099")
string(APPEND CMAKE_SHARED_LINKER_FLAGS " /SAFESEH:NO /ignore:4099")
string(APPEND CMAKE_MODULE_LINKER_FLAGS " /SAFESEH:NO /ignore:4099")

list(APPEND PLATFORM_LINKLIBS
  ws2_32 vfw32 winmm kernel32 user32 gdi32 comdlg32 Comctl32 version
  advapi32 shfolder shell32 ole32 oleaut32 uuid psapi Dbghelp Shlwapi
  pathcch Shcore
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

# RTTI is on by default even without this switch
# however having it in the CXX Flags makes it difficult
# to remove for individual files that want to disable it
# using the /GR- flag without generating a build warning
# that both /GR and /GR- are specified.
remove_cc_flag("/GR")

# Make the Windows 8.1 API available for use.
add_definitions(-D_WIN32_WINNT=0x603)
include(build_files/cmake/platform/platform_win32_bundle_crt.cmake)
remove_cc_flag("/MDd" "/MD" "/Zi")

if(MSVC_CLANG) # Clangs version of cl doesn't support all flags
  string(APPEND CMAKE_CXX_FLAGS " ${CXX_WARN_FLAGS} /nologo /J /Gd /EHsc -Wno-unused-command-line-argument -Wno-microsoft-enum-forward-reference ")
  set(CMAKE_C_FLAGS     "${CMAKE_C_FLAGS} /nologo /J /Gd -Wno-unused-command-line-argument -Wno-microsoft-enum-forward-reference")
else()
  string(APPEND CMAKE_CXX_FLAGS " /nologo /J /Gd /MP /EHsc /bigobj /Zc:inline")
  set(CMAKE_C_FLAGS     "${CMAKE_C_FLAGS} /nologo /J /Gd /MP /bigobj /Zc:inline")
endif()

# X64 ASAN is available and usable on MSVC 16.9 preview 4 and up)
if(WITH_COMPILER_ASAN AND MSVC AND NOT MSVC_CLANG)
  if(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 19.28.29828)
    #set a flag so we don't have to do this comparison all the time
    SET(MSVC_ASAN ON)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /fsanitize=address")
    set(CMAKE_C_FLAGS     "${CMAKE_C_FLAGS} /fsanitize=address")
    string(APPEND CMAKE_EXE_LINKER_FLAGS_DEBUG " /INCREMENTAL:NO")
    string(APPEND CMAKE_SHARED_LINKER_FLAGS_DEBUG " /INCREMENTAL:NO")
  else()
    message("-- ASAN not supported on MSVC ${CMAKE_CXX_COMPILER_VERSION}")
  endif()
endif()


# C++ standards conformace (/permissive-) is available on msvc 15.5 (1912) and up
if(MSVC_VERSION GREATER 1911 AND NOT MSVC_CLANG)
  string(APPEND CMAKE_CXX_FLAGS " /permissive-")
  # Two-phase name lookup does not place nicely with OpenMP yet, so disable for now
  string(APPEND CMAKE_CXX_FLAGS " /Zc:twoPhase-")
endif()

if(WITH_WINDOWS_SCCACHE AND CMAKE_VS_MSBUILD_COMMAND)
    message(WARNING "Disabling sccache, sccache is not supported with msbuild")
    set(WITH_WINDOWS_SCCACHE OFF)
endif()

# Debug Symbol format
# sccache # MSVC_ASAN # format # why
# ON      # ON        # Z7     # sccache will only play nice with Z7
# ON      # OFF       # Z7     # sccache will only play nice with Z7
# OFF     # ON        # Zi     # Asan will not play nice with Edit and Continue
# OFF     # OFF       # ZI     # Neither asan nor sscache is enabled Edit and Continue is available

# Release Symbol format
# sccache # MSVC_ASAN # format # why
# ON      # ON        # Z7     # sccache will only play nice with Z7
# ON      # OFF       # Z7     # sccache will only play nice with Z7
# OFF     # ON        # Zi     # Asan will not play nice with Edit and Continue
# OFF     # OFF       # Zi     # Edit and Continue disables some optimizations


if(WITH_WINDOWS_SCCACHE)
    set(CMAKE_C_COMPILER_LAUNCHER sccache)
    set(CMAKE_CXX_COMPILER_LAUNCHER sccache)
    set(SYMBOL_FORMAT /Z7)
    set(SYMBOL_FORMAT_RELEASE /Z7)
else()
    unset(CMAKE_C_COMPILER_LAUNCHER)
    unset(CMAKE_CXX_COMPILER_LAUNCHER)
    if(MSVC_ASAN)
      set(SYMBOL_FORMAT /Z7)
      set(SYMBOL_FORMAT_RELEASE /Z7)
    else()
      set(SYMBOL_FORMAT /ZI)
      set(SYMBOL_FORMAT_RELEASE /Zi)
    endif()
endif()

if(WITH_WINDOWS_PDB)
  set(PDB_INFO_OVERRIDE_FLAGS "${SYMBOL_FORMAT_RELEASE}")
  set(PDB_INFO_OVERRIDE_LINKER_FLAGS "/DEBUG /OPT:REF /OPT:ICF /INCREMENTAL:NO")
endif()

string(APPEND CMAKE_CXX_FLAGS_DEBUG " /MDd ${SYMBOL_FORMAT}")
string(APPEND CMAKE_C_FLAGS_DEBUG " /MDd ${SYMBOL_FORMAT}")
string(APPEND CMAKE_CXX_FLAGS_RELEASE " /MD ${PDB_INFO_OVERRIDE_FLAGS}")
string(APPEND CMAKE_C_FLAGS_RELEASE " /MD ${PDB_INFO_OVERRIDE_FLAGS}")
string(APPEND CMAKE_CXX_FLAGS_MINSIZEREL " /MD ${PDB_INFO_OVERRIDE_FLAGS}")
string(APPEND CMAKE_C_FLAGS_MINSIZEREL " /MD ${PDB_INFO_OVERRIDE_FLAGS}")
string(APPEND CMAKE_CXX_FLAGS_RELWITHDEBINFO " /MD ${SYMBOL_FORMAT_RELEASE}")
string(APPEND CMAKE_C_FLAGS_RELWITHDEBINFO " /MD ${SYMBOL_FORMAT_RELEASE}")
unset(SYMBOL_FORMAT)
unset(SYMBOL_FORMAT_RELEASE)

# JMC is available on msvc 15.8 (1915) and up
if(MSVC_VERSION GREATER 1914 AND NOT MSVC_CLANG)
  string(APPEND CMAKE_CXX_FLAGS_DEBUG " /JMC")
endif()

string(APPEND PLATFORM_LINKFLAGS " /SUBSYSTEM:CONSOLE /STACK:2097152")
set(PLATFORM_LINKFLAGS_RELEASE "/NODEFAULTLIB:libcmt.lib /NODEFAULTLIB:libcmtd.lib /NODEFAULTLIB:msvcrtd.lib")
string(APPEND PLATFORM_LINKFLAGS_DEBUG " /IGNORE:4099 /NODEFAULTLIB:libcmt.lib /NODEFAULTLIB:msvcrt.lib /NODEFAULTLIB:libcmtd.lib")

# Ignore meaningless for us linker warnings.
string(APPEND PLATFORM_LINKFLAGS " /ignore:4049 /ignore:4217 /ignore:4221")
set(PLATFORM_LINKFLAGS_RELEASE "${PLATFORM_LINKFLAGS} ${PDB_INFO_OVERRIDE_LINKER_FLAGS}")
string(APPEND CMAKE_STATIC_LINKER_FLAGS " /ignore:4221")

if(CMAKE_CL_64)
  string(PREPEND PLATFORM_LINKFLAGS "/MACHINE:X64 ")
else()
  string(PREPEND PLATFORM_LINKFLAGS "/MACHINE:IX86 /LARGEADDRESSAWARE ")
endif()

if(NOT DEFINED LIBDIR)

  # Setup 64bit and 64bit windows systems
  if(CMAKE_CL_64)
    message(STATUS "64 bit compiler detected.")
    set(LIBDIR_BASE "win64")
  else()
    message(FATAL_ERROR "32 bit compiler detected, blender no longer provides pre-build libraries for 32 bit windows, please set the LIBDIR cmake variable to your own library folder")
  endif()
  if(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 19.30.30423)
    message(STATUS "Visual Studio 2022 detected.")
    set(LIBDIR ${CMAKE_SOURCE_DIR}/../lib/${LIBDIR_BASE}_vc15)
  elseif(MSVC_VERSION GREATER 1919)
    message(STATUS "Visual Studio 2019 detected.")
    set(LIBDIR ${CMAKE_SOURCE_DIR}/../lib/${LIBDIR_BASE}_vc15)
  endif()
else()
  message(STATUS "Using pre-compiled LIBDIR: ${LIBDIR}")
endif()
if(NOT EXISTS "${LIBDIR}/")
  message(FATAL_ERROR "\n\nWindows requires pre-compiled libs at: '${LIBDIR}'. Please run `make update` in the blender source folder to obtain them.")
endif()

if(CMAKE_GENERATOR MATCHES "^Visual Studio.+" AND # Only supported in the VS IDE
   MSVC_VERSION GREATER_EQUAL 1924            AND # Supported for 16.4+
   WITH_CLANG_TIDY                                # And Clang Tidy needs to be on
  )
  set(CMAKE_VS_GLOBALS
    "RunCodeAnalysis=false"
    "EnableMicrosoftCodeAnalysis=false"
    "EnableClangTidyCodeAnalysis=true"
  )
  set(VS_CLANG_TIDY ON)
endif()

# Mark libdir as system headers with a lower warn level, to resolve some warnings
# that we have very little control over
if(MSVC_VERSION GREATER_EQUAL 1914 AND # Available with 15.7+
   NOT MSVC_CLANG                  AND # But not for clang
   NOT WITH_WINDOWS_SCCACHE        AND # And not when sccache is enabled
   NOT VS_CLANG_TIDY)                  # Clang-tidy does not like these options
  add_compile_options(/experimental:external /external:templates- /external:I "${LIBDIR}" /external:W0)
endif()

# Add each of our libraries to our cmake_prefix_path so find_package() could work
file(GLOB children RELATIVE ${LIBDIR} ${LIBDIR}/*)
foreach(child ${children})
  if(IS_DIRECTORY ${LIBDIR}/${child})
    list(APPEND CMAKE_PREFIX_PATH  ${LIBDIR}/${child})
  endif()
endforeach()

if(WITH_PUGIXML)
  set(PUGIXML_LIBRARIES optimized ${LIBDIR}/pugixml/lib/pugixml.lib debug ${LIBDIR}/pugixml/lib/pugixml_d.lib)
  set(PUGIXML_INCLUDE_DIR ${LIBDIR}/pugixml/include)
endif()

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
  set(PNG_LIBRARIES ${LIBDIR}/png/lib/libpng.lib ${ZLIB_LIBRARY})
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
set(FREETYPE_LIBRARIES
  ${LIBDIR}/freetype/lib/freetype2ST.lib
  ${LIBDIR}/brotli/lib/brotlidec-static.lib
  ${LIBDIR}/brotli/lib/brotlicommon-static.lib
)
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
    optimized ${OPENCOLLADA}/lib/opencollada/OpenCOLLADASaxFrameworkLoader.lib
    optimized ${OPENCOLLADA}/lib/opencollada/OpenCOLLADAFramework.lib
    optimized ${OPENCOLLADA}/lib/opencollada/OpenCOLLADABaseUtils.lib
    optimized ${OPENCOLLADA}/lib/opencollada/OpenCOLLADAStreamWriter.lib
    optimized ${OPENCOLLADA}/lib/opencollada/MathMLSolver.lib
    optimized ${OPENCOLLADA}/lib/opencollada/GeneratedSaxParser.lib
    optimized ${OPENCOLLADA}/lib/opencollada/xml.lib
    optimized ${OPENCOLLADA}/lib/opencollada/buffer.lib
    optimized ${OPENCOLLADA}/lib/opencollada/ftoa.lib

    debug ${OPENCOLLADA}/lib/opencollada/OpenCOLLADASaxFrameworkLoader_d.lib
    debug ${OPENCOLLADA}/lib/opencollada/OpenCOLLADAFramework_d.lib
    debug ${OPENCOLLADA}/lib/opencollada/OpenCOLLADABaseUtils_d.lib
    debug ${OPENCOLLADA}/lib/opencollada/OpenCOLLADAStreamWriter_d.lib
    debug ${OPENCOLLADA}/lib/opencollada/MathMLSolver_d.lib
    debug ${OPENCOLLADA}/lib/opencollada/GeneratedSaxParser_d.lib
    debug ${OPENCOLLADA}/lib/opencollada/xml_d.lib
    debug ${OPENCOLLADA}/lib/opencollada/buffer_d.lib
    debug ${OPENCOLLADA}/lib/opencollada/ftoa_d.lib
  )

  list(APPEND OPENCOLLADA_LIBRARIES ${OPENCOLLADA}/lib/opencollada/UTF.lib)

  set(PCRE_LIBRARIES
    optimized ${OPENCOLLADA}/lib/opencollada/pcre.lib

    debug ${OPENCOLLADA}/lib/opencollada/pcre_d.lib
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
  set(PYTHON_VERSION 3.10) # CACHE STRING)

  string(REPLACE "." "" _PYTHON_VERSION_NO_DOTS ${PYTHON_VERSION})
  set(PYTHON_LIBRARY ${LIBDIR}/python/${_PYTHON_VERSION_NO_DOTS}/libs/python${_PYTHON_VERSION_NO_DOTS}.lib)
  set(PYTHON_LIBRARY_DEBUG ${LIBDIR}/python/${_PYTHON_VERSION_NO_DOTS}/libs/python${_PYTHON_VERSION_NO_DOTS}_d.lib)

  set(PYTHON_INCLUDE_DIR ${LIBDIR}/python/${_PYTHON_VERSION_NO_DOTS}/include)
  set(PYTHON_NUMPY_INCLUDE_DIRS ${LIBDIR}/python/${_PYTHON_VERSION_NO_DOTS}/lib/site-packages/numpy/core/include)
  set(NUMPY_FOUND ON)
  unset(_PYTHON_VERSION_NO_DOTS)
  # uncached vars
  set(PYTHON_INCLUDE_DIRS "${PYTHON_INCLUDE_DIR}")
  set(PYTHON_LIBRARIES debug "${PYTHON_LIBRARY_DEBUG}" optimized "${PYTHON_LIBRARY}" )
endif()

if(WITH_BOOST)
  if(WITH_CYCLES AND WITH_CYCLES_OSL)
    set(boost_extra_libs wave)
  endif()
  if(WITH_INTERNATIONAL)
    list(APPEND boost_extra_libs locale)
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
    set(BOOST_VERSION_HEADER ${BOOST_INCLUDE_DIR}/boost/version.hpp)
    if(EXISTS ${BOOST_VERSION_HEADER})
      file(STRINGS "${BOOST_VERSION_HEADER}" BOOST_LIB_VERSION REGEX "#define BOOST_LIB_VERSION ")
      if(BOOST_LIB_VERSION MATCHES "#define BOOST_LIB_VERSION \"([0-9_]+)\"")
        set(BOOST_VERSION "${CMAKE_MATCH_1}")
      endif()
    endif()
    if(NOT BOOST_VERSION)
      message(FATAL_ERROR "Unable to determine Boost version")
    endif()
    set(BOOST_POSTFIX "vc141-mt-x64-${BOOST_VERSION}.lib")
    set(BOOST_DEBUG_POSTFIX "vc141-mt-gd-x64-${BOOST_VERSION}.lib")
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
    if(WITH_CYCLES AND WITH_CYCLES_OSL)
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
  set(OPENIMAGEIO ${LIBDIR}/OpenImageIO)
  set(OPENIMAGEIO_LIBPATH ${OPENIMAGEIO}/lib)
  set(OPENIMAGEIO_INCLUDE_DIRS ${OPENIMAGEIO}/include)
  set(OIIO_OPTIMIZED optimized ${OPENIMAGEIO_LIBPATH}/OpenImageIO.lib optimized ${OPENIMAGEIO_LIBPATH}/OpenImageIO_Util.lib)
  set(OIIO_DEBUG debug ${OPENIMAGEIO_LIBPATH}/OpenImageIO_d.lib debug ${OPENIMAGEIO_LIBPATH}/OpenImageIO_Util_d.lib)
  set(OPENIMAGEIO_LIBRARIES ${OIIO_OPTIMIZED} ${OIIO_DEBUG})

  set(OPENIMAGEIO_DEFINITIONS "-DUSE_TBB=0")
  set(OPENIMAGEIO_IDIFF "${OPENIMAGEIO}/bin/idiff.exe")
  add_definitions(-DOIIO_STATIC_DEFINE)
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
  set(OPENCOLORIO ${LIBDIR}/OpenColorIO)
  set(OPENCOLORIO_INCLUDE_DIRS ${OPENCOLORIO}/include)
  set(OPENCOLORIO_LIBPATH ${OPENCOLORIO}/lib)
  set(OPENCOLORIO_LIBRARIES
    optimized ${OPENCOLORIO_LIBPATH}/OpenColorIO.lib
    optimized ${OPENCOLORIO_LIBPATH}/libyaml-cpp.lib
    optimized ${OPENCOLORIO_LIBPATH}/libexpatMD.lib
    optimized ${OPENCOLORIO_LIBPATH}/pystring.lib
    debug ${OPENCOLORIO_LIBPATH}/OpencolorIO_d.lib
    debug ${OPENCOLORIO_LIBPATH}/libyaml-cpp_d.lib
    debug ${OPENCOLORIO_LIBPATH}/libexpatdMD.lib
    debug ${OPENCOLORIO_LIBPATH}/pystring_d.lib
  )
  set(OPENCOLORIO_DEFINITIONS "-DOpenColorIO_SKIP_IMPORTS")
endif()

if(WITH_OPENVDB)
  set(OPENVDB ${LIBDIR}/openVDB)
  set(OPENVDB_LIBPATH ${OPENVDB}/lib)
  set(OPENVDB_INCLUDE_DIRS ${OPENVDB}/include)
  set(OPENVDB_LIBRARIES optimized ${OPENVDB_LIBPATH}/openvdb.lib debug ${OPENVDB_LIBPATH}/openvdb_d.lib )
  set(OPENVDB_DEFINITIONS -DNOMINMAX -D_USE_MATH_DEFINES)
endif()

if(WITH_NANOVDB)
  set(NANOVDB ${LIBDIR}/nanoVDB)
  set(NANOVDB_INCLUDE_DIR ${NANOVDB}/include)
endif()

if(WITH_OPENIMAGEDENOISE)
  set(OPENIMAGEDENOISE ${LIBDIR}/OpenImageDenoise)
  set(OPENIMAGEDENOISE_LIBPATH ${LIBDIR}/OpenImageDenoise/lib)
  set(OPENIMAGEDENOISE_INCLUDE_DIRS ${OPENIMAGEDENOISE}/include)
  set(OPENIMAGEDENOISE_LIBRARIES
    optimized ${OPENIMAGEDENOISE_LIBPATH}/OpenImageDenoise.lib
    optimized ${OPENIMAGEDENOISE_LIBPATH}/common.lib
    optimized ${OPENIMAGEDENOISE_LIBPATH}/dnnl.lib
    debug ${OPENIMAGEDENOISE_LIBPATH}/OpenImageDenoise_d.lib
    debug ${OPENIMAGEDENOISE_LIBPATH}/common_d.lib
    debug ${OPENIMAGEDENOISE_LIBPATH}/dnnl_d.lib)
  set(OPENIMAGEDENOISE_DEFINITIONS)
endif()

if(WITH_ALEMBIC)
  set(ALEMBIC ${LIBDIR}/alembic)
  set(ALEMBIC_INCLUDE_DIR ${ALEMBIC}/include)
  set(ALEMBIC_INCLUDE_DIRS ${ALEMBIC_INCLUDE_DIR})
  set(ALEMBIC_LIBPATH ${ALEMBIC}/lib)
  set(ALEMBIC_LIBRARIES optimized ${ALEMBIC}/lib/Alembic.lib debug ${ALEMBIC}/lib/Alembic_d.lib)
  set(ALEMBIC_FOUND 1)
endif()

if(WITH_IMAGE_OPENJPEG)
  set(OPENJPEG ${LIBDIR}/openjpeg)
  set(OPENJPEG_INCLUDE_DIRS ${OPENJPEG}/include/openjpeg-2.3)
  set(OPENJPEG_LIBRARIES ${OPENJPEG}/lib/openjp2.lib)
endif()

if(WITH_OPENSUBDIV)
  set(OPENSUBDIV_INCLUDE_DIRS ${LIBDIR}/opensubdiv/include)
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

if(WITH_TBB)
  set(TBB_LIBRARIES optimized ${LIBDIR}/tbb/lib/tbb.lib debug ${LIBDIR}/tbb/lib/tbb_debug.lib)
  set(TBB_INCLUDE_DIR ${LIBDIR}/tbb/include)
  set(TBB_INCLUDE_DIRS ${TBB_INCLUDE_DIR})
  if(WITH_TBB_MALLOC_PROXY)
    set(TBB_MALLOC_LIBRARIES optimized ${LIBDIR}/tbb/lib/tbbmalloc.lib debug ${LIBDIR}/tbb/lib/tbbmalloc_debug.lib)
    add_definitions(-DWITH_TBB_MALLOC)
  endif()
endif()

# used in many places so include globally, like OpenGL
blender_include_dirs_sys("${PTHREADS_INCLUDE_DIRS}")

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

if(WITH_CYCLES AND WITH_CYCLES_OSL)
  set(CYCLES_OSL ${LIBDIR}/osl CACHE PATH "Path to OpenShadingLanguage installation")
  set(OSL_SHADER_DIR ${CYCLES_OSL}/shaders)
  # Shaders have moved around a bit between OSL versions, check multiple locations
  if(NOT EXISTS "${OSL_SHADER_DIR}")
    set(OSL_SHADER_DIR ${CYCLES_OSL}/share/OSL/shaders)
  endif()
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
    debug ${OSL_LIB_EXEC_DEBUG}
    debug ${OSL_LIB_COMP_DEBUG}
    debug ${OSL_LIB_QUERY_DEBUG}
    ${PUGIXML_LIBRARIES}
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

if(WITH_CYCLES AND WITH_CYCLES_EMBREE)
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

if(WITH_USD)
  windows_find_package(USD)
  if(NOT USD_FOUND)
    set(USD_FOUND ON)
    set(USD_INCLUDE_DIRS ${LIBDIR}/usd/include)
    set(USD_RELEASE_LIB ${LIBDIR}/usd/lib/libusd_m.lib)
    set(USD_DEBUG_LIB ${LIBDIR}/usd/lib/libusd_m_d.lib)
    set(USD_LIBRARY_DIR ${LIBDIR}/usd/lib)
    set(USD_LIBRARIES
      debug ${USD_DEBUG_LIB}
      optimized ${USD_RELEASE_LIB}
    )
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

  # If the user scripts env var is set, include scripts from there otherwise
  # include user scripts in the profile folder.
  if(DEFINED ENV{BLENDER_USER_SCRIPTS})
    message(STATUS "Including user scripts from environment BLENDER_USER_SCRIPTS=$ENV{BLENDER_USER_SCRIPTS}")
    set(USER_SCRIPTS_ROOT "$ENV{BLENDER_USER_SCRIPTS}")
  else()
    message(STATUS "Including user scripts from the profile folder")
    # Include the user scripts from the profile folder in the blender_python_user_scripts project.
    set(USER_SCRIPTS_ROOT "$ENV{appdata}/blender foundation/blender/${BLENDER_VERSION}/scripts")
  endif()

  file(TO_CMAKE_PATH ${USER_SCRIPTS_ROOT} USER_SCRIPTS_ROOT)
  FILE(GLOB_RECURSE inFiles "${USER_SCRIPTS_ROOT}/*.*" )
  ADD_CUSTOM_TARGET(blender_python_user_scripts SOURCES ${inFiles})
  foreach(_source IN ITEMS ${inFiles})
    get_filename_component(_source_path "${_source}" PATH)
    string(REPLACE "${USER_SCRIPTS_ROOT}" "" _source_path "${_source_path}")
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

if(WITH_XR_OPENXR)
  if(EXISTS ${LIBDIR}/xr_openxr_sdk)
    set(XR_OPENXR_SDK ${LIBDIR}/xr_openxr_sdk)
    set(XR_OPENXR_SDK_LIBPATH ${LIBDIR}/xr_openxr_sdk/lib)
    set(XR_OPENXR_SDK_INCLUDE_DIR ${XR_OPENXR_SDK}/include)
    # This is the old name of this library, it is checked to
    # support the transition between the old and new lib versions
    # this can be removed after the next lib update.
    if(EXISTS ${XR_OPENXR_SDK_LIBPATH}/openxr_loader_d.lib)
      set(XR_OPENXR_SDK_LIBRARIES optimized ${XR_OPENXR_SDK_LIBPATH}/openxr_loader.lib debug ${XR_OPENXR_SDK_LIBPATH}/openxr_loader_d.lib)
    else()
      set(XR_OPENXR_SDK_LIBRARIES optimized ${XR_OPENXR_SDK_LIBPATH}/openxr_loader.lib debug ${XR_OPENXR_SDK_LIBPATH}/openxr_loaderd.lib)
    endif()
  else()
    message(WARNING "OpenXR-SDK was not found, disabling WITH_XR_OPENXR")
    set(WITH_XR_OPENXR OFF)
  endif()
endif()

if(WITH_GMP)
  set(GMP_INCLUDE_DIRS ${LIBDIR}/gmp/include)
  set(GMP_LIBRARIES ${LIBDIR}/gmp/lib/libgmp-10.lib optimized ${LIBDIR}/gmp/lib/libgmpxx.lib debug ${LIBDIR}/gmp/lib/libgmpxx_d.lib)
  set(GMP_ROOT_DIR ${LIBDIR}/gmp)
  set(GMP_FOUND ON)
endif()

if(WITH_POTRACE)
  set(POTRACE_INCLUDE_DIRS ${LIBDIR}/potrace/include)
  set(POTRACE_LIBRARIES ${LIBDIR}/potrace/lib/potrace.lib)
  set(POTRACE_FOUND ON)
endif()

if(WITH_HARU)
  if(EXISTS ${LIBDIR}/haru)
    set(HARU_FOUND ON)
    set(HARU_ROOT_DIR ${LIBDIR}/haru)
    set(HARU_INCLUDE_DIRS ${HARU_ROOT_DIR}/include)
    set(HARU_LIBRARIES ${HARU_ROOT_DIR}/lib/libhpdfs.lib)
  else()
    message(WARNING "Haru was not found, disabling WITH_HARU")
    set(WITH_HARU OFF)
  endif()
endif()

set(ZSTD_INCLUDE_DIRS ${LIBDIR}/zstd/include)
set(ZSTD_LIBRARIES ${LIBDIR}/zstd/lib/zstd_static.lib)
