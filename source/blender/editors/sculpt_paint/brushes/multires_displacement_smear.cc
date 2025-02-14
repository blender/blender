/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "editors/sculpt_paint/brushes/types.hh"

#include "DNA_brush_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BKE_paint.hh"
#include "BKE_paint_bvh.hh"
#include "BKE_subdiv_ccg.hh"

#include "BLI_enumerable_thread_specific.hh"
#include "BLI_math_vector.hh"
#include "BLI_task.hh"

#include "editors/sculpt_paint/mesh_brush_common.hh"
#include "editors/sculpt_paint/sculpt_intern.hh"

namespace blender::ed::sculpt_paint {

inline namespace multires_displacement_smear_cc {

struct LocalData {
  Vector<float3> positions;
  Vector<float> factors;
  Vector<float> distances;
};

static void calc_node(const Depsgraph &depsgraph,
                      Object &object,
                      const Brush &brush,
                      const float strength,
                      const bke::pbvh::GridsNode &node,
                      LocalData &tls)
{
  SculptSession &ss = *object.sculpt;
  const StrokeCache &cache = *ss.cache;
  SubdivCCG &subdiv_ccg = *ss.subdiv_ccg;
  MutableSpan<float3> ccg_positions = subdiv_ccg.positions;
  const CCGKey key = BKE_subdiv_ccg_key_top_level(subdiv_ccg);

  const Span<int> grids = node.grids();
  const MutableSpan positions = gather_grids_positions(subdiv_ccg, grids, tls.positions);

  calc_factors_common_grids(depsgraph, brush, object, positions, node, tls.factors, tls.distances);

  scale_factors(tls.factors, strength);

  for (const int i : grids.index_range()) {
    const IndexRange node_grid_range = bke::ccg::grid_range(key.grid_area, i);
    const IndexRange grid_range = bke::ccg::grid_range(key.grid_area, grids[i]);
    const int grid = grids[i];
    for (const int y : IndexRange(key.grid_size)) {
      for (const int x : IndexRange(key.grid_size)) {
        const int offset = CCG_grid_xy_to_index(key.grid_size, x, y);
        const int node_vert = node_grid_range[offset];
        const int vert = grid_range[offset];

        float3 current_disp;
        switch (brush.smear_deform_type) {
          case BRUSH_SMEAR_DEFORM_DRAG:
            current_disp = cache.location_symm - cache.last_location_symm;
            break;
          case BRUSH_SMEAR_DEFORM_PINCH:
            current_disp = cache.location_symm - ccg_positions[vert];
            break;
          case BRUSH_SMEAR_DEFORM_EXPAND:
            current_disp = ccg_positions[vert] - cache.location_symm;
            break;
        }

        const float3 current_disp_norm = math::normalize(current_disp);

        SubdivCCGCoord coord{};
        coord.grid_index = grid;
        coord.x = x;
        coord.y = y;

        float3 interp_limit_surface_disp = cache.displacement_smear.prev_displacement[vert];
        float weights_accum = 1.0f;

        SubdivCCGNeighbors neighbors;
        BKE_subdiv_ccg_neighbor_coords_get(*ss.subdiv_ccg, coord, false, neighbors);

        for (const SubdivCCGCoord neighbor : neighbors.coords) {
          const int neighbor_index = neighbor.to_index(key);
          const float3 vert_disp = cache.displacement_smear.limit_surface_co[neighbor_index] -
                                   cache.displacement_smear.limit_surface_co[vert];
          const float3 &neighbor_limit_surface_disp =
              cache.displacement_smear.prev_displacement[neighbor_index];
          const float3 vert_disp_norm = math::normalize(vert_disp);

          if (math::dot(current_disp_norm, vert_disp_norm) >= 0.0f) {
            continue;
          }

          const float disp_interp = std::clamp(
              -math::dot(current_disp_norm, vert_disp_norm), 0.0f, 1.0f);
          interp_limit_surface_disp += neighbor_limit_surface_disp * disp_interp;
          weights_accum += disp_interp;
        }

        interp_limit_surface_disp *= math::rcp(weights_accum);

        float3 new_co = cache.displacement_smear.limit_surface_co[vert] +
                        interp_limit_surface_disp;
        ccg_positions[vert] = math::interpolate(
            ccg_positions[vert], new_co, tls.factors[node_vert]);
      }
    }
  }
}

BLI_NOINLINE static void eval_all_limit_positions(const SubdivCCG &subdiv_ccg,
                                                  const MutableSpan<float3> limit_positions)
{
  const CCGKey key = BKE_subdiv_ccg_key_top_level(subdiv_ccg);
  threading::parallel_for(IndexRange(subdiv_ccg.grids_num), 1024, [&](const IndexRange range) {
    for (const int grid : range) {
      const MutableSpan grid_limit_positions = limit_positions.slice(
          bke::ccg::grid_range(key.grid_area, grid));
      BKE_subdiv_ccg_eval_limit_positions(subdiv_ccg, key, grid, grid_limit_positions);
    }
  });
}

BLI_NOINLINE static void store_node_prev_displacement(const Span<float3> limit_positions,
                                                      const Span<float3> positions,
                                                      const CCGKey &key,
                                                      const bke::pbvh::GridsNode &node,
                                                      const MutableSpan<float3> prev_displacement)
{
  for (const int grid : node.grids()) {
    for (const int i : bke::ccg::grid_range(key.grid_area, grid)) {
      prev_displacement[i] = positions[i] - limit_positions[i];
    }
  }
}

}  // namespace multires_displacement_smear_cc

void do_displacement_smear_brush(const Depsgraph &depsgraph,
                                 const Sculpt &sd,
                                 Object &ob,
                                 const IndexMask &node_mask)
{
  const Brush &brush = *BKE_paint_brush_for_read(&sd.paint);
  SculptSession &ss = *ob.sculpt;
  bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(ob);
  MutableSpan<bke::pbvh::GridsNode> nodes = pbvh.nodes<bke::pbvh::GridsNode>();

  SubdivCCG &subdiv_ccg = *ss.subdiv_ccg;
  MutableSpan<float3> positions = subdiv_ccg.positions;
  const CCGKey key = BKE_subdiv_ccg_key_top_level(subdiv_ccg);

  if (ss.cache->displacement_smear.limit_surface_co.is_empty()) {
    ss.cache->displacement_smear.prev_displacement = Array<float3>(positions.size(), float3(0.0f));
    ss.cache->displacement_smear.limit_surface_co = Array<float3>(positions.size());

    eval_all_limit_positions(subdiv_ccg, ss.cache->displacement_smear.limit_surface_co);
  }

  node_mask.foreach_index(GrainSize(1), [&](const int i) {
    store_node_prev_displacement(ss.cache->displacement_smear.limit_surface_co,
                                 subdiv_ccg.positions,
                                 key,
                                 nodes[i],
                                 ss.cache->displacement_smear.prev_displacement);
  });

  const float strength = std::clamp(ss.cache->bstrength, 0.0f, 1.0f);

  threading::EnumerableThreadSpecific<LocalData> all_tls;
  node_mask.foreach_index(GrainSize(1), [&](const int i) {
    LocalData &tls = all_tls.local();
    calc_node(depsgraph, ob, brush, strength, nodes[i], tls);
    bke::pbvh::update_node_bounds_grids(subdiv_ccg.grid_area, positions, nodes[i]);
  });
  pbvh.tag_positions_changed(node_mask);
  pbvh.flush_bounds_to_parents();
}

}  // namespace blender::ed::sculpt_paint
