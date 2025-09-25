/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_math_quaternion_types.hh"

#include "node_function_util.hh"

namespace blender::nodes::node_fn_rotation_to_quaternion_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.is_function_node();
  b.add_input<decl::Rotation>("Rotation");
  b.add_output<decl::Float>("W");
  b.add_output<decl::Float>("X");
  b.add_output<decl::Float>("Y");
  b.add_output<decl::Float>("Z");
};

class SeparateQuaternionFunction : public mf::MultiFunction {
 public:
  SeparateQuaternionFunction()
  {
    static mf::Signature signature_;
    mf::SignatureBuilder builder{"Rotation to Quaternion", signature_};
    builder.single_input<math::Quaternion>("Quaternion");
    builder.single_output<float>("W");
    builder.single_output<float>("X");
    builder.single_output<float>("Y");
    builder.single_output<float>("Z");
    this->set_signature(&signature_);
  }

  void call(const IndexMask &mask, mf::Params params, mf::Context /*context*/) const override
  {
    const VArraySpan<math::Quaternion> quats = params.readonly_single_input<math::Quaternion>(
        0, "Quaternion");
    MutableSpan<float> w = params.uninitialized_single_output<float>(1, "W");
    MutableSpan<float> x = params.uninitialized_single_output<float>(2, "X");
    MutableSpan<float> y = params.uninitialized_single_output<float>(3, "Y");
    MutableSpan<float> z = params.uninitialized_single_output<float>(4, "Z");
    mask.foreach_index([&](const int64_t i) {
      const math::Quaternion quat = quats[i];
      w[i] = quat.w;
      x[i] = quat.x;
      y[i] = quat.y;
      z[i] = quat.z;
    });
  }
};

static void node_build_multi_function(NodeMultiFunctionBuilder &builder)
{
  static SeparateQuaternionFunction fn;
  builder.set_matching_fn(fn);
}

static void node_register()
{
  static blender::bke::bNodeType ntype;
  fn_node_type_base(&ntype, "FunctionNodeRotationToQuaternion", FN_NODE_ROTATION_TO_QUATERNION);
  ntype.ui_name = "Rotation to Quaternion";
  ntype.ui_description = "Retrieve the quaternion components representing a rotation";
  ntype.enum_name_legacy = "ROTATION_TO_QUATERNION";
  ntype.nclass = NODE_CLASS_CONVERTER;
  ntype.declare = node_declare;
  ntype.build_multi_function = node_build_multi_function;
  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_fn_rotation_to_quaternion_cc
