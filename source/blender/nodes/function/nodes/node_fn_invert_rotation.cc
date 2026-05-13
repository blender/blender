/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_math_quaternion.hh"

#include "node_function_util.hh"
#include "node_shader_util.hh"

namespace blender::nodes::node_fn_invert_rotation_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.use_custom_socket_order();
  b.allow_any_socket_order();
  b.is_function_node();
  b.add_input<decl::Rotation>("Rotation"_ustr);
  b.add_output<decl::Rotation>("Rotation"_ustr).align_with_previous();
};

static int node_gpu_material(GPUMaterial *mat,
                             bNode *node,
                             bNodeExecData * /*execdata*/,
                             GPUNodeStack *in,
                             GPUNodeStack *out)
{
  return GPU_stack_link(mat, node, "invert_rotation", in, out);
}

static void node_build_multi_function(NodeMultiFunctionBuilder &builder)
{
  static auto fn = mf::build::SI1_SO<math::Quaternion, math::Quaternion>(
      "Invert Quaternion", [](math::Quaternion quat) { return math::invert(quat); });
  builder.set_matching_fn(fn);
}

static void node_register()
{
  static bke::bNodeType ntype;
  fn_cmp_node_type_base(&ntype, "FunctionNodeInvertRotation"_ustr, FN_NODE_INVERT_ROTATION);
  ntype.ui_name = "Invert Rotation";
  ntype.ui_description = "Compute the inverse of the given rotation";
  ntype.enum_name_legacy = "INVERT_ROTATION";
  ntype.nclass = NODE_CLASS_CONVERTER;
  ntype.declare = node_declare;
  ntype.gpu_fn = node_gpu_material;
  ntype.build_multi_function = node_build_multi_function;
  bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_fn_invert_rotation_cc
