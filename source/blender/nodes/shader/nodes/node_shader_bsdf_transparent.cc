/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2005 Blender Foundation */

#include "node_shader_util.hh"

namespace blender::nodes::node_shader_bsdf_transparent_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>("Color").default_value({1.0f, 1.0f, 1.0f, 1.0f});
  b.add_input<decl::Float>("Weight").unavailable();
  b.add_output<decl::Shader>("BSDF");
}

static int node_shader_gpu_bsdf_transparent(GPUMaterial *mat,
                                            bNode *node,
                                            bNodeExecData * /*execdata*/,
                                            GPUNodeStack *in,
                                            GPUNodeStack *out)
{
  if (in[0].link || !is_zero_v3(in[0].vec)) {
    GPU_material_flag_set(mat, GPU_MATFLAG_TRANSPARENT);
  }
  return GPU_stack_link(mat, node, "node_bsdf_transparent", in, out);
}

}  // namespace blender::nodes::node_shader_bsdf_transparent_cc

/* node type definition */
void register_node_type_sh_bsdf_transparent()
{
  namespace file_ns = blender::nodes::node_shader_bsdf_transparent_cc;

  static bNodeType ntype;

  sh_node_type_base(&ntype, SH_NODE_BSDF_TRANSPARENT, "Transparent BSDF", NODE_CLASS_SHADER);
  ntype.add_ui_poll = object_shader_nodes_poll;
  ntype.declare = file_ns::node_declare;
  ntype.gpu_fn = file_ns::node_shader_gpu_bsdf_transparent;

  nodeRegisterType(&ntype);
}
