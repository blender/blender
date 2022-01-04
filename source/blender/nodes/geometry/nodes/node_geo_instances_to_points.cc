/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "DNA_pointcloud_types.h"

#include "BKE_attribute_math.hh"
#include "BKE_pointcloud.h"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_instances_to_points_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>(N_("Instances")).only_instances();
  b.add_input<decl::Bool>(N_("Selection")).default_value(true).hide_value().supports_field();
  b.add_input<decl::Vector>(N_("Position")).implicit_field();
  b.add_input<decl::Float>(N_("Radius"))
      .default_value(0.05f)
      .min(0.0f)
      .subtype(PROP_DISTANCE)
      .supports_field();
  b.add_output<decl::Geometry>(N_("Points"));
}

template<typename T>
static void copy_attribute_to_points(const VArray<T> &src,
                                     const IndexMask mask,
                                     MutableSpan<T> dst)
{
  for (const int i : mask.index_range()) {
    dst[i] = src[mask[i]];
  }
}

static void convert_instances_to_points(GeometrySet &geometry_set,
                                        Field<float3> position_field,
                                        Field<float> radius_field,
                                        const Field<bool> selection_field)
{
  const InstancesComponent &instances = *geometry_set.get_component_for_read<InstancesComponent>();

  GeometryComponentFieldContext field_context{instances, ATTR_DOMAIN_INSTANCE};
  const int domain_size = instances.attribute_domain_size(ATTR_DOMAIN_INSTANCE);

  fn::FieldEvaluator evaluator{field_context, domain_size};
  evaluator.set_selection(std::move(selection_field));
  evaluator.add(std::move(position_field));
  evaluator.add(std::move(radius_field));
  evaluator.evaluate();
  const IndexMask selection = evaluator.get_evaluated_selection_as_mask();
  if (selection.is_empty()) {
    return;
  }

  PointCloud *pointcloud = BKE_pointcloud_new_nomain(selection.size());
  geometry_set.replace_pointcloud(pointcloud);

  PointCloudComponent &points = geometry_set.get_component_for_write<PointCloudComponent>();

  const VArray<float3> &positions = evaluator.get_evaluated<float3>(0);
  copy_attribute_to_points(positions, selection, {(float3 *)pointcloud->co, pointcloud->totpoint});
  const VArray<float> &radii = evaluator.get_evaluated<float>(1);
  copy_attribute_to_points(radii, selection, {pointcloud->radius, pointcloud->totpoint});

  Map<AttributeIDRef, AttributeKind> attributes_to_propagate;
  geometry_set.gather_attributes_for_propagation({GEO_COMPONENT_TYPE_INSTANCES},
                                                 GEO_COMPONENT_TYPE_POINT_CLOUD,
                                                 false,
                                                 attributes_to_propagate);
  /* These two attributes are added by the implicit inputs above. */
  attributes_to_propagate.remove("position");
  attributes_to_propagate.remove("radius");

  for (const auto item : attributes_to_propagate.items()) {
    const AttributeIDRef &attribute_id = item.key;
    const AttributeKind attribute_kind = item.value;

    const GVArray src = instances.attribute_get_for_read(
        attribute_id, ATTR_DOMAIN_INSTANCE, attribute_kind.data_type);
    BLI_assert(src);
    OutputAttribute dst = points.attribute_try_get_for_output_only(
        attribute_id, ATTR_DOMAIN_POINT, attribute_kind.data_type);
    BLI_assert(dst);

    attribute_math::convert_to_static_type(attribute_kind.data_type, [&](auto dummy) {
      using T = decltype(dummy);
      copy_attribute_to_points(src.typed<T>(), selection, dst.as_span().typed<T>());
    });
    dst.save();
  }
}

static void node_geo_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Instances");

  if (geometry_set.has_instances()) {
    convert_instances_to_points(geometry_set,
                                params.extract_input<Field<float3>>("Position"),
                                params.extract_input<Field<float>>("Radius"),
                                params.extract_input<Field<bool>>("Selection"));
    geometry_set.keep_only({GEO_COMPONENT_TYPE_POINT_CLOUD});
    params.set_output("Points", std::move(geometry_set));
  }
  else {
    params.set_default_remaining_outputs();
  }
}

}  // namespace blender::nodes::node_geo_instances_to_points_cc

void register_node_type_geo_instances_to_points()
{
  namespace file_ns = blender::nodes::node_geo_instances_to_points_cc;

  static bNodeType ntype;

  geo_node_type_base(
      &ntype, GEO_NODE_INSTANCES_TO_POINTS, "Instances to Points", NODE_CLASS_GEOMETRY);
  ntype.declare = file_ns::node_declare;
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  nodeRegisterType(&ntype);
}
