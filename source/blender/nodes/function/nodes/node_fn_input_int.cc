/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_function_util.hh"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "NOD_geometry_nodes_gizmos.hh"

namespace blender::nodes::node_fn_input_int_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_output<decl::Int>("Integer").custom_draw([](CustomSocketDrawParams &params) {
    uiLayout &row = params.layout.row(true);
    row.prop(&params.node_ptr, "integer", UI_ITEM_R_SPLIT_EMPTY_NAME, "", ICON_NONE);
    if (gizmos::value_node_has_gizmo(params.tree, params.node)) {
      row.prop(&params.socket_ptr, "pin_gizmo", UI_ITEM_NONE, "", ICON_GIZMO);
    }
  });
  ;
}

static void node_build_multi_function(NodeMultiFunctionBuilder &builder)
{
  const bNode &bnode = builder.node();
  NodeInputInt *node_storage = static_cast<NodeInputInt *>(bnode.storage);
  builder.construct_and_set_matching_fn<mf::CustomMF_Constant<int>>(node_storage->integer);
}

static void node_init(bNodeTree * /*tree*/, bNode *node)
{
  NodeInputInt *data = MEM_callocN<NodeInputInt>(__func__);
  node->storage = data;
}

static void node_register()
{
  static blender::bke::bNodeType ntype;

  fn_node_type_base(&ntype, "FunctionNodeInputInt", FN_NODE_INPUT_INT);
  ntype.ui_name = "Integer";
  ntype.ui_description =
      "Provide an integer value that can be connected to other nodes in the tree";
  ntype.enum_name_legacy = "INPUT_INT";
  ntype.nclass = NODE_CLASS_INPUT;
  ntype.declare = node_declare;
  ntype.initfunc = node_init;
  blender::bke::node_type_storage(
      ntype, "NodeInputInt", node_free_standard_storage, node_copy_standard_storage);
  ntype.build_multi_function = node_build_multi_function;
  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_fn_input_int_cc
