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

namespace blender::nodes {

static void sh_node_tex_checker_declare(NodeDeclarationBuilder &b)
{
  b.is_function_node();
  b.add_input<decl::Vector>("Vector").min(-10000.0f).max(10000.0f).implicit_field();
  b.add_input<decl::Color>("Color1").default_value({0.8f, 0.8f, 0.8f, 1.0f});
  b.add_input<decl::Color>("Color2").default_value({0.2f, 0.2f, 0.2f, 1.0f});
  b.add_input<decl::Float>("Scale")
      .min(-10000.0f)
      .max(10000.0f)
      .default_value(5.0f)
      .no_muted_links();
  b.add_output<decl::Color>("Color");
  b.add_output<decl::Float>("Fac");
};

}  // namespace blender::nodes

static void node_shader_init_tex_checker(bNodeTree *UNUSED(ntree), bNode *node)
{
  NodeTexChecker *tex = (NodeTexChecker *)MEM_callocN(sizeof(NodeTexChecker), "NodeTexChecker");
  BKE_texture_mapping_default(&tex->base.tex_mapping, TEXMAP_TYPE_POINT);
  BKE_texture_colormapping_default(&tex->base.color_mapping);

  node->storage = tex;
}

static int node_shader_gpu_tex_checker(GPUMaterial *mat,
                                       bNode *node,
                                       bNodeExecData *UNUSED(execdata),
                                       GPUNodeStack *in,
                                       GPUNodeStack *out)
{
  node_shader_gpu_default_tex_coord(mat, node, &in[0].link);
  node_shader_gpu_tex_mapping(mat, node, in, out);

  return GPU_stack_link(mat, node, "node_tex_checker", in, out);
}

/* node type definition */
void register_node_type_sh_tex_checker(void)
{
  static bNodeType ntype;

  sh_node_type_base(&ntype, SH_NODE_TEX_CHECKER, "Checker Texture", NODE_CLASS_TEXTURE, 0);
  ntype.declare = blender::nodes::sh_node_tex_checker_declare;
  node_type_init(&ntype, node_shader_init_tex_checker);
  node_type_storage(
      &ntype, "NodeTexChecker", node_free_standard_storage, node_copy_standard_storage);
  node_type_gpu(&ntype, node_shader_gpu_tex_checker);

  nodeRegisterType(&ntype);
}
