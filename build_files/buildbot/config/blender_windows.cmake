# SPDX-FileCopyrightText: 2024 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/config/blender_release.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/blender_common.cmake")

# Variables to help identifying the target platform to be built.
# Works around the fact that CMAKE_SYSTEM_PROCESSOR variable is not yet available at the time
# this configuration is run by the CMake.
if ("$ENV{PROCESSOR_ARCHITECTURE}" STREQUAL "ARM64")
  set(BUILDBOT_ARM64 TRUE)
else()
  set(BUILDBOT_X64 TRUE)
endif()

set(WITH_CYCLES_TEST_OSL     ON CACHE BOOL "" FORCE)

set(HIPRT_COMPILER_PARALLEL_JOBS        6 CACHE STRING "" FORCE)
set(SYCL_OFFLINE_COMPILER_PARALLEL_JOBS 6 CACHE STRING "" FORCE)

# Paths to CUDA tookits with explicit versioning (CUDA<version>_) variables.
function(buildbot_set_cuda_toolkit_major_version_paths_checked major_version)
  get_cuda_toolkit_folder_version("${CUDA${major_version}_VERSION}" folder_version)
  set(CUDA${major_version}_TOOLKIT_ROOT_DIR
      "${BUILDBOT_CUDA_TOOLKITS_ROOT_DIR}/CUDA/v${folder_version}"
      CACHE PATH "" FORCE)
  set(CUDA${major_version}_NVCC_EXECUTABLE
      "${CUDA${major_version}_TOOLKIT_ROOT_DIR}/bin/nvcc.exe" CACHE PATH "" FORCE)
  buildbot_assert_path_exists(
      "${CUDA${major_version}_NVCC_EXECUTABLE}"
      "CUDA-${major_version} NVCC executable")
endfunction()
if(NOT BUILDBOT_ARM64)
  # No CUDA 12 on Windows arm64.
  buildbot_set_cuda_toolkit_major_version_paths_checked(12)
endif()
buildbot_set_cuda_toolkit_major_version_paths_checked(13)

# Paths to the default CUDA toolkit.
set(CUDA_TOOLKIT_ROOT_DIR "${CUDA${DEFAULT_CUDA_MAJOR_VERSION}_TOOLKIT_ROOT_DIR}" CACHE PATH "" FORCE)
set(CUDA_NVCC_EXECUTABLE  "${CUDA${DEFAULT_CUDA_MAJOR_VERSION}_NVCC_EXECUTABLE}"  CACHE PATH "" FORCE)

# DLSS configuration.
set(DLSS_SDK_ROOT "${BUILDBOT_DLSS_TOOLKITS_ROOT_DIR}/DLSS SDK ${DLSS_VERSION}" CACHE PATH "" FORCE)
buildbot_assert_path_exists("${DLSS_SDK_ROOT}" "DLSS root directory")

# HIP configuration.
if(NOT BUILDBOT_ARM64)
  set(HIP_ROOT_DIR "${BUILDBOT_HIP_TOOLKITS_ROOT_DIR}/hip_sdk_${HIP_VERSION}" CACHE PATH "" FORCE)
  buildbot_assert_path_exists("${HIP_ROOT_DIR}" "HIP root directory")
endif()

# OCLOC configuration.
if(NOT BUILDBOT_ARM64)
  set(OCLOC_INSTALL_DIR "${BUILDBOT_OCLOC_TOOLKITS_ROOT_DIR}/ocloc_${OCLOC_VERSION}" CACHE PATH "" FORCE)
  buildbot_assert_path_exists("${OCLOC_INSTALL_DIR}" "OCLOC install directory")
endif()

# OptiX configuration.
set(OPTIX_ROOT_DIR "${BUILDBOT_OPTIX_TOOLKITS_ROOT_DIR}/OptiX SDK ${OPTIX_VERSION}" CACHE PATH "" FORCE)
buildbot_assert_path_exists("${OPTIX_ROOT_DIR}" "OptiX root directory")
