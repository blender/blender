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

/* **************** Scale  ******************** */

namespace blender::nodes::node_composite_scale_cc {

static void cmp_node_scale_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>(N_("Image")).default_value({1.0f, 1.0f, 1.0f, 1.0f});
  b.add_input<decl::Float>(N_("X")).default_value(1.0f).min(0.0001f).max(CMP_SCALE_MAX);
  b.add_input<decl::Float>(N_("Y")).default_value(1.0f).min(0.0001f).max(CMP_SCALE_MAX);
  b.add_output<decl::Color>(N_("Image"));
}

static void node_composite_update_scale(bNodeTree *ntree, bNode *node)
{
  bNodeSocket *sock;
  bool use_xy_scale = ELEM(node->custom1, CMP_SCALE_RELATIVE, CMP_SCALE_ABSOLUTE);

  /* Only show X/Y scale factor inputs for modes using them! */
  for (sock = (bNodeSocket *)node->inputs.first; sock; sock = sock->next) {
    if (STR_ELEM(sock->name, "X", "Y")) {
      nodeSetSocketAvailability(ntree, sock, use_xy_scale);
    }
  }
}

static void node_composit_buts_scale(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiItemR(layout, ptr, "space", UI_ITEM_R_SPLIT_EMPTY_NAME, "", ICON_NONE);

  if (RNA_enum_get(ptr, "space") == CMP_SCALE_RENDERPERCENT) {
    uiLayout *row;
    uiItemR(layout,
            ptr,
            "frame_method",
            UI_ITEM_R_SPLIT_EMPTY_NAME | UI_ITEM_R_EXPAND,
            nullptr,
            ICON_NONE);
    row = uiLayoutRow(layout, true);
    uiItemR(row, ptr, "offset_x", UI_ITEM_R_SPLIT_EMPTY_NAME, "X", ICON_NONE);
    uiItemR(row, ptr, "offset_y", UI_ITEM_R_SPLIT_EMPTY_NAME, "Y", ICON_NONE);
  }
}

using namespace blender::realtime_compositor;

class ScaleOperation : public NodeOperation {
 public:
  using NodeOperation::NodeOperation;

  void execute() override
  {
    get_input("Image").pass_through(get_result("Image"));
  }
};

static NodeOperation *get_compositor_operation(Context &context, DNode node)
{
  return new ScaleOperation(context, node);
}

}  // namespace blender::nodes::node_composite_scale_cc

void register_node_type_cmp_scale()
{
  namespace file_ns = blender::nodes::node_composite_scale_cc;

  static bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_SCALE, "Scale", NODE_CLASS_DISTORT);
  ntype.declare = file_ns::cmp_node_scale_declare;
  ntype.draw_buttons = file_ns::node_composit_buts_scale;
  node_type_update(&ntype, file_ns::node_composite_update_scale);
  ntype.get_compositor_operation = file_ns::get_compositor_operation;

  nodeRegisterType(&ntype);
}
