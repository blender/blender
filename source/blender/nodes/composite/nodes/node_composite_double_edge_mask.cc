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
 * The Original Code is Copyright (C) 2011 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup cmpnodes
 */

#include "UI_interface.h"
#include "UI_resources.h"

#include "node_composite_util.hh"

/* **************** Double Edge Mask ******************** */

namespace blender::nodes {

static void cmp_node_double_edge_mask_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Float>(N_("Inner Mask")).default_value(0.8f).min(0.0f).max(1.0f);
  b.add_input<decl::Float>(N_("Outer Mask")).default_value(0.8f).min(0.0f).max(1.0f);
  b.add_output<decl::Float>(N_("Mask"));
}

}  // namespace blender::nodes

static void node_composit_buts_double_edge_mask(uiLayout *layout,
                                                bContext *UNUSED(C),
                                                PointerRNA *ptr)
{
  uiLayout *col;

  col = uiLayoutColumn(layout, false);

  uiItemL(col, IFACE_("Inner Edge:"), ICON_NONE);
  uiItemR(col, ptr, "inner_mode", UI_ITEM_R_SPLIT_EMPTY_NAME, "", ICON_NONE);
  uiItemL(col, IFACE_("Buffer Edge:"), ICON_NONE);
  uiItemR(col, ptr, "edge_mode", UI_ITEM_R_SPLIT_EMPTY_NAME, "", ICON_NONE);
}

void register_node_type_cmp_doubleedgemask()
{
  static bNodeType ntype; /* Allocate a node type data structure. */

  cmp_node_type_base(&ntype, CMP_NODE_DOUBLEEDGEMASK, "Double Edge Mask", NODE_CLASS_MATTE);
  ntype.declare = blender::nodes::cmp_node_double_edge_mask_declare;
  ntype.draw_buttons = node_composit_buts_double_edge_mask;

  nodeRegisterType(&ntype);
}
