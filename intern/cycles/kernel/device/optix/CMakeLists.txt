# SPDX-FileCopyrightText: 2011-2026 Blender Foundation
#
# SPDX-License-Identifier: Apache-2.0

set(INC
  ../../..
)

set(INC_SYS

)

set(SRC_KERNEL_DEVICE_OPTIX
  kernel.cu
  kernel_shader_raytrace.cu
)

if(WITH_CYCLES_OSL)
  math(EXPR OSL_LIBRARY_VERSION_CODE "${OSL_VERSION_MAJOR} * 10000 + ${OSL_VERSION_MINOR} * 100 + ${OSL_VERSION_PATCH}")

  set(SRC_KERNEL_DEVICE_OPTIX
    ${SRC_KERNEL_DEVICE_OPTIX}
    ../../osl/services_optix.cu
    kernel_osl.cu
    kernel_osl_camera.cu
  )
endif()

set(SRC_KERNEL_DEVICE_OPTIX_HEADERS
  bvh.h
  compat.h
  globals.h
)

set(LIB

)

if(WITH_CYCLES_CUDA_BINARIES AND WITH_CYCLES_DEVICE_OPTIX)
  # CUDA version
  cuda_get_version(CUDA_VERSION)

  macro(cycles_optix_kernel_add name input flags)
    set(output "${CMAKE_CURRENT_BINARY_DIR}/${name}.ptx")
    set(output_compressed "${output}.zst")

    set(cuda_flags ${flags}
      -I "${OPTIX_INCLUDE_DIR}"
      -I "${CMAKE_CURRENT_SOURCE_DIR}/../../.."
      -o ${output})

    if(WITH_CYCLES_OSL)
      set(cuda_flags ${cuda_flags}
        -D OSL_LIBRARY_VERSION_CODE=${OSL_LIBRARY_VERSION_CODE})
    endif()

    set(arch compute_50)
    set(cuda_nvcc_executable ${CUDA_NVCC_EXECUTABLE})
    set(cuda_version ${CUDA_VERSION})
    if("${CUDA_VERSION}" GREATER_EQUAL 130) # Support for Maxwell, Pascal and Volta was dropped in CUDA 13
      if(DEFINED CUDA11_NVCC_EXECUTABLE)
        # Use CUDA 11 for the OptiX PTX kernel, to retain support for older architectures.
        set(cuda_nvcc_executable ${CUDA11_NVCC_EXECUTABLE})
        set(cuda_version 110)
      else()
        set(arch compute_75)
      endif()
    endif()

    cuda_add_common_flags(${cuda_version} ${arch} "${cuda_flags}" cuda_flags)

    add_custom_command(
      OUTPUT
        ${output}
      DEPENDS
        ${input}
        ${SRC_KERNEL_DEVICE_OPTIX_HEADERS}
        $<TARGET_PROPERTY:cycles_kernel,INTERFACE_SOURCES>
      COMMAND
        ${cuda_nvcc_executable}
        --ptx
        -arch=${arch}
        ${cuda_flags}
        ${input}
      WORKING_DIRECTORY
        "${CMAKE_CURRENT_SOURCE_DIR}"
    )

    unset(cuda_nvcc_executable)

    add_custom_command(
      OUTPUT ${output_compressed}
      COMMAND "$<TARGET_FILE:zstd_compress>" ${output} ${output_compressed}
      DEPENDS ${output})

    list(APPEND optix_ptx ${output_compressed})

    delayed_install("${CMAKE_CURRENT_BINARY_DIR}" "${output_compressed}" ${CYCLES_INSTALL_PATH}/lib)
  endmacro()

  cycles_optix_kernel_add(
    kernel_optix
    "kernel.cu"
    "")
  cycles_optix_kernel_add(
    kernel_optix_shader_raytrace
    "kernel_shader_raytrace.cu"
    "--keep-device-functions")
  if(WITH_CYCLES_OSL)
    cycles_optix_kernel_add(
      kernel_optix_osl
      "kernel_osl.cu"
      "--relocatable-device-code=true")
    cycles_optix_kernel_add(
      kernel_optix_osl_camera
      "kernel_osl_camera.cu"
      "--relocatable-device-code=true")
    cycles_optix_kernel_add(
      kernel_optix_osl_services
      "../../osl/services_optix.cu"
      "--relocatable-device-code=true")
  endif()

  add_custom_target(cycles_kernel_optix
    ALL
    DEPENDS ${optix_ptx}
    SOURCES ${SRC_KERNEL_DEVICE_OPTIX} ${SRC_KERNEL_DEVICE_OPTIX_HEADERS}
  )
  cycles_set_solution_folder(cycles_kernel_optix)

  source_group("device\\optix" FILES ${SRC_KERNEL_DEVICE_OPTIX} ${SRC_KERNEL_DEVICE_OPTIX_HEADERS})

  add_dependencies(cycles_kernel cycles_kernel_optix)
endif()

delayed_install(${CMAKE_CURRENT_SOURCE_DIR} "${SRC_KERNEL_DEVICE_OPTIX}" ${CYCLES_INSTALL_PATH}/source/kernel/device/optix)
delayed_install(${CMAKE_CURRENT_SOURCE_DIR} "${SRC_KERNEL_DEVICE_OPTIX_HEADERS}" ${CYCLES_INSTALL_PATH}/source/kernel/device/optix)
