# Copyright 2011-2020 Blender Foundation
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

set(INC
  ..
  ../../glew-mx
)

set(INC_SYS
  ${GLEW_INCLUDE_DIR}
  ../../../extern/clew/include
)

if(WITH_CYCLES_DEVICE_OPTIX OR WITH_CYCLES_DEVICE_CUDA)
  if(WITH_CUDA_DYNLOAD)
    list(APPEND INC
      ../../../extern/cuew/include
    )
    add_definitions(-DWITH_CUDA_DYNLOAD)
  else()
    list(APPEND INC_SYS
      ${CUDA_TOOLKIT_INCLUDE}
    )
    add_definitions(-DCYCLES_CUDA_NVCC_EXECUTABLE="${CUDA_NVCC_EXECUTABLE}")
  endif()
endif()

if(WITH_CYCLES_DEVICE_HIP AND WITH_HIP_DYNLOAD)
  list(APPEND INC
    ../../../extern/hipew/include
  )
  add_definitions(-DWITH_HIP_DYNLOAD)
endif()

set(SRC
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

set(SRC_DUMMY
  dummy/device.cpp
  dummy/device.h
)

set(SRC_MULTI
  multi/device.cpp
  multi/device.h
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

set(LIB
  cycles_kernel
  cycles_util
  ${CYCLES_GL_LIBRARIES}
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

add_definitions(${GL_DEFINITIONS})

if(WITH_CYCLES_DEVICE_CUDA)
  add_definitions(-DWITH_CUDA)
endif()
if(WITH_CYCLES_DEVICE_HIP)
  add_definitions(-DWITH_HIP)
endif()
if(WITH_CYCLES_DEVICE_OPTIX)
  add_definitions(-DWITH_OPTIX)
endif()

if(WITH_OPENIMAGEDENOISE)
  list(APPEND LIB
    ${OPENIMAGEDENOISE_LIBRARIES}
  )
endif()

include_directories(${INC})
include_directories(SYSTEM ${INC_SYS})

cycles_add_library(cycles_device "${LIB}"
  ${SRC}
  ${SRC_CPU}
  ${SRC_CUDA}
  ${SRC_HIP}
  ${SRC_DUMMY}
  ${SRC_MULTI}
  ${SRC_OPTIX}
  ${SRC_HEADERS}
)

source_group("cpu" FILES ${SRC_CPU})
source_group("cuda" FILES ${SRC_CUDA})
source_group("dummy" FILES ${SRC_DUMMY})
source_group("multi" FILES ${SRC_MULTI})
source_group("optix" FILES ${SRC_OPTIX})
source_group("common" FILES ${SRC} ${SRC_HEADERS})
