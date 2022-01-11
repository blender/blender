/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2006 Blender Foundation.
 * All rights reserved.
 */

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

  cmp_node_type_base(&ntype, CMP_NODE_SEPYCCA, "Separate YCbCrA", NODE_CLASS_CONVERTER);
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

  cmp_node_type_base(&ntype, CMP_NODE_COMBYCCA, "Combine YCbCrA", NODE_CLASS_CONVERTER);
  ntype.declare = file_ns::cmp_node_combycca_declare;
  node_type_init(&ntype, file_ns::node_composit_init_mode_combycca);

  nodeRegisterType(&ntype);
}
