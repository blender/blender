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

#include "BLI_math_matrix.hh"
#include "BLI_math_vector.hh"
#include "BLI_task.h"
#include "BLI_task.hh"

#include "editors/sculpt_paint/mesh_brush_common.hh"
#include "editors/sculpt_paint/sculpt_intern.hh"

namespace blender::ed::sculpt_paint {

inline namespace multires_displacement_eraser_cc {

static void calc_node(
    const Sculpt &sd, Object &object, const Brush &brush, const float strength, PBVHNode &node)
{
  SculptSession &ss = *object.sculpt;

  SculptBrushTest test;
  SculptBrushTestFn sculpt_brush_test_sq_fn = SCULPT_brush_test_init_with_falloff_shape(
      ss, test, brush.falloff_shape);
  const int thread_id = BLI_task_parallel_thread_id(nullptr);
  auto_mask::NodeData automask_data = auto_mask::node_begin(
      object, ss.cache->automasking.get(), node);

  SubdivCCG &subdiv_ccg = *ss.subdiv_ccg;
  const CCGKey key = *BKE_pbvh_get_grid_key(*ss.pbvh);
  const Span<CCGElem *> grids = subdiv_ccg.grids;
  const BitGroupVector<> &grid_hidden = subdiv_ccg.grid_hidden;

  int i = 0;
  for (const int grid : bke::pbvh::node_grid_indices(node)) {
    const int grid_verts_start = grid * key.grid_area;
    CCGElem *elem = grids[grid];
    for (const int y : IndexRange(key.grid_size)) {
      for (const int x : IndexRange(key.grid_size)) {
        const int offset = CCG_grid_xy_to_index(key.grid_size, x, y);
        if (!grid_hidden.is_empty() && grid_hidden[grid][offset]) {
          i++;
          continue;
        }
        float3 &co = CCG_elem_offset_co(key, elem, offset);
        if (!sculpt_brush_test_sq_fn(test, co)) {
          i++;
          continue;
        }
        auto_mask::node_update(automask_data, i);
        const float fade = SCULPT_brush_strength_factor(
            ss,
            brush,
            co,
            math::sqrt(test.dist),
            CCG_elem_offset_no(key, elem, offset),
            nullptr,
            key.has_mask ? CCG_elem_offset_mask(key, elem, offset) : 0.0f,
            BKE_pbvh_make_vref(grid_verts_start + offset),
            thread_id,
            &automask_data);

        SubdivCCGCoord coord{};
        coord.grid_index = grid;
        coord.x = x;
        coord.y = y;
        float3 limit_co;
        BKE_subdiv_ccg_eval_limit_point(*ss.subdiv_ccg, coord, limit_co);

        const float3 translation = (limit_co - co) * fade * strength;

        SCULPT_clip(sd, ss, co, co + translation);

        i++;
      }
    }
  }
}

}  // namespace multires_displacement_eraser_cc

void do_displacement_eraser_brush(const Sculpt &sd, Object &object, Span<PBVHNode *> nodes)
{
  SculptSession &ss = *object.sculpt;
  const Brush &brush = *BKE_paint_brush_for_read(&sd.paint);
  const float strength = std::min(ss.cache->bstrength, 1.0f);

  threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
    for (const int i : range) {
      calc_node(sd, object, brush, strength, *nodes[i]);
    }
  });
}

}  // namespace blender::ed::sculpt_paint
