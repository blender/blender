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

/* **************** OUTPUT ******************** */

static bNodeSocketTemplate sh_node_attribute_out[] = {
    {SOCK_RGBA, N_("Color")},
    {SOCK_VECTOR, N_("Vector"), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
    {SOCK_FLOAT, N_("Fac"), 0.0f, 0.0f, 0.0f, 0.0f, -FLT_MAX, FLT_MAX, PROP_FACTOR},
    {-1, ""},
};

static void node_shader_init_attribute(bNodeTree *UNUSED(ntree), bNode *node)
{
  NodeShaderAttribute *attr = MEM_callocN(sizeof(NodeShaderAttribute), "NodeShaderAttribute");
  node->storage = attr;
}

static int node_shader_gpu_attribute(GPUMaterial *mat,
                                     bNode *node,
                                     bNodeExecData *UNUSED(execdata),
                                     GPUNodeStack *in,
                                     GPUNodeStack *out)
{
  NodeShaderAttribute *attr = node->storage;

  /* FIXME : if an attribute layer (like vertex color) has one of these names,
   * it will not work as expected. */
  if (strcmp(attr->name, "density") == 0) {
    return GPU_stack_link(
        mat, node, "node_attribute_volume_density", in, out, GPU_builtin(GPU_VOLUME_DENSITY));
  }
  else if (strcmp(attr->name, "color") == 0) {
    return GPU_stack_link(
        mat, node, "node_attribute_volume_color", in, out, GPU_builtin(GPU_VOLUME_DENSITY));
  }
  else if (strcmp(attr->name, "flame") == 0) {
    return GPU_stack_link(
        mat, node, "node_attribute_volume_flame", in, out, GPU_builtin(GPU_VOLUME_FLAME));
  }
  else if (strcmp(attr->name, "temperature") == 0) {
    return GPU_stack_link(mat,
                          node,
                          "node_attribute_volume_temperature",
                          in,
                          out,
                          GPU_builtin(GPU_VOLUME_FLAME),
                          GPU_builtin(GPU_VOLUME_TEMPERATURE));
  }
  else {
    GPUNodeLink *cd_attr = GPU_attribute(mat, CD_AUTO_FROM_NAME, attr->name);
    GPU_stack_link(mat, node, "node_attribute", in, out, cd_attr);

    /* for each output. */
    for (int i = 0; sh_node_attribute_out[i].type != -1; i++) {
      node_shader_gpu_bump_tex_coord(mat, node, &out[i].link);
    }

    return 1;
  }
}

/* node type definition */
void register_node_type_sh_attribute(void)
{
  static bNodeType ntype;

  sh_node_type_base(&ntype, SH_NODE_ATTRIBUTE, "Attribute", NODE_CLASS_INPUT, 0);
  node_type_socket_templates(&ntype, NULL, sh_node_attribute_out);
  node_type_init(&ntype, node_shader_init_attribute);
  node_type_storage(
      &ntype, "NodeShaderAttribute", node_free_standard_storage, node_copy_standard_storage);
  node_type_gpu(&ntype, node_shader_gpu_attribute);

  nodeRegisterType(&ntype);
}
