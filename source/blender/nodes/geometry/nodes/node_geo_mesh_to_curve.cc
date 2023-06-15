/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "DNA_mesh_types.h"

#include "GEO_mesh_to_curve.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_mesh_to_curve_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>("Mesh").supported_type(GeometryComponent::Type::Mesh);
  b.add_input<decl::Bool>("Selection").default_value(true).hide_value().field_on_all();
  b.add_output<decl::Geometry>("Curve").propagate_all();
}

static void node_geo_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Mesh");

  geometry_set.modify_geometry_sets([&](GeometrySet &geometry_set) {
    const Mesh *mesh = geometry_set.get_mesh_for_read();
    if (mesh == nullptr) {
      geometry_set.remove_geometry_during_modify();
      return;
    }

    const bke::MeshFieldContext context{*mesh, ATTR_DOMAIN_EDGE};
    fn::FieldEvaluator evaluator{context, mesh->totedge};
    evaluator.add(params.get_input<Field<bool>>("Selection"));
    evaluator.evaluate();
    const IndexMask selection = evaluator.get_evaluated_as_mask(0);
    if (selection.size() == 0) {
      geometry_set.remove_geometry_during_modify();
      return;
    }

    bke::CurvesGeometry curves = geometry::mesh_to_curve_convert(
        *mesh, selection, params.get_output_propagation_info("Curve"));
    geometry_set.replace_curves(bke::curves_new_nomain(std::move(curves)));
    geometry_set.keep_only_during_modify({GeometryComponent::Type::Curve});
  });

  params.set_output("Curve", std::move(geometry_set));
}

}  // namespace blender::nodes::node_geo_mesh_to_curve_cc

void register_node_type_geo_mesh_to_curve()
{
  namespace file_ns = blender::nodes::node_geo_mesh_to_curve_cc;

  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_MESH_TO_CURVE, "Mesh to Curve", NODE_CLASS_GEOMETRY);
  ntype.declare = file_ns::node_declare;
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  nodeRegisterType(&ntype);
}
