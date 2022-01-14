/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2005 Blender Foundation.
 * All rights reserved.
 */

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

static void node_shader_buts_bevel(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiItemR(layout, ptr, "samples", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
}

static void node_shader_init_bevel(bNodeTree *UNUSED(ntree), bNode *node)
{
  node->custom1 = 4; /* samples */
}

static int gpu_shader_bevel(GPUMaterial *mat,
                            bNode *node,
                            bNodeExecData *UNUSED(execdata),
                            GPUNodeStack *in,
                            GPUNodeStack *out)
{
  if (!in[1].link) {
    GPU_link(mat,
             "direction_transform_m4v3",
             GPU_builtin(GPU_VIEW_NORMAL),
             GPU_builtin(GPU_INVERSE_VIEW_MATRIX),
             &in[1].link);
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
  node_type_init(&ntype, file_ns::node_shader_init_bevel);
  node_type_gpu(&ntype, file_ns::gpu_shader_bevel);

  nodeRegisterType(&ntype);
}
