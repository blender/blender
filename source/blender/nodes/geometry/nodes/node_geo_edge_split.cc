/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "DNA_mesh_types.h"

#include "GEO_mesh_split_edges.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_edge_split_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>(N_("Mesh")).supported_type(GEO_COMPONENT_TYPE_MESH);
  b.add_input<decl::Bool>(N_("Selection")).default_value(true).hide_value().field_on_all();
  b.add_output<decl::Geometry>(N_("Mesh")).propagate_all();
}

static void node_geo_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Mesh");

  const Field<bool> selection_field = params.extract_input<Field<bool>>("Selection");

  geometry_set.modify_geometry_sets([&](GeometrySet &geometry_set) {
    if (const Mesh *mesh = geometry_set.get_mesh_for_read()) {
      const bke::MeshFieldContext field_context{*mesh, ATTR_DOMAIN_EDGE};
      fn::FieldEvaluator selection_evaluator{field_context, mesh->totedge};
      selection_evaluator.set_selection(selection_field);
      selection_evaluator.evaluate();
      const IndexMask mask = selection_evaluator.get_evaluated_selection_as_mask();
      if (mask.is_empty()) {
        return;
      }

      geometry::split_edges(
          *geometry_set.get_mesh_for_write(), mask, params.get_output_propagation_info("Mesh"));
    }
  });

  params.set_output("Mesh", std::move(geometry_set));
}

}  // namespace blender::nodes::node_geo_edge_split_cc

void register_node_type_geo_edge_split()
{
  namespace file_ns = blender::nodes::node_geo_edge_split_cc;

  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_SPLIT_EDGES, "Split Edges", NODE_CLASS_GEOMETRY);
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  ntype.declare = file_ns::node_declare;
  nodeRegisterType(&ntype);
}
