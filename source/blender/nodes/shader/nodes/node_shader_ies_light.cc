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
 * The Original Code is Copyright (C) 2018 Blender Foundation.
 * All rights reserved.
 */

#include "../node_shader_util.h"

namespace blender::nodes::node_shader_ies_light_cc {

/* **************** IES Light ******************** */

static bNodeSocketTemplate sh_node_tex_ies_in[] = {
    {SOCK_VECTOR, N_("Vector"), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, PROP_NONE, SOCK_HIDE_VALUE},
    {SOCK_FLOAT, N_("Strength"), 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1000000.0f, PROP_NONE},
    {-1, ""},
};

static bNodeSocketTemplate sh_node_tex_ies_out[] = {
    {SOCK_FLOAT, N_("Fac")},
    {-1, ""},
};

static void node_shader_init_tex_ies(bNodeTree *UNUSED(ntree), bNode *node)
{
  NodeShaderTexIES *tex = MEM_cnew<NodeShaderTexIES>("NodeShaderIESLight");
  node->storage = tex;
}

}  // namespace blender::nodes::node_shader_ies_light_cc

/* node type definition */
void register_node_type_sh_tex_ies()
{
  namespace file_ns = blender::nodes::node_shader_ies_light_cc;

  static bNodeType ntype;

  sh_node_type_base(&ntype, SH_NODE_TEX_IES, "IES Texture", NODE_CLASS_TEXTURE, 0);
  node_type_socket_templates(&ntype, file_ns::sh_node_tex_ies_in, file_ns::sh_node_tex_ies_out);
  node_type_init(&ntype, file_ns::node_shader_init_tex_ies);
  node_type_storage(
      &ntype, "NodeShaderTexIES", node_free_standard_storage, node_copy_standard_storage);

  nodeRegisterType(&ntype);
}
