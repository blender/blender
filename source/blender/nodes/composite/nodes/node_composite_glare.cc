/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2006 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup cmpnodes
 */

#include "RNA_access.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "COM_node_operation.hh"

#include "node_composite_util.hh"

namespace blender::nodes::node_composite_glare_cc {

static void cmp_node_glare_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>(N_("Image")).default_value({1.0f, 1.0f, 1.0f, 1.0f});
  b.add_output<decl::Color>(N_("Image"));
}

static void node_composit_init_glare(bNodeTree *UNUSED(ntree), bNode *node)
{
  NodeGlare *ndg = MEM_cnew<NodeGlare>(__func__);
  ndg->quality = 1;
  ndg->type = 2;
  ndg->iter = 3;
  ndg->colmod = 0.25;
  ndg->mix = 0;
  ndg->threshold = 1;
  ndg->star_45 = true;
  ndg->streaks = 4;
  ndg->angle_ofs = 0.0f;
  ndg->fade = 0.9;
  ndg->size = 8;
  node->storage = ndg;
}

static void node_composit_buts_glare(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiItemR(layout, ptr, "glare_type", UI_ITEM_R_SPLIT_EMPTY_NAME, "", ICON_NONE);
  uiItemR(layout, ptr, "quality", UI_ITEM_R_SPLIT_EMPTY_NAME, "", ICON_NONE);

  if (RNA_enum_get(ptr, "glare_type") != 1) {
    uiItemR(layout, ptr, "iterations", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);

    if (RNA_enum_get(ptr, "glare_type") != 0) {
      uiItemR(layout,
              ptr,
              "color_modulation",
              UI_ITEM_R_SPLIT_EMPTY_NAME | UI_ITEM_R_SLIDER,
              nullptr,
              ICON_NONE);
    }
  }

  uiItemR(layout, ptr, "mix", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
  uiItemR(layout, ptr, "threshold", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);

  if (RNA_enum_get(ptr, "glare_type") == 2) {
    uiItemR(layout, ptr, "streaks", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
    uiItemR(layout, ptr, "angle_offset", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
  }
  if (RNA_enum_get(ptr, "glare_type") == 0 || RNA_enum_get(ptr, "glare_type") == 2) {
    uiItemR(
        layout, ptr, "fade", UI_ITEM_R_SPLIT_EMPTY_NAME | UI_ITEM_R_SLIDER, nullptr, ICON_NONE);

    if (RNA_enum_get(ptr, "glare_type") == 0) {
      uiItemR(layout, ptr, "use_rotate_45", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
    }
  }
  if (RNA_enum_get(ptr, "glare_type") == 1) {
    uiItemR(layout, ptr, "size", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
  }
}

using namespace blender::realtime_compositor;

class GlareOperation : public NodeOperation {
 public:
  using NodeOperation::NodeOperation;

  void execute() override
  {
    get_input("Image").pass_through(get_result("Image"));
  }
};

static NodeOperation *get_compositor_operation(Context &context, DNode node)
{
  return new GlareOperation(context, node);
}

}  // namespace blender::nodes::node_composite_glare_cc

void register_node_type_cmp_glare()
{
  namespace file_ns = blender::nodes::node_composite_glare_cc;

  static bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_GLARE, "Glare", NODE_CLASS_OP_FILTER);
  ntype.declare = file_ns::cmp_node_glare_declare;
  ntype.draw_buttons = file_ns::node_composit_buts_glare;
  node_type_init(&ntype, file_ns::node_composit_init_glare);
  node_type_storage(&ntype, "NodeGlare", node_free_standard_storage, node_copy_standard_storage);
  ntype.get_compositor_operation = file_ns::get_compositor_operation;

  nodeRegisterType(&ntype);
}
