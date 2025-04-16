/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "editors/sculpt_paint/brushes/brushes.hh"

#include "DNA_brush_types.h"
#include "DNA_mesh_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BKE_paint.hh"
#include "BKE_paint_bvh.hh"
#include "BKE_subdiv_ccg.hh"

#include "BLI_enumerable_thread_specific.hh"
#include "BLI_task.hh"

#include "editors/sculpt_paint/mesh_brush_common.hh"
#include "editors/sculpt_paint/sculpt_automask.hh"
#include "editors/sculpt_paint/sculpt_intern.hh"

namespace blender::ed::sculpt_paint::brushes {

inline namespace multires_displacement_eraser_cc {

struct LocalData {
  Vector<float3> positions;
  Vector<float> factors;
  Vector<float> distances;
  Vector<float3> translations;
};

static BLI_NOINLINE void calc_limit_positions(const SubdivCCG &subdiv_ccg,
                                              const Span<int> grids,
                                              const MutableSpan<float3> limit_positions)
{
  const CCGKey key = BKE_subdiv_ccg_key_top_level(subdiv_ccg);
  for (const int i : grids.index_range()) {
    const int start = i * key.grid_area;
    BKE_subdiv_ccg_eval_limit_positions(
        subdiv_ccg, key, grids[i], limit_positions.slice(start, key.grid_area));
  }
}

static void calc_node(const Depsgraph &depsgraph,
                      const Sculpt &sd,
                      Object &object,
                      const Brush &brush,
                      const float strength,
                      bke::pbvh::GridsNode &node,
                      LocalData &tls)
{
  SculptSession &ss = *object.sculpt;
  SubdivCCG &subdiv_ccg = *ss.subdiv_ccg;

  const Span<int> grids = node.grids();
  const MutableSpan positions = gather_grids_positions(subdiv_ccg, grids, tls.positions);

  calc_factors_common_grids(depsgraph, brush, object, positions, node, tls.factors, tls.distances);

  scale_factors(tls.factors, strength);

  tls.translations.resize(positions.size());
  const MutableSpan<float3> translations = tls.translations;
  calc_limit_positions(subdiv_ccg, grids, translations);
  for (const int i : positions.index_range()) {
    translations[i] -= positions[i];
  }
  scale_translations(translations, tls.factors);

  clip_and_lock_translations(sd, ss, positions, translations);
  apply_translations(translations, grids, subdiv_ccg);
}

}  // namespace multires_displacement_eraser_cc

void do_displacement_eraser_brush(const Depsgraph &depsgraph,
                                  const Sculpt &sd,
                                  Object &object,
                                  const IndexMask &node_mask)
{
  SculptSession &ss = *object.sculpt;
  SubdivCCG &subdiv_ccg = *object.sculpt->subdiv_ccg;
  MutableSpan<float3> positions = subdiv_ccg.positions;
  const Brush &brush = *BKE_paint_brush_for_read(&sd.paint);
  const float strength = std::min(ss.cache->bstrength, 1.0f);

  threading::EnumerableThreadSpecific<LocalData> all_tls;
  bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(object);
  MutableSpan<bke::pbvh::GridsNode> nodes = pbvh.nodes<bke::pbvh::GridsNode>();
  node_mask.foreach_index(GrainSize(1), [&](const int i) {
    LocalData &tls = all_tls.local();
    calc_node(depsgraph, sd, object, brush, strength, nodes[i], tls);
    bke::pbvh::update_node_bounds_grids(subdiv_ccg.grid_area, positions, nodes[i]);
  });
  pbvh.tag_positions_changed(node_mask);
  pbvh.flush_bounds_to_parents();
}

}  // namespace blender::ed::sculpt_paint::brushes
