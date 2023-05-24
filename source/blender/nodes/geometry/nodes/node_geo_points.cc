/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_pointcloud.h"
#include "DNA_pointcloud_types.h"

#include "BLI_task.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_points_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Int>("Count")
      .default_value(1)
      .description("The number of points to create")
      .min(0);
  b.add_input<decl::Vector>("Position")
      .supports_field()
      .default_value(float3(0.0f))
      .description("The positions of the new points");
  b.add_input<decl::Float>("Radius")
      .min(0.0f)
      .default_value(0.1f)
      .supports_field()
      .subtype(PROP_DISTANCE)
      .description("The radii of the new points");
  b.add_output<decl::Geometry>("Geometry");
}

class PointsFieldContext : public FieldContext {
 private:
  int points_num_;

 public:
  PointsFieldContext(const int points_num) : points_num_(points_num) {}

  int64_t points_num() const
  {
    return points_num_;
  }

  GVArray get_varray_for_input(const FieldInput &field_input,
                               const IndexMask &mask,
                               ResourceScope & /*scope*/) const
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

  PointCloud *points = BKE_pointcloud_new_nomain(count);
  MutableAttributeAccessor attributes = points->attributes_for_write();
  AttributeWriter<float> output_radii = attributes.lookup_or_add_for_write<float>(
      "radius", ATTR_DOMAIN_POINT);

  PointsFieldContext context{count};
  fn::FieldEvaluator evaluator{context, count};
  evaluator.add_with_destination(position_field, points->positions_for_write());
  evaluator.add_with_destination(radius_field, output_radii.varray);
  evaluator.evaluate();

  output_radii.finish();
  params.set_output("Geometry", GeometrySet::create_with_pointcloud(points));
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
