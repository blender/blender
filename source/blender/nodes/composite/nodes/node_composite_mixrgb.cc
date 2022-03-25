/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2006 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup cmpnodes
 */

#include "node_composite_util.hh"

/* **************** MIX RGB ******************** */

namespace blender::nodes::node_composite_mixrgb_cc {

static void cmp_node_mixrgb_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Float>(N_("Fac")).default_value(1.0f).min(0.0f).max(1.0f).subtype(PROP_FACTOR);
  b.add_input<decl::Color>(N_("Image")).default_value({1.0f, 1.0f, 1.0f, 1.0f});
  b.add_input<decl::Color>(N_("Image"), "Image_001").default_value({1.0f, 1.0f, 1.0f, 1.0f});
  b.add_output<decl::Color>(N_("Image"));
}

}  // namespace blender::nodes::node_composite_mixrgb_cc

void register_node_type_cmp_mix_rgb()
{
  namespace file_ns = blender::nodes::node_composite_mixrgb_cc;

  static bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_MIX_RGB, "Mix", NODE_CLASS_OP_COLOR);
  ntype.flag |= NODE_PREVIEW;
  ntype.declare = file_ns::cmp_node_mixrgb_declare;
  ntype.labelfunc = node_blend_label;

  nodeRegisterType(&ntype);
}
