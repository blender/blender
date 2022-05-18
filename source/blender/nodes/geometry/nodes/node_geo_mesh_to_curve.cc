/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "GEO_mesh_to_curve.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_mesh_to_curve_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>(N_("Mesh")).supported_type(GEO_COMPONENT_TYPE_MESH);
  b.add_input<decl::Bool>(N_("Selection")).default_value(true).hide_value().supports_field();
  b.add_output<decl::Geometry>(N_("Curve"));
}

static void node_geo_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Mesh");

  geometry_set.modify_geometry_sets([&](GeometrySet &geometry_set) {
    if (!geometry_set.has_mesh()) {
      geometry_set.keep_only({GEO_COMPONENT_TYPE_INSTANCES});
      return;
    }

    const MeshComponent &component = *geometry_set.get_component_for_read<MeshComponent>();
    GeometryComponentFieldContext context{component, ATTR_DOMAIN_EDGE};
    fn::FieldEvaluator evaluator{context, component.attribute_domain_num(ATTR_DOMAIN_EDGE)};
    evaluator.add(params.get_input<Field<bool>>("Selection"));
    evaluator.evaluate();
    const IndexMask selection = evaluator.get_evaluated_as_mask(0);
    if (selection.size() == 0) {
      geometry_set.keep_only({GEO_COMPONENT_TYPE_INSTANCES});
      return;
    }

    geometry_set.replace_curves(geometry::mesh_to_curve_convert(component, selection));
    geometry_set.keep_only({GEO_COMPONENT_TYPE_CURVE, GEO_COMPONENT_TYPE_INSTANCES});
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
