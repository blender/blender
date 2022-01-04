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

/* **************** VECTOR BLUR ******************** */

namespace blender::nodes {

static void cmp_node_vec_blur_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>(N_("Image")).default_value({1.0f, 1.0f, 1.0f, 1.0f});
  b.add_input<decl::Float>(N_("Z")).default_value(0.0f).min(0.0f).max(1.0f);
  b.add_input<decl::Vector>(N_("Speed"))
      .default_value({0.0f, 0.0f, 0.0f})
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_VELOCITY);
  b.add_output<decl::Color>(N_("Image"));
}

}  // namespace blender::nodes

static void node_composit_init_vecblur(bNodeTree *UNUSED(ntree), bNode *node)
{
  NodeBlurData *nbd = MEM_cnew<NodeBlurData>(__func__);
  node->storage = nbd;
  nbd->samples = 32;
  nbd->fac = 1.0f;
}

static void node_composit_buts_vecblur(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiLayout *col;

  col = uiLayoutColumn(layout, false);
  uiItemR(col, ptr, "samples", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
  uiItemR(col, ptr, "factor", UI_ITEM_R_SPLIT_EMPTY_NAME, IFACE_("Blur"), ICON_NONE);

  col = uiLayoutColumn(layout, true);
  uiItemL(col, IFACE_("Speed:"), ICON_NONE);
  uiItemR(col, ptr, "speed_min", UI_ITEM_R_SPLIT_EMPTY_NAME, IFACE_("Min"), ICON_NONE);
  uiItemR(col, ptr, "speed_max", UI_ITEM_R_SPLIT_EMPTY_NAME, IFACE_("Max"), ICON_NONE);

  uiItemR(layout, ptr, "use_curved", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
}

/* custom1: iterations, custom2: max_speed (0 = no_limit). */
void register_node_type_cmp_vecblur()
{
  static bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_VECBLUR, "Vector Blur", NODE_CLASS_OP_FILTER);
  ntype.declare = blender::nodes::cmp_node_vec_blur_declare;
  ntype.draw_buttons = node_composit_buts_vecblur;
  node_type_init(&ntype, node_composit_init_vecblur);
  node_type_storage(
      &ntype, "NodeBlurData", node_free_standard_storage, node_copy_standard_storage);

  nodeRegisterType(&ntype);
}
