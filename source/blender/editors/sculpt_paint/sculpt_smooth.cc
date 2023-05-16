/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2020 Blender Foundation */

/** \file
 * \ingroup edsculpt
 */

#include "MEM_guardedalloc.h"

#include "BLI_alloca.h"
#include "BLI_math.h"
#include "BLI_math_vector_types.hh"
#include "BLI_task.h"
#include "BLI_vector.hh"

#include "DNA_brush_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "BKE_brush.h"
#include "BKE_context.h"
#include "BKE_mesh.hh"
#include "BKE_mesh_mapping.h"
#include "BKE_paint.h"
#include "BKE_pbvh.h"

#include "sculpt_intern.hh"

#include "bmesh.h"

#include <cmath>
#include <cstdlib>

using blender::float2;
using blender::float3;
using blender::Vector;

static void SCULPT_neighbor_coords_average_interior_ex(SculptSession *ss,
                                                       float result[3],
                                                       PBVHVertRef vertex,
                                                       float projection,
                                                       float hard_corner_pin,
                                                       bool weighted,
                                                       eSculptBoundary bound_type,
                                                       eSculptCorner corner_type)
{
  float3 avg(0.0f, 0.0f, 0.0f);
  int neighbor_count = 0;

  const eSculptBoundary is_boundary = SCULPT_vertex_is_boundary(ss, vertex, bound_type);
  const eSculptCorner is_corner = SCULPT_vertex_is_corner(ss, vertex, corner_type);

  float *areas = nullptr;
  float3 no, co;
  SCULPT_vertex_normal_get(ss, vertex, no);
  co = SCULPT_vertex_co_get(ss, vertex);

  if (weighted) {
    const int valence = SCULPT_vertex_valence_get(ss, vertex);
    areas = reinterpret_cast<float *>(BLI_array_alloca(areas, valence));
    BKE_pbvh_get_vert_face_areas(ss->pbvh, vertex, areas, valence);
  }

  float total = 0.0f;

  SculptVertexNeighborIter ni;
  SCULPT_VERTEX_NEIGHBORS_ITER_BEGIN (ss, vertex, ni) {
    neighbor_count++;
    float w = weighted ? areas[ni.i] : 1.0f;

    eSculptBoundary is_boundary2;

    if (ni.has_edge) {
      is_boundary2 = SCULPT_edge_is_boundary(ss, ni.edge, bound_type);
    }
    else {
      is_boundary2 = SCULPT_vertex_is_boundary(ss, ni.vertex, bound_type);
    }
    const eSculptBoundary smooth_types = !ss->hard_edge_mode ?
                                             SCULPT_BOUNDARY_FACE_SET | SCULPT_BOUNDARY_SEAM |
                                                 SCULPT_BOUNDARY_UV :
                                             SCULPT_BOUNDARY_UV;

    /* Boundary vertices use only other boundary vertices. */
    if (is_boundary) {
      /* Handle smooth boundaries. */
      if (bool(is_boundary2 & smooth_types) != bool(is_boundary & smooth_types)) {
        /* Project to plane. */
        float3 t1 = float3(SCULPT_vertex_co_get(ss, ni.vertex)) - co;

        avg += (co + no * dot_v3v3(t1, no)) * w;
        total += w;
      }
      else if (is_boundary & is_boundary2) {
        avg += float3(SCULPT_vertex_co_get(ss, ni.vertex)) * w;
        total += w;
      }
    }
    else {
      /* Interior vertices use all neighbors. */
      avg += float3(SCULPT_vertex_co_get(ss, ni.vertex)) * w;
      total += w;
    }
  }
  SCULPT_VERTEX_NEIGHBORS_ITER_END(ni);

  /* Avoid division by 0 when there are no neighbors. */
  if (total == 0) {
    copy_v3_v3(result, SCULPT_vertex_co_get(ss, vertex));
    return;
  }

  /* Project to plane if desired. */
  avg = avg / (float)total - co;
  float t = dot_v3v3(avg, no);

  avg += -no * t * projection + co;

  if (is_corner) {
    interp_v3_v3v3(result, co, avg, 1.0f - hard_corner_pin);
  }
  else {
    copy_v3_v3(result, avg);
  }
}

void SCULPT_neighbor_coords_average_interior(SculptSession *ss,
                                             float result[3],
                                             PBVHVertRef vertex,
                                             float projection,
                                             float hard_corner_pin,
                                             bool use_area_weights)
{
  eSculptBoundary bound_type = SCULPT_BOUNDARY_MESH | SCULPT_BOUNDARY_FACE_SET |
                               SCULPT_BOUNDARY_SEAM | SCULPT_BOUNDARY_SHARP;
  eSculptCorner corner_type = SCULPT_CORNER_MESH | SCULPT_CORNER_SHARP;

  if (ss->hard_edge_mode) {
    corner_type |= SCULPT_CORNER_FACE_SET | SCULPT_CORNER_SEAM;
  }

  SCULPT_neighbor_coords_average_interior_ex(
      ss, result, vertex, projection, hard_corner_pin, use_area_weights, bound_type, corner_type);
}

/* Compares four vectors seperated by 90 degrees around normal and picks the one closest
 * to perpindicular to dir. Used for building a cross field.
 */
int closest_vec_to_perp(float dir[3], float r_dir2[3], float no[3], float *buckets, float w)
{
  int bits = 0;

  if (dot_v3v3(r_dir2, dir) < 0.0f) {
    negate_v3(r_dir2);
    bits |= 1;
  }

  float dir4[3];
  cross_v3_v3v3(dir4, r_dir2, no);
  normalize_v3(dir4);

  if (dot_v3v3(dir4, dir) < 0.0f) {
    negate_v3(dir4);
    bits |= 2;
  }

  if (dot_v3v3(dir4, dir) > dot_v3v3(r_dir2, dir)) {
    copy_v3_v3(r_dir2, dir4);
    bits |= 4;
  }

  buckets[bits] += w;

  return bits;
}

void vec_transform(float r_dir2[3], float no[3], int bits)
{
  if (bits & 4) {
    float dir4[3];

    copy_v3_v3(dir4, r_dir2);

    if (bits & 2) {
      negate_v3(dir4);
    }

    float dir5[3];

    cross_v3_v3v3(dir5, no, dir4);
    normalize_v3(dir5);

    copy_v3_v3(r_dir2, dir5);
  }

  if (bits & 1) {
    negate_v3(r_dir2);
  }
}

void SCULPT_bmesh_four_neighbor_average(SculptSession *ss,
                                        float avg[3],
                                        float direction[3],
                                        BMVert *v,
                                        float projection,
                                        float hard_corner_pin,
                                        int cd_temp,
                                        bool weighted,
                                        bool do_origco)
{
  float avg_co[3] = {0.0f, 0.0f, 0.0f};
  float tot_co = 0.0f;

  float buckets[8] = {0};
  PBVHVertRef vertex = {(intptr_t)v};

  // zero_v3(direction);

  float *field = BM_ELEM_CD_PTR<float *>(v, cd_temp);
  float dir[3];
  float dir3[3] = {0.0f, 0.0f, 0.0f};

  float *areas;

  SCULPT_vertex_check_origdata(ss, BKE_pbvh_make_vref(intptr_t(v)));

  float *origco = BM_ELEM_CD_PTR<float *>(v, ss->attrs.orig_co->bmesh_cd_offset);
  float *origno = BM_ELEM_CD_PTR<float *>(v, ss->attrs.orig_no->bmesh_cd_offset);

  if (do_origco) {
    madd_v3_v3fl(direction, origno, -dot_v3v3(origno, direction));
    normalize_v3(direction);
  }

  float *co1 = do_origco ? origco : v->co;
  float *no1 = do_origco ? origno : v->no;

  int valence = SCULPT_vertex_valence_get(ss, vertex);

  if (weighted) {
    areas = (float *)BLI_array_alloca(areas, valence * 2);

    BKE_pbvh_get_vert_face_areas(ss->pbvh, vertex, areas, valence);
    float totarea = 0.0f;

    for (int i = 0; i < valence; i++) {
      totarea += areas[i];
    }

    totarea = totarea != 0.0f ? 1.0f / totarea : 0.0f;

    for (int i = 0; i < valence; i++) {
      areas[i] *= totarea;
    }
  }

  /* Build final direction from input direction and the cross field. */
  copy_v3_v3(dir, field);

  if (dot_v3v3(dir, dir) == 0.0f) {
    copy_v3_v3(dir, direction);
  }
  else {
    closest_vec_to_perp(dir, direction, no1, buckets, 1.0f);
  }

  float totdir3 = 0.0f;

  const float selfw = (float)valence * 0.0025f;
  madd_v3_v3fl(dir3, direction, selfw);

  totdir3 += selfw;

  BMIter eiter;
  BMEdge *e;
  bool had_bound = false;
  int area_i = 0;

  BM_ITER_ELEM_INDEX (e, &eiter, v, BM_EDGES_OF_VERT, area_i) {
    BMVert *v_other = (e->v1 == v) ? e->v2 : e->v1;
    PBVHVertRef vertex_other = {reinterpret_cast<intptr_t>(v_other)};

    float dir2[3];
    float *field2 = BM_ELEM_CD_PTR<float *>(v_other, cd_temp);

    float bucketw = 1.0f;

    float *co2;

    if (!do_origco ||
        blender::bke::sculpt::stroke_id_test_no_update(ss, vertex_other, STROKEID_USER_ORIGINAL))
    {
      co2 = v_other->co;
    }
    else {
      co2 = BM_ELEM_CD_PTR<float *>(v_other, ss->attrs.orig_co->bmesh_cd_offset);
    }

    eSculptBoundary bflag = SCULPT_BOUNDARY_FACE_SET | SCULPT_BOUNDARY_MESH |
                            SCULPT_BOUNDARY_SHARP | SCULPT_BOUNDARY_SEAM | SCULPT_BOUNDARY_UV;

    int bound = SCULPT_edge_is_boundary(ss, BKE_pbvh_make_eref(intptr_t(e)), bflag);
    float dirw = 1.0f;

    /* Add to cross field. */
    if (bound) {
      had_bound = true;

      sub_v3_v3v3(dir2, co2, co1);
      madd_v3_v3fl(dir2, no1, -dot_v3v3(no1, dir2));
      normalize_v3(dir2);
      dirw = 100000.0f;
    }
    else {
      dirw = field2[3];

      copy_v3_v3(dir2, field2);
      if (dot_v3v3(dir2, dir2) == 0.0f) {
        copy_v3_v3(dir2, dir);
      }
    }

    closest_vec_to_perp(dir, dir2, no1, buckets, bucketw);  // field2[3]);

    madd_v3_v3fl(dir3, dir2, dirw);
    totdir3 += dirw;

    if (had_bound) {
      tot_co = 0.0f;
      continue;
    }
    float vec[3];
    sub_v3_v3v3(vec, co2, co1);

    madd_v3_v3fl(vec, no1, -dot_v3v3(vec, no1) * projection);
    normalize_v3(vec);

    /* fac is a measure of how orthogonal or parallel the edge is
     * relative to the direction. */
    float fac = dot_v3v3(vec, dir);

    fac = fac * fac - 0.5f;
    fac *= fac;

    if (weighted) {
      fac *= areas[area_i];
    }

    madd_v3_v3fl(avg_co, co2, fac);
    tot_co += fac;
  }

  if (tot_co > 0.0f) {
    mul_v3_v3fl(avg, avg_co, 1.0f / tot_co);

    /* Preserve volume. */
    float vec[3];
    sub_v3_v3(avg, co1);
    mul_v3_v3fl(vec, no1, dot_v3v3(avg, no1) * projection);
    sub_v3_v3(avg, vec);
    add_v3_v3(avg, co1);
  }
  else {
    // zero_v3(avg);
    copy_v3_v3(avg, co1);
  }

  eSculptCorner corner_type = SCULPT_CORNER_MESH | SCULPT_CORNER_SHARP;
  if (ss->hard_edge_mode) {
    corner_type |= SCULPT_CORNER_FACE_SET;
  }

  if (SCULPT_vertex_is_corner(ss, vertex, corner_type)) {
    interp_v3_v3v3(avg, avg, SCULPT_vertex_co_get(ss, vertex), hard_corner_pin);
  }

  PBVH_CHECK_NAN(avg);

  // do not update in do_origco
  if (do_origco) {
    return;
  }

  if (totdir3 > 0.0f) {
    float outdir = totdir3 / (float)valence;

    // mul_v3_fl(dir3, 1.0 / totdir3);
    normalize_v3(dir3);
    if (had_bound) {
      copy_v3_v3(field, dir3);
      field[3] = 1000.0f;
    }
    else {

      mul_v3_fl(field, field[3]);
      madd_v3_v3fl(field, dir3, outdir);

      field[3] = (field[3] + outdir) * 0.4;
      normalize_v3(field);
    }

    float maxb = 0.0f;
    int bi = 0;
    for (int i = 0; i < 8; i++) {
      if (buckets[i] > maxb) {
        maxb = buckets[i];
        bi = i;
      }
    }

    vec_transform(field, no1, bi);
  }
}

/* Generic functions for laplacian smoothing. These functions do not take boundary vertices into
 * account. */

void SCULPT_neighbor_coords_average(SculptSession *ss,
                                    float result[3],
                                    PBVHVertRef vertex,
                                    float projection,
                                    float hard_corner_pin,
                                    bool weighted)
{
  eSculptCorner corner_type = SCULPT_CORNER_SHARP | SCULPT_CORNER_FACE_SET;
  eSculptBoundary bound_type = SCULPT_BOUNDARY_SHARP | SCULPT_BOUNDARY_SEAM | SCULPT_BOUNDARY_UV |
                               SCULPT_BOUNDARY_FACE_SET;

  SCULPT_neighbor_coords_average_interior_ex(
      ss, result, vertex, projection, hard_corner_pin, weighted, bound_type, corner_type);
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
    float *detail_dir = blender::bke::paint::vertex_attr_ptr<float>(vd.vertex,
                                                                    ss->attrs.detail_directions);
    madd_v3_v3v3fl(disp, vd.co, detail_dir, fade);
    SCULPT_clip(sd, ss, vd.co, disp);

    if (vd.is_mesh) {
      BKE_pbvh_vert_tag_update_normal(ss->pbvh, vd.vertex);
    }
  }
  BKE_pbvh_vertex_iter_end;
}

static void SCULPT_enhance_details_brush(Sculpt *sd, Object *ob, Span<PBVHNode *> nodes)
{
  SculptSession *ss = ob->sculpt;
  Brush *brush = BKE_paint_brush(&sd->paint);

  SCULPT_smooth_undo_push(ob, nodes);

  SCULPT_vertex_random_access_ensure(ss);
  SCULPT_boundary_info_ensure(ob);

  float projection = brush->autosmooth_projection;
  bool use_area_weights = brush->flag2 & BRUSH_SMOOTH_USE_AREA_WEIGHT;
  float hard_corner_pin = BKE_brush_hard_corner_pin_get(ss->scene, brush);

  if (SCULPT_stroke_is_first_brush_step(ss->cache)) {
    const int totvert = SCULPT_vertex_count_get(ss);

    if (!ss->attrs.detail_directions) {
      SculptAttributeParams params = {};
      params.stroke_only = true;

      ss->attrs.detail_directions = BKE_sculpt_attribute_ensure(
          ob, ATTR_DOMAIN_POINT, CD_PROP_FLOAT3, SCULPT_ATTRIBUTE_NAME(laplacian_disp), &params);
    }

    for (int i = 0; i < totvert; i++) {
      PBVHVertRef vertex = BKE_pbvh_index_to_vertex(ss->pbvh, i);

      float avg[3];
      SCULPT_neighbor_coords_average(
          ss, avg, vertex, projection, hard_corner_pin, use_area_weights);
      float *detail_dir = blender::bke::paint::vertex_attr_ptr<float>(vertex,
                                                                      ss->attrs.detail_directions);

      sub_v3_v3v3(detail_dir, avg, SCULPT_vertex_co_get(ss, vertex));
    }
  }

  SculptThreadedTaskData data{};
  data.sd = sd;
  data.ob = ob;
  data.brush = brush;
  data.nodes = nodes;

  TaskParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, true, nodes.size());
  BLI_task_parallel_range(0, nodes.size(), &data, do_enhance_details_brush_task_cb_ex, &settings);
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
  const bool do_reproject = SCULPT_need_reproject(ss);

  CLAMP(bstrength, 0.0f, 1.0f);

  SculptBrushTest test;
  SculptBrushTestFn sculpt_brush_test_sq_fn = SCULPT_brush_test_init_with_falloff_shape(
      ss, &test, data->brush->falloff_shape);

  if (brush->flag2 & BRUSH_SMOOTH_USE_AREA_WEIGHT) {
    BKE_pbvh_check_tri_areas(ss->pbvh, data->nodes[n]);
  }

  const int thread_id = BLI_task_parallel_thread_id(tls);
  AutomaskingNodeData automask_data;
  SCULPT_automasking_node_begin(
      data->ob, ss, ss->cache->automasking, &automask_data, data->nodes[n]);

  float projection = brush->autosmooth_projection;
  bool weighted = brush->flag2 & BRUSH_SMOOTH_USE_AREA_WEIGHT;
  bool modified = false;

  float hard_corner_pin = BKE_brush_hard_corner_pin_get(ss->scene, brush);

  BKE_pbvh_vertex_iter_begin (ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE) {
    if (!sculpt_brush_test_sq_fn(&test, vd.co)) {
      continue;
    }

    modified = true;

    SCULPT_automasking_node_update(ss, &automask_data, &vd);

    float fade = bstrength *
                 SCULPT_brush_strength_factor(ss,
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
      float oldco[3];
      float oldno[3];
      copy_v3_v3(oldco, vd.co);
      SCULPT_vertex_normal_get(ss, vd.vertex, oldno);

      float avg[3], val[3];
      SCULPT_neighbor_coords_average_interior(
          ss, avg, vd.vertex, projection, hard_corner_pin, weighted);
      sub_v3_v3v3(val, avg, vd.co);
      madd_v3_v3v3fl(val, vd.co, val, fade);
      SCULPT_clip(sd, ss, vd.co, val);

      if (do_reproject) {
        BKE_sculpt_reproject_cdata(ss, vd.vertex, oldco, oldno);
      }

      if (vd.is_mesh) {
        BKE_pbvh_vert_tag_update_normal(ss->pbvh, vd.vertex);
      }
    }
  }
  BKE_pbvh_vertex_iter_end;

  if (modified) {
    BKE_pbvh_node_mark_update(data->nodes[n]);
  }
}

void SCULPT_smooth_undo_push(Object *ob, Span<PBVHNode *> nodes)
{
  SculptSession *ss = ob->sculpt;

  if (BKE_pbvh_type(ss->pbvh) == PBVH_BMESH && SCULPT_need_reproject(ss)) {
    BM_log_entry_add_ex(ss->bm, ss->bm_log, true);

    for (PBVHNode *node : nodes) {
      PBVHVertexIter vd;
      PBVHFaceIter fd;
      BKE_pbvh_face_iter_begin (ss->pbvh, node, fd) {
        BM_log_face_modified(ss->bm, ss->bm_log, reinterpret_cast<BMFace *>(fd.face.i));
      }
      BKE_pbvh_face_iter_end(fd);
    }
  }
}

void SCULPT_smooth(
    Sculpt *sd, Object *ob, Span<PBVHNode *> nodes, float bstrength, const bool smooth_mask)
{
  SculptSession *ss = ob->sculpt;
  Brush *brush = BKE_paint_brush(&sd->paint);

  const int max_iterations = 4;
  const float fract = 1.0f / max_iterations;
  PBVHType type = BKE_pbvh_type(ss->pbvh);
  int iteration, count;
  float last;

  SCULPT_smooth_undo_push(ob, nodes);

  /* PBVH_FACES needs ss->epmap. */
  if (BKE_pbvh_type(ss->pbvh) == PBVH_FACES && !ss->epmap) {
    Mesh *mesh = static_cast<Mesh *>(ob->data);

    BKE_mesh_edge_poly_map_create(&ss->epmap,
                                  &ss->epmap_mem,
                                  mesh->edges().size(),
                                  mesh->polys(),
                                  mesh->corner_edges().data(),
                                  mesh->corner_edges().size());
  }

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

    if (brush->flag2 & BRUSH_SMOOTH_USE_AREA_WEIGHT) {
      BKE_pbvh_face_areas_begin(ss->pbvh);
    }

    SculptThreadedTaskData data{};
    data.sd = sd;
    data.ob = ob;
    data.brush = brush;
    data.nodes = nodes;
    data.smooth_mask = smooth_mask;
    data.strength = strength;

    TaskParallelSettings settings;
    BKE_pbvh_parallel_range_settings(&settings, true, nodes.size());
    BLI_task_parallel_range(0, nodes.size(), &data, do_smooth_brush_task_cb_ex, &settings);
  }
}

void SCULPT_do_smooth_brush(Sculpt *sd, Object *ob, Span<PBVHNode *> nodes)
{
  SculptSession *ss = ob->sculpt;

  /* NOTE: The enhance brush needs to initialize its state on the first brush step. The stroke
   * strength can become 0 during the stroke, but it can not change sign (the sign is determined
   * in the beginning of the stroke. So here it is important to not switch to enhance brush in the
   * middle of the stroke. */
  if (ss->cache->bstrength < 0.0f) {
    /* Invert mode, intensify details. */
    SCULPT_enhance_details_brush(sd, ob, nodes);
  }
  else {
    /* Regular mode, smooth. */
    SCULPT_smooth(sd, ob, nodes, ss->cache->bstrength, false);
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
                                          const float alpha,
                                          bool use_area_weights)
{
  float laplacian_smooth_co[3];
  float weigthed_o[3], weigthed_q[3], d[3];
  int v_index = BKE_pbvh_vertex_to_index(ss->pbvh, vertex);

  SCULPT_neighbor_coords_average(ss, laplacian_smooth_co, vertex, 0.0f, 0.0f, use_area_weights);

  mul_v3_v3fl(weigthed_o, origco, alpha);
  mul_v3_v3fl(weigthed_q, co, 1.0f - alpha);
  add_v3_v3v3(d, weigthed_o, weigthed_q);
  float *laplacian_disp = blender::bke::paint::vertex_attr_ptr<float>(vertex,
                                                                      ss->attrs.laplacian_disp);

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
    float *laplacian_disp = blender::bke::paint::vertex_attr_ptr<float>(ni.vertex,
                                                                        ss->attrs.laplacian_disp);
    add_v3_v3(b_avg, laplacian_disp);
    total++;
  }

  SCULPT_VERTEX_NEIGHBORS_ITER_END(ni);
  if (total > 0) {
    float *laplacian_disp = blender::bke::paint::vertex_attr_ptr<float>(vertex,
                                                                        ss->attrs.laplacian_disp);

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

  bool weighted = brush->flag2 & BRUSH_SMOOTH_USE_AREA_WEIGHT;

  BKE_pbvh_vertex_iter_begin (ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE) {
    SCULPT_orig_vert_data_update(ss, &orig_data, vd.vertex);
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
        ss, disp, vd.co, vd.vertex, orig_data.co, alpha, weighted);
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

void SCULPT_do_surface_smooth_brush(Sculpt *sd, Object *ob, Span<PBVHNode *> nodes)
{
  Brush *brush = BKE_paint_brush(&sd->paint);

  SCULPT_smooth_undo_push(ob, nodes);

  /* Threaded loop over nodes. */
  SculptThreadedTaskData data{};
  data.sd = sd;
  data.ob = ob;
  data.brush = brush;
  data.nodes = nodes;

  TaskParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, true, nodes.size());
  for (int i = 0; i < brush->surface_smooth_iterations; i++) {
    BLI_task_parallel_range(
        0, nodes.size(), &data, SCULPT_do_surface_smooth_brush_laplacian_task_cb_ex, &settings);
    BLI_task_parallel_range(
        0, nodes.size(), &data, SCULPT_do_surface_smooth_brush_displace_task_cb_ex, &settings);
  }
}
