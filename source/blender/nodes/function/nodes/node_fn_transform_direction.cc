/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_math_matrix.hh"

#include "node_function_util.hh"

namespace blender::nodes::node_fn_transform_direction_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.use_custom_socket_order();
  b.allow_any_socket_order();
  b.is_function_node();
  b.add_input<decl::Vector>("Direction").subtype(PROP_XYZ);
  b.add_output<decl::Vector>("Direction").subtype(PROP_XYZ).align_with_previous();
  b.add_input<decl::Matrix>("Transform");
}

static void node_build_multi_function(NodeMultiFunctionBuilder &builder)
{
  static auto fn = mf::build::SI2_SO<float3, float4x4, float3>(
      "Transform Direction", [](float3 direction, float4x4 matrix) {
        return math::transform_direction(matrix, direction);
      });
  builder.set_matching_fn(fn);
}

static void node_register()
{
  static blender::bke::bNodeType ntype;
  fn_node_type_base(&ntype, "FunctionNodeTransformDirection", FN_NODE_TRANSFORM_DIRECTION);
  ntype.ui_name = "Transform Direction";
  ntype.ui_description =
      "Apply a transformation matrix (excluding translation) to the given vector";
  ntype.enum_name_legacy = "TRANSFORM_DIRECTION";
  ntype.nclass = NODE_CLASS_CONVERTER;
  ntype.declare = node_declare;
  ntype.build_multi_function = node_build_multi_function;
  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_fn_transform_direction_cc
