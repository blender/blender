/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2006 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup cmpnodes
 */

#include "UI_interface.h"
#include "UI_resources.h"

#include "COM_node_operation.hh"

#include "node_composite_util.hh"

/* **************** LEVELS ******************** */

namespace blender::nodes::node_composite_levels_cc {

static void cmp_node_levels_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>(N_("Image")).default_value({0.0f, 0.0f, 0.0f, 1.0f});
  b.add_output<decl::Float>(N_("Mean"));
  b.add_output<decl::Float>(N_("Std Dev"));
}

static void node_composit_init_view_levels(bNodeTree *UNUSED(ntree), bNode *node)
{
  node->custom1 = 1; /* All channels. */
}

static void node_composit_buts_view_levels(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiItemR(layout, ptr, "channel", UI_ITEM_R_SPLIT_EMPTY_NAME, "", ICON_NONE);
}

using namespace blender::realtime_compositor;

class LevelsOperation : public NodeOperation {
 public:
  using NodeOperation::NodeOperation;

  void execute() override
  {
    get_result("Mean").allocate_invalid();
    get_result("Std Dev").allocate_invalid();
  }
};

static NodeOperation *get_compositor_operation(Context &context, DNode node)
{
  return new LevelsOperation(context, node);
}

}  // namespace blender::nodes::node_composite_levels_cc

void register_node_type_cmp_view_levels()
{
  namespace file_ns = blender::nodes::node_composite_levels_cc;

  static bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_VIEW_LEVELS, "Levels", NODE_CLASS_OUTPUT);
  ntype.declare = file_ns::cmp_node_levels_declare;
  ntype.draw_buttons = file_ns::node_composit_buts_view_levels;
  ntype.flag |= NODE_PREVIEW;
  node_type_init(&ntype, file_ns::node_composit_init_view_levels);
  ntype.get_compositor_operation = file_ns::get_compositor_operation;

  nodeRegisterType(&ntype);
}
