/* SPDX-FileCopyrightText: 2020 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edsculpt
 */

#include "BLI_array.hh"
#include "BLI_index_range.hh"
#include "BLI_math_base.h"
#include "BLI_math_base.hh"
#include "BLI_math_vector.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_set.hh"
#include "BLI_vector.hh"

#include "DNA_brush_types.h"
#include "DNA_mesh_types.h"

#include "BKE_colortools.hh"
#include "BKE_paint.hh"
#include "BKE_paint_bvh.hh"
#include "BKE_subdiv_ccg.hh"

#include "mesh_brush_common.hh"
#include "paint_intern.hh"
#include "sculpt_automask.hh"
#include "sculpt_boundary.hh"
#include "sculpt_dyntopo.hh"
#include "sculpt_face_set.hh"
#include "sculpt_filter.hh"
#include "sculpt_flood_fill.hh"
#include "sculpt_intern.hh"
#include "sculpt_islands.hh"
#include "sculpt_undo.hh"

#include "bmesh.hh"

#include <cmath>

namespace blender::ed::sculpt_paint::auto_mask {
const Cache *active_cache_get(const SculptSession &ss)
{
  if (ss.cache) {
    return ss.cache->automasking.get();
  }
  if (ss.filter_cache) {
    return ss.filter_cache->automasking.get();
  }
  return nullptr;
}

bool mode_enabled(const Sculpt &sd, const Brush *br, const eAutomasking_flag mode)
{
  int automasking = sd.automasking_flags;

  if (br) {
    automasking |= br->automasking_flags;
  }

  return (eAutomasking_flag)automasking & mode;
}

bool is_enabled(const Sculpt &sd, const Object &object, const Brush *br)
{
  if (object.sculpt && br && dyntopo::stroke_is_dyntopo(object, *br)) {
    return false;
  }
  if (mode_enabled(sd, br, BRUSH_AUTOMASKING_TOPOLOGY)) {
    return true;
  }
  if (mode_enabled(sd, br, BRUSH_AUTOMASKING_FACE_SETS)) {
    return true;
  }
  if (mode_enabled(sd, br, BRUSH_AUTOMASKING_BOUNDARY_EDGES)) {
    return true;
  }
  if (mode_enabled(sd, br, BRUSH_AUTOMASKING_BOUNDARY_FACE_SETS)) {
    return true;
  }
  if (mode_enabled(sd, br, BRUSH_AUTOMASKING_BRUSH_NORMAL)) {
    return true;
  }
  if (mode_enabled(sd, br, BRUSH_AUTOMASKING_VIEW_NORMAL)) {
    return true;
  }
  if (mode_enabled(sd, br, BRUSH_AUTOMASKING_CAVITY_ALL)) {
    return true;
  }

  return false;
}

static int calc_effective_bits(const Sculpt &sd, const Brush *brush)
{
  if (brush) {
    int flags = sd.automasking_flags | brush->automasking_flags;

    /* Check if we are using brush cavity settings. */
    if (brush->automasking_flags & BRUSH_AUTOMASKING_CAVITY_ALL) {
      flags &= ~(BRUSH_AUTOMASKING_CAVITY_ALL | BRUSH_AUTOMASKING_CAVITY_USE_CURVE |
                 BRUSH_AUTOMASKING_CAVITY_NORMAL);
      flags |= brush->automasking_flags;
    }
    else if (sd.automasking_flags & BRUSH_AUTOMASKING_CAVITY_ALL) {
      flags &= ~(BRUSH_AUTOMASKING_CAVITY_ALL | BRUSH_AUTOMASKING_CAVITY_USE_CURVE |
                 BRUSH_AUTOMASKING_CAVITY_NORMAL);
      flags |= sd.automasking_flags;
    }

    return flags;
  }
  return sd.automasking_flags;
}

static float normal_calc(const float3 &compare_normal,
                         const float3 &normal,
                         float limit_lower,
                         float limit_upper)
{
  float angle = math::safe_acos(math::dot(compare_normal, normal));

  /* note that limit is pre-divided by M_PI */

  if (angle > limit_lower && angle < limit_upper) {
    float t = 1.0f - (angle - limit_lower) / (limit_upper - limit_lower);

    /* smoothstep */
    t = t * t * (3.0 - 2.0 * t);

    return t;
  }
  if (angle > limit_upper) {
    return 0.0f;
  }

  return 1.0f;
}

static bool is_constrained_by_radius(const Brush *br)
{
  if (br == nullptr) {
    return false;
  }

  /* 2D falloff is not constrained by radius. */
  if (br->falloff_shape == PAINT_FALLOFF_SHAPE_TUBE) {
    return false;
  }

  if (ELEM(br->sculpt_brush_type,
           SCULPT_BRUSH_TYPE_GRAB,
           SCULPT_BRUSH_TYPE_THUMB,
           SCULPT_BRUSH_TYPE_ROTATE))
  {
    return true;
  }
  return false;
}

/* Fetch the propogation_steps value, preferring the brush level value over the global sculpt tool
 * value. */
static int boundary_propagation_steps(const Sculpt &sd, const Brush *brush)
{
  return brush && brush->automasking_flags &
                      (BRUSH_AUTOMASKING_BOUNDARY_EDGES | BRUSH_AUTOMASKING_BOUNDARY_FACE_SETS) ?
             brush->automasking_boundary_edges_propagation_steps :
             sd.automasking_boundary_edges_propagation_steps;
}

/* Determine if the given automasking settings require values to be precomputed and cached. */
static bool needs_factors_cache(const Sculpt &sd, const Brush *brush)
{
  const int automasking_flags = calc_effective_bits(sd, brush);

  if (automasking_flags & BRUSH_AUTOMASKING_TOPOLOGY && brush && is_constrained_by_radius(brush)) {
    return true;
  }

  if (automasking_flags &
      (BRUSH_AUTOMASKING_BOUNDARY_EDGES | BRUSH_AUTOMASKING_BOUNDARY_FACE_SETS))
  {
    return boundary_propagation_steps(sd, brush) != 1;
  }
  return false;
}

static float calc_brush_normal_factor(const Cache &automasking,
                                      const Object &object,
                                      const float3 &normal)
{
  const SculptSession &ss = *object.sculpt;
  float falloff = automasking.settings.start_normal_falloff * M_PI;
  float3 initial_normal;

  if (ss.cache) {
    initial_normal = ss.cache->initial_normal_symm;
  }
  else {
    initial_normal = ss.filter_cache->initial_normal;
  }

  return normal_calc(normal,
                     initial_normal,
                     automasking.settings.start_normal_limit - falloff * 0.5f,
                     automasking.settings.start_normal_limit + falloff * 0.5f);
}

static float calc_view_normal_factor(const Cache &automasking,
                                     const Object &object,
                                     const float3 &normal)
{
  const SculptSession &ss = *object.sculpt;
  float falloff = automasking.settings.view_normal_falloff * M_PI;

  float3 view_normal;

  if (ss.cache) {
    view_normal = ss.cache->view_normal_symm;
  }
  else {
    view_normal = ss.filter_cache->view_normal;
  }

  return normal_calc(normal,
                     view_normal,
                     automasking.settings.view_normal_limit,
                     automasking.settings.view_normal_limit + falloff);
}

static bool calc_view_occlusion_factor(const Depsgraph &depsgraph,
                                       Cache &automasking,
                                       const Object &object,
                                       const int vert,
                                       const float3 &vert_position)
{
  if (automasking.occlusion[vert] == Cache::OcclusionValue::Unknown) {
    const bool occluded = vertex_is_occluded(depsgraph, object, vert_position, true);
    automasking.occlusion[vert] = occluded ? Cache::OcclusionValue::Occluded :
                                             Cache::OcclusionValue::Visible;
  }
  return automasking.occlusion[vert] == Cache::OcclusionValue::Occluded;
}

static float calc_cavity_factor(const Cache &automasking, float factor)
{
  float sign = signf(factor);

  factor = fabsf(factor) * automasking.settings.cavity_factor * 50.0f;

  factor = factor * sign * 0.5f + 0.5f;
  CLAMP(factor, 0.0f, 1.0f);

  return (automasking.settings.flags & BRUSH_AUTOMASKING_CAVITY_INVERTED) ? 1.0f - factor : factor;
}

struct AccumulatedVert {
  float3 position = float3(0.0f);
  float3 normal = float3(0.0f);
  float distance = 0.0f;
  int count = 0;
};

static void calc_blurred_cavity_mesh(const Depsgraph &depsgraph,
                                     const Object &object,
                                     const Cache &automasking,
                                     const int steps,
                                     const int vert,
                                     MutableSpan<float> cavity_factors)
{
  struct CavityBlurVert {
    int vertex;
    int depth;
  };

  const Mesh &mesh = *static_cast<Mesh *>(object.data);

  const OffsetIndices faces = mesh.faces();
  const Span<int> corner_verts = mesh.corner_verts();
  const GroupedSpan<int> vert_to_face_map = mesh.vert_to_face_map();

  const bke::AttributeAccessor attributes = mesh.attributes();
  const VArraySpan hide_poly = *attributes.lookup<bool>(".hide_poly", bke::AttrDomain::Face);

  const Span<float3> positions_eval = bke::pbvh::vert_positions_eval(depsgraph, object);
  const Span<float3> normals_eval = bke::pbvh::vert_normals_eval(depsgraph, object);

  AccumulatedVert all_verts;
  AccumulatedVert verts_in_range;
  /* Steps starts at 1, but API and user interface
   * are zero-based.
   */
  const int num_steps = steps + 1;

  std::queue<CavityBlurVert> queue;
  Set<int, 64> visited_verts;

  const CavityBlurVert initial{vert, 0};
  visited_verts.add_new(vert);
  queue.push(initial);

  const float3 starting_position = positions_eval[vert];

  Vector<int> neighbors;
  while (!queue.empty()) {
    const CavityBlurVert blurvert = queue.front();
    queue.pop();

    const int current_vert = blurvert.vertex;

    const float3 &blur_vert_position = positions_eval[current_vert];
    const float3 &blur_vert_normal = normals_eval[current_vert];

    const float dist_to_start = math::distance(blur_vert_position, starting_position);

    all_verts.position += blur_vert_position;
    all_verts.distance += dist_to_start;
    all_verts.count++;

    if (blurvert.depth < num_steps) {
      verts_in_range.position += blur_vert_position;
      verts_in_range.normal += blur_vert_normal;
      verts_in_range.count++;
    }

    /* Use the total number of steps used to get to this particular vert to determine if we should
     * keep processing */
    if (blurvert.depth >= num_steps) {
      continue;
    }

    for (const int neighbor : vert_neighbors_get_mesh(
             faces, corner_verts, vert_to_face_map, hide_poly, current_vert, neighbors))
    {
      if (visited_verts.contains(neighbor)) {
        continue;
      }

      visited_verts.add_new(neighbor);
      queue.push({neighbor, blurvert.depth + 1});
    }
  }

  BLI_assert(all_verts.count != verts_in_range.count);

  if (all_verts.count == 0) {
    all_verts.position = positions_eval[vert];
  }
  else {
    all_verts.position /= float(all_verts.count);
    all_verts.distance /= all_verts.count;
  }

  if (verts_in_range.count == 0) {
    verts_in_range.position = positions_eval[vert];
  }
  else {
    verts_in_range.position /= float(verts_in_range.count);
  }

  verts_in_range.normal = math::normalize(verts_in_range.normal);
  if (math::dot(verts_in_range.normal, verts_in_range.normal) == 0.0f) {
    verts_in_range.normal = normals_eval[vert];
  }

  const float3 vec = all_verts.position - verts_in_range.position;
  float factor_sum = math::safe_divide(math::dot(vec, verts_in_range.normal), all_verts.distance);
  cavity_factors[vert] = calc_cavity_factor(automasking, factor_sum);
}

static void calc_blurred_cavity_grids(const Object &object,
                                      const Cache &automasking,
                                      const int steps,
                                      const SubdivCCGCoord vert,
                                      MutableSpan<float> cavity_factors)
{
  struct CavityBlurVert {
    int vert;
    int depth;
  };

  const SculptSession &ss = *object.sculpt;
  const SubdivCCG &subdiv_ccg = *ss.subdiv_ccg;
  const Span<float3> positions = subdiv_ccg.positions;
  const Span<float3> normals = subdiv_ccg.normals;
  const CCGKey key = BKE_subdiv_ccg_key_top_level(subdiv_ccg);

  AccumulatedVert all_verts;
  AccumulatedVert verts_in_range;
  /* Steps starts at 1, but API and user interface
   * are zero-based.
   */
  const int num_steps = steps + 1;

  std::queue<CavityBlurVert> queue;
  Set<int, 64> visited_verts;

  const CavityBlurVert initial{vert.to_index(key), 0};
  visited_verts.add_new(initial.vert);
  queue.push(initial);

  const float3 starting_position = positions[vert.to_index(key)];

  SubdivCCGNeighbors neighbors;
  while (!queue.empty()) {
    const CavityBlurVert blurvert = queue.front();
    queue.pop();

    const int current_vert = blurvert.vert;

    const float3 &blur_vert_position = positions[current_vert];
    const float3 &blur_vert_normal = normals[current_vert];

    const float dist_to_start = math::distance(blur_vert_position, starting_position);

    all_verts.position += blur_vert_position;
    all_verts.distance += dist_to_start;
    all_verts.count++;

    if (blurvert.depth < num_steps) {
      verts_in_range.position += blur_vert_position;
      verts_in_range.normal += blur_vert_normal;
      verts_in_range.count++;
    }

    /* Use the total number of steps used to get to this particular vert to determine if we should
     * keep processing */
    if (blurvert.depth >= num_steps) {
      continue;
    }

    BKE_subdiv_ccg_neighbor_coords_get(
        subdiv_ccg, SubdivCCGCoord::from_index(key, current_vert), false, neighbors);
    for (const SubdivCCGCoord neighbor : neighbors.coords) {
      const int neighbor_idx = neighbor.to_index(key);
      if (visited_verts.contains(neighbor_idx)) {
        continue;
      }

      visited_verts.add_new(neighbor_idx);
      queue.push({neighbor_idx, blurvert.depth + 1});
    }
  }

  BLI_assert(all_verts.count != verts_in_range.count);

  if (all_verts.count == 0) {
    all_verts.position = positions[vert.to_index(key)];
  }
  else {
    all_verts.position /= float(all_verts.count);
    all_verts.distance /= all_verts.count;
  }

  if (verts_in_range.count == 0) {
    verts_in_range.position = positions[vert.to_index(key)];
  }
  else {
    verts_in_range.position /= float(verts_in_range.count);
  }

  verts_in_range.normal = math::normalize(verts_in_range.normal);
  if (math::dot(verts_in_range.normal, verts_in_range.normal) == 0.0f) {
    verts_in_range.normal = normals[vert.to_index(key)];
  }

  const float3 vec = all_verts.position - verts_in_range.position;
  float factor_sum = math::dot(vec, verts_in_range.normal) / all_verts.distance;
  cavity_factors[vert.to_index(key)] = calc_cavity_factor(automasking, factor_sum);
}

static void calc_blurred_cavity_bmesh(const Cache &automasking,
                                      const int steps,
                                      BMVert *vert,
                                      MutableSpan<float> cavity_factors)
{
  struct CavityBlurVert {
    BMVert *vertex;
    int index;
    int depth;
  };

  AccumulatedVert all_verts;
  AccumulatedVert verts_in_range;
  /* Steps starts at 1, but API and user interface
   * are zero-based.
   */
  const int num_steps = steps + 1;

  std::queue<CavityBlurVert> queue;
  Set<int, 64> visited_verts;

  const CavityBlurVert initial{vert, BM_elem_index_get(vert), 0};
  visited_verts.add_new(initial.index);
  queue.push(initial);

  const float3 starting_position = vert->co;

  BMeshNeighborVerts neighbors;
  while (!queue.empty()) {
    const CavityBlurVert blurvert = queue.front();
    queue.pop();

    BMVert *current_vert = blurvert.vertex;

    const float3 blur_vert_position = current_vert->co;
    const float3 blur_vert_normal = current_vert->no;

    const float dist_to_start = math::distance(blur_vert_position, starting_position);

    all_verts.position += blur_vert_position;
    all_verts.distance += dist_to_start;
    all_verts.count++;

    if (blurvert.depth < num_steps) {
      verts_in_range.position += blur_vert_position;
      verts_in_range.normal += blur_vert_normal;
      verts_in_range.count++;
    }

    /* Use the total number of steps used to get to this particular vert to determine if we should
     * keep processing */
    if (blurvert.depth >= num_steps) {
      continue;
    }

    for (BMVert *neighbor : vert_neighbors_get_bmesh(*current_vert, neighbors)) {
      const int neighbor_idx = BM_elem_index_get(neighbor);
      if (visited_verts.contains(neighbor_idx)) {
        continue;
      }

      visited_verts.add_new(neighbor_idx);
      queue.push({neighbor, neighbor_idx, blurvert.depth + 1});
    }
  }

  BLI_assert(all_verts.count != verts_in_range.count);

  if (all_verts.count == 0) {
    all_verts.position = vert->co;
  }
  else {
    all_verts.position /= float(all_verts.count);
    all_verts.distance /= all_verts.count;
  }

  if (verts_in_range.count == 0) {
    verts_in_range.position = vert->co;
  }
  else {
    verts_in_range.position /= float(verts_in_range.count);
  }

  verts_in_range.normal = math::normalize(verts_in_range.normal);
  if (math::dot(verts_in_range.normal, verts_in_range.normal) == 0.0f) {
    verts_in_range.normal = vert->no;
  }

  const float3 vec = all_verts.position - verts_in_range.position;
  float factor_sum = math::dot(vec, verts_in_range.normal) / all_verts.distance;
  cavity_factors[BM_elem_index_get(vert)] = calc_cavity_factor(automasking, factor_sum);
}

static float process_cavity_factor(const Cache &automasking, float factor)
{
  bool inverted = automasking.settings.flags & BRUSH_AUTOMASKING_CAVITY_INVERTED;

  if ((automasking.settings.flags & BRUSH_AUTOMASKING_CAVITY_ALL) &&
      (automasking.settings.flags & BRUSH_AUTOMASKING_CAVITY_USE_CURVE))
  {
    factor = inverted ? 1.0f - factor : factor;
    factor = BKE_curvemapping_evaluateF(automasking.settings.cavity_curve, 0, factor);
    factor = inverted ? 1.0f - factor : factor;
  }

  return factor;
}

static void calc_cavity_factor_mesh(const Depsgraph &depsgraph,
                                    const Cache &automasking,
                                    const Object &object,
                                    const int vert)
{
  if (automasking.cavity_factor[vert] == -1.0f) {
    calc_blurred_cavity_mesh(depsgraph,
                             object,
                             automasking,
                             automasking.settings.cavity_blur_steps,
                             vert,
                             const_cast<Cache &>(automasking).cavity_factor);
  }
}

static void calc_cavity_factor_grids(const CCGKey &key,
                                     const Cache &automasking,
                                     const Object &object,
                                     const int vert)
{
  if (automasking.cavity_factor[vert] == -1.0f) {
    calc_blurred_cavity_grids(object,
                              automasking,
                              automasking.settings.cavity_blur_steps,
                              SubdivCCGCoord::from_index(key, vert),
                              const_cast<Cache &>(automasking).cavity_factor);
  }
}

static void calc_cavity_factor_bmesh(const Cache &automasking, BMVert *vert, const int vert_i)
{
  if (automasking.cavity_factor[vert_i] == -1.0f) {
    calc_blurred_cavity_bmesh(automasking,
                              automasking.settings.cavity_blur_steps,
                              vert,
                              const_cast<Cache &>(automasking).cavity_factor);
  }
}

void calc_vert_factors(const Depsgraph &depsgraph,
                       const Object &object,
                       const Cache &automasking,
                       const bke::pbvh::MeshNode &node,
                       const Span<int> verts,
                       const MutableSpan<float> factors)
{
  const SculptSession &ss = *object.sculpt;
  const Mesh &mesh = *static_cast<const Mesh *>(object.data);
  const Span<float3> vert_positions = bke::pbvh::vert_positions_eval(depsgraph, object);
  const Span<float3> vert_normals = blender::bke::pbvh::vert_normals_eval(depsgraph, object);
  const GroupedSpan<int> vert_to_face_map = mesh.vert_to_face_map();
  const BitSpan boundary = ss.vertex_info.boundary;
  const bke::AttributeAccessor attributes = mesh.attributes();
  const VArraySpan face_sets = *attributes.lookup<int>(".sculpt_face_set", bke::AttrDomain::Face);
  const VArraySpan hide_poly = *attributes.lookup<bool>(".hide_poly", bke::AttrDomain::Face);
  Span<float3> orig_normals;
  if (automasking.settings.flags &
      (BRUSH_AUTOMASKING_BRUSH_NORMAL | BRUSH_AUTOMASKING_VIEW_NORMAL))
  {
    if (std::optional<OrigPositionData> orig_data = orig_position_data_lookup_mesh(object, node)) {
      orig_normals = orig_data->normals;
    }
  }

  for (const int i : verts.index_range()) {
    const int vert = verts[i];
    const float3 &normal = orig_normals.is_empty() ? vert_normals[vert] : orig_normals[i];

    /* Since brush normal mode depends on the current mirror symmetry pass
     * it is not folded into the factor cache (when it exists). */
    if ((ss.cache || ss.filter_cache) &&
        (automasking.settings.flags & BRUSH_AUTOMASKING_BRUSH_NORMAL))
    {
      factors[i] *= calc_brush_normal_factor(automasking, object, normal);
    }

    /* If the cache is initialized with valid info, use the cache. This is used when the
     * automasking information can't be computed in real time per vertex and needs to be
     * initialized for the whole mesh when the stroke starts. */
    if (!automasking.factor.is_empty()) {
      float cached_factor = automasking.factor[vert];

      if (automasking.settings.flags & BRUSH_AUTOMASKING_CAVITY_ALL) {
        BLI_assert(automasking.cavity_factor[vert] != -1.0f);
        cached_factor *= process_cavity_factor(automasking, automasking.cavity_factor[vert]);
      }

      factors[i] *= cached_factor;
      continue;
    }

    bool do_occlusion = (automasking.settings.flags &
                         (BRUSH_AUTOMASKING_VIEW_OCCLUSION | BRUSH_AUTOMASKING_VIEW_NORMAL)) ==
                        (BRUSH_AUTOMASKING_VIEW_OCCLUSION | BRUSH_AUTOMASKING_VIEW_NORMAL);
    if (do_occlusion) {
      const bool occluded = calc_view_occlusion_factor(
          depsgraph, const_cast<Cache &>(automasking), object, vert, vert_positions[vert]);
      if (occluded) {
        factors[i] = 0.0f;
        continue;
      }
    }

    if (!automasking.settings.topology_use_brush_limit &&
        automasking.settings.flags & BRUSH_AUTOMASKING_TOPOLOGY &&
        islands::vert_id_get(ss, vert) != automasking.settings.initial_island_nr)
    {
      factors[i] = 0.0f;
      continue;
    }

    if (automasking.settings.flags & BRUSH_AUTOMASKING_FACE_SETS) {
      if (!face_set::vert_has_face_set(
              vert_to_face_map, face_sets, vert, automasking.settings.initial_face_set))
      {
        factors[i] = 0.0f;
        continue;
      }
    }

    if (automasking.settings.flags & BRUSH_AUTOMASKING_BOUNDARY_EDGES) {
      if (boundary::vert_is_boundary(vert_to_face_map, hide_poly, boundary, vert)) {
        factors[i] = 0.0f;
        continue;
      }
    }

    if (automasking.settings.flags & BRUSH_AUTOMASKING_BOUNDARY_FACE_SETS) {
      bool ignore = ss.cache && ss.cache->brush &&
                    ss.cache->brush->sculpt_brush_type == SCULPT_BRUSH_TYPE_DRAW_FACE_SETS &&
                    face_set::vert_face_set_get(vert_to_face_map, face_sets, vert) ==
                        ss.cache->paint_face_set;

      if (!ignore && !face_set::vert_has_unique_face_set(vert_to_face_map, face_sets, vert)) {
        factors[i] = 0.0f;
        continue;
      }
    }

    if ((ss.cache || ss.filter_cache) &&
        (automasking.settings.flags & BRUSH_AUTOMASKING_VIEW_NORMAL))
    {
      factors[i] *= calc_view_normal_factor(automasking, object, normal);
    }

    if (automasking.settings.flags & BRUSH_AUTOMASKING_CAVITY_ALL) {
      BLI_assert(automasking.cavity_factor[vert] != -1.0f);
      factors[i] *= process_cavity_factor(automasking, automasking.cavity_factor[vert]);
    }
  }
}

void calc_face_factors(const Depsgraph &depsgraph,
                       const Object &object,
                       const OffsetIndices<int> faces,
                       const Span<int> corner_verts,
                       const Cache &automasking,
                       const bke::pbvh::MeshNode & /*node*/,
                       const Span<int> face_indices,
                       const MutableSpan<float> factors)
{
  const SculptSession &ss = *object.sculpt;
  const Mesh &mesh = *static_cast<const Mesh *>(object.data);
  const Span<float3> vert_positions = bke::pbvh::vert_positions_eval(depsgraph, object);
  const Span<float3> vert_normals = blender::bke::pbvh::vert_normals_eval(depsgraph, object);
  const GroupedSpan<int> vert_to_face_map = mesh.vert_to_face_map();
  const BitSpan boundary = ss.vertex_info.boundary;
  const bke::AttributeAccessor attributes = mesh.attributes();
  const VArraySpan face_sets = *attributes.lookup<int>(".sculpt_face_set", bke::AttrDomain::Face);
  const VArraySpan hide_poly = *attributes.lookup<bool>(".hide_poly", bke::AttrDomain::Face);
  for (const int i : face_indices.index_range()) {
    const Span<int> face_verts = corner_verts.slice(faces[face_indices[i]]);
    float sum = 0.0f;
    for (const int vert : face_verts) {
      float factor = 1.0f;
      BLI_SCOPED_DEFER([&]() { sum += factor; });

      /* Since brush normal mode depends on the current mirror symmetry pass
       * it is not folded into the factor cache (when it exists). */
      if ((ss.cache || ss.filter_cache) &&
          (automasking.settings.flags & BRUSH_AUTOMASKING_BRUSH_NORMAL))
      {
        factor *= calc_brush_normal_factor(automasking, object, vert_normals[vert]);
      }

      /* If the cache is initialized with valid info, use the cache. This is used when the
       * automasking information can't be computed in real time per vertex and needs to be
       * initialized for the whole mesh when the stroke starts. */
      if (!automasking.factor.is_empty()) {
        float cached_factor = automasking.factor[vert];

        if (automasking.settings.flags & BRUSH_AUTOMASKING_CAVITY_ALL) {
          BLI_assert(automasking.cavity_factor[vert] != -1.0f);
          cached_factor *= process_cavity_factor(automasking, automasking.cavity_factor[vert]);
        }

        factor *= cached_factor;
        continue;
      }

      bool do_occlusion = (automasking.settings.flags &
                           (BRUSH_AUTOMASKING_VIEW_OCCLUSION | BRUSH_AUTOMASKING_VIEW_NORMAL)) ==
                          (BRUSH_AUTOMASKING_VIEW_OCCLUSION | BRUSH_AUTOMASKING_VIEW_NORMAL);
      if (do_occlusion) {
        const bool occluded = calc_view_occlusion_factor(
            depsgraph, const_cast<Cache &>(automasking), object, vert, vert_positions[vert]);
        if (occluded) {
          factor = 0.0f;
          continue;
        }
      }

      if (!automasking.settings.topology_use_brush_limit &&
          automasking.settings.flags & BRUSH_AUTOMASKING_TOPOLOGY &&
          islands::vert_id_get(ss, vert) != automasking.settings.initial_island_nr)
      {
        factor = 0.0f;
        continue;
      }

      if (automasking.settings.flags & BRUSH_AUTOMASKING_FACE_SETS) {
        if (!face_set::vert_has_face_set(
                vert_to_face_map, face_sets, vert, automasking.settings.initial_face_set))
        {
          factor = 0.0f;
          continue;
        }
      }

      if (automasking.settings.flags & BRUSH_AUTOMASKING_BOUNDARY_EDGES) {
        if (boundary::vert_is_boundary(vert_to_face_map, hide_poly, boundary, vert)) {
          factor = 0.0f;
          continue;
        }
      }

      if (automasking.settings.flags & BRUSH_AUTOMASKING_BOUNDARY_FACE_SETS) {
        bool ignore = ss.cache && ss.cache->brush &&
                      ss.cache->brush->sculpt_brush_type == SCULPT_BRUSH_TYPE_DRAW_FACE_SETS &&
                      face_set::vert_face_set_get(vert_to_face_map, face_sets, vert) ==
                          ss.cache->paint_face_set;

        if (!ignore && !face_set::vert_has_unique_face_set(vert_to_face_map, face_sets, vert)) {
          factor = 0.0f;
          continue;
        }
      }

      if ((ss.cache || ss.filter_cache) &&
          (automasking.settings.flags & BRUSH_AUTOMASKING_VIEW_NORMAL))
      {
        factor *= calc_view_normal_factor(automasking, object, vert_normals[vert]);
      }

      if (automasking.settings.flags & BRUSH_AUTOMASKING_CAVITY_ALL) {
        BLI_assert(automasking.cavity_factor[vert] != -1.0f);
        factor *= process_cavity_factor(automasking, automasking.cavity_factor[vert]);
      }
    }
    factors[i] *= sum * math::rcp(float(face_verts.size()));
  }
}

void calc_grids_factors(const Depsgraph &depsgraph,
                        const Object &object,
                        const Cache &automasking,
                        const bke::pbvh::GridsNode &node,
                        const Span<int> grids,
                        const MutableSpan<float> factors)
{
  const SculptSession &ss = *object.sculpt;
  const Mesh &base_mesh = *static_cast<const Mesh *>(object.data);
  const OffsetIndices<int> faces = base_mesh.faces();
  const Span<int> corner_verts = base_mesh.corner_verts();
  const GroupedSpan<int> vert_to_face_map = base_mesh.vert_to_face_map();
  const BitSpan boundary = ss.vertex_info.boundary;
  const bke::AttributeAccessor attributes = base_mesh.attributes();
  const VArraySpan face_sets = *attributes.lookup<int>(".sculpt_face_set", bke::AttrDomain::Face);
  const SubdivCCG &subdiv_ccg = *ss.subdiv_ccg;
  const CCGKey key = BKE_subdiv_ccg_key_top_level(subdiv_ccg);

  Span<float3> orig_normals;
  if (automasking.settings.flags &
      (BRUSH_AUTOMASKING_BRUSH_NORMAL | BRUSH_AUTOMASKING_VIEW_NORMAL))
  {
    if (std::optional<OrigPositionData> orig_data = orig_position_data_lookup_grids(object, node))
    {
      orig_normals = orig_data->normals;
    }
  }

  for (const int i : grids.index_range()) {
    const int grid_face_set = face_sets.is_empty() ?
                                  SCULPT_FACE_SET_NONE :
                                  face_sets[subdiv_ccg.grid_to_face_map[grids[i]]];
    const int node_start = i * key.grid_area;
    const int grids_start = grids[i] * key.grid_area;
    for (const int offset : IndexRange(key.grid_area)) {
      const int node_vert = node_start + offset;
      const int vert = grids_start + offset;
      const float3 &normal = orig_normals.is_empty() ? subdiv_ccg.normals[vert] :
                                                       orig_normals[node_vert];

      /* Since brush normal mode depends on the current mirror symmetry pass
       * it is not folded into the factor cache (when it exists). */
      if ((ss.cache || ss.filter_cache) &&
          (automasking.settings.flags & BRUSH_AUTOMASKING_BRUSH_NORMAL))
      {
        factors[node_vert] *= calc_brush_normal_factor(automasking, object, normal);
      }

      /* If the cache is initialized with valid info, use the cache. This is used when the
       * automasking information can't be computed in real time per vertex and needs to be
       * initialized for the whole mesh when the stroke starts. */
      if (!automasking.factor.is_empty()) {
        float cached_factor = automasking.factor[vert];

        if (automasking.settings.flags & BRUSH_AUTOMASKING_CAVITY_ALL) {
          BLI_assert(automasking.cavity_factor[vert] != -1.0f);
          cached_factor *= process_cavity_factor(automasking, automasking.cavity_factor[vert]);
        }

        factors[node_vert] *= cached_factor;
        continue;
      }

      bool do_occlusion = (automasking.settings.flags &
                           (BRUSH_AUTOMASKING_VIEW_OCCLUSION | BRUSH_AUTOMASKING_VIEW_NORMAL)) ==
                          (BRUSH_AUTOMASKING_VIEW_OCCLUSION | BRUSH_AUTOMASKING_VIEW_NORMAL);
      if (do_occlusion) {
        const bool occluded = calc_view_occlusion_factor(
            depsgraph, const_cast<Cache &>(automasking), object, vert, subdiv_ccg.positions[vert]);
        if (occluded) {
          factors[node_vert] = 0.0f;
          continue;
        }
      }

      if (!automasking.settings.topology_use_brush_limit &&
          automasking.settings.flags & BRUSH_AUTOMASKING_TOPOLOGY &&
          islands::vert_id_get(ss, vert) != automasking.settings.initial_island_nr)
      {
        factors[node_vert] = 0.0f;
        continue;
      }

      if (automasking.settings.flags & BRUSH_AUTOMASKING_FACE_SETS) {
        if (grid_face_set != automasking.settings.initial_face_set) {
          factors[node_vert] = 0.0f;
          continue;
        }
      }

      if (automasking.settings.flags & BRUSH_AUTOMASKING_BOUNDARY_EDGES) {
        if (boundary::vert_is_boundary(
                faces, corner_verts, boundary, subdiv_ccg, SubdivCCGCoord::from_index(key, vert)))
        {
          factors[node_vert] = 0.0f;
          continue;
        }
      }

      if (automasking.settings.flags & BRUSH_AUTOMASKING_BOUNDARY_FACE_SETS) {
        bool ignore = ss.cache && ss.cache->brush &&
                      ss.cache->brush->sculpt_brush_type == SCULPT_BRUSH_TYPE_DRAW_FACE_SETS &&
                      grid_face_set == ss.cache->paint_face_set;

        if (!ignore && !face_set::vert_has_unique_face_set(faces,
                                                           corner_verts,
                                                           vert_to_face_map,
                                                           face_sets,
                                                           subdiv_ccg,
                                                           SubdivCCGCoord::from_index(key, vert)))
        {
          factors[node_vert] = 0.0f;
          continue;
        }
      }

      if ((ss.cache || ss.filter_cache) &&
          (automasking.settings.flags & BRUSH_AUTOMASKING_VIEW_NORMAL))
      {
        factors[node_vert] *= calc_view_normal_factor(automasking, object, normal);
      }

      if (automasking.settings.flags & BRUSH_AUTOMASKING_CAVITY_ALL) {
        BLI_assert(automasking.cavity_factor[vert] != -1.0f);
        factors[node_vert] *= process_cavity_factor(automasking, automasking.cavity_factor[vert]);
      }
    }
  }
}

void calc_vert_factors(const Depsgraph &depsgraph,
                       const Object &object,
                       const Cache &automasking,
                       const bke::pbvh::BMeshNode & /*node*/,
                       const Set<BMVert *, 0> &verts,
                       const MutableSpan<float> factors)
{
  SculptSession &ss = *object.sculpt;
  BMesh &bm = *ss.bm;
  const int face_set_offset = CustomData_get_offset_named(
      &bm.pdata, CD_PROP_INT32, ".sculpt_face_set");

  Array<float3> orig_normals;
  if (automasking.settings.flags &
      (BRUSH_AUTOMASKING_BRUSH_NORMAL | BRUSH_AUTOMASKING_VIEW_NORMAL))
  {
    orig_position_data_gather_bmesh(*ss.bm_log, verts, {}, orig_normals);
  }

  int i = 0;
  for (BMVert *vert : verts) {
    BLI_SCOPED_DEFER([&]() { i++; });
    const int vert_i = BM_elem_index_get(vert);
    const float3 normal = orig_normals.is_empty() ? float3(vert->no) : orig_normals[i];

    /* Since brush normal mode depends on the current mirror symmetry pass
     * it is not folded into the factor cache (when it exists). */
    if ((ss.cache || ss.filter_cache) &&
        (automasking.settings.flags & BRUSH_AUTOMASKING_BRUSH_NORMAL))
    {
      factors[i] *= calc_brush_normal_factor(automasking, object, normal);
    }

    /* If the cache is initialized with valid info, use the cache. This is used when the
     * automasking information can't be computed in real time per vertex and needs to be
     * initialized for the whole mesh when the stroke starts. */
    if (!automasking.factor.is_empty()) {
      float cached_factor = automasking.factor[vert_i];

      if (automasking.settings.flags & BRUSH_AUTOMASKING_CAVITY_ALL) {
        BLI_assert(automasking.cavity_factor[vert_i] != -1.0f);
        cached_factor *= process_cavity_factor(automasking, automasking.cavity_factor[vert_i]);
      }

      factors[i] *= cached_factor;
      continue;
    }

    bool do_occlusion = (automasking.settings.flags &
                         (BRUSH_AUTOMASKING_VIEW_OCCLUSION | BRUSH_AUTOMASKING_VIEW_NORMAL)) ==
                        (BRUSH_AUTOMASKING_VIEW_OCCLUSION | BRUSH_AUTOMASKING_VIEW_NORMAL);
    if (do_occlusion) {
      const bool occluded = calc_view_occlusion_factor(
          depsgraph, const_cast<Cache &>(automasking), object, vert_i, vert->co);
      if (occluded) {
        factors[i] = 0.0f;
        continue;
      }
    }

    if (!automasking.settings.topology_use_brush_limit &&
        automasking.settings.flags & BRUSH_AUTOMASKING_TOPOLOGY &&
        islands::vert_id_get(ss, vert_i) != automasking.settings.initial_island_nr)
    {
      factors[i] = 0.0f;
      continue;
    }

    if (automasking.settings.flags & BRUSH_AUTOMASKING_FACE_SETS) {
      if (!face_set::vert_has_face_set(
              face_set_offset, *vert, automasking.settings.initial_face_set))
      {
        factors[i] = 0.0f;
        continue;
      }
    }

    if (automasking.settings.flags & BRUSH_AUTOMASKING_BOUNDARY_EDGES) {
      if (boundary::vert_is_boundary(vert)) {
        factors[i] = 0.0f;
        continue;
      }
    }

    if (automasking.settings.flags & BRUSH_AUTOMASKING_BOUNDARY_FACE_SETS) {
      bool ignore = ss.cache && ss.cache->brush &&
                    ss.cache->brush->sculpt_brush_type == SCULPT_BRUSH_TYPE_DRAW_FACE_SETS &&
                    face_set::vert_face_set_get(face_set_offset, *vert) ==
                        ss.cache->paint_face_set;

      if (!ignore && !face_set::vert_has_unique_face_set(face_set_offset, *vert)) {
        factors[i] = 0.0f;
        continue;
      }
    }

    if ((ss.cache || ss.filter_cache) &&
        (automasking.settings.flags & BRUSH_AUTOMASKING_VIEW_NORMAL))
    {
      factors[i] *= calc_view_normal_factor(automasking, object, normal);
    }

    if (automasking.settings.flags & BRUSH_AUTOMASKING_CAVITY_ALL) {
      BLI_assert(automasking.cavity_factor[vert_i] != -1.0f);
      factors[i] *= process_cavity_factor(automasking, automasking.cavity_factor[vert_i]);
    }
  }
}

static void fill_topology_automasking_factors_mesh(const Depsgraph &depsgraph,
                                                   const Sculpt &sd,
                                                   Object &ob,
                                                   const Span<float3> vert_positions,
                                                   MutableSpan<float> factors)
{
  SculptSession &ss = *ob.sculpt;
  const Brush *brush = BKE_paint_brush_for_read(&sd.paint);
  const Mesh &mesh = *static_cast<const Mesh *>(ob.data);
  const GroupedSpan<int> vert_to_face_map = mesh.vert_to_face_map();

  const float radius = ss.cache ? ss.cache->radius : std::numeric_limits<float>::max();
  const int active_vert = std::get<int>(ss.active_vert());
  flood_fill::FillDataMesh flood = flood_fill::FillDataMesh(vert_positions.size());

  flood.add_initial(find_symm_verts_mesh(depsgraph, ob, active_vert, radius));

  const bool use_radius = ss.cache && is_constrained_by_radius(brush);
  const ePaintSymmetryFlags symm = SCULPT_mesh_symmetry_xyz_get(ob);

  float3 location = vert_positions[active_vert];

  if (use_radius) {
    flood.execute(ob, vert_to_face_map, [&](int from_v, int to_v) {
      factors[from_v] = 1.0f;
      factors[to_v] = 1.0f;
      return SCULPT_is_vertex_inside_brush_radius_symm(
          vert_positions[to_v], location, radius, symm);
    });
  }
  else {
    flood.execute(ob, vert_to_face_map, [&](int from_v, int to_v) {
      factors[from_v] = 1.0f;
      factors[to_v] = 1.0f;
      return true;
    });
  }
}

static void fill_topology_automasking_factors_grids(const Sculpt &sd,
                                                    Object &ob,
                                                    const SubdivCCG &subdiv_ccg,
                                                    MutableSpan<float> factors)

{
  SculptSession &ss = *ob.sculpt;
  const Brush *brush = BKE_paint_brush_for_read(&sd.paint);

  const float radius = ss.cache ? ss.cache->radius : std::numeric_limits<float>::max();
  const int active_vert = ss.active_vert_index();

  const Span<float3> positions = subdiv_ccg.positions;
  const CCGKey key = BKE_subdiv_ccg_key_top_level(subdiv_ccg);

  flood_fill::FillDataGrids flood = flood_fill::FillDataGrids(positions.size());

  flood.add_initial(key, find_symm_verts_grids(ob, active_vert, radius));

  const bool use_radius = ss.cache && is_constrained_by_radius(brush);
  const ePaintSymmetryFlags symm = SCULPT_mesh_symmetry_xyz_get(ob);

  float3 location = positions[active_vert];

  if (use_radius) {
    flood.execute(
        ob, subdiv_ccg, [&](SubdivCCGCoord from_v, SubdivCCGCoord to_v, bool /*is_duplicate*/) {
          factors[from_v.to_index(key)] = 1.0f;
          factors[to_v.to_index(key)] = 1.0f;
          return SCULPT_is_vertex_inside_brush_radius_symm(
              positions[to_v.to_index(key)], location, radius, symm);
        });
  }
  else {
    flood.execute(
        ob, subdiv_ccg, [&](SubdivCCGCoord from_v, SubdivCCGCoord to_v, bool /*is_duplicate*/) {
          factors[from_v.to_index(key)] = 1.0f;
          factors[to_v.to_index(key)] = 1.0f;
          return true;
        });
  }
}

static void fill_topology_automasking_factors_bmesh(const Sculpt &sd,
                                                    Object &ob,
                                                    BMesh &bm,
                                                    MutableSpan<float> factors)
{
  SculptSession &ss = *ob.sculpt;
  const Brush *brush = BKE_paint_brush_for_read(&sd.paint);

  const float radius = ss.cache ? ss.cache->radius : std::numeric_limits<float>::max();
  BMVert *active_vert = std::get<BMVert *>(ss.active_vert());
  const int num_verts = BM_mesh_elem_count(&bm, BM_VERT);
  flood_fill::FillDataBMesh flood = flood_fill::FillDataBMesh(num_verts);

  flood.add_initial(*ss.bm, find_symm_verts_bmesh(ob, BM_elem_index_get(active_vert), radius));

  const bool use_radius = ss.cache && is_constrained_by_radius(brush);
  const ePaintSymmetryFlags symm = SCULPT_mesh_symmetry_xyz_get(ob);

  float3 location = active_vert->co;

  if (use_radius) {
    flood.execute(ob, [&](BMVert *from_v, BMVert *to_v) {
      factors[BM_elem_index_get(from_v)] = 1.0f;
      factors[BM_elem_index_get(to_v)] = 1.0f;
      return SCULPT_is_vertex_inside_brush_radius_symm(to_v->co, location, radius, symm);
    });
  }
  else {
    flood.execute(ob, [&](BMVert *from_v, BMVert *to_v) {
      factors[BM_elem_index_get(from_v)] = 1.0f;
      factors[BM_elem_index_get(to_v)] = 1.0f;
      return true;
    });
  }
}

static void fill_topology_automasking_factors(const Depsgraph &depsgraph,
                                              const Sculpt &sd,
                                              Object &ob,
                                              MutableSpan<float> factors)
{
  /* TODO: This method is to be removed when more of the automasking code handles the different
   * pbvh types. */
  SculptSession &ss = *ob.sculpt;
  if (std::holds_alternative<std::monostate>(ss.active_vert())) {
    /* If we don't have an active vertex (i.e. the cursor is not over the mesh), we cannot
     * accurately calculate the topology automasking factor as it may be ambiguous which island the
     * user is intending to affect. */
    return;
  }

  switch (bke::object::pbvh_get(ob)->type()) {
    case bke::pbvh::Type::Mesh:
      fill_topology_automasking_factors_mesh(
          depsgraph, sd, ob, bke::pbvh::vert_positions_eval(depsgraph, ob), factors);
      break;
    case bke::pbvh::Type::Grids:
      fill_topology_automasking_factors_grids(sd, ob, *ss.subdiv_ccg, factors);
      break;
    case bke::pbvh::Type::BMesh:
      fill_topology_automasking_factors_bmesh(sd, ob, *ss.bm, factors);
      break;
  }
}

static void init_face_sets_masking(const Sculpt &sd, Object &ob, MutableSpan<float> factors)
{
  const Brush *brush = BKE_paint_brush_for_read(&sd.paint);

  if (!is_enabled(sd, ob, brush)) {
    return;
  }

  const int active_face_set = face_set::active_face_set_get(ob);
  switch (bke::object::pbvh_get(ob)->type()) {
    case bke::pbvh::Type::Mesh: {
      const Mesh &mesh = *static_cast<const Mesh *>(ob.data);
      const GroupedSpan<int> vert_to_face_map = mesh.vert_to_face_map();
      const bke::AttributeAccessor attributes = mesh.attributes();
      const VArraySpan face_sets = *attributes.lookup<int>(".sculpt_face_set",
                                                           bke::AttrDomain::Face);
      if (face_sets.is_empty()) {
        return;
      }
      threading::parallel_for(IndexRange(mesh.verts_num), 1024, [&](const IndexRange range) {
        for (const int vert : range) {
          if (!face_set::vert_has_face_set(vert_to_face_map, face_sets, vert, active_face_set)) {
            factors[vert] = 0.0f;
          }
        }
      });
      break;
    }
    case bke::pbvh::Type::Grids: {
      const Mesh &base_mesh = *static_cast<const Mesh *>(ob.data);
      const OffsetIndices<int> faces = base_mesh.faces();
      const bke::AttributeAccessor attributes = base_mesh.attributes();
      const VArraySpan face_sets = *attributes.lookup<int>(".sculpt_face_set",
                                                           bke::AttrDomain::Face);
      if (face_sets.is_empty()) {
        return;
      }
      const SculptSession &ss = *ob.sculpt;
      const SubdivCCG &subdiv_ccg = *ss.subdiv_ccg;
      const int grid_area = subdiv_ccg.grid_area;
      threading::parallel_for(faces.index_range(), 128, [&](const IndexRange range) {
        for (const int face : range) {
          if (face_sets[face] != active_face_set) {
            factors.slice(bke::ccg::face_range(faces, grid_area, face)).fill(0.0f);
          }
        }
      });

      break;
    }
    case bke::pbvh::Type::BMesh: {
      const SculptSession &ss = *ob.sculpt;
      const BMesh &bm = *ss.bm;
      const int face_set_offset = CustomData_get_offset_named(
          &bm.pdata, CD_PROP_INT32, ".sculpt_face_set");
      if (face_set_offset == -1) {
        return;
      }
      threading::parallel_for(IndexRange(bm.totvert), 1024, [&](const IndexRange range) {
        for (const int i : range) {
          const BMVert *vert = BM_vert_at_index(&const_cast<BMesh &>(bm), i);
          if (!face_set::vert_has_face_set(face_set_offset, *vert, active_face_set)) {
            factors[i] = 0.0f;
          }
        }
      });
      break;
    }
  }
}

static constexpr int EDGE_DISTANCE_INF = -1;

enum class BoundaryAutomaskMode {
  Edges = 1,
  FaceSets = 2,
};

static void init_boundary_masking_mesh(Object &object,
                                       const Depsgraph &depsgraph,
                                       const BoundaryAutomaskMode mode,
                                       const int propagation_steps,
                                       MutableSpan<float> factors)
{
  SculptSession &ss = *object.sculpt;
  Mesh &mesh = *static_cast<Mesh *>(object.data);

  const OffsetIndices faces = mesh.faces();
  const Span<int> corner_verts = mesh.corner_verts();
  const GroupedSpan<int> vert_to_face_map = mesh.vert_to_face_map();
  const bke::AttributeAccessor attributes = mesh.attributes();
  const VArraySpan hide_poly = *attributes.lookup<bool>(".hide_poly", bke::AttrDomain::Face);
  const VArraySpan face_sets = *attributes.lookup<int>(".sculpt_face_set", bke::AttrDomain::Face);

  const int num_verts = bke::pbvh::vert_positions_eval(depsgraph, object).size();
  Array<int> edge_distance(num_verts, EDGE_DISTANCE_INF);

  for (const int i : IndexRange(num_verts)) {
    switch (mode) {
      case BoundaryAutomaskMode::Edges:
        if (boundary::vert_is_boundary(vert_to_face_map, hide_poly, ss.vertex_info.boundary, i)) {
          edge_distance[i] = 0;
        }
        break;
      case BoundaryAutomaskMode::FaceSets:
        if (!face_set::vert_has_unique_face_set(vert_to_face_map, face_sets, i)) {
          edge_distance[i] = 0;
        }
        break;
    }
  }

  Vector<int> neighbors;
  for (const int propagation_it : IndexRange(propagation_steps)) {
    for (const int i : IndexRange(num_verts)) {
      if (edge_distance[i] != EDGE_DISTANCE_INF) {
        continue;
      }

      for (const int neighbor :
           vert_neighbors_get_mesh(faces, corner_verts, vert_to_face_map, hide_poly, i, neighbors))
      {
        if (edge_distance[neighbor] == propagation_it) {
          edge_distance[i] = propagation_it + 1;
        }
      }
    }
  }

  for (const int i : IndexRange(num_verts)) {
    if (edge_distance[i] == EDGE_DISTANCE_INF) {
      continue;
    }

    const float p = 1.0f - (float(edge_distance[i]) / float(propagation_steps));
    const float edge_boundary_automask = pow2f(p);

    factors[i] *= (1.0f - edge_boundary_automask);
  }
}

static void init_boundary_masking_grids(Object &object,
                                        const BoundaryAutomaskMode mode,
                                        const int propagation_steps,
                                        MutableSpan<float> factors)
{
  SculptSession &ss = *object.sculpt;
  const SubdivCCG &subdiv_ccg = *ss.subdiv_ccg;
  Mesh &mesh = *static_cast<Mesh *>(object.data);

  const Span<float3> positions = subdiv_ccg.positions;
  const CCGKey key = BKE_subdiv_ccg_key_top_level(subdiv_ccg);

  const OffsetIndices faces = mesh.faces();
  const Span<int> corner_verts = mesh.corner_verts();
  const GroupedSpan<int> vert_to_face_map = mesh.vert_to_face_map();
  const bke::AttributeAccessor attributes = mesh.attributes();
  const VArraySpan face_sets = *attributes.lookup<int>(".sculpt_face_set", bke::AttrDomain::Face);

  Array<int> edge_distance(positions.size(), EDGE_DISTANCE_INF);
  for (const int i : positions.index_range()) {
    const SubdivCCGCoord coord = SubdivCCGCoord::from_index(key, i);
    switch (mode) {
      case BoundaryAutomaskMode::Edges:
        if (boundary::vert_is_boundary(
                faces, corner_verts, ss.vertex_info.boundary, subdiv_ccg, coord))
        {
          edge_distance[i] = 0;
        }
        break;
      case BoundaryAutomaskMode::FaceSets:
        if (!face_set::vert_has_unique_face_set(
                faces, corner_verts, vert_to_face_map, face_sets, subdiv_ccg, coord))
        {
          edge_distance[i] = 0;
        }
        break;
    }
  }

  SubdivCCGNeighbors neighbors;
  for (const int propagation_it : IndexRange(propagation_steps)) {
    for (const int i : positions.index_range()) {
      if (edge_distance[i] != EDGE_DISTANCE_INF) {
        continue;
      }

      const SubdivCCGCoord coord = SubdivCCGCoord::from_index(key, i);

      BKE_subdiv_ccg_neighbor_coords_get(subdiv_ccg, coord, false, neighbors);
      for (const SubdivCCGCoord neighbor : neighbors.coords) {
        const int neighbor_idx = neighbor.to_index(key);
        if (edge_distance[neighbor_idx] == propagation_it) {
          edge_distance[i] = propagation_it + 1;
        }
      }
    }
  }

  for (const int i : positions.index_range()) {
    if (edge_distance[i] == EDGE_DISTANCE_INF) {
      continue;
    }

    const float p = 1.0f - (float(edge_distance[i]) / float(propagation_steps));
    const float edge_boundary_automask = pow2f(p);

    factors[i] *= (1.0f - edge_boundary_automask);
  }
}

static void init_boundary_masking_bmesh(Object &object,
                                        const BoundaryAutomaskMode mode,
                                        const int propagation_steps,
                                        MutableSpan<float> factors)
{
  SculptSession &ss = *object.sculpt;
  BMesh &bm = *ss.bm;
  const int face_set_offset = CustomData_get_offset_named(
      &bm.pdata, CD_PROP_INT32, ".sculpt_face_set");
  const int num_verts = BM_mesh_elem_count(&bm, BM_VERT);

  Array<int> edge_distance(num_verts, EDGE_DISTANCE_INF);

  for (const int i : IndexRange(num_verts)) {
    BMVert *vert = BM_vert_at_index(&bm, i);
    switch (mode) {
      case BoundaryAutomaskMode::Edges:
        if (boundary::vert_is_boundary(vert)) {
          edge_distance[i] = 0;
        }
        break;
      case BoundaryAutomaskMode::FaceSets:
        if (!face_set::vert_has_unique_face_set(face_set_offset, *vert)) {
          edge_distance[i] = 0;
        }
    }
  }

  BMeshNeighborVerts neighbors;
  for (const int propagation_it : IndexRange(propagation_steps)) {
    for (const int i : IndexRange(num_verts)) {
      if (edge_distance[i] != EDGE_DISTANCE_INF) {
        continue;
      }

      BMVert *vert = BM_vert_at_index(&bm, i);
      for (BMVert *neighbor : vert_neighbors_get_bmesh(*vert, neighbors)) {
        const int neighbor_idx = BM_elem_index_get(neighbor);

        if (edge_distance[neighbor_idx] == propagation_it) {
          edge_distance[i] = propagation_it + 1;
        }
      }
    }
  }

  for (const int i : IndexRange(num_verts)) {
    if (edge_distance[i] == EDGE_DISTANCE_INF) {
      continue;
    }

    const float p = 1.0f - (float(edge_distance[i]) / float(propagation_steps));
    const float edge_boundary_automask = pow2f(p);

    factors[i] *= (1.0f - edge_boundary_automask);
  }
}

static void init_boundary_masking(Object &object,
                                  const Depsgraph &depsgraph,
                                  const BoundaryAutomaskMode mode,
                                  const int propagation_steps,
                                  MutableSpan<float> factors)
{
  switch (bke::object::pbvh_get(object)->type()) {
    case bke::pbvh::Type::Mesh:
      init_boundary_masking_mesh(object, depsgraph, mode, propagation_steps, factors);
      break;
    case bke::pbvh::Type::Grids:
      init_boundary_masking_grids(object, mode, propagation_steps, factors);
      break;
    case bke::pbvh::Type::BMesh:
      init_boundary_masking_bmesh(object, mode, propagation_steps, factors);
      break;
  }
}

/* Updates the cached values, preferring brush settings over tool-level settings. */
static void cache_settings_update(Cache &automasking,
                                  Object &object,
                                  const Sculpt &sd,
                                  const Brush *brush)
{
  automasking.settings.flags = calc_effective_bits(sd, brush);
  automasking.settings.initial_face_set = face_set::active_face_set_get(object);

  if (brush && (brush->automasking_flags & BRUSH_AUTOMASKING_VIEW_NORMAL)) {
    automasking.settings.view_normal_limit = brush->automasking_view_normal_limit;
    automasking.settings.view_normal_falloff = brush->automasking_view_normal_falloff;
  }
  else {
    automasking.settings.view_normal_limit = sd.automasking_view_normal_limit;
    automasking.settings.view_normal_falloff = sd.automasking_view_normal_falloff;
  }

  if (brush && (brush->automasking_flags & BRUSH_AUTOMASKING_BRUSH_NORMAL)) {
    automasking.settings.start_normal_limit = brush->automasking_start_normal_limit;
    automasking.settings.start_normal_falloff = brush->automasking_start_normal_falloff;
  }
  else {
    automasking.settings.start_normal_limit = sd.automasking_start_normal_limit;
    automasking.settings.start_normal_falloff = sd.automasking_start_normal_falloff;
  }

  if (brush && (brush->automasking_flags & BRUSH_AUTOMASKING_CAVITY_ALL)) {
    automasking.settings.cavity_curve = brush->automasking_cavity_curve;
    automasking.settings.cavity_factor = brush->automasking_cavity_factor;
    automasking.settings.cavity_blur_steps = brush->automasking_cavity_blur_steps;
  }
  else {
    automasking.settings.cavity_curve = sd.automasking_cavity_curve;
    automasking.settings.cavity_factor = sd.automasking_cavity_factor;
    automasking.settings.cavity_blur_steps = sd.automasking_cavity_blur_steps;
  }
}

static void normal_occlusion_automasking_fill(const Depsgraph &depsgraph,
                                              Cache &automasking,
                                              Object &ob,
                                              eAutomasking_flag mode,
                                              MutableSpan<float> factors)
{
  const int totvert = SCULPT_vertex_count_get(ob);
  /* No need to build original data since this is only called at the beginning of strokes. */
  switch (bke::object::pbvh_get(ob)->type()) {
    case bke::pbvh::Type::Mesh: {
      const Span<float3> vert_positions = bke::pbvh::vert_positions_eval(depsgraph, ob);
      const Span<float3> vert_normals = blender::bke::pbvh::vert_normals_eval(depsgraph, ob);
      threading::parallel_for(IndexRange(totvert), 1024, [&](const IndexRange range) {
        for (const int vert : range) {
          float f = factors[vert];

          if (int(mode) & BRUSH_AUTOMASKING_VIEW_NORMAL) {
            if (int(mode) & BRUSH_AUTOMASKING_VIEW_OCCLUSION) {
              f *= calc_view_occlusion_factor(
                  depsgraph, automasking, ob, vert, vert_positions[vert]);
            }

            f *= calc_view_normal_factor(automasking, ob, vert_normals[vert]);
          }

          factors[vert] = f;
        }
      });
      break;
    }
    case bke::pbvh::Type::Grids: {
      const SculptSession &ss = *ob.sculpt;
      const SubdivCCG &subdiv_ccg = *ss.subdiv_ccg;
      threading::parallel_for(IndexRange(totvert), 1024, [&](const IndexRange range) {
        for (const int vert : range) {
          float f = factors[vert];

          if (int(mode) & BRUSH_AUTOMASKING_VIEW_NORMAL) {
            if (int(mode) & BRUSH_AUTOMASKING_VIEW_OCCLUSION) {
              f *= calc_view_occlusion_factor(
                  depsgraph, automasking, ob, vert, subdiv_ccg.positions[vert]);
            }

            f *= calc_view_normal_factor(automasking, ob, subdiv_ccg.normals[vert]);
          }

          factors[vert] = f;
        }
      });
      break;
    }
    case bke::pbvh::Type::BMesh: {
      const SculptSession &ss = *ob.sculpt;
      BMesh &bm = *ss.bm;
      threading::parallel_for(IndexRange(bm.totvert), 1024, [&](const IndexRange range) {
        for (const int i : range) {
          const BMVert *vert = BM_vert_at_index(&bm, i);
          float f = factors[i];

          if (int(mode) & BRUSH_AUTOMASKING_VIEW_NORMAL) {
            if (int(mode) & BRUSH_AUTOMASKING_VIEW_OCCLUSION) {
              f *= calc_view_occlusion_factor(depsgraph, automasking, ob, i, vert->co);
            }

            f *= calc_view_normal_factor(automasking, ob, vert->no);
          }

          factors[i] = f;
        }
      });
      break;
    }
  }
}

std::unique_ptr<Cache> cache_init(const Depsgraph &depsgraph,
                                  const Sculpt &sd,
                                  const Brush *brush,
                                  Object &ob)
{
  SculptSession &ss = *ob.sculpt;

  if (!is_enabled(sd, ob, brush)) {
    return nullptr;
  }

  std::unique_ptr<Cache> automasking = std::make_unique<Cache>();
  cache_settings_update(*automasking, ob, sd, brush);
  boundary::ensure_boundary_info(ob);

  int mode = calc_effective_bits(sd, brush);

  vert_random_access_ensure(ob);
  if (mode & BRUSH_AUTOMASKING_TOPOLOGY && ss.active_vert_index() != -1) {
    islands::ensure_cache(ob);
    automasking->settings.initial_island_nr = islands::vert_id_get(ss, ss.active_vert_index());
  }

  const int verts_num = SCULPT_vertex_count_get(ob);

  if ((mode & BRUSH_AUTOMASKING_VIEW_OCCLUSION) && (mode & BRUSH_AUTOMASKING_VIEW_NORMAL)) {
    automasking->occlusion = Array<Cache::OcclusionValue>(verts_num,
                                                          Cache::OcclusionValue::Unknown);
  }

  if (mode & BRUSH_AUTOMASKING_CAVITY_ALL) {
    if (mode_enabled(sd, brush, BRUSH_AUTOMASKING_CAVITY_USE_CURVE)) {
      if (brush) {
        BKE_curvemapping_init(brush->automasking_cavity_curve);
      }

      BKE_curvemapping_init(sd.automasking_cavity_curve);
    }
    automasking->cavity_factor = Array<float>(verts_num, -1.0f);
  }

  /* Avoid precomputing data on the vertex level if the current auto-masking modes do not require
   * it to function. */
  if (!needs_factors_cache(sd, brush)) {
    return automasking;
  }

  /* Topology builds up the mask from zero which other modes can subtract from.
   * If it isn't enabled, initialize to 1. */
  const float initial_value = !(mode & BRUSH_AUTOMASKING_TOPOLOGY) ? 1.0f : 0.0f;
  automasking->factor = Array<float>(verts_num, initial_value);
  MutableSpan<float> factors = automasking->factor;

  /* Additive modes. */
  if (mode_enabled(sd, brush, BRUSH_AUTOMASKING_TOPOLOGY)) {
    vert_random_access_ensure(ob);

    automasking->settings.topology_use_brush_limit = is_constrained_by_radius(brush);
    fill_topology_automasking_factors(depsgraph, sd, ob, factors);
  }

  if (mode_enabled(sd, brush, BRUSH_AUTOMASKING_FACE_SETS)) {
    vert_random_access_ensure(ob);
    init_face_sets_masking(sd, ob, factors);
  }

  const int steps = boundary_propagation_steps(sd, brush);
  if (mode_enabled(sd, brush, BRUSH_AUTOMASKING_BOUNDARY_EDGES)) {
    vert_random_access_ensure(ob);
    init_boundary_masking(ob, depsgraph, BoundaryAutomaskMode::Edges, steps, factors);
  }
  if (mode_enabled(sd, brush, BRUSH_AUTOMASKING_BOUNDARY_FACE_SETS)) {
    vert_random_access_ensure(ob);
    init_boundary_masking(ob, depsgraph, BoundaryAutomaskMode::FaceSets, steps, factors);
  }

  /* Subtractive modes. */
  int normal_bits = calc_effective_bits(sd, brush) &
                    (BRUSH_AUTOMASKING_VIEW_NORMAL | BRUSH_AUTOMASKING_VIEW_OCCLUSION);

  if (normal_bits) {
    normal_occlusion_automasking_fill(
        depsgraph, *automasking, ob, (eAutomasking_flag)normal_bits, factors);
  }

  return automasking;
}

Cache &filter_cache_ensure(const Depsgraph &depsgraph, const Sculpt &sd, Object &ob)
{
  BLI_assert(is_enabled(sd, ob, nullptr));
  if (ob.sculpt->filter_cache->automasking) {
    return *ob.sculpt->filter_cache->automasking;
  }

  ob.sculpt->filter_cache->automasking = cache_init(depsgraph, sd, nullptr, ob);
  return *ob.sculpt->filter_cache->automasking;
}

Cache &stroke_cache_ensure(const Depsgraph &depsgraph,
                           const Sculpt &sd,
                           const Brush *brush,
                           Object &ob)
{
  BLI_assert(is_enabled(sd, ob, brush));
  if (ob.sculpt->cache->automasking) {
    return *ob.sculpt->cache->automasking;
  }

  ob.sculpt->cache->automasking = cache_init(depsgraph, sd, brush, ob);
  return *ob.sculpt->cache->automasking;
}

void Cache::calc_cavity_factor(const Depsgraph &depsgraph,
                               const Object &object,
                               const IndexMask &node_mask)
{
  if ((this->settings.flags & BRUSH_AUTOMASKING_CAVITY_ALL) == 0) {
    return;
  }

  BLI_assert(!this->cavity_factor.is_empty());

  const SculptSession &ss = *object.sculpt;
  const bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(object);
  switch (pbvh.type()) {
    case bke::pbvh::Type::Mesh: {
      const Span<bke::pbvh::MeshNode> nodes = pbvh.nodes<bke::pbvh::MeshNode>();
      node_mask.foreach_index(GrainSize(1), [&](const int i) {
        const Span<int> verts = nodes[i].verts();
        for (const int vert : verts) {
          calc_cavity_factor_mesh(depsgraph, *this, object, vert);
        }
      });
      break;
    }
    case bke::pbvh::Type::Grids: {
      const SubdivCCG &subdiv_ccg = *ss.subdiv_ccg;
      const CCGKey key = BKE_subdiv_ccg_key_top_level(subdiv_ccg);
      const Span<bke::pbvh::GridsNode> nodes = pbvh.nodes<bke::pbvh::GridsNode>();
      node_mask.foreach_index(GrainSize(1), [&](const int i) {
        const Span<int> grids = nodes[i].grids();
        for (const int grid : grids) {
          for (const int vert : bke::ccg::grid_range(subdiv_ccg.grid_area, grid)) {
            calc_cavity_factor_grids(key, *this, object, vert);
          }
        }
      });
      break;
    }
    case bke::pbvh::Type::BMesh: {
      const Span<bke::pbvh::BMeshNode> nodes = pbvh.nodes<bke::pbvh::BMeshNode>();
      node_mask.foreach_index(GrainSize(1), [&](const int i) {
        const Set<BMVert *, 0> verts = nodes[i].bm_unique_verts_;
        for (BMVert *vert : verts) {
          calc_cavity_factor_bmesh(*this, vert, BM_elem_index_get(vert));
        }
      });
    }
  }
}

}  // namespace blender::ed::sculpt_paint::auto_mask
