/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2006 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup cmpnodes
 */

#include "node_composite_util.hh"

#include "BKE_colortools.h"

namespace blender::nodes::node_composite_huecorrect_cc {

static void cmp_node_huecorrect_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Float>(N_("Fac")).default_value(1.0f).min(0.0f).max(1.0f).subtype(PROP_FACTOR);
  b.add_input<decl::Color>(N_("Image")).default_value({1.0f, 1.0f, 1.0f, 1.0f});
  b.add_output<decl::Color>(N_("Image"));
}

static void node_composit_init_huecorrect(bNodeTree *UNUSED(ntree), bNode *node)
{
  node->storage = BKE_curvemapping_add(1, 0.0f, 0.0f, 1.0f, 1.0f);

  CurveMapping *cumapping = (CurveMapping *)node->storage;

  cumapping->preset = CURVE_PRESET_MID9;

  for (int c = 0; c < 3; c++) {
    CurveMap *cuma = &cumapping->cm[c];
    BKE_curvemap_reset(cuma, &cumapping->clipr, cumapping->preset, CURVEMAP_SLOPE_POSITIVE);
  }

  /* default to showing Saturation */
  cumapping->cur = 1;
}

}  // namespace blender::nodes::node_composite_huecorrect_cc

void register_node_type_cmp_huecorrect()
{
  namespace file_ns = blender::nodes::node_composite_huecorrect_cc;

  static bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_HUECORRECT, "Hue Correct", NODE_CLASS_OP_COLOR);
  ntype.declare = file_ns::cmp_node_huecorrect_declare;
  node_type_size(&ntype, 320, 140, 500);
  node_type_init(&ntype, file_ns::node_composit_init_huecorrect);
  node_type_storage(&ntype, "CurveMapping", node_free_curves, node_copy_curves);

  nodeRegisterType(&ntype);
}
