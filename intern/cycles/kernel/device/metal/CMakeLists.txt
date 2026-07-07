# SPDX-FileCopyrightText: 2011-2026 Blender Foundation
#
# SPDX-License-Identifier: Apache-2.0

set(INC
  ../../..
)

set(INC_SYS

)

set(SRC_KERNEL_DEVICE_METAL
  kernel.metal
)

set(SRC_KERNEL_DEVICE_METAL_HEADERS
  bvh.h
  compat.h
  context_begin.h
  context_end.h
  function_constants.h
  globals.h
)


set(LIB

)

if(WITH_CYCLES_DEVICE_METAL)
  add_library(cycles_kernel_metal INTERFACE)
  target_sources(cycles_kernel_metal INTERFACE
    ${SRC_KERNEL_DEVICE_METAL}
    ${SRC_KERNEL_DEVICE_METAL_HEADERS}
  )
  cycles_set_solution_folder(cycles_kernel_metal)

  source_group("device\\metal" FILES ${SRC_KERNEL_DEVICE_METAL} ${SRC_KERNEL_DEVICE_METAL_HEADERS})

  add_dependencies(cycles_kernel cycles_kernel_metal)
endif()

delayed_install(${CMAKE_CURRENT_SOURCE_DIR} "${SRC_KERNEL_DEVICE_METAL}" ${CYCLES_INSTALL_PATH}/source/kernel/device/metal)
delayed_install(${CMAKE_CURRENT_SOURCE_DIR} "${SRC_KERNEL_DEVICE_METAL_HEADERS}" ${CYCLES_INSTALL_PATH}/source/kernel/device/metal)
