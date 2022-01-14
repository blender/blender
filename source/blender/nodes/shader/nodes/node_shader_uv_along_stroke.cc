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

namespace blender::nodes::node_shader_uv_along_stroke_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_output<decl::Vector>(N_("UV"));
}

static void node_shader_buts_uvalongstroke(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiItemR(layout, ptr, "use_tips", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, 0);
}

}  // namespace blender::nodes::node_shader_uv_along_stroke_cc

/* node type definition */
void register_node_type_sh_uvalongstroke()
{
  namespace file_ns = blender::nodes::node_shader_uv_along_stroke_cc;

  static bNodeType ntype;

  sh_node_type_base(&ntype, SH_NODE_UVALONGSTROKE, "UV Along Stroke", NODE_CLASS_INPUT);
  ntype.declare = file_ns::node_declare;
  ntype.draw_buttons = file_ns::node_shader_buts_uvalongstroke;

  nodeRegisterType(&ntype);
}
