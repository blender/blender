/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2019 Blender Foundation */

/** \file
 * \ingroup cmpnodes
 */

#include "BLI_system.h"

#include "BLT_translation.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "COM_node_operation.hh"

#include "node_composite_util.hh"

namespace blender::nodes::node_composite_denoise_cc {

static void cmp_node_denoise_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>(N_("Image")).default_value({1.0f, 1.0f, 1.0f, 1.0f});
  b.add_input<decl::Vector>(N_("Normal"))
      .default_value({0.0f, 0.0f, 0.0f})
      .min(-1.0f)
      .max(1.0f)
      .hide_value();
  b.add_input<decl::Color>(N_("Albedo")).default_value({1.0f, 1.0f, 1.0f, 1.0f}).hide_value();
  b.add_output<decl::Color>(N_("Image"));
}

static void node_composit_init_denonise(bNodeTree * /*ntree*/, bNode *node)
{
  NodeDenoise *ndg = MEM_cnew<NodeDenoise>(__func__);
  ndg->hdr = true;
  ndg->prefilter = CMP_NODE_DENOISE_PREFILTER_ACCURATE;
  node->storage = ndg;
}

static void node_composit_buts_denoise(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
#ifndef WITH_OPENIMAGEDENOISE
  uiItemL(layout, IFACE_("Disabled, built without OpenImageDenoise"), ICON_ERROR);
#else
  /* Always supported through Accelerate framework BNNS on macOS. */
#  ifndef __APPLE__
  if (!BLI_cpu_support_sse41()) {
    uiItemL(layout, IFACE_("Disabled, CPU with SSE4.1 is required"), ICON_ERROR);
  }
#  endif
#endif

  uiItemL(layout, IFACE_("Prefilter:"), ICON_NONE);
  uiItemR(layout, ptr, "prefilter", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
  uiItemR(layout, ptr, "use_hdr", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
}

using namespace blender::realtime_compositor;

class DenoiseOperation : public NodeOperation {
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
  return new DenoiseOperation(context, node);
}

}  // namespace blender::nodes::node_composite_denoise_cc

void register_node_type_cmp_denoise()
{
  namespace file_ns = blender::nodes::node_composite_denoise_cc;

  static bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_DENOISE, "Denoise", NODE_CLASS_OP_FILTER);
  ntype.declare = file_ns::cmp_node_denoise_declare;
  ntype.draw_buttons = file_ns::node_composit_buts_denoise;
  ntype.initfunc = file_ns::node_composit_init_denonise;
  node_type_storage(&ntype, "NodeDenoise", node_free_standard_storage, node_copy_standard_storage);
  ntype.get_compositor_operation = file_ns::get_compositor_operation;
  ntype.realtime_compositor_unsupported_message = N_(
      "Node not supported in the Viewport compositor");

  nodeRegisterType(&ntype);
}
