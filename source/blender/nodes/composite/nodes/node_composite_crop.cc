/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2006 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup cmpnodes
 */

#include "RNA_access.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "node_composite_util.hh"

/* **************** Crop  ******************** */

namespace blender::nodes::node_composite_crop_cc {

static void cmp_node_crop_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>(N_("Image")).default_value({1.0f, 1.0f, 1.0f, 1.0f});
  b.add_output<decl::Color>(N_("Image"));
}

static void node_composit_init_crop(bNodeTree *UNUSED(ntree), bNode *node)
{
  NodeTwoXYs *nxy = MEM_cnew<NodeTwoXYs>(__func__);
  node->storage = nxy;
  nxy->x1 = 0;
  nxy->x2 = 0;
  nxy->y1 = 0;
  nxy->y2 = 0;
}

static void node_composit_buts_crop(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiLayout *col;

  uiItemR(layout, ptr, "use_crop_size", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
  uiItemR(layout, ptr, "relative", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);

  col = uiLayoutColumn(layout, true);
  if (RNA_boolean_get(ptr, "relative")) {
    uiItemR(col, ptr, "rel_min_x", UI_ITEM_R_SPLIT_EMPTY_NAME, IFACE_("Left"), ICON_NONE);
    uiItemR(col, ptr, "rel_max_x", UI_ITEM_R_SPLIT_EMPTY_NAME, IFACE_("Right"), ICON_NONE);
    uiItemR(col, ptr, "rel_min_y", UI_ITEM_R_SPLIT_EMPTY_NAME, IFACE_("Up"), ICON_NONE);
    uiItemR(col, ptr, "rel_max_y", UI_ITEM_R_SPLIT_EMPTY_NAME, IFACE_("Down"), ICON_NONE);
  }
  else {
    uiItemR(col, ptr, "min_x", UI_ITEM_R_SPLIT_EMPTY_NAME, IFACE_("Left"), ICON_NONE);
    uiItemR(col, ptr, "max_x", UI_ITEM_R_SPLIT_EMPTY_NAME, IFACE_("Right"), ICON_NONE);
    uiItemR(col, ptr, "min_y", UI_ITEM_R_SPLIT_EMPTY_NAME, IFACE_("Up"), ICON_NONE);
    uiItemR(col, ptr, "max_y", UI_ITEM_R_SPLIT_EMPTY_NAME, IFACE_("Down"), ICON_NONE);
  }
}

}  // namespace blender::nodes::node_composite_crop_cc

void register_node_type_cmp_crop()
{
  namespace file_ns = blender::nodes::node_composite_crop_cc;

  static bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_CROP, "Crop", NODE_CLASS_DISTORT);
  ntype.declare = file_ns::cmp_node_crop_declare;
  ntype.draw_buttons = file_ns::node_composit_buts_crop;
  node_type_init(&ntype, file_ns::node_composit_init_crop);
  node_type_storage(&ntype, "NodeTwoXYs", node_free_standard_storage, node_copy_standard_storage);

  nodeRegisterType(&ntype);
}
