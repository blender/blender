/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_math_matrix.hh"

#include "NOD_inverse_eval_params.hh"
#include "NOD_value_elem_eval.hh"

#include "node_function_util.hh"

namespace blender::nodes::node_fn_matrix_multiply_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.is_function_node();
  b.add_input<decl::Matrix>("Matrix");
  b.add_input<decl::Matrix>("Matrix", "Matrix_001");
  b.add_output<decl::Matrix>("Matrix");
}

static void node_build_multi_function(NodeMultiFunctionBuilder &builder)
{
  static auto fn = mf::build::SI2_SO<float4x4, float4x4, float4x4>(
      "Multiply Matrices", [](float4x4 a, float4x4 b) { return a * b; });
  builder.set_matching_fn(fn);
}

static void node_eval_elem(value_elem::ElemEvalParams &params)
{
  using namespace value_elem;
  params.set_output_elem("Matrix", MatrixElem::all());
}

static void node_eval_inverse_elem(value_elem::InverseElemEvalParams &params)
{
  using namespace value_elem;
  const MatrixElem first_input_elem = MatrixElem::all();
  params.set_input_elem("Matrix", first_input_elem);
}

static void node_eval_inverse(inverse_eval::InverseEvalParams &params)
{
  const float4x4 output = params.get_output<float4x4>("Matrix");
  const float4x4 second_input = params.get_input<float4x4>("Matrix_001");
  const float4x4 first_input = output * math::invert(second_input);
  params.set_input("Matrix", first_input);
}

static void node_register()
{
  static blender::bke::bNodeType ntype;
  fn_node_type_base(&ntype, "FunctionNodeMatrixMultiply", FN_NODE_MATRIX_MULTIPLY);
  ntype.ui_name = "Multiply Matrices";
  ntype.ui_description = "Perform a matrix multiplication on two input matrices";
  ntype.enum_name_legacy = "MATRIX_MULTIPLY";
  ntype.nclass = NODE_CLASS_CONVERTER;
  ntype.declare = node_declare;
  ntype.build_multi_function = node_build_multi_function;
  ntype.eval_elem = node_eval_elem;
  ntype.eval_inverse_elem = node_eval_inverse_elem;
  ntype.eval_inverse = node_eval_inverse;
  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_fn_matrix_multiply_cc
