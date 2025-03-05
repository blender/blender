# SPDX-FileCopyrightText: 2019-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

# Check against installed versions.

message(STATUS "Found C Compiler: ${CMAKE_C_COMPILER_ID} ${CMAKE_C_COMPILER_VERSION}")
if(UNIX AND NOT APPLE)
  if(NOT CMAKE_COMPILER_IS_GNUCC OR NOT (CMAKE_C_COMPILER_VERSION MATCHES "${RELEASE_GCC_VERSION}.*"))
    message(STATUS "  NOTE: Official releases uses GCC ${RELEASE_GCC_VERSION}")
  endif()
endif()

message(STATUS "Found C++ Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
if(UNIX AND NOT APPLE)
  if(NOT CMAKE_COMPILER_IS_GNUCC OR NOT (CMAKE_CXX_COMPILER_VERSION MATCHES "${RELEASE_GCC_VERSION}.*"))
    message(STATUS "  NOTE: Official releases uses GCC ${RELEASE_GCC_VERSION}")
  endif()
endif()

if(NOT APPLE)
  find_package(CUDAToolkit)
  if(NOT CUDAToolkit_NVCC_EXECUTABLE)
    message(STATUS "Missing CUDA compiler")
  else()
    message(STATUS "Found CUDA Compiler: ${CUDAToolkit_NVCC_EXECUTABLE} ${CUDAToolkit_VERSION}")
    if(NOT CUDAToolkit_VERSION MATCHES "${RELEASE_CUDA_VERSION}.*")
      message(STATUS "  NOTE: Official releases uses CUDA ${RELEASE_CUDA_VERSION}")
    endif()
  endif()

  unset(HIP_VERSION)
  find_package(HIP QUIET)
  if(NOT HIP_FOUND)
    message(STATUS "Missing HIP compiler")
  else()
    message(STATUS "Found HIP Compiler: ${HIP_HIPCC_EXECUTABLE} ${HIP_VERSION}")
    if(NOT HIP_VERSION MATCHES "${RELEASE_HIP_VERSION}.*")
      message(STATUS "  NOTE: Official releases uses HIP ${RELEASE_HIP_VERSION}")
    endif()
  endif()
endif()
