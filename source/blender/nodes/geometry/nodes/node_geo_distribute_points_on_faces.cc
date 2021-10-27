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
#include "BLI_task.hh"
#include "BLI_timeit.hh"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_pointcloud_types.h"

#include "BKE_attribute_math.hh"
#include "BKE_bvhutils.h"
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
  b.add_input<decl::Geometry>("Mesh").supported_type(GEO_COMPONENT_TYPE_MESH);
  b.add_input<decl::Bool>("Selection").default_value(true).hide_value().supports_field();
  b.add_input<decl::Float>("Distance Min").min(0.0f).subtype(PROP_DISTANCE);
  b.add_input<decl::Float>("Density Max").default_value(10.0f).min(0.0f);
  b.add_input<decl::Float>("Density").default_value(10.0f).supports_field();
  b.add_input<decl::Float>("Density Factor")
      .default_value(1.0f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR)
      .supports_field();
  b.add_input<decl::Int>("Seed");

  b.add_output<decl::Geometry>("Points");
  b.add_output<decl::Vector>("Normal").field_source();
  b.add_output<decl::Vector>("Rotation").subtype(PROP_EULER).field_source();
}

static void geo_node_point_distribute_points_on_faces_layout(uiLayout *layout,
                                                             bContext *UNUSED(C),
                                                             PointerRNA *ptr)
{
  uiItemR(layout, ptr, "distribute_method", 0, "", ICON_NONE);
}

static void node_point_distribute_points_on_faces_update(bNodeTree *UNUSED(ntree), bNode *node)
{
  bNodeSocket *sock_distance_min = (bNodeSocket *)BLI_findlink(&node->inputs, 2);
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
    const float3 v0_pos = float3(mesh.mvert[v0_index].co);
    const float3 v1_pos = float3(mesh.mvert[v1_index].co);
    const float3 v2_pos = float3(mesh.mvert[v2_index].co);

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

BLI_NOINLINE static KDTree_3d *build_kdtree(Span<float3> positions)
{
  KDTree_3d *kdtree = BLI_kdtree_3d_new(positions.size());

  int i_point = 0;
  for (const float3 position : positions) {
    BLI_kdtree_3d_insert(kdtree, i_point, position);
    i_point++;
  }

  BLI_kdtree_3d_balance(kdtree);
  return kdtree;
}

BLI_NOINLINE static void update_elimination_mask_for_close_points(
    Span<float3> positions, const float minimum_distance, MutableSpan<bool> elimination_mask)
{
  if (minimum_distance <= 0.0f) {
    return;
  }

  KDTree_3d *kdtree = build_kdtree(positions);

  for (const int i : positions.index_range()) {
    if (elimination_mask[i]) {
      continue;
    }

    struct CallbackData {
      int index;
      MutableSpan<bool> elimination_mask;
    } callback_data = {i, elimination_mask};

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

BLI_NOINLINE static void propagate_existing_attributes(
    const MeshComponent &mesh_component,
    const Map<AttributeIDRef, AttributeKind> &attributes,
    GeometryComponent &point_component,
    const Span<float3> bary_coords,
    const Span<int> looptri_indices)
{
  const Mesh &mesh = *mesh_component.get_for_read();

  for (Map<AttributeIDRef, AttributeKind>::Item entry : attributes.items()) {
    const AttributeIDRef attribute_id = entry.key;
    const CustomDataType output_data_type = entry.value.data_type;
    /* The output domain is always #ATTR_DOMAIN_POINT, since we are creating a point cloud. */
    OutputAttribute attribute_out = point_component.attribute_try_get_for_output_only(
        attribute_id, ATTR_DOMAIN_POINT, output_data_type);
    if (!attribute_out) {
      continue;
    }

    GMutableSpan out_span = attribute_out.as_span();

    std::optional<AttributeMetaData> attribute_info = point_component.attribute_get_meta_data(
        attribute_id);
    if (!attribute_info) {
      continue;
    }

    const AttributeDomain source_domain = attribute_info->domain;
    GVArrayPtr source_attribute = mesh_component.attribute_get_for_read(
        attribute_id, source_domain, output_data_type, nullptr);
    if (!source_attribute) {
      continue;
    }

    interpolate_attribute(
        mesh, bary_coords, looptri_indices, source_domain, *source_attribute, out_span);

    attribute_out.save();
  }
}

namespace {
struct AttributeOutputs {
  StrongAnonymousAttributeID normal_id;
  StrongAnonymousAttributeID rotation_id;
};
}  // namespace

BLI_NOINLINE static void compute_attribute_outputs(const MeshComponent &mesh_component,
                                                   PointCloudComponent &point_component,
                                                   const Span<float3> bary_coords,
                                                   const Span<int> looptri_indices,
                                                   const AttributeOutputs &attribute_outputs)
{
  OutputAttribute_Typed<int> id_attribute = point_component.attribute_try_get_for_output_only<int>(
      "id", ATTR_DOMAIN_POINT);
  MutableSpan<int> ids = id_attribute.as_span();

  OutputAttribute_Typed<float3> normal_attribute;
  OutputAttribute_Typed<float3> rotation_attribute;

  MutableSpan<float3> normals;
  MutableSpan<float3> rotations;

  if (attribute_outputs.normal_id) {
    normal_attribute = point_component.attribute_try_get_for_output_only<float3>(
        attribute_outputs.normal_id.get(), ATTR_DOMAIN_POINT);
    normals = normal_attribute.as_span();
  }
  if (attribute_outputs.rotation_id) {
    rotation_attribute = point_component.attribute_try_get_for_output_only<float3>(
        attribute_outputs.rotation_id.get(), ATTR_DOMAIN_POINT);
    rotations = rotation_attribute.as_span();
  }

  const Mesh &mesh = *mesh_component.get_for_read();
  const Span<MLoopTri> looptris{BKE_mesh_runtime_looptri_ensure(&mesh),
                                BKE_mesh_runtime_looptri_len(&mesh)};

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

    ids[i] = noise::hash(noise::hash_float(bary_coord), looptri_index);

    float3 normal;
    if (!normals.is_empty() || !rotations.is_empty()) {
      normal_tri_v3(normal, v0_pos, v1_pos, v2_pos);
    }
    if (!normals.is_empty()) {
      normals[i] = normal;
    }
    if (!rotations.is_empty()) {
      rotations[i] = normal_to_euler_rotation(normal);
    }
  }

  id_attribute.save();

  if (normal_attribute) {
    normal_attribute.save();
  }
  if (rotation_attribute) {
    rotation_attribute.save();
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

static void distribute_points_random(const MeshComponent &component,
                                     const Field<float> &density_field,
                                     const Field<bool> &selection_field,
                                     const int seed,
                                     Vector<float3> &positions,
                                     Vector<float3> &bary_coords,
                                     Vector<int> &looptri_indices)
{
  const Array<float> densities = calc_full_density_factors_with_selection(
      component, density_field, selection_field);
  const Mesh &mesh = *component.get_for_read();
  sample_mesh_surface(mesh, 1.0f, densities, seed, positions, bary_coords, looptri_indices);
}

static void distribute_points_poisson_disk(const MeshComponent &mesh_component,
                                           const float minimum_distance,
                                           const float max_density,
                                           const Field<float> &density_factor_field,
                                           const Field<bool> &selection_field,
                                           const int seed,
                                           Vector<float3> &positions,
                                           Vector<float3> &bary_coords,
                                           Vector<int> &looptri_indices)
{
  const Mesh &mesh = *mesh_component.get_for_read();
  sample_mesh_surface(mesh, max_density, {}, seed, positions, bary_coords, looptri_indices);

  Array<bool> elimination_mask(positions.size(), false);
  update_elimination_mask_for_close_points(positions, minimum_distance, elimination_mask);

  const Array<float> density_factors = calc_full_density_factors_with_selection(
      mesh_component, density_factor_field, selection_field);

  update_elimination_mask_based_on_density_factors(
      mesh, density_factors, bary_coords, looptri_indices, elimination_mask.as_mutable_span());

  eliminate_points_based_on_mask(
      elimination_mask.as_span(), positions, bary_coords, looptri_indices);
}

static void point_distribution_calculate(GeometrySet &geometry_set,
                                         const Field<bool> selection_field,
                                         const GeometryNodeDistributePointsOnFacesMode method,
                                         const int seed,
                                         const AttributeOutputs &attribute_outputs,
                                         const GeoNodeExecParams &params)
{
  if (!geometry_set.has_mesh()) {
    return;
  }

  const MeshComponent &mesh_component = *geometry_set.get_component_for_read<MeshComponent>();

  Vector<float3> positions;
  Vector<float3> bary_coords;
  Vector<int> looptri_indices;

  switch (method) {
    case GEO_NODE_POINT_DISTRIBUTE_POINTS_ON_FACES_RANDOM: {
      const Field<float> density_field = params.get_input<Field<float>>("Density");
      distribute_points_random(mesh_component,
                               density_field,
                               selection_field,
                               seed,
                               positions,
                               bary_coords,
                               looptri_indices);
      break;
    }
    case GEO_NODE_POINT_DISTRIBUTE_POINTS_ON_FACES_POISSON: {
      const float minimum_distance = params.get_input<float>("Distance Min");
      const float density_max = params.get_input<float>("Density Max");
      const Field<float> density_factors_field = params.get_input<Field<float>>("Density Factor");
      distribute_points_poisson_disk(mesh_component,
                                     minimum_distance,
                                     density_max,
                                     density_factors_field,
                                     selection_field,
                                     seed,
                                     positions,
                                     bary_coords,
                                     looptri_indices);
      break;
    }
  }

  if (positions.is_empty()) {
    return;
  }

  PointCloud *pointcloud = BKE_pointcloud_new_nomain(positions.size());
  memcpy(pointcloud->co, positions.data(), sizeof(float3) * positions.size());
  uninitialized_fill_n(pointcloud->radius, pointcloud->totpoint, 0.05f);
  geometry_set.replace_pointcloud(pointcloud);

  PointCloudComponent &point_component =
      geometry_set.get_component_for_write<PointCloudComponent>();

  Map<AttributeIDRef, AttributeKind> attributes;
  geometry_set.gather_attributes_for_propagation(
      {GEO_COMPONENT_TYPE_MESH}, GEO_COMPONENT_TYPE_POINT_CLOUD, false, attributes);

  /* Position is set separately. */
  attributes.remove("position");

  propagate_existing_attributes(
      mesh_component, attributes, point_component, bary_coords, looptri_indices);

  compute_attribute_outputs(
      mesh_component, point_component, bary_coords, looptri_indices, attribute_outputs);
}

static void geo_node_point_distribute_points_on_faces_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Mesh");

  const GeometryNodeDistributePointsOnFacesMode method =
      static_cast<GeometryNodeDistributePointsOnFacesMode>(params.node().custom1);

  const int seed = params.get_input<int>("Seed") * 5383843;
  const Field<bool> selection_field = params.extract_input<Field<bool>>("Selection");

  AttributeOutputs attribute_outputs;
  if (params.output_is_required("Normal")) {
    attribute_outputs.normal_id = StrongAnonymousAttributeID("Normal");
  }
  if (params.output_is_required("Rotation")) {
    attribute_outputs.rotation_id = StrongAnonymousAttributeID("Rotation");
  }

  geometry_set.modify_geometry_sets([&](GeometrySet &geometry_set) {
    point_distribution_calculate(
        geometry_set, selection_field, method, seed, attribute_outputs, params);
    /* Keep instances because the original geometry set may contain instances that are processed as
     * well. */
    geometry_set.keep_only({GEO_COMPONENT_TYPE_POINT_CLOUD, GEO_COMPONENT_TYPE_INSTANCES});
  });

  params.set_output("Points", std::move(geometry_set));

  if (attribute_outputs.normal_id) {
    params.set_output(
        "Normal",
        AnonymousAttributeFieldInput::Create<float3>(std::move(attribute_outputs.normal_id),
                                                     params.attribute_producer_name()));
  }
  if (attribute_outputs.rotation_id) {
    params.set_output(
        "Rotation",
        AnonymousAttributeFieldInput::Create<float3>(std::move(attribute_outputs.rotation_id),
                                                     params.attribute_producer_name()));
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
