/* SPDX-FileCopyrightText: 2006 by Nicholas Bishop. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edsculpt
 * Implements the Sculpt Mode tools.
 */

#include "MEM_guardedalloc.h"

#include "BLI_ghash.h"
#include "BLI_math_geom.h"
#include "BLI_math_matrix.h"
#include "BLI_math_rotation.h"
#include "BLI_math_vector.h"
#include "BLI_math_vector.hh"
#include "BLI_span.hh"
#include "BLI_task.h"
#include "BLI_utildefines.h"

#include "DNA_brush_types.h"
#include "DNA_customdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BKE_brush.hh"
#include "BKE_ccg.hh"
#include "BKE_colortools.hh"
#include "BKE_kelvinlet.h"
#include "BKE_paint.hh"
#include "BKE_pbvh_api.hh"

#include "ED_view3d.hh"

#include "paint_intern.hh"
#include "sculpt_intern.hh"

#include "bmesh.hh"

#include <cmath>
#include <cstdlib>
#include <cstring>

using blender::float3;
using blender::MutableSpan;
using blender::Span;

/* -------------------------------------------------------------------- */
/** \name SculptProjectVector
 *
 * Fast-path for #project_plane_v3_v3v3
 * \{ */

struct SculptProjectVector {
  float plane[3];
  float len_sq;
  float len_sq_inv_neg;
  bool is_valid;
};

/**
 * \param plane: Direction, can be any length.
 */
static void sculpt_project_v3_cache_init(SculptProjectVector *spvc, const float plane[3])
{
  copy_v3_v3(spvc->plane, plane);
  spvc->len_sq = len_squared_v3(spvc->plane);
  spvc->is_valid = (spvc->len_sq > FLT_EPSILON);
  spvc->len_sq_inv_neg = (spvc->is_valid) ? -1.0f / spvc->len_sq : 0.0f;
}

/**
 * Calculate the projection.
 */
static void sculpt_project_v3(const SculptProjectVector *spvc, const float vec[3], float r_vec[3])
{
#if 0
  project_plane_v3_v3v3(r_vec, vec, spvc->plane);
#else
  /* inline the projection, cache `-1.0 / dot_v3_v3(v_proj, v_proj)` */
  madd_v3_v3fl(r_vec, spvc->plane, dot_v3v3(vec, spvc->plane) * spvc->len_sq_inv_neg);
#endif
}

static void sculpt_rake_rotate(const SculptSession &ss,
                               const float sculpt_co[3],
                               const float v_co[3],
                               float factor,
                               float r_delta[3])
{
  float vec_rot[3];

#if 0
  /* lerp */
  sub_v3_v3v3(vec_rot, v_co, sculpt_co);
  mul_qt_v3(ss.cache->rake_rotation_symmetry, vec_rot);
  add_v3_v3(vec_rot, sculpt_co);
  sub_v3_v3v3(r_delta, vec_rot, v_co);
  mul_v3_fl(r_delta, factor);
#else
  /* slerp */
  float q_interp[4];
  sub_v3_v3v3(vec_rot, v_co, sculpt_co);

  copy_qt_qt(q_interp, ss.cache->rake_rotation_symmetry);
  pow_qt_fl_normalized(q_interp, factor);
  mul_qt_v3(q_interp, vec_rot);

  add_v3_v3(vec_rot, sculpt_co);
  sub_v3_v3v3(r_delta, vec_rot, v_co);
#endif
}

void sculpt_project_v3_normal_align(const SculptSession &ss,
                                    const float normal_weight,
                                    float grab_delta[3])
{
  /* Signed to support grabbing in (to make a hole) as well as out. */
  const float len_signed = dot_v3v3(ss.cache->sculpt_normal_symm, grab_delta);

  /* This scale effectively projects the offset so dragging follows the cursor,
   * as the normal points towards the view, the scale increases. */
  float len_view_scale;
  {
    float view_aligned_normal[3];
    project_plane_v3_v3v3(
        view_aligned_normal, ss.cache->sculpt_normal_symm, ss.cache->view_normal);
    len_view_scale = fabsf(dot_v3v3(view_aligned_normal, ss.cache->sculpt_normal_symm));
    len_view_scale = (len_view_scale > FLT_EPSILON) ? 1.0f / len_view_scale : 1.0f;
  }

  mul_v3_fl(grab_delta, 1.0f - normal_weight);
  madd_v3_v3fl(
      grab_delta, ss.cache->sculpt_normal_symm, (len_signed * normal_weight) * len_view_scale);
}

/** \} */

static void do_snake_hook_brush_task(Object &ob,
                                     const Brush &brush,
                                     SculptProjectVector *spvc,
                                     const float *grab_delta,
                                     PBVHNode *node)
{
  using namespace blender::ed::sculpt_paint;
  SculptSession &ss = *ob.sculpt;

  PBVHVertexIter vd;
  const MutableSpan<float3> proxy = BKE_pbvh_node_add_proxy(*ss.pbvh, *node).co;
  const float bstrength = ss.cache->bstrength;
  const bool do_rake_rotation = ss.cache->is_rake_rotation_valid;
  const bool do_pinch = (brush.crease_pinch_factor != 0.5f);
  const float pinch = do_pinch ? (2.0f * (0.5f - brush.crease_pinch_factor) *
                                  (len_v3(grab_delta) / ss.cache->radius)) :
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

    mul_v3_v3fl(proxy[vd.i], grab_delta, fade);

    /* Negative pinch will inflate, helps maintain volume. */
    if (do_pinch) {
      float delta_pinch_init[3], delta_pinch[3];

      sub_v3_v3v3(delta_pinch, vd.co, test.location);
      if (brush.falloff_shape == PAINT_FALLOFF_SHAPE_TUBE) {
        project_plane_v3_v3v3(delta_pinch, delta_pinch, ss.cache->true_view_normal);
      }

      /* Important to calculate based on the grabbed location
       * (intentionally ignore fade here). */
      add_v3_v3(delta_pinch, grab_delta);

      sculpt_project_v3(spvc, delta_pinch, delta_pinch);

      copy_v3_v3(delta_pinch_init, delta_pinch);

      float pinch_fade = pinch * fade;
      /* When reducing, scale reduction back by how close to the center we are,
       * so we don't pinch into nothingness. */
      if (pinch > 0.0f) {
        /* Square to have even less impact for close vertices. */
        pinch_fade *= pow2f(min_ff(1.0f, len_v3(delta_pinch) / ss.cache->radius));
      }
      mul_v3_fl(delta_pinch, 1.0f + pinch_fade);
      sub_v3_v3v3(delta_pinch, delta_pinch_init, delta_pinch);
      add_v3_v3(proxy[vd.i], delta_pinch);
    }

    if (do_rake_rotation) {
      float delta_rotate[3];
      sculpt_rake_rotate(ss, test.location, vd.co, fade, delta_rotate);
      add_v3_v3(proxy[vd.i], delta_rotate);
    }

    if (do_elastic) {
      float disp[3];
      BKE_kelvinlet_grab_triscale(disp, &params, vd.co, ss.cache->location, proxy[vd.i]);
      mul_v3_fl(disp, bstrength * 20.0f);
      mul_v3_fl(disp, 1.0f - vd.mask);
      mul_v3_fl(disp,
                auto_mask::factor_get(ss.cache->automasking.get(), ss, vd.vertex, &automask_data));
      copy_v3_v3(proxy[vd.i], disp);
    }
  }
  BKE_pbvh_vertex_iter_end;
}

void SCULPT_do_snake_hook_brush(const Sculpt &sd, Object &ob, Span<PBVHNode *> nodes)
{
  using namespace blender;
  SculptSession &ss = *ob.sculpt;
  const Brush &brush = *BKE_paint_brush_for_read(&sd.paint);
  const float bstrength = ss.cache->bstrength;
  float grab_delta[3];

  SculptProjectVector spvc;

  copy_v3_v3(grab_delta, ss.cache->grab_delta_symmetry);

  if (bstrength < 0.0f) {
    negate_v3(grab_delta);
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

static void do_layer_brush_task(Object &ob, const Sculpt &sd, const Brush &brush, PBVHNode *node)
{
  using namespace blender::ed::sculpt_paint;
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

void SCULPT_do_layer_brush(const Sculpt &sd, Object &ob, Span<PBVHNode *> nodes)
{
  using namespace blender;
  SculptSession &ss = *ob.sculpt;
  const Brush &brush = *BKE_paint_brush_for_read(&sd.paint);

  if (ss.cache->layer_displacement_factor == nullptr) {
    ss.cache->layer_displacement_factor = MEM_cnew_array<float>(SCULPT_vertex_count_get(ss),
                                                                __func__);
  }

  threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
    for (const int i : range) {
      do_layer_brush_task(ob, sd, brush, nodes[i]);
    }
  });
}

static void do_elastic_deform_brush_task(Object &ob,
                                         const Brush &brush,
                                         const float *grab_delta,
                                         PBVHNode *node)
{
  using namespace blender::ed::sculpt_paint;
  SculptSession &ss = *ob.sculpt;
  const float *location = ss.cache->location;

  PBVHVertexIter vd;
  const MutableSpan<float3> proxy = BKE_pbvh_node_add_proxy(*ss.pbvh, *node).co;

  const float bstrength = ss.cache->bstrength;

  SculptOrigVertData orig_data = SCULPT_orig_vert_data_init(ob, *node, undo::Type::Position);
  auto_mask::NodeData automask_data = auto_mask::node_begin(
      ob, ss.cache->automasking.get(), *node);

  float dir;
  if (ss.cache->mouse[0] > ss.cache->initial_mouse[0]) {
    dir = 1.0f;
  }
  else {
    dir = -1.0f;
  }

  if (brush.elastic_deform_type == BRUSH_ELASTIC_DEFORM_TWIST) {
    int symm = ss.cache->mirror_symmetry_pass;
    if (ELEM(symm, 1, 2, 4, 7)) {
      dir = -dir;
    }
  }

  KelvinletParams params;
  float force = len_v3(grab_delta) * dir * bstrength;
  BKE_kelvinlet_init_params(
      &params, ss.cache->radius, force, 1.0f, brush.elastic_deform_volume_preservation);

  BKE_pbvh_vertex_iter_begin (*ss.pbvh, node, vd, PBVH_ITER_UNIQUE) {
    SCULPT_orig_vert_data_update(orig_data, vd);
    auto_mask::node_update(automask_data, vd);

    float final_disp[3];
    switch (brush.elastic_deform_type) {
      case BRUSH_ELASTIC_DEFORM_GRAB:
        BKE_kelvinlet_grab(final_disp, &params, orig_data.co, location, grab_delta);
        mul_v3_fl(final_disp, bstrength * 20.0f);
        break;
      case BRUSH_ELASTIC_DEFORM_GRAB_BISCALE: {
        BKE_kelvinlet_grab_biscale(final_disp, &params, orig_data.co, location, grab_delta);
        mul_v3_fl(final_disp, bstrength * 20.0f);
        break;
      }
      case BRUSH_ELASTIC_DEFORM_GRAB_TRISCALE: {
        BKE_kelvinlet_grab_triscale(final_disp, &params, orig_data.co, location, grab_delta);
        mul_v3_fl(final_disp, bstrength * 20.0f);
        break;
      }
      case BRUSH_ELASTIC_DEFORM_SCALE:
        BKE_kelvinlet_scale(
            final_disp, &params, orig_data.co, location, ss.cache->sculpt_normal_symm);
        break;
      case BRUSH_ELASTIC_DEFORM_TWIST:
        BKE_kelvinlet_twist(
            final_disp, &params, orig_data.co, location, ss.cache->sculpt_normal_symm);
        break;
    }

    mul_v3_fl(final_disp, 1.0f - vd.mask);

    mul_v3_fl(final_disp,
              auto_mask::factor_get(ss.cache->automasking.get(), ss, vd.vertex, &automask_data));

    copy_v3_v3(proxy[vd.i], final_disp);
  }
  BKE_pbvh_vertex_iter_end;
}

void SCULPT_do_elastic_deform_brush(const Sculpt &sd, Object &ob, Span<PBVHNode *> nodes)
{
  using namespace blender;
  SculptSession &ss = *ob.sculpt;
  const Brush &brush = *BKE_paint_brush_for_read(&sd.paint);
  float grab_delta[3];

  copy_v3_v3(grab_delta, ss.cache->grab_delta_symmetry);

  if (ss.cache->normal_weight > 0.0f) {
    sculpt_project_v3_normal_align(ss, ss.cache->normal_weight, grab_delta);
  }

  threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
    for (const int i : range) {
      do_elastic_deform_brush_task(ob, brush, grab_delta, nodes[i]);
    }
  });
}

/* -------------------------------------------------------------------- */
/** \name Sculpt Draw Sharp Brush
 * \{ */

static void do_draw_sharp_brush_task(Object &ob,
                                     const Brush &brush,
                                     const float *offset,
                                     PBVHNode *node)
{
  using namespace blender::ed::sculpt_paint;
  SculptSession &ss = *ob.sculpt;

  PBVHVertexIter vd;
  SculptOrigVertData orig_data = SCULPT_orig_vert_data_init(ob, *node, undo::Type::Position);
  const MutableSpan<float3> proxy = BKE_pbvh_node_add_proxy(*ss.pbvh, *node).co;

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
    /* Offset vertex. */
    auto_mask::node_update(automask_data, vd);

    const float fade = SCULPT_brush_strength_factor(ss,
                                                    brush,
                                                    orig_data.co,
                                                    sqrtf(test.dist),
                                                    orig_data.no,
                                                    nullptr,
                                                    vd.mask,
                                                    vd.vertex,
                                                    thread_id,
                                                    &automask_data);

    mul_v3_v3fl(proxy[vd.i], offset, fade);
  }
  BKE_pbvh_vertex_iter_end;
}

void SCULPT_do_draw_sharp_brush(const Sculpt &sd, Object &ob, Span<PBVHNode *> nodes)
{
  using namespace blender;
  SculptSession &ss = *ob.sculpt;
  const Brush &brush = *BKE_paint_brush_for_read(&sd.paint);
  float offset[3];
  const float bstrength = ss.cache->bstrength;

  /* Offset with as much as possible factored in already. */
  float effective_normal[3];
  SCULPT_tilt_effective_normal_get(ss, brush, effective_normal);
  mul_v3_v3fl(offset, effective_normal, ss.cache->radius);
  mul_v3_v3(offset, ss.cache->scale);
  mul_v3_fl(offset, bstrength);

  /* XXX: this shouldn't be necessary, but sculpting crashes in blender2.8 otherwise
   * initialize before threads so they can do curve mapping. */
  BKE_curvemapping_init(brush.curve);

  threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
    for (const int i : range) {
      do_draw_sharp_brush_task(ob, brush, offset, nodes[i]);
    }
  });
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Sculpt Topology Brush
 * \{ */

static void do_topology_slide_task(Object &ob, const Brush &brush, PBVHNode *node)
{
  using namespace blender::ed::sculpt_paint;
  SculptSession &ss = *ob.sculpt;

  PBVHVertexIter vd;
  const MutableSpan<float3> proxy = BKE_pbvh_node_add_proxy(*ss.pbvh, *node).co;

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
                                                    orig_data.co,
                                                    sqrtf(test.dist),
                                                    orig_data.no,
                                                    nullptr,
                                                    vd.mask,
                                                    vd.vertex,
                                                    thread_id,
                                                    &automask_data);
    float current_disp[3];
    float current_disp_norm[3];
    float final_disp[3] = {0.0f, 0.0f, 0.0f};

    switch (brush.slide_deform_type) {
      case BRUSH_SLIDE_DEFORM_DRAG:
        sub_v3_v3v3(current_disp, ss.cache->location, ss.cache->last_location);
        break;
      case BRUSH_SLIDE_DEFORM_PINCH:
        sub_v3_v3v3(current_disp, ss.cache->location, vd.co);
        break;
      case BRUSH_SLIDE_DEFORM_EXPAND:
        sub_v3_v3v3(current_disp, vd.co, ss.cache->location);
        break;
    }

    normalize_v3_v3(current_disp_norm, current_disp);
    mul_v3_v3fl(current_disp, current_disp_norm, ss.cache->bstrength);

    SculptVertexNeighborIter ni;
    SCULPT_VERTEX_NEIGHBORS_ITER_BEGIN (ss, vd.vertex, ni) {
      float vertex_disp[3];
      float vertex_disp_norm[3];
      sub_v3_v3v3(vertex_disp, SCULPT_vertex_co_get(ss, ni.vertex), vd.co);
      normalize_v3_v3(vertex_disp_norm, vertex_disp);
      if (dot_v3v3(current_disp_norm, vertex_disp_norm) > 0.0f) {
        madd_v3_v3fl(final_disp, vertex_disp_norm, dot_v3v3(current_disp, vertex_disp));
      }
    }
    SCULPT_VERTEX_NEIGHBORS_ITER_END(ni);

    mul_v3_v3fl(proxy[vd.i], final_disp, fade);
  }
  BKE_pbvh_vertex_iter_end;
}

namespace blender::ed::sculpt_paint::smooth {

static void relax_vertex_interior(SculptSession &ss,
                                  PBVHVertexIter *vd,
                                  const float factor,
                                  const bool filter_boundary_face_sets,
                                  float *r_final_pos)
{
  float smooth_pos[3];
  int avg_count = 0;
  int neighbor_count = 0;
  zero_v3(smooth_pos);

  SculptVertexNeighborIter ni;
  SCULPT_VERTEX_NEIGHBORS_ITER_BEGIN (ss, vd->vertex, ni) {
    neighbor_count++;
    if (!filter_boundary_face_sets ||
        (filter_boundary_face_sets && !face_set::vert_has_unique_face_set(ss, ni.vertex)))
    {

      add_v3_v3(smooth_pos, SCULPT_vertex_co_get(ss, ni.vertex));
      avg_count++;
    }
  }
  SCULPT_VERTEX_NEIGHBORS_ITER_END(ni);

  /* Don't modify corner vertices. */
  if (neighbor_count <= 2) {
    copy_v3_v3(r_final_pos, vd->co);
    return;
  }

  if (avg_count == 0) {
    copy_v3_v3(r_final_pos, vd->co);
    return;
  }

  mul_v3_fl(smooth_pos, 1.0f / avg_count);

  const float3 vno = SCULPT_vertex_normal_get(ss, vd->vertex);

  if (is_zero_v3(vno)) {
    copy_v3_v3(r_final_pos, vd->co);
    return;
  }

  float plane[4];
  plane_from_point_normal_v3(plane, vd->co, vno);

  float smooth_closest_plane[3];
  closest_to_plane_v3(smooth_closest_plane, plane, smooth_pos);

  float final_disp[3];
  sub_v3_v3v3(final_disp, smooth_closest_plane, vd->co);

  mul_v3_fl(final_disp, factor);
  add_v3_v3v3(r_final_pos, vd->co, final_disp);
}

static void relax_vertex_boundary(SculptSession &ss,
                                  PBVHVertexIter *vd,
                                  const float factor,
                                  const bool filter_boundary_face_sets,
                                  float *r_final_pos)
{
  float smooth_pos[3];
  float boundary_normal[3];
  int avg_count = 0;
  int neighbor_count = 0;
  zero_v3(smooth_pos);
  zero_v3(boundary_normal);

  SculptVertexNeighborIter ni;
  SCULPT_VERTEX_NEIGHBORS_ITER_BEGIN (ss, vd->vertex, ni) {
    neighbor_count++;
    if (!filter_boundary_face_sets ||
        (filter_boundary_face_sets && !face_set::vert_has_unique_face_set(ss, ni.vertex)))
    {

      /* When the vertex to relax is boundary, use only connected boundary vertices for the average
       * position. */
      if (!SCULPT_vertex_is_boundary(ss, ni.vertex)) {
        continue;
      }
      add_v3_v3(smooth_pos, SCULPT_vertex_co_get(ss, ni.vertex));
      avg_count++;

      /* Calculate a normal for the constraint plane using the edges of the boundary. */
      float to_neighbor[3];
      sub_v3_v3v3(to_neighbor, SCULPT_vertex_co_get(ss, ni.vertex), vd->co);
      normalize_v3(to_neighbor);
      add_v3_v3(boundary_normal, to_neighbor);
    }
  }
  SCULPT_VERTEX_NEIGHBORS_ITER_END(ni);

  /* Don't modify corner vertices. */
  if (neighbor_count <= 2) {
    copy_v3_v3(r_final_pos, vd->co);
    return;
  }

  if (avg_count == 0) {
    copy_v3_v3(r_final_pos, vd->co);
    return;
  }

  mul_v3_fl(smooth_pos, 1.0f / avg_count);

  float3 vno;
  if (avg_count == 2) {
    normalize_v3_v3(vno, boundary_normal);
  }
  else {
    vno = SCULPT_vertex_normal_get(ss, vd->vertex);
  }

  if (is_zero_v3(vno)) {
    copy_v3_v3(r_final_pos, vd->co);
    return;
  }

  float plane[4];
  plane_from_point_normal_v3(plane, vd->co, vno);

  float smooth_closest_plane[3];
  closest_to_plane_v3(smooth_closest_plane, plane, smooth_pos);

  float final_disp[3];
  sub_v3_v3v3(final_disp, smooth_closest_plane, vd->co);

  mul_v3_fl(final_disp, factor);
  add_v3_v3v3(r_final_pos, vd->co, final_disp);
}

void relax_vertex(SculptSession &ss,
                  PBVHVertexIter *vd,
                  const float factor,
                  const bool filter_boundary_face_sets,
                  float *r_final_pos)
{
  if (SCULPT_vertex_is_boundary(ss, vd->vertex)) {
    relax_vertex_boundary(ss, vd, factor, filter_boundary_face_sets, r_final_pos);
  }
  else {
    relax_vertex_interior(ss, vd, factor, filter_boundary_face_sets, r_final_pos);
  }
}

}  // namespace blender::ed::sculpt_paint::smooth

static void do_topology_relax_task(Object &ob, const Brush &brush, PBVHNode *node)
{
  using namespace blender::ed::sculpt_paint;
  SculptSession &ss = *ob.sculpt;
  const float bstrength = ss.cache->bstrength;

  PBVHVertexIter vd;

  SculptOrigVertData orig_data = SCULPT_orig_vert_data_init(ob, *node, undo::Type::Position);

  /* TODO(@sergey): This looks very suspicious: proxy is added but is never written.
   * Either this needs to be documented better why it is needed, or removed. The removal is likely
   * to lead to performance improvements as well. */
  BKE_pbvh_node_add_proxy(*ss.pbvh, *node);

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
                                                    orig_data.co,
                                                    sqrtf(test.dist),
                                                    orig_data.no,
                                                    nullptr,
                                                    vd.mask,
                                                    vd.vertex,
                                                    thread_id,
                                                    &automask_data);

    smooth::relax_vertex(ss, &vd, fade * bstrength, false, vd.co);
  }
  BKE_pbvh_vertex_iter_end;
}

void SCULPT_do_slide_relax_brush(const Sculpt &sd, Object &ob, Span<PBVHNode *> nodes)
{
  using namespace blender;
  SculptSession &ss = *ob.sculpt;
  const Brush &brush = *BKE_paint_brush_for_read(&sd.paint);

  if (SCULPT_stroke_is_first_brush_step_of_symmetry_pass(*ss.cache)) {
    return;
  }

  BKE_curvemapping_init(brush.curve);

  if (ss.cache->alt_smooth) {
    SCULPT_boundary_info_ensure(ob);
    for (int i = 0; i < 4; i++) {
      threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
        for (const int i : range) {
          do_topology_relax_task(ob, brush, nodes[i]);
        }
      });
    }
  }
  else {
    threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
      for (const int i : range) {
        do_topology_slide_task(ob, brush, nodes[i]);
      }
    });
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Sculpt Topology Rake (Shared Utility)
 * \{ */

static void do_topology_rake_bmesh_task(
    Object &ob, const Sculpt &sd, const Brush &brush, const float strength, PBVHNode *node)
{
  using namespace blender::ed::sculpt_paint;
  SculptSession &ss = *ob.sculpt;

  float direction[3];
  copy_v3_v3(direction, ss.cache->grab_delta_symmetry);

  float tmp[3];
  mul_v3_v3fl(
      tmp, ss.cache->sculpt_normal_symm, dot_v3v3(ss.cache->sculpt_normal_symm, direction));
  sub_v3_v3(direction, tmp);
  normalize_v3(direction);

  /* Cancel if there's no grab data. */
  if (is_zero_v3(direction)) {
    return;
  }

  const float bstrength = clamp_f(strength, 0.0f, 1.0f);

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

    const float fade = bstrength *
                       SCULPT_brush_strength_factor(ss,
                                                    brush,
                                                    vd.co,
                                                    sqrtf(test.dist),
                                                    vd.no,
                                                    vd.fno,
                                                    vd.mask,
                                                    vd.vertex,
                                                    thread_id,
                                                    &automask_data) *
                       ss.cache->pressure;

    float avg[3], val[3];

    smooth::bmesh_four_neighbor_average(avg, direction, vd.bm_vert);

    sub_v3_v3v3(val, avg, vd.co);

    madd_v3_v3v3fl(val, vd.co, val, fade);

    SCULPT_clip(sd, ss, vd.co, val);
  }
  BKE_pbvh_vertex_iter_end;
}

void SCULPT_bmesh_topology_rake(const Sculpt &sd,
                                Object &ob,
                                Span<PBVHNode *> nodes,
                                float bstrength)
{
  using namespace blender;
  const Brush &brush = *BKE_paint_brush_for_read(&sd.paint);
  const float strength = clamp_f(bstrength, 0.0f, 1.0f);

  /* Interactions increase both strength and quality. */
  const int iterations = 3;

  int iteration;
  const int count = iterations * strength + 1;
  const float factor = iterations * strength / count;

  for (iteration = 0; iteration <= count; iteration++) {
    threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
      for (const int i : range) {
        do_topology_rake_bmesh_task(ob, sd, brush, factor, nodes[i]);
      }
    });
  }
}

/** \} */
