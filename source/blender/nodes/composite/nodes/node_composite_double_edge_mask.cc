/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2011 Blender Foundation */

/** \file
 * \ingroup cmpnodes
 */

#include "UI_interface.h"
#include "UI_resources.h"

#include "BLT_translation.h"

#include "COM_node_operation.hh"

#include "node_composite_util.hh"

/* **************** Double Edge Mask ******************** */

namespace blender::nodes::node_composite_double_edge_mask_cc {

static void cmp_node_double_edge_mask_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Float>("Inner Mask").default_value(0.8f).min(0.0f).max(1.0f);
  b.add_input<decl::Float>("Outer Mask").default_value(0.8f).min(0.0f).max(1.0f);
  b.add_output<decl::Float>("Mask");
}

static void node_composit_buts_double_edge_mask(uiLayout *layout,
                                                bContext * /*C*/,
                                                PointerRNA *ptr)
{
  uiLayout *col;

  col = uiLayoutColumn(layout, false);

  uiItemL(col, IFACE_("Inner Edge:"), ICON_NONE);
  uiItemR(col, ptr, "inner_mode", UI_ITEM_R_SPLIT_EMPTY_NAME, "", ICON_NONE);
  uiItemL(col, IFACE_("Buffer Edge:"), ICON_NONE);
  uiItemR(col, ptr, "edge_mode", UI_ITEM_R_SPLIT_EMPTY_NAME, "", ICON_NONE);
}

using namespace blender::realtime_compositor;

class DoubleEdgeMaskOperation : public NodeOperation {
 public:
  using NodeOperation::NodeOperation;

  void execute() override
  {
    get_input("Inner Mask").pass_through(get_result("Mask"));
    context().set_info_message("Viewport compositor setup not fully supported");
  }
};

static NodeOperation *get_compositor_operation(Context &context, DNode node)
{
  return new DoubleEdgeMaskOperation(context, node);
}

}  // namespace blender::nodes::node_composite_double_edge_mask_cc

void register_node_type_cmp_doubleedgemask()
{
  namespace file_ns = blender::nodes::node_composite_double_edge_mask_cc;

  static bNodeType ntype; /* Allocate a node type data structure. */

  cmp_node_type_base(&ntype, CMP_NODE_DOUBLEEDGEMASK, "Double Edge Mask", NODE_CLASS_MATTE);
  ntype.declare = file_ns::cmp_node_double_edge_mask_declare;
  ntype.draw_buttons = file_ns::node_composit_buts_double_edge_mask;
  ntype.get_compositor_operation = file_ns::get_compositor_operation;
  ntype.realtime_compositor_unsupported_message = N_(
      "Node not supported in the Viewport compositor");

  nodeRegisterType(&ntype);
}
