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
add_executable(shader_tool ${SRC})
optimize_debug_target(shader_tool)
