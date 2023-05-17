/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_function_util.hh"

#include "BLI_hash.h"

#include "UI_interface.h"
#include "UI_resources.h"

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

}  // namespace blender::nodes::node_fn_input_bool_cc

void register_node_type_fn_input_bool()
{
  namespace file_ns = blender::nodes::node_fn_input_bool_cc;

  static bNodeType ntype;

  fn_node_type_base(&ntype, FN_NODE_INPUT_BOOL, "Boolean", 0);
  ntype.declare = file_ns::node_declare;
  ntype.initfunc = file_ns::node_init;
  node_type_storage(
      &ntype, "NodeInputBool", node_free_standard_storage, node_copy_standard_storage);
  ntype.build_multi_function = file_ns::node_build_multi_function;
  ntype.draw_buttons = file_ns::node_layout;
  nodeRegisterType(&ntype);
}
