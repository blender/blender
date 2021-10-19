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

#include "BLI_kdopbvh.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_pointcloud_types.h"

#include "BKE_bvhutils.h"
#include "BKE_mesh_runtime.h"
#include "BKE_mesh_sample.hh"

#include "UI_interface.h"
#include "UI_resources.h"

#include "node_geometry_util.hh"

namespace blender::nodes {

static void geo_node_attribute_transfer_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>("Geometry");
  b.add_input<decl::Geometry>("Source Geometry");
  b.add_input<decl::String>("Source");
  b.add_input<decl::String>("Destination");
  b.add_output<decl::Geometry>("Geometry");
}

static void geo_node_attribute_transfer_layout(uiLayout *layout,
                                               bContext *UNUSED(C),
                                               PointerRNA *ptr)
{
  uiLayoutSetPropSep(layout, true);
  uiLayoutSetPropDecorate(layout, false);
  uiItemR(layout, ptr, "domain", 0, IFACE_("Domain"), ICON_NONE);
  uiItemR(layout, ptr, "mapping", 0, IFACE_("Mapping"), ICON_NONE);
}

static void geo_node_attribute_transfer_init(bNodeTree *UNUSED(tree), bNode *node)
{
  NodeGeometryAttributeTransfer *data = (NodeGeometryAttributeTransfer *)MEM_callocN(
      sizeof(NodeGeometryAttributeTransfer), __func__);
  data->domain = ATTR_DOMAIN_AUTO;
  node->storage = data;
}

static void get_result_domain_and_data_type(const GeometrySet &src_geometry,
                                            const GeometryComponent &dst_component,
                                            const StringRef attribute_name,
                                            CustomDataType *r_data_type,
                                            AttributeDomain *r_domain)
{
  Vector<CustomDataType> data_types;
  Vector<AttributeDomain> domains;

  const PointCloudComponent *pointcloud_component =
      src_geometry.get_component_for_read<PointCloudComponent>();
  if (pointcloud_component != nullptr) {
    std::optional<AttributeMetaData> meta_data = pointcloud_component->attribute_get_meta_data(
        attribute_name);
    if (meta_data.has_value()) {
      data_types.append(meta_data->data_type);
      domains.append(meta_data->domain);
    }
  }

  const MeshComponent *mesh_component = src_geometry.get_component_for_read<MeshComponent>();
  if (mesh_component != nullptr) {
    std::optional<AttributeMetaData> meta_data = mesh_component->attribute_get_meta_data(
        attribute_name);
    if (meta_data.has_value()) {
      data_types.append(meta_data->data_type);
      domains.append(meta_data->domain);
    }
  }

  *r_data_type = bke::attribute_data_type_highest_complexity(data_types);

  if (dst_component.type() == GEO_COMPONENT_TYPE_POINT_CLOUD) {
    *r_domain = ATTR_DOMAIN_POINT;
  }
  else {
    *r_domain = bke::attribute_domain_highest_priority(domains);
  }
}

static void get_closest_in_bvhtree(BVHTreeFromMesh &tree_data,
                                   const VArray<float3> &positions,
                                   const MutableSpan<int> r_indices,
                                   const MutableSpan<float> r_distances_sq,
                                   const MutableSpan<float3> r_positions)
{
  BLI_assert(positions.size() == r_indices.size() || r_indices.is_empty());
  BLI_assert(positions.size() == r_distances_sq.size() || r_distances_sq.is_empty());
  BLI_assert(positions.size() == r_positions.size() || r_positions.is_empty());

  for (const int i : positions.index_range()) {
    BVHTreeNearest nearest;
    nearest.dist_sq = FLT_MAX;
    const float3 position = positions[i];
    BLI_bvhtree_find_nearest(
        tree_data.tree, position, &nearest, tree_data.nearest_callback, &tree_data);
    if (!r_indices.is_empty()) {
      r_indices[i] = nearest.index;
    }
    if (!r_distances_sq.is_empty()) {
      r_distances_sq[i] = nearest.dist_sq;
    }
    if (!r_positions.is_empty()) {
      r_positions[i] = nearest.co;
    }
  }
}

static void get_closest_pointcloud_points(const PointCloud &pointcloud,
                                          const VArray<float3> &positions,
                                          const MutableSpan<int> r_indices,
                                          const MutableSpan<float> r_distances_sq)
{
  BLI_assert(positions.size() == r_indices.size());
  BLI_assert(pointcloud.totpoint > 0);

  BVHTreeFromPointCloud tree_data;
  BKE_bvhtree_from_pointcloud_get(&tree_data, &pointcloud, 2);

  for (const int i : positions.index_range()) {
    BVHTreeNearest nearest;
    nearest.dist_sq = FLT_MAX;
    const float3 position = positions[i];
    BLI_bvhtree_find_nearest(
        tree_data.tree, position, &nearest, tree_data.nearest_callback, &tree_data);
    r_indices[i] = nearest.index;
    r_distances_sq[i] = nearest.dist_sq;
  }

  free_bvhtree_from_pointcloud(&tree_data);
}

static void get_closest_mesh_points(const Mesh &mesh,
                                    const VArray<float3> &positions,
                                    const MutableSpan<int> r_point_indices,
                                    const MutableSpan<float> r_distances_sq,
                                    const MutableSpan<float3> r_positions)
{
  BLI_assert(mesh.totvert > 0);
  BVHTreeFromMesh tree_data;
  BKE_bvhtree_from_mesh_get(&tree_data, &mesh, BVHTREE_FROM_VERTS, 2);
  get_closest_in_bvhtree(tree_data, positions, r_point_indices, r_distances_sq, r_positions);
  free_bvhtree_from_mesh(&tree_data);
}

static void get_closest_mesh_edges(const Mesh &mesh,
                                   const VArray<float3> &positions,
                                   const MutableSpan<int> r_edge_indices,
                                   const MutableSpan<float> r_distances_sq,
                                   const MutableSpan<float3> r_positions)
{
  BLI_assert(mesh.totedge > 0);
  BVHTreeFromMesh tree_data;
  BKE_bvhtree_from_mesh_get(&tree_data, &mesh, BVHTREE_FROM_EDGES, 2);
  get_closest_in_bvhtree(tree_data, positions, r_edge_indices, r_distances_sq, r_positions);
  free_bvhtree_from_mesh(&tree_data);
}

static void get_closest_mesh_looptris(const Mesh &mesh,
                                      const VArray<float3> &positions,
                                      const MutableSpan<int> r_looptri_indices,
                                      const MutableSpan<float> r_distances_sq,
                                      const MutableSpan<float3> r_positions)
{
  BLI_assert(mesh.totpoly > 0);
  BVHTreeFromMesh tree_data;
  BKE_bvhtree_from_mesh_get(&tree_data, &mesh, BVHTREE_FROM_LOOPTRI, 2);
  get_closest_in_bvhtree(tree_data, positions, r_looptri_indices, r_distances_sq, r_positions);
  free_bvhtree_from_mesh(&tree_data);
}

static void get_closest_mesh_polygons(const Mesh &mesh,
                                      const VArray<float3> &positions,
                                      const MutableSpan<int> r_poly_indices,
                                      const MutableSpan<float> r_distances_sq,
                                      const MutableSpan<float3> r_positions)
{
  BLI_assert(mesh.totpoly > 0);

  Array<int> looptri_indices(positions.size());
  get_closest_mesh_looptris(mesh, positions, looptri_indices, r_distances_sq, r_positions);

  const Span<MLoopTri> looptris{BKE_mesh_runtime_looptri_ensure(&mesh),
                                BKE_mesh_runtime_looptri_len(&mesh)};
  for (const int i : positions.index_range()) {
    const MLoopTri &looptri = looptris[looptri_indices[i]];
    r_poly_indices[i] = looptri.poly;
  }
}

/* The closest corner is defined to be the closest corner on the closest face. */
static void get_closest_mesh_corners(const Mesh &mesh,
                                     const VArray<float3> &positions,
                                     const MutableSpan<int> r_corner_indices,
                                     const MutableSpan<float> r_distances_sq,
                                     const MutableSpan<float3> r_positions)
{
  BLI_assert(mesh.totloop > 0);
  Array<int> poly_indices(positions.size());
  get_closest_mesh_polygons(mesh, positions, poly_indices, {}, {});

  for (const int i : positions.index_range()) {
    const float3 position = positions[i];
    const int poly_index = poly_indices[i];
    const MPoly &poly = mesh.mpoly[poly_index];

    /* Find the closest vertex in the polygon. */
    float min_distance_sq = FLT_MAX;
    const MVert *closest_mvert;
    int closest_loop_index = 0;
    for (const int loop_index : IndexRange(poly.loopstart, poly.totloop)) {
      const MLoop &loop = mesh.mloop[loop_index];
      const int vertex_index = loop.v;
      const MVert &mvert = mesh.mvert[vertex_index];
      const float distance_sq = float3::distance_squared(position, mvert.co);
      if (distance_sq < min_distance_sq) {
        min_distance_sq = distance_sq;
        closest_loop_index = loop_index;
        closest_mvert = &mvert;
      }
    }
    if (!r_corner_indices.is_empty()) {
      r_corner_indices[i] = closest_loop_index;
    }
    if (!r_positions.is_empty()) {
      r_positions[i] = closest_mvert->co;
    }
    if (!r_distances_sq.is_empty()) {
      r_distances_sq[i] = min_distance_sq;
    }
  }
}

static void transfer_attribute_nearest_face_interpolated(const GeometrySet &src_geometry,
                                                         GeometryComponent &dst_component,
                                                         const VArray<float3> &dst_positions,
                                                         const AttributeDomain dst_domain,
                                                         const CustomDataType data_type,
                                                         const StringRef src_name,
                                                         const StringRef dst_name)
{
  const int tot_samples = dst_positions.size();
  const MeshComponent *component = src_geometry.get_component_for_read<MeshComponent>();
  if (component == nullptr) {
    return;
  }
  const Mesh *mesh = component->get_for_read();
  if (mesh == nullptr) {
    return;
  }
  if (mesh->totpoly == 0) {
    return;
  }

  ReadAttributeLookup src_attribute = component->attribute_try_get_for_read(src_name, data_type);
  OutputAttribute dst_attribute = dst_component.attribute_try_get_for_output_only(
      dst_name, dst_domain, data_type);
  if (!src_attribute || !dst_attribute) {
    return;
  }

  /* Find closest points on the mesh surface. */
  Array<int> looptri_indices(tot_samples);
  Array<float3> positions(tot_samples);
  get_closest_mesh_looptris(*mesh, dst_positions, looptri_indices, {}, positions);

  bke::mesh_surface_sample::MeshAttributeInterpolator interp(
      mesh, IndexMask(tot_samples), positions, looptri_indices);
  interp.sample_attribute(
      src_attribute, dst_attribute, bke::mesh_surface_sample::eAttributeMapMode::INTERPOLATED);

  dst_attribute.save();
}

static void transfer_attribute_nearest(const GeometrySet &src_geometry,
                                       GeometryComponent &dst_component,
                                       const VArray<float3> &dst_positions,
                                       const AttributeDomain dst_domain,
                                       const CustomDataType data_type,
                                       const StringRef src_name,
                                       const StringRef dst_name)
{
  const CPPType &type = *bke::custom_data_type_to_cpp_type(data_type);

  /* Get pointcloud data from geometry. */
  const PointCloudComponent *pointcloud_component =
      src_geometry.get_component_for_read<PointCloudComponent>();
  const PointCloud *pointcloud = pointcloud_component ? pointcloud_component->get_for_read() :
                                                        nullptr;

  /* Get mesh data from geometry. */
  const MeshComponent *mesh_component = src_geometry.get_component_for_read<MeshComponent>();
  const Mesh *mesh = mesh_component ? mesh_component->get_for_read() : nullptr;

  const int tot_samples = dst_positions.size();

  Array<int> pointcloud_indices;
  Array<float> pointcloud_distances_sq;
  bool use_pointcloud = false;

  /* Depending on where what domain the source attribute lives, these indices are either vertex,
   * corner, edge or polygon indices. */
  Array<int> mesh_indices;
  Array<float> mesh_distances_sq;
  bool use_mesh = false;

  /* If there is a pointcloud, find the closest points. */
  if (pointcloud != nullptr && pointcloud->totpoint > 0) {
    if (pointcloud_component->attribute_exists(src_name)) {
      use_pointcloud = true;
      pointcloud_indices.reinitialize(tot_samples);
      pointcloud_distances_sq.reinitialize(tot_samples);
      get_closest_pointcloud_points(
          *pointcloud, dst_positions, pointcloud_indices, pointcloud_distances_sq);
    }
  }

  /* If there is a mesh, find the closest mesh elements. */
  if (mesh != nullptr) {
    ReadAttributeLookup src_attribute = mesh_component->attribute_try_get_for_read(src_name);
    if (src_attribute) {
      switch (src_attribute.domain) {
        case ATTR_DOMAIN_POINT: {
          if (mesh->totvert > 0) {
            use_mesh = true;
            mesh_indices.reinitialize(tot_samples);
            mesh_distances_sq.reinitialize(tot_samples);
            get_closest_mesh_points(*mesh, dst_positions, mesh_indices, mesh_distances_sq, {});
          }
          break;
        }
        case ATTR_DOMAIN_EDGE: {
          if (mesh->totedge > 0) {
            use_mesh = true;
            mesh_indices.reinitialize(tot_samples);
            mesh_distances_sq.reinitialize(tot_samples);
            get_closest_mesh_edges(*mesh, dst_positions, mesh_indices, mesh_distances_sq, {});
          }
          break;
        }
        case ATTR_DOMAIN_FACE: {
          if (mesh->totpoly > 0) {
            use_mesh = true;
            mesh_indices.reinitialize(tot_samples);
            mesh_distances_sq.reinitialize(tot_samples);
            get_closest_mesh_polygons(*mesh, dst_positions, mesh_indices, mesh_distances_sq, {});
          }
          break;
        }
        case ATTR_DOMAIN_CORNER: {
          if (mesh->totloop > 0) {
            use_mesh = true;
            mesh_indices.reinitialize(tot_samples);
            mesh_distances_sq.reinitialize(tot_samples);
            get_closest_mesh_corners(*mesh, dst_positions, mesh_indices, mesh_distances_sq, {});
          }
          break;
        }
        default: {
          break;
        }
      }
    }
  }

  if (!use_pointcloud && !use_mesh) {
    return;
  }

  OutputAttribute dst_attribute = dst_component.attribute_try_get_for_output_only(
      dst_name, dst_domain, data_type);
  if (!dst_attribute) {
    return;
  }

  /* Create a buffer for intermediate values. */
  BUFFER_FOR_CPP_TYPE_VALUE(type, buffer);

  if (use_mesh && use_pointcloud) {
    /* When there is a mesh and a pointcloud, we still have to check whether a pointcloud point or
     * a mesh element is closer to every point. */
    ReadAttributeLookup pointcloud_src_attribute =
        pointcloud_component->attribute_try_get_for_read(src_name, data_type);
    ReadAttributeLookup mesh_src_attribute = mesh_component->attribute_try_get_for_read(src_name,
                                                                                        data_type);
    for (const int i : IndexRange(tot_samples)) {
      if (pointcloud_distances_sq[i] < mesh_distances_sq[i]) {
        /* Point-cloud point is closer. */
        const int index = pointcloud_indices[i];
        pointcloud_src_attribute.varray->get(index, buffer);
        dst_attribute->set_by_relocate(i, buffer);
      }
      else {
        /* Mesh element is closer. */
        const int index = mesh_indices[i];
        mesh_src_attribute.varray->get(index, buffer);
        dst_attribute->set_by_relocate(i, buffer);
      }
    }
  }
  else if (use_pointcloud) {
    /* The source geometry only has a pointcloud. */
    ReadAttributeLookup src_attribute = pointcloud_component->attribute_try_get_for_read(
        src_name, data_type);
    for (const int i : IndexRange(tot_samples)) {
      const int index = pointcloud_indices[i];
      src_attribute.varray->get(index, buffer);
      dst_attribute->set_by_relocate(i, buffer);
    }
  }
  else if (use_mesh) {
    /* The source geometry only has a mesh. */
    ReadAttributeLookup src_attribute = mesh_component->attribute_try_get_for_read(src_name,
                                                                                   data_type);
    for (const int i : IndexRange(tot_samples)) {
      const int index = mesh_indices[i];
      src_attribute.varray->get(index, buffer);
      dst_attribute->set_by_relocate(i, buffer);
    }
  }

  dst_attribute.save();
}

static void transfer_attribute(const GeoNodeExecParams &params,
                               const GeometrySet &src_geometry,
                               GeometryComponent &dst_component,
                               const StringRef src_name,
                               const StringRef dst_name)
{
  const NodeGeometryAttributeTransfer &storage =
      *(const NodeGeometryAttributeTransfer *)params.node().storage;
  const GeometryNodeAttributeTransferMapMode mapping = (GeometryNodeAttributeTransferMapMode)
                                                           storage.mapping;
  const AttributeDomain input_domain = (AttributeDomain)storage.domain;

  CustomDataType data_type;
  AttributeDomain auto_domain;
  get_result_domain_and_data_type(src_geometry, dst_component, src_name, &data_type, &auto_domain);
  const AttributeDomain dst_domain = (input_domain == ATTR_DOMAIN_AUTO) ? auto_domain :
                                                                          input_domain;

  GVArray_Typed<float3> dst_positions = dst_component.attribute_get_for_read<float3>(
      "position", dst_domain, {0, 0, 0});

  switch (mapping) {
    case GEO_NODE_LEGACY_ATTRIBUTE_TRANSFER_NEAREST_FACE_INTERPOLATED: {
      transfer_attribute_nearest_face_interpolated(
          src_geometry, dst_component, dst_positions, dst_domain, data_type, src_name, dst_name);
      break;
    }
    case GEO_NODE_LEGACY_ATTRIBUTE_TRANSFER_NEAREST: {
      transfer_attribute_nearest(
          src_geometry, dst_component, dst_positions, dst_domain, data_type, src_name, dst_name);
      break;
    }
  }
}

static void geo_node_attribute_transfer_exec(GeoNodeExecParams params)
{
  GeometrySet dst_geometry_set = params.extract_input<GeometrySet>("Geometry");
  GeometrySet src_geometry_set = params.extract_input<GeometrySet>("Source Geometry");
  const std::string src_attribute_name = params.extract_input<std::string>("Source");
  const std::string dst_attribute_name = params.extract_input<std::string>("Destination");

  if (src_attribute_name.empty() || dst_attribute_name.empty()) {
    params.set_output("Geometry", dst_geometry_set);
    return;
  }

  dst_geometry_set = bke::geometry_set_realize_instances(dst_geometry_set);
  src_geometry_set = bke::geometry_set_realize_instances(src_geometry_set);

  if (dst_geometry_set.has<MeshComponent>()) {
    transfer_attribute(params,
                       src_geometry_set,
                       dst_geometry_set.get_component_for_write<MeshComponent>(),
                       src_attribute_name,
                       dst_attribute_name);
  }
  if (dst_geometry_set.has<PointCloudComponent>()) {
    transfer_attribute(params,
                       src_geometry_set,
                       dst_geometry_set.get_component_for_write<PointCloudComponent>(),
                       src_attribute_name,
                       dst_attribute_name);
  }

  params.set_output("Geometry", dst_geometry_set);
}

}  // namespace blender::nodes

void register_node_type_geo_legacy_attribute_transfer()
{
  static bNodeType ntype;

  geo_node_type_base(
      &ntype, GEO_NODE_LEGACY_ATTRIBUTE_TRANSFER, "Attribute Transfer", NODE_CLASS_ATTRIBUTE, 0);
  node_type_init(&ntype, blender::nodes::geo_node_attribute_transfer_init);
  node_type_storage(&ntype,
                    "NodeGeometryAttributeTransfer",
                    node_free_standard_storage,
                    node_copy_standard_storage);
  ntype.declare = blender::nodes::geo_node_attribute_transfer_declare;
  ntype.geometry_node_execute = blender::nodes::geo_node_attribute_transfer_exec;
  ntype.draw_buttons = blender::nodes::geo_node_attribute_transfer_layout;
  nodeRegisterType(&ntype);
}
