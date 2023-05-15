/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2005 Blender Foundation */

#include "node_shader_util.hh"

namespace blender::nodes::node_shader_bsdf_glass_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>(N_("Color")).default_value({1.0f, 1.0f, 1.0f, 1.0f});
  b.add_input<decl::Float>(N_("Roughness"))
      .default_value(0.0f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR);
  b.add_input<decl::Float>(N_("IOR")).default_value(1.45f).min(0.0f).max(1000.0f);
  b.add_input<decl::Vector>(N_("Normal")).hide_value();
  b.add_input<decl::Float>(N_("Weight")).unavailable();
  b.add_output<decl::Shader>(N_("BSDF"));
}

static void node_shader_init_glass(bNodeTree * /*ntree*/, bNode *node)
{
  node->custom1 = SHD_GLOSSY_BECKMANN;
}

static int node_shader_gpu_bsdf_glass(GPUMaterial *mat,
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

  GPU_material_flag_set(mat, GPU_MATFLAG_GLOSSY | GPU_MATFLAG_REFRACT);

  float use_multi_scatter = (node->custom1 == SHD_GLOSSY_MULTI_GGX) ? 1.0f : 0.0f;

  return GPU_stack_link(mat, node, "node_bsdf_glass", in, out, GPU_constant(&use_multi_scatter));
}

}  // namespace blender::nodes::node_shader_bsdf_glass_cc

/* node type definition */
void register_node_type_sh_bsdf_glass()
{
  namespace file_ns = blender::nodes::node_shader_bsdf_glass_cc;

  static bNodeType ntype;

  sh_node_type_base(&ntype, SH_NODE_BSDF_GLASS, "Glass BSDF", NODE_CLASS_SHADER);
  ntype.declare = file_ns::node_declare;
  ntype.add_ui_poll = object_shader_nodes_poll;
  blender::bke::node_type_size_preset(&ntype, blender::bke::eNodeSizePreset::MIDDLE);
  ntype.initfunc = file_ns::node_shader_init_glass;
  ntype.gpu_fn = file_ns::node_shader_gpu_bsdf_glass;

  nodeRegisterType(&ntype);
}
