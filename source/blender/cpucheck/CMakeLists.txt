# SPDX-FileCopyrightText: 2024 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

# The cpu check module cannot build with sse42 since it will be executed
# by cpus that may not support these instructions

if(COMPILER_SSE42_FLAG)
  remove_cc_flag("${COMPILER_SSE42_FLAG}")
endif()
add_library(blender_cpu_check SHARED cpu_check.cc)

target_link_libraries(blender_cpu_check PRIVATE ${PLATFORM_LINKLIBS})
target_compile_definitions(blender_cpu_check PUBLIC WITH_CPU_CHECK)

if(NOT WIN32)
  set(_LIB_SUB_FOLDER "lib/")
endif()

set_target_properties(blender_cpu_check
  PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/${_LIB_SUB_FOLDER}"
  LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/${_LIB_SUB_FOLDER}"
)

unset(_LIB_SUB_FOLDER)
