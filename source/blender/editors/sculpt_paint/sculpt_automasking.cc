/* SPDX-FileCopyrightText: 2020 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edsculpt
 */

#include "sculpt_automask.hh"

#include "MEM_guardedalloc.h"

#include "BLI_array.hh"
#include "BLI_hash.h"
#include "BLI_index_range.hh"
#include "BLI_math_base.hh"
#include "BLI_math_base_safe.h"
#include "BLI_math_vector.h"
#include "BLI_math_vector_types.hh"
#include "BLI_set.hh"
#include "BLI_vector.hh"

#include "DNA_brush_types.h"
#include "DNA_mesh_types.h"

#include "BKE_colortools.hh"
#include "BKE_paint.hh"
#include "BKE_pbvh_api.hh"
#include "BKE_subdiv_ccg.hh"

#include "mesh_brush_common.hh"
#include "paint_intern.hh"
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
#include <cstdlib>

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

bool needs_normal(const SculptSession & /*ss*/, const Sculpt &sd, const Brush *brush)
{
  int flags = calc_effective_bits(sd, brush);

  return flags & (BRUSH_AUTOMASKING_BRUSH_NORMAL | BRUSH_AUTOMASKING_VIEW_NORMAL);
}

static float normal_calc(const Depsgraph &depsgraph,
                         const Object &object,
                         PBVHVertRef vertex,
                         float3 &normal,
                         float limit_lower,
                         float limit_upper,
                         const std::optional<float3> &orig_normal)
{
  float3 normal_v;

  if (orig_normal) {
    normal_v = *orig_normal;
  }
  else {
    normal_v = SCULPT_vertex_normal_get(depsgraph, object, vertex);
  }

  float angle = safe_acosf(dot_v3v3(normal, normal_v));

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

  if (automasking_flags & BRUSH_AUTOMASKING_VIEW_NORMAL) {
    return brush && brush->automasking_boundary_edges_propagation_steps != 1;
  }

  if (automasking_flags &
      (BRUSH_AUTOMASKING_BOUNDARY_EDGES | BRUSH_AUTOMASKING_BOUNDARY_FACE_SETS))
  {
    return boundary_propagation_steps(sd, brush) != 1;
  }
  return false;
}

static float calc_brush_normal_factor(const Depsgraph &depsgraph,
                                      const Cache *automasking,
                                      const Object &object,
                                      PBVHVertRef vertex,
                                      const std::optional<float3> &orig_normal)
{
  const SculptSession &ss = *object.sculpt;
  float falloff = automasking->settings.start_normal_falloff * M_PI;
  float3 initial_normal;

  if (ss.cache) {
    initial_normal = ss.cache->initial_normal_symm;
  }
  else {
    initial_normal = ss.filter_cache->initial_normal;
  }

  return normal_calc(depsgraph,
                     object,
                     vertex,
                     initial_normal,
                     automasking->settings.start_normal_limit - falloff * 0.5f,
                     automasking->settings.start_normal_limit + falloff * 0.5f,
                     orig_normal);
}

static float calc_view_normal_factor(const Depsgraph &depsgraph,
                                     const Cache &automasking,
                                     const Object &object,
                                     PBVHVertRef vertex,
                                     const std::optional<float3> &orig_normal)
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

  return normal_calc(depsgraph,
                     object,
                     vertex,
                     view_normal,
                     automasking.settings.view_normal_limit,
                     automasking.settings.view_normal_limit + falloff,
                     orig_normal);
}

static float calc_view_occlusion_factor(const Depsgraph &depsgraph,
                                        const Cache &automasking,
                                        const Object &object,
                                        PBVHVertRef vertex,
                                        uchar stroke_id)
{
  SculptSession &ss = *object.sculpt;
  char f = *(char *)SCULPT_vertex_attr_get(vertex, ss.attrs.automasking_occlusion);

  if (stroke_id != automasking.current_stroke_id) {
    f = *(char *)SCULPT_vertex_attr_get(vertex, ss.attrs.automasking_occlusion) =
        SCULPT_vertex_is_occluded(object, SCULPT_vertex_co_get(depsgraph, object, vertex), true) ?
            2 :
            1;
  }

  return f == 2;
}

/* Updates vertex stroke id. */
static float automasking_factor_end(SculptSession &ss,
                                    const Cache *automasking,
                                    PBVHVertRef vertex,
                                    float value)
{
  if (ss.attrs.automasking_stroke_id) {
    *(uchar *)SCULPT_vertex_attr_get(
        vertex, ss.attrs.automasking_stroke_id) = automasking->current_stroke_id;
  }

  return value;
}

static float calc_cavity_factor(const Cache *automasking, float factor)
{
  float sign = signf(factor);

  factor = fabsf(factor) * automasking->settings.cavity_factor * 50.0f;

  factor = factor * sign * 0.5f + 0.5f;
  CLAMP(factor, 0.0f, 1.0f);

  return (automasking->settings.flags & BRUSH_AUTOMASKING_CAVITY_INVERTED) ? 1.0f - factor :
                                                                             factor;
}

struct AccumulatedVert {
  float3 position = float3(0.0f);
  float3 normal = float3(0.0f);
  float distance = 0.0f;
  int count = 0;
};

static void calc_blurred_cavity_mesh(const Depsgraph &depsgraph,
                                     const Object &object,
                                     const Cache *automasking,
                                     const int steps,
                                     const int vert)
{
  struct CavityBlurVert {
    int vertex;
    int depth;
  };

  Mesh &mesh = *static_cast<Mesh *>(object.data);

  const OffsetIndices faces = mesh.faces();
  const Span<int> corner_verts = mesh.corner_verts();

  const bke::AttributeAccessor attributes = mesh.attributes();
  const VArraySpan hide_poly = *attributes.lookup<bool>(".hide_poly", bke::AttrDomain::Face);

  const SculptSession &ss = *object.sculpt;

  Span<float3> positions_eval = bke::pbvh::vert_positions_eval(depsgraph, object);
  Span<float3> normals_eval = bke::pbvh::vert_normals_eval(depsgraph, object);

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

    const float3 blur_vert_position = positions_eval[current_vert];
    const float3 blur_vert_normal = normals_eval[current_vert];

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
             current_vert, faces, corner_verts, ss.vert_to_face_map, hide_poly, neighbors))
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
  float factor_sum = math::dot(vec, verts_in_range.normal) / all_verts.distance;
  *(float *)SCULPT_vertex_attr_get(vert, ss.attrs.automasking_cavity) = calc_cavity_factor(
      automasking, factor_sum);
}

static void calc_blurred_cavity_grids(const Object &object,
                                      const Cache *automasking,
                                      const int steps,
                                      const SubdivCCGCoord vert)
{
  struct CavityBlurVert {
    SubdivCCGCoord vertex;
    int index;
    int depth;
  };

  const SculptSession &ss = *object.sculpt;
  const SubdivCCG &subdiv_ccg = *ss.subdiv_ccg;
  const Span<CCGElem *> grids = subdiv_ccg.grids;
  const CCGKey key = BKE_subdiv_ccg_key_top_level(subdiv_ccg);

  AccumulatedVert all_verts;
  AccumulatedVert verts_in_range;
  /* Steps starts at 1, but API and user interface
   * are zero-based.
   */
  const int num_steps = steps + 1;

  std::queue<CavityBlurVert> queue;
  Set<int, 64> visited_verts;

  const CavityBlurVert initial{vert, vert.to_index(key), 0};
  visited_verts.add_new(initial.index);
  queue.push(initial);

  const float3 starting_position = CCG_grid_elem_co(key, grids[vert.grid_index], vert.x, vert.y);

  SubdivCCGNeighbors neighbors;
  while (!queue.empty()) {
    const CavityBlurVert blurvert = queue.front();
    queue.pop();

    const SubdivCCGCoord current_vert = blurvert.vertex;

    const float3 blur_vert_position = CCG_grid_elem_co(
        key, grids[current_vert.grid_index], current_vert.x, current_vert.y);
    const float3 blur_vert_normal = CCG_grid_elem_no(
        key, grids[current_vert.grid_index], current_vert.x, current_vert.y);

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

    BKE_subdiv_ccg_neighbor_coords_get(subdiv_ccg, current_vert, false, neighbors);
    for (const SubdivCCGCoord neighbor : neighbors.coords) {
      const int neighbor_idx = neighbor.to_index(key);
      if (visited_verts.contains(neighbor_idx)) {
        continue;
      }

      visited_verts.add_new(neighbor_idx);
      queue.push({neighbor, neighbor_idx, blurvert.depth + 1});
    }
  }

  BLI_assert(all_verts.count != verts_in_range.count);

  if (all_verts.count == 0) {
    all_verts.position = CCG_grid_elem_co(key, grids[vert.grid_index], vert.x, vert.y);
  }
  else {
    all_verts.position /= float(all_verts.count);
    all_verts.distance /= all_verts.count;
  }

  if (verts_in_range.count == 0) {
    verts_in_range.position = CCG_grid_elem_co(key, grids[vert.grid_index], vert.x, vert.y);
  }
  else {
    verts_in_range.position /= float(verts_in_range.count);
  }

  verts_in_range.normal = math::normalize(verts_in_range.normal);
  if (math::dot(verts_in_range.normal, verts_in_range.normal) == 0.0f) {
    verts_in_range.normal = CCG_grid_elem_no(key, grids[vert.grid_index], vert.x, vert.y);
  }

  const float3 vec = all_verts.position - verts_in_range.position;
  float factor_sum = math::dot(vec, verts_in_range.normal) / all_verts.distance;
  *(float *)SCULPT_vertex_attr_get(key, vert, ss.attrs.automasking_cavity) = calc_cavity_factor(
      automasking, factor_sum);
}

static void calc_blurred_cavity_bmesh(const Object &object,
                                      const Cache *automasking,
                                      const int steps,
                                      BMVert *vert)
{
  struct CavityBlurVert {
    BMVert *vertex;
    int index;
    int depth;
  };

  const SculptSession &ss = *object.sculpt;

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

  Vector<BMVert *, 64> neighbors;
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
  *(float *)SCULPT_vertex_attr_get(vert, ss.attrs.automasking_cavity) = calc_cavity_factor(
      automasking, factor_sum);
}

static void calc_blurred_cavity(const Depsgraph &depsgraph,
                                const Object &object,
                                const Cache *automasking,
                                const int steps,
                                const PBVHVertRef vertex)
{
  const SculptSession &ss = *object.sculpt;
  switch (ss.pbvh->type()) {
    case bke::pbvh::Type::Mesh:
      calc_blurred_cavity_mesh(depsgraph, object, automasking, steps, int(vertex.i));
      break;
    case bke::pbvh::Type::Grids: {
      const CCGKey key = BKE_subdiv_ccg_key_top_level(*ss.subdiv_ccg);
      calc_blurred_cavity_grids(
          object, automasking, steps, SubdivCCGCoord::from_index(key, int(vertex.i)));
      break;
    }
    case bke::pbvh::Type::BMesh:
      calc_blurred_cavity_bmesh(object, automasking, steps, reinterpret_cast<BMVert *>(vertex.i));
      break;
  }
}

int settings_hash(const Object &ob, const Cache &automasking)
{
  int hash;
  int totvert = SCULPT_vertex_count_get(ob);

  hash = BLI_hash_int(automasking.settings.flags);
  hash = BLI_hash_int_2d(hash, totvert);

  if (automasking.settings.flags & BRUSH_AUTOMASKING_CAVITY_ALL) {
    hash = BLI_hash_int_2d(hash, automasking.settings.cavity_blur_steps);
    hash = BLI_hash_int_2d(hash,
                           *reinterpret_cast<const uint *>(&automasking.settings.cavity_factor));

    if (automasking.settings.cavity_curve) {
      CurveMap *cm = automasking.settings.cavity_curve->cm;

      for (int i = 0; i < cm->totpoint; i++) {
        hash = BLI_hash_int_2d(hash, *reinterpret_cast<const uint *>(&cm->curve[i].x));
        hash = BLI_hash_int_2d(hash, *reinterpret_cast<const uint *>(&cm->curve[i].y));
        hash = BLI_hash_int_2d(hash, uint(cm->curve[i].flag));
        hash = BLI_hash_int_2d(hash, uint(cm->curve[i].shorty));
      }
    }
  }

  if (automasking.settings.flags & BRUSH_AUTOMASKING_FACE_SETS) {
    hash = BLI_hash_int_2d(hash, automasking.settings.initial_face_set);
  }

  if (automasking.settings.flags & BRUSH_AUTOMASKING_VIEW_NORMAL) {
    hash = BLI_hash_int_2d(
        hash, *reinterpret_cast<const uint *>(&automasking.settings.view_normal_falloff));
    hash = BLI_hash_int_2d(
        hash, *reinterpret_cast<const uint *>(&automasking.settings.view_normal_limit));
  }

  if (automasking.settings.flags & BRUSH_AUTOMASKING_BRUSH_NORMAL) {
    hash = BLI_hash_int_2d(
        hash, *reinterpret_cast<const uint *>(&automasking.settings.start_normal_falloff));
    hash = BLI_hash_int_2d(
        hash, *reinterpret_cast<const uint *>(&automasking.settings.start_normal_limit));
  }

  return hash;
}

static float calc_cavity_factor(const Depsgraph &depsgraph,
                                const Cache *automasking,
                                const Object &object,
                                PBVHVertRef vertex)
{
  SculptSession &ss = *object.sculpt;
  uchar stroke_id = *(const uchar *)SCULPT_vertex_attr_get(vertex, ss.attrs.automasking_stroke_id);

  if (stroke_id != automasking->current_stroke_id) {
    calc_blurred_cavity(
        depsgraph, object, automasking, automasking->settings.cavity_blur_steps, vertex);
  }

  float factor = *(const float *)SCULPT_vertex_attr_get(vertex, ss.attrs.automasking_cavity);
  bool inverted = automasking->settings.flags & BRUSH_AUTOMASKING_CAVITY_INVERTED;

  if ((automasking->settings.flags & BRUSH_AUTOMASKING_CAVITY_ALL) &&
      (automasking->settings.flags & BRUSH_AUTOMASKING_CAVITY_USE_CURVE))
  {
    factor = inverted ? 1.0f - factor : factor;
    factor = BKE_curvemapping_evaluateF(automasking->settings.cavity_curve, 0, factor);
    factor = inverted ? 1.0f - factor : factor;
  }

  return factor;
}

static float factor_get(const Depsgraph &depsgraph,
                        const Cache *automasking,
                        const Object &object,
                        PBVHVertRef vert,
                        const std::optional<float3> &orig_normal)
{
  SculptSession &ss = *object.sculpt;
  float mask = 1.0f;

  /* Since brush normal mode depends on the current mirror symmetry pass
   * it is not folded into the factor cache (when it exists). */
  if ((ss.cache || ss.filter_cache) &&
      (automasking->settings.flags & BRUSH_AUTOMASKING_BRUSH_NORMAL))
  {
    mask *= calc_brush_normal_factor(depsgraph, automasking, object, vert, orig_normal);
  }

  /* If the cache is initialized with valid info, use the cache. This is used when the
   * automasking information can't be computed in real time per vertex and needs to be
   * initialized for the whole mesh when the stroke starts. */
  if (ss.attrs.automasking_factor) {
    float factor = *(const float *)SCULPT_vertex_attr_get(vert, ss.attrs.automasking_factor);

    if (automasking->settings.flags & BRUSH_AUTOMASKING_CAVITY_ALL) {
      factor *= calc_cavity_factor(depsgraph, automasking, object, vert);
    }

    return automasking_factor_end(ss, automasking, vert, factor * mask);
  }

  uchar stroke_id = ss.attrs.automasking_stroke_id ?
                        *(uchar *)SCULPT_vertex_attr_get(vert, ss.attrs.automasking_stroke_id) :
                        -1;

  bool do_occlusion = (automasking->settings.flags &
                       (BRUSH_AUTOMASKING_VIEW_OCCLUSION | BRUSH_AUTOMASKING_VIEW_NORMAL)) ==
                      (BRUSH_AUTOMASKING_VIEW_OCCLUSION | BRUSH_AUTOMASKING_VIEW_NORMAL);
  if (do_occlusion && calc_view_occlusion_factor(depsgraph, *automasking, object, vert, stroke_id))
  {
    return automasking_factor_end(ss, automasking, vert, 0.0f);
  }

  if (!automasking->settings.topology_use_brush_limit &&
      automasking->settings.flags & BRUSH_AUTOMASKING_TOPOLOGY &&
      islands::vert_id_get(ss, BKE_pbvh_vertex_to_index(*bke::object::pbvh_get(object), vert)) !=
          automasking->settings.initial_island_nr)
  {
    return 0.0f;
  }

  if (automasking->settings.flags & BRUSH_AUTOMASKING_FACE_SETS) {
    if (!face_set::vert_has_face_set(object, vert, automasking->settings.initial_face_set)) {
      return 0.0f;
    }
  }

  if (automasking->settings.flags & BRUSH_AUTOMASKING_BOUNDARY_EDGES) {
    if (boundary::vert_is_boundary(object, vert)) {
      return 0.0f;
    }
  }

  if (automasking->settings.flags & BRUSH_AUTOMASKING_BOUNDARY_FACE_SETS) {
    bool ignore = ss.cache && ss.cache->brush &&
                  ss.cache->brush->sculpt_brush_type == SCULPT_BRUSH_TYPE_DRAW_FACE_SETS &&
                  face_set::vert_face_set_get(object, vert) == ss.cache->paint_face_set;

    if (!ignore && !face_set::vert_has_unique_face_set(object, vert)) {
      return 0.0f;
    }
  }

  if ((ss.cache || ss.filter_cache) &&
      (automasking->settings.flags & BRUSH_AUTOMASKING_VIEW_NORMAL))
  {
    mask *= calc_view_normal_factor(depsgraph, *automasking, object, vert, orig_normal);
  }

  if (automasking->settings.flags & BRUSH_AUTOMASKING_CAVITY_ALL) {
    mask *= calc_cavity_factor(depsgraph, automasking, object, vert);
  }

  return automasking_factor_end(ss, automasking, vert, mask);
}

void calc_vert_factors(const Depsgraph &depsgraph,
                       const Object &object,
                       const Cache &cache,
                       const bke::pbvh::MeshNode &node,
                       const Span<int> verts,
                       const MutableSpan<float> factors)
{
  Span<float3> orig_normals;
  if (cache.settings.flags & (BRUSH_AUTOMASKING_BRUSH_NORMAL | BRUSH_AUTOMASKING_VIEW_NORMAL)) {
    if (std::optional<OrigPositionData> orig_data = orig_position_data_lookup_mesh(object, node)) {
      orig_normals = orig_data->normals;
    }
  }

  for (const int i : verts.index_range()) {
    factors[i] *= factor_get(depsgraph,
                             &cache,
                             object,
                             BKE_pbvh_make_vref(verts[i]),
                             orig_normals.is_empty() ? std::nullopt :
                                                       std::make_optional(orig_normals[i]));
  }
}

void calc_face_factors(const Depsgraph &depsgraph,
                       const Object &object,
                       const OffsetIndices<int> faces,
                       const Span<int> corner_verts,
                       const Cache &cache,
                       const bke::pbvh::MeshNode & /*node*/,
                       const Span<int> face_indices,
                       const MutableSpan<float> factors)
{
  for (const int i : face_indices.index_range()) {
    const Span<int> face_verts = corner_verts.slice(faces[face_indices[i]]);
    float sum = 0.0f;
    for (const int vert : face_verts) {
      sum += factor_get(depsgraph, &cache, object, BKE_pbvh_make_vref(vert), std::nullopt);
    }
    factors[i] *= sum * math::rcp(float(face_verts.size()));
  }
}

void calc_grids_factors(const Depsgraph &depsgraph,
                        const Object &object,
                        const Cache &cache,
                        const bke::pbvh::GridsNode &node,
                        const Span<int> grids,
                        const MutableSpan<float> factors)
{
  SculptSession &ss = *object.sculpt;
  const SubdivCCG &subdiv_ccg = *ss.subdiv_ccg;
  const CCGKey key = BKE_subdiv_ccg_key_top_level(subdiv_ccg);

  Span<float3> orig_normals;
  if (cache.settings.flags & (BRUSH_AUTOMASKING_BRUSH_NORMAL | BRUSH_AUTOMASKING_VIEW_NORMAL)) {
    if (std::optional<OrigPositionData> orig_data = orig_position_data_lookup_grids(object, node))
    {
      orig_normals = orig_data->normals;
    }
  }

  for (const int i : grids.index_range()) {
    const int node_start = i * key.grid_area;
    const int grids_start = grids[i] * key.grid_area;
    for (const int offset : IndexRange(key.grid_area)) {
      factors[node_start + offset] *= factor_get(
          depsgraph,
          &cache,
          object,
          BKE_pbvh_make_vref(grids_start + offset),
          orig_normals.is_empty() ? std::nullopt :
                                    std::make_optional(orig_normals[node_start + offset]));
    }
  }
}

void calc_vert_factors(const Depsgraph &depsgraph,
                       const Object &object,
                       const Cache &cache,
                       const bke::pbvh::BMeshNode & /*node*/,
                       const Set<BMVert *, 0> &verts,
                       const MutableSpan<float> factors)
{
  SculptSession &ss = *object.sculpt;

  Array<float3> orig_normals;
  if (cache.settings.flags & (BRUSH_AUTOMASKING_BRUSH_NORMAL | BRUSH_AUTOMASKING_VIEW_NORMAL)) {
    orig_position_data_gather_bmesh(*ss.bm_log, verts, {}, orig_normals);
  }

  int i = 0;
  for (BMVert *vert : verts) {
    factors[i] *= factor_get(depsgraph,
                             &cache,
                             object,
                             BKE_pbvh_make_vref(intptr_t(vert)),
                             orig_normals.is_empty() ? std::nullopt :
                                                       std::make_optional(orig_normals[i]));
    i++;
  }
}

static void fill_topology_automasking_factors_mesh(const Depsgraph &depsgraph,
                                                   const Sculpt &sd,
                                                   Object &ob,
                                                   const Span<float3> vert_positions)
{
  SculptSession &ss = *ob.sculpt;
  const Brush *brush = BKE_paint_brush_for_read(&sd.paint);

  const float radius = ss.cache ? ss.cache->radius : std::numeric_limits<float>::max();
  const int active_vert = std::get<int>(ss.active_vert());
  flood_fill::FillDataMesh flood = flood_fill::FillDataMesh(vert_positions.size());

  flood.add_initial_with_symmetry(depsgraph, ob, *bke::object::pbvh_get(ob), active_vert, radius);

  const bool use_radius = ss.cache && is_constrained_by_radius(brush);
  const ePaintSymmetryFlags symm = SCULPT_mesh_symmetry_xyz_get(ob);

  float3 location = vert_positions[active_vert];

  flood.execute(ob, ss.vert_to_face_map, [&](int from_v, int to_v) {
    *(float *)SCULPT_vertex_attr_get(to_v, ss.attrs.automasking_factor) = 1.0f;
    *(float *)SCULPT_vertex_attr_get(from_v, ss.attrs.automasking_factor) = 1.0f;
    return (use_radius || SCULPT_is_vertex_inside_brush_radius_symm(
                              vert_positions[to_v], location, radius, symm));
  });
}

static void fill_topology_automasking_factors_grids(const Sculpt &sd,
                                                    Object &ob,
                                                    const SubdivCCG &subdiv_ccg)

{
  SculptSession &ss = *ob.sculpt;
  const Brush *brush = BKE_paint_brush_for_read(&sd.paint);

  const float radius = ss.cache ? ss.cache->radius : std::numeric_limits<float>::max();
  const SubdivCCGCoord active_vert = std::get<SubdivCCGCoord>(ss.active_vert());

  const Span<CCGElem *> grids = subdiv_ccg.grids;
  const CCGKey key = BKE_subdiv_ccg_key_top_level(subdiv_ccg);
  const int grid_verts_num = subdiv_ccg.grids.size() * key.grid_area;

  flood_fill::FillDataGrids flood = flood_fill::FillDataGrids(grid_verts_num);

  flood.add_initial_with_symmetry(ob, *bke::object::pbvh_get(ob), subdiv_ccg, active_vert, radius);

  const bool use_radius = ss.cache && is_constrained_by_radius(brush);
  const ePaintSymmetryFlags symm = SCULPT_mesh_symmetry_xyz_get(ob);

  float3 location = CCG_grid_elem_co(
      key, grids[active_vert.grid_index], active_vert.x, active_vert.y);

  flood.execute(
      ob, subdiv_ccg, [&](SubdivCCGCoord from_v, SubdivCCGCoord to_v, bool /*is_duplicate*/) {
        *(float *)SCULPT_vertex_attr_get(key, to_v, ss.attrs.automasking_factor) = 1.0f;
        *(float *)SCULPT_vertex_attr_get(key, from_v, ss.attrs.automasking_factor) = 1.0f;
        return (use_radius || SCULPT_is_vertex_inside_brush_radius_symm(
                                  CCG_grid_elem_co(key, grids[to_v.grid_index], to_v.x, to_v.y),
                                  location,
                                  radius,
                                  symm));
      });
}

static void fill_topology_automasking_factors_bmesh(const Sculpt &sd, Object &ob, BMesh &bm)
{
  SculptSession &ss = *ob.sculpt;
  const Brush *brush = BKE_paint_brush_for_read(&sd.paint);

  const float radius = ss.cache ? ss.cache->radius : std::numeric_limits<float>::max();
  BMVert *active_vert = std::get<BMVert *>(ss.active_vert());
  const int num_verts = BM_mesh_elem_count(&bm, BM_VERT);
  flood_fill::FillDataBMesh flood = flood_fill::FillDataBMesh(num_verts);

  flood.add_initial_with_symmetry(ob, *bke::object::pbvh_get(ob), active_vert, radius);

  const bool use_radius = ss.cache && is_constrained_by_radius(brush);
  const ePaintSymmetryFlags symm = SCULPT_mesh_symmetry_xyz_get(ob);

  float3 location = active_vert->co;

  flood.execute(ob, [&](BMVert *from_v, BMVert *to_v) {
    *(float *)SCULPT_vertex_attr_get(to_v, ss.attrs.automasking_factor) = 1.0f;
    *(float *)SCULPT_vertex_attr_get(from_v, ss.attrs.automasking_factor) = 1.0f;
    return (use_radius ||
            SCULPT_is_vertex_inside_brush_radius_symm(active_vert->co, location, radius, symm));
  });
}

static void fill_topology_automasking_factors(const Depsgraph &depsgraph,
                                              const Sculpt &sd,
                                              Object &ob)
{
  /* TODO: This method is to be removed when more of the automasking code handles the different
   * pbvh types. */
  SculptSession &ss = *ob.sculpt;

  switch (bke::object::pbvh_get(ob)->type()) {
    case bke::pbvh::Type::Mesh:
      fill_topology_automasking_factors_mesh(
          depsgraph, sd, ob, bke::pbvh::vert_positions_eval(depsgraph, ob));
      break;
    case bke::pbvh::Type::Grids:
      fill_topology_automasking_factors_grids(sd, ob, *ss.subdiv_ccg);
      break;
    case bke::pbvh::Type::BMesh:
      fill_topology_automasking_factors_bmesh(sd, ob, *ss.bm);
      break;
  }
}

static void init_face_sets_masking(const Sculpt &sd, Object &ob)
{
  SculptSession &ss = *ob.sculpt;
  const Brush *brush = BKE_paint_brush_for_read(&sd.paint);

  if (!is_enabled(sd, ob, brush)) {
    return;
  }

  int tot_vert = SCULPT_vertex_count_get(ob);
  int active_face_set = face_set::active_face_set_get(ob);
  for (int i : IndexRange(tot_vert)) {
    PBVHVertRef vertex = BKE_pbvh_index_to_vertex(ob, i);

    if (!face_set::vert_has_face_set(ob, vertex, active_face_set)) {
      *(float *)SCULPT_vertex_attr_get(vertex, ss.attrs.automasking_factor) = 0.0f;
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
                                       const int propagation_steps)
{
  SculptSession &ss = *object.sculpt;
  Mesh &mesh = *static_cast<Mesh *>(object.data);

  const OffsetIndices faces = mesh.faces();
  const Span<int> corner_verts = mesh.corner_verts();

  const bke::AttributeAccessor attributes = mesh.attributes();
  const VArraySpan hide_poly = *attributes.lookup<bool>(".hide_poly", bke::AttrDomain::Face);

  const int num_verts = bke::pbvh::vert_positions_eval(depsgraph, object).size();
  Array<int> edge_distance(num_verts, EDGE_DISTANCE_INF);

  for (const int i : IndexRange(num_verts)) {
    switch (mode) {
      case BoundaryAutomaskMode::Edges:
        if (boundary::vert_is_boundary(hide_poly, ss.vert_to_face_map, ss.vertex_info.boundary, i))
        {
          edge_distance[i] = 0;
        }
        break;
      case BoundaryAutomaskMode::FaceSets:
        if (!face_set::vert_has_unique_face_set(ss.vert_to_face_map, ss.face_sets, i)) {
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

      for (const int neighbor : vert_neighbors_get_mesh(
               i, faces, corner_verts, ss.vert_to_face_map, hide_poly, neighbors))
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

    *(float *)SCULPT_vertex_attr_get(i, ss.attrs.automasking_factor) *= (1.0f -
                                                                         edge_boundary_automask);
  }
}

static void init_boundary_masking_grids(Object &object,
                                        const BoundaryAutomaskMode mode,
                                        const int propagation_steps)
{
  SculptSession &ss = *object.sculpt;
  const SubdivCCG &subdiv_ccg = *ss.subdiv_ccg;
  Mesh &mesh = *static_cast<Mesh *>(object.data);

  const Span<CCGElem *> grids = subdiv_ccg.grids;
  const CCGKey key = BKE_subdiv_ccg_key_top_level(subdiv_ccg);

  const OffsetIndices faces = mesh.faces();
  const Span<int> corner_verts = mesh.corner_verts();

  const int num_grids = key.grid_area * grids.size();
  Array<int> edge_distance(num_grids, EDGE_DISTANCE_INF);
  for (const int i : IndexRange(num_grids)) {
    const SubdivCCGCoord coord = SubdivCCGCoord::from_index(key, i);
    switch (mode) {
      case BoundaryAutomaskMode::Edges:
        if (boundary::vert_is_boundary(
                subdiv_ccg, corner_verts, faces, ss.vertex_info.boundary, coord))
        {
          edge_distance[i] = 0;
        }
        break;
      case BoundaryAutomaskMode::FaceSets:
        if (!face_set::vert_has_unique_face_set(
                ss.vert_to_face_map, corner_verts, faces, ss.face_sets, subdiv_ccg, coord))
        {
          edge_distance[i] = 0;
        }
        break;
    }
  }

  SubdivCCGNeighbors neighbors;
  for (const int propagation_it : IndexRange(propagation_steps)) {
    for (const int i : IndexRange(num_grids)) {
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

  for (const int i : IndexRange(num_grids)) {
    if (edge_distance[i] == EDGE_DISTANCE_INF) {
      continue;
    }

    const SubdivCCGCoord coord = SubdivCCGCoord::from_index(key, i);

    const float p = 1.0f - (float(edge_distance[i]) / float(propagation_steps));
    const float edge_boundary_automask = pow2f(p);

    *(float *)SCULPT_vertex_attr_get(
        key, coord, ss.attrs.automasking_factor) *= (1.0f - edge_boundary_automask);
  }
}

static void init_boundary_masking_bmesh(Object &object,
                                        const BoundaryAutomaskMode mode,
                                        const int propagation_steps)
{
  SculptSession &ss = *object.sculpt;
  BMesh *bm = ss.bm;
  const int num_verts = BM_mesh_elem_count(bm, BM_VERT);

  Array<int> edge_distance(num_verts, EDGE_DISTANCE_INF);

  for (const int i : IndexRange(num_verts)) {
    BMVert *vert = BM_vert_at_index(bm, i);
    switch (mode) {
      case BoundaryAutomaskMode::Edges:
        if (boundary::vert_is_boundary(vert)) {
          edge_distance[i] = 0;
        }
        break;
      case BoundaryAutomaskMode::FaceSets:
        if (!face_set::vert_has_unique_face_set(vert)) {
          edge_distance[i] = 0;
        }
    }
  }

  Vector<BMVert *, 64> neighbors;
  for (const int propagation_it : IndexRange(propagation_steps)) {
    for (const int i : IndexRange(num_verts)) {
      if (edge_distance[i] != EDGE_DISTANCE_INF) {
        continue;
      }

      BMVert *vert = BM_vert_at_index(bm, i);
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

    BMVert *vert = BM_vert_at_index(bm, i);

    const float p = 1.0f - (float(edge_distance[i]) / float(propagation_steps));
    const float edge_boundary_automask = pow2f(p);

    *(float *)SCULPT_vertex_attr_get(
        vert, ss.attrs.automasking_factor) *= (1.0f - edge_boundary_automask);
  }
}

static void init_boundary_masking(Object &object,
                                  const Depsgraph &depsgraph,
                                  const BoundaryAutomaskMode mode,
                                  const int propagation_steps)
{
  switch (bke::object::pbvh_get(object)->type()) {
    case bke::pbvh::Type::Mesh:
      init_boundary_masking_mesh(object, depsgraph, mode, propagation_steps);
      break;
    case bke::pbvh::Type::Grids:
      init_boundary_masking_grids(object, mode, propagation_steps);
      break;
    case bke::pbvh::Type::BMesh:
      init_boundary_masking_bmesh(object, mode, propagation_steps);
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
                                              eAutomasking_flag mode)
{
  SculptSession &ss = *ob.sculpt;
  const int totvert = SCULPT_vertex_count_get(ob);

  /* No need to build original data since this is only called at the beginning of strokes. */
  for (int i = 0; i < totvert; i++) {
    PBVHVertRef vertex = BKE_pbvh_index_to_vertex(ob, i);

    float f = *(float *)SCULPT_vertex_attr_get(vertex, ss.attrs.automasking_factor);

    if (int(mode) & BRUSH_AUTOMASKING_VIEW_NORMAL) {
      if (int(mode) & BRUSH_AUTOMASKING_VIEW_OCCLUSION) {
        f *= calc_view_occlusion_factor(depsgraph, automasking, ob, vertex, -1);
      }

      f *= calc_view_normal_factor(depsgraph, automasking, ob, vertex, {});
    }

    if (ss.attrs.automasking_stroke_id) {
      *(uchar *)SCULPT_vertex_attr_get(vertex, ss.attrs.automasking_stroke_id) = ss.stroke_id;
    }

    *(float *)SCULPT_vertex_attr_get(vertex, ss.attrs.automasking_factor) = f;
  }
}

bool brush_type_can_reuse_automask(int sculpt_brush_type)
{
  return ELEM(sculpt_brush_type,
              SCULPT_BRUSH_TYPE_PAINT,
              SCULPT_BRUSH_TYPE_SMEAR,
              SCULPT_BRUSH_TYPE_MASK,
              SCULPT_BRUSH_TYPE_DRAW_FACE_SETS);
}

std::unique_ptr<Cache> cache_init(const Depsgraph &depsgraph, const Sculpt &sd, Object &ob)
{
  return cache_init(depsgraph, sd, nullptr, ob);
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

  automasking->current_stroke_id = ss.stroke_id;

  int mode = calc_effective_bits(sd, brush);

  SCULPT_vertex_random_access_ensure(ob);
  if (mode & BRUSH_AUTOMASKING_TOPOLOGY && ss.active_vert_index() != -1) {
    islands::ensure_cache(ob);
    automasking->settings.initial_island_nr = islands::vert_id_get(ss, ss.active_vert_index());
  }

  bool use_stroke_id = false;
  if ((mode & BRUSH_AUTOMASKING_VIEW_OCCLUSION) && (mode & BRUSH_AUTOMASKING_VIEW_NORMAL)) {
    use_stroke_id = true;

    if (!ss.attrs.automasking_occlusion) {
      SculptAttributeParams params = {0};
      ss.attrs.automasking_occlusion = BKE_sculpt_attribute_ensure(
          &ob,
          bke::AttrDomain::Point,
          CD_PROP_INT8,
          SCULPT_ATTRIBUTE_NAME(automasking_occlusion),
          &params);
    }
  }

  if (mode & BRUSH_AUTOMASKING_CAVITY_ALL) {
    use_stroke_id = true;

    if (mode_enabled(sd, brush, BRUSH_AUTOMASKING_CAVITY_USE_CURVE)) {
      if (brush) {
        BKE_curvemapping_init(brush->automasking_cavity_curve);
      }

      BKE_curvemapping_init(sd.automasking_cavity_curve);
    }

    if (!ss.attrs.automasking_cavity) {
      SculptAttributeParams params = {0};
      ss.attrs.automasking_cavity = BKE_sculpt_attribute_ensure(
          &ob,
          bke::AttrDomain::Point,
          CD_PROP_FLOAT,
          SCULPT_ATTRIBUTE_NAME(automasking_cavity),
          &params);
    }
  }

  if (use_stroke_id) {
    SCULPT_stroke_id_ensure(ob);

    bool have_occlusion = (mode & BRUSH_AUTOMASKING_VIEW_OCCLUSION) &&
                          (mode & BRUSH_AUTOMASKING_VIEW_NORMAL);

    if (brush && auto_mask::brush_type_can_reuse_automask(brush->sculpt_brush_type) &&
        !have_occlusion)
    {
      int hash = settings_hash(ob, *automasking);

      if (hash == ss.last_automasking_settings_hash) {
        automasking->current_stroke_id = ss.last_automask_stroke_id;
        automasking->can_reuse_mask = true;
      }
    }

    if (!automasking->can_reuse_mask) {
      ss.last_automask_stroke_id = ss.stroke_id;
    }
  }

  /* Avoid precomputing data on the vertex level if the current auto-masking modes do not require
   * it to function. */
  if (!needs_factors_cache(sd, brush)) {
    if (ss.attrs.automasking_factor) {
      BKE_sculpt_attribute_destroy(&ob, ss.attrs.automasking_factor);
    }
    return automasking;
  }

  SculptAttributeParams params = {0};
  params.stroke_only = true;

  ss.attrs.automasking_factor = BKE_sculpt_attribute_ensure(
      &ob,
      bke::AttrDomain::Point,
      CD_PROP_FLOAT,
      SCULPT_ATTRIBUTE_NAME(automasking_factor),
      &params);

  /* Topology builds up the mask from zero which other modes can subtract from.
   * If it isn't enabled, initialize to 1. */
  const float initial_value = !(mode & BRUSH_AUTOMASKING_TOPOLOGY) ? 1.0f : 0.0f;

  const int totvert = SCULPT_vertex_count_get(ob);
  for (int i : IndexRange(totvert)) {
    PBVHVertRef vertex = BKE_pbvh_index_to_vertex(ob, i);

    (*(float *)SCULPT_vertex_attr_get(vertex, ss.attrs.automasking_factor)) = initial_value;
  }

  /* Additive modes. */
  if (mode_enabled(sd, brush, BRUSH_AUTOMASKING_TOPOLOGY)) {
    SCULPT_vertex_random_access_ensure(ob);

    automasking->settings.topology_use_brush_limit = is_constrained_by_radius(brush);
    fill_topology_automasking_factors(depsgraph, sd, ob);
  }

  if (mode_enabled(sd, brush, BRUSH_AUTOMASKING_FACE_SETS)) {
    SCULPT_vertex_random_access_ensure(ob);
    init_face_sets_masking(sd, ob);
  }

  const int steps = boundary_propagation_steps(sd, brush);
  if (mode_enabled(sd, brush, BRUSH_AUTOMASKING_BOUNDARY_EDGES)) {
    SCULPT_vertex_random_access_ensure(ob);
    init_boundary_masking(ob, depsgraph, BoundaryAutomaskMode::Edges, steps);
  }
  if (mode_enabled(sd, brush, BRUSH_AUTOMASKING_BOUNDARY_FACE_SETS)) {
    SCULPT_vertex_random_access_ensure(ob);
    init_boundary_masking(ob, depsgraph, BoundaryAutomaskMode::FaceSets, steps);
  }

  /* Subtractive modes. */
  int normal_bits = calc_effective_bits(sd, brush) &
                    (BRUSH_AUTOMASKING_VIEW_NORMAL | BRUSH_AUTOMASKING_VIEW_OCCLUSION);

  if (normal_bits) {
    normal_occlusion_automasking_fill(depsgraph, *automasking, ob, (eAutomasking_flag)normal_bits);
  }

  return automasking;
}

}  // namespace blender::ed::sculpt_paint::auto_mask
