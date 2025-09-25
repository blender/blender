/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_math_axis_angle.hh"

#include "NOD_inverse_eval_params.hh"
#include "NOD_value_elem_eval.hh"

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

static void node_eval_elem(value_elem::ElemEvalParams &params)
{
  using namespace value_elem;
  RotationElem rotation_elem;
  rotation_elem.axis = params.get_input_elem<VectorElem>("Axis");
  rotation_elem.angle = params.get_input_elem<FloatElem>("Angle");
  if (rotation_elem) {
    rotation_elem.euler = VectorElem::all();
  }
  params.set_output_elem("Rotation", rotation_elem);
}

static void node_eval_inverse_elem(value_elem::InverseElemEvalParams &params)
{
  using namespace value_elem;
  const RotationElem rotation_elem = params.get_output_elem<RotationElem>("Rotation");
  VectorElem axis_elem = rotation_elem.axis;
  FloatElem angle_elem = rotation_elem.angle;
  params.set_input_elem("Axis", axis_elem);
  params.set_input_elem("Angle", angle_elem);
}

static void node_eval_inverse(inverse_eval::InverseEvalParams &params)
{
  using namespace inverse_eval;
  const math::Quaternion rotation = params.get_output<math::Quaternion>("Rotation");
  const math::AxisAngle axis_angle = math::to_axis_angle(rotation);
  params.set_input("Axis", axis_angle.axis());
  params.set_input("Angle", axis_angle.angle().radian());
}

static void node_register()
{
  static blender::bke::bNodeType ntype;
  fn_node_type_base(&ntype, "FunctionNodeAxisAngleToRotation", FN_NODE_AXIS_ANGLE_TO_ROTATION);
  ntype.ui_name = "Axis Angle to Rotation";
  ntype.ui_description = "Build a rotation from an axis and a rotation around that axis";
  ntype.enum_name_legacy = "AXIS_ANGLE_TO_ROTATION";
  ntype.nclass = NODE_CLASS_CONVERTER;
  ntype.declare = node_declare;
  ntype.build_multi_function = node_build_multi_function;
  ntype.eval_elem = node_eval_elem;
  ntype.eval_inverse_elem = node_eval_inverse_elem;
  ntype.eval_inverse = node_eval_inverse;
  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_fn_axis_angle_to_rotation_cc
