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

namespace blender::nodes {

static void cmp_node_tonemap_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>(N_("Image")).default_value({1.0f, 1.0f, 1.0f, 1.0f});
  b.add_output<decl::Color>(N_("Image"));
}

}  // namespace blender::nodes

static void node_composit_init_tonemap(bNodeTree *UNUSED(ntree), bNode *node)
{
  NodeTonemap *ntm = MEM_cnew<NodeTonemap>(__func__);
  ntm->type = 1;
  ntm->key = 0.18;
  ntm->offset = 1;
  ntm->gamma = 1;
  ntm->f = 0;
  ntm->m = 0; /* Actual value is set according to input. */
  /* Default a of 1 works well with natural HDR images, but not always so for CGI.
   * Maybe should use 0 or at least lower initial value instead. */
  ntm->a = 1;
  ntm->c = 0;
  node->storage = ntm;
}

static void node_composit_buts_tonemap(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiLayout *col;

  col = uiLayoutColumn(layout, false);
  uiItemR(col, ptr, "tonemap_type", UI_ITEM_R_SPLIT_EMPTY_NAME, "", ICON_NONE);
  if (RNA_enum_get(ptr, "tonemap_type") == 0) {
    uiItemR(col, ptr, "key", UI_ITEM_R_SPLIT_EMPTY_NAME | UI_ITEM_R_SLIDER, nullptr, ICON_NONE);
    uiItemR(col, ptr, "offset", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
    uiItemR(col, ptr, "gamma", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
  }
  else {
    uiItemR(col, ptr, "intensity", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
    uiItemR(
        col, ptr, "contrast", UI_ITEM_R_SPLIT_EMPTY_NAME | UI_ITEM_R_SLIDER, nullptr, ICON_NONE);
    uiItemR(
        col, ptr, "adaptation", UI_ITEM_R_SPLIT_EMPTY_NAME | UI_ITEM_R_SLIDER, nullptr, ICON_NONE);
    uiItemR(
        col, ptr, "correction", UI_ITEM_R_SPLIT_EMPTY_NAME | UI_ITEM_R_SLIDER, nullptr, ICON_NONE);
  }
}

void register_node_type_cmp_tonemap()
{
  static bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_TONEMAP, "Tonemap", NODE_CLASS_OP_COLOR, 0);
  ntype.declare = blender::nodes::cmp_node_tonemap_declare;
  ntype.draw_buttons = node_composit_buts_tonemap;
  node_type_init(&ntype, node_composit_init_tonemap);
  node_type_storage(&ntype, "NodeTonemap", node_free_standard_storage, node_copy_standard_storage);

  nodeRegisterType(&ntype);
}
