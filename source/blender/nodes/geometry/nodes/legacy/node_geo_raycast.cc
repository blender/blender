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

#include "DNA_mesh_types.h"

#include "BKE_bvhutils.h"
#include "BKE_mesh_sample.hh"

#include "UI_interface.h"
#include "UI_resources.h"

#include "node_geometry_util.hh"

namespace blender::nodes {

static void geo_node_raycast_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>("Geometry");
  b.add_input<decl::Geometry>("Target Geometry");
  b.add_input<decl::String>("Ray Direction");
  b.add_input<decl::Vector>("Ray Direction", "Ray Direction_001")
      .default_value({0.0f, 0.0f, 1.0f});
  b.add_input<decl::String>("Ray Length");
  b.add_input<decl::Float>("Ray Length", "Ray Length_001")
      .default_value(100.0f)
      .min(0.0f)
      .subtype(PROP_DISTANCE);
  b.add_input<decl::String>("Target Attribute");
  b.add_input<decl::String>("Is Hit");
  b.add_input<decl::String>("Hit Position");
  b.add_input<decl::String>("Hit Normal");
  b.add_input<decl::String>("Hit Distance");
  b.add_input<decl::String>("Hit Attribute");
  b.add_output<decl::Geometry>("Geometry");
}

static void geo_node_raycast_layout(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiLayoutSetPropSep(layout, true);
  uiLayoutSetPropDecorate(layout, false);
  uiItemR(layout, ptr, "mapping", 0, IFACE_("Mapping"), ICON_NONE);
  uiItemR(layout, ptr, "input_type_ray_direction", 0, IFACE_("Ray Direction"), ICON_NONE);
  uiItemR(layout, ptr, "input_type_ray_length", 0, IFACE_("Ray Length"), ICON_NONE);
}

static void geo_node_raycast_init(bNodeTree *UNUSED(tree), bNode *node)
{
  NodeGeometryRaycast *data = (NodeGeometryRaycast *)MEM_callocN(sizeof(NodeGeometryRaycast),
                                                                 __func__);
  data->input_type_ray_direction = GEO_NODE_ATTRIBUTE_INPUT_VECTOR;
  data->input_type_ray_length = GEO_NODE_ATTRIBUTE_INPUT_FLOAT;
  node->storage = data;
}

static void geo_node_raycast_update(bNodeTree *UNUSED(ntree), bNode *node)
{
  NodeGeometryRaycast *node_storage = (NodeGeometryRaycast *)node->storage;
  update_attribute_input_socket_availabilities(
      *node,
      "Ray Direction",
      (GeometryNodeAttributeInputMode)node_storage->input_type_ray_direction);
  update_attribute_input_socket_availabilities(
      *node, "Ray Length", (GeometryNodeAttributeInputMode)node_storage->input_type_ray_length);
}

static void raycast_to_mesh(const Mesh &mesh,
                            const VArray<float3> &ray_origins,
                            const VArray<float3> &ray_directions,
                            const VArray<float> &ray_lengths,
                            const MutableSpan<bool> r_hit,
                            const MutableSpan<int> r_hit_indices,
                            const MutableSpan<float3> r_hit_positions,
                            const MutableSpan<float3> r_hit_normals,
                            const MutableSpan<float> r_hit_distances)
{
  BLI_assert(ray_origins.size() == ray_directions.size());
  BLI_assert(ray_origins.size() == ray_lengths.size());
  BLI_assert(ray_origins.size() == r_hit.size() || r_hit.is_empty());
  BLI_assert(ray_origins.size() == r_hit_indices.size() || r_hit_indices.is_empty());
  BLI_assert(ray_origins.size() == r_hit_positions.size() || r_hit_positions.is_empty());
  BLI_assert(ray_origins.size() == r_hit_normals.size() || r_hit_normals.is_empty());
  BLI_assert(ray_origins.size() == r_hit_distances.size() || r_hit_distances.is_empty());

  BVHTreeFromMesh tree_data;
  BKE_bvhtree_from_mesh_get(&tree_data, &mesh, BVHTREE_FROM_LOOPTRI, 4);
  if (tree_data.tree == nullptr) {
    free_bvhtree_from_mesh(&tree_data);
    return;
  }

  for (const int i : ray_origins.index_range()) {
    const float ray_length = ray_lengths[i];
    const float3 ray_origin = ray_origins[i];
    const float3 ray_direction = ray_directions[i].normalized();

    BVHTreeRayHit hit;
    hit.index = -1;
    hit.dist = ray_length;
    if (BLI_bvhtree_ray_cast(tree_data.tree,
                             ray_origin,
                             ray_direction,
                             0.0f,
                             &hit,
                             tree_data.raycast_callback,
                             &tree_data) != -1) {
      if (!r_hit.is_empty()) {
        r_hit[i] = hit.index >= 0;
      }
      if (!r_hit_indices.is_empty()) {
        /* Index should always be a valid looptri index, use 0 when hit failed. */
        r_hit_indices[i] = max_ii(hit.index, 0);
      }
      if (!r_hit_positions.is_empty()) {
        r_hit_positions[i] = hit.co;
      }
      if (!r_hit_normals.is_empty()) {
        r_hit_normals[i] = hit.no;
      }
      if (!r_hit_distances.is_empty()) {
        r_hit_distances[i] = hit.dist;
      }
    }
    else {
      if (!r_hit.is_empty()) {
        r_hit[i] = false;
      }
      if (!r_hit_indices.is_empty()) {
        r_hit_indices[i] = 0;
      }
      if (!r_hit_positions.is_empty()) {
        r_hit_positions[i] = float3(0.0f, 0.0f, 0.0f);
      }
      if (!r_hit_normals.is_empty()) {
        r_hit_normals[i] = float3(0.0f, 0.0f, 0.0f);
      }
      if (!r_hit_distances.is_empty()) {
        r_hit_distances[i] = ray_length;
      }
    }
  }

  free_bvhtree_from_mesh(&tree_data);
}

static bke::mesh_surface_sample::eAttributeMapMode get_map_mode(
    GeometryNodeRaycastMapMode map_mode)
{
  switch (map_mode) {
    case GEO_NODE_RAYCAST_INTERPOLATED:
      return bke::mesh_surface_sample::eAttributeMapMode::INTERPOLATED;
    default:
    case GEO_NODE_RAYCAST_NEAREST:
      return bke::mesh_surface_sample::eAttributeMapMode::NEAREST;
  }
}

static void raycast_from_points(const GeoNodeExecParams &params,
                                const GeometrySet &target_geometry,
                                GeometryComponent &dst_component,
                                const StringRef hit_name,
                                const StringRef hit_position_name,
                                const StringRef hit_normal_name,
                                const StringRef hit_distance_name,
                                const Span<std::string> hit_attribute_names,
                                const Span<std::string> hit_attribute_output_names)
{
  BLI_assert(hit_attribute_names.size() == hit_attribute_output_names.size());

  const MeshComponent *src_mesh_component =
      target_geometry.get_component_for_read<MeshComponent>();
  if (src_mesh_component == nullptr) {
    return;
  }
  const Mesh *src_mesh = src_mesh_component->get_for_read();
  if (src_mesh == nullptr) {
    return;
  }
  if (src_mesh->totpoly == 0) {
    return;
  }

  const NodeGeometryRaycast &storage = *(const NodeGeometryRaycast *)params.node().storage;
  bke::mesh_surface_sample::eAttributeMapMode map_mode = get_map_mode(
      (GeometryNodeRaycastMapMode)storage.mapping);
  const AttributeDomain result_domain = ATTR_DOMAIN_POINT;

  GVArray_Typed<float3> ray_origins = dst_component.attribute_get_for_read<float3>(
      "position", result_domain, {0, 0, 0});
  GVArray_Typed<float3> ray_directions = params.get_input_attribute<float3>(
      "Ray Direction", dst_component, result_domain, {0, 0, 0});
  GVArray_Typed<float> ray_lengths = params.get_input_attribute<float>(
      "Ray Length", dst_component, result_domain, 0);

  OutputAttribute_Typed<bool> hit_attribute =
      dst_component.attribute_try_get_for_output_only<bool>(hit_name, result_domain);
  OutputAttribute_Typed<float3> hit_position_attribute =
      dst_component.attribute_try_get_for_output_only<float3>(hit_position_name, result_domain);
  OutputAttribute_Typed<float3> hit_normal_attribute =
      dst_component.attribute_try_get_for_output_only<float3>(hit_normal_name, result_domain);
  OutputAttribute_Typed<float> hit_distance_attribute =
      dst_component.attribute_try_get_for_output_only<float>(hit_distance_name, result_domain);

  /* Positions and looptri indices are always needed for interpolation,
   * so create temporary arrays if no output attribute is given. */
  Array<int> hit_indices;
  Array<float3> hit_positions_internal;
  if (!hit_attribute_names.is_empty()) {
    hit_indices.reinitialize(ray_origins->size());

    if (!hit_position_attribute) {
      hit_positions_internal.reinitialize(ray_origins->size());
    }
  }
  const MutableSpan<bool> is_hit = hit_attribute ? hit_attribute.as_span() : MutableSpan<bool>();
  const MutableSpan<float3> hit_positions = hit_position_attribute ?
                                                hit_position_attribute.as_span() :
                                                hit_positions_internal;
  const MutableSpan<float3> hit_normals = hit_normal_attribute ? hit_normal_attribute.as_span() :
                                                                 MutableSpan<float3>();
  const MutableSpan<float> hit_distances = hit_distance_attribute ?
                                               hit_distance_attribute.as_span() :
                                               MutableSpan<float>();

  raycast_to_mesh(*src_mesh,
                  ray_origins,
                  ray_directions,
                  ray_lengths,
                  is_hit,
                  hit_indices,
                  hit_positions,
                  hit_normals,
                  hit_distances);

  hit_attribute.save();
  hit_position_attribute.save();
  hit_normal_attribute.save();
  hit_distance_attribute.save();

  /* Custom interpolated attributes */
  bke::mesh_surface_sample::MeshAttributeInterpolator interp(
      src_mesh, IndexMask(ray_origins.size()), hit_positions, hit_indices);
  for (const int i : hit_attribute_names.index_range()) {
    const std::optional<AttributeMetaData> meta_data = src_mesh_component->attribute_get_meta_data(
        hit_attribute_names[i]);
    if (meta_data) {
      ReadAttributeLookup hit_attribute = src_mesh_component->attribute_try_get_for_read(
          hit_attribute_names[i]);
      OutputAttribute hit_attribute_output = dst_component.attribute_try_get_for_output_only(
          hit_attribute_output_names[i], result_domain, meta_data->data_type);

      interp.sample_attribute(hit_attribute, hit_attribute_output, map_mode);

      hit_attribute_output.save();
    }
  }
}

static void geo_node_raycast_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Geometry");
  GeometrySet target_geometry_set = params.extract_input<GeometrySet>("Target Geometry");

  const std::string hit_name = params.extract_input<std::string>("Is Hit");
  const std::string hit_position_name = params.extract_input<std::string>("Hit Position");
  const std::string hit_normal_name = params.extract_input<std::string>("Hit Normal");
  const std::string hit_distance_name = params.extract_input<std::string>("Hit Distance");

  const Array<std::string> hit_names = {params.extract_input<std::string>("Target Attribute")};
  const Array<std::string> hit_output_names = {params.extract_input<std::string>("Hit Attribute")};

  geometry_set = bke::geometry_set_realize_instances(geometry_set);
  target_geometry_set = bke::geometry_set_realize_instances(target_geometry_set);

  static const Array<GeometryComponentType> types = {
      GEO_COMPONENT_TYPE_MESH, GEO_COMPONENT_TYPE_POINT_CLOUD, GEO_COMPONENT_TYPE_CURVE};
  for (const GeometryComponentType type : types) {
    if (geometry_set.has(type)) {
      raycast_from_points(params,
                          target_geometry_set,
                          geometry_set.get_component_for_write(type),
                          hit_name,
                          hit_position_name,
                          hit_normal_name,
                          hit_distance_name,
                          hit_names,
                          hit_output_names);
    }
  }

  params.set_output("Geometry", geometry_set);
}

}  // namespace blender::nodes

void register_node_type_geo_legacy_raycast()
{
  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_LEGACY_RAYCAST, "Raycast", NODE_CLASS_GEOMETRY, 0);
  node_type_size_preset(&ntype, NODE_SIZE_LARGE);
  node_type_init(&ntype, blender::nodes::geo_node_raycast_init);
  node_type_update(&ntype, blender::nodes::geo_node_raycast_update);
  node_type_storage(
      &ntype, "NodeGeometryRaycast", node_free_standard_storage, node_copy_standard_storage);
  ntype.declare = blender::nodes::geo_node_raycast_declare;
  ntype.geometry_node_execute = blender::nodes::geo_node_raycast_exec;
  ntype.draw_buttons = blender::nodes::geo_node_raycast_layout;
  nodeRegisterType(&ntype);
}
