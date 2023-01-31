# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright 2016 Blender Foundation. All rights reserved.

# Libraries configuration for any *nix system including Linux and Unix (excluding APPLE).

# Detect precompiled library directory

if(NOT WITH_LIBS_PRECOMPILED)
  unset(LIBDIR)
else()
  if(NOT DEFINED LIBDIR)
    # Path to a locally compiled libraries.
    set(LIBDIR_NAME ${CMAKE_SYSTEM_NAME}_${CMAKE_SYSTEM_PROCESSOR})
    string(TOLOWER ${LIBDIR_NAME} LIBDIR_NAME)
    set(LIBDIR_NATIVE_ABI ${CMAKE_SOURCE_DIR}/../lib/${LIBDIR_NAME})

    # Path to precompiled libraries with known glibc 2.28 ABI.
    set(LIBDIR_GLIBC228_ABI ${CMAKE_SOURCE_DIR}/../lib/linux_x86_64_glibc_228)

    # Choose the best suitable libraries.
    if(EXISTS ${LIBDIR_NATIVE_ABI})
      set(LIBDIR ${LIBDIR_NATIVE_ABI})
      set(WITH_LIBC_MALLOC_HOOK_WORKAROUND True)
    elseif(EXISTS ${LIBDIR_GLIBC228_ABI})
      set(LIBDIR ${LIBDIR_GLIBC228_ABI})
      if(WITH_MEM_JEMALLOC)
        # jemalloc provides malloc hooks.
        set(WITH_LIBC_MALLOC_HOOK_WORKAROUND False)
      else()
        set(WITH_LIBC_MALLOC_HOOK_WORKAROUND True)
      endif()
    endif()

    # Avoid namespace pollustion.
    unset(LIBDIR_NATIVE_ABI)
    unset(LIBDIR_GLIBC228_ABI)
  endif()

  if(NOT (EXISTS ${LIBDIR}))
    message(STATUS
      "Unable to find LIBDIR: ${LIBDIR}, system libraries may be used "
      "(disable WITH_LIBS_PRECOMPILED to suppress this message)."
    )
    unset(LIBDIR)
  endif()
endif()


# Support restoring this value once pre-compiled libraries have been handled.
set(WITH_STATIC_LIBS_INIT ${WITH_STATIC_LIBS})

if(DEFINED LIBDIR)
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
  set(MaterialX_DIR ${LIBDIR}/materialx/lib/cmake/MaterialX)
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

if(DEFINED LIBDIR)
  without_system_libs_begin()
endif()

find_package_wrapper(JPEG REQUIRED)
find_package_wrapper(PNG REQUIRED)
find_package_wrapper(ZLIB REQUIRED)
find_package_wrapper(Zstd REQUIRED)
find_package_wrapper(Epoxy REQUIRED)

if(WITH_VULKAN_BACKEND)
  find_package_wrapper(Vulkan REQUIRED)
  find_package_wrapper(ShaderC REQUIRED)
endif()

function(check_freetype_for_brotli)
  include(CheckSymbolExists)
  set(CMAKE_REQUIRED_INCLUDES ${FREETYPE_INCLUDE_DIRS})
  check_symbol_exists(FT_CONFIG_OPTION_USE_BROTLI "freetype/config/ftconfig.h" HAVE_BROTLI)
  unset(CMAKE_REQUIRED_INCLUDES)
  if(NOT HAVE_BROTLI)
    unset(HAVE_BROTLI CACHE)
    message(FATAL_ERROR "Freetype needs to be compiled with brotli support!")
  endif()
  unset(HAVE_BROTLI CACHE)
endfunction()

if(NOT WITH_SYSTEM_FREETYPE)
  # FreeType compiled with Brotli compression for woff2.
  find_package_wrapper(Freetype REQUIRED)
  if(DEFINED LIBDIR)
    find_package_wrapper(Brotli REQUIRED)

    # NOTE: This is done on WIN32 & APPLE but fails on some Linux systems.
    # See: https://devtalk.blender.org/t/22536
    # So `BROTLI_LIBRARIES` need to be added directly after `FREETYPE_LIBRARIES`.
    #
    # list(APPEND FREETYPE_LIBRARIES
    #   ${BROTLI_LIBRARIES}
    # )
  endif()
  check_freetype_for_brotli()
endif()

if(WITH_PYTHON)
  # This could be used, see: D14954 for details.
  # `find_package(PythonLibs)`

  # Use our own instead, since without Python is such a rare case,
  # require this package.
  # XXX: Linking errors with Debian static Python (sigh).
  # find_package_wrapper(PythonLibsUnix REQUIRED)
  find_package(PythonLibsUnix REQUIRED)

  if(WITH_PYTHON_MODULE AND NOT WITH_INSTALL_PORTABLE)
    # Installing into `site-packages`, warn when installing into `./../lib/`
    # which script authors almost certainly don't want.
    if(DEFINED LIBDIR)
      path_is_prefix(LIBDIR PYTHON_SITE_PACKAGES _is_prefix)
      if(_is_prefix)
        message(WARNING "
Building Blender with the following configuration:
  - WITH_PYTHON_MODULE=ON
  - WITH_INSTALL_PORTABLE=OFF
  - LIBDIR=\"${LIBDIR}\"
  - PYTHON_SITE_PACKAGES=\"${PYTHON_SITE_PACKAGES}\"
In this case you may want to either:
  - Use the system Python's site-packages, see:
    python -c \"import site; print(site.getsitepackages()[0])\"
  - Set WITH_INSTALL_PORTABLE=ON to create a stand-alone \"bpy\" module
    which you will need to ensure is in Python's module search path.
Proceeding with PYTHON_SITE_PACKAGES install target, you have been warned!"
        )
      endif()
      unset(_is_prefix)
    endif()
  endif()
endif()

if(WITH_IMAGE_OPENEXR)
  find_package_wrapper(OpenEXR)  # our own module
  set_and_warn_library_found("OpenEXR" OPENEXR_FOUND WITH_IMAGE_OPENEXR)
endif()
add_bundled_libraries(openexr/lib)
add_bundled_libraries(imath/lib)

if(WITH_IMAGE_OPENJPEG)
  find_package_wrapper(OpenJPEG)
  set_and_warn_library_found("OpenJPEG" OPENJPEG_FOUND WITH_IMAGE_OPENJPEG)
endif()

if(WITH_IMAGE_TIFF)
  # XXX Linking errors with debian static tiff :/
#       find_package_wrapper(TIFF)
  find_package(TIFF)
  set_and_warn_library_found("TIFF" TIFF_FOUND WITH_IMAGE_TIFF)
endif()

if(WITH_OPENAL)
  find_package_wrapper(OpenAL)
  set_and_warn_library_found("OpenAL" OPENAL_FOUND WITH_OPENAL)
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
    set_and_warn_library_found("SDL" SDL_FOUND WITH_SDL)
  endif()
endif()

# Codecs
if(WITH_CODEC_SNDFILE)
  find_package_wrapper(SndFile)
  set_and_warn_library_found("libsndfile" SNDFILE_FOUND WITH_CODEC_SNDFILE)
endif()

if(WITH_CODEC_FFMPEG)
  if(DEFINED LIBDIR)
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
    if((DEFINED LIBDIR) AND (EXISTS ${LIBDIR}/ffmpeg/lib/libaom.a))
      list(APPEND FFMPEG_FIND_COMPONENTS aom)
    endif()
  elseif(FFMPEG)
    # Old cache variable used for root dir, convert to new standard.
    set(FFMPEG_ROOT_DIR ${FFMPEG})
  endif()
  find_package(FFmpeg)

  set_and_warn_library_found("FFmpeg" FFMPEG_FOUND WITH_CODEC_FFMPEG)
endif()

if(WITH_FFTW3)
  find_package_wrapper(Fftw3)
  set_and_warn_library_found("fftw3" FFTW3_FOUND WITH_FFTW3)
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
    set_and_warn_library_found("OpenCollada" OPENCOLLADA_FOUND WITH_OPENCOLLADA)
  endif()
endif()

if(WITH_MEM_JEMALLOC)
  find_package_wrapper(JeMalloc)
  set_and_warn_library_found("JeMalloc" JEMALLOC_FOUND WITH_MEM_JEMALLOC)
endif()

if(WITH_INPUT_NDOF)
  find_package_wrapper(Spacenav)
  set_and_warn_library_found("SpaceNav" SPACENAV_FOUND WITH_INPUT_NDOF)

  if(SPACENAV_FOUND)
    # use generic names within blenders buildsystem.
    set(NDOF_INCLUDE_DIRS ${SPACENAV_INCLUDE_DIRS})
    set(NDOF_LIBRARIES ${SPACENAV_LIBRARIES})
  endif()
endif()

if(WITH_CYCLES AND WITH_CYCLES_OSL)
  set(CYCLES_OSL ${LIBDIR}/osl CACHE PATH "Path to OpenShadingLanguage installation")
  if(EXISTS ${CYCLES_OSL} AND NOT OSL_ROOT)
    set(OSL_ROOT ${CYCLES_OSL})
  endif()
  find_package_wrapper(OSL)
  set_and_warn_library_found("OSL" OSL_FOUND WITH_CYCLES_OSL)

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
  endif()
endif()

if(WITH_CYCLES AND WITH_CYCLES_DEVICE_ONEAPI)
  set(CYCLES_LEVEL_ZERO ${LIBDIR}/level-zero CACHE PATH "Path to Level Zero installation")
  if(EXISTS ${CYCLES_LEVEL_ZERO} AND NOT LEVEL_ZERO_ROOT_DIR)
    set(LEVEL_ZERO_ROOT_DIR ${CYCLES_LEVEL_ZERO})
  endif()

  set(CYCLES_SYCL ${LIBDIR}/dpcpp CACHE PATH "Path to oneAPI DPC++ compiler")
  if(EXISTS ${CYCLES_SYCL} AND NOT SYCL_ROOT_DIR)
    set(SYCL_ROOT_DIR ${CYCLES_SYCL})
  endif()
  file(GLOB _sycl_runtime_libraries
    ${SYCL_ROOT_DIR}/lib/libsycl.so
    ${SYCL_ROOT_DIR}/lib/libsycl.so.*
    ${SYCL_ROOT_DIR}/lib/libpi_*.so
  )
  list(FILTER _sycl_runtime_libraries EXCLUDE REGEX ".*\.py")
  list(REMOVE_ITEM _sycl_runtime_libraries "${SYCL_ROOT_DIR}/lib/libpi_opencl.so")
  list(APPEND PLATFORM_BUNDLED_LIBRARIES ${_sycl_runtime_libraries})
  unset(_sycl_runtime_libraries)
endif()

if(WITH_OPENVDB)
  find_package(OpenVDB)
  set_and_warn_library_found("OpenVDB" OPENVDB_FOUND WITH_OPENVDB)
endif()
add_bundled_libraries(openvdb/lib)

if(WITH_NANOVDB)
  find_package_wrapper(NanoVDB)
  set_and_warn_library_found("NanoVDB" NANOVDB_FOUND WITH_NANOVDB)
endif()

if(WITH_CPU_SIMD AND SUPPORT_NEON_BUILD)
  find_package_wrapper(sse2neon)
endif()

if(WITH_ALEMBIC)
  find_package_wrapper(Alembic)
  set_and_warn_library_found("Alembic" ALEMBIC_FOUND WITH_ALEMBIC)
endif()

if(WITH_USD)
  find_package_wrapper(USD)
  set_and_warn_library_found("USD" USD_FOUND WITH_USD)
endif()
add_bundled_libraries(usd/lib)

if(WITH_MATERIALX)
  find_package_wrapper(MaterialX)
  set_and_warn_library_found("MaterialX" MaterialX_FOUND WITH_MATERIALX)
endif()
add_bundled_libraries(materialx/lib)

if(WITH_BOOST)
  # uses in build instructions to override include and library variables
  if(NOT BOOST_CUSTOM)
    if(WITH_STATIC_LIBS)
      set(Boost_USE_STATIC_LIBS OFF)
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
    if(WITH_USD AND USD_PYTHON_SUPPORT)
      list(APPEND __boost_packages python${PYTHON_VERSION_NO_DOTS})
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

  # Boost Python is separate to avoid linking Python into tests that don't need it.
  set(BOOST_LIBRARIES ${Boost_LIBRARIES})
  if(WITH_USD AND USD_PYTHON_SUPPORT)
    set(BOOST_PYTHON_LIBRARIES ${Boost_PYTHON${PYTHON_VERSION_NO_DOTS}_LIBRARY})
    list(REMOVE_ITEM BOOST_LIBRARIES ${BOOST_PYTHON_LIBRARIES})
  endif()
  set(BOOST_INCLUDE_DIR ${Boost_INCLUDE_DIRS})
  set(BOOST_LIBPATH ${Boost_LIBRARY_DIRS})
  set(BOOST_DEFINITIONS "-DBOOST_ALL_NO_LIB")

  if(Boost_USE_STATIC_LIBS AND WITH_BOOST_ICU)
    find_package(IcuLinux)
    list(APPEND BOOST_LIBRARIES ${ICU_LIBRARIES})
  endif()
endif()
add_bundled_libraries(boost/lib)

if(WITH_PUGIXML)
  find_package_wrapper(PugiXML)
  set_and_warn_library_found("PugiXML" PUGIXML_FOUND WITH_PUGIXML)
endif()

if(WITH_IMAGE_WEBP)
  set(WEBP_ROOT_DIR ${LIBDIR}/webp)
  find_package_wrapper(WebP)
  set_and_warn_library_found("WebP" WEBP_FOUND WITH_IMAGE_WEBP)
endif()

if(WITH_OPENIMAGEIO)
  find_package_wrapper(OpenImageIO)
  set(OPENIMAGEIO_LIBRARIES
    ${OPENIMAGEIO_LIBRARIES}
    ${PNG_LIBRARIES}
    ${JPEG_LIBRARIES}
    ${ZLIB_LIBRARIES}
  )

  set(OPENIMAGEIO_DEFINITIONS "")

  if(WITH_BOOST)
    list(APPEND OPENIMAGEIO_LIBRARIES "${BOOST_LIBRARIES}")
  endif()
  if(WITH_IMAGE_TIFF)
    list(APPEND OPENIMAGEIO_LIBRARIES "${TIFF_LIBRARY}")
  endif()
  if(WITH_IMAGE_OPENEXR)
    list(APPEND OPENIMAGEIO_LIBRARIES "${OPENEXR_LIBRARIES}")
  endif()
  if(WITH_IMAGE_WEBP)
    list(APPEND OPENIMAGEIO_LIBRARIES "${WEBP_LIBRARIES}")
  endif()

  set_and_warn_library_found("OPENIMAGEIO" OPENIMAGEIO_FOUND WITH_OPENIMAGEIO)
endif()
add_bundled_libraries(openimageio/lib)

if(WITH_OPENCOLORIO)
  find_package_wrapper(OpenColorIO 2.0.0)

  set(OPENCOLORIO_DEFINITIONS "")
  set_and_warn_library_found("OpenColorIO" OPENCOLORIO_FOUND WITH_OPENCOLORIO)
endif()
add_bundled_libraries(opencolorio/lib)

if(WITH_CYCLES AND WITH_CYCLES_EMBREE)
  find_package(Embree 3.8.0 REQUIRED)
endif()

if(WITH_OPENIMAGEDENOISE)
  find_package_wrapper(OpenImageDenoise)
  set_and_warn_library_found("OpenImageDenoise" OPENIMAGEDENOISE_FOUND WITH_OPENIMAGEDENOISE)
endif()

if(WITH_LLVM)
  if(DEFINED LIBDIR)
    set(LLVM_STATIC ON)
  endif()

  find_package_wrapper(LLVM)
  set_and_warn_library_found("LLVM" LLVM_FOUND WITH_LLVM)

  if(LLVM_FOUND)
    if(WITH_CLANG)
      find_package_wrapper(Clang)
      set_and_warn_library_found("Clang" CLANG_FOUND WITH_CLANG)
    endif()

    # Symbol conflicts with same UTF library used by OpenCollada
    if(DEFINED LIBDIR)
      if(WITH_OPENCOLLADA AND (${LLVM_VERSION} VERSION_LESS "4.0.0"))
        list(REMOVE_ITEM OPENCOLLADA_LIBRARIES ${OPENCOLLADA_UTF_LIBRARY})
      endif()
    endif()
  endif()
endif()

if(WITH_OPENSUBDIV)
  find_package(OpenSubdiv)

  set(OPENSUBDIV_LIBRARIES ${OPENSUBDIV_LIBRARIES})
  set(OPENSUBDIV_LIBPATH)  # TODO, remove and reference the absolute path everywhere

  set_and_warn_library_found("OpenSubdiv" OPENSUBDIV_FOUND WITH_OPENSUBDIV)
endif()
add_bundled_libraries(opensubdiv/lib)

if(WITH_TBB)
  find_package_wrapper(TBB)
  set_and_warn_library_found("TBB" TBB_FOUND WITH_TBB)
endif()
add_bundled_libraries(tbb/lib)

if(WITH_XR_OPENXR)
  find_package(XR_OpenXR_SDK)
  set_and_warn_library_found("OpenXR-SDK" XR_OPENXR_SDK_FOUND WITH_XR_OPENXR)
endif()

if(WITH_GMP)
  find_package_wrapper(GMP)
  set_and_warn_library_found("GMP" GMP_FOUND WITH_GMP)
endif()

if(WITH_POTRACE)
  find_package_wrapper(Potrace)
  set_and_warn_library_found("Potrace" POTRACE_FOUND WITH_POTRACE)
endif()

if(WITH_HARU)
  find_package_wrapper(Haru)
  set_and_warn_library_found("Haru" HARU_FOUND WITH_HARU)
endif()

if(WITH_CYCLES AND WITH_CYCLES_PATH_GUIDING)
  find_package_wrapper(openpgl)
  if(openpgl_FOUND)
    get_target_property(OPENPGL_LIBRARIES openpgl::openpgl LOCATION)
    get_target_property(OPENPGL_INCLUDE_DIR openpgl::openpgl INTERFACE_INCLUDE_DIRECTORIES)
    message(STATUS "Found OpenPGL: ${OPENPGL_LIBRARIES}")
  else()
    set(WITH_CYCLES_PATH_GUIDING OFF)
    message(STATUS "OpenPGL not found, disabling WITH_CYCLES_PATH_GUIDING")
  endif()
endif()

if(DEFINED LIBDIR)
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
# `FindThreads` documentation notes that this may be empty
# with the system libraries provide threading functionality.
if(CMAKE_THREAD_LIBS_INIT)
  list(APPEND PLATFORM_LINKLIBS ${CMAKE_THREAD_LIBS_INIT})
  # used by other platforms
  set(PTHREADS_LIBRARIES ${CMAKE_THREAD_LIBS_INIT})
endif()


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

if(DEFINED LIBDIR)
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
  check_freetype_for_brotli()
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
  set_and_warn_library_found("JACK" JACK_FOUND WITH_JACK)
endif()

# Pulse is intended to use the system library.
if(WITH_PULSEAUDIO)
  find_package_wrapper(Pulse)
  set_and_warn_library_found("PulseAudio" PULSE_FOUND WITH_PULSEAUDIO)
endif()

# Audio IO
if(WITH_SYSTEM_AUDASPACE)
  find_package_wrapper(Audaspace)
  set(AUDASPACE_FOUND ${AUDASPACE_FOUND} AND ${AUDASPACE_C_FOUND})
  set_and_warn_library_found("External Audaspace" AUDASPACE_FOUND WITH_SYSTEM_AUDASPACE)
endif()

if(WITH_GHOST_WAYLAND)
  find_package(PkgConfig)
  pkg_check_modules(xkbcommon xkbcommon)

  # When dynamically linked WAYLAND is used and `${LIBDIR}/wayland` is present,
  # there is no need to search for the libraries as they are not needed for building.
  # Only the headers are needed which can reference the known paths.
  if((DEFINED LIBDIR) AND (EXISTS "${LIBDIR}/wayland" AND WITH_GHOST_WAYLAND_DYNLOAD))
    set(_use_system_wayland OFF)
  else()
    set(_use_system_wayland ON)
  endif()

  if(_use_system_wayland)
    pkg_check_modules(wayland-client wayland-client>=1.12)
    pkg_check_modules(wayland-egl wayland-egl)
    pkg_check_modules(wayland-scanner wayland-scanner)
    pkg_check_modules(wayland-cursor wayland-cursor)
    pkg_check_modules(wayland-protocols wayland-protocols>=1.15)
    pkg_get_variable(WAYLAND_PROTOCOLS_DIR wayland-protocols pkgdatadir)
  else()
    # Rocky8 packages have too old a version, a newer version exist in the pre-compiled libraries.
    find_path(WAYLAND_PROTOCOLS_DIR
      NAMES unstable/xdg-decoration/xdg-decoration-unstable-v1.xml
      PATH_SUFFIXES share/wayland-protocols
      PATHS ${LIBDIR}/wayland-protocols
    )

    if(EXISTS ${WAYLAND_PROTOCOLS_DIR})
      set(wayland-protocols_FOUND ON)
    endif()

    set(wayland-client_INCLUDE_DIRS "${LIBDIR}/wayland/include")
    set(wayland-egl_INCLUDE_DIRS "${LIBDIR}/wayland/include")
    set(wayland-cursor_INCLUDE_DIRS "${LIBDIR}/wayland/include")

    set(wayland-client_FOUND ON)
    set(wayland-egl_FOUND ON)
    set(wayland-scanner_FOUND ON)
    set(wayland-cursor_FOUND ON)
  endif()
  mark_as_advanced(WAYLAND_PROTOCOLS_DIR)

  set_and_warn_library_found("wayland-client" wayland-client_FOUND WITH_GHOST_WAYLAND)
  set_and_warn_library_found("wayland-egl" wayland-egl_FOUND WITH_GHOST_WAYLAND)
  set_and_warn_library_found("wayland-scanner" wayland-scanner_FOUND WITH_GHOST_WAYLAND)
  set_and_warn_library_found("wayland-cursor" wayland-cursor_FOUND WITH_GHOST_WAYLAND)
  set_and_warn_library_found("wayland-protocols" wayland-protocols_FOUND WITH_GHOST_WAYLAND)
  set_and_warn_library_found("xkbcommon" xkbcommon_FOUND WITH_GHOST_WAYLAND)

  if(WITH_GHOST_WAYLAND)
    if(WITH_GHOST_WAYLAND_DBUS)
      pkg_check_modules(dbus REQUIRED dbus-1)
    endif()

    if(WITH_GHOST_WAYLAND_LIBDECOR)
      if(_use_system_wayland)
        pkg_check_modules(libdecor REQUIRED libdecor-0>=0.1)
      else()
        set(libdecor_INCLUDE_DIRS "${LIBDIR}/wayland_libdecor/include/libdecor-0")
      endif()
    endif()

    if(WITH_GHOST_WAYLAND_DBUS)
      add_definitions(-DWITH_GHOST_WAYLAND_DBUS)
    endif()

    if(WITH_GHOST_WAYLAND_LIBDECOR)
      add_definitions(-DWITH_GHOST_WAYLAND_LIBDECOR)
    endif()

    if((DEFINED LIBDIR) AND (EXISTS "${LIBDIR}/wayland/bin/wayland-scanner"))
      set(WAYLAND_SCANNER "${LIBDIR}/wayland/bin/wayland-scanner")
    else()
      pkg_get_variable(WAYLAND_SCANNER wayland-scanner wayland_scanner)
    endif()
    mark_as_advanced(WAYLAND_SCANNER)

    # When using dynamic loading, headers generated
    # from older versions of `wayland-scanner` aren't compatible.
    if(WITH_GHOST_WAYLAND_DYNLOAD)
      execute_process(
        COMMAND ${WAYLAND_SCANNER} --version
        # The version is written to the `stderr`.
        ERROR_VARIABLE _wayland_scanner_out
        ERROR_STRIP_TRAILING_WHITESPACE
      )
      if(NOT "${_wayland_scanner_out}" STREQUAL "")
        string(
          REGEX REPLACE
          "^wayland-scanner[ \t]+([0-9]+)\.([0-9]+).*"
          "\\1.\\2"
          _wayland_scanner_ver
          "${_wayland_scanner_out}"
        )
        if("${_wayland_scanner_ver}" VERSION_LESS "1.20")
          message(
            FATAL_ERROR
            "Found ${WAYLAND_SCANNER} version \"${_wayland_scanner_ver}\", "
            "the minimum version is 1.20!"
          )
        endif()
        unset(_wayland_scanner_ver)
      else()
        message(WARNING "Unable to access the version from ${WAYLAND_SCANNER}, continuing.")
      endif()
      unset(_wayland_scanner_out)
    endif()
    # End wayland-scanner version check.

  endif()

  unset(_use_system_wayland)
endif()

if(WITH_GHOST_X11)
  find_package(X11 REQUIRED)

  find_path(X11_XF86keysym_INCLUDE_PATH X11/XF86keysym.h ${X11_INC_SEARCH_PATH})
  mark_as_advanced(X11_XF86keysym_INCLUDE_PATH)

  if(WITH_X11_XINPUT)
    if(NOT X11_Xinput_LIB)
      message(FATAL_ERROR "LibXi not found. Disable WITH_X11_XINPUT if you
      want to build without tablet support")
    endif()
  endif()

  if(WITH_X11_XF86VMODE)
    # XXX, why doesn't cmake make this available?
    find_library(X11_Xxf86vmode_LIB Xxf86vm   ${X11_LIB_SEARCH_PATH})
    mark_as_advanced(X11_Xxf86vmode_LIB)
    if(NOT X11_Xxf86vmode_LIB)
      message(FATAL_ERROR "libXxf86vm not found. Disable WITH_X11_XF86VMODE if you
      want to build without")
    endif()
  endif()

  if(WITH_X11_XFIXES)
    if(NOT X11_Xfixes_LIB)
      message(FATAL_ERROR "libXfixes not found. Disable WITH_X11_XFIXES if you
      want to build without")
    endif()
  endif()

  if(WITH_X11_ALPHA)
    find_library(X11_Xrender_LIB Xrender ${X11_LIB_SEARCH_PATH})
    mark_as_advanced(X11_Xrender_LIB)
    if(NOT X11_Xrender_LIB)
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
  # this until we support it. Seems to default to off in clang and the intel
  # compiler.
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
          "\"${MOLD_PREFIX}/lib/mold/ld\") using system linker."
        )
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
  set(PLATFORM_CFLAGS "-pipe -fPIC -funsigned-char -fno-strict-aliasing")

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

# Always link with libatomic if available, as it is required for data types
# which don't have intrinsics.
function(configure_atomic_lib_if_needed)
  set(_source
      "#include <atomic>
      #include <cstdint>
      int main(int argc, char **argv) {
        std::atomic<uint64_t> uint64; uint64++;
        return 0;
      }"
  )

  include(CheckCXXSourceCompiles)
  set(CMAKE_REQUIRED_LIBRARIES atomic)
  check_cxx_source_compiles("${_source}" ATOMIC_OPS_WITH_LIBATOMIC)
  unset(CMAKE_REQUIRED_LIBRARIES)

  if(ATOMIC_OPS_WITH_LIBATOMIC)
    set(PLATFORM_LINKFLAGS "${PLATFORM_LINKFLAGS} -latomic" PARENT_SCOPE)
  endif()
endfunction()

configure_atomic_lib_if_needed()

if(PLATFORM_BUNDLED_LIBRARIES)
  # For the installed Python module and installed Blender executable, we set the
  # rpath to the relative path where the install step will copy the shared libraries.
  set(CMAKE_SKIP_INSTALL_RPATH FALSE)
  list(APPEND CMAKE_INSTALL_RPATH $ORIGIN/lib)

  # For executables that are built but not installed (mainly tests) we set an absolute
  # rpath to the lib folder. This is needed because these can be in different folders,
  # and because the build and install folder may be different.
  set(CMAKE_SKIP_BUILD_RPATH FALSE)
  list(APPEND CMAKE_BUILD_RPATH $ORIGIN/lib ${CMAKE_INSTALL_PREFIX_WITH_CONFIG}/lib)

  # Environment variables to run precompiled executables that needed libraries.
  list(JOIN PLATFORM_BUNDLED_LIBRARY_DIRS ":" _library_paths)
  set(PLATFORM_ENV_BUILD "LD_LIBRARY_PATH=\"${_library_paths};${LD_LIBRARY_PATH}\"")
  set(PLATFORM_ENV_INSTALL "LD_LIBRARY_PATH=${CMAKE_INSTALL_PREFIX_WITH_CONFIG}/lib/;$LD_LIBRARY_PATH")
  unset(_library_paths)
endif()
