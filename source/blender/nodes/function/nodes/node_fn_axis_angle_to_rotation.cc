/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_math_axis_angle.hh"
#include "BLI_math_quaternion.hh"

#include "node_function_util.hh"

namespace blender::nodes::node_fn_axis_angle_to_rotation_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.is_function_node();
  b.add_input<decl::Vector>("Axis").default_value({0.0f, 0.0f, 1.0f});
  b.add_input<decl::Float>("Angle").subtype(PROP_ANGLE);
  b.add_output<decl::Rotation>("Rotation");
};

static void node_build_multi_function(NodeMultiFunctionBuilder &builder)
{
  static auto fn = mf::build::SI2_SO<float3, float, math::Quaternion>(
      "Axis Angle to Quaternion", [](float3 axis, float angle) {
        if (UNLIKELY(math::is_zero(axis))) {
          return math::Quaternion::identity();
        }
        const float3 axis_normalized = math::normalize(axis);
        const math::AxisAngle axis_angle = math::AxisAngle(axis_normalized, angle);
        return math::to_quaternion(axis_angle);
      });
  builder.set_matching_fn(fn);
}

static void node_register()
{
  static bNodeType ntype;
  fn_node_type_base(
      &ntype, FN_NODE_AXIS_ANGLE_TO_ROTATION, "Axis Angle to Rotation", NODE_CLASS_CONVERTER);
  ntype.declare = node_declare;
  ntype.build_multi_function = node_build_multi_function;
  nodeRegisterType(&ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_fn_axis_angle_to_rotation_cc
