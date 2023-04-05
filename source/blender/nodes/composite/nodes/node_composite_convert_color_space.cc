/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2021 Blender Foundation */

/** \file
 * \ingroup cmpnodes
 */

#include "node_composite_util.hh"

#include "BLT_translation.h"

#include "RNA_access.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "IMB_colormanagement.h"

#include "COM_node_operation.hh"

namespace blender::nodes::node_composite_convert_color_space_cc {

static void CMP_NODE_CONVERT_COLOR_SPACE_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>(N_("Image")).default_value({1.0f, 1.0f, 1.0f, 1.0f});
  b.add_output<decl::Color>(N_("Image"));
}

static void node_composit_init_convert_colorspace(bNodeTree * /*ntree*/, bNode *node)
{
  NodeConvertColorSpace *ncs = static_cast<NodeConvertColorSpace *>(
      MEM_callocN(sizeof(NodeConvertColorSpace), "node colorspace"));
  const char *first_colorspace = IMB_colormanagement_role_colorspace_name_get(
      COLOR_ROLE_SCENE_LINEAR);
  if (first_colorspace && first_colorspace[0]) {
    STRNCPY(ncs->from_color_space, first_colorspace);
    STRNCPY(ncs->to_color_space, first_colorspace);
  }
  else {
    ncs->from_color_space[0] = 0;
    ncs->to_color_space[0] = 0;
  }
  node->storage = ncs;
}

static void node_composit_buts_convert_colorspace(uiLayout *layout,
                                                  bContext * /*C*/,
                                                  PointerRNA *ptr)
{
  uiItemR(layout, ptr, "from_color_space", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
  uiItemR(layout, ptr, "to_color_space", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
}

using namespace blender::realtime_compositor;

class ConvertColorSpaceOperation : public NodeOperation {
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
  return new ConvertColorSpaceOperation(context, node);
}

}  // namespace blender::nodes::node_composite_convert_color_space_cc

void register_node_type_cmp_convert_color_space(void)
{
  namespace file_ns = blender::nodes::node_composite_convert_color_space_cc;
  static bNodeType ntype;

  cmp_node_type_base(
      &ntype, CMP_NODE_CONVERT_COLOR_SPACE, "Convert Colorspace", NODE_CLASS_CONVERTER);
  ntype.declare = file_ns::CMP_NODE_CONVERT_COLOR_SPACE_declare;
  ntype.draw_buttons = file_ns::node_composit_buts_convert_colorspace;
  node_type_size_preset(&ntype, NODE_SIZE_MIDDLE);
  ntype.initfunc = file_ns::node_composit_init_convert_colorspace;
  node_type_storage(
      &ntype, "NodeConvertColorSpace", node_free_standard_storage, node_copy_standard_storage);
  ntype.get_compositor_operation = file_ns::get_compositor_operation;
  ntype.realtime_compositor_unsupported_message = N_(
      "Node not supported in the Viewport compositor");

  nodeRegisterType(&ntype);
}
