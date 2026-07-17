# SPDX-FileCopyrightText: 2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later


# -----------------------------------------------------------------------------
# Build `shader_tool` executable.
set(SRC
  ast.cc
  attribute.cc
  enum.cc
  flow_control.cc
  function.cc
  grammar.cc
  grammar_bsl.cc
  intermediate.cc
  metadata.cc
  namespace.cc
  processor.cc
  resource_table.cc
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

add_executable(shader_tool shader_tool.cc ${SRC})
optimize_debug_target(shader_tool)

if(WITH_GTESTS)
  set(TEST_SRC
    tests/shader_tool_testing.hh
    tests/expression_test.cc
    tests/syntax_test.cc
    tests/parser_test.cc
    tests/metadata_test.cc
    tests/templates_test.cc
    tests/flow_control_test.cc
    tests/resource_table_test.cc
    tests/function_test.cc
    tests/namespace_test.cc
    tests/struct_test.cc
  )

  set(TEST_INC
    .
  )

  blender_add_test_suite_executable(
    shader_tool
    "${TEST_SRC};${SRC}"
    "${TEST_INC}"
    ""
    ""
    ""
  )
endif()
