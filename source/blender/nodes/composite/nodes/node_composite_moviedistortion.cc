/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2011 Blender Foundation */

/** \file
 * \ingroup cmpnodes
 */

#include "BLT_translation.h"

#include "BLI_string_utf8.h"

#include "BKE_context.h"
#include "BKE_lib_id.h"
#include "BKE_tracking.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "COM_node_operation.hh"

#include "node_composite_util.hh"

/* **************** Translate  ******************** */

namespace blender::nodes::node_composite_moviedistortion_cc {

static void cmp_node_moviedistortion_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>(N_("Image")).default_value({0.8f, 0.8f, 0.8f, 1.0f});
  b.add_output<decl::Color>(N_("Image"));
}

static void label(const bNodeTree * /*ntree*/, const bNode *node, char *label, int label_maxncpy)
{
  if (node->custom1 == 0) {
    BLI_strncpy_utf8(label, IFACE_("Undistortion"), label_maxncpy);
  }
  else {
    BLI_strncpy_utf8(label, IFACE_("Distortion"), label_maxncpy);
  }
}

static void init(const bContext *C, PointerRNA *ptr)
{
  bNode *node = (bNode *)ptr->data;
  Scene *scene = CTX_data_scene(C);

  node->id = (ID *)scene->clip;
  id_us_plus(node->id);
}

static void storage_free(bNode *node)
{
  if (node->storage) {
    BKE_tracking_distortion_free((MovieDistortion *)node->storage);
  }

  node->storage = nullptr;
}

static void storage_copy(bNodeTree * /*dst_ntree*/, bNode *dest_node, const bNode *src_node)
{
  if (src_node->storage) {
    dest_node->storage = BKE_tracking_distortion_copy((MovieDistortion *)src_node->storage);
  }
}

static void node_composit_buts_moviedistortion(uiLayout *layout, bContext *C, PointerRNA *ptr)
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

  uiItemR(layout, ptr, "distortion_type", UI_ITEM_R_SPLIT_EMPTY_NAME, "", ICON_NONE);
}

using namespace blender::realtime_compositor;

class MovieDistortionOperation : public NodeOperation {
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
  return new MovieDistortionOperation(context, node);
}

}  // namespace blender::nodes::node_composite_moviedistortion_cc

void register_node_type_cmp_moviedistortion()
{
  namespace file_ns = blender::nodes::node_composite_moviedistortion_cc;

  static bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_MOVIEDISTORTION, "Movie Distortion", NODE_CLASS_DISTORT);
  ntype.declare = file_ns::cmp_node_moviedistortion_declare;
  ntype.draw_buttons = file_ns::node_composit_buts_moviedistortion;
  ntype.labelfunc = file_ns::label;
  ntype.initfunc_api = file_ns::init;
  node_type_storage(&ntype, nullptr, file_ns::storage_free, file_ns::storage_copy);
  ntype.get_compositor_operation = file_ns::get_compositor_operation;
  ntype.realtime_compositor_unsupported_message = N_(
      "Node not supported in the Viewport compositor");

  nodeRegisterType(&ntype);
}
