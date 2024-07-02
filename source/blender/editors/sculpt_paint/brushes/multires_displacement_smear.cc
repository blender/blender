/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "editors/sculpt_paint/brushes/types.hh"

#include "DNA_brush_types.h"
#include "DNA_mesh_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BKE_paint.hh"
#include "BKE_pbvh.hh"
#include "BKE_subdiv_ccg.hh"

#include "BLI_enumerable_thread_specific.hh"
#include "BLI_math_matrix.hh"
#include "BLI_math_vector.h"
#include "BLI_math_vector.hh"
#include "BLI_task.h"
#include "BLI_task.hh"

#include "editors/sculpt_paint/mesh_brush_common.hh"
#include "editors/sculpt_paint/sculpt_intern.hh"

namespace blender::ed::sculpt_paint {

inline namespace multires_displacement_smear_cc {

static void calc_node(Object &object, const Brush &brush, PBVHNode &node)
{
  SculptSession &ss = *object.sculpt;
  const StrokeCache &cache = *ss.cache;
  SubdivCCG &subdiv_ccg = *ss.subdiv_ccg;
  const Span<CCGElem *> elems = subdiv_ccg.grids;
  const CCGKey key = BKE_subdiv_ccg_key_top_level(subdiv_ccg);
  const float bstrength = clamp_f(cache.bstrength, 0.0f, 1.0f);

  SculptBrushTest test;
  SculptBrushTestFn sculpt_brush_test_sq_fn = SCULPT_brush_test_init_with_falloff_shape(
      ss, test, brush.falloff_shape);
  const int thread_id = BLI_task_parallel_thread_id(nullptr);

  auto_mask::NodeData automask_data = auto_mask::node_begin(object, cache.automasking.get(), node);

  const Span<int> grids = bke::pbvh::node_grid_indices(node);

  int i = 0;
  for (const int grid : grids) {
    const int start = grid * key.grid_area;
    CCGElem *elem = elems[grid];
    for (const int y : IndexRange(key.grid_size)) {
      for (const int x : IndexRange(key.grid_size)) {
        const int offset = CCG_grid_xy_to_index(key.grid_size, x, y);
        const int grid_vert_index = start + offset;
        float3 &co = CCG_elem_offset_co(key, elem, offset);
        if (!sculpt_brush_test_sq_fn(test, co)) {
          i++;
          continue;
        }
        auto_mask::node_update(automask_data, i);

        const float mask = key.has_mask ? CCG_elem_offset_mask(key, elem, offset) : 0.0f;
        const float fade = bstrength *
                           SCULPT_brush_strength_factor(ss,
                                                        brush,
                                                        co,
                                                        sqrtf(test.dist),
                                                        CCG_elem_offset_no(key, elem, offset),
                                                        nullptr,
                                                        mask,
                                                        BKE_pbvh_make_vref(grid_vert_index),
                                                        thread_id,
                                                        &automask_data);

        float current_disp[3];
        float current_disp_norm[3];
        float interp_limit_surface_disp[3];

        copy_v3_v3(interp_limit_surface_disp, cache.prev_displacement[grid_vert_index]);

        switch (brush.smear_deform_type) {
          case BRUSH_SMEAR_DEFORM_DRAG:
            sub_v3_v3v3(current_disp, cache.location, cache.last_location);
            break;
          case BRUSH_SMEAR_DEFORM_PINCH:
            sub_v3_v3v3(current_disp, cache.location, co);
            break;
          case BRUSH_SMEAR_DEFORM_EXPAND:
            sub_v3_v3v3(current_disp, co, cache.location);
            break;
        }

        normalize_v3_v3(current_disp_norm, current_disp);
        mul_v3_v3fl(current_disp, current_disp_norm, cache.bstrength);

        float weights_accum = 1.0f;

        SculptVertexNeighborIter ni;
        SCULPT_VERTEX_NEIGHBORS_ITER_BEGIN (ss, BKE_pbvh_make_vref(grid_vert_index), ni) {
          float vertex_disp[3];
          float vertex_disp_norm[3];
          sub_v3_v3v3(vertex_disp,
                      cache.limit_surface_co[ni.index],
                      cache.limit_surface_co[grid_vert_index]);
          const float *neighbor_limit_surface_disp = cache.prev_displacement[ni.index];
          normalize_v3_v3(vertex_disp_norm, vertex_disp);

          if (dot_v3v3(current_disp_norm, vertex_disp_norm) >= 0.0f) {
            continue;
          }

          const float disp_interp = clamp_f(
              -dot_v3v3(current_disp_norm, vertex_disp_norm), 0.0f, 1.0f);
          madd_v3_v3fl(interp_limit_surface_disp, neighbor_limit_surface_disp, disp_interp);
          weights_accum += disp_interp;
        }
        SCULPT_VERTEX_NEIGHBORS_ITER_END(ni);

        mul_v3_fl(interp_limit_surface_disp, 1.0f / weights_accum);

        float new_co[3];
        add_v3_v3v3(new_co, cache.limit_surface_co[grid_vert_index], interp_limit_surface_disp);
        interp_v3_v3v3(co, co, new_co, fade);
        i++;
      }
    }
  }
}

BLI_NOINLINE static void eval_all_limit_positions(const SubdivCCG &subdiv_ccg,
                                                  const MutableSpan<float3> limit_positions)
{
  const CCGKey key = BKE_subdiv_ccg_key_top_level(subdiv_ccg);
  threading::parallel_for(subdiv_ccg.grids.index_range(), 1024, [&](const IndexRange range) {
    for (const int grid : range) {
      const MutableSpan grid_limit_positions = limit_positions.slice(grid * key.grid_area,
                                                                     key.grid_area);
      BKE_subdiv_ccg_eval_limit_positions(subdiv_ccg, key, grid, grid_limit_positions);
    }
  });
}

BLI_NOINLINE static void store_node_prev_displacement(const Span<float3> limit_positions,
                                                      const Span<CCGElem *> elems,
                                                      const CCGKey &key,
                                                      const PBVHNode &node,
                                                      const MutableSpan<float3> prev_displacement)
{
  for (const int grid : bke::pbvh::node_grid_indices(node)) {
    const int start = grid * key.grid_area;
    CCGElem *elem = elems[grid];
    for (const int i : IndexRange(key.grid_area)) {
      prev_displacement[start + i] = CCG_elem_offset_co(key, elem, i) - limit_positions[start + i];
    }
  }
}

}  // namespace multires_displacement_smear_cc

void do_displacement_smear_brush(const Sculpt &sd, Object &ob, Span<PBVHNode *> nodes)
{
  const Brush &brush = *BKE_paint_brush_for_read(&sd.paint);
  SculptSession &ss = *ob.sculpt;
  SubdivCCG &subdiv_ccg = *ss.subdiv_ccg;
  const Span<CCGElem *> elems = subdiv_ccg.grids;
  const CCGKey key = BKE_subdiv_ccg_key_top_level(subdiv_ccg);

  if (ss.cache->limit_surface_co.is_empty()) {
    ss.cache->prev_displacement = Array<float3>(elems.size() * key.grid_area);
    ss.cache->limit_surface_co = Array<float3>(elems.size() * key.grid_area);

    eval_all_limit_positions(subdiv_ccg, ss.cache->limit_surface_co);
  }

  threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
    for (const int i : range) {
      store_node_prev_displacement(
          ss.cache->limit_surface_co, elems, key, *nodes[i], ss.cache->prev_displacement);
    }
  });
  threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
    for (const int i : range) {
      calc_node(ob, brush, *nodes[i]);
    }
  });
}

}  // namespace blender::ed::sculpt_paint
