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

namespace blender::nodes::node_shader_shader_to_rgb_cc {

/* **************** OUTPUT ******************** */

static bNodeSocketTemplate sh_node_shadertorgb_in[] = {
    {SOCK_SHADER, N_("Shader")},
    {-1, ""},
};

static bNodeSocketTemplate sh_node_shadertorgb_out[] = {
    {SOCK_RGBA, N_("Color")},
    {SOCK_FLOAT, N_("Alpha")},
    {-1, ""},
};

static int node_shader_gpu_shadertorgb(GPUMaterial *mat,
                                       bNode *node,
                                       bNodeExecData *UNUSED(execdata),
                                       GPUNodeStack *in,
                                       GPUNodeStack *out)
{
  /* Because node_shader_to_rgba is using fallback_cubemap()
   * we need to tag material as glossy. */
  GPU_material_flag_set(mat, GPU_MATFLAG_GLOSSY);

  return GPU_stack_link(mat, node, "node_shader_to_rgba", in, out);
}

}  // namespace blender::nodes::node_shader_shader_to_rgb_cc

/* node type definition */
void register_node_type_sh_shadertorgb()
{
  namespace file_ns = blender::nodes::node_shader_shader_to_rgb_cc;

  static bNodeType ntype;

  sh_node_type_base(&ntype, SH_NODE_SHADERTORGB, "Shader to RGB", NODE_CLASS_CONVERTER, 0);
  node_type_socket_templates(
      &ntype, file_ns::sh_node_shadertorgb_in, file_ns::sh_node_shadertorgb_out);
  node_type_gpu(&ntype, file_ns::node_shader_gpu_shadertorgb);

  nodeRegisterType(&ntype);
}
