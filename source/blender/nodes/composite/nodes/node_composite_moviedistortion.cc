/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2011 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup cmpnodes
 */

#include "BKE_context.h"
#include "BKE_lib_id.h"
#include "BKE_tracking.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "node_composite_util.hh"

/* **************** Translate  ******************** */

namespace blender::nodes::node_composite_moviedistortion_cc {

static void cmp_node_moviedistortion_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>(N_("Image")).default_value({0.8f, 0.8f, 0.8f, 1.0f});
  b.add_output<decl::Color>(N_("Image"));
}

static void label(const bNodeTree *UNUSED(ntree), const bNode *node, char *label, int maxlen)
{
  if (node->custom1 == 0) {
    BLI_strncpy(label, IFACE_("Undistortion"), maxlen);
  }
  else {
    BLI_strncpy(label, IFACE_("Distortion"), maxlen);
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

static void storage_copy(bNodeTree *UNUSED(dest_ntree), bNode *dest_node, const bNode *src_node)
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

  nodeRegisterType(&ntype);
}
