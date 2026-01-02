# SPDX-FileCopyrightText: 2011-2026 Blender Foundation
#
# SPDX-License-Identifier: Apache-2.0

set(INC
  ../../..
)

set(INC_SYS

)

set(SRC_KERNEL_DEVICE_HIPRT
  kernel.cpp
)

set(SRC_KERNEL_DEVICE_HIPRT_HEADERS
  bvh.h
  common.h
  globals.h
)

set(LIB

)

if(WITH_CYCLES_HIP_BINARIES AND WITH_CYCLES_DEVICE_HIPRT)
  set(HIPRT_COMPILER_PARALLEL_JOBS 1 CACHE STRING "Number of parallel compiler instances to use for HIP-RT kernels")
  mark_as_advanced(HIPRT_COMPILER_PARALLEL_JOBS)

  if(WIN32)
    set(hiprt_compile_command ${CMAKE_COMMAND})
    set(hiprt_compile_flags
      -E env "HIP_PATH=${HIP_ROOT_DIR}"
      ${HIP_HIPCC_EXECUTABLE})
  else()
    set(hiprt_compile_command ${HIP_HIPCC_EXECUTABLE})
    set(hiprt_compile_flags)
  endif()

  if(WITH_NANOVDB)
    set(hiprt_compile_flags ${hiprt_compile_flags} -D WITH_NANOVDB)
  endif()

  if(WITH_CYCLES_DEBUG)
    set(hiprt_compile_flags ${hiprt_compile_flags} -D WITH_CYCLES_DEBUG)
  endif()

  if(WIN32 AND (${HIP_VERSION} STREQUAL "6.1.40252"))
    message(WARNING "HIP SDK ${HIP_VERSION} has known rendering artifacts with HIPRT. 5.7 is recommended instead")
  endif()

  set(hiprt_sources
    kernel.cpp
    ${SRC_KERNEL_DEVICE_HIPRT_HEADERS}
    $<TARGET_PROPERTY:cycles_kernel,INTERFACE_SOURCES>
  )

  set(kernel_sources ${hiprt_sources})
  set(hiprt_kernel_src "/kernel.cpp")
  set(hiprt_compile_flags
    ${hiprt_compile_flags}
    ${HIP_HIPCC_FLAGS}
    ${CMAKE_CURRENT_SOURCE_DIR}${hiprt_kernel_src}
    -D CCL_NAMESPACE_BEGIN=
    -D CCL_NAMESPACE_END=
    -D HIPCC
    -D __HIPRT__
    -std=c++17
    -mllvm
    -amdgpu-early-inline-all=false
    -mllvm
    -amdgpu-function-calls=true
    -parallel-jobs=${HIPRT_COMPILER_PARALLEL_JOBS}
    --genco
    -I ${CMAKE_CURRENT_SOURCE_DIR}/../../..
    -I ${HIPRT_INCLUDE_DIR}
    -Wno-parentheses-equality
    -Wno-unused-value
    -ffast-math
  )

  set(hiprt_hipfb)
  foreach(arch ${CYCLES_HIP_BINARIES_ARCH})
    set(hiprt_file ${CMAKE_CURRENT_BINARY_DIR}/kernel_rt_${arch}.hipfb)
    set(hiprt_file_compressed ${hiprt_file}.zst)
    set(hiprt_flags
    ${hiprt_compile_flags}
    --offload-arch=${arch}
    -o ${hiprt_file})

    add_custom_command(
      OUTPUT  ${hiprt_file}
      COMMAND ${hiprt_compile_command} ${hiprt_flags}
      DEPENDS ${kernel_sources}
    )
    add_custom_command(
      OUTPUT ${hiprt_file_compressed}
      COMMAND "$<TARGET_FILE:zstd_compress>" ${hiprt_file} ${hiprt_file_compressed}
      DEPENDS ${hiprt_file}
    )
    delayed_install("${CMAKE_CURRENT_BINARY_DIR}" "${hiprt_file_compressed}" ${CYCLES_INSTALL_PATH}/lib)
    list(APPEND hiprt_hipfb ${hiprt_file_compressed})
  endforeach()

  add_custom_target(cycles_kernel_hiprt
    ALL
    DEPENDS ${hiprt_hipfb}
    SOURCES ${SRC_KERNEL_DEVICE_HIPRT} ${SRC_KERNEL_DEVICE_HIPRT_HEADERS}
  )
  cycles_set_solution_folder(cycles_kernel_hiprt)

  source_group("device\\hiprt" FILES ${SRC_KERNEL_DEVICE_HIPRT} ${SRC_KERNEL_DEVICE_HIPRT_HEADERS})

  add_dependencies(cycles_kernel cycles_kernel_hip)
endif()

delayed_install(${CMAKE_CURRENT_SOURCE_DIR} "${SRC_KERNEL_DEVICE_HIPRT}" ${CYCLES_INSTALL_PATH}/source/kernel/device/hiprt)
delayed_install(${CMAKE_CURRENT_SOURCE_DIR} "${SRC_KERNEL_DEVICE_HIPRT_HEADERS}" ${CYCLES_INSTALL_PATH}/source/kernel/device/hiprt)
