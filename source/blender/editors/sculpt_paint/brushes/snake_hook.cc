/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "editors/sculpt_paint/brushes/types.hh"

#include "DNA_brush_types.h"
#include "DNA_mesh_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BKE_kelvinlet.h"
#include "BKE_key.hh"
#include "BKE_mesh.hh"
#include "BKE_paint.hh"
#include "BKE_pbvh.hh"
#include "BKE_subdiv_ccg.hh"

#include "BLI_array.hh"
#include "BLI_enumerable_thread_specific.hh"
#include "BLI_math_matrix.hh"
#include "BLI_math_rotation.h"
#include "BLI_math_vector.h"
#include "BLI_math_vector.hh"
#include "BLI_task.h"
#include "BLI_task.hh"

#include "editors/sculpt_paint/mesh_brush_common.hh"
#include "editors/sculpt_paint/sculpt_intern.hh"

namespace blender::ed::sculpt_paint {

inline namespace snake_hook_cc {

struct LocalData {
  Vector<float> factors;
  Vector<float> distances;
  Vector<float3> translations;
};

struct SculptProjectVector {
  float3 plane;
  float len_sq;
  float len_sq_inv_neg;
  bool is_valid;
};

/**
 * \param plane: Direction, can be any length.
 */
static void sculpt_project_v3_cache_init(SculptProjectVector *spvc, const float3 &plane)
{
  spvc->plane = plane;
  spvc->len_sq = math::length_squared(spvc->plane);
  spvc->is_valid = (spvc->len_sq > FLT_EPSILON);
  spvc->len_sq_inv_neg = (spvc->is_valid) ? -1.0f / spvc->len_sq : 0.0f;
}

/**
 * Calculate the projection.
 */
static void sculpt_project_v3(const SculptProjectVector *spvc, const float3 &vec, float3 &r_vec)
{
#if 0
  project_plane_v3_v3v3(r_vec, vec, spvc->plane);
#else
  /* inline the projection, cache `-1.0 / dot_v3_v3(v_proj, v_proj)` */
  r_vec += spvc->plane * math::dot(vec, spvc->plane) * spvc->len_sq_inv_neg;
#endif
}

static float3 sculpt_rake_rotate(const SculptSession &ss,
                                 const float3 &sculpt_co,
                                 const float3 &v_co,
                                 float factor)
{
  float q_interp[4];
  float3 vec_rot = v_co - sculpt_co;

  copy_qt_qt(q_interp, ss.cache->rake_rotation_symmetry);
  pow_qt_fl_normalized(q_interp, factor);
  mul_qt_v3(q_interp, vec_rot);

  vec_rot += sculpt_co;
  return vec_rot - v_co;
}

static void do_snake_hook_brush_task(Object &ob,
                                     const Brush &brush,
                                     SculptProjectVector *spvc,
                                     const float3 &grab_delta,
                                     PBVHNode *node)
{
  SculptSession &ss = *ob.sculpt;

  PBVHVertexIter vd;
  const MutableSpan<float3> proxy = BKE_pbvh_node_add_proxy(*ss.pbvh, *node).co;
  const float bstrength = ss.cache->bstrength;
  const bool do_rake_rotation = ss.cache->is_rake_rotation_valid;
  const bool do_pinch = (brush.crease_pinch_factor != 0.5f);
  const float pinch = do_pinch ? (2.0f * (0.5f - brush.crease_pinch_factor) *
                                  (math::length(grab_delta) / ss.cache->radius)) :
                                 0.0f;

  const bool do_elastic = brush.snake_hook_deform_type == BRUSH_SNAKE_HOOK_DEFORM_ELASTIC;

  SculptBrushTest test;
  SculptBrushTestFn sculpt_brush_test_sq_fn = SCULPT_brush_test_init_with_falloff_shape(
      ss, test, brush.falloff_shape);
  const int thread_id = BLI_task_parallel_thread_id(nullptr);

  KelvinletParams params;
  BKE_kelvinlet_init_params(&params, ss.cache->radius, bstrength, 1.0f, 0.4f);

  auto_mask::NodeData automask_data = auto_mask::node_begin(
      ob, ss.cache->automasking.get(), *node);

  BKE_pbvh_vertex_iter_begin (*ss.pbvh, node, vd, PBVH_ITER_UNIQUE) {
    if (!do_elastic && !sculpt_brush_test_sq_fn(test, vd.co)) {
      continue;
    }

    float fade;
    if (do_elastic) {
      fade = 1.0f;
    }
    else {
      auto_mask::node_update(automask_data, vd);

      fade = bstrength * SCULPT_brush_strength_factor(ss,
                                                      brush,
                                                      vd.co,
                                                      sqrtf(test.dist),
                                                      vd.no,
                                                      vd.fno,
                                                      vd.mask,
                                                      vd.vertex,
                                                      thread_id,
                                                      &automask_data);
    }

    proxy[vd.i] += grab_delta * fade;

    /* Negative pinch will inflate, helps maintain volume. */
    if (do_pinch) {
      float3 delta_pinch = float3(vd.co) - test.location;

      if (brush.falloff_shape == PAINT_FALLOFF_SHAPE_TUBE) {
        project_plane_v3_v3v3(delta_pinch, delta_pinch, ss.cache->true_view_normal);
      }

      /* Important to calculate based on the grabbed location
       * (intentionally ignore fade here). */
      delta_pinch += grab_delta;

      sculpt_project_v3(spvc, delta_pinch, delta_pinch);

      float3 delta_pinch_init = delta_pinch;

      float pinch_fade = pinch * fade;
      /* When reducing, scale reduction back by how close to the center we are,
       * so we don't pinch into nothingness. */
      if (pinch > 0.0f) {
        /* Square to have even less impact for close vertices. */
        pinch_fade *= pow2f(std::min(1.0f, math::length(delta_pinch) / ss.cache->radius));
      }
      delta_pinch *= (1.0f + pinch_fade);
      delta_pinch = delta_pinch_init - delta_pinch;
      proxy[vd.i] += delta_pinch;
    }

    if (do_rake_rotation) {
      proxy[vd.i] += sculpt_rake_rotate(ss, test.location, vd.co, fade);
    }

    if (do_elastic) {
      float3 disp;
      BKE_kelvinlet_grab_triscale(disp, &params, vd.co, ss.cache->location, proxy[vd.i]);
      fade = 1.0f;
      fade *= bstrength * 20.0f;
      fade *= (1.0f - vd.mask);
      fade *= auto_mask::factor_get(ss.cache->automasking.get(), ss, vd.vertex, &automask_data);
      proxy[vd.i] = disp * fade;
    }
  }
  BKE_pbvh_vertex_iter_end;
}

}  // namespace snake_hook_cc

void do_snake_hook_brush(const Sculpt &sd, Object &ob, Span<PBVHNode *> nodes)
{
  SculptSession &ss = *ob.sculpt;
  const Brush &brush = *BKE_paint_brush_for_read(&sd.paint);
  const float bstrength = ss.cache->bstrength;

  SculptProjectVector spvc;

  float3 grab_delta = ss.cache->grab_delta_symmetry;

  if (bstrength < 0.0f) {
    grab_delta *= -1.0f;
  }

  if (ss.cache->normal_weight > 0.0f) {
    sculpt_project_v3_normal_align(ss, ss.cache->normal_weight, grab_delta);
  }

  /* Optionally pinch while painting. */
  if (brush.crease_pinch_factor != 0.5f) {
    sculpt_project_v3_cache_init(&spvc, grab_delta);
  }

  threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
    for (const int i : range) {
      do_snake_hook_brush_task(ob, brush, &spvc, grab_delta, nodes[i]);
    }
  });
}

}  // namespace blender::ed::sculpt_paint
