# SPDX-FileCopyrightText: 2011-2026 Blender Foundation
#
# SPDX-License-Identifier: Apache-2.0

set(INC
  ../../..
)

set(INC_SYS

)

set(SRC_KERNEL_DEVICE_HIP
  kernel.cpp
)

set(SRC_KERNEL_DEVICE_HIP_HEADERS
  compat.h
  config.h
  globals.h
)

set(LIB

)

if(WITH_CYCLES_HIP_BINARIES AND WITH_CYCLES_DEVICE_HIP)
  # build for each arch
  set(hip_sources
    kernel.cpp
    ${SRC_KERNEL_DEVICE_HIP_HEADERS}
    $<TARGET_PROPERTY:cycles_kernel,INTERFACE_SOURCES>
  )
  set(hip_fatbins "")

  macro(CYCLES_HIP_KERNEL_ADD arch name flags sources experimental)
    set(format "fatbin")
    set(hip_file ${name}_${arch}.${format})
    set(hip_file_compressed ${hip_file}.zst)
    set(kernel_sources ${sources})

    set(hip_kernel_src "/${name}.cpp")

    if(WIN32)
      set(hip_command ${CMAKE_COMMAND})
      set(hip_flags
        -E env "HIP_PATH=${HIP_ROOT_DIR}"
        ${HIP_HIPCC_EXECUTABLE})
    else()
      set(hip_command ${HIP_HIPCC_EXECUTABLE})
      set(hip_flags "")
    endif()

    # There's a few bugs in the HIP compiler causing some scenes to fail to render,
    # or render improperly with specific combinations of device and/or compiler.
    # - Vega (gfx90x) fails to render some scenes
    # - Other GPUs (E.g. RDNA3) render volumes improperly with HIP 6.1.40252
    # A workaround is to set -O1 opt level during kernel compilation for these
    # configurations.
    if(WIN32 AND ((${arch} MATCHES "gfx90[a-z0-9]+") OR (${HIP_VERSION} STREQUAL "6.1.40252")))
      set(hip_opt_flags "-O1")
    else()
      set(hip_opt_flags "")
    endif()

    set(hip_flags
      ${hip_flags}
      --offload-arch=${arch}
      ${HIP_HIPCC_FLAGS}
      --genco
      ${CMAKE_CURRENT_SOURCE_DIR}${hip_kernel_src}
      ${flags}
      -D CCL_NAMESPACE_BEGIN=
      -D CCL_NAMESPACE_END=
      -D HIPCC
      -I ${CMAKE_CURRENT_SOURCE_DIR}/../../..
      -Wno-parentheses-equality
      -Wno-unused-value
      -ffast-math
      -std=c++17
      ${hip_opt_flags}
      -o ${CMAKE_CURRENT_BINARY_DIR}/${hip_file})

    if(WITH_NANOVDB)
      set(hip_flags ${hip_flags}
        -D WITH_NANOVDB)
    endif()

    if(WITH_CYCLES_DEBUG)
      set(hip_flags ${hip_flags} -D WITH_CYCLES_DEBUG)
    endif()

    add_custom_command(
      OUTPUT ${hip_file}
      COMMAND ${hip_command} ${hip_flags}
      DEPENDS ${kernel_sources})
    add_custom_command(
      OUTPUT ${hip_file_compressed}
      COMMAND "$<TARGET_FILE:zstd_compress>" ${hip_file} ${hip_file_compressed}
      DEPENDS ${hip_file})
    delayed_install("${CMAKE_CURRENT_BINARY_DIR}" "${hip_file_compressed}" ${CYCLES_INSTALL_PATH}/lib)
    list(APPEND hip_fatbins ${hip_file_compressed})
  endmacro()

  foreach(arch ${CYCLES_HIP_BINARIES_ARCH})
    # Compile regular kernel
    cycles_hip_kernel_add(${arch} kernel "" "${hip_sources}" FALSE)
  endforeach()

  add_custom_target(cycles_kernel_hip
    ALL
    DEPENDS ${hip_fatbins}
    SOURCES ${SRC_KERNEL_DEVICE_HIP} ${SRC_KERNEL_DEVICE_HIP_HEADERS}
  )
  cycles_set_solution_folder(cycles_kernel_hip)

  source_group("device\\hip" FILES ${SRC_KERNEL_DEVICE_HIP} ${SRC_KERNEL_DEVICE_HIP_HEADERS})

  add_dependencies(cycles_kernel cycles_kernel_hip)
endif()

delayed_install(${CMAKE_CURRENT_SOURCE_DIR} "${SRC_KERNEL_DEVICE_HIP}" ${CYCLES_INSTALL_PATH}/source/kernel/device/hip)
delayed_install(${CMAKE_CURRENT_SOURCE_DIR} "${SRC_KERNEL_DEVICE_HIP_HEADERS}" ${CYCLES_INSTALL_PATH}/source/kernel/device/hip)
