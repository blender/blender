/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2005 Blender Foundation */

#include "node_shader_util.hh"

namespace blender::nodes::node_shader_shader_to_rgb_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Shader>(N_("Shader"));
  b.add_output<decl::Color>(N_("Color"));
  b.add_output<decl::Float>(N_("Alpha"));
}

static int node_shader_gpu_shadertorgb(GPUMaterial *mat,
                                       bNode *node,
                                       bNodeExecData * /*execdata*/,
                                       GPUNodeStack *in,
                                       GPUNodeStack *out)
{
  GPU_material_flag_set(mat, GPU_MATFLAG_SHADER_TO_RGBA);

  return GPU_stack_link(mat, node, "node_shader_to_rgba", in, out);
}

}  // namespace blender::nodes::node_shader_shader_to_rgb_cc

/* node type definition */
void register_node_type_sh_shadertorgb()
{
  namespace file_ns = blender::nodes::node_shader_shader_to_rgb_cc;

  static bNodeType ntype;

  sh_node_type_base(&ntype, SH_NODE_SHADERTORGB, "Shader to RGB", NODE_CLASS_CONVERTER);
  ntype.declare = file_ns::node_declare;
  ntype.add_ui_poll = object_eevee_shader_nodes_poll;
  ntype.gpu_fn = file_ns::node_shader_gpu_shadertorgb;

  nodeRegisterType(&ntype);
}
