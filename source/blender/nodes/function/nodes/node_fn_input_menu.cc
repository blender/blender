/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_function_util.hh"
#include "node_shader_util.hh"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "BKE_node_runtime.hh"

namespace blender::nodes::node_fn_input_menu_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_output<decl::Menu>("Menu"_ustr).custom_draw([](CustomSocketDrawParams &params) {
    params.layout.alignment_set(ui::LayoutAlign::Expand);
    ui::Layout &row = params.layout.row(true);

    const bNodeSocketValueMenu *default_value =
        params.node.output_socket(0).default_value_typed<bNodeSocketValueMenu>();
    BLI_assert(default_value);

    if (default_value->enum_items) {
      if (default_value->enum_items->items.is_empty()) {
        row.label(IFACE_("No Items"), ICON_NONE);
      }
      else {
        row.prop(&params.node_ptr, "value", UI_ITEM_NONE, "", ICON_NONE);
      }
      return;
    }

    if (default_value->has_conflict()) {
      row.label(IFACE_("Menu Error"), ICON_STATUS_ERROR);
    }
    else {
      row.label(IFACE_("Menu Undefined"), ICON_QUESTION);
    }
  });
}

static void node_build_multi_function(NodeMultiFunctionBuilder &builder)
{
  const bNode &bnode = builder.node();
  const NodeInputMenu &node_storage = *static_cast<const NodeInputMenu *>(bnode.storage);
  builder.construct_and_set_matching_fn<mf::CustomMF_Constant<MenuValue>>(
      MenuValue(node_storage.value));
}

static void node_init(bNodeTree * /*tree*/, bNode *node)
{
  NodeInputMenu *data = MEM_new<NodeInputMenu>(__func__);
  node->storage = data;
}

static void node_register()
{
  static bke::bNodeType ntype;

  common_node_type_base(&ntype, "FunctionNodeInputMenu"_ustr);
  ntype.ui_name = "Menu";
  ntype.ui_description = "Provide a menu value that can be connected to other nodes in the tree";
  ntype.nclass = NODE_CLASS_INPUT;
  ntype.declare = node_declare;
  ntype.initfunc = node_init;
  bke::node_type_storage(
      ntype, "NodeInputMenu", node_free_standard_storage, node_copy_standard_storage);
  ntype.build_multi_function = node_build_multi_function;
  bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_fn_input_menu_cc
