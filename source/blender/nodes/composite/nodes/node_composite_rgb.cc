/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2006 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup cmpnodes
 */

#include "node_composite_util.hh"

/* **************** RGB ******************** */

namespace blender::nodes::node_composite_rgb_cc {

static void cmp_node_rgb_declare(NodeDeclarationBuilder &b)
{
  b.add_output<decl::Color>(N_("RGBA")).default_value({0.5f, 0.5f, 0.5f, 1.0f});
}

}  // namespace blender::nodes::node_composite_rgb_cc

void register_node_type_cmp_rgb()
{
  namespace file_ns = blender::nodes::node_composite_rgb_cc;

  static bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_RGB, "RGB", NODE_CLASS_INPUT);
  ntype.declare = file_ns::cmp_node_rgb_declare;
  node_type_size_preset(&ntype, NODE_SIZE_SMALL);

  nodeRegisterType(&ntype);
}
