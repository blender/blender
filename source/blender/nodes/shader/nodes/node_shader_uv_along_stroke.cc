/* SPDX-FileCopyrightText: 2005 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_shader_util.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"

namespace blender::nodes::node_shader_uv_along_stroke_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_output<decl::Vector>("UV");
}

static void node_shader_buts_uvalongstroke(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiItemR(layout, ptr, "use_tips", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
}

}  // namespace blender::nodes::node_shader_uv_along_stroke_cc

/* node type definition */
void register_node_type_sh_uvalongstroke()
{
  namespace file_ns = blender::nodes::node_shader_uv_along_stroke_cc;

  static bNodeType ntype;

  sh_node_type_base(&ntype, SH_NODE_UVALONGSTROKE, "UV Along Stroke", NODE_CLASS_INPUT);
  ntype.declare = file_ns::node_declare;
  ntype.add_ui_poll = line_style_shader_nodes_poll;
  ntype.draw_buttons = file_ns::node_shader_buts_uvalongstroke;

  nodeRegisterType(&ntype);
}
