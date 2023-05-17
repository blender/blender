/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "DNA_pointcloud_types.h"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_set_point_radius_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>("Points").supported_type(GEO_COMPONENT_TYPE_POINT_CLOUD);
  b.add_input<decl::Bool>("Selection").default_value(true).hide_value().field_on_all();
  b.add_input<decl::Float>("Radius").default_value(0.05f).min(0.0f).field_on_all().subtype(
      PROP_DISTANCE);
  b.add_output<decl::Geometry>("Points").propagate_all();
}

static void set_radius_in_component(PointCloud &pointcloud,
                                    const Field<bool> &selection_field,
                                    const Field<float> &radius_field)
{
  if (pointcloud.totpoint == 0) {
    return;
  }
  MutableAttributeAccessor attributes = pointcloud.attributes_for_write();
  AttributeWriter<float> radii = attributes.lookup_or_add_for_write<float>("radius",
                                                                           ATTR_DOMAIN_POINT);

  const bke::PointCloudFieldContext field_context{pointcloud};
  fn::FieldEvaluator evaluator{field_context, pointcloud.totpoint};
  evaluator.set_selection(selection_field);
  evaluator.add_with_destination(radius_field, radii.varray);
  evaluator.evaluate();

  radii.finish();
}

static void node_geo_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Points");
  Field<bool> selection_field = params.extract_input<Field<bool>>("Selection");
  Field<float> radii_field = params.extract_input<Field<float>>("Radius");

  geometry_set.modify_geometry_sets([&](GeometrySet &geometry_set) {
    if (PointCloud *pointcloud = geometry_set.get_pointcloud_for_write()) {
      set_radius_in_component(*pointcloud, selection_field, radii_field);
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
