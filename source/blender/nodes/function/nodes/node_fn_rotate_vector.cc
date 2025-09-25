/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_math_quaternion.hh"

#include "node_function_util.hh"

namespace blender::nodes::node_fn_rotate_vector_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.use_custom_socket_order();
  b.allow_any_socket_order();
  b.is_function_node();
  b.add_input<decl::Vector>("Vector").is_default_link_socket();
  b.add_output<decl::Vector>("Vector").align_with_previous();
  b.add_input<decl::Rotation>("Rotation");
};

static void node_build_multi_function(NodeMultiFunctionBuilder &builder)
{
  static auto fn = mf::build::SI2_SO<float3, math::Quaternion, float3>(
      "Rotate Vector",
      [](float3 vector, math::Quaternion quat) { return math::transform_point(quat, vector); });
  builder.set_matching_fn(fn);
}

static void node_register()
{
  static blender::bke::bNodeType ntype;
  fn_node_type_base(&ntype, "FunctionNodeRotateVector", FN_NODE_ROTATE_VECTOR);
  ntype.ui_name = "Rotate Vector";
  ntype.ui_description = "Apply a rotation to a given vector";
  ntype.enum_name_legacy = "ROTATE_VECTOR";
  ntype.nclass = NODE_CLASS_CONVERTER;
  ntype.declare = node_declare;
  ntype.build_multi_function = node_build_multi_function;
  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_fn_rotate_vector_cc
