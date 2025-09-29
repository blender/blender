# SPDX-FileCopyrightText: 2006 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

# Libs that adhere to strict flags
add_subdirectory(curve_fit_nd)
add_subdirectory(fmtlib)
if(WITH_IO_FBX)
  add_subdirectory(ufbx)
endif()

# Otherwise we get warnings here that we cant fix in external projects
remove_strict_flags()

# Not a strict flag, but noisy for code we don't maintain
if(CMAKE_COMPILER_IS_GNUCC)
  remove_cc_flag(
    "-Wmisleading-indentation"
  )
elseif(CMAKE_C_COMPILER_ID MATCHES "Clang")
  remove_cc_flag(
    "-Weverything"
    "-Wextra"
    "-Wnull-pointer-subtraction"
    "-Wcast-function-type-mismatch"
    "-Wignored-qualifiers"
  )
endif()

if(WITH_COMPILER_ASAN AND NOT WITH_COMPILER_ASAN_EXTERN)
  # Not only does MSVC not have an -fno-sanitize=all option, if you remove the /fsanitize=address
  # flag at this point, it will give a linker error as it generates an ODR violation for some
  # vector classes, for details see :
  # https://learn.microsoft.com/en-us/cpp/sanitizers/error-container-overflow?view=msvc-170
  if(NOT MSVC)
    # Disable ASAN for extern dependencies, as it can lead to linking issues due to too large binaries.
    string(APPEND CMAKE_CXX_FLAGS_RELWITHDEBINFO " -fno-sanitize=all")
    string(APPEND CMAKE_C_FLAGS_RELWITHDEBINFO " -fno-sanitize=all")
    string(APPEND CMAKE_CXX_FLAGS_DEBUG " -fno-sanitize=all")
    string(APPEND CMAKE_C_FLAGS_DEBUG " -fno-sanitize=all")
  endif()
endif()


add_subdirectory(rangetree)
add_subdirectory(nanosvg)
add_subdirectory(wcwidth)
add_subdirectory(xxhash)

if(WITH_BULLET)
  if(NOT WITH_SYSTEM_BULLET)
    add_subdirectory(bullet2)
  endif()
endif()

if(WITH_DRACO)
  add_subdirectory(draco)
endif()

if(WITH_BINRELOC)
  add_subdirectory(binreloc)
endif()

if(WITH_CYCLES OR WITH_OPENSUBDIV)
  if((WITH_CYCLES_DEVICE_CUDA OR WITH_CYCLES_DEVICE_OPTIX) AND WITH_CUDA_DYNLOAD)
    add_subdirectory(cuew)
  endif()
  if(WITH_CYCLES_DEVICE_HIP AND WITH_HIP_DYNLOAD)
    add_subdirectory(hipew)
  endif()
endif()

if(WITH_GHOST_X11 AND WITH_GHOST_XDND)
  add_subdirectory(xdnd)
endif()

if(WITH_LIBMV)
  add_subdirectory(ceres)
endif()

if(WITH_LIBMV OR WITH_GTESTS)
  if(NOT WITH_SYSTEM_GFLAGS)
    add_subdirectory(gflags)
  endif()
  add_subdirectory(glog)
endif()

if(WITH_GTESTS)
  add_subdirectory(gtest)
  add_subdirectory(gmock)
endif()

if(WITH_AUDASPACE AND NOT WITH_SYSTEM_AUDASPACE)
  set(AUDASPACE_CMAKE_CFG ${CMAKE_CURRENT_SOURCE_DIR}/audaspace/blender_config.cmake)
  set(LIB_SUFFIX "")  # Quiet uninitialized warning.
  add_subdirectory(audaspace)
  unset(LIB_SUFFIX)
endif()

if(WITH_QUADRIFLOW)
  set(QUADRIFLOW_CMAKE_CFG ${CMAKE_CURRENT_SOURCE_DIR}/quadriflow/blender_config.cmake)
  add_subdirectory(quadriflow)
endif()

if(WITH_MOD_FLUID)
  add_subdirectory(mantaflow)
endif()

if(WITH_VULKAN_BACKEND)
  add_subdirectory(vulkan_memory_allocator)
endif()
