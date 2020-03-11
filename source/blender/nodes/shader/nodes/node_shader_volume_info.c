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

static bNodeSocketTemplate sh_node_volume_info_out[] = {
    {SOCK_RGBA, N_("Color")},
    {SOCK_FLOAT, N_("Density")},
    {SOCK_FLOAT, N_("Flame")},
    {SOCK_FLOAT, N_("Temperature")},
    {-1, ""},
};

static int node_shader_gpu_volume_info(GPUMaterial *mat,
                                       bNode *UNUSED(node),
                                       bNodeExecData *UNUSED(execdata),
                                       GPUNodeStack *UNUSED(in),
                                       GPUNodeStack *out)
{
  if (out[0].hasoutput) {
    out[0].link = GPU_volume_grid(mat, "color");
  }
  if (out[1].hasoutput) {
    out[1].link = GPU_volume_grid(mat, "density");
  }
  if (out[2].hasoutput) {
    out[2].link = GPU_volume_grid(mat, "flame");
  }
  if (out[3].hasoutput) {
    out[3].link = GPU_volume_grid(mat, "temperature");
  }

  return true;
}

void register_node_type_sh_volume_info(void)
{
  static bNodeType ntype;

  sh_node_type_base(&ntype, SH_NODE_VOLUME_INFO, "Volume Info", NODE_CLASS_INPUT, 0);
  node_type_socket_templates(&ntype, NULL, sh_node_volume_info_out);
  node_type_gpu(&ntype, node_shader_gpu_volume_info);

  nodeRegisterType(&ntype);
}
