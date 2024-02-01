/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_math_quaternion.hh"

#include "node_function_util.hh"

namespace blender::nodes::node_fn_rotate_rotation_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.is_function_node();
  b.add_input<decl::Rotation>("Rotation 1");
  b.add_input<decl::Rotation>("Rotation 2");
  b.add_output<decl::Rotation>("Rotation");
};

static void node_build_multi_function(NodeMultiFunctionBuilder &builder)
{
  static auto fn = mf::build::SI2_SO<math::Quaternion, math::Quaternion, math::Quaternion>(
      "Rotate Rotation", [](math::Quaternion a, math::Quaternion b) { return b * a; });
  builder.set_matching_fn(fn);
}

static void node_register()
{
  static bNodeType ntype;
  fn_node_type_base(&ntype, FN_NODE_ROTATE_ROTATION, "Rotate Rotation", NODE_CLASS_CONVERTER);
  ntype.declare = node_declare;
  ntype.build_multi_function = node_build_multi_function;
  nodeRegisterType(&ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_fn_rotate_rotation_cc
