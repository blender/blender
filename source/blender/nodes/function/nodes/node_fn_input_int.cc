/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_function_util.hh"

#include "BLI_hash.h"

#include "UI_interface.h"
#include "UI_resources.h"

namespace blender::nodes::node_fn_input_int_cc {

static void fn_node_input_int_declare(NodeDeclarationBuilder &b)
{
  b.add_output<decl::Int>(N_("Integer"));
}

static void fn_node_input_int_layout(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiLayout *col = uiLayoutColumn(layout, true);
  uiItemR(col, ptr, "integer", UI_ITEM_R_EXPAND, "", ICON_NONE);
}

static void fn_node_input_int_build_multi_function(NodeMultiFunctionBuilder &builder)
{
  const bNode &bnode = builder.node();
  NodeInputInt *node_storage = static_cast<NodeInputInt *>(bnode.storage);
  builder.construct_and_set_matching_fn<fn::CustomMF_Constant<int>>(node_storage->integer);
}

static void fn_node_input_int_init(bNodeTree *UNUSED(ntree), bNode *node)
{
  NodeInputInt *data = MEM_cnew<NodeInputInt>(__func__);
  node->storage = data;
}

}  // namespace blender::nodes::node_fn_input_int_cc

void register_node_type_fn_input_int()
{
  namespace file_ns = blender::nodes::node_fn_input_int_cc;

  static bNodeType ntype;

  fn_node_type_base(&ntype, FN_NODE_INPUT_INT, "Integer", 0);
  ntype.declare = file_ns::fn_node_input_int_declare;
  node_type_init(&ntype, file_ns::fn_node_input_int_init);
  node_type_storage(
      &ntype, "NodeInputInt", node_free_standard_storage, node_copy_standard_storage);
  ntype.build_multi_function = file_ns::fn_node_input_int_build_multi_function;
  ntype.draw_buttons = file_ns::fn_node_input_int_layout;
  nodeRegisterType(&ntype);
}
