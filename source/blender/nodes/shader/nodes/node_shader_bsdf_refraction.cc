/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2005 Blender Foundation */

#include "node_shader_util.hh"

namespace blender::nodes::node_shader_bsdf_refraction_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>("Color").default_value({1.0f, 1.0f, 1.0f, 1.0f});
  b.add_input<decl::Float>("Roughness")
      .default_value(0.0f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR);
  b.add_input<decl::Float>("IOR").default_value(1.45f).min(0.0f).max(1000.0f);
  b.add_input<decl::Vector>("Normal").hide_value();
  b.add_input<decl::Float>("Weight").unavailable();
  b.add_output<decl::Shader>("BSDF");
}

static void node_shader_init_refraction(bNodeTree * /*ntree*/, bNode *node)
{
  node->custom1 = SHD_GLOSSY_BECKMANN;
}

static int node_shader_gpu_bsdf_refraction(GPUMaterial *mat,
                                           bNode *node,
                                           bNodeExecData * /*execdata*/,
                                           GPUNodeStack *in,
                                           GPUNodeStack *out)
{
  if (!in[3].link) {
    GPU_link(mat, "world_normals_get", &in[3].link);
  }

  if (node->custom1 == SHD_GLOSSY_SHARP) {
    GPU_link(mat, "set_value_zero", &in[1].link);
  }

  GPU_material_flag_set(mat, GPU_MATFLAG_REFRACT);

  return GPU_stack_link(mat, node, "node_bsdf_refraction", in, out);
}

}  // namespace blender::nodes::node_shader_bsdf_refraction_cc

/* node type definition */
void register_node_type_sh_bsdf_refraction()
{
  namespace file_ns = blender::nodes::node_shader_bsdf_refraction_cc;

  static bNodeType ntype;

  sh_node_type_base(&ntype, SH_NODE_BSDF_REFRACTION, "Refraction BSDF", NODE_CLASS_SHADER);
  ntype.declare = file_ns::node_declare;
  ntype.add_ui_poll = object_shader_nodes_poll;
  blender::bke::node_type_size_preset(&ntype, blender::bke::eNodeSizePreset::MIDDLE);
  ntype.initfunc = file_ns::node_shader_init_refraction;
  ntype.gpu_fn = file_ns::node_shader_gpu_bsdf_refraction;

  nodeRegisterType(&ntype);
}
