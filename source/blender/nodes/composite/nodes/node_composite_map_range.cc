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
 * The Original Code is Copyright (C) 2006 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup cmpnodes
 */

#include "UI_interface.h"
#include "UI_resources.h"

#include "node_composite_util.hh"

/* **************** Map Range ******************** */

namespace blender::nodes::node_composite_map_range_cc {

static void cmp_node_map_range_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Float>(N_("Value")).default_value(1.0f).min(0.0f).max(1.0f);
  b.add_input<decl::Float>(N_("From Min")).default_value(0.0f).min(-10000.0f).max(10000.0f);
  b.add_input<decl::Float>(N_("From Max")).default_value(0.0f).min(-10000.0f).max(10000.0f);
  b.add_input<decl::Float>(N_("To Min")).default_value(0.0f).min(-10000.0f).max(10000.0f);
  b.add_input<decl::Float>(N_("To Max")).default_value(0.0f).min(-10000.0f).max(10000.0f);
  b.add_output<decl::Float>(N_("Value"));
}

static void node_composit_buts_map_range(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiLayout *col;

  col = uiLayoutColumn(layout, true);
  uiItemR(col, ptr, "use_clamp", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
}

}  // namespace blender::nodes::node_composite_map_range_cc

void register_node_type_cmp_map_range()
{
  namespace file_ns = blender::nodes::node_composite_map_range_cc;

  static bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_MAP_RANGE, "Map Range", NODE_CLASS_OP_VECTOR);
  ntype.declare = file_ns::cmp_node_map_range_declare;
  ntype.draw_buttons = file_ns::node_composit_buts_map_range;

  nodeRegisterType(&ntype);
}
