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

#include "BLI_hash.h"
#include "BLI_kdtree.h"
#include "BLI_rand.hh"
#include "BLI_timeit.hh"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_pointcloud_types.h"

#include "BKE_attribute_math.hh"
#include "BKE_bvhutils.h"
#include "BKE_geometry_set_instances.hh"
#include "BKE_mesh.h"
#include "BKE_mesh_runtime.h"
#include "BKE_mesh_sample.hh"
#include "BKE_pointcloud.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "node_geometry_util.hh"

using blender::bke::GeometryInstanceGroup;

namespace blender::nodes {

static void geo_node_point_distribute_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>("Geometry");
  b.add_input<decl::Float>("Distance Min").min(0.0f).max(100000.0f).subtype(PROP_DISTANCE);
  b.add_input<decl::Float>("Density Max")
      .default_value(1.0f)
      .min(0.0f)
      .max(100000.0f)
      .subtype(PROP_NONE);
  b.add_input<decl::String>("Density Attribute");
  b.add_input<decl::Int>("Seed").min(-10000).max(10000);
  b.add_output<decl::Geometry>("Geometry");
}

static void geo_node_point_distribute_layout(uiLayout *layout,
                                             bContext *UNUSED(C),
                                             PointerRNA *ptr)
{
  uiItemR(layout, ptr, "distribute_method", 0, "", ICON_NONE);
}

static void node_point_distribute_update(bNodeTree *UNUSED(ntree), bNode *node)
{
  bNodeSocket *sock_min_dist = (bNodeSocket *)BLI_findlink(&node->inputs, 1);

  nodeSetSocketAvailability(sock_min_dist, ELEM(node->custom1, GEO_NODE_POINT_DISTRIBUTE_POISSON));
}

/**
 * Use an arbitrary choice of axes for a usable rotation attribute directly out of this node.
 */
static float3 normal_to_euler_rotation(const float3 normal)
{
  float quat[4];
  vec_to_quat(quat, normal, OB_NEGZ, OB_POSY);
  float3 rotation;
  quat_to_eul(rotation, quat);
  return rotation;
}

static void sample_mesh_surface(const Mesh &mesh,
                                const float4x4 &transform,
                                const float base_density,
                                const VArray<float> *density_factors,
                                const int seed,
                                Vector<float3> &r_positions,
                                Vector<float3> &r_bary_coords,
                                Vector<int> &r_looptri_indices)
{
  const Span<MLoopTri> looptris{BKE_mesh_runtime_looptri_ensure(&mesh),
                                BKE_mesh_runtime_looptri_len(&mesh)};

  for (const int looptri_index : looptris.index_range()) {
    const MLoopTri &looptri = looptris[looptri_index];
    const int v0_loop = looptri.tri[0];
    const int v1_loop = looptri.tri[1];
    const int v2_loop = looptri.tri[2];
    const int v0_index = mesh.mloop[v0_loop].v;
    const int v1_index = mesh.mloop[v1_loop].v;
    const int v2_index = mesh.mloop[v2_loop].v;
    const float3 v0_pos = transform * float3(mesh.mvert[v0_index].co);
    const float3 v1_pos = transform * float3(mesh.mvert[v1_index].co);
    const float3 v2_pos = transform * float3(mesh.mvert[v2_index].co);

    float looptri_density_factor = 1.0f;
    if (density_factors != nullptr) {
      const float v0_density_factor = std::max(0.0f, density_factors->get(v0_loop));
      const float v1_density_factor = std::max(0.0f, density_factors->get(v1_loop));
      const float v2_density_factor = std::max(0.0f, density_factors->get(v2_loop));
      looptri_density_factor = (v0_density_factor + v1_density_factor + v2_density_factor) / 3.0f;
    }
    const float area = area_tri_v3(v0_pos, v1_pos, v2_pos);

    const int looptri_seed = BLI_hash_int(looptri_index + seed);
    RandomNumberGenerator looptri_rng(looptri_seed);

    const float points_amount_fl = area * base_density * looptri_density_factor;
    const float add_point_probability = fractf(points_amount_fl);
    const bool add_point = add_point_probability > looptri_rng.get_float();
    const int point_amount = (int)points_amount_fl + (int)add_point;

    for (int i = 0; i < point_amount; i++) {
      const float3 bary_coord = looptri_rng.get_barycentric_coordinates();
      float3 point_pos;
      interp_v3_v3v3v3(point_pos, v0_pos, v1_pos, v2_pos, bary_coord);
      r_positions.append(point_pos);
      r_bary_coords.append(bary_coord);
      r_looptri_indices.append(looptri_index);
    }
  }
}

BLI_NOINLINE static KDTree_3d *build_kdtree(Span<Vector<float3>> positions_all,
                                            const int initial_points_len)
{
  KDTree_3d *kdtree = BLI_kdtree_3d_new(initial_points_len);

  int i_point = 0;
  for (const Vector<float3> &positions : positions_all) {
    for (const float3 position : positions) {
      BLI_kdtree_3d_insert(kdtree, i_point, position);
      i_point++;
    }
  }
  BLI_kdtree_3d_balance(kdtree);
  return kdtree;
}

BLI_NOINLINE static void update_elimination_mask_for_close_points(
    Span<Vector<float3>> positions_all,
    Span<int> instance_start_offsets,
    const float minimum_distance,
    MutableSpan<bool> elimination_mask,
    const int initial_points_len)
{
  if (minimum_distance <= 0.0f) {
    return;
  }

  KDTree_3d *kdtree = build_kdtree(positions_all, initial_points_len);

  /* The elimination mask is a flattened array for every point,
   * so keep track of the index to it separately. */
  for (const int i_instance : positions_all.index_range()) {
    Span<float3> positions = positions_all[i_instance];
    const int offset = instance_start_offsets[i_instance];

    for (const int i : positions.index_range()) {
      if (elimination_mask[offset + i]) {
        continue;
      }

      struct CallbackData {
        int index;
        MutableSpan<bool> elimination_mask;
      } callback_data = {offset + i, elimination_mask};

      BLI_kdtree_3d_range_search_cb(
          kdtree,
          positions[i],
          minimum_distance,
          [](void *user_data, int index, const float *UNUSED(co), float UNUSED(dist_sq)) {
            CallbackData &callback_data = *static_cast<CallbackData *>(user_data);
            if (index != callback_data.index) {
              callback_data.elimination_mask[index] = true;
            }
            return true;
          },
          &callback_data);
    }
  }
  BLI_kdtree_3d_free(kdtree);
}

BLI_NOINLINE static void update_elimination_mask_based_on_density_factors(
    const Mesh &mesh,
    const VArray<float> &density_factors,
    Span<float3> bary_coords,
    Span<int> looptri_indices,
    MutableSpan<bool> elimination_mask)
{
  const Span<MLoopTri> looptris{BKE_mesh_runtime_looptri_ensure(&mesh),
                                BKE_mesh_runtime_looptri_len(&mesh)};
  for (const int i : bary_coords.index_range()) {
    if (elimination_mask[i]) {
      continue;
    }

    const MLoopTri &looptri = looptris[looptri_indices[i]];
    const float3 bary_coord = bary_coords[i];

    const int v0_loop = looptri.tri[0];
    const int v1_loop = looptri.tri[1];
    const int v2_loop = looptri.tri[2];

    const float v0_density_factor = std::max(0.0f, density_factors[v0_loop]);
    const float v1_density_factor = std::max(0.0f, density_factors[v1_loop]);
    const float v2_density_factor = std::max(0.0f, density_factors[v2_loop]);

    const float probablity = v0_density_factor * bary_coord.x + v1_density_factor * bary_coord.y +
                             v2_density_factor * bary_coord.z;

    const float hash = BLI_hash_int_01(bary_coord.hash());
    if (hash > probablity) {
      elimination_mask[i] = true;
    }
  }
}

BLI_NOINLINE static void eliminate_points_based_on_mask(Span<bool> elimination_mask,
                                                        Vector<float3> &positions,
                                                        Vector<float3> &bary_coords,
                                                        Vector<int> &looptri_indices)
{
  for (int i = positions.size() - 1; i >= 0; i--) {
    if (elimination_mask[i]) {
      positions.remove_and_reorder(i);
      bary_coords.remove_and_reorder(i);
      looptri_indices.remove_and_reorder(i);
    }
  }
}

BLI_NOINLINE static void interpolate_attribute(const Mesh &mesh,
                                               Span<float3> bary_coords,
                                               Span<int> looptri_indices,
                                               const AttributeDomain source_domain,
                                               const GVArray &source_data,
                                               GMutableSpan output_data)
{
  switch (source_domain) {
    case ATTR_DOMAIN_POINT: {
      bke::mesh_surface_sample::sample_point_attribute(mesh,
                                                       looptri_indices,
                                                       bary_coords,
                                                       source_data,
                                                       IndexMask(output_data.size()),
                                                       output_data);
      break;
    }
    case ATTR_DOMAIN_CORNER: {
      bke::mesh_surface_sample::sample_corner_attribute(mesh,
                                                        looptri_indices,
                                                        bary_coords,
                                                        source_data,
                                                        IndexMask(output_data.size()),
                                                        output_data);
      break;
    }
    case ATTR_DOMAIN_FACE: {
      bke::mesh_surface_sample::sample_face_attribute(
          mesh, looptri_indices, source_data, IndexMask(output_data.size()), output_data);
      break;
    }
    default: {
      /* Not supported currently. */
      return;
    }
  }
}

BLI_NOINLINE static void interpolate_existing_attributes(
    Span<GeometryInstanceGroup> set_groups,
    Span<int> instance_start_offsets,
    const Map<AttributeIDRef, AttributeKind> &attributes,
    GeometryComponent &component,
    Span<Vector<float3>> bary_coords_array,
    Span<Vector<int>> looptri_indices_array)
{
  for (Map<AttributeIDRef, AttributeKind>::Item entry : attributes.items()) {
    const AttributeIDRef attribute_id = entry.key;
    const CustomDataType output_data_type = entry.value.data_type;
    /* The output domain is always #ATTR_DOMAIN_POINT, since we are creating a point cloud. */
    OutputAttribute attribute_out = component.attribute_try_get_for_output_only(
        attribute_id, ATTR_DOMAIN_POINT, output_data_type);
    if (!attribute_out) {
      continue;
    }

    GMutableSpan out_span = attribute_out.as_span();

    int i_instance = 0;
    for (const GeometryInstanceGroup &set_group : set_groups) {
      const GeometrySet &set = set_group.geometry_set;
      const MeshComponent &source_component = *set.get_component_for_read<MeshComponent>();
      const Mesh &mesh = *source_component.get_for_read();

      std::optional<AttributeMetaData> attribute_info = component.attribute_get_meta_data(
          attribute_id);
      if (!attribute_info) {
        i_instance += set_group.transforms.size();
        continue;
      }

      const AttributeDomain source_domain = attribute_info->domain;
      GVArrayPtr source_attribute = source_component.attribute_get_for_read(
          attribute_id, source_domain, output_data_type, nullptr);
      if (!source_attribute) {
        i_instance += set_group.transforms.size();
        continue;
      }

      for (const int UNUSED(i_set_instance) : set_group.transforms.index_range()) {
        const int offset = instance_start_offsets[i_instance];
        Span<float3> bary_coords = bary_coords_array[i_instance];
        Span<int> looptri_indices = looptri_indices_array[i_instance];

        GMutableSpan instance_span = out_span.slice(offset, bary_coords.size());
        interpolate_attribute(
            mesh, bary_coords, looptri_indices, source_domain, *source_attribute, instance_span);

        i_instance++;
      }

      attribute_math::convert_to_static_type(output_data_type, [&](auto dummy) {
        using T = decltype(dummy);

        GVArray_Span<T> source_span{*source_attribute};
      });
    }

    attribute_out.save();
  }
}

BLI_NOINLINE static void compute_special_attributes(Span<GeometryInstanceGroup> sets,
                                                    Span<int> instance_start_offsets,
                                                    GeometryComponent &component,
                                                    Span<Vector<float3>> bary_coords_array,
                                                    Span<Vector<int>> looptri_indices_array)
{
  OutputAttribute_Typed<int> id_attribute = component.attribute_try_get_for_output_only<int>(
      "id", ATTR_DOMAIN_POINT);
  OutputAttribute_Typed<float3> normal_attribute =
      component.attribute_try_get_for_output_only<float3>("normal", ATTR_DOMAIN_POINT);
  OutputAttribute_Typed<float3> rotation_attribute =
      component.attribute_try_get_for_output_only<float3>("rotation", ATTR_DOMAIN_POINT);

  MutableSpan<int> result_ids = id_attribute.as_span();
  MutableSpan<float3> result_normals = normal_attribute.as_span();
  MutableSpan<float3> result_rotations = rotation_attribute.as_span();

  int i_instance = 0;
  for (const GeometryInstanceGroup &set_group : sets) {
    const GeometrySet &set = set_group.geometry_set;
    const MeshComponent &component = *set.get_component_for_read<MeshComponent>();
    const Mesh &mesh = *component.get_for_read();
    const Span<MLoopTri> looptris{BKE_mesh_runtime_looptri_ensure(&mesh),
                                  BKE_mesh_runtime_looptri_len(&mesh)};

    for (const float4x4 &transform : set_group.transforms) {
      const int offset = instance_start_offsets[i_instance];

      Span<float3> bary_coords = bary_coords_array[i_instance];
      Span<int> looptri_indices = looptri_indices_array[i_instance];
      MutableSpan<int> ids = result_ids.slice(offset, bary_coords.size());
      MutableSpan<float3> normals = result_normals.slice(offset, bary_coords.size());
      MutableSpan<float3> rotations = result_rotations.slice(offset, bary_coords.size());

      /* Use one matrix multiplication per point instead of three (for each triangle corner). */
      float rotation_matrix[3][3];
      mat4_to_rot(rotation_matrix, transform.values);

      for (const int i : bary_coords.index_range()) {
        const int looptri_index = looptri_indices[i];
        const MLoopTri &looptri = looptris[looptri_index];
        const float3 &bary_coord = bary_coords[i];

        const int v0_index = mesh.mloop[looptri.tri[0]].v;
        const int v1_index = mesh.mloop[looptri.tri[1]].v;
        const int v2_index = mesh.mloop[looptri.tri[2]].v;
        const float3 v0_pos = float3(mesh.mvert[v0_index].co);
        const float3 v1_pos = float3(mesh.mvert[v1_index].co);
        const float3 v2_pos = float3(mesh.mvert[v2_index].co);

        ids[i] = (int)(bary_coord.hash() + (uint64_t)looptri_index);
        normal_tri_v3(normals[i], v0_pos, v1_pos, v2_pos);
        mul_m3_v3(rotation_matrix, normals[i]);
        rotations[i] = normal_to_euler_rotation(normals[i]);
      }

      i_instance++;
    }
  }

  id_attribute.save();
  normal_attribute.save();
  rotation_attribute.save();
}

BLI_NOINLINE static void add_remaining_point_attributes(
    Span<GeometryInstanceGroup> set_groups,
    Span<int> instance_start_offsets,
    const Map<AttributeIDRef, AttributeKind> &attributes,
    GeometryComponent &component,
    Span<Vector<float3>> bary_coords_array,
    Span<Vector<int>> looptri_indices_array)
{
  interpolate_existing_attributes(set_groups,
                                  instance_start_offsets,
                                  attributes,
                                  component,
                                  bary_coords_array,
                                  looptri_indices_array);
  compute_special_attributes(
      set_groups, instance_start_offsets, component, bary_coords_array, looptri_indices_array);
}

static void distribute_points_random(Span<GeometryInstanceGroup> set_groups,
                                     const StringRef density_attribute_name,
                                     const float density,
                                     const int seed,
                                     MutableSpan<Vector<float3>> positions_all,
                                     MutableSpan<Vector<float3>> bary_coords_all,
                                     MutableSpan<Vector<int>> looptri_indices_all)
{
  /* If there is an attribute name, the default value for the densities should be zero so that
   * points are only scattered where the attribute exists. Otherwise, just "ignore" the density
   * factors. */
  const bool use_one_default = density_attribute_name.is_empty();

  int i_instance = 0;
  for (const GeometryInstanceGroup &set_group : set_groups) {
    const GeometrySet &set = set_group.geometry_set;
    const MeshComponent &component = *set.get_component_for_read<MeshComponent>();
    GVArray_Typed<float> density_factors = component.attribute_get_for_read<float>(
        density_attribute_name, ATTR_DOMAIN_CORNER, use_one_default ? 1.0f : 0.0f);
    const Mesh &mesh = *component.get_for_read();
    for (const float4x4 &transform : set_group.transforms) {
      Vector<float3> &positions = positions_all[i_instance];
      Vector<float3> &bary_coords = bary_coords_all[i_instance];
      Vector<int> &looptri_indices = looptri_indices_all[i_instance];
      sample_mesh_surface(mesh,
                          transform,
                          density,
                          &*density_factors,
                          seed,
                          positions,
                          bary_coords,
                          looptri_indices);
      i_instance++;
    }
  }
}

static void distribute_points_poisson_disk(Span<GeometryInstanceGroup> set_groups,
                                           const StringRef density_attribute_name,
                                           const float density,
                                           const int seed,
                                           const float minimum_distance,
                                           MutableSpan<Vector<float3>> positions_all,
                                           MutableSpan<Vector<float3>> bary_coords_all,
                                           MutableSpan<Vector<int>> looptri_indices_all)
{
  Array<int> instance_start_offsets(positions_all.size());
  int initial_points_len = 0;
  int i_instance = 0;
  for (const GeometryInstanceGroup &set_group : set_groups) {
    const GeometrySet &set = set_group.geometry_set;
    const MeshComponent &component = *set.get_component_for_read<MeshComponent>();
    const Mesh &mesh = *component.get_for_read();
    for (const float4x4 &transform : set_group.transforms) {
      Vector<float3> &positions = positions_all[i_instance];
      Vector<float3> &bary_coords = bary_coords_all[i_instance];
      Vector<int> &looptri_indices = looptri_indices_all[i_instance];
      sample_mesh_surface(
          mesh, transform, density, nullptr, seed, positions, bary_coords, looptri_indices);

      instance_start_offsets[i_instance] = initial_points_len;
      initial_points_len += positions.size();
      i_instance++;
    }
  }

  /* If there is an attribute name, the default value for the densities should be zero so that
   * points are only scattered where the attribute exists. Otherwise, just "ignore" the density
   * factors. */
  const bool use_one_default = density_attribute_name.is_empty();

  /* Unlike the other result arrays, the elimination mask in stored as a flat array for every
   * point, in order to simplify culling points from the KDTree (which needs to know about all
   * points at once). */
  Array<bool> elimination_mask(initial_points_len, false);
  update_elimination_mask_for_close_points(positions_all,
                                           instance_start_offsets,
                                           minimum_distance,
                                           elimination_mask,
                                           initial_points_len);

  i_instance = 0;
  for (const GeometryInstanceGroup &set_group : set_groups) {
    const GeometrySet &set = set_group.geometry_set;
    const MeshComponent &component = *set.get_component_for_read<MeshComponent>();
    const Mesh &mesh = *component.get_for_read();
    const GVArray_Typed<float> density_factors = component.attribute_get_for_read<float>(
        density_attribute_name, ATTR_DOMAIN_CORNER, use_one_default ? 1.0f : 0.0f);

    for (const int UNUSED(i_set_instance) : set_group.transforms.index_range()) {
      Vector<float3> &positions = positions_all[i_instance];
      Vector<float3> &bary_coords = bary_coords_all[i_instance];
      Vector<int> &looptri_indices = looptri_indices_all[i_instance];

      const int offset = instance_start_offsets[i_instance];
      update_elimination_mask_based_on_density_factors(
          mesh,
          density_factors,
          bary_coords,
          looptri_indices,
          elimination_mask.as_mutable_span().slice(offset, positions.size()));

      eliminate_points_based_on_mask(elimination_mask.as_span().slice(offset, positions.size()),
                                     positions,
                                     bary_coords,
                                     looptri_indices);

      i_instance++;
    }
  }
}

static void geo_node_point_distribute_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Geometry");

  const GeometryNodePointDistributeMode distribute_method =
      static_cast<GeometryNodePointDistributeMode>(params.node().custom1);

  const int seed = params.get_input<int>("Seed") * 5383843;
  const float density = params.extract_input<float>("Density Max");
  const std::string density_attribute_name = params.extract_input<std::string>(
      "Density Attribute");

  if (density <= 0.0f) {
    params.set_output("Geometry", GeometrySet());
    return;
  }

  Vector<GeometryInstanceGroup> set_groups;
  geometry_set_gather_instances(geometry_set, set_groups);
  if (set_groups.is_empty()) {
    params.set_output("Geometry", GeometrySet());
    return;
  }

  /* Remove any set inputs that don't contain a mesh, to avoid checking later on. */
  for (int i = set_groups.size() - 1; i >= 0; i--) {
    const GeometrySet &set = set_groups[i].geometry_set;
    if (!set.has_mesh()) {
      set_groups.remove_and_reorder(i);
    }
  }

  if (set_groups.is_empty()) {
    params.error_message_add(NodeWarningType::Error, TIP_("Input geometry must contain a mesh"));
    params.set_output("Geometry", GeometrySet());
    return;
  }

  int instances_len = 0;
  for (GeometryInstanceGroup &set_group : set_groups) {
    instances_len += set_group.transforms.size();
  }

  /* Store data per-instance in order to simplify attribute access after the scattering,
   * and to make the point elimination simpler for the poisson disk mode. Note that some
   * vectors will be empty if any instances don't contain mesh data. */
  Array<Vector<float3>> positions_all(instances_len);
  Array<Vector<float3>> bary_coords_all(instances_len);
  Array<Vector<int>> looptri_indices_all(instances_len);

  switch (distribute_method) {
    case GEO_NODE_POINT_DISTRIBUTE_RANDOM: {
      distribute_points_random(set_groups,
                               density_attribute_name,
                               density,
                               seed,
                               positions_all,
                               bary_coords_all,
                               looptri_indices_all);
      break;
    }
    case GEO_NODE_POINT_DISTRIBUTE_POISSON: {
      const float minimum_distance = params.extract_input<float>("Distance Min");
      distribute_points_poisson_disk(set_groups,
                                     density_attribute_name,
                                     density,
                                     seed,
                                     minimum_distance,
                                     positions_all,
                                     bary_coords_all,
                                     looptri_indices_all);
      break;
    }
  }

  int final_points_len = 0;
  Array<int> instance_start_offsets(set_groups.size());
  for (const int i : positions_all.index_range()) {
    Vector<float3> &positions = positions_all[i];
    instance_start_offsets[i] = final_points_len;
    final_points_len += positions.size();
  }

  if (final_points_len == 0) {
    params.set_output("Geometry", GeometrySet());
    return;
  }

  PointCloud *pointcloud = BKE_pointcloud_new_nomain(final_points_len);
  for (const int instance_index : positions_all.index_range()) {
    const int offset = instance_start_offsets[instance_index];
    Span<float3> positions = positions_all[instance_index];
    memcpy(pointcloud->co + offset, positions.data(), sizeof(float3) * positions.size());
  }

  uninitialized_fill_n(pointcloud->radius, pointcloud->totpoint, 0.05f);

  GeometrySet geometry_set_out = GeometrySet::create_with_pointcloud(pointcloud);
  PointCloudComponent &point_component =
      geometry_set_out.get_component_for_write<PointCloudComponent>();

  Map<AttributeIDRef, AttributeKind> attributes;
  bke::geometry_set_gather_instances_attribute_info(
      set_groups, {GEO_COMPONENT_TYPE_MESH}, {"position", "normal", "id"}, attributes);
  add_remaining_point_attributes(set_groups,
                                 instance_start_offsets,
                                 attributes,
                                 point_component,
                                 bary_coords_all,
                                 looptri_indices_all);

  params.set_output("Geometry", std::move(geometry_set_out));
}

}  // namespace blender::nodes

void register_node_type_geo_point_distribute()
{
  static bNodeType ntype;

  geo_node_type_base(
      &ntype, GEO_NODE_LEGACY_POINT_DISTRIBUTE, "Point Distribute", NODE_CLASS_GEOMETRY, 0);
  node_type_update(&ntype, blender::nodes::node_point_distribute_update);
  ntype.declare = blender::nodes::geo_node_point_distribute_declare;
  ntype.geometry_node_execute = blender::nodes::geo_node_point_distribute_exec;
  ntype.draw_buttons = blender::nodes::geo_node_point_distribute_layout;
  nodeRegisterType(&ntype);
}
