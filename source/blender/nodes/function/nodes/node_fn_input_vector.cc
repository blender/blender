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

#include "BLI_hash.h"

#include "UI_interface.h"
#include "UI_resources.h"

static bNodeSocketTemplate fn_node_input_vector_out[] = {
    {SOCK_VECTOR, N_("Vector")},
    {-1, ""},
};

static void fn_node_input_vector_layout(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiLayout *col = uiLayoutColumn(layout, true);
  uiItemR(col, ptr, "vector", UI_ITEM_R_EXPAND, "", ICON_NONE);
}

static void fn_node_vector_input_expand_in_mf_network(
    blender::nodes::NodeMFNetworkBuilder &builder)
{
  bNode &bnode = builder.bnode();
  NodeInputVector *node_storage = static_cast<NodeInputVector *>(bnode.storage);
  blender::float3 vector(node_storage->vector);

  builder.construct_and_set_matching_fn<blender::fn::CustomMF_Constant<blender::float3>>(vector);
}

static void fn_node_input_vector_init(bNodeTree *UNUSED(ntree), bNode *node)
{
  NodeInputVector *data = (NodeInputVector *)MEM_callocN(sizeof(NodeInputVector),
                                                         "input vector node");
  node->storage = data;
}

void register_node_type_fn_input_vector()
{
  static bNodeType ntype;

  fn_node_type_base(&ntype, FN_NODE_INPUT_VECTOR, "Vector", 0, 0);
  node_type_socket_templates(&ntype, nullptr, fn_node_input_vector_out);
  node_type_init(&ntype, fn_node_input_vector_init);
  node_type_storage(
      &ntype, "NodeInputVector", node_free_standard_storage, node_copy_standard_storage);
  ntype.expand_in_mf_network = fn_node_vector_input_expand_in_mf_network;
  ntype.draw_buttons = fn_node_input_vector_layout;
  nodeRegisterType(&ntype);
}
