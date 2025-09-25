/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_math_axis_angle.hh"
#include "BLI_math_quaternion.hh"

#include "NOD_inverse_eval_params.hh"
#include "NOD_value_elem_eval.hh"

#include "node_function_util.hh"

namespace blender::nodes::node_fn_rotation_to_axis_angle_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.is_function_node();
  b.add_input<decl::Rotation>("Rotation");
  b.add_output<decl::Vector>("Axis");
  b.add_output<decl::Float>("Angle").subtype(PROP_ANGLE);
};

class QuaterniontoAxisAngleFunction : public mf::MultiFunction {
 public:
  QuaterniontoAxisAngleFunction()
  {
    static mf::Signature signature_;
    mf::SignatureBuilder builder{"Quaternion to Axis Angle", signature_};
    builder.single_input<math::Quaternion>("Quaternion");
    builder.single_output<float3>("Axis");
    builder.single_output<float>("Angle");
    this->set_signature(&signature_);
  }

  void call(const IndexMask &mask, mf::Params params, mf::Context /*context*/) const override
  {
    const VArraySpan<math::Quaternion> quaternions =
        params.readonly_single_input<math::Quaternion>(0, "Quaternion");
    MutableSpan<float3> axes = params.uninitialized_single_output<float3>(1, "Axis");
    MutableSpan<float> angles = params.uninitialized_single_output<float>(2, "Angle");
    mask.foreach_index([&](const int64_t i) {
      const math::AxisAngle axis_angle = math::to_axis_angle(quaternions[i]);
      axes[i] = axis_angle.axis();
      angles[i] = axis_angle.angle().radian();
    });
  }
};

static void node_build_multi_function(NodeMultiFunctionBuilder &builder)
{
  static QuaterniontoAxisAngleFunction fn;
  builder.set_matching_fn(fn);
}

static void node_eval_elem(value_elem::ElemEvalParams &params)
{
  using namespace value_elem;
  const RotationElem rotation_elem = params.get_input_elem<RotationElem>("Rotation");
  params.set_output_elem("Axis", rotation_elem.axis);
  params.set_output_elem("Angle", rotation_elem.angle);
}

static void node_eval_inverse_elem(value_elem::InverseElemEvalParams &params)
{
  using namespace value_elem;
  RotationElem rotation_elem;
  rotation_elem.axis = params.get_output_elem<VectorElem>("Axis");
  rotation_elem.angle = params.get_output_elem<FloatElem>("Angle");
  if (rotation_elem) {
    rotation_elem.euler = VectorElem::all();
  }
  params.set_input_elem("Rotation", rotation_elem);
}

static void node_eval_inverse(inverse_eval::InverseEvalParams &params)
{
  const float3 axis = params.get_output<float3>("Axis");
  const float angle = params.get_output<float>("Angle");
  math::Quaternion rotation;
  if (math::is_zero(axis)) {
    rotation = math::Quaternion::identity();
  }
  else {
    rotation = math::to_quaternion(math::AxisAngle(math::normalize(axis), angle));
  }
  params.set_input("Rotation", rotation);
}

static void node_register()
{
  static blender::bke::bNodeType ntype;
  fn_node_type_base(&ntype, "FunctionNodeRotationToAxisAngle", FN_NODE_ROTATION_TO_AXIS_ANGLE);
  ntype.ui_name = "Rotation to Axis Angle";
  ntype.ui_description = "Convert a rotation to axis angle components";
  ntype.enum_name_legacy = "ROTATION_TO_AXIS_ANGLE";
  ntype.nclass = NODE_CLASS_CONVERTER;
  ntype.declare = node_declare;
  ntype.build_multi_function = node_build_multi_function;
  ntype.eval_elem = node_eval_elem;
  ntype.eval_inverse_elem = node_eval_inverse_elem;
  ntype.eval_inverse = node_eval_inverse;
  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_fn_rotation_to_axis_angle_cc
