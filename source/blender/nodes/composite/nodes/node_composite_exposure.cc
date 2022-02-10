/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2020 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup cmpnodes
 */

#include "node_composite_util.hh"

/* **************** Exposure ******************** */

namespace blender::nodes::node_composite_exposure_cc {

static void cmp_node_exposure_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>(N_("Image")).default_value({1.0f, 1.0f, 1.0f, 1.0f});
  b.add_input<decl::Float>(N_("Exposure")).min(-10.0f).max(10.0f);
  b.add_output<decl::Color>(N_("Image"));
}

}  // namespace blender::nodes::node_composite_exposure_cc

void register_node_type_cmp_exposure()
{
  namespace file_ns = blender::nodes::node_composite_exposure_cc;

  static bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_EXPOSURE, "Exposure", NODE_CLASS_OP_COLOR);
  ntype.declare = file_ns::cmp_node_exposure_declare;

  nodeRegisterType(&ntype);
}
