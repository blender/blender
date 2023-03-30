/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2005 Blender Foundation */

#include "node_shader_util.hh"

#include "UI_interface.h"
#include "UI_resources.h"

namespace blender::nodes::node_shader_bevel_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Float>(N_("Radius")).default_value(0.05f).min(0.0f).max(1000.0f);
  b.add_input<decl::Vector>(N_("Normal")).hide_value();
  b.add_output<decl::Vector>(N_("Normal"));
}

static void node_shader_buts_bevel(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiItemR(layout, ptr, "samples", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
}

static void node_shader_init_bevel(bNodeTree * /*ntree*/, bNode *node)
{
  node->custom1 = 4; /* samples */
}

static int gpu_shader_bevel(GPUMaterial *mat,
                            bNode *node,
                            bNodeExecData * /*execdata*/,
                            GPUNodeStack *in,
                            GPUNodeStack *out)
{
  if (!in[1].link) {
    GPU_link(mat, "world_normals_get", &in[1].link);
  }

  return GPU_stack_link(mat, node, "node_bevel", in, out);
}

}  // namespace blender::nodes::node_shader_bevel_cc

/* node type definition */
void register_node_type_sh_bevel()
{
  namespace file_ns = blender::nodes::node_shader_bevel_cc;

  static bNodeType ntype;

  sh_node_type_base(&ntype, SH_NODE_BEVEL, "Bevel", NODE_CLASS_INPUT);
  ntype.declare = file_ns::node_declare;
  ntype.draw_buttons = file_ns::node_shader_buts_bevel;
  ntype.initfunc = file_ns::node_shader_init_bevel;
  ntype.gpu_fn = file_ns::gpu_shader_bevel;

  nodeRegisterType(&ntype);
}
