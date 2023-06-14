# SPDX-FileCopyrightText: 2011-2022 Blender Foundation
#
# SPDX-License-Identifier: Apache-2.0

###########################################################################
# SDL
###########################################################################

if(WITH_CYCLES_STANDALONE AND WITH_CYCLES_STANDALONE_GUI)
  # We can't use the version from the Blender precompiled libraries because
  # it does not include the video subsystem.
  find_package(SDL2 REQUIRED)
  set_and_warn_library_found("SDL" SDL2_FOUND WITH_CYCLES_STANDALONE_GUI)

  if(SDL2_FOUND)
    include_directories(
      SYSTEM
      ${SDL2_INCLUDE_DIRS}
    )
  endif()
endif()

###########################################################################
# CUDA
###########################################################################

if(WITH_CYCLES_DEVICE_CUDA AND (WITH_CYCLES_CUDA_BINARIES OR NOT WITH_CUDA_DYNLOAD))
  find_package(CUDA) # Try to auto locate CUDA toolkit
  set_and_warn_library_found("CUDA compiler" CUDA_FOUND WITH_CYCLES_CUDA_BINARIES)

  if(CUDA_FOUND)
    message(STATUS "Found CUDA ${CUDA_NVCC_EXECUTABLE} (${CUDA_VERSION})")
  else()
    if(NOT WITH_CUDA_DYNLOAD)
      message(STATUS "Additionally falling back to dynamic CUDA load")
      set(WITH_CUDA_DYNLOAD ON)
    endif()
  endif()
endif()

###########################################################################
# HIP
###########################################################################

if(WITH_CYCLES_DEVICE_HIP)
  if(WITH_CYCLES_HIP_BINARIES)
    # Need at least HIP 5.5 to solve compiler bug affecting the kernel.
    find_package(HIP 5.5.0)
    set_and_warn_library_found("HIP compiler" HIP_FOUND WITH_CYCLES_HIP_BINARIES)

    if(HIP_FOUND)
      message(STATUS "Found HIP ${HIP_HIPCC_EXECUTABLE} (${HIP_VERSION})")
    endif()
  endif()

  # HIP RT
  if(WITH_CYCLES_DEVICE_HIP AND WITH_CYCLES_DEVICE_HIPRT)
    find_package(HIPRT)
    set_and_warn_library_found("HIP RT" HIPRT_FOUND WITH_CYCLES_DEVICE_HIPRT)
  endif()
endif()

if(NOT WITH_CYCLES_DEVICE_HIP)
  set(WITH_CYCLES_DEVICE_HIPRT OFF)
endif()

if(NOT WITH_HIP_DYNLOAD)
  set(WITH_HIP_DYNLOAD ON)
endif()

###########################################################################
# Metal
###########################################################################

if(WITH_CYCLES_DEVICE_METAL)
  find_library(METAL_LIBRARY Metal)

  # This file was added in the 12.0 SDK, use it as a way to detect the version.
  if(METAL_LIBRARY)
    if(EXISTS "${METAL_LIBRARY}/Headers/MTLFunctionStitching.h")
      set(METAL_FOUND ON)
    else()
      message(STATUS "Metal version too old, must be SDK 12.0 or newer")
      set(METAL_FOUND OFF)
    endif()
  endif()

  set_and_warn_library_found("Metal" METAL_FOUND WITH_CYCLES_DEVICE_METAL)
  if(METAL_FOUND)
    message(STATUS "Found Metal: ${METAL_LIBRARY}")
  endif()
endif()

###########################################################################
# oneAPI
###########################################################################

if(WITH_CYCLES_DEVICE_ONEAPI)
  find_package(SYCL)
  find_package(LevelZero)
  set_and_warn_library_found("oneAPI" SYCL_FOUND WITH_CYCLES_DEVICE_ONEAPI)
  set_and_warn_library_found("Level Zero" LEVEL_ZERO_FOUND WITH_CYCLES_DEVICE_ONEAPI)

  if(SYCL_FOUND AND SYCL_VERSION VERSION_GREATER_EQUAL 6.0 AND LEVEL_ZERO_FOUND)
    message(STATUS "Found Level Zero: ${LEVEL_ZERO_LIBRARY}")

    if(WITH_CYCLES_ONEAPI_BINARIES)
      if(NOT OCLOC_INSTALL_DIR)
        get_filename_component(_sycl_compiler_root ${SYCL_COMPILER} DIRECTORY)
        get_filename_component(OCLOC_INSTALL_DIR "${_sycl_compiler_root}/../lib/ocloc" ABSOLUTE)
        unset(_sycl_compiler_root)
      endif()

      if(NOT EXISTS ${OCLOC_INSTALL_DIR})
        set(OCLOC_FOUND OFF)
        message(STATUS "oneAPI ocloc not found in ${OCLOC_INSTALL_DIR}."
                       " A different ocloc directory can be set using OCLOC_INSTALL_DIR cmake variable.")
        set_and_warn_library_found("ocloc" OCLOC_FOUND WITH_CYCLES_ONEAPI_BINARIES)
      endif()
    endif()
  else()
    message(STATUS "SYCL 6.0+ or Level Zero not found, disabling WITH_CYCLES_DEVICE_ONEAPI")
    set(WITH_CYCLES_DEVICE_ONEAPI OFF)
  endif()
endif()
