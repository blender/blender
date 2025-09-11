/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_function_util.hh"

#include "BLI_math_vector.h"

#include "UI_interface.hh"
#include "UI_interface_layout.hh"
#include "UI_resources.hh"

namespace blender::nodes::node_fn_input_color_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_output<decl::Color>("Color").custom_draw([](CustomSocketDrawParams &params) {
    params.layout.alignment_set(ui::LayoutAlign::Expand);
    uiLayout &col = params.layout.column(false);
    uiTemplateColorPicker(&col, &params.node_ptr, "value", true, false, false, true);
    col.prop(&params.node_ptr, "value", UI_ITEM_R_SPLIT_EMPTY_NAME, "", ICON_NONE);
  });
}

static void node_build_multi_function(blender::nodes::NodeMultiFunctionBuilder &builder)
{
  const bNode &bnode = builder.node();
  NodeInputColor *node_storage = static_cast<NodeInputColor *>(bnode.storage);
  blender::ColorGeometry4f color = (ColorGeometry4f)node_storage->color;
  builder.construct_and_set_matching_fn<blender::mf::CustomMF_Constant<ColorGeometry4f>>(color);
}

static void node_init(bNodeTree * /*tree*/, bNode *node)
{
  NodeInputColor *data = MEM_callocN<NodeInputColor>(__func__);
  copy_v4_fl4(data->color, 0.5f, 0.5f, 0.5f, 1.0f);
  node->storage = data;
}

static void node_register()
{
  static blender::bke::bNodeType ntype;

  fn_node_type_base(&ntype, "FunctionNodeInputColor", FN_NODE_INPUT_COLOR);
  ntype.ui_name = "Color";
  ntype.ui_description = "Output a color value chosen with the color picker widget";
  ntype.enum_name_legacy = "INPUT_COLOR";
  ntype.nclass = NODE_CLASS_INPUT;
  ntype.declare = node_declare;
  ntype.initfunc = node_init;
  blender::bke::node_type_storage(
      ntype, "NodeInputColor", node_free_standard_storage, node_copy_standard_storage);
  ntype.build_multi_function = node_build_multi_function;
  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_fn_input_color_cc
