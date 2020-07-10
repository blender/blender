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

static bNodeSocketTemplate sh_node_vertex_color_out[] = {
    {SOCK_RGBA, N_("Color")},
    {SOCK_FLOAT, N_("Alpha")},
    {-1, ""},
};

static void node_shader_init_vertex_color(bNodeTree *UNUSED(ntree), bNode *node)
{
  NodeShaderVertexColor *vertexColor = MEM_callocN(sizeof(NodeShaderVertexColor),
                                                   "NodeShaderVertexColor");
  node->storage = vertexColor;
}

static int node_shader_gpu_vertex_color(GPUMaterial *mat,
                                        bNode *node,
                                        bNodeExecData *UNUSED(execdata),
                                        GPUNodeStack *in,
                                        GPUNodeStack *out)
{
  NodeShaderVertexColor *vertexColor = (NodeShaderVertexColor *)node->storage;
  if (U.experimental.use_sculpt_vertex_colors) {
    GPUNodeLink *vertexColorLink = GPU_attribute(mat, CD_PROP_COLOR, vertexColor->layer_name);
    return GPU_stack_link(mat, node, "node_vertex_color", in, out, vertexColorLink);
  }
  GPUNodeLink *vertexColorLink = GPU_attribute(mat, CD_MCOL, vertexColor->layer_name);
  return GPU_stack_link(mat, node, "node_vertex_color", in, out, vertexColorLink);
}

void register_node_type_sh_vertex_color(void)
{
  static bNodeType ntype;

  sh_node_type_base(&ntype, SH_NODE_VERTEX_COLOR, "Vertex Color", NODE_CLASS_INPUT, 0);
  node_type_socket_templates(&ntype, NULL, sh_node_vertex_color_out);
  node_type_init(&ntype, node_shader_init_vertex_color);
  node_type_storage(
      &ntype, "NodeShaderVertexColor", node_free_standard_storage, node_copy_standard_storage);
  node_type_gpu(&ntype, node_shader_gpu_vertex_color);

  nodeRegisterType(&ntype);
}
