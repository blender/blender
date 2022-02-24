/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2005 Blender Foundation. All rights reserved. */

#include "node_shader_util.hh"

namespace blender::nodes::node_shader_output_world_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Shader>(N_("Surface"));
  b.add_input<decl::Shader>(N_("Volume"));
}

static int node_shader_gpu_output_world(GPUMaterial *mat,
                                        bNode *node,
                                        bNodeExecData *UNUSED(execdata),
                                        GPUNodeStack *in,
                                        GPUNodeStack *out)
{
  GPUNodeLink *outlink;

  GPU_stack_link(mat, node, "node_output_world", in, out, &outlink);
  GPU_material_output_link(mat, outlink);

  return true;
}

}  // namespace blender::nodes::node_shader_output_world_cc

/* node type definition */
void register_node_type_sh_output_world()
{
  namespace file_ns = blender::nodes::node_shader_output_world_cc;

  static bNodeType ntype;

  sh_node_type_base(&ntype, SH_NODE_OUTPUT_WORLD, "World Output", NODE_CLASS_OUTPUT);
  ntype.declare = file_ns::node_declare;
  node_type_gpu(&ntype, file_ns::node_shader_gpu_output_world);

  ntype.no_muting = true;

  nodeRegisterType(&ntype);
}
