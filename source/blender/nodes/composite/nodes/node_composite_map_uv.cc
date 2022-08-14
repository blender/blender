/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2006 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup cmpnodes
 */

#include "UI_interface.h"
#include "UI_resources.h"

#include "COM_node_operation.hh"

#include "node_composite_util.hh"

/* **************** Map UV  ******************** */

namespace blender::nodes::node_composite_map_uv_cc {

static void cmp_node_map_uv_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>(N_("Image")).default_value({1.0f, 1.0f, 1.0f, 1.0f});
  b.add_input<decl::Vector>(N_("UV")).default_value({1.0f, 0.0f, 0.0f}).min(0.0f).max(1.0f);
  b.add_output<decl::Color>(N_("Image"));
}

static void node_composit_buts_map_uv(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiItemR(layout, ptr, "alpha", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
}

using namespace blender::realtime_compositor;

class MapUVOperation : public NodeOperation {
 public:
  using NodeOperation::NodeOperation;

  void execute() override
  {
    get_input("Image").pass_through(get_result("Image"));
  }
};

static NodeOperation *get_compositor_operation(Context &context, DNode node)
{
  return new MapUVOperation(context, node);
}

}  // namespace blender::nodes::node_composite_map_uv_cc

void register_node_type_cmp_mapuv()
{
  namespace file_ns = blender::nodes::node_composite_map_uv_cc;

  static bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_MAP_UV, "Map UV", NODE_CLASS_DISTORT);
  ntype.declare = file_ns::cmp_node_map_uv_declare;
  ntype.draw_buttons = file_ns::node_composit_buts_map_uv;
  ntype.get_compositor_operation = file_ns::get_compositor_operation;

  nodeRegisterType(&ntype);
}
