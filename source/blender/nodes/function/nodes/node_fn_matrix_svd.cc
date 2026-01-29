/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_math_solvers.h"

#include "node_function_util.hh"

namespace blender::nodes::node_fn_matrix_svd_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.is_function_node();
  b.add_input<decl::Matrix>("Matrix").description(
      "Matrix to decompose, only the 3x3 part is used");
  b.add_output<decl::Matrix>("U").description("Left singular vectors");
  b.add_output<decl::Vector>("S").description("Singular values");
  b.add_output<decl::Matrix>("V").description("Right singular vectors");
}

class MatrixSVDFunction : public mf::MultiFunction {
 public:
  MatrixSVDFunction()
  {
    static mf::Signature signature_;
    mf::SignatureBuilder builder{"Matrix SVD", signature_};
    builder.single_input<float4x4>("Matrix");
    builder.single_output<float4x4>("U");
    builder.single_output<float3>("S");
    builder.single_output<float4x4>("V");
    this->set_signature(&signature_);
  }

  void call(const IndexMask &mask, mf::Params params, mf::Context /*context*/) const override
  {
    const VArraySpan<float4x4> matrices = params.readonly_single_input<float4x4>(0, "Matrix");
    MutableSpan<float4x4> Us = params.uninitialized_single_output<float4x4>(1, "U");
    MutableSpan<float3> Ss = params.uninitialized_single_output<float3>(2, "S");
    MutableSpan<float4x4> Vs = params.uninitialized_single_output<float4x4>(3, "V");
    mask.foreach_index([&](const int64_t i) {
      const float3x3 matrix = matrices[i].view<3, 3>();
      float3x3 matrix_U, matrix_V;
      BLI_svd_m3(matrix.ptr(), matrix_U.ptr(), Ss[i], matrix_V.ptr());
      Us[i] = float4x4(matrix_U);
      Vs[i] = float4x4(matrix_V);
    });
  }
};

static void node_build_multi_function(NodeMultiFunctionBuilder &builder)
{
  static MatrixSVDFunction fn;
  builder.set_matching_fn(fn);
}

static void node_register()
{
  static bke::bNodeType ntype;
  fn_node_type_base(&ntype, "FunctionNodeMatrixSVD");
  ntype.ui_name = "Matrix SVD";
  ntype.ui_description = "Compute the singular value decomposition of the 3x3 part of a matrix";
  ntype.nclass = NODE_CLASS_CONVERTER;
  ntype.declare = node_declare;
  ntype.build_multi_function = node_build_multi_function;
  bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_fn_matrix_svd_cc
