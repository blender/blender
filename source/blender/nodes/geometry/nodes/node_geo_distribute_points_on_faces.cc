/* SPDX-License-Identifier: GPL-2.0-or-later */

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
#include "BKE_mesh.hh"
#include "BKE_mesh_runtime.h"
#include "BKE_mesh_sample.hh"
#include "BKE_pointcloud.h"

#include "UI_interface.h"
#include "UI_resources.h"

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

  b.add_input<decl::Geometry>("Mesh").supported_type(GEO_COMPONENT_TYPE_MESH);
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
  b.add_output<decl::Vector>("Rotation").subtype(PROP_EULER).field_on_all();
}

static void node_layout(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiItemR(layout, ptr, "distribute_method", 0, "", ICON_NONE);
}

static void node_layout_ex(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiItemR(layout, ptr, "use_legacy_normal", 0, nullptr, ICON_NONE);
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
  const Span<float3> positions = mesh.vert_positions();
  const Span<int> corner_verts = mesh.corner_verts();
  const Span<MLoopTri> looptris = mesh.looptris();

  for (const int looptri_index : looptris.index_range()) {
    const MLoopTri &looptri = looptris[looptri_index];
    const int v0_loop = looptri.tri[0];
    const int v1_loop = looptri.tri[1];
    const int v2_loop = looptri.tri[2];
    const float3 &v0_pos = positions[corner_verts[v0_loop]];
    const float3 &v1_pos = positions[corner_verts[v1_loop]];
    const float3 &v2_pos = positions[corner_verts[v2_loop]];

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

    const int point_amount = looptri_rng.round_probabilistic(area * base_density *
                                                             looptri_density_factor);

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
    const Span<int> looptri_indices,
    const MutableSpan<bool> elimination_mask)
{
  const Span<MLoopTri> looptris = mesh.looptris();
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
                                               const eAttrDomain source_domain,
                                               const GVArray &source_data,
                                               GMutableSpan output_data)
{
  switch (source_domain) {
    case ATTR_DOMAIN_POINT: {
      bke::mesh_surface_sample::sample_point_attribute(mesh.corner_verts(),
                                                       mesh.looptris(),
                                                       looptri_indices,
                                                       bary_coords,
                                                       source_data,
                                                       IndexMask(output_data.size()),
                                                       output_data);
      break;
    }
    case ATTR_DOMAIN_CORNER: {
      bke::mesh_surface_sample::sample_corner_attribute(mesh.looptris(),
                                                        looptri_indices,
                                                        bary_coords,
                                                        source_data,
                                                        IndexMask(output_data.size()),
                                                        output_data);
      break;
    }
    case ATTR_DOMAIN_FACE: {
      bke::mesh_surface_sample::sample_face_attribute(mesh.looptri_polys(),
                                                      looptri_indices,
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
    const Span<int> looptri_indices)
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
    if (src.domain == ATTR_DOMAIN_EDGE) {
      continue;
    }

    GSpanAttributeWriter dst = point_attributes.lookup_or_add_for_write_only_span(
        attribute_id, ATTR_DOMAIN_POINT, output_data_type);
    if (!dst) {
      continue;
    }

    interpolate_attribute(mesh, bary_coords, looptri_indices, src.domain, src.varray, dst.span);
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
                                   const Span<int> looptri_indices,
                                   MutableSpan<float3> r_normals)
{
  Array<float3> corner_normals(mesh.totloop);
  BKE_mesh_calc_normals_split_ex(
      const_cast<Mesh *>(&mesh), nullptr, reinterpret_cast<float(*)[3]>(corner_normals.data()));

  const Span<MLoopTri> looptris = mesh.looptris();
  threading::parallel_for(bary_coords.index_range(), 512, [&](const IndexRange range) {
    bke::mesh_surface_sample::sample_corner_normals(
        looptris, looptri_indices, bary_coords, corner_normals, range, r_normals);
  });
}

static void compute_legacy_normal_outputs(const Mesh &mesh,
                                          const Span<float3> bary_coords,
                                          const Span<int> looptri_indices,
                                          MutableSpan<float3> r_normals)
{
  const Span<float3> positions = mesh.vert_positions();
  const Span<int> corner_verts = mesh.corner_verts();
  const Span<MLoopTri> looptris = mesh.looptris();

  for (const int i : bary_coords.index_range()) {
    const int looptri_index = looptri_indices[i];
    const MLoopTri &looptri = looptris[looptri_index];

    const int v0_index = corner_verts[looptri.tri[0]];
    const int v1_index = corner_verts[looptri.tri[1]];
    const int v2_index = corner_verts[looptri.tri[2]];
    const float3 v0_pos = positions[v0_index];
    const float3 v1_pos = positions[v1_index];
    const float3 v2_pos = positions[v2_index];

    float3 normal;
    normal_tri_v3(normal, v0_pos, v1_pos, v2_pos);
    r_normals[i] = normal;
  }
}

static void compute_rotation_output(const Span<float3> normals, MutableSpan<float3> r_rotations)
{
  threading::parallel_for(normals.index_range(), 256, [&](const IndexRange range) {
    for (const int i : range) {
      r_rotations[i] = normal_to_euler_rotation(normals[i]);
    }
  });
}

BLI_NOINLINE static void compute_attribute_outputs(const Mesh &mesh,
                                                   PointCloud &points,
                                                   const Span<float3> bary_coords,
                                                   const Span<int> looptri_indices,
                                                   const AttributeOutputs &attribute_outputs,
                                                   const bool use_legacy_normal)
{
  MutableAttributeAccessor point_attributes = points.attributes_for_write();

  SpanAttributeWriter<int> ids = point_attributes.lookup_or_add_for_write_only_span<int>(
      "id", ATTR_DOMAIN_POINT);

  SpanAttributeWriter<float3> normals;
  SpanAttributeWriter<float3> rotations;

  if (attribute_outputs.normal_id) {
    normals = point_attributes.lookup_or_add_for_write_only_span<float3>(
        attribute_outputs.normal_id.get(), ATTR_DOMAIN_POINT);
  }
  if (attribute_outputs.rotation_id) {
    rotations = point_attributes.lookup_or_add_for_write_only_span<float3>(
        attribute_outputs.rotation_id.get(), ATTR_DOMAIN_POINT);
  }

  threading::parallel_for(bary_coords.index_range(), 1024, [&](const IndexRange range) {
    for (const int i : range) {
      const int looptri_index = looptri_indices[i];
      const float3 &bary_coord = bary_coords[i];
      ids.span[i] = noise::hash(noise::hash_float(bary_coord), looptri_index);
    }
  });

  if (normals) {
    if (use_legacy_normal) {
      compute_legacy_normal_outputs(mesh, bary_coords, looptri_indices, normals.span);
    }
    else {
      compute_normal_outputs(mesh, bary_coords, looptri_indices, normals.span);
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
  const eAttrDomain domain = ATTR_DOMAIN_CORNER;
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
                                     Vector<int> &looptri_indices)
{
  const Array<float> densities = calc_full_density_factors_with_selection(
      mesh, density_field, selection_field);
  sample_mesh_surface(mesh, 1.0f, densities, seed, positions, bary_coords, looptri_indices);
}

static void distribute_points_poisson_disk(const Mesh &mesh,
                                           const float minimum_distance,
                                           const float max_density,
                                           const Field<float> &density_factor_field,
                                           const Field<bool> &selection_field,
                                           const int seed,
                                           Vector<float3> &positions,
                                           Vector<float3> &bary_coords,
                                           Vector<int> &looptri_indices)
{
  sample_mesh_surface(mesh, max_density, {}, seed, positions, bary_coords, looptri_indices);

  Array<bool> elimination_mask(positions.size(), false);
  update_elimination_mask_for_close_points(positions, minimum_distance, elimination_mask);

  const Array<float> density_factors = calc_full_density_factors_with_selection(
      mesh, density_factor_field, selection_field);

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

  const Mesh &mesh = *geometry_set.get_mesh_for_read();

  Vector<float3> positions;
  Vector<float3> bary_coords;
  Vector<int> looptri_indices;

  switch (method) {
    case GEO_NODE_POINT_DISTRIBUTE_POINTS_ON_FACES_RANDOM: {
      const Field<float> density_field = params.get_input<Field<float>>("Density");
      distribute_points_random(
          mesh, density_field, selection_field, seed, positions, bary_coords, looptri_indices);
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
                                     looptri_indices);
      break;
    }
  }

  if (positions.is_empty()) {
    return;
  }

  PointCloud *pointcloud = BKE_pointcloud_new_nomain(positions.size());
  bke::MutableAttributeAccessor point_attributes = pointcloud->attributes_for_write();
  bke::SpanAttributeWriter<float> point_radii =
      point_attributes.lookup_or_add_for_write_only_span<float>("radius", ATTR_DOMAIN_POINT);
  pointcloud->positions_for_write().copy_from(positions);
  point_radii.span.fill(0.05f);
  point_radii.finish();

  geometry_set.replace_pointcloud(pointcloud);

  Map<AttributeIDRef, AttributeKind> attributes;
  geometry_set.gather_attributes_for_propagation({GEO_COMPONENT_TYPE_MESH},
                                                 GEO_COMPONENT_TYPE_POINT_CLOUD,
                                                 false,
                                                 params.get_output_propagation_info("Points"),
                                                 attributes);

  /* Position is set separately. */
  attributes.remove("position");

  propagate_existing_attributes(mesh, attributes, *pointcloud, bary_coords, looptri_indices);

  const bool use_legacy_normal = params.node().custom2 != 0;
  compute_attribute_outputs(
      mesh, *pointcloud, bary_coords, looptri_indices, attribute_outputs, use_legacy_normal);
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
    geometry_set.keep_only_during_modify({GEO_COMPONENT_TYPE_POINT_CLOUD});
  });

  params.set_output("Points", std::move(geometry_set));
}

}  // namespace blender::nodes::node_geo_distribute_points_on_faces_cc

void register_node_type_geo_distribute_points_on_faces()
{
  namespace file_ns = blender::nodes::node_geo_distribute_points_on_faces_cc;

  static bNodeType ntype;

  geo_node_type_base(&ntype,
                     GEO_NODE_DISTRIBUTE_POINTS_ON_FACES,
                     "Distribute Points on Faces",
                     NODE_CLASS_GEOMETRY);
  ntype.updatefunc = file_ns::node_point_distribute_points_on_faces_update;
  blender::bke::node_type_size(&ntype, 170, 100, 320);
  ntype.declare = file_ns::node_declare;
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  ntype.draw_buttons = file_ns::node_layout;
  ntype.draw_buttons_ex = file_ns::node_layout_ex;
  nodeRegisterType(&ntype);
}
