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
    find_package(HIP 6.0.0)
    set_and_warn_library_found("HIP compiler" HIP_FOUND WITH_CYCLES_HIP_BINARIES)

    if(HIP_FOUND)
      message(STATUS "Found HIP ${HIP_HIPCC_EXECUTABLE} (${HIP_VERSION})")
    endif()
  endif()

  # HIP RT
  if(WITH_CYCLES_DEVICE_HIP AND WITH_CYCLES_DEVICE_HIPRT)
    if(DEFINED LIBDIR)
      set(HIPRT_ROOT_DIR ${LIBDIR}/hiprt)
    endif()
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

if(WITH_CYCLES_DEVICE_ONEAPI OR EMBREE_SYCL_SUPPORT)
  # Find packages for even when WITH_CYCLES_DEVICE_ONEAPI is OFF, as it's
  # needed for linking to Embree with SYCL support.
  find_package(SYCL)
  find_package(LevelZero)

  if(WITH_CYCLES_DEVICE_ONEAPI)
    set_and_warn_library_found("oneAPI" SYCL_FOUND WITH_CYCLES_DEVICE_ONEAPI)
    set_and_warn_library_found("Level Zero" LEVEL_ZERO_FOUND WITH_CYCLES_DEVICE_ONEAPI)
    if(NOT (SYCL_FOUND AND SYCL_VERSION VERSION_GREATER_EQUAL 6.0 AND LEVEL_ZERO_FOUND))
      message(STATUS "SYCL 6.0+ or Level Zero not found, disabling WITH_CYCLES_DEVICE_ONEAPI")
      set(WITH_CYCLES_DEVICE_ONEAPI OFF)
    endif()
  endif()
endif()

if(WITH_CYCLES_DEVICE_ONEAPI AND WITH_CYCLES_ONEAPI_BINARIES)
  # Centralized place for ocloc usage initialization
  #
  # All ocloc-related setup is done here, in one place, so that every
  # subsequent use of ocloc (validation below, device support queries
  # in oneAPI CMakeLists.txt, and the final AoT compilation command) shares
  # consistent paths and an environment to properly use ocloc.
  #
  # Six variables are established here, for later use:
  #   OCLOC_INSTALL_DIR          - Root directory of the ocloc installation.
  #   IGC_INSTALL_DIR            - Root directory of the Intel Graphics
  #                                Compiler (IGC) installation.
  #   OCLOC_LD_LIBRARY_PATH      - All library paths required to run
  #                                the ocloc binary (can be empty if
  #                                no library paths are needed).
  #   OCLOC_ENV_COMMAND          - The full command to set up library paths
  #                                with cmake -E env command
  #                                (can be empty if no library paths are
  #                                needed).
  #   OCLOC_BINARY_FULL_FILEPATH - Full path to the ocloc binary.
  #   OCLOC_FOUND                - Indicates whether a fully working ocloc binary
  #                                was found.

  # Resolve OCLOC_INSTALL_DIR if not set by user.
  # Without user-specified directory, we are expected to use precompiled
  # (Linux) / bundled (Windows) version from Blender's dependencies at
  # the path:
  # <DPCPP_ROOT_DIRECTORY>/lib/ocloc/
  if(NOT OCLOC_INSTALL_DIR)
    get_filename_component(_sycl_compiler_root ${SYCL_COMPILER} DIRECTORY)
    get_filename_component(OCLOC_INSTALL_DIR "${_sycl_compiler_root}/../lib/ocloc" ABSOLUTE)
    unset(_sycl_compiler_root)
  endif()

  # Resolve IGC_INSTALL_DIR if not set by user.
  #
  # Ocloc at runtime searches for and loads IGC
  # (Intel Graphics Compiler) libraries (libigc.so, etc.).
  #
  # On Windows, IGC_INSTALL_DIR is identical to OCLOC_INSTALL_DIR,
  # since all shared libraries of ocloc are bundled together.
  # On Linux, we are expected to use a precompiled version from Blender's
  # dependencies at the path:
  # <DPCPP_ROOT_DIRECTORY>/lib/igc/
  if(NOT IGC_INSTALL_DIR)
    if (WIN32)
      set(IGC_INSTALL_DIR "${OCLOC_INSTALL_DIR}")
    else()
      get_filename_component(_sycl_compiler_root ${SYCL_COMPILER} DIRECTORY)
      get_filename_component(IGC_INSTALL_DIR "${_sycl_compiler_root}/../lib/igc" ABSOLUTE)
      unset(_sycl_compiler_root)
    endif()
  endif()

  if(WIN32)
    set(OCLOC_BINARY_FULL_FILEPATH ${OCLOC_INSTALL_DIR}/ocloc.exe)
  else()
    set(OCLOC_BINARY_FULL_FILEPATH ${OCLOC_INSTALL_DIR}/bin/ocloc)
  endif()

  # Build the reusable ocloc environment command prefix
  if(WIN32)
    # On Windows, it is expected to be empty, as all shared
    # libraries are expected to be located in the same directory
    # where ocloc binary is located.
    set(OCLOC_LD_LIBRARY_PATH "")
    set(OCLOC_ENV_COMMAND "")
  else()
    set(OCLOC_LD_LIBRARY_PATH "${OCLOC_INSTALL_DIR}/lib:${IGC_INSTALL_DIR}/lib")
    set(OCLOC_ENV_COMMAND
      ${CMAKE_COMMAND} -E env "LD_LIBRARY_PATH=${OCLOC_LD_LIBRARY_PATH}"
    )
  endif()

  set(OCLOC_FOUND ON)
  # Three-stage validation: directory -> binary -> execution
  if(NOT EXISTS ${OCLOC_INSTALL_DIR})
    set(OCLOC_FOUND OFF)
    set(_ocloc_missing_error_msg "oneAPI ocloc directory not found as ${OCLOC_INSTALL_DIR}.")
  elseif (NOT EXISTS ${OCLOC_BINARY_FULL_FILEPATH})
    set(OCLOC_FOUND OFF)
    set(_ocloc_missing_error_msg
      "oneAPI ocloc directory ${OCLOC_INSTALL_DIR} was found."
      "However, the ocloc binary ${OCLOC_BINARY_FULL_FILEPATH} was not found."
    )
  else()
    # Verify that the binary is functional by testing execution.
    # The --help flag is a safe way to test if the binary works properly.
    execute_process(
      COMMAND ${OCLOC_ENV_COMMAND} ${OCLOC_BINARY_FULL_FILEPATH} --help
      RESULT_VARIABLE _ocloc_retcode
      OUTPUT_QUIET
      ERROR_QUIET
    )

    if(NOT _ocloc_retcode EQUAL 0)
      set(OCLOC_FOUND OFF)
      set(_ocloc_missing_error_msg
        "oneAPI ocloc binary ${OCLOC_BINARY_FULL_FILEPATH} was found."
        "However, it has failed to be executed with ${_ocloc_retcode} return code."
      )
    endif()
    unset(_ocloc_retcode)
  endif()

  if(NOT OCLOC_FOUND)
    list(JOIN _ocloc_missing_error_msg " " _ocloc_missing_error_msg)
    message(STATUS "${_ocloc_missing_error_msg} A different ocloc directory can be set using OCLOC_INSTALL_DIR cmake variable.")
    set_and_warn_library_found("ocloc" OCLOC_FOUND WITH_CYCLES_ONEAPI_BINARIES)
    unset(_ocloc_missing_error_msg)
  endif()

endif()
