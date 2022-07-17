/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2006 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup cmpnodes
 */

#include "node_composite_util.hh"

/* **************** SEPARATE YCCA ******************** */

namespace blender::nodes::node_composite_sepcomb_ycca_cc {

static void cmp_node_sepycca_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>(N_("Image")).default_value({1.0f, 1.0f, 1.0f, 1.0f});
  b.add_output<decl::Float>(N_("Y"));
  b.add_output<decl::Float>(N_("Cb"));
  b.add_output<decl::Float>(N_("Cr"));
  b.add_output<decl::Float>(N_("A"));
}

static void node_composit_init_mode_sepycca(bNodeTree *UNUSED(ntree), bNode *node)
{
  node->custom1 = 1; /* BLI_YCC_ITU_BT709 */
}

}  // namespace blender::nodes::node_composite_sepcomb_ycca_cc

void register_node_type_cmp_sepycca()
{
  namespace file_ns = blender::nodes::node_composite_sepcomb_ycca_cc;

  static bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_SEPYCCA_LEGACY, "Separate YCbCrA", NODE_CLASS_CONVERTER);
  ntype.declare = file_ns::cmp_node_sepycca_declare;
  node_type_init(&ntype, file_ns::node_composit_init_mode_sepycca);

  nodeRegisterType(&ntype);
}

/* **************** COMBINE YCCA ******************** */

namespace blender::nodes::node_composite_sepcomb_ycca_cc {

static void cmp_node_combycca_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Float>(N_("Y")).min(0.0f).max(1.0f);
  b.add_input<decl::Float>(N_("Cb")).default_value(0.5f).min(0.0f).max(1.0f);
  b.add_input<decl::Float>(N_("Cr")).default_value(0.5f).min(0.0f).max(1.0f);
  b.add_input<decl::Float>(N_("A")).default_value(1.0f).min(0.0f).max(1.0f);
  b.add_output<decl::Color>(N_("Image"));
}

static void node_composit_init_mode_combycca(bNodeTree *UNUSED(ntree), bNode *node)
{
  node->custom1 = 1; /* BLI_YCC_ITU_BT709 */
}

}  // namespace blender::nodes::node_composite_sepcomb_ycca_cc

void register_node_type_cmp_combycca()
{
  namespace file_ns = blender::nodes::node_composite_sepcomb_ycca_cc;

  static bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_COMBYCCA_LEGACY, "Combine YCbCrA", NODE_CLASS_CONVERTER);
  ntype.declare = file_ns::cmp_node_combycca_declare;
  node_type_init(&ntype, file_ns::node_composit_init_mode_combycca);

  nodeRegisterType(&ntype);
}
