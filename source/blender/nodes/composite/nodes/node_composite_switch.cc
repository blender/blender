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

/* **************** Switch ******************** */

namespace blender::nodes {

static void cmp_node_switch_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>(N_("Off")).default_value({0.8f, 0.8f, 0.8f, 1.0f});
  b.add_input<decl::Color>(N_("On")).default_value({0.8f, 0.8f, 0.8f, 1.0f});
  b.add_output<decl::Color>(N_("Image"));
}

}  // namespace blender::nodes

static void node_composit_buts_switch(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiItemR(layout, ptr, "check", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
}

/* custom1 = mix type */
void register_node_type_cmp_switch()
{
  static bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_SWITCH, "Switch", NODE_CLASS_LAYOUT);
  ntype.declare = blender::nodes::cmp_node_switch_declare;
  ntype.draw_buttons = node_composit_buts_switch;
  node_type_size_preset(&ntype, NODE_SIZE_SMALL);
  nodeRegisterType(&ntype);
}
