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

/* **************** MAP VALUE ******************** */

namespace blender::nodes::node_composite_map_value_cc {

static void cmp_node_map_value_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Float>(N_("Value")).default_value(1.0f).min(0.0f).max(1.0f);
  b.add_output<decl::Float>(N_("Value"));
}

static void node_composit_init_map_value(bNodeTree *UNUSED(ntree), bNode *node)
{
  node->storage = BKE_texture_mapping_add(TEXMAP_TYPE_POINT);
}

static void node_composit_buts_map_value(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiLayout *sub, *col;

  col = uiLayoutColumn(layout, true);
  uiItemR(col, ptr, "offset", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
  uiItemR(col, ptr, "size", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);

  col = uiLayoutColumn(layout, true);
  uiItemR(col, ptr, "use_min", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
  sub = uiLayoutColumn(col, false);
  uiLayoutSetActive(sub, RNA_boolean_get(ptr, "use_min"));
  uiItemR(sub, ptr, "min", UI_ITEM_R_SPLIT_EMPTY_NAME, "", ICON_NONE);

  col = uiLayoutColumn(layout, true);
  uiItemR(col, ptr, "use_max", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
  sub = uiLayoutColumn(col, false);
  uiLayoutSetActive(sub, RNA_boolean_get(ptr, "use_max"));
  uiItemR(sub, ptr, "max", UI_ITEM_R_SPLIT_EMPTY_NAME, "", ICON_NONE);
}

}  // namespace blender::nodes::node_composite_map_value_cc

void register_node_type_cmp_map_value()
{
  namespace file_ns = blender::nodes::node_composite_map_value_cc;

  static bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_MAP_VALUE, "Map Value", NODE_CLASS_OP_VECTOR);
  ntype.declare = file_ns::cmp_node_map_value_declare;
  ntype.draw_buttons = file_ns::node_composit_buts_map_value;
  node_type_init(&ntype, file_ns::node_composit_init_map_value);
  node_type_storage(&ntype, "TexMapping", node_free_standard_storage, node_copy_standard_storage);

  nodeRegisterType(&ntype);
}
