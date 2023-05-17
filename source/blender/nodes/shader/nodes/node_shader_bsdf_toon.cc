/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2005 Blender Foundation */

#include "node_shader_util.hh"

#include "UI_interface.h"
#include "UI_resources.h"

namespace blender::nodes::node_shader_bsdf_toon_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>("Color").default_value({0.8f, 0.8f, 0.8f, 1.0f});
  b.add_input<decl::Float>("Size").default_value(0.5f).min(0.0f).max(1.0f).subtype(PROP_FACTOR);
  b.add_input<decl::Float>("Smooth").default_value(0.0f).min(0.0f).max(1.0f).subtype(PROP_FACTOR);
  b.add_input<decl::Vector>("Normal").hide_value();
  b.add_input<decl::Float>("Weight").unavailable();
  b.add_output<decl::Shader>("BSDF");
}

static void node_shader_buts_toon(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiItemR(layout, ptr, "component", UI_ITEM_R_SPLIT_EMPTY_NAME, "", ICON_NONE);
}

static int node_shader_gpu_bsdf_toon(GPUMaterial *mat,
                                     bNode *node,
                                     bNodeExecData * /*execdata*/,
                                     GPUNodeStack *in,
                                     GPUNodeStack *out)
{
  if (!in[3].link) {
    GPU_link(mat, "world_normals_get", &in[3].link);
  }

  GPU_material_flag_set(mat, GPU_MATFLAG_DIFFUSE);

  return GPU_stack_link(mat, node, "node_bsdf_toon", in, out);
}

}  // namespace blender::nodes::node_shader_bsdf_toon_cc

/* node type definition */
void register_node_type_sh_bsdf_toon()
{
  namespace file_ns = blender::nodes::node_shader_bsdf_toon_cc;

  static bNodeType ntype;

  sh_node_type_base(&ntype, SH_NODE_BSDF_TOON, "Toon BSDF", NODE_CLASS_SHADER);
  ntype.declare = file_ns::node_declare;
  ntype.add_ui_poll = object_cycles_shader_nodes_poll;
  ntype.draw_buttons = file_ns::node_shader_buts_toon;
  blender::bke::node_type_size_preset(&ntype, blender::bke::eNodeSizePreset::MIDDLE);
  ntype.gpu_fn = file_ns::node_shader_gpu_bsdf_toon;

  nodeRegisterType(&ntype);
}
