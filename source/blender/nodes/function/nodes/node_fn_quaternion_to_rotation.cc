/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_math_quaternion.hh"

#include "node_function_util.hh"

namespace blender::nodes::node_fn_quaternion_to_rotation_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.is_function_node();
  b.add_input<decl::Float>("W").default_value(1.0f);
  b.add_input<decl::Float>("X").default_value(0.0f);
  b.add_input<decl::Float>("Y").default_value(0.0f);
  b.add_input<decl::Float>("Z").default_value(0.0f);
  b.add_output<decl::Rotation>("Rotation");
};

static void node_build_multi_function(NodeMultiFunctionBuilder &builder)
{
  static auto fn = mf::build::SI4_SO<float, float, float, float, math::Quaternion>(
      "Quaternion to Rotation", [](float w, float x, float y, float z) {
        math::Quaternion combined(w, x, y, z);
        return math::normalize(combined);
      });
  builder.set_matching_fn(fn);
}

static void node_register()
{
  static blender::bke::bNodeType ntype;
  fn_node_type_base(&ntype, "FunctionNodeQuaternionToRotation", FN_NODE_QUATERNION_TO_ROTATION);
  ntype.ui_name = "Quaternion to Rotation";
  ntype.ui_description = "Build a rotation from quaternion components";
  ntype.enum_name_legacy = "QUATERNION_TO_ROTATION";
  ntype.nclass = NODE_CLASS_CONVERTER;
  ntype.declare = node_declare;
  ntype.build_multi_function = node_build_multi_function;
  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_fn_quaternion_to_rotation_cc
