/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_function_util.hh"

#include "BLI_hash.h"

#include "UI_interface.hh"
#include "UI_resources.hh"

namespace blender::nodes::node_fn_input_vector_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_output<decl::Vector>("Vector");
}

static void node_layout(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiLayout *col = uiLayoutColumn(layout, true);
  uiItemR(col, ptr, "vector", UI_ITEM_R_EXPAND, "", ICON_NONE);
}

static void node_build_multi_function(NodeMultiFunctionBuilder &builder)
{
  const bNode &bnode = builder.node();
  NodeInputVector *node_storage = static_cast<NodeInputVector *>(bnode.storage);
  float3 vector(node_storage->vector);
  builder.construct_and_set_matching_fn<mf::CustomMF_Constant<float3>>(vector);
}

static void node_init(bNodeTree * /*tree*/, bNode *node)
{
  NodeInputVector *data = MEM_cnew<NodeInputVector>(__func__);
  node->storage = data;
}

static void node_register()
{
  static bNodeType ntype;

  fn_node_type_base(&ntype, FN_NODE_INPUT_VECTOR, "Vector", 0);
  ntype.declare = node_declare;
  ntype.initfunc = node_init;
  node_type_storage(
      &ntype, "NodeInputVector", node_free_standard_storage, node_copy_standard_storage);
  ntype.build_multi_function = node_build_multi_function;
  ntype.draw_buttons = node_layout;
  nodeRegisterType(&ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_fn_input_vector_cc
