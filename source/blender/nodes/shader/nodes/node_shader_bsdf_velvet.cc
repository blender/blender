/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2005 Blender Foundation */

#include "node_shader_util.hh"

namespace blender::nodes::node_shader_bsdf_velvet_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>("Color").default_value({0.8f, 0.8f, 0.8f, 1.0f});
  b.add_input<decl::Float>("Sigma").default_value(1.0f).min(0.0f).max(1.0f).subtype(PROP_FACTOR);
  b.add_input<decl::Vector>("Normal").hide_value();
  b.add_input<decl::Float>("Weight").unavailable();
  b.add_output<decl::Shader>("BSDF");
}

static int node_shader_gpu_bsdf_velvet(GPUMaterial *mat,
                                       bNode *node,
                                       bNodeExecData * /*execdata*/,
                                       GPUNodeStack *in,
                                       GPUNodeStack *out)
{
  if (!in[2].link) {
    GPU_link(mat, "world_normals_get", &in[2].link);
  }

  GPU_material_flag_set(mat, GPU_MATFLAG_DIFFUSE);

  return GPU_stack_link(mat, node, "node_bsdf_velvet", in, out);
}

}  // namespace blender::nodes::node_shader_bsdf_velvet_cc

/* node type definition */
void register_node_type_sh_bsdf_velvet()
{
  namespace file_ns = blender::nodes::node_shader_bsdf_velvet_cc;

  static bNodeType ntype;

  sh_node_type_base(&ntype, SH_NODE_BSDF_VELVET, "Velvet BSDF", NODE_CLASS_SHADER);
  ntype.add_ui_poll = object_cycles_shader_nodes_poll;
  ntype.declare = file_ns::node_declare;
  ntype.gpu_fn = file_ns::node_shader_gpu_bsdf_velvet;

  nodeRegisterType(&ntype);
}
