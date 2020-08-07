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
 * The Original Code is Copyright (C) 2013 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup shdnodes
 */

#include "../node_shader_util.h"

/* **************** Vector Rotate ******************** */
static bNodeSocketTemplate sh_node_vector_rotate_in[] = {
    {SOCK_VECTOR, N_("Vector"), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, PROP_NONE, SOCK_HIDE_VALUE},
    {SOCK_VECTOR, N_("Center"), 0.0f, 0.0f, 0.0f, 1.0f, -FLT_MAX, FLT_MAX, PROP_NONE},
    {SOCK_VECTOR, N_("Axis"), 0.0f, 0.0f, 1.0f, 0.0f, -1.0f, 1.0f, PROP_NONE, PROP_NONE},
    {SOCK_FLOAT, N_("Angle"), 0.0f, 0.0f, 0.0f, 1.0f, -FLT_MAX, FLT_MAX, PROP_ANGLE, PROP_NONE},
    {SOCK_VECTOR, N_("Rotation"), 0.0f, 0.0f, 0.0f, 1.0f, -FLT_MAX, FLT_MAX, PROP_EULER},
    {-1, ""}};

static bNodeSocketTemplate sh_node_vector_rotate_out[] = {{SOCK_VECTOR, N_("Vector")}, {-1, ""}};

static int gpu_shader_vector_rotate(GPUMaterial *mat,
                                    bNode *node,
                                    bNodeExecData *UNUSED(execdata),
                                    GPUNodeStack *in,
                                    GPUNodeStack *out)
{

  static const char *names[] = {
      [NODE_VECTOR_ROTATE_TYPE_AXIS] = "node_vector_rotate_axis_angle",
      [NODE_VECTOR_ROTATE_TYPE_AXIS_X] = "node_vector_rotate_axis_x",
      [NODE_VECTOR_ROTATE_TYPE_AXIS_Y] = "node_vector_rotate_axis_y",
      [NODE_VECTOR_ROTATE_TYPE_AXIS_Z] = "node_vector_rotate_axis_z",
      [NODE_VECTOR_ROTATE_TYPE_EULER_XYZ] = "node_vector_rotate_euler_xyz",
  };

  if (node->custom1 < ARRAY_SIZE(names) && names[node->custom1]) {
    float invert = (node->custom2) ? -1.0 : 1.0;
    return GPU_stack_link(mat, node, names[node->custom1], in, out, GPU_constant(&invert));
  }

  return 0;
}

static void node_shader_update_vector_rotate(bNodeTree *UNUSED(ntree), bNode *node)
{
  bNodeSocket *sock_rotation = nodeFindSocket(node, SOCK_IN, "Rotation");
  nodeSetSocketAvailability(sock_rotation, ELEM(node->custom1, NODE_VECTOR_ROTATE_TYPE_EULER_XYZ));
  bNodeSocket *sock_axis = nodeFindSocket(node, SOCK_IN, "Axis");
  nodeSetSocketAvailability(sock_axis, ELEM(node->custom1, NODE_VECTOR_ROTATE_TYPE_AXIS));
  bNodeSocket *sock_angle = nodeFindSocket(node, SOCK_IN, "Angle");
  nodeSetSocketAvailability(sock_angle, !ELEM(node->custom1, NODE_VECTOR_ROTATE_TYPE_EULER_XYZ));
}

void register_node_type_sh_vector_rotate(void)
{
  static bNodeType ntype;

  sh_node_type_base(&ntype, SH_NODE_VECTOR_ROTATE, "Vector Rotate", NODE_CLASS_OP_VECTOR, 0);
  node_type_socket_templates(&ntype, sh_node_vector_rotate_in, sh_node_vector_rotate_out);
  node_type_gpu(&ntype, gpu_shader_vector_rotate);
  node_type_update(&ntype, node_shader_update_vector_rotate);

  nodeRegisterType(&ntype);
}
