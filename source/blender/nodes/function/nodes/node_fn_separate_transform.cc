/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_math_matrix.hh"
#include "BLI_math_rotation.hh"

#include "NOD_socket_search_link.hh"

#include "node_function_util.hh"

namespace blender::nodes::node_fn_separate_transform_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.is_function_node();
  b.add_input<decl::Matrix>("Transform");
  b.add_output<decl::Vector>("Location").subtype(PROP_TRANSLATION);
  b.add_output<decl::Rotation>("Rotation");
  b.add_output<decl::Vector>("Scale").subtype(PROP_XYZ);
};

static void search_link_ops(GatherLinkSearchOpParams &params)
{
  if (U.experimental.use_new_matrix_socket) {
    nodes::search_link_ops_for_basic_node(params);
  }
}

class SeparateTransformFunction : public mf::MultiFunction {
 public:
  SeparateTransformFunction()
  {
    static const mf::Signature signature = []() {
      mf::Signature signature;
      mf::SignatureBuilder builder{"Separate Transform", signature};
      builder.single_input<float4x4>("Transform");
      builder.single_output<float3>("Location", mf::ParamFlag::SupportsUnusedOutput);
      builder.single_output<math::Quaternion>("Rotation", mf::ParamFlag::SupportsUnusedOutput);
      builder.single_output<float3>("Scale", mf::ParamFlag::SupportsUnusedOutput);
      return signature;
    }();
    this->set_signature(&signature);
  }

  void call(const IndexMask &mask, mf::Params params, mf::Context /*context*/) const override
  {
    const VArraySpan transforms = params.readonly_single_input<float4x4>(0, "Transform");
    MutableSpan location = params.uninitialized_single_output_if_required<float3>(1, "Location");
    MutableSpan rotation = params.uninitialized_single_output_if_required<math::Quaternion>(
        2, "Rotation");
    MutableSpan scale = params.uninitialized_single_output_if_required<float3>(3, "Scale");

    if (!location.is_empty()) {
      mask.foreach_index_optimized<int64_t>(
          [&](const int64_t i) { location[i] = transforms[i].location(); });
    }

    if (rotation.is_empty() && !scale.is_empty()) {
      mask.foreach_index([&](const int64_t i) { location[i] = math::to_scale(transforms[i]); });
    }
    else if (!rotation.is_empty() && scale.is_empty()) {
      mask.foreach_index(
          [&](const int64_t i) { rotation[i] = math::to_quaternion(transforms[i]); });
    }
    else if (!rotation.is_empty() && !scale.is_empty()) {
      mask.foreach_index([&](const int64_t i) {
        math::to_rot_scale(float3x3(transforms[i]), rotation[i], scale[i]);
      });
    }
  }
};

static void node_build_multi_function(NodeMultiFunctionBuilder &builder)
{
  static SeparateTransformFunction fn;
  builder.set_matching_fn(fn);
}

static void node_register()
{
  static bNodeType ntype;
  fn_node_type_base(
      &ntype, FN_NODE_SEPARATE_TRANSFORM, "Separate Transform", NODE_CLASS_CONVERTER);
  ntype.declare = node_declare;
  ntype.gather_link_search_ops = search_link_ops;
  ntype.build_multi_function = node_build_multi_function;
  nodeRegisterType(&ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_fn_separate_transform_cc
