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

/* **************** MAPPING  ******************** */
static bNodeSocketTemplate sh_node_mapping_in[] = {
    {SOCK_VECTOR, N_("Vector"), 0.0f, 0.0f, 0.0f, 1.0f, -FLT_MAX, FLT_MAX, PROP_NONE},
    {SOCK_VECTOR, N_("Location"), 0.0f, 0.0f, 0.0f, 1.0f, -FLT_MAX, FLT_MAX, PROP_TRANSLATION},
    {SOCK_VECTOR, N_("Rotation"), 0.0f, 0.0f, 0.0f, 1.0f, -FLT_MAX, FLT_MAX, PROP_EULER},
    {SOCK_VECTOR, N_("Scale"), 1.0f, 1.0f, 1.0f, 1.0f, -FLT_MAX, FLT_MAX, PROP_XYZ},
    {-1, ""},
};

static bNodeSocketTemplate sh_node_mapping_out[] = {
    {SOCK_VECTOR, N_("Vector")},
    {-1, ""},
};

static int gpu_shader_mapping(GPUMaterial *mat,
                              bNode *node,
                              bNodeExecData *UNUSED(execdata),
                              GPUNodeStack *in,
                              GPUNodeStack *out)
{
  static const char *names[] = {
      [NODE_MAPPING_TYPE_POINT] = "mapping_point",
      [NODE_MAPPING_TYPE_TEXTURE] = "mapping_texture",
      [NODE_MAPPING_TYPE_VECTOR] = "mapping_vector",
      [NODE_MAPPING_TYPE_NORMAL] = "mapping_normal",
  };

  if (node->custom1 < ARRAY_SIZE(names) && names[node->custom1]) {
    return GPU_stack_link(mat, node, names[node->custom1], in, out);
  }

  return 0;
}

static void node_shader_update_mapping(bNodeTree *UNUSED(ntree), bNode *node)
{
  bNodeSocket *sock = nodeFindSocket(node, SOCK_IN, "Location");
  nodeSetSocketAvailability(
      sock, ELEM(node->custom1, NODE_MAPPING_TYPE_POINT, NODE_MAPPING_TYPE_TEXTURE));
}

void register_node_type_sh_mapping(void)
{
  static bNodeType ntype;

  sh_node_type_base(&ntype, SH_NODE_MAPPING, "Mapping", NODE_CLASS_OP_VECTOR, 0);
  node_type_socket_templates(&ntype, sh_node_mapping_in, sh_node_mapping_out);
  node_type_gpu(&ntype, gpu_shader_mapping);
  node_type_update(&ntype, node_shader_update_mapping);

  nodeRegisterType(&ntype);
}
