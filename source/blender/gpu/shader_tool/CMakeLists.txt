# SPDX-FileCopyrightText: 2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later


# -----------------------------------------------------------------------------
# Build `shader_tool` executable.

set(INC
  .

  bsl
  lexit
)

set(SRC
  ast.cc
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
  string.cc
  struct.cc
  template.cc
  union.cc

  ast.hh
  enums.hh
  expression.hh
  intermediate.hh
  metadata.hh
  pratt_parser.hh
  processor.hh
  scope.hh
  time_it.hh
  token.hh
  token_stream.hh
  utils.hh

  bsl/codegen.cc
  bsl/constructor.cc
  bsl/grammar.cc
  bsl/operator.cc
  bsl/resource.cc
  bsl/symbol.cc
  bsl/symbol_class.cc
  bsl/symbol_function.cc
  bsl/symbol_scope.cc
  bsl/symbol_table.cc
  bsl/symbol_template.cc
  bsl/symbol_variable.cc
  bsl/type_checker.cc

  bsl/diagnostic.hh
  bsl/resource.hh
  bsl/source_location.hh
  bsl/symbol_class.hh
  bsl/symbol_function.hh
  bsl/symbol_scope.hh
  bsl/symbol_table.hh
  bsl/symbol_template.hh
  bsl/symbol_variable.hh
  bsl/type_checker.hh

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

# When cross-compiling the executable is separately compiled for the host architecture
# in a sub-build defined in host_tools.cmake. As this is a build-time code-gen tool
# that must run on the host build machine, the executable is then provided by the
# SHADER_TOOL_EXECUTABLE variable.
if(NOT CMAKE_CROSSCOMPILING)
  add_executable(shader_tool shader_tool.cc ${SRC})
  blender_target_include_dirs(shader_tool ${INC})

  optimize_debug_target(shader_tool)
endif()

if(WITH_GTESTS)
  set(TEST_SRC
    tests/expression_test.cc
    tests/flow_control_test.cc
    tests/function_test.cc
    tests/grammar_test.cc
    tests/metadata_test.cc
    tests/namespace_test.cc
    tests/parser_test.cc
    tests/resource_table_test.cc
    tests/shader_tool_testing.hh
    tests/struct_test.cc
    tests/syntax_test.cc
    tests/templates_test.cc
  )

  blender_add_test_suite_executable(
    shader_tool
    "${TEST_SRC}"
    "${INC}"
    ""
    ""
    "${SRC}"
  )
endif()
