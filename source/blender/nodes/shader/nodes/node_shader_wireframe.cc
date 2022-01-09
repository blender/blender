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

namespace blender::nodes::node_shader_wireframe_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Float>(N_("Size")).default_value(0.01f).min(0.0f).max(100.0f);
  b.add_output<decl::Float>(N_("Fac"));
}

static void node_shader_buts_wireframe(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiItemR(layout, ptr, "use_pixel_size", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, 0);
}

static int node_shader_gpu_wireframe(GPUMaterial *mat,
                                     bNode *node,
                                     bNodeExecData *UNUSED(execdata),
                                     GPUNodeStack *in,
                                     GPUNodeStack *out)
{
  GPU_material_flag_set(mat, GPU_MATFLAG_BARYCENTRIC);
  /* node->custom1 is use_pixel_size */
  if (node->custom1) {
    return GPU_stack_link(
        mat, node, "node_wireframe_screenspace", in, out, GPU_builtin(GPU_BARYCENTRIC_TEXCO));
  }

  return GPU_stack_link(mat,
                        node,
                        "node_wireframe",
                        in,
                        out,
                        GPU_builtin(GPU_BARYCENTRIC_TEXCO),
                        GPU_builtin(GPU_BARYCENTRIC_DIST));
}

}  // namespace blender::nodes::node_shader_wireframe_cc

/* node type definition */
void register_node_type_sh_wireframe()
{
  namespace file_ns = blender::nodes::node_shader_wireframe_cc;

  static bNodeType ntype;

  sh_node_type_base(&ntype, SH_NODE_WIREFRAME, "Wireframe", NODE_CLASS_INPUT);
  ntype.declare = file_ns::node_declare;
  ntype.draw_buttons = file_ns::node_shader_buts_wireframe;
  node_type_gpu(&ntype, file_ns::node_shader_gpu_wireframe);

  nodeRegisterType(&ntype);
}
