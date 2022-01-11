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

/* **************** Hue Saturation ******************** */

namespace blender::nodes::node_composite_hue_sat_val_cc {

static void cmp_node_huesatval_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>(N_("Image")).default_value({1.0f, 1.0f, 1.0f, 1.0f});
  b.add_input<decl::Float>(N_("Hue")).default_value(0.5f).min(0.0f).max(1.0f).subtype(PROP_FACTOR);
  b.add_input<decl::Float>(N_("Saturation"))
      .default_value(1.0f)
      .min(0.0f)
      .max(2.0f)
      .subtype(PROP_FACTOR);
  b.add_input<decl::Float>(N_("Value"))
      .default_value(1.0f)
      .min(0.0f)
      .max(2.0f)
      .subtype(PROP_FACTOR);
  b.add_input<decl::Float>(N_("Fac")).default_value(1.0f).min(0.0f).max(1.0f).subtype(PROP_FACTOR);
  b.add_output<decl::Color>(N_("Image"));
}

}  // namespace blender::nodes::node_composite_hue_sat_val_cc

void register_node_type_cmp_hue_sat()
{
  namespace file_ns = blender::nodes::node_composite_hue_sat_val_cc;

  static bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_HUE_SAT, "Hue Saturation Value", NODE_CLASS_OP_COLOR);
  ntype.declare = file_ns::cmp_node_huesatval_declare;

  nodeRegisterType(&ntype);
}
