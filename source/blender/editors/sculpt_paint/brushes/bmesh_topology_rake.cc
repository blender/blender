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

#include "BLI_array.hh"
#include "BLI_enumerable_thread_specific.hh"
#include "BLI_math_vector.hh"
#include "BLI_task.hh"

#include "editors/sculpt_paint/mesh_brush_common.hh"
#include "editors/sculpt_paint/sculpt_intern.hh"

namespace blender::ed::sculpt_paint {

inline namespace bmesh_topology_rake_cc {

struct LocalData {
  Vector<float3> positions;
  Vector<float> factors;
  Vector<float> distances;
  Vector<float3> translations;
};

BLI_NOINLINE static void calc_translations(const Set<BMVert *, 0> &verts,
                                           const float3 &direction,
                                           const MutableSpan<float3> translations)
{
  int i = 0;
  for (const BMVert *vert : verts) {
    float3 average;
    smooth::bmesh_four_neighbor_average(average, direction, vert);
    translations[i] = average - float3(vert->co);
    i++;
  }
}

static void calc_bmesh(const Sculpt &sd,
                       Object &object,
                       const Brush &brush,
                       const float3 &direction,
                       const float strength,
                       PBVHNode &node,
                       LocalData &tls)
{
  SculptSession &ss = *object.sculpt;
  const StrokeCache &cache = *ss.cache;

  const Set<BMVert *, 0> &verts = BKE_pbvh_bmesh_node_unique_verts(&node);

  tls.positions.reinitialize(verts.size());
  MutableSpan<float3> positions = tls.positions;
  gather_bmesh_positions(verts, positions);

  tls.factors.reinitialize(verts.size());
  const MutableSpan<float> factors = tls.factors;
  fill_factor_from_hide_and_mask(*ss.bm, verts, factors);
  filter_region_clip_factors(ss, positions, factors);
  if (brush.flag & BRUSH_FRONTFACE) {
    calc_front_face(cache.view_normal, verts, factors);
  }

  tls.distances.reinitialize(verts.size());
  const MutableSpan<float> distances = tls.distances;
  calc_brush_distances(ss, positions, eBrushFalloffShape(brush.falloff_shape), distances);
  filter_distances_with_radius(cache.radius, distances, factors);
  apply_hardness_to_distances(cache, distances);
  calc_brush_strength_factors(cache, brush, distances, factors);

  if (cache.automasking) {
    auto_mask::calc_vert_factors(object, *cache.automasking, node, verts, factors);
  }

  calc_brush_texture_factors(ss, brush, positions, factors);

  scale_factors(factors, strength);

  tls.translations.reinitialize(verts.size());
  const MutableSpan<float3> translations = tls.translations;
  calc_translations(verts, direction, translations);
  scale_translations(translations, factors);

  clip_and_lock_translations(sd, ss, positions, translations);
  apply_translations(translations, verts);
}

}  // namespace bmesh_topology_rake_cc

void do_bmesh_topology_rake_brush(const Sculpt &sd,
                                  Object &object,
                                  Span<PBVHNode *> nodes,
                                  const float input_strength)
{
  const SculptSession &ss = *object.sculpt;
  const Brush &brush = *BKE_paint_brush_for_read(&sd.paint);
  const float strength = std::clamp(input_strength, 0.0f, 1.0f);

  /* Interactions increase both strength and quality. */
  const int iterations = 3;

  const int count = iterations * strength + 1;
  const float factor = iterations * strength / count;

  float3 direction = ss.cache->grab_delta_symmetry;

  /* TODO: Is this just the same as one of the projection utility functions? */
  float3 tmp = ss.cache->sculpt_normal_symm * math::dot(ss.cache->sculpt_normal_symm, direction);
  direction -= tmp;
  direction = math::normalize(direction);

  /* Cancel if there's no grab data. */
  if (math::is_zero(direction)) {
    return;
  }

  threading::EnumerableThreadSpecific<LocalData> all_tls;
  for ([[maybe_unused]] const int i : IndexRange(count)) {
    threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
      LocalData &tls = all_tls.local();
      for (const int i : range) {
        calc_bmesh(sd, object, brush, direction, factor * ss.cache->pressure, *nodes[i], tls);
      }
    });
  }
}

}  // namespace blender::ed::sculpt_paint
