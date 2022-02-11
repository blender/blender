/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2006 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup cmpnodes
 */

#include "node_composite_util.hh"

/* **************** Pixelate ******************** */

namespace blender::nodes::node_composite_pixelate_cc {

static void cmp_node_pixelate_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>(N_("Color"));
  b.add_output<decl::Color>(N_("Color"));
}

}  // namespace blender::nodes::node_composite_pixelate_cc

void register_node_type_cmp_pixelate()
{
  namespace file_ns = blender::nodes::node_composite_pixelate_cc;

  static bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_PIXELATE, "Pixelate", NODE_CLASS_OP_FILTER);
  ntype.declare = file_ns::cmp_node_pixelate_declare;

  nodeRegisterType(&ntype);
}
