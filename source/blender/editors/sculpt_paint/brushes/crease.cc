/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "editors/sculpt_paint/brushes/types.hh"

#include "DNA_brush_types.h"
#include "DNA_mesh_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BKE_brush.hh"
#include "BKE_key.hh"
#include "BKE_mesh.hh"
#include "BKE_paint.hh"
#include "BKE_pbvh.hh"
#include "BKE_subdiv_ccg.hh"

#include "BLI_array.hh"
#include "BLI_enumerable_thread_specific.hh"
#include "BLI_math_geom.h"
#include "BLI_math_matrix.hh"
#include "BLI_math_vector.hh"
#include "BLI_task.h"
#include "BLI_task.hh"

#include "editors/sculpt_paint/mesh_brush_common.hh"
#include "editors/sculpt_paint/sculpt_intern.hh"

namespace blender::ed::sculpt_paint {

inline namespace crease_cc {

struct LocalData {
  Vector<float3> positions;
  Vector<float> factors;
  Vector<float> distances;
  Vector<float3> translations;
};

BLI_NOINLINE static void translations_from_position(const Span<float3> positions_eval,
                                                    const Span<int> verts,
                                                    const float3 &location,
                                                    const MutableSpan<float3> translations)
{
  for (const int i : verts.index_range()) {
    translations[i] = location - positions_eval[verts[i]];
  }
}

BLI_NOINLINE static void translations_from_position(const Span<float3> positions,
                                                    const float3 &location,
                                                    const MutableSpan<float3> translations)
{
  for (const int i : positions.index_range()) {
    translations[i] = location - positions[i];
  }
}

BLI_NOINLINE static void add_offset_to_translations(const MutableSpan<float3> translations,
                                                    const Span<float> factors,
                                                    const float3 &offset)
{
  for (const int i : translations.index_range()) {
    translations[i] += offset * factors[i];
  }
}

static void calc_faces(const Sculpt &sd,
                       const Brush &brush,
                       const float3 &offset,
                       const float strength,
                       const Span<float3> positions_eval,
                       const Span<float3> vert_normals,
                       const bke::pbvh::Node &node,
                       Object &object,
                       LocalData &tls,
                       const MutableSpan<float3> positions_orig)
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

  calc_brush_texture_factors(ss, brush, positions_eval, verts, factors);

  tls.translations.resize(verts.size());
  const MutableSpan<float3> translations = tls.translations;
  translations_from_position(positions_eval, verts, cache.location, translations);

  if (brush.falloff_shape == PAINT_FALLOFF_SHAPE_TUBE) {
    project_translations(translations, cache.view_normal);
  }

  scale_translations(translations, factors);
  scale_translations(translations, strength);

  /* The vertices are pinched towards a line instead of a single point. Without this we get a
   * 'flat' surface surrounding the pinch. */
  project_translations(translations, cache.sculpt_normal_symm);

  add_offset_to_translations(translations, factors, offset);

  write_translations(sd, object, positions_eval, verts, translations, positions_orig);
}

static void calc_grids(const Sculpt &sd,
                       Object &object,
                       const Brush &brush,
                       const float3 &offset,
                       const float strength,
                       bke::pbvh::Node &node,
                       LocalData &tls)
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

  calc_brush_texture_factors(ss, brush, positions, factors);

  tls.translations.resize(positions.size());
  const MutableSpan<float3> translations = tls.translations;
  translations_from_position(positions, cache.location, translations);

  if (brush.falloff_shape == PAINT_FALLOFF_SHAPE_TUBE) {
    project_translations(translations, cache.view_normal);
  }

  scale_translations(translations, factors);
  scale_translations(translations, strength);

  project_translations(translations, cache.sculpt_normal_symm);

  add_offset_to_translations(translations, factors, offset);

  clip_and_lock_translations(sd, ss, positions, translations);
  apply_translations(translations, grids, subdiv_ccg);
}

static void calc_bmesh(const Sculpt &sd,
                       Object &object,
                       const Brush &brush,
                       const float3 &offset,
                       const float strength,
                       bke::pbvh::Node &node,
                       LocalData &tls)
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

  calc_brush_texture_factors(ss, brush, positions, factors);

  tls.translations.resize(verts.size());
  const MutableSpan<float3> translations = tls.translations;
  translations_from_position(positions, cache.location, translations);

  if (brush.falloff_shape == PAINT_FALLOFF_SHAPE_TUBE) {
    project_translations(translations, cache.view_normal);
  }

  scale_translations(translations, factors);
  scale_translations(translations, strength);

  project_translations(translations, cache.sculpt_normal_symm);

  add_offset_to_translations(translations, factors, offset);

  clip_and_lock_translations(sd, ss, positions, translations);
  apply_translations(translations, verts);
}

static void do_crease_or_blob_brush(const Scene &scene,
                                    const Sculpt &sd,
                                    const bool invert_strength,
                                    Object &object,
                                    Span<bke::pbvh::Node *> nodes)
{
  const SculptSession &ss = *object.sculpt;
  const StrokeCache &cache = *ss.cache;
  const Brush &brush = *BKE_paint_brush_for_read(&sd.paint);

  /* Offset with as much as possible factored in already. */
  const float3 offset = cache.sculpt_normal_symm * cache.scale * cache.radius * cache.bstrength;

  /* We divide out the squared alpha and multiply by the squared crease
   * to give us the pinch strength. */
  float crease_correction = brush.crease_pinch_factor * brush.crease_pinch_factor;
  float brush_alpha = BKE_brush_alpha_get(&scene, &brush);
  if (brush_alpha > 0.0f) {
    crease_correction /= brush_alpha * brush_alpha;
  }

  /* We always want crease to pinch or blob to relax even when draw is negative. */
  const float strength = std::abs(cache.bstrength) * crease_correction *
                         (invert_strength ? -1.0f : 1.0f);

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
          calc_faces(sd,
                     brush,
                     offset,
                     strength,
                     positions_eval,
                     vert_normals,
                     *nodes[i],
                     object,
                     tls,
                     positions_orig);
          BKE_pbvh_node_mark_positions_update(nodes[i]);
        }
      });
      break;
    }
    case bke::pbvh::Type::Grids:
      threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
        LocalData &tls = all_tls.local();
        for (const int i : range) {
          calc_grids(sd, object, brush, offset, strength, *nodes[i], tls);
        }
      });
      break;
    case bke::pbvh::Type::BMesh:
      threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
        LocalData &tls = all_tls.local();
        for (const int i : range) {
          calc_bmesh(sd, object, brush, offset, strength, *nodes[i], tls);
        }
      });
      break;
  }
}

}  // namespace crease_cc

void do_crease_brush(const Scene &scene,
                     const Sculpt &sd,
                     Object &object,
                     Span<bke::pbvh::Node *> nodes)
{
  do_crease_or_blob_brush(scene, sd, false, object, nodes);
}

void do_blob_brush(const Scene &scene,
                   const Sculpt &sd,
                   Object &object,
                   Span<bke::pbvh::Node *> nodes)
{
  do_crease_or_blob_brush(scene, sd, true, object, nodes);
}

}  // namespace blender::ed::sculpt_paint
