# SPDX-License-Identifier: Apache-2.0
# Copyright 2011-2022 Blender Foundation

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
  cpu/kernel_thread_globals.cpp
  cpu/kernel_thread_globals.h
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

set(SRC_ONEAPI
  oneapi/device_impl.cpp
  oneapi/device_impl.h
  oneapi/device.cpp
  oneapi/device.h
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
  list(APPEND LIB ${cycles_kernel_oneapi_lib})
  if(WIN32)
    list(APPEND LIB debug ${SYCL_LIBRARY_DEBUG} optimized ${SYCL_LIBRARY})
  else()
    list(APPEND LIB ${SYCL_LIBRARY})
  endif()
  list(APPEND SRC
    ${SRC_ONEAPI}
  )
  list(APPEND INC_SYS
    ${SYCL_INCLUDE_DIR}
  )
endif()

if(WITH_OPENIMAGEDENOISE)
  list(APPEND LIB
    ${OPENIMAGEDENOISE_LIBRARIES}
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
source_group("multi" FILES ${SRC_MULTI})
source_group("metal" FILES ${SRC_METAL})
source_group("optix" FILES ${SRC_OPTIX})
source_group("oneapi" FILES ${SRC_ONEAPI})
source_group("common" FILES ${SRC_BASE} ${SRC_HEADERS})
