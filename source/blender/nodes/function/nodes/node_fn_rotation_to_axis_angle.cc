/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_math_axis_angle.hh"
#include "BLI_math_quaternion.hh"

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

static void node_register()
{
  static blender::bke::bNodeType ntype;
  fn_node_type_base(
      &ntype, FN_NODE_ROTATION_TO_AXIS_ANGLE, "Rotation to Axis Angle", NODE_CLASS_CONVERTER);
  ntype.declare = node_declare;
  ntype.build_multi_function = node_build_multi_function;
  blender::bke::nodeRegisterType(&ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_fn_rotation_to_axis_angle_cc
