/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2011 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup cmpnodes
 */

#include "UI_interface.h"
#include "UI_resources.h"

#include "BKE_context.h"
#include "BKE_lib_id.h"

#include "COM_node_operation.hh"

#include "node_composite_util.hh"

/* **************** Stabilize 2D ******************** */

namespace blender::nodes::node_composite_stabilize2d_cc {

static void cmp_node_stabilize2d_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>(N_("Image")).default_value({0.8f, 0.8f, 0.8f, 1.0f});
  b.add_output<decl::Color>(N_("Image"));
}

static void init(const bContext *C, PointerRNA *ptr)
{
  bNode *node = (bNode *)ptr->data;
  Scene *scene = CTX_data_scene(C);

  node->id = (ID *)scene->clip;
  id_us_plus(node->id);

  /* default to bilinear, see node_sampler_type_items in rna_nodetree.c */
  node->custom1 = 1;
}

static void node_composit_buts_stabilize2d(uiLayout *layout, bContext *C, PointerRNA *ptr)
{
  bNode *node = (bNode *)ptr->data;

  uiTemplateID(layout,
               C,
               ptr,
               "clip",
               nullptr,
               "CLIP_OT_open",
               nullptr,
               UI_TEMPLATE_ID_FILTER_ALL,
               false,
               nullptr);

  if (!node->id) {
    return;
  }

  uiItemR(layout, ptr, "filter_type", UI_ITEM_R_SPLIT_EMPTY_NAME, "", ICON_NONE);
  uiItemR(layout, ptr, "invert", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
}

using namespace blender::realtime_compositor;

class Stabilize2DOperation : public NodeOperation {
 public:
  using NodeOperation::NodeOperation;

  void execute() override
  {
    get_input("Image").pass_through(get_result("Image"));
  }
};

static NodeOperation *get_compositor_operation(Context &context, DNode node)
{
  return new Stabilize2DOperation(context, node);
}

}  // namespace blender::nodes::node_composite_stabilize2d_cc

void register_node_type_cmp_stabilize2d()
{
  namespace file_ns = blender::nodes::node_composite_stabilize2d_cc;

  static bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_STABILIZE2D, "Stabilize 2D", NODE_CLASS_DISTORT);
  ntype.declare = file_ns::cmp_node_stabilize2d_declare;
  ntype.draw_buttons = file_ns::node_composit_buts_stabilize2d;
  ntype.initfunc_api = file_ns::init;
  ntype.get_compositor_operation = file_ns::get_compositor_operation;

  nodeRegisterType(&ntype);
}
