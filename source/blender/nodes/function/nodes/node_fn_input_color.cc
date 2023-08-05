/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_function_util.hh"

#include "BLI_math_vector.h"

#include "UI_interface.hh"
#include "UI_resources.hh"

namespace blender::nodes::node_fn_input_color_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_output<decl::Color>("Color");
}

static void node_layout(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiTemplateColorPicker(layout, ptr, "color", true, false, false, true);
  uiItemR(layout, ptr, "color", UI_ITEM_R_SPLIT_EMPTY_NAME, "", ICON_NONE);
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
  NodeInputColor *data = MEM_cnew<NodeInputColor>(__func__);
  copy_v4_fl4(data->color, 0.5f, 0.5f, 0.5f, 1.0f);
  node->storage = data;
}

}  // namespace blender::nodes::node_fn_input_color_cc

void register_node_type_fn_input_color()
{
  namespace file_ns = blender::nodes::node_fn_input_color_cc;

  static bNodeType ntype;

  fn_node_type_base(&ntype, FN_NODE_INPUT_COLOR, "Color", NODE_CLASS_INPUT);
  ntype.declare = file_ns::node_declare;
  ntype.initfunc = file_ns::node_init;
  node_type_storage(
      &ntype, "NodeInputColor", node_free_standard_storage, node_copy_standard_storage);
  ntype.build_multi_function = file_ns::node_build_multi_function;
  ntype.draw_buttons = file_ns::node_layout;
  nodeRegisterType(&ntype);
}
