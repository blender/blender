/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2006 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup cmpnodes
 */

#include "UI_interface.h"
#include "UI_resources.h"

#include "COM_node_operation.hh"

#include "node_composite_util.hh"

/* **************** ID Mask  ******************** */

namespace blender::nodes::node_composite_id_mask_cc {

static void cmp_node_idmask_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Float>(N_("ID value")).default_value(1.0f).min(0.0f).max(1.0f);
  b.add_output<decl::Float>(N_("Alpha"));
}

static void node_composit_buts_id_mask(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiItemR(layout, ptr, "index", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
  uiItemR(layout, ptr, "use_antialiasing", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
}

using namespace blender::realtime_compositor;

class IDMaskOperation : public NodeOperation {
 public:
  using NodeOperation::NodeOperation;

  void execute() override
  {
    get_input("ID value").pass_through(get_result("Alpha"));
  }
};

static NodeOperation *get_compositor_operation(Context &context, DNode node)
{
  return new IDMaskOperation(context, node);
}

}  // namespace blender::nodes::node_composite_id_mask_cc

void register_node_type_cmp_idmask()
{
  namespace file_ns = blender::nodes::node_composite_id_mask_cc;

  static bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_ID_MASK, "ID Mask", NODE_CLASS_CONVERTER);
  ntype.declare = file_ns::cmp_node_idmask_declare;
  ntype.draw_buttons = file_ns::node_composit_buts_id_mask;
  ntype.get_compositor_operation = file_ns::get_compositor_operation;

  nodeRegisterType(&ntype);
}
