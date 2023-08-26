/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_math_euler.hh"

#include "node_function_util.hh"

namespace blender::nodes::node_fn_rotation_to_euler_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.is_function_node();
  b.add_input<decl::Rotation>("Rotation");
  b.add_output<decl::Vector>("Euler").subtype(PROP_EULER);
};

static void node_build_multi_function(NodeMultiFunctionBuilder &builder)
{
  static auto fn = mf::build::SI1_SO<math::Quaternion, float3>(
      "Quaternion to Euler XYZ", [](math::Quaternion quat) { return math::to_euler(quat); });
  builder.set_matching_fn(fn);
}

static void node_register()
{
  static bNodeType ntype;
  fn_node_type_base(&ntype, FN_NODE_ROTATION_TO_EULER, "Rotation to Euler", NODE_CLASS_CONVERTER);
  ntype.declare = node_declare;
  ntype.build_multi_function = node_build_multi_function;
  nodeRegisterType(&ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_fn_rotation_to_euler_cc
