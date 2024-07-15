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
#include "BLI_math_vector.h"
#include "BLI_math_vector.hh"
#include "BLI_task.h"
#include "BLI_task.hh"

#include "editors/sculpt_paint/mesh_brush_common.hh"
#include "editors/sculpt_paint/sculpt_intern.hh"

namespace blender::ed::sculpt_paint {

inline namespace layer_cc {

static void do_layer_brush_task(Object &ob, const Sculpt &sd, const Brush &brush, PBVHNode *node)
{
  SculptSession &ss = *ob.sculpt;

  const bool use_persistent_base = !ss.bm && ss.attrs.persistent_co &&
                                   brush.flag & BRUSH_PERSISTENT;

  PBVHVertexIter vd;
  const float bstrength = ss.cache->bstrength;
  SculptOrigVertData orig_data = SCULPT_orig_vert_data_init(ob, *node, undo::Type::Position);

  SculptBrushTest test;
  SculptBrushTestFn sculpt_brush_test_sq_fn = SCULPT_brush_test_init_with_falloff_shape(
      ss, test, brush.falloff_shape);
  const int thread_id = BLI_task_parallel_thread_id(nullptr);

  auto_mask::NodeData automask_data = auto_mask::node_begin(
      ob, ss.cache->automasking.get(), *node);

  BKE_pbvh_vertex_iter_begin (*ss.pbvh, node, vd, PBVH_ITER_UNIQUE) {
    SCULPT_orig_vert_data_update(orig_data, vd);

    if (!sculpt_brush_test_sq_fn(test, orig_data.co)) {
      continue;
    }
    auto_mask::node_update(automask_data, vd);

    const float fade = SCULPT_brush_strength_factor(ss,
                                                    brush,
                                                    vd.co,
                                                    sqrtf(test.dist),
                                                    vd.no,
                                                    vd.fno,
                                                    vd.mask,
                                                    vd.vertex,
                                                    thread_id,
                                                    &automask_data);

    const int vi = vd.index;
    float *disp_factor;
    if (use_persistent_base) {
      disp_factor = (float *)SCULPT_vertex_attr_get(vd.vertex, ss.attrs.persistent_disp);
    }
    else {
      disp_factor = &ss.cache->layer_displacement_factor[vi];
    }

    /* When using persistent base, the layer brush (holding Control) invert mode resets the
     * height of the layer to 0. This makes possible to clean edges of previously added layers
     * on top of the base. */
    /* The main direction of the layers is inverted using the regular brush strength with the
     * brush direction property. */
    if (use_persistent_base && ss.cache->invert) {
      (*disp_factor) += fabsf(fade * bstrength * (*disp_factor)) *
                        ((*disp_factor) > 0.0f ? -1.0f : 1.0f);
    }
    else {
      (*disp_factor) += fade * bstrength * (1.05f - fabsf(*disp_factor));
    }
    const float clamp_mask = 1.0f - vd.mask;
    *disp_factor = clamp_f(*disp_factor, -clamp_mask, clamp_mask);

    float final_co[3];
    float3 normal;

    if (use_persistent_base) {
      normal = SCULPT_vertex_persistent_normal_get(ss, vd.vertex);
      mul_v3_fl(normal, brush.height);
      madd_v3_v3v3fl(
          final_co, SCULPT_vertex_persistent_co_get(ss, vd.vertex), normal, *disp_factor);
    }
    else {
      copy_v3_v3(normal, orig_data.no);
      mul_v3_fl(normal, brush.height);
      madd_v3_v3v3fl(final_co, orig_data.co, normal, *disp_factor);
    }

    float vdisp[3];
    sub_v3_v3v3(vdisp, final_co, vd.co);
    mul_v3_fl(vdisp, fabsf(fade));
    add_v3_v3v3(final_co, vd.co, vdisp);

    SCULPT_clip(sd, ss, vd.co, final_co);
  }
  BKE_pbvh_vertex_iter_end;
}

}  // namespace layer_cc

void do_layer_brush(const Sculpt &sd, Object &object, Span<PBVHNode *> nodes)
{
  SculptSession &ss = *object.sculpt;
  const Brush &brush = *BKE_paint_brush_for_read(&sd.paint);

  if (ss.cache->layer_displacement_factor.is_empty()) {
    ss.cache->layer_displacement_factor = Array<float>(SCULPT_vertex_count_get(ss), 0.0f);
  }

  threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
    for (const int i : range) {
      do_layer_brush_task(object, sd, brush, nodes[i]);
    }
  });
}

}  // namespace blender::ed::sculpt_paint
