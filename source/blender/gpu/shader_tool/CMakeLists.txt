# SPDX-FileCopyrightText: 2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later


# -----------------------------------------------------------------------------
# Build `shader_tool` executable.
set(SRC
  attribute.cc
  enum.cc
  flow_control.cc
  function.cc
  grammar.cc
  intermediate.cc
  metadata.cc
  namespace.cc
  processor.cc
  resource_table.cc
  shader_tool.cc
  string.cc
  struct.cc
  template.cc
  union.cc

  enums.hh
  expression.hh
  intermediate.hh
  metadata.hh
  processor.hh
  scope.hh
  time_it.hh
  token.hh
  token_stream.hh
  utils.hh

  lexit/lexit.cc

  lexit/identifier.hh
  lexit/lexit.hh
  lexit/simd.hh
  lexit/tables.hh
  lexit/types.hh
  lexit/vector.hh
)

if(WITH_GPU_SHADER_ASSERT)
  add_definitions(-DWITH_GPU_SHADER_ASSERT)
endif()

# `SRC_DNA_INC` is defined in the parent directory.

# When cross-compiling the executable is separately compiled for the host architecture
# in a sub-build defined in host_tools.cmake. As this is a build-time code-gen tool
# that must run on the host build machine, the executable is then provided by the
# SHADER_TOOL_EXECUTABLE variable.
if(NOT CMAKE_CROSSCOMPILING)
  add_executable(shader_tool ${SRC})
  optimize_debug_target(shader_tool)
endif()
