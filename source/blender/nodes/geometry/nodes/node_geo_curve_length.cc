/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_curves.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_curve_length_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>("Curve").supported_type(GeometryComponent::Type::Curve);
  b.add_output<decl::Float>("Length");
}

static void node_geo_exec(GeoNodeExecParams params)
{
  GeometrySet curve_set = params.extract_input<GeometrySet>("Curve");
  if (!curve_set.has_curves()) {
    params.set_default_remaining_outputs();
    return;
  }

  const Curves &curves_id = *curve_set.get_curves_for_read();
  const bke::CurvesGeometry &curves = curves_id.geometry.wrap();
  const VArray<bool> cyclic = curves.cyclic();

  curves.ensure_evaluated_lengths();

  float length = 0.0f;
  for (const int i : curves.curves_range()) {
    length += curves.evaluated_length_total_for_curve(i, cyclic[i]);
  }

  params.set_output("Length", length);
}

}  // namespace blender::nodes::node_geo_curve_length_cc

void register_node_type_geo_curve_length()
{
  namespace file_ns = blender::nodes::node_geo_curve_length_cc;

  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_CURVE_LENGTH, "Curve Length", NODE_CLASS_GEOMETRY);
  ntype.declare = file_ns::node_declare;
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  nodeRegisterType(&ntype);
}
