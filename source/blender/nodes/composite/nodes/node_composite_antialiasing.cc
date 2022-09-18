/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2017 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup cmpnodes
 */

#include "UI_interface.h"
#include "UI_resources.h"

#include "COM_node_operation.hh"

#include "node_composite_util.hh"

/* **************** Anti-Aliasing (SMAA 1x) ******************** */

namespace blender::nodes::node_composite_antialiasing_cc {

static void cmp_node_antialiasing_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>(N_("Image")).default_value({1.0f, 1.0f, 1.0f, 1.0f});
  b.add_output<decl::Color>(N_("Image"));
}

static void node_composit_init_antialiasing(bNodeTree *UNUSED(ntree), bNode *node)
{
  NodeAntiAliasingData *data = MEM_cnew<NodeAntiAliasingData>(__func__);

  data->threshold = CMP_DEFAULT_SMAA_THRESHOLD;
  data->contrast_limit = CMP_DEFAULT_SMAA_CONTRAST_LIMIT;
  data->corner_rounding = CMP_DEFAULT_SMAA_CORNER_ROUNDING;

  node->storage = data;
}

static void node_composit_buts_antialiasing(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiLayout *col;

  col = uiLayoutColumn(layout, false);

  uiItemR(col, ptr, "threshold", 0, nullptr, ICON_NONE);
  uiItemR(col, ptr, "contrast_limit", 0, nullptr, ICON_NONE);
  uiItemR(col, ptr, "corner_rounding", 0, nullptr, ICON_NONE);
}

using namespace blender::realtime_compositor;

class AntiAliasingOperation : public NodeOperation {
 public:
  using NodeOperation::NodeOperation;

  void execute() override
  {
    get_input("Image").pass_through(get_result("Image"));
  }
};

static NodeOperation *get_compositor_operation(Context &context, DNode node)
{
  return new AntiAliasingOperation(context, node);
}

}  // namespace blender::nodes::node_composite_antialiasing_cc

void register_node_type_cmp_antialiasing()
{
  namespace file_ns = blender::nodes::node_composite_antialiasing_cc;

  static bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_ANTIALIASING, "Anti-Aliasing", NODE_CLASS_OP_FILTER);
  ntype.declare = file_ns::cmp_node_antialiasing_declare;
  ntype.draw_buttons = file_ns::node_composit_buts_antialiasing;
  ntype.flag |= NODE_PREVIEW;
  node_type_size(&ntype, 170, 140, 200);
  node_type_init(&ntype, file_ns::node_composit_init_antialiasing);
  node_type_storage(
      &ntype, "NodeAntiAliasingData", node_free_standard_storage, node_copy_standard_storage);
  ntype.get_compositor_operation = file_ns::get_compositor_operation;

  nodeRegisterType(&ntype);
}
