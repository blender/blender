# SPDX-FileCopyrightText: 2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later


# -----------------------------------------------------------------------------
# Build `shader_tool` executable.
set(SRC
  intermediate.cc
  metadata.cc
  processor.cc
  shader_tool.cc

  intermediate.hh
  processor.hh
  scope.hh
  token_stream.hh
  token.hh
)

if(WITH_GPU_SHADER_ASSERT)
  add_definitions(-DWITH_GPU_SHADER_ASSERT)
endif()

# `SRC_DNA_INC` is defined in the parent directory.
add_executable(shader_tool ${SRC})
optimize_debug_target(shader_tool)
