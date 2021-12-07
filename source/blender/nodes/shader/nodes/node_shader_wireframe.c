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

#include "../node_shader_util.h"

/* **************** Wireframe ******************** */
static bNodeSocketTemplate sh_node_wireframe_in[] = {
    {SOCK_FLOAT, N_("Size"), 0.01f, 0.0f, 0.0f, 0.0f, 0.0f, 100.0f},
    {-1, ""},
};

static bNodeSocketTemplate sh_node_wireframe_out[] = {
    {SOCK_FLOAT, N_("Fac"), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, PROP_FACTOR},
    {-1, ""},
};

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

/* node type definition */
void register_node_type_sh_wireframe(void)
{
  static bNodeType ntype;

  sh_node_type_base(&ntype, SH_NODE_WIREFRAME, "Wireframe", NODE_CLASS_INPUT, 0);
  node_type_socket_templates(&ntype, sh_node_wireframe_in, sh_node_wireframe_out);
  node_type_init(&ntype, NULL);
  node_type_storage(&ntype, "", NULL, NULL);
  node_type_gpu(&ntype, node_shader_gpu_wireframe);

  nodeRegisterType(&ntype);
}
