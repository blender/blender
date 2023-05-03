/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2005 Blender Foundation */

#include "node_shader_util.hh"

#include "UI_interface.h"
#include "UI_resources.h"

namespace blender::nodes::node_shader_bsdf_anisotropic_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>(N_("Color")).default_value({0.8f, 0.8f, 0.8f, 1.0f});
  b.add_input<decl::Float>(N_("Roughness"))
      .default_value(0.5f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR);
  b.add_input<decl::Float>(N_("Anisotropy")).default_value(0.5f).min(-1.0f).max(1.0f);
  b.add_input<decl::Float>(N_("Rotation"))
      .default_value(0.0f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR);
  b.add_input<decl::Vector>(N_("Normal")).hide_value();
  b.add_input<decl::Vector>(N_("Tangent")).hide_value();
  b.add_input<decl::Float>(N_("Weight")).unavailable();
  b.add_output<decl::Shader>(N_("BSDF"));
}

static void node_shader_buts_anisotropic(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiItemR(layout, ptr, "distribution", UI_ITEM_R_SPLIT_EMPTY_NAME, "", ICON_NONE);
}

static void node_shader_init_anisotropic(bNodeTree * /*ntree*/, bNode *node)
{
  node->custom1 = SHD_GLOSSY_GGX;
}

static int node_shader_gpu_bsdf_anisotropic(GPUMaterial *mat,
                                            bNode *node,
                                            bNodeExecData * /*execdata*/,
                                            GPUNodeStack *in,
                                            GPUNodeStack *out)
{
  if (!in[4].link) {
    GPU_link(mat, "world_normals_get", &in[4].link);
  }

  GPU_material_flag_set(mat, GPU_MATFLAG_GLOSSY);

  float use_multi_scatter = (node->custom1 == SHD_GLOSSY_MULTI_GGX) ? 1.0f : 0.0f;

  return GPU_stack_link(
      mat, node, "node_bsdf_anisotropic", in, out, GPU_constant(&use_multi_scatter));
}

}  // namespace blender::nodes::node_shader_bsdf_anisotropic_cc

/* node type definition */
void register_node_type_sh_bsdf_anisotropic()
{
  namespace file_ns = blender::nodes::node_shader_bsdf_anisotropic_cc;

  static bNodeType ntype;

  sh_node_type_base(&ntype, SH_NODE_BSDF_ANISOTROPIC, "Anisotropic BSDF", NODE_CLASS_SHADER);
  ntype.declare = file_ns::node_declare;
  ntype.add_ui_poll = object_cycles_shader_nodes_poll;
  ntype.draw_buttons = file_ns::node_shader_buts_anisotropic;
  node_type_size_preset(&ntype, NODE_SIZE_MIDDLE);
  ntype.initfunc = file_ns::node_shader_init_anisotropic;
  ntype.gpu_fn = file_ns::node_shader_gpu_bsdf_anisotropic;

  nodeRegisterType(&ntype);
}
