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

namespace blender::nodes {

static void fn_node_input_int_declare(NodeDeclarationBuilder &b)
{
  b.add_output<decl::Int>("Integer");
};

static void fn_node_input_int_layout(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiLayout *col = uiLayoutColumn(layout, true);
  uiItemR(col, ptr, "integer", UI_ITEM_R_EXPAND, "", ICON_NONE);
}

static void fn_node_input_int_build_multi_function(NodeMultiFunctionBuilder &builder)
{
  bNode &bnode = builder.node();
  NodeInputInt *node_storage = static_cast<NodeInputInt *>(bnode.storage);
  builder.construct_and_set_matching_fn<fn::CustomMF_Constant<int>>(node_storage->integer);
}

static void fn_node_input_int_init(bNodeTree *UNUSED(ntree), bNode *node)
{
  NodeInputInt *data = (NodeInputInt *)MEM_callocN(sizeof(NodeInputInt), __func__);
  node->storage = data;
}

}  // namespace blender::nodes

void register_node_type_fn_input_int()
{
  static bNodeType ntype;

  fn_node_type_base(&ntype, FN_NODE_INPUT_INT, "Integer", 0, 0);
  ntype.declare = blender::nodes::fn_node_input_int_declare;
  node_type_init(&ntype, blender::nodes::fn_node_input_int_init);
  node_type_storage(
      &ntype, "NodeInputInt", node_free_standard_storage, node_copy_standard_storage);
  ntype.build_multi_function = blender::nodes::fn_node_input_int_build_multi_function;
  ntype.draw_buttons = blender::nodes::fn_node_input_int_layout;
  nodeRegisterType(&ntype);
}
