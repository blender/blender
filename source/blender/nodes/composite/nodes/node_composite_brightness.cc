/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2006 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup cmpnodes
 */

#include "UI_interface.h"
#include "UI_resources.h"

#include "node_composite_util.hh"

/* **************** Bright and Contrast  ******************** */

namespace blender::nodes::node_composite_brightness_cc {

static void cmp_node_brightcontrast_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>(N_("Image")).default_value({1.0f, 1.0f, 1.0f, 1.0f});
  b.add_input<decl::Float>(N_("Bright")).min(-100.0f).max(100.0f);
  b.add_input<decl::Float>(N_("Contrast")).min(-100.0f).max(100.0f);
  b.add_output<decl::Color>(N_("Image"));
}

static void node_composit_init_brightcontrast(bNodeTree *UNUSED(ntree), bNode *node)
{
  node->custom1 = 1;
}

static void node_composit_buts_brightcontrast(uiLayout *layout,
                                              bContext *UNUSED(C),
                                              PointerRNA *ptr)
{
  uiItemR(layout, ptr, "use_premultiply", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
}

}  // namespace blender::nodes::node_composite_brightness_cc

void register_node_type_cmp_brightcontrast()
{
  namespace file_ns = blender::nodes::node_composite_brightness_cc;

  static bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_BRIGHTCONTRAST, "Bright/Contrast", NODE_CLASS_OP_COLOR);
  ntype.declare = file_ns::cmp_node_brightcontrast_declare;
  ntype.draw_buttons = file_ns::node_composit_buts_brightcontrast;
  node_type_init(&ntype, file_ns::node_composit_init_brightcontrast);

  nodeRegisterType(&ntype);
}
