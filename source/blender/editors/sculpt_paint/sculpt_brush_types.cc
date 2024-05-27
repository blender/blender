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
#include "BKE_ccg.h"
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

static bool plane_point_side_flip(const float co[3], const float plane[4], const bool flip)
{
  float d = plane_point_side_v3(plane, co);
  if (flip) {
    d = -d;
  }
  return d <= 0.0f;
}

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

/**
 * Align the grab delta to the brush normal.
 *
 * \param grab_delta: Typically from `ss.cache->grab_delta_symmetry`.
 */
static void sculpt_project_v3_normal_align(const SculptSession &ss,
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

/* -------------------------------------------------------------------- */
/** \name Sculpt Draw Brush
 * \{ */

static void do_draw_brush_task(Object &ob, const Brush &brush, const float *offset, PBVHNode *node)
{
  using namespace blender::ed::sculpt_paint;
  SculptSession &ss = *ob.sculpt;
  PBVHVertexIter vd;
  const MutableSpan<float3> proxy = BKE_pbvh_node_add_proxy(*ss.pbvh, *node).co;

  SculptBrushTest test;
  SculptBrushTestFn sculpt_brush_test_sq_fn = SCULPT_brush_test_init_with_falloff_shape(
      ss, test, brush.falloff_shape);
  const int thread_id = BLI_task_parallel_thread_id(nullptr);

  auto_mask::NodeData automask_data = auto_mask::node_begin(
      ob, ss.cache->automasking.get(), *node);

  BKE_pbvh_vertex_iter_begin (*ss.pbvh, node, vd, PBVH_ITER_UNIQUE) {
    if (!sculpt_brush_test_sq_fn(test, vd.co)) {
      continue;
    }

    auto_mask::node_update(automask_data, vd);

    /* Offset vertex. */
    if (ss.cache->brush->flag2 & BRUSH_USE_COLOR_AS_DISPLACEMENT &&
        (brush.mtex.brush_map_mode == MTEX_MAP_MODE_AREA))
    {
      float r_rgba[4];
      SCULPT_brush_strength_color(ss,
                                  brush,
                                  vd.co,
                                  sqrtf(test.dist),
                                  vd.no,
                                  vd.fno,
                                  vd.mask,
                                  vd.vertex,
                                  thread_id,
                                  &automask_data,
                                  r_rgba);
      SCULPT_calc_vertex_displacement(ss, brush, r_rgba, proxy[vd.i]);
    }
    else {
      float fade = SCULPT_brush_strength_factor(ss,
                                                brush,
                                                vd.co,
                                                sqrtf(test.dist),
                                                vd.no,
                                                vd.fno,
                                                vd.mask,
                                                vd.vertex,
                                                thread_id,
                                                &automask_data);
      mul_v3_v3fl(proxy[vd.i], offset, fade);
    }
  }
  BKE_pbvh_vertex_iter_end;
}

void SCULPT_do_draw_brush(const Sculpt &sd, Object &ob, Span<PBVHNode *> nodes)
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
      do_draw_brush_task(ob, brush, offset, nodes[i]);
    }
  });
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Sculpt Fill Brush
 * \{ */

static void do_fill_brush_task(
    Object &ob, const Brush &brush, const float *area_no, const float *area_co, PBVHNode *node)
{
  using namespace blender::ed::sculpt_paint;
  SculptSession &ss = *ob.sculpt;

  PBVHVertexIter vd;
  const MutableSpan<float3> proxy = BKE_pbvh_node_add_proxy(*ss.pbvh, *node).co;
  const float bstrength = ss.cache->bstrength;

  SculptBrushTest test;
  SculptBrushTestFn sculpt_brush_test_sq_fn = SCULPT_brush_test_init_with_falloff_shape(
      ss, test, brush.falloff_shape);
  const int thread_id = BLI_task_parallel_thread_id(nullptr);

  plane_from_point_normal_v3(test.plane_tool, area_co, area_no);

  auto_mask::NodeData automask_data = auto_mask::node_begin(
      ob, ss.cache->automasking.get(), *node);

  BKE_pbvh_vertex_iter_begin (*ss.pbvh, node, vd, PBVH_ITER_UNIQUE) {
    if (!sculpt_brush_test_sq_fn(test, vd.co)) {
      continue;
    }

    if (!SCULPT_plane_point_side(vd.co, test.plane_tool)) {
      continue;
    }

    float intr[3];
    float val[3];
    closest_to_plane_normalized_v3(intr, test.plane_tool, vd.co);
    sub_v3_v3v3(val, intr, vd.co);

    if (!SCULPT_plane_trim(*ss.cache, brush, val)) {
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

    mul_v3_v3fl(proxy[vd.i], val, fade);
  }
  BKE_pbvh_vertex_iter_end;
}

void SCULPT_do_fill_brush(const Sculpt &sd, Object &ob, Span<PBVHNode *> nodes)
{
  using namespace blender;
  SculptSession &ss = *ob.sculpt;
  const Brush &brush = *BKE_paint_brush_for_read(&sd.paint);

  const float radius = ss.cache->radius;

  float area_no[3];
  float area_co[3];
  float offset = SCULPT_brush_plane_offset_get(sd, ss);

  float displace;

  float temp[3];

  SCULPT_calc_brush_plane(sd, ob, nodes, area_no, area_co);

  SCULPT_tilt_apply_to_normal(area_no, ss.cache, brush.tilt_strength_factor);

  displace = radius * offset;

  mul_v3_v3v3(temp, area_no, ss.cache->scale);
  mul_v3_fl(temp, displace);
  add_v3_v3(area_co, temp);

  threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
    for (const int i : range) {
      do_fill_brush_task(ob, brush, area_no, area_co, nodes[i]);
    }
  });
}

static void do_scrape_brush_task(
    Object &ob, const Brush &brush, const float *area_no, const float *area_co, PBVHNode *node)
{
  using namespace blender::ed::sculpt_paint;
  SculptSession &ss = *ob.sculpt;

  PBVHVertexIter vd;
  const MutableSpan<float3> proxy = BKE_pbvh_node_add_proxy(*ss.pbvh, *node).co;
  const float bstrength = ss.cache->bstrength;

  SculptBrushTest test;
  SculptBrushTestFn sculpt_brush_test_sq_fn = SCULPT_brush_test_init_with_falloff_shape(
      ss, test, brush.falloff_shape);
  const int thread_id = BLI_task_parallel_thread_id(nullptr);
  plane_from_point_normal_v3(test.plane_tool, area_co, area_no);

  auto_mask::NodeData automask_data = auto_mask::node_begin(
      ob, ss.cache->automasking.get(), *node);

  BKE_pbvh_vertex_iter_begin (*ss.pbvh, node, vd, PBVH_ITER_UNIQUE) {
    if (!sculpt_brush_test_sq_fn(test, vd.co)) {
      continue;
    }

    if (SCULPT_plane_point_side(vd.co, test.plane_tool)) {
      continue;
    }

    float intr[3];
    float val[3];
    closest_to_plane_normalized_v3(intr, test.plane_tool, vd.co);
    sub_v3_v3v3(val, intr, vd.co);

    if (!SCULPT_plane_trim(*ss.cache, brush, val)) {
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

    mul_v3_v3fl(proxy[vd.i], val, fade);
  }
  BKE_pbvh_vertex_iter_end;
}

void SCULPT_do_scrape_brush(const Sculpt &sd, Object &ob, Span<PBVHNode *> nodes)
{
  using namespace blender;
  SculptSession &ss = *ob.sculpt;
  const Brush &brush = *BKE_paint_brush_for_read(&sd.paint);

  const float radius = ss.cache->radius;

  float area_no[3];
  float area_co[3];
  float offset = SCULPT_brush_plane_offset_get(sd, ss);

  float displace;

  float temp[3];

  SCULPT_calc_brush_plane(sd, ob, nodes, area_no, area_co);

  SCULPT_tilt_apply_to_normal(area_no, ss.cache, brush.tilt_strength_factor);

  displace = -radius * offset;

  mul_v3_v3v3(temp, area_no, ss.cache->scale);
  mul_v3_fl(temp, displace);
  add_v3_v3(area_co, temp);

  threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
    for (const int i : range) {
      do_scrape_brush_task(ob, brush, area_no, area_co, nodes[i]);
    }
  });
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Sculpt Clay Thumb Brush
 * \{ */

static void do_clay_thumb_brush_task(Object &ob,
                                     const Brush &brush,
                                     float (*mat)[4],
                                     const float *area_no_sp,
                                     const float *area_co,
                                     const float bstrength,
                                     PBVHNode *node)
{
  using namespace blender::ed::sculpt_paint;
  SculptSession &ss = *ob.sculpt;

  PBVHVertexIter vd;
  const MutableSpan<float3> proxy = BKE_pbvh_node_add_proxy(*ss.pbvh, *node).co;

  SculptBrushTest test;
  SculptBrushTestFn sculpt_brush_test_sq_fn = SCULPT_brush_test_init_with_falloff_shape(
      ss, test, brush.falloff_shape);
  const int thread_id = BLI_task_parallel_thread_id(nullptr);

  float plane_tilt[4];
  float normal_tilt[3];
  float imat[4][4];

  invert_m4_m4(imat, mat);
  rotate_v3_v3v3fl(normal_tilt, area_no_sp, imat[0], DEG2RADF(-ss.cache->clay_thumb_front_angle));

  /* Plane aligned to the geometry normal (back part of the brush). */
  plane_from_point_normal_v3(test.plane_tool, area_co, area_no_sp);
  /* Tilted plane (front part of the brush). */
  plane_from_point_normal_v3(plane_tilt, area_co, normal_tilt);

  auto_mask::NodeData automask_data = auto_mask::node_begin(
      ob, ss.cache->automasking.get(), *node);

  BKE_pbvh_vertex_iter_begin (*ss.pbvh, node, vd, PBVH_ITER_UNIQUE) {
    if (!sculpt_brush_test_sq_fn(test, vd.co)) {
      continue;
    }
    float local_co[3];
    mul_v3_m4v3(local_co, mat, vd.co);
    float intr[3], intr_tilt[3];
    float val[3];

    closest_to_plane_normalized_v3(intr, test.plane_tool, vd.co);
    closest_to_plane_normalized_v3(intr_tilt, plane_tilt, vd.co);

    /* Mix the deformation of the aligned and the tilted plane based on the brush space vertex
     * coordinates. */
    /* We can also control the mix with a curve if it produces noticeable artifacts in the center
     * of the brush. */
    const float tilt_mix = local_co[1] > 0.0f ? 0.0f : 1.0f;
    interp_v3_v3v3(intr, intr, intr_tilt, tilt_mix);
    sub_v3_v3v3(val, intr_tilt, vd.co);

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

    mul_v3_v3fl(proxy[vd.i], val, fade);
  }
  BKE_pbvh_vertex_iter_end;
}

float SCULPT_clay_thumb_get_stabilized_pressure(
    const blender::ed::sculpt_paint::StrokeCache &cache)
{
  float final_pressure = 0.0f;
  for (int i = 0; i < SCULPT_CLAY_STABILIZER_LEN; i++) {
    final_pressure += cache.clay_pressure_stabilizer[i];
  }
  return final_pressure / SCULPT_CLAY_STABILIZER_LEN;
}

void SCULPT_do_clay_thumb_brush(const Sculpt &sd, Object &ob, Span<PBVHNode *> nodes)
{
  using namespace blender;
  SculptSession &ss = *ob.sculpt;
  const Brush &brush = *BKE_paint_brush_for_read(&sd.paint);

  /* Sampled geometry normal and area center. */
  float3 area_no_sp;
  float3 area_no;
  float3 area_co_tmp;

  float mat[4][4];
  float scale[4][4];
  float tmat[4][4];

  SCULPT_calc_brush_plane(sd, ob, nodes, area_no_sp, area_co_tmp);

  if (brush.sculpt_plane != SCULPT_DISP_DIR_AREA || (brush.flag & BRUSH_ORIGINAL_NORMAL)) {
    area_no = SCULPT_calc_area_normal(sd, ob, nodes).value_or(float3(0));
  }
  else {
    area_no = area_no_sp;
  }

  /* Delay the first daub because grab delta is not setup. */
  if (SCULPT_stroke_is_first_brush_step_of_symmetry_pass(*ss.cache)) {
    ss.cache->clay_thumb_front_angle = 0.0f;
    return;
  }

  /* Simulate the clay accumulation by increasing the plane angle as more samples are added to the
   * stroke. */
  if (SCULPT_stroke_is_main_symmetry_pass(*ss.cache)) {
    ss.cache->clay_thumb_front_angle += 0.8f;
    ss.cache->clay_thumb_front_angle = clamp_f(ss.cache->clay_thumb_front_angle, 0.0f, 60.0f);
  }

  if (is_zero_v3(ss.cache->grab_delta_symmetry)) {
    return;
  }

  /* Initialize brush local-space matrix. */
  cross_v3_v3v3(mat[0], area_no, ss.cache->grab_delta_symmetry);
  mat[0][3] = 0.0f;
  cross_v3_v3v3(mat[1], area_no, mat[0]);
  mat[1][3] = 0.0f;
  copy_v3_v3(mat[2], area_no);
  mat[2][3] = 0.0f;
  copy_v3_v3(mat[3], ss.cache->location);
  mat[3][3] = 1.0f;
  normalize_m4(mat);

  /* Scale brush local space matrix. */
  scale_m4_fl(scale, ss.cache->radius);
  mul_m4_m4m4(tmat, mat, scale);
  invert_m4_m4(mat, tmat);

  float clay_strength = ss.cache->bstrength * SCULPT_clay_thumb_get_stabilized_pressure(*ss.cache);

  threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
    for (const int i : range) {
      do_clay_thumb_brush_task(
          ob, brush, mat, area_no_sp, ss.cache->location, clay_strength, nodes[i]);
    }
  });
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Sculpt Flatten Brush
 * \{ */

static void do_flatten_brush_task(
    Object &ob, const Brush &brush, const float *area_no, const float *area_co, PBVHNode *node)
{
  using namespace blender::ed::sculpt_paint;
  SculptSession &ss = *ob.sculpt;

  PBVHVertexIter vd;
  const MutableSpan<float3> proxy = BKE_pbvh_node_add_proxy(*ss.pbvh, *node).co;
  const float bstrength = ss.cache->bstrength;

  SculptBrushTest test;
  SculptBrushTestFn sculpt_brush_test_sq_fn = SCULPT_brush_test_init_with_falloff_shape(
      ss, test, brush.falloff_shape);
  const int thread_id = BLI_task_parallel_thread_id(nullptr);

  plane_from_point_normal_v3(test.plane_tool, area_co, area_no);

  auto_mask::NodeData automask_data = auto_mask::node_begin(
      ob, ss.cache->automasking.get(), *node);

  BKE_pbvh_vertex_iter_begin (*ss.pbvh, node, vd, PBVH_ITER_UNIQUE) {
    if (!sculpt_brush_test_sq_fn(test, vd.co)) {
      continue;
    }
    float intr[3];
    float val[3];

    closest_to_plane_normalized_v3(intr, test.plane_tool, vd.co);

    sub_v3_v3v3(val, intr, vd.co);

    if (SCULPT_plane_trim(*ss.cache, brush, val)) {
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

      mul_v3_v3fl(proxy[vd.i], val, fade);
    }
  }
  BKE_pbvh_vertex_iter_end;
}

void SCULPT_do_flatten_brush(const Sculpt &sd, Object &ob, Span<PBVHNode *> nodes)
{
  using namespace blender;
  SculptSession &ss = *ob.sculpt;
  const Brush &brush = *BKE_paint_brush_for_read(&sd.paint);

  const float radius = ss.cache->radius;

  float area_no[3];
  float area_co[3];

  float offset = SCULPT_brush_plane_offset_get(sd, ss);
  float displace;
  float temp[3];

  SCULPT_calc_brush_plane(sd, ob, nodes, area_no, area_co);

  SCULPT_tilt_apply_to_normal(area_no, ss.cache, brush.tilt_strength_factor);

  displace = radius * offset;

  mul_v3_v3v3(temp, area_no, ss.cache->scale);
  mul_v3_fl(temp, displace);
  add_v3_v3(area_co, temp);

  threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
    for (const int i : range) {
      do_flatten_brush_task(ob, brush, area_no, area_co, nodes[i]);
    }
  });
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Sculpt Clay Brush
 * \{ */

struct ClaySampleData {
  blender::float2 plane_dist;
};

static void calc_clay_surface_task_cb(Object &ob,
                                      const Brush &brush,
                                      const float *area_no,
                                      const float *area_co,
                                      PBVHNode *node,
                                      ClaySampleData *csd)
{
  SculptSession &ss = *ob.sculpt;
  float plane[4];

  PBVHVertexIter vd;

  SculptBrushTest test;
  SculptBrushTestFn sculpt_brush_test_sq_fn = SCULPT_brush_test_init_with_falloff_shape(
      ss, test, brush.falloff_shape);

  /* Apply the brush normal radius to the test before sampling. */
  float test_radius = sqrtf(test.radius_squared);
  test_radius *= brush.normal_radius_factor;
  test.radius_squared = test_radius * test_radius;
  plane_from_point_normal_v3(plane, area_co, area_no);

  if (is_zero_v4(plane)) {
    return;
  }

  BKE_pbvh_vertex_iter_begin (*ss.pbvh, node, vd, PBVH_ITER_UNIQUE) {
    if (!sculpt_brush_test_sq_fn(test, vd.co)) {
      continue;
    }

    float plane_dist = dist_signed_to_plane_v3(vd.co, plane);
    float plane_dist_abs = fabsf(plane_dist);
    if (plane_dist > 0.0f) {
      csd->plane_dist[0] = std::min(csd->plane_dist[0], plane_dist_abs);
    }
    else {
      csd->plane_dist[1] = std::min(csd->plane_dist[1], plane_dist_abs);
    }
    BKE_pbvh_vertex_iter_end;
  }
}

static void do_clay_brush_task(
    Object &ob, const Brush &brush, const float *area_no, const float *area_co, PBVHNode *node)
{
  using namespace blender::ed::sculpt_paint;
  SculptSession &ss = *ob.sculpt;

  PBVHVertexIter vd;
  const MutableSpan<float3> proxy = BKE_pbvh_node_add_proxy(*ss.pbvh, *node).co;
  const float bstrength = fabsf(ss.cache->bstrength);

  SculptBrushTest test;
  SculptBrushTestFn sculpt_brush_test_sq_fn = SCULPT_brush_test_init_with_falloff_shape(
      ss, test, brush.falloff_shape);
  const int thread_id = BLI_task_parallel_thread_id(nullptr);

  plane_from_point_normal_v3(test.plane_tool, area_co, area_no);

  auto_mask::NodeData automask_data = auto_mask::node_begin(
      ob, ss.cache->automasking.get(), *node);

  BKE_pbvh_vertex_iter_begin (*ss.pbvh, node, vd, PBVH_ITER_UNIQUE) {
    if (!sculpt_brush_test_sq_fn(test, vd.co)) {
      continue;
    }

    float intr[3];
    float val[3];
    closest_to_plane_normalized_v3(intr, test.plane_tool, vd.co);

    sub_v3_v3v3(val, intr, vd.co);

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

    mul_v3_v3fl(proxy[vd.i], val, fade);
  }
  BKE_pbvh_vertex_iter_end;
}

void SCULPT_do_clay_brush(const Sculpt &sd, Object &ob, Span<PBVHNode *> nodes)
{
  using namespace blender;
  SculptSession &ss = *ob.sculpt;
  const Brush &brush = *BKE_paint_brush_for_read(&sd.paint);

  const float radius = fabsf(ss.cache->radius);
  const float initial_radius = fabsf(ss.cache->initial_radius);
  bool flip = ss.cache->bstrength < 0.0f;

  float offset = SCULPT_brush_plane_offset_get(sd, ss);
  float displace;

  float area_no[3];
  float area_co[3];
  float temp[3];

  SCULPT_calc_brush_plane(sd, ob, nodes, area_no, area_co);

  ClaySampleData csd = threading::parallel_reduce(
      nodes.index_range(),
      1,
      ClaySampleData{},
      [&](const IndexRange range, ClaySampleData csd) {
        for (const int i : range) {
          calc_clay_surface_task_cb(ob, brush, area_no, area_co, nodes[i], &csd);
        }
        return csd;
      },
      [](const ClaySampleData &a, const ClaySampleData &b) {
        return ClaySampleData{math::min(a.plane_dist, b.plane_dist)};
      });

  float d_offset = (csd.plane_dist[0] + csd.plane_dist[1]);
  d_offset = min_ff(radius, d_offset);
  d_offset = d_offset / radius;
  d_offset = 1.0f - d_offset;
  displace = fabsf(initial_radius * (0.25f + offset + (d_offset * 0.15f)));
  if (flip) {
    displace = -displace;
  }

  mul_v3_v3v3(temp, area_no, ss.cache->scale);
  mul_v3_fl(temp, displace);
  copy_v3_v3(area_co, ss.cache->location);
  add_v3_v3(area_co, temp);

  threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
    for (const int i : range) {
      do_clay_brush_task(ob, brush, area_no, area_co, nodes[i]);
    }
  });
}

static void do_clay_strips_brush_task(Object &ob,
                                      const Brush &brush,
                                      const float (*mat)[4],
                                      const float *area_no_sp,
                                      const float *area_co,
                                      PBVHNode *node)
{
  using namespace blender::ed::sculpt_paint;
  SculptSession &ss = *ob.sculpt;

  PBVHVertexIter vd;
  SculptBrushTest test;
  const MutableSpan<float3> proxy = BKE_pbvh_node_add_proxy(*ss.pbvh, *node).co;
  const bool flip = (ss.cache->bstrength < 0.0f);
  const float bstrength = flip ? -ss.cache->bstrength : ss.cache->bstrength;

  SCULPT_brush_test_init(ss, test);
  plane_from_point_normal_v3(test.plane_tool, area_co, area_no_sp);
  const int thread_id = BLI_task_parallel_thread_id(nullptr);

  auto_mask::NodeData automask_data = auto_mask::node_begin(
      ob, ss.cache->automasking.get(), *node);

  BKE_pbvh_vertex_iter_begin (*ss.pbvh, node, vd, PBVH_ITER_UNIQUE) {
    if (!SCULPT_brush_test_cube(test, vd.co, mat, brush.tip_roundness, brush.tip_scale_x)) {
      continue;
    }

    if (!plane_point_side_flip(vd.co, test.plane_tool, flip)) {
      continue;
    }

    float intr[3];
    float val[3];
    closest_to_plane_normalized_v3(intr, test.plane_tool, vd.co);
    sub_v3_v3v3(val, intr, vd.co);

    if (!SCULPT_plane_trim(*ss.cache, brush, val)) {
      continue;
    }

    auto_mask::node_update(automask_data, vd);

    /* The normal from the vertices is ignored, it causes glitch with planes, see: #44390. */
    const float fade = bstrength * SCULPT_brush_strength_factor(ss,
                                                                brush,
                                                                vd.co,
                                                                ss.cache->radius * test.dist,
                                                                vd.no,
                                                                vd.fno,
                                                                vd.mask,
                                                                vd.vertex,
                                                                thread_id,
                                                                &automask_data);

    mul_v3_v3fl(proxy[vd.i], val, fade);
  }
  BKE_pbvh_vertex_iter_end;
}

void SCULPT_do_clay_strips_brush(const Sculpt &sd, Object &ob, Span<PBVHNode *> nodes)
{
  using namespace blender;
  SculptSession &ss = *ob.sculpt;
  const Brush &brush = *BKE_paint_brush_for_read(&sd.paint);

  const bool flip = (ss.cache->bstrength < 0.0f);
  const float radius = flip ? -ss.cache->radius : ss.cache->radius;
  const float offset = SCULPT_brush_plane_offset_get(sd, ss);
  const float displace = radius * (0.18f + offset);

  /* The sculpt-plane normal (whatever its set to). */
  float3 area_no_sp;

  /* Geometry normal */
  float3 area_no;
  float3 area_co;

  float temp[3];
  float mat[4][4];
  float scale[4][4];
  float tmat[4][4];

  SCULPT_calc_brush_plane(sd, ob, nodes, area_no_sp, area_co);
  SCULPT_tilt_apply_to_normal(area_no_sp, ss.cache, brush.tilt_strength_factor);

  if (brush.sculpt_plane != SCULPT_DISP_DIR_AREA || (brush.flag & BRUSH_ORIGINAL_NORMAL)) {
    area_no = SCULPT_calc_area_normal(sd, ob, nodes).value_or(float3(0));
  }
  else {
    area_no = area_no_sp;
  }

  if (is_zero_v3(ss.cache->grab_delta_symmetry)) {
    return;
  }

  mul_v3_v3v3(temp, area_no_sp, ss.cache->scale);
  mul_v3_fl(temp, displace);
  add_v3_v3(area_co, temp);

  /* Clay Strips uses a cube test with falloff in the XY axis (not in Z) and a plane to deform the
   * vertices. When in Add mode, vertices that are below the plane and inside the cube are moved
   * towards the plane. In this situation, there may be cases where a vertex is outside the cube
   * but below the plane, so won't be deformed, causing artifacts. In order to prevent these
   * artifacts, this displaces the test cube space in relation to the plane in order to
   * deform more vertices that may be below it. */
  /* The 0.7 and 1.25 factors are arbitrary and don't have any relation between them, they were set
   * by doing multiple tests using the default "Clay Strips" brush preset. */
  float area_co_displaced[3];
  madd_v3_v3v3fl(area_co_displaced, area_co, area_no, -radius * 0.7f);

  cross_v3_v3v3(mat[0], area_no, ss.cache->grab_delta_symmetry);
  mat[0][3] = 0.0f;
  cross_v3_v3v3(mat[1], area_no, mat[0]);
  mat[1][3] = 0.0f;
  copy_v3_v3(mat[2], area_no);
  mat[2][3] = 0.0f;
  copy_v3_v3(mat[3], area_co_displaced);
  mat[3][3] = 1.0f;
  normalize_m4(mat);

  /* Scale brush local space matrix. */
  scale_m4_fl(scale, ss.cache->radius);
  mul_m4_m4m4(tmat, mat, scale);

  mul_v3_fl(tmat[1], brush.tip_scale_x);

  /* Deform the local space in Z to scale the test cube. As the test cube does not have falloff in
   * Z this does not produce artifacts in the falloff cube and allows to deform extra vertices
   * during big deformation while keeping the surface as uniform as possible. */
  mul_v3_fl(tmat[2], 1.25f);

  invert_m4_m4(mat, tmat);

  threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
    for (const int i : range) {
      do_clay_strips_brush_task(ob, brush, mat, area_no_sp, area_co, nodes[i]);
    }
  });
}

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

static void do_thumb_brush_task(Object &ob, const Brush &brush, const float *cono, PBVHNode *node)
{
  using namespace blender::ed::sculpt_paint;
  SculptSession &ss = *ob.sculpt;

  PBVHVertexIter vd;
  SculptOrigVertData orig_data;
  const MutableSpan<float3> proxy = BKE_pbvh_node_add_proxy(*ss.pbvh, *node).co;
  const float bstrength = ss.cache->bstrength;

  SCULPT_orig_vert_data_init(orig_data, ob, *node, undo::Type::Position);

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

    const float fade = bstrength * SCULPT_brush_strength_factor(ss,
                                                                brush,
                                                                orig_data.co,
                                                                sqrtf(test.dist),
                                                                orig_data.no,
                                                                nullptr,
                                                                vd.mask,
                                                                vd.vertex,
                                                                thread_id,
                                                                &automask_data);

    mul_v3_v3fl(proxy[vd.i], cono, fade);
  }
  BKE_pbvh_vertex_iter_end;
}

void SCULPT_do_thumb_brush(const Sculpt &sd, Object &ob, Span<PBVHNode *> nodes)
{
  using namespace blender;
  SculptSession &ss = *ob.sculpt;
  const Brush &brush = *BKE_paint_brush_for_read(&sd.paint);
  float grab_delta[3];
  float tmp[3], cono[3];

  copy_v3_v3(grab_delta, ss.cache->grab_delta_symmetry);

  cross_v3_v3v3(tmp, ss.cache->sculpt_normal_symm, grab_delta);
  cross_v3_v3v3(cono, tmp, ss.cache->sculpt_normal_symm);

  threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
    for (const int i : range) {
      do_thumb_brush_task(ob, brush, cono, nodes[i]);
    }
  });
}

static void do_rotate_brush_task(Object &ob, const Brush &brush, const float angle, PBVHNode *node)
{
  using namespace blender::ed::sculpt_paint;
  SculptSession &ss = *ob.sculpt;

  PBVHVertexIter vd;
  SculptOrigVertData orig_data;
  const MutableSpan<float3> proxy = BKE_pbvh_node_add_proxy(*ss.pbvh, *node).co;
  const float bstrength = ss.cache->bstrength;

  SCULPT_orig_vert_data_init(orig_data, ob, *node, undo::Type::Position);

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

    float vec[3], rot[3][3];
    const float fade = bstrength * SCULPT_brush_strength_factor(ss,
                                                                brush,
                                                                orig_data.co,
                                                                sqrtf(test.dist),
                                                                orig_data.no,
                                                                nullptr,
                                                                vd.mask,
                                                                vd.vertex,
                                                                thread_id,
                                                                &automask_data);

    sub_v3_v3v3(vec, orig_data.co, ss.cache->location);
    axis_angle_normalized_to_mat3(rot, ss.cache->sculpt_normal_symm, angle * fade);
    mul_v3_m3v3(proxy[vd.i], rot, vec);
    add_v3_v3(proxy[vd.i], ss.cache->location);
    sub_v3_v3(proxy[vd.i], orig_data.co);
  }
  BKE_pbvh_vertex_iter_end;
}

void SCULPT_do_rotate_brush(const Sculpt &sd, Object &ob, Span<PBVHNode *> nodes)
{
  using namespace blender;
  SculptSession &ss = *ob.sculpt;
  const Brush &brush = *BKE_paint_brush_for_read(&sd.paint);

  static const int flip[8] = {1, -1, -1, 1, -1, 1, 1, -1};
  const float angle = ss.cache->vertex_rotation * flip[ss.cache->mirror_symmetry_pass];

  threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
    for (const int i : range) {
      do_rotate_brush_task(ob, brush, angle, nodes[i]);
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
  SculptOrigVertData orig_data;
  const float bstrength = ss.cache->bstrength;
  SCULPT_orig_vert_data_init(orig_data, ob, *node, undo::Type::Position);

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

static void do_inflate_brush_task(Object &ob, const Brush &brush, PBVHNode *node)
{
  using namespace blender::ed::sculpt_paint;
  SculptSession &ss = *ob.sculpt;

  PBVHVertexIter vd;
  const MutableSpan<float3> proxy = BKE_pbvh_node_add_proxy(*ss.pbvh, *node).co;
  const float bstrength = ss.cache->bstrength;

  SculptBrushTest test;
  SculptBrushTestFn sculpt_brush_test_sq_fn = SCULPT_brush_test_init_with_falloff_shape(
      ss, test, brush.falloff_shape);
  const int thread_id = BLI_task_parallel_thread_id(nullptr);

  auto_mask::NodeData automask_data = auto_mask::node_begin(
      ob, ss.cache->automasking.get(), *node);

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
    float val[3];

    if (vd.fno) {
      copy_v3_v3(val, vd.fno);
    }
    else {
      copy_v3_v3(val, vd.no);
    }

    mul_v3_fl(val, fade * ss.cache->radius);
    mul_v3_v3v3(proxy[vd.i], val, ss.cache->scale);
  }
  BKE_pbvh_vertex_iter_end;
}

void SCULPT_do_inflate_brush(const Sculpt &sd, Object &ob, Span<PBVHNode *> nodes)
{
  using namespace blender;
  const Brush &brush = *BKE_paint_brush_for_read(&sd.paint);

  threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
    for (const int i : range) {
      do_inflate_brush_task(ob, brush, nodes[i]);
    }
  });
}

static void do_nudge_brush_task(Object &ob, const Brush &brush, const float *cono, PBVHNode *node)
{
  using namespace blender::ed::sculpt_paint;
  SculptSession &ss = *ob.sculpt;

  PBVHVertexIter vd;
  const MutableSpan<float3> proxy = BKE_pbvh_node_add_proxy(*ss.pbvh, *node).co;
  const float bstrength = ss.cache->bstrength;

  SculptBrushTest test;
  SculptBrushTestFn sculpt_brush_test_sq_fn = SCULPT_brush_test_init_with_falloff_shape(
      ss, test, brush.falloff_shape);
  const int thread_id = BLI_task_parallel_thread_id(nullptr);

  auto_mask::NodeData automask_data = auto_mask::node_begin(
      ob, ss.cache->automasking.get(), *node);

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

    mul_v3_v3fl(proxy[vd.i], cono, fade);
  }
  BKE_pbvh_vertex_iter_end;
}

void SCULPT_do_nudge_brush(const Sculpt &sd, Object &ob, Span<PBVHNode *> nodes)
{
  using namespace blender;
  SculptSession &ss = *ob.sculpt;
  const Brush &brush = *BKE_paint_brush_for_read(&sd.paint);
  float grab_delta[3];
  float tmp[3], cono[3];

  copy_v3_v3(grab_delta, ss.cache->grab_delta_symmetry);

  cross_v3_v3v3(tmp, ss.cache->sculpt_normal_symm, grab_delta);
  cross_v3_v3v3(cono, tmp, ss.cache->sculpt_normal_symm);

  threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
    for (const int i : range) {
      do_nudge_brush_task(ob, brush, cono, nodes[i]);
    }
  });
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Sculpt Crease & Blob Brush
 * \{ */

/**
 * Used for 'SCULPT_TOOL_CREASE' and 'SCULPT_TOOL_BLOB'
 */
static void do_crease_brush_task(Object &ob,
                                 const Brush &brush,
                                 SculptProjectVector *spvc,
                                 const float flippedbstrength,
                                 const float *offset,
                                 PBVHNode *node)
{
  using namespace blender::ed::sculpt_paint;
  SculptSession &ss = *ob.sculpt;

  PBVHVertexIter vd;
  const MutableSpan<float3> proxy = BKE_pbvh_node_add_proxy(*ss.pbvh, *node).co;

  SculptBrushTest test;
  SculptBrushTestFn sculpt_brush_test_sq_fn = SCULPT_brush_test_init_with_falloff_shape(
      ss, test, brush.falloff_shape);
  const int thread_id = BLI_task_parallel_thread_id(nullptr);

  auto_mask::NodeData automask_data = auto_mask::node_begin(
      ob, ss.cache->automasking.get(), *node);

  BKE_pbvh_vertex_iter_begin (*ss.pbvh, node, vd, PBVH_ITER_UNIQUE) {
    if (!sculpt_brush_test_sq_fn(test, vd.co)) {
      continue;
    }
    /* Offset vertex. */
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
    float val1[3];
    float val2[3];

    /* First we pinch. */
    sub_v3_v3v3(val1, test.location, vd.co);
    if (brush.falloff_shape == PAINT_FALLOFF_SHAPE_TUBE) {
      project_plane_v3_v3v3(val1, val1, ss.cache->view_normal);
    }

    mul_v3_fl(val1, fade * flippedbstrength);

    sculpt_project_v3(spvc, val1, val1);

    /* Then we draw. */
    mul_v3_v3fl(val2, offset, fade);

    add_v3_v3v3(proxy[vd.i], val1, val2);
  }
  BKE_pbvh_vertex_iter_end;
}

void SCULPT_do_crease_brush(const Sculpt &sd, Object &ob, Span<PBVHNode *> nodes)
{
  using namespace blender;
  SculptSession &ss = *ob.sculpt;
  const Scene *scene = ss.cache->vc->scene;
  const Brush &brush = *BKE_paint_brush_for_read(&sd.paint);
  float offset[3];
  float bstrength = ss.cache->bstrength;
  float flippedbstrength, crease_correction;
  float brush_alpha;

  SculptProjectVector spvc;

  /* Offset with as much as possible factored in already. */
  mul_v3_v3fl(offset, ss.cache->sculpt_normal_symm, ss.cache->radius);
  mul_v3_v3(offset, ss.cache->scale);
  mul_v3_fl(offset, bstrength);

  /* We divide out the squared alpha and multiply by the squared crease
   * to give us the pinch strength. */
  crease_correction = brush.crease_pinch_factor * brush.crease_pinch_factor;
  brush_alpha = BKE_brush_alpha_get(scene, &brush);
  if (brush_alpha > 0.0f) {
    crease_correction /= brush_alpha * brush_alpha;
  }

  /* We always want crease to pinch or blob to relax even when draw is negative. */
  flippedbstrength = (bstrength < 0.0f) ? -crease_correction * bstrength :
                                          crease_correction * bstrength;

  if (brush.sculpt_tool == SCULPT_TOOL_BLOB) {
    flippedbstrength *= -1.0f;
  }

  /* Use surface normal for 'spvc', so the vertices are pinched towards a line instead of a single
   * point. Without this we get a 'flat' surface surrounding the pinch. */
  sculpt_project_v3_cache_init(&spvc, ss.cache->sculpt_normal_symm);

  threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
    for (const int i : range) {
      do_crease_brush_task(ob, brush, &spvc, flippedbstrength, offset, nodes[i]);
    }
  });
}

static void do_pinch_brush_task(Object &ob,
                                const Brush &brush,
                                const float (*stroke_xz)[3],
                                PBVHNode *node)
{
  using namespace blender::ed::sculpt_paint;
  SculptSession &ss = *ob.sculpt;

  PBVHVertexIter vd;
  const MutableSpan<float3> proxy = BKE_pbvh_node_add_proxy(*ss.pbvh, *node).co;
  const float bstrength = ss.cache->bstrength;

  SculptBrushTest test;
  SculptBrushTestFn sculpt_brush_test_sq_fn = SCULPT_brush_test_init_with_falloff_shape(
      ss, test, brush.falloff_shape);
  const int thread_id = BLI_task_parallel_thread_id(nullptr);

  float x_object_space[3];
  float z_object_space[3];
  copy_v3_v3(x_object_space, stroke_xz[0]);
  copy_v3_v3(z_object_space, stroke_xz[1]);

  auto_mask::NodeData automask_data = auto_mask::node_begin(
      ob, ss.cache->automasking.get(), *node);

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
    float disp_center[3];
    float x_disp[3];
    float z_disp[3];
    /* Calculate displacement from the vertex to the brush center. */
    sub_v3_v3v3(disp_center, test.location, vd.co);

    /* Project the displacement into the X vector (aligned to the stroke). */
    mul_v3_v3fl(x_disp, x_object_space, dot_v3v3(disp_center, x_object_space));

    /* Project the displacement into the Z vector (aligned to the surface normal). */
    mul_v3_v3fl(z_disp, z_object_space, dot_v3v3(disp_center, z_object_space));

    /* Add the two projected vectors to calculate the final displacement.
     * The Y component is removed. */
    add_v3_v3v3(disp_center, x_disp, z_disp);

    if (brush.falloff_shape == PAINT_FALLOFF_SHAPE_TUBE) {
      project_plane_v3_v3v3(disp_center, disp_center, ss.cache->view_normal);
    }
    mul_v3_v3fl(proxy[vd.i], disp_center, fade);
  }
  BKE_pbvh_vertex_iter_end;
}

void SCULPT_do_pinch_brush(const Sculpt &sd, Object &ob, Span<PBVHNode *> nodes)
{
  using namespace blender;
  SculptSession &ss = *ob.sculpt;
  const Brush &brush = *BKE_paint_brush_for_read(&sd.paint);

  float area_no[3];
  float area_co[3];

  float mat[4][4];
  SCULPT_calc_brush_plane(sd, ob, nodes, area_no, area_co);

  /* delay the first daub because grab delta is not setup */
  if (SCULPT_stroke_is_first_brush_step_of_symmetry_pass(*ss.cache)) {
    return;
  }

  if (is_zero_v3(ss.cache->grab_delta_symmetry)) {
    return;
  }

  /* Initialize `mat`. */
  cross_v3_v3v3(mat[0], area_no, ss.cache->grab_delta_symmetry);
  mat[0][3] = 0.0f;
  cross_v3_v3v3(mat[1], area_no, mat[0]);
  mat[1][3] = 0.0f;
  copy_v3_v3(mat[2], area_no);
  mat[2][3] = 0.0f;
  copy_v3_v3(mat[3], ss.cache->location);
  mat[3][3] = 1.0f;
  normalize_m4(mat);

  float stroke_xz[2][3];
  normalize_v3_v3(stroke_xz[0], mat[0]);
  normalize_v3_v3(stroke_xz[1], mat[2]);

  threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
    for (const int i : range) {
      do_pinch_brush_task(ob, brush, stroke_xz, nodes[i]);
    }
  });
}

static void do_grab_brush_task(Object &ob,
                               const Brush &brush,
                               const float *grab_delta,
                               PBVHNode *node)
{
  using namespace blender::ed::sculpt_paint;
  SculptSession &ss = *ob.sculpt;

  PBVHVertexIter vd;
  SculptOrigVertData orig_data;
  const MutableSpan<float3> proxy = BKE_pbvh_node_add_proxy(*ss.pbvh, *node).co;
  const float bstrength = ss.cache->bstrength;

  SCULPT_orig_vert_data_init(orig_data, ob, *node, undo::Type::Position);

  SculptBrushTest test;
  SculptBrushTestFn sculpt_brush_test_sq_fn = SCULPT_brush_test_init_with_falloff_shape(
      ss, test, brush.falloff_shape);
  const int thread_id = BLI_task_parallel_thread_id(nullptr);

  const bool grab_silhouette = brush.flag2 & BRUSH_GRAB_SILHOUETTE;

  auto_mask::NodeData automask_data = auto_mask::node_begin(
      ob, ss.cache->automasking.get(), *node);

  BKE_pbvh_vertex_iter_begin (*ss.pbvh, node, vd, PBVH_ITER_UNIQUE) {
    SCULPT_orig_vert_data_update(orig_data, vd);

    if (!sculpt_brush_test_sq_fn(test, orig_data.co)) {
      continue;
    }
    auto_mask::node_update(automask_data, vd);

    float fade = bstrength * SCULPT_brush_strength_factor(ss,
                                                          brush,
                                                          orig_data.co,
                                                          sqrtf(test.dist),
                                                          orig_data.no,
                                                          nullptr,
                                                          vd.mask,
                                                          vd.vertex,
                                                          thread_id,
                                                          &automask_data);

    if (grab_silhouette) {
      float silhouette_test_dir[3];
      normalize_v3_v3(silhouette_test_dir, grab_delta);
      if (dot_v3v3(ss.cache->initial_normal, ss.cache->grab_delta_symmetry) < 0.0f) {
        mul_v3_fl(silhouette_test_dir, -1.0f);
      }
      float vno[3];
      copy_v3_v3(vno, orig_data.no);
      fade *= max_ff(dot_v3v3(vno, silhouette_test_dir), 0.0f);
    }

    mul_v3_v3fl(proxy[vd.i], grab_delta, fade);
  }
  BKE_pbvh_vertex_iter_end;
}

void SCULPT_do_grab_brush(const Sculpt &sd, Object &ob, Span<PBVHNode *> nodes)
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
      do_grab_brush_task(ob, brush, grab_delta, nodes[i]);
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
  SculptOrigVertData orig_data;
  const MutableSpan<float3> proxy = BKE_pbvh_node_add_proxy(*ss.pbvh, *node).co;

  const float bstrength = ss.cache->bstrength;

  SCULPT_orig_vert_data_init(orig_data, ob, *node, undo::Type::Position);
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

/** \} */

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
  SculptOrigVertData orig_data;
  const MutableSpan<float3> proxy = BKE_pbvh_node_add_proxy(*ss.pbvh, *node).co;

  SCULPT_orig_vert_data_init(orig_data, ob, *node, undo::Type::Position);

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
  SculptOrigVertData orig_data;
  const MutableSpan<float3> proxy = BKE_pbvh_node_add_proxy(*ss.pbvh, *node).co;

  SCULPT_orig_vert_data_init(orig_data, ob, *node, undo::Type::Position);

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

void relax_vertex(SculptSession &ss,
                  PBVHVertexIter *vd,
                  float factor,
                  bool filter_boundary_face_sets,
                  float *r_final_pos)
{
  float smooth_pos[3];
  float final_disp[3];
  float boundary_normal[3];
  int avg_count = 0;
  int neighbor_count = 0;
  zero_v3(smooth_pos);
  zero_v3(boundary_normal);
  const bool is_boundary = SCULPT_vertex_is_boundary(ss, vd->vertex);

  SculptVertexNeighborIter ni;
  SCULPT_VERTEX_NEIGHBORS_ITER_BEGIN (ss, vd->vertex, ni) {
    neighbor_count++;
    if (!filter_boundary_face_sets ||
        (filter_boundary_face_sets && !face_set::vert_has_unique_face_set(ss, ni.vertex)))
    {

      /* When the vertex to relax is boundary, use only connected boundary vertices for the average
       * position. */
      if (is_boundary) {
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
      else {
        add_v3_v3(smooth_pos, SCULPT_vertex_co_get(ss, ni.vertex));
        avg_count++;
      }
    }
  }
  SCULPT_VERTEX_NEIGHBORS_ITER_END(ni);

  /* Don't modify corner vertices. */
  if (neighbor_count <= 2) {
    copy_v3_v3(r_final_pos, vd->co);
    return;
  }

  if (avg_count > 0) {
    mul_v3_fl(smooth_pos, 1.0f / avg_count);
  }
  else {
    copy_v3_v3(r_final_pos, vd->co);
    return;
  }

  float plane[4];
  float smooth_closest_plane[3];
  float3 vno;

  if (is_boundary && avg_count == 2) {
    normalize_v3_v3(vno, boundary_normal);
  }
  else {
    vno = SCULPT_vertex_normal_get(ss, vd->vertex);
  }

  if (is_zero_v3(vno)) {
    copy_v3_v3(r_final_pos, vd->co);
    return;
  }

  plane_from_point_normal_v3(plane, vd->co, vno);
  closest_to_plane_v3(smooth_closest_plane, plane, smooth_pos);
  sub_v3_v3v3(final_disp, smooth_closest_plane, vd->co);

  mul_v3_fl(final_disp, factor);
  add_v3_v3v3(r_final_pos, vd->co, final_disp);
}

}  // namespace blender::ed::sculpt_paint::smooth

static void do_topology_relax_task(Object &ob, const Brush &brush, PBVHNode *node)
{
  using namespace blender::ed::sculpt_paint;
  SculptSession &ss = *ob.sculpt;
  const float bstrength = ss.cache->bstrength;

  PBVHVertexIter vd;
  SculptOrigVertData orig_data;

  SCULPT_orig_vert_data_init(orig_data, ob, *node, undo::Type::Position);

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
/** \name Sculpt Multires Displacement Eraser Brush
 * \{ */

static void do_displacement_eraser_brush_task(Object &ob, const Brush &brush, PBVHNode *node)
{
  using namespace blender::ed::sculpt_paint;
  SculptSession &ss = *ob.sculpt;
  const float bstrength = clamp_f(ss.cache->bstrength, 0.0f, 1.0f);

  const MutableSpan<float3> proxy = BKE_pbvh_node_add_proxy(*ss.pbvh, *node).co;

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

    float3 limit_co = SCULPT_vertex_limit_surface_get(ss, vd.vertex);
    float disp[3];
    sub_v3_v3v3(disp, limit_co, vd.co);
    mul_v3_v3fl(proxy[vd.i], disp, fade);
  }
  BKE_pbvh_vertex_iter_end;
}

void SCULPT_do_displacement_eraser_brush(const Sculpt &sd, Object &ob, Span<PBVHNode *> nodes)
{
  using namespace blender;
  const Brush &brush = *BKE_paint_brush_for_read(&sd.paint);
  BKE_curvemapping_init(brush.curve);

  threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
    for (const int i : range) {
      do_displacement_eraser_brush_task(ob, brush, nodes[i]);
    }
  });
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Sculpt Multires Displacement Smear Brush
 * \{ */

static void do_displacement_smear_brush_task(Object &ob, const Brush &brush, PBVHNode *node)
{
  using namespace blender::ed::sculpt_paint;
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

void SCULPT_do_displacement_smear_brush(const Sculpt &sd, Object &ob, Span<PBVHNode *> nodes)
{
  using namespace blender;
  const Brush &brush = *BKE_paint_brush_for_read(&sd.paint);
  SculptSession &ss = *ob.sculpt;

  BKE_curvemapping_init(brush.curve);

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

/* -------------------------------------------------------------------- */
/** \name Sculpt Mask Brush
 * \{ */

static void do_mask_brush_draw_task(Object &ob,
                                    const Brush &brush,
                                    const SculptMaskWriteInfo mask_write,
                                    PBVHNode *node)
{
  using namespace blender::ed::sculpt_paint;
  SculptSession &ss = *ob.sculpt;
  const float bstrength = ss.cache->bstrength;

  PBVHVertexIter vd;

  SculptBrushTest test;
  SculptBrushTestFn sculpt_brush_test_sq_fn = SCULPT_brush_test_init_with_falloff_shape(
      ss, test, brush.falloff_shape);
  const int thread_id = BLI_task_parallel_thread_id(nullptr);

  auto_mask::NodeData automask_data = auto_mask::node_begin(
      ob, ss.cache->automasking.get(), *node);

  BKE_pbvh_vertex_iter_begin (*ss.pbvh, node, vd, PBVH_ITER_UNIQUE) {
    if (!sculpt_brush_test_sq_fn(test, vd.co)) {
      continue;
    }

    auto_mask::node_update(automask_data, vd);
    const float fade = SCULPT_brush_strength_factor(ss,
                                                    brush,
                                                    vd.co,
                                                    sqrtf(test.dist),
                                                    vd.no,
                                                    vd.fno,
                                                    0.0f,
                                                    vd.vertex,
                                                    thread_id,
                                                    &automask_data);

    float mask = vd.mask;
    if (bstrength > 0.0f) {
      mask += fade * bstrength * (1.0f - vd.mask);
    }
    else {
      mask += fade * bstrength * mask;
    }
    mask = clamp_f(mask, 0.0f, 1.0f);
    SCULPT_mask_vert_set(BKE_pbvh_type(*ss.pbvh), mask_write, mask, vd);
  }
  BKE_pbvh_vertex_iter_end;
}

void SCULPT_do_mask_brush_draw(const Sculpt &sd, Object &ob, Span<PBVHNode *> nodes)
{
  using namespace blender;
  const Brush &brush = *BKE_paint_brush_for_read(&sd.paint);
  const SculptMaskWriteInfo mask_write = SCULPT_mask_get_for_write(*ob.sculpt);
  threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
    for (const int i : range) {
      do_mask_brush_draw_task(ob, brush, mask_write, nodes[i]);
    }
  });
}

void SCULPT_do_mask_brush(const Sculpt &sd, Object &ob, Span<PBVHNode *> nodes)
{
  using namespace blender::ed::sculpt_paint;
  SculptSession &ss = *ob.sculpt;
  const Brush &brush = *BKE_paint_brush_for_read(&sd.paint);

  switch ((BrushMaskTool)brush.mask_tool) {
    case BRUSH_MASK_DRAW:
      SCULPT_do_mask_brush_draw(sd, ob, nodes);
      break;
    case BRUSH_MASK_SMOOTH:
      smooth::do_smooth_mask_brush(sd, ob, nodes, ss.cache->bstrength);
      break;
  }
}

/** \} */
