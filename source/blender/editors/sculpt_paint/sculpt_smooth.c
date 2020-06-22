/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2020 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup edsculpt
 */

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_hash.h"
#include "BLI_math.h"
#include "BLI_task.h"

#include "DNA_brush_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "BKE_brush.h"
#include "BKE_context.h"
#include "BKE_mesh.h"
#include "BKE_mesh_mapping.h"
#include "BKE_object.h"
#include "BKE_paint.h"
#include "BKE_pbvh.h"
#include "BKE_scene.h"

#include "DEG_depsgraph.h"

#include "WM_api.h"
#include "WM_message.h"
#include "WM_toolsystem.h"
#include "WM_types.h"

#include "ED_object.h"
#include "ED_screen.h"
#include "ED_sculpt.h"
#include "paint_intern.h"
#include "sculpt_intern.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "bmesh.h"

#include <math.h>
#include <stdlib.h>

/* For the smooth brush, uses the neighboring vertices around vert to calculate
 * a smoothed location for vert. Skips corner vertices (used by only one
 * polygon). */
void SCULPT_neighbor_average(SculptSession *ss, float avg[3], uint vert)
{
  const MeshElemMap *vert_map = &ss->pmap[vert];
  const MVert *mvert = ss->mvert;
  float(*deform_co)[3] = ss->deform_cos;

  /* Don't modify corner vertices. */
  if (vert_map->count > 1) {
    int total = 0;

    zero_v3(avg);

    for (int i = 0; i < vert_map->count; i++) {
      const MPoly *p = &ss->mpoly[vert_map->indices[i]];
      uint f_adj_v[2];

      if (poly_get_adj_loops_from_vert(p, ss->mloop, vert, f_adj_v) != -1) {
        for (int j = 0; j < ARRAY_SIZE(f_adj_v); j += 1) {
          if (vert_map->count != 2 || ss->pmap[f_adj_v[j]].count <= 2) {
            add_v3_v3(avg, deform_co ? deform_co[f_adj_v[j]] : mvert[f_adj_v[j]].co);

            total++;
          }
        }
      }
    }

    if (total > 0) {
      mul_v3_fl(avg, 1.0f / total);
      return;
    }
  }

  copy_v3_v3(avg, deform_co ? deform_co[vert] : mvert[vert].co);
}

/* Same logic as neighbor_average(), but for bmesh rather than mesh. */
void SCULPT_bmesh_neighbor_average(float avg[3], BMVert *v)
{
  /* logic for 3 or more is identical. */
  const int vfcount = BM_vert_face_count_at_most(v, 3);

  /* Don't modify corner vertices. */
  if (vfcount > 1) {
    BMIter liter;
    BMLoop *l;
    int total = 0;

    zero_v3(avg);

    BM_ITER_ELEM (l, &liter, v, BM_LOOPS_OF_VERT) {
      const BMVert *adj_v[2] = {l->prev->v, l->next->v};

      for (int i = 0; i < ARRAY_SIZE(adj_v); i++) {
        const BMVert *v_other = adj_v[i];
        if (vfcount != 2 || BM_vert_face_count_at_most(v_other, 2) <= 2) {
          add_v3_v3(avg, v_other->co);
          total++;
        }
      }
    }

    if (total > 0) {
      mul_v3_fl(avg, 1.0f / total);
      return;
    }
  }

  copy_v3_v3(avg, v->co);
}

/* For bmesh: Average surrounding verts based on an orthogonality measure.
 * Naturally converges to a quad-like structure. */
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

void SCULPT_neighbor_coords_average(SculptSession *ss, float result[3], int index)
{
  float avg[3] = {0.0f, 0.0f, 0.0f};
  int total = 0;

  SculptVertexNeighborIter ni;
  SCULPT_VERTEX_NEIGHBORS_ITER_BEGIN (ss, index, ni) {
    add_v3_v3(avg, SCULPT_vertex_co_get(ss, ni.index));
    total++;
  }
  SCULPT_VERTEX_NEIGHBORS_ITER_END(ni);

  if (total > 0) {
    mul_v3_v3fl(result, avg, 1.0f / (float)total);
  }
  else {
    copy_v3_v3(result, SCULPT_vertex_co_get(ss, index));
  }
}

float SCULPT_neighbor_mask_average(SculptSession *ss, int index)
{
  float avg = 0.0f;
  int total = 0;

  SculptVertexNeighborIter ni;
  SCULPT_VERTEX_NEIGHBORS_ITER_BEGIN (ss, index, ni) {
    avg += SCULPT_vertex_mask_get(ss, ni.index);
    total++;
  }
  SCULPT_VERTEX_NEIGHBORS_ITER_END(ni);

  if (total > 0) {
    return avg / (float)total;
  }
  else {
    return SCULPT_vertex_mask_get(ss, index);
  }
}

void SCULPT_neighbor_color_average(SculptSession *ss, float result[4], int index)
{
  float avg[4] = {0.0f, 0.0f, 0.0f, 0.0f};
  int total = 0;

  SculptVertexNeighborIter ni;
  SCULPT_VERTEX_NEIGHBORS_ITER_BEGIN (ss, index, ni) {
    add_v4_v4(avg, SCULPT_vertex_color_get(ss, ni.index));
    total++;
  }
  SCULPT_VERTEX_NEIGHBORS_ITER_END(ni);

  if (total > 0) {
    mul_v4_v4fl(result, avg, 1.0f / (float)total);
  }
  else {
    copy_v4_v4(result, SCULPT_vertex_color_get(ss, index));
  }
}

static void do_smooth_brush_mesh_task_cb_ex(void *__restrict userdata,
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

  BKE_pbvh_vertex_iter_begin(ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE)
  {
    if (sculpt_brush_test_sq_fn(&test, vd.co)) {
      const float fade = bstrength * SCULPT_brush_strength_factor(
                                         ss,
                                         brush,
                                         vd.co,
                                         sqrtf(test.dist),
                                         vd.no,
                                         vd.fno,
                                         smooth_mask ? 0.0f : (vd.mask ? *vd.mask : 0.0f),
                                         vd.index,
                                         thread_id);
      if (smooth_mask) {
        float val = SCULPT_neighbor_mask_average(ss, vd.vert_indices[vd.i]) - *vd.mask;
        val *= fade * bstrength;
        *vd.mask += val;
        CLAMP(*vd.mask, 0.0f, 1.0f);
      }
      else {
        float avg[3], val[3];

        SCULPT_neighbor_average(ss, avg, vd.vert_indices[vd.i]);
        sub_v3_v3v3(val, avg, vd.co);

        madd_v3_v3v3fl(val, vd.co, val, fade);

        SCULPT_clip(sd, ss, vd.co, val);
      }

      if (vd.mvert) {
        vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
      }
    }
  }
  BKE_pbvh_vertex_iter_end;
}

static void do_smooth_brush_bmesh_task_cb_ex(void *__restrict userdata,
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

  BKE_pbvh_vertex_iter_begin(ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE)
  {
    if (sculpt_brush_test_sq_fn(&test, vd.co)) {
      const float fade = bstrength * SCULPT_brush_strength_factor(ss,
                                                                  brush,
                                                                  vd.co,
                                                                  sqrtf(test.dist),
                                                                  vd.no,
                                                                  vd.fno,
                                                                  smooth_mask ? 0.0f : *vd.mask,
                                                                  vd.index,
                                                                  thread_id);
      if (smooth_mask) {
        float val = SCULPT_neighbor_mask_average(ss, vd.index) - *vd.mask;
        val *= fade * bstrength;
        *vd.mask += val;
        CLAMP(*vd.mask, 0.0f, 1.0f);
      }
      else {
        float avg[3], val[3];

        SCULPT_bmesh_neighbor_average(avg, vd.bm_vert);
        sub_v3_v3v3(val, avg, vd.co);

        madd_v3_v3v3fl(val, vd.co, val, fade);

        SCULPT_clip(sd, ss, vd.co, val);
      }

      if (vd.mvert) {
        vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
      }
    }
  }
  BKE_pbvh_vertex_iter_end;
}

static void do_smooth_brush_multires_task_cb_ex(void *__restrict userdata,
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

  BKE_pbvh_vertex_iter_begin(ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE)
  {
    if (sculpt_brush_test_sq_fn(&test, vd.co)) {
      const float fade = bstrength * SCULPT_brush_strength_factor(
                                         ss,
                                         brush,
                                         vd.co,
                                         sqrtf(test.dist),
                                         vd.no,
                                         vd.fno,
                                         smooth_mask ? 0.0f : (vd.mask ? *vd.mask : 0.0f),
                                         vd.index,
                                         thread_id);
      if (smooth_mask) {
        float val = SCULPT_neighbor_mask_average(ss, vd.index) - *vd.mask;
        val *= fade * bstrength;
        *vd.mask += val;
        CLAMP(*vd.mask, 0.0f, 1.0f);
      }
      else {
        float avg[3], val[3];
        SCULPT_neighbor_coords_average(ss, avg, vd.index);
        sub_v3_v3v3(val, avg, vd.co);
        madd_v3_v3v3fl(val, vd.co, val, fade);
        SCULPT_clip(sd, ss, vd.co, val);
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
    BLI_assert(!"sculpt smooth: pmap missing");
    return;
  }

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
    BKE_pbvh_parallel_range_settings(&settings, (sd->flags & SCULPT_USE_OPENMP), totnode);

    switch (type) {
      case PBVH_GRIDS:
        BLI_task_parallel_range(0, totnode, &data, do_smooth_brush_multires_task_cb_ex, &settings);
        break;
      case PBVH_FACES:
        BLI_task_parallel_range(0, totnode, &data, do_smooth_brush_mesh_task_cb_ex, &settings);
        break;
      case PBVH_BMESH:
        BLI_task_parallel_range(0, totnode, &data, do_smooth_brush_bmesh_task_cb_ex, &settings);
        break;
    }
  }
}

void SCULPT_do_smooth_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
  SculptSession *ss = ob->sculpt;
  SCULPT_smooth(sd, ob, nodes, totnode, ss->cache->bstrength, false);
}

/* HC Smooth Algorithm. */
/* From: Improved Laplacian Smoothing of Noisy Surface Meshes */

void SCULPT_surface_smooth_laplacian_step(SculptSession *ss,
                                          float *disp,
                                          const float co[3],
                                          float (*laplacian_disp)[3],
                                          const int v_index,
                                          const float origco[3],
                                          const float alpha)
{
  float laplacian_smooth_co[3];
  float weigthed_o[3], weigthed_q[3], d[3];
  SCULPT_neighbor_coords_average(ss, laplacian_smooth_co, v_index);

  mul_v3_v3fl(weigthed_o, origco, alpha);
  mul_v3_v3fl(weigthed_q, co, 1.0f - alpha);
  add_v3_v3v3(d, weigthed_o, weigthed_q);
  sub_v3_v3v3(laplacian_disp[v_index], laplacian_smooth_co, d);

  sub_v3_v3v3(disp, laplacian_smooth_co, co);
}

void SCULPT_surface_smooth_displace_step(SculptSession *ss,
                                         float *co,
                                         float (*laplacian_disp)[3],
                                         const int v_index,
                                         const float beta,
                                         const float fade)
{
  float b_avg[3] = {0.0f, 0.0f, 0.0f};
  float b_current_vertex[3];
  int total = 0;
  SculptVertexNeighborIter ni;
  SCULPT_VERTEX_NEIGHBORS_ITER_BEGIN (ss, v_index, ni) {
    add_v3_v3(b_avg, laplacian_disp[ni.index]);
    total++;
  }
  SCULPT_VERTEX_NEIGHBORS_ITER_END(ni);
  if (total > 0) {
    mul_v3_v3fl(b_current_vertex, b_avg, (1.0f - beta) / (float)total);
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

  SCULPT_orig_vert_data_init(&orig_data, data->ob, data->nodes[n]);

  BKE_pbvh_vertex_iter_begin(ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE)
  {
    SCULPT_orig_vert_data_update(&orig_data, &vd);
    if (sculpt_brush_test_sq_fn(&test, vd.co)) {
      const float fade =
          bstrength *
          SCULPT_brush_strength_factor(
              ss, brush, vd.co, sqrtf(test.dist), vd.no, vd.fno, 0.0f, vd.index, thread_id);

      float disp[3];
      SCULPT_surface_smooth_laplacian_step(ss,
                                           disp,
                                           vd.co,
                                           ss->cache->surface_smooth_laplacian_disp,
                                           vd.index,
                                           orig_data.co,
                                           alpha);
      madd_v3_v3fl(vd.co, disp, clamp_f(fade, 0.0f, 1.0f));
      if (vd.mvert) {
        vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
      }
    }
    BKE_pbvh_vertex_iter_end;
  }
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

  BKE_pbvh_vertex_iter_begin(ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE)
  {
    if (sculpt_brush_test_sq_fn(&test, vd.co)) {
      const float fade =
          bstrength *
          SCULPT_brush_strength_factor(
              ss, brush, vd.co, sqrtf(test.dist), vd.no, vd.fno, 0.0f, vd.index, thread_id);
      SCULPT_surface_smooth_displace_step(
          ss, vd.co, ss->cache->surface_smooth_laplacian_disp, vd.index, beta, fade);
    }
  }
  BKE_pbvh_vertex_iter_end;
}

void SCULPT_do_surface_smooth_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
  Brush *brush = BKE_paint_brush(&sd->paint);
  SculptSession *ss = ob->sculpt;

  if (ss->cache->first_time && ss->cache->mirror_symmetry_pass == 0 &&
      ss->cache->radial_symmetry_pass == 0) {
    BLI_assert(ss->cache->surface_smooth_laplacian_disp == NULL);
    ss->cache->surface_smooth_laplacian_disp = MEM_callocN(
        SCULPT_vertex_count_get(ss) * 3 * sizeof(float), "HC smooth laplacian b");
  }

  /* Threaded loop over nodes. */
  SculptThreadedTaskData data = {
      .sd = sd,
      .ob = ob,
      .brush = brush,
      .nodes = nodes,
  };

  TaskParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, (sd->flags & SCULPT_USE_OPENMP), totnode);
  for (int i = 0; i < brush->surface_smooth_iterations; i++) {
    BLI_task_parallel_range(
        0, totnode, &data, SCULPT_do_surface_smooth_brush_laplacian_task_cb_ex, &settings);
    BLI_task_parallel_range(
        0, totnode, &data, SCULPT_do_surface_smooth_brush_displace_task_cb_ex, &settings);
  }
}
