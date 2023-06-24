# SPDX-FileCopyrightText: 2006 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

add_library(bf_intern_mikktspace INTERFACE)
target_include_directories(bf_intern_mikktspace INTERFACE .)

if(WITH_TBB)
  target_compile_definitions(bf_intern_mikktspace INTERFACE -DWITH_TBB)
  target_include_directories(bf_intern_mikktspace INTERFACE ${TBB_INCLUDE_DIRS})
  target_link_libraries(bf_intern_mikktspace INTERFACE ${TBB_LIBRARIES})
endif()

# CMake 3.19+ allows one to populate the interface library with
# source files to show in the IDE.
if(${CMAKE_VERSION} VERSION_GREATER_EQUAL "3.19")
  set(SRC
    mikk_atomic_hash_set.hh
    mikk_float3.hh
    mikk_util.hh
    mikktspace.hh
  )
  target_sources(bf_intern_mikktspace PRIVATE ${SRC})
  blender_source_group(bf_intern_mikktspace ${SRC})
endif()
