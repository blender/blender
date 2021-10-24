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

#include "UI_interface.h"
#include "UI_resources.h"

namespace blender::nodes {

static void fn_node_input_color_declare(NodeDeclarationBuilder &b)
{
  b.add_output<decl::Color>("Color");
};

static void fn_node_input_color_layout(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiTemplateColorPicker(layout, ptr, "color", true, false, false, true);
  uiItemR(layout, ptr, "color", UI_ITEM_R_SPLIT_EMPTY_NAME, "", ICON_NONE);
}

static void fn_node_input_color_build_multi_function(
    blender::nodes::NodeMultiFunctionBuilder &builder)
{
  bNode &bnode = builder.node();
  NodeInputColor *node_storage = static_cast<NodeInputColor *>(bnode.storage);
  blender::ColorGeometry4f color = (ColorGeometry4f)node_storage->color;
  builder.construct_and_set_matching_fn<blender::fn::CustomMF_Constant<ColorGeometry4f>>(color);
}

static void fn_node_input_color_init(bNodeTree *UNUSED(ntree), bNode *node)
{
  NodeInputColor *data = (NodeInputColor *)MEM_callocN(sizeof(NodeInputColor), __func__);
  copy_v4_fl4(data->color, 0.5f, 0.5f, 0.5f, 1.0f);
  node->storage = data;
}

}  // namespace blender::nodes

void register_node_type_fn_input_color()
{
  static bNodeType ntype;

  fn_node_type_base(&ntype, FN_NODE_INPUT_COLOR, "Color", NODE_CLASS_INPUT, 0);
  ntype.declare = blender::nodes::fn_node_input_color_declare;
  node_type_init(&ntype, blender::nodes::fn_node_input_color_init);
  node_type_storage(
      &ntype, "NodeInputColor", node_free_standard_storage, node_copy_standard_storage);
  ntype.build_multi_function = blender::nodes::fn_node_input_color_build_multi_function;
  ntype.draw_buttons = blender::nodes::fn_node_input_color_layout;
  nodeRegisterType(&ntype);
}
