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

namespace blender::nodes::node_fn_input_bool_cc {

static void fn_node_input_bool_declare(NodeDeclarationBuilder &b)
{
  b.add_output<decl::Bool>(N_("Boolean"));
}

static void fn_node_input_bool_layout(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiLayout *col = uiLayoutColumn(layout, true);
  uiItemR(col, ptr, "boolean", UI_ITEM_R_EXPAND, IFACE_("Value"), ICON_NONE);
}

static void fn_node_input_bool_build_multi_function(NodeMultiFunctionBuilder &builder)
{
  bNode &bnode = builder.node();
  NodeInputBool *node_storage = static_cast<NodeInputBool *>(bnode.storage);
  builder.construct_and_set_matching_fn<fn::CustomMF_Constant<bool>>(node_storage->boolean);
}

static void fn_node_input_bool_init(bNodeTree *UNUSED(ntree), bNode *node)
{
  NodeInputBool *data = MEM_cnew<NodeInputBool>(__func__);
  node->storage = data;
}

}  // namespace blender::nodes::node_fn_input_bool_cc

void register_node_type_fn_input_bool()
{
  namespace file_ns = blender::nodes::node_fn_input_bool_cc;

  static bNodeType ntype;

  fn_node_type_base(&ntype, FN_NODE_INPUT_BOOL, "Boolean", 0);
  ntype.declare = file_ns::fn_node_input_bool_declare;
  node_type_init(&ntype, file_ns::fn_node_input_bool_init);
  node_type_storage(
      &ntype, "NodeInputBool", node_free_standard_storage, node_copy_standard_storage);
  ntype.build_multi_function = file_ns::fn_node_input_bool_build_multi_function;
  ntype.draw_buttons = file_ns::fn_node_input_bool_layout;
  nodeRegisterType(&ntype);
}
