# SPDX-FileCopyrightText: 2016 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

# Libraries configuration for Windows.

add_definitions(-DWIN32)

if(NOT MSVC)
  message(FATAL_ERROR "Compiler is unsupported")
endif()

# By default CMAKE will map imported configs that lack a specific RELWITHDEBINFO
# or MINSIZEREL location, to the debug libs, which is not good as this will cause
# all sorts of linking issues with MSVC. Map them explicitly to Release libs.
# for further reading: https://gitlab.kitware.com/cmake/cmake/-/issues/20319
set(CMAKE_MAP_IMPORTED_CONFIG_MINSIZEREL MinSizeRel RelWithDebInfo Release Debug)
set(CMAKE_MAP_IMPORTED_CONFIG_RELWITHDEBINFO RelWithDebInfo Release MinSizeRel Debug)
set(CMAKE_MAP_IMPORTED_CONFIG_RELEASE Release RelWithDebInfo MinSizeRel Debug)

if(CMAKE_C_COMPILER_ID MATCHES "Clang")
  set(MSVC_CLANG ON)
  if(NOT WITH_WINDOWS_EXTERNAL_MANIFEST)
    message(WARNING "WITH_WINDOWS_EXTERNAL_MANIFEST is required for clang, turning ON")
    set(WITH_WINDOWS_EXTERNAL_MANIFEST ON)
  endif()
  set(VC_TOOLS_DIR $ENV{VCToolsRedistDir} CACHE STRING "Location of the msvc redistributables")
  set(MSVC_REDIST_DIR ${VC_TOOLS_DIR})
  if(DEFINED MSVC_REDIST_DIR)
    file(TO_CMAKE_PATH ${MSVC_REDIST_DIR} MSVC_REDIST_DIR)
  else()
    message(WARNING
      "Unable to detect the Visual Studio redist directory, "
      "copying of the runtime dlls will not work, "
      "try running from the visual studio developer prompt."
    )
  endif()
else()
  if(WITH_BLENDER)
    if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 19.44.35216) # MSVC 2022 17.14.14
      message(FATAL_ERROR
        "Compiler is unsupported, MSVC 2022 17.14.14 or newer is required for building blender."
      )
    endif()
  endif()
endif()

set(WINDOWS_ARM64_MIN_VSCMD_VER 17.14.23)
# We have a minimum version of VSCMD for ARM64 (ie, the version the libs were compiled against)
# This checks for the version on initial run, and caches it,
# so users do not have to run the VS CMD window every time
if(CMAKE_SYSTEM_PROCESSOR STREQUAL "ARM64")
  set(VC_VSCMD_VER $ENV{VSCMD_VER} CACHE STRING "Version of the VSCMD initially run from")
  mark_as_advanced(VC_VSCMD_VER)
  set(VSCMD_VER ${VC_VSCMD_VER})
  if(DEFINED VSCMD_VER)
    if(VSCMD_VER VERSION_LESS WINDOWS_ARM64_MIN_VSCMD_VER)
      message(FATAL_ERROR
        "Windows ARM64 requires VS2022 version ${WINDOWS_ARM64_MIN_VSCMD_VER} or greater - "
        "detected ${VSCMD_VER}, please update your VS2022 install!"
      )
    endif()
  else()
    message(FATAL_ERROR
      "Unable to detect the Visual Studio CMD version, "
      "try running from the visual studio developer prompt."
    )
  endif()
endif()

if(WITH_BLENDER AND NOT WITH_PYTHON_MODULE)
  set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT blender)
endif()

macro(warn_hardcoded_paths package_name)
  if(WITH_WINDOWS_FIND_MODULES)
    message(WARNING "Using HARDCODED ${package_name} locations")
  endif()
endmacro()

macro(windows_find_package package_name)
  if(WITH_WINDOWS_FIND_MODULES)
    find_package(${package_name})
  endif()
endmacro()

macro(find_package_wrapper)
  if(WITH_WINDOWS_FIND_MODULES)
    find_package(${ARGV})
  endif()
endmacro()

# Needed, otherwise system encoding causes utf-8 encoding to fail in some cases (C4819)
add_compile_options("$<$<C_COMPILER_ID:MSVC>:/utf-8>")
add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")

# Needed for some MSVC installations, example warning:
# `4099 : PDB {filename} was not found with {object/library}`.
string(APPEND CMAKE_EXE_LINKER_FLAGS " /SAFESEH:NO /ignore:4099")
string(APPEND CMAKE_SHARED_LINKER_FLAGS " /SAFESEH:NO /ignore:4099")
string(APPEND CMAKE_MODULE_LINKER_FLAGS " /SAFESEH:NO /ignore:4099")

if(WITH_WINDOWS_EXTERNAL_MANIFEST)
  string(APPEND CMAKE_EXE_LINKER_FLAGS " /manifest:no")
endif()

list(APPEND PLATFORM_LINKLIBS
  ws2_32 vfw32 winmm kernel32 user32 gdi32 comdlg32 Comctl32 version
  advapi32 shfolder shell32 ole32 oleaut32 uuid psapi Dbghelp Shlwapi
  pathcch Shcore Dwmapi Crypt32 Bcrypt Mpr
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

# First generate the manifest for tests since it will not need the dependency on the CRT.
configure_file(
  ${CMAKE_SOURCE_DIR}/release/windows/manifest/blender.exe.manifest.in
  ${CMAKE_CURRENT_BINARY_DIR}/tests.exe.manifest
  @ONLY
)

# Always detect CRT paths, but only manually install with WITH_WINDOWS_BUNDLE_CRT.
set(CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS_SKIP TRUE)
set(CMAKE_INSTALL_UCRT_LIBRARIES TRUE)
include(InstallRequiredSystemLibraries)

if(WITH_WINDOWS_BUNDLE_CRT)
  # ucrtbase(d).dll cannot be in the manifest, due to the way windows 10 handles
  # redirects for this dll, for details see #88813.
  foreach(lib ${CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS})
    string(FIND ${lib} "ucrtbase" pos)
    if(NOT pos EQUAL -1)
      list(REMOVE_ITEM CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS ${lib})
      install(FILES ${lib} DESTINATION . COMPONENT Libraries)
    endif()
  endforeach()
  # Install the CRT to the blender.crt Sub folder.
  install(FILES ${CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS} DESTINATION blender.crt COMPONENT Libraries)

  windows_generate_manifest(
    FILES "${CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS}"
    OUTPUT "${CMAKE_BINARY_DIR}/blender.crt.manifest"
    NAME "blender.crt"
  )

  install(FILES ${CMAKE_BINARY_DIR}/blender.crt.manifest DESTINATION blender.crt)
  set(BUNDLECRT "<dependency><dependentAssembly><assemblyIdentity type=\"win32\" name=\"blender.crt\" version=\"1.0.0.0\" /></dependentAssembly></dependency>")
endif()
if(NOT WITH_PYTHON_MODULE)
  set(BUNDLECRT "${BUNDLECRT}<dependency><dependentAssembly><assemblyIdentity type=\"win32\" name=\"blender.shared\" version=\"1.0.0.0\" /></dependentAssembly></dependency>")
endif()
configure_file(
  ${CMAKE_SOURCE_DIR}/release/windows/manifest/blender.exe.manifest.in
  ${CMAKE_CURRENT_BINARY_DIR}/blender.exe.manifest
  @ONLY
)

remove_cc_flag(
  "/MDd"
  "/MD"
  "/Zi"
)

if(MSVC_CLANG) # Clangs version of cl doesn't support all flags
  string(APPEND CMAKE_CXX_FLAGS " ${CXX_WARN_FLAGS} /Gy /MP /nologo /J /Gd /showFilenames /EHsc -Wno-unused-command-line-argument -Wno-microsoft-enum-forward-reference /clang:-funsigned-char /clang:-fno-strict-aliasing /clang:-ffp-contract=off")
  string(APPEND CMAKE_C_FLAGS   " /MP /nologo /J /Gy /Gd /showFilenames -Wno-unused-command-line-argument -Wno-microsoft-enum-forward-reference /clang:-funsigned-char /clang:-fno-strict-aliasing /clang:-ffp-contract=off")
else()
  string(APPEND CMAKE_CXX_FLAGS " /nologo /J /Gd /MP /EHsc /bigobj")
  string(APPEND CMAKE_C_FLAGS   " /nologo /J /Gd /MP /bigobj")
endif()

# X64 ASAN is available and usable on MSVC 16.9 preview 4 and up)
if(WITH_COMPILER_ASAN AND MSVC AND NOT MSVC_CLANG)
  if(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 19.28.29828)
    # Set a flag so we don't have to do this comparison all the time.
    set(MSVC_ASAN ON)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /fsanitize=address /D_DISABLE_VECTOR_ANNOTATION /D_DISABLE_STRING_ANNOTATION")
    set(CMAKE_C_FLAGS     "${CMAKE_C_FLAGS} /fsanitize=address")
    string(APPEND CMAKE_EXE_LINKER_FLAGS_DEBUG " /INCREMENTAL:NO")
    string(APPEND CMAKE_SHARED_LINKER_FLAGS_DEBUG " /INCREMENTAL:NO")
  else()
    message(WARNING "ASAN not supported on MSVC ${CMAKE_CXX_COMPILER_VERSION}")
  endif()
endif()


# C++ standards conformance
# /permissive-    : Available from MSVC 15.5 (1912) and up. Enables standards-confirming compiler
#                   behavior. Required until the project is marked as c++20.
# /Zc:__cplusplus : Available from MSVC 15.7 (1914) and up. Ensures correct value of the __cplusplus
#                   preprocessor macro.
# /Zc:inline      : Enforces C++11 requirement that all functions declared 'inline' must have a
#                   definition available in the same translation unit if they're used.
# /Zc:preprocessor: Available from MSVC 16.5 (1925) and up. Enables standards-conforming
#                   preprocessor.
if(NOT MSVC_CLANG)
  string(APPEND CMAKE_CXX_FLAGS " /permissive- /Zc:__cplusplus /Zc:inline")
  string(APPEND CMAKE_C_FLAGS   " /Zc:inline")

  # For VS2022+ we can enable the new preprocessor
  if(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 19.30.30423)
    string(APPEND CMAKE_CXX_FLAGS " /Zc:preprocessor")
    string(APPEND CMAKE_C_FLAGS " /Zc:preprocessor")
  endif()
endif()

if(WITH_WINDOWS_SCCACHE AND CMAKE_VS_MSBUILD_COMMAND)
  message(WARNING "Disabling sccache, sccache is not supported with msbuild")
  set(WITH_WINDOWS_SCCACHE OFF)
endif()

if(WITH_WINDOWS_SCCACHE)
  set(CMAKE_C_COMPILER_LAUNCHER sccache)
  set(CMAKE_CXX_COMPILER_LAUNCHER sccache)
  # sccache will only play nice with Z7.
  set(SYMBOL_FORMAT /Z7)
  set(SYMBOL_FORMAT_RELEASE /Z7)
else()
  unset(CMAKE_C_COMPILER_LAUNCHER)
  unset(CMAKE_CXX_COMPILER_LAUNCHER)
  if(MSVC_ASAN OR MSVC_CLANG)
    # Neither Asan nor Clang will play nice with Edit and Continue.
    set(SYMBOL_FORMAT /Zi)
    set(SYMBOL_FORMAT_RELEASE /Zi)
  else()
    # Otherwise enable Edit and Continue.
    set(SYMBOL_FORMAT /ZI)
    # Except for Release builds, since it disables some optimizations.
    set(SYMBOL_FORMAT_RELEASE /Zi)
  endif()
endif()

if(WITH_WINDOWS_RELEASE_PDB)
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
if(NOT MSVC_CLANG)
  string(APPEND CMAKE_CXX_FLAGS_DEBUG " /JMC")
endif()

string(APPEND PLATFORM_LINKFLAGS " /SUBSYSTEM:CONSOLE /STACK:2097152")
set(PLATFORM_LINKFLAGS_RELEASE "/NODEFAULTLIB:libcmt.lib /NODEFAULTLIB:libcmtd.lib /NODEFAULTLIB:msvcrtd.lib")

string(APPEND PLATFORM_LINKFLAGS_DEBUG " /debug /IGNORE:4099 /NODEFAULTLIB:libcmt.lib /NODEFAULTLIB:msvcrt.lib /NODEFAULTLIB:libcmtd.lib")

# Ignore meaningless for us linker warnings.
string(APPEND PLATFORM_LINKFLAGS " /ignore:4049 /ignore:4217 /ignore:4221")
set(PLATFORM_LINKFLAGS_RELEASE "${PLATFORM_LINKFLAGS} ${PDB_INFO_OVERRIDE_LINKER_FLAGS}")
string(APPEND CMAKE_STATIC_LINKER_FLAGS " /ignore:4221")

if(CMAKE_CL_64)
  if(CMAKE_SYSTEM_PROCESSOR STREQUAL "ARM64")
    string(PREPEND PLATFORM_LINKFLAGS "/MACHINE:ARM64 ")
  else()
    string(PREPEND PLATFORM_LINKFLAGS "/MACHINE:X64 ")
  endif()
else()
  string(PREPEND PLATFORM_LINKFLAGS "/MACHINE:IX86 /LARGEADDRESSAWARE ")
endif()

if(NOT DEFINED LIBDIR)
  # Setup 64bit and 64bit windows systems
  if(CMAKE_CL_64)
    if(CMAKE_SYSTEM_PROCESSOR STREQUAL "ARM64")
      set(LIBDIR_BASE "windows_arm64")
    else()
      set(LIBDIR_BASE "windows_x64")
    endif()
  else()
    message(FATAL_ERROR
      "32 bit compiler detected, "
      "blender no longer provides pre-build libraries for 32 bit windows, "
      "please set the LIBDIR cmake variable to your own library folder"
    )
  endif()
  if(MSVC_CLANG)
    message(STATUS
      "Clang version ${CMAKE_CXX_COMPILER_VERSION} detected, masquerading as MSVC ${MSVC_VERSION}"
    )
    set(LIBDIR ${CMAKE_SOURCE_DIR}/lib/${LIBDIR_BASE})
  elseif(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 19.50.0)
    message(STATUS "Visual Studio 2026 detected.")
    set(LIBDIR ${CMAKE_SOURCE_DIR}/lib/${LIBDIR_BASE})
  elseif(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 19.30.30423)
    message(STATUS "Visual Studio 2022 detected.")
    set(LIBDIR ${CMAKE_SOURCE_DIR}/lib/${LIBDIR_BASE})
  elseif(MSVC_VERSION GREATER 1919)
    message(STATUS "Visual Studio 2019 detected.")
    set(LIBDIR ${CMAKE_SOURCE_DIR}/lib/${LIBDIR_BASE})
  endif()
else()
  if(FIRST_RUN)
    message(STATUS "Using pre-compiled LIBDIR: ${LIBDIR}")
  endif()
endif()
if(NOT EXISTS "${LIBDIR}/.git")
  message(FATAL_ERROR
    "\n\nWindows requires pre-compiled libs at: '${LIBDIR}'. "
    "Please run `make update` in the blender source folder to obtain them."
  )
endif()

include(platform_old_libs_update)

# Only supported in the VS IDE & Clang Tidy needs to be on.
if(CMAKE_GENERATOR MATCHES "^Visual Studio.+" AND WITH_CLANG_TIDY)
  set(CMAKE_VS_GLOBALS
    "RunCodeAnalysis=false"
    "EnableMicrosoftCodeAnalysis=false"
    "EnableClangTidyCodeAnalysis=true"
  )
  set(VS_CLANG_TIDY ON)
endif()

# To support building against both 3.4 and 3.5 lib folders, disable materialX if it is not found
set(MATERIALX_LIB_FOLDER_EXISTS EXISTS ${LIBDIR}/materialx)
set_and_warn_library_found("MaterialX" MATERIALX_LIB_FOLDER_EXISTS WITH_MATERIALX)
unset(MATERIALX_LIB_FOLDER_EXISTS)

# Mark libdir as system headers with a lower warn level, to resolve some warnings
# that we have very little control over
if(NOT MSVC_CLANG                  AND # Available with MSVC 15.7+ but not for CLANG.
   NOT WITH_WINDOWS_SCCACHE        AND # And not when sccache is enabled
   NOT VS_CLANG_TIDY)                  # Clang-tidy does not like these options
  add_compile_options(/experimental:external /external:I "${LIBDIR}" /external:W0)
endif()

# Add each of our libraries to our cmake_prefix_path so find_package() could work
file(GLOB children RELATIVE ${LIBDIR} ${LIBDIR}/*)
foreach(child ${children})
  if(IS_DIRECTORY ${LIBDIR}/${child})
    list(APPEND CMAKE_PREFIX_PATH ${LIBDIR}/${child})
  endif()
endforeach()

if(WITH_PUGIXML)
  set(PUGIXML_LIBRARIES
    optimized ${LIBDIR}/pugixml/lib/pugixml.lib
    debug ${LIBDIR}/pugixml/lib/pugixml_d.lib
  )
  set(PUGIXML_INCLUDE_DIR ${LIBDIR}/pugixml/include)
endif()

set(ZLIB_INCLUDE_DIRS ${LIBDIR}/zlib/include)
set(ZLIB_LIBRARIES ${LIBDIR}/zlib/lib/libz_st.lib)
set(ZLIB_INCLUDE_DIR ${LIBDIR}/zlib/include)
set(ZLIB_LIBRARY ${LIBDIR}/zlib/lib/libz_st.lib)
set(ZLIB_DIR ${LIBDIR}/zlib)

set(fmt_DIR ${LIBDIR}/fmt/lib/cmake/config)
find_package(fmt REQUIRED CONFIG)

set(Eigen3_DIR ${LIBDIR}/eigen)
find_package(Eigen3 REQUIRED CONFIG)

if(WITH_LIBMV)
  set(absl_DIR ${LIBDIR}/abseil)
  set(Ceres_DIR ${LIBDIR}/ceres)
  find_package(Ceres REQUIRED CONFIG)
endif()

windows_find_package(ZLIB) # We want to find before finding things that depend on it like PNG.
windows_find_package(PNG)
if(NOT PNG_FOUND)
  warn_hardcoded_paths(libpng)
  set(PNG_PNG_INCLUDE_DIR ${LIBDIR}/png/include)
  set(PNG_LIBRARIES ${LIBDIR}/png/lib/libpng.lib ${ZLIB_LIBRARY})
  set(PNG "${LIBDIR}/png")
  set(PNG_INCLUDE_DIRS "${PNG}/include")
  set(PNG_LIBPATH ${PNG}/lib) # not cmake defined
endif()

set(JPEG_NAMES ${JPEG_NAMES} libjpeg)
windows_find_package(JPEG REQUIRED)
if(NOT JPEG_FOUND)
  warn_hardcoded_paths(libjpeg)
  set(JPEG_INCLUDE_DIR ${LIBDIR}/jpeg/include)
  set(JPEG_LIBRARIES ${LIBDIR}/jpeg/lib/libjpeg.lib)
endif()

set(EPOXY_ROOT_DIR ${LIBDIR}/epoxy)
windows_find_package(Epoxy REQUIRED)
if(NOT EPOXY_FOUND)
  set(Epoxy_INCLUDE_DIRS ${LIBDIR}/epoxy/include)
  set(Epoxy_LIBRARIES ${LIBDIR}/epoxy/lib/epoxy.lib)
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
)
set(BROTLI_LIBRARIES
  ${LIBDIR}/brotli/lib/brotlidec-static.lib
  ${LIBDIR}/brotli/lib/brotlicommon-static.lib
)

windows_find_package(Freetype REQUIRED)

if(WITH_HARFBUZZ)
  windows_find_package(Harfbuzz)
  if(NOT Harfbuzz_FOUND)
    set(LIBHARFBUZZ_INCLUDE_DIRS ${LIBDIR}/harfbuzz/include)
    set(LIBHARFBUZZ_LIBRARIES
      optimized ${LIBDIR}/harfbuzz/lib/libharfbuzz.lib
      debug ${LIBDIR}/harfbuzz/lib/libharfbuzz_d.lib
    )
    set(Harfbuzz_FOUND ON)
  endif()
endif()

if(WITH_FRIBIDI)
  windows_find_package(Fribidi)
  if(NOT Fribidi_FOUND)
    set(LIBFRIBIDI_INCLUDE_DIRS ${LIBDIR}/fribidi/include)
    set(LIBFRIBIDI_LIBRARIES ${LIBDIR}/fribidi/lib/libfribidi.lib)
    set(Fribidi_FOUND ON)
  endif()
endif()

if(WITH_FFTW3)
  set(FFTW3 ${LIBDIR}/fftw3)
  set(FFTW3_LIBRARIES
    ${FFTW3}/lib/fftw3.lib
    ${FFTW3}/lib/fftw3f.lib
    ${FFTW3}/lib/fftw3_threads.lib
    ${FFTW3}/lib/fftw3f_threads.lib
  )
  set(FFTW3_INCLUDE_DIRS ${FFTW3}/include)
  set(FFTW3_LIBPATH ${FFTW3}/lib)
endif()

if(WITH_IMAGE_WEBP)
  set(WEBP_INCLUDE_DIRS ${LIBDIR}/webp/include)
  set(WEBP_ROOT_DIR ${LIBDIR}/webp)
  if(EXISTS ${LIBDIR}/webp/lib/libsharpyuv.lib) # webp 1.3.x+
    set(WEBP_LIBRARIES
      ${LIBDIR}/webp/lib/libwebp.lib
      ${LIBDIR}/webp/lib/libwebpdemux.lib
      ${LIBDIR}/webp/lib/libwebpmux.lib
      ${LIBDIR}/webp/lib/libsharpyuv.lib
    )
  else()
    set(WEBP_LIBRARIES
      ${LIBDIR}/webp/lib/webp.lib
      ${LIBDIR}/webp/lib/webpdemux.lib
      ${LIBDIR}/webp/lib/webpmux.lib
    )
  endif()
  set(WEBP_FOUND ON)
endif()

if(WITH_CODEC_FFMPEG)
  set(FFMPEG_INCLUDE_DIRS
    ${LIBDIR}/ffmpeg/include
    ${LIBDIR}/ffmpeg/include/msvc
  )
  windows_find_package(FFmpeg)
  if(NOT FFmpeg_FOUND)
    warn_hardcoded_paths(FFmpeg)
    set(FFMPEG_LIBRARIES
      ${LIBDIR}/ffmpeg/lib/avcodec.lib
      ${LIBDIR}/ffmpeg/lib/avformat.lib
      ${LIBDIR}/ffmpeg/lib/avdevice.lib
      ${LIBDIR}/ffmpeg/lib/avutil.lib
      ${LIBDIR}/ffmpeg/lib/swscale.lib
    )
  endif()
endif()

set(openjph_ROOT ${LIBDIR}/openjph)

if(WITH_IMAGE_OPENEXR)
  set(IMATH_ROOT ${LIBDIR}/imath)
  find_package(IMATH REQUIRED CONFIG)
  set(OpenEXR_ROOT ${LIBDIR}/openexr)
  find_package(OpenEXR REQUIRED CONFIG)
endif()

# Try to find tiff first then complain and set static and maybe wrong paths
windows_find_package(TIFF)
if(NOT TIFF_FOUND)
  warn_hardcoded_paths(libtiff)
  set(TIFF_LIBRARY ${LIBDIR}/tiff/lib/libtiff.lib)
  set(TIFF_INCLUDE_DIR ${LIBDIR}/tiff/include)
endif()

if(WITH_JACK)
  set(JACK_INCLUDE_DIRS
    ${LIBDIR}/jack/include/jack
    ${LIBDIR}/jack/include
  )
  set(JACK_LIBRARIES
    optimized ${LIBDIR}/jack/lib/libjack.lib
    debug ${LIBDIR}/jack/lib/libjack_d.lib
  )
endif()

set(_PYTHON_VERSION "3.13")
string(REPLACE "." "" _PYTHON_VERSION_NO_DOTS ${_PYTHON_VERSION})

# Enable for a short time when bumping to the next Python version.
if(TRUE)
  if(NOT EXISTS ${LIBDIR}/python/${_PYTHON_VERSION_NO_DOTS})
    set(_PYTHON_VERSION "3.11")
    string(REPLACE "." "" _PYTHON_VERSION_NO_DOTS ${_PYTHON_VERSION})
    if(NOT EXISTS ${LIBDIR}/python/${_PYTHON_VERSION_NO_DOTS})
      message(FATAL_ERROR
        "Missing python libraries! Neither 3.13 nor 3.11 are found in ${LIBDIR}/python"
      )
    endif()
  endif()
endif()

# Python executable is needed as part of the build-process,
# note that building without Python is quite unusual.
set(PYTHON_EXECUTABLE ${LIBDIR}/python/${_PYTHON_VERSION_NO_DOTS}/bin/python$<$<CONFIG:Debug>:_d>.exe)

if(WITH_PYTHON)
  # Cache version for make_bpy_wheel.py to detect.
  unset(PYTHON_VERSION CACHE)
  set(PYTHON_VERSION "${_PYTHON_VERSION}" CACHE STRING "Python version")

  set(PYTHON_LIBRARY ${LIBDIR}/python/${_PYTHON_VERSION_NO_DOTS}/libs/python${_PYTHON_VERSION_NO_DOTS}.lib)
  set(PYTHON_LIBRARY_DEBUG ${LIBDIR}/python/${_PYTHON_VERSION_NO_DOTS}/libs/python${_PYTHON_VERSION_NO_DOTS}_d.lib)

  set(PYTHON_INCLUDE_DIR ${LIBDIR}/python/${_PYTHON_VERSION_NO_DOTS}/include)
  set(PYTHON_NUMPY_INCLUDE_DIRS ${LIBDIR}/python/${_PYTHON_VERSION_NO_DOTS}/lib/site-packages/numpy/_core/include)
  set(NUMPY_FOUND ON)
  # uncached vars
  set(PYTHON_INCLUDE_DIRS "${PYTHON_INCLUDE_DIR}")
  set(PYTHON_LIBRARIES
    debug "${PYTHON_LIBRARY_DEBUG}"
    optimized "${PYTHON_LIBRARY}"
  )
endif()

unset(_PYTHON_VERSION)
unset(_PYTHON_VERSION_NO_DOTS)

set(OpenImageIO_ROOT ${LIBDIR}/OpenImageIO)
find_package(OpenImageIO REQUIRED CONFIG)

if(WITH_LLVM)
  set(LLVM_ROOT_DIR ${LIBDIR}/llvm CACHE PATH "Path to the LLVM installation")
  set(LLVM_INCLUDE_DIRS ${LLVM_ROOT_DIR}/$<$<CONFIG:Debug>:Debug>/include CACHE PATH "Path to the LLVM include directory")
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
    message(WARNING
      "LLVM debug libs not present on this system. Using release libs for debug builds."
    )
    set(LLVM_LIBRARY ${LLVM_LIBRARY_OPTIMIZED})
  endif()

endif()

if(WITH_OPENCOLORIO)
  windows_find_package(OpenColorIO)
  if(NOT OpenColorIO_FOUND)
    set(OPENCOLORIO ${LIBDIR}/OpenColorIO)
    set(OPENCOLORIO_INCLUDE_DIRS ${OPENCOLORIO}/include)
    set(OPENCOLORIO_LIBPATH ${OPENCOLORIO}/lib)
    if(EXISTS ${OPENCOLORIO_LIBPATH}/libexpatMD.lib) # 3.4
      set(OPENCOLORIO_LIBRARIES
        optimized ${OPENCOLORIO_LIBPATH}/OpenColorIO.lib
        optimized ${OPENCOLORIO_LIBPATH}/libexpatMD.lib
        optimized ${OPENCOLORIO_LIBPATH}/pystring.lib
        optimized ${OPENCOLORIO_LIBPATH}/libyaml-cpp.lib
        debug ${OPENCOLORIO_LIBPATH}/OpencolorIO_d.lib
        debug ${OPENCOLORIO_LIBPATH}/libexpatdMD.lib
        debug ${OPENCOLORIO_LIBPATH}/pystring_d.lib
        debug ${OPENCOLORIO_LIBPATH}/libyaml-cpp_d.lib
      )
      set(OPENCOLORIO_DEFINITIONS "-DOpenColorIO_SKIP_IMPORTS")
    else()
      set(OPENCOLORIO_LIBRARIES
        optimized ${OPENCOLORIO_LIBPATH}/OpenColorIO.lib
        debug ${OPENCOLORIO_LIBPATH}/OpencolorIO_d.lib
      )
    endif()
  endif()
endif()

if(WITH_OPENVDB)
  windows_find_package(OpenVDB)
  if(NOT OpenVDB_FOUND)
    set(OPENVDB ${LIBDIR}/openVDB)
    set(OPENVDB_LIBPATH ${OPENVDB}/lib)
    set(OPENVDB_INCLUDE_DIRS ${OPENVDB}/include)
    set(OPENVDB_LIBRARIES
      optimized ${OPENVDB_LIBPATH}/openvdb.lib
      debug ${OPENVDB_LIBPATH}/openvdb_d.lib
    )
  endif()
  set(OPENVDB_DEFINITIONS -DNOMINMAX -D_USE_MATH_DEFINES)
endif()

if(WITH_NANOVDB)
  set(NANOVDB ${LIBDIR}/openvdb)
  set(NANOVDB_INCLUDE_DIR ${NANOVDB}/include)
  if(NOT EXISTS "${NANOVDB_INCLUDE_DIR}/nanovdb")
    # When not found, could be an older lib folder with where nanovdb
    # had its own lib folder, to ease the transition period, fall back
    # to that copy if the copy in openvdb is not found.
    set(NANOVDB ${LIBDIR}/nanoVDB)
    set(NANOVDB_INCLUDE_DIR ${NANOVDB}/include)
  endif()
endif()

if(WITH_OPENIMAGEDENOISE)
  if(EXISTS ${LIBDIR}/OpenImageDenoise/bin/OpenImageDenoise.dll) # 4.0 libs
    find_package(OpenImageDenoise REQUIRED CONFIG)
    if(OpenImageDenoise_FOUND)
      get_target_property(OPENIMAGEDENOISE_LIBRARIES_RELEASE OpenImageDenoise IMPORTED_IMPLIB_RELEASE)
      get_target_property(OPENIMAGEDENOISE_LIBRARIES_DEBUG OpenImageDenoise IMPORTED_IMPLIB_DEBUG)
      if(EXISTS ${OPENIMAGEDENOISE_LIBRARIES_DEBUG})
        set(OPENIMAGEDENOISE_LIBRARIES
          optimized ${OPENIMAGEDENOISE_LIBRARIES_RELEASE}
          debug ${OPENIMAGEDENOISE_LIBRARIES_DEBUG}
        )
      else()
        if(EXISTS ${OPENIMAGEDENOISE_LIBRARIES_RELEASE})
          set(OPENIMAGEDENOISE_LIBRARIES ${OPENIMAGEDENOISE_LIBRARIES_RELEASE})
        else()
          set(WITH_OPENIMAGEDENOISE OFF)
          message(STATUS "OpenImageDenoise not found, disabling WITH_OPENIMAGEDENOISE")
        endif()
      endif()
      get_target_property(OPENIMAGEDENOISE_INCLUDE_DIRS OpenImageDenoise INTERFACE_INCLUDE_DIRECTORIES)
    else()
      set(WITH_OPENIMAGEDENOISE OFF)
      message(STATUS "OpenImageDenoise not found, disabling WITH_OPENIMAGEDENOISE")
    endif()
  else()
    set(OPENIMAGEDENOISE ${LIBDIR}/OpenImageDenoise)
    set(OPENIMAGEDENOISE_LIBPATH ${LIBDIR}/OpenImageDenoise/lib)
    set(OPENIMAGEDENOISE_INCLUDE_DIRS ${OPENIMAGEDENOISE}/include)
    set(OPENIMAGEDENOISE_LIBRARIES
      optimized ${OPENIMAGEDENOISE_LIBPATH}/OpenImageDenoise.lib
      optimized ${OPENIMAGEDENOISE_LIBPATH}/common.lib
      optimized ${OPENIMAGEDENOISE_LIBPATH}/dnnl.lib
      debug ${OPENIMAGEDENOISE_LIBPATH}/OpenImageDenoise_d.lib
      debug ${OPENIMAGEDENOISE_LIBPATH}/common_d.lib
      debug ${OPENIMAGEDENOISE_LIBPATH}/dnnl_d.lib
    )
  endif()
  set(OPENIMAGEDENOISE_DEFINITIONS "")
endif()

if(WITH_MANIFOLD)
  set(MANIFOLD ${LIBDIR}/manifold)
  if(EXISTS ${MANIFOLD})
    set(MANIFOLD_INCLUDE_DIR ${MANIFOLD}/include)
    set(MANIFOLD_INCLUDE_DIRS ${MANIFOLD_INCLUDE_DIR})
    set(MANIFOLD_LIBDIR ${MANIFOLD}/lib)
    set(MANIFOLD_LIBRARIES
      optimized ${MANIFOLD_LIBDIR}/manifold.lib
      debug ${MANIFOLD_LIBDIR}/manifold_d.lib
    )
    set(MANIFOLD_FOUND 1)
  else()
    set(WITH_MANIFOLD OFF)
    message(STATUS "Manifold not found, disabling WITH_MANIFOLD")
  endif()
endif()

if(WITH_ALEMBIC)
  set(ALEMBIC ${LIBDIR}/alembic)
  set(ALEMBIC_INCLUDE_DIR ${ALEMBIC}/include)
  set(ALEMBIC_INCLUDE_DIRS ${ALEMBIC_INCLUDE_DIR})
  set(ALEMBIC_LIBPATH ${ALEMBIC}/lib)
  set(ALEMBIC_LIBRARIES
    optimized ${ALEMBIC}/lib/Alembic.lib
    debug ${ALEMBIC}/lib/Alembic_d.lib
  )
  set(ALEMBIC_FOUND 1)
endif()

if(WITH_IMAGE_OPENJPEG)
  set(OPENJPEG ${LIBDIR}/openjpeg)
  set(OPENJPEG_INCLUDE_DIRS ${OPENJPEG}/include/openjpeg-2.5)
  if(NOT EXISTS "${OPENJPEG_INCLUDE_DIRS}")
    # when not found, could be an older lib folder with openjpeg 2.4
    # to ease the transition period, fall back if 2.5 is not found.
    set(OPENJPEG_INCLUDE_DIRS ${OPENJPEG}/include/openjpeg-2.4)
  endif()
  set(OPENJPEG_LIBRARIES ${OPENJPEG}/lib/openjp2.lib)
endif()

if(WITH_OPENSUBDIV)
  windows_find_package(OpenSubdiv)
  if(NOT OpenSubdiv_FOUND)
    set(OPENSUBDIV ${LIBDIR}/opensubdiv)
    set(OPENSUBDIV_INCLUDE_DIRS ${OPENSUBDIV}/include)
    set(OPENSUBDIV_LIBPATH ${OPENSUBDIV}/lib)
    set(OPENSUBDIV_LIBRARIES
      optimized ${OPENSUBDIV_LIBPATH}/osdCPU.lib
      optimized ${OPENSUBDIV_LIBPATH}/osdGPU.lib
      debug ${OPENSUBDIV_LIBPATH}/osdCPU_d.lib
      debug ${OPENSUBDIV_LIBPATH}/osdGPU_d.lib
    )
  endif()
endif()

if(WITH_RUBBERBAND)
  set(RUBBERBAND_FOUND TRUE)
  set(RUBBERBAND_INCLUDE_DIRS ${LIBDIR}/rubberband/include)
  set(RUBBERBAND_LIBRARIES
    optimized ${LIBDIR}/rubberband/lib/rubberband-static.lib
    debug ${LIBDIR}/rubberband/lib/rubberband-static_d.lib
  )
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
  windows_find_package(TBB)
  if(TBB_FOUND)
    get_target_property(TBB_LIBRARIES_RELEASE TBB::tbb LOCATION_RELEASE)
    get_target_property(TBB_LIBRARIES_DEBUG TBB::tbb LOCATION_DEBUG)
    set(TBB_LIBRARIES
      optimized ${TBB_LIBRARIES_RELEASE}
      debug ${TBB_LIBRARIES_DEBUG}
    )
    get_target_property(TBB_INCLUDE_DIRS TBB::tbb INTERFACE_INCLUDE_DIRECTORIES)
  else()
    if(EXISTS ${LIBDIR}/tbb/lib/tbb12.lib) # 4.4
      set(TBB_LIBRARIES
        optimized ${LIBDIR}/tbb/lib/tbb12.lib
        debug ${LIBDIR}/tbb/lib/tbb12_debug.lib
      )
    else() # 4.3-
      set(TBB_LIBRARIES
        optimized ${LIBDIR}/tbb/lib/tbb.lib
        debug ${LIBDIR}/tbb/lib/tbb_debug.lib
      )
    endif()
    set(TBB_INCLUDE_DIR ${LIBDIR}/tbb/include)
    set(TBB_INCLUDE_DIRS ${TBB_INCLUDE_DIR})
    if(WITH_TBB_MALLOC_PROXY)
      set(TBB_MALLOC_LIBRARIES
        optimized ${LIBDIR}/tbb/lib/tbbmalloc.lib
        debug ${LIBDIR}/tbb/lib/tbbmalloc_debug.lib
      )
      add_definitions(-DWITH_TBB_MALLOC)
    endif()
  endif()
endif()

# used in many places so include globally, like OpenGL
include_directories(SYSTEM "${PTHREADS_INCLUDE_DIRS}")

set(WINTAB_INC ${CMAKE_SOURCE_DIR}/extern/wintab/include)

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
  if(EXISTS ${LIBSNDFILE_LIBPATH}/sndfile.lib)
    set(LIBSNDFILE_LIBRARIES ${LIBSNDFILE_LIBPATH}/sndfile.lib)
  else()
    set(LIBSNDFILE_LIBRARIES ${LIBSNDFILE_LIBPATH}/libsndfile-1.lib)
  endif()
endif()

test_neon_support()
if(SUPPORTS_NEON_BUILD)
  windows_find_package(sse2neon)
  if(NOT SSE2NEON_FOUND)
    set(SSE2NEON_ROOT_DIR ${LIBDIR}/sse2neon)
    set(SSE2NEON_INCLUDE_DIRS ${LIBDIR}/sse2neon)
    set(SSE2NEON_FOUND True)
  endif()
endif()

if(WITH_CYCLES AND WITH_CYCLES_OSL)
  set(CYCLES_OSL ${LIBDIR}/osl CACHE PATH "Path to OpenShadingLanguage installation")
  set(OSL_ROOT ${CYCLES_OSL}) 
  find_package(OSL REQUIRED CONFIG) 
endif()

if(WITH_CYCLES AND WITH_CYCLES_EMBREE)
  windows_find_package(Embree)
  if(NOT Embree_FOUND)
    set(EMBREE_ROOT_DIR ${LIBDIR}/embree)
    set(EMBREE_INCLUDE_DIRS ${LIBDIR}/embree/include)

    if(EXISTS ${LIBDIR}/embree/include/embree4/rtcore_config.h)
      set(EMBREE_MAJOR_VERSION 4)
    else()
      set(EMBREE_MAJOR_VERSION 3)
    endif()

    file(READ ${LIBDIR}/embree/include/embree${EMBREE_MAJOR_VERSION}/rtcore_config.h _embree_config_header)
    if(_embree_config_header MATCHES "#define EMBREE_STATIC_LIB")
      set(EMBREE_STATIC_LIB TRUE)
    else()
      set(EMBREE_STATIC_LIB FALSE)
    endif()

    if(_embree_config_header MATCHES "#define EMBREE_SYCL_SUPPORT")
      set(EMBREE_SYCL_SUPPORT TRUE)
    else()
      set(EMBREE_SYCL_SUPPORT FALSE)
    endif()

    set(EMBREE_LIBRARIES
      optimized ${LIBDIR}/embree/lib/embree${EMBREE_MAJOR_VERSION}.lib
      debug ${LIBDIR}/embree/lib/embree${EMBREE_MAJOR_VERSION}_d.lib
    )

    if(EMBREE_SYCL_SUPPORT)
      # MSVC debug version of embree may have been compiled without SYCL support
      if(EXISTS ${LIBDIR}/embree/lib/embree4_sycl_d.lib)
        set(EMBREE_LIBRARIES
          ${EMBREE_LIBRARIES}
          optimized ${LIBDIR}/embree/lib/embree4_sycl.lib
          debug ${LIBDIR}/embree/lib/embree4_sycl_d.lib
        )
      else()
        set(EMBREE_LIBRARIES
          ${EMBREE_LIBRARIES}
          optimized ${LIBDIR}/embree/lib/embree4_sycl.lib
        )
      endif()
    endif()

    if(EMBREE_STATIC_LIB)
      set(EMBREE_LIBRARIES
        ${EMBREE_LIBRARIES}
        optimized ${LIBDIR}/embree/lib/embree_avx2.lib
        optimized ${LIBDIR}/embree/lib/embree_avx.lib
        optimized ${LIBDIR}/embree/lib/embree_sse42.lib
        optimized ${LIBDIR}/embree/lib/lexers.lib
        optimized ${LIBDIR}/embree/lib/math.lib
        optimized ${LIBDIR}/embree/lib/simd.lib
        optimized ${LIBDIR}/embree/lib/sys.lib
        optimized ${LIBDIR}/embree/lib/tasking.lib
        debug ${LIBDIR}/embree/lib/embree_avx2_d.lib
        debug ${LIBDIR}/embree/lib/embree_avx_d.lib
        debug ${LIBDIR}/embree/lib/embree_sse42_d.lib
        debug ${LIBDIR}/embree/lib/lexers_d.lib
        debug ${LIBDIR}/embree/lib/math_d.lib
        debug ${LIBDIR}/embree/lib/simd_d.lib
        debug ${LIBDIR}/embree/lib/sys_d.lib
        debug ${LIBDIR}/embree/lib/tasking_d.lib
      )

      if(EMBREE_SYCL_SUPPORT)
        # MSVC debug version of embree may have been compiled without SYCL support
        if(EXISTS ${LIBDIR}/embree/lib/embree_rthwif_d.lib)
          set(EMBREE_LIBRARIES
            ${EMBREE_LIBRARIES}
            optimized ${LIBDIR}/embree/lib/embree_rthwif.lib
            debug ${LIBDIR}/embree/lib/embree_rthwif_d.lib
          )
        else()
          set(EMBREE_LIBRARIES
            ${EMBREE_LIBRARIES}
            optimized ${LIBDIR}/embree/lib/embree_rthwif.lib
          )
        endif()
      endif()
    endif()
  endif()
  if(NOT EMBREE_STATIC_LIB)
    list(APPEND PLATFORM_BUNDLED_LIBRARIES
      RELEASE ${EMBREE_ROOT_DIR}/bin/embree${EMBREE_MAJOR_VERSION}.dll
      DEBUG ${EMBREE_ROOT_DIR}/bin/embree${EMBREE_MAJOR_VERSION}_d.dll
    )
  endif()
endif()

if(WITH_USD)
  windows_find_package(USD)
  if(NOT USD_FOUND)
    # 3.5 22.03 libs
    set(USD_INCLUDE_DIRS ${LIBDIR}/usd/include)
    set(USD_RELEASE_LIB ${LIBDIR}/usd/lib/usd_usd_ms.lib)
    set(USD_DEBUG_LIB ${LIBDIR}/usd/lib/usd_usd_ms_d.lib)
    set(USD_LIBRARY_DIR ${LIBDIR}/usd/lib)
    if(NOT EXISTS "${USD_RELEASE_LIB}") # 3.5 22.11 libs
      set(USD_RELEASE_LIB ${LIBDIR}/usd/lib/usd_ms.lib)
      set(USD_DEBUG_LIB ${LIBDIR}/usd/lib/usd_ms_d.lib)
    endif()
    # Older USD had different filenames, if the new ones are
    # not found see if the older ones exist, to ease the
    # transition period while landing libs.
    if(NOT EXISTS "${USD_RELEASE_LIB}") # 3.3 static libs
      set(USD_RELEASE_LIB ${LIBDIR}/usd/lib/usd_usd_m.lib)
      set(USD_DEBUG_LIB ${LIBDIR}/usd/lib/usd_usd_m_d.lib)
    endif()
    set(USD_LIBRARIES
      debug ${USD_DEBUG_LIB}
      optimized ${USD_RELEASE_LIB}
    )
  endif()
endif()

if(WITH_MATERIALX)
  include("${LIBDIR}/MaterialX/lib/cmake/MaterialX/MaterialXTargets.cmake")
  set_target_properties(MaterialXCore PROPERTIES MAP_IMPORTED_CONFIG_RELWITHDEBINFO RELEASE)
  set_target_properties(MaterialXFormat PROPERTIES MAP_IMPORTED_CONFIG_RELWITHDEBINFO RELEASE)
endif()

if(WINDOWS_PYTHON_DEBUG)
  # Include the system scripts in the blender_python_system_scripts project.
  file(GLOB_RECURSE inFiles "${CMAKE_SOURCE_DIR}/scripts/*.*" )
  add_custom_target(blender_python_system_scripts SOURCES ${inFiles})
  foreach(_source IN ITEMS ${inFiles})
    get_filename_component(_source_path "${_source}" PATH)
    string(REPLACE "${CMAKE_SOURCE_DIR}/scripts/" "" _source_path "${_source_path}")
    string(REPLACE "/" "\\" _group_path "${_source_path}")
    source_group("${_group_path}" FILES "${_source}")
  endforeach()

  # If the user scripts env var is set, include scripts from there otherwise
  # include user scripts in the profile folder.
  if(DEFINED ENV{BLENDER_USER_SCRIPTS})
    message(STATUS
      "Including user scripts from environment BLENDER_USER_SCRIPTS=$ENV{BLENDER_USER_SCRIPTS}"
    )
    set(USER_SCRIPTS_ROOT "$ENV{BLENDER_USER_SCRIPTS}")
  else()
    message(STATUS "Including user scripts from the profile folder")
    # Include the user scripts from the profile folder in the blender_python_user_scripts project.
    set(USER_SCRIPTS_ROOT "$ENV{appdata}/blender foundation/blender/${BLENDER_VERSION}/scripts")
  endif()

  file(TO_CMAKE_PATH ${USER_SCRIPTS_ROOT} USER_SCRIPTS_ROOT)
  file(GLOB_RECURSE inFiles "${USER_SCRIPTS_ROOT}/*.*" )
  add_custom_target(blender_python_user_scripts SOURCES ${inFiles})
  foreach(_source IN ITEMS ${inFiles})
    get_filename_component(_source_path "${_source}" PATH)
    string(REPLACE "${USER_SCRIPTS_ROOT}" "" _source_path "${_source_path}")
    string(REPLACE "/" "\\" _group_path "${_source_path}")
    source_group("${_group_path}" FILES "${_source}")
  endforeach()
  set_target_properties(blender_python_system_scripts PROPERTIES FOLDER "scripts")
  set_target_properties(blender_python_user_scripts PROPERTIES FOLDER "scripts")
  # Set the default debugging options for the project, only write this file once so the user
  # is free to override them at their own peril.
  set(USER_PROPS_FILE "${CMAKE_CURRENT_BINARY_DIR}/source/creator/blender.Cpp.user.props")
  if(NOT EXISTS ${USER_PROPS_FILE})
    # Layout below is messy, because otherwise the generated file will look messy.
    file(WRITE ${USER_PROPS_FILE} "<?xml version=\"1.0\" encoding=\"utf-8\"?>
<Project DefaultTargets=\"Build\" xmlns=\"http://schemas.microsoft.com/developer/msbuild/2003\">
  <PropertyGroup>
    <LocalDebuggerCommandArguments>-con --env-system-scripts \"${CMAKE_SOURCE_DIR}/scripts\" </LocalDebuggerCommandArguments>
  </PropertyGroup>
</Project>")
  endif()
endif()

if(WITH_XR_OPENXR)
  set(XR_OPENXR_SDK ${LIBDIR}/xr_openxr_sdk)
  set(XR_OPENXR_SDK_LIBPATH ${LIBDIR}/xr_openxr_sdk/lib)
  set(XR_OPENXR_SDK_INCLUDE_DIR ${XR_OPENXR_SDK}/include)
  # This is the old name of this library, it is checked to
  # support the transition between the old and new lib versions
  # this can be removed after the next lib update.
  if(EXISTS ${XR_OPENXR_SDK_LIBPATH}/openxr_loader_d.lib)
    set(XR_OPENXR_SDK_LIBRARIES
      optimized ${XR_OPENXR_SDK_LIBPATH}/openxr_loader.lib
      debug ${XR_OPENXR_SDK_LIBPATH}/openxr_loader_d.lib
    )
  else()
    set(XR_OPENXR_SDK_LIBRARIES
      optimized ${XR_OPENXR_SDK_LIBPATH}/openxr_loader.lib
      debug ${XR_OPENXR_SDK_LIBPATH}/openxr_loaderd.lib
    )
  endif()
endif()

if(WITH_GMP)
  set(GMP_INCLUDE_DIRS ${LIBDIR}/gmp/include)
  if(EXISTS ${LIBDIR}/gmp/lib/gmp.dll.lib)
    set(GMP_DLL_LIB_NAME gmp.dll.lib)
  else()
    set(GMP_DLL_LIB_NAME libgmp-10.lib)
  endif()
  set(GMP_LIBRARIES ${LIBDIR}/gmp/lib/${GMP_DLL_LIB_NAME}
    optimized ${LIBDIR}/gmp/lib/libgmpxx.lib
    debug ${LIBDIR}/gmp/lib/libgmpxx_d.lib
  )
  set(GMP_ROOT_DIR ${LIBDIR}/gmp)
  set(GMP_FOUND ON)
endif()

if(WITH_POTRACE)
  set(POTRACE_INCLUDE_DIRS ${LIBDIR}/potrace/include)
  set(POTRACE_LIBRARIES ${LIBDIR}/potrace/lib/potrace.lib)
  set(POTRACE_FOUND ON)
endif()

if(WITH_HARU)
  set(HARU_FOUND ON)
  set(HARU_ROOT_DIR ${LIBDIR}/haru)
  set(HARU_INCLUDE_DIRS ${HARU_ROOT_DIR}/include)
  if(EXISTS ${HARU_ROOT_DIR}/lib/hpdf.lib) # blender 5.0+
    set(HARU_LIBRARIES ${HARU_ROOT_DIR}/lib/hpdf.lib)
  else()
    set(HARU_LIBRARIES ${HARU_ROOT_DIR}/lib/libhpdfs.lib)
  endif()
endif()

if(WITH_VULKAN_BACKEND)
  if(EXISTS ${LIBDIR}/vulkan)
    set(VULKAN_FOUND ON)
    set(VULKAN_ROOT_DIR ${LIBDIR}/vulkan)
    set(VULKAN_INCLUDE_DIR ${VULKAN_ROOT_DIR}/include)
    set(VULKAN_INCLUDE_DIRS ${VULKAN_INCLUDE_DIR})
    set(VULKAN_LIBRARY ${VULKAN_ROOT_DIR}/lib/vulkan-1.lib)
    set(VULKAN_LIBRARIES ${VULKAN_LIBRARY})
  else()
    message(WARNING "Vulkan SDK was not found, disabling WITH_VULKAN_BACKEND")
    set(WITH_VULKAN_BACKEND OFF)
  endif()
endif()

if(WITH_VULKAN_BACKEND)
  if(EXISTS ${LIBDIR}/shaderc)
    set(SHADERC_FOUND ON)
    set(SHADERC_ROOT_DIR ${LIBDIR}/shaderc)
    set(SHADERC_INCLUDE_DIR ${SHADERC_ROOT_DIR}/include)
    set(SHADERC_INCLUDE_DIRS ${SHADERC_INCLUDE_DIR})
    set(SHADERC_LIBRARY
      ${SHADERC_ROOT_DIR}/lib/shaderc_shared.lib
    )
    set(SHADERC_LIBRARIES ${SHADERC_LIBRARY})
  else()
    message(WARNING "Shaderc was not found, disabling WITH_VULKAN_BACKEND")
    set(WITH_VULKAN_BACKEND OFF)
  endif()
endif()

if(WITH_CYCLES AND WITH_CYCLES_PATH_GUIDING)
  find_package(openpgl QUIET)
  if(openpgl_FOUND)
    get_target_property(OPENPGL_LIBRARIES_RELEASE openpgl::openpgl LOCATION_RELEASE)
    get_target_property(OPENPGL_LIBRARIES_DEBUG openpgl::openpgl LOCATION_DEBUG)
    set(OPENPGL_LIBRARIES
      optimized ${OPENPGL_LIBRARIES_RELEASE}
      debug ${OPENPGL_LIBRARIES_DEBUG}
    )
    get_target_property(OPENPGL_INCLUDE_DIR openpgl::openpgl INTERFACE_INCLUDE_DIRECTORIES)
  else()
    set(WITH_CYCLES_PATH_GUIDING OFF)
    message(STATUS "OpenPGL not found, disabling WITH_CYCLES_PATH_GUIDING")
  endif()
endif()

set(ZSTD_INCLUDE_DIRS ${LIBDIR}/zstd/include)
set(ZSTD_LIBRARIES ${LIBDIR}/zstd/lib/zstd_static.lib)

if(WITH_CYCLES AND (WITH_CYCLES_DEVICE_ONEAPI OR (WITH_CYCLES_EMBREE AND EMBREE_SYCL_SUPPORT)))
  set(LEVEL_ZERO_ROOT_DIR ${LIBDIR}/level_zero)
  set(CYCLES_SYCL ${LIBDIR}/dpcpp CACHE PATH "Path to oneAPI DPC++ compiler")
  mark_as_advanced(CYCLES_SYCL)
  if(EXISTS ${CYCLES_SYCL} AND NOT SYCL_ROOT_DIR)
    set(SYCL_ROOT_DIR ${CYCLES_SYCL})
  endif()
  file(GLOB _sycl_runtime_libraries_glob
    ${SYCL_ROOT_DIR}/bin/sycl.dll
    ${SYCL_ROOT_DIR}/bin/sycl[0-9].dll
  )
  foreach(sycl_runtime_library IN LISTS _sycl_runtime_libraries_glob)
    string(REPLACE ".dll" "d.dll" sycl_runtime_library_debug ${sycl_runtime_library})
    list(APPEND _sycl_runtime_libraries RELEASE ${sycl_runtime_library})
    list(APPEND _sycl_runtime_libraries DEBUG ${sycl_runtime_library_debug})
  endforeach()
  unset(_sycl_runtime_libraries_glob)

  file(GLOB _sycl_unified_runtime_libraries_glob
    ${SYCL_ROOT_DIR}/bin/ur_*.dll
  )
  # Cycles doesn't currently support the OpenCL backend
  list(FILTER _sycl_unified_runtime_libraries_glob EXCLUDE REGEX "opencl")

  foreach(sycl_unified_runtime_library IN LISTS _sycl_unified_runtime_libraries_glob)
    # We do not know, which library we would discover first, debug or release, so we check for both.
    string(REPLACE ".dll" "d.dll" sycl_unified_runtime_library_debug ${sycl_unified_runtime_library})
    # In case we are processing release version (no "d.dll" to replace), then
    # sycl_unified_runtime_library_release will be identical to sycl_unified_runtime_library,
    # there is a safe guard against it below.
    string(REPLACE "d.dll" ".dll" sycl_unified_runtime_library_release ${sycl_unified_runtime_library})

    list(FIND _sycl_unified_runtime_libraries_glob ${sycl_unified_runtime_library_debug} debug_index)
    list(FIND _sycl_unified_runtime_libraries_glob ${sycl_unified_runtime_library_release} release_index)
    if(NOT debug_index EQUAL -1)
      set (sycl_unified_runtime_library_release ${sycl_unified_runtime_library})
    elseif(NOT release_index EQUAL -1 AND NOT sycl_unified_runtime_library_release STREQUAL sycl_unified_runtime_library)
      set (sycl_unified_runtime_library_debug ${sycl_unified_runtime_library})
    else()
      # If there is no debug pair version of the library, then we are assuming
      # that this dll dependency is unique, and should be just added as both
      # release and debug dependency.
      set (sycl_unified_runtime_library_release ${sycl_unified_runtime_library})
      set (sycl_unified_runtime_library_debug ${sycl_unified_runtime_library})
    endif()
    list(FIND _sycl_runtime_libraries ${sycl_unified_runtime_library_release} found_index)
    if (found_index EQUAL -1)
      list(APPEND _sycl_runtime_libraries RELEASE ${sycl_unified_runtime_library_release})
      list(APPEND _sycl_runtime_libraries DEBUG ${sycl_unified_runtime_library_debug})
      # NOTE(Sirgienko) Due to a bug in DPC++ runtime, in versions 6.2 and 6.3
      # at least, the debug builds need the release versions of the
      # unified-runtime adapters to be installed.
      list(APPEND _sycl_runtime_libraries DEBUG ${sycl_unified_runtime_library_release})
    endif()
  endforeach()
  unset(_sycl_unified_runtime_libraries_glob)

  list(APPEND PLATFORM_BUNDLED_LIBRARIES ${_sycl_runtime_libraries})
  unset(_sycl_runtime_libraries)

  set(SYCL_LIBRARIES
    optimized ${SYCL_LIBRARY}
    debug ${SYCL_LIBRARY_DEBUG}
  )
endif()

# Add the MSVC directory to the path so when building with ASAN enabled tools such as
# `msgfmt` which run before the install phase can find the asan shared libraries.
get_filename_component(_msvc_path ${CMAKE_C_COMPILER} DIRECTORY)
# Environment variables to run precompiled executables that needed libraries.
list(JOIN PLATFORM_BUNDLED_LIBRARY_DIRS ";" _library_paths)
set(PLATFORM_ENV_BUILD_DIRS "${_msvc_path}\;${LIBDIR}/epoxy/bin\;${LIBDIR}/tbb/bin\;${LIBDIR}/OpenImageIO/bin\;${LIBDIR}/boost/lib\;${LIBDIR}/openexr/bin\;${LIBDIR}/imath/bin\;${LIBDIR}/shaderc/bin\;${LIBDIR}/opencolorio/bin\;${LIBDIR}/aom/bin\;${LIBDIR}/openjph/bin\;${PATH}")
set(PLATFORM_ENV_BUILD "PATH=${PLATFORM_ENV_BUILD_DIRS}")
# Install needs the additional folders from PLATFORM_ENV_BUILD_DIRS as well, as tools like:
# `idiff` and `abcls` use the release mode dlls.
# Escape semicolons, since in cmake they denote elements in a list if surrounded by square brackets
string(REPLACE ";" "\\;" ESCAPED_PATH "$ENV{PATH}")
set(PLATFORM_ENV_INSTALL "PATH=${CMAKE_INSTALL_PREFIX_WITH_CONFIG}/blender.shared/\;${PLATFORM_ENV_BUILD_DIRS}\;${ESCAPED_PATH}")
unset(_library_paths)
unset(_msvc_path)
