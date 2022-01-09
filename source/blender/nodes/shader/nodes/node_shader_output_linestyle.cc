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
 *
 * The Original Code is Copyright (C) 2005 Blender Foundation.
 * All rights reserved.
 */

#include "node_shader_util.hh"

#include "UI_interface.h"
#include "UI_resources.h"

namespace blender::nodes::node_shader_output_linestyle_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>(N_("Color")).default_value({1.0f, 0.0f, 1.0f, 1.0f});
  b.add_input<decl::Float>(N_("Color Fac"))
      .default_value(1.0f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR);
  b.add_input<decl::Float>(N_("Alpha"))
      .default_value(1.0f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR);
  b.add_input<decl::Float>(N_("Alpha Fac"))
      .default_value(1.0f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR);
}

static void node_buts_output_linestyle(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiLayout *row, *col;

  col = uiLayoutColumn(layout, false);
  row = uiLayoutRow(col, true);
  uiItemR(row, ptr, "blend_type", UI_ITEM_R_SPLIT_EMPTY_NAME, "", ICON_NONE);
  uiItemR(col, ptr, "use_clamp", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
}

}  // namespace blender::nodes::node_shader_output_linestyle_cc

/* node type definition */
void register_node_type_sh_output_linestyle()
{
  namespace file_ns = blender::nodes::node_shader_output_linestyle_cc;

  static bNodeType ntype;

  sh_node_type_base(&ntype, SH_NODE_OUTPUT_LINESTYLE, "Line Style Output", NODE_CLASS_OUTPUT);
  ntype.declare = file_ns::node_declare;
  ntype.draw_buttons = file_ns::node_buts_output_linestyle;
  ntype.no_muting = true;

  nodeRegisterType(&ntype);
}
