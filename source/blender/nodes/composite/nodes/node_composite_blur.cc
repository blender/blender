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
 * Juho Vepsäläinen
 */

/** \file
 * \ingroup cmpnodes
 */

#include "RNA_access.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "node_composite_util.hh"

/* **************** BLUR ******************** */

namespace blender::nodes {

static void cmp_node_blur_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>(N_("Image")).default_value({1.0f, 1.0f, 1.0f, 1.0f});
  b.add_input<decl::Float>(N_("Size")).default_value(1.0f).min(0.0f).max(1.0f);
  b.add_output<decl::Color>(N_("Image"));
}

}  // namespace blender::nodes

static void node_composit_init_blur(bNodeTree *UNUSED(ntree), bNode *node)
{
  NodeBlurData *data = MEM_cnew<NodeBlurData>(__func__);
  data->filtertype = R_FILTER_GAUSS;
  node->storage = data;
}

static void node_composit_buts_blur(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiLayout *col, *row;

  col = uiLayoutColumn(layout, false);
  const int filter = RNA_enum_get(ptr, "filter_type");
  const int reference = RNA_boolean_get(ptr, "use_variable_size");

  uiItemR(col, ptr, "filter_type", UI_ITEM_R_SPLIT_EMPTY_NAME, "", ICON_NONE);
  if (filter != R_FILTER_FAST_GAUSS) {
    uiItemR(col, ptr, "use_variable_size", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
    if (!reference) {
      uiItemR(col, ptr, "use_bokeh", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
    }
    uiItemR(col, ptr, "use_gamma_correction", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
  }

  uiItemR(col, ptr, "use_relative", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);

  if (RNA_boolean_get(ptr, "use_relative")) {
    uiItemL(col, IFACE_("Aspect Correction"), ICON_NONE);
    row = uiLayoutRow(layout, true);
    uiItemR(row,
            ptr,
            "aspect_correction",
            UI_ITEM_R_SPLIT_EMPTY_NAME | UI_ITEM_R_EXPAND,
            nullptr,
            ICON_NONE);

    col = uiLayoutColumn(layout, true);
    uiItemR(col, ptr, "factor_x", UI_ITEM_R_SPLIT_EMPTY_NAME, IFACE_("X"), ICON_NONE);
    uiItemR(col, ptr, "factor_y", UI_ITEM_R_SPLIT_EMPTY_NAME, IFACE_("Y"), ICON_NONE);
  }
  else {
    col = uiLayoutColumn(layout, true);
    uiItemR(col, ptr, "size_x", UI_ITEM_R_SPLIT_EMPTY_NAME, IFACE_("X"), ICON_NONE);
    uiItemR(col, ptr, "size_y", UI_ITEM_R_SPLIT_EMPTY_NAME, IFACE_("Y"), ICON_NONE);
  }
  uiItemR(col, ptr, "use_extended_bounds", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
}

void register_node_type_cmp_blur()
{
  static bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_BLUR, "Blur", NODE_CLASS_OP_FILTER, NODE_PREVIEW);
  ntype.declare = blender::nodes::cmp_node_blur_declare;
  ntype.draw_buttons = node_composit_buts_blur;
  node_type_init(&ntype, node_composit_init_blur);
  node_type_storage(
      &ntype, "NodeBlurData", node_free_standard_storage, node_copy_standard_storage);

  nodeRegisterType(&ntype);
}
