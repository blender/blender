/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2005 Blender Foundation */

#include "node_shader_util.hh"

namespace blender::nodes::node_shader_layer_weight_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Float>(N_("Blend")).default_value(0.5f).min(0.0f).max(1.0f);
  b.add_input<decl::Vector>(N_("Normal")).hide_value();
  b.add_output<decl::Float>(N_("Fresnel"));
  b.add_output<decl::Float>(N_("Facing"));
}

static int node_shader_gpu_layer_weight(GPUMaterial *mat,
                                        bNode *node,
                                        bNodeExecData * /*execdata*/,
                                        GPUNodeStack *in,
                                        GPUNodeStack *out)
{
  if (!in[1].link) {
    GPU_link(mat, "world_normals_get", &in[1].link);
  }

  return GPU_stack_link(mat, node, "node_layer_weight", in, out);
}

}  // namespace blender::nodes::node_shader_layer_weight_cc

/* node type definition */
void register_node_type_sh_layer_weight()
{
  namespace file_ns = blender::nodes::node_shader_layer_weight_cc;

  static bNodeType ntype;

  sh_node_type_base(&ntype, SH_NODE_LAYER_WEIGHT, "Layer Weight", NODE_CLASS_INPUT);
  ntype.declare = file_ns::node_declare;
  ntype.gpu_fn = file_ns::node_shader_gpu_layer_weight;

  nodeRegisterType(&ntype);
}
