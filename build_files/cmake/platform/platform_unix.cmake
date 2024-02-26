# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright 2016 Blender Foundation. All rights reserved.

# Libraries configuration for any *nix system including Linux and Unix (excluding APPLE).

# Detect precompiled library directory
if(NOT DEFINED LIBDIR)
  # Path to a locally compiled libraries.
  set(LIBDIR_NAME ${CMAKE_SYSTEM_NAME}_${CMAKE_SYSTEM_PROCESSOR})
  string(TOLOWER ${LIBDIR_NAME} LIBDIR_NAME)
  set(LIBDIR_NATIVE_ABI ${CMAKE_SOURCE_DIR}/../lib/${LIBDIR_NAME})

  # Path to precompiled libraries with known CentOS 7 ABI.
  set(LIBDIR_CENTOS7_ABI ${CMAKE_SOURCE_DIR}/lib/linux_x64)

  # Choose the best suitable libraries.
  if(EXISTS ${LIBDIR_NATIVE_ABI})
    set(LIBDIR ${LIBDIR_NATIVE_ABI})
    set(WITH_LIBC_MALLOC_HOOK_WORKAROUND True)
  elseif(EXISTS ${LIBDIR_CENTOS7_ABI})
    set(LIBDIR ${LIBDIR_CENTOS7_ABI})
    set(WITH_CXX11_ABI OFF)
    if(WITH_MEM_JEMALLOC)
      # jemalloc provides malloc hooks.
      set(WITH_LIBC_MALLOC_HOOK_WORKAROUND False)
    else()
      set(WITH_LIBC_MALLOC_HOOK_WORKAROUND True)
    endif()

    if(CMAKE_COMPILER_IS_GNUCC AND
       CMAKE_C_COMPILER_VERSION VERSION_LESS 9.3)
      message(FATAL_ERROR "GCC version must be at least 9.3 for precompiled libraries, found ${CMAKE_C_COMPILER_VERSION}")
    endif()
  endif()

  # Avoid namespace pollustion.
  unset(LIBDIR_NATIVE_ABI)
  unset(LIBDIR_CENTOS7_ABI)
endif()

# Support restoring this value once pre-compiled libraries have been handled.
set(WITH_STATIC_LIBS_INIT ${WITH_STATIC_LIBS})

if(EXISTS ${LIBDIR})
  message(STATUS "Using pre-compiled LIBDIR: ${LIBDIR}")

  file(GLOB LIB_SUBDIRS ${LIBDIR}/*)

  # Ignore Mesa software OpenGL libraries, they are not intended to be
  # linked against but to optionally override at runtime.
  list(REMOVE_ITEM LIB_SUBDIRS ${LIBDIR}/mesa)

  # Ignore DPC++ as it contains its own copy of LLVM/CLang which we do
  # not need to be ever discovered for the Blender linking.
  list(REMOVE_ITEM LIB_SUBDIRS ${LIBDIR}/dpcpp)

  # NOTE: Make sure "proper" compiled zlib comes first before the one
  # which is a part of OpenCollada. They have different ABI, and we
  # do need to use the official one.
  set(CMAKE_PREFIX_PATH ${LIBDIR}/zlib ${LIB_SUBDIRS})

  include(platform_old_libs_update)

  set(WITH_STATIC_LIBS ON)
  # OpenMP usually can't be statically linked into shared libraries,
  # due to not being compiled with position independent code.
  if(NOT WITH_PYTHON_MODULE)
    set(WITH_OPENMP_STATIC ON)
  endif()
  set(Boost_NO_BOOST_CMAKE ON)
  set(BOOST_ROOT ${LIBDIR}/boost)
  set(BOOST_LIBRARYDIR ${LIBDIR}/boost/lib)
  set(Boost_NO_SYSTEM_PATHS ON)
  set(OPENEXR_ROOT_DIR ${LIBDIR}/openexr)
  set(CLANG_ROOT_DIR ${LIBDIR}/llvm)
endif()

if(WITH_STATIC_LIBS)
  string(APPEND CMAKE_EXE_LINKER_FLAGS " -static-libstdc++")
endif()

# Wrapper to prefer static libraries
macro(find_package_wrapper)
  if(WITH_STATIC_LIBS)
    find_package_static(${ARGV})
  else()
    find_package(${ARGV})
  endif()
endmacro()

# ----------------------------------------------------------------------------
# Precompiled Libraries
#
# These are libraries that may be precompiled. For this we disable searching in
# the system directories so that we don't accidentally use them instead.

if(EXISTS ${LIBDIR})
  without_system_libs_begin()
endif()

find_package_wrapper(JPEG REQUIRED)
find_package_wrapper(PNG REQUIRED)
find_package_wrapper(ZLIB REQUIRED)
find_package_wrapper(Zstd REQUIRED)

if(NOT WITH_SYSTEM_FREETYPE)
  # FreeType compiled with Brotli compression for woff2.
  find_package_wrapper(Freetype REQUIRED)
  if(EXISTS ${LIBDIR})
    find_package_wrapper(Brotli REQUIRED)

    # NOTE: This is done on WIN32 & APPLE but fails on some Linux systems.
    # See: https://devtalk.blender.org/t/22536
    # So `BROTLI_LIBRARIES` need to be added directly after `FREETYPE_LIBRARIES`.
    #
    # list(APPEND FREETYPE_LIBRARIES
    #   ${BROTLI_LIBRARIES}
    # )
  endif()
endif()

if(WITH_PYTHON)
  # No way to set py35, remove for now.
  # find_package(PythonLibs)

  # Use our own instead, since without py is such a rare case,
  # require this package
  # XXX Linking errors with debian static python :/
#       find_package_wrapper(PythonLibsUnix REQUIRED)
  find_package(PythonLibsUnix REQUIRED)
endif()

if(WITH_IMAGE_OPENEXR)
  find_package_wrapper(OpenEXR)  # our own module
  if(NOT OPENEXR_FOUND)
    set(WITH_IMAGE_OPENEXR OFF)
  endif()
endif()

if(WITH_IMAGE_OPENJPEG)
  find_package_wrapper(OpenJPEG)
  if(NOT OPENJPEG_FOUND)
    set(WITH_IMAGE_OPENJPEG OFF)
  endif()
endif()

if(WITH_IMAGE_TIFF)
  # XXX Linking errors with debian static tiff :/
#       find_package_wrapper(TIFF)
  find_package(TIFF)
  if(NOT TIFF_FOUND)
    set(WITH_IMAGE_TIFF OFF)
  endif()
endif()

if(WITH_OPENAL)
  find_package_wrapper(OpenAL)
  if(NOT OPENAL_FOUND)
    set(WITH_OPENAL OFF)
  endif()
endif()

if(WITH_SDL)
  if(WITH_SDL_DYNLOAD)
    set(SDL_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/extern/sdlew/include/SDL2")
    set(SDL_LIBRARY)
  else()
    find_package_wrapper(SDL2)
    if(SDL2_FOUND)
      # Use same names for both versions of SDL until we move to 2.x.
      set(SDL_INCLUDE_DIR "${SDL2_INCLUDE_DIR}")
      set(SDL_LIBRARY "${SDL2_LIBRARY}")
      set(SDL_FOUND "${SDL2_FOUND}")
    else()
      find_package_wrapper(SDL)
    endif()
    mark_as_advanced(
      SDL_INCLUDE_DIR
      SDL_LIBRARY
    )
    # unset(SDLMAIN_LIBRARY CACHE)
    if(NOT SDL_FOUND)
      set(WITH_SDL OFF)
    endif()
  endif()
endif()

# Codecs
if(WITH_CODEC_SNDFILE)
  find_package_wrapper(SndFile)
  if(NOT SNDFILE_FOUND)
    set(WITH_CODEC_SNDFILE OFF)
  endif()
endif()

if(WITH_CODEC_FFMPEG)
  if(EXISTS ${LIBDIR})
    set(FFMPEG_ROOT_DIR ${LIBDIR}/ffmpeg)
    # Override FFMPEG components to also include static library dependencies
    # included with precompiled libraries, and to ensure correct link order.
    set(FFMPEG_FIND_COMPONENTS
      avformat avcodec avdevice avutil swresample swscale
      sndfile
      FLAC
      mp3lame
      opus
      theora theoradec theoraenc
      vorbis vorbisenc vorbisfile ogg
      vpx
      x264
      xvidcore)
    if(EXISTS ${LIBDIR}/ffmpeg/lib/libaom.a)
      list(APPEND FFMPEG_FIND_COMPONENTS aom)
    endif()
  elseif(FFMPEG)
    # Old cache variable used for root dir, convert to new standard.
    set(FFMPEG_ROOT_DIR ${FFMPEG})
  endif()
  find_package(FFmpeg)

  if(NOT FFMPEG_FOUND)
    set(WITH_CODEC_FFMPEG OFF)
    message(STATUS "FFmpeg not found, disabling it")
  endif()
endif()

if(WITH_FFTW3)
  find_package_wrapper(Fftw3)
  if(NOT FFTW3_FOUND)
    set(WITH_FFTW3 OFF)
  endif()
endif()

if(WITH_OPENCOLLADA)
  find_package_wrapper(OpenCOLLADA)
  if(OPENCOLLADA_FOUND)
    if(WITH_STATIC_LIBS)
      # PCRE is bundled with OpenCollada without headers, so can't use
      # find_package reliably to detect it.
      set(PCRE_LIBRARIES ${LIBDIR}/opencollada/lib/libpcre.a)
    else()
      find_package_wrapper(PCRE)
    endif()
    find_package_wrapper(XML2)
  else()
    set(WITH_OPENCOLLADA OFF)
  endif()
endif()

if(WITH_MEM_JEMALLOC)
  find_package_wrapper(JeMalloc)
  if(NOT JEMALLOC_FOUND)
    set(WITH_MEM_JEMALLOC OFF)
  endif()
endif()

if(WITH_INPUT_NDOF)
  find_package_wrapper(Spacenav)
  if(SPACENAV_FOUND)
    # use generic names within blenders buildsystem.
    set(NDOF_INCLUDE_DIRS ${SPACENAV_INCLUDE_DIRS})
    set(NDOF_LIBRARIES ${SPACENAV_LIBRARIES})
  else()
    set(WITH_INPUT_NDOF OFF)
  endif()
endif()

if(WITH_CYCLES AND WITH_CYCLES_OSL)
  set(CYCLES_OSL ${LIBDIR}/osl CACHE PATH "Path to OpenShadingLanguage installation")
  if(EXISTS ${CYCLES_OSL} AND NOT OSL_ROOT)
    set(OSL_ROOT ${CYCLES_OSL})
  endif()
  find_package_wrapper(OSL)
  if(OSL_FOUND)
    if(${OSL_LIBRARY_VERSION_MAJOR} EQUAL "1" AND ${OSL_LIBRARY_VERSION_MINOR} LESS "6")
      # Note: --whole-archive is needed to force loading of all symbols in liboslexec,
      # otherwise LLVM is missing the osl_allocate_closure_component function
      set(OSL_LIBRARIES
        ${OSL_OSLCOMP_LIBRARY}
        -Wl,--whole-archive ${OSL_OSLEXEC_LIBRARY}
        -Wl,--no-whole-archive ${OSL_OSLQUERY_LIBRARY}
      )
    endif()
  else()
    message(STATUS "OSL not found, disabling it from Cycles")
    set(WITH_CYCLES_OSL OFF)
  endif()
endif()

if(WITH_CYCLES_DEVICE_ONEAPI)
  set(CYCLES_LEVEL_ZERO ${LIBDIR}/level-zero CACHE PATH "Path to Level Zero installation")
  if(EXISTS ${CYCLES_LEVEL_ZERO} AND NOT LEVEL_ZERO_ROOT_DIR)
    set(LEVEL_ZERO_ROOT_DIR ${CYCLES_LEVEL_ZERO})
  endif()

  set(CYCLES_SYCL ${LIBDIR}/dpcpp CACHE PATH "Path to DPC++ and SYCL installation")
  if(EXISTS ${CYCLES_SYCL} AND NOT SYCL_ROOT_DIR)
    set(SYCL_ROOT_DIR ${CYCLES_SYCL})
  endif()
endif()

if(WITH_OPENVDB)
  find_package_wrapper(OpenVDB)
  find_package_wrapper(Blosc)

  if(NOT OPENVDB_FOUND)
    set(WITH_OPENVDB OFF)
    set(WITH_OPENVDB_BLOSC OFF)
    message(STATUS "OpenVDB not found, disabling it")
  elseif(NOT BLOSC_FOUND)
    set(WITH_OPENVDB_BLOSC OFF)
    message(STATUS "Blosc not found, disabling it for OpenVBD")
  endif()
endif()

if(WITH_NANOVDB)
  find_package_wrapper(NanoVDB)

  if(NOT NANOVDB_FOUND)
    set(WITH_NANOVDB OFF)
    message(STATUS "NanoVDB not found, disabling it")
  endif()
endif()

if(WITH_CPU_SIMD AND SUPPORT_NEON_BUILD)
  find_package_wrapper(sse2neon)
endif()

if(WITH_ALEMBIC)
  find_package_wrapper(Alembic)

  if(NOT ALEMBIC_FOUND)
    set(WITH_ALEMBIC OFF)
  endif()
endif()

if(WITH_USD)
  find_package_wrapper(USD)

  if(NOT USD_FOUND)
    set(WITH_USD OFF)
  endif()
endif()

if(WITH_BOOST)
  # uses in build instructions to override include and library variables
  if(NOT BOOST_CUSTOM)
    if(WITH_STATIC_LIBS)
      set(Boost_USE_STATIC_LIBS ON)
    endif()
    set(Boost_USE_MULTITHREADED ON)
    set(__boost_packages filesystem regex thread date_time)
    if(WITH_CYCLES AND WITH_CYCLES_OSL)
      if(NOT (${OSL_LIBRARY_VERSION_MAJOR} EQUAL "1" AND ${OSL_LIBRARY_VERSION_MINOR} LESS "6"))
        list(APPEND __boost_packages wave)
      else()
      endif()
    endif()
    if(WITH_INTERNATIONAL)
      list(APPEND __boost_packages locale)
    endif()
    if(WITH_OPENVDB)
      list(APPEND __boost_packages iostreams)
    endif()
    list(APPEND __boost_packages system)
    find_package(Boost 1.48 COMPONENTS ${__boost_packages})
    if(NOT Boost_FOUND)
      # try to find non-multithreaded if -mt not found, this flag
      # doesn't matter for us, it has nothing to do with thread
      # safety, but keep it to not disturb build setups
      set(Boost_USE_MULTITHREADED OFF)
      find_package(Boost 1.48 COMPONENTS ${__boost_packages})
    endif()
    unset(__boost_packages)
    if(Boost_USE_STATIC_LIBS AND WITH_BOOST_ICU)
      find_package(IcuLinux)
    endif()
    mark_as_advanced(Boost_DIR)  # why doesn't boost do this?
    mark_as_advanced(Boost_INCLUDE_DIR)  # why doesn't boost do this?
  endif()

  set(BOOST_INCLUDE_DIR ${Boost_INCLUDE_DIRS})
  set(BOOST_LIBRARIES ${Boost_LIBRARIES})
  set(BOOST_LIBPATH ${Boost_LIBRARY_DIRS})
  set(BOOST_DEFINITIONS "-DBOOST_ALL_NO_LIB")

  if(Boost_USE_STATIC_LIBS AND WITH_BOOST_ICU)
    find_package(IcuLinux)
    list(APPEND BOOST_LIBRARIES ${ICU_LIBRARIES})
  endif()
endif()

if(WITH_PUGIXML)
  find_package_wrapper(PugiXML)

  if(NOT PUGIXML_FOUND)
    set(WITH_PUGIXML OFF)
    message(STATUS "PugiXML not found, disabling WITH_PUGIXML")
  endif()
endif()

if(WITH_IMAGE_WEBP)
  set(WEBP_ROOT_DIR ${LIBDIR}/webp)
  find_package_wrapper(WebP)
  if(NOT WEBP_FOUND)
    set(WITH_IMAGE_WEBP OFF)
    message(WARNING "WebP not found, disabling WITH_IMAGE_WEBP")
  endif()
endif()

if(WITH_OPENIMAGEIO)
  find_package_wrapper(OpenImageIO)
  set(OPENIMAGEIO_LIBRARIES
    ${OPENIMAGEIO_LIBRARIES}
    ${PNG_LIBRARIES}
    ${JPEG_LIBRARIES}
    ${ZLIB_LIBRARIES}
    ${BOOST_LIBRARIES}
  )
  set(OPENIMAGEIO_LIBPATH)  # TODO, remove and reference the absolute path everywhere
  set(OPENIMAGEIO_DEFINITIONS "")

  if(WITH_IMAGE_TIFF)
    list(APPEND OPENIMAGEIO_LIBRARIES "${TIFF_LIBRARY}")
  endif()
  if(WITH_IMAGE_OPENEXR)
    list(APPEND OPENIMAGEIO_LIBRARIES "${OPENEXR_LIBRARIES}")
  endif()
  if(WITH_IMAGE_WEBP)
    list(APPEND OPENIMAGEIO_LIBRARIES "${WEBP_LIBRARIES}")
  endif()

  if(NOT OPENIMAGEIO_FOUND)
    set(WITH_OPENIMAGEIO OFF)
    message(STATUS "OpenImageIO not found, disabling WITH_CYCLES")
  endif()
endif()

if(WITH_OPENCOLORIO)
  find_package_wrapper(OpenColorIO 2.0.0)

  set(OPENCOLORIO_LIBRARIES ${OPENCOLORIO_LIBRARIES})
  set(OPENCOLORIO_LIBPATH)  # TODO, remove and reference the absolute path everywhere
  set(OPENCOLORIO_DEFINITIONS)

  if(NOT OPENCOLORIO_FOUND)
    set(WITH_OPENCOLORIO OFF)
    message(STATUS "OpenColorIO not found")
  endif()
endif()

if(WITH_CYCLES AND WITH_CYCLES_EMBREE)
  find_package(Embree 3.8.0 REQUIRED)
endif()

if(WITH_OPENIMAGEDENOISE)
  find_package_wrapper(OpenImageDenoise)

  if(NOT OPENIMAGEDENOISE_FOUND)
    set(WITH_OPENIMAGEDENOISE OFF)
    message(STATUS "OpenImageDenoise not found")
  endif()
endif()

if(WITH_LLVM)
  if(EXISTS ${LIBDIR})
    set(LLVM_STATIC ON)
  endif()

  find_package_wrapper(LLVM)
  if(WITH_CLANG)
    find_package_wrapper(Clang)
  endif()
  # Symbol conflicts with same UTF library used by OpenCollada
  if(EXISTS ${LIBDIR})
    if(WITH_OPENCOLLADA AND (${LLVM_VERSION} VERSION_LESS "4.0.0"))
      list(REMOVE_ITEM OPENCOLLADA_LIBRARIES ${OPENCOLLADA_UTF_LIBRARY})
    endif()
  endif()

  if(NOT LLVM_FOUND)
    set(WITH_LLVM OFF)
    set(WITH_CLANG OFF)
    message(STATUS "LLVM not found")
  else()
    if(NOT CLANG_FOUND)
      set(WITH_CLANG OFF)
      message(STATUS "Clang not found")
    endif()
  endif()
endif()

if(WITH_OPENSUBDIV)
  find_package_wrapper(OpenSubdiv)

  set(OPENSUBDIV_LIBRARIES ${OPENSUBDIV_LIBRARIES})
  set(OPENSUBDIV_LIBPATH)  # TODO, remove and reference the absolute path everywhere

  if(NOT OPENSUBDIV_FOUND)
    set(WITH_OPENSUBDIV OFF)
    message(STATUS "OpenSubdiv not found")
  endif()
endif()

if(WITH_TBB)
  find_package_wrapper(TBB)
  if(NOT TBB_FOUND)
    message(WARNING "TBB not found, disabling WITH_TBB")
    set(WITH_TBB OFF)
  endif()
endif()

if(WITH_XR_OPENXR)
  find_package(XR_OpenXR_SDK)
  if(NOT XR_OPENXR_SDK_FOUND)
    message(WARNING "OpenXR-SDK not found, disabling WITH_XR_OPENXR")
    set(WITH_XR_OPENXR OFF)
  endif()
endif()

if(WITH_GMP)
  find_package_wrapper(GMP)
  if(NOT GMP_FOUND)
    message(WARNING "GMP not found, disabling WITH_GMP")
    set(WITH_GMP OFF)
  endif()
endif()

if(WITH_POTRACE)
  find_package_wrapper(Potrace)
  if(NOT POTRACE_FOUND)
    message(WARNING "potrace not found, disabling WITH_POTRACE")
    set(WITH_POTRACE OFF)
  endif()
endif()

if(WITH_HARU)
  find_package_wrapper(Haru)
  if(NOT HARU_FOUND)
    message(WARNING "Haru not found, disabling WITH_HARU")
    set(WITH_HARU OFF)
  endif()
endif()

if(EXISTS ${LIBDIR})
  without_system_libs_end()
endif()

# ----------------------------------------------------------------------------
# Build and Link Flags

# OpenSuse needs lutil, ArchLinux not, for now keep, can avoid by using --as-needed
if(HAIKU)
  list(APPEND PLATFORM_LINKLIBS -lnetwork)
else()
  list(APPEND PLATFORM_LINKLIBS -lutil -lc -lm)
endif()

find_package(Threads REQUIRED)
list(APPEND PLATFORM_LINKLIBS ${CMAKE_THREAD_LIBS_INIT})
# used by other platforms
set(PTHREADS_LIBRARIES ${CMAKE_THREAD_LIBS_INIT})

if(CMAKE_DL_LIBS)
  list(APPEND PLATFORM_LINKLIBS ${CMAKE_DL_LIBS})
endif()

if(CMAKE_SYSTEM_NAME MATCHES "Linux")
  if(NOT WITH_PYTHON_MODULE)
    # binreloc is linux only
    set(BINRELOC_INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/extern/binreloc/include)
    set(WITH_BINRELOC ON)
  endif()
endif()

# lfs on glibc, all compilers should use
add_definitions(-D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64 -D_LARGEFILE64_SOURCE)

# ----------------------------------------------------------------------------
# System Libraries
#
# Keep last, so indirectly linked libraries don't override our own pre-compiled libs.

if(EXISTS ${LIBDIR})
  # Clear the prefix path as it causes the `LIBDIR` to override system locations.
  unset(CMAKE_PREFIX_PATH)

  # Since the pre-compiled `LIBDIR` directories have been handled, don't prefer static libraries.
  set(WITH_STATIC_LIBS ${WITH_STATIC_LIBS_INIT})
endif()

if(WITH_SYSTEM_FREETYPE)
  find_package_wrapper(Freetype)
  if(NOT FREETYPE_FOUND)
    message(FATAL_ERROR "Failed finding system FreeType version!")
  endif()
endif()

if(WITH_LZO AND WITH_SYSTEM_LZO)
  find_package_wrapper(LZO)
  if(NOT LZO_FOUND)
    message(FATAL_ERROR "Failed finding system LZO version!")
  endif()
endif()

if(WITH_SYSTEM_EIGEN3)
  find_package_wrapper(Eigen3)
  if(NOT EIGEN3_FOUND)
    message(FATAL_ERROR "Failed finding system Eigen3 version!")
  endif()
endif()

# Jack is intended to use the system library.
if(WITH_JACK)
  find_package_wrapper(Jack)
  if(NOT JACK_FOUND)
    set(WITH_JACK OFF)
  endif()
endif()

# Pulse is intended to use the system library.
if(WITH_PULSEAUDIO)
  find_package_wrapper(Pulse)
  if(NOT PULSE_FOUND)
    set(WITH_PULSEAUDIO OFF)
  endif()
endif()

# Audio IO
if(WITH_SYSTEM_AUDASPACE)
  find_package_wrapper(Audaspace)
  if(NOT AUDASPACE_FOUND OR NOT AUDASPACE_C_FOUND)
    message(FATAL_ERROR "Audaspace external library not found!")
  endif()
endif()

if(WITH_GHOST_WAYLAND)
  find_package(PkgConfig)
  pkg_check_modules(wayland-client REQUIRED wayland-client>=1.12)
  pkg_check_modules(wayland-egl REQUIRED wayland-egl)
  pkg_check_modules(wayland-scanner REQUIRED wayland-scanner)
  pkg_check_modules(xkbcommon REQUIRED xkbcommon)
  pkg_check_modules(wayland-cursor REQUIRED wayland-cursor)

  if(WITH_GHOST_WAYLAND_DBUS)
    pkg_check_modules(dbus REQUIRED dbus-1)
  endif()

  if(WITH_GHOST_WAYLAND_LIBDECOR)
    pkg_check_modules(libdecor REQUIRED libdecor-0>=0.1)
  endif()

  list(APPEND PLATFORM_LINKLIBS
    ${xkbcommon_LINK_LIBRARIES}
  )

  if(NOT WITH_GHOST_WAYLAND_DYNLOAD)
    list(APPEND PLATFORM_LINKLIBS
      ${wayland-client_LINK_LIBRARIES}
      ${wayland-egl_LINK_LIBRARIES}
      ${wayland-cursor_LINK_LIBRARIES}
    )
  endif()

  if(WITH_GHOST_WAYLAND_DBUS)
    list(APPEND PLATFORM_LINKLIBS
      ${dbus_LINK_LIBRARIES}
    )
    add_definitions(-DWITH_GHOST_WAYLAND_DBUS)
  endif()

  if(WITH_GHOST_WAYLAND_LIBDECOR)
    if(NOT WITH_GHOST_WAYLAND_DYNLOAD)
      list(APPEND PLATFORM_LINKLIBS
        ${libdecor_LIBRARIES}
      )
    endif()
    add_definitions(-DWITH_GHOST_WAYLAND_LIBDECOR)
  endif()
endif()

if(WITH_GHOST_X11)
  find_package(X11 REQUIRED)

  find_path(X11_XF86keysym_INCLUDE_PATH X11/XF86keysym.h ${X11_INC_SEARCH_PATH})
  mark_as_advanced(X11_XF86keysym_INCLUDE_PATH)

  list(APPEND PLATFORM_LINKLIBS ${X11_X11_LIB})

  if(WITH_X11_XINPUT)
    if(X11_Xinput_LIB)
      list(APPEND PLATFORM_LINKLIBS ${X11_Xinput_LIB})
    else()
      message(FATAL_ERROR "LibXi not found. Disable WITH_X11_XINPUT if you
      want to build without tablet support")
    endif()
  endif()

  if(WITH_X11_XF86VMODE)
    # XXX, why doesn't cmake make this available?
    find_library(X11_Xxf86vmode_LIB Xxf86vm   ${X11_LIB_SEARCH_PATH})
    mark_as_advanced(X11_Xxf86vmode_LIB)
    if(X11_Xxf86vmode_LIB)
      list(APPEND PLATFORM_LINKLIBS ${X11_Xxf86vmode_LIB})
    else()
      message(FATAL_ERROR "libXxf86vm not found. Disable WITH_X11_XF86VMODE if you
      want to build without")
    endif()
  endif()

  if(WITH_X11_XFIXES)
    if(X11_Xfixes_LIB)
      list(APPEND PLATFORM_LINKLIBS ${X11_Xfixes_LIB})
    else()
      message(FATAL_ERROR "libXfixes not found. Disable WITH_X11_XFIXES if you
      want to build without")
    endif()
  endif()

  if(WITH_X11_ALPHA)
    find_library(X11_Xrender_LIB Xrender  ${X11_LIB_SEARCH_PATH})
    mark_as_advanced(X11_Xrender_LIB)
    if(X11_Xrender_LIB)
      list(APPEND PLATFORM_LINKLIBS ${X11_Xrender_LIB})
    else()
      message(FATAL_ERROR "libXrender not found. Disable WITH_X11_ALPHA if you
      want to build without")
    endif()
  endif()

endif()

# ----------------------------------------------------------------------------
# Compilers

# Only set the linker once.
set(_IS_LINKER_DEFAULT ON)

# GNU Compiler
if(CMAKE_COMPILER_IS_GNUCC)
  # ffp-contract=off:
  # Automatically turned on when building with "-march=native". This is
  # explicitly turned off here as it will make floating point math give a bit
  # different results. This will lead to automated test failures. So disable
  # this until we support it.
  set(PLATFORM_CFLAGS "-pipe -fPIC -funsigned-char -fno-strict-aliasing -ffp-contract=off")

  # `maybe-uninitialized` is unreliable in release builds, but fine in debug builds.
  set(GCC_EXTRA_FLAGS_RELEASE "-Wno-maybe-uninitialized")
  set(CMAKE_C_FLAGS_RELEASE          "${GCC_EXTRA_FLAGS_RELEASE} ${CMAKE_C_FLAGS_RELEASE}")
  set(CMAKE_C_FLAGS_RELWITHDEBINFO   "${GCC_EXTRA_FLAGS_RELEASE} ${CMAKE_C_FLAGS_RELWITHDEBINFO}")
  set(CMAKE_CXX_FLAGS_RELEASE        "${GCC_EXTRA_FLAGS_RELEASE} ${CMAKE_CXX_FLAGS_RELEASE}")
  string(PREPEND CMAKE_CXX_FLAGS_RELWITHDEBINFO "${GCC_EXTRA_FLAGS_RELEASE} ")
  unset(GCC_EXTRA_FLAGS_RELEASE)

  # NOTE(@campbellbarton): Eventually mold will be able to use `-fuse-ld=mold`,
  # however at the moment this only works for GCC 12.1+ (unreleased at time of writing).
  # So a workaround is used here "-B" which points to another path to find system commands
  # such as `ld`.
  if(WITH_LINKER_MOLD AND _IS_LINKER_DEFAULT)
    find_program(MOLD_BIN "mold")
    mark_as_advanced(MOLD_BIN)
    if(NOT MOLD_BIN)
      message(STATUS "The \"mold\" binary could not be found, using system linker.")
      set(WITH_LINKER_MOLD OFF)
    else()
      # By default mold installs the binary to:
      # - `{PREFIX}/bin/mold` as well as a symbolic-link in...
      # - `{PREFIX}/lib/mold/ld`.
      # (where `PREFIX` is typically `/usr/`).
      #
      # This block of code finds `{PREFIX}/lib/mold` from the `mold` binary.
      # Other methods of searching for the path could also be made to work,
      # we could even make our own directory and symbolic-link, however it's more
      # convenient to use the one provided by mold.
      #
      # Use the binary path to "mold", to find the common prefix which contains "lib/mold".
      # The parent directory: e.g. `/usr/bin/mold` -> `/usr/bin/`.
      get_filename_component(MOLD_PREFIX "${MOLD_BIN}" DIRECTORY)
      # The common prefix path: e.g. `/usr/bin/` -> `/usr/` to use as a hint.
      get_filename_component(MOLD_PREFIX "${MOLD_PREFIX}" DIRECTORY)
      # Find `{PREFIX}/lib/mold/ld`, store the directory component (without the `ld`).
      # Then pass `-B {PREFIX}/lib/mold` to GCC so the `ld` located there overrides the default.
      find_path(
        MOLD_BIN_DIR "ld"
        HINTS "${MOLD_PREFIX}"
        # The default path is `libexec`, Arch Linux for e.g.
        # replaces this with `lib` so check both.
        PATH_SUFFIXES "libexec/mold" "lib/mold" "lib64/mold"
        NO_DEFAULT_PATH
        NO_CACHE
      )
      if(NOT MOLD_BIN_DIR)
        message(STATUS
          "The mold linker could not find the directory containing the linker command "
          "(typically "
          "\"${MOLD_PREFIX}/libexec/mold/ld\") or "
          "\"${MOLD_PREFIX}/lib/mold/ld\") using system linker.")
        set(WITH_LINKER_MOLD OFF)
      endif()
      unset(MOLD_PREFIX)
    endif()

    if(WITH_LINKER_MOLD)
      # GCC will search for `ld` in this directory first.
      string(APPEND CMAKE_EXE_LINKER_FLAGS    " -B \"${MOLD_BIN_DIR}\"")
      string(APPEND CMAKE_SHARED_LINKER_FLAGS " -B \"${MOLD_BIN_DIR}\"")
      string(APPEND CMAKE_MODULE_LINKER_FLAGS " -B \"${MOLD_BIN_DIR}\"")
      set(_IS_LINKER_DEFAULT OFF)
    endif()
    unset(MOLD_BIN)
    unset(MOLD_BIN_DIR)
  endif()

  if(WITH_LINKER_GOLD AND _IS_LINKER_DEFAULT)
    execute_process(
      COMMAND ${CMAKE_C_COMPILER} -fuse-ld=gold -Wl,--version
      ERROR_QUIET OUTPUT_VARIABLE LD_VERSION)
    if("${LD_VERSION}" MATCHES "GNU gold")
      string(APPEND CMAKE_EXE_LINKER_FLAGS    " -fuse-ld=gold")
      string(APPEND CMAKE_SHARED_LINKER_FLAGS " -fuse-ld=gold")
      string(APPEND CMAKE_MODULE_LINKER_FLAGS " -fuse-ld=gold")
      set(_IS_LINKER_DEFAULT OFF)
    else()
      message(STATUS "GNU gold linker isn't available, using the default system linker.")
    endif()
    unset(LD_VERSION)
  endif()

  if(WITH_LINKER_LLD AND _IS_LINKER_DEFAULT)
    execute_process(
      COMMAND ${CMAKE_C_COMPILER} -fuse-ld=lld -Wl,--version
      ERROR_QUIET OUTPUT_VARIABLE LD_VERSION)
    if("${LD_VERSION}" MATCHES "LLD")
      string(APPEND CMAKE_EXE_LINKER_FLAGS    " -fuse-ld=lld")
      string(APPEND CMAKE_SHARED_LINKER_FLAGS " -fuse-ld=lld")
      string(APPEND CMAKE_MODULE_LINKER_FLAGS " -fuse-ld=lld")
      set(_IS_LINKER_DEFAULT OFF)
    else()
      message(STATUS "LLD linker isn't available, using the default system linker.")
    endif()
    unset(LD_VERSION)
  endif()

# CLang is the same as GCC for now.
elseif(CMAKE_C_COMPILER_ID MATCHES "Clang")
  set(PLATFORM_CFLAGS "-pipe -fPIC -funsigned-char -fno-strict-aliasing -ffp-contract=off")

  if(WITH_LINKER_MOLD AND _IS_LINKER_DEFAULT)
    find_program(MOLD_BIN "mold")
    mark_as_advanced(MOLD_BIN)
    if(NOT MOLD_BIN)
      message(STATUS "The \"mold\" binary could not be found, using system linker.")
      set(WITH_LINKER_MOLD OFF)
    else()
      if(CMAKE_C_COMPILER_VERSION VERSION_GREATER_EQUAL 12.0)
        string(APPEND CMAKE_EXE_LINKER_FLAGS    " --ld-path=\"${MOLD_BIN}\"")
        string(APPEND CMAKE_SHARED_LINKER_FLAGS " --ld-path=\"${MOLD_BIN}\"")
        string(APPEND CMAKE_MODULE_LINKER_FLAGS " --ld-path=\"${MOLD_BIN}\"")
      else()
        string(APPEND CMAKE_EXE_LINKER_FLAGS    " -fuse-ld=\"${MOLD_BIN}\"")
        string(APPEND CMAKE_SHARED_LINKER_FLAGS " -fuse-ld=\"${MOLD_BIN}\"")
        string(APPEND CMAKE_MODULE_LINKER_FLAGS " -fuse-ld=\"${MOLD_BIN}\"")
      endif()
      set(_IS_LINKER_DEFAULT OFF)
    endif()
    unset(MOLD_BIN)
  endif()

# Intel C++ Compiler
elseif(CMAKE_C_COMPILER_ID MATCHES "Intel")
  # think these next two are broken
  find_program(XIAR xiar)
  if(XIAR)
    set(CMAKE_AR "${XIAR}")
  endif()
  mark_as_advanced(XIAR)

  find_program(XILD xild)
  if(XILD)
    set(CMAKE_LINKER "${XILD}")
  endif()
  mark_as_advanced(XILD)

  string(APPEND CMAKE_C_FLAGS " -fp-model precise -prec_div -parallel")
  string(APPEND CMAKE_CXX_FLAGS " -fp-model precise -prec_div -parallel")

  # string(APPEND PLATFORM_CFLAGS " -diag-enable sc3")
  set(PLATFORM_CFLAGS "-pipe -fPIC -funsigned-char -fno-strict-aliasing")
  string(APPEND PLATFORM_LINKFLAGS " -static-intel")
endif()

unset(_IS_LINKER_DEFAULT)

# Avoid conflicts with Mesa llvmpipe, Luxrender, and other plug-ins that may
# use the same libraries as Blender with a different version or build options.
set(PLATFORM_SYMBOLS_MAP ${CMAKE_SOURCE_DIR}/source/creator/symbols_unix.map)
set(PLATFORM_LINKFLAGS
  "${PLATFORM_LINKFLAGS} -Wl,--version-script='${PLATFORM_SYMBOLS_MAP}'"
)

# Don't use position independent executable for portable install since file
# browsers can't properly detect blender as an executable then. Still enabled
# for non-portable installs as typically used by Linux distributions.
if(WITH_INSTALL_PORTABLE)
  string(APPEND CMAKE_EXE_LINKER_FLAGS " -no-pie")
endif()

if(WITH_COMPILER_CCACHE)
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

# On some platforms certain atomic operations are not possible with assembly and/or intrinsics and
# they are emulated in software with locks. For example, on armel there is no intrinsics to grant
# 64 bit atomic operations and STL library uses libatomic to offload software emulation of atomics
# to.
# This function will check whether libatomic is required and if so will configure linker flags.
# If atomic operations are possible without libatomic then linker flags are left as-is.
function(CONFIGURE_ATOMIC_LIB_IF_NEEDED)
  # Source which is used to enforce situation when software emulation of atomics is required.
  # Assume that using 64bit integer gives a definitive answer (as in, if 64bit atomic operations
  # are possible using assembly/intrinsics 8, 16, and 32 bit operations will also be possible.
  set(_source
      "#include <atomic>
      #include <cstdint>
      int main(int argc, char **argv) {
        std::atomic<uint64_t> uint64; uint64++;
        return 0;
      }")

  include(CheckCXXSourceCompiles)
  check_cxx_source_compiles("${_source}" ATOMIC_OPS_WITHOUT_LIBATOMIC)

  if(NOT ATOMIC_OPS_WITHOUT_LIBATOMIC)
    # Compilation of the test program has failed.
    # Try it again with -latomic to see if this is what is needed, or whether something else is
    # going on.

    set(CMAKE_REQUIRED_LIBRARIES atomic)
    check_cxx_source_compiles("${_source}" ATOMIC_OPS_WITH_LIBATOMIC)

    if(ATOMIC_OPS_WITH_LIBATOMIC)
      set(PLATFORM_LINKFLAGS "${PLATFORM_LINKFLAGS} -latomic" PARENT_SCOPE)
    else()
      # Atomic operations are required part of Blender and it is not possible to process forward.
      # We expect that either standard library or libatomic will make atomics to work. If both
      # cases has failed something fishy o na bigger scope is going on.
      message(FATAL_ERROR "Failed to detect required configuration for atomic operations")
    endif()
  endif()
endfunction()

CONFIGURE_ATOMIC_LIB_IF_NEEDED()
