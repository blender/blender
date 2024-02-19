/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_math_matrix.hh"
#include "BLI_math_rotation.hh"

#include "NOD_socket_search_link.hh"

#include "node_function_util.hh"

namespace blender::nodes::node_fn_combine_transform_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.is_function_node();
  b.add_input<decl::Vector>("Location").subtype(PROP_TRANSLATION);
  b.add_input<decl::Rotation>("Rotation");
  b.add_input<decl::Vector>("Scale").default_value(float3(1)).subtype(PROP_XYZ);
  b.add_output<decl::Matrix>("Transform");
}

static void search_link_ops(GatherLinkSearchOpParams &params)
{
  if (U.experimental.use_new_matrix_socket) {
    nodes::search_link_ops_for_basic_node(params);
  }
}

class CombineTransformFunction : public mf::MultiFunction {
 public:
  CombineTransformFunction()
  {
    static const mf::Signature signature = []() {
      mf::Signature signature;
      mf::SignatureBuilder builder{"Combine Transform", signature};
      builder.single_input<float3>("Location");
      builder.single_input<math::Quaternion>("Rotation");
      builder.single_input<float3>("Scale");
      builder.single_output<float4x4>("Transform");
      return signature;
    }();
    this->set_signature(&signature);
  }

  void call(const IndexMask &mask, mf::Params params, mf::Context /*context*/) const override
  {
    const VArray location = params.readonly_single_input<float3>(0, "Location");
    const VArray rotation = params.readonly_single_input<math::Quaternion>(1, "Rotation");
    const VArray scale = params.readonly_single_input<float3>(2, "Scale");
    MutableSpan transforms = params.uninitialized_single_output<float4x4>(3, "Transform");

    const std::optional<float3> location_single = location.get_if_single();
    const std::optional<math::Quaternion> rotation_single = rotation.get_if_single();
    const std::optional<float3> scale_single = scale.get_if_single();

    const bool no_translation = location_single && math::is_zero(*location_single);
    const bool no_rotation = rotation_single && math::angle_of(*rotation_single).radian() < 1e-7f;
    const bool no_scale = scale_single && math::is_equal(*scale_single, float3(1), 1e-7f);

    if (no_rotation && no_scale) {
      mask.foreach_index(
          [&](const int64_t i) { transforms[i] = math::from_location<float4x4>(location[i]); });
    }
    else if (no_translation && no_scale) {
      mask.foreach_index(
          [&](const int64_t i) { transforms[i] = math::from_rotation<float4x4>(rotation[i]); });
    }
    else if (no_translation && no_rotation) {
      mask.foreach_index(
          [&](const int64_t i) { transforms[i] = math::from_scale<float4x4>(scale[i]); });
    }
    else {
      mask.foreach_index([&](const int64_t i) {
        transforms[i] = math::from_loc_rot_scale<float4x4>(location[i], rotation[i], scale[i]);
      });
    }
  }
};

static void node_build_multi_function(NodeMultiFunctionBuilder &builder)
{
  static CombineTransformFunction fn;
  builder.set_matching_fn(fn);
}

static void node_register()
{
  static bNodeType ntype;
  fn_node_type_base(&ntype, FN_NODE_COMBINE_TRANSFORM, "Combine Transform", NODE_CLASS_CONVERTER);
  ntype.declare = node_declare;
  ntype.gather_link_search_ops = search_link_ops;
  ntype.build_multi_function = node_build_multi_function;
  nodeRegisterType(&ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_fn_combine_transform_cc
