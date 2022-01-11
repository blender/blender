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

/* **************** BILATERALBLUR ******************** */

namespace blender::nodes::node_composite_bilateralblur_cc {

static void cmp_node_bilateralblur_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>(N_("Image")).default_value({1.0f, 1.0f, 1.0f, 1.0f});
  b.add_input<decl::Color>(N_("Determinator")).default_value({1.0f, 1.0f, 1.0f, 1.0f});
  b.add_output<decl::Color>(N_("Image"));
}

static void node_composit_init_bilateralblur(bNodeTree *UNUSED(ntree), bNode *node)
{
  NodeBilateralBlurData *nbbd = MEM_cnew<NodeBilateralBlurData>(__func__);
  node->storage = nbbd;
  nbbd->iter = 1;
  nbbd->sigma_color = 0.3;
  nbbd->sigma_space = 5.0;
}

static void node_composit_buts_bilateralblur(uiLayout *layout,
                                             bContext *UNUSED(C),
                                             PointerRNA *ptr)
{
  uiLayout *col;

  col = uiLayoutColumn(layout, true);
  uiItemR(col, ptr, "iterations", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
  uiItemR(col, ptr, "sigma_color", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
  uiItemR(col, ptr, "sigma_space", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
}

}  // namespace blender::nodes::node_composite_bilateralblur_cc

void register_node_type_cmp_bilateralblur()
{
  namespace file_ns = blender::nodes::node_composite_bilateralblur_cc;

  static bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_BILATERALBLUR, "Bilateral Blur", NODE_CLASS_OP_FILTER);
  ntype.declare = file_ns::cmp_node_bilateralblur_declare;
  ntype.draw_buttons = file_ns::node_composit_buts_bilateralblur;
  node_type_init(&ntype, file_ns::node_composit_init_bilateralblur);
  node_type_storage(
      &ntype, "NodeBilateralBlurData", node_free_standard_storage, node_copy_standard_storage);

  nodeRegisterType(&ntype);
}
