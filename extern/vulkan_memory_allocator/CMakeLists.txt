# SPDX-FileCopyrightText: 2022 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  PUBLIC .
)

set(INC_SYS
  ${VULKAN_INCLUDE_DIRS}
)

set(SRC
  vk_mem_alloc_impl.cc

  vk_mem_alloc.h
)

set(LIB
)

blender_add_lib(extern_vulkan_memory_allocator "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
add_library(bf::extern::vulkan_memory_allocator ALIAS extern_vulkan_memory_allocator)

if((CMAKE_C_COMPILER_ID STREQUAL "GNU") OR (CMAKE_C_COMPILER_ID MATCHES "Clang"))
  target_compile_options(extern_vulkan_memory_allocator
    PRIVATE "-Wno-nullability-completeness"
  )
endif()
