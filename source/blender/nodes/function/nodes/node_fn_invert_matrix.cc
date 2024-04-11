/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_math_matrix.hh"

#include "NOD_socket_search_link.hh"

#include "node_function_util.hh"

namespace blender::nodes::node_fn_invert_matrix_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.is_function_node();
  b.add_input<decl::Matrix>("Matrix");
  b.add_output<decl::Matrix>("Matrix").description(
      "The inverted matrix or the identity matrix if the input is not invertable");
  b.add_output<decl::Bool>("Invertable").description("True if the input matrix is invertable");
}

static void search_link_ops(GatherLinkSearchOpParams &params)
{
  if (U.experimental.use_new_matrix_socket) {
    nodes::search_link_ops_for_basic_node(params);
  }
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
      builder.single_output<bool>("Invertable", mf::ParamFlag::SupportsUnusedOutput);
      return signature;
    }();
    this->set_signature(&signature);
  }

  void call(const IndexMask &mask, mf::Params params, mf::Context /*context*/) const override
  {
    const VArraySpan<float4x4> in_matrices = params.readonly_single_input<float4x4>(0, "Matrix");
    MutableSpan<float4x4> out_matrices = params.uninitialized_single_output_if_required<float4x4>(
        1, "Matrix");
    MutableSpan<bool> out_invertable = params.uninitialized_single_output_if_required<bool>(
        2, "Invertable");
    mask.foreach_index([&](const int64_t i) {
      const float4x4 &matrix = in_matrices[i];
      bool success;
      float4x4 inverted_matrix = math::invert(matrix, success);
      if (!out_matrices.is_empty()) {
        out_matrices[i] = success ? inverted_matrix : float4x4::identity();
      }
      if (!out_invertable.is_empty()) {
        out_invertable[i] = success;
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
  static bNodeType ntype;
  fn_node_type_base(&ntype, FN_NODE_INVERT_MATRIX, "Invert Matrix", NODE_CLASS_CONVERTER);
  ntype.declare = node_declare;
  ntype.gather_link_search_ops = search_link_ops;
  ntype.build_multi_function = node_build_multi_function;
  nodeRegisterType(&ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_fn_invert_matrix_cc
