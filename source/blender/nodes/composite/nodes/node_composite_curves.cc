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

/* **************** CURVE Time  ******************** */

namespace blender::nodes {

static void cmp_node_time_declare(NodeDeclarationBuilder &b)
{
  b.add_output<decl::Float>(N_("Fac"));
}

}  // namespace blender::nodes

/* custom1 = start_frame, custom2 = end_frame */
static void node_composit_init_curves_time(bNodeTree *UNUSED(ntree), bNode *node)
{
  node->custom1 = 1;
  node->custom2 = 250;
  node->storage = BKE_curvemapping_add(1, 0.0f, 0.0f, 1.0f, 1.0f);
}

void register_node_type_cmp_curve_time(void)
{
  static bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_TIME, "Time", NODE_CLASS_INPUT, 0);
  ntype.declare = blender::nodes::cmp_node_time_declare;
  node_type_size(&ntype, 140, 100, 320);
  node_type_init(&ntype, node_composit_init_curves_time);
  node_type_storage(&ntype, "CurveMapping", node_free_curves, node_copy_curves);

  nodeRegisterType(&ntype);
}

/* **************** CURVE VEC  ******************** */
static bNodeSocketTemplate cmp_node_curve_vec_in[] = {
    {SOCK_VECTOR, N_("Vector"), 0.0f, 0.0f, 0.0f, 1.0f, -1.0f, 1.0f, PROP_NONE},
    {-1, ""},
};

static bNodeSocketTemplate cmp_node_curve_vec_out[] = {
    {SOCK_VECTOR, N_("Vector")},
    {-1, ""},
};

static void node_composit_init_curve_vec(bNodeTree *UNUSED(ntree), bNode *node)
{
  node->storage = BKE_curvemapping_add(3, -1.0f, -1.0f, 1.0f, 1.0f);
}

void register_node_type_cmp_curve_vec(void)
{
  static bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_CURVE_VEC, "Vector Curves", NODE_CLASS_OP_VECTOR, 0);
  node_type_socket_templates(&ntype, cmp_node_curve_vec_in, cmp_node_curve_vec_out);
  node_type_size(&ntype, 200, 140, 320);
  node_type_init(&ntype, node_composit_init_curve_vec);
  node_type_storage(&ntype, "CurveMapping", node_free_curves, node_copy_curves);

  nodeRegisterType(&ntype);
}

/* **************** CURVE RGB  ******************** */

namespace blender::nodes {

static void cmp_node_rgbcurves_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Float>(N_("Fac")).default_value(1.0f).min(-1.0f).max(1.0f).subtype(
      PROP_FACTOR);
  b.add_input<decl::Color>(N_("Image")).default_value({1.0f, 1.0f, 1.0f, 1.0f});
  b.add_input<decl::Color>(N_("Black Level")).default_value({0.0f, 0.0f, 0.0f, 1.0f});
  b.add_input<decl::Color>(N_("White Level")).default_value({1.0f, 1.0f, 1.0f, 1.0f});
  b.add_output<decl::Color>(N_("Image"));
}

}  // namespace blender::nodes

static void node_composit_init_curve_rgb(bNodeTree *UNUSED(ntree), bNode *node)
{
  node->storage = BKE_curvemapping_add(4, 0.0f, 0.0f, 1.0f, 1.0f);
}

void register_node_type_cmp_curve_rgb(void)
{
  static bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_CURVE_RGB, "RGB Curves", NODE_CLASS_OP_COLOR, 0);
  ntype.declare = blender::nodes::cmp_node_rgbcurves_declare;
  node_type_size(&ntype, 200, 140, 320);
  node_type_init(&ntype, node_composit_init_curve_rgb);
  node_type_storage(&ntype, "CurveMapping", node_free_curves, node_copy_curves);

  nodeRegisterType(&ntype);
}
