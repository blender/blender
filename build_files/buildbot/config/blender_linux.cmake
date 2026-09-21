# SPDX-FileCopyrightText: 2015-2022 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

# ######## Global feature set settings ########

include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/config/blender_release.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/blender_common.cmake")

message(STATUS "Building in Rocky 8 Linux 64bit environment")

# ######## Linux-specific build options ########
# Options which are specific to Linux-only platforms

set(WITH_DOC_MANPAGE         OFF CACHE BOOL "" FORCE)
set(WITH_CYCLES_TEST_OSL     ON CACHE BOOL "" FORCE)

set(HIPRT_COMPILER_PARALLEL_JOBS        6 CACHE STRING "" FORCE)
set(SYCL_OFFLINE_COMPILER_PARALLEL_JOBS 6 CACHE STRING "" FORCE)

set(WITH_LINUX_OFFICIAL_RELEASE_TESTS   ON CACHE BOOL "" FORCE)

# Validate that some python scripts in our `build_files` and `docs` directories
# can be used with the builder's system python.
set(WITH_SYSTEM_PYTHON_TESTS ON CACHE BOOL "" FORCE)
set(TEST_SYSTEM_PYTHON_EXE "/usr/bin/python3.6" CACHE PATH "" FORCE)

# Paths to CUDA tookits with explicit versioning (CUDA<version>_) variables.
function(buildbot_set_cuda_toolkit_major_version_paths_checked major_version)
  get_cuda_toolkit_folder_version("${CUDA${major_version}_VERSION}" folder_version)
  set(CUDA${major_version}_TOOLKIT_ROOT_DIR
      "${BUILDBOT_CUDA_TOOLKITS_ROOT_DIR}/cuda-${folder_version}"
      CACHE PATH "" FORCE)
  set(CUDA${major_version}_NVCC_EXECUTABLE
      "${CUDA${major_version}_TOOLKIT_ROOT_DIR}/bin/nvcc" CACHE PATH "" FORCE)
  buildbot_assert_path_exists(
      "${CUDA${major_version}_NVCC_EXECUTABLE}"
      "CUDA-${major_version} NVCC executable")
endfunction()
buildbot_set_cuda_toolkit_major_version_paths_checked(12)
buildbot_set_cuda_toolkit_major_version_paths_checked(13)

# Paths to the default CUDA toolkit.
set(CUDA_TOOLKIT_ROOT_DIR "${CUDA${DEFAULT_CUDA_MAJOR_VERSION}_TOOLKIT_ROOT_DIR}" CACHE PATH "" FORCE)
set(CUDA_NVCC_EXECUTABLE  "${CUDA${DEFAULT_CUDA_MAJOR_VERSION}_NVCC_EXECUTABLE}"  CACHE PATH "" FORCE)

# DLSS configuration.
set(DLSS_SDK_ROOT "${BUILDBOT_DLSS_TOOLKITS_ROOT_DIR}/dlss-sdk-${DLSS_VERSION}" CACHE PATH "" FORCE)
buildbot_assert_path_exists("${DLSS_SDK_ROOT}" "DLSS root directory")

# HIP configuration.
set(HIP_ROOT_DIR "${BUILDBOT_HIP_TOOLKITS_ROOT_DIR}/rocm-${HIP_VERSION}" CACHE PATH "" FORCE)
buildbot_assert_path_exists("${HIP_ROOT_DIR}" "HIP root directory")

# OptiX configuration.
if(EXISTS "${BUILDBOT_OPTIX_TOOLKITS_ROOT_DIR}/optix-sdk-${OPTIX_VERSION}")
  # New deployment style.
  set(OPTIX_ROOT_DIR "${BUILDBOT_OPTIX_TOOLKITS_ROOT_DIR}/optix-sdk-${OPTIX_VERSION}"
      CACHE PATH "" FORCE)
else()
  # Old deployment style. Will be phased out after the buildbot re-deploy.
  set(
    OPTIX_ROOT_DIR
    "${BUILDBOT_OPTIX_TOOLKITS_ROOT_DIR}/NVIDIA-OptiX-SDK-${OPTIX_VERSION}-linux64-x86_64"
    CACHE PATH "" FORCE
  )
endif()
buildbot_assert_path_exists("${OPTIX_ROOT_DIR}" "OptiX root directory")
