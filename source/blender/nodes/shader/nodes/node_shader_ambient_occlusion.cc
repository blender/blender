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

namespace blender::nodes::node_shader_ambient_occlusion_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>(N_("Color")).default_value({1.0f, 1.0f, 1.0f, 1.0f});
  b.add_input<decl::Float>(N_("Distance")).default_value(1.0f).min(0.0f).max(1000.0f);
  b.add_input<decl::Vector>(N_("Normal")).min(-1.0f).max(1.0f).hide_value();
  b.add_output<decl::Color>(N_("Color"));
  b.add_output<decl::Float>(N_("AO"));
}

static void node_shader_buts_ambient_occlusion(uiLayout *layout,
                                               bContext *UNUSED(C),
                                               PointerRNA *ptr)
{
  uiItemR(layout, ptr, "samples", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
  uiItemR(layout, ptr, "inside", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
  uiItemR(layout, ptr, "only_local", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
}

static int node_shader_gpu_ambient_occlusion(GPUMaterial *mat,
                                             bNode *node,
                                             bNodeExecData *UNUSED(execdata),
                                             GPUNodeStack *in,
                                             GPUNodeStack *out)
{
  if (!in[2].link) {
    GPU_link(mat, "world_normals_get", &in[2].link);
  }

  GPU_material_flag_set(mat, GPU_MATFLAG_DIFFUSE);

  float inverted = (node->custom2 & SHD_AO_INSIDE) ? 1.0f : 0.0f;
  float f_samples = divide_ceil_u(node->custom1, 4);

  return GPU_stack_link(mat,
                        node,
                        "node_ambient_occlusion",
                        in,
                        out,
                        GPU_constant(&inverted),
                        GPU_constant(&f_samples));
}

static void node_shader_init_ambient_occlusion(bNodeTree *UNUSED(ntree), bNode *node)
{
  node->custom1 = 16; /* samples */
  node->custom2 = 0;
}

}  // namespace blender::nodes::node_shader_ambient_occlusion_cc

/* node type definition */
void register_node_type_sh_ambient_occlusion()
{
  namespace file_ns = blender::nodes::node_shader_ambient_occlusion_cc;

  static bNodeType ntype;

  sh_node_type_base(&ntype, SH_NODE_AMBIENT_OCCLUSION, "Ambient Occlusion", NODE_CLASS_INPUT);
  ntype.declare = file_ns::node_declare;
  ntype.draw_buttons = file_ns::node_shader_buts_ambient_occlusion;
  node_type_init(&ntype, file_ns::node_shader_init_ambient_occlusion);
  node_type_gpu(&ntype, file_ns::node_shader_gpu_ambient_occlusion);

  nodeRegisterType(&ntype);
}
