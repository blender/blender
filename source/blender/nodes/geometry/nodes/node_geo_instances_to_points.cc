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

#include "BKE_pointcloud.h"
#include "DNA_pointcloud_types.h"

#include "node_geometry_util.hh"

namespace blender::nodes {

static void geo_node_instances_to_points_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>("Instances").only_instances();
  b.add_input<decl::Bool>("Selection").default_value(true).hide_value().supports_field();
  b.add_input<decl::Vector>("Position").implicit_field();
  b.add_input<decl::Float>("Radius")
      .default_value(0.05f)
      .min(0.0f)
      .subtype(PROP_DISTANCE)
      .supports_field();
  b.add_output<decl::Geometry>("Points");
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

  const AttributeDomain attribute_domain = ATTR_DOMAIN_POINT;
  GeometryComponentFieldContext field_context{instances, attribute_domain};
  const int domain_size = instances.attribute_domain_size(attribute_domain);

  fn::FieldEvaluator selection_evaluator{field_context, domain_size};
  selection_evaluator.add(std::move(selection_field));
  selection_evaluator.evaluate();
  const IndexMask selection = selection_evaluator.get_evaluated_as_mask(0);
  if (selection.is_empty()) {
    return;
  }

  PointCloud *pointcloud = BKE_pointcloud_new_nomain(selection.size());
  geometry_set.replace_pointcloud(pointcloud);

  PointCloudComponent &points = geometry_set.get_component_for_write<PointCloudComponent>();

  fn::FieldEvaluator evaluator{field_context, &selection};
  evaluator.add(std::move(position_field));
  evaluator.add(std::move(radius_field));
  evaluator.evaluate();
  const VArray<float3> &positions = evaluator.get_evaluated<float3>(0);
  copy_attribute_to_points(positions, selection, {(float3 *)pointcloud->co, pointcloud->totpoint});
  const VArray<float> &radii = evaluator.get_evaluated<float>(1);
  copy_attribute_to_points(radii, selection, {pointcloud->radius, pointcloud->totpoint});

  if (!instances.instance_ids().is_empty()) {
    OutputAttribute_Typed<int> id_attribute = points.attribute_try_get_for_output<int>(
        "id", ATTR_DOMAIN_POINT, CD_PROP_INT32);
    MutableSpan<int> ids = id_attribute.as_span();
    for (const int i : selection.index_range()) {
      ids[i] = instances.instance_ids()[selection[i]];
    }
    id_attribute.save();
  }
}

static void geo_node_instances_to_points_exec(GeoNodeExecParams params)
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
    params.set_output("Points", GeometrySet());
  }
}

}  // namespace blender::nodes

void register_node_type_geo_instances_to_points()
{
  static bNodeType ntype;

  geo_node_type_base(
      &ntype, GEO_NODE_INSTANCES_TO_POINTS, "Instances to Points", NODE_CLASS_GEOMETRY, 0);
  ntype.declare = blender::nodes::geo_node_instances_to_points_declare;
  ntype.geometry_node_execute = blender::nodes::geo_node_instances_to_points_exec;
  nodeRegisterType(&ntype);
}
