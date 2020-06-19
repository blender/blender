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
# The Original Code is Copyright (C) 2006, Blender Foundation
# All rights reserved.
#
# ***** END GPL LICENSE BLOCK *****

#-----------------------------------------------------------------------------
# We don't allow in-source builds. This causes no end of troubles because
# all out-of-source builds will use the CMakeCache.txt file there and even
# build the libs and objects in it.

if(${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})
  if(NOT DEFINED WITH_IN_SOURCE_BUILD)
    message(FATAL_ERROR
      "CMake generation for blender is not allowed within the source directory!"
      "\n Remove \"${CMAKE_SOURCE_DIR}/CMakeCache.txt\" and try again from another folder, e.g.:"
      "\n "
      "\n rm CMakeCache.txt"
      "\n cd .."
      "\n mkdir cmake-make"
      "\n cd cmake-make"
      "\n cmake ../blender"
      "\n "
      "\n Alternately define WITH_IN_SOURCE_BUILD to force this option (not recommended!)"
    )
  endif()
endif()

cmake_minimum_required(VERSION 3.5)

# Prever LEGACY OpenGL to eb compatible with all the existing releases and
# platforms which don't hare GLVND yet. Only do it if preference was not set
# externally.
if(NOT DEFINED OpenGL_GL_PREFERENCE)
  set(OpenGL_GL_PREFERENCE "LEGACY")
endif()

if(NOT EXECUTABLE_OUTPUT_PATH)
  set(FIRST_RUN TRUE)
else()
  set(FIRST_RUN FALSE)
endif()

# this starts out unset
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/build_files/cmake/Modules")
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/build_files/cmake/platform")

# avoid having empty buildtype
if(NOT DEFINED CMAKE_BUILD_TYPE_INIT)
  set(CMAKE_BUILD_TYPE_INIT "Release")
endif()

# Omit superfluous "Up-to-date" messages.
if(NOT DEFINED CMAKE_INSTALL_MESSAGE)
  set(CMAKE_INSTALL_MESSAGE "LAZY")
endif()

# quiet output for Makefiles, 'make -s' helps too
# set_property(GLOBAL PROPERTY RULE_MESSAGES OFF)

# global compile definitions since add_definitions() adds for all.
set_property(DIRECTORY APPEND PROPERTY COMPILE_DEFINITIONS
  $<$<CONFIG:Debug>:DEBUG;_DEBUG>
  $<$<CONFIG:Release>:NDEBUG>
  $<$<CONFIG:MinSizeRel>:NDEBUG>
  $<$<CONFIG:RelWithDebInfo>:NDEBUG>
)

#-----------------------------------------------------------------------------
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

#-----------------------------------------------------------------------------
# Load some macros.
include(build_files/cmake/macros.cmake)


#-----------------------------------------------------------------------------
# Initialize project.

blender_project_hack_pre()

project(Blender)

blender_project_hack_post()

enable_testing()

#-----------------------------------------------------------------------------
# Redirect output files

set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR}/bin CACHE INTERNAL "" FORCE)
set(LIBRARY_OUTPUT_PATH ${CMAKE_BINARY_DIR}/lib CACHE INTERNAL "" FORCE)
if(MSVC)
  set(TESTS_OUTPUT_DIR ${EXECUTABLE_OUTPUT_PATH}/tests/$<CONFIG>/ CACHE INTERNAL "" FORCE)
else()
  set(TESTS_OUTPUT_DIR ${EXECUTABLE_OUTPUT_PATH}/tests/ CACHE INTERNAL "" FORCE)
endif()

#-----------------------------------------------------------------------------
# Set default config options

get_blender_version()


#-----------------------------------------------------------------------------
# Options

# Blender internal features
option(WITH_BLENDER "Build blender (disable to build only the blender player)" ON)
mark_as_advanced(WITH_BLENDER)

option(WITH_INTERNATIONAL "Enable I18N (International fonts and text)" ON)

option(WITH_PYTHON        "Enable Embedded Python API  (only disable for development)" ON)
option(WITH_PYTHON_SECURITY "Disables execution of scripts within blend files by default" ON)
mark_as_advanced(WITH_PYTHON)  # don't want people disabling this unless they really know what they are doing.
mark_as_advanced(WITH_PYTHON_SECURITY)  # some distributions see this as a security issue, rather than have them patch it, make a build option.

option(WITH_PYTHON_SAFETY "Enable internal API error checking to track invalid data to prevent crash on access (at the expense of some efficiency, only enable for development)." OFF)
mark_as_advanced(WITH_PYTHON_SAFETY)
option(WITH_PYTHON_MODULE "Enable building as a python module which runs without a user interface, like running regular blender in background mode (experimental, only enable for development), installs to PYTHON_SITE_PACKAGES (or CMAKE_INSTALL_PREFIX if WITH_INSTALL_PORTABLE is enabled)." OFF)
if(APPLE)
  option(WITH_PYTHON_FRAMEWORK "Enable building using the Python available in the framework (OSX only)" OFF)
endif()

option(WITH_BUILDINFO     "Include extra build details (only disable for development & faster builds)" ON)
if(${CMAKE_VERSION} VERSION_LESS 2.8.8)
  # add_library OBJECT arg unsupported
  set(WITH_BUILDINFO OFF)
endif()
set(BUILDINFO_OVERRIDE_DATE "" CACHE STRING "Use instead of the current date for reproducible builds (empty string disables this option)")
set(BUILDINFO_OVERRIDE_TIME "" CACHE STRING "Use instead of the current time for reproducible builds (empty string disables this option)")
set(CPACK_OVERRIDE_PACKAGENAME "" CACHE STRING "Use instead of the standard packagename (empty string disables this option)")
mark_as_advanced(CPACK_OVERRIDE_PACKAGENAME)
mark_as_advanced(BUILDINFO_OVERRIDE_DATE)
mark_as_advanced(BUILDINFO_OVERRIDE_TIME)

option(WITH_IK_ITASC      "Enable ITASC IK solver (only disable for development & for incompatible C++ compilers)" ON)
option(WITH_IK_SOLVER     "Enable Legacy IK solver (only disable for development)" ON)
option(WITH_FFTW3         "Enable FFTW3 support (Used for smoke, ocean sim, and audio effects)" ON)
option(WITH_BULLET        "Enable Bullet (Physics Engine)" ON)
option(WITH_SYSTEM_BULLET "Use the systems bullet library (currently unsupported due to missing features in upstream!)" )
mark_as_advanced(WITH_SYSTEM_BULLET)
option(WITH_OPENCOLORIO   "Enable OpenColorIO color management" ON)
if(APPLE)
  # There's no OpenXR runtime in sight for macOS, neither is code well
  # tested there -> disable it by default.
  option(WITH_XR_OPENXR   "Enable VR features through the OpenXR specification" OFF)
  mark_as_advanced(WITH_XR_OPENXR)
else()
  option(WITH_XR_OPENXR   "Enable VR features through the OpenXR specification" ON)
endif()

# Compositor
option(WITH_COMPOSITOR         "Enable the tile based nodal compositor" ON)
option(WITH_OPENIMAGEDENOISE   "Enable the OpenImageDenoise compositing node" ON)

option(WITH_OPENSUBDIV    "Enable OpenSubdiv for surface subdivision" ON)

option(WITH_OPENVDB       "Enable features relying on OpenVDB" ON)
option(WITH_OPENVDB_BLOSC "Enable blosc compression for OpenVDB, only enable if OpenVDB was built with blosc support" ON)
option(WITH_OPENVDB_3_ABI_COMPATIBLE "Assume OpenVDB library has been compiled with version 3 ABI compatibility" OFF)
mark_as_advanced(WITH_OPENVDB_3_ABI_COMPATIBLE)

# GHOST Windowing Library Options
option(WITH_GHOST_DEBUG   "Enable debugging output for the GHOST library" OFF)
mark_as_advanced(WITH_GHOST_DEBUG)

option(WITH_GHOST_SDL    "Enable building Blender against SDL for windowing rather than the native APIs" OFF)
mark_as_advanced(WITH_GHOST_SDL)

if(UNIX AND NOT (APPLE OR HAIKU))
  option(WITH_GHOST_X11 "Enable building Blender against X11 for windowing" ON)
  mark_as_advanced(WITH_GHOST_X11)

  option(WITH_GHOST_WAYLAND "Enable building Blender against Wayland for windowing (under development)" OFF)
  mark_as_advanced(WITH_GHOST_WAYLAND)
endif()

if(WITH_GHOST_X11)
  option(WITH_GHOST_XDND    "Enable drag'n'drop support on X11 using XDND protocol" ON)
endif()

# Misc...
option(WITH_HEADLESS      "Build without graphical support (renderfarm, server mode only)" OFF)
mark_as_advanced(WITH_HEADLESS)

option(WITH_QUADRIFLOW    "Build with quadriflow remesher support" ON)

option(WITH_AUDASPACE    "Build with blenders audio library (only disable if you know what you're doing!)" ON)
option(WITH_SYSTEM_AUDASPACE "Build with external audaspace library installed on the system (only enable if you know what you're doing!)" OFF)
mark_as_advanced(WITH_AUDASPACE)
mark_as_advanced(WITH_SYSTEM_AUDASPACE)

if(NOT WITH_AUDASPACE)
  set(WITH_SYSTEM_AUDASPACE OFF)
endif()

option(WITH_OPENMP        "Enable OpenMP (has to be supported by the compiler)" ON)
if(UNIX AND NOT APPLE)
  option(WITH_OPENMP_STATIC "Link OpenMP statically (only used by the release environment)" OFF)
  mark_as_advanced(WITH_OPENMP_STATIC)
endif()

if(WITH_GHOST_X11)
  option(WITH_X11_XINPUT    "Enable X11 Xinput (tablet support and unicode input)"  ON)
  option(WITH_X11_XF86VMODE "Enable X11 video mode switching"                       ON)
  option(WITH_X11_XFIXES    "Enable X11 XWayland cursor warping workaround"         ON)
  option(WITH_X11_ALPHA     "Enable X11 transparent background"                     ON)
endif()

if(UNIX AND NOT APPLE)
  option(WITH_SYSTEM_GLEW "Use GLEW OpenGL wrapper library provided by the operating system" OFF)
  option(WITH_SYSTEM_GLES "Use OpenGL ES library provided by the operating system"           ON)
else()
  # not an option for other OS's
  set(WITH_SYSTEM_GLEW OFF)
  set(WITH_SYSTEM_GLES OFF)
endif()


if(UNIX AND NOT APPLE)
  option(WITH_SYSTEM_EIGEN3 "Use the systems Eigen3 library" OFF)
endif()


# Modifiers
option(WITH_MOD_FLUID           "Enable Mantaflow Fluid Simulation Framework" ON)
option(WITH_MOD_REMESH          "Enable Remesh Modifier" ON)
option(WITH_MOD_OCEANSIM        "Enable Ocean Modifier" ON)

# Image format support
option(WITH_OPENIMAGEIO         "Enable OpenImageIO Support (http://www.openimageio.org)" ON)
option(WITH_IMAGE_OPENEXR       "Enable OpenEXR Support (http://www.openexr.com)" ON)
option(WITH_IMAGE_OPENJPEG      "Enable OpenJpeg Support (http://www.openjpeg.org)" ON)
option(WITH_IMAGE_TIFF          "Enable LibTIFF Support" ON)
option(WITH_IMAGE_DDS           "Enable DDS Image Support" ON)
option(WITH_IMAGE_CINEON        "Enable CINEON and DPX Image Support" ON)
option(WITH_IMAGE_HDR           "Enable HDR Image Support" ON)

# Audio/Video format support
option(WITH_CODEC_AVI           "Enable Blenders own AVI file support (raw/jpeg)" ON)
option(WITH_CODEC_FFMPEG        "Enable FFMPeg Support (http://ffmpeg.org)" ON)
option(WITH_CODEC_SNDFILE       "Enable libsndfile Support (http://www.mega-nerd.com/libsndfile)" ON)

# Alembic support
option(WITH_ALEMBIC             "Enable Alembic Support" ON)

# Universal Scene Description support
option(WITH_USD                 "Enable Universal Scene Description (USD) Support" ON)

# 3D format support
# Disable opencollada when we don't have precompiled libs
option(WITH_OPENCOLLADA   "Enable OpenCollada Support (http://www.opencollada.org)" ON)

# Sound output
option(WITH_SDL           "Enable SDL for sound and joystick support" ON)
option(WITH_OPENAL        "Enable OpenAL Support (http://www.openal.org)" ON)
if(NOT WIN32)
  option(WITH_JACK          "Enable JACK Support (http://www.jackaudio.org)" ON)
  if(UNIX AND NOT APPLE)
    option(WITH_JACK_DYNLOAD  "Enable runtime dynamic JACK libraries loading" OFF)
  endif()
endif()
if(UNIX AND NOT APPLE)
  option(WITH_SDL_DYNLOAD  "Enable runtime dynamic SDL libraries loading" OFF)
endif()

# Compression
option(WITH_LZO           "Enable fast LZO compression (used for pointcache)" ON)
option(WITH_LZMA          "Enable best LZMA compression, (used for pointcache)" ON)
if(UNIX AND NOT APPLE)
  option(WITH_SYSTEM_LZO    "Use the system LZO library" OFF)
endif()
option(WITH_DRACO         "Enable Draco mesh compression Python module (used for glTF)" ON)

# Camera/motion tracking
option(WITH_LIBMV         "Enable Libmv structure from motion library" ON)
option(WITH_LIBMV_SCHUR_SPECIALIZATIONS "Enable fixed-size schur specializations." ON)
mark_as_advanced(WITH_LIBMV_SCHUR_SPECIALIZATIONS)

# Logging/unbit test libraries.
option(WITH_SYSTEM_GFLAGS   "Use system-wide Gflags instead of a bundled one" OFF)
option(WITH_SYSTEM_GLOG     "Use system-wide Glog instead of a bundled one" OFF)
mark_as_advanced(WITH_SYSTEM_GFLAGS)
mark_as_advanced(WITH_SYSTEM_GLOG)

# Freestyle
option(WITH_FREESTYLE     "Enable Freestyle (advanced edges rendering)" ON)

# New object types
option(WITH_NEW_OBJECT_TYPES "Enable new hair and pointcloud objects (use for development only, don't save in files)" OFF)
mark_as_advanced(WITH_NEW_OBJECT_TYPES)

# New simulation data block
option(WITH_NEW_SIMULATION_TYPE "Enable simulation data block (use for development only, don't save in files)" OFF)
mark_as_advanced(WITH_NEW_SIMULATION_TYPE)

# Misc
if(WIN32)
  option(WITH_INPUT_IME "Enable Input Method Editor (IME) for complex Asian character input" ON)
endif()
option(WITH_INPUT_NDOF "Enable NDOF input devices (SpaceNavigator and friends)" ON)
if(UNIX AND NOT APPLE)
  option(WITH_INSTALL_PORTABLE "Install redistributeable runtime, otherwise install into CMAKE_INSTALL_PREFIX" ON)
  option(WITH_STATIC_LIBS "Try to link with static libraries, as much as possible, to make blender more portable across distributions" OFF)
  if(WITH_STATIC_LIBS)
    option(WITH_BOOST_ICU "Boost uses ICU library (required for linking with static Boost built with libicu)." OFF)
    mark_as_advanced(WITH_BOOST_ICU)
  endif()
endif()

option(WITH_PYTHON_INSTALL       "Copy system python into the blender install folder" ON)
if(WIN32 OR APPLE)
  # Windows and macOS have this bundled with Python libraries.
elseif(WITH_PYTHON_INSTALL OR (WITH_AUDASPACE AND NOT WITH_SYSTEM_AUDASPACE))
  set(PYTHON_NUMPY_PATH            "" CACHE PATH "Path to python site-packages or dist-packages containing 'numpy' module")
  mark_as_advanced(PYTHON_NUMPY_PATH)
  set(PYTHON_NUMPY_INCLUDE_DIRS ${PYTHON_NUMPY_PATH}/numpy/core/include CACHE PATH "Path to the include directory of the numpy module")
  mark_as_advanced(PYTHON_NUMPY_INCLUDE_DIRS)
endif()
if(WITH_PYTHON_INSTALL)
  option(WITH_PYTHON_INSTALL_NUMPY "Copy system numpy into the blender install folder"  ON)

  if(UNIX AND NOT APPLE)
    option(WITH_PYTHON_INSTALL_REQUESTS "Copy system requests into the blender install folder" ON)
    set(PYTHON_REQUESTS_PATH "" CACHE PATH "Path to python site-packages or dist-packages containing 'requests' module")
    mark_as_advanced(PYTHON_REQUESTS_PATH)
  endif()
endif()

option(WITH_CPU_SSE              "Enable SIMD instruction if they're detected on the host machine" ON)
mark_as_advanced(WITH_CPU_SSE)

# Cycles
option(WITH_CYCLES                  "Enable Cycles Render Engine" ON)
option(WITH_CYCLES_STANDALONE       "Build Cycles standalone application" OFF)
option(WITH_CYCLES_STANDALONE_GUI   "Build Cycles standalone with GUI" OFF)
option(WITH_CYCLES_OSL              "Build Cycles with OSL support" ON)
option(WITH_CYCLES_EMBREE           "Build Cycles with Embree support" OFF)
option(WITH_CYCLES_CUDA_BINARIES    "Build Cycles CUDA binaries" OFF)
option(WITH_CYCLES_CUBIN_COMPILER   "Build cubins with nvrtc based compiler instead of nvcc" OFF)
option(WITH_CYCLES_CUDA_BUILD_SERIAL "Build cubins one after another (useful on machines with limited RAM)" OFF)
mark_as_advanced(WITH_CYCLES_CUDA_BUILD_SERIAL)
set(CYCLES_CUDA_BINARIES_ARCH sm_30 sm_35 sm_37 sm_50 sm_52 sm_60 sm_61 sm_70 sm_75 CACHE STRING "CUDA architectures to build binaries for")
mark_as_advanced(CYCLES_CUDA_BINARIES_ARCH)
unset(PLATFORM_DEFAULT)
option(WITH_CYCLES_LOGGING  "Build Cycles with logging support" ON)
option(WITH_CYCLES_DEBUG    "Build Cycles with extra debug capabilities" OFF)
option(WITH_CYCLES_NATIVE_ONLY  "Build Cycles with native kernel only (which fits current CPU, use for development only)" OFF)
option(WITH_CYCLES_KERNEL_ASAN  "Build Cycles kernels with address sanitizer when WITH_COMPILER_ASAN is on, even if it's very slow" OFF)
mark_as_advanced(WITH_CYCLES_KERNEL_ASAN)
mark_as_advanced(WITH_CYCLES_CUBIN_COMPILER)
mark_as_advanced(WITH_CYCLES_LOGGING)
mark_as_advanced(WITH_CYCLES_DEBUG)
mark_as_advanced(WITH_CYCLES_NATIVE_ONLY)

option(WITH_CYCLES_DEVICE_CUDA              "Enable Cycles CUDA compute support" ON)
option(WITH_CYCLES_DEVICE_OPTIX             "Enable Cycles OptiX support" OFF)
option(WITH_CYCLES_DEVICE_OPENCL            "Enable Cycles OpenCL compute support" ON)
option(WITH_CYCLES_NETWORK              "Enable Cycles compute over network support (EXPERIMENTAL and unfinished)" OFF)
mark_as_advanced(WITH_CYCLES_DEVICE_CUDA)
mark_as_advanced(WITH_CYCLES_DEVICE_OPENCL)
mark_as_advanced(WITH_CYCLES_NETWORK)

option(WITH_CUDA_DYNLOAD "Dynamically load CUDA libraries at runtime" ON)
mark_as_advanced(WITH_CUDA_DYNLOAD)

# LLVM
option(WITH_LLVM                    "Use LLVM" OFF)
if(APPLE)
  option(LLVM_STATIC                  "Link with LLVM static libraries" ON) # we prefer static llvm build on Apple, dyn build possible though
else()
  option(LLVM_STATIC                  "Link with LLVM static libraries" OFF)
endif()
mark_as_advanced(LLVM_STATIC)

# disable for now, but plan to support on all platforms eventually
option(WITH_MEM_JEMALLOC   "Enable malloc replacement (http://www.canonware.com/jemalloc)" ON)
mark_as_advanced(WITH_MEM_JEMALLOC)

# currently only used for BLI_mempool
option(WITH_MEM_VALGRIND "Enable extended valgrind support for better reporting" OFF)
mark_as_advanced(WITH_MEM_VALGRIND)

# Debug
option(WITH_CXX_GUARDEDALLOC "Enable GuardedAlloc for C++ memory allocation tracking (only enable for development)" OFF)
mark_as_advanced(WITH_CXX_GUARDEDALLOC)

option(WITH_ASSERT_ABORT "Call abort() when raising an assertion through BLI_assert()" ON)
mark_as_advanced(WITH_ASSERT_ABORT)

option(WITH_BOOST "Enable features depending on boost" ON)
option(WITH_TBB   "Enable features depending on TBB (OpenVDB, OpenImageDenoise, sculpt multithreading)" ON)

# TBB malloc is only supported on for windows currently
if(WIN32)
  option(WITH_TBB_MALLOC_PROXY "Enable the TBB malloc replacement" ON)
endif()

# Unit testsing
option(WITH_GTESTS "Enable GTest unit testing" OFF)
option(WITH_OPENGL_RENDER_TESTS "Enable OpenGL render related unit testing (Experimental)" OFF)
option(WITH_OPENGL_DRAW_TESTS "Enable OpenGL UI drawing related unit testing (Experimental)" OFF)
set(TEST_PYTHON_EXE "" CACHE PATH "Python executable to run unit tests")
mark_as_advanced(TEST_PYTHON_EXE)

# Documentation
if(UNIX AND NOT APPLE)
  option(WITH_DOC_MANPAGE "Create a manual page (Unix manpage)" OFF)
endif()


# OpenGL

option(WITH_OPENGL              "When off limits visibility of the opengl headers to just bf_gpu and gawain (temporary option for development purposes)" ON)
option(WITH_GLEW_ES             "Switches to experimental copy of GLEW that has support for OpenGL ES. (temporary option for development purposes)" OFF)
option(WITH_GL_EGL              "Use the EGL OpenGL system library instead of the platform specific OpenGL system library (CGL, glX, or WGL)"       OFF)
option(WITH_GL_PROFILE_ES20     "Support using OpenGL ES 2.0. (through either EGL or the AGL/WGL/XGL 'es20' profile)"                               OFF)

mark_as_advanced(
  WITH_OPENGL
  WITH_GLEW_ES
  WITH_GL_EGL
  WITH_GL_PROFILE_ES20
)

if(WIN32)
  option(WITH_GL_ANGLE "Link with the ANGLE library, an OpenGL ES 2.0 implementation based on Direct3D, instead of the system OpenGL library." OFF)
  mark_as_advanced(WITH_GL_ANGLE)
endif()

if(WITH_GLEW_ES AND WITH_SYSTEM_GLEW)
  message(WARNING Ignoring WITH_SYSTEM_GLEW and using WITH_GLEW_ES)
  set(WITH_SYSTEM_GLEW OFF)
endif()

if(WIN32)
  getDefaultWindowsPrefixBase(CMAKE_GENERIC_PROGRAM_FILES)
  set(CPACK_INSTALL_PREFIX ${CMAKE_GENERIC_PROGRAM_FILES}/${})
endif()

# Compiler toolchain
if(CMAKE_COMPILER_IS_GNUCC)
  option(WITH_LINKER_GOLD "Use ld.gold linker which is usually faster than ld.bfd" ON)
  mark_as_advanced(WITH_LINKER_GOLD)
  option(WITH_LINKER_LLD "Use ld.lld linker which is usually faster than ld.gold" OFF)
  mark_as_advanced(WITH_LINKER_LLD)
endif()

if(CMAKE_COMPILER_IS_GNUCC OR CMAKE_C_COMPILER_ID MATCHES "Clang")
  option(WITH_COMPILER_ASAN "Build and link against address sanitizer (only for Debug & RelWithDebInfo targets)." OFF)
  mark_as_advanced(WITH_COMPILER_ASAN)

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

    if(NOT MSVC) # not all sanitizers are supported with clang-cl, these two however are very vocal about it
      set(_asan_defaults "${_asan_defaults} -fsanitize=leak -fsanitize=object-size" )
    endif()
    set(COMPILER_ASAN_CFLAGS "${_asan_defaults}" CACHE STRING "C flags for address sanitizer")
    mark_as_advanced(COMPILER_ASAN_CFLAGS)
    set(COMPILER_ASAN_CXXFLAGS "${_asan_defaults}" CACHE STRING "C++ flags for address sanitizer")
    mark_as_advanced(COMPILER_ASAN_CXXFLAGS)

    unset(_asan_defaults)

    if(NOT MSVC)
      find_library(COMPILER_ASAN_LIBRARY asan ${CMAKE_C_IMPLICIT_LINK_DIRECTORIES})
    else()
      find_library(
        COMPILER_ASAN_LIBRARY NAMES clang_rt.asan-x86_64
        PATHS
        [HKEY_LOCAL_MACHINE\\SOFTWARE\\Wow6432Node\\LLVM\\LLVM;]/lib/clang/7.0.0/lib/windows
        [HKEY_LOCAL_MACHINE\\SOFTWARE\\Wow6432Node\\LLVM\\LLVM;]/lib/clang/6.0.0/lib/windows
      )
    endif()
    mark_as_advanced(COMPILER_ASAN_LIBRARY)
  endif()
endif()

if(WIN32)
  # Use hardcoded paths or find_package to find externals
  option(WITH_WINDOWS_FIND_MODULES "Use find_package to locate libraries" OFF)
  mark_as_advanced(WITH_WINDOWS_FIND_MODULES)

  option(WINDOWS_USE_VISUAL_STUDIO_PROJECT_FOLDERS "Organize the visual studio projects according to source folder structure." ON)
  mark_as_advanced(WINDOWS_USE_VISUAL_STUDIO_PROJECT_FOLDERS)

  option(WINDOWS_USE_VISUAL_STUDIO_SOURCE_FOLDERS "Organize the source files in filters matching the source folders." ON)
  mark_as_advanced(WINDOWS_USE_VISUAL_STUDIO_SOURCE_FOLDERS)

  option(WINDOWS_PYTHON_DEBUG "Include the files needed for debugging python scripts with visual studio 2017+." OFF)
  mark_as_advanced(WINDOWS_PYTHON_DEBUG)

  option(WITH_WINDOWS_BUNDLE_CRT "Bundle the C runtime for install free distribution." ON)
  mark_as_advanced(WITH_WINDOWS_BUNDLE_CRT)

  option(WITH_WINDOWS_SCCACHE "Use sccache to speed up builds (Ninja builder only)" OFF)
  mark_as_advanced(WITH_WINDOWS_SCCACHE)

  option(WITH_WINDOWS_PDB "Generate a pdb file for client side stacktraces" ON)
  mark_as_advanced(WITH_WINDOWS_PDB)

  option(WITH_WINDOWS_STRIPPED_PDB "Use a stripped PDB file" On)
  mark_as_advanced(WITH_WINDOWS_STRIPPED_PDB)

endif()

# The following only works with the Ninja generator in CMake >= 3.0.
if("${CMAKE_GENERATOR}" MATCHES "Ninja")
  option(WITH_NINJA_POOL_JOBS
         "Enable Ninja pools of jobs, to try to ease building on machines with 16GB of RAM or less (if not yet defined, will try to set best values based on detected machine specifications)."
         OFF)
  mark_as_advanced(WITH_NINJA_POOL_JOBS)
endif()

if(UNIX AND NOT APPLE)
  option(WITH_CXX11_ABI "Use native C++11 ABI of compiler" ON)
  mark_as_advanced(WITH_CXX11_ABI)
endif()

# Installation process.
option(POSTINSTALL_SCRIPT "Run given CMake script after installation process" OFF)
mark_as_advanced(POSTINSTALL_SCRIPT)

# end option(...)



# By default we want to install to the directory we are compiling our executables
# unless specified otherwise, which we currently do not allow
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
  if(WIN32)
    set(CMAKE_INSTALL_PREFIX ${EXECUTABLE_OUTPUT_PATH}/\${BUILD_TYPE} CACHE PATH "default install path" FORCE)
  elseif(APPLE)
    set(CMAKE_INSTALL_PREFIX ${EXECUTABLE_OUTPUT_PATH}/\${BUILD_TYPE} CACHE PATH "default install path" FORCE)
  else()
    if(WITH_INSTALL_PORTABLE)
      set(CMAKE_INSTALL_PREFIX ${EXECUTABLE_OUTPUT_PATH} CACHE PATH "default install path" FORCE)
    endif()
  endif()
endif()



# Apple

if(APPLE)
  include(platform_apple_xcode)
endif()


#-----------------------------------------------------------------------------
# Check for conflicting/unsupported configurations

if(NOT WITH_BLENDER AND NOT WITH_CYCLES_STANDALONE)
  message(FATAL_ERROR
    "At least one of WITH_BLENDER or WITH_CYCLES_STANDALONE "
    "must be enabled, nothing to do!"
  )
endif()

if(NOT WITH_AUDASPACE)
  if(WITH_OPENAL)
    message(WARNING "WITH_OPENAL requires WITH_AUDASPACE which is disabled")
    set(WITH_OPENAL OFF)
  endif()
  if(WITH_JACK)
    message(WARNING "WITH_JACK requires WITH_AUDASPACE which is disabled")
    set(WITH_JACK OFF)
  endif()
endif()

if(NOT WITH_SDL AND WITH_GHOST_SDL)
  message(FATAL_ERROR "WITH_GHOST_SDL requires WITH_SDL")
endif()

# python module, needs some different options
if(WITH_PYTHON_MODULE AND WITH_PYTHON_INSTALL)
  message(FATAL_ERROR "WITH_PYTHON_MODULE requires WITH_PYTHON_INSTALL to be OFF")
endif()

if(NOT WITH_PYTHON)
  set(WITH_CYCLES OFF)
  set(WITH_DRACO OFF)
endif()

if(WITH_DRACO AND NOT WITH_PYTHON_INSTALL)
  message(STATUS "WITH_DRACO requires WITH_PYTHON_INSTALL to be ON, disabling WITH_DRACO for now")
  set(WITH_DRACO OFF)
endif()

# enable boost for cycles, audaspace or i18n
# otherwise if the user disabled

set_and_warn_dependency(WITH_BOOST WITH_CYCLES         OFF)
set_and_warn_dependency(WITH_BOOST WITH_INTERNATIONAL  OFF)
set_and_warn_dependency(WITH_BOOST WITH_OPENVDB        OFF)
set_and_warn_dependency(WITH_BOOST WITH_OPENCOLORIO    OFF)
set_and_warn_dependency(WITH_BOOST WITH_QUADRIFLOW     OFF)
set_and_warn_dependency(WITH_BOOST WITH_USD            OFF)
set_and_warn_dependency(WITH_BOOST WITH_ALEMBIC        OFF)

if(WITH_BOOST AND NOT (WITH_CYCLES OR WITH_OPENIMAGEIO OR WITH_INTERNATIONAL OR
   WITH_OPENVDB OR WITH_OPENCOLORIO OR WITH_USD OR WITH_ALEMBIC))
  message(STATUS "No dependencies need 'WITH_BOOST' forcing WITH_BOOST=OFF")
  set(WITH_BOOST OFF)
endif()

set_and_warn_dependency(WITH_TBB WITH_USD               OFF)
set_and_warn_dependency(WITH_TBB WITH_OPENIMAGEDENOISE  OFF)
set_and_warn_dependency(WITH_TBB WITH_OPENVDB           OFF)
set_and_warn_dependency(WITH_TBB WITH_MOD_FLUID         OFF)

# OpenVDB uses 'half' type from OpenEXR & fails to link without OpenEXR enabled.
set_and_warn_dependency(WITH_IMAGE_OPENEXR WITH_OPENVDB OFF)

# auto enable openimageio for cycles
if(WITH_CYCLES)
  set(WITH_OPENIMAGEIO ON)

  # auto enable llvm for cycles_osl
  if(WITH_CYCLES_OSL)
    set(WITH_LLVM ON CACHE BOOL "" FORCE)
  endif()
else()
  set(WITH_CYCLES_OSL OFF)
endif()

# auto enable openimageio linking dependencies
if(WITH_OPENIMAGEIO)
  set(WITH_IMAGE_OPENEXR ON)
  set(WITH_IMAGE_TIFF ON)
endif()

# auto enable alembic linking dependencies
if(WITH_ALEMBIC)
  set(WITH_IMAGE_OPENEXR ON)
endif()

# don't store paths to libs for portable distribution
if(WITH_INSTALL_PORTABLE)
  set(CMAKE_SKIP_BUILD_RPATH TRUE)
endif()

if(WITH_GHOST_SDL OR WITH_HEADLESS)
  set(WITH_GHOST_WAYLAND OFF)
  set(WITH_GHOST_X11     OFF)
  set(WITH_X11_XINPUT    OFF)
  set(WITH_X11_XF86VMODE OFF)
  set(WITH_X11_XFIXES    OFF)
  set(WITH_X11_ALPHA     OFF)
  set(WITH_GHOST_XDND    OFF)
  set(WITH_INPUT_IME     OFF)
  set(WITH_XR_OPENXR     OFF)
endif()

if(WITH_CPU_SSE)
  TEST_SSE_SUPPORT(COMPILER_SSE_FLAG COMPILER_SSE2_FLAG)
else()
  message(STATUS "SSE and SSE2 optimizations are DISABLED!")
  set(COMPILER_SSE_FLAG)
  set(COMPILER_SSE2_FLAG)
endif()

if(WITH_BUILDINFO)
  find_package(Git)
  if(NOT GIT_FOUND)
    message(WARNING "Git was not found, disabling WITH_BUILDINFO")
    set(WITH_BUILDINFO OFF)
  endif()
endif()

if(WITH_AUDASPACE)
  if(NOT WITH_SYSTEM_AUDASPACE)
    set(AUDASPACE_C_INCLUDE_DIRS "${CMAKE_SOURCE_DIR}/extern/audaspace/bindings/C" "${CMAKE_BINARY_DIR}/extern/audaspace")
    set(AUDASPACE_PY_INCLUDE_DIRS "${CMAKE_SOURCE_DIR}/extern/audaspace/bindings")
  endif()
endif()

# Auto-enable CUDA dynload if toolkit is not found.
if(NOT WITH_CUDA_DYNLOAD)
  find_package(CUDA)
  if(NOT CUDA_FOUND)
    message("CUDA toolkit not found, using dynamic runtime loading of libraries instead")
    set(WITH_CUDA_DYNLOAD ON)
  endif()
endif()

#-----------------------------------------------------------------------------
# Check check if submodules are cloned

if(WITH_INTERNATIONAL)
  file(GLOB RESULT "${CMAKE_SOURCE_DIR}/release/datafiles/locale")
  list(LENGTH RESULT DIR_LEN)
  if(DIR_LEN EQUAL 0)
    message(WARNING
      "Translation path '${CMAKE_SOURCE_DIR}/release/datafiles/locale' is missing, "
      "This is a 'git submodule', which are known not to work with bridges to other version "
      "control systems, disabling 'WITH_INTERNATIONAL'."
    )
    set(WITH_INTERNATIONAL OFF)
  endif()
endif()

if(WITH_PYTHON)
  # While we have this as an '#error' in 'bpy_capi_utils.h',
  # upgrading Python tends to cause confusion for users who build.
  # Give the error message early to make this more obvious.
  #
  # Do this before main 'platform_*' checks,
  # because UNIX will search for the old Python paths which may not exist.
  # giving errors about missing paths before this case is met.
  if(DEFINED PYTHON_VERSION AND "${PYTHON_VERSION}" VERSION_LESS "3.7")
    message(FATAL_ERROR "At least Python 3.7 is required to build")
  endif()

  file(GLOB RESULT "${CMAKE_SOURCE_DIR}/release/scripts/addons")
  list(LENGTH RESULT DIR_LEN)
  if(DIR_LEN EQUAL 0)
    message(WARNING
      "Addons path '${CMAKE_SOURCE_DIR}/release/scripts/addons' is missing, "
      "This is a 'git submodule', which are known not to work with bridges to other version "
      "control systems: * CONTINUING WITHOUT ADDONS *"
    )
  endif()
endif()

#-----------------------------------------------------------------------------
# Initialize un-cached vars, avoid unused warning

# linux only, not cached
set(WITH_BINRELOC OFF)

# MACOSX only, set to avoid uninitialized
set(EXETYPE "")

# C/C++ flags
set(PLATFORM_CFLAGS)

# these are added to later on.
set(C_WARNINGS)
set(CXX_WARNINGS)

# for gcc -Wno-blah-blah
set(C_REMOVE_STRICT_FLAGS)
set(CXX_REMOVE_STRICT_FLAGS)

# libraries to link the binary with passed to target_link_libraries()
# known as LLIBS to scons
set(PLATFORM_LINKLIBS "")

# Added to linker flags in setup_liblinks
# - CMAKE_EXE_LINKER_FLAGS
# - CMAKE_EXE_LINKER_FLAGS_DEBUG
set(PLATFORM_LINKFLAGS "")
set(PLATFORM_LINKFLAGS_DEBUG "")

if(NOT CMAKE_BUILD_TYPE MATCHES "Release")
  if(WITH_COMPILER_ASAN)
    set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} ${COMPILER_ASAN_CFLAGS}")
    set(CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_RELWITHDEBINFO} ${COMPILER_ASAN_CFLAGS}")

    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} ${COMPILER_ASAN_CXXFLAGS}")
    set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} ${COMPILER_ASAN_CXXFLAGS}")
    if(MSVC)
      set(COMPILER_ASAN_LINKER_FLAGS "/FUNCTIONPADMIN:6")
    endif()
    if(COMPILER_ASAN_LIBRARY)
      set(PLATFORM_LINKLIBS "${PLATFORM_LINKLIBS};${COMPILER_ASAN_LIBRARY}")
      set(PLATFORM_LINKFLAGS "${COMPILER_ASAN_LIBRARY} ${COMPILER_ASAN_LINKER_FLAGS}")
      set(PLATFORM_LINKFLAGS_DEBUG "${COMPILER_ASAN_LIBRARY} ${COMPILER_ASAN_LINKER_FLAGS}")
    endif()
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

#-----------------------------------------------------------------------------
# Common.

if(NOT WITH_FFTW3 AND WITH_MOD_OCEANSIM)
  message(FATAL_ERROR "WITH_MOD_OCEANSIM requires WITH_FFTW3 to be ON")
endif()

if(WITH_CYCLES)
  if(NOT WITH_OPENIMAGEIO)
    message(FATAL_ERROR
      "Cycles requires WITH_OPENIMAGEIO, the library may not have been found. "
      "Configure OIIO or disable WITH_CYCLES"
    )
  endif()
  if(NOT WITH_BOOST)
    message(FATAL_ERROR
      "Cycles requires WITH_BOOST, the library may not have been found. "
      "Configure BOOST or disable WITH_CYCLES"
    )
  endif()

  if(WITH_CYCLES_OSL)
    if(NOT WITH_LLVM)
      message(FATAL_ERROR
        "Cycles OSL requires WITH_LLVM, the library may not have been found. "
        "Configure LLVM or disable WITH_CYCLES_OSL"
      )
    endif()
  endif()
endif()

if(WITH_INTERNATIONAL)
  if(NOT WITH_BOOST)
    message(FATAL_ERROR
      "Internationalization requires WITH_BOOST, the library may not have been found. "
      "Configure BOOST or disable WITH_INTERNATIONAL"
    )
  endif()
endif()

# See TEST_SSE_SUPPORT() for how this is defined.

# Do it globally, SSE2 is required for quite some time now.
# Doing it now allows to use SSE/SSE2 in inline headers.
if(SUPPORT_SSE_BUILD)
  set(PLATFORM_CFLAGS " ${COMPILER_SSE_FLAG} ${PLATFORM_CFLAGS}")
  add_definitions(-D__SSE__ -D__MMX__)
endif()
if(SUPPORT_SSE2_BUILD)
  set(PLATFORM_CFLAGS " ${PLATFORM_CFLAGS} ${COMPILER_SSE2_FLAG}")
  add_definitions(-D__SSE2__)
  if(NOT SUPPORT_SSE_BUILD) # don't double up
    add_definitions(-D__MMX__)
  endif()
endif()


# set the endian define
if(MSVC)
  # for some reason this fails on msvc
  add_definitions(-D__LITTLE_ENDIAN__)

  # OSX-Note: as we do cross-compiling with specific set architecture,
  # endianess-detection and auto-setting is counterproductive
  # so we just set endianness according CMAKE_OSX_ARCHITECTURES

elseif(CMAKE_OSX_ARCHITECTURES MATCHES i386 OR CMAKE_OSX_ARCHITECTURES MATCHES x86_64)
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

  list(APPEND OPENVDB_INCLUDE_DIRS
    ${BOOST_INCLUDE_DIR}
    ${TBB_INCLUDE_DIRS}
    ${OPENEXR_INCLUDE_DIRS})

  list(APPEND OPENVDB_LIBRARIES ${OPENEXR_LIBRARIES} ${ZLIB_LIBRARIES})

  if(WITH_OPENVDB_BLOSC)
    list(APPEND OPENVDB_DEFINITIONS -DWITH_OPENVDB_BLOSC)
    list(APPEND OPENVDB_LIBRARIES ${BLOSC_LIBRARIES} ${ZLIB_LIBRARIES})
  endif()

  list(APPEND OPENVDB_LIBRARIES ${BOOST_LIBRARIES} ${TBB_LIBRARIES})
endif()

#-----------------------------------------------------------------------------
# Configure OpenGL.

find_package(OpenGL)
blender_include_dirs_sys("${OPENGL_INCLUDE_DIR}")

if(WITH_OPENGL)
  add_definitions(-DWITH_OPENGL)
endif()

if(WITH_SYSTEM_GLES)
  find_package_wrapper(OpenGLES)
endif()

if(WITH_GL_PROFILE_ES20)
  if(WITH_SYSTEM_GLES)
    if(NOT OPENGLES_LIBRARY)
      message(FATAL_ERROR
        "Unable to find OpenGL ES libraries. "
        "Install them or disable WITH_SYSTEM_GLES."
      )
    endif()

    list(APPEND BLENDER_GL_LIBRARIES "${OPENGLES_LIBRARY}")

  else()
    set(OPENGLES_LIBRARY "" CACHE FILEPATH "OpenGL ES 2.0 library file")
    mark_as_advanced(OPENGLES_LIBRARY)

    list(APPEND BLENDER_GL_LIBRARIES "${OPENGLES_LIBRARY}")

    if(NOT OPENGLES_LIBRARY)
      message(FATAL_ERROR
        "To compile WITH_GL_EGL you need to set OPENGLES_LIBRARY "
        "to the file path of an OpenGL ES 2.0 library."
      )
    endif()

  endif()

  if(WIN32)
    # Setup paths to files needed to install and redistribute Windows Blender with OpenGL ES

    set(OPENGLES_DLL "" CACHE FILEPATH "OpenGL ES 2.0 redistributable DLL file")
    mark_as_advanced(OPENGLES_DLL)

    if(NOT OPENGLES_DLL)
      message(FATAL_ERROR
        "To compile WITH_GL_PROFILE_ES20 you need to set OPENGLES_DLL to the file "
        "path of an OpenGL ES 2.0 runtime dynamic link library (DLL)."
      )
    endif()

    if(WITH_GL_ANGLE)
      list(APPEND GL_DEFINITIONS -DWITH_ANGLE)

      set(D3DCOMPILER_DLL "" CACHE FILEPATH "Direct3D Compiler redistributable DLL file (needed by ANGLE)")

      get_filename_component(D3DCOMPILER_FILENAME "${D3DCOMPILER_DLL}" NAME)
      list(APPEND GL_DEFINITIONS "-DD3DCOMPILER=\"\\\"${D3DCOMPILER_FILENAME}\\\"\"")

      mark_as_advanced(D3DCOMPILER_DLL)

      if(D3DCOMPILER_DLL STREQUAL "")
        message(FATAL_ERROR
          "To compile WITH_GL_ANGLE you need to set D3DCOMPILER_DLL to the file "
          "path of a copy of the DirectX redistributable DLL file: D3DCompiler_46.dll"
        )
      endif()

    endif()

  endif()

else()
  if(OpenGL_GL_PREFERENCE STREQUAL "LEGACY" AND OPENGL_gl_LIBRARY)
    list(APPEND BLENDER_GL_LIBRARIES ${OPENGL_gl_LIBRARY})
  else()
    list(APPEND BLENDER_GL_LIBRARIES ${OPENGL_opengl_LIBRARY} ${OPENGL_glx_LIBRARY})
  endif()
endif()

if(WITH_GL_EGL)
  find_package(OpenGL REQUIRED EGL)
  list(APPEND BLENDER_GL_LIBRARIES OpenGL::EGL)

  list(APPEND GL_DEFINITIONS -DWITH_GL_EGL -DGLEW_EGL -DGLEW_INC_EGL)

  if(WITH_SYSTEM_GLES)
    if(NOT OPENGLES_EGL_LIBRARY)
      message(FATAL_ERROR
        "Unable to find OpenGL ES libraries. "
        "Install them or disable WITH_SYSTEM_GLES."
      )
    endif()

    list(APPEND BLENDER_GL_LIBRARIES ${OPENGLES_EGL_LIBRARY})

  else()
    set(OPENGLES_EGL_LIBRARY "" CACHE FILEPATH "EGL library file")
    mark_as_advanced(OPENGLES_EGL_LIBRARY)

    list(APPEND BLENDER_GL_LIBRARIES "${OPENGLES_LIBRARY}" "${OPENGLES_EGL_LIBRARY}")

    if(NOT OPENGLES_EGL_LIBRARY)
      message(FATAL_ERROR
        "To compile WITH_GL_EGL you need to set OPENGLES_EGL_LIBRARY "
        "to the file path of an EGL library."
      )
    endif()

  endif()

  if(WIN32)
    # Setup paths to files needed to install and redistribute Windows Blender with OpenGL ES

    set(OPENGLES_EGL_DLL "" CACHE FILEPATH "EGL redistributable DLL file")
    mark_as_advanced(OPENGLES_EGL_DLL)

    if(NOT OPENGLES_EGL_DLL)
      message(FATAL_ERROR
        "To compile WITH_GL_EGL you need to set OPENGLES_EGL_DLL "
        "to the file path of an EGL runtime dynamic link library (DLL)."
      )
    endif()

  endif()

endif()

if(WITH_GL_PROFILE_ES20)
  list(APPEND GL_DEFINITIONS -DWITH_GL_PROFILE_ES20)
else()
  list(APPEND GL_DEFINITIONS -DWITH_GL_PROFILE_CORE)
endif()

#-----------------------------------------------------------------------------
# Configure OpenMP.
if(WITH_OPENMP)
  if(NOT OPENMP_CUSTOM)
    find_package(OpenMP)
  endif()

  if(OPENMP_FOUND)
    if(NOT WITH_OPENMP_STATIC)
      set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
    else()
      # Typically avoid adding flags as defines but we can't
      # pass OpenMP flags to the linker for static builds, meaning
      # we can't add any OpenMP related flags to CFLAGS variables
      # since they're passed to the linker as well.
      add_definitions("${OpenMP_C_FLAGS}")

      find_library_static(OpenMP_LIBRARIES gomp ${CMAKE_CXX_IMPLICIT_LINK_DIRECTORIES})
    endif()
  else()
    set(WITH_OPENMP OFF)
  endif()

  mark_as_advanced(
    OpenMP_C_FLAGS
    OpenMP_CXX_FLAGS
  )
endif()

#-----------------------------------------------------------------------------
# Configure GLEW

if(WITH_SYSTEM_GLEW)
  find_package(GLEW)

  # Note: There is an assumption here that the system GLEW is not a static library.

  if(NOT GLEW_FOUND)
    message(FATAL_ERROR "GLEW is required to build Blender. Install it or disable WITH_SYSTEM_GLEW.")
  endif()

  set(GLEW_INCLUDE_PATH "${GLEW_INCLUDE_DIR}")
  set(BLENDER_GLEW_LIBRARIES ${GLEW_LIBRARY})
else()
  if(WITH_GLEW_ES)
    set(GLEW_INCLUDE_PATH "${CMAKE_SOURCE_DIR}/extern/glew-es/include")

    list(APPEND GL_DEFINITIONS -DGLEW_STATIC -DWITH_GLEW_ES)

    # These definitions remove APIs from glew.h, making GLEW smaller, and catching unguarded API usage
    if(WITH_GL_PROFILE_ES20)
      list(APPEND GL_DEFINITIONS -DGLEW_ES_ONLY)
    else()
      # No ES functions are needed
      list(APPEND GL_DEFINITIONS -DGLEW_NO_ES)
    endif()

    if(WITH_GL_PROFILE_ES20)
      if(WITH_GL_EGL)
        list(APPEND GL_DEFINITIONS -DGLEW_USE_LIB_ES20)
      endif()

      # ToDo: This is an experiment to eliminate ES 1 symbols,
      # GLEW doesn't really properly provide this level of control
      # (for example, without modification it eliminates too many symbols)
      # so there are lots of modifications to GLEW to make this work,
      # and no attempt to make it work beyond Blender at this point.
      list(APPEND GL_DEFINITIONS -DGL_ES_VERSION_1_0=0 -DGL_ES_VERSION_CL_1_1=0 -DGL_ES_VERSION_CM_1_1=0)
    endif()

    set(BLENDER_GLEW_LIBRARIES extern_glew_es bf_intern_glew_mx)

  else()
    set(GLEW_INCLUDE_PATH "${CMAKE_SOURCE_DIR}/extern/glew/include")

    list(APPEND GL_DEFINITIONS -DGLEW_STATIC)

    # This won't affect the non-experimental glew library,
    # but is used for conditional compilation elsewhere.
    list(APPEND GL_DEFINITIONS -DGLEW_NO_ES)

    set(BLENDER_GLEW_LIBRARIES extern_glew)

  endif()

endif()

list(APPEND GL_DEFINITIONS -DGLEW_NO_GLU)

#-----------------------------------------------------------------------------
# Configure Bullet

if(WITH_BULLET AND WITH_SYSTEM_BULLET)
  find_package(Bullet)
  if(NOT BULLET_FOUND)
    set(WITH_BULLET OFF)
  endif()
else()
  set(BULLET_INCLUDE_DIRS "${CMAKE_SOURCE_DIR}/extern/bullet2/src")
  # set(BULLET_LIBRARIES "")
endif()

#-----------------------------------------------------------------------------
# Configure Python.

if(WITH_PYTHON_MODULE)
  add_definitions(-DPy_ENABLE_SHARED)
endif()

#-----------------------------------------------------------------------------
# Configure GLog/GFlags

if(WITH_LIBMV OR WITH_GTESTS OR (WITH_CYCLES AND WITH_CYCLES_LOGGING))
  if(WITH_SYSTEM_GFLAGS)
    find_package(Gflags)
    if(NOT GFLAGS_FOUND)
      message(FATAL_ERROR "System wide Gflags is requested but was not found")
    endif()
    # FindGflags does not define this, and we are not even sure what to use here.
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
    # FindGlog does not define this, and we are not even sure what to use here.
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

#-----------------------------------------------------------------------------
# Configure Ceres

if(WITH_LIBMV)
  # We always have C++11 which includes unordered_map.
  set(CERES_DEFINES "-DCERES_STD_UNORDERED_MAP;-DCERES_USE_CXX_THREADS")
endif()

#-----------------------------------------------------------------------------
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

    # Heuristics... the more cores we have, the more free mem we have to keep for the non-heavy tasks too.
    if(${_TOT_MEM} LESS 8000 AND ${_NUM_CORES} GREATER 2)
      set(_compile_heavy_jobs "1")
    elseif(${_TOT_MEM} LESS 16000 AND ${_NUM_CORES} GREATER 4)
      set(_compile_heavy_jobs "2")
    elseif(${_TOT_MEM} LESS 24000 AND ${_NUM_CORES} GREATER 8)
      set(_compile_heavy_jobs "3")
    elseif(${_TOT_MEM} LESS 32000 AND ${_NUM_CORES} GREATER 16)
      set(_compile_heavy_jobs "4")
    elseif(${_TOT_MEM} LESS 64000 AND ${_NUM_CORES} GREATER 32)
      set(_compile_heavy_jobs "8")
    else()
      set(_compile_heavy_jobs "")
    endif()

    set(NINJA_MAX_NUM_PARALLEL_COMPILE_HEAVY_JOBS "${_compile_heavy_jobs}" CACHE STRING
        "Define the maximum number of concurrent heavy compilation jobs, for ninja build system (used for some targets which cpp files can take several GB each during compilation)." FORCE)
    mark_as_advanced(NINJA_MAX_NUM_PARALLEL_COMPILE_HEAVY_JOBS)
    set(_compile_heavy_jobs)

    # Only set regular compile jobs if we set heavy jobs, otherwise default (using all cores) if fine.
    if(NINJA_MAX_NUM_PARALLEL_COMPILE_HEAVY_JOBS)
      math(EXPR _compile_jobs "${_NUM_CORES} - 1")
    else()
      set(_compile_jobs "")
    endif()
    set(NINJA_MAX_NUM_PARALLEL_COMPILE_JOBS "${_compile_jobs}" CACHE STRING
        "Define the maximum number of concurrent compilation jobs, for ninja build system." FORCE)
    mark_as_advanced(NINJA_MAX_NUM_PARALLEL_COMPILE_JOBS)
    set(_compile_jobs)

    # In practice, even when there is RAM available, this proves to be quicker than running in parallel
    # (due to slow disks accesses).
    set(NINJA_MAX_NUM_PARALLEL_LINK_JOBS "1" CACHE STRING
        "Define the maximum number of concurrent link jobs, for ninja build system." FORCE)
    mark_as_advanced(NINJA_MAX_NUM_PARALLEL_LINK_JOBS)

    set(_NUM_CORES)
    set(_TOT_MEM)
  endif()

  if(NINJA_MAX_NUM_PARALLEL_COMPILE_JOBS)
    set_property(GLOBAL APPEND PROPERTY JOB_POOLS compile_job_pool=${NINJA_MAX_NUM_PARALLEL_COMPILE_JOBS})
    set(CMAKE_JOB_POOL_COMPILE compile_job_pool)
  endif()

  if(NINJA_MAX_NUM_PARALLEL_COMPILE_HEAVY_JOBS)
    set_property(GLOBAL APPEND PROPERTY JOB_POOLS compile_heavy_job_pool=${NINJA_MAX_NUM_PARALLEL_COMPILE_HEAVY_JOBS})
  endif()

  if(NINJA_MAX_NUM_PARALLEL_LINK_JOBS)
    set_property(GLOBAL APPEND PROPERTY JOB_POOLS link_job_pool=${NINJA_MAX_NUM_PARALLEL_LINK_JOBS})
    set(CMAKE_JOB_POOL_LINK link_job_pool)
  endif()
endif()

#-----------------------------------------------------------------------------
# Extra compile flags

if(CMAKE_COMPILER_IS_GNUCC)

  ADD_CHECK_C_COMPILER_FLAG(C_WARNINGS C_WARN_ALL -Wall)
  ADD_CHECK_C_COMPILER_FLAG(C_WARNINGS C_WARN_CAST_ALIGN -Wcast-align)
  ADD_CHECK_C_COMPILER_FLAG(C_WARNINGS C_WARN_ERROR_IMPLICIT_FUNCTION_DECLARATION -Werror=implicit-function-declaration)
  ADD_CHECK_C_COMPILER_FLAG(C_WARNINGS C_WARN_ERROR_RETURN_TYPE  -Werror=return-type)
  ADD_CHECK_C_COMPILER_FLAG(C_WARNINGS C_WARN_ERROR_VLA -Werror=vla)
  # system headers sometimes do this, disable for now, was: -Werror=strict-prototypes
  ADD_CHECK_C_COMPILER_FLAG(C_WARNINGS C_WARN_STRICT_PROTOTYPES  -Wstrict-prototypes)
  ADD_CHECK_C_COMPILER_FLAG(C_WARNINGS C_WARN_MISSING_PROTOTYPES -Wmissing-prototypes)
  ADD_CHECK_C_COMPILER_FLAG(C_WARNINGS C_WARN_NO_CHAR_SUBSCRIPTS -Wno-char-subscripts)
  ADD_CHECK_C_COMPILER_FLAG(C_WARNINGS C_WARN_NO_UNKNOWN_PRAGMAS -Wno-unknown-pragmas)
  ADD_CHECK_C_COMPILER_FLAG(C_WARNINGS C_WARN_POINTER_ARITH -Wpointer-arith)
  ADD_CHECK_C_COMPILER_FLAG(C_WARNINGS C_WARN_UNUSED_PARAMETER -Wunused-parameter)
  ADD_CHECK_C_COMPILER_FLAG(C_WARNINGS C_WARN_WRITE_STRINGS -Wwrite-strings)
  ADD_CHECK_C_COMPILER_FLAG(C_WARNINGS C_WARN_LOGICAL_OP -Wlogical-op)
  ADD_CHECK_C_COMPILER_FLAG(C_WARNINGS C_WARN_UNDEF -Wundef)
  ADD_CHECK_C_COMPILER_FLAG(C_WARNINGS C_WARN_INIT_SELF -Winit-self)  # needs -Wuninitialized
  ADD_CHECK_C_COMPILER_FLAG(C_WARNINGS C_WARN_MISSING_INCLUDE_DIRS -Wmissing-include-dirs)
  ADD_CHECK_C_COMPILER_FLAG(C_WARNINGS C_WARN_NO_DIV_BY_ZERO -Wno-div-by-zero)
  ADD_CHECK_C_COMPILER_FLAG(C_WARNINGS C_WARN_TYPE_LIMITS -Wtype-limits)
  ADD_CHECK_C_COMPILER_FLAG(C_WARNINGS C_WARN_FORMAT_SIGN -Wformat-signedness)
  ADD_CHECK_C_COMPILER_FLAG(C_WARNINGS C_WARN_RESTRICT -Wrestrict)

  # C-only.
  ADD_CHECK_C_COMPILER_FLAG(C_WARNINGS C_WARN_NO_NULL -Wnonnull)
  ADD_CHECK_C_COMPILER_FLAG(C_WARNINGS C_WARN_ABSOLUTE_VALUE -Wabsolute-value)

  # gcc 4.2 gives annoying warnings on every file with this
  if(NOT "${CMAKE_C_COMPILER_VERSION}" VERSION_LESS "4.3")
    ADD_CHECK_C_COMPILER_FLAG(C_WARNINGS C_WARN_UNINITIALIZED -Wuninitialized)
  endif()

  # versions before gcc4.6 give many BLI_math warnings
  if(NOT "${CMAKE_C_COMPILER_VERSION}" VERSION_LESS "4.6")
    ADD_CHECK_C_COMPILER_FLAG(C_WARNINGS C_WARN_REDUNDANT_DECLS       -Wredundant-decls)
    ADD_CHECK_CXX_COMPILER_FLAG(CXX_WARNINGS CXX_WARN_REDUNDANT_DECLS -Wredundant-decls)
  endif()

  # versions before gcc4.8 include global name-space.
  if(NOT "${CMAKE_C_COMPILER_VERSION}" VERSION_LESS "4.8")
    ADD_CHECK_C_COMPILER_FLAG(C_WARNINGS C_WARN_SHADOW -Wshadow)
  endif()

  # disable because it gives warnings for printf() & friends.
  # ADD_CHECK_C_COMPILER_FLAG(C_WARNINGS C_WARN_DOUBLE_PROMOTION -Wdouble-promotion -Wno-error=double-promotion)

  if(NOT APPLE)
    ADD_CHECK_C_COMPILER_FLAG(C_WARNINGS C_WARN_NO_ERROR_UNUSED_BUT_SET_VARIABLE -Wno-error=unused-but-set-variable)
  endif()

  ADD_CHECK_CXX_COMPILER_FLAG(CXX_WARNINGS CXX_WARN_ALL -Wall)
  ADD_CHECK_CXX_COMPILER_FLAG(CXX_WARNINGS CXX_WARN_NO_INVALID_OFFSETOF -Wno-invalid-offsetof)
  ADD_CHECK_CXX_COMPILER_FLAG(CXX_WARNINGS CXX_WARN_NO_SIGN_COMPARE -Wno-sign-compare)
  ADD_CHECK_CXX_COMPILER_FLAG(CXX_WARNINGS CXX_WARN_LOGICAL_OP -Wlogical-op)
  ADD_CHECK_CXX_COMPILER_FLAG(CXX_WARNINGS CXX_WARN_INIT_SELF -Winit-self)  # needs -Wuninitialized
  ADD_CHECK_CXX_COMPILER_FLAG(CXX_WARNINGS CXX_WARN_MISSING_INCLUDE_DIRS -Wmissing-include-dirs)
  ADD_CHECK_CXX_COMPILER_FLAG(CXX_WARNINGS CXX_WARN_NO_DIV_BY_ZERO -Wno-div-by-zero)
  ADD_CHECK_CXX_COMPILER_FLAG(CXX_WARNINGS CXX_WARN_TYPE_LIMITS -Wtype-limits)
  ADD_CHECK_CXX_COMPILER_FLAG(CXX_WARNINGS CXX_WARN_ERROR_RETURN_TYPE  -Werror=return-type)
  ADD_CHECK_CXX_COMPILER_FLAG(CXX_WARNINGS CXX_WARN_NO_CHAR_SUBSCRIPTS -Wno-char-subscripts)
  ADD_CHECK_CXX_COMPILER_FLAG(CXX_WARNINGS CXX_WARN_NO_UNKNOWN_PRAGMAS -Wno-unknown-pragmas)
  ADD_CHECK_CXX_COMPILER_FLAG(CXX_WARNINGS CXX_WARN_POINTER_ARITH -Wpointer-arith)
  ADD_CHECK_CXX_COMPILER_FLAG(CXX_WARNINGS CXX_WARN_UNUSED_PARAMETER -Wunused-parameter)
  ADD_CHECK_CXX_COMPILER_FLAG(CXX_WARNINGS CXX_WARN_WRITE_STRINGS -Wwrite-strings)
  ADD_CHECK_CXX_COMPILER_FLAG(CXX_WARNINGS CXX_WARN_UNDEF -Wundef)
  ADD_CHECK_CXX_COMPILER_FLAG(CXX_WARNINGS CXX_WARN_FORMAT_SIGN -Wformat-signedness)
  ADD_CHECK_CXX_COMPILER_FLAG(CXX_WARNINGS CXX_WARN_RESTRICT -Wrestrict)

  # gcc 4.2 gives annoying warnings on every file with this
  if(NOT "${CMAKE_C_COMPILER_VERSION}" VERSION_LESS "4.3")
    ADD_CHECK_CXX_COMPILER_FLAG(CXX_WARNINGS CXX_WARN_UNINITIALIZED -Wuninitialized)
  endif()

  # causes too many warnings
  if(NOT APPLE)
    ADD_CHECK_CXX_COMPILER_FLAG(CXX_WARNINGS CXX_WARN_UNDEF -Wundef)
    ADD_CHECK_CXX_COMPILER_FLAG(CXX_WARNINGS CXX_WARN_MISSING_DECLARATIONS -Wmissing-declarations)
  endif()

  # Use 'ATTR_FALLTHROUGH' macro to suppress.
  if(CMAKE_COMPILER_IS_GNUCC AND (NOT "${CMAKE_C_COMPILER_VERSION}" VERSION_LESS "7.0"))
    ADD_CHECK_C_COMPILER_FLAG(C_WARNINGS C_WARN_IMPLICIT_FALLTHROUGH -Wimplicit-fallthrough=5)
    ADD_CHECK_CXX_COMPILER_FLAG(CXX_WARNINGS CXX_WARN_IMPLICIT_FALLTHROUGH -Wimplicit-fallthrough=5)
  endif()

  # flags to undo strict flags
  ADD_CHECK_C_COMPILER_FLAG(C_REMOVE_STRICT_FLAGS C_WARN_NO_DEPRECATED_DECLARATIONS -Wno-deprecated-declarations)
  ADD_CHECK_C_COMPILER_FLAG(C_REMOVE_STRICT_FLAGS C_WARN_NO_UNUSED_PARAMETER        -Wno-unused-parameter)
  ADD_CHECK_C_COMPILER_FLAG(C_REMOVE_STRICT_FLAGS C_WARN_NO_UNUSED_FUNCTION         -Wno-unused-function)
  ADD_CHECK_C_COMPILER_FLAG(C_REMOVE_STRICT_FLAGS C_WARN_NO_TYPE_LIMITS             -Wno-type-limits)
  ADD_CHECK_C_COMPILER_FLAG(C_REMOVE_STRICT_FLAGS C_WARN_NO_INT_IN_BOOL_CONTEXT     -Wno-int-in-bool-context)
  ADD_CHECK_C_COMPILER_FLAG(C_REMOVE_STRICT_FLAGS C_WARN_NO_FORMAT                  -Wno-format)
  ADD_CHECK_C_COMPILER_FLAG(C_REMOVE_STRICT_FLAGS C_WARN_NO_SWITCH                  -Wno-switch)

  ADD_CHECK_CXX_COMPILER_FLAG(CXX_REMOVE_STRICT_FLAGS CXX_WARN_NO_CLASS_MEMACCESS     -Wno-class-memaccess)
  ADD_CHECK_CXX_COMPILER_FLAG(CXX_REMOVE_STRICT_FLAGS CXX_WARN_NO_COMMENT             -Wno-comment)
  ADD_CHECK_CXX_COMPILER_FLAG(CXX_REMOVE_STRICT_FLAGS CXX_WARN_NO_UNUSED_TYPEDEFS     -Wno-unused-local-typedefs)

  if(CMAKE_COMPILER_IS_GNUCC AND (NOT "${CMAKE_C_COMPILER_VERSION}" VERSION_LESS "7.0"))
    ADD_CHECK_C_COMPILER_FLAG(C_REMOVE_STRICT_FLAGS C_WARN_NO_IMPLICIT_FALLTHROUGH    -Wno-implicit-fallthrough)
  endif()

  if(NOT APPLE)
    ADD_CHECK_C_COMPILER_FLAG(C_REMOVE_STRICT_FLAGS C_WARN_NO_ERROR_UNUSED_BUT_SET_VARIABLE -Wno-error=unused-but-set-variable)
  endif()

elseif(CMAKE_C_COMPILER_ID MATCHES "Clang")

  # strange, clang complains these are not supported, but then uses them.
  ADD_CHECK_C_COMPILER_FLAG(C_WARNINGS C_WARN_ALL -Wall)
  ADD_CHECK_C_COMPILER_FLAG(C_WARNINGS C_WARN_ERROR_IMPLICIT_FUNCTION_DECLARATION -Werror=implicit-function-declaration)
  ADD_CHECK_C_COMPILER_FLAG(C_WARNINGS C_WARN_ERROR_RETURN_TYPE  -Werror=return-type)
  ADD_CHECK_C_COMPILER_FLAG(C_WARNINGS C_WARN_NO_AUTOLOGICAL_COMPARE -Wno-tautological-compare)
  ADD_CHECK_C_COMPILER_FLAG(C_WARNINGS C_WARN_NO_UNKNOWN_PRAGMAS -Wno-unknown-pragmas)
  ADD_CHECK_C_COMPILER_FLAG(C_WARNINGS C_WARN_NO_CHAR_SUBSCRIPTS -Wno-char-subscripts)
  ADD_CHECK_C_COMPILER_FLAG(C_WARNINGS C_WARN_STRICT_PROTOTYPES  -Wstrict-prototypes)
  ADD_CHECK_C_COMPILER_FLAG(C_WARNINGS C_WARN_MISSING_PROTOTYPES -Wmissing-prototypes)
  ADD_CHECK_C_COMPILER_FLAG(C_WARNINGS C_WARN_UNUSED_PARAMETER -Wunused-parameter)

  ADD_CHECK_CXX_COMPILER_FLAG(CXX_WARNINGS CXX_WARN_ALL -Wall)
  ADD_CHECK_CXX_COMPILER_FLAG(CXX_WARNINGS CXX_WARN_NO_AUTOLOGICAL_COMPARE -Wno-tautological-compare)
  ADD_CHECK_CXX_COMPILER_FLAG(CXX_WARNINGS CXX_WARN_NO_UNKNOWN_PRAGMAS     -Wno-unknown-pragmas)
  ADD_CHECK_CXX_COMPILER_FLAG(CXX_WARNINGS CXX_WARN_NO_CHAR_SUBSCRIPTS     -Wno-char-subscripts)
  ADD_CHECK_CXX_COMPILER_FLAG(CXX_WARNINGS CXX_WARN_NO_OVERLOADED_VIRTUAL  -Wno-overloaded-virtual)  # we get a lot of these, if its a problem a dev needs to look into it.
  ADD_CHECK_CXX_COMPILER_FLAG(CXX_WARNINGS CXX_WARN_NO_SIGN_COMPARE        -Wno-sign-compare)
  ADD_CHECK_CXX_COMPILER_FLAG(CXX_WARNINGS CXX_WARN_NO_INVALID_OFFSETOF    -Wno-invalid-offsetof)

  # gives too many unfixable warnings
  # ADD_CHECK_C_COMPILER_FLAG(C_WARNINGS C_WARN_UNUSED_MACROS      -Wunused-macros)
  # ADD_CHECK_CXX_COMPILER_FLAG(CXX_WARNINGS CXX_WARN_UNUSED_MACROS          -Wunused-macros)

  # flags to undo strict flags
  ADD_CHECK_C_COMPILER_FLAG(C_REMOVE_STRICT_FLAGS C_WARN_NO_UNUSED_PARAMETER -Wno-unused-parameter)
  ADD_CHECK_C_COMPILER_FLAG(C_REMOVE_STRICT_FLAGS C_WARN_NO_UNUSED_VARIABLE  -Wno-unused-variable)
  ADD_CHECK_C_COMPILER_FLAG(C_REMOVE_STRICT_FLAGS C_WARN_NO_UNUSED_MACROS    -Wno-unused-macros)

  ADD_CHECK_C_COMPILER_FLAG(C_REMOVE_STRICT_FLAGS C_WARN_NO_MISSING_VARIABLE_DECLARATIONS -Wno-missing-variable-declarations)
  ADD_CHECK_C_COMPILER_FLAG(C_REMOVE_STRICT_FLAGS C_WARN_NO_INCOMPAT_PTR_DISCARD_QUAL -Wno-incompatible-pointer-types-discards-qualifiers)
  ADD_CHECK_C_COMPILER_FLAG(C_REMOVE_STRICT_FLAGS C_WARN_NO_UNUSED_FUNCTION -Wno-unused-function)
  ADD_CHECK_C_COMPILER_FLAG(C_REMOVE_STRICT_FLAGS C_WARN_NO_INT_TO_VOID_POINTER_CAST -Wno-int-to-void-pointer-cast)
  ADD_CHECK_C_COMPILER_FLAG(C_REMOVE_STRICT_FLAGS C_WARN_NO_MISSING_PROTOTYPES -Wno-missing-prototypes)
  ADD_CHECK_C_COMPILER_FLAG(C_REMOVE_STRICT_FLAGS C_WARN_NO_DUPLICATE_ENUM -Wno-duplicate-enum)
  ADD_CHECK_C_COMPILER_FLAG(C_REMOVE_STRICT_FLAGS C_WARN_NO_UNDEF -Wno-undef)
  ADD_CHECK_C_COMPILER_FLAG(C_REMOVE_STRICT_FLAGS C_WARN_NO_MISSING_NORETURN -Wno-missing-noreturn)

  ADD_CHECK_CXX_COMPILER_FLAG(CXX_REMOVE_STRICT_FLAGS CXX_WARN_NO_UNUSED_PRIVATE_FIELD -Wno-unused-private-field)
  ADD_CHECK_CXX_COMPILER_FLAG(CXX_REMOVE_STRICT_FLAGS CXX_WARN_NO_CXX11_NARROWING -Wno-c++11-narrowing)
  ADD_CHECK_CXX_COMPILER_FLAG(CXX_REMOVE_STRICT_FLAGS CXX_WARN_NO_NON_VIRTUAL_DTOR -Wno-non-virtual-dtor)
  ADD_CHECK_CXX_COMPILER_FLAG(CXX_REMOVE_STRICT_FLAGS CXX_WARN_NO_UNUSED_MACROS -Wno-unused-macros)
  ADD_CHECK_CXX_COMPILER_FLAG(CXX_REMOVE_STRICT_FLAGS CXX_WARN_NO_REORDER -Wno-reorder)
  ADD_CHECK_CXX_COMPILER_FLAG(CXX_REMOVE_STRICT_FLAGS CXX_WARN_NO_COMMENT -Wno-comment)
  ADD_CHECK_CXX_COMPILER_FLAG(CXX_REMOVE_STRICT_FLAGS CXX_WARN_NO_UNUSED_TYPEDEFS -Wno-unused-local-typedefs)
  ADD_CHECK_CXX_COMPILER_FLAG(CXX_REMOVE_STRICT_FLAGS CXX_WARN_NO_UNDEFINED_VAR_TEMPLATE -Wno-undefined-var-template)
  ADD_CHECK_CXX_COMPILER_FLAG(CXX_REMOVE_STRICT_FLAGS CXX_WARN_NO_INSTANTIATION_AFTER_SPECIALIZATION -Wno-instantiation-after-specialization)

elseif(CMAKE_C_COMPILER_ID MATCHES "Intel")

  ADD_CHECK_C_COMPILER_FLAG(C_WARNINGS C_WARN_ALL -Wall)
  ADD_CHECK_C_COMPILER_FLAG(C_WARNINGS C_WARN_POINTER_ARITH -Wpointer-arith)
  ADD_CHECK_C_COMPILER_FLAG(C_WARNINGS C_WARN_NO_UNKNOWN_PRAGMAS -Wno-unknown-pragmas)

  ADD_CHECK_CXX_COMPILER_FLAG(CXX_WARNINGS CXX_WARN_ALL -Wall)
  ADD_CHECK_CXX_COMPILER_FLAG(CXX_WARNINGS CXX_WARN_NO_INVALID_OFFSETOF -Wno-invalid-offsetof)
  ADD_CHECK_CXX_COMPILER_FLAG(CXX_WARNINGS CXX_WARN_NO_SIGN_COMPARE -Wno-sign-compare)

  # disable numbered, false positives
  set(C_WARNINGS "${C_WARNINGS} -wd188,186,144,913,556,858,597,177,1292,167,279,592,94,2722,3199")
  set(CXX_WARNINGS "${CXX_WARNINGS} -wd188,186,144,913,556,858,597,177,1292,167,279,592,94,2722,3199")
elseif(CMAKE_C_COMPILER_ID MATCHES "MSVC")
  # most msvc warnings are C & C++
  set(_WARNINGS
    # warning level:
    "/W3"
    "/w34062"  # switch statement contains 'default' but no 'case' labels
    "/w34115"  # 'type' : named type definition in parentheses
    "/w34189"  # local variable is initialized but not referenced
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
    # errors:
    "/we4013"  # 'function' undefined; assuming extern returning int
    "/we4133"  # incompatible pointer types
    "/we4431"  # missing type specifier - int assumed
  )

  if(MSVC_VERSION GREATER_EQUAL 1911)
    # see https://docs.microsoft.com/en-us/cpp/error-messages/compiler-warnings/c5038?view=vs-2017
    set(_WARNINGS "${_WARNINGS} /w35038") # order of initialization in c++ constructors
  endif()

  string(REPLACE ";" " " _WARNINGS "${_WARNINGS}")
  set(C_WARNINGS "${_WARNINGS}")
  set(CXX_WARNINGS "${_WARNINGS}")
  unset(_WARNINGS)
endif()

# ensure python header is found since detection can fail, this could happen
# with _any_ library but since we used a fixed python version this tends to
# be most problematic.
if(WITH_PYTHON)
  if(NOT EXISTS "${PYTHON_INCLUDE_DIR}/Python.h")
    message(FATAL_ERROR
      "Missing: \"${PYTHON_INCLUDE_DIR}/Python.h\",\n"
      "Set the cache entry 'PYTHON_INCLUDE_DIR' to point "
      "to a valid python include path. Containing "
      "Python.h for python version \"${PYTHON_VERSION}\""
    )
  endif()

  if(WIN32 OR APPLE)
    # Windows and macOS have this bundled with Python libraries.
  elseif((WITH_PYTHON_INSTALL AND WITH_PYTHON_INSTALL_NUMPY) OR (WITH_AUDASPACE AND NOT WITH_SYSTEM_AUDASPACE))
    if(("${PYTHON_NUMPY_PATH}" STREQUAL "") OR (${PYTHON_NUMPY_PATH} MATCHES NOTFOUND))
      find_python_package(numpy)
      unset(PYTHON_NUMPY_INCLUDE_DIRS CACHE)
      set(PYTHON_NUMPY_INCLUDE_DIRS ${PYTHON_NUMPY_PATH}/numpy/core/include CACHE PATH "Path to the include directory of the numpy module")
      mark_as_advanced(PYTHON_NUMPY_INCLUDE_DIRS)
    endif()
  endif()

  if(WIN32 OR APPLE)
    # pass, we have this in lib/python/site-packages
  elseif(WITH_PYTHON_INSTALL_REQUESTS)
    find_python_package(requests)
  endif()
endif()

if(MSVC)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /std:c++17")
elseif(
  CMAKE_COMPILER_IS_GNUCC OR
  CMAKE_C_COMPILER_ID MATCHES "Clang" OR
  CMAKE_C_COMPILER_ID MATCHES "Intel"
)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17")
else()
  message(FATAL_ERROR "Unknown compiler ${CMAKE_C_COMPILER_ID}, can't enable C++17 build")
endif()

# Visual Studio has all standards it supports available by default
# Clang on windows copies this behavior and does not support these switches
if(
  CMAKE_COMPILER_IS_GNUCC OR
  (CMAKE_C_COMPILER_ID MATCHES "Clang" AND (NOT MSVC)) OR
  (CMAKE_C_COMPILER_ID MATCHES "Intel")
)
  # Use C11 + GNU extensions, works with GCC, Clang, ICC
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu11")
endif()

if(UNIX AND NOT APPLE)
  if(NOT WITH_CXX11_ABI)
    set(PLATFORM_CFLAGS "${PLATFORM_CFLAGS} -D_GLIBCXX_USE_CXX11_ABI=0")
  endif()
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

#-------------------------------------------------------------------------------
# Global Defines

# better not set includes here but this debugging option is off by default.
if(WITH_CXX_GUARDEDALLOC)
  include_directories(${CMAKE_SOURCE_DIR}/intern/guardedalloc)
  add_definitions(-DWITH_CXX_GUARDEDALLOC)
endif()

if(WITH_ASSERT_ABORT)
  add_definitions(-DWITH_ASSERT_ABORT)
endif()

# message(STATUS "Using CFLAGS: ${CMAKE_C_FLAGS}")
# message(STATUS "Using CXXFLAGS: ${CMAKE_CXX_FLAGS}")

#-----------------------------------------------------------------------------
# Libraries

if(WITH_GTESTS)
  include(GTestTesting)
endif()

if(WITH_BLENDER)
  add_subdirectory(intern)
  add_subdirectory(extern)

  # source after intern and extern to gather all
  # internal and external library information first, for test linking
  add_subdirectory(source)
elseif(WITH_CYCLES_STANDALONE)
  add_subdirectory(intern/cycles)
  add_subdirectory(extern/clew)
  if(WITH_CUDA_DYNLOAD)
    add_subdirectory(extern/cuew)
  endif()
  if(NOT WITH_SYSTEM_GLEW)
    add_subdirectory(extern/glew)
  endif()
endif()

#-----------------------------------------------------------------------------
# Blender Application
if(WITH_BLENDER)
  add_subdirectory(source/creator)
endif()


#-----------------------------------------------------------------------------
# Testing
add_subdirectory(tests)


#-----------------------------------------------------------------------------
# Define 'heavy' submodules (for Ninja builder when using pools).
setup_heavy_lib_pool()


#-----------------------------------------------------------------------------
# CPack for generating packages
include(build_files/cmake/packaging.cmake)

#-----------------------------------------------------------------------------
# Use dynamic loading for OpenMP
if(WITH_BLENDER)
  openmp_delayload(blender)
endif()

#-----------------------------------------------------------------------------
# Print Final Configuration

if(FIRST_RUN)

  set(_config_msg "\nBlender Configuration\n=====================")

  function(info_cfg_option
    _setting
    )

    set(_msg "  - ${_setting}")
    string(LENGTH "${_msg}" _len)
    while("32" GREATER "${_len}")
      set(_msg "${_msg} ")
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
  info_cfg_option(WITH_BULLET)
  info_cfg_option(WITH_IK_SOLVER)
  info_cfg_option(WITH_IK_ITASC)
  info_cfg_option(WITH_OPENCOLLADA)
  info_cfg_option(WITH_FFTW3)
  info_cfg_option(WITH_INTERNATIONAL)
  info_cfg_option(WITH_INPUT_NDOF)
  info_cfg_option(WITH_CYCLES)
  info_cfg_option(WITH_FREESTYLE)
  info_cfg_option(WITH_OPENCOLORIO)
  info_cfg_option(WITH_XR_OPENXR)
  info_cfg_option(WITH_OPENIMAGEDENOISE)
  info_cfg_option(WITH_OPENVDB)
  info_cfg_option(WITH_ALEMBIC)
  info_cfg_option(WITH_QUADRIFLOW)
  info_cfg_option(WITH_USD)
  info_cfg_option(WITH_TBB)

  info_cfg_text("Compiler Options:")
  info_cfg_option(WITH_BUILDINFO)
  info_cfg_option(WITH_OPENMP)

  info_cfg_text("System Options:")
  info_cfg_option(WITH_INSTALL_PORTABLE)
  info_cfg_option(WITH_X11_ALPHA)
  info_cfg_option(WITH_X11_XF86VMODE)
  info_cfg_option(WITH_X11_XFIXES)
  info_cfg_option(WITH_X11_XINPUT)
  info_cfg_option(WITH_MEM_JEMALLOC)
  info_cfg_option(WITH_MEM_VALGRIND)
  info_cfg_option(WITH_SYSTEM_GLEW)

  info_cfg_text("Image Formats:")
  info_cfg_option(WITH_OPENIMAGEIO)
  info_cfg_option(WITH_IMAGE_CINEON)
  info_cfg_option(WITH_IMAGE_DDS)
  info_cfg_option(WITH_IMAGE_HDR)
  info_cfg_option(WITH_IMAGE_OPENEXR)
  info_cfg_option(WITH_IMAGE_OPENJPEG)
  info_cfg_option(WITH_IMAGE_TIFF)

  info_cfg_text("Audio:")
  info_cfg_option(WITH_OPENAL)
  info_cfg_option(WITH_SDL)
  info_cfg_option(WITH_SDL_DYNLOAD)
  info_cfg_option(WITH_JACK)
  info_cfg_option(WITH_JACK_DYNLOAD)
  info_cfg_option(WITH_CODEC_AVI)
  info_cfg_option(WITH_CODEC_FFMPEG)
  info_cfg_option(WITH_CODEC_SNDFILE)

  info_cfg_text("Compression:")
  info_cfg_option(WITH_LZMA)
  info_cfg_option(WITH_LZO)

  info_cfg_text("Python:")
  info_cfg_option(WITH_PYTHON_INSTALL)
  info_cfg_option(WITH_PYTHON_INSTALL_NUMPY)
  info_cfg_option(WITH_PYTHON_MODULE)
  info_cfg_option(WITH_PYTHON_SAFETY)
  if(APPLE)
    info_cfg_option(WITH_PYTHON_FRAMEWORK)
  endif()

  info_cfg_text("Modifiers:")
  info_cfg_option(WITH_MOD_REMESH)
  info_cfg_option(WITH_MOD_FLUID)
  info_cfg_option(WITH_MOD_OCEANSIM)

  info_cfg_text("OpenGL:")
  info_cfg_option(WITH_GLEW_ES)
  info_cfg_option(WITH_GL_EGL)
  info_cfg_option(WITH_GL_PROFILE_ES20)
  if(WIN32)
    info_cfg_option(WITH_GL_ANGLE)
  endif()

  info_cfg_text("")

  message("${_config_msg}")
endif()

if(0)
  print_all_vars()
endif()
