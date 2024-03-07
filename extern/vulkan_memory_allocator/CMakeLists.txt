# SPDX-FileCopyrightText: 2022 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  .
)

set(INC_SYS
  ${VULKAN_INCLUDE_DIRS}
)

if(APPLE)
  list(APPEND INC_SYS
    ${MOLTENVK_INCLUDE_DIRS}
  )
endif()

set(SRC
  vk_mem_alloc_impl.cc

  vk_mem_alloc.h
)

set(LIB
)

blender_add_lib(extern_vulkan_memory_allocator "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")

if(CMAKE_COMPILER_IS_GNUCC OR CMAKE_C_COMPILER_ID MATCHES "Clang")
  target_compile_options(extern_vulkan_memory_allocator
    PRIVATE "-Wno-nullability-completeness"
  )
endif()
