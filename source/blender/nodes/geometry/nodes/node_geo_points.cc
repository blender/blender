/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_pointcloud.h"

#include "BLI_task.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_points_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Int>(N_("Count"))
      .default_value(1)
      .description(N_("The number of points to create"))
      .min(0);
  b.add_input<decl::Vector>(N_("Position"))
      .supports_field()
      .default_value(float3(0.0f))
      .description(N_("The positions of the new points"));
  b.add_input<decl::Float>(N_("Radius"))
      .supports_field()
      .subtype(PROP_DISTANCE)
      .default_value(float(0.1f))
      .description(N_("The radii of the new points"));
  b.add_output<decl::Geometry>(N_("Geometry"));
}

class PointsFieldContext : public FieldContext {
 private:
  int points_num_;

 public:
  PointsFieldContext(const int points_num) : points_num_(points_num)
  {
  }

  int64_t points_num() const
  {
    return points_num_;
  }

  GVArray get_varray_for_input(const FieldInput &field_input,
                               const IndexMask mask,
                               ResourceScope &UNUSED(scope)) const
  {
    const bke::IDAttributeFieldInput *id_field_input =
        dynamic_cast<const bke::IDAttributeFieldInput *>(&field_input);

    const fn::IndexFieldInput *index_field_input = dynamic_cast<const fn::IndexFieldInput *>(
        &field_input);

    if (id_field_input == nullptr && index_field_input == nullptr) {
      return {};
    }

    return fn::IndexFieldInput::get_index_varray(mask);
  }
};

static void node_geo_exec(GeoNodeExecParams params)
{
  const int count = params.extract_input<int>("Count");
  if (count <= 0) {
    params.set_default_remaining_outputs();
    return;
  }

  Field<float3> position_field = params.extract_input<Field<float3>>("Position");
  Field<float> radius_field = params.extract_input<Field<float>>("Radius");

  PointCloud *new_point_cloud = BKE_pointcloud_new_nomain(count);
  GeometrySet geometry_set = GeometrySet::create_with_pointcloud(new_point_cloud);
  PointCloudComponent &points = geometry_set.get_component_for_write<PointCloudComponent>();
  MutableAttributeAccessor attributes = *points.attributes_for_write();
  AttributeWriter<float3> output_position = attributes.lookup_or_add_for_write<float3>(
      "position", ATTR_DOMAIN_POINT);
  AttributeWriter<float> output_radii = attributes.lookup_or_add_for_write<float>(
      "radius", ATTR_DOMAIN_POINT);

  PointsFieldContext context{count};
  fn::FieldEvaluator evaluator{context, count};
  evaluator.add_with_destination(position_field, output_position.varray);
  evaluator.add_with_destination(radius_field, output_radii.varray);
  evaluator.evaluate();

  output_position.finish();
  output_radii.finish();
  params.set_output("Geometry", std::move(geometry_set));
}

}  // namespace blender::nodes::node_geo_points_cc

void register_node_type_geo_points()
{
  namespace file_ns = blender::nodes::node_geo_points_cc;
  static bNodeType ntype;
  geo_node_type_base(&ntype, GEO_NODE_POINTS, "Points", NODE_CLASS_GEOMETRY);
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  ntype.declare = file_ns::node_declare;
  nodeRegisterType(&ntype);
}
