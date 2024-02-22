# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright 2016 Blender Foundation

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

# Find system ZLIB, not the pre-compiled one supplied with OpenCollada.
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
if(WITH_PYTHON AND (WITH_PYTHON_MODULE AND PYTHON_ROOT_DIR))
  find_package(PythonLibsUnix REQUIRED)
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

if(WITH_VULKAN_BACKEND)
  find_package(MoltenVK REQUIRED)
  find_package(ShaderC REQUIRED)
  find_package(Vulkan REQUIRED)
endif()

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

if(WITH_PYTHON AND NOT (WITH_PYTHON_MODULE AND PYTHON_ROOT_DIR))
  find_package(PythonLibsUnix REQUIRED)
endif()

if(WITH_FFTW3)
  find_package(Fftw3)
endif()

# FreeType compiled with Brotli compression for woff2.
find_package(Freetype REQUIRED)
list(APPEND FREETYPE_LIBRARIES
  ${LIBDIR}/brotli/lib/libbrotlicommon-static.a
  ${LIBDIR}/brotli/lib/libbrotlidec-static.a)

if(WITH_IMAGE_OPENEXR)
  find_package(OpenEXR)
endif()
add_bundled_libraries(openexr/lib)
add_bundled_libraries(imath/lib)

if(WITH_CODEC_FFMPEG)
  set(FFMPEG_ROOT_DIR ${LIBDIR}/ffmpeg)
  set(FFMPEG_FIND_COMPONENTS
    avcodec avdevice avformat avutil
    mp3lame ogg opus swresample swscale
    theora theoradec theoraenc vorbis vorbisenc
    vorbisfile vpx x264 xvidcore)
  if(EXISTS ${LIBDIR}/ffmpeg/lib/libaom.a)
    list(APPEND FFMPEG_FIND_COMPONENTS aom)
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

string(APPEND PLATFORM_CFLAGS " -pipe -funsigned-char -fno-strict-aliasing -ffp-contract=off")
set(PLATFORM_LINKFLAGS
  "-fexceptions -framework CoreServices -framework Foundation -framework IOKit -framework AppKit -framework Cocoa -framework Carbon -framework AudioUnit -framework AudioToolbox -framework CoreAudio -framework Metal -framework QuartzCore"
)

list(APPEND PLATFORM_LINKLIBS c++)

if(WITH_OPENIMAGEDENOISE)
  if("${CMAKE_OSX_ARCHITECTURES}" STREQUAL "arm64")
    # OpenImageDenoise uses BNNS from the Accelerate framework.
    string(APPEND PLATFORM_LINKFLAGS " -framework Accelerate")
  endif()
endif()

if(WITH_JACK)
  string(APPEND PLATFORM_LINKFLAGS " -F/Library/Frameworks -weak_framework jackmp")
endif()

if(WITH_OPENCOLLADA)
  find_package(OpenCOLLADA)
  find_library(PCRE_LIBRARIES NAMES pcre HINTS ${LIBDIR}/opencollada/lib)
  find_library(XML2_LIBRARIES NAMES xml2 HINTS ${LIBDIR}/opencollada/lib)
  print_found_status("PCRE" "${PCRE_LIBRARIES}")
  print_found_status("XML2" "${XML2_LIBRARIES}")
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

if(WITH_BOOST)
  set(Boost_NO_BOOST_CMAKE ON)
  set(BOOST_ROOT ${LIBDIR}/boost)
  set(Boost_NO_SYSTEM_PATHS ON)
  set(_boost_FIND_COMPONENTS date_time filesystem regex system thread wave)
  if(WITH_INTERNATIONAL)
    list(APPEND _boost_FIND_COMPONENTS locale)
  endif()
  if(WITH_OPENVDB)
    list(APPEND _boost_FIND_COMPONENTS iostreams)
  endif()
  if(WITH_USD AND USD_PYTHON_SUPPORT)
    list(APPEND _boost_FIND_COMPONENTS python${PYTHON_VERSION_NO_DOTS})
  endif()
  set(Boost_NO_WARN_NEW_VERSIONS ON)
  find_package(Boost COMPONENTS ${_boost_FIND_COMPONENTS})

  # Boost Python is separate to avoid linking Python into tests that don't need it.
  set(BOOST_LIBRARIES ${Boost_LIBRARIES})
  if(WITH_USD AND USD_PYTHON_SUPPORT)
    set(BOOST_PYTHON_LIBRARIES ${Boost_PYTHON${PYTHON_VERSION_NO_DOTS}_LIBRARY})
    list(REMOVE_ITEM BOOST_LIBRARIES ${BOOST_PYTHON_LIBRARIES})
  endif()
  set(BOOST_INCLUDE_DIR ${Boost_INCLUDE_DIRS})
  set(BOOST_DEFINITIONS)

  mark_as_advanced(Boost_LIBRARIES)
  mark_as_advanced(Boost_INCLUDE_DIRS)
  unset(_boost_FIND_COMPONENTS)
endif()
add_bundled_libraries(boost/lib)

if(WITH_INTERNATIONAL OR WITH_CODEC_FFMPEG)
  string(APPEND PLATFORM_LINKFLAGS " -liconv") # boost_locale and ffmpeg needs it !
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
  find_package(OSL REQUIRED)
endif()

if(WITH_CYCLES AND WITH_CYCLES_EMBREE)
  find_package(Embree 3.8.0 REQUIRED)

  # Embree static library linking can mix up SSE and AVX symbols, causing
  # crashes on macOS systems with older CPUs that don't have AVX. Using
  # force load avoids that. The Embree shared library does not suffer from
  # this problem, precisely because linking a shared library uses force load.
  set(_embree_libraries_force_load)
  foreach(_embree_library ${EMBREE_LIBRARIES})
    list(APPEND _embree_libraries_force_load "-Wl,-force_load,${_embree_library}")
  endforeach()
  set(EMBREE_LIBRARIES ${_embree_libraries_force_load})
endif()
add_bundled_libraries(embree/lib)

if(WITH_OPENIMAGEDENOISE)
  find_package(OpenImageDenoise REQUIRED)
endif()

if(WITH_TBB)
  find_package(TBB REQUIRED)
endif()
add_bundled_libraries(tbb/lib)

if(WITH_POTRACE)
  find_package(Potrace REQUIRED)
endif()

# CMake FindOpenMP doesn't know about AppleClang before 3.12, so provide custom flags.
if(WITH_OPENMP)
  if(CMAKE_C_COMPILER_ID MATCHES "Clang")
    # Use OpenMP from our precompiled libraries.
    message(STATUS "Using ${LIBDIR}/openmp for OpenMP")
    set(OPENMP_CUSTOM ON)
    set(OPENMP_FOUND ON)
    set(OpenMP_C_FLAGS "-Xclang -fopenmp -I'${LIBDIR}/openmp/include'")
    set(OpenMP_CXX_FLAGS "-Xclang -fopenmp -I'${LIBDIR}/openmp/include'")
    set(OpenMP_LIBRARY_DIR "${LIBDIR}/openmp/lib/")
    set(OpenMP_LINKER_FLAGS "-L'${OpenMP_LIBRARY_DIR}' -lomp")
    set(OpenMP_LIBRARY "${OpenMP_LIBRARY_DIR}/libomp.dylib")
  endif()
endif()
add_bundled_libraries(openmp/lib)

if(WITH_XR_OPENXR)
  find_package(XR_OpenXR_SDK REQUIRED)
endif()

if(WITH_GMP)
  find_package(GMP REQUIRED)
endif()

if(WITH_HARU)
  find_package(Haru REQUIRED)
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

string(APPEND CMAKE_CXX_FLAGS " -stdlib=libc++")
string(APPEND PLATFORM_LINKFLAGS " -stdlib=libc++")

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
    if(CCACHE_PROGRAM)
      # Makefiles and ninja
      set(CMAKE_C_COMPILER_LAUNCHER   "${CCACHE_PROGRAM}" CACHE STRING "" FORCE)
      set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}" CACHE STRING "" FORCE)
    else()
      message(WARNING "Ccache NOT found, disabling WITH_COMPILER_CCACHE")
      set(WITH_COMPILER_CCACHE OFF)
    endif()
  endif()
endif()

unset(_custom_LINKER_FUSE_FLAG)
if(WITH_LINKER_LLD)
  find_program(LLD_PROGRAM ld.lld)
  if(LLD_PROGRAM)
    set(_custom_LINKER_FUSE_FLAG "-fuse-ld=lld")
  else()
    message(WARNING "LLD linker NOT found, disabling WITH_LINKER_LLD")
    set(WITH_LINKER_LLD OFF)
  endif()
endif()
if(WITH_LINKER_MOLD)
  find_program(MOLD_PROGRAM mold)
  if(MOLD_PROGRAM)
    set(_custom_LINKER_FUSE_FLAG "-fuse-ld=mold")
  else()
    message(WARNING "Mold linker NOT found, disabling WITH_LINKER_MOLD")
    set(WITH_LINKER_MOLD OFF)
  endif()
endif()

if(_custom_LINKER_FUSE_FLAG)
  add_link_options(${_custom_LINKER_FUSE_FLAG})
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
  set(PLATFORM_ENV_BUILD "DYLD_LIBRARY_PATH=\"${_library_paths};${DYLD_LIBRARY_PATH}\"")
  set(PLATFORM_ENV_INSTALL "DYLD_LIBRARY_PATH=${CMAKE_INSTALL_PREFIX_WITH_CONFIG}/Blender.app/Contents/Resources/lib/;$DYLD_LIBRARY_PATH")
  unset(_library_paths)
endif()

# Same as `CFBundleIdentifier` in Info.plist.
set(CMAKE_XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "org.blenderfoundation.blender")
