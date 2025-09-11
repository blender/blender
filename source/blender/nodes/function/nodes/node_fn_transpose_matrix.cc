/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_math_matrix.hh"

#include "node_function_util.hh"

namespace blender::nodes::node_fn_transpose_matrix_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.use_custom_socket_order();
  b.allow_any_socket_order();
  b.is_function_node();
  b.add_input<decl::Matrix>("Matrix");
  b.add_output<decl::Matrix>("Matrix").align_with_previous();
}

static void node_build_multi_function(NodeMultiFunctionBuilder &builder)
{
  static auto fn = mf::build::SI1_SO<float4x4, float4x4>(
      "Transpose Matrix", [](float4x4 matrix) { return math::transpose(matrix); });
  builder.set_matching_fn(fn);
}

static void node_register()
{
  static blender::bke::bNodeType ntype;
  fn_node_type_base(&ntype, "FunctionNodeTransposeMatrix", FN_NODE_TRANSPOSE_MATRIX);
  ntype.ui_name = "Transpose Matrix";
  ntype.ui_description =
      "Flip a matrix over its diagonal, turning columns into rows and vice-versa";
  ntype.enum_name_legacy = "TRANSPOSE_MATRIX";
  ntype.nclass = NODE_CLASS_CONVERTER;
  ntype.declare = node_declare;
  ntype.build_multi_function = node_build_multi_function;
  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_fn_transpose_matrix_cc
