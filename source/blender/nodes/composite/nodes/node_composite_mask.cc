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
 * The Original Code is Copyright (C) 2012 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup cmpnodes
 */

#include "DNA_mask_types.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "node_composite_util.hh"

/* **************** Mask  ******************** */

namespace blender::nodes {

static void cmp_node_mask_declare(NodeDeclarationBuilder &b)
{
  b.add_output<decl::Float>(N_("Mask"));
}

}  // namespace blender::nodes

static void node_composit_init_mask(bNodeTree *UNUSED(ntree), bNode *node)
{
  NodeMask *data = MEM_cnew<NodeMask>(__func__);
  data->size_x = data->size_y = 256;
  node->storage = data;

  node->custom2 = 16;   /* samples */
  node->custom3 = 0.5f; /* shutter */
}

static void node_mask_label(const bNodeTree *UNUSED(ntree),
                            const bNode *node,
                            char *label,
                            int maxlen)
{
  if (node->id != nullptr) {
    BLI_strncpy(label, node->id->name + 2, maxlen);
  }
  else {
    BLI_strncpy(label, IFACE_("Mask"), maxlen);
  }
}

static void node_composit_buts_mask(uiLayout *layout, bContext *C, PointerRNA *ptr)
{
  bNode *node = (bNode *)ptr->data;

  uiTemplateID(layout,
               C,
               ptr,
               "mask",
               nullptr,
               nullptr,
               nullptr,
               UI_TEMPLATE_ID_FILTER_ALL,
               false,
               nullptr);
  uiItemR(layout, ptr, "use_feather", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);

  uiItemR(layout, ptr, "size_source", UI_ITEM_R_SPLIT_EMPTY_NAME, "", ICON_NONE);

  if (node->custom1 & (CMP_NODEFLAG_MASK_FIXED | CMP_NODEFLAG_MASK_FIXED_SCENE)) {
    uiItemR(layout, ptr, "size_x", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
    uiItemR(layout, ptr, "size_y", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
  }

  uiItemR(layout, ptr, "use_motion_blur", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
  if (node->custom1 & CMP_NODEFLAG_MASK_MOTION_BLUR) {
    uiItemR(layout, ptr, "motion_blur_samples", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
    uiItemR(layout, ptr, "motion_blur_shutter", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
  }
}

void register_node_type_cmp_mask()
{
  static bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_MASK, "Mask", NODE_CLASS_INPUT);
  ntype.declare = blender::nodes::cmp_node_mask_declare;
  ntype.draw_buttons = node_composit_buts_mask;
  node_type_init(&ntype, node_composit_init_mask);
  ntype.labelfunc = node_mask_label;

  node_type_storage(&ntype, "NodeMask", node_free_standard_storage, node_copy_standard_storage);

  nodeRegisterType(&ntype);
}
