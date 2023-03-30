/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2014 Blender Foundation */

/** \file
 * \ingroup cmpnodes
 */

#include "BLT_translation.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "COM_node_operation.hh"

#include "node_composite_util.hh"

namespace blender::nodes::node_composite_sunbeams_cc {

static void cmp_node_sunbeams_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>(N_("Image")).default_value({1.0f, 1.0f, 1.0f, 1.0f});
  b.add_output<decl::Color>(N_("Image"));
}

static void init(bNodeTree * /*ntree*/, bNode *node)
{
  NodeSunBeams *data = MEM_cnew<NodeSunBeams>(__func__);

  data->source[0] = 0.5f;
  data->source[1] = 0.5f;
  node->storage = data;
}

static void node_composit_buts_sunbeams(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiItemR(layout, ptr, "source", UI_ITEM_R_SPLIT_EMPTY_NAME | UI_ITEM_R_EXPAND, "", ICON_NONE);
  uiItemR(layout,
          ptr,
          "ray_length",
          UI_ITEM_R_SPLIT_EMPTY_NAME | UI_ITEM_R_SLIDER,
          nullptr,
          ICON_NONE);
}

using namespace blender::realtime_compositor;

class SunBeamsOperation : public NodeOperation {
 public:
  using NodeOperation::NodeOperation;

  void execute() override
  {
    get_input("Image").pass_through(get_result("Image"));
    context().set_info_message("Viewport compositor setup not fully supported");
  }
};

static NodeOperation *get_compositor_operation(Context &context, DNode node)
{
  return new SunBeamsOperation(context, node);
}

}  // namespace blender::nodes::node_composite_sunbeams_cc

void register_node_type_cmp_sunbeams()
{
  namespace file_ns = blender::nodes::node_composite_sunbeams_cc;

  static bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_SUNBEAMS, "Sun Beams", NODE_CLASS_OP_FILTER);
  ntype.declare = file_ns::cmp_node_sunbeams_declare;
  ntype.draw_buttons = file_ns::node_composit_buts_sunbeams;
  ntype.initfunc = file_ns::init;
  node_type_storage(
      &ntype, "NodeSunBeams", node_free_standard_storage, node_copy_standard_storage);
  ntype.get_compositor_operation = file_ns::get_compositor_operation;
  ntype.realtime_compositor_unsupported_message = N_(
      "Node not supported in the Viewport compositor");

  nodeRegisterType(&ntype);
}
