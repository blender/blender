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

#include "UI_interface.h"
#include "UI_resources.h"

#include "node_composite_util.hh"

/* **************** CURVE Time  ******************** */

namespace blender::nodes::node_composite_curves_cc {

static void cmp_node_time_declare(NodeDeclarationBuilder &b)
{
  b.add_output<decl::Float>(N_("Fac"));
}

/* custom1 = start_frame, custom2 = end_frame */
static void node_composit_init_curves_time(bNodeTree *UNUSED(ntree), bNode *node)
{
  node->custom1 = 1;
  node->custom2 = 250;
  node->storage = BKE_curvemapping_add(1, 0.0f, 0.0f, 1.0f, 1.0f);
}

}  // namespace blender::nodes::node_composite_curves_cc

void register_node_type_cmp_curve_time()
{
  namespace file_ns = blender::nodes::node_composite_curves_cc;

  static bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_TIME, "Time Curve", NODE_CLASS_INPUT);
  ntype.declare = file_ns::cmp_node_time_declare;
  node_type_size(&ntype, 200, 140, 320);
  node_type_init(&ntype, file_ns::node_composit_init_curves_time);
  node_type_storage(&ntype, "CurveMapping", node_free_curves, node_copy_curves);

  nodeRegisterType(&ntype);
}

/* **************** CURVE VEC  ******************** */

namespace blender::nodes::node_composite_curves_cc {

static void cmp_node_curve_vec_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Vector>(N_("Vector")).default_value({0.0f, 0.0f, 0.0f}).min(-1.0f).max(1.0f);
  b.add_output<decl::Vector>(N_("Vector"));
}

static void node_composit_init_curve_vec(bNodeTree *UNUSED(ntree), bNode *node)
{
  node->storage = BKE_curvemapping_add(3, -1.0f, -1.0f, 1.0f, 1.0f);
}

static void node_buts_curvevec(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiTemplateCurveMapping(layout, ptr, "mapping", 'v', false, false, false, false);
}

}  // namespace blender::nodes::node_composite_curves_cc

void register_node_type_cmp_curve_vec()
{
  namespace file_ns = blender::nodes::node_composite_curves_cc;

  static bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_CURVE_VEC, "Vector Curves", NODE_CLASS_OP_VECTOR);
  ntype.declare = file_ns::cmp_node_curve_vec_declare;
  ntype.draw_buttons = file_ns::node_buts_curvevec;
  node_type_size(&ntype, 200, 140, 320);
  node_type_init(&ntype, file_ns::node_composit_init_curve_vec);
  node_type_storage(&ntype, "CurveMapping", node_free_curves, node_copy_curves);

  nodeRegisterType(&ntype);
}

/* **************** CURVE RGB  ******************** */

namespace blender::nodes::node_composite_curves_cc {

static void cmp_node_rgbcurves_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Float>(N_("Fac")).default_value(1.0f).min(-1.0f).max(1.0f).subtype(
      PROP_FACTOR);
  b.add_input<decl::Color>(N_("Image")).default_value({1.0f, 1.0f, 1.0f, 1.0f});
  b.add_input<decl::Color>(N_("Black Level")).default_value({0.0f, 0.0f, 0.0f, 1.0f});
  b.add_input<decl::Color>(N_("White Level")).default_value({1.0f, 1.0f, 1.0f, 1.0f});
  b.add_output<decl::Color>(N_("Image"));
}

static void node_composit_init_curve_rgb(bNodeTree *UNUSED(ntree), bNode *node)
{
  node->storage = BKE_curvemapping_add(4, 0.0f, 0.0f, 1.0f, 1.0f);
}

}  // namespace blender::nodes::node_composite_curves_cc

void register_node_type_cmp_curve_rgb()
{
  namespace file_ns = blender::nodes::node_composite_curves_cc;

  static bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_CURVE_RGB, "RGB Curves", NODE_CLASS_OP_COLOR);
  ntype.declare = file_ns::cmp_node_rgbcurves_declare;
  node_type_size(&ntype, 200, 140, 320);
  node_type_init(&ntype, file_ns::node_composit_init_curve_rgb);
  node_type_storage(&ntype, "CurveMapping", node_free_curves, node_copy_curves);

  nodeRegisterType(&ntype);
}
