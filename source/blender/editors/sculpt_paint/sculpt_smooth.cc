/* SPDX-FileCopyrightText: 2020 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edsculpt
 */

#include "MEM_guardedalloc.h"

#include "BLI_alloca.h"
#include "BLI_math_vector.h"
#include "BLI_math_vector_types.hh"
#include "BLI_set.hh"
#include "BLI_task.h"
#include "BLI_timeit.hh"
#include "BLI_vector.hh"

#include "DNA_brush_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "BKE_brush.hh"
#include "BKE_context.hh"
#include "BKE_global.hh"
#include "BKE_mesh.hh"
#include "BKE_mesh_mapping.hh"
#include "BKE_paint.hh"
#include "BKE_pbvh_api.hh"

#include "sculpt_intern.hh"

#include "bmesh.hh"

#include <cmath>
#include <cstdlib>

using namespace blender;

#define SMOOTH_FACE_CORNERS

namespace blender::ed::sculpt_paint::smooth {

#if 0
static void SCULPT_neighbor_coors_average_for_detail(SculptSession *ss,
                                                     float result[3],
                                                     PBVHVertRef vertex)

{
  float detail = bke::pbvh::bmesh_detail_size_avg_get(ss->pbvh);

  float original_vertex_co[3];
  copy_v3_v3(original_vertex_co, SCULPT_vertex_co_get(ss, vertex));

  float edge_length_accum = 0;
  int neighbor_count = 0;
  SculptVertexNeighborIter ni;
  SCULPT_VERTEX_NEIGHBORS_ITER_BEGIN (ss, vertex, ni) {
    edge_length_accum = len_v3v3(original_vertex_co, SCULPT_vertex_co_get(ss, ni.vertex));
    neighbor_count++;
  }
  SCULPT_VERTEX_NEIGHBORS_ITER_END(ni);

  if (neighbor_count == 0) {
    copy_v3_v3(result, original_vertex_co);
    return;
  }

  const float edge_length_avg = edge_length_accum / neighbor_count;
  /* This ensures a common length average for all vertices. The smaller this factor is, the more
   * uniform smoothing is going to be across different mesh detail areas, but it will make the
   * smooth brush effect weaker. It can be exposed as a parameter in the future. */
  const float detail_factor = detail * 0.1f;

  float pos_accum[3] = {0.0f};
  SCULPT_VERTEX_NEIGHBORS_ITER_BEGIN (ss, vertex, ni) {
    float disp[3];
    sub_v3_v3v3(disp, SCULPT_vertex_co_get(ss, ni.vertex), original_vertex_co);
    const float original_length = normalize_v3(disp);
    float new_length = min_ff(original_length, detail_factor * original_length / edge_length_avg);
    float new_co[3];
    madd_v3_v3v3fl(new_co, original_vertex_co, disp, new_length);
    add_v3_v3(pos_accum, new_co);
  }
  SCULPT_VERTEX_NEIGHBORS_ITER_END(ni);
  mul_v3_v3fl(result, pos_accum, 1.0f / neighbor_count);
}
#endif

template<bool smooth_face_corners>
static void SCULPT_neighbor_coords_average_interior_ex(SculptSession *ss,
                                                       float result[3],
                                                       PBVHVertRef vertex,
                                                       float projection,
                                                       float hard_corner_pin,
                                                       bool weighted,
                                                       eSculptBoundary bound_type,
                                                       eSculptCorner corner_type,
                                                       bool smooth_origco,
                                                       float factor)
{
  float3 avg(0.0f, 0.0f, 0.0f);

#if 0
  if (weighted) {
    SCULPT_neighbor_coors_average_for_detail(ss, result, vertex);
    return;
  }
#endif

  eSculptBoundary uvflag = ss->distort_correction_mode ? SCULPT_BOUNDARY_UV : SCULPT_BOUNDARY_NONE;
  eSculptBoundary hard_flags = SCULPT_BOUNDARY_SHARP_MARK | SCULPT_BOUNDARY_SHARP_ANGLE |
                               SCULPT_BOUNDARY_MESH;

  hard_flags &= bound_type;

  if (ss->hard_edge_mode) {
    hard_flags |= SCULPT_BOUNDARY_FACE_SET;
  }

  bound_type |= uvflag;

  const eSculptBoundary is_boundary = SCULPT_vertex_is_boundary(ss, vertex, bound_type);
  const eSculptCorner is_corner = SCULPT_vertex_is_corner(ss, vertex, corner_type);

  const float *(*vertex_co_get)(const SculptSession *ss,
                                PBVHVertRef vertex) = smooth_origco ? SCULPT_vertex_origco_get :
                                                                      SCULPT_vertex_co_get;
  void (*vertex_no_get)(const SculptSession *ss,
                        PBVHVertRef vertex,
                        float r_no[3]) = smooth_origco ? SCULPT_vertex_origno_get :
                                                         SCULPT_vertex_normal_get;

  float *areas = nullptr;
  float3 no, co;
  vertex_no_get(ss, vertex, no);
  co = vertex_co_get(ss, vertex);

  const int valence = SCULPT_vertex_valence_get(ss, vertex);

  if (weighted) {
    areas = reinterpret_cast<float *>(BLI_array_alloca(areas, valence));
    BKE_pbvh_get_vert_face_areas(ss->pbvh, vertex, areas, valence);
  }

  float total = 0.0f;
  int totboundary = 0;

  Vector<float, 32> ws;
  Vector<BMLoop *, 32> loops;

  bool is_bmesh = BKE_pbvh_type(ss->pbvh) == PBVH_BMESH;
  auto addblock = [&](PBVHVertRef vertex2, PBVHEdgeRef edge2, float w) {
    if (!is_bmesh) {
      return;
    }
    BMVert *v = reinterpret_cast<BMVert *>(vertex2.i);
    BMEdge *e = reinterpret_cast<BMEdge *>(edge2.i);

    if (!e->l) {
      return;
    }

    BMLoop *l = e->l;
    do {
      BMLoop *l2 = l->v == v ? l : l->next;

      loops.append(l2);
      ws.append(w);
    } while ((l = l->radial_next) != e->l);
  };

  SculptVertexNeighborIter ni;
  SCULPT_VERTEX_NEIGHBORS_ITER_BEGIN (ss, vertex, ni) {
    bool project_ok;
    float w = weighted ? areas[ni.i] : 1.0f;

    eSculptBoundary is_boundary2;

    if (ni.has_edge) {
      is_boundary2 = SCULPT_edge_is_boundary(ss, ni.edge, bound_type);
    }
    else {
      is_boundary2 = SCULPT_vertex_is_boundary(ss, ni.vertex, bound_type);
    }

    eSculptBoundary smooth_types = (!ss->hard_edge_mode ?
                                        SCULPT_BOUNDARY_FACE_SET | SCULPT_BOUNDARY_SEAM :
                                        SCULPT_BOUNDARY_SEAM) |
                                   uvflag;

    if (is_boundary2) {
      totboundary++;
    }

    /* Boundary vertices use only other boundary vertices. */
    if (is_boundary) {
      project_ok = false;

      eSculptBoundary smooth_flag = is_boundary & smooth_types;
      eSculptBoundary smooth_flag2 = is_boundary2 & smooth_types;

      /* Handle smooth boundaries. */
      if (smooth_flag && !(is_boundary & hard_flags) && bool(smooth_flag) != bool(smooth_flag2)) {
        /* Project to plane. */
        float3 t1 = float3(vertex_co_get(ss, ni.vertex)) - co;
        float fac = dot_v3v3(t1, no);

        float3 tco = (co + no * fac);

        if constexpr (smooth_face_corners) {
          addblock(vertex, ni.edge, w);
        }

        avg += tco * w;
        total += w;
      }
      else if ((is_boundary & hard_flags) == (is_boundary2 & hard_flags)) {
        avg += float3(vertex_co_get(ss, ni.vertex)) * w;
        total += w;
        project_ok = true;
      }
    }
    else {
      /* Interior vertices use all neighbors. */
      avg += float3(vertex_co_get(ss, ni.vertex)) * w;
      total += w;
      project_ok = true;
    }

    if constexpr (smooth_face_corners) {
      if (project_ok) {
        addblock(ni.vertex, ni.edge, w);
      }
    }
  }
  SCULPT_VERTEX_NEIGHBORS_ITER_END(ni);

  /* Ensure open strings of boundary edges don't shrink at the endpoints. */
  if (is_boundary && totboundary == 1) {
    total = 0.0;
    zero_v3(avg);

    if constexpr (smooth_face_corners) {
      loops.clear();
      ws.clear();
    }

    SculptVertexNeighborIter ni;
    SCULPT_VERTEX_NEIGHBORS_ITER_BEGIN (ss, vertex, ni) {
      float w = weighted ? areas[ni.i] : 1.0f;
      avg += float3(vertex_co_get(ss, ni.vertex)) * w;
      total += w;

      if constexpr (smooth_face_corners) {
        addblock(ni.vertex, ni.edge, w);
      }
    }
    SCULPT_VERTEX_NEIGHBORS_ITER_END(ni);
  }

  /* Avoid division by 0 when there are no neighbors. */
  if (total == 0) {
    copy_v3_v3(result, vertex_co_get(ss, vertex));
    return;
  }

  if constexpr (smooth_face_corners) {
    if (is_bmesh && !smooth_origco) {
      blender::bke::sculpt::interp_face_corners(
          ss->pbvh, vertex, loops, ws, factor, ss->attrs.boundary_flags->bmesh_cd_offset);
    }
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

void neighbor_coords_average_interior(SculptSession *ss,
                                      float result[3],
                                      PBVHVertRef vertex,
                                      float projection,
                                      float hard_corner_pin,
                                      bool use_area_weights,
                                      bool smooth_origco,
                                      float factor)
{
  eSculptBoundary bound_type = ss->smooth_boundary_flag;
  eSculptCorner corner_type;

  corner_type = eSculptCorner(int(bound_type & (SCULPT_BOUNDARY_MESH | SCULPT_BOUNDARY_SHARP_MARK |
                                                SCULPT_BOUNDARY_SHARP_ANGLE))
                              << SCULPT_CORNER_BIT_SHIFT);

  if (ss->hard_edge_mode && ss->smooth_boundary_flag & SCULPT_BOUNDARY_FACE_SET) {
    corner_type |= SCULPT_CORNER_FACE_SET;
  }

  if (ss->distort_correction_mode & UNDISTORT_RELAX_UVS) {
    SCULPT_neighbor_coords_average_interior_ex<true>(ss,
                                                     result,
                                                     vertex,
                                                     projection,
                                                     hard_corner_pin,
                                                     use_area_weights,
                                                     bound_type,
                                                     corner_type,
                                                     smooth_origco,
                                                     factor);
  }
  else {
    SCULPT_neighbor_coords_average_interior_ex<false>(ss,
                                                      result,
                                                      vertex,
                                                      projection,
                                                      hard_corner_pin,
                                                      use_area_weights,
                                                      bound_type,
                                                      corner_type,
                                                      smooth_origco,
                                                      factor);
  }
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

void SCULPT_get_normal_average(
    SculptSession *ss, float avg[3], PBVHVertRef vertex, bool weighted, bool use_original)
{
  int valence = SCULPT_vertex_valence_get(ss, vertex);

  float *areas = nullptr;

  if (weighted) {
    areas = (float *)BLI_array_alloca(areas, valence * 2);

    BKE_pbvh_get_vert_face_areas(ss->pbvh, vertex, areas, valence);
  }

  int total = 0;
  zero_v3(avg);

  SculptVertexNeighborIter ni;
  SCULPT_VERTEX_NEIGHBORS_ITER_BEGIN (ss, vertex, ni) {
    float w = weighted ? areas[ni.i] : 1.0f;
    float no2[3];

    if (!use_original) {
      SCULPT_vertex_normal_get(ss, ni.vertex, no2);
    }
    else {
      SCULPT_vertex_origno_get(ss, ni.vertex, no2);
    }

    madd_v3_v3fl(avg, no2, w);
    total++;
  }
  SCULPT_VERTEX_NEIGHBORS_ITER_END(ni);

  if (total) {
    normalize_v3(avg);
  }
  else {
    if (!use_original) {
      SCULPT_vertex_normal_get(ss, ni.vertex, avg);
    }
    else {
      SCULPT_vertex_origno_get(ss, ni.vertex, avg);
    }
  }
}

void bmesh_four_neighbor_average(SculptSession *ss,
                                 float avg[3],
                                 float direction_in[3],
                                 BMVert *v,
                                 float projection,
                                 float hard_corner_pin,
                                 int cd_temp,
                                 bool weighted,
                                 bool do_origco,
                                 float factor,
                                 bool reproject_uvs)
{
  float avg_co[3] = {0.0f, 0.0f, 0.0f};
  float tot_co = 0.0f;

  float buckets[8] = {0};
  PBVHVertRef vertex = {(intptr_t)v};

  float *field = BM_ELEM_CD_PTR<float *>(v, cd_temp);
  float dir[3];
  float dir3[3] = {0.0f, 0.0f, 0.0f};

  PBVH_CHECK_NAN4(field);

  float *areas = nullptr;

  SCULPT_vertex_check_origdata(ss, BKE_pbvh_make_vref(intptr_t(v)));

  float *origco = BM_ELEM_CD_PTR<float *>(v, ss->attrs.orig_co->bmesh_cd_offset);
  float *origno = BM_ELEM_CD_PTR<float *>(v, ss->attrs.orig_no->bmesh_cd_offset);

  float direction[3];
  copy_v3_v3(direction, direction_in);

  if (do_origco) {
    /* Project direction into original normal's plane. */
    madd_v3_v3fl(direction, origno, -dot_v3v3(origno, direction));
    normalize_v3(direction);
  }

  eSculptBoundary boundary_mask = SCULPT_BOUNDARY_FACE_SET | SCULPT_BOUNDARY_MESH |
                                  SCULPT_BOUNDARY_SHARP_MARK | SCULPT_BOUNDARY_SEAM |
                                  SCULPT_BOUNDARY_UV;
  eSculptBoundary boundary = SCULPT_vertex_is_boundary(ss, vertex, boundary_mask);

  eSculptCorner corner_mask = eSculptCorner(int(boundary) << SCULPT_CORNER_BIT_SHIFT);
  eSculptCorner corner = SCULPT_vertex_is_corner(ss, vertex, corner_mask);

  eSculptBoundary smooth_mask = SCULPT_BOUNDARY_SEAM | SCULPT_BOUNDARY_UV;
  if (!ss->hard_edge_mode) {
    smooth_mask |= SCULPT_BOUNDARY_FACE_SET;
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
  int totboundary = 0;

  Vector<BMLoop *, 32> loops;
  Vector<float, 32> ws;

  auto addloop = [&](BMEdge *e, float w) {
    if (!e->l) {
      return;
    }

    BMLoop *l = e->l;
    l = l->v == v ? l->next : l;

    if (e->l->radial_next != e->l) {
      w *= 0.5f;

      BMLoop *l2 = e->l->radial_next;
      l2 = l2->v == v ? l2->next : l2;

      loops.append(l2);
      ws.append(w);
    }

    loops.append(l);
    ws.append(w);
  };

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

    eSculptBoundary boundary2 = SCULPT_edge_is_boundary(
        ss, BKE_pbvh_make_eref(intptr_t(e)), boundary_mask);
    float dirw = 1.0f;

    PBVH_CHECK_NAN(no1);
    PBVH_CHECK_NAN(dir2);

    /* Add to cross field. */
    if (boundary2 != SCULPT_BOUNDARY_NONE) {
      had_bound = true;

      totboundary++;

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

    closest_vec_to_perp(dir, dir2, no1, buckets, bucketw);

    madd_v3_v3fl(dir3, dir2, dirw);
    totdir3 += dirw;

    if (boundary2) {
      float fac = weighted ? areas[area_i] : 1.0f;

      madd_v3_v3fl(avg_co, co2, fac);
      tot_co += fac;

      addloop(e, fac);
      continue;
    }
    else if (boundary != SCULPT_BOUNDARY_NONE) {
      if (boundary & smooth_mask) {
        float fac = weighted ? areas[area_i] : 1.0f;
        float vec[3], co3[3];

        sub_v3_v3v3(vec, co2, co1);
        copy_v3_v3(co3, co1);
        madd_v3_v3fl(co3, no1, dot_v3v3(vec, no1));

        madd_v3_v3fl(avg_co, co3, fac);
        tot_co += fac;

        addloop(e, fac);
      }
      continue;
    }

    float vec[3];
    sub_v3_v3v3(vec, co2, co1);

    /* Project into no1's plane. */
    madd_v3_v3fl(vec, no1, -dot_v3v3(vec, no1) * 1.0f);
    normalize_v3(vec);

    /* Fac is a measure of how orthogonal or parallel the edge is
     * relative to the direction. */
    float fac = dot_v3v3(vec, dir);

    fac = fac * fac - 0.5f;
    fac *= fac;

    PBVH_CHECK_NAN1(fac);
    PBVH_CHECK_NAN(dir);
    PBVH_CHECK_NAN(vec);

    if (weighted) {
      fac *= areas[area_i];
    }

    madd_v3_v3fl(avg_co, co2, fac);
    tot_co += fac;
    addloop(e, fac);
  }

  if (totboundary == 1) {
    BM_ITER_ELEM_INDEX (e, &eiter, v, BM_EDGES_OF_VERT, area_i) {
      BMVert *v_other = (e->v1 == v) ? e->v2 : e->v1;
      float fac = weighted ? areas[area_i] : 1.0f;

      madd_v3_v3fl(avg_co, v_other->co, fac);
      tot_co += fac;
      addloop(e, fac);
    }
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
    copy_v3_v3(avg, co1);
  }

  if (reproject_uvs && tot_co > 0.0f && !(boundary & SCULPT_BOUNDARY_UV)) {
    float totw = 0.0f;
    for (float w : ws) {
      totw += w;
    }
    for (int i = 0; i < ws.size(); i++) {
      ws[i] /= totw;
    }

    blender::bke::sculpt::interp_face_corners(
        ss->pbvh, vertex, loops, ws, factor, ss->attrs.boundary_flags->bmesh_cd_offset);
  }

  eSculptCorner corner_type = SCULPT_CORNER_MESH | SCULPT_CORNER_SHARP_MARK;
  if (ss->hard_edge_mode) {
    corner_type |= SCULPT_CORNER_FACE_SET;
  }

  if (corner & corner_type) {
    interp_v3_v3v3(avg, avg, SCULPT_vertex_co_get(ss, vertex), hard_corner_pin);
  }

  PBVH_CHECK_NAN(avg);

  /* Do not update field when doing original coordinates. */
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
    PBVH_CHECK_NAN4(field);
  }
}

/* Generic functions for laplacian smoothing. These functions do not take boundary vertices into
 * account. */

void neighbor_coords_average(SculptSession *ss,
                             float result[3],
                             PBVHVertRef vertex,
                             float projection,
                             float hard_corner_pin,
                             bool weighted,
                             float factor)
{
  eSculptCorner corner_type = SCULPT_CORNER_SHARP_MARK | SCULPT_CORNER_FACE_SET;
  eSculptBoundary bound_type = SCULPT_BOUNDARY_SHARP_MARK | SCULPT_BOUNDARY_SEAM |
                               SCULPT_BOUNDARY_UV | SCULPT_BOUNDARY_FACE_SET;

  SCULPT_neighbor_coords_average_interior_ex<false>(ss,
                                                    result,
                                                    vertex,
                                                    projection,
                                                    hard_corner_pin,
                                                    weighted,
                                                    bound_type,
                                                    corner_type,
                                                    false,
                                                    factor);
}

float neighbor_mask_average(SculptSession *ss,
                            const SculptMaskWriteInfo write_info,
                            PBVHVertRef vertex)
{
  float avg = 0.0f;
  int total = 0;
  SculptVertexNeighborIter ni;
  switch (BKE_pbvh_type(ss->pbvh)) {
    case PBVH_FACES:
      SCULPT_VERTEX_NEIGHBORS_ITER_BEGIN (ss, vertex, ni) {
        avg += write_info.layer[ni.vertex.i];
        total++;
      }
      SCULPT_VERTEX_NEIGHBORS_ITER_END(ni);
      break;
    case PBVH_GRIDS:
      SCULPT_VERTEX_NEIGHBORS_ITER_BEGIN (ss, vertex, ni) {
        avg += SCULPT_mask_get_at_grids_vert_index(
            *ss->subdiv_ccg, *BKE_pbvh_get_grid_key(ss->pbvh), vertex.i);
        total++;
      }
      SCULPT_VERTEX_NEIGHBORS_ITER_END(ni);
      break;
    case PBVH_BMESH:
      SCULPT_VERTEX_NEIGHBORS_ITER_BEGIN (ss, vertex, ni) {
        BMVert *vert = reinterpret_cast<BMVert *>(vertex.i);
        avg += BM_ELEM_CD_GET_FLOAT(vert, write_info.bm_offset);
        total++;
      }
      SCULPT_VERTEX_NEIGHBORS_ITER_END(ni);
      break;
  }
  BLI_assert(total > 0);
  return avg / total;
}

void neighbor_color_average(SculptSession *ss, float result[4], PBVHVertRef vertex)
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

static void do_enhance_details_brush_task(Object *ob,
                                          Sculpt *sd,
                                          const Brush *brush,
                                          PBVHNode *node)
{
  SculptSession *ss = ob->sculpt;

  PBVHVertexIter vd;

  float bstrength = ss->cache->bstrength;
  CLAMP(bstrength, -1.0f, 1.0f);

  SculptBrushTest test;
  SculptBrushTestFn sculpt_brush_test_sq_fn = SCULPT_brush_test_init_with_falloff_shape(
      ss, &test, brush->falloff_shape);

  const int thread_id = BLI_task_parallel_thread_id(nullptr);
  auto_mask::NodeData automask_data = auto_mask::node_begin(
      *ob, ss->cache->automasking.get(), *node);

  BKE_pbvh_vertex_iter_begin (ss->pbvh, node, vd, PBVH_ITER_UNIQUE) {
    if (!sculpt_brush_test_sq_fn(&test, vd.co)) {
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

    float disp[3];
    float *detail_dir = blender::bke::paint::vertex_attr_ptr<float>(vd.vertex,
                                                                    ss->attrs.detail_directions);
    madd_v3_v3v3fl(disp, vd.co, detail_dir, fade);
    SCULPT_clip(sd, ss, vd.co, disp);

    BKE_sculpt_sharp_boundary_flag_update(ss, vd.vertex);
  }
  BKE_pbvh_vertex_iter_end;
}

static void enhance_details_brush(Sculpt *sd, Object *ob, Span<PBVHNode *> nodes)
{
  SculptSession *ss = ob->sculpt;
  Brush *brush = BKE_paint_brush(&sd->paint);

  smooth_undo_push(sd, ob, nodes, brush);

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
          ob, AttrDomain::Point, CD_PROP_FLOAT3, SCULPT_ATTRIBUTE_NAME(laplacian_disp), &params);
    }

    for (int i = 0; i < totvert; i++) {
      PBVHVertRef vertex = BKE_pbvh_index_to_vertex(ss->pbvh, i);

      float avg[3];
      neighbor_coords_average(ss, avg, vertex, projection, hard_corner_pin, use_area_weights);
      float *detail_dir = blender::bke::paint::vertex_attr_ptr<float>(vertex,
                                                                      ss->attrs.detail_directions);

      sub_v3_v3v3(detail_dir, avg, SCULPT_vertex_co_get(ss, vertex));
    }
  }

  threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
    for (const int i : range) {
      do_enhance_details_brush_task(ob, sd, brush, nodes[i]);
    }
  });
}

static void do_smooth_brush_task(Object *ob,
                                 Sculpt *sd,
                                 const Brush *brush,
                                 const bool smooth_mask,
                                 const bool smooth_origco,
                                 const SculptMaskWriteInfo mask_write,
                                 float bstrength,
                                 PBVHNode *node)
{
  SculptSession *ss = ob->sculpt;

  PBVHVertexIter vd;
  const bool do_reproject = need_reproject(ss);

  CLAMP(bstrength, 0.0f, 1.0f);

  SculptBrushTest test;
  SculptBrushTestFn sculpt_brush_test_sq_fn = SCULPT_brush_test_init_with_falloff_shape(
      ss, &test, brush->falloff_shape);

  const int thread_id = BLI_task_parallel_thread_id(nullptr);
  auto_mask::NodeData automask_data = auto_mask::node_begin(
      *ob, ss->cache->automasking.get(), *node);

  float projection = brush->autosmooth_projection;
  bool weighted = brush->flag2 & BRUSH_SMOOTH_USE_AREA_WEIGHT;
  bool modified = false;

  float hard_corner_pin = BKE_brush_hard_corner_pin_get(ss->scene, brush);

  BKE_pbvh_vertex_iter_begin (ss->pbvh, node, vd, PBVH_ITER_UNIQUE) {
    if (!sculpt_brush_test_sq_fn(&test, vd.co)) {
      continue;
    }

    modified = true;

    auto_mask::node_update(automask_data, vd);

    float fade = bstrength * SCULPT_brush_strength_factor(ss,
                                                          brush,
                                                          vd.co,
                                                          sqrtf(test.dist),
                                                          vd.no,
                                                          vd.fno,
                                                          smooth_mask ? 0.0f : vd.mask,
                                                          vd.vertex,
                                                          thread_id,
                                                          &automask_data);

    if (smooth_mask) {
      float val = neighbor_mask_average(ss, mask_write, vd.vertex) - vd.mask;
      val *= fade * bstrength;
      float new_mask = vd.mask + val;
      CLAMP(new_mask, 0.0f, 1.0f);

      SCULPT_mask_vert_set(BKE_pbvh_type(ss->pbvh), mask_write, new_mask, vd);
    }
    else {
      float oldco[3];
      float oldno[3];
      copy_v3_v3(oldco, vd.co);
      SCULPT_vertex_normal_get(ss, vd.vertex, oldno);

      float avg[3], val[3];
      neighbor_coords_average_interior(
          ss, avg, vd.vertex, projection, hard_corner_pin, weighted, false, fade);

      if (smooth_origco) {
        float origco_avg[3];

        neighbor_coords_average_interior(
            ss, origco_avg, vd.vertex, projection, hard_corner_pin, weighted, true, fade);

        float *origco = blender::bke::paint::vertex_attr_ptr<float>(vd.vertex, ss->attrs.orig_co);
        interp_v3_v3v3(origco, origco, origco_avg, fade);
      }

      sub_v3_v3v3(val, avg, vd.co);
      madd_v3_v3v3fl(val, vd.co, val, fade);
      SCULPT_clip(sd, ss, vd.co, val);

      if (do_reproject) {
        BKE_sculpt_reproject_cdata(ss, vd.vertex, oldco, oldno, ss->distort_correction_mode);
      }

      BKE_sculpt_sharp_boundary_flag_update(ss, vd.vertex);
    }
  }
  BKE_pbvh_vertex_iter_end;

  if (modified) {
    BKE_pbvh_node_mark_update(node);
  }
}

void smooth_undo_push(Sculpt *sd, Object *ob, Span<PBVHNode *> nodes, Brush *brush)
{
  SculptSession *ss = ob->sculpt;

  if (BKE_pbvh_type(ss->pbvh) == PBVH_BMESH && need_reproject(ss)) {
    bool have_dyntopo = dyntopo::stroke_is_dyntopo(ss, sd, brush);

    for (PBVHNode *node : nodes) {
      PBVHFaceIter fd;

      BKE_pbvh_face_iter_begin (ss->pbvh, node, fd) {
        if (have_dyntopo) {
          /* Always log face, uses more memory and is slower. */
          BM_log_face_modified(ss->bm, ss->bm_log, reinterpret_cast<BMFace *>(fd.face.i));
        }
        else {
          /* Logs face once per stroke. */
          BM_log_face_if_modified(ss->bm, ss->bm_log, reinterpret_cast<BMFace *>(fd.face.i));
        }
      }
      BKE_pbvh_face_iter_end(fd);
    }
  }
}

void do_smooth_brush(
    Sculpt *sd, Object *ob, Span<PBVHNode *> nodes, float bstrength, const bool smooth_mask)
{
  SculptSession *ss = ob->sculpt;
  Brush *brush = BKE_paint_brush(&sd->paint);

  const int max_iterations = 4;
  const float fract = 1.0f / max_iterations;
  int iteration, count;
  float last;

  SCULPT_boundary_info_ensure(ob);
  smooth_undo_push(sd, ob, nodes, brush);

  /* PBVH_FACES needs ss->edge_to_face_map. */
  if (BKE_pbvh_type(ss->pbvh) == PBVH_FACES && ss->edge_to_face_map.is_empty()) {
    SCULPT_ensure_epmap(ss);
  }

  CLAMP(bstrength, 0.0f, 1.0f);

  count = int(bstrength * max_iterations);
  last = max_iterations * (bstrength - count * fract);

  if (BKE_pbvh_type(ss->pbvh) == PBVH_FACES && ss->vert_to_face_map.is_empty()) {
    BLI_assert_msg(0, "sculpt smooth: pmap missing");
    return;
  }

  SculptMaskWriteInfo mask_write;
  if (smooth_mask) {
    mask_write = SCULPT_mask_get_for_write(ss);
  }

  for (iteration = 0; iteration <= count; iteration++) {
    const float strength = (iteration != count) ? 1.0f : last;

    if (brush->flag2 & BRUSH_SMOOTH_USE_AREA_WEIGHT) {
      threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
        for (const int i : range) {
          BKE_pbvh_check_tri_areas(ss->pbvh, nodes[i]);
        }
      });

      BKE_pbvh_face_areas_begin(ss->pbvh);
    }

    bool smooth_origco = SCULPT_tool_needs_smooth_origco(brush->sculpt_tool);
    threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
      for (const int i : range) {
        do_smooth_brush_task(
            ob, sd, brush, smooth_mask, smooth_origco, mask_write, strength, nodes[i]);
      }
    });
  }
}

void do_smooth_brush(Sculpt *sd, Object *ob, Span<PBVHNode *> nodes)
{
  SculptSession *ss = ob->sculpt;

  /* NOTE: The enhance brush needs to initialize its state on the first brush step. The stroke
   * strength can become 0 during the stroke, but it can not change sign (the sign is
   * determined in the beginning of the stroke. So here it is important to not switch to
   * enhance brush in the middle of the stroke. */
  if (ss->cache->bstrength < 0.0f) {
    /* Invert mode, intensify details. */
    enhance_details_brush(sd, ob, nodes);
  }
  else {
    /* Regular mode, smooth. */
    do_smooth_brush(sd, ob, nodes, ss->cache->bstrength, false);
  }
}

void surface_smooth_laplacian_init(Object *ob)
{
  SculptAttributeParams params = {};

  params.stroke_only = true;

  ob->sculpt->attrs.laplacian_disp = BKE_sculpt_attribute_ensure(
      ob, AttrDomain::Point, CD_PROP_FLOAT3, SCULPT_ATTRIBUTE_NAME(laplacian_disp), &params);
}

/* HC Smooth Algorithm. */
/* From: Improved Laplacian Smoothing of Noisy Surface Meshes */

void surface_smooth_laplacian_step(SculptSession *ss,
                                   float *disp,
                                   const float co[3],
                                   const PBVHVertRef vertex,
                                   const float origco[3],
                                   const float alpha,
                                   bool use_area_weights)
{
  float laplacian_smooth_co[3];
  float weigthed_o[3], weigthed_q[3], d[3];

  neighbor_coords_average(ss, laplacian_smooth_co, vertex, 0.0f, 0.0f, use_area_weights);

  mul_v3_v3fl(weigthed_o, origco, alpha);
  mul_v3_v3fl(weigthed_q, co, 1.0f - alpha);
  add_v3_v3v3(d, weigthed_o, weigthed_q);
  float *laplacian_disp = blender::bke::paint::vertex_attr_ptr<float>(vertex,
                                                                      ss->attrs.laplacian_disp);

  sub_v3_v3v3(laplacian_disp, laplacian_smooth_co, d);

  sub_v3_v3v3(disp, laplacian_smooth_co, co);
}

void surface_smooth_displace_step(
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

static void do_surface_smooth_brush_laplacian_task(Object *ob, const Brush *brush, PBVHNode *node)
{
  SculptSession *ss = ob->sculpt;
  const float bstrength = ss->cache->bstrength;
  float alpha = brush->surface_smooth_shape_preservation;

  PBVHVertexIter vd;
  SculptOrigVertData orig_data;

  SculptBrushTest test;
  SculptBrushTestFn sculpt_brush_test_sq_fn = SCULPT_brush_test_init_with_falloff_shape(
      ss, &test, brush->falloff_shape);
  const int thread_id = BLI_task_parallel_thread_id(nullptr);

  SCULPT_orig_vert_data_init(&orig_data, ob, node, undo::Type::Position);
  auto_mask::NodeData automask_data = auto_mask::node_begin(
      *ob, ss->cache->automasking.get(), *node);

  bool weighted = brush->flag2 & BRUSH_SMOOTH_USE_AREA_WEIGHT;

  BKE_pbvh_vertex_iter_begin (ss->pbvh, node, vd, PBVH_ITER_UNIQUE) {
    SCULPT_orig_vert_data_update(&orig_data, vd.vertex);
    if (!sculpt_brush_test_sq_fn(&test, vd.co)) {
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

    float disp[3];
    surface_smooth_laplacian_step(ss, disp, vd.co, vd.vertex, orig_data.co, alpha, weighted);
    madd_v3_v3fl(vd.co, disp, clamp_f(fade, 0.0f, 1.0f));
  }
  BKE_pbvh_vertex_iter_end;
}

static void do_surface_smooth_brush_displace_task(Object *ob, const Brush *brush, PBVHNode *node)
{
  SculptSession *ss = ob->sculpt;
  const float bstrength = ss->cache->bstrength;
  const float beta = brush->surface_smooth_current_vertex;

  PBVHVertexIter vd;

  SculptBrushTest test;
  SculptBrushTestFn sculpt_brush_test_sq_fn = SCULPT_brush_test_init_with_falloff_shape(
      ss, &test, brush->falloff_shape);
  const int thread_id = BLI_task_parallel_thread_id(nullptr);
  auto_mask::NodeData automask_data = auto_mask::node_begin(
      *ob, ss->cache->automasking.get(), *node);

  BKE_pbvh_vertex_iter_begin (ss->pbvh, node, vd, PBVH_ITER_UNIQUE) {
    if (!sculpt_brush_test_sq_fn(&test, vd.co)) {
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
    surface_smooth_displace_step(ss, vd.co, vd.vertex, beta, fade);
    BKE_sculpt_sharp_boundary_flag_update(ss, vd.vertex);
  }
  BKE_pbvh_vertex_iter_end;
}

void do_surface_smooth_brush(Sculpt *sd, Object *ob, Span<PBVHNode *> nodes)
{
  Brush *brush = BKE_paint_brush(&sd->paint);

  SCULPT_boundary_info_ensure(ob);
  smooth_undo_push(sd, ob, nodes, brush);

  TaskParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, true, nodes.size());
  for (int i = 0; i < brush->surface_smooth_iterations; i++) {
    threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
      for (const int i : range) {
        do_surface_smooth_brush_laplacian_task(ob, brush, nodes[i]);
      }
    });
    threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
      for (const int i : range) {
        do_surface_smooth_brush_displace_task(ob, brush, nodes[i]);
      }
    });
  }
}

}  // namespace blender::ed::sculpt_paint::smooth
