/* SPDX-FileCopyrightText: 2005 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_shader_util.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"

namespace blender::nodes::node_shader_bsdf_glossy_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>("Color").default_value({0.8f, 0.8f, 0.8f, 1.0f});
  b.add_input<decl::Float>("Roughness")
      .default_value(0.5f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR);
  b.add_input<decl::Float>("Anisotropy").default_value(0.0f).min(-1.0f).max(1.0f);
  b.add_input<decl::Float>("Rotation")
      .default_value(0.0f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR);
  b.add_input<decl::Vector>("Normal").hide_value();
  b.add_input<decl::Vector>("Tangent").hide_value();
  b.add_input<decl::Float>("Weight").unavailable();
  b.add_output<decl::Shader>("BSDF");
}

static void node_shader_buts_glossy(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiItemR(layout, ptr, "distribution", UI_ITEM_R_SPLIT_EMPTY_NAME, "", ICON_NONE);
}

static void node_shader_init_glossy(bNodeTree * /*ntree*/, bNode *node)
{
  node->custom1 = SHD_GLOSSY_GGX;
}

static int node_shader_gpu_bsdf_glossy(GPUMaterial *mat,
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

  return GPU_stack_link(mat, node, "node_bsdf_glossy", in, out, GPU_constant(&use_multi_scatter));
}

}  // namespace blender::nodes::node_shader_bsdf_glossy_cc

/* node type definition */
void register_node_type_sh_bsdf_glossy()
{
  namespace file_ns = blender::nodes::node_shader_bsdf_glossy_cc;

  static bNodeType ntype;

  sh_node_type_base(&ntype, SH_NODE_BSDF_GLOSSY, "Glossy BSDF", NODE_CLASS_SHADER);
  ntype.declare = file_ns::node_declare;
  ntype.add_ui_poll = object_shader_nodes_poll;
  ntype.draw_buttons = file_ns::node_shader_buts_glossy;
  blender::bke::node_type_size_preset(&ntype, blender::bke::eNodeSizePreset::MIDDLE);
  ntype.initfunc = file_ns::node_shader_init_glossy;
  ntype.gpu_fn = file_ns::node_shader_gpu_bsdf_glossy;

  nodeRegisterType(&ntype);

  /* Needed to preserve API compatibility with older versions which had separate
   * Glossy and Anisotropic nodes. */
  nodeRegisterAlias(&ntype, "ShaderNodeBsdfGlossy");
}
