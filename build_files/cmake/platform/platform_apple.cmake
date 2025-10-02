# SPDX-FileCopyrightText: 2016 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

# Libraries configuration for Apple.

macro(find_package_wrapper)
  # do nothing, just satisfy the macro
endmacro()

function(print_found_status
  lib_name
  lib_path
  )

  if(FIRST_RUN)
    if(lib_path)
      message(STATUS "Found ${lib_name}: ${lib_path}")
    else()
      message(WARNING "Could NOT find ${lib_name}")
    endif()
  endif()
endfunction()

# ------------------------------------------------------------------------
# Find system provided libraries.

# Find system ZLIB
set(ZLIB_ROOT /usr)
find_package(ZLIB REQUIRED)
find_package(BZip2 REQUIRED)
list(APPEND ZLIB_LIBRARIES ${BZIP2_LIBRARIES})

if(WITH_OPENAL)
  find_package(OpenAL REQUIRED)
endif()

if(WITH_JACK)
  find_library(JACK_FRAMEWORK
    NAMES jackmp
  )

  if(JACK_FRAMEWORK)
    set(JACK_INCLUDE_DIRS ${JACK_FRAMEWORK}/headers)
  else()
    set_and_warn_library_found("JACK" JACK_FRAMEWORK WITH_JACK)
  endif()
endif()

if(NOT DEFINED LIBDIR)
  if("${CMAKE_OSX_ARCHITECTURES}" STREQUAL "x86_64")
    set(LIBDIR ${CMAKE_SOURCE_DIR}/lib/macos_x64)
  else()
    set(LIBDIR ${CMAKE_SOURCE_DIR}/lib/macos_${CMAKE_OSX_ARCHITECTURES})
  endif()
endif()
if(NOT EXISTS "${LIBDIR}/.git")
  message(FATAL_ERROR "Mac OSX requires pre-compiled libs at: '${LIBDIR}'")
endif()
if(FIRST_RUN)
  message(STATUS "Using pre-compiled LIBDIR: ${LIBDIR}")
endif()

# Avoid searching for headers since this would otherwise override our lib
# directory as well as PYTHON_ROOT_DIR.
set(CMAKE_FIND_FRAMEWORK NEVER)

# Optionally use system Python if PYTHON_ROOT_DIR is specified.
if(WITH_PYTHON)
  if(WITH_PYTHON_MODULE AND PYTHON_ROOT_DIR)
    find_package(PythonLibsUnix REQUIRED)
  endif()
else()
  # Python executable is needed as part of the build-process,
  # note that building without Python is quite unusual.
  find_program(PYTHON_EXECUTABLE "python3")
endif()

# Prefer lib directory paths
file(GLOB LIB_SUBDIRS ${LIBDIR}/*)
set(CMAKE_PREFIX_PATH ${LIB_SUBDIRS})

# -------------------------------------------------------------------------
# Find precompiled libraries, and avoid system or user-installed ones.

if(EXISTS ${LIBDIR})
  include(platform_old_libs_update)
  without_system_libs_begin()
endif()

if(WITH_ALEMBIC)
  find_package(Alembic)
endif()

if(WITH_USD)
  find_package(USD REQUIRED)
endif()
add_bundled_libraries(usd/lib)

if(WITH_MATERIALX)
  find_package(MaterialX)
  set_and_warn_library_found("MaterialX" MaterialX_FOUND WITH_MATERIALX)
endif()
add_bundled_libraries(materialx/lib)

if(WITH_OPENSUBDIV)
  find_package(OpenSubdiv)
endif()
add_bundled_libraries(opensubdiv/lib)

if(WITH_CODEC_SNDFILE)
  find_package(SndFile)
  find_library(_sndfile_FLAC_LIBRARY NAMES flac HINTS ${LIBDIR}/sndfile/lib)
  find_library(_sndfile_OGG_LIBRARY NAMES ogg HINTS ${LIBDIR}/ffmpeg/lib)
  find_library(_sndfile_VORBIS_LIBRARY NAMES vorbis HINTS ${LIBDIR}/ffmpeg/lib)
  find_library(_sndfile_VORBISENC_LIBRARY NAMES vorbisenc HINTS ${LIBDIR}/ffmpeg/lib)
  list(APPEND LIBSNDFILE_LIBRARIES
    ${_sndfile_FLAC_LIBRARY}
    ${_sndfile_OGG_LIBRARY}
    ${_sndfile_VORBIS_LIBRARY}
    ${_sndfile_VORBISENC_LIBRARY}
  )

  print_found_status("SndFile libraries" "${LIBSNDFILE_LIBRARIES}")
  unset(_sndfile_FLAC_LIBRARY)
  unset(_sndfile_OGG_LIBRARY)
  unset(_sndfile_VORBIS_LIBRARY)
  unset(_sndfile_VORBISENC_LIBRARY)
endif()

if(WITH_PYTHON)
  if(NOT (WITH_PYTHON_MODULE AND PYTHON_ROOT_DIR))
    find_package(PythonLibsUnix REQUIRED)
  endif()
endif()

if(WITH_FFTW3)
  find_package(Fftw3)
endif()

# FreeType compiled with Brotli compression for woff2.
find_package(Freetype REQUIRED)
set(BROTLI_LIBRARIES
  ${LIBDIR}/brotli/lib/libbrotlicommon-static.a
  ${LIBDIR}/brotli/lib/libbrotlidec-static.a
)

if(WITH_HARFBUZZ)
  find_package(Harfbuzz)
endif()

if(WITH_FRIBIDI)
  find_package(Fribidi)
endif()

# Header dependency of required OpenImageIO.
find_package(OpenEXR REQUIRED)
add_bundled_libraries(openexr/lib)
add_bundled_libraries(imath/lib)

string(APPEND PLATFORM_CFLAGS " -pipe -funsigned-char -fno-strict-aliasing -ffp-contract=off")
set(PLATFORM_LINKFLAGS
  "-fexceptions -framework CoreServices -framework Foundation -framework IOKit -framework AppKit -framework Cocoa \
   -framework Carbon -framework AudioUnit -framework AudioToolbox -framework CoreAudio -framework Metal \
   -framework QuartzCore"
)

if(WITH_CODEC_FFMPEG)
  set(FFMPEG_ROOT_DIR ${LIBDIR}/ffmpeg)
  set(FFMPEG_FIND_COMPONENTS
    avcodec avdevice avfilter avformat avutil
    mp3lame ogg opus swresample swscale
    theora theoradec theoraenc vorbis vorbisenc
    vorbisfile vpx x264)
  # Frameworks required by libavfilter, using legacy macOS CGL
  string(APPEND PLATFORM_LINKFLAGS " -framework CoreImage -framework OpenGL")
  if(EXISTS ${LIBDIR}/ffmpeg/lib/libaom.a)
    list(APPEND FFMPEG_FIND_COMPONENTS aom)
  endif()
  if(EXISTS ${LIBDIR}/ffmpeg/lib/libx265.a)
    list(APPEND FFMPEG_FIND_COMPONENTS x265)
  endif()
  if(EXISTS ${LIBDIR}/ffmpeg/lib/libxvidcore.a)
    list(APPEND FFMPEG_FIND_COMPONENTS xvidcore)
  endif()
  find_package(FFmpeg)
endif()

if(WITH_IMAGE_OPENJPEG OR WITH_CODEC_FFMPEG)
  # use openjpeg from libdir that is linked into ffmpeg
  find_package(OpenJPEG)
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

if(WITH_OPENIMAGEDENOISE)
  if("${CMAKE_OSX_ARCHITECTURES}" STREQUAL "arm64")
    # OpenImageDenoise uses BNNS from the Accelerate framework.
    string(APPEND PLATFORM_LINKFLAGS " -framework Accelerate")
  endif()
endif()

if(WITH_JACK)
  string(APPEND PLATFORM_LINKFLAGS " -F/Library/Frameworks -weak_framework jackmp")
endif()

if(WITH_SDL)
  find_package(SDL2)
  set(SDL_INCLUDE_DIR ${SDL2_INCLUDE_DIRS})
  set(SDL_LIBRARY ${SDL2_LIBRARIES})
  string(APPEND PLATFORM_LINKFLAGS " -framework ForceFeedback -framework GameController")
  if("${CMAKE_OSX_ARCHITECTURES}" STREQUAL "arm64")
    # The minimum macOS version of the libraries makes it so this is included in SDL on arm64
    # but not x86_64.
    string(APPEND PLATFORM_LINKFLAGS " -framework CoreHaptics")
  endif()
endif()

set(EPOXY_ROOT_DIR ${LIBDIR}/epoxy)
find_package(Epoxy REQUIRED)

set(PNG_ROOT ${LIBDIR}/png)
find_package(PNG REQUIRED)

set(JPEG_ROOT ${LIBDIR}/jpeg)
find_package(JPEG REQUIRED)

set(TIFF_ROOT ${LIBDIR}/tiff)
find_package(TIFF REQUIRED)

if(WITH_IMAGE_WEBP)
  set(WEBP_ROOT_DIR ${LIBDIR}/webp)
  find_package(WebP REQUIRED)
endif()

# With Blender 4.4 libraries there is no more Boost. This code is only
# here until we can reasonably assume everyone has upgraded to them.
if(WITH_BOOST)
  if(DEFINED LIBDIR AND NOT EXISTS "${LIBDIR}/boost")
    set(WITH_BOOST OFF)
    set(BOOST_LIBRARIES)
    set(BOOST_PYTHON_LIBRARIES)
    set(BOOST_INCLUDE_DIR)
  endif()
endif()

if(WITH_BOOST)
  set(Boost_NO_BOOST_CMAKE ON)
  set(Boost_ROOT ${LIBDIR}/boost)
  set(Boost_NO_SYSTEM_PATHS ON)
  set(_boost_FIND_COMPONENTS)
  if(WITH_USD AND USD_PYTHON_SUPPORT)
    list(APPEND _boost_FIND_COMPONENTS python${PYTHON_VERSION_NO_DOTS})
  endif()
  set(Boost_NO_WARN_NEW_VERSIONS ON)
  find_package(Boost COMPONENTS ${_boost_FIND_COMPONENTS})

  # Boost Python is the only library Blender directly depends on, though USD headers.
  if(WITH_USD AND USD_PYTHON_SUPPORT)
    set(BOOST_PYTHON_LIBRARIES ${Boost_PYTHON${PYTHON_VERSION_NO_DOTS}_LIBRARY})
  endif()
  set(BOOST_INCLUDE_DIR ${Boost_INCLUDE_DIRS})
  set(BOOST_DEFINITIONS)

  mark_as_advanced(Boost_LIBRARIES)
  mark_as_advanced(Boost_INCLUDE_DIRS)
  unset(_boost_FIND_COMPONENTS)
endif()
add_bundled_libraries(boost/lib)

if(WITH_CODEC_FFMPEG)
  string(APPEND PLATFORM_LINKFLAGS " -liconv") # ffmpeg needs it !
endif()

if(WITH_PUGIXML)
  find_package(PugiXML REQUIRED)
endif()

find_package(OpenImageIO REQUIRED)
add_bundled_libraries(openimageio/lib)

if(WITH_OPENCOLORIO)
  find_package(OpenColorIO 2.0.0 REQUIRED)
endif()
add_bundled_libraries(opencolorio/lib)

if(WITH_OPENVDB)
  find_package(OpenVDB)
  find_library(BLOSC_LIBRARIES NAMES blosc HINTS ${LIBDIR}/openvdb/lib)
  if(BLOSC_LIBRARIES)
    list(APPEND OPENVDB_LIBRARIES ${BLOSC_LIBRARIES})
  else()
    unset(BLOSC_LIBRARIES CACHE)
  endif()
  set(OPENVDB_DEFINITIONS)
endif()
add_bundled_libraries(openvdb/lib)

if(WITH_NANOVDB)
  find_package(NanoVDB)
endif()

if(WITH_CPU_SIMD AND SUPPORT_NEON_BUILD)
  find_package(sse2neon)
endif()

if(WITH_LLVM)
  find_package(LLVM)
  if(NOT LLVM_FOUND)
    message(FATAL_ERROR "LLVM not found.")
  endif()
  if(WITH_CLANG)
    find_package(Clang)
    if(NOT CLANG_FOUND)
      message(FATAL_ERROR "Clang not found.")
    endif()
  endif()

endif()

if(WITH_CYCLES AND WITH_CYCLES_OSL)
  find_package(OSL 1.13.4 REQUIRED)
endif()
add_bundled_libraries(osl/lib)

if(WITH_CYCLES AND WITH_CYCLES_EMBREE)
  find_package(Embree 4.0.0 REQUIRED)
endif()
add_bundled_libraries(embree/lib)

if(WITH_OPENIMAGEDENOISE)
  find_package(OpenImageDenoise REQUIRED)
  add_bundled_libraries(openimagedenoise/lib)
endif()

if(WITH_TBB)
  find_package(TBB 2021.13.0 REQUIRED)
  if(TBB_FOUND)
    get_target_property(TBB_LIBRARIES TBB::tbb LOCATION)
    get_target_property(TBB_INCLUDE_DIRS TBB::tbb INTERFACE_INCLUDE_DIRECTORIES)
  endif()
  set_and_warn_library_found("TBB" TBB_FOUND WITH_TBB)
endif()
add_bundled_libraries(tbb/lib)

if(WITH_POTRACE)
  find_package(Potrace REQUIRED)
endif()

if(WITH_XR_OPENXR)
  find_package(XR_OpenXR_SDK REQUIRED)
endif()

if(WITH_GMP)
  find_package(GMP REQUIRED)
endif()

if(WITH_HARU)
  find_package(Haru REQUIRED)
endif()

if(WITH_MANIFOLD)
  find_package(manifold REQUIRED)
endif()

if(WITH_RUBBERBAND)
  find_package(Rubberband REQUIRED)
endif()

if(WITH_CYCLES AND WITH_CYCLES_PATH_GUIDING)
  find_package(openpgl QUIET)
  if(openpgl_FOUND)
    get_target_property(OPENPGL_LIBRARIES openpgl::openpgl LOCATION)
    get_target_property(OPENPGL_INCLUDE_DIR openpgl::openpgl INTERFACE_INCLUDE_DIRECTORIES)
    message(STATUS "Found OpenPGL: ${OPENPGL_LIBRARIES}")
  else()
    set(WITH_CYCLES_PATH_GUIDING OFF)
    message(STATUS "OpenPGL not found, disabling WITH_CYCLES_PATH_GUIDING")
  endif()
endif()

set(ZSTD_ROOT_DIR ${LIBDIR}/zstd)
find_package(Zstd REQUIRED)

if(EXISTS ${LIBDIR})
  without_system_libs_end()
endif()

# Restore to default.
set(CMAKE_FIND_FRAMEWORK FIRST)

# ---------------------------------------------------------------------
# Set compiler and linker flags.

set(EXETYPE MACOSX_BUNDLE)

set(CMAKE_C_FLAGS_DEBUG "-g")
set(CMAKE_CXX_FLAGS_DEBUG "-g")
if(CMAKE_OSX_ARCHITECTURES MATCHES "x86_64" OR CMAKE_OSX_ARCHITECTURES MATCHES "i386")
  set(CMAKE_CXX_FLAGS_RELEASE "-O2 -mdynamic-no-pic -msse -msse2 -msse3 -mssse3")
  set(CMAKE_C_FLAGS_RELEASE "-O2 -mdynamic-no-pic  -msse -msse2 -msse3 -mssse3")
  if(NOT CMAKE_C_COMPILER_ID MATCHES "Clang")
    string(APPEND CMAKE_C_FLAGS_RELEASE " -ftree-vectorize  -fvariable-expansion-in-unroller")
    string(APPEND CMAKE_CXX_FLAGS_RELEASE " -ftree-vectorize  -fvariable-expansion-in-unroller")
  endif()
else()
  set(CMAKE_C_FLAGS_RELEASE "-O2 -mdynamic-no-pic")
  set(CMAKE_CXX_FLAGS_RELEASE "-O2 -mdynamic-no-pic")
endif()

# Clang has too low template depth of 128 for libmv.
string(APPEND CMAKE_CXX_FLAGS " -ftemplate-depth=1024")

# Avoid conflicts with Luxrender, and other plug-ins that may use the same
# libraries as Blender with a different version or build options.
set(PLATFORM_SYMBOLS_MAP ${CMAKE_SOURCE_DIR}/source/creator/symbols_apple.map)
string(APPEND PLATFORM_LINKFLAGS
  " -Wl,-unexported_symbols_list,'${PLATFORM_SYMBOLS_MAP}'"
)

if(${XCODE_VERSION} VERSION_GREATER_EQUAL 15.0)
  if("${CMAKE_OSX_ARCHITECTURES}" STREQUAL "x86_64" AND WITH_LEGACY_MACOS_X64_LINKER)
    # Silence "no platform load command found in <static library>, assuming: macOS".
    #
    # NOTE: Using ld_classic costs minutes of extra linking time.
    string(APPEND PLATFORM_LINKFLAGS " -Wl,-ld_classic")
  else()
    # Silence "ld: warning: ignoring duplicate libraries".
    #
    # The warning is introduced with Xcode 15 and is triggered when the same library
    # is passed to the linker multiple times. This situation could happen with either
    # cyclic libraries, or some transitive dependencies where CMake might decide to
    # pass library to the linker multiple times to force it re-scan symbols. It is
    # not necessary for Xcode linker to ensure all symbols from library are used and
    # it is corrected in CMake 3.29:
    #    https://gitlab.kitware.com/cmake/cmake/-/issues/25297
    string(APPEND PLATFORM_LINKFLAGS " -Xlinker -no_warn_duplicate_libraries")
  endif()
endif()

# Make stack size more similar to Embree, required for Embree.
string(APPEND PLATFORM_LINKFLAGS_EXECUTABLE " -Wl,-stack_size,0x100000")

# Suppress ranlib "has no symbols" warnings (workaround for #48250).
set(CMAKE_C_ARCHIVE_CREATE   "<CMAKE_AR> Scr <TARGET> <LINK_FLAGS> <OBJECTS>")
set(CMAKE_CXX_ARCHIVE_CREATE "<CMAKE_AR> Scr <TARGET> <LINK_FLAGS> <OBJECTS>")
# llvm-ranlib doesn't support this flag. Xcode's libtool does.
if(NOT ${CMAKE_RANLIB} MATCHES ".*llvm-ranlib$")
  set(CMAKE_C_ARCHIVE_FINISH   "<CMAKE_RANLIB> -no_warning_for_no_symbols -c <TARGET>")
  set(CMAKE_CXX_ARCHIVE_FINISH "<CMAKE_RANLIB> -no_warning_for_no_symbols -c <TARGET>")
endif()

if(WITH_COMPILER_CCACHE)
  if(NOT CMAKE_GENERATOR STREQUAL "Xcode")
    find_program(CCACHE_PROGRAM ccache)
    mark_as_advanced(CCACHE_PROGRAM)
    if(CCACHE_PROGRAM)
      # Makefiles and ninja
      set(CMAKE_C_COMPILER_LAUNCHER   "${CCACHE_PROGRAM}" CACHE STRING "" FORCE)
      set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}" CACHE STRING "" FORCE)
      mark_as_advanced(
        CMAKE_C_COMPILER_LAUNCHER
        CMAKE_CXX_COMPILER_LAUNCHER
      )
    else()
      message(WARNING "Ccache NOT found, disabling WITH_COMPILER_CCACHE")
      set(WITH_COMPILER_CCACHE OFF)
    endif()
  endif()
endif()

if(WITH_COMPILER_ASAN)
  list(APPEND PLATFORM_BUNDLED_LIBRARIES ${COMPILER_ASAN_LIBRARY})
endif()

if(PLATFORM_BUNDLED_LIBRARIES)
  # For the installed Python module and installed Blender executable, we set the
  # rpath to the location where install step will copy the shared libraries.
  set(CMAKE_SKIP_INSTALL_RPATH FALSE)
  if(WITH_PYTHON_MODULE)
    list(APPEND CMAKE_INSTALL_RPATH "@loader_path/lib")
  else()
    list(APPEND CMAKE_INSTALL_RPATH "@loader_path/../Resources/lib")
  endif()

  # For binaries that are built but not installed (like makesdan or tests), we add
  # the original directory of all shared libraries to the rpath. This is needed because
  # these can be in different folders, and because the build and install folder may be
  # different.
  set(CMAKE_SKIP_BUILD_RPATH FALSE)
  list(APPEND CMAKE_BUILD_RPATH ${PLATFORM_BUNDLED_LIBRARY_DIRS})

  # Environment variables to run precompiled executables that needed libraries.
  list(JOIN PLATFORM_BUNDLED_LIBRARY_DIRS ":" _library_paths)
  # Intentionally double "$$" which expands into "$" when instantiated.
  set(PLATFORM_ENV_BUILD "DYLD_LIBRARY_PATH=\"${_library_paths}:$$DYLD_LIBRARY_PATH\"")
  set(PLATFORM_ENV_INSTALL "DYLD_LIBRARY_PATH=${CMAKE_INSTALL_PREFIX_WITH_CONFIG}/Blender.app/Contents/Resources/lib/:$$DYLD_LIBRARY_PATH")
  unset(_library_paths)
endif()

# Same as `CFBundleIdentifier` in Info.plist.
set(CMAKE_XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "org.blenderfoundation.blender")
