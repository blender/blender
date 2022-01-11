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

/* **************** VALTORGB ******************** */

namespace blender::nodes::node_composite_val_to_rgb_cc {

static void cmp_node_valtorgb_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Float>(N_("Fac")).default_value(0.5f).min(0.0f).max(1.0f).subtype(PROP_FACTOR);
  b.add_output<decl::Color>(N_("Image"));
  b.add_output<decl::Float>(N_("Alpha"));
}

static void node_composit_init_valtorgb(bNodeTree *UNUSED(ntree), bNode *node)
{
  node->storage = BKE_colorband_add(true);
}

}  // namespace blender::nodes::node_composite_val_to_rgb_cc

void register_node_type_cmp_valtorgb()
{
  namespace file_ns = blender::nodes::node_composite_val_to_rgb_cc;

  static bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_VALTORGB, "ColorRamp", NODE_CLASS_CONVERTER);
  ntype.declare = file_ns::cmp_node_valtorgb_declare;
  node_type_size(&ntype, 240, 200, 320);
  node_type_init(&ntype, file_ns::node_composit_init_valtorgb);
  node_type_storage(&ntype, "ColorBand", node_free_standard_storage, node_copy_standard_storage);

  nodeRegisterType(&ntype);
}

/* **************** RGBTOBW ******************** */

namespace blender::nodes::node_composite_val_to_rgb_cc {

static void cmp_node_rgbtobw_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>(N_("Image")).default_value({0.8f, 0.8f, 0.8f, 1.0f});
  b.add_output<decl::Color>(N_("Val"));
}

}  // namespace blender::nodes::node_composite_val_to_rgb_cc

void register_node_type_cmp_rgbtobw()
{
  namespace file_ns = blender::nodes::node_composite_val_to_rgb_cc;

  static bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_RGBTOBW, "RGB to BW", NODE_CLASS_CONVERTER);
  ntype.declare = file_ns::cmp_node_rgbtobw_declare;
  node_type_size_preset(&ntype, NODE_SIZE_SMALL);

  nodeRegisterType(&ntype);
}
