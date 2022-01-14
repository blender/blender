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

/* **************** ALPHAOVER ******************** */

namespace blender::nodes::node_composite_alpha_over_cc {

static void cmp_node_alphaover_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Float>(N_("Fac")).default_value(1.0f).min(0.0f).max(1.0f).subtype(PROP_FACTOR);
  b.add_input<decl::Color>(N_("Image")).default_value({1.0f, 1.0f, 1.0f, 1.0f});
  b.add_input<decl::Color>(N_("Image"), "Image_001").default_value({1.0f, 1.0f, 1.0f, 1.0f});
  b.add_output<decl::Color>(N_("Image"));
}

static void node_alphaover_init(bNodeTree *UNUSED(ntree), bNode *node)
{
  node->storage = MEM_cnew<NodeTwoFloats>(__func__);
}

static void node_composit_buts_alphaover(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiLayout *col;

  col = uiLayoutColumn(layout, true);
  uiItemR(col, ptr, "use_premultiply", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
  uiItemR(col, ptr, "premul", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
}

}  // namespace blender::nodes::node_composite_alpha_over_cc

void register_node_type_cmp_alphaover()
{
  namespace file_ns = blender::nodes::node_composite_alpha_over_cc;

  static bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_ALPHAOVER, "Alpha Over", NODE_CLASS_OP_COLOR);
  ntype.declare = file_ns::cmp_node_alphaover_declare;
  ntype.draw_buttons = file_ns::node_composit_buts_alphaover;
  node_type_init(&ntype, file_ns::node_alphaover_init);
  node_type_storage(
      &ntype, "NodeTwoFloats", node_free_standard_storage, node_copy_standard_storage);

  nodeRegisterType(&ntype);
}
