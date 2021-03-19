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

/* **************** WHITE NOISE **************** */

static bNodeSocketTemplate sh_node_tex_white_noise_in[] = {
    {SOCK_VECTOR, N_("Vector"), 0.0f, 0.0f, 0.0f, 0.0f, -10000.0f, 10000.0f, PROP_NONE},
    {SOCK_FLOAT, N_("W"), 0.0f, 0.0f, 0.0f, 0.0f, -10000.0f, 10000.0f, PROP_NONE},
    {-1, ""}};

static bNodeSocketTemplate sh_node_tex_white_noise_out[] = {
    {SOCK_FLOAT, N_("Value")},
    {SOCK_RGBA, N_("Color")},
    {-1, ""},
};

static void node_shader_init_tex_white_noise(bNodeTree *UNUSED(ntree), bNode *node)
{
  node->custom1 = 3;
}

static int gpu_shader_tex_white_noise(GPUMaterial *mat,
                                      bNode *node,
                                      bNodeExecData *UNUSED(execdata),
                                      GPUNodeStack *in,
                                      GPUNodeStack *out)
{
  static const char *names[] = {
      "",
      "node_white_noise_1d",
      "node_white_noise_2d",
      "node_white_noise_3d",
      "node_white_noise_4d",
  };

  if (node->custom1 < ARRAY_SIZE(names) && names[node->custom1]) {
    return GPU_stack_link(mat, node, names[node->custom1], in, out);
  }

  return 0;
}

static void node_shader_update_tex_white_noise(bNodeTree *UNUSED(ntree), bNode *node)
{
  bNodeSocket *sockVector = nodeFindSocket(node, SOCK_IN, "Vector");
  bNodeSocket *sockW = nodeFindSocket(node, SOCK_IN, "W");

  nodeSetSocketAvailability(sockVector, node->custom1 != 1);
  nodeSetSocketAvailability(sockW, node->custom1 == 1 || node->custom1 == 4);
}

void register_node_type_sh_tex_white_noise(void)
{
  static bNodeType ntype;

  sh_node_type_base(&ntype, SH_NODE_TEX_WHITE_NOISE, "White Noise Texture", NODE_CLASS_TEXTURE, 0);
  node_type_socket_templates(&ntype, sh_node_tex_white_noise_in, sh_node_tex_white_noise_out);
  node_type_init(&ntype, node_shader_init_tex_white_noise);
  node_type_gpu(&ntype, gpu_shader_tex_white_noise);
  node_type_update(&ntype, node_shader_update_tex_white_noise);

  nodeRegisterType(&ntype);
}
