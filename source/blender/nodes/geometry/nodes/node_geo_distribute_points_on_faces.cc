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

#include "BLI_kdtree.h"
#include "BLI_noise.hh"
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

static void geo_node_point_distribute_points_on_faces_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>("Geometry");
  b.add_input<decl::Float>("Distance Min").min(0.0f).subtype(PROP_DISTANCE);
  b.add_input<decl::Float>("Density Max").default_value(10.0f).min(0.0f);
  b.add_input<decl::Float>("Density").default_value(10.0f).supports_field();
  b.add_input<decl::Float>("Density Factor")
      .default_value(1.0f)
      .min(0.0f)
      .max(1.0f)
      .supports_field();
  b.add_input<decl::Int>("Seed");
  b.add_input<decl::Bool>("Selection").default_value(true).hide_value().supports_field();

  b.add_output<decl::Geometry>("Points");
  b.add_output<decl::Vector>("Normal").field_source();
  b.add_output<decl::Vector>("Rotation").subtype(PROP_EULER).field_source();
  b.add_output<decl::Int>("Stable ID").field_source();
}

static void geo_node_point_distribute_points_on_faces_layout(uiLayout *layout,
                                                             bContext *UNUSED(C),
                                                             PointerRNA *ptr)
{
  uiItemR(layout, ptr, "distribute_method", 0, "", ICON_NONE);
}

static void node_point_distribute_points_on_faces_update(bNodeTree *UNUSED(ntree), bNode *node)
{
  bNodeSocket *sock_distance_min = (bNodeSocket *)BLI_findlink(&node->inputs, 1);
  bNodeSocket *sock_density_max = (bNodeSocket *)sock_distance_min->next;
  bNodeSocket *sock_density = sock_density_max->next;
  bNodeSocket *sock_density_factor = sock_density->next;
  nodeSetSocketAvailability(sock_distance_min,
                            node->custom1 == GEO_NODE_POINT_DISTRIBUTE_POINTS_ON_FACES_POISSON);
  nodeSetSocketAvailability(sock_density_max,
                            node->custom1 == GEO_NODE_POINT_DISTRIBUTE_POINTS_ON_FACES_POISSON);
  nodeSetSocketAvailability(sock_density,
                            node->custom1 == GEO_NODE_POINT_DISTRIBUTE_POINTS_ON_FACES_RANDOM);
  nodeSetSocketAvailability(sock_density_factor,
                            node->custom1 == GEO_NODE_POINT_DISTRIBUTE_POINTS_ON_FACES_POISSON);
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
                                const Span<float> density_factors,
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
    if (!density_factors.is_empty()) {
      const float v0_density_factor = std::max(0.0f, density_factors[v0_loop]);
      const float v1_density_factor = std::max(0.0f, density_factors[v1_loop]);
      const float v2_density_factor = std::max(0.0f, density_factors[v2_loop]);
      looptri_density_factor = (v0_density_factor + v1_density_factor + v2_density_factor) / 3.0f;
    }
    const float area = area_tri_v3(v0_pos, v1_pos, v2_pos);

    const int looptri_seed = noise::hash(looptri_index, seed);
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
    const Span<float> density_factors,
    const Span<float3> bary_coords,
    const Span<int> looptri_indices,
    const MutableSpan<bool> elimination_mask)
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

    const float hash = noise::hash_float_to_float(bary_coord);
    if (hash > probablity) {
      elimination_mask[i] = true;
    }
  }
}

BLI_NOINLINE static void eliminate_points_based_on_mask(const Span<bool> elimination_mask,
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
                                               const Span<float3> bary_coords,
                                               const Span<int> looptri_indices,
                                               const AttributeDomain source_domain,
                                               const GVArray &source_data,
                                               GMutableSpan output_data)
{
  switch (source_domain) {
    case ATTR_DOMAIN_POINT: {
      bke::mesh_surface_sample::sample_point_attribute(
          mesh, looptri_indices, bary_coords, source_data, output_data);
      break;
    }
    case ATTR_DOMAIN_CORNER: {
      bke::mesh_surface_sample::sample_corner_attribute(
          mesh, looptri_indices, bary_coords, source_data, output_data);
      break;
    }
    case ATTR_DOMAIN_FACE: {
      bke::mesh_surface_sample::sample_face_attribute(
          mesh, looptri_indices, source_data, output_data);
      break;
    }
    default: {
      /* Not supported currently. */
      return;
    }
  }
}

BLI_NOINLINE static void propagate_existing_attributes(
    const Span<GeometryInstanceGroup> set_groups,
    const Span<int> instance_start_offsets,
    const Map<AttributeIDRef, AttributeKind> &attributes,
    GeometryComponent &component,
    const Span<Vector<float3>> bary_coords_array,
    const Span<Vector<int>> looptri_indices_array)
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

namespace {
struct AttributeOutputs {
  StrongAnonymousAttributeID normal_id;
  StrongAnonymousAttributeID rotation_id;
  StrongAnonymousAttributeID stable_id_id;
};
}  // namespace

BLI_NOINLINE static void compute_attribute_outputs(const Span<GeometryInstanceGroup> sets,
                                                   const Span<int> instance_start_offsets,
                                                   GeometryComponent &component,
                                                   const Span<Vector<float3>> bary_coords_array,
                                                   const Span<Vector<int>> looptri_indices_array,
                                                   const AttributeOutputs &attribute_outputs)
{
  std::optional<OutputAttribute_Typed<int>> id_attribute;
  std::optional<OutputAttribute_Typed<float3>> normal_attribute;
  std::optional<OutputAttribute_Typed<float3>> rotation_attribute;

  MutableSpan<int> result_ids;
  MutableSpan<float3> result_normals;
  MutableSpan<float3> result_rotations;

  if (attribute_outputs.stable_id_id) {
    id_attribute.emplace(component.attribute_try_get_for_output_only<int>(
        attribute_outputs.stable_id_id.get(), ATTR_DOMAIN_POINT));
    result_ids = id_attribute->as_span();
  }
  if (attribute_outputs.normal_id) {
    normal_attribute.emplace(component.attribute_try_get_for_output_only<float3>(
        attribute_outputs.normal_id.get(), ATTR_DOMAIN_POINT));
    result_normals = normal_attribute->as_span();
  }
  if (attribute_outputs.rotation_id) {
    rotation_attribute.emplace(component.attribute_try_get_for_output_only<float3>(
        attribute_outputs.rotation_id.get(), ATTR_DOMAIN_POINT));
    result_rotations = rotation_attribute->as_span();
  }

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

        if (!result_ids.is_empty()) {
          ids[i] = noise::hash(noise::hash_float(bary_coord), looptri_index);
        }
        float3 normal;
        if (!result_normals.is_empty() || !result_rotations.is_empty()) {
          normal_tri_v3(normal, v0_pos, v1_pos, v2_pos);
          mul_m3_v3(rotation_matrix, normal);
        }
        if (!result_normals.is_empty()) {
          normals[i] = normal;
        }
        if (!result_rotations.is_empty()) {
          rotations[i] = normal_to_euler_rotation(normal);
        }
      }

      i_instance++;
    }
  }

  if (id_attribute) {
    id_attribute->save();
  }
  if (normal_attribute) {
    normal_attribute->save();
  }
  if (rotation_attribute) {
    rotation_attribute->save();
  }
}

static Array<float> calc_full_density_factors_with_selection(const MeshComponent &component,
                                                             const Field<float> &density_field,
                                                             const Field<bool> &selection_field)
{
  const AttributeDomain attribute_domain = ATTR_DOMAIN_CORNER;
  GeometryComponentFieldContext field_context{component, attribute_domain};
  const int domain_size = component.attribute_domain_size(attribute_domain);

  fn::FieldEvaluator selection_evaluator{field_context, domain_size};
  selection_evaluator.add(selection_field);
  selection_evaluator.evaluate();
  const IndexMask selection_mask = selection_evaluator.get_evaluated_as_mask(0);

  Array<float> densities(domain_size, 0.0f);

  fn::FieldEvaluator density_evaluator{field_context, &selection_mask};
  density_evaluator.add_with_destination(density_field, densities.as_mutable_span());
  density_evaluator.evaluate();
  return densities;
}

static void distribute_points_random(Span<GeometryInstanceGroup> set_groups,
                                     const Field<float> &density_field,
                                     const Field<bool> &selection_field,
                                     const int seed,
                                     MutableSpan<Vector<float3>> positions_all,
                                     MutableSpan<Vector<float3>> bary_coords_all,
                                     MutableSpan<Vector<int>> looptri_indices_all)
{
  int i_instance = 0;
  for (const GeometryInstanceGroup &set_group : set_groups) {
    const GeometrySet &set = set_group.geometry_set;
    const MeshComponent &component = *set.get_component_for_read<MeshComponent>();
    const Array<float> densities = calc_full_density_factors_with_selection(
        component, density_field, selection_field);
    const Mesh &mesh = *component.get_for_read();
    for (const float4x4 &transform : set_group.transforms) {
      Vector<float3> &positions = positions_all[i_instance];
      Vector<float3> &bary_coords = bary_coords_all[i_instance];
      Vector<int> &looptri_indices = looptri_indices_all[i_instance];
      const int instance_seed = noise::hash(seed, i_instance);
      sample_mesh_surface(mesh,
                          transform,
                          1.0f,
                          densities,
                          instance_seed,
                          positions,
                          bary_coords,
                          looptri_indices);
      i_instance++;
    }
  }
}

static void distribute_points_poisson_disk(Span<GeometryInstanceGroup> set_groups,
                                           const float minimum_distance,
                                           const float max_density,
                                           const Field<float> &density_factor_field,
                                           const Field<bool> &selection_field,
                                           const int seed,
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
      const int instance_seed = noise::hash(seed, i_instance);
      sample_mesh_surface(mesh,
                          transform,
                          max_density,
                          {},
                          instance_seed,
                          positions,
                          bary_coords,
                          looptri_indices);

      instance_start_offsets[i_instance] = initial_points_len;
      initial_points_len += positions.size();
      i_instance++;
    }
  }

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

    const Array<float> density_factors = calc_full_density_factors_with_selection(
        component, density_factor_field, selection_field);

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

static void geo_node_point_distribute_points_on_faces_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Geometry");

  const GeometryNodeDistributePointsOnFacesMode distribute_method =
      static_cast<GeometryNodeDistributePointsOnFacesMode>(params.node().custom1);

  const int seed = params.get_input<int>("Seed") * 5383843;
  const Field<bool> selection_field = params.extract_input<Field<bool>>("Selection");

  Vector<GeometryInstanceGroup> set_groups;
  geometry_set_gather_instances(geometry_set, set_groups);
  if (set_groups.is_empty()) {
    params.set_output("Points", GeometrySet());
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
    params.set_output("Points", GeometrySet());
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
    case GEO_NODE_POINT_DISTRIBUTE_POINTS_ON_FACES_RANDOM: {
      const Field<float> density_field = params.extract_input<Field<float>>("Density");
      distribute_points_random(set_groups,
                               density_field,
                               selection_field,
                               seed,
                               positions_all,
                               bary_coords_all,
                               looptri_indices_all);
      break;
    }
    case GEO_NODE_POINT_DISTRIBUTE_POINTS_ON_FACES_POISSON: {
      const float minimum_distance = params.extract_input<float>("Distance Min");
      const float density_max = params.extract_input<float>("Density Max");
      const Field<float> density_factors_field = params.extract_input<Field<float>>(
          "Density Factor");
      distribute_points_poisson_disk(set_groups,
                                     minimum_distance,
                                     density_max,
                                     density_factors_field,
                                     selection_field,
                                     seed,
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
  geometry_set.gather_attributes_for_propagation(
      {GEO_COMPONENT_TYPE_MESH}, GEO_COMPONENT_TYPE_POINT_CLOUD, true, attributes);

  /* Position is set separately. */
  attributes.remove("position");

  propagate_existing_attributes(set_groups,
                                instance_start_offsets,
                                attributes,
                                point_component,
                                bary_coords_all,
                                looptri_indices_all);

  AttributeOutputs attribute_outputs;
  if (params.output_is_required("Normal")) {
    attribute_outputs.normal_id = StrongAnonymousAttributeID("normal");
  }
  if (params.output_is_required("Rotation")) {
    attribute_outputs.rotation_id = StrongAnonymousAttributeID("rotation");
  }
  if (params.output_is_required("Stable ID")) {
    attribute_outputs.stable_id_id = StrongAnonymousAttributeID("stable id");
  }

  compute_attribute_outputs(set_groups,
                            instance_start_offsets,
                            point_component,
                            bary_coords_all,
                            looptri_indices_all,
                            attribute_outputs);

  params.set_output("Points", std::move(geometry_set_out));

  if (attribute_outputs.normal_id) {
    params.set_output(
        "Normal",
        AnonymousAttributeFieldInput::Create<float3>(std::move(attribute_outputs.normal_id)));
  }
  if (attribute_outputs.rotation_id) {
    params.set_output(
        "Rotation",
        AnonymousAttributeFieldInput::Create<float3>(std::move(attribute_outputs.rotation_id)));
  }
  if (attribute_outputs.stable_id_id) {
    params.set_output(
        "Stable ID",
        AnonymousAttributeFieldInput::Create<int>(std::move(attribute_outputs.stable_id_id)));
  }
}

}  // namespace blender::nodes

void register_node_type_geo_distribute_points_on_faces()
{
  static bNodeType ntype;

  geo_node_type_base(&ntype,
                     GEO_NODE_DISTRIBUTE_POINTS_ON_FACES,
                     "Distribute Points on Faces",
                     NODE_CLASS_GEOMETRY,
                     0);
  node_type_update(&ntype, blender::nodes::node_point_distribute_points_on_faces_update);
  node_type_size(&ntype, 170, 100, 320);
  ntype.declare = blender::nodes::geo_node_point_distribute_points_on_faces_declare;
  ntype.geometry_node_execute = blender::nodes::geo_node_point_distribute_points_on_faces_exec;
  ntype.draw_buttons = blender::nodes::geo_node_point_distribute_points_on_faces_layout;
  nodeRegisterType(&ntype);
}
