/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_math_matrix.hh"
#include "BLI_math_rotation.hh"

#include "NOD_inverse_eval_params.hh"
#include "NOD_value_elem_eval.hh"

#include "node_function_util.hh"

namespace blender::nodes::node_fn_combine_transform_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.is_function_node();
  b.add_input<decl::Vector>("Translation").subtype(PROP_TRANSLATION);
  b.add_input<decl::Rotation>("Rotation");
  b.add_input<decl::Vector>("Scale").default_value(float3(1)).subtype(PROP_XYZ);
  b.add_output<decl::Matrix>("Transform");
}

class CombineTransformFunction : public mf::MultiFunction {
 public:
  CombineTransformFunction()
  {
    static const mf::Signature signature = []() {
      mf::Signature signature;
      mf::SignatureBuilder builder{"Combine Transform", signature};
      builder.single_input<float3>("Translation");
      builder.single_input<math::Quaternion>("Rotation");
      builder.single_input<float3>("Scale");
      builder.single_output<float4x4>("Transform");
      return signature;
    }();
    this->set_signature(&signature);
  }

  void call(const IndexMask &mask, mf::Params params, mf::Context /*context*/) const override
  {
    const VArray translation = params.readonly_single_input<float3>(0, "Translation");
    const VArray rotation = params.readonly_single_input<math::Quaternion>(1, "Rotation");
    const VArray scale = params.readonly_single_input<float3>(2, "Scale");
    MutableSpan transforms = params.uninitialized_single_output<float4x4>(3, "Transform");

    const std::optional<float3> translation_single = translation.get_if_single();
    const std::optional<math::Quaternion> rotation_single = rotation.get_if_single();
    const std::optional<float3> scale_single = scale.get_if_single();

    const bool no_translation = translation_single && math::is_zero(*translation_single);
    const bool no_rotation = rotation_single && math::angle_of(*rotation_single).radian() < 1e-7f;
    const bool no_scale = scale_single && math::is_equal(*scale_single, float3(1), 1e-7f);

    if (no_rotation && no_scale) {
      mask.foreach_index(
          [&](const int64_t i) { transforms[i] = math::from_location<float4x4>(translation[i]); });
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
        transforms[i] = math::from_loc_rot_scale<float4x4>(translation[i], rotation[i], scale[i]);
      });
    }
  }
};

static void node_build_multi_function(NodeMultiFunctionBuilder &builder)
{
  static CombineTransformFunction fn;
  builder.set_matching_fn(fn);
}

static void node_eval_elem(value_elem::ElemEvalParams &params)
{
  using namespace value_elem;
  MatrixElem matrix_elem;
  matrix_elem.translation = params.get_input_elem<VectorElem>("Translation");
  matrix_elem.rotation = params.get_input_elem<RotationElem>("Rotation");
  matrix_elem.scale = params.get_input_elem<VectorElem>("Scale");
  params.set_output_elem("Transform", matrix_elem);
}

static void node_eval_inverse_elem(value_elem::InverseElemEvalParams &params)
{
  using namespace value_elem;
  const MatrixElem matrix_elem = params.get_output_elem<MatrixElem>("Transform");
  params.set_input_elem("Translation", matrix_elem.translation);
  params.set_input_elem("Rotation", matrix_elem.rotation);
  params.set_input_elem("Scale", matrix_elem.scale);
}

static void node_eval_inverse(inverse_eval::InverseEvalParams &params)
{
  const float4x4 transform = params.get_output<float4x4>("Transform");
  float3 translation;
  math::Quaternion rotation;
  float3 scale;
  math::to_loc_rot_scale_safe<true>(transform, translation, rotation, scale);
  params.set_input("Translation", translation);
  params.set_input("Rotation", rotation);
  params.set_input("Scale", scale);
}

static void node_register()
{
  static blender::bke::bNodeType ntype;
  fn_node_type_base(&ntype, "FunctionNodeCombineTransform", FN_NODE_COMBINE_TRANSFORM);
  ntype.ui_name = "Combine Transform";
  ntype.ui_description =
      "Combine a translation vector, a rotation, and a scale vector into a transformation matrix";
  ntype.enum_name_legacy = "COMBINE_TRANSFORM";
  ntype.nclass = NODE_CLASS_CONVERTER;
  ntype.declare = node_declare;
  ntype.build_multi_function = node_build_multi_function;
  ntype.eval_elem = node_eval_elem;
  ntype.eval_inverse_elem = node_eval_inverse_elem;
  ntype.eval_inverse = node_eval_inverse;
  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_fn_combine_transform_cc
