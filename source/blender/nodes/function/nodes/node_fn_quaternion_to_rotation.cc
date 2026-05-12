/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_math_quaternion.hh"

#include "node_function_util.hh"
#include "node_shader_util.hh"

namespace blender::nodes::node_fn_quaternion_to_rotation_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.is_function_node();
  b.add_input<decl::Float>("W"_ustr).default_value(1.0f);
  b.add_input<decl::Float>("X"_ustr).default_value(0.0f);
  b.add_input<decl::Float>("Y"_ustr).default_value(0.0f);
  b.add_input<decl::Float>("Z"_ustr).default_value(0.0f);
  b.add_output<decl::Rotation>("Rotation"_ustr);
};

static void node_build_multi_function(NodeMultiFunctionBuilder &builder)
{
  static auto fn = mf::build::SI4_SO<float, float, float, float, math::Quaternion>(
      "Quaternion to Rotation", [](float w, float x, float y, float z) {
        math::Quaternion combined(w, x, y, z);
        return math::normalize(combined);
      });
  builder.set_matching_fn(fn);
}

static int node_gpu_material(GPUMaterial *mat,
                             bNode *node,
                             bNodeExecData * /*execdata*/,
                             GPUNodeStack *in,
                             GPUNodeStack *out)
{
  return GPU_stack_link(mat, node, "quaternion_to_rotation", in, out);
}

static void node_register()
{
  static bke::bNodeType ntype;
  fn_cmp_node_type_base(
      &ntype, "FunctionNodeQuaternionToRotation"_ustr, FN_NODE_QUATERNION_TO_ROTATION);
  ntype.ui_name = "Quaternion to Rotation";
  ntype.ui_description = "Build a rotation from quaternion components";
  ntype.enum_name_legacy = "QUATERNION_TO_ROTATION";
  ntype.nclass = NODE_CLASS_CONVERTER;
  ntype.declare = node_declare;
  ntype.gpu_fn = node_gpu_material;
  ntype.build_multi_function = node_build_multi_function;
  bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_fn_quaternion_to_rotation_cc
