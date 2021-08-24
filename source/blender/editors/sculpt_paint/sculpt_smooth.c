
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

#include "BLI_alloca.h"
#include "BLI_blenlib.h"
#include "BLI_compiler_attrs.h"
#include "BLI_hash.h"
#include "BLI_math.h"
#include "BLI_task.h"

#include "DNA_brush_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "BKE_attribute.h"
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
#ifdef PROXY_ADVANCED
/* clang-format off */
#include "BKE_DerivedMesh.h"
#include "../../blenkernel/intern/pbvh_intern.h"
/* clang-format on */
#endif
#include <math.h>
#include <stdlib.h>

void SCULPT_neighbor_coords_average_interior(SculptSession *ss,
                                             float result[3],
                                             SculptVertRef vertex,
                                             float projection)
{
  float avg[3] = {0.0f, 0.0f, 0.0f};
  float total = 0.0f;
  int neighbor_count = 0;
  bool check_fsets = ss->cache->brush->flag2 & BRUSH_SMOOTH_PRESERVE_FACE_SETS;

  const bool is_boundary = SCULPT_vertex_is_boundary(ss, vertex, check_fsets);

  const float *co = SCULPT_vertex_co_get(ss, vertex);
  float no[3];

  if (projection > 0.0f) {
    SCULPT_vertex_normal_get(ss, vertex, no);
  }

  const bool weighted = (ss->cache->brush->flag2 & BRUSH_SMOOTH_USE_AREA_WEIGHT) && !is_boundary;
  float *areas;

  if (weighted) {
    int val = SCULPT_vertex_valence_get(ss, vertex);
    areas = BLI_array_alloca(areas, val);

    BKE_pbvh_get_vert_face_areas(ss->pbvh, vertex, areas, val);
  }

  SculptVertexNeighborIter ni;
  SCULPT_VERTEX_NEIGHBORS_ITER_BEGIN (ss, vertex, ni) {
    neighbor_count++;

    float tmp[3], w;
    bool ok = false;

    if (weighted) {
      w = areas[ni.i];
    }
    else {
      w = 1.0f;
    }

    if (is_boundary) {
      /* Boundary vertices use only other boundary vertices. */
      if (SCULPT_vertex_is_boundary(ss, ni.vertex, check_fsets)) {
        copy_v3_v3(tmp, SCULPT_vertex_co_get(ss, ni.vertex));
        ok = true;
      }
    }
    else {
      /* Interior vertices use all neighbors. */
      copy_v3_v3(tmp, SCULPT_vertex_co_get(ss, ni.vertex));
      ok = true;
    }

    if (!ok) {
      continue;
    }

    if (projection > 0.0f) {
      sub_v3_v3(tmp, co);
      float fac = dot_v3v3(tmp, no);
      madd_v3_v3fl(tmp, no, -fac * projection);
      madd_v3_v3fl(avg, tmp, w);
    }
    else {
      madd_v3_v3fl(avg, tmp, w);
    }

    total += w;
  }
  SCULPT_VERTEX_NEIGHBORS_ITER_END(ni);

  /* Do not modify corner vertices. */
  if (neighbor_count <= 2 && is_boundary) {
    copy_v3_v3(result, SCULPT_vertex_co_get(ss, vertex));
    return;
  }

  /* Avoid division by 0 when there are no neighbors. */
  if (total == 0.0f) {
    copy_v3_v3(result, SCULPT_vertex_co_get(ss, vertex));
    return;
  }

  mul_v3_v3fl(result, avg, 1.0f / total);

  if (projection > 0.0f) {
    add_v3_v3(result, co);
  }
}

void SCULPT_neighbor_coords_average_interior_velocity(SculptSession *ss,
                                                      float result[3],
                                                      SculptVertRef vertex,
                                                      float projection,
                                                      SculptCustomLayer *scl)
{
  float avg[3] = {0.0f, 0.0f, 0.0f};
  int total = 0;
  int neighbor_count = 0;
  bool check_fsets = ss->cache->brush->flag2 & BRUSH_SMOOTH_PRESERVE_FACE_SETS;
  const bool is_boundary = SCULPT_vertex_is_boundary(ss, vertex, check_fsets);
  const float *co = SCULPT_vertex_co_get(ss, vertex);
  float no[3];

  if (projection > 0.0f) {
    SCULPT_vertex_normal_get(ss, vertex, no);
  }

  float vel[3];

  copy_v3_v3(vel, (float *)SCULPT_temp_cdata_get(vertex, scl));
  mul_v3_fl(vel, 0.4f);

  SculptVertexNeighborIter ni;
  SCULPT_VERTEX_NEIGHBORS_ITER_BEGIN (ss, vertex, ni) {
    neighbor_count++;

    float tmp[3];
    bool ok = false;

    float *vel2 = SCULPT_temp_cdata_get(ni.vertex, scl);

    // propegate smooth velocities a bit
    madd_v3_v3fl(vel2, vel, 1.0f / (float)ni.size);

    if (is_boundary) {
      /* Boundary vertices use only other boundary vertices. */
      if (SCULPT_vertex_is_boundary(ss, ni.vertex, check_fsets)) {
        copy_v3_v3(tmp, SCULPT_vertex_co_get(ss, ni.vertex));
        ok = true;
      }
    }
    else {
      /* Interior vertices use all neighbors. */
      copy_v3_v3(tmp, SCULPT_vertex_co_get(ss, ni.vertex));
      ok = true;
    }

    if (!ok) {
      continue;
    }

    if (projection > 0.0f) {
      sub_v3_v3(tmp, co);
      float fac = dot_v3v3(tmp, no);
      madd_v3_v3fl(tmp, no, -fac * projection);
      add_v3_v3(avg, tmp);
    }
    else {
      add_v3_v3(avg, tmp);
    }

    total++;
  }
  SCULPT_VERTEX_NEIGHBORS_ITER_END(ni);

  /* Do not modify corner vertices. */
  if (neighbor_count <= 2) {
    copy_v3_v3(result, SCULPT_vertex_co_get(ss, vertex));
    return;
  }

  /* Avoid division by 0 when there are no neighbors. */
  if (total == 0) {
    copy_v3_v3(result, SCULPT_vertex_co_get(ss, vertex));
    return;
  }

  mul_v3_v3fl(result, avg, 1.0f / total);

  if (projection > 0.0f) {
    add_v3_v3(result, co);
  }
}
/* For bmesh: Average surrounding verts based on an orthogonality measure.
 * Naturally converges to a quad-like structure. */
void SCULPT_bmesh_four_neighbor_average(
    float avg[3], float direction[3], BMVert *v, float projection, bool check_fsets)
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

    madd_v3_v3fl(vec, v->no, -dot_v3v3(vec, v->no) * projection);
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
    mul_v3_v3fl(vec, v->no, dot_v3v3(avg, v->no) * projection);
    sub_v3_v3(avg, vec);
    add_v3_v3(avg, v->co);
  }
  else {
    zero_v3(avg);
  }
}

static void sculpt_neighbor_coords_average_fset(SculptSession *ss,
                                                float result[3],
                                                SculptVertRef vertex,
                                                float projection)
{
  float avg[3] = {0.0f, 0.0f, 0.0f};
  float *co, no[3];
  float total = 0.0f;

  bool boundary = !SCULPT_vertex_has_unique_face_set(ss, vertex);

  if (projection > 0.0f) {
    co = (float *)SCULPT_vertex_co_get(ss, vertex);
    SCULPT_vertex_normal_get(ss, vertex, no);
  }

  const bool weighted = (ss->cache->brush->flag2 & BRUSH_SMOOTH_USE_AREA_WEIGHT) && !boundary;
  float *areas;

  if (weighted) {
    int val = SCULPT_vertex_valence_get(ss, vertex);
    areas = BLI_array_alloca(areas, val);

    BKE_pbvh_get_vert_face_areas(ss->pbvh, vertex, areas, val);
  }

  SculptVertexNeighborIter ni;
  SCULPT_VERTEX_NEIGHBORS_ITER_BEGIN (ss, vertex, ni) {
    const float *co2 = SCULPT_vertex_co_get(ss, ni.vertex);
    float w;

    if (weighted) {
      w = areas[ni.i];
    }
    else {
      w = 1.0f;
    }

    if (boundary && SCULPT_vertex_has_unique_face_set(ss, ni.vertex)) {
      continue;
    }

    if (projection > 0.0f) {
      float tmp[3];

      sub_v3_v3v3(tmp, co2, co);
      float fac = dot_v3v3(tmp, no);
      madd_v3_v3fl(tmp, no, -fac * projection);

      madd_v3_v3fl(avg, tmp, w);
    }
    else {
      madd_v3_v3fl(avg, co2, w);
    }
    total += w;
  }
  SCULPT_VERTEX_NEIGHBORS_ITER_END(ni);

  if (total > (boundary ? 1.0f : 0.0f)) {
    mul_v3_v3fl(result, avg, 1.0f / total);

    if (projection > 0.0) {
      add_v3_v3(result, co);
    }
  }
  else {
    copy_v3_v3(result, SCULPT_vertex_co_get(ss, vertex));
  }
}
/* Generic functions for laplacian smoothing. These functions do not take boundary vertices into
 * account. */

void SCULPT_neighbor_coords_average(
    SculptSession *ss, float result[3], SculptVertRef vertex, float projection, bool check_fsets)
{
  if (check_fsets) {
    sculpt_neighbor_coords_average_fset(ss, result, vertex, projection);
    return;
  }

  float avg[3] = {0.0f, 0.0f, 0.0f};
  float *co, no[3];
  float total = 0.0f;

  if (projection > 0.0f) {
    co = (float *)SCULPT_vertex_co_get(ss, vertex);
    SCULPT_vertex_normal_get(ss, vertex, no);
  }

  const bool weighted = ss->cache->brush->flag2 & BRUSH_SMOOTH_USE_AREA_WEIGHT;
  float *areas;

  if (weighted) {
    int val = SCULPT_vertex_valence_get(ss, vertex);
    areas = BLI_array_alloca(areas, val);

    BKE_pbvh_get_vert_face_areas(ss->pbvh, vertex, areas, val);
  }

  SculptVertexNeighborIter ni;
  SCULPT_VERTEX_NEIGHBORS_ITER_BEGIN (ss, vertex, ni) {
    const float *co2 = SCULPT_vertex_co_get(ss, ni.vertex);
    float w;

    if (weighted) {
      w = areas[ni.i];
    }
    else {
      w = 1.0f;
    }

    if (projection > 0.0f) {
      float tmp[3];

      sub_v3_v3v3(tmp, co2, co);
      float fac = dot_v3v3(tmp, no);
      madd_v3_v3fl(tmp, no, -fac * projection);

      madd_v3_v3fl(avg, tmp, w);
    }
    else {
      madd_v3_v3fl(avg, co2, w);
    }
    total += w;
  }
  SCULPT_VERTEX_NEIGHBORS_ITER_END(ni);

  if (total > 0.0f) {
    mul_v3_v3fl(result, avg, 1.0f / total);

    if (projection > 0.0) {
      add_v3_v3(result, co);
    }
  }
  else {
    copy_v3_v3(result, SCULPT_vertex_co_get(ss, vertex));
  }
}

float SCULPT_neighbor_mask_average(SculptSession *ss, SculptVertRef index)
{
  float avg = 0.0f;
  int total = 0;

  SculptVertexNeighborIter ni;
  SCULPT_VERTEX_NEIGHBORS_ITER_BEGIN (ss, index, ni) {
    avg += SCULPT_vertex_mask_get(ss, ni.vertex);
    total++;
  }
  SCULPT_VERTEX_NEIGHBORS_ITER_END(ni);

  if (total > 0) {
    return avg / total;
  }
  return SCULPT_vertex_mask_get(ss, index);
}

void SCULPT_neighbor_color_average(SculptSession *ss, float result[4], SculptVertRef index)
{
  float avg[4] = {0.0f, 0.0f, 0.0f, 0.0f};
  int total = 0;

  SculptVertexNeighborIter ni;
  SCULPT_VERTEX_NEIGHBORS_ITER_BEGIN (ss, index, ni) {
    add_v4_v4(avg, SCULPT_vertex_color_get(ss, ni.vertex));
    total++;
  }
  SCULPT_VERTEX_NEIGHBORS_ITER_END(ni);

  if (total > 0) {
    mul_v4_v4fl(result, avg, 1.0f / total);
  }
  else {
    copy_v4_v4(result, SCULPT_vertex_color_get(ss, index));
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
  BKE_pbvh_vertex_iter_begin (ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE) {
    if (!sculpt_brush_test_sq_fn(&test, vd.co)) {
      continue;
    }

    const float fade = bstrength * SCULPT_brush_strength_factor(ss,
                                                                brush,
                                                                vd.co,
                                                                sqrtf(test.dist),
                                                                vd.no,
                                                                vd.fno,
                                                                vd.mask ? *vd.mask : 0.0f,
                                                                vd.vertex,
                                                                thread_id);

    float disp[3];
    float *dir = SCULPT_temp_cdata_get(vd.vertex, data->scl);

    madd_v3_v3v3fl(disp, vd.co, dir, fade);
    SCULPT_clip(sd, ss, vd.co, disp);

    if (vd.mvert) {
      vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
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

  SculptCustomLayer scl;

  SCULPT_temp_customlayer_ensure(ss, ATTR_DOMAIN_POINT, CD_PROP_FLOAT3, "__dyntopo_detail_dir");
  SCULPT_temp_customlayer_get(ss, ATTR_DOMAIN_POINT, CD_PROP_FLOAT3, "__dyntopo_detail_dir", &scl);

  if (SCULPT_stroke_is_first_brush_step(ss->cache) &&
      (ss->cache->brush->flag2 & BRUSH_SMOOTH_USE_AREA_WEIGHT)) {
    BKE_pbvh_update_all_tri_areas(ss->pbvh);
  }

  SCULPT_vertex_random_access_ensure(ss);
  SCULPT_boundary_info_ensure(ob);

  if (SCULPT_stroke_is_first_brush_step(ss->cache)) {
    const int totvert = SCULPT_vertex_count_get(ss);

    for (int i = 0; i < totvert; i++) {
      float avg[3];
      SculptVertRef vertex = BKE_pbvh_table_index_to_vertex(ss->pbvh, i);
      float *dir = SCULPT_temp_cdata_get(vertex, &scl);

      SCULPT_neighbor_coords_average(ss, avg, vertex, 0.0f, false);
      sub_v3_v3v3(dir, avg, SCULPT_vertex_co_get(ss, vertex));
    }
  }

  SculptThreadedTaskData data = {.sd = sd, .ob = ob, .brush = brush, .nodes = nodes, .scl = &scl};

  TaskParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, true, totnode);
  BLI_task_parallel_range(0, totnode, &data, do_enhance_details_brush_task_cb_ex, &settings);
}

#ifdef PROXY_ADVANCED
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

  PBVHNode **nodes = data->nodes;
  ProxyVertArray *p = &nodes[n]->proxyverts;

  for (int i = 0; i < p->size; i++) {
    float co[3] = {0.0f, 0.0f, 0.0f};
    int ni = 0;

#  if 1
    if (sculpt_brush_test_sq_fn(&test, p->co[i])) {
      const float fade = bstrength * SCULPT_brush_strength_factor(
                                         ss,
                                         brush,
                                         p->co[i],
                                         sqrtf(test.dist),
                                         p->no[i],
                                         p->fno[i],
                                         smooth_mask ? 0.0f : (p->mask ? p->mask[i] : 0.0f),
                                         p->index[i],
                                         thread_id);
#  else
    if (1) {
      const float fade = 1.0;
#  endif

      while (ni < MAX_PROXY_NEIGHBORS && p->neighbors[i][ni].node >= 0) {
        ProxyKey *key = p->neighbors[i] + ni;
        PBVHNode *n2 = ss->pbvh->nodes + key->node;

        // printf("%d %d %d %p\n", key->node, key->pindex, ss->pbvh->totnode, n2);

        if (key->pindex < 0 || key->pindex >= n2->proxyverts.size) {
          printf("corruption!\n");
          fflush(stdout);
          ni++;
          continue;
        }

        if (n2->proxyverts.co) {
          add_v3_v3(co, n2->proxyverts.co[key->pindex]);
          ni++;
        }
      }

      // printf("ni %d\n", ni);

      if (ni > 2) {
        mul_v3_fl(co, 1.0f / (float)ni);
      }
      else {
        copy_v3_v3(co, p->co[i]);
      }

      // printf("%f %f %f   ", co[0], co[1], co[2]);

      interp_v3_v3v3(p->co[i], p->co[i], co, fade);
    }
  }
}

#else

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
  float projection = data->smooth_projection;

  PBVHVertexIter vd;

  CLAMP(bstrength, 0.0f, 1.0f);

  SculptBrushTest test;
  SculptBrushTestFn sculpt_brush_test_sq_fn = SCULPT_brush_test_init_with_falloff_shape(
      ss, &test, data->brush->falloff_shape);

  const int thread_id = BLI_task_parallel_thread_id(tls);
  const bool weighted = ss->cache->brush->flag2 & BRUSH_SMOOTH_USE_AREA_WEIGHT;
  const bool check_fsets = ss->cache->brush->flag2 & BRUSH_SMOOTH_PRESERVE_FACE_SETS;

  if (weighted) {
    BKE_pbvh_check_tri_areas(ss->pbvh, data->nodes[n]);
  }

  bool modified = false;

  BKE_pbvh_vertex_iter_begin (ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE) {
    if (!sculpt_brush_test_sq_fn(&test, vd.co)) {
      continue;
    }
    const float fade = bstrength * SCULPT_brush_strength_factor(
                                       ss,
                                       brush,
                                       vd.co,
                                       sqrtf(test.dist),
                                       vd.no,
                                       vd.fno,
                                       smooth_mask ? 0.0f : (vd.mask ? *vd.mask : 0.0f),
                                       vd.vertex,
                                       thread_id);
    if (smooth_mask) {
      float val = SCULPT_neighbor_mask_average(ss, vd.vertex) - *vd.mask;
      val *= fade * bstrength;
      *vd.mask += val;
      CLAMP(*vd.mask, 0.0f, 1.0f);
    }
    else {
      float avg[3], val[3];

      if (SCULPT_vertex_is_corner(ss, vd.vertex, check_fsets)) {
        continue;
      }

      SCULPT_neighbor_coords_average_interior(ss, avg, vd.vertex, projection);
      sub_v3_v3v3(val, avg, vd.co);
      madd_v3_v3v3fl(val, vd.co, val, fade);
      SCULPT_clip(sd, ss, vd.co, val);
    }
    if (vd.mvert) {
      vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
    }

    modified = true;
  }
  BKE_pbvh_vertex_iter_end;

  if (modified && weighted) {
    BKE_pbvh_node_mark_update_tri_area(data->nodes[n]);
  }
}
#endif

static void do_smooth_brush_task_cb_ex_scl(void *__restrict userdata,
                                           const int n,
                                           const TaskParallelTLS *__restrict tls)
{
  SculptThreadedTaskData *data = userdata;
  SculptSession *ss = data->ob->sculpt;
  Sculpt *sd = data->sd;
  const Brush *brush = data->brush;
  float bstrength = data->strength;
  float projection = data->smooth_projection;

  SculptCustomLayer *scl = data->scl;

  PBVHVertexIter vd;

  CLAMP(bstrength, 0.0f, 1.0f);

  SculptBrushTest test;
  SculptBrushTestFn sculpt_brush_test_sq_fn = SCULPT_brush_test_init_with_falloff_shape(
      ss, &test, data->brush->falloff_shape);

  const int thread_id = BLI_task_parallel_thread_id(tls);

  BKE_pbvh_vertex_iter_begin (ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE) {
    if (!sculpt_brush_test_sq_fn(&test, vd.co)) {
      continue;
    }
    const float fade = bstrength * SCULPT_brush_strength_factor(ss,
                                                                brush,
                                                                vd.co,
                                                                sqrtf(test.dist),
                                                                vd.no,
                                                                vd.fno,
                                                                (vd.mask ? *vd.mask : 0.0f),
                                                                vd.vertex,
                                                                thread_id);

    float avg[3], val[3];

    SCULPT_neighbor_coords_average_interior_velocity(ss, avg, vd.vertex, projection, scl);

    sub_v3_v3v3(val, avg, vd.co);

    float *vel = (float *)SCULPT_temp_cdata_get(vd.vertex, scl);
    interp_v3_v3v3(vel, vel, val, 0.5);

    madd_v3_v3v3fl(val, vd.co, vel, fade);

    SCULPT_clip(sd, ss, vd.co, val);

    if (vd.mvert) {
      vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
    }
  }
  BKE_pbvh_vertex_iter_end;
}

void SCULPT_smooth(Sculpt *sd,
                   Object *ob,
                   PBVHNode **nodes,
                   const int totnode,
                   float bstrength,
                   const bool smooth_mask,
                   float projection)
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

  SculptCustomLayer scl;
#if 0
  bool have_scl = smooth_mask ? false :
                                SCULPT_temp_customlayer_ensure(
                                    ss, ATTR_DOMAIN_POINT, CD_PROP_FLOAT3, "__scl_smooth_vel");
  if (have_scl) {
    SCULPT_temp_customlayer_get(ss, ATTR_DOMAIN_POINT, CD_PROP_FLOAT3, "__scl_smooth_vel", &scl);
  }
#else
  bool have_scl = false;
#endif

  if (type == PBVH_FACES && !ss->pmap) {
    BLI_assert_msg(0, "sculpt smooth: pmap missing");
    return;
  }

  SCULPT_vertex_random_access_ensure(ss);
  SCULPT_boundary_info_ensure(ob);

#ifdef PROXY_ADVANCED
  int datamask = PV_CO | PV_NEIGHBORS | PV_NO | PV_INDEX | PV_MASK;
  BKE_pbvh_ensure_proxyarrays(ss, ss->pbvh, nodes, totnode, datamask);

  BKE_pbvh_load_proxyarrays(ss->pbvh, nodes, totnode, PV_CO | PV_NO | PV_MASK);
#endif
  for (iteration = 0; iteration <= count; iteration++) {
    const float strength = (iteration != count) ? 1.0f : last;

    SculptThreadedTaskData data = {.sd = sd,
                                   .ob = ob,
                                   .brush = brush,
                                   .nodes = nodes,
                                   .smooth_mask = smooth_mask,
                                   .strength = strength,
                                   .smooth_projection = projection,
                                   .scl = have_scl ? &scl : NULL};

    TaskParallelSettings settings;
    BKE_pbvh_parallel_range_settings(&settings, true, totnode);
    if (have_scl) {
      BLI_task_parallel_range(0, totnode, &data, do_smooth_brush_task_cb_ex_scl, &settings);
    }
    else {
      BLI_task_parallel_range(0, totnode, &data, do_smooth_brush_task_cb_ex, &settings);
    }

#ifdef PROXY_ADVANCED
    BKE_pbvh_gather_proxyarray(ss->pbvh, nodes, totnode);
#endif
  }
}

void SCULPT_do_smooth_brush(
    Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode, float projection)
{
  SculptSession *ss = ob->sculpt;

  if (SCULPT_stroke_is_first_brush_step(ss->cache) &&
      (ss->cache->brush->flag2 & BRUSH_SMOOTH_USE_AREA_WEIGHT)) {
    BKE_pbvh_update_all_tri_areas(ss->pbvh);
  }

  if (ss->cache->bstrength <= 0.0f) {
    /* Invert mode, intensify details. */
    SCULPT_enhance_details_brush(sd, ob, nodes, totnode);
  }
  else {
    /* Regular mode, smooth. */
    SCULPT_smooth(sd, ob, nodes, totnode, ss->cache->bstrength, false, projection);
  }
}

/* HC Smooth Algorithm. */
/* From: Improved Laplacian Smoothing of Noisy Surface Meshes */

void SCULPT_surface_smooth_laplacian_step(SculptSession *ss,
                                          float *disp,
                                          const float co[3],
                                          SculptCustomLayer *scl,
                                          const SculptVertRef v_index,
                                          const float origco[3],
                                          const float alpha,
                                          const float projection,
                                          bool check_fsets)
{
  float laplacian_smooth_co[3];
  float weigthed_o[3], weigthed_q[3], d[3];
  SCULPT_neighbor_coords_average(ss, laplacian_smooth_co, v_index, projection, check_fsets);

  int index = BKE_pbvh_vertex_index_to_table(ss->pbvh, v_index);

  mul_v3_v3fl(weigthed_o, origco, alpha);
  mul_v3_v3fl(weigthed_q, co, 1.0f - alpha);
  add_v3_v3v3(d, weigthed_o, weigthed_q);
  sub_v3_v3v3((float *)SCULPT_temp_cdata_get(v_index, scl), laplacian_smooth_co, d);

  sub_v3_v3v3(disp, laplacian_smooth_co, co);
}

void SCULPT_surface_smooth_displace_step(SculptSession *ss,
                                         float *co,
                                         SculptCustomLayer *scl,
                                         const SculptVertRef v_index,
                                         const float beta,
                                         const float fade)
{
  float b_avg[3] = {0.0f, 0.0f, 0.0f};
  float b_current_vertex[3];
  int total = 0;
  int index = BKE_pbvh_vertex_index_to_table(ss->pbvh, v_index);

  SculptVertexNeighborIter ni;
  SCULPT_VERTEX_NEIGHBORS_ITER_BEGIN (ss, v_index, ni) {
    add_v3_v3(b_avg, (float *)SCULPT_temp_cdata_get(ni.vertex, scl));
    total++;
  }
  SCULPT_VERTEX_NEIGHBORS_ITER_END(ni);

  if (total > 0) {
    mul_v3_v3fl(b_current_vertex, b_avg, (1.0f - beta) / total);
    madd_v3_v3fl(b_current_vertex, (float *)SCULPT_temp_cdata_get(v_index, scl), beta);
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

  const bool weighted = ss->cache->brush->flag2 & BRUSH_SMOOTH_USE_AREA_WEIGHT;

  if (weighted) {
    BKE_pbvh_check_tri_areas(ss->pbvh, data->nodes[n]);
  }

  bool modified = false;

  bool check_fsets = ss->cache->brush->flag2 & BRUSH_SMOOTH_PRESERVE_FACE_SETS;
  SCULPT_orig_vert_data_init(&orig_data, data->ob, data->nodes[n], SCULPT_UNDO_COORDS);

  BKE_pbvh_vertex_iter_begin (ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE) {
    SCULPT_orig_vert_data_update(&orig_data, &vd);

    if (!sculpt_brush_test_sq_fn(&test, vd.co)) {
      continue;
    }
    const float fade = bstrength * SCULPT_brush_strength_factor(ss,
                                                                brush,
                                                                vd.co,
                                                                sqrtf(test.dist),
                                                                vd.no,
                                                                vd.fno,
                                                                vd.mask ? *vd.mask : 0.0f,
                                                                vd.vertex,
                                                                thread_id);

    float disp[3];
    SCULPT_surface_smooth_laplacian_step(ss,
                                         disp,
                                         vd.co,
                                         data->scl,
                                         vd.vertex,
                                         orig_data.co,
                                         alpha,
                                         data->smooth_projection,
                                         check_fsets);
    madd_v3_v3fl(vd.co, disp, clamp_f(fade, 0.0f, 1.0f));
    if (vd.mvert) {
      vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
    }

    modified = true;
  }
  BKE_pbvh_vertex_iter_end;

  if (modified && weighted) {
    BKE_pbvh_node_mark_update_tri_area(data->nodes[n]);
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

  BKE_pbvh_vertex_iter_begin (ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE) {
    if (!sculpt_brush_test_sq_fn(&test, vd.co)) {
      continue;
    }
    const float fade = bstrength * SCULPT_brush_strength_factor(ss,
                                                                brush,
                                                                vd.co,
                                                                sqrtf(test.dist),
                                                                vd.no,
                                                                vd.fno,
                                                                vd.mask ? *vd.mask : 0.0f,
                                                                vd.vertex,
                                                                thread_id);
    SCULPT_surface_smooth_displace_step(ss, vd.co, data->scl, vd.vertex, beta, fade);
  }
  BKE_pbvh_vertex_iter_end;
}

void SCULPT_do_surface_smooth_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
  Brush *brush = BKE_paint_brush(&sd->paint);
  SculptSession *ss = ob->sculpt;

  SculptCustomLayer scl;

  SCULPT_temp_customlayer_ensure(ss, ATTR_DOMAIN_POINT, CD_PROP_FLOAT3, "__dyntopo_lapsmooth");
  SCULPT_temp_customlayer_get(ss, ATTR_DOMAIN_POINT, CD_PROP_FLOAT3, "__dyntopo_lapsmooth", &scl);

  if (SCULPT_stroke_is_first_brush_step(ss->cache) &&
      (ss->cache->brush->flag2 & BRUSH_SMOOTH_USE_AREA_WEIGHT)) {
    BKE_pbvh_update_all_tri_areas(ss->pbvh);
  }

  if (SCULPT_stroke_is_first_brush_step(ss->cache)) {
    // BLI_assert(ss->cache->surface_smooth_laplacian_disp == NULL);
    // ss->cache->surface_smooth_laplacian_disp = MEM_callocN(
    //    sizeof(float[3]) * SCULPT_vertex_count_get(ss), "HC smooth laplacian b");
  }

  /* Threaded loop over nodes. */
  SculptThreadedTaskData data = {.sd = sd,
                                 .ob = ob,
                                 .brush = brush,
                                 .nodes = nodes,
                                 .smooth_projection = brush->autosmooth_projection,
                                 .scl = &scl};

  TaskParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, true, totnode);
  for (int i = 0; i < brush->surface_smooth_iterations; i++) {
    BLI_task_parallel_range(
        0, totnode, &data, SCULPT_do_surface_smooth_brush_laplacian_task_cb_ex, &settings);
    BLI_task_parallel_range(
        0, totnode, &data, SCULPT_do_surface_smooth_brush_displace_task_cb_ex, &settings);
  }
}

static void do_smooth_vcol_boundary_brush_task_cb_ex(void *__restrict userdata,
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

  float avg[4] = {0.0f, 0.0f, 0.0f, 0.0f};
  float tot = 0.0f;
  BKE_pbvh_vertex_iter_begin (ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE) {
    if (!vd.col) {
      continue;
    }

    if (sculpt_brush_test_sq_fn(&test, vd.co)) {
      const float fade = bstrength * SCULPT_brush_strength_factor(
                                         ss,
                                         brush,
                                         vd.co,
                                         sqrtf(test.dist),
                                         vd.no,
                                         vd.fno,
                                         smooth_mask ? 0.0f : (vd.mask ? *vd.mask : 0.0f),
                                         vd.vertex,
                                         thread_id);

      madd_v3_v3fl(avg, vd.col, fade);
      tot += fade;
    }
  }
  BKE_pbvh_vertex_iter_end;

  if (tot == 0.0f) {
    return;
  }
  tot = 1.0f / tot;

  mul_v3_fl(avg, tot);

  float exp = brush->vcol_boundary_exponent;
  // detect bad value

  if (exp == 0.0f) {
    exp = 1.0f;
  }

  BKE_pbvh_vertex_iter_begin (ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE) {
    if (sculpt_brush_test_sq_fn(&test, vd.co)) {
      const float fade = bstrength * SCULPT_brush_strength_factor(
                                         ss,
                                         brush,
                                         vd.co,
                                         sqrtf(test.dist),
                                         vd.no,
                                         vd.fno,
                                         smooth_mask ? 0.0f : (vd.mask ? *vd.mask : 0.0f),
                                         vd.vertex,
                                         thread_id);
      if (!vd.col) {
        continue;
      }

      float avg2[3], avg3[3], val[3];
      float tot2 = 0.0f, tot4 = 0.0f;

      copy_v4_v4(avg, vd.col);

      zero_v3(avg2);
      zero_v3(avg3);

      madd_v3_v3fl(avg2, vd.co, 0.5f);
      tot2 += 0.5f;

      SculptVertexNeighborIter ni;
      SCULPT_VERTEX_NEIGHBORS_ITER_BEGIN (ss, vd.vertex, ni) {
        const float *col = SCULPT_vertex_color_get(ss, ni.vertex);
        const float *co = SCULPT_vertex_co_get(ss, ni.vertex);

        // simple color metric.  TODO: plug in appropriate color space code?
        float dv[4];
        sub_v4_v4v4(dv, col, avg);
        float w = (fabs(dv[0]) + fabs(dv[1]) + fabs(dv[2]) + fabs(dv[3])) / 4.0;

        w = powf(w, exp);

        madd_v3_v3fl(avg3, co, 1.0f);
        tot4 += 1.0f;

        madd_v3_v3fl(avg2, co, w);
        tot2 += w;
      }
      SCULPT_VERTEX_NEIGHBORS_ITER_END(ni);

      if (tot2 == 0.0f) {
        continue;
      }

      if (tot4 > 0.0f) {
        mul_v3_fl(avg3, 1.0f / tot4);
      }

      /* try to avoid perfectly colinear triangles, and the normal discontinuities they create,
         by blending slightly with unweighted smoothed position */
      mul_v3_fl(avg2, 1.0f / tot2);
      interp_v3_v3v3(avg2, avg2, avg3, 0.025);

      sub_v3_v3v3(val, avg2, vd.co);
      madd_v3_v3v3fl(val, vd.co, val, fade);
      SCULPT_clip(sd, ss, vd.co, val);

      if (vd.mvert) {
        vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
      }
    }
  }
  BKE_pbvh_vertex_iter_end;
}

void SCULPT_smooth_vcol_boundary(
    Sculpt *sd, Object *ob, PBVHNode **nodes, const int totnode, float bstrength)
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

  SCULPT_vertex_random_access_ensure(ss);
  SCULPT_boundary_info_ensure(ob);

#ifdef PROXY_ADVANCED
  int datamask = PV_CO | PV_NEIGHBORS | PV_NO | PV_INDEX | PV_MASK;
  BKE_pbvh_ensure_proxyarrays(ss, ss->pbvh, nodes, totnode, datamask);

  BKE_pbvh_load_proxyarrays(ss->pbvh, nodes, totnode, PV_CO | PV_NO | PV_MASK);
#endif
  for (iteration = 0; iteration <= count; iteration++) {
    const float strength = (iteration != count) ? 1.0f : last;

    SculptThreadedTaskData data = {
        .sd = sd,
        .ob = ob,
        .brush = brush,
        .nodes = nodes,
        .smooth_mask = false,
        .strength = strength,
    };

    TaskParallelSettings settings;
    BKE_pbvh_parallel_range_settings(&settings, true, totnode);
    BLI_task_parallel_range(
        0, totnode, &data, do_smooth_vcol_boundary_brush_task_cb_ex, &settings);

#ifdef PROXY_ADVANCED
    BKE_pbvh_gather_proxyarray(ss->pbvh, nodes, totnode);
#endif
  }
}
