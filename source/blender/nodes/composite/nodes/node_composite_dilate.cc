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

/* **************** Dilate/Erode ******************** */

namespace blender::nodes::node_composite_dilate_cc {

static void cmp_node_dilate_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Float>(N_("Mask")).default_value(0.0f).min(0.0f).max(1.0f);
  b.add_output<decl::Float>(N_("Mask"));
}

static void node_composit_init_dilateerode(bNodeTree *UNUSED(ntree), bNode *node)
{
  NodeDilateErode *data = MEM_cnew<NodeDilateErode>(__func__);
  data->falloff = PROP_SMOOTH;
  node->storage = data;
}

static void node_composit_buts_dilateerode(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiItemR(layout, ptr, "mode", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
  uiItemR(layout, ptr, "distance", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
  switch (RNA_enum_get(ptr, "mode")) {
    case CMP_NODE_DILATEERODE_DISTANCE_THRESH:
      uiItemR(layout, ptr, "edge", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
      break;
    case CMP_NODE_DILATEERODE_DISTANCE_FEATHER:
      uiItemR(layout, ptr, "falloff", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
      break;
  }
}

using namespace blender::realtime_compositor;

class DilateErodeOperation : public NodeOperation {
 public:
  using NodeOperation::NodeOperation;

  void execute() override
  {
    get_input("Mask").pass_through(get_result("Mask"));
  }
};

static NodeOperation *get_compositor_operation(Context &context, DNode node)
{
  return new DilateErodeOperation(context, node);
}

}  // namespace blender::nodes::node_composite_dilate_cc

void register_node_type_cmp_dilateerode()
{
  namespace file_ns = blender::nodes::node_composite_dilate_cc;

  static bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_DILATEERODE, "Dilate/Erode", NODE_CLASS_OP_FILTER);
  ntype.draw_buttons = file_ns::node_composit_buts_dilateerode;
  ntype.declare = file_ns::cmp_node_dilate_declare;
  node_type_init(&ntype, file_ns::node_composit_init_dilateerode);
  node_type_storage(
      &ntype, "NodeDilateErode", node_free_standard_storage, node_copy_standard_storage);
  ntype.get_compositor_operation = file_ns::get_compositor_operation;

  nodeRegisterType(&ntype);
}
