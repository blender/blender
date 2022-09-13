/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2012 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup cmpnodes
 */

#include "DNA_mask_types.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "COM_node_operation.hh"

#include "node_composite_util.hh"

/* **************** Mask  ******************** */

namespace blender::nodes::node_composite_mask_cc {

static void cmp_node_mask_declare(NodeDeclarationBuilder &b)
{
  b.add_output<decl::Float>(N_("Mask"));
}

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

using namespace blender::realtime_compositor;

class MaskOperation : public NodeOperation {
 public:
  using NodeOperation::NodeOperation;

  void execute() override
  {
    get_result("Mask").allocate_invalid();
  }
};

static NodeOperation *get_compositor_operation(Context &context, DNode node)
{
  return new MaskOperation(context, node);
}

}  // namespace blender::nodes::node_composite_mask_cc

void register_node_type_cmp_mask()
{
  namespace file_ns = blender::nodes::node_composite_mask_cc;

  static bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_MASK, "Mask", NODE_CLASS_INPUT);
  ntype.declare = file_ns::cmp_node_mask_declare;
  ntype.draw_buttons = file_ns::node_composit_buts_mask;
  node_type_init(&ntype, file_ns::node_composit_init_mask);
  ntype.labelfunc = file_ns::node_mask_label;
  ntype.get_compositor_operation = file_ns::get_compositor_operation;

  node_type_storage(&ntype, "NodeMask", node_free_standard_storage, node_copy_standard_storage);

  nodeRegisterType(&ntype);
}
