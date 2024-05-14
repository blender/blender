/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_kdtree.h"
#include "BLI_math_geom.h"
#include "BLI_math_rotation.h"
#include "BLI_noise.hh"
#include "BLI_rand.hh"
#include "BLI_task.hh"

#include "DNA_pointcloud_types.h"

#include "BKE_attribute_math.hh"
#include "BKE_mesh.hh"
#include "BKE_mesh_sample.hh"
#include "BKE_pointcloud.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "GEO_randomize.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_distribute_points_on_faces_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  auto enable_random = [](bNode &node) {
    node.custom1 = GEO_NODE_POINT_DISTRIBUTE_POINTS_ON_FACES_RANDOM;
  };
  auto enable_poisson = [](bNode &node) {
    node.custom1 = GEO_NODE_POINT_DISTRIBUTE_POINTS_ON_FACES_POISSON;
  };

  b.add_input<decl::Geometry>("Mesh").supported_type(GeometryComponent::Type::Mesh);
  b.add_input<decl::Bool>("Selection").default_value(true).hide_value().field_on_all();
  b.add_input<decl::Float>("Distance Min")
      .min(0.0f)
      .subtype(PROP_DISTANCE)
      .make_available(enable_poisson);
  b.add_input<decl::Float>("Density Max")
      .default_value(10.0f)
      .min(0.0f)
      .make_available(enable_poisson);
  b.add_input<decl::Float>("Density").default_value(10.0f).min(0.0f).field_on_all().make_available(
      enable_random);
  b.add_input<decl::Float>("Density Factor")
      .default_value(1.0f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR)
      .field_on_all()
      .make_available(enable_poisson);
  b.add_input<decl::Int>("Seed");

  b.add_output<decl::Geometry>("Points").propagate_all();
  b.add_output<decl::Vector>("Normal").field_on_all();
  b.add_output<decl::Rotation>("Rotation").field_on_all();
}

static void node_layout(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiItemR(layout, ptr, "distribute_method", UI_ITEM_NONE, "", ICON_NONE);
}

static void node_layout_ex(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiItemR(layout, ptr, "use_legacy_normal", UI_ITEM_NONE, nullptr, ICON_NONE);
}

static void node_point_distribute_points_on_faces_update(bNodeTree *ntree, bNode *node)
{
  bNodeSocket *sock_distance_min = static_cast<bNodeSocket *>(BLI_findlink(&node->inputs, 2));
  bNodeSocket *sock_density_max = static_cast<bNodeSocket *>(sock_distance_min->next);
  bNodeSocket *sock_density = sock_density_max->next;
  bNodeSocket *sock_density_factor = sock_density->next;
  bke::nodeSetSocketAvailability(ntree,
                                 sock_distance_min,
                                 node->custom1 ==
                                     GEO_NODE_POINT_DISTRIBUTE_POINTS_ON_FACES_POISSON);
  bke::nodeSetSocketAvailability(
      ntree, sock_density_max, node->custom1 == GEO_NODE_POINT_DISTRIBUTE_POINTS_ON_FACES_POISSON);
  bke::nodeSetSocketAvailability(
      ntree, sock_density, node->custom1 == GEO_NODE_POINT_DISTRIBUTE_POINTS_ON_FACES_RANDOM);
  bke::nodeSetSocketAvailability(ntree,
                                 sock_density_factor,
                                 node->custom1 ==
                                     GEO_NODE_POINT_DISTRIBUTE_POINTS_ON_FACES_POISSON);
}

/**
 * Use an arbitrary choice of axes for a usable rotation attribute directly out of this node.
 */
static math::Quaternion normal_to_rotation(const float3 normal)
{
  float quat[4];
  vec_to_quat(quat, normal, OB_NEGZ, OB_POSY);
  return math::normalize(math::Quaternion(quat));
}

static void sample_mesh_surface(const Mesh &mesh,
                                const float base_density,
                                const Span<float> density_factors,
                                const int seed,
                                Vector<float3> &r_positions,
                                Vector<float3> &r_bary_coords,
                                Vector<int> &r_tri_indices)
{
  const Span<float3> positions = mesh.vert_positions();
  const Span<int> corner_verts = mesh.corner_verts();
  const Span<int3> corner_tris = mesh.corner_tris();

  for (const int tri_i : corner_tris.index_range()) {
    const int3 &tri = corner_tris[tri_i];
    const int v0_loop = tri[0];
    const int v1_loop = tri[1];
    const int v2_loop = tri[2];
    const float3 &v0_pos = positions[corner_verts[v0_loop]];
    const float3 &v1_pos = positions[corner_verts[v1_loop]];
    const float3 &v2_pos = positions[corner_verts[v2_loop]];

    float corner_tri_density_factor = 1.0f;
    if (!density_factors.is_empty()) {
      const float v0_density_factor = std::max(0.0f, density_factors[v0_loop]);
      const float v1_density_factor = std::max(0.0f, density_factors[v1_loop]);
      const float v2_density_factor = std::max(0.0f, density_factors[v2_loop]);
      corner_tri_density_factor = (v0_density_factor + v1_density_factor + v2_density_factor) /
                                  3.0f;
    }
    const float area = area_tri_v3(v0_pos, v1_pos, v2_pos);

    const int corner_tri_seed = noise::hash(tri_i, seed);
    RandomNumberGenerator corner_tri_rng(corner_tri_seed);

    const int point_amount = corner_tri_rng.round_probabilistic(area * base_density *
                                                                corner_tri_density_factor);

    for (int i = 0; i < point_amount; i++) {
      const float3 bary_coord = corner_tri_rng.get_barycentric_coordinates();
      float3 point_pos;
      interp_v3_v3v3v3(point_pos, v0_pos, v1_pos, v2_pos, bary_coord);
      r_positions.append(point_pos);
      r_bary_coords.append(bary_coord);
      r_tri_indices.append(tri_i);
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
  BLI_SCOPED_DEFER([&]() { BLI_kdtree_3d_free(kdtree); });

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
        [](void *user_data, int index, const float * /*co*/, float /*dist_sq*/) {
          CallbackData &callback_data = *static_cast<CallbackData *>(user_data);
          if (index != callback_data.index) {
            callback_data.elimination_mask[index] = true;
          }
          return true;
        },
        &callback_data);
  }
}

BLI_NOINLINE static void update_elimination_mask_based_on_density_factors(
    const Mesh &mesh,
    const Span<float> density_factors,
    const Span<float3> bary_coords,
    const Span<int> tri_indices,
    const MutableSpan<bool> elimination_mask)
{
  const Span<int3> corner_tris = mesh.corner_tris();
  for (const int i : bary_coords.index_range()) {
    if (elimination_mask[i]) {
      continue;
    }

    const int3 &tri = corner_tris[tri_indices[i]];
    const float3 bary_coord = bary_coords[i];

    const float v0_density_factor = std::max(0.0f, density_factors[tri[0]]);
    const float v1_density_factor = std::max(0.0f, density_factors[tri[1]]);
    const float v2_density_factor = std::max(0.0f, density_factors[tri[2]]);

    const float probability = v0_density_factor * bary_coord.x + v1_density_factor * bary_coord.y +
                              v2_density_factor * bary_coord.z;

    const float hash = noise::hash_float_to_float(bary_coord);
    if (hash > probability) {
      elimination_mask[i] = true;
    }
  }
}

BLI_NOINLINE static void eliminate_points_based_on_mask(const Span<bool> elimination_mask,
                                                        Vector<float3> &positions,
                                                        Vector<float3> &bary_coords,
                                                        Vector<int> &tri_indices)
{
  for (int i = positions.size() - 1; i >= 0; i--) {
    if (elimination_mask[i]) {
      positions.remove_and_reorder(i);
      bary_coords.remove_and_reorder(i);
      tri_indices.remove_and_reorder(i);
    }
  }
}

BLI_NOINLINE static void interpolate_attribute(const Mesh &mesh,
                                               const Span<float3> bary_coords,
                                               const Span<int> tri_indices,
                                               const AttrDomain source_domain,
                                               const GVArray &source_data,
                                               GMutableSpan output_data)
{
  switch (source_domain) {
    case AttrDomain::Point: {
      bke::mesh_surface_sample::sample_point_attribute(mesh.corner_verts(),
                                                       mesh.corner_tris(),
                                                       tri_indices,
                                                       bary_coords,
                                                       source_data,
                                                       IndexMask(output_data.size()),
                                                       output_data);
      break;
    }
    case AttrDomain::Corner: {
      bke::mesh_surface_sample::sample_corner_attribute(mesh.corner_tris(),
                                                        tri_indices,
                                                        bary_coords,
                                                        source_data,
                                                        IndexMask(output_data.size()),
                                                        output_data);
      break;
    }
    case AttrDomain::Face: {
      bke::mesh_surface_sample::sample_face_attribute(mesh.corner_tri_faces(),
                                                      tri_indices,
                                                      source_data,
                                                      IndexMask(output_data.size()),
                                                      output_data);
      break;
    }
    default: {
      /* Not supported currently. */
      return;
    }
  }
}

BLI_NOINLINE static void propagate_existing_attributes(
    const Mesh &mesh,
    const Map<AttributeIDRef, AttributeKind> &attributes,
    PointCloud &points,
    const Span<float3> bary_coords,
    const Span<int> tri_indices)
{
  const AttributeAccessor mesh_attributes = mesh.attributes();
  MutableAttributeAccessor point_attributes = points.attributes_for_write();

  for (MapItem<AttributeIDRef, AttributeKind> entry : attributes.items()) {
    const AttributeIDRef attribute_id = entry.key;
    const eCustomDataType output_data_type = entry.value.data_type;

    GAttributeReader src = mesh_attributes.lookup(attribute_id);
    if (!src) {
      continue;
    }
    if (src.domain == AttrDomain::Edge) {
      continue;
    }

    GSpanAttributeWriter dst = point_attributes.lookup_or_add_for_write_only_span(
        attribute_id, AttrDomain::Point, output_data_type);
    if (!dst) {
      continue;
    }

    interpolate_attribute(mesh, bary_coords, tri_indices, src.domain, src.varray, dst.span);
    dst.finish();
  }
}

namespace {
struct AttributeOutputs {
  AnonymousAttributeIDPtr normal_id;
  AnonymousAttributeIDPtr rotation_id;
};
}  // namespace

static void compute_normal_outputs(const Mesh &mesh,
                                   const Span<float3> bary_coords,
                                   const Span<int> tri_indices,
                                   MutableSpan<float3> r_normals)
{
  switch (mesh.normals_domain()) {
    case bke::MeshNormalDomain::Point: {
      const Span<int> corner_verts = mesh.corner_verts();
      const Span<int3> corner_tris = mesh.corner_tris();
      const Span<float3> vert_normals = mesh.vert_normals();
      threading::parallel_for(bary_coords.index_range(), 512, [&](const IndexRange range) {
        bke::mesh_surface_sample::sample_point_normals(
            corner_verts, corner_tris, tri_indices, bary_coords, vert_normals, range, r_normals);
      });
      break;
    }
    case bke::MeshNormalDomain::Face: {
      const Span<int> tri_faces = mesh.corner_tri_faces();
      VArray<float3> face_normals = VArray<float3>::ForSpan(mesh.face_normals());
      threading::parallel_for(bary_coords.index_range(), 512, [&](const IndexRange range) {
        bke::mesh_surface_sample::sample_face_attribute(
            tri_faces, tri_indices, face_normals, range, r_normals);
      });
      break;
    }
    case bke::MeshNormalDomain::Corner: {
      const Span<int3> corner_tris = mesh.corner_tris();
      const Span<float3> corner_normals = mesh.corner_normals();
      threading::parallel_for(bary_coords.index_range(), 512, [&](const IndexRange range) {
        bke::mesh_surface_sample::sample_corner_normals(
            corner_tris, tri_indices, bary_coords, corner_normals, range, r_normals);
      });
      break;
    }
  }
}

static void compute_legacy_normal_outputs(const Mesh &mesh,
                                          const Span<float3> bary_coords,
                                          const Span<int> tri_indices,
                                          MutableSpan<float3> r_normals)
{
  const Span<float3> positions = mesh.vert_positions();
  const Span<int> corner_verts = mesh.corner_verts();
  const Span<int3> corner_tris = mesh.corner_tris();

  for (const int i : bary_coords.index_range()) {
    const int tri_i = tri_indices[i];
    const int3 &tri = corner_tris[tri_i];

    const int v0_index = corner_verts[tri[0]];
    const int v1_index = corner_verts[tri[1]];
    const int v2_index = corner_verts[tri[2]];
    const float3 v0_pos = positions[v0_index];
    const float3 v1_pos = positions[v1_index];
    const float3 v2_pos = positions[v2_index];

    float3 normal;
    normal_tri_v3(normal, v0_pos, v1_pos, v2_pos);
    r_normals[i] = normal;
  }
}

static void compute_rotation_output(const Span<float3> normals,
                                    MutableSpan<math::Quaternion> r_rotations)
{
  threading::parallel_for(normals.index_range(), 512, [&](const IndexRange range) {
    for (const int i : range) {
      r_rotations[i] = normal_to_rotation(normals[i]);
    }
  });
}

BLI_NOINLINE static void compute_attribute_outputs(const Mesh &mesh,
                                                   PointCloud &points,
                                                   const Span<float3> bary_coords,
                                                   const Span<int> tri_indices,
                                                   const AttributeOutputs &attribute_outputs,
                                                   const bool use_legacy_normal)
{
  MutableAttributeAccessor point_attributes = points.attributes_for_write();

  SpanAttributeWriter<int> ids = point_attributes.lookup_or_add_for_write_only_span<int>(
      "id", AttrDomain::Point);

  SpanAttributeWriter<float3> normals;
  SpanAttributeWriter<math::Quaternion> rotations;

  if (attribute_outputs.normal_id) {
    normals = point_attributes.lookup_or_add_for_write_only_span<float3>(
        attribute_outputs.normal_id.get(), AttrDomain::Point);
  }
  if (attribute_outputs.rotation_id) {
    rotations = point_attributes.lookup_or_add_for_write_only_span<math::Quaternion>(
        attribute_outputs.rotation_id.get(), AttrDomain::Point);
  }

  threading::parallel_for(bary_coords.index_range(), 1024, [&](const IndexRange range) {
    for (const int i : range) {
      const int tri_i = tri_indices[i];
      const float3 &bary_coord = bary_coords[i];
      ids.span[i] = noise::hash(noise::hash_float(bary_coord), tri_i);
    }
  });

  if (normals) {
    if (use_legacy_normal) {
      compute_legacy_normal_outputs(mesh, bary_coords, tri_indices, normals.span);
    }
    else {
      compute_normal_outputs(mesh, bary_coords, tri_indices, normals.span);
    }

    if (rotations) {
      compute_rotation_output(normals.span, rotations.span);
    }
  }

  ids.finish();
  normals.finish();
  rotations.finish();
}

static Array<float> calc_full_density_factors_with_selection(const Mesh &mesh,
                                                             const Field<float> &density_field,
                                                             const Field<bool> &selection_field)
{
  const AttrDomain domain = AttrDomain::Corner;
  const int domain_size = mesh.attributes().domain_size(domain);
  Array<float> densities(domain_size, 0.0f);

  const bke::MeshFieldContext field_context{mesh, domain};
  fn::FieldEvaluator evaluator{field_context, domain_size};
  evaluator.set_selection(selection_field);
  evaluator.add_with_destination(density_field, densities.as_mutable_span());
  evaluator.evaluate();
  return densities;
}

static void distribute_points_random(const Mesh &mesh,
                                     const Field<float> &density_field,
                                     const Field<bool> &selection_field,
                                     const int seed,
                                     Vector<float3> &positions,
                                     Vector<float3> &bary_coords,
                                     Vector<int> &tri_indices)
{
  const Array<float> densities = calc_full_density_factors_with_selection(
      mesh, density_field, selection_field);
  sample_mesh_surface(mesh, 1.0f, densities, seed, positions, bary_coords, tri_indices);
}

static void distribute_points_poisson_disk(const Mesh &mesh,
                                           const float minimum_distance,
                                           const float max_density,
                                           const Field<float> &density_factor_field,
                                           const Field<bool> &selection_field,
                                           const int seed,
                                           Vector<float3> &positions,
                                           Vector<float3> &bary_coords,
                                           Vector<int> &tri_indices)
{
  sample_mesh_surface(mesh, max_density, {}, seed, positions, bary_coords, tri_indices);

  Array<bool> elimination_mask(positions.size(), false);
  update_elimination_mask_for_close_points(positions, minimum_distance, elimination_mask);

  const Array<float> density_factors = calc_full_density_factors_with_selection(
      mesh, density_factor_field, selection_field);

  update_elimination_mask_based_on_density_factors(
      mesh, density_factors, bary_coords, tri_indices, elimination_mask.as_mutable_span());

  eliminate_points_based_on_mask(elimination_mask.as_span(), positions, bary_coords, tri_indices);
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

  const Mesh &mesh = *geometry_set.get_mesh();

  Vector<float3> positions;
  Vector<float3> bary_coords;
  Vector<int> tri_indices;

  switch (method) {
    case GEO_NODE_POINT_DISTRIBUTE_POINTS_ON_FACES_RANDOM: {
      const Field<float> density_field = params.get_input<Field<float>>("Density");
      distribute_points_random(
          mesh, density_field, selection_field, seed, positions, bary_coords, tri_indices);
      break;
    }
    case GEO_NODE_POINT_DISTRIBUTE_POINTS_ON_FACES_POISSON: {
      const float minimum_distance = params.get_input<float>("Distance Min");
      const float density_max = params.get_input<float>("Density Max");
      const Field<float> density_factors_field = params.get_input<Field<float>>("Density Factor");
      distribute_points_poisson_disk(mesh,
                                     minimum_distance,
                                     density_max,
                                     density_factors_field,
                                     selection_field,
                                     seed,
                                     positions,
                                     bary_coords,
                                     tri_indices);
      break;
    }
  }

  if (positions.is_empty()) {
    return;
  }

  PointCloud *pointcloud = BKE_pointcloud_new_nomain(positions.size());
  bke::MutableAttributeAccessor point_attributes = pointcloud->attributes_for_write();
  bke::SpanAttributeWriter<float> point_radii =
      point_attributes.lookup_or_add_for_write_only_span<float>("radius", AttrDomain::Point);
  pointcloud->positions_for_write().copy_from(positions);
  point_radii.span.fill(0.05f);
  point_radii.finish();

  geometry_set.replace_pointcloud(pointcloud);

  Map<AttributeIDRef, AttributeKind> attributes;
  geometry_set.gather_attributes_for_propagation({GeometryComponent::Type::Mesh},
                                                 GeometryComponent::Type::PointCloud,
                                                 false,
                                                 params.get_output_propagation_info("Points"),
                                                 attributes);

  /* Position is set separately. */
  attributes.remove("position");

  propagate_existing_attributes(mesh, attributes, *pointcloud, bary_coords, tri_indices);

  const bool use_legacy_normal = params.node().custom2 != 0;
  compute_attribute_outputs(
      mesh, *pointcloud, bary_coords, tri_indices, attribute_outputs, use_legacy_normal);

  geometry::debug_randomize_point_order(pointcloud);
}

static void node_geo_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Mesh");

  const GeometryNodeDistributePointsOnFacesMode method = GeometryNodeDistributePointsOnFacesMode(
      params.node().custom1);

  const int seed = params.get_input<int>("Seed") * 5383843;
  const Field<bool> selection_field = params.extract_input<Field<bool>>("Selection");

  AttributeOutputs attribute_outputs;
  attribute_outputs.rotation_id = params.get_output_anonymous_attribute_id_if_needed("Rotation");
  attribute_outputs.normal_id = params.get_output_anonymous_attribute_id_if_needed(
      "Normal", bool(attribute_outputs.rotation_id));

  lazy_threading::send_hint();

  geometry_set.modify_geometry_sets([&](GeometrySet &geometry_set) {
    point_distribution_calculate(
        geometry_set, selection_field, method, seed, attribute_outputs, params);
    /* Keep instances because the original geometry set may contain instances that are processed as
     * well. */
    geometry_set.keep_only_during_modify({GeometryComponent::Type::PointCloud});
  });

  params.set_output("Points", std::move(geometry_set));
}

static void node_register()
{
  static blender::bke::bNodeType ntype;

  geo_node_type_base(&ntype,
                     GEO_NODE_DISTRIBUTE_POINTS_ON_FACES,
                     "Distribute Points on Faces",
                     NODE_CLASS_GEOMETRY);
  ntype.updatefunc = node_point_distribute_points_on_faces_update;
  blender::bke::node_type_size(&ntype, 170, 100, 320);
  ntype.declare = node_declare;
  ntype.geometry_node_execute = node_geo_exec;
  ntype.draw_buttons = node_layout;
  ntype.draw_buttons_ex = node_layout_ex;
  blender::bke::nodeRegisterType(&ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_distribute_points_on_faces_cc
