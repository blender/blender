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

#include "UI_interface.h"
#include "UI_resources.h"

static bNodeSocketTemplate fn_node_input_string_out[] = {
    {SOCK_STRING, N_("String")},
    {-1, ""},
};

static void fn_node_input_string_layout(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiItemR(layout, ptr, "string", 0, "", ICON_NONE);
}

static void fn_node_input_string_expand_in_mf_network(
    blender::nodes::NodeMFNetworkBuilder &builder)
{
  bNode &bnode = builder.bnode();
  NodeInputString *node_storage = static_cast<NodeInputString *>(bnode.storage);
  std::string string = std::string((node_storage->string) ? node_storage->string : "");

  builder.construct_and_set_matching_fn<blender::fn::CustomMF_Constant<std::string>>(string);
}

static void fn_node_input_string_init(bNodeTree *UNUSED(ntree), bNode *node)
{
  node->storage = MEM_callocN(sizeof(NodeInputString), __func__);
}

static void fn_node_input_string_free(bNode *node)
{
  NodeInputString *storage = (NodeInputString *)node->storage;
  if (storage == nullptr) {
    return;
  }
  if (storage->string != nullptr) {
    MEM_freeN(storage->string);
  }
  MEM_freeN(storage);
}

static void fn_node_string_copy(bNodeTree *UNUSED(dest_ntree),
                                bNode *dest_node,
                                const bNode *src_node)
{
  NodeInputString *source_storage = (NodeInputString *)src_node->storage;
  NodeInputString *destination_storage = (NodeInputString *)MEM_dupallocN(source_storage);

  if (source_storage->string) {
    destination_storage->string = (char *)MEM_dupallocN(source_storage->string);
  }

  dest_node->storage = destination_storage;
}

void register_node_type_fn_input_string()
{
  static bNodeType ntype;

  fn_node_type_base(&ntype, FN_NODE_INPUT_STRING, "String", NODE_CLASS_INPUT, 0);
  node_type_socket_templates(&ntype, nullptr, fn_node_input_string_out);
  node_type_init(&ntype, fn_node_input_string_init);
  node_type_storage(&ntype, "NodeInputString", fn_node_input_string_free, fn_node_string_copy);
  ntype.expand_in_mf_network = fn_node_input_string_expand_in_mf_network;
  ntype.draw_buttons = fn_node_input_string_layout;
  nodeRegisterType(&ntype);
}
