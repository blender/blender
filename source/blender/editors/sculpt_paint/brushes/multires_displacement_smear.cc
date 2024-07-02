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

static void do_displacement_smear_brush_task(Object &ob, const Brush &brush, PBVHNode *node)
{
  SculptSession &ss = *ob.sculpt;
  const float bstrength = clamp_f(ss.cache->bstrength, 0.0f, 1.0f);

  SculptBrushTest test;
  SculptBrushTestFn sculpt_brush_test_sq_fn = SCULPT_brush_test_init_with_falloff_shape(
      ss, test, brush.falloff_shape);
  const int thread_id = BLI_task_parallel_thread_id(nullptr);

  auto_mask::NodeData automask_data = auto_mask::node_begin(
      ob, ss.cache->automasking.get(), *node);

  PBVHVertexIter vd;
  BKE_pbvh_vertex_iter_begin (*ss.pbvh, node, vd, PBVH_ITER_UNIQUE) {
    if (!sculpt_brush_test_sq_fn(test, vd.co)) {
      continue;
    }
    auto_mask::node_update(automask_data, vd);

    const float fade = bstrength * SCULPT_brush_strength_factor(ss,
                                                                brush,
                                                                vd.co,
                                                                sqrtf(test.dist),
                                                                vd.no,
                                                                vd.fno,
                                                                vd.mask,
                                                                vd.vertex,
                                                                thread_id,
                                                                &automask_data);

    float current_disp[3];
    float current_disp_norm[3];
    float interp_limit_surface_disp[3];

    copy_v3_v3(interp_limit_surface_disp, ss.cache->prev_displacement[vd.index]);

    switch (brush.smear_deform_type) {
      case BRUSH_SMEAR_DEFORM_DRAG:
        sub_v3_v3v3(current_disp, ss.cache->location, ss.cache->last_location);
        break;
      case BRUSH_SMEAR_DEFORM_PINCH:
        sub_v3_v3v3(current_disp, ss.cache->location, vd.co);
        break;
      case BRUSH_SMEAR_DEFORM_EXPAND:
        sub_v3_v3v3(current_disp, vd.co, ss.cache->location);
        break;
    }

    normalize_v3_v3(current_disp_norm, current_disp);
    mul_v3_v3fl(current_disp, current_disp_norm, ss.cache->bstrength);

    float weights_accum = 1.0f;

    SculptVertexNeighborIter ni;
    SCULPT_VERTEX_NEIGHBORS_ITER_BEGIN (ss, vd.vertex, ni) {
      float vertex_disp[3];
      float vertex_disp_norm[3];
      sub_v3_v3v3(
          vertex_disp, ss.cache->limit_surface_co[ni.index], ss.cache->limit_surface_co[vd.index]);
      const float *neighbor_limit_surface_disp = ss.cache->prev_displacement[ni.index];
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
    add_v3_v3v3(new_co, ss.cache->limit_surface_co[vd.index], interp_limit_surface_disp);
    interp_v3_v3v3(vd.co, vd.co, new_co, fade);
  }
  BKE_pbvh_vertex_iter_end;
}

static void do_displacement_smear_store_prev_disp_task(SculptSession &ss, PBVHNode *node)
{
  PBVHVertexIter vd;
  BKE_pbvh_vertex_iter_begin (*ss.pbvh, node, vd, PBVH_ITER_UNIQUE) {
    sub_v3_v3v3(ss.cache->prev_displacement[vd.index],
                SCULPT_vertex_co_get(ss, vd.vertex),
                ss.cache->limit_surface_co[vd.index]);
  }
  BKE_pbvh_vertex_iter_end;
}

}  // namespace multires_displacement_smear_cc

void do_displacement_smear_brush(const Sculpt &sd, Object &ob, Span<PBVHNode *> nodes)
{
  const Brush &brush = *BKE_paint_brush_for_read(&sd.paint);
  SculptSession &ss = *ob.sculpt;

  const int totvert = SCULPT_vertex_count_get(ss);
  if (ss.cache->prev_displacement.is_empty()) {
    ss.cache->prev_displacement = Array<float3>(totvert);
    ss.cache->limit_surface_co = Array<float3>(totvert);
    for (int i = 0; i < totvert; i++) {
      PBVHVertRef vertex = BKE_pbvh_index_to_vertex(*ss.pbvh, i);

      ss.cache->limit_surface_co[i] = SCULPT_vertex_limit_surface_get(ss, vertex);
      sub_v3_v3v3(ss.cache->prev_displacement[i],
                  SCULPT_vertex_co_get(ss, vertex),
                  ss.cache->limit_surface_co[i]);
    }
  }

  threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
    for (const int i : range) {
      do_displacement_smear_store_prev_disp_task(ss, nodes[i]);
    }
  });
  threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
    for (const int i : range) {
      do_displacement_smear_brush_task(ob, brush, nodes[i]);
    }
  });
}

}  // namespace blender::ed::sculpt_paint
