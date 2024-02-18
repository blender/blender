# SPDX-FileCopyrightText: 2006 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

# -----------------------------------------------------------------------------
# Early Initialization

# NOTE: We don't allow in-source builds. This causes no end of troubles because
# all out-of-source builds will use the CMakeCache.txt file there and even
# build the libs and objects in it.
if(${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})
  if(NOT DEFINED WITH_IN_SOURCE_BUILD)
    message(FATAL_ERROR
      "CMake generation for blender is not allowed within the source directory!"
      "\n Remove \"${CMAKE_SOURCE_DIR}/CMakeCache.txt\" and try again from another folder, e.g.:"
      "\n "
      "\n rm -rf CMakeCache.txt CMakeFiles"
      "\n cd .."
      "\n mkdir cmake-make"
      "\n cd cmake-make"
      "\n cmake ../blender"
      "\n "
      "\n Alternately define WITH_IN_SOURCE_BUILD to force this option (not recommended!)"
    )
  endif()
endif()

cmake_minimum_required(VERSION 3.10)

if(NOT EXECUTABLE_OUTPUT_PATH)
  set(FIRST_RUN TRUE)
else()
  set(FIRST_RUN FALSE)
endif()

# this starts out unset
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/build_files/cmake/Modules")
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/build_files/cmake/platform")

# Avoid having an empty `CMAKE_BUILD_TYPE`.
if(NOT DEFINED CMAKE_BUILD_TYPE_INIT)
  set(CMAKE_BUILD_TYPE_INIT "Release")
  # Internal logic caches this variable, avoid showing it by default
  # since it's easy to accidentally set instead of the build type.
endif()
mark_as_advanced(CMAKE_BUILD_TYPE_INIT)

# Omit superfluous "Up-to-date" messages.
if(NOT DEFINED CMAKE_INSTALL_MESSAGE)
  set(CMAKE_INSTALL_MESSAGE "LAZY")
endif()

# quiet output for Makefiles, 'make -s' helps too
# set_property(GLOBAL PROPERTY RULE_MESSAGES OFF)

# Global compile definitions since add_definitions() adds for all.
# _DEBUG is a Visual Studio define, enabled for all platforms.
set_property(DIRECTORY APPEND PROPERTY COMPILE_DEFINITIONS
  $<$<CONFIG:Debug>:_DEBUG>
)
# -----------------------------------------------------------------------------
# Set policy

# see "cmake --help-policy CMP0003"
# So library linking is more sane
cmake_policy(SET CMP0003 NEW)

# So BUILDINFO and BLENDERPATH strings are automatically quoted
cmake_policy(SET CMP0005 NEW)

# So syntax problems are errors
cmake_policy(SET CMP0010 NEW)

# Input directories must have CMakeLists.txt
cmake_policy(SET CMP0014 NEW)

# Silence draco warning on macOS, new policy works fine.
if(POLICY CMP0068)
  cmake_policy(SET CMP0068 NEW)
endif()

# find_package() uses <PackageName>_ROOT variables.
if(POLICY CMP0074)
  cmake_policy(SET CMP0074 NEW)
endif()

# Install CODE|SCRIPT allow the use of generator expressions.
if(POLICY CMP0087)
  cmake_policy(SET CMP0087 NEW)
endif()


# -----------------------------------------------------------------------------
# Load Blender's Local Macros

include(build_files/cmake/macros.cmake)

# -----------------------------------------------------------------------------
# Initialize Project

blender_project_hack_pre()

project(Blender)

blender_project_hack_post()

enable_testing()


# -----------------------------------------------------------------------------
# Test Compiler Support
#
# Keep in sync with: https://developer.blender.org/docs/handbook/building_blender/

if(CMAKE_COMPILER_IS_GNUCC)
  if("${CMAKE_C_COMPILER_VERSION}" VERSION_LESS "11.0.0")
    message(FATAL_ERROR "\
The minimum supported version of GCC is 11.0.0, found ${CMAKE_C_COMPILER_VERSION}"
    )
  endif()
elseif(CMAKE_C_COMPILER_ID MATCHES "Clang")
  if(CMAKE_COMPILER_IS_GNUCC AND ("${CMAKE_C_COMPILER_VERSION}" VERSION_LESS "8.0"))
    message(FATAL_ERROR "\
The minimum supported version of CLANG is 8.0, found ${CMAKE_C_COMPILER_VERSION}"
    )
  endif()
elseif(CMAKE_CXX_COMPILER_ID MATCHES MSVC)
  if(MSVC_VERSION VERSION_LESS "1928")
    # MSVC_VERSION is an internal version number, it doesn't map to something
    # the end user would recognize as a version. Because of this, for MSVC we do
    # not show the found number. When using our make.bat the actual VS version
    # will be displayed on the console before starting the build, anyway.
    message(FATAL_ERROR "The minimum supported version of MSVC is 2019 (16.9.16)")
  endif()
endif()

# -----------------------------------------------------------------------------
# Test Compiler/Library Features

include(build_files/cmake/have_features.cmake)


# -----------------------------------------------------------------------------
# Redirect Output Files

set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR}/bin CACHE INTERNAL "" FORCE)
set(LIBRARY_OUTPUT_PATH ${CMAKE_BINARY_DIR}/lib CACHE INTERNAL "" FORCE)

get_property(GENERATOR_IS_MULTI_CONFIG GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(GENERATOR_IS_MULTI_CONFIG)
  set(TESTS_OUTPUT_DIR ${EXECUTABLE_OUTPUT_PATH}/tests/$<CONFIG>/ CACHE INTERNAL "" FORCE)
else()
  set(TESTS_OUTPUT_DIR ${EXECUTABLE_OUTPUT_PATH}/tests/ CACHE INTERNAL "" FORCE)
endif()


# -----------------------------------------------------------------------------
# Set Default Configuration Options

get_blender_version()

if(WIN32)
  add_definitions(
    # This is the app ID used for file registration, given it's used from several modules
    # there really is no nice way to get this information consistent without a global define.
    -DBLENDER_WIN_APPID="blender.${BLENDER_VERSION_MAJOR}.${BLENDER_VERSION_MINOR}"
    # This is the name that will be shown in the taskbar and OpenWith windows UI
    -DBLENDER_WIN_APPID_FRIENDLY_NAME="Blender ${BLENDER_VERSION_MAJOR}.${BLENDER_VERSION_MINOR}"
  )
endif()


if(${CMAKE_VERSION} VERSION_GREATER_EQUAL "3.19")
  # This changes the default value from Off to On, but will still allow people to manually change
  # this setting through their CMakeCache.txt if they desire to do so.
  set(CMAKE_OPTIMIZE_DEPENDENCIES ON CACHE INTERNAL "")
endif()

# -----------------------------------------------------------------------------
# Declare Options

# Blender internal features
option(WITH_BLENDER "Build blender (disable to build only Cycles stand-alone)." ON)
mark_as_advanced(WITH_BLENDER)

if(WIN32)
  option(WITH_BLENDER_THUMBNAILER "\
Build \"BlendThumb.dll\" helper for Windows explorer integration to support extracting \
thumbnails from `.blend` files."
    ON
  )
else()
  set(_option_default ON)
  if(APPLE)
    # In future, can be used with `quicklookthumbnailing/qlthumbnailreply`
    # to create file thumbnails for say Finder.
    # Turn it off for now, even though it can build on APPLE, it's not likely to be useful.
    set(_option_default OFF)
  endif()
  option(WITH_BLENDER_THUMBNAILER "\
Build stand-alone \"blender-thumbnailer\" command-line thumbnail extraction utility, \
intended for use by file-managers to extract PNG images from `.blend` files."
    ${_option_default}
  )
  unset(_option_default)
endif()

option(WITH_INTERNATIONAL "Enable I18N (International fonts and text)" ON)

option(WITH_PYTHON "Enable Embedded Python API (only disable for development)" ON)
option(WITH_PYTHON_SECURITY "Disables execution of scripts within blend files by default" ON)
# Don't want people disabling this unless they really know what they are doing.
mark_as_advanced(WITH_PYTHON)
# Some distributions see this as a security issue, rather than have them patch it,
# make a build option.
mark_as_advanced(WITH_PYTHON_SECURITY)

option(WITH_PYTHON_SAFETY "\
Enable internal API error checking to track invalid data to prevent crash on access \
(at the expense of some efficiency, only enable for development)."
  OFF
)
mark_as_advanced(WITH_PYTHON_SAFETY)
option(WITH_PYTHON_MODULE "\
Enable building as a python module which runs without a user interface, \
like running regular blender in background mode (only enable for development), \
installs to PYTHON_SITE_PACKAGES (or CMAKE_INSTALL_PREFIX if WITH_INSTALL_PORTABLE is enabled)."
  OFF
)

option(WITH_BUILDINFO "\
Include extra build details (only disable for development & faster builds)"
  ON
)
set(BUILDINFO_OVERRIDE_DATE "" CACHE STRING "\
Use instead of the current date for reproducible builds (empty string disables this option)"
)
set(BUILDINFO_OVERRIDE_TIME "" CACHE STRING "\
Use instead of the current time for reproducible builds (empty string disables this option)"
)
set(CPACK_OVERRIDE_PACKAGENAME "" CACHE STRING "\
Use instead of the standard packagename (empty string disables this option)"
)
mark_as_advanced(CPACK_OVERRIDE_PACKAGENAME)
mark_as_advanced(BUILDINFO_OVERRIDE_DATE)
mark_as_advanced(BUILDINFO_OVERRIDE_TIME)

# CMAKE 3.28.2 has issues with the combination of PCH and unity builds, disable for now.
# upstream ticket https://gitlab.kitware.com/cmake/cmake/-/issues/25650
if(${CMAKE_VERSION} VERSION_GREATER_EQUAL "3.16" AND NOT ${CMAKE_VERSION} VERSION_EQUAL "3.28.2")
  option(WITH_UNITY_BUILD "\
Enable unity build for modules that support it to improve compile times.\n\
WARNING: this option allows files to be built without all necessary headers!\n
This option should be disabled before manipulating or removing headers."
    ON
  )
  mark_as_advanced(WITH_UNITY_BUILD)
else()
  set(WITH_UNITY_BUILD OFF)
endif()

option(WITH_IK_ITASC "\
Enable ITASC IK solver (only disable for development & for incompatible C++ compilers)"
  ON
)
option(WITH_IK_SOLVER "\
Enable Legacy IK solver (only disable for development)"
  ON
)
option(WITH_FFTW3 "Enable FFTW3 support (Used for smoke, ocean sim, and audio effects)" ON)
option(WITH_PUGIXML "Enable PugiXML support (Used for OpenImageIO, Grease Pencil SVG export)" ON)
option(WITH_BULLET "Enable Bullet (Physics Engine)" ON)
option(WITH_SYSTEM_BULLET "\
Use the systems bullet library (currently unsupported due to missing features in upstream!)"
  OFF
)
mark_as_advanced(WITH_SYSTEM_BULLET)
option(WITH_OPENCOLORIO "Enable OpenColorIO color management" ON)

set(_option_default ON)
if(APPLE)
  # There's no OpenXR runtime in sight for macOS, neither is code well
  # tested there -> disable it by default.
  set(_option_default OFF)
endif()
option(WITH_XR_OPENXR "Enable VR features through the OpenXR specification" ${_option_default})
if(APPLE)
  mark_as_advanced(WITH_XR_OPENXR)
endif()
unset(_option_default)

option(WITH_GMP "Enable features depending on GMP (Exact Boolean)" ON)

# Compositor
option(WITH_COMPOSITOR_CPU "Enable the tile based CPU nodal compositor" ON)
option(WITH_OPENIMAGEDENOISE "Enable the OpenImageDenoise compositing node" ON)

option(WITH_OPENSUBDIV "Enable OpenSubdiv for surface subdivision" ON)

option(WITH_POTRACE "Enable features relying on potrace" ON)
option(WITH_OPENVDB "Enable features relying on OpenVDB" ON)
option(WITH_OPENVDB_BLOSC "\
Enable blosc compression for OpenVDB, only enable if OpenVDB was built with blosc support"
  ON
)
option(WITH_OPENVDB_3_ABI_COMPATIBLE "\
Assume OpenVDB library has been compiled with version 3 ABI compatibility"
  OFF
)
mark_as_advanced(WITH_OPENVDB_3_ABI_COMPATIBLE)
option(WITH_NANOVDB "Enable usage of NanoVDB data structure for rendering on the GPU" ON)
option(WITH_HARU "Enable features relying on Libharu (Grease pencil PDF export)" ON)

# GHOST Windowing Library Options
option(WITH_GHOST_DEBUG "Enable debugging output for the GHOST library" OFF)
mark_as_advanced(WITH_GHOST_DEBUG)

option(WITH_GHOST_SDL "\
Enable building Blender against SDL for windowing rather than the native APIs"
  OFF
)
mark_as_advanced(WITH_GHOST_SDL)

if(UNIX AND NOT (APPLE OR HAIKU))
  option(WITH_GHOST_X11 "Enable building Blender against X11 for windowing" ON)
  mark_as_advanced(WITH_GHOST_X11)

  option(WITH_GHOST_WAYLAND "Enable building Blender against Wayland for windowing" ON)
  mark_as_advanced(WITH_GHOST_WAYLAND)

  if(WITH_GHOST_WAYLAND)
    option(WITH_GHOST_WAYLAND_LIBDECOR "Optionally build with LibDecor window decorations" ON)
    mark_as_advanced(WITH_GHOST_WAYLAND_LIBDECOR)

    option(WITH_GHOST_WAYLAND_DYNLOAD "Enable runtime dynamic WAYLAND libraries loading" ON)
    mark_as_advanced(WITH_GHOST_WAYLAND_DYNLOAD)

    set(WITH_GHOST_WAYLAND_APP_ID "" CACHE STRING "\
The application ID used for Blender (use default when an empty string), \
this can be used to differentiate Blender instances by version or branch for example."
    )
    mark_as_advanced(WITH_GHOST_WAYLAND_APP_ID)
  endif()
endif()

if(WITH_GHOST_X11)
  option(WITH_GHOST_XDND "Enable drag'n'drop support on X11 using XDND protocol" ON)
endif()

# Misc...
option(WITH_HEADLESS "Build without graphical support (renderfarm, server mode only)" OFF)
mark_as_advanced(WITH_HEADLESS)

option(WITH_QUADRIFLOW "Build with quadriflow remesher support" ON)

option(WITH_AUDASPACE "\
Build with blenders audio library (only disable if you know what you're doing!)"
  ON
)
option(WITH_SYSTEM_AUDASPACE "\
Build with external audaspace library installed on the system \
(only enable if you know what you're doing!)"
  OFF
)
mark_as_advanced(WITH_AUDASPACE)
mark_as_advanced(WITH_SYSTEM_AUDASPACE)

set_and_warn_dependency(WITH_AUDASPACE WITH_SYSTEM_AUDASPACE OFF)

option(WITH_OPENMP "Enable OpenMP (has to be supported by the compiler)" ON)
if(UNIX AND NOT APPLE)
  option(WITH_OPENMP_STATIC "Link OpenMP statically (only used by the release environment)" OFF)
  mark_as_advanced(WITH_OPENMP_STATIC)
endif()

if(WITH_GHOST_X11)
  option(WITH_X11_XINPUT "Enable X11 Xinput (tablet support and unicode input)" ON)
  option(WITH_X11_XF86VMODE "Enable X11 video mode switching" ON)
  option(WITH_X11_XFIXES "Enable X11 XWayland cursor warping workaround" ON)
endif()

if(UNIX AND NOT APPLE)
  option(WITH_SYSTEM_FREETYPE "Use the freetype library provided by the operating system" OFF)
  option(WITH_SYSTEM_EIGEN3 "Use the systems Eigen3 library" OFF)
else()
  set(WITH_SYSTEM_FREETYPE OFF)
  set(WITH_SYSTEM_EIGEN3 OFF)
endif()


# Modifiers
option(WITH_MOD_FLUID "Enable Mantaflow Fluid Simulation Framework" ON)
option(WITH_MOD_REMESH "Enable Remesh Modifier" ON)
option(WITH_MOD_OCEANSIM "Enable Ocean Modifier" ON)

# Image format support
option(WITH_IMAGE_OPENEXR "Enable OpenEXR Support (http://www.openexr.com)" ON)
option(WITH_IMAGE_OPENJPEG "Enable OpenJpeg Support (http://www.openjpeg.org)" ON)
option(WITH_IMAGE_CINEON "Enable CINEON and DPX Image Support" ON)
option(WITH_IMAGE_WEBP "Enable WebP Image Support" ON)

# Audio/Video format support
option(WITH_CODEC_AVI "Enable Blenders own AVI file support (raw/jpeg)" ON)
option(WITH_CODEC_FFMPEG "Enable FFMPeg Support (http://ffmpeg.org)" ON)
option(WITH_CODEC_SNDFILE "Enable libsndfile Support (http://www.mega-nerd.com/libsndfile)" ON)

# Alembic support
option(WITH_ALEMBIC "Enable Alembic Support" ON)

# Universal Scene Description support
option(WITH_USD "Enable Universal Scene Description (USD) Support" ON)

# MaterialX
option(WITH_MATERIALX "Enable MaterialX Support" ON)

# Hydra render engine
option(WITH_HYDRA "Enable Hydra render engine" ON)

# RTL Languages, Complex Shaping, OpenType Features
option(WITH_FRIBIDI "Enable features relying on fribidi" OFF)
option(WITH_HARFBUZZ "Enable features relying on harfbuzz" OFF)

# 3D format support
# Disable opencollada when we don't have precompiled libs
option(WITH_OPENCOLLADA "Enable OpenCollada Support (http://www.opencollada.org)" ON)
option(WITH_IO_WAVEFRONT_OBJ "Enable Wavefront-OBJ 3D file format support (*.obj)" ON)
option(WITH_IO_PLY "Enable PLY 3D file format support (*.ply)" ON)
option(WITH_IO_STL "Enable STL 3D file format support (*.stl)" ON)
option(WITH_IO_GPENCIL "Enable grease-pencil file format IO (*.svg, *.pdf)" ON)

# Sound output
option(WITH_SDL "Enable SDL for sound" ON)
option(WITH_OPENAL "Enable OpenAL Support (http://www.openal.org)" ON)
if(APPLE)
  option(WITH_COREAUDIO "Enable CoreAudio for audio support on macOS" ON)
else()
  set(WITH_COREAUDIO OFF)
endif()
if(NOT WIN32)
  set(_option_default ON)
  if(APPLE)
    set(_option_default OFF)
  endif()
  option(WITH_JACK "Enable JACK Support (http://www.jackaudio.org)" ${_option_default})
  unset(_option_default)
  option(WITH_JACK_DYNLOAD "Enable runtime dynamic JACK libraries loading" OFF)
else()
  set(WITH_JACK OFF)
endif()
if(UNIX AND NOT APPLE)
  option(WITH_SDL_DYNLOAD "Enable runtime dynamic SDL libraries loading" OFF)
endif()
if(UNIX AND NOT APPLE)
  option(WITH_PULSEAUDIO "Enable PulseAudio for audio support on Linux" ON)
  option(WITH_PULSEAUDIO_DYNLOAD "Enable runtime dynamic PulseAudio libraries loading" OFF)
else()
  set(WITH_PULSEAUDIO OFF)
endif()
if(WIN32)
  option(WITH_WASAPI "Enable Windows Audio Sessions API for audio support on Windows" ON)
else()
  set(WITH_WASAPI OFF)
endif()

# Compression
option(WITH_LZO "Enable fast LZO compression (used for pointcache)" ON)
option(WITH_LZMA "Enable best LZMA compression, (used for pointcache)" ON)
if(UNIX AND NOT APPLE)
  option(WITH_SYSTEM_LZO "Use the system LZO library" OFF)
endif()
option(WITH_DRACO "Enable Draco mesh compression Python module (used for glTF)" ON)

# Camera/motion tracking
option(WITH_LIBMV "Enable Libmv structure from motion library" ON)
option(WITH_LIBMV_SCHUR_SPECIALIZATIONS "Enable fixed-size schur specializations." ON)
mark_as_advanced(WITH_LIBMV_SCHUR_SPECIALIZATIONS)

# Logging/unbit test libraries.
option(WITH_SYSTEM_GFLAGS "Use system-wide Gflags instead of a bundled one" OFF)
option(WITH_SYSTEM_GLOG "Use system-wide Glog instead of a bundled one" OFF)
mark_as_advanced(WITH_SYSTEM_GFLAGS)
mark_as_advanced(WITH_SYSTEM_GLOG)

# Freestyle
option(WITH_FREESTYLE "Enable Freestyle (advanced edges rendering)" ON)

# Libraries.
if(UNIX AND NOT APPLE)
  # Optionally build without pre-compiled libraries.
  # NOTE: this could be supported on all platforms however in practice UNIX is the only platform
  # that has good support for detecting installed libraries.
  option(WITH_LIBS_PRECOMPILED "\
Detect and link against pre-compiled libraries (typically found under \"../lib/\"). \
Disabling this option will use the system libraries although cached paths \
that point to pre-compiled libraries will be left as-is."
    ON
  )
  mark_as_advanced(WITH_LIBS_PRECOMPILED)

  option(WITH_STATIC_LIBS "\
Try to link with static libraries, as much as possible, \
to make blender more portable across distributions"
    OFF
  )
  if(WITH_STATIC_LIBS)
    option(WITH_BOOST_ICU "\
Boost uses ICU library (required for linking with static Boost built with libicu)."
      OFF
    )
    mark_as_advanced(WITH_BOOST_ICU)
  endif()
endif()

# Misc
if(WIN32 OR APPLE OR ((UNIX AND (NOT HAIKU)) AND WITH_GHOST_WAYLAND))
  option(WITH_INPUT_IME "Enable Input Method Editor (IME) for complex Asian character input" ON)
else()
  set(WITH_INPUT_IME OFF)
endif()
option(WITH_INPUT_NDOF "Enable NDOF input devices (SpaceNavigator and friends)" ON)
# On Windows and for the Blender application on macOS, portable install
# is the only supported installation type, so there is no option.
if(UNIX AND (NOT APPLE OR WITH_PYTHON_MODULE))
  option(WITH_INSTALL_PORTABLE "\
Install redistributable runtime, otherwise install into CMAKE_INSTALL_PREFIX"
    ON
  )
endif()

option(WITH_PYTHON_INSTALL "Copy system python into the blender install folder" ON)

option(WITH_INSTALL_COPYRIGHT "\
Copy the official Blender Authors's copyright.txt into the Blender install folder"
  OFF
)
mark_as_advanced(WITH_INSTALL_COPYRIGHT)

if((WITH_AUDASPACE AND NOT WITH_SYSTEM_AUDASPACE) OR WITH_MOD_FLUID)
  option(WITH_PYTHON_NUMPY "Include NumPy in Blender (used by Audaspace and Mantaflow)" ON)
endif()

if(WIN32 OR APPLE)
  # Windows and macOS have this bundled with Python libraries.
elseif(WITH_PYTHON_INSTALL OR WITH_PYTHON_NUMPY)
  set(PYTHON_NUMPY_PATH "" CACHE PATH "\
Path to python site-packages or dist-packages containing 'numpy' module"
  )
  mark_as_advanced(PYTHON_NUMPY_PATH)
  set(PYTHON_NUMPY_INCLUDE_DIRS "" CACHE PATH "Path to the include directory of the NumPy module")
  mark_as_advanced(PYTHON_NUMPY_INCLUDE_DIRS)
endif()
if(WITH_PYTHON_INSTALL)
  option(WITH_PYTHON_INSTALL_NUMPY "Copy system NumPy into the blender install folder" ON)

  if(UNIX AND NOT APPLE)
    option(WITH_PYTHON_INSTALL_REQUESTS "Copy system requests into the blender install folder" ON)
    set(PYTHON_REQUESTS_PATH "" CACHE PATH "\
Path to python site-packages or dist-packages containing 'requests' module"
    )
    mark_as_advanced(PYTHON_REQUESTS_PATH)
  endif()

  option(WITH_PYTHON_INSTALL_ZSTANDARD "Copy zstandard into the blender install folder" ON)
  set(PYTHON_ZSTANDARD_PATH "" CACHE PATH "\
Path to python site-packages or dist-packages containing 'zstandard' module"
  )
  mark_as_advanced(PYTHON_ZSTANDARD_PATH)
endif()

option(WITH_CPU_SIMD "Enable SIMD instruction if they're detected on the host machine" ON)
mark_as_advanced(WITH_CPU_SIMD)

# Cycles
option(WITH_CYCLES "Enable Cycles Render Engine" ON)
option(WITH_CYCLES_OSL "Build Cycles with OpenShadingLanguage support" ON)
option(WITH_CYCLES_PATH_GUIDING "Build Cycles with path guiding support" ON)
option(WITH_CYCLES_EMBREE "Build Cycles with Embree support" ON)
option(WITH_CYCLES_LOGGING "Build Cycles with logging support" ON)
option(WITH_CYCLES_DEBUG "Build Cycles with options useful for debugging (e.g., MIS)" OFF)

option(WITH_CYCLES_STANDALONE "Build Cycles standalone application" OFF)
option(WITH_CYCLES_STANDALONE_GUI "Build Cycles standalone with GUI" OFF)
option(WITH_CYCLES_PRECOMPUTE "Build Cycles data precomputation tool" OFF)

option(WITH_CYCLES_HYDRA_RENDER_DELEGATE "Build Cycles Hydra render delegate" OFF)

option(WITH_CYCLES_DEBUG_NAN "\
Build Cycles with additional asserts for detecting NaNs and invalid values"
  OFF
)
option(WITH_CYCLES_NATIVE_ONLY "\
Build Cycles with native kernel only (which fits current CPU, use for development only)"
  OFF
)
option(WITH_CYCLES_KERNEL_ASAN "\
Build Cycles kernels with address sanitizer when WITH_COMPILER_ASAN is on, even if it's very slow"
  OFF
)
set(CYCLES_TEST_DEVICES CPU CACHE STRING "\
Run regression tests on the specified device types (CPU CUDA OPTIX HIP)"
)
mark_as_advanced(WITH_CYCLES_KERNEL_ASAN)
mark_as_advanced(WITH_CYCLES_LOGGING)
mark_as_advanced(WITH_CYCLES_DEBUG_NAN)
mark_as_advanced(WITH_CYCLES_NATIVE_ONLY)
mark_as_advanced(WITH_CYCLES_PRECOMPUTE)
mark_as_advanced(CYCLES_TEST_DEVICES)

# NVIDIA CUDA & OptiX
if(NOT APPLE)
  option(WITH_CYCLES_DEVICE_CUDA "Enable Cycles NVIDIA CUDA compute support" ON)
  option(WITH_CYCLES_DEVICE_OPTIX "Enable Cycles NVIDIA OptiX support" ON)
  mark_as_advanced(WITH_CYCLES_DEVICE_CUDA)

  option(WITH_CYCLES_CUDA_BINARIES "Build Cycles NVIDIA CUDA binaries" OFF)
  set(CYCLES_CUDA_BINARIES_ARCH
    sm_30 sm_35 sm_37 sm_50 sm_52 sm_60 sm_61 sm_70 sm_75 sm_86 sm_89 compute_75
    CACHE STRING "CUDA architectures to build binaries for"
  )
  option(WITH_CYCLES_CUDA_BUILD_SERIAL "\
Build cubins one after another (useful on machines with limited RAM)"
    OFF
  )
  option(WITH_CUDA_DYNLOAD "\
Dynamically load CUDA libraries at runtime (for developers, makes cuda-gdb work)"
    ON
  )

  set(OPTIX_ROOT_DIR "" CACHE PATH "\
Path to the OptiX SDK root directory, for building Cycles OptiX kernels."
  )
  set(CYCLES_RUNTIME_OPTIX_ROOT_DIR "" CACHE PATH "\
Path to the OptiX SDK root directory. \
When set, this path will be used at runtime to compile OptiX kernels."
  )

  mark_as_advanced(CYCLES_CUDA_BINARIES_ARCH)
  mark_as_advanced(WITH_CYCLES_CUDA_BUILD_SERIAL)
  mark_as_advanced(WITH_CUDA_DYNLOAD)
  mark_as_advanced(OPTIX_ROOT_DIR)
  mark_as_advanced(CYCLES_RUNTIME_OPTIX_ROOT_DIR)
endif()

# AMD HIP
if(NOT APPLE)
  option(WITH_CYCLES_DEVICE_HIP "Enable Cycles AMD HIP support" ON)
  option(WITH_CYCLES_HIP_BINARIES "Build Cycles AMD HIP binaries" OFF)
  # Radeon VII (gfx906) not currently working with HIP SDK, so left out of the list.
  set(CYCLES_HIP_BINARIES_ARCH
    gfx900 gfx90c gfx902
    gfx1010 gfx1011 gfx1012
    gfx1030 gfx1031 gfx1032 gfx1034 gfx1035 gfx1036
    gfx1100 gfx1101 gfx1102 gfx1103
    CACHE STRING "AMD HIP architectures to build binaries for"
  )
  mark_as_advanced(WITH_CYCLES_DEVICE_HIP)
  mark_as_advanced(CYCLES_HIP_BINARIES_ARCH)

  # HIPRT is only available on Windows for now.
  if(WIN32)
    option(WITH_CYCLES_DEVICE_HIPRT "Enable Cycles AMD HIPRT support" OFF)
    mark_as_advanced(WITH_CYCLES_DEVICE_HIPRT)
  endif()
endif()

# Apple Metal
if(APPLE)
  option(WITH_CYCLES_DEVICE_METAL "Enable Cycles Apple Metal compute support" ON)
endif()

# oneAPI
if(NOT APPLE)
  option(WITH_CYCLES_DEVICE_ONEAPI "Enable Cycles oneAPI compute support" OFF)
  option(WITH_CYCLES_ONEAPI_BINARIES "\
Enable Ahead-Of-Time compilation for Cycles oneAPI device"
    OFF
  )
  option(WITH_CYCLES_ONEAPI_HOST_TASK_EXECUTION "\
Switch target of oneAPI implementation from SYCL devices to Host Task (single thread on CPU). \
This option is only for debugging purposes."
    OFF
  )

  # https://www.intel.com/content/www/us/en/develop/documentation/oneapi-dpcpp-cpp-compiler-dev-guide-and-reference/top/compilation/ahead-of-time-compilation.html
  # The target architectures levels can be retrieved from `ocloc` output when running
  # `ocloc compile -device {device_id} test.c` for given GPUs PCI device IDs.
  # 12.55.8 is for Arc Alchemist GPUs. 12.70.0 for Meteor Lake iGPUs.
  set(CYCLES_ONEAPI_INTEL_BINARIES_ARCH 12.55.8 12.70.0 CACHE STRING "\
oneAPI Intel GPU architectures to build binaries for"
  )
  set(CYCLES_ONEAPI_SYCL_TARGETS spir64 spir64_gen CACHE STRING "\
oneAPI targets to build AOT binaries for"
  )

  mark_as_advanced(WITH_CYCLES_ONEAPI_HOST_TASK_EXECUTION)
  mark_as_advanced(CYCLES_ONEAPI_INTEL_BINARIES_ARCH)
  mark_as_advanced(CYCLES_ONEAPI_SYCL_TARGETS)
endif()

# Draw Manager
option(WITH_DRAW_DEBUG "Add extra debug capabilities to Draw Manager" OFF)
mark_as_advanced(WITH_DRAW_DEBUG)

# LLVM
option(WITH_LLVM "Use LLVM" OFF)
option(LLVM_STATIC "Link with LLVM static libraries" OFF)
mark_as_advanced(LLVM_STATIC)
option(WITH_CLANG "Use Clang" OFF)

# disable for now, but plan to support on all platforms eventually
option(WITH_MEM_JEMALLOC "Enable malloc replacement (http://www.canonware.com/jemalloc)" ON)
mark_as_advanced(WITH_MEM_JEMALLOC)

# currently only used for BLI_mempool
option(WITH_MEM_VALGRIND "Enable extended valgrind support for better reporting" OFF)
mark_as_advanced(WITH_MEM_VALGRIND)

# Debug
option(WITH_CXX_GUARDEDALLOC "\
Enable GuardedAlloc for C++ memory allocation tracking (only enable for development)"
  OFF
)
mark_as_advanced(WITH_CXX_GUARDEDALLOC)

option(WITH_ASSERT_ABORT "Call abort() when raising an assertion through BLI_assert()" ON)
mark_as_advanced(WITH_ASSERT_ABORT)

option(WITH_ASSERT_RELEASE "Build with asserts enabled even for non-debug configurations" OFF)
mark_as_advanced(WITH_ASSERT_RELEASE)

if((UNIX AND NOT APPLE) OR (CMAKE_GENERATOR MATCHES "^Visual Studio.+"))
  option(WITH_CLANG_TIDY "\
Use Clang Tidy to analyze the source code \
(only enable for development on Linux using Clang, or Windows using the Visual Studio IDE)"
    OFF
  )
  mark_as_advanced(WITH_CLANG_TIDY)
endif()

option(WITH_BOOST "Enable features depending on boost" ON)
option(WITH_TBB "\
Enable multi-threading. TBB is also required for features such as Cycles, OpenVDB and USD"
  ON
)

# TBB malloc is only supported on for windows currently
if(WIN32)
  option(WITH_TBB_MALLOC_PROXY "Enable the TBB malloc replacement" ON)
endif()

# This should be turned off when Blender enter beta/rc/release
if("${BLENDER_VERSION_CYCLE}" STREQUAL "alpha")
  set(WITH_EXPERIMENTAL_FEATURES ON)
else()
  set(WITH_EXPERIMENTAL_FEATURES OFF)
endif()

# Unit testing
option(WITH_GTESTS "Enable GTest unit testing" OFF)
option(WITH_GPU_RENDER_TESTS "Enable GPU render related unit testing (EEVEE, Workbench and Grease Pencil)" OFF)
option(WITH_GPU_RENDER_TESTS_SILENT "Run GPU render tests silently (finished tests will pass). Generated report will show failing tests" ON)
option(WITH_GPU_DRAW_TESTS "Enable GPU drawing related unit testing (GPU backends and draw manager)" OFF)
option(WITH_COMPOSITOR_REALTIME_TESTS "Enable regression testing for realtime compositor" OFF)
if(UNIX AND NOT (APPLE OR HAIKU))
  option(WITH_UI_TESTS "\
Enable user-interface tests using a headless display server. \
Currently this depends on WITH_GHOST_WAYLAND and the weston compositor \
(Experimental)"
    OFF
  )
else()
  # TODO: support running GUI tests on other platforms.
  set(WITH_UI_TESTS OFF)
endif()

# Enabled by default for typical use cases to speed up development cycles. However, when looking
# into threading or memory related issues (in dependency graph, out-of-bounds, etc) forcing single
# test per Blender instance could give much better clues about the root of the problem.
option(WITH_TESTS_BATCHED "Run multiple tests in a single Blender invocation, for faster test execution" ON)
mark_as_advanced(WITH_TESTS_BATCHED)

option(WITH_TESTS_SINGLE_BINARY "\
Link GTest tests into a single binary. \
For faster overall build and less disk space, but slower individual test build"
  ON
)
mark_as_advanced(WITH_TESTS_SINGLE_BINARY)

# NOTE: All callers of this must add `TEST_PYTHON_EXE_EXTRA_ARGS` before any other arguments.
set(TEST_PYTHON_EXE "" CACHE PATH "Python executable to run unit tests")
mark_as_advanced(TEST_PYTHON_EXE)

# Documentation
if(UNIX AND NOT APPLE)
  option(WITH_DOC_MANPAGE "Create a manual page (Unix manpage)" OFF)
endif()


# GPU Module
option(WITH_GPU_BUILDTIME_SHADER_BUILDER "\
Shader builder is a developer option enabling linting on GLSL during compilation"
  OFF
)
option(WITH_RENDERDOC "Use Renderdoc API to capture frames" OFF)

mark_as_advanced(
  WITH_GPU_BUILDTIME_SHADER_BUILDER
  WITH_RENDERDOC
)

# OpenGL
if(NOT APPLE)
  option(WITH_OPENGL_BACKEND "Enable OpenGL support as graphic backend" ON)
  mark_as_advanced(WITH_OPENGL_BACKEND)
else()
  set(WITH_OPENGL_BACKEND OFF)
endif()

# Vulkan
if(NOT APPLE)
  option(WITH_VULKAN_BACKEND "Enable Vulkan as graphics backend (experimental)" ON)
  option(WITH_VULKAN_GUARDEDALLOC "\
Use guardedalloc for host allocations done inside Vulkan (development option)"
    OFF
  )
  mark_as_advanced(
    WITH_VULKAN_BACKEND
    WITH_VULKAN_GUARDEDALLOC
  )
  if(NOT WITH_EXPERIMENTAL_FEATURES)
    set(WITH_VULKAN_BACKEND OFF)
  endif()
endif()

# Metal
if(APPLE)
  option(WITH_METAL_BACKEND "\
Use Metal for graphics instead of (or as well as) OpenGL on macOS."
    ON
  )
  mark_as_advanced(WITH_METAL_BACKEND)
else()
  set(WITH_METAL_BACKEND OFF)
endif()

if(WIN32)
  getDefaultWindowsPrefixBase(CMAKE_GENERIC_PROGRAM_FILES)
  set(CPACK_INSTALL_PREFIX ${CMAKE_GENERIC_PROGRAM_FILES}/${})
endif()

option(WITH_STRSIZE_DEBUG "\
Ensure string operations on fixed size buffers \
(works well with with \"WITH_COMPILER_ASAN\" & valgrind to detect incorrect buffer size arguments)"
  OFF)
mark_as_advanced(WITH_STRSIZE_DEBUG)

# Compiler tool-chain.
if(UNIX)
  if(CMAKE_COMPILER_IS_GNUCC)
    option(WITH_LINKER_GOLD "Use ld.gold linker which is usually faster than ld.bfd" ON)
    mark_as_advanced(WITH_LINKER_GOLD)
  endif()
  if(CMAKE_COMPILER_IS_GNUCC OR CMAKE_C_COMPILER_ID MATCHES "Clang")
    option(WITH_LINKER_LLD "Use ld.lld linker which is usually faster than ld.gold" OFF)
    mark_as_advanced(WITH_LINKER_LLD)
    option(WITH_LINKER_MOLD "Use ld.mold linker which is usually faster than ld.gold & ld.lld" OFF)
    mark_as_advanced(WITH_LINKER_MOLD)
  endif()
endif()

option(WITH_COMPILER_ASAN "\
Build and link against address sanitizer (only for Debug & RelWithDebInfo targets)."
  OFF
)
mark_as_advanced(WITH_COMPILER_ASAN)
option(WITH_COMPILER_ASAN_EXTERN "\
Build `extern` dependencies with address sanitizer when WITH_COMPILER_ASAN is on. \
Can cause linking issues due to too large binary size."
  OFF
)
mark_as_advanced(WITH_COMPILER_ASAN_EXTERN)

if(CMAKE_COMPILER_IS_GNUCC OR CMAKE_C_COMPILER_ID MATCHES "Clang")
  if(WITH_COMPILER_ASAN)
    set(_asan_defaults "\
-fsanitize=address \
-fsanitize=bool \
-fsanitize=bounds \
-fsanitize=enum \
-fsanitize=float-cast-overflow \
-fsanitize=float-divide-by-zero \
-fsanitize=nonnull-attribute \
-fsanitize=returns-nonnull-attribute \
-fsanitize=signed-integer-overflow \
-fsanitize=undefined \
-fsanitize=vla-bound \
-fno-sanitize=alignment \
")

    if(MSVC)
      # clang-cl doesn't support all sanitizers, but leak and object-size give errors/warnings.
      set(_asan_defaults "${_asan_defaults}")
    elseif(APPLE)
      # AppleClang doesn't support all sanitizers, but leak gives error.
      # Build type is not known for multi-config generator, so don't add object-size sanitizer.
      if(CMAKE_BUILD_TYPE MATCHES "Debug" OR GENERATOR_IS_MULTI_CONFIG)
        # Silence the warning that object-size is not effective in -O0.
        set(_asan_defaults "${_asan_defaults}")
      else()
        string(APPEND _asan_defaults " -fsanitize=object-size")
      endif()
    elseif(CMAKE_COMPILER_IS_GNUCC)
      string(APPEND _asan_defaults " -fsanitize=leak -fsanitize=object-size")
    else()
      string(APPEND _asan_defaults " -fsanitize=leak")
    endif()

    set(COMPILER_ASAN_CFLAGS "${_asan_defaults}" CACHE STRING "C flags for address sanitizer")
    mark_as_advanced(COMPILER_ASAN_CFLAGS)
    set(COMPILER_ASAN_CXXFLAGS "${_asan_defaults}" CACHE STRING "C++ flags for address sanitizer")
    mark_as_advanced(COMPILER_ASAN_CXXFLAGS)

    unset(_asan_defaults)

    if(MSVC)
      find_library(
        COMPILER_ASAN_LIBRARY NAMES clang_rt.asan-x86_64
        PATHS
        [HKEY_LOCAL_MACHINE\\SOFTWARE\\Wow6432Node\\LLVM\\LLVM;]/lib/clang/7.0.0/lib/windows
        [HKEY_LOCAL_MACHINE\\SOFTWARE\\Wow6432Node\\LLVM\\LLVM;]/lib/clang/6.0.0/lib/windows
      )
      mark_as_advanced(COMPILER_ASAN_LIBRARY)
    elseif(APPLE)
      execute_process(COMMAND ${CMAKE_CXX_COMPILER}
        -print-file-name=lib
        OUTPUT_VARIABLE CLANG_LIB_DIR
      )
      string(STRIP "${CLANG_LIB_DIR}" CLANG_LIB_DIR)
      find_library(
        COMPILER_ASAN_LIBRARY
        NAMES
          libclang_rt.asan_osx_dynamic.dylib
        PATHS
          "${CLANG_LIB_DIR}/darwin/"
      )
      unset(CLANG_LIB_DIR)
      mark_as_advanced(COMPILER_ASAN_LIBRARY)
    elseif(CMAKE_COMPILER_IS_GNUCC)
      find_library(
        COMPILER_ASAN_LIBRARY asan ${CMAKE_C_IMPLICIT_LINK_DIRECTORIES}
      )
      mark_as_advanced(COMPILER_ASAN_LIBRARY)
    endif()

  endif()
endif()

if(CMAKE_COMPILER_IS_GNUCC OR CMAKE_C_COMPILER_ID MATCHES "Clang")
  option(WITH_COMPILER_SHORT_FILE_MACRO "\
Make paths in macros like __FILE__ relative to top level source and build directories."
    ON
  )
  mark_as_advanced(WITH_COMPILER_SHORT_FILE_MACRO)
endif()

if(WIN32)
  # Use hardcoded paths or find_package to find externals
  option(WITH_WINDOWS_FIND_MODULES "Use find_package to locate libraries" OFF)
  mark_as_advanced(WITH_WINDOWS_FIND_MODULES)

  option(WINDOWS_PYTHON_DEBUG "\
Include the files needed for debugging python scripts with visual studio 2017+."
    OFF
  )
  mark_as_advanced(WINDOWS_PYTHON_DEBUG)

  option(WITH_WINDOWS_BUNDLE_CRT "Bundle the C runtime for install free distribution." ON)
  mark_as_advanced(WITH_WINDOWS_BUNDLE_CRT)

  option(WITH_WINDOWS_SCCACHE "Use sccache to speed up builds (Ninja builder only)" OFF)
  mark_as_advanced(WITH_WINDOWS_SCCACHE)

  option(WITH_WINDOWS_RELEASE_PDB "\
Generate a pdb file for client side stacktraces for release builds"
    ON
  )
  mark_as_advanced(WITH_WINDOWS_RELEASE_PDB)

  option(WITH_WINDOWS_RELEASE_STRIPPED_PDB "Use a stripped PDB file for release builds" ON)
  mark_as_advanced(WITH_WINDOWS_RELEASE_STRIPPED_PDB)

endif()

if(WIN32 OR XCODE)
  option(IDE_GROUP_SOURCES_IN_FOLDERS "\
Organize the source files in filters matching the source folders."
    ON
  )
  mark_as_advanced(IDE_GROUP_SOURCES_IN_FOLDERS)

  option(IDE_GROUP_PROJECTS_IN_FOLDERS "\
Organize the projects according to source folder structure."
    ON
  )
  mark_as_advanced(IDE_GROUP_PROJECTS_IN_FOLDERS)

  if(IDE_GROUP_PROJECTS_IN_FOLDERS)
    set_property(GLOBAL PROPERTY USE_FOLDERS ON)
  endif()
endif()

if(UNIX)
  # See WITH_WINDOWS_SCCACHE for Windows.
  option(WITH_COMPILER_CCACHE "\
Use ccache to improve rebuild times (Works with Ninja, Makefiles and Xcode)"
    OFF
  )
  mark_as_advanced(WITH_COMPILER_CCACHE)
endif()

# The following only works with the Ninja generator in CMake >= 3.0.
if("${CMAKE_GENERATOR}" MATCHES "Ninja")
  option(WITH_NINJA_POOL_JOBS "\
Enable Ninja pools of jobs, to try to ease building on machines with 16GB of RAM or less \
(if not yet defined, will try to set best values based on detected machine specifications)."
    ON
  )
  mark_as_advanced(WITH_NINJA_POOL_JOBS)
endif()

# Installation process.
set(POSTINSTALL_SCRIPT "" CACHE FILEPATH "Run given CMake script after installation process")
mark_as_advanced(POSTINSTALL_SCRIPT)

set(POSTCONFIGURE_SCRIPT "" CACHE FILEPATH "\
Run given CMake script as the last step of CMake configuration"
)
mark_as_advanced(POSTCONFIGURE_SCRIPT)

# end option(...)



# By default we want to install to the directory we are compiling our executables
# unless specified otherwise, which we currently do not allow
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
  if(WIN32)
    set(CMAKE_INSTALL_PREFIX ${EXECUTABLE_OUTPUT_PATH}/\${BUILD_TYPE} CACHE PATH "\
default install path"
      FORCE
    )
  elseif(APPLE)
    set(CMAKE_INSTALL_PREFIX ${EXECUTABLE_OUTPUT_PATH}/\${BUILD_TYPE} CACHE PATH "\
default install path"
      FORCE
    )
  else()
    if(WITH_INSTALL_PORTABLE)
      set(CMAKE_INSTALL_PREFIX ${EXECUTABLE_OUTPUT_PATH} CACHE PATH "default install path" FORCE)
    endif()
  endif()
endif()

# Effective install path including config folder, as a generator expression.
get_property(GENERATOR_IS_MULTI_CONFIG GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(GENERATOR_IS_MULTI_CONFIG)
  string(
    REPLACE "\${BUILD_TYPE}" "$<CONFIG>"
    CMAKE_INSTALL_PREFIX_WITH_CONFIG ${CMAKE_INSTALL_PREFIX}
  )
else()
  string(
    REPLACE "\${BUILD_TYPE}" ""
    CMAKE_INSTALL_PREFIX_WITH_CONFIG ${CMAKE_INSTALL_PREFIX}
  )
endif()


# Apple

if(APPLE)
  include(platform_apple_xcode)
endif()


# -----------------------------------------------------------------------------
# Check for Conflicting/Unsupported Configurations

option(WITH_STRICT_BUILD_OPTIONS "\
When requirements for a build option are not met, error instead of disabling the option."
  OFF
)

if(NOT WITH_BLENDER AND NOT WITH_CYCLES_STANDALONE AND NOT WITH_CYCLES_HYDRA_RENDER_DELEGATE)
  message(FATAL_ERROR
    "At least one of WITH_BLENDER or WITH_CYCLES_STANDALONE "
    "or WITH_CYCLES_HYDRA_RENDER_DELEGATE "
    "must be enabled, nothing to do!"
  )
endif()

set_and_warn_dependency(WITH_AUDASPACE WITH_OPENAL OFF)
set_and_warn_dependency(WITH_AUDASPACE WITH_COREAUDIO OFF)
set_and_warn_dependency(WITH_AUDASPACE WITH_JACK OFF)
set_and_warn_dependency(WITH_AUDASPACE WITH_PULSEAUDIO OFF)
set_and_warn_dependency(WITH_AUDASPACE WITH_WASAPI OFF)

if(NOT WITH_SDL AND WITH_GHOST_SDL)
  message(FATAL_ERROR "WITH_GHOST_SDL requires WITH_SDL")
endif()

# python module, needs some different options
if(WITH_PYTHON_MODULE AND WITH_PYTHON_INSTALL)
  message(FATAL_ERROR "WITH_PYTHON_MODULE requires WITH_PYTHON_INSTALL to be OFF")
endif()

set_and_warn_dependency(WITH_PYTHON WITH_CYCLES        OFF)
set_and_warn_dependency(WITH_PYTHON WITH_DRACO         OFF)
set_and_warn_dependency(WITH_PYTHON WITH_MOD_FLUID     OFF)

if(WITH_DRACO AND NOT WITH_PYTHON_INSTALL)
  message(STATUS "WITH_DRACO requires WITH_PYTHON_INSTALL to be ON, disabling WITH_DRACO for now")
  set(WITH_DRACO OFF)
endif()

# enable boost for cycles, audaspace or i18n
# otherwise if the user disabled

set_and_warn_dependency(WITH_BOOST WITH_INTERNATIONAL  OFF)
set_and_warn_dependency(WITH_BOOST WITH_OPENVDB        OFF)
set_and_warn_dependency(WITH_BOOST WITH_QUADRIFLOW     OFF)
set_and_warn_dependency(WITH_BOOST WITH_USD            OFF)
if(WITH_CYCLES)
  set_and_warn_dependency(WITH_BOOST   WITH_CYCLES_OSL   OFF)
  set_and_warn_dependency(WITH_PUGIXML WITH_CYCLES_OSL   OFF)
endif()

set_and_warn_dependency(WITH_TBB WITH_CYCLES            OFF)
set_and_warn_dependency(WITH_TBB WITH_USD               OFF)
set_and_warn_dependency(WITH_TBB WITH_OPENVDB           OFF)
set_and_warn_dependency(WITH_TBB WITH_MOD_FLUID         OFF)

# NanoVDB requires OpenVDB to convert the data structure
set_and_warn_dependency(WITH_OPENVDB WITH_NANOVDB       OFF)

# OpenVDB, Alembic and OSL uses 'half' or 'imath' from OpenEXR
set_and_warn_dependency(WITH_IMAGE_OPENEXR WITH_OPENVDB OFF)
set_and_warn_dependency(WITH_IMAGE_OPENEXR WITH_ALEMBIC OFF)
set_and_warn_dependency(WITH_IMAGE_OPENEXR WITH_CYCLES_OSL OFF)

# Hydra requires USD.
set_and_warn_dependency(WITH_USD WITH_HYDRA OFF)

if(NOT WITH_CYCLES)
  set(WITH_CYCLES_OSL OFF)
endif()

# don't store paths to libs for portable distribution
if(WITH_INSTALL_PORTABLE)
  set(CMAKE_SKIP_BUILD_RPATH TRUE)
endif()

if(UNIX AND NOT (APPLE OR HAIKU))
  set_and_warn_incompatible(WITH_HEADLESS WITH_GHOST_WAYLAND OFF)
  set_and_warn_incompatible(WITH_HEADLESS WITH_GHOST_X11 OFF)
endif()
set_and_warn_incompatible(WITH_HEADLESS WITH_GHOST_SDL OFF)

if(WITH_INPUT_IME)
  set_and_warn_incompatible(WITH_HEADLESS WITH_INPUT_IME OFF)
  set_and_warn_incompatible(WITH_GHOST_SDL WITH_INPUT_IME OFF)
endif()

set_and_warn_incompatible(WITH_HEADLESS WITH_XR_OPENXR OFF)
set_and_warn_incompatible(WITH_GHOST_SDL WITH_XR_OPENXR OFF)

if(WITH_UI_TESTS)
  set_and_warn_dependency(WITH_GHOST_WAYLAND WITH_UI_TESTS OFF)
endif()

if(WITH_BUILDINFO)
  find_package(Git)
  set_and_warn_library_found("Git" GIT_FOUND WITH_BUILDINFO)
endif()

if(WITH_AUDASPACE)
  if(NOT WITH_SYSTEM_AUDASPACE)
    set(AUDASPACE_C_INCLUDE_DIRS
      "${CMAKE_SOURCE_DIR}/extern/audaspace/bindings/C"
      "${CMAKE_BINARY_DIR}/extern/audaspace"
    )
    set(AUDASPACE_PY_INCLUDE_DIRS
      "${CMAKE_SOURCE_DIR}/extern/audaspace/bindings"
    )
  endif()
endif()

# Auto-enable CUDA dynload if toolkit is not found.
if(WITH_CYCLES AND WITH_CYCLES_DEVICE_CUDA AND NOT WITH_CUDA_DYNLOAD)
  find_package(CUDA)
  if(NOT CUDA_FOUND)
    message(
      STATUS
      "CUDA toolkit not found, "
      "using dynamic runtime loading of libraries (WITH_CUDA_DYNLOAD) instead"
    )
    set(WITH_CUDA_DYNLOAD ON)
  endif()
endif()

if(WITH_CYCLES_DEVICE_HIP)
  # Currently HIP must be dynamically loaded, this may change in future toolkits
  set(WITH_HIP_DYNLOAD ON)
endif()


# -----------------------------------------------------------------------------
# Check if Sub-modules are Cloned

if(WITH_PYTHON)
  # While we have this as an '#error' in 'bpy_capi_utils.h',
  # upgrading Python tends to cause confusion for users who build.
  # Give the error message early to make this more obvious.
  #
  # Do this before main 'platform_*' checks,
  # because UNIX will search for the old Python paths which may not exist.
  # giving errors about missing paths before this case is met.
  if(DEFINED PYTHON_VERSION AND "${PYTHON_VERSION}" VERSION_LESS "3.11")
    message(
      FATAL_ERROR
      "At least Python 3.11 is required to build, but found Python ${PYTHON_VERSION}"
    )
  endif()

  file(GLOB RESULT "${CMAKE_SOURCE_DIR}/scripts/addons")
  list(LENGTH RESULT DIR_LEN)
  if(DIR_LEN EQUAL 0)
    message(
      WARNING
      "Addons path '${CMAKE_SOURCE_DIR}/scripts/addons' is missing. "
      "This is an external repository which needs to be checked out. Use `make update` to do so. "
      "* CONTINUING WITHOUT ADDONS *"
    )
  endif()
endif()


# -----------------------------------------------------------------------------
# InitialIze Un-cached Vars, Avoid Unused Warning

# linux only, not cached
set(WITH_BINRELOC OFF)

# MACOSX only, set to avoid uninitialized
set(EXETYPE "")

# C/C++ flags
set(PLATFORM_CFLAGS)

# these are added to later on.
set(C_WARNINGS)
set(CXX_WARNINGS)

# NOTE: These flags are intended for situations where where it's impractical to
# suppress warnings by modifying the code or for code which is maintained externally.
# For GCC this typically means adding `-Wno-*` arguments to negate warnings
# that are useful in the general case.
set(C_REMOVE_STRICT_FLAGS)
set(CXX_REMOVE_STRICT_FLAGS)

# Libraries to link to targets in setup_platform_linker_libs
set(PLATFORM_LINKLIBS "")

# Added to target linker flags in setup_platform_linker_flags
# - CMAKE_EXE_LINKER_FLAGS
# - CMAKE_EXE_LINKER_FLAGS_DEBUG
set(PLATFORM_LINKFLAGS "")
set(PLATFORM_LINKFLAGS_DEBUG "")
set(PLATFORM_LINKFLAGS_RELEASE "")
set(PLATFORM_LINKFLAGS_EXECUTABLE "")

if(NOT CMAKE_BUILD_TYPE MATCHES "Release")
  if(WITH_COMPILER_ASAN)
    if(NOT APPLE)
      # Avoid passing address sanitizer compiler flags to `try_compile`.
      # Since linker flags are not set, all compiler checks and `find_package`
      # calls that rely on `try_compile` will fail.
      # See CMP0066 also.
      string(APPEND CMAKE_C_FLAGS_DEBUG " ${COMPILER_ASAN_CFLAGS}")
      string(APPEND CMAKE_C_FLAGS_RELWITHDEBINFO " ${COMPILER_ASAN_CFLAGS}")

      string(APPEND CMAKE_CXX_FLAGS_DEBUG " ${COMPILER_ASAN_CXXFLAGS}")
      string(APPEND CMAKE_CXX_FLAGS_RELWITHDEBINFO " ${COMPILER_ASAN_CXXFLAGS}")
    endif()
    if(MSVC)
      set(COMPILER_ASAN_LINKER_FLAGS "/FUNCTIONPADMIN:6")
    endif()

    if(APPLE AND COMPILER_ASAN_LIBRARY)
      string(REPLACE " " ";" _list_COMPILER_ASAN_CFLAGS ${COMPILER_ASAN_CFLAGS})
      set(_is_CONFIG_DEBUG "$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>")
      add_compile_options("$<${_is_CONFIG_DEBUG}:${_list_COMPILER_ASAN_CFLAGS}>")

      # Skip generation of the unwind tables, as they might require a lot of space when sanitizers
      # are enabled and not fit into the .eh_frame section. Disabling the unwind tables might have
      # side effects on code which does frame walking, such as
      #   - backtrace()
      #   - __attribute__((__cleanup__(f)))
      #   - __builtin_return_address(n), for n > 0
      #   - pthread_cleanup_push when it is implemented using __attribute__((__cleanup__(f)))
      # It should not have affect on debugging, since it uses -g flag which generates debugging
      # tables in the .debug_frame section.
      # At the time of adding these flags calling backtrace() from C code on Apple M2 did not
      # affect on the printed backtrace, and exception handling was correct as well.
      #
      # Related discussion:
      #  https://stackoverflow.com/questions/26300819/why-gcc-compiled-c-program-needs-eh-frame-section
      add_compile_options("$<${_is_CONFIG_DEBUG}:-fno-unwind-tables>")
      add_compile_options("$<${_is_CONFIG_DEBUG}:-fno-asynchronous-unwind-tables>")

      add_compile_options("$<${_is_CONFIG_DEBUG}:-fno-omit-frame-pointer>")
      add_link_options("$<${_is_CONFIG_DEBUG}:-fno-omit-frame-pointer;-fsanitize=address>")
      unset(_list_COMPILER_ASAN_CFLAGS)
      unset(_is_CONFIG_DEBUG)
    elseif(COMPILER_ASAN_LIBRARY)
      set(PLATFORM_LINKLIBS "${PLATFORM_LINKLIBS};${COMPILER_ASAN_LIBRARY}")
      set(PLATFORM_LINKFLAGS "${COMPILER_ASAN_LIBRARY}")
      set(PLATFORM_LINKFLAGS_DEBUG "${COMPILER_ASAN_LIBRARY}")
      if(DEFINED COMPILER_ASAN_LINKER_FLAGS)
        set(PLATFORM_LINKFLAGS "${PLATFORM_LINKFLAGS} ${COMPILER_ASAN_LINKER_FLAGS}")
        set(PLATFORM_LINKFLAGS_DEBUG "${PLATFORM_LINKFLAGS_DEBUG} ${COMPILER_ASAN_LINKER_FLAGS}")
      endif()
    endif()
  endif()
endif()

# Test SIMD support, before platform includes to determine if sse2neon is needed.
if(WITH_CPU_SIMD)
  set(COMPILER_SSE_FLAG)
  set(COMPILER_SSE2_FLAG)

  # Test Neon first since macOS Arm can compile and run x86-64 SSE binaries.
  test_neon_support()
  if(NOT SUPPORT_NEON_BUILD)
    test_sse_support(COMPILER_SSE_FLAG COMPILER_SSE2_FLAG)
  endif()
endif()


# ----------------------------------------------------------------------------
# Main Platform Checks
#
# - UNIX
# - WIN32
# - APPLE

if(UNIX AND NOT APPLE)
  include(platform_unix)
elseif(WIN32)
  include(platform_win32)
elseif(APPLE)
  include(platform_apple)
endif()


# -----------------------------------------------------------------------------
# Common Checks for Compatible Options

if(NOT WITH_FFTW3 AND WITH_MOD_OCEANSIM)
  message(FATAL_ERROR "WITH_MOD_OCEANSIM requires WITH_FFTW3 to be ON")
endif()

if(WITH_INTERNATIONAL)
  if(NOT WITH_BOOST)
    message(
      FATAL_ERROR
      "Internationalization requires WITH_BOOST, the library may not have been found. "
      "Configure BOOST or disable WITH_INTERNATIONAL"
    )
  endif()
endif()

# Enable SIMD support if detected by `test_sse_support()` or `test_neon_support()`.
#
# This is done globally, so that all modules can use it if available, and
# because these are used in headers used by many modules.
if(WITH_CPU_SIMD)
  if(SUPPORT_NEON_BUILD)
    # Neon
    if(SSE2NEON_FOUND)
      include_directories(SYSTEM "${SSE2NEON_INCLUDE_DIRS}")
      add_definitions(-DWITH_SSE2NEON)
    endif()
  else()
    # SSE
    if(SUPPORT_SSE_BUILD)
      string(PREPEND PLATFORM_CFLAGS "${COMPILER_SSE_FLAG} ")
      add_definitions(-D__SSE__ -D__MMX__)
    endif()
    if(SUPPORT_SSE2_BUILD)
      string(APPEND PLATFORM_CFLAGS " ${COMPILER_SSE2_FLAG}")
      add_definitions(-D__SSE2__)
      if(NOT SUPPORT_SSE_BUILD) # don't double up
        add_definitions(-D__MMX__)
      endif()
    endif()
  endif()
endif()

# Print instructions used on first run.
if(FIRST_RUN)
  if(WITH_CPU_SIMD)
    if(SUPPORT_NEON_BUILD)
      if(SSE2NEON_FOUND)
        message(STATUS "Neon SIMD instructions enabled")
      else()
        message(STATUS "Neon SIMD instructions detected but unused, requires sse2neon")
      endif()
    elseif(SUPPORT_SSE2_BUILD)
      message(STATUS "SSE2 SIMD instructions enabled")
    elseif(SUPPORT_SSE_BUILD)
      message(STATUS "SSE SIMD instructions enabled")
    else()
      message(STATUS "No SIMD instructions detected")
    endif()
  else()
    message(STATUS "SIMD instructions disabled")
  endif()
endif()

# set the endian define
if(MSVC)
  # for some reason this fails on msvc
  add_definitions(-D__LITTLE_ENDIAN__)

  # OSX-Note: as we do cross-compiling with specific set architecture,
  # endianness-detection and auto-setting is counterproductive
  # so we just set endianness according CMAKE_OSX_ARCHITECTURES

elseif(CMAKE_OSX_ARCHITECTURES MATCHES i386 OR
       CMAKE_OSX_ARCHITECTURES MATCHES x86_64 OR
       CMAKE_OSX_ARCHITECTURES MATCHES arm64)
  add_definitions(-D__LITTLE_ENDIAN__)
elseif(CMAKE_OSX_ARCHITECTURES MATCHES ppc OR CMAKE_OSX_ARCHITECTURES MATCHES ppc64)
  add_definitions(-D__BIG_ENDIAN__)

else()
  include(TestBigEndian)
  test_big_endian(_SYSTEM_BIG_ENDIAN)
  if(_SYSTEM_BIG_ENDIAN)
    add_definitions(-D__BIG_ENDIAN__)
  else()
    add_definitions(-D__LITTLE_ENDIAN__)
  endif()
  unset(_SYSTEM_BIG_ENDIAN)
endif()
if(WITH_IMAGE_OPENJPEG)
  # Special handling of Windows platform where openjpeg is always static.
  if(WIN32)
    set(OPENJPEG_DEFINES "-DOPJ_STATIC")
  else()
    set(OPENJPEG_DEFINES "")
  endif()
endif()

if(NOT WITH_SYSTEM_EIGEN3)
  set(EIGEN3_INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/extern/Eigen3)
endif()

if(WITH_OPENVDB)
  list(APPEND OPENVDB_DEFINITIONS -DWITH_OPENVDB)

  if(WITH_OPENVDB_3_ABI_COMPATIBLE)
    list(APPEND OPENVDB_DEFINITIONS -DOPENVDB_3_ABI_COMPATIBLE)
  endif()

  # OpenVDB headers use deprecated TBB headers, silence warning.
  list(APPEND OPENVDB_DEFINITIONS -DTBB_SUPPRESS_DEPRECATED_MESSAGES=1)

  list(APPEND OPENVDB_INCLUDE_DIRS
    ${BOOST_INCLUDE_DIR}
    ${TBB_INCLUDE_DIRS}
    ${OPENEXR_INCLUDE_DIRS}
  )

  list(APPEND OPENVDB_LIBRARIES ${OPENEXR_LIBRARIES} ${ZLIB_LIBRARIES})

  if(WITH_OPENVDB_BLOSC)
    list(APPEND OPENVDB_DEFINITIONS -DWITH_OPENVDB_BLOSC)
    # Even when `WITH_OPENVDB_BLOSC` is set, `FindBlosc.cmake` isn't running.
    # As this might be used at some point, check the libraries are defined.
    if(DEFINED BLOSC_LIBRARIES)
      list(APPEND OPENVDB_LIBRARIES ${BLOSC_LIBRARIES})
    endif()
    list(APPEND OPENVDB_LIBRARIES ${ZLIB_LIBRARIES})
  endif()

  list(APPEND OPENVDB_LIBRARIES ${BOOST_LIBRARIES} ${TBB_LIBRARIES})
endif()

# -----------------------------------------------------------------------------
# Configure Metal

if(WITH_METAL_BACKEND)
  add_definitions(-DWITH_METAL_BACKEND)

  # No need to add frameworks here, all the ones we need for Metal and
  # Metal-OpenGL Interop are already being added by
  # build_files/cmake/platform/platform_apple.cmake
endif()


# -----------------------------------------------------------------------------
# Configure OpenMP

if(WITH_OPENMP)
  if(NOT OPENMP_CUSTOM)
    find_package(OpenMP)
  endif()

  set_and_warn_library_found("OpenMP" OPENMP_FOUND WITH_OPENMP)

  if(OPENMP_FOUND)
    if(NOT WITH_OPENMP_STATIC)
      string(APPEND CMAKE_C_FLAGS " ${OpenMP_C_FLAGS}")
      string(APPEND CMAKE_CXX_FLAGS " ${OpenMP_CXX_FLAGS}")
      string(APPEND CMAKE_EXE_LINKER_FLAGS " ${OpenMP_LINKER_FLAGS}")
      string(APPEND CMAKE_MODULE_LINKER_FLAGS " ${OpenMP_LINKER_FLAGS}")
    else()
      # Typically avoid adding flags as defines but we can't
      # pass OpenMP flags to the linker for static builds, meaning
      # we can't add any OpenMP related flags to CFLAGS variables
      # since they're passed to the linker as well.
      add_definitions("${OpenMP_C_FLAGS}")

      find_library_static(OpenMP_LIBRARIES gomp ${CMAKE_CXX_IMPLICIT_LINK_DIRECTORIES})
      mark_as_advanced(
        OpenMP_LIBRARIES
      )
    endif()
  endif()

  mark_as_advanced(
    OpenMP_C_FLAGS
    OpenMP_CXX_FLAGS
  )
endif()


# -----------------------------------------------------------------------------
# Configure Bullet

if(WITH_BULLET)
  if(WITH_SYSTEM_BULLET)
    find_package(Bullet)
    set_and_warn_library_found("Bullet" BULLET_FOUND WITH_BULLET)
  else()
    set(BULLET_INCLUDE_DIRS "${CMAKE_SOURCE_DIR}/extern/bullet2/src")
    set(BULLET_LIBRARIES "extern_bullet")
  endif()
endif()


# -----------------------------------------------------------------------------
# Configure Python

# Not currently supported due to different required Python link flags.
set_and_warn_incompatible(WITH_PYTHON_MODULE WITH_GTESTS OFF)


# -----------------------------------------------------------------------------
# Configure `GLog/GFlags`

if(WITH_LIBMV OR WITH_GTESTS OR (WITH_CYCLES AND WITH_CYCLES_LOGGING))
  if(WITH_SYSTEM_GFLAGS)
    find_package(Gflags)
    if(NOT GFLAGS_FOUND)
      message(FATAL_ERROR "System wide Gflags is requested but was not found")
    endif()
    # `FindGflags` does not define this, and we are not even sure what to use here.
    set(GFLAGS_DEFINES)
  else()
    set(GFLAGS_DEFINES
      -DGFLAGS_DLL_DEFINE_FLAG=
      -DGFLAGS_DLL_DECLARE_FLAG=
      -DGFLAGS_DLL_DECL=
    )
    set(GFLAGS_NAMESPACE "gflags")
    set(GFLAGS_LIBRARIES extern_gflags)
    set(GFLAGS_INCLUDE_DIRS "${PROJECT_SOURCE_DIR}/extern/gflags/src")
  endif()

  if(WITH_SYSTEM_GLOG)
    find_package(Glog)
    if(NOT GLOG_FOUND)
      message(FATAL_ERROR "System wide Glog is requested but was not found")
    endif()
    # `FindGlog` does not define this, and we are not even sure what to use here.
    set(GLOG_DEFINES)
  else()
    set(GLOG_DEFINES
      -DGOOGLE_GLOG_DLL_DECL=
    )
    set(GLOG_LIBRARIES extern_glog)
    if(WIN32)
      set(GLOG_INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/extern/glog/src/windows)
    else()
      set(GLOG_INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/extern/glog/include)
    endif()
  endif()
endif()


# -----------------------------------------------------------------------------
# Ninja Job Limiting

# Extra limits to number of jobs running in parallel for some kind os tasks.
# Only supported by Ninja build system currently.

if("${CMAKE_GENERATOR}" MATCHES "Ninja" AND WITH_NINJA_POOL_JOBS)
  if(NOT NINJA_MAX_NUM_PARALLEL_COMPILE_JOBS AND
     NOT NINJA_MAX_NUM_PARALLEL_COMPILE_HEAVY_JOBS AND
     NOT NINJA_MAX_NUM_PARALLEL_LINK_JOBS)
    # Try to define good default values.
    # Max mem of heavy cpp files compilation: about 2.5GB
    # Max mem during linking: about 3.3GB
    cmake_host_system_information(RESULT _NUM_CORES QUERY NUMBER_OF_LOGICAL_CORES)
    # Note: this gives mem in MB.
    cmake_host_system_information(RESULT _TOT_MEM QUERY TOTAL_PHYSICAL_MEMORY)

    # Heuristics: Assume 8Gb of RAM is needed per heavy compile job.
    # Typical RAM peak usage of these is actually less than 3GB currently,
    # but this also accounts for the part of the physical RAM being used by other unrelated
    # processes on the system, and the part being used by the 'regular' compile and linking jobs.
    #
    # Also always cap heavy jobs amount to `number of available threads - 1`, to ensure that even if
    # there would be enough RAM, the machine never ends up handling only heavy jobs at some point.
    # This can have annoying sides effects, like lack of output in the console for several minutes,
    # which can lead to a wrong detection of 'unresponsive' state by the buildbots e.g.
    #
    # Currently, these settings applied to a 64GB/16threads linux machine will use, for a full build:
    #   - release build:
    #      * RAM: typically less than 20%, with some peaks at 25%.
    #      * CPU: over 90% of usage on average over the whole build time.
    #   - debug with ASAN build:
    #      * RAM: typically less than 40%, with some peaks at 50%.
    #      * CPU: over 90% of usage on average over the whole build time.
    math(EXPR _compile_heavy_jobs "${_TOT_MEM} / 8000")
    math(EXPR _compile_heavy_jobs_max "${_NUM_CORES} - 1")
    if(${_compile_heavy_jobs} GREATER ${_compile_heavy_jobs_max})
      set(_compile_heavy_jobs ${_compile_heavy_jobs_max})
    elseif(${_compile_heavy_jobs} LESS 1)
      set(_compile_heavy_jobs 1)
    endif()
    set(NINJA_MAX_NUM_PARALLEL_COMPILE_HEAVY_JOBS "${_compile_heavy_jobs}" CACHE STRING "\
Define the maximum number of concurrent heavy compilation jobs, for ninja build system \
(used for some targets which cpp files can take several GB each during compilation)."
      FORCE
    )
    mark_as_advanced(NINJA_MAX_NUM_PARALLEL_COMPILE_HEAVY_JOBS)
    set(_compile_heavy_jobs)
    set(_compile_heavy_jobs_max)

    # Heuristics: Assume 2Gb of RAM is needed per heavy compile job.
    # Typical RAM peak usage of these is actually way less than 1GB usually,
    # but this also accounts for the part of the physical RAM being used by other unrelated
    # processes on the system, and the part being used by the 'heavy' compile and linking jobs.
    #
    # If there are 'enough' cores available, cap the maximum number of regular jobs to
    # `number of cores - 1`, otherwise allow using all cores if there is enough RAM available.
    # This allows to ensure that the heavy jobs won't get starved by too many normal jobs,
    # since the former usually take a long time to process.
    math(EXPR _compile_jobs "${_TOT_MEM} / 2000")
    if(${_NUM_CORES} GREATER 3)
      math(EXPR _compile_jobs_max "${_NUM_CORES} - 1")
    else()
      set(_compile_jobs_max ${_NUM_CORES})
    endif()
    if(${_compile_jobs} GREATER ${_compile_jobs_max})
      set(_compile_jobs ${_compile_jobs_max})
    elseif(${_compile_jobs} LESS 1)
      set(_compile_jobs 1)
    endif()
    set(NINJA_MAX_NUM_PARALLEL_COMPILE_JOBS "${_compile_jobs}" CACHE STRING
        "Define the maximum number of concurrent compilation jobs, for ninja build system." FORCE)
    mark_as_advanced(NINJA_MAX_NUM_PARALLEL_COMPILE_JOBS)
    set(_compile_jobs)
    set(_compile_jobs_max)

    # In practice, even when there is RAM available,
    # this proves to be quicker than running in parallel (due to slow disks accesses).
    set(NINJA_MAX_NUM_PARALLEL_LINK_JOBS "1" CACHE STRING
        "Define the maximum number of concurrent link jobs, for ninja build system." FORCE)
    mark_as_advanced(NINJA_MAX_NUM_PARALLEL_LINK_JOBS)

    set(_NUM_CORES)
    set(_TOT_MEM)
  endif()

  if(NINJA_MAX_NUM_PARALLEL_COMPILE_JOBS)
    set_property(
      GLOBAL APPEND PROPERTY
      JOB_POOLS compile_job_pool=${NINJA_MAX_NUM_PARALLEL_COMPILE_JOBS}
    )
    set(CMAKE_JOB_POOL_COMPILE compile_job_pool)
  endif()

  if(NINJA_MAX_NUM_PARALLEL_COMPILE_HEAVY_JOBS)
    set_property(
      GLOBAL APPEND PROPERTY
      JOB_POOLS compile_heavy_job_pool=${NINJA_MAX_NUM_PARALLEL_COMPILE_HEAVY_JOBS}
    )
  endif()

  if(NINJA_MAX_NUM_PARALLEL_LINK_JOBS)
    set_property(
      GLOBAL APPEND PROPERTY
      JOB_POOLS link_job_pool=${NINJA_MAX_NUM_PARALLEL_LINK_JOBS}
    )
    set(CMAKE_JOB_POOL_LINK link_job_pool)
  endif()
endif()


# -----------------------------------------------------------------------------
# Extra Compile Flags

if(CMAKE_COMPILER_IS_GNUCC)

  add_check_c_compiler_flags(
    C_WARNINGS

    C_WARN_ALL -Wall
    C_WARN_ERROR_IMPLICIT_FUNCTION_DECLARATION -Werror=implicit-function-declaration

    # System headers sometimes do this, disable for now, was: `-Werror=strict-prototypes`.
    C_WARN_STRICT_PROTOTYPES -Wstrict-prototypes

    C_WARN_ERROR_RETURN_TYPE -Werror=return-type
    C_WARN_ERROR_VLA -Werror=vla
    C_WARN_MISSING_PROTOTYPES -Wmissing-prototypes
    C_WARN_NO_CHAR_SUBSCRIPTS -Wno-char-subscripts
    C_WARN_NO_UNKNOWN_PRAGMAS -Wno-unknown-pragmas
    C_WARN_POINTER_ARITH -Wpointer-arith
    C_WARN_UNUSED_PARAMETER -Wunused-parameter
    C_WARN_WRITE_STRINGS -Wwrite-strings
    C_WARN_LOGICAL_OP -Wlogical-op
    C_WARN_UNDEF -Wundef

    # Needs: `-Wuninitialized`.
    C_WARN_INIT_SELF -Winit-self

    C_WARN_MISSING_INCLUDE_DIRS -Wmissing-include-dirs
    C_WARN_NO_DIV_BY_ZERO -Wno-div-by-zero
    C_WARN_TYPE_LIMITS -Wtype-limits
    C_WARN_FORMAT_SIGN -Wformat-signedness
    C_WARN_RESTRICT -Wrestrict

    # Useful but too many false positives and inconvenient to suppress each occurrence.
    C_WARN_NO_STRINGOP_OVERREAD -Wno-stringop-overread
    C_WARN_NO_STRINGOP_OVERFLOW -Wno-stringop-overflow

    # C-only.
    C_WARN_NO_NULL -Wnonnull
    C_WARN_ABSOLUTE_VALUE -Wabsolute-value

    C_WARN_UNINITIALIZED -Wuninitialized
    C_WARN_REDUNDANT_DECLS -Wredundant-decls
    C_WARN_SHADOW -Wshadow

    # Disable because it gives warnings for printf() & friends.
    # C_WARN_DOUBLE_PROMOTION "-Wdouble-promotion -Wno-error=double-promotion"

    # Use `ATTR_FALLTHROUGH` macro to suppress.
    C_WARN_IMPLICIT_FALLTHROUGH -Wimplicit-fallthrough=5
  )

  if(NOT APPLE)
    add_check_c_compiler_flags(
      C_WARNINGS
      C_WARN_NO_ERROR_UNUSED_BUT_SET_VARIABLE -Wno-error=unused-but-set-variable
    )
  endif()

  add_check_cxx_compiler_flags(
    CXX_WARNINGS

    CXX_WARN_UNINITIALIZED -Wuninitialized
    CXX_WARN_REDUNDANT_DECLS -Wredundant-decls

    CXX_WARN_ALL -Wall
    CXX_WARN_NO_INVALID_OFFSETOF -Wno-invalid-offsetof
    CXX_WARN_NO_SIGN_COMPARE -Wno-sign-compare
    CXX_WARN_LOGICAL_OP -Wlogical-op

    # Needs: `-Wuninitialized`.
    CXX_WARN_INIT_SELF -Winit-self

    CXX_WARN_MISSING_INCLUDE_DIRS -Wmissing-include-dirs
    CXX_WARN_NO_DIV_BY_ZERO -Wno-div-by-zero
    CXX_WARN_TYPE_LIMITS -Wtype-limits
    CXX_WARN_ERROR_RETURN_TYPE -Werror=return-type
    CXX_WARN_NO_CHAR_SUBSCRIPTS -Wno-char-subscripts
    CXX_WARN_NO_UNKNOWN_PRAGMAS -Wno-unknown-pragmas
    CXX_WARN_POINTER_ARITH -Wpointer-arith
    CXX_WARN_UNUSED_PARAMETER -Wunused-parameter
    CXX_WARN_WRITE_STRINGS -Wwrite-strings
    CXX_WARN_UNDEF -Wundef
    CXX_WARN_COMMA_SUBSCRIPT -Wcomma-subscript
    CXX_WARN_FORMAT_SIGN -Wformat-signedness
    CXX_WARN_RESTRICT -Wrestrict
    CXX_WARN_NO_SUGGEST_OVERRIDE -Wno-suggest-override
    CXX_WARN_UNINITIALIZED -Wuninitialized

    # NOTE(@ideasman42): In GCC 13.2.1 on Linux this causes internal compiler errors.
    # The crashes can be resolved by disabling the flag per module (but not via pragmas).
    # However this also causes a type mix-up FreeStyle  (Blender & FreeStyle's `Curve`)
    # so it seems to impact GCC's the internal state enough that it's too risky to enable.
    # When this is resolved the check can be enabled for fixed GCC versions.
    #
    # Prevents linking errors with MSVC.
    # `CXX_WARN_MISMATCHED_TAGS -Wmismatched-tags`

    # Useful but too many false positives and inconvenient to suppress each occurrence.
    CXX_WARN_NO_STRINGOP_OVERREAD -Wno-stringop-overread
    CXX_WARN_NO_STRINGOP_OVERFLOW -Wno-stringop-overflow

    # Use `[[fallthrough]]` or `ATTR_FALLTHROUGH` macro to suppress.
    CXX_WARN_IMPLICIT_FALLTHROUGH -Wimplicit-fallthrough=5
  )

  # causes too many warnings
  if(NOT APPLE)
    add_check_cxx_compiler_flags(
      CXX_WARNINGS
      CXX_WARN_UNDEF -Wundef
      CXX_WARN_MISSING_DECLARATIONS -Wmissing-declarations
    )
  endif()

  # ---------------------
  # Suppress Strict Flags
  #
  # Exclude the following warnings from this list:
  # - `-Wno-address`:
  #   This can give useful hints that point to bugs/misleading logic.
  # - `-Wno-strict-prototypes`:
  #   No need to support older C-style prototypes.
  #
  # If code in `./extern/` needs to suppress these flags that can be done on a case-by-case basis.

  # flags to undo strict flags
  add_check_c_compiler_flags(
    C_REMOVE_STRICT_FLAGS

    C_WARN_NO_DEPRECATED_DECLARATIONS -Wno-deprecated-declarations
    C_WARN_NO_UNUSED_PARAMETER -Wno-unused-parameter
    C_WARN_NO_UNUSED_FUNCTION -Wno-unused-function
    C_WARN_NO_TYPE_LIMITS -Wno-type-limits
    C_WARN_NO_INT_IN_BOOL_CONTEXT -Wno-int-in-bool-context
    C_WARN_NO_FORMAT -Wno-format
    C_WARN_NO_SWITCH -Wno-switch
    C_WARN_NO_UNUSED_VARIABLE -Wno-unused-variable
    C_WARN_NO_UNUSED_VARIABLE -Wno-uninitialized
    C_WARN_NO_IMPLICIT_FALLTHROUGH -Wno-implicit-fallthrough
  )


  add_check_cxx_compiler_flags(
    CXX_REMOVE_STRICT_FLAGS

    CXX_WARN_NO_CLASS_MEMACCESS -Wno-class-memaccess
    CXX_WARN_NO_COMMENT -Wno-comment
    CXX_WARN_NO_UNUSED_TYPEDEFS -Wno-unused-local-typedefs
    CXX_WARN_NO_UNUSED_VARIABLE -Wno-unused-variable
    CXX_WARN_NO_UNUSED_VARIABLE -Wno-uninitialized
  )


  if(NOT APPLE)
    add_check_c_compiler_flags(
      C_REMOVE_STRICT_FLAGS
      C_WARN_NO_ERROR_UNUSED_BUT_SET_VARIABLE -Wno-error=unused-but-set-variable
    )
  endif()

elseif(CMAKE_C_COMPILER_ID MATCHES "Clang")

  add_check_c_compiler_flags(
    C_WARNINGS

    # Strange, clang complains these are not supported, but then uses them.
    C_WARN_ALL -Wall
    C_WARN_ERROR_IMPLICIT_FUNCTION_DECLARATION -Werror=implicit-function-declaration
    C_WARN_ERROR_RETURN_TYPE -Werror=return-type
    C_WARN_NO_AUTOLOGICAL_COMPARE -Wno-tautological-compare
    C_WARN_NO_UNKNOWN_PRAGMAS -Wno-unknown-pragmas
    C_WARN_NO_CHAR_SUBSCRIPTS -Wno-char-subscripts
    C_WARN_STRICT_PROTOTYPES -Wstrict-prototypes
    C_WARN_MISSING_PROTOTYPES -Wmissing-prototypes
    C_WARN_UNUSED_PARAMETER -Wunused-parameter
    C_WARN_UNDEF -Wundef
    C_WARN_UNDEF_PREFIX -Wundef-prefix

    C_WARN_ERROR_UNGUARDED_AVAILABILITY_NEW -Werror=unguarded-availability-new
  )

  add_check_cxx_compiler_flags(
    CXX_WARNINGS

    CXX_WARN_ALL -Wall
    # Using C++20 features while having C++17 as the project language isn't allowed by MSVC.
    CXX_CXX20_DESIGNATOR -Wc++20-designator

    CXX_WARN_NO_AUTOLOGICAL_COMPARE -Wno-tautological-compare
    CXX_WARN_NO_UNKNOWN_PRAGMAS -Wno-unknown-pragmas
    CXX_WARN_NO_CHAR_SUBSCRIPTS -Wno-char-subscripts

    # We get a lot of these, if its a problem a dev needs to look into it.
    CXX_WARN_NO_OVERLOADED_VIRTUAL -Wno-overloaded-virtual

    CXX_WARN_NO_SIGN_COMPARE -Wno-sign-compare
    CXX_WARN_NO_INVALID_OFFSETOF -Wno-invalid-offsetof

    # Apple Clang (tested on version 12) doesn't support this flag while LLVM Clang 11 does.
    CXX_WARN_NO_SUGGEST_OVERRIDE -Wno-suggest-override

    CXX_WARN_UNDEF -Wundef
    CXX_WARN_UNDEF_PREFIX -Wundef-prefix
    CXX_WARN_UNUSED_PARAMETER -Wunused-parameter

    # Prevents linking errors with MSVC.
    CXX_WARN_MISMATCHED_TAGS -Wmismatched-tags

    # Gives too many unfixable warnings.
    # `C_WARN_UNUSED_MACROS -Wunused-macros`
    # `CXX_WARN_UNUSED_MACROS -Wunused-macros`

    CXX_WARN_ERROR_UNGUARDED_AVAILABILITY_NEW -Werror=unguarded-availability-new
  )




  # ---------------------
  # Suppress Strict Flags

  # flags to undo strict flags

  add_check_c_compiler_flags(
    C_REMOVE_STRICT_FLAGS

    C_WARN_NO_UNUSED_PARAMETER -Wno-unused-parameter
    C_WARN_NO_UNUSED_VARIABLE -Wno-unused-variable
    C_WARN_NO_UNUSED_MACROS -Wno-unused-macros
    C_WARN_NO_MISLEADING_INDENTATION -Wno-misleading-indentation

    C_WARN_NO_MISSING_VARIABLE_DECLARATIONS -Wno-missing-variable-declarations
    C_WARN_NO_INCOMPAT_PTR_DISCARD_QUAL -Wno-incompatible-pointer-types-discards-qualifiers
    C_WARN_NO_UNUSED_FUNCTION -Wno-unused-function
    C_WARN_NO_INT_TO_VOID_POINTER_CAST -Wno-int-to-void-pointer-cast
    C_WARN_NO_MISSING_PROTOTYPES -Wno-missing-prototypes
    C_WARN_NO_DUPLICATE_ENUM -Wno-duplicate-enum
    C_WARN_NO_UNDEF -Wno-undef
    C_WARN_NO_MISSING_NORETURN -Wno-missing-noreturn
    C_WARN_NO_UNUSED_BUT_SET_VARIABLE -Wno-unused-but-set-variable
    C_WARN_NO_DEPRECATED_DECLARATIONS -Wno-deprecated-declarations
    C_WARN_NO_STRICT_PROTOTYPES -Wno-strict-prototypes
    C_WARN_NO_BITWISE_INSTEAD_OF_LOGICAL -Wno-bitwise-instead-of-logical
    C_WARN_NO_IMPLICIT_CONST_INT_FLOAT_CONVERSION -Wno-implicit-const-int-float-conversion
    C_WARN_NO_SINGLE_BIT_BITFIELD_CONSTANT_CONVERSION -Wno-single-bit-bitfield-constant-conversion
  )

  add_check_cxx_compiler_flags(
    CXX_REMOVE_STRICT_FLAGS

    CXX_WARN_NO_UNUSED_PARAMETER -Wno-unused-parameter
    CXX_WARN_NO_UNUSED_PRIVATE_FIELD -Wno-unused-private-field
    CXX_WARN_NO_CXX11_NARROWING -Wno-c++11-narrowing
    CXX_WARN_NO_NON_VIRTUAL_DTOR -Wno-non-virtual-dtor
    CXX_WARN_NO_UNUSED_MACROS -Wno-unused-macros
    CXX_WARN_NO_UNUSED_VARIABLE -Wno-unused-variable
    CXX_WARN_NO_REORDER -Wno-reorder
    CXX_WARN_NO_COMMENT -Wno-comment
    CXX_WARN_NO_UNUSED_TYPEDEFS -Wno-unused-local-typedefs
    CXX_WARN_NO_UNDEFINED_VAR_TEMPLATE -Wno-undefined-var-template
    CXX_WARN_NO_INSTANTIATION_AFTER_SPECIALIZATION -Wno-instantiation-after-specialization
    CXX_WARN_NO_MISLEADING_INDENTATION -Wno-misleading-indentation
    CXX_WARN_NO_BITWISE_INSTEAD_OF_LOGICAL -Wno-bitwise-instead-of-logical
    CXX_WARN_NO_IMPLICIT_CONST_INT_FLOAT_CONVERSION -Wno-implicit-const-int-float-conversion
    CXX_WARN_NO_UNDEF -Wno-undef
    CXX_WARN_NO_UNDEF_PREFIX -Wno-undef-prefix
  )

elseif(CMAKE_C_COMPILER_ID MATCHES "Intel")

  add_check_c_compiler_flags(
    C_WARNINGS

    C_WARN_ALL -Wall
    C_WARN_POINTER_ARITH -Wpointer-arith
    C_WARN_NO_UNKNOWN_PRAGMAS -Wno-unknown-pragmas
  )

  add_check_cxx_compiler_flags(
    CXX_WARNINGS

    CXX_WARN_ALL -Wall
    CXX_WARN_NO_INVALID_OFFSETOF -Wno-invalid-offsetof
    CXX_WARN_NO_SIGN_COMPARE -Wno-sign-compare
  )

  # Disable numbered, false positives.
  string(APPEND C_WARNINGS " -wd188,186,144,913,556,858,597,177,1292,167,279,592,94,2722,3199")
  string(APPEND CXX_WARNINGS " -wd188,186,144,913,556,858,597,177,1292,167,279,592,94,2722,3199")
elseif(CMAKE_C_COMPILER_ID MATCHES "MSVC")
  # most msvc warnings are C & C++
  set(_WARNINGS
    # warning level:
    "/W3"
    "/w34062"  # switch statement contains 'default' but no 'case' labels
    "/w34100"  # 'identifier' : unreferenced formal parameter
    "/w34115"  # 'type' : named type definition in parentheses
    "/w34189"  # local variable is initialized but not referenced
    # see https://docs.microsoft.com/en-us/cpp/error-messages/compiler-warnings/c5038?view=vs-2017
    "/w35038"  # order of initialization in c++ constructors
    # disable:
    "/wd4018"  # signed/unsigned mismatch
    "/wd4146"  # unary minus operator applied to unsigned type, result still unsigned
    "/wd4065"  # switch statement contains 'default' but no 'case' labels
    "/wd4127"  # conditional expression is constant
    "/wd4181"  # qualifier applied to reference type; ignored
    "/wd4200"  # zero-sized array in struct/union
    "/wd4244"  # conversion from 'type1' to 'type2', possible loss of data
    "/wd4267"  # conversion from 'size_t' to 'type', possible loss of data
    "/wd4305"  # truncation from 'type1' to 'type2'
    "/wd4800"  # forcing value to bool 'true' or 'false'
    "/wd4828"  # The file contains a character that is illegal
    "/wd4996"  # identifier was declared deprecated
    "/wd4661"  # no suitable definition provided for explicit template instantiation request
    "/wd4848"  # 'no_unique_address' is a vendor extension in C++17
    # errors:
    "/we4013"  # 'function' undefined; assuming extern returning int
    "/we4133"  # incompatible pointer types
    "/we4431"  # missing type specifier - int assumed
    "/we4033"  # 'function' must return a value
  )

  string(REPLACE ";" " " _WARNINGS "${_WARNINGS}")
  set(C_WARNINGS "${_WARNINGS}")
  set(CXX_WARNINGS "${_WARNINGS}")
  unset(_WARNINGS)
endif()

# Xcode enables additional warning flags by default. Disable some to match
# command line build and other platforms more closely.
if(XCODE)
  set(CMAKE_XCODE_ATTRIBUTE_GCC_WARN_64_TO_32_BIT_CONVERSION NO)
endif()

# ensure python header is found since detection can fail, this could happen
# with _any_ library but since we used a fixed python version this tends to
# be most problematic.
if(WITH_PYTHON)
  if(NOT EXISTS "${PYTHON_INCLUDE_DIR}/Python.h")
    message(
      FATAL_ERROR
      "Missing: \"${PYTHON_INCLUDE_DIR}/Python.h\",\n"
      "Set the cache entry 'PYTHON_INCLUDE_DIR' to point "
      "to a valid python include path. Containing "
      "Python.h for python version \"${PYTHON_VERSION}\""
    )
  endif()

  if(WIN32)
    # Always use numpy bundled in precompiled libs.
  elseif((WITH_PYTHON_INSTALL AND WITH_PYTHON_INSTALL_NUMPY) OR WITH_PYTHON_NUMPY)
    if(("${PYTHON_NUMPY_PATH}" STREQUAL "") OR (${PYTHON_NUMPY_PATH} MATCHES NOTFOUND))
      find_python_package(numpy "core/include")
    endif()
  endif()

  if(WIN32 OR APPLE)
    # Always copy from precompiled libs.
  elseif(WITH_PYTHON_INSTALL_REQUESTS)
    find_python_package(requests "")
  endif()

  if(WIN32 OR APPLE)
    # Always copy from precompiled libs.
  elseif(WITH_PYTHON_INSTALL_ZSTANDARD)
    find_python_package(zstandard "")
  endif()
endif()

# Select C++17 as the standard for C++ projects.
set(CMAKE_CXX_STANDARD 17)
# If C++17 is not available, downgrading to an earlier standard is NOT OK.
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# Do not enable compiler specific language extensions.
set(CMAKE_CXX_EXTENSIONS OFF)

# Make MSVC properly report the value of the __cplusplus preprocessor macro
# Available MSVC 15.7 (1914) and up, without this it reports 199711L regardless
# of the C++ standard chosen above.
if(MSVC)
  string(APPEND CMAKE_CXX_FLAGS " /Zc:__cplusplus")
endif()

# Visual Studio has all standards it supports available by default
# Clang on windows copies this behavior and does not support these switches
if(
  CMAKE_COMPILER_IS_GNUCC OR
  (CMAKE_C_COMPILER_ID MATCHES "Clang" AND (NOT MSVC)) OR
  (CMAKE_C_COMPILER_ID MATCHES "Intel")
)
  # Use C11 + GNU extensions, works with GCC, Clang, ICC
  string(APPEND CMAKE_C_FLAGS " -std=gnu11")
endif()

if(WITH_COMPILER_SHORT_FILE_MACRO)
  # Use '-fmacro-prefix-map' for Clang and GCC (MSVC doesn't support this).
  set(C_PREFIX_MAP_FLAGS "")
  set(CXX_PREFIX_MAP_FLAGS "")
  add_check_c_compiler_flags(
    C_PREFIX_MAP_FLAGS
    C_MACRO_PREFIX_MAP -fmacro-prefix-map=foo=bar
  )
  add_check_cxx_compiler_flags(
    CXX_PREFIX_MAP_FLAGS
    CXX_MACRO_PREFIX_MAP -fmacro-prefix-map=foo=bar
  )
  if(C_MACRO_PREFIX_MAP AND CXX_MACRO_PREFIX_MAP)
    if(APPLE)
      if(XCODE AND ${XCODE_VERSION} VERSION_LESS 12.0)
      # Developers may have say LLVM Clang-10.0.1 toolchain (which supports the flag)
      # with Xcode-11 (the Clang of which doesn't support the flag).
        message(
          WARNING
          "-fmacro-prefix-map flag is NOT supported by Clang shipped with Xcode-${XCODE_VERSION}."
          " Some Xcode functionality in Product menu may not work. "
          "Disabling WITH_COMPILER_SHORT_FILE_MACRO."
        )
        set(WITH_COMPILER_SHORT_FILE_MACRO OFF)
      endif()
    endif()
    if(WITH_COMPILER_SHORT_FILE_MACRO)
      path_ensure_trailing_slash(_src_dir "${CMAKE_SOURCE_DIR}")
      path_ensure_trailing_slash(_bin_dir "${CMAKE_BINARY_DIR}")
      # Keep this variable so it can be stripped from build-info.
      set(PLATFORM_CFLAGS_FMACRO_PREFIX_MAP
        "-fmacro-prefix-map=\"${_src_dir}\"=\"\" -fmacro-prefix-map=\"${_bin_dir}\"=\"\"")
      string(APPEND PLATFORM_CFLAGS " ${PLATFORM_CFLAGS_FMACRO_PREFIX_MAP}")
      unset(_src_dir)
      unset(_bin_dir)
    endif()
  else()
    message(
      WARNING
      "-fmacro-prefix-map flag is NOT supported by C/C++ compiler."
      " Disabling WITH_COMPILER_SHORT_FILE_MACRO."
    )
    set(WITH_COMPILER_SHORT_FILE_MACRO OFF)
  endif()
  unset(C_PREFIX_MAP_FLAGS)
  unset(CXX_PREFIX_MAP_FLAGS)
endif()

# Include warnings first, so its possible to disable them with user defined flags
# eg: -Wno-uninitialized
set(CMAKE_C_FLAGS "${C_WARNINGS} ${CMAKE_C_FLAGS} ${PLATFORM_CFLAGS}")
set(CMAKE_CXX_FLAGS "${CXX_WARNINGS} ${CMAKE_CXX_FLAGS} ${PLATFORM_CFLAGS}")

# defined above, platform specific but shared names
mark_as_advanced(
  CYCLES_OSL
  OSL_LIB_EXEC
  OSL_COMPILER
  OSL_LIB_COMP
  OSL_LIB_QUERY
  OSL_INCLUDE_DIR
)

mark_as_advanced(
  LLVM_CONFIG
  LLVM_ROOT_DIR
  LLVM_LIBRARY
  LLVM_VERSION
)


# -------------------------------------------------------------------------------
# Global Defines

# better not set includes here but this debugging option is off by default.
if(WITH_CXX_GUARDEDALLOC)
  include_directories(${CMAKE_SOURCE_DIR}/intern/guardedalloc)
  add_definitions(-DWITH_CXX_GUARDEDALLOC)
endif()

if(WITH_ASSERT_ABORT)
  add_definitions(-DWITH_ASSERT_ABORT)
endif()

# NDEBUG is the standard C define to disable asserts.
if(WITH_ASSERT_RELEASE)
  # CMake seemingly be setting the NDEBUG flag on its own already on some configurations
  # therefore we need to remove the flags if they happen to be set.
  remove_cc_flag("-DNDEBUG") # GCC/CLang
  remove_cc_flag("/DNDEBUG") # MSVC
else()
  set_property(DIRECTORY APPEND PROPERTY COMPILE_DEFINITIONS
    $<$<CONFIG:Release>:NDEBUG>
    $<$<CONFIG:MinSizeRel>:NDEBUG>
    $<$<CONFIG:RelWithDebInfo>:NDEBUG>
  )
endif()

# message(STATUS "Using CFLAGS: ${CMAKE_C_FLAGS}")
# message(STATUS "Using CXXFLAGS: ${CMAKE_CXX_FLAGS}")

# -----------------------------------------------------------------------------
# Testing Functions

include(build_files/cmake/testing.cmake)

# -----------------------------------------------------------------------------
# Add Sub-Directories

if(WITH_BLENDER)
  add_subdirectory(intern)
  add_subdirectory(extern)

  # source after intern and extern to gather all
  # internal and external library information first, for test linking
  add_subdirectory(source)
elseif(WITH_CYCLES_STANDALONE OR WITH_CYCLES_HYDRA_RENDER_DELEGATE)
  add_subdirectory(intern/guardedalloc)
  add_subdirectory(intern/libc_compat)
  add_subdirectory(intern/sky)

  add_subdirectory(intern/cycles)
  if(WITH_CYCLES_LOGGING)
    if(NOT WITH_SYSTEM_GFLAGS)
      add_subdirectory(extern/gflags)
    endif()
    add_subdirectory(extern/glog)
  endif()
  if(WITH_CUDA_DYNLOAD)
    add_subdirectory(extern/cuew)
  endif()
  if(WITH_HIP_DYNLOAD)
    add_subdirectory(extern/hipew)
  endif()
endif()


# -----------------------------------------------------------------------------
# Add Testing Directory

add_subdirectory(tests)


# -----------------------------------------------------------------------------
# Add Blender Application

if(WITH_BLENDER)
  add_subdirectory(source/creator)
endif()


# -----------------------------------------------------------------------------
# Define 'heavy' sub-modules (for Ninja builder when using pools)
setup_heavy_lib_pool()


# -----------------------------------------------------------------------------
# CPack for generating packages

include(build_files/cmake/packaging.cmake)


# -----------------------------------------------------------------------------
# Use Dynamic Loading for OpenMP

if(WITH_BLENDER)
  openmp_delayload(blender)
endif()


# -----------------------------------------------------------------------------
# Print Final Configuration

if(FIRST_RUN)

  set(_config_msg "\nBlender Configuration\n=====================")

  function(info_cfg_option
    _setting
    )

    set(_msg "  - ${_setting}")
    string(LENGTH "${_msg}" _len)
    while("36" GREATER "${_len}")
      string(APPEND _msg " ")
      math(EXPR _len "${_len} + 1")
    endwhile()

    set(_config_msg "${_config_msg}\n${_msg}${${_setting}}" PARENT_SCOPE)
  endfunction()

  function(info_cfg_text
    _text
    )

    set(_config_msg "${_config_msg}\n\n  ${_text}" PARENT_SCOPE)
  endfunction()

  message(STATUS "C Compiler:   \"${CMAKE_C_COMPILER_ID}\"")
  message(STATUS "C++ Compiler: \"${CMAKE_CXX_COMPILER_ID}\"")

  info_cfg_text("Build Options:")
  info_cfg_option(WITH_ALEMBIC)
  info_cfg_option(WITH_BULLET)
  info_cfg_option(WITH_CLANG)
  info_cfg_option(WITH_CYCLES)
  info_cfg_option(WITH_FFTW3)
  info_cfg_option(WITH_FREESTYLE)
  info_cfg_option(WITH_GMP)
  info_cfg_option(WITH_HARU)
  info_cfg_option(WITH_IK_ITASC)
  info_cfg_option(WITH_IK_SOLVER)
  info_cfg_option(WITH_INPUT_NDOF)
  info_cfg_option(WITH_INPUT_IME)
  info_cfg_option(WITH_INTERNATIONAL)
  info_cfg_option(WITH_OPENCOLLADA)
  info_cfg_option(WITH_OPENCOLORIO)
  info_cfg_option(WITH_OPENIMAGEDENOISE)
  info_cfg_option(WITH_OPENVDB)
  info_cfg_option(WITH_POTRACE)
  info_cfg_option(WITH_PUGIXML)
  info_cfg_option(WITH_QUADRIFLOW)
  info_cfg_option(WITH_TBB)
  info_cfg_option(WITH_USD)
  info_cfg_option(WITH_MATERIALX)
  info_cfg_option(WITH_XR_OPENXR)

  info_cfg_text("Compiler Options:")
  info_cfg_option(WITH_BUILDINFO)
  info_cfg_option(WITH_OPENMP)

  info_cfg_text("System Options:")
  info_cfg_option(WITH_INSTALL_PORTABLE)
  info_cfg_option(WITH_MEM_JEMALLOC)
  info_cfg_option(WITH_MEM_VALGRIND)

  info_cfg_text("GHOST Options:")
  info_cfg_option(WITH_GHOST_DEBUG)
  info_cfg_option(WITH_GHOST_SDL)
  if(UNIX AND NOT APPLE)
    info_cfg_option(WITH_GHOST_X11)
    info_cfg_option(WITH_GHOST_WAYLAND)
    if(WITH_GHOST_X11)
      info_cfg_option(WITH_GHOST_XDND)
      info_cfg_option(WITH_X11_XF86VMODE)
      info_cfg_option(WITH_X11_XFIXES)
      info_cfg_option(WITH_X11_XINPUT)
    endif()
    if(WITH_GHOST_WAYLAND)
      info_cfg_option(WITH_GHOST_WAYLAND_DYNLOAD)
      info_cfg_option(WITH_GHOST_WAYLAND_LIBDECOR)
    endif()
  endif()

  info_cfg_text("Image Formats:")
  info_cfg_option(WITH_IMAGE_CINEON)
  info_cfg_option(WITH_IMAGE_OPENEXR)
  info_cfg_option(WITH_IMAGE_OPENJPEG)

  info_cfg_text("Audio:")
  info_cfg_option(WITH_CODEC_AVI)
  info_cfg_option(WITH_CODEC_FFMPEG)
  info_cfg_option(WITH_CODEC_SNDFILE)
  info_cfg_option(WITH_COREAUDIO)
  info_cfg_option(WITH_JACK)
  info_cfg_option(WITH_JACK_DYNLOAD)
  info_cfg_option(WITH_OPENAL)
  info_cfg_option(WITH_PULSEAUDIO)
  info_cfg_option(WITH_PULSEAUDIO_DYNLOAD)
  info_cfg_option(WITH_SDL)
  info_cfg_option(WITH_SDL_DYNLOAD)
  info_cfg_option(WITH_WASAPI)

  info_cfg_text("Compression:")
  info_cfg_option(WITH_LZMA)
  info_cfg_option(WITH_LZO)

  if(WITH_PYTHON)
    info_cfg_text("Python:")
    info_cfg_option(WITH_PYTHON_INSTALL)
    info_cfg_option(WITH_PYTHON_INSTALL_NUMPY)
    info_cfg_option(WITH_PYTHON_INSTALL_ZSTANDARD)
    info_cfg_option(WITH_PYTHON_MODULE)
    info_cfg_option(WITH_PYTHON_SAFETY)
  endif()

  info_cfg_text("Modifiers:")
  info_cfg_option(WITH_MOD_FLUID)
  info_cfg_option(WITH_MOD_OCEANSIM)
  info_cfg_option(WITH_MOD_REMESH)

  info_cfg_text("Rendering:")
  info_cfg_option(WITH_HYDRA)

  if(WITH_CYCLES)
    info_cfg_text("Rendering (Cycles):")
    info_cfg_option(WITH_CYCLES_OSL)
    info_cfg_option(WITH_CYCLES_EMBREE)
    info_cfg_option(WITH_CYCLES_PATH_GUIDING)
    if(NOT APPLE)
      info_cfg_option(WITH_CYCLES_DEVICE_OPTIX)
      info_cfg_option(WITH_CYCLES_DEVICE_CUDA)
      info_cfg_option(WITH_CYCLES_CUDA_BINARIES)
      info_cfg_option(WITH_CYCLES_DEVICE_ONEAPI)
      info_cfg_option(WITH_CYCLES_ONEAPI_BINARIES)
      info_cfg_option(WITH_CYCLES_DEVICE_HIP)
      info_cfg_option(WITH_CYCLES_HIP_BINARIES)
    endif()
    if(WIN32)
      info_cfg_option(WITH_CYCLES_DEVICE_HIPRT)
    endif()
  endif()

  info_cfg_text("")

  message("${_config_msg}")
endif()

if(0)
  print_all_vars()
endif()

# Should be the last step of configuration.
if(POSTCONFIGURE_SCRIPT)
  include(${POSTCONFIGURE_SCRIPT})
endif()
