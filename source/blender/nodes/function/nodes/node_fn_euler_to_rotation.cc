/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_math_euler.hh"

#include "NOD_inverse_eval_params.hh"
#include "NOD_value_elem_eval.hh"

#include "node_function_util.hh"

namespace blender::nodes::node_fn_euler_to_rotation_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.is_function_node();
  b.add_input<decl::Vector>("Euler").subtype(PROP_EULER);
  b.add_output<decl::Rotation>("Rotation");
};

static void node_build_multi_function(NodeMultiFunctionBuilder &builder)
{
  static auto fn = mf::build::SI1_SO<float3, math::Quaternion>(
      "Euler XYZ to Quaternion",
      [](float3 euler) { return math::to_quaternion(math::EulerXYZ(euler)); });
  builder.set_matching_fn(fn);
}

static void node_eval_elem(value_elem::ElemEvalParams &params)
{
  using namespace value_elem;
  RotationElem rotation_elem;
  rotation_elem.euler = params.get_input_elem<VectorElem>("Euler");
  if (rotation_elem) {
    rotation_elem.axis = VectorElem::all();
    rotation_elem.angle = FloatElem::all();
  }
  params.set_output_elem("Rotation", rotation_elem);
}

static void node_eval_inverse_elem(value_elem::InverseElemEvalParams &params)
{
  using namespace value_elem;
  const RotationElem rotation_elem = params.get_output_elem<RotationElem>("Rotation");
  VectorElem vector_elem = rotation_elem.euler;
  params.set_input_elem("Euler", vector_elem);
}

static void node_eval_inverse(inverse_eval::InverseEvalParams &params)
{
  const math::Quaternion rotation = params.get_output<math::Quaternion>("Rotation");
  params.set_input("Euler", float3(math::to_euler(rotation)));
}

static void node_register()
{
  static blender::bke::bNodeType ntype;
  fn_node_type_base(&ntype, "FunctionNodeEulerToRotation", FN_NODE_EULER_TO_ROTATION);
  ntype.ui_name = "Euler to Rotation";
  ntype.ui_description = "Build a rotation from separate angles around each axis";
  ntype.enum_name_legacy = "EULER_TO_ROTATION";
  ntype.nclass = NODE_CLASS_CONVERTER;
  ntype.declare = node_declare;
  ntype.build_multi_function = node_build_multi_function;
  ntype.eval_elem = node_eval_elem;
  ntype.eval_inverse_elem = node_eval_inverse_elem;
  ntype.eval_inverse = node_eval_inverse;
  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_fn_euler_to_rotation_cc
