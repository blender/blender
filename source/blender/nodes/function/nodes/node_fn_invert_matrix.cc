/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_math_matrix.hh"

#include "node_function_util.hh"

namespace blender::nodes::node_fn_invert_matrix_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.use_custom_socket_order();
  b.allow_any_socket_order();
  b.is_function_node();
  b.add_input<decl::Matrix>("Matrix");
  b.add_output<decl::Matrix>("Matrix")
      .description("The inverted matrix or the identity matrix if the input is not invertible")
      .align_with_previous();
  b.add_output<decl::Bool>("Invertible").description("True if the input matrix is invertible");
}

class InvertMatrixFunction : public mf::MultiFunction {
 public:
  InvertMatrixFunction()
  {
    static mf::Signature signature = []() {
      mf::Signature signature;
      mf::SignatureBuilder builder{"Invert Matrix", signature};
      builder.single_input<float4x4>("Matrix");
      builder.single_output<float4x4>("Matrix", mf::ParamFlag::SupportsUnusedOutput);
      builder.single_output<bool>("Invertible", mf::ParamFlag::SupportsUnusedOutput);
      return signature;
    }();
    this->set_signature(&signature);
  }

  void call(const IndexMask &mask, mf::Params params, mf::Context /*context*/) const override
  {
    const VArraySpan<float4x4> in_matrices = params.readonly_single_input<float4x4>(0, "Matrix");
    MutableSpan<float4x4> out_matrices = params.uninitialized_single_output_if_required<float4x4>(
        1, "Matrix");
    MutableSpan<bool> out_invertible = params.uninitialized_single_output_if_required<bool>(
        2, "Invertible");
    mask.foreach_index([&](const int64_t i) {
      const float4x4 &matrix = in_matrices[i];
      bool success;
      float4x4 inverted_matrix = math::invert(matrix, success);
      if (!out_matrices.is_empty()) {
        out_matrices[i] = success ? inverted_matrix : float4x4::identity();
      }
      if (!out_invertible.is_empty()) {
        out_invertible[i] = success;
      }
    });
  }
};

static void node_build_multi_function(NodeMultiFunctionBuilder &builder)
{
  static InvertMatrixFunction fn;
  builder.set_matching_fn(fn);
}

static void node_register()
{
  static blender::bke::bNodeType ntype;
  fn_node_type_base(&ntype, "FunctionNodeInvertMatrix", FN_NODE_INVERT_MATRIX);
  ntype.ui_name = "Invert Matrix";
  ntype.ui_description = "Compute the inverse of the given matrix, if one exists";
  ntype.enum_name_legacy = "INVERT_MATRIX";
  ntype.nclass = NODE_CLASS_CONVERTER;
  ntype.declare = node_declare;
  ntype.build_multi_function = node_build_multi_function;
  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_fn_invert_matrix_cc
