/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_function_util.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"

namespace blender::nodes::node_fn_input_bool_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_output<decl::Bool>("Boolean");
}

static void node_layout(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiLayout *col = uiLayoutColumn(layout, true);
  uiItemR(col, ptr, "boolean", UI_ITEM_R_EXPAND, IFACE_("Value"), ICON_NONE);
}

static void node_build_multi_function(NodeMultiFunctionBuilder &builder)
{
  const bNode &bnode = builder.node();
  NodeInputBool *node_storage = static_cast<NodeInputBool *>(bnode.storage);
  builder.construct_and_set_matching_fn<mf::CustomMF_Constant<bool>>(node_storage->boolean);
}

static void node_init(bNodeTree * /*tree*/, bNode *node)
{
  NodeInputBool *data = MEM_cnew<NodeInputBool>(__func__);
  node->storage = data;
}

static void node_register()
{
  static bNodeType ntype;

  fn_node_type_base(&ntype, FN_NODE_INPUT_BOOL, "Boolean", 0);
  ntype.declare = node_declare;
  ntype.initfunc = node_init;
  node_type_storage(
      &ntype, "NodeInputBool", node_free_standard_storage, node_copy_standard_storage);
  ntype.build_multi_function = node_build_multi_function;
  ntype.draw_buttons = node_layout;
  nodeRegisterType(&ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_fn_input_bool_cc
