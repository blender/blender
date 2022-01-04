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

static void cmp_node_lensdist_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>(N_("Image")).default_value({1.0f, 1.0f, 1.0f, 1.0f});
  b.add_input<decl::Float>(N_("Distort")).default_value(0.0f).min(-0.999f).max(1.0f);
  b.add_input<decl::Float>(N_("Dispersion")).default_value(0.0f).min(0.0f).max(1.0f);
  b.add_output<decl::Color>(N_("Image"));
}

}  // namespace blender::nodes

static void node_composit_init_lensdist(bNodeTree *UNUSED(ntree), bNode *node)
{
  NodeLensDist *nld = MEM_cnew<NodeLensDist>(__func__);
  nld->jit = nld->proj = nld->fit = 0;
  node->storage = nld;
}

static void node_composit_buts_lensdist(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiLayout *col;

  col = uiLayoutColumn(layout, false);
  uiItemR(col, ptr, "use_projector", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);

  col = uiLayoutColumn(col, false);
  uiLayoutSetActive(col, RNA_boolean_get(ptr, "use_projector") == false);
  uiItemR(col, ptr, "use_jitter", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
  uiItemR(col, ptr, "use_fit", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
}

void register_node_type_cmp_lensdist()
{
  static bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_LENSDIST, "Lens Distortion", NODE_CLASS_DISTORT);
  ntype.declare = blender::nodes::cmp_node_lensdist_declare;
  ntype.draw_buttons = node_composit_buts_lensdist;
  node_type_init(&ntype, node_composit_init_lensdist);
  node_type_storage(
      &ntype, "NodeLensDist", node_free_standard_storage, node_copy_standard_storage);

  nodeRegisterType(&ntype);
}
