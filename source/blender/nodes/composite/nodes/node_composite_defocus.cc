/* SPDX-FileCopyrightText: 2006 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup cmpnodes
 */

#include <climits>

#include "RNA_access.h"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "COM_node_operation.hh"

#include "node_composite_util.hh"

/* ************ Defocus Node ****************** */

namespace blender::nodes::node_composite_defocus_cc {

static void cmp_node_defocus_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>("Image").default_value({1.0f, 1.0f, 1.0f, 1.0f});
  b.add_input<decl::Float>("Z").default_value(1.0f).min(0.0f).max(1.0f);
  b.add_output<decl::Color>("Image");
}

static void node_composit_init_defocus(bNodeTree * /*ntree*/, bNode *node)
{
  /* defocus node */
  NodeDefocus *nbd = MEM_cnew<NodeDefocus>(__func__);
  nbd->bktype = 0;
  nbd->rotation = 0.0f;
  nbd->preview = 1;
  nbd->gamco = 0;
  nbd->samples = 16;
  nbd->fstop = 128.0f;
  nbd->maxblur = 16;
  nbd->bthresh = 1.0f;
  nbd->scale = 1.0f;
  nbd->no_zbuf = 1;
  node->storage = nbd;
}

static void node_composit_buts_defocus(uiLayout *layout, bContext *C, PointerRNA *ptr)
{
  uiLayout *sub, *col;

  col = uiLayoutColumn(layout, false);
  uiItemL(col, IFACE_("Bokeh Type:"), ICON_NONE);
  uiItemR(col, ptr, "bokeh", UI_ITEM_R_SPLIT_EMPTY_NAME, "", ICON_NONE);
  uiItemR(col, ptr, "angle", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);

  uiItemR(layout, ptr, "use_gamma_correction", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);

  col = uiLayoutColumn(layout, false);
  uiLayoutSetActive(col, RNA_boolean_get(ptr, "use_zbuffer") == true);
  uiItemR(col, ptr, "f_stop", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);

  uiItemR(layout, ptr, "blur_max", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
  uiItemR(layout, ptr, "threshold", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);

  col = uiLayoutColumn(layout, false);
  uiItemR(col, ptr, "use_preview", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);

  uiTemplateID(layout,
               C,
               ptr,
               "scene",
               nullptr,
               nullptr,
               nullptr,
               UI_TEMPLATE_ID_FILTER_ALL,
               false,
               nullptr);

  col = uiLayoutColumn(layout, false);
  uiItemR(col, ptr, "use_zbuffer", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
  sub = uiLayoutColumn(col, false);
  uiLayoutSetActive(sub, RNA_boolean_get(ptr, "use_zbuffer") == false);
  uiItemR(sub, ptr, "z_scale", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
}

using namespace blender::realtime_compositor;

class DefocusOperation : public NodeOperation {
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
  return new DefocusOperation(context, node);
}

}  // namespace blender::nodes::node_composite_defocus_cc

void register_node_type_cmp_defocus()
{
  namespace file_ns = blender::nodes::node_composite_defocus_cc;

  static bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_DEFOCUS, "Defocus", NODE_CLASS_OP_FILTER);
  ntype.declare = file_ns::cmp_node_defocus_declare;
  ntype.draw_buttons = file_ns::node_composit_buts_defocus;
  ntype.initfunc = file_ns::node_composit_init_defocus;
  node_type_storage(&ntype, "NodeDefocus", node_free_standard_storage, node_copy_standard_storage);
  ntype.get_compositor_operation = file_ns::get_compositor_operation;
  ntype.realtime_compositor_unsupported_message = N_(
      "Node not supported in the Viewport compositor");

  nodeRegisterType(&ntype);
}
