/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "editors/sculpt_paint/brushes/types.hh"

#include "DNA_brush_types.h"
#include "DNA_mesh_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BKE_key.hh"
#include "BKE_mesh.hh"
#include "BKE_paint.hh"
#include "BKE_pbvh.hh"
#include "BKE_subdiv_ccg.hh"

#include "BLI_array.hh"
#include "BLI_enumerable_thread_specific.hh"
#include "BLI_math_vector.hh"
#include "BLI_task.h"
#include "BLI_task.hh"

#include "editors/sculpt_paint/mesh_brush_common.hh"
#include "editors/sculpt_paint/sculpt_intern.hh"

namespace blender::ed::sculpt_paint {

inline namespace draw_vector_displacement_cc {

struct LocalData {
  Vector<float3> positions;
  Vector<float> factors;
  Vector<float> distances;
  Vector<float4> colors;
  Vector<float3> translations;
};

static void calc_brush_texture_colors(SculptSession &ss,
                                      const Brush &brush,
                                      const Span<float3> vert_positions,
                                      const Span<int> verts,
                                      const Span<float> factors,
                                      const MutableSpan<float4> r_colors)
{
  BLI_assert(verts.size() == r_colors.size());

  const int thread_id = BLI_task_parallel_thread_id(nullptr);

  for (const int i : verts.index_range()) {
    float texture_value;
    float4 texture_rgba;
    /* NOTE: This is not a thread-safe call. */
    sculpt_apply_texture(
        ss, brush, vert_positions[verts[i]], thread_id, &texture_value, texture_rgba);

    r_colors[i] = texture_rgba * factors[i];
  }
}

static void calc_brush_texture_colors(SculptSession &ss,
                                      const Brush &brush,
                                      const Span<float3> positions,
                                      const Span<float> factors,
                                      const MutableSpan<float4> r_colors)
{
  BLI_assert(positions.size() == r_colors.size());

  const int thread_id = BLI_task_parallel_thread_id(nullptr);

  for (const int i : positions.index_range()) {
    float texture_value;
    float4 texture_rgba;
    /* NOTE: This is not a thread-safe call. */
    sculpt_apply_texture(ss, brush, positions[i], thread_id, &texture_value, texture_rgba);

    r_colors[i] = texture_rgba * factors[i];
  }
}

static void calc_faces(const Sculpt &sd,
                       const Brush &brush,
                       const Span<float3> positions_eval,
                       const Span<float3> vert_normals,
                       const bke::pbvh::Node &node,
                       Object &object,
                       LocalData &tls,
                       MutableSpan<float3> positions_orig)
{
  SculptSession &ss = *object.sculpt;
  const StrokeCache &cache = *ss.cache;
  Mesh &mesh = *static_cast<Mesh *>(object.data);

  const Span<int> verts = bke::pbvh::node_unique_verts(node);

  tls.factors.resize(verts.size());
  const MutableSpan<float> factors = tls.factors;
  fill_factor_from_hide_and_mask(mesh, verts, factors);
  filter_region_clip_factors(ss, positions_eval, verts, factors);
  if (brush.flag & BRUSH_FRONTFACE) {
    calc_front_face(cache.view_normal, vert_normals, verts, factors);
  }

  tls.distances.resize(verts.size());
  const MutableSpan<float> distances = tls.distances;
  calc_brush_distances(
      ss, positions_eval, verts, eBrushFalloffShape(brush.falloff_shape), distances);
  filter_distances_with_radius(cache.radius, distances, factors);
  apply_hardness_to_distances(cache, distances);
  calc_brush_strength_factors(cache, brush, distances, factors);

  if (cache.automasking) {
    auto_mask::calc_vert_factors(object, *cache.automasking, node, verts, factors);
  }

  tls.colors.resize(verts.size());
  const MutableSpan<float4> colors = tls.colors;
  calc_brush_texture_colors(ss, brush, positions_eval, verts, factors, colors);

  tls.translations.resize(verts.size());
  const MutableSpan<float3> translations = tls.translations;
  for (const int i : verts.index_range()) {
    SCULPT_calc_vertex_displacement(ss, brush, colors[i], translations[i]);
  }

  write_translations(sd, object, positions_eval, verts, translations, positions_orig);
}

static void calc_grids(
    const Sculpt &sd, Object &object, const Brush &brush, bke::pbvh::Node &node, LocalData &tls)
{
  SculptSession &ss = *object.sculpt;
  const StrokeCache &cache = *ss.cache;
  SubdivCCG &subdiv_ccg = *ss.subdiv_ccg;

  const Span<int> grids = bke::pbvh::node_grid_indices(node);
  const MutableSpan positions = gather_grids_positions(subdiv_ccg, grids, tls.positions);

  tls.factors.resize(positions.size());
  const MutableSpan<float> factors = tls.factors;
  fill_factor_from_hide_and_mask(subdiv_ccg, grids, factors);
  filter_region_clip_factors(ss, positions, factors);
  if (brush.flag & BRUSH_FRONTFACE) {
    calc_front_face(cache.view_normal, subdiv_ccg, grids, factors);
  }

  tls.distances.resize(positions.size());
  const MutableSpan<float> distances = tls.distances;
  calc_brush_distances(ss, positions, eBrushFalloffShape(brush.falloff_shape), distances);
  filter_distances_with_radius(cache.radius, distances, factors);
  apply_hardness_to_distances(cache, distances);
  calc_brush_strength_factors(cache, brush, distances, factors);

  if (cache.automasking) {
    auto_mask::calc_grids_factors(object, *cache.automasking, node, grids, factors);
  }

  tls.colors.resize(positions.size());
  const MutableSpan<float4> colors = tls.colors;
  calc_brush_texture_colors(ss, brush, positions, factors, colors);

  tls.translations.resize(positions.size());
  const MutableSpan<float3> translations = tls.translations;
  for (const int i : positions.index_range()) {
    SCULPT_calc_vertex_displacement(ss, brush, colors[i], translations[i]);
  }

  clip_and_lock_translations(sd, ss, positions, translations);
  apply_translations(translations, grids, subdiv_ccg);
}

static void calc_bmesh(
    const Sculpt &sd, Object &object, const Brush &brush, bke::pbvh::Node &node, LocalData &tls)
{
  SculptSession &ss = *object.sculpt;
  const StrokeCache &cache = *ss.cache;

  const Set<BMVert *, 0> &verts = BKE_pbvh_bmesh_node_unique_verts(&node);
  const MutableSpan positions = gather_bmesh_positions(verts, tls.positions);

  tls.factors.resize(verts.size());
  const MutableSpan<float> factors = tls.factors;
  fill_factor_from_hide_and_mask(*ss.bm, verts, factors);
  filter_region_clip_factors(ss, positions, factors);
  if (brush.flag & BRUSH_FRONTFACE) {
    calc_front_face(cache.view_normal, verts, factors);
  }

  tls.distances.resize(verts.size());
  const MutableSpan<float> distances = tls.distances;
  calc_brush_distances(ss, positions, eBrushFalloffShape(brush.falloff_shape), distances);
  filter_distances_with_radius(cache.radius, distances, factors);
  apply_hardness_to_distances(cache, distances);
  calc_brush_strength_factors(cache, brush, distances, factors);

  if (cache.automasking) {
    auto_mask::calc_vert_factors(object, *cache.automasking, node, verts, factors);
  }

  tls.colors.resize(verts.size());
  const MutableSpan<float4> colors = tls.colors;
  calc_brush_texture_colors(ss, brush, positions, factors, colors);

  tls.translations.resize(verts.size());
  const MutableSpan<float3> translations = tls.translations;
  for (const int i : positions.index_range()) {
    SCULPT_calc_vertex_displacement(ss, brush, colors[i], translations[i]);
  }

  clip_and_lock_translations(sd, ss, positions, translations);
  apply_translations(translations, verts);
}

}  // namespace draw_vector_displacement_cc

void do_draw_vector_displacement_brush(const Sculpt &sd,
                                       Object &object,
                                       Span<bke::pbvh::Node *> nodes)
{
  const SculptSession &ss = *object.sculpt;
  const Brush &brush = *BKE_paint_brush_for_read(&sd.paint);

  threading::EnumerableThreadSpecific<LocalData> all_tls;
  switch (object.sculpt->pbvh->type()) {
    case bke::pbvh::Type::Mesh: {
      Mesh &mesh = *static_cast<Mesh *>(object.data);
      const bke::pbvh::Tree &pbvh = *ss.pbvh;
      const Span<float3> positions_eval = BKE_pbvh_get_vert_positions(pbvh);
      const Span<float3> vert_normals = BKE_pbvh_get_vert_normals(pbvh);
      MutableSpan<float3> positions_orig = mesh.vert_positions_for_write();
      threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
        LocalData &tls = all_tls.local();
        for (const int i : range) {
          calc_faces(
              sd, brush, positions_eval, vert_normals, *nodes[i], object, tls, positions_orig);
          BKE_pbvh_node_mark_positions_update(nodes[i]);
        }
      });
      break;
    }
    case bke::pbvh::Type::Grids:
      threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
        LocalData &tls = all_tls.local();
        for (const int i : range) {
          calc_grids(sd, object, brush, *nodes[i], tls);
        }
      });
      break;
    case bke::pbvh::Type::BMesh:
      threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
        LocalData &tls = all_tls.local();
        for (const int i : range) {
          calc_bmesh(sd, object, brush, *nodes[i], tls);
        }
      });
      break;
  }
}

}  // namespace blender::ed::sculpt_paint
