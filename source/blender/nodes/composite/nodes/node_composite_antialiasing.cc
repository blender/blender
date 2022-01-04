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
 * The Original Code is Copyright (C) 2017 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 */

/** \file
 * \ingroup cmpnodes
 */

#include "UI_interface.h"
#include "UI_resources.h"

#include "node_composite_util.hh"

/* **************** Anti-Aliasing (SMAA 1x) ******************** */

namespace blender::nodes {

static void cmp_node_antialiasing_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>(N_("Image")).default_value({1.0f, 1.0f, 1.0f, 1.0f});
  b.add_output<decl::Color>(N_("Image"));
}

}  // namespace blender::nodes

static void node_composit_init_antialiasing(bNodeTree *UNUSED(ntree), bNode *node)
{
  NodeAntiAliasingData *data = MEM_cnew<NodeAntiAliasingData>(__func__);

  data->threshold = CMP_DEFAULT_SMAA_THRESHOLD;
  data->contrast_limit = CMP_DEFAULT_SMAA_CONTRAST_LIMIT;
  data->corner_rounding = CMP_DEFAULT_SMAA_CORNER_ROUNDING;

  node->storage = data;
}

static void node_composit_buts_antialiasing(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiLayout *col;

  col = uiLayoutColumn(layout, false);

  uiItemR(col, ptr, "threshold", 0, nullptr, ICON_NONE);
  uiItemR(col, ptr, "contrast_limit", 0, nullptr, ICON_NONE);
  uiItemR(col, ptr, "corner_rounding", 0, nullptr, ICON_NONE);
}

void register_node_type_cmp_antialiasing()
{
  static bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_ANTIALIASING, "Anti-Aliasing", NODE_CLASS_OP_FILTER);
  ntype.declare = blender::nodes::cmp_node_antialiasing_declare;
  ntype.draw_buttons = node_composit_buts_antialiasing;
  ntype.flag |= NODE_PREVIEW;
  node_type_size(&ntype, 170, 140, 200);
  node_type_init(&ntype, node_composit_init_antialiasing);
  node_type_storage(
      &ntype, "NodeAntiAliasingData", node_free_standard_storage, node_copy_standard_storage);

  nodeRegisterType(&ntype);
}
