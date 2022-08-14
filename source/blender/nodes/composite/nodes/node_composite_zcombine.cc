/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2006 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup cmpnodes
 */

#include "UI_interface.h"
#include "UI_resources.h"

#include "COM_node_operation.hh"

#include "node_composite_util.hh"

/* **************** Z COMBINE ******************** */

namespace blender::nodes::node_composite_zcombine_cc {

static void cmp_node_zcombine_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>(N_("Image")).default_value({1.0f, 1.0f, 1.0f, 1.0f});
  b.add_input<decl::Float>(N_("Z")).default_value(1.0f).min(0.0f).max(10000.0f);
  b.add_input<decl::Color>(N_("Image"), "Image_001").default_value({1.0f, 1.0f, 1.0f, 1.0f});
  b.add_input<decl::Float>(N_("Z"), "Z_001").default_value(1.0f).min(0.0f).max(10000.0f);
  b.add_output<decl::Color>(N_("Image"));
  b.add_output<decl::Float>(N_("Z"));
}

static void node_composit_buts_zcombine(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiLayout *col;

  col = uiLayoutColumn(layout, true);
  uiItemR(col, ptr, "use_alpha", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
  uiItemR(col, ptr, "use_antialias_z", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
}

using namespace blender::realtime_compositor;

class ZCombineOperation : public NodeOperation {
 public:
  using NodeOperation::NodeOperation;

  void execute() override
  {
    get_input("Image").pass_through(get_result("Image"));
    get_result("Z").allocate_invalid();
  }
};

static NodeOperation *get_compositor_operation(Context &context, DNode node)
{
  return new ZCombineOperation(context, node);
}

}  // namespace blender::nodes::node_composite_zcombine_cc

void register_node_type_cmp_zcombine()
{
  namespace file_ns = blender::nodes::node_composite_zcombine_cc;

  static bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_ZCOMBINE, "Z Combine", NODE_CLASS_OP_COLOR);
  ntype.declare = file_ns::cmp_node_zcombine_declare;
  ntype.draw_buttons = file_ns::node_composit_buts_zcombine;
  ntype.get_compositor_operation = file_ns::get_compositor_operation;

  nodeRegisterType(&ntype);
}
