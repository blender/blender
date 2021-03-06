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
 */

#include "node_function_util.hh"

static bNodeSocketTemplate fn_node_group_instance_id_out[] = {
    {SOCK_STRING, N_("Identifier")},
    {-1, ""},
};

static void fn_node_group_instance_id_expand_in_mf_network(
    blender::nodes::NodeMFNetworkBuilder &builder)
{
  const blender::nodes::DNode &node = builder.dnode();
  std::string id = "/";
  for (const blender::nodes::DTreeContext *context = node.context();
       context->parent_node() != nullptr;
       context = context->parent_context()) {
    id = "/" + context->parent_node()->name() + id;
  }
  builder.construct_and_set_matching_fn<blender::fn::CustomMF_Constant<std::string>>(
      std::move(id));
}

void register_node_type_fn_group_instance_id()
{
  static bNodeType ntype;

  fn_node_type_base(&ntype, FN_NODE_GROUP_INSTANCE_ID, "Group Instance ID", 0, 0);
  node_type_socket_templates(&ntype, nullptr, fn_node_group_instance_id_out);
  ntype.expand_in_mf_network = fn_node_group_instance_id_expand_in_mf_network;
  nodeRegisterType(&ntype);
}
