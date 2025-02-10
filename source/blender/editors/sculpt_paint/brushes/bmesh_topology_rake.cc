/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "editors/sculpt_paint/brushes/types.hh"

#include "DNA_brush_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BKE_mesh.hh"
#include "BKE_paint.hh"

#include "BLI_enumerable_thread_specific.hh"
#include "BLI_math_vector.hh"
#include "BLI_task.hh"

#include "editors/sculpt_paint/mesh_brush_common.hh"
#include "editors/sculpt_paint/sculpt_automask.hh"
#include "editors/sculpt_paint/sculpt_intern.hh"
#include "editors/sculpt_paint/sculpt_smooth.hh"

#include "bmesh.hh"

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

static void calc_bmesh(const Depsgraph &depsgraph,
                       const Sculpt &sd,
                       Object &object,
                       const Brush &brush,
                       const float3 &direction,
                       const float strength,
                       bke::pbvh::BMeshNode &node,
                       LocalData &tls)
{
  SculptSession &ss = *object.sculpt;

  const Set<BMVert *, 0> &verts = BKE_pbvh_bmesh_node_unique_verts(&node);
  const MutableSpan positions = gather_bmesh_positions(verts, tls.positions);

  calc_factors_common_bmesh(depsgraph, brush, object, positions, node, tls.factors, tls.distances);

  scale_factors(tls.factors, strength);

  tls.translations.resize(verts.size());
  const MutableSpan<float3> translations = tls.translations;
  calc_translations(verts, direction, translations);
  scale_translations(translations, tls.factors);

  clip_and_lock_translations(sd, ss, positions, translations);
  apply_translations(translations, verts);
}

}  // namespace bmesh_topology_rake_cc

void do_bmesh_topology_rake_brush(const Depsgraph &depsgraph,
                                  const Sculpt &sd,
                                  Object &object,
                                  const IndexMask &node_mask,
                                  const float input_strength)
{
  const SculptSession &ss = *object.sculpt;
  bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(object);
  const Brush &brush = *BKE_paint_brush_for_read(&sd.paint);
  const float strength = std::clamp(input_strength, 0.0f, 1.0f);

  /* Interactions increase both strength and quality. */
  const int iterations = 3;

  const int count = iterations * strength + 1;
  const float factor = iterations * strength / count;

  float3 direction = ss.cache->grab_delta_symm;

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
    MutableSpan<bke::pbvh::BMeshNode> nodes = pbvh.nodes<bke::pbvh::BMeshNode>();
    node_mask.foreach_index(GrainSize(1), [&](const int i) {
      LocalData &tls = all_tls.local();
      calc_bmesh(
          depsgraph, sd, object, brush, direction, factor * ss.cache->pressure, nodes[i], tls);
      bke::pbvh::update_node_bounds_bmesh(nodes[i]);
    });
  }
  pbvh.tag_positions_changed(node_mask);
  pbvh.flush_bounds_to_parents();
}

}  // namespace blender::ed::sculpt_paint
