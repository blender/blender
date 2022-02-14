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

#include "GEO_mesh_merge_by_distance.hh"
#include "GEO_point_merge_by_distance.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_merge_by_distance_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>(N_("Geometry"))
      .supported_type({GEO_COMPONENT_TYPE_POINT_CLOUD, GEO_COMPONENT_TYPE_MESH});
  b.add_input<decl::Bool>(N_("Selection")).default_value(true).hide_value().supports_field();
  b.add_input<decl::Float>(N_("Distance")).default_value(0.001f).min(0.0f).subtype(PROP_DISTANCE);
  b.add_output<decl::Geometry>(N_("Geometry"));
}

static PointCloud *pointcloud_merge_by_distance(const PointCloudComponent &src_points,
                                                const float merge_distance,
                                                const Field<bool> &selection_field)
{
  const int src_size = src_points.attribute_domain_size(ATTR_DOMAIN_POINT);
  GeometryComponentFieldContext context{src_points, ATTR_DOMAIN_POINT};
  FieldEvaluator evaluator{context, src_size};
  evaluator.add(selection_field);
  evaluator.evaluate();

  const IndexMask selection = evaluator.get_evaluated_as_mask(0);
  if (selection.is_empty()) {
    return nullptr;
  }

  return geometry::point_merge_by_distance(src_points, merge_distance, selection);
}

static std::optional<Mesh *> mesh_merge_by_distance(const MeshComponent &mesh_component,
                                                    const float merge_distance,
                                                    const Field<bool> &selection_field)
{
  const int src_size = mesh_component.attribute_domain_size(ATTR_DOMAIN_POINT);
  GeometryComponentFieldContext context{mesh_component, ATTR_DOMAIN_POINT};
  FieldEvaluator evaluator{context, src_size};
  evaluator.add(selection_field);
  evaluator.evaluate();

  const IndexMask selection = evaluator.get_evaluated_as_mask(0);
  if (selection.is_empty()) {
    return std::nullopt;
  }

  const Mesh &mesh = *mesh_component.get_for_read();
  return geometry::mesh_merge_by_distance_all(mesh, selection, merge_distance);
}

static void node_geo_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Geometry");

  const Field<bool> selection = params.extract_input<Field<bool>>("Selection");
  const float merge_distance = params.extract_input<float>("Distance");

  geometry_set.modify_geometry_sets([&](GeometrySet &geometry_set) {
    if (geometry_set.has_pointcloud()) {
      PointCloud *result = pointcloud_merge_by_distance(
          *geometry_set.get_component_for_read<PointCloudComponent>(), merge_distance, selection);
      if (result) {
        geometry_set.replace_pointcloud(result);
      }
    }
    if (geometry_set.has_mesh()) {
      std::optional<Mesh *> result = mesh_merge_by_distance(
          *geometry_set.get_component_for_read<MeshComponent>(), merge_distance, selection);
      if (result) {
        geometry_set.replace_mesh(*result);
      }
    }
  });

  params.set_output("Geometry", std::move(geometry_set));
}

}  // namespace blender::nodes::node_geo_merge_by_distance_cc

void register_node_type_geo_merge_by_distance()
{
  namespace file_ns = blender::nodes::node_geo_merge_by_distance_cc;

  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_MERGE_BY_DISTANCE, "Merge by Distance", NODE_CLASS_GEOMETRY);

  ntype.declare = file_ns::node_declare;
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  nodeRegisterType(&ntype);
}
