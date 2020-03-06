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

/** \file
 * \ingroup shdnodes
 */

#include "node_shader_util.h"

/* **************** Map Range ******************** */
static bNodeSocketTemplate sh_node_map_range_in[] = {
    {SOCK_FLOAT, N_("Value"), 1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 1.0f, PROP_NONE},
    {SOCK_FLOAT, N_("From Min"), 0.0f, 1.0f, 1.0f, 1.0f, -10000.0f, 10000.0f, PROP_NONE},
    {SOCK_FLOAT, N_("From Max"), 1.0f, 1.0f, 1.0f, 1.0f, -10000.0f, 10000.0f, PROP_NONE},
    {SOCK_FLOAT, N_("To Min"), 0.0f, 1.0f, 1.0f, 1.0f, -10000.0f, 10000.0f, PROP_NONE},
    {SOCK_FLOAT, N_("To Max"), 1.0f, 1.0f, 1.0f, 1.0f, -10000.0f, 10000.0f, PROP_NONE},
    {SOCK_FLOAT, N_("Steps"), 4.0f, 1.0f, 1.0f, 1.0f, 0.0f, 10000.0f, PROP_NONE},
    {-1, ""},
};
static bNodeSocketTemplate sh_node_map_range_out[] = {
    {SOCK_FLOAT, N_("Result")},
    {-1, ""},
};

static void node_shader_update_map_range(bNodeTree *UNUSED(ntree), bNode *node)
{
  bNodeSocket *sockSteps = nodeFindSocket(node, SOCK_IN, "Steps");
  nodeSetSocketAvailability(sockSteps, node->custom2 == NODE_MAP_RANGE_STEPPED);
}

static void node_shader_init_map_range(bNodeTree *UNUSED(ntree), bNode *node)
{
  node->custom1 = true;                  /* use_clamp */
  node->custom2 = NODE_MAP_RANGE_LINEAR; /* interpolation */
}

static int gpu_shader_map_range(GPUMaterial *mat,
                                bNode *node,
                                bNodeExecData *UNUSED(execdata),
                                GPUNodeStack *in,
                                GPUNodeStack *out)
{
  static const char *names[] = {
      [NODE_MAP_RANGE_LINEAR] = "map_range_linear",
      [NODE_MAP_RANGE_STEPPED] = "map_range_stepped",
      [NODE_MAP_RANGE_SMOOTHSTEP] = "map_range_smoothstep",
      [NODE_MAP_RANGE_SMOOTHERSTEP] = "map_range_smootherstep",
  };

  int ret = 0;
  if (node->custom2 < ARRAY_SIZE(names) && names[node->custom2]) {
    ret = GPU_stack_link(mat, node, names[node->custom2], in, out);
  }
  else {
    ret = GPU_stack_link(mat, node, "map_range_linear", in, out);
  }
  if (ret && node->custom1 &&
      !ELEM(node->custom2, NODE_MAP_RANGE_SMOOTHSTEP, NODE_MAP_RANGE_SMOOTHERSTEP)) {
    GPU_link(mat, "clamp_range", out[0].link, in[3].link, in[4].link, &out[0].link);
  }
  return ret;
}

void register_node_type_sh_map_range(void)
{
  static bNodeType ntype;

  sh_node_type_base(&ntype, SH_NODE_MAP_RANGE, "Map Range", NODE_CLASS_CONVERTOR, 0);
  node_type_socket_templates(&ntype, sh_node_map_range_in, sh_node_map_range_out);
  node_type_init(&ntype, node_shader_init_map_range);
  node_type_update(&ntype, node_shader_update_map_range);
  node_type_gpu(&ntype, gpu_shader_map_range);

  nodeRegisterType(&ntype);
}
