/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_set_point_radius_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>(N_("Points")).supported_type(GEO_COMPONENT_TYPE_POINT_CLOUD);
  b.add_input<decl::Bool>(N_("Selection")).default_value(true).hide_value().supports_field();
  b.add_input<decl::Float>(N_("Radius"))
      .default_value(0.05f)
      .min(0.0f)
      .supports_field()
      .subtype(PROP_DISTANCE);
  b.add_output<decl::Geometry>(N_("Points"));
}

static void set_radius_in_component(GeometryComponent &component,
                                    const Field<bool> &selection_field,
                                    const Field<float> &radius_field)
{
  GeometryComponentFieldContext field_context{component, ATTR_DOMAIN_POINT};
  const int domain_num = component.attribute_domain_num(ATTR_DOMAIN_POINT);
  if (domain_num == 0) {
    return;
  }

  OutputAttribute_Typed<float> radii = component.attribute_try_get_for_output_only<float>(
      "radius", ATTR_DOMAIN_POINT);

  fn::FieldEvaluator evaluator{field_context, domain_num};
  evaluator.set_selection(selection_field);
  evaluator.add_with_destination(radius_field, radii.varray());
  evaluator.evaluate();

  radii.save();
}

static void node_geo_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Points");
  Field<bool> selection_field = params.extract_input<Field<bool>>("Selection");
  Field<float> radii_field = params.extract_input<Field<float>>("Radius");

  geometry_set.modify_geometry_sets([&](GeometrySet &geometry_set) {
    if (geometry_set.has_pointcloud()) {
      set_radius_in_component(geometry_set.get_component_for_write<PointCloudComponent>(),
                              selection_field,
                              radii_field);
    }
  });

  params.set_output("Points", std::move(geometry_set));
}

}  // namespace blender::nodes::node_geo_set_point_radius_cc

void register_node_type_geo_set_point_radius()
{
  namespace file_ns = blender::nodes::node_geo_set_point_radius_cc;

  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_SET_POINT_RADIUS, "Set Point Radius", NODE_CLASS_GEOMETRY);
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  ntype.declare = file_ns::node_declare;
  nodeRegisterType(&ntype);
}
