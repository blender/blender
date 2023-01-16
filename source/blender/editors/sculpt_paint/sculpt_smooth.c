/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2020 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup edsculpt
 */

#include "MEM_guardedalloc.h"

#include "BLI_math.h"
#include "BLI_task.h"

#include "DNA_brush_types.h"
#include "DNA_meshdata_types.h"

#include "BKE_context.h"
#include "BKE_paint.h"
#include "BKE_pbvh.h"

#include "sculpt_intern.h"

#include "bmesh.h"

#include <math.h>
#include <stdlib.h>

void SCULPT_neighbor_coords_average_interior(SculptSession *ss,
                                             float result[3],
                                             PBVHVertRef vertex)
{
  float avg[3] = {0.0f, 0.0f, 0.0f};
  int total = 0;
  int neighbor_count = 0;
  const bool is_boundary = SCULPT_vertex_is_boundary(ss, vertex);

  SculptVertexNeighborIter ni;
  SCULPT_VERTEX_NEIGHBORS_ITER_BEGIN (ss, vertex, ni) {
    neighbor_count++;
    if (is_boundary) {
      /* Boundary vertices use only other boundary vertices. */
      if (SCULPT_vertex_is_boundary(ss, ni.vertex)) {
        add_v3_v3(avg, SCULPT_vertex_co_get(ss, ni.vertex));
        total++;
      }
    }
    else {
      /* Interior vertices use all neighbors. */
      add_v3_v3(avg, SCULPT_vertex_co_get(ss, ni.vertex));
      total++;
    }
  }
  SCULPT_VERTEX_NEIGHBORS_ITER_END(ni);

  /* Do not modify corner vertices. */
  if (neighbor_count <= 2 && is_boundary) {
    copy_v3_v3(result, SCULPT_vertex_co_get(ss, vertex));
    return;
  }

  /* Avoid division by 0 when there are no neighbors. */
  if (total == 0) {
    copy_v3_v3(result, SCULPT_vertex_co_get(ss, vertex));
    return;
  }

  mul_v3_v3fl(result, avg, 1.0f / total);
}

void SCULPT_bmesh_four_neighbor_average(float avg[3], float direction[3], BMVert *v)
{

  float avg_co[3] = {0.0f, 0.0f, 0.0f};
  float tot_co = 0.0f;

  BMIter eiter;
  BMEdge *e;

  BM_ITER_ELEM (e, &eiter, v, BM_EDGES_OF_VERT) {
    if (BM_edge_is_boundary(e)) {
      copy_v3_v3(avg, v->co);
      return;
    }
    BMVert *v_other = (e->v1 == v) ? e->v2 : e->v1;
    float vec[3];
    sub_v3_v3v3(vec, v_other->co, v->co);
    madd_v3_v3fl(vec, v->no, -dot_v3v3(vec, v->no));
    normalize_v3(vec);

    /* fac is a measure of how orthogonal or parallel the edge is
     * relative to the direction. */
    float fac = dot_v3v3(vec, direction);
    fac = fac * fac - 0.5f;
    fac *= fac;
    madd_v3_v3fl(avg_co, v_other->co, fac);
    tot_co += fac;
  }

  /* In case vert has no Edge s. */
  if (tot_co > 0.0f) {
    mul_v3_v3fl(avg, avg_co, 1.0f / tot_co);

    /* Preserve volume. */
    float vec[3];
    sub_v3_v3(avg, v->co);
    mul_v3_v3fl(vec, v->no, dot_v3v3(avg, v->no));
    sub_v3_v3(avg, vec);
    add_v3_v3(avg, v->co);
  }
  else {
    zero_v3(avg);
  }
}

/* Generic functions for laplacian smoothing. These functions do not take boundary vertices into
 * account. */

void SCULPT_neighbor_coords_average(SculptSession *ss, float result[3], PBVHVertRef vertex)
{
  float avg[3] = {0.0f, 0.0f, 0.0f};
  int total = 0;

  SculptVertexNeighborIter ni;
  SCULPT_VERTEX_NEIGHBORS_ITER_BEGIN (ss, vertex, ni) {
    add_v3_v3(avg, SCULPT_vertex_co_get(ss, ni.vertex));
    total++;
  }
  SCULPT_VERTEX_NEIGHBORS_ITER_END(ni);

  if (total > 0) {
    mul_v3_v3fl(result, avg, 1.0f / total);
  }
  else {
    copy_v3_v3(result, SCULPT_vertex_co_get(ss, vertex));
  }
}

float SCULPT_neighbor_mask_average(SculptSession *ss, PBVHVertRef vertex)
{
  float avg = 0.0f;
  int total = 0;

  SculptVertexNeighborIter ni;
  SCULPT_VERTEX_NEIGHBORS_ITER_BEGIN (ss, vertex, ni) {
    avg += SCULPT_vertex_mask_get(ss, ni.vertex);
    total++;
  }
  SCULPT_VERTEX_NEIGHBORS_ITER_END(ni);

  if (total > 0) {
    return avg / total;
  }
  return SCULPT_vertex_mask_get(ss, vertex);
}

void SCULPT_neighbor_color_average(SculptSession *ss, float result[4], PBVHVertRef vertex)
{
  float avg[4] = {0.0f, 0.0f, 0.0f, 0.0f};
  int total = 0;

  SculptVertexNeighborIter ni;
  SCULPT_VERTEX_NEIGHBORS_ITER_BEGIN (ss, vertex, ni) {
    float tmp[4] = {0};

    SCULPT_vertex_color_get(ss, ni.vertex, tmp);

    add_v4_v4(avg, tmp);
    total++;
  }
  SCULPT_VERTEX_NEIGHBORS_ITER_END(ni);

  if (total > 0) {
    mul_v4_v4fl(result, avg, 1.0f / total);
  }
  else {
    SCULPT_vertex_color_get(ss, vertex, result);
  }
}

static void do_enhance_details_brush_task_cb_ex(void *__restrict userdata,
                                                const int n,
                                                const TaskParallelTLS *__restrict tls)
{
  SculptThreadedTaskData *data = userdata;
  SculptSession *ss = data->ob->sculpt;
  Sculpt *sd = data->sd;
  const Brush *brush = data->brush;

  PBVHVertexIter vd;

  float bstrength = ss->cache->bstrength;
  CLAMP(bstrength, -1.0f, 1.0f);

  SculptBrushTest test;
  SculptBrushTestFn sculpt_brush_test_sq_fn = SCULPT_brush_test_init_with_falloff_shape(
      ss, &test, data->brush->falloff_shape);

  const int thread_id = BLI_task_parallel_thread_id(tls);
  AutomaskingNodeData automask_data;
  SCULPT_automasking_node_begin(
      data->ob, ss, ss->cache->automasking, &automask_data, data->nodes[n]);

  BKE_pbvh_vertex_iter_begin (ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE) {
    if (!sculpt_brush_test_sq_fn(&test, vd.co)) {
      continue;
    }

    SCULPT_automasking_node_update(ss, &automask_data, &vd);

    const float fade = bstrength * SCULPT_brush_strength_factor(ss,
                                                                brush,
                                                                vd.co,
                                                                sqrtf(test.dist),
                                                                vd.no,
                                                                vd.fno,
                                                                vd.mask ? *vd.mask : 0.0f,
                                                                vd.vertex,
                                                                thread_id,
                                                                &automask_data);

    float disp[3];
    madd_v3_v3v3fl(disp, vd.co, ss->cache->detail_directions[vd.index], fade);
    SCULPT_clip(sd, ss, vd.co, disp);

    if (vd.is_mesh) {
      BKE_pbvh_vert_tag_update_normal(ss->pbvh, vd.vertex);
    }
  }
  BKE_pbvh_vertex_iter_end;
}

static void SCULPT_enhance_details_brush(Sculpt *sd,
                                         Object *ob,
                                         PBVHNode **nodes,
                                         const int totnode)
{
  SculptSession *ss = ob->sculpt;
  Brush *brush = BKE_paint_brush(&sd->paint);

  SCULPT_vertex_random_access_ensure(ss);
  SCULPT_boundary_info_ensure(ob);

  if (SCULPT_stroke_is_first_brush_step(ss->cache)) {
    const int totvert = SCULPT_vertex_count_get(ss);
    ss->cache->detail_directions = MEM_malloc_arrayN(
        totvert, sizeof(float[3]), "details directions");

    for (int i = 0; i < totvert; i++) {
      PBVHVertRef vertex = BKE_pbvh_index_to_vertex(ss->pbvh, i);

      float avg[3];
      SCULPT_neighbor_coords_average(ss, avg, vertex);
      sub_v3_v3v3(ss->cache->detail_directions[i], avg, SCULPT_vertex_co_get(ss, vertex));
    }
  }

  SculptThreadedTaskData data = {
      .sd = sd,
      .ob = ob,
      .brush = brush,
      .nodes = nodes,
  };

  TaskParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, true, totnode);
  BLI_task_parallel_range(0, totnode, &data, do_enhance_details_brush_task_cb_ex, &settings);
}

static void do_smooth_brush_task_cb_ex(void *__restrict userdata,
                                       const int n,
                                       const TaskParallelTLS *__restrict tls)
{
  SculptThreadedTaskData *data = userdata;
  SculptSession *ss = data->ob->sculpt;
  Sculpt *sd = data->sd;
  const Brush *brush = data->brush;
  const bool smooth_mask = data->smooth_mask;
  float bstrength = data->strength;

  PBVHVertexIter vd;

  CLAMP(bstrength, 0.0f, 1.0f);

  SculptBrushTest test;
  SculptBrushTestFn sculpt_brush_test_sq_fn = SCULPT_brush_test_init_with_falloff_shape(
      ss, &test, data->brush->falloff_shape);

  const int thread_id = BLI_task_parallel_thread_id(tls);
  AutomaskingNodeData automask_data;
  SCULPT_automasking_node_begin(
      data->ob, ss, ss->cache->automasking, &automask_data, data->nodes[n]);

  BKE_pbvh_vertex_iter_begin (ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE) {
    if (!sculpt_brush_test_sq_fn(&test, vd.co)) {
      continue;
    }

    SCULPT_automasking_node_update(ss, &automask_data, &vd);

    const float fade = bstrength * SCULPT_brush_strength_factor(
                                       ss,
                                       brush,
                                       vd.co,
                                       sqrtf(test.dist),
                                       vd.no,
                                       vd.fno,
                                       smooth_mask ? 0.0f : (vd.mask ? *vd.mask : 0.0f),
                                       vd.vertex,
                                       thread_id,
                                       &automask_data);
    if (smooth_mask) {
      float val = SCULPT_neighbor_mask_average(ss, vd.vertex) - *vd.mask;
      val *= fade * bstrength;
      *vd.mask += val;
      CLAMP(*vd.mask, 0.0f, 1.0f);
    }
    else {
      float avg[3], val[3];
      SCULPT_neighbor_coords_average_interior(ss, avg, vd.vertex);
      sub_v3_v3v3(val, avg, vd.co);
      madd_v3_v3v3fl(val, vd.co, val, fade);
      SCULPT_clip(sd, ss, vd.co, val);
      if (vd.is_mesh) {
        BKE_pbvh_vert_tag_update_normal(ss->pbvh, vd.vertex);
      }
    }
  }
  BKE_pbvh_vertex_iter_end;
}

void SCULPT_smooth(Sculpt *sd,
                   Object *ob,
                   PBVHNode **nodes,
                   const int totnode,
                   float bstrength,
                   const bool smooth_mask)
{
  SculptSession *ss = ob->sculpt;
  Brush *brush = BKE_paint_brush(&sd->paint);

  const int max_iterations = 4;
  const float fract = 1.0f / max_iterations;
  PBVHType type = BKE_pbvh_type(ss->pbvh);
  int iteration, count;
  float last;

  CLAMP(bstrength, 0.0f, 1.0f);

  count = (int)(bstrength * max_iterations);
  last = max_iterations * (bstrength - count * fract);

  if (type == PBVH_FACES && !ss->pmap) {
    BLI_assert_msg(0, "sculpt smooth: pmap missing");
    return;
  }

  SCULPT_vertex_random_access_ensure(ss);
  SCULPT_boundary_info_ensure(ob);

  for (iteration = 0; iteration <= count; iteration++) {
    const float strength = (iteration != count) ? 1.0f : last;

    SculptThreadedTaskData data = {
        .sd = sd,
        .ob = ob,
        .brush = brush,
        .nodes = nodes,
        .smooth_mask = smooth_mask,
        .strength = strength,
    };

    TaskParallelSettings settings;
    BKE_pbvh_parallel_range_settings(&settings, true, totnode);
    BLI_task_parallel_range(0, totnode, &data, do_smooth_brush_task_cb_ex, &settings);
  }
}

void SCULPT_do_smooth_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
  SculptSession *ss = ob->sculpt;

  /* NOTE: The enhance brush needs to initialize its state on the first brush step. The stroke
   * strength can become 0 during the stroke, but it can not change sign (the sign is determined
   * in the beginning of the stroke. So here it is important to not switch to enhance brush in the
   * middle of the stroke. */
  if (ss->cache->bstrength < 0.0f) {
    /* Invert mode, intensify details. */
    SCULPT_enhance_details_brush(sd, ob, nodes, totnode);
  }
  else {
    /* Regular mode, smooth. */
    SCULPT_smooth(sd, ob, nodes, totnode, ss->cache->bstrength, false);
  }
}

/* HC Smooth Algorithm. */
/* From: Improved Laplacian Smoothing of Noisy Surface Meshes */

void SCULPT_surface_smooth_laplacian_step(SculptSession *ss,
                                          float *disp,
                                          const float co[3],
                                          float (*laplacian_disp)[3],
                                          const PBVHVertRef vertex,
                                          const float origco[3],
                                          const float alpha)
{
  float laplacian_smooth_co[3];
  float weigthed_o[3], weigthed_q[3], d[3];
  int v_index = BKE_pbvh_vertex_to_index(ss->pbvh, vertex);

  SCULPT_neighbor_coords_average(ss, laplacian_smooth_co, vertex);

  mul_v3_v3fl(weigthed_o, origco, alpha);
  mul_v3_v3fl(weigthed_q, co, 1.0f - alpha);
  add_v3_v3v3(d, weigthed_o, weigthed_q);
  sub_v3_v3v3(laplacian_disp[v_index], laplacian_smooth_co, d);

  sub_v3_v3v3(disp, laplacian_smooth_co, co);
}

void SCULPT_surface_smooth_displace_step(SculptSession *ss,
                                         float *co,
                                         float (*laplacian_disp)[3],
                                         const PBVHVertRef vertex,
                                         const float beta,
                                         const float fade)
{
  float b_avg[3] = {0.0f, 0.0f, 0.0f};
  float b_current_vertex[3];
  int total = 0;
  SculptVertexNeighborIter ni;
  SCULPT_VERTEX_NEIGHBORS_ITER_BEGIN (ss, vertex, ni) {
    add_v3_v3(b_avg, laplacian_disp[ni.index]);
    total++;
  }

  SCULPT_VERTEX_NEIGHBORS_ITER_END(ni);
  if (total > 0) {
    int v_index = BKE_pbvh_vertex_to_index(ss->pbvh, vertex);

    mul_v3_v3fl(b_current_vertex, b_avg, (1.0f - beta) / total);
    madd_v3_v3fl(b_current_vertex, laplacian_disp[v_index], beta);
    mul_v3_fl(b_current_vertex, clamp_f(fade, 0.0f, 1.0f));
    sub_v3_v3(co, b_current_vertex);
  }
}

static void SCULPT_do_surface_smooth_brush_laplacian_task_cb_ex(
    void *__restrict userdata, const int n, const TaskParallelTLS *__restrict tls)
{
  SculptThreadedTaskData *data = userdata;
  SculptSession *ss = data->ob->sculpt;
  const Brush *brush = data->brush;
  const float bstrength = ss->cache->bstrength;
  float alpha = brush->surface_smooth_shape_preservation;

  PBVHVertexIter vd;
  SculptOrigVertData orig_data;

  SculptBrushTest test;
  SculptBrushTestFn sculpt_brush_test_sq_fn = SCULPT_brush_test_init_with_falloff_shape(
      ss, &test, data->brush->falloff_shape);
  const int thread_id = BLI_task_parallel_thread_id(tls);

  SCULPT_orig_vert_data_init(&orig_data, data->ob, data->nodes[n], SCULPT_UNDO_COORDS);
  AutomaskingNodeData automask_data;
  SCULPT_automasking_node_begin(
      data->ob, ss, ss->cache->automasking, &automask_data, data->nodes[n]);

  BKE_pbvh_vertex_iter_begin (ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE) {
    SCULPT_orig_vert_data_update(&orig_data, &vd);
    if (!sculpt_brush_test_sq_fn(&test, vd.co)) {
      continue;
    }

    SCULPT_automasking_node_update(ss, &automask_data, &vd);

    const float fade = bstrength * SCULPT_brush_strength_factor(ss,
                                                                brush,
                                                                vd.co,
                                                                sqrtf(test.dist),
                                                                vd.no,
                                                                vd.fno,
                                                                vd.mask ? *vd.mask : 0.0f,
                                                                vd.vertex,
                                                                thread_id,
                                                                &automask_data);

    float disp[3];
    SCULPT_surface_smooth_laplacian_step(
        ss, disp, vd.co, ss->cache->surface_smooth_laplacian_disp, vd.vertex, orig_data.co, alpha);
    madd_v3_v3fl(vd.co, disp, clamp_f(fade, 0.0f, 1.0f));
    if (vd.is_mesh) {
      BKE_pbvh_vert_tag_update_normal(ss->pbvh, vd.vertex);
    }
  }
  BKE_pbvh_vertex_iter_end;
}

static void SCULPT_do_surface_smooth_brush_displace_task_cb_ex(
    void *__restrict userdata, const int n, const TaskParallelTLS *__restrict tls)
{
  SculptThreadedTaskData *data = userdata;
  SculptSession *ss = data->ob->sculpt;
  const Brush *brush = data->brush;
  const float bstrength = ss->cache->bstrength;
  const float beta = brush->surface_smooth_current_vertex;

  PBVHVertexIter vd;

  SculptBrushTest test;
  SculptBrushTestFn sculpt_brush_test_sq_fn = SCULPT_brush_test_init_with_falloff_shape(
      ss, &test, data->brush->falloff_shape);
  const int thread_id = BLI_task_parallel_thread_id(tls);
  AutomaskingNodeData automask_data;
  SCULPT_automasking_node_begin(
      data->ob, ss, ss->cache->automasking, &automask_data, data->nodes[n]);

  BKE_pbvh_vertex_iter_begin (ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE) {
    if (!sculpt_brush_test_sq_fn(&test, vd.co)) {
      continue;
    }

    SCULPT_automasking_node_update(ss, &automask_data, &vd);

    const float fade = bstrength * SCULPT_brush_strength_factor(ss,
                                                                brush,
                                                                vd.co,
                                                                sqrtf(test.dist),
                                                                vd.no,
                                                                vd.fno,
                                                                vd.mask ? *vd.mask : 0.0f,
                                                                vd.vertex,
                                                                thread_id,
                                                                &automask_data);
    SCULPT_surface_smooth_displace_step(
        ss, vd.co, ss->cache->surface_smooth_laplacian_disp, vd.vertex, beta, fade);
  }
  BKE_pbvh_vertex_iter_end;
}

void SCULPT_do_surface_smooth_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
  Brush *brush = BKE_paint_brush(&sd->paint);

  /* Threaded loop over nodes. */
  SculptThreadedTaskData data = {
      .sd = sd,
      .ob = ob,
      .brush = brush,
      .nodes = nodes,
  };

  TaskParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, true, totnode);
  for (int i = 0; i < brush->surface_smooth_iterations; i++) {
    BLI_task_parallel_range(
        0, totnode, &data, SCULPT_do_surface_smooth_brush_laplacian_task_cb_ex, &settings);
    BLI_task_parallel_range(
        0, totnode, &data, SCULPT_do_surface_smooth_brush_displace_task_cb_ex, &settings);
  }
}
