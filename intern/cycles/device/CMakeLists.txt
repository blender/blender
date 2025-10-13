# SPDX-FileCopyrightText: 2011-2022 Blender Foundation
#
# SPDX-License-Identifier: Apache-2.0

set(INC
  ..
)

set(INC_SYS )

if(WITH_CYCLES_DEVICE_OPTIX OR WITH_CYCLES_DEVICE_CUDA)
  if(NOT WITH_CUDA_DYNLOAD)
    add_definitions(-DCYCLES_CUDA_NVCC_EXECUTABLE="${CUDA_NVCC_EXECUTABLE}")
  endif()

  add_definitions(-DCYCLES_RUNTIME_OPTIX_ROOT_DIR="${CYCLES_RUNTIME_OPTIX_ROOT_DIR}")
endif()

set(SRC_BASE
  device.cpp
  denoise.cpp
  graphics_interop.cpp
  kernel.cpp
  memory.cpp
  queue.cpp
)

set(SRC_CPU
  cpu/device.cpp
  cpu/device.h
  cpu/device_impl.cpp
  cpu/device_impl.h
  cpu/kernel.cpp
  cpu/kernel.h
  cpu/kernel_function.h
)

set(SRC_CUDA
  cuda/device.cpp
  cuda/device.h
  cuda/device_impl.cpp
  cuda/device_impl.h
  cuda/graphics_interop.cpp
  cuda/graphics_interop.h
  cuda/kernel.cpp
  cuda/kernel.h
  cuda/queue.cpp
  cuda/queue.h
  cuda/util.cpp
  cuda/util.h
)

set(SRC_HIP
  hip/device.cpp
  hip/device.h
  hip/device_impl.cpp
  hip/device_impl.h
  hip/graphics_interop.cpp
  hip/graphics_interop.h
  hip/kernel.cpp
  hip/kernel.h
  hip/queue.cpp
  hip/queue.h
  hip/util.cpp
  hip/util.h
)

set(SRC_HIPRT
  hiprt/device_impl.cpp
  hiprt/device_impl.h
  hiprt/queue.cpp
  hiprt/queue.h
)

set(SRC_ONEAPI
  oneapi/device_impl.cpp
  oneapi/device_impl.h
  oneapi/device.cpp
  oneapi/device.h
  oneapi/graphics_interop.cpp
  oneapi/graphics_interop.h
  oneapi/queue.cpp
  oneapi/queue.h
)

set(SRC_DUMMY
  dummy/device.cpp
  dummy/device.h
)

set(SRC_MULTI
  multi/device.cpp
  multi/device.h
)

set(SRC_METAL
  metal/bvh.mm
  metal/bvh.h
  metal/device.mm
  metal/device.h
  metal/device_impl.mm
  metal/device_impl.h
  metal/graphics_interop.mm
  metal/graphics_interop.h
  metal/kernel.mm
  metal/kernel.h
  metal/queue.mm
  metal/queue.h
  metal/util.mm
  metal/util.h
)

set(SRC_OPTIX
  optix/device.cpp
  optix/device.h
  optix/device_impl.cpp
  optix/device_impl.h
  optix/queue.cpp
  optix/queue.h
  optix/util.h
)

set(SRC_HEADERS
  device.h
  denoise.h
  graphics_interop.h
  memory.h
  kernel.h
  queue.h
)

set(SRC
  ${SRC_BASE}
  ${SRC_CPU}
  ${SRC_CUDA}
  ${SRC_HIP}
  ${SRC_HIPRT}
  ${SRC_DUMMY}
  ${SRC_MULTI}
  ${SRC_OPTIX}
  ${SRC_HEADERS}
)

set(LIB
  cycles_kernel
  cycles_util
)

if(WITH_CYCLES_DEVICE_OPTIX OR WITH_CYCLES_DEVICE_CUDA)
  if(WITH_CUDA_DYNLOAD)
    list(APPEND LIB
      extern_cuew
    )
  else()
    list(APPEND LIB
      ${CUDA_CUDA_LIBRARY}
    )
  endif()
endif()

if(WITH_CYCLES_DEVICE_HIP AND WITH_HIP_DYNLOAD)
  list(APPEND LIB
    extern_hipew
  )
endif()

if(WITH_CYCLES_DEVICE_METAL)
  list(APPEND LIB
    ${METAL_LIBRARY}
  )
  list(APPEND SRC
    ${SRC_METAL}
  )
endif()

if(WITH_CYCLES_DEVICE_ONEAPI)
  if(WITH_CYCLES_ONEAPI_HOST_TASK_EXECUTION)
    add_definitions(-DWITH_ONEAPI_SYCL_HOST_TASK)
  endif()
  if(WITH_CYCLES_ONEAPI_BINARIES)
    set(cycles_kernel_oneapi_lib_suffix "_aot")
  else()
    set(cycles_kernel_oneapi_lib_suffix "_jit")
  endif()
  if(WIN32)
    set(cycles_kernel_oneapi_lib ${CMAKE_CURRENT_BINARY_DIR}/../kernel/cycles_kernel_oneapi${cycles_kernel_oneapi_lib_suffix}.lib)
  else()
    set(cycles_kernel_oneapi_lib ${CMAKE_CURRENT_BINARY_DIR}/../kernel/libcycles_kernel_oneapi${cycles_kernel_oneapi_lib_suffix}.so)
  endif()
  list(APPEND LIB
    ${cycles_kernel_oneapi_lib}
    ${SYCL_LIBRARIES}
  )
  list(APPEND SRC
    ${SRC_ONEAPI}
  )
  list(APPEND INC_SYS
    ${SYCL_INCLUDE_DIR}
  )

  # Test for the presence of sycl::ext::oneapi::experimental::unmap_external_linear_memory (necessary for interop).
  # https://github.com/intel/llvm/blob/sycl/sycl/doc/extensions/experimental/sycl_ext_oneapi_bindless_images.asciidoc
  include(CheckCXXSourceCompiles)
  set(CMAKE_REQUIRED_INCLUDES "${SYCL_INCLUDE_DIR}")
  set(CMAKE_REQUIRED_LIBRARIES "${SYCL_LIBRARIES}")
  check_cxx_source_compiles("
      #include <sycl/ext/oneapi/bindless_images.hpp>
      int main()
      {
          sycl::queue sycl_queue{sycl::gpu_selector_v};
          sycl::ext::oneapi::experimental::unmap_external_linear_memory(nullptr, sycl_queue);
      }
  " SYCL_UNMAP_EXTERNAL_LINEAR_MEMORY_SUPPORTED)
  if (SYCL_UNMAP_EXTERNAL_LINEAR_MEMORY_SUPPORTED)
    foreach(FILE ${SRC_ONEAPI})
      set_source_files_properties(
          ${FILE} PROPERTIES COMPILE_DEFINITIONS "SYCL_LINEAR_MEMORY_INTEROP_AVAILABLE"
      )
    endforeach()
  else()
    message(WARNING
      "The installed SYCL version does not support unmap_external_linear_memory. "
      "Upgrade to oneAPI >= 2025.3 or DPC++ >= 6.2 to support Vulkan-oneAPI interoperability.")
  endif()
  unset(CMAKE_REQUIRED_INCLUDES)
  unset(CMAKE_REQUIRED_LIBRARIES)
endif()

if(WITH_OPENIMAGEDENOISE)
  list(APPEND LIB
    ${OPENIMAGEDENOISE_LIBRARIES}
  )
endif()

if(WITH_CYCLES_OSL)
  list(APPEND LIB
    ${OSL_LIBRARIES}
  )
endif()


include_directories(${INC})
include_directories(SYSTEM ${INC_SYS})

cycles_add_library(cycles_device "${LIB}" ${SRC})

if(WITH_CYCLES_DEVICE_ONEAPI)
  # Need to have proper rebuilding in case of changes
  # in cycles_kernel_oneapi due external project behavior.
  add_dependencies(cycles_device cycles_kernel_oneapi)
endif()

source_group("cpu" FILES ${SRC_CPU})
source_group("cuda" FILES ${SRC_CUDA})
source_group("dummy" FILES ${SRC_DUMMY})
source_group("hip" FILES ${SRC_HIP})
source_group("hiprt" FILES ${SRC_HIPRT})
source_group("multi" FILES ${SRC_MULTI})
source_group("metal" FILES ${SRC_METAL})
source_group("optix" FILES ${SRC_OPTIX})
source_group("oneapi" FILES ${SRC_ONEAPI})
source_group("common" FILES ${SRC_BASE} ${SRC_HEADERS})
