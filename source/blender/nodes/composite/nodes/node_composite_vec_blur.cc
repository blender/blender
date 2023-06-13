/* SPDX-FileCopyrightText: 2006 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup cmpnodes
 */

#include "UI_interface.h"
#include "UI_resources.h"

#include "COM_node_operation.hh"

#include "node_composite_util.hh"

/* **************** VECTOR BLUR ******************** */

namespace blender::nodes::node_composite_vec_blur_cc {

static void cmp_node_vec_blur_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>("Image").default_value({1.0f, 1.0f, 1.0f, 1.0f});
  b.add_input<decl::Float>("Z").default_value(0.0f).min(0.0f).max(1.0f);
  b.add_input<decl::Vector>("Speed")
      .default_value({0.0f, 0.0f, 0.0f})
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_VELOCITY);
  b.add_output<decl::Color>("Image");
}

/* custom1: iterations, custom2: max_speed (0 = no_limit). */
static void node_composit_init_vecblur(bNodeTree * /*ntree*/, bNode *node)
{
  NodeBlurData *nbd = MEM_cnew<NodeBlurData>(__func__);
  node->storage = nbd;
  nbd->samples = 32;
  nbd->fac = 1.0f;
}

static void node_composit_buts_vecblur(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiLayout *col;

  col = uiLayoutColumn(layout, false);
  uiItemR(col, ptr, "samples", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
  uiItemR(col, ptr, "factor", UI_ITEM_R_SPLIT_EMPTY_NAME, IFACE_("Blur"), ICON_NONE);

  col = uiLayoutColumn(layout, true);
  uiItemL(col, IFACE_("Speed:"), ICON_NONE);
  uiItemR(col, ptr, "speed_min", UI_ITEM_R_SPLIT_EMPTY_NAME, IFACE_("Min"), ICON_NONE);
  uiItemR(col, ptr, "speed_max", UI_ITEM_R_SPLIT_EMPTY_NAME, IFACE_("Max"), ICON_NONE);

  uiItemR(layout, ptr, "use_curved", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
}

using namespace blender::realtime_compositor;

class VectorBlurOperation : public NodeOperation {
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
  return new VectorBlurOperation(context, node);
}

}  // namespace blender::nodes::node_composite_vec_blur_cc

void register_node_type_cmp_vecblur()
{
  namespace file_ns = blender::nodes::node_composite_vec_blur_cc;

  static bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_VECBLUR, "Vector Blur", NODE_CLASS_OP_FILTER);
  ntype.declare = file_ns::cmp_node_vec_blur_declare;
  ntype.draw_buttons = file_ns::node_composit_buts_vecblur;
  ntype.initfunc = file_ns::node_composit_init_vecblur;
  node_type_storage(
      &ntype, "NodeBlurData", node_free_standard_storage, node_copy_standard_storage);
  ntype.get_compositor_operation = file_ns::get_compositor_operation;
  ntype.realtime_compositor_unsupported_message = N_(
      "Node not supported in the Viewport compositor");

  nodeRegisterType(&ntype);
}
