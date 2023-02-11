/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2020 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup edsculpt
 */

#include "MEM_guardedalloc.h"

#include "BLI_alloca.h"
#include "BLI_math.h"
#include "BLI_task.h"
#include "BLI_vector.hh"

#include "DNA_brush_types.h"
#include "DNA_meshdata_types.h"

#include "BKE_context.h"
#include "BKE_paint.h"
#include "BKE_pbvh.h"

#include "sculpt_intern.hh"

#include "bmesh.h"

#include <cmath>
#include <cstdlib>

using blender::Vector;

ATTR_NO_OPT static void SCULPT_neighbor_coords_average_interior_ex(SculptSession *ss,
                                                                   float result[3],
                                                                   PBVHVertRef vertex,
                                                                   float projection,
                                                                   float fset_projection,
                                                                   bool weighted,
                                                                   eSculptBoundary bound_type,
                                                                   eSculptCorner corner_type)
{
  float avg[3] = {0.0f, 0.0f, 0.0f};
  int total = 0;
  int neighbor_count = 0;

  const eSculptBoundary is_boundary = SCULPT_vertex_is_boundary(ss, vertex, bound_type);
  const eSculptCorner is_corner = SCULPT_vertex_is_corner(ss, vertex, corner_type);

  if ((is_boundary & SCULPT_BOUNDARY_FACE_SET) || (is_corner & SCULPT_CORNER_FACE_SET)) {
    projection = max_ff(projection, fset_projection);
  }

  SculptVertexNeighborIter ni;
  SCULPT_VERTEX_NEIGHBORS_ITER_BEGIN (ss, vertex, ni) {
    neighbor_count++;
    if (is_boundary) {
      /* Boundary vertices use only other boundary vertices. */
      if (SCULPT_vertex_is_boundary(ss, ni.vertex, bound_type)) {
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
  if (is_corner & (SCULPT_CORNER_MESH | SCULPT_CORNER_SHARP)) {
    copy_v3_v3(result, SCULPT_vertex_co_get(ss, vertex));
    return;
  }

  /* Avoid division by 0 when there are no neighbors. */
  if (total == 0) {
    copy_v3_v3(result, SCULPT_vertex_co_get(ss, vertex));
    return;
  }

  float no[3];
  const float *co = SCULPT_vertex_co_get(ss, vertex);
  SCULPT_vertex_normal_get(ss, vertex, no);

  mul_v3_fl(avg, 1.0f / (float)total);
  sub_v3_v3(avg, co);
  float t = dot_v3v3(avg, no);

  madd_v3_v3fl(avg, no, -t * projection);
  add_v3_v3(avg, co);
  copy_v3_v3(result, avg);
}

void SCULPT_neighbor_coords_average_interior(SculptSession *ss,
                                             float result[3],
                                             PBVHVertRef vertex,
                                             float projection,
                                             float fset_projection)
{
  eSculptBoundary bound_type = SCULPT_BOUNDARY_MESH | SCULPT_BOUNDARY_FACE_SET |
                               SCULPT_BOUNDARY_SEAM | SCULPT_BOUNDARY_SHARP;
  eSculptCorner corner_type = SCULPT_CORNER_MESH | SCULPT_CORNER_FACE_SET | SCULPT_CORNER_SEAM |
                              SCULPT_CORNER_SHARP;
  SCULPT_neighbor_coords_average_interior_ex(
      ss, result, vertex, projection, fset_projection, true, bound_type, corner_type);
}

void SCULPT_bmesh_four_neighbor_average(SculptSession *ss,
                                        float avg[3],
                                        float direction[3],
                                        struct BMVert *v,
                                        float projection,
                                        bool check_fsets,
                                        int cd_temp,
                                        int cd_sculpt_vert,
                                        bool do_origco)
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

void SCULPT_neighbor_coords_average(SculptSession *ss,
                                    float result[3],
                                    PBVHVertRef vertex,
                                    float projection,
                                    float fset_projection,
                                    bool weighted)
{
  eSculptCorner corner_type = SCULPT_CORNER_SHARP | SCULPT_CORNER_FACE_SET;
  eSculptBoundary bound_type = SCULPT_BOUNDARY_SHARP | SCULPT_BOUNDARY_SEAM | SCULPT_BOUNDARY_UV |
                               SCULPT_BOUNDARY_FACE_SET;

  SCULPT_neighbor_coords_average_interior_ex(
      ss, result, vertex, projection, fset_projection, weighted, bound_type, corner_type);
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
  SculptThreadedTaskData *data = static_cast<SculptThreadedTaskData *>(userdata);
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

  float projection = brush->autosmooth_projection;
  float fset_projection = brush->autosmooth_fset_slide;
  bool use_weighted = brush->flag2 & BRUSH_SMOOTH_USE_AREA_WEIGHT;

  if (SCULPT_stroke_is_first_brush_step(ss->cache)) {
    const int totvert = SCULPT_vertex_count_get(ss);
    ss->cache->detail_directions = static_cast<float(*)[3]>(
        MEM_malloc_arrayN(totvert, sizeof(float[3]), "details directions"));

    for (int i = 0; i < totvert; i++) {
      PBVHVertRef vertex = BKE_pbvh_index_to_vertex(ss->pbvh, i);

      float avg[3];
      SCULPT_neighbor_coords_average(ss, avg, vertex, projection, fset_projection, use_weighted);
      sub_v3_v3v3(ss->cache->detail_directions[i], avg, SCULPT_vertex_co_get(ss, vertex));
    }
  }

  SculptThreadedTaskData data{};
  data.sd = sd;
  data.ob = ob;
  data.brush = brush;
  data.nodes = nodes;

  TaskParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, true, totnode);
  BLI_task_parallel_range(0, totnode, &data, do_enhance_details_brush_task_cb_ex, &settings);
}

static void do_smooth_brush_task_cb_ex(void *__restrict userdata,
                                       const int n,
                                       const TaskParallelTLS *__restrict tls)
{
  SculptThreadedTaskData *data = static_cast<SculptThreadedTaskData *>(userdata);
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

  float fset_projection = SCULPT_get_fset_projection(ss, brush->autosmooth_fset_slide);
  float projection = brush->autosmooth_projection;

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
      SCULPT_neighbor_coords_average_interior(ss, avg, vd.vertex, projection, fset_projection);
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

  count = int(bstrength * max_iterations);
  last = max_iterations * (bstrength - count * fract);

  if (type == PBVH_FACES && !ss->pmap) {
    BLI_assert_msg(0, "sculpt smooth: pmap missing");
    return;
  }

  SCULPT_vertex_random_access_ensure(ss);
  SCULPT_boundary_info_ensure(ob);

  for (iteration = 0; iteration <= count; iteration++) {
    const float strength = (iteration != count) ? 1.0f : last;

    SculptThreadedTaskData data{};
    data.sd = sd;
    data.ob = ob;
    data.brush = brush;
    data.nodes = nodes;
    data.smooth_mask = smooth_mask;
    data.strength = strength;

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

void SCULPT_surface_smooth_laplacian_init(Object *ob)
{
  SculptAttributeParams params = {};

  params.stroke_only = true;

  ob->sculpt->attrs.laplacian_disp = BKE_sculpt_attribute_ensure(
      ob, ATTR_DOMAIN_POINT, CD_PROP_FLOAT3, SCULPT_ATTRIBUTE_NAME(laplacian_disp), &params);
}

/* HC Smooth Algorithm. */
/* From: Improved Laplacian Smoothing of Noisy Surface Meshes */

void SCULPT_surface_smooth_laplacian_step(SculptSession *ss,
                                          float *disp,
                                          const float co[3],
                                          const PBVHVertRef vertex,
                                          const float origco[3],
                                          const float alpha)
{
  float laplacian_smooth_co[3];
  float weigthed_o[3], weigthed_q[3], d[3];
  int v_index = BKE_pbvh_vertex_to_index(ss->pbvh, vertex);

  SCULPT_neighbor_coords_average(ss, laplacian_smooth_co, vertex, 0.0f, 1.0f, true);

  mul_v3_v3fl(weigthed_o, origco, alpha);
  mul_v3_v3fl(weigthed_q, co, 1.0f - alpha);
  add_v3_v3v3(d, weigthed_o, weigthed_q);
  float *laplacian_disp = SCULPT_vertex_attr_get<float *>(vertex, ss->attrs.laplacian_disp);

  sub_v3_v3v3(laplacian_disp, laplacian_smooth_co, d);

  sub_v3_v3v3(disp, laplacian_smooth_co, co);
}

void SCULPT_surface_smooth_displace_step(
    SculptSession *ss, float *co, const PBVHVertRef vertex, const float beta, const float fade)
{
  float b_avg[3] = {0.0f, 0.0f, 0.0f};
  float b_current_vertex[3];
  int total = 0;
  SculptVertexNeighborIter ni;
  SCULPT_VERTEX_NEIGHBORS_ITER_BEGIN (ss, vertex, ni) {
    float *laplacian_disp = SCULPT_vertex_attr_get<float *>(ni.vertex, ss->attrs.laplacian_disp);
    add_v3_v3(b_avg, laplacian_disp);
    total++;
  }

  SCULPT_VERTEX_NEIGHBORS_ITER_END(ni);
  if (total > 0) {
    float *laplacian_disp = SCULPT_vertex_attr_get<float *>(vertex, ss->attrs.laplacian_disp);

    mul_v3_v3fl(b_current_vertex, b_avg, (1.0f - beta) / total);
    madd_v3_v3fl(b_current_vertex, laplacian_disp, beta);
    mul_v3_fl(b_current_vertex, clamp_f(fade, 0.0f, 1.0f));
    sub_v3_v3(co, b_current_vertex);
  }
}

static void SCULPT_do_surface_smooth_brush_laplacian_task_cb_ex(
    void *__restrict userdata, const int n, const TaskParallelTLS *__restrict tls)
{
  SculptThreadedTaskData *data = static_cast<SculptThreadedTaskData *>(userdata);
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
    SCULPT_orig_vert_data_update(&orig_data, vd.vertex);
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
    SCULPT_surface_smooth_laplacian_step(ss, disp, vd.co, vd.vertex, orig_data.co, alpha);
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
  SculptThreadedTaskData *data = static_cast<SculptThreadedTaskData *>(userdata);
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
    SCULPT_surface_smooth_displace_step(ss, vd.co, vd.vertex, beta, fade);
  }
  BKE_pbvh_vertex_iter_end;
}

void SCULPT_do_surface_smooth_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
  Brush *brush = BKE_paint_brush(&sd->paint);

  /* Threaded loop over nodes. */
  SculptThreadedTaskData data{};
  data.sd = sd;
  data.ob = ob;
  data.brush = brush;
  data.nodes = nodes;

  TaskParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, true, totnode);
  for (int i = 0; i < brush->surface_smooth_iterations; i++) {
    BLI_task_parallel_range(
        0, totnode, &data, SCULPT_do_surface_smooth_brush_laplacian_task_cb_ex, &settings);
    BLI_task_parallel_range(
        0, totnode, &data, SCULPT_do_surface_smooth_brush_displace_task_cb_ex, &settings);
  }
}

void SCULPT_reproject_cdata(SculptSession *ss,
                            PBVHVertRef vertex,
                            float origco[3],
                            float origno[3])
{
  BMVert *v = (BMVert *)vertex.i;

  if (!ss->bm || !v->e) {
    return;
  }

  MSculptVert *mv = BKE_PBVH_SCULPTVERT(ss->cd_sculpt_vert, v);

  // int totuv = CustomData_number_of_layers(&ss->bm->ldata, CD_PROP_FLOAT2);
  CustomData *ldata = &ss->bm->ldata;

  int totuv = 0;
  CustomDataLayer *uvlayer = NULL;

  if (ldata->typemap[CD_PROP_FLOAT2] != -1) {
    for (int i = ldata->typemap[CD_PROP_FLOAT2];
         i < ldata->totlayer && ldata->layers[i].type == CD_PROP_FLOAT2;
         i++) {
      totuv++;
    }

    uvlayer = ldata->layers + ldata->typemap[CD_PROP_FLOAT2];
  }

  BMEdge *e;
  int tag = BM_ELEM_TAG_ALT;

  float origin[3];
  float ray[3];

  copy_v3_v3(origin, v->co);
  copy_v3_v3(ray, v->no);
  negate_v3(ray);

  struct IsectRayPrecalc precalc;
  isect_ray_tri_watertight_v3_precalc(&precalc, ray);

  float *lastuvs = (float *)BLI_array_alloca(lastuvs, totuv * 2);
  bool *snapuvs = (bool *)BLI_array_alloca(snapuvs, totuv);

  e = v->e;

  /* first clear some flags */
  do {
    e->head.api_flag &= ~tag;

    if (!e->l) {
      continue;
    }

    BMLoop *l = e->l;
    do {
      l->head.hflag &= ~tag;
      l->next->head.hflag &= ~tag;
      l->prev->head.hflag &= ~tag;
    } while ((l = l->radial_next) != e->l);
  } while ((e = BM_DISK_EDGE_NEXT(e, v)) != v->e);

  Vector<BMLoop *, 32> ls;

  bool first = true;
  bool bad = false;

  for (int i = 0; i < totuv; i++) {
    snapuvs[i] = true;  //!(mv->flag & SCULPTVERT_UV_BOUNDARY);
  }

  do {
    BMLoop *l = e->l;

    if (!l) {
      continue;
    }
#if 0
    bool bound = l == l->radial_next;

    // check for faceset boundaries
    bound = bound || (BM_ELEM_CD_GET_INT(l->f,ss->cd_faceset_offset) !=
      BM_ELEM_CD_GET_INT(l->radial_next->f,ss->cd_faceset_offset));

    // check for seam and sharp edges
    bound = bound || (e->head.hflag & BM_ELEM_SEAM) || !(e->head.hflag & BM_ELEM_SMOOTH);

    if (bound) {
      continue;
    }
#endif
    do {
      BMLoop *l2 = l->v != v ? l->next : l;

      if (l2->head.hflag & tag) {
        continue;
      }

      l2->head.hflag |= tag;
      ls.append(l2);

      for (int i = 0; i < totuv; i++) {
        const int cd_uv = uvlayer[i].offset;
        float *luv = BM_ELEM_CD_PTR<float *>(l2, cd_uv);

        // check that we are not part of a uv seam
        if (!first) {
          const float dx = lastuvs[i * 2] - luv[0];
          const float dy = lastuvs[i * 2 + 1] - luv[1];
          const float eps = 0.00001f;

          if (dx * dx + dy * dy > eps) {
            bad = true;
            snapuvs[i] = false;
          }
        }

        lastuvs[i * 2] = luv[0];
        lastuvs[i * 2 + 1] = luv[1];
      }

      first = false;

      if (bad) {
        break;
      }
    } while ((l = l->radial_next) != e->l);

    if (bad) {
      break;
    }
  } while ((e = BM_DISK_EDGE_NEXT(e, v)) != v->e);

  if (bad || !ls.size()) {
    return;
  }

  int totloop = ls.size();

  const float *v_proj_axis = v->no;
  float v_proj[3][3];

  project_plane_normalized_v3_v3v3(v_proj[1], mv->origco, v_proj_axis);

  /* original (l->prev, l, l->next) projections for each loop ('l' remains unchanged) */

  char *_blocks = (char *)alloca(ldata->totsize * totloop);
  void **blocks = BLI_array_alloca(blocks, totloop);

  for (int i = 0; i < totloop; i++, _blocks += ldata->totsize) {
    blocks[i] = (void *)_blocks;
  }

  float vco[3], vno[3];

  copy_v3_v3(vco, v->co);
  copy_v3_v3(vno, v->no);

  BMFace _fakef, *fakef = &_fakef;

#if 0
  BMFace *projf = NULL;
  // find face vertex projects into
  for (int i = 0; i < totloop; i++) {
    BMLoop *l = ls[i];

    copy_v3_v3(ray,l->f->no);
    negate_v3(ray);

    float t,uv[2];

    //*
    bool hit = isect_ray_tri_v3(origin,ray,l->prev->v->co,origco,l->next->v->co,&t,uv);
    if (hit) {
      projf = l->f;
      break;
    }  //*/
  }

  if (!projf) {
    return;
  }
#endif

  // build fake f with original coordinates
  for (int i = 0; i < totloop; i++) {
    // create fake face
    BMLoop *l = ls[i];
    float no[3] = {0.0f, 0.0f, 0.0f};

    BMLoop *fakels = BLI_array_alloca(fakels, l->f->len);
    BMVert *fakevs = BLI_array_alloca(fakevs, l->f->len);
    BMLoop *l2 = l->f->l_first;
    BMLoop *fakel = fakels;
    BMVert *fakev = fakevs;
    int j = 0;

    do {
      *fakel = *l2;
      fakel->next = fakels + ((j + 1) % l->f->len);
      fakel->prev = fakels + ((j + l->f->len - 1) % l->f->len);

      *fakev = *l2->v;
      fakel->v = fakev;

      SCULPT_vertex_check_origdata(ss, (PBVHVertRef){.i = (intptr_t)l2->v});

      if (l2->v == v) {
        copy_v3_v3(fakev->co, origco);
        copy_v3_v3(fakev->no, origno);
        add_v3_v3(no, origno);
      }
      else {
        add_v3_v3(no, l2->v->no);
      }

      fakel++;
      fakev++;
      j++;
    } while ((l2 = l2->next) != l->f->l_first);

    *fakef = *l->f;
    fakef->l_first = fakels;

    // set original face normal
    // normalize_v3(no);
    // copy_v3_v3(fakef->no, no);

    // interpolate
    BMLoop _interpl, *interpl = &_interpl;

    MSculptVert saved = *mv;

    *interpl = *l;
    interpl->head.data = blocks[i];
    // memcpy(interpl->head.data, l2->head.data, ldata->totsize);

    BM_loop_interp_from_face(ss->bm, interpl, fakef, false, false);

    *mv = saved;

    CustomData_bmesh_copy_data(&ss->bm->ldata, &ss->bm->ldata, interpl->head.data, &l->head.data);
  }

  int *tots = BLI_array_alloca(tots, totuv);

  for (int i = 0; i < totuv; i++) {
    lastuvs[i * 2] = lastuvs[i * 2 + 1] = 0.0f;
    tots[i] = 0;
  }

  // re-snap uvs
  v = (BMVert *)vertex.i;

  e = v->e;
  do {
    if (!e->l) {
      continue;
    }

    BMLoop *l_iter = e->l;
    do {
      BMLoop *l = l_iter->v != v ? l_iter->next : l_iter;

      for (int i = 0; i < totuv; i++) {
        const int cd_uv = uvlayer[i].offset;
        float *luv = BM_ELEM_CD_PTR<float *>(l, cd_uv);

        add_v2_v2(lastuvs + i * 2, luv);
        tots[i]++;
      }
    } while ((l_iter = l_iter->radial_next) != e->l);
  } while ((e = BM_DISK_EDGE_NEXT(e, v)) != v->e);

  for (int i = 0; i < totuv; i++) {
    if (tots[i]) {
      mul_v2_fl(lastuvs + i * 2, 1.0f / (float)tots[i]);
    }
  }

  e = v->e;
  do {
    if (!e->l) {
      continue;
    }

    BMLoop *l_iter = e->l;
    do {
      BMLoop *l = l_iter->v != v ? l_iter->next : l_iter;

      for (int i = 0; i < totuv; i++) {
        const int cd_uv = uvlayer[i].offset;
        float *luv = BM_ELEM_CD_PTR<float *>(l, cd_uv);

        if (snapuvs[i]) {
          copy_v2_v2(luv, lastuvs + i * 2);
        }
      }
    } while ((l_iter = l_iter->radial_next) != e->l);
  } while ((e = BM_DISK_EDGE_NEXT(e, v)) != v->e);
}
