/* SPDX-FileCopyrightText: 2005 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_shader_util.hh"

namespace blender::nodes::node_shader_object_info_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_output<decl::Vector>("Location");
  b.add_output<decl::Color>("Color");
  b.add_output<decl::Float>("Alpha");
  b.add_output<decl::Float>("Object Index");
  b.add_output<decl::Float>("Material Index");
  b.add_output<decl::Float>("Random");
}

static int node_shader_gpu_object_info(GPUMaterial *mat,
                                       bNode *node,
                                       bNodeExecData * /*execdata*/,
                                       GPUNodeStack *in,
                                       GPUNodeStack *out)
{
  Material *ma = GPU_material_get_material(mat);
  float index = ma ? ma->index : 0.0f;
  GPU_material_flag_set(mat, GPU_MATFLAG_OBJECT_INFO);
  return GPU_stack_link(mat, node, "node_object_info", in, out, GPU_constant(&index));
}

}  // namespace blender::nodes::node_shader_object_info_cc

void register_node_type_sh_object_info()
{
  namespace file_ns = blender::nodes::node_shader_object_info_cc;

  static bNodeType ntype;

  sh_node_type_base(&ntype, SH_NODE_OBJECT_INFO, "Object Info", NODE_CLASS_INPUT);
  ntype.declare = file_ns::node_declare;
  ntype.gpu_fn = file_ns::node_shader_gpu_object_info;

  nodeRegisterType(&ntype);
}
