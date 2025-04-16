/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "editors/sculpt_paint/brushes/brushes.hh"

#include "DNA_brush_types.h"
#include "DNA_mesh_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BKE_kelvinlet.h"
#include "BKE_mesh.hh"
#include "BKE_paint.hh"
#include "BKE_paint_bvh.hh"
#include "BKE_subdiv_ccg.hh"

#include "BLI_enumerable_thread_specific.hh"
#include "BLI_math_quaternion.hh"
#include "BLI_math_vector.h"
#include "BLI_math_vector.hh"
#include "BLI_task.hh"

#include "editors/sculpt_paint/mesh_brush_common.hh"
#include "editors/sculpt_paint/sculpt_automask.hh"
#include "editors/sculpt_paint/sculpt_intern.hh"

#include "bmesh.hh"

namespace blender::ed::sculpt_paint::brushes {

inline namespace snake_hook_cc {

struct LocalData {
  Vector<float3> positions;
  Vector<float> factors;
  Vector<float> distances;
  Vector<float3> translations;
};

struct SculptProjectVector {
  float3 plane;
  float len_sq;
  float len_sq_inv_neg;
  bool is_valid;
};

/**
 * \param plane: Direction, can be any length.
 */
static void sculpt_project_v3_cache_init(SculptProjectVector *spvc, const float3 &plane)
{
  spvc->plane = plane;
  spvc->len_sq = math::length_squared(spvc->plane);
  spvc->is_valid = (spvc->len_sq > FLT_EPSILON);
  spvc->len_sq_inv_neg = (spvc->is_valid) ? -1.0f / spvc->len_sq : 0.0f;
}

/**
 * Calculate the projection.
 */
static void sculpt_project_v3(const SculptProjectVector *spvc, const float3 &vec, float3 &r_vec)
{
#if 0
  project_plane_v3_v3v3(r_vec, vec, spvc->plane);
#else
  /* inline the projection, cache `-1.0 / dot_v3_v3(v_proj, v_proj)` */
  r_vec += spvc->plane * math::dot(vec, spvc->plane) * spvc->len_sq_inv_neg;
#endif
}

static float3 sculpt_rake_rotate(const StrokeCache &cache,
                                 const float3 &sculpt_co,
                                 const float3 &v_co,
                                 float factor)
{
  float3 vec_rot = v_co - sculpt_co;
  const math::Quaternion rotation = math::pow(*cache.rake_rotation_symm, factor);
  vec_rot = math::transform_point(rotation, vec_rot);

  vec_rot += sculpt_co;
  return vec_rot - v_co;
}

BLI_NOINLINE static void calc_pinch_influence(const Brush &brush,
                                              const StrokeCache &cache,
                                              const float3 &grab_delta,
                                              const SculptProjectVector *spvc,
                                              const Span<float3> positions,
                                              const Span<float> factors,
                                              const MutableSpan<float3> translations)
{
  if (brush.crease_pinch_factor == 0.5f) {
    return;
  }

  const float pinch = 2.0f * (0.5f - brush.crease_pinch_factor) * math::length(grab_delta) /
                      cache.radius;

  for (const int i : positions.index_range()) {
    /* Negative pinch will inflate, helps maintain volume. */
    float3 delta_pinch = positions[i] - cache.location_symm;

    if (brush.falloff_shape == PAINT_FALLOFF_SHAPE_TUBE) {
      project_plane_v3_v3v3(delta_pinch, delta_pinch, cache.view_normal);
    }

    /* Important to calculate based on the grabbed location
     * (intentionally ignore fade here). */
    delta_pinch += grab_delta;

    sculpt_project_v3(spvc, delta_pinch, delta_pinch);

    float3 delta_pinch_init = delta_pinch;

    float pinch_fade = pinch * factors[i];
    /* When reducing, scale reduction back by how close to the center we are,
     * so we don't pinch into nothingness. */
    if (pinch > 0.0f) {
      /* Square to have even less impact for close vertices. */
      pinch_fade *= pow2f(std::min(1.0f, math::length(delta_pinch) / cache.radius));
    }
    delta_pinch *= (1.0f + pinch_fade);
    delta_pinch = delta_pinch_init - delta_pinch;
    translations[i] += delta_pinch;
  }
}

BLI_NOINLINE static void calc_rake_rotation_influence(const StrokeCache &cache,
                                                      const Span<float3> positions,
                                                      const Span<float> factors,
                                                      const MutableSpan<float3> translations)
{
  if (!cache.rake_rotation_symm) {
    return;
  }
  for (const int i : positions.index_range()) {
    translations[i] += sculpt_rake_rotate(cache, cache.location_symm, positions[i], factors[i]);
  }
}

BLI_NOINLINE static void calc_kelvinet_translation(const StrokeCache &cache,
                                                   const Span<float3> positions,
                                                   const Span<float> factors,
                                                   const MutableSpan<float3> translations)
{
  KelvinletParams params;
  BKE_kelvinlet_init_params(&params, cache.radius, cache.bstrength, 1.0f, 0.4f);
  for (const int i : positions.index_range()) {
    float3 disp;
    BKE_kelvinlet_grab_triscale(disp, &params, positions[i], cache.location_symm, translations[i]);
    translations[i] = disp * factors[i];
  }
}

static void calc_faces(const Depsgraph &depsgraph,
                       const Sculpt &sd,
                       Object &object,
                       const Brush &brush,
                       const SculptProjectVector *spvc,
                       const float3 &grab_delta,
                       const Span<float3> vert_normals,
                       const MeshAttributeData &attribute_data,
                       bke::pbvh::MeshNode &node,
                       LocalData &tls,
                       const PositionDeformData &position_data)
{
  SculptSession &ss = *object.sculpt;
  const StrokeCache &cache = *ss.cache;
  const bool do_elastic = brush.snake_hook_deform_type == BRUSH_SNAKE_HOOK_DEFORM_ELASTIC;

  const Span<int> verts = node.verts();
  const MutableSpan positions = gather_data_mesh(position_data.eval, verts, tls.positions);

  tls.factors.resize(verts.size());
  const MutableSpan<float> factors = tls.factors;

  if (do_elastic) {
    factors.fill(1.0f);
  }
  else {
    fill_factor_from_hide_and_mask(attribute_data.hide_vert, attribute_data.mask, verts, factors);
    filter_region_clip_factors(ss, positions, factors);
    if (brush.flag & BRUSH_FRONTFACE) {
      calc_front_face(cache.view_normal_symm, vert_normals, verts, factors);
    }

    tls.distances.resize(verts.size());
    const MutableSpan<float> distances = tls.distances;
    calc_brush_distances(ss, positions, eBrushFalloffShape(brush.falloff_shape), distances);
    filter_distances_with_radius(cache.radius, distances, factors);
    apply_hardness_to_distances(cache, distances);
    calc_brush_strength_factors(cache, brush, distances, factors);

    auto_mask::calc_vert_factors(depsgraph, object, cache.automasking.get(), node, verts, factors);
    calc_brush_texture_factors(ss, brush, positions, factors);
    scale_factors(factors, cache.bstrength);
  }

  tls.translations.resize(verts.size());
  const MutableSpan<float3> translations = tls.translations;

  translations_from_offset_and_factors(grab_delta, factors, translations);

  calc_pinch_influence(brush, cache, grab_delta, spvc, positions, factors, translations);

  calc_rake_rotation_influence(cache, positions, factors, translations);

  if (do_elastic) {
    fill_factor_from_hide_and_mask(attribute_data.hide_vert, attribute_data.mask, verts, factors);
    scale_factors(factors, cache.bstrength * 20.0f);
    auto_mask::calc_vert_factors(depsgraph, object, cache.automasking.get(), node, verts, factors);

    calc_kelvinet_translation(cache, positions, factors, translations);
  }

  clip_and_lock_translations(sd, ss, position_data.eval, verts, translations);
  position_data.deform(translations, verts);
}

static void calc_grids(const Depsgraph &depsgraph,
                       const Sculpt &sd,
                       Object &object,
                       const Brush &brush,
                       SculptProjectVector *spvc,
                       const float3 &grab_delta,
                       bke::pbvh::GridsNode &node,
                       LocalData &tls)
{
  SculptSession &ss = *object.sculpt;
  const StrokeCache &cache = *ss.cache;
  const bool do_elastic = brush.snake_hook_deform_type == BRUSH_SNAKE_HOOK_DEFORM_ELASTIC;
  SubdivCCG &subdiv_ccg = *ss.subdiv_ccg;

  const Span<int> grids = node.grids();
  const MutableSpan positions = gather_grids_positions(subdiv_ccg, grids, tls.positions);

  tls.factors.resize(positions.size());
  const MutableSpan<float> factors = tls.factors;

  if (do_elastic) {
    factors.fill(1.0f);
  }
  else {
    fill_factor_from_hide_and_mask(subdiv_ccg, grids, factors);
    filter_region_clip_factors(ss, positions, factors);
    if (brush.flag & BRUSH_FRONTFACE) {
      calc_front_face(cache.view_normal_symm, subdiv_ccg, grids, factors);
    }

    tls.distances.resize(positions.size());
    const MutableSpan<float> distances = tls.distances;
    calc_brush_distances(ss, positions, eBrushFalloffShape(brush.falloff_shape), distances);
    filter_distances_with_radius(cache.radius, distances, factors);
    apply_hardness_to_distances(cache, distances);
    calc_brush_strength_factors(cache, brush, distances, factors);

    auto_mask::calc_grids_factors(
        depsgraph, object, cache.automasking.get(), node, grids, factors);
    calc_brush_texture_factors(ss, brush, positions, factors);
    scale_factors(factors, cache.bstrength);
  }

  tls.translations.resize(positions.size());
  const MutableSpan<float3> translations = tls.translations;

  translations_from_offset_and_factors(grab_delta, factors, translations);

  calc_pinch_influence(brush, cache, grab_delta, spvc, positions, factors, translations);

  calc_rake_rotation_influence(cache, positions, factors, translations);

  if (do_elastic) {
    fill_factor_from_hide_and_mask(subdiv_ccg, grids, factors);
    scale_factors(factors, cache.bstrength * 20.0f);
    auto_mask::calc_grids_factors(
        depsgraph, object, cache.automasking.get(), node, grids, factors);

    calc_kelvinet_translation(cache, positions, factors, translations);
  }

  clip_and_lock_translations(sd, ss, positions, translations);
  apply_translations(translations, grids, subdiv_ccg);
}

static void calc_bmesh(const Depsgraph &depsgraph,
                       const Sculpt &sd,
                       Object &object,
                       const Brush &brush,
                       SculptProjectVector *spvc,
                       const float3 &grab_delta,
                       bke::pbvh::BMeshNode &node,
                       LocalData &tls)
{
  SculptSession &ss = *object.sculpt;
  const StrokeCache &cache = *ss.cache;
  const bool do_elastic = brush.snake_hook_deform_type == BRUSH_SNAKE_HOOK_DEFORM_ELASTIC;

  const Set<BMVert *, 0> &verts = BKE_pbvh_bmesh_node_unique_verts(&node);
  const MutableSpan positions = gather_bmesh_positions(verts, tls.positions);

  tls.factors.resize(verts.size());
  const MutableSpan<float> factors = tls.factors;

  if (do_elastic) {
    factors.fill(1.0f);
  }
  else {
    fill_factor_from_hide_and_mask(*ss.bm, verts, factors);
    filter_region_clip_factors(ss, positions, factors);
    if (brush.flag & BRUSH_FRONTFACE) {
      calc_front_face(cache.view_normal_symm, verts, factors);
    }

    tls.distances.resize(verts.size());
    const MutableSpan<float> distances = tls.distances;
    calc_brush_distances(ss, positions, eBrushFalloffShape(brush.falloff_shape), distances);
    filter_distances_with_radius(cache.radius, distances, factors);
    apply_hardness_to_distances(cache, distances);
    calc_brush_strength_factors(cache, brush, distances, factors);

    auto_mask::calc_vert_factors(depsgraph, object, cache.automasking.get(), node, verts, factors);
    calc_brush_texture_factors(ss, brush, positions, factors);
    scale_factors(factors, cache.bstrength);
  }

  tls.translations.resize(verts.size());
  const MutableSpan<float3> translations = tls.translations;

  translations_from_offset_and_factors(grab_delta, factors, translations);

  calc_pinch_influence(brush, cache, grab_delta, spvc, positions, factors, translations);

  calc_rake_rotation_influence(cache, positions, factors, translations);

  if (do_elastic) {
    fill_factor_from_hide_and_mask(*ss.bm, verts, factors);
    scale_factors(factors, cache.bstrength * 20.0f);
    auto_mask::calc_vert_factors(depsgraph, object, cache.automasking.get(), node, verts, factors);

    calc_kelvinet_translation(cache, positions, factors, translations);
  }

  clip_and_lock_translations(sd, ss, positions, translations);
  apply_translations(translations, verts);
}

}  // namespace snake_hook_cc

void do_snake_hook_brush(const Depsgraph &depsgraph,
                         const Sculpt &sd,
                         Object &object,
                         const IndexMask &node_mask)
{
  SculptSession &ss = *object.sculpt;
  bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(object);
  const Brush &brush = *BKE_paint_brush_for_read(&sd.paint);
  const float bstrength = ss.cache->bstrength;

  SculptProjectVector spvc;

  float3 grab_delta = ss.cache->grab_delta_symm;

  if (bstrength < 0.0f) {
    grab_delta *= -1.0f;
  }

  if (ss.cache->normal_weight > 0.0f) {
    sculpt_project_v3_normal_align(ss, ss.cache->normal_weight, grab_delta);
  }

  /* Optionally pinch while painting. */
  if (brush.crease_pinch_factor != 0.5f) {
    sculpt_project_v3_cache_init(&spvc, grab_delta);
  }

  threading::EnumerableThreadSpecific<LocalData> all_tls;
  switch (pbvh.type()) {
    case bke::pbvh::Type::Mesh: {
      Mesh &mesh = *static_cast<Mesh *>(object.data);
      const MeshAttributeData attribute_data(mesh);
      const PositionDeformData position_data(depsgraph, object);
      const Span<float3> vert_normals = bke::pbvh::vert_normals_eval(depsgraph, object);
      MutableSpan<bke::pbvh::MeshNode> nodes = pbvh.nodes<bke::pbvh::MeshNode>();
      node_mask.foreach_index(GrainSize(1), [&](const int i) {
        LocalData &tls = all_tls.local();
        calc_faces(depsgraph,
                   sd,
                   object,
                   brush,
                   &spvc,
                   grab_delta,
                   vert_normals,
                   attribute_data,
                   nodes[i],
                   tls,
                   position_data);
        bke::pbvh::update_node_bounds_mesh(position_data.eval, nodes[i]);
      });
      break;
    }
    case bke::pbvh::Type::Grids: {
      SubdivCCG &subdiv_ccg = *object.sculpt->subdiv_ccg;
      MutableSpan<float3> positions = subdiv_ccg.positions;
      MutableSpan<bke::pbvh::GridsNode> nodes = pbvh.nodes<bke::pbvh::GridsNode>();
      node_mask.foreach_index(GrainSize(1), [&](const int i) {
        LocalData &tls = all_tls.local();
        calc_grids(depsgraph, sd, object, brush, &spvc, grab_delta, nodes[i], tls);
        bke::pbvh::update_node_bounds_grids(subdiv_ccg.grid_area, positions, nodes[i]);
      });
      break;
    }
    case bke::pbvh::Type::BMesh: {
      MutableSpan<bke::pbvh::BMeshNode> nodes = pbvh.nodes<bke::pbvh::BMeshNode>();
      node_mask.foreach_index(GrainSize(1), [&](const int i) {
        LocalData &tls = all_tls.local();
        calc_bmesh(depsgraph, sd, object, brush, &spvc, grab_delta, nodes[i], tls);
        bke::pbvh::update_node_bounds_bmesh(nodes[i]);
      });
      break;
    }
  }
  pbvh.tag_positions_changed(node_mask);
  pbvh.flush_bounds_to_parents();
}

}  // namespace blender::ed::sculpt_paint::brushes
