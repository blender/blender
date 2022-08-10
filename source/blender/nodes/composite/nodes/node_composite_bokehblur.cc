/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2006 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup cmpnodes
 */

#include "UI_interface.h"
#include "UI_resources.h"

#include "COM_node_operation.hh"

#include "node_composite_util.hh"

/* **************** BLUR ******************** */

namespace blender::nodes::node_composite_bokehblur_cc {

static void cmp_node_bokehblur_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>(N_("Image")).default_value({0.8f, 0.8f, 0.8f, 1.0f});
  b.add_input<decl::Color>(N_("Bokeh")).default_value({1.0f, 1.0f, 1.0f, 1.0f});
  b.add_input<decl::Float>(N_("Size")).default_value(1.0f).min(0.0f).max(10.0f);
  b.add_input<decl::Float>(N_("Bounding box")).default_value(1.0f).min(0.0f).max(1.0f);
  b.add_output<decl::Color>(N_("Image"));
}

static void node_composit_init_bokehblur(bNodeTree *UNUSED(ntree), bNode *node)
{
  node->custom3 = 4.0f;
  node->custom4 = 16.0f;
}

static void node_composit_buts_bokehblur(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiItemR(layout, ptr, "use_variable_size", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
  // uiItemR(layout, ptr, "f_stop", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE); /* UNUSED */
  uiItemR(layout, ptr, "blur_max", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
  uiItemR(layout, ptr, "use_extended_bounds", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
}

using namespace blender::realtime_compositor;

class BokehBlurOperation : public NodeOperation {
 public:
  using NodeOperation::NodeOperation;

  void execute() override
  {
    get_input("Image").pass_through(get_result("Image"));
  }
};

static NodeOperation *get_compositor_operation(Context &context, DNode node)
{
  return new BokehBlurOperation(context, node);
}

}  // namespace blender::nodes::node_composite_bokehblur_cc

void register_node_type_cmp_bokehblur()
{
  namespace file_ns = blender::nodes::node_composite_bokehblur_cc;

  static bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_BOKEHBLUR, "Bokeh Blur", NODE_CLASS_OP_FILTER);
  ntype.declare = file_ns::cmp_node_bokehblur_declare;
  ntype.draw_buttons = file_ns::node_composit_buts_bokehblur;
  node_type_init(&ntype, file_ns::node_composit_init_bokehblur);
  ntype.get_compositor_operation = file_ns::get_compositor_operation;

  nodeRegisterType(&ntype);
}
