/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2006 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup cmpnodes
 */

#include "node_composite_util.hh"

/* **************** Gamma Tools  ******************** */

namespace blender::nodes::node_composite_gamma_cc {

static void cmp_node_gamma_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>(N_("Image")).default_value({1.0f, 1.0f, 1.0f, 1.0f});
  b.add_input<decl::Float>(N_("Gamma"))
      .default_value(1.0f)
      .min(0.001f)
      .max(10.0f)
      .subtype(PROP_UNSIGNED);
  b.add_output<decl::Color>(N_("Image"));
}

}  // namespace blender::nodes::node_composite_gamma_cc

void register_node_type_cmp_gamma()
{
  namespace file_ns = blender::nodes::node_composite_gamma_cc;

  static bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_GAMMA, "Gamma", NODE_CLASS_OP_COLOR);
  ntype.declare = file_ns::cmp_node_gamma_declare;

  nodeRegisterType(&ntype);
}
