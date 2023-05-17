/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2005 Blender Foundation */

#include "node_shader_util.hh"

namespace blender::nodes::node_shader_emission_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>("Color").default_value({1.0f, 1.0f, 1.0f, 1.0f});
  b.add_input<decl::Float>("Strength").default_value(1.0f).min(0.0f).max(1000000.0f);
  b.add_input<decl::Float>("Weight").unavailable();
  b.add_output<decl::Shader>("Emission");
}

static int node_shader_gpu_emission(GPUMaterial *mat,
                                    bNode *node,
                                    bNodeExecData * /*execdata*/,
                                    GPUNodeStack *in,
                                    GPUNodeStack *out)
{
  GPU_material_flag_set(mat, GPU_MATFLAG_EMISSION);
  return GPU_stack_link(mat, node, "node_emission", in, out);
}

}  // namespace blender::nodes::node_shader_emission_cc

/* node type definition */
void register_node_type_sh_emission()
{
  namespace file_ns = blender::nodes::node_shader_emission_cc;

  static bNodeType ntype;

  sh_node_type_base(&ntype, SH_NODE_EMISSION, "Emission", NODE_CLASS_SHADER);
  ntype.declare = file_ns::node_declare;
  ntype.gpu_fn = file_ns::node_shader_gpu_emission;

  nodeRegisterType(&ntype);
}
