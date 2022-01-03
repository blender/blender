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

#include "RNA_access.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "node_composite_util.hh"

/* ******************* Channel Matte Node ********************************* */

namespace blender::nodes {

static void cmp_node_channel_matte_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>(N_("Image")).default_value({1.0f, 1.0f, 1.0f, 1.0f});
  b.add_output<decl::Color>(N_("Image"));
  b.add_output<decl::Float>(N_("Matte"));
}

}  // namespace blender::nodes

static void node_composit_init_channel_matte(bNodeTree *UNUSED(ntree), bNode *node)
{
  NodeChroma *c = MEM_cnew<NodeChroma>(__func__);
  node->storage = c;
  c->t1 = 1.0f;
  c->t2 = 0.0f;
  c->t3 = 0.0f;
  c->fsize = 0.0f;
  c->fstrength = 0.0f;
  c->algorithm = 1;  /* Max channel limiting. */
  c->channel = 1;    /* Limit by red. */
  node->custom1 = 1; /* RGB channel. */
  node->custom2 = 2; /* Green Channel. */
}

static void node_composit_buts_channel_matte(uiLayout *layout,
                                             bContext *UNUSED(C),
                                             PointerRNA *ptr)
{
  uiLayout *col, *row;

  uiItemL(layout, IFACE_("Color Space:"), ICON_NONE);
  row = uiLayoutRow(layout, false);
  uiItemR(
      row, ptr, "color_space", UI_ITEM_R_SPLIT_EMPTY_NAME | UI_ITEM_R_EXPAND, nullptr, ICON_NONE);

  col = uiLayoutColumn(layout, false);
  uiItemL(col, IFACE_("Key Channel:"), ICON_NONE);
  row = uiLayoutRow(col, false);
  uiItemR(row,
          ptr,
          "matte_channel",
          UI_ITEM_R_SPLIT_EMPTY_NAME | UI_ITEM_R_EXPAND,
          nullptr,
          ICON_NONE);

  col = uiLayoutColumn(layout, false);

  uiItemR(col, ptr, "limit_method", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
  if (RNA_enum_get(ptr, "limit_method") == 0) {
    uiItemL(col, IFACE_("Limiting Channel:"), ICON_NONE);
    row = uiLayoutRow(col, false);
    uiItemR(row,
            ptr,
            "limit_channel",
            UI_ITEM_R_SPLIT_EMPTY_NAME | UI_ITEM_R_EXPAND,
            nullptr,
            ICON_NONE);
  }

  uiItemR(
      col, ptr, "limit_max", UI_ITEM_R_SPLIT_EMPTY_NAME | UI_ITEM_R_SLIDER, nullptr, ICON_NONE);
  uiItemR(
      col, ptr, "limit_min", UI_ITEM_R_SPLIT_EMPTY_NAME | UI_ITEM_R_SLIDER, nullptr, ICON_NONE);
}

void register_node_type_cmp_channel_matte()
{
  static bNodeType ntype;

  cmp_node_type_base(
      &ntype, CMP_NODE_CHANNEL_MATTE, "Channel Key", NODE_CLASS_MATTE, NODE_PREVIEW);
  ntype.declare = blender::nodes::cmp_node_channel_matte_declare;
  ntype.draw_buttons = node_composit_buts_channel_matte;
  node_type_init(&ntype, node_composit_init_channel_matte);
  node_type_storage(&ntype, "NodeChroma", node_free_standard_storage, node_copy_standard_storage);

  nodeRegisterType(&ntype);
}
