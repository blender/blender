/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_kdtree.hh"
#include "BLI_math_geom.h"
#include "BLI_math_quaternion.hh"
#include "BLI_math_rotation.h"
#include "BLI_noise.hh"
#include "BLI_rand.hh"
#include "BLI_task.hh"

#include "DNA_pointcloud_types.h"

#include "BKE_attribute_math.hh"
#include "BKE_mesh.hh"
#include "BKE_mesh_sample.hh"
#include "BKE_pointcloud.hh"

#include "FN_multi_function_registry.hh"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "GEO_foreach_geometry.hh"
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

  b.add_input<decl::Geometry>("Mesh"_ustr)
      .supported_type(GeometryComponent::Type::Mesh)
      .description("Mesh on whose faces to distribute points on");
  b.add_input<decl::Bool>("Selection"_ustr).default_value(true).hide_value().field_on_all();
  auto &distance_min = b.add_input<decl::Float>("Distance Min"_ustr)
                           .min(0.0f)
                           .subtype(PROP_DISTANCE)
                           .make_available(enable_poisson)
                           .available(false);
  auto &density_max = b.add_input<decl::Float>("Density Max"_ustr)
                          .default_value(10.0f)
                          .min(0.0f)
                          .make_available(enable_poisson)
                          .available(false);
  auto &density = b.add_input<decl::Float>("Density"_ustr)
                      .default_value(10.0f)
                      .min(0.0f)
                      .field_on_all()
                      .make_available(enable_random)
                      .available(false);
  auto &density_factor = b.add_input<decl::Float>("Density Factor"_ustr)
                             .default_value(1.0f)
                             .min(0.0f)
                             .max(1.0f)
                             .subtype(PROP_FACTOR)
                             .field_on_all()
                             .make_available(enable_poisson)
                             .available(false);
  b.add_input<decl::Int>("Seed"_ustr);

  b.add_output<decl::Geometry>("Points"_ustr).propagate_all();
  b.add_output<decl::Vector>("Normal"_ustr).field_on_all();
  b.add_output<decl::Rotation>("Rotation"_ustr).field_on_all();

  const bNode *node = b.node_or_null();
  if (node != nullptr) {
    switch (node->custom1) {
      case GEO_NODE_POINT_DISTRIBUTE_POINTS_ON_FACES_POISSON:
        distance_min.available(true);
        density_max.available(true);
        density_factor.available(true);
        break;
      case GEO_NODE_POINT_DISTRIBUTE_POINTS_ON_FACES_RANDOM:
        density.available(true);
        break;
    }
  }
}

static void node_layout(ui::Layout &layout, bContext * /*C*/, PointerRNA *ptr)
{
  layout.prop(ptr, "distribute_method", UI_ITEM_NONE, "", ICON_NONE);
}

static void node_layout_ex(ui::Layout &layout, bContext * /*C*/, PointerRNA *ptr)
{
  layout.prop(ptr, "use_legacy_normal", UI_ITEM_NONE, std::nullopt, ICON_NONE);
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

static OffsetIndices<int> calc_tri_point_offsets(const Mesh &mesh,
                                                 const Span<float> densities,
                                                 const int seed,
                                                 Array<int> &r_count_data)
{
  const Span<float3> positions = mesh.vert_positions();
  const Span<int> corner_verts = mesh.corner_verts();
  const Span<int3> corner_tris = mesh.corner_tris();

  r_count_data = Array<int>(corner_tris.size() + 1);
  threading::parallel_for(corner_tris.index_range(), 1024, [&](const IndexRange range) {
    for (const int64_t tri_i : range) {
      const int3 &tri = corner_tris[tri_i];
      const float density = (densities[tri[0]] + densities[tri[1]] + densities[tri[2]]) / 3.0f;
      const float area = area_tri_v3(positions[corner_verts[tri[0]]],
                                     positions[corner_verts[tri[1]]],
                                     positions[corner_verts[tri[2]]]);
      const int corner_tri_seed = noise::hash(tri_i, seed);
      RandomNumberGenerator corner_tri_rng(corner_tri_seed);
      r_count_data[tri_i] = corner_tri_rng.round_probabilistic(area * density);
    }
  });

  return offset_indices::accumulate_counts_to_offsets(r_count_data);
}

static OffsetIndices<int> calc_tri_point_offsets(const Mesh &mesh,
                                                 const float density,
                                                 const int seed,
                                                 Array<int> &r_count_data)
{
  const Span<float3> positions = mesh.vert_positions();
  const Span<int> corner_verts = mesh.corner_verts();
  const Span<int3> corner_tris = mesh.corner_tris();

  r_count_data = Array<int>(corner_tris.size() + 1);
  threading::parallel_for(corner_tris.index_range(), 1024, [&](const IndexRange range) {
    for (const int64_t tri_i : range) {
      const int3 &tri = corner_tris[tri_i];
      const float area = area_tri_v3(positions[corner_verts[tri[0]]],
                                     positions[corner_verts[tri[1]]],
                                     positions[corner_verts[tri[2]]]);
      const int corner_tri_seed = noise::hash(tri_i, seed);
      RandomNumberGenerator corner_tri_rng(corner_tri_seed);
      r_count_data[tri_i] = corner_tri_rng.round_probabilistic(area * density);
    }
  });

  return offset_indices::accumulate_counts_to_offsets(r_count_data);
}

static void sample_bary_coords(const Mesh &mesh,
                               const int seed,
                               const OffsetIndices<int> points_by_tri,
                               Vector<float3> &r_positions,
                               Vector<float3> &r_bary_coords,
                               Vector<int> &r_tri_indices)
{
  const Span<float3> positions = mesh.vert_positions();
  const Span<int> corner_verts = mesh.corner_verts();
  const Span<int3> corner_tris = mesh.corner_tris();

  r_positions.resize(points_by_tri.total_size());
  r_bary_coords.resize(points_by_tri.total_size());
  r_tri_indices.resize(points_by_tri.total_size());

  threading::parallel_for(
      corner_tris.index_range(),
      4096,
      [&](const IndexRange range) {
        for (const int64_t tri_i : range) {
          const int3 &tri = corner_tris[tri_i];
          const float3 &v0_pos = positions[corner_verts[tri[0]]];
          const float3 &v1_pos = positions[corner_verts[tri[1]]];
          const float3 &v2_pos = positions[corner_verts[tri[2]]];

          const int corner_tri_seed = noise::hash(tri_i, seed);
          RandomNumberGenerator corner_tri_rng(corner_tri_seed);

          /* Retain legacy behavior. */
          corner_tri_rng.skip(1);

          for (const int i : points_by_tri[tri_i]) {
            const float3 bary_coord = corner_tri_rng.get_barycentric_coordinates();
            r_positions[i] = bke::attribute_math::mix3(bary_coord, v0_pos, v1_pos, v2_pos);
            r_bary_coords[i] = bary_coord;
            r_tri_indices[i] = tri_i;
          }
        }
      },
      threading::accumulated_task_sizes(
          [&](const IndexRange range) { return points_by_tri[range].size(); }));
}

BLI_NOINLINE static KDTree<float3> *build_kdtree(Span<float3> positions)
{
  KDTree<float3> *kdtree = kdtree_new<float3>(positions.size());

  int i_point = 0;
  for (const float3 position : positions) {
    kdtree_insert<float3>(kdtree, i_point, position);
    i_point++;
  }

  kdtree_balance<float3>(kdtree);
  return kdtree;
}

BLI_NOINLINE static void update_elimination_mask_for_close_points(
    Span<float3> positions, const float minimum_distance, MutableSpan<bool> elimination_mask)
{
  if (minimum_distance <= 0.0f) {
    return;
  }

  KDTree<float3> *kdtree = build_kdtree(positions);
  BLI_SCOPED_DEFER([&]() { kdtree_free<float3>(kdtree); });

  for (const int i : positions.index_range()) {
    if (elimination_mask[i]) {
      continue;
    }

    kdtree_range_search_cb<float3>(kdtree,
                                   positions[i],
                                   minimum_distance,
                                   [&](int index, const float3 & /*co*/, float /*dist_sq*/) {
                                     if (index != i) {
                                       elimination_mask[index] = true;
                                     }
                                     return true;
                                   });
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

    const float v0_density_factor = density_factors[tri[0]];
    const float v1_density_factor = density_factors[tri[1]];
    const float v2_density_factor = density_factors[tri[2]];

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

BLI_NOINLINE static void propagate_attributes(const Mesh &mesh,
                                              const bke::AttributeFilter &filter,
                                              PointCloud &points,
                                              const Span<float3> bary_coords,
                                              const Span<int> tri_indices)
{
  const AttributeAccessor mesh_attributes = mesh.attributes();
  MutableAttributeAccessor point_attributes = points.attributes_for_write();

  mesh_attributes.foreach_attribute([&](const bke::AttributeIter &iter) {
    if (iter.domain == AttrDomain::Edge) {
      return;
    }
    const StringRef name = iter.name;
    if (iter.is_builtin && !point_attributes.is_builtin(name)) {
      return;
    }
    if (name == "position") {
      return;
    }
    if (filter.allow_skip(name)) {
      return;
    }
    GAttributeReader src = iter.get();
    if (!src) {
      return;
    }
    const CommonVArrayInfo info = src.varray.common_info();
    if (info.type == CommonVArrayInfo::Type::Single) {
      const bke::AttributeInitValue init(GPointer(src.varray.type(), info.data));
      if (point_attributes.add(iter.name, AttrDomain::Point, iter.data_type, init)) {
        return;
      }
    }
    GSpanAttributeWriter dst = point_attributes.lookup_or_add_for_write_only_span(
        name, AttrDomain::Point, iter.data_type);
    if (!dst) {
      return;
    }
    interpolate_attribute(mesh, bary_coords, tri_indices, src.domain, src.varray, dst.span);
    dst.finish();
  });
}

namespace {
struct AttributeOutputs {
  std::optional<std::string> normal_id;
  std::optional<std::string> rotation_id;
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
      VArray<float3> face_normals = VArray<float3>::from_span(mesh.face_normals());
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
        *attribute_outputs.normal_id, AttrDomain::Point);
  }
  if (attribute_outputs.rotation_id) {
    rotations = point_attributes.lookup_or_add_for_write_only_span<math::Quaternion>(
        *attribute_outputs.rotation_id, AttrDomain::Point);
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

static PointCloud *create_points_random(const Mesh &mesh,
                                        const Field<bool> &selection_field,
                                        const Field<float> &density_field,
                                        const int seed,
                                        const AttributeOutputs &attribute_outputs,
                                        const bke::AttributeFilter &attribute_filter,
                                        const bool use_legacy_normal)
{
  Array<int> count_data;
  OffsetIndices<int> points_by_tri;
  if (selection_field.depends_on_input() || density_field.depends_on_input()) {
    const Array<float> densities = calc_full_density_factors_with_selection(
        mesh, density_field, selection_field);
    points_by_tri = calc_tri_point_offsets(mesh, densities, seed, count_data);
  }
  else {
    const float density = fn::evaluate_constant_field<float>(density_field);
    points_by_tri = calc_tri_point_offsets(mesh, density, seed, count_data);
  }
  if (points_by_tri.total_size() == 0) {
    return nullptr;
  }

  Vector<float3> positions;
  Vector<float3> bary_coords;
  Vector<int> tri_indices;
  sample_bary_coords(mesh, seed, points_by_tri, positions, bary_coords, tri_indices);

  PointCloud *pointcloud = bke::pointcloud_new_no_attributes(positions.size());
  bke::MutableAttributeAccessor point_attributes = pointcloud->attributes_for_write();
  VectorData<float3, GuardedAllocator> positions_data = positions.release();
  const auto *attr_data = implicit_sharing::info_for_mem_free(positions_data.data);
  point_attributes.add<float3>("position",
                               bke::AttrDomain::Point,
                               bke::AttributeInitShared(positions_data.data, *attr_data));
  attr_data->remove_user_and_delete_if_last();
  point_attributes.add<float>("radius", bke::AttrDomain::Point, bke::AttributeInitValue(0.05f));

  propagate_attributes(mesh, attribute_filter, *pointcloud, bary_coords, tri_indices);

  compute_attribute_outputs(
      mesh, *pointcloud, bary_coords, tri_indices, attribute_outputs, use_legacy_normal);

  geometry::debug_randomize_point_order(pointcloud);

  return pointcloud;
}

static PointCloud *create_points_poisson_disk(const Mesh &mesh,
                                              const Field<bool> &selection_field,
                                              const Field<float> &density_factor_field,
                                              const float minimum_distance,
                                              const float density_max,
                                              const int seed,
                                              const AttributeOutputs &attribute_outputs,
                                              const bke::AttributeFilter &attribute_filter,
                                              const bool use_legacy_normal)
{
  Array<int> count_data;
  const OffsetIndices<int> points_by_tri = calc_tri_point_offsets(
      mesh, density_max, seed, count_data);
  if (points_by_tri.total_size() == 0) {
    return nullptr;
  }

  Vector<float3> positions;
  Vector<float3> bary_coords;
  Vector<int> tri_indices;
  sample_bary_coords(mesh, seed, points_by_tri, positions, bary_coords, tri_indices);

  Array<bool> elimination_mask(positions.size(), false);
  update_elimination_mask_for_close_points(positions, minimum_distance, elimination_mask);

  const Array<float> density_factors = calc_full_density_factors_with_selection(
      mesh, density_factor_field, selection_field);

  update_elimination_mask_based_on_density_factors(
      mesh, density_factors, bary_coords, tri_indices, elimination_mask.as_mutable_span());

  eliminate_points_based_on_mask(elimination_mask.as_span(), positions, bary_coords, tri_indices);

  if (positions.is_empty()) {
    return nullptr;
  }

  PointCloud *pointcloud = bke::pointcloud_new_no_attributes(positions.size());
  bke::MutableAttributeAccessor point_attributes = pointcloud->attributes_for_write();
  pointcloud->positions_for_write().copy_from(positions);
  point_attributes.add<float>("radius", bke::AttrDomain::Point, bke::AttributeInitValue(0.05f));

  propagate_attributes(mesh, attribute_filter, *pointcloud, bary_coords, tri_indices);

  compute_attribute_outputs(
      mesh, *pointcloud, bary_coords, tri_indices, attribute_outputs, use_legacy_normal);

  geometry::debug_randomize_point_order(pointcloud);

  return pointcloud;
}

static Field<float> extract_non_negative_density(GeoNodeExecParams &params, const UString input)
{
  const static mf::MultiFunction &max_fn = fn::multi_function::registry::lookup(
      "max(float, float)"_ustr);
  return Field<float>(FieldOperation::from(
      max_fn, {params.extract_input<Field<float>>(input), fn::Field<float>(0.0f)}));
}

static void node_geo_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Mesh"_ustr);

  const Field<bool> selection = params.extract_input<Field<bool>>("Selection"_ustr);
  const int seed = params.extract_input<int>("Seed"_ustr) * 5383843;
  const NodeAttributeFilter attribute_filter = params.get_attribute_filter("Points"_ustr);
  const bool use_legacy_normal = params.node().custom2 != 0;

  AttributeOutputs attribute_outputs;
  attribute_outputs.rotation_id = params.get_output_anonymous_attribute_id_if_needed(
      "Rotation"_ustr);
  attribute_outputs.normal_id = params.get_output_anonymous_attribute_id_if_needed(
      "Normal"_ustr, bool(attribute_outputs.rotation_id));

  lazy_threading::send_hint();

  switch (GeometryNodeDistributePointsOnFacesMode(params.node().custom1)) {
    case GEO_NODE_POINT_DISTRIBUTE_POINTS_ON_FACES_RANDOM: {
      const Field<float> density = extract_non_negative_density(params, "Density"_ustr);
      geometry::foreach_real_geometry(geometry_set, [&](GeometrySet &geometry_set) {
        if (const Mesh *mesh = geometry_set.get_mesh()) {
          PointCloud *pointcloud = create_points_random(*mesh,
                                                        selection,
                                                        density,
                                                        seed,
                                                        attribute_outputs,
                                                        attribute_filter,
                                                        use_legacy_normal);
          geometry_set.replace_pointcloud(pointcloud);
        }
        geometry_set.keep_only(
            {GeometryComponent::Type::PointCloud, GeometryComponent::Type::Edit});
      });
      break;
    }
    case GEO_NODE_POINT_DISTRIBUTE_POINTS_ON_FACES_POISSON: {
      const Field<float> factors = extract_non_negative_density(params, "Density Factor"_ustr);
      const float minimum_distance = params.extract_input<float>("Distance Min"_ustr);
      const float density_max = params.extract_input<float>("Density Max"_ustr);
      geometry::foreach_real_geometry(geometry_set, [&](GeometrySet &geometry_set) {
        if (const Mesh *mesh = geometry_set.get_mesh()) {
          PointCloud *pointcloud = create_points_poisson_disk(*mesh,
                                                              selection,
                                                              factors,
                                                              minimum_distance,
                                                              density_max,
                                                              seed,
                                                              attribute_outputs,
                                                              attribute_filter,
                                                              use_legacy_normal);
          geometry_set.replace_pointcloud(pointcloud);
          geometry_set.keep_only(
              {GeometryComponent::Type::PointCloud, GeometryComponent::Type::Edit});
        }
      });
      break;
    }
  }

  params.set_output("Points"_ustr, std::move(geometry_set));
}

static void node_register()
{
  static bke::bNodeType ntype;

  geo_node_type_base(
      &ntype, "GeometryNodeDistributePointsOnFaces"_ustr, GEO_NODE_DISTRIBUTE_POINTS_ON_FACES);
  ntype.ui_name = "Distribute Points on Faces";
  ntype.ui_description = "Generate points spread out on the surface of a mesh";
  ntype.enum_name_legacy = "DISTRIBUTE_POINTS_ON_FACES";
  ntype.nclass = NODE_CLASS_GEOMETRY;
  bke::node_type_size(ntype, 170, 100, 320);
  ntype.declare = node_declare;
  ntype.geometry_node_execute = node_geo_exec;
  ntype.draw_buttons = node_layout;
  ntype.draw_buttons_ex = node_layout_ex;
  bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_distribute_points_on_faces_cc
