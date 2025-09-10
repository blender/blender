/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_array_utils.hh"

#include "DNA_pointcloud_types.h"

#include "BKE_instances.hh"
#include "BKE_pointcloud.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_instances_to_points_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>("Instances")
      .only_instances()
      .description("Instances to convert to points");
  b.add_input<decl::Bool>("Selection").default_value(true).hide_value().field_on_all();
  b.add_input<decl::Vector>("Position").implicit_field_on_all(NODE_DEFAULT_INPUT_POSITION_FIELD);
  b.add_input<decl::Float>("Radius")
      .default_value(0.05f)
      .min(0.0f)
      .subtype(PROP_DISTANCE)
      .field_on_all();
  b.add_output<decl::Geometry>("Points").propagate_all();
}

static void convert_instances_to_points(GeometrySet &geometry_set,
                                        Field<float3> position_field,
                                        Field<float> radius_field,
                                        Field<bool> selection_field,
                                        const AttributeFilter &attribute_filter)
{
  const bke::Instances &instances = *geometry_set.get_instances();

  const bke::InstancesFieldContext context{instances};
  fn::FieldEvaluator evaluator{context, instances.instances_num()};
  evaluator.set_selection(std::move(selection_field));
  evaluator.add(std::move(position_field));
  evaluator.add(std::move(radius_field));
  evaluator.evaluate();
  const IndexMask selection = evaluator.get_evaluated_selection_as_mask();
  if (selection.is_empty()) {
    return;
  }
  const VArray<float3> positions = evaluator.get_evaluated<float3>(0);
  const VArray<float> radii = evaluator.get_evaluated<float>(1);

  PointCloud *pointcloud = BKE_pointcloud_new_nomain(selection.size());
  geometry_set.replace_pointcloud(pointcloud);
  array_utils::gather(positions, selection, pointcloud->positions_for_write());

  bke::MutableAttributeAccessor dst_attributes = pointcloud->attributes_for_write();
  bke::SpanAttributeWriter<float> point_radii =
      dst_attributes.lookup_or_add_for_write_only_span<float>("radius", AttrDomain::Point);
  array_utils::gather(radii, selection, point_radii.span);
  point_radii.finish();

  const bke::AttributeAccessor src_attributes = instances.attributes();
  bke::GeometrySet::GatheredAttributes attributes_to_propagate;
  geometry_set.gather_attributes_for_propagation({GeometryComponent::Type::Instance},
                                                 GeometryComponent::Type::PointCloud,
                                                 false,
                                                 attribute_filter,
                                                 attributes_to_propagate);

  for (const int i : attributes_to_propagate.names.index_range()) {
    /* These two attributes are added by the implicit inputs above. */
    if (ELEM(attributes_to_propagate.names[i], "position", "radius")) {
      continue;
    }
    const StringRef id = attributes_to_propagate.names[i];
    const bke::AttrType type = attributes_to_propagate.kinds[i].data_type;

    const GAttributeReader src = src_attributes.lookup(id);
    if (selection.size() == instances.instances_num() && src.sharing_info && src.varray.is_span())
    {
      const bke::AttributeInitShared init(src.varray.get_internal_span().data(),
                                          *src.sharing_info);
      dst_attributes.add(id, AttrDomain::Point, type, init);
    }
    else {
      GSpanAttributeWriter dst = dst_attributes.lookup_or_add_for_write_only_span(
          id, AttrDomain::Point, type);
      array_utils::gather(src.varray, selection, dst.span);
      dst.finish();
    }
  }
}

static void node_geo_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Instances");

  if (geometry_set.has_instances()) {
    convert_instances_to_points(geometry_set,
                                params.extract_input<Field<float3>>("Position"),
                                params.extract_input<Field<float>>("Radius"),
                                params.extract_input<Field<bool>>("Selection"),
                                params.get_attribute_filter("Points"));
    geometry_set.keep_only({GeometryComponent::Type::PointCloud, GeometryComponent::Type::Edit});
    params.set_output("Points", std::move(geometry_set));
  }
  else {
    params.set_default_remaining_outputs();
  }
}

static void node_register()
{
  static blender::bke::bNodeType ntype;

  geo_node_type_base(&ntype, "GeometryNodeInstancesToPoints", GEO_NODE_INSTANCES_TO_POINTS);
  ntype.ui_name = "Instances to Points";
  ntype.ui_description =
      "Generate points at the origins of instances.\nNote: Nested instances are not affected by "
      "this node";
  ntype.enum_name_legacy = "INSTANCES_TO_POINTS";
  ntype.nclass = NODE_CLASS_GEOMETRY;
  ntype.declare = node_declare;
  ntype.geometry_node_execute = node_geo_exec;
  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_instances_to_points_cc
