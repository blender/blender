
set(INC
  ..
  ../../glew-mx
)

set(INC_SYS
  ${GLEW_INCLUDE_DIR}
  ../../../extern/clew/include
)

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

set(SRC
  device.cpp
  device_cpu.cpp
  device_cuda.cpp
  device_denoising.cpp
  device_memory.cpp
  device_multi.cpp
  device_opencl.cpp
  device_optix.cpp
  device_split_kernel.cpp
  device_task.cpp
)

set(SRC_CUDA
  cuda/device_cuda.h
  cuda/device_cuda_impl.cpp
)

set(SRC_OPENCL
  opencl/device_opencl.h
  opencl/device_opencl_impl.cpp
  opencl/memory_manager.h
  opencl/memory_manager.cpp
  opencl/opencl_util.cpp
)

if(WITH_CYCLES_NETWORK)
  list(APPEND SRC
    device_network.cpp
  )
endif()

set(SRC_HEADERS
  device.h
  device_denoising.h
  device_memory.h
  device_intern.h
  device_network.h
  device_split_kernel.h
  device_task.h
)

set(LIB
  cycles_render
  cycles_kernel
  cycles_util
)

if(WITH_CUDA_DYNLOAD)
  list(APPEND LIB
    extern_cuew
  )
else()
  list(APPEND LIB
    ${CUDA_CUDA_LIBRARY}
  )
endif()

add_definitions(${GL_DEFINITIONS})
if(WITH_CYCLES_NETWORK)
  add_definitions(-DWITH_NETWORK)
endif()
if(WITH_CYCLES_DEVICE_OPENCL)
  list(APPEND LIB
    extern_clew
  )
  add_definitions(-DWITH_OPENCL)
endif()
if(WITH_CYCLES_DEVICE_CUDA)
  add_definitions(-DWITH_CUDA)
endif()
if(WITH_CYCLES_DEVICE_OPTIX)
  add_definitions(-DWITH_OPTIX)
endif()
if(WITH_CYCLES_DEVICE_MULTI)
  add_definitions(-DWITH_MULTI)
endif()

if(WITH_OPENIMAGEDENOISE)
  add_definitions(-DWITH_OPENIMAGEDENOISE)
  add_definitions(-DOIDN_STATIC_LIB)
  list(APPEND INC_SYS
    ${OPENIMAGEDENOISE_INCLUDE_DIRS}
  )
  list(APPEND LIB
    ${OPENIMAGEDENOISE_LIBRARIES}
    ${TBB_LIBRARIES}
  )
endif()

include_directories(${INC})
include_directories(SYSTEM ${INC_SYS})

cycles_add_library(cycles_device "${LIB}" ${SRC} ${SRC_CUDA} ${SRC_OPENCL} ${SRC_HEADERS})
