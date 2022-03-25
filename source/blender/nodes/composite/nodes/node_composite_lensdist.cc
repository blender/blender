/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2006 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup cmpnodes
 */

#include "RNA_access.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "node_composite_util.hh"

namespace blender::nodes::node_composite_lensdist_cc {

static void cmp_node_lensdist_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>(N_("Image")).default_value({1.0f, 1.0f, 1.0f, 1.0f});
  b.add_input<decl::Float>(N_("Distort")).default_value(0.0f).min(-0.999f).max(1.0f);
  b.add_input<decl::Float>(N_("Dispersion")).default_value(0.0f).min(0.0f).max(1.0f);
  b.add_output<decl::Color>(N_("Image"));
}

static void node_composit_init_lensdist(bNodeTree *UNUSED(ntree), bNode *node)
{
  NodeLensDist *nld = MEM_cnew<NodeLensDist>(__func__);
  nld->jit = nld->proj = nld->fit = 0;
  node->storage = nld;
}

static void node_composit_buts_lensdist(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiLayout *col;

  col = uiLayoutColumn(layout, false);
  uiItemR(col, ptr, "use_projector", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);

  col = uiLayoutColumn(col, false);
  uiLayoutSetActive(col, RNA_boolean_get(ptr, "use_projector") == false);
  uiItemR(col, ptr, "use_jitter", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
  uiItemR(col, ptr, "use_fit", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
}

}  // namespace blender::nodes::node_composite_lensdist_cc

void register_node_type_cmp_lensdist()
{
  namespace file_ns = blender::nodes::node_composite_lensdist_cc;

  static bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_LENSDIST, "Lens Distortion", NODE_CLASS_DISTORT);
  ntype.declare = file_ns::cmp_node_lensdist_declare;
  ntype.draw_buttons = file_ns::node_composit_buts_lensdist;
  node_type_init(&ntype, file_ns::node_composit_init_lensdist);
  node_type_storage(
      &ntype, "NodeLensDist", node_free_standard_storage, node_copy_standard_storage);

  nodeRegisterType(&ntype);
}
