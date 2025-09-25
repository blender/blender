/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_math_matrix.hh"
#include "BLI_math_rotation.hh"

#include "NOD_inverse_eval_params.hh"
#include "NOD_value_elem_eval.hh"

#include "node_function_util.hh"

namespace blender::nodes::node_fn_separate_transform_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.is_function_node();
  b.add_input<decl::Matrix>("Transform");
  b.add_output<decl::Vector>("Translation").subtype(PROP_TRANSLATION);
  b.add_output<decl::Rotation>("Rotation");
  b.add_output<decl::Vector>("Scale").subtype(PROP_XYZ);
};

class SeparateTransformFunction : public mf::MultiFunction {
 public:
  SeparateTransformFunction()
  {
    static const mf::Signature signature = []() {
      mf::Signature signature;
      mf::SignatureBuilder builder{"Separate Transform", signature};
      builder.single_input<float4x4>("Transform");
      builder.single_output<float3>("Translation", mf::ParamFlag::SupportsUnusedOutput);
      builder.single_output<math::Quaternion>("Rotation", mf::ParamFlag::SupportsUnusedOutput);
      builder.single_output<float3>("Scale", mf::ParamFlag::SupportsUnusedOutput);
      return signature;
    }();
    this->set_signature(&signature);
  }

  void call(const IndexMask &mask, mf::Params params, mf::Context /*context*/) const override
  {
    const VArraySpan transforms = params.readonly_single_input<float4x4>(0, "Transform");
    MutableSpan translation = params.uninitialized_single_output_if_required<float3>(
        1, "Translation");
    MutableSpan rotation = params.uninitialized_single_output_if_required<math::Quaternion>(
        2, "Rotation");
    MutableSpan scale = params.uninitialized_single_output_if_required<float3>(3, "Scale");

    if (!translation.is_empty()) {
      mask.foreach_index_optimized<int64_t>(
          [&](const int64_t i) { translation[i] = transforms[i].location(); });
    }

    if (rotation.is_empty() && !scale.is_empty()) {
      mask.foreach_index([&](const int64_t i) { scale[i] = math::to_scale(transforms[i]); });
    }
    else if (!rotation.is_empty() && scale.is_empty()) {
      mask.foreach_index([&](const int64_t i) {
        rotation[i] = math::normalized_to_quaternion_safe(
            math::normalize(float3x3(transforms[i])));
      });
    }
    else if (!rotation.is_empty() && !scale.is_empty()) {
      mask.foreach_index([&](const int64_t i) {
        const float3x3 normalized_mat = math::normalize_and_get_size(float3x3(transforms[i]),
                                                                     scale[i]);
        rotation[i] = math::normalized_to_quaternion_safe(normalized_mat);
      });
    }
  }
};

static void node_build_multi_function(NodeMultiFunctionBuilder &builder)
{
  static SeparateTransformFunction fn;
  builder.set_matching_fn(fn);
}

static void node_eval_elem(value_elem::ElemEvalParams &params)
{
  using namespace value_elem;
  const MatrixElem matrix_elem = params.get_input_elem<MatrixElem>("Transform");
  params.set_output_elem("Translation", matrix_elem.translation);
  params.set_output_elem("Rotation", matrix_elem.rotation);
  params.set_output_elem("Scale", matrix_elem.scale);
}

static void node_eval_inverse_elem(value_elem::InverseElemEvalParams &params)
{
  using namespace value_elem;
  MatrixElem transform_elem;
  transform_elem.translation = params.get_output_elem<VectorElem>("Translation");
  transform_elem.rotation = params.get_output_elem<RotationElem>("Rotation");
  transform_elem.scale = params.get_output_elem<VectorElem>("Scale");
  params.set_input_elem("Transform", transform_elem);
}

static void node_eval_inverse(inverse_eval::InverseEvalParams &params)
{
  const float3 translation = params.get_output<float3>("Translation");
  const math::Quaternion rotation = params.get_output<math::Quaternion>("Rotation");
  const float3 scale = params.get_output<float3>("Scale");
  params.set_input("Transform", math::from_loc_rot_scale<float4x4>(translation, rotation, scale));
}

static void node_register()
{
  static blender::bke::bNodeType ntype;
  fn_node_type_base(&ntype, "FunctionNodeSeparateTransform", FN_NODE_SEPARATE_TRANSFORM);
  ntype.ui_name = "Separate Transform";
  ntype.ui_description =
      "Split a transformation matrix into a translation vector, a rotation, and a scale vector";
  ntype.enum_name_legacy = "SEPARATE_TRANSFORM";
  ntype.nclass = NODE_CLASS_CONVERTER;
  ntype.declare = node_declare;
  ntype.build_multi_function = node_build_multi_function;
  ntype.eval_elem = node_eval_elem;
  ntype.eval_inverse_elem = node_eval_inverse_elem;
  ntype.eval_inverse = node_eval_inverse;
  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_fn_separate_transform_cc
