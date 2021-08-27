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
 * The Original Code is Copyright (C) 2007 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup bmesh
 *
 * Functions for interpolating data across the surface of a mesh.
 */

#include "MEM_guardedalloc.h"

#include "DNA_meshdata_types.h"

#include "BLI_alloca.h"
#include "BLI_array.h"
#include "BLI_linklist.h"
#include "BLI_math.h"
#include "BLI_memarena.h"
#include "BLI_task.h"
#include "BLI_utildefines.h"

#include "BKE_customdata.h"
#include "BKE_multires.h"

#include "bmesh.h"
#include "intern/bmesh_private.h"

int bm_save_id(BMesh *bm, BMElem *elem)
{
  if (!elem->head.data) {
    return -1;
  }

  if (bm->idmap.flag & elem->head.htype) {
    return BM_ELEM_GET_ID(bm, elem);
  }
  else {
    return -1;
  }
}

void bm_restore_id(BMesh *bm, BMElem *elem, int id)
{
  if (!elem->head.data || id == -1) {
    return;
  }

  if (bm->idmap.flag & elem->head.htype) {
    BM_ELEM_CD_SET_INT(elem, bm->idmap.cd_id_off[elem->head.htype], id);
  }
}

/* edge and vertex share, currently there's no need to have different logic */
static void bm_data_interp_from_elem(BMesh *bm,
                                     CustomData *data_layer,
                                     const BMElem *ele_src_1,
                                     const BMElem *ele_src_2,
                                     BMElem *ele_dst,
                                     const float fac)
{
  if (ele_src_1->head.data && ele_src_2->head.data) {
    /* first see if we can avoid interpolation */
    if (fac <= 0.0f) {
      if (ele_src_1 == ele_dst) {
        /* do nothing */
      }
      else {
        int id = bm_save_id(bm, ele_dst);

        CustomData_bmesh_free_block_data(data_layer, ele_dst->head.data);
        CustomData_bmesh_copy_data(
            data_layer, data_layer, ele_src_1->head.data, &ele_dst->head.data);

        bm_restore_id(bm, ele_dst, id);
      }
    }
    else if (fac >= 1.0f) {
      if (ele_src_2 == ele_dst) {
        /* do nothing */
      }
      else {
        int id = bm_save_id(bm, ele_dst);

        CustomData_bmesh_free_block_data(data_layer, ele_dst->head.data);
        CustomData_bmesh_copy_data(
            data_layer, data_layer, ele_src_2->head.data, &ele_dst->head.data);

        bm_restore_id(bm, ele_dst, id);
      }
    }
    else {
      const void *src[2];
      float w[2];

      src[0] = ele_src_1->head.data;
      src[1] = ele_src_2->head.data;
      w[0] = 1.0f - fac;
      w[1] = fac;
      CustomData_bmesh_interp(data_layer, src, w, NULL, 2, ele_dst->head.data);
    }
  }
}

/**
 * \brief Data, Interp From Verts
 *
 * Interpolates per-vertex data from two sources to \a v_dst
 *
 * \note This is an exact match to #BM_data_interp_from_edges
 */
void BM_data_interp_from_verts(
    BMesh *bm, const BMVert *v_src_1, const BMVert *v_src_2, BMVert *v_dst, const float fac)
{
  bm_data_interp_from_elem(
      bm, &bm->vdata, (const BMElem *)v_src_1, (const BMElem *)v_src_2, (BMElem *)v_dst, fac);
}

/**
 * \brief Data, Interp From Edges
 *
 * Interpolates per-edge data from two sources to \a e_dst.
 *
 * \note This is an exact match to #BM_data_interp_from_verts
 */
void BM_data_interp_from_edges(
    BMesh *bm, const BMEdge *e_src_1, const BMEdge *e_src_2, BMEdge *e_dst, const float fac)
{
  bm_data_interp_from_elem(
      bm, &bm->edata, (const BMElem *)e_src_1, (const BMElem *)e_src_2, (BMElem *)e_dst, fac);
}

/**
 * \brief Data Vert Average
 *
 * Sets all the customdata (e.g. vert, loop) associated with a vert
 * to the average of the face regions surrounding it.
 */
static void UNUSED_FUNCTION(BM_Data_Vert_Average)(BMesh *UNUSED(bm), BMFace *UNUSED(f))
{
  // BMIter iter;
}

/**
 * \brief Data Face-Vert Edge Interp
 *
 * Walks around the faces of \a e and interpolates
 * the loop data between two sources.
 */
void BM_data_interp_face_vert_edge(BMesh *bm,
                                   const BMVert *v_src_1,
                                   const BMVert *UNUSED(v_src_2),
                                   BMVert *v,
                                   BMEdge *e,
                                   const float fac)
{
  float w[2];
  BMLoop *l_v1 = NULL, *l_v = NULL, *l_v2 = NULL;
  BMLoop *l_iter = NULL;

  if (!e->l) {
    return;
  }

  w[1] = 1.0f - fac;
  w[0] = fac;

  l_iter = e->l;
  do {
    if (l_iter->v == v_src_1) {
      l_v1 = l_iter;
      l_v = l_v1->next;
      l_v2 = l_v->next;
    }
    else if (l_iter->v == v) {
      l_v1 = l_iter->next;
      l_v = l_iter;
      l_v2 = l_iter->prev;
    }

    if (!l_v1 || !l_v2) {
      return;
    }

    const void *src[2];
    src[0] = l_v1->head.data;
    src[1] = l_v2->head.data;

    CustomData_bmesh_interp(&bm->ldata, src, w, NULL, 2, l_v->head.data);
  } while ((l_iter = l_iter->radial_next) != e->l);
}

/**
 * \brief Data Interp From Face
 *
 * projects target onto source, and pulls interpolated customdata from
 * source.
 *
 * \note Only handles loop customdata. multires is handled.
 */
void BM_face_interp_from_face_ex(BMesh *bm,
                                 BMFace *f_dst,
                                 const BMFace *f_src,
                                 const bool do_vertex,
                                 const void **blocks_l,
                                 const void **blocks_v,
                                 float (*cos_2d)[2],
                                 float axis_mat[3][3])
{
  BMLoop *l_iter;
  BMLoop *l_first;

  float *w = BLI_array_alloca(w, f_src->len);
  float co[2];
  int i;

  if (f_src != f_dst) {
    BM_elem_attrs_copy(bm, bm, f_src, f_dst);
  }

  /* interpolate */
  i = 0;
  l_iter = l_first = BM_FACE_FIRST_LOOP(f_dst);
  do {
    mul_v2_m3v3(co, axis_mat, l_iter->v->co);
    interp_weights_poly_v2(w, cos_2d, f_src->len, co);
    CustomData_bmesh_interp(&bm->ldata, blocks_l, w, NULL, f_src->len, l_iter->head.data);
    if (do_vertex) {
      CustomData_bmesh_interp(&bm->vdata, blocks_v, w, NULL, f_src->len, l_iter->v->head.data);
    }
  } while ((void)i++, (l_iter = l_iter->next) != l_first);
}

void BM_face_interp_from_face(BMesh *bm, BMFace *f_dst, const BMFace *f_src, const bool do_vertex)
{
  BMLoop *l_iter;
  BMLoop *l_first;

  const void **blocks_l = BLI_array_alloca(blocks_l, f_src->len);
  const void **blocks_v = do_vertex ? BLI_array_alloca(blocks_v, f_src->len) : NULL;
  float(*cos_2d)[2] = BLI_array_alloca(cos_2d, f_src->len);
  float axis_mat[3][3]; /* use normal to transform into 2d xy coords */
  int i;

  /* convert the 3d coords into 2d for projection */
  BLI_assert(BM_face_is_normal_valid(f_src));
  axis_dominant_v3_to_m3(axis_mat, f_src->no);

  i = 0;
  l_iter = l_first = BM_FACE_FIRST_LOOP(f_src);
  do {
    mul_v2_m3v3(cos_2d[i], axis_mat, l_iter->v->co);
    blocks_l[i] = l_iter->head.data;
    if (do_vertex) {
      blocks_v[i] = l_iter->v->head.data;
    }
  } while ((void)i++, (l_iter = l_iter->next) != l_first);

  BM_face_interp_from_face_ex(bm, f_dst, f_src, do_vertex, blocks_l, blocks_v, cos_2d, axis_mat);
}

/**
 * \brief Multires Interpolation
 *
 * mdisps is a grid of displacements, ordered thus:
 * <pre>
 *      v1/center----v4/next -> x
 *          |           |
 *          |           |
 *       v2/prev------v3/cur
 *          |
 *          V
 *          y
 * </pre>
 */
static int compute_mdisp_quad(const BMLoop *l,
                              const float l_f_center[3],
                              float v1[3],
                              float v2[3],
                              float v3[3],
                              float v4[3],
                              float e1[3],
                              float e2[3])
{
  float n[3], p[3];

#ifndef NDEBUG
  {
    float cent[3];
    /* computer center */
    BM_face_calc_center_median(l->f, cent);
    BLI_assert(equals_v3v3(cent, l_f_center));
  }
#endif

  mid_v3_v3v3(p, l->prev->v->co, l->v->co);
  mid_v3_v3v3(n, l->next->v->co, l->v->co);

  copy_v3_v3(v1, l_f_center);
  copy_v3_v3(v2, p);
  copy_v3_v3(v3, l->v->co);
  copy_v3_v3(v4, n);

  sub_v3_v3v3(e1, v2, v1);
  sub_v3_v3v3(e2, v3, v4);

  return 1;
}

static bool quad_co(const float v1[3],
                    const float v2[3],
                    const float v3[3],
                    const float v4[3],
                    const float p[3],
                    const float n[3],
                    float r_uv[2])
{
  float projverts[5][3], n2[3];
  const float origin[2] = {0.0f, 0.0f};
  int i;

  /* project points into 2d along normal */
  copy_v3_v3(projverts[0], v1);
  copy_v3_v3(projverts[1], v2);
  copy_v3_v3(projverts[2], v3);
  copy_v3_v3(projverts[3], v4);
  copy_v3_v3(projverts[4], p);

  normal_quad_v3(n2, projverts[0], projverts[1], projverts[2], projverts[3]);

  if (dot_v3v3(n, n2) < -FLT_EPSILON) {
    return false;
  }

  /* rotate */
  poly_rotate_plane(n, projverts, 5);

  float projverts2[4][3];

  /* subtract origin */
  for (i = 0; i < 4; i++) {
    sub_v2_v2(projverts[i], projverts[4]);

    copy_v3_v3(projverts2[i], projverts[i]);
  }

  // expand quad a bit
#if 0
  float eps = FLT_EPSILON * 40000;
  float c[3];

  mid_v3_v3v3v3v3(c, projverts[0], projverts[1], projverts[2], projverts[3]);

  sub_v3_v3(projverts2[0], c);
  sub_v3_v3(projverts2[1], c);
  sub_v3_v3(projverts2[2], c);
  sub_v3_v3(projverts2[3], c);
  mul_v3_fl(projverts2[0], 1.0f + eps);
  mul_v3_fl(projverts2[1], 1.0f + eps);
  mul_v3_fl(projverts2[2], 1.0f + eps);
  mul_v3_fl(projverts2[3], 1.0f + eps);
  add_v3_v3(projverts2[0], c);
  add_v3_v3(projverts2[1], c);
  add_v3_v3(projverts2[2], c);
  add_v3_v3(projverts2[3], c);
#endif

  if (!isect_point_quad_v2(origin, projverts2[0], projverts2[1], projverts2[2], projverts2[3])) {
    return false;
  }

  resolve_quad_uv_v2(r_uv, origin, projverts[0], projverts[3], projverts[2], projverts[1]);

#if 0
  float eps2 = FLT_EPSILON * 4000;
  if (r_uv[0] < -eps2 || r_uv[1] < -eps2 || r_uv[0] > 1.0 + eps2 || r_uv[1] > 1.0 + eps2) {
    return false;
  }
#endif

  CLAMP(r_uv[0], 0.0f, 0.99999f);
  CLAMP(r_uv[1], 0.0f, 0.99999f);

  return true;
}

static void mdisp_axis_from_quad(const float v1[3],
                                 const float v2[3],
                                 float UNUSED(v3[3]),
                                 const float v4[3],
                                 float r_axis_x[3],
                                 float r_axis_y[3])
{
  sub_v3_v3v3(r_axis_x, v4, v1);
  sub_v3_v3v3(r_axis_y, v2, v1);

  normalize_v3(r_axis_x);
  normalize_v3(r_axis_y);
}

/**
 * \param l_src: is loop whose internal displacement.
 * \param l_dst: is loop to project onto.
 * \param p: The point being projected.
 * \param r_axis_x, r_axis_y: The location in loop's #CD_MDISPS grid of point `p`.
 */
static bool mdisp_in_mdispquad(BMLoop *l_src,
                               BMLoop *l_dst,
                               const float l_dst_f_center[3],
                               const float p[3],
                               int res,
                               float r_axis_x[3],
                               float r_axis_y[3],
                               float r_uv[2])
{
  float v1[3], v2[3], v3[3], v4[3], e1[3], e2[3];

  if (is_zero_v3(l_src->v->no)) {
    BM_vert_normal_update_all(l_src->v);
  }
  if (is_zero_v3(l_dst->v->no)) {
    BM_vert_normal_update_all(l_dst->v);
  }

  compute_mdisp_quad(l_dst, l_dst_f_center, v1, v2, v3, v4, e1, e2);

  /* expand quad a bit */
  float c[3];
  float eps = FLT_EPSILON * 400;
  mid_v3_v3v3v3v3(c, v1, v2, v3, v4);

  sub_v3_v3(v1, c);
  sub_v3_v3(v2, c);
  sub_v3_v3(v3, c);
  sub_v3_v3(v4, c);
  mul_v3_fl(v1, 1.0f + eps);
  mul_v3_fl(v2, 1.0f + eps);
  mul_v3_fl(v3, 1.0f + eps);
  mul_v3_fl(v4, 1.0f + eps);
  add_v3_v3(v1, c);
  add_v3_v3(v2, c);
  add_v3_v3(v3, c);
  add_v3_v3(v4, c);

  if (!quad_co(v1, v2, v3, v4, p, l_src->v->no, r_uv)) {
    return 0;
  }

  mul_v2_fl(r_uv, (float)(res - 1));

  mdisp_axis_from_quad(v1, v2, v3, v4, r_axis_x, r_axis_y);

  return 1;
}

static float bm_loop_flip_equotion(float mat[2][2],
                                   float b[2],
                                   const float target_axis_x[3],
                                   const float target_axis_y[3],
                                   const float coord[3],
                                   int i,
                                   int j)
{
  mat[0][0] = target_axis_x[i];
  mat[0][1] = target_axis_y[i];
  mat[1][0] = target_axis_x[j];
  mat[1][1] = target_axis_y[j];
  b[0] = coord[i];
  b[1] = coord[j];

  return cross_v2v2(mat[0], mat[1]);
}

static void bm_loop_flip_disp(const float source_axis_x[3],
                              const float source_axis_y[3],
                              const float target_axis_x[3],
                              const float target_axis_y[3],
                              float disp[3])
{
  float vx[3], vy[3], coord[3];
  float n[3], vec[3];
  float b[2], mat[2][2], d;

  mul_v3_v3fl(vx, source_axis_x, disp[0]);
  mul_v3_v3fl(vy, source_axis_y, disp[1]);
  add_v3_v3v3(coord, vx, vy);

  /* project displacement from source grid plane onto target grid plane */
  cross_v3_v3v3(n, target_axis_x, target_axis_y);
  project_v3_v3v3(vec, coord, n);
  sub_v3_v3v3(coord, coord, vec);

  d = bm_loop_flip_equotion(mat, b, target_axis_x, target_axis_y, coord, 0, 1);

  if (fabsf(d) < 1e-4f) {
    d = bm_loop_flip_equotion(mat, b, target_axis_x, target_axis_y, coord, 0, 2);
    if (fabsf(d) < 1e-4f) {
      d = bm_loop_flip_equotion(mat, b, target_axis_x, target_axis_y, coord, 1, 2);
    }
  }

  disp[0] = (b[0] * mat[1][1] - mat[0][1] * b[1]) / d;
  disp[1] = (mat[0][0] * b[1] - b[0] * mat[1][0]) / d;
}

typedef struct BMLoopInterpMultiresData {
  BMLoop *l_dst;
  BMLoop *l_src_first;
  int cd_loop_mdisp_offset;

  int space;
  MDisps *md_dst;
  const float *f_src_center;
  const float *f_dst_center;

  float *axis_x, *axis_y;
  float *v1, *v4;
  float *e1, *e2;

  int res;
  float d;
} BMLoopInterpMultiresData;

static void loop_interp_multires_cb(void *__restrict userdata,
                                    const int ix,
                                    const TaskParallelTLS *__restrict UNUSED(tls))
{
  BMLoopInterpMultiresData *data = userdata;

  BMLoop *l_first = data->l_src_first;
  BMLoop *l_dst = data->l_dst;
  const int cd_loop_mdisp_offset = data->cd_loop_mdisp_offset;
  int space = data->space;

  MDisps *md_dst = data->md_dst;
  const float *f_src_center = data->f_src_center;

  float *axis_x = data->axis_x;
  float *axis_y = data->axis_y;

  float *v1 = data->v1;
  float *v4 = data->v4;
  float *e1 = data->e1;
  float *e2 = data->e2;

  const int res = data->res;
  const float d = data->d;
  float quad[4][3];

  float n1[3], n2[3];
  normal_tri_v3(n1, l_dst->v->co, l_dst->next->v->co, data->f_dst_center);

  if (space == MULTIRES_SPACE_ABSOLUTE) {
    BMLoop *l = l_dst;

    copy_v3_v3(quad[0], data->f_dst_center);
    interp_v3_v3v3(quad[1], l->v->co, l->next->v->co, 0.5);
    copy_v3_v3(quad[2], l->v->co);
    interp_v3_v3v3(quad[3], l->v->co, l->prev->v->co, 0.5);
  }

  float x = d * ix, y;
  int iy;
  for (y = 0.0f, iy = 0; iy < res; y += d, iy++) {
    BMLoop *l_iter = l_first;
    float co1[3], co2[3], co[3];

    madd_v3_v3v3fl(co1, v1, e1, y);
    madd_v3_v3v3fl(co2, v4, e2, y);
    interp_v3_v3v3(co, co1, co2, x);

    float sum[3];
    int tot = 0;
    zero_v3(sum);
    float mindis = 1e17;

    float baseco[3];
    if (space == MULTIRES_SPACE_ABSOLUTE) {
      interp_bilinear_quad_v3(quad, x, y, baseco);
    }

    do {
      MDisps *md_src;
      float src_axis_x[3], src_axis_y[3];
      float uv[2];

      normal_tri_v3(n2, l_iter->v->co, l_iter->next->v->co, data->f_src_center);
      float th = dot_v3v3(n1, n2);
      if (th < 0.0f) {
        negate_v3(n2);
      }

      th = acos(dot_v3v3(n1, n2) * 0.999999f);
      if (th > M_PI * 0.1) {
        continue;
      }

      md_src = BM_ELEM_CD_GET_VOID_P(l_iter, cd_loop_mdisp_offset);

      if (mdisp_in_mdispquad(l_dst, l_iter, f_src_center, co, res, src_axis_x, src_axis_y, uv)) {
        float disp[3];
        copy_v3_v3(disp, md_dst->disps[iy * res + ix]);

        old_mdisps_bilinear(disp, md_src->disps, res, uv[0], uv[1]);

        if (space == MULTIRES_SPACE_TANGENT) {
          bm_loop_flip_disp(src_axis_x, src_axis_y, axis_x, axis_y, disp);
        }

        float l = len_v3v3(disp, baseco);
        if (l < mindis) {
          mindis = l;
          // tot++;
          // copy_v3_v3(sum, disp);
        }
        add_v3_v3(sum, disp);
        tot++;
        // break;
      }
    } while ((l_iter = l_iter->next) != l_first);

    if (tot) {
      mul_v3_fl(sum, 1.0 / (float)tot);
      copy_v3_v3(md_dst->disps[iy * res + ix], sum);
    }
    else {
      // printf("failed to set disp: %f %f\n", x, y);
      if (space == MULTIRES_SPACE_ABSOLUTE) {
        // copy_v3_v3(md_dst->disps[iy * res + ix], baseco);
        // copy_v3_v3(md_dst->disps[iy * res + ix], baseco);
      }
    }
  }
}

void BM_loop_interp_multires_ex(BMesh *bm,
                                BMLoop *l_dst,
                                const BMFace *f_src,
                                const float f_dst_center[3],
                                const float f_src_center[3],
                                const int cd_loop_mdisp_offset)
{
  MDisps *md_dst;
  float v1[3], v2[3], v3[3], v4[3] = {0.0f, 0.0f, 0.0f}, e1[3], e2[3];
  float axis_x[3], axis_y[3];

  /* ignore 2-edged faces */
  if (UNLIKELY(l_dst->f->len < 3)) {
    return;
  }

  md_dst = BM_ELEM_CD_GET_VOID_P(l_dst, cd_loop_mdisp_offset);
  compute_mdisp_quad(l_dst, f_dst_center, v1, v2, v3, v4, e1, e2);

  /* if no disps data allocate a new grid, the size of the first grid in f_src. */
  if (!md_dst->totdisp) {
    const MDisps *md_src = BM_ELEM_CD_GET_VOID_P(BM_FACE_FIRST_LOOP(f_src), cd_loop_mdisp_offset);

    md_dst->totdisp = md_src->totdisp;
    md_dst->level = md_src->level;
    if (md_dst->totdisp) {
      md_dst->disps = MEM_callocN(sizeof(float[3]) * md_dst->totdisp, __func__);
    }
    else {
      return;
    }
  }

  mdisp_axis_from_quad(v1, v2, v3, v4, axis_x, axis_y);

  const int res = (int)sqrt(md_dst->totdisp);
  BMLoopInterpMultiresData data = {
      .l_dst = l_dst,
      .l_src_first = BM_FACE_FIRST_LOOP(f_src),
      .cd_loop_mdisp_offset = cd_loop_mdisp_offset,
      .md_dst = md_dst,
      .f_src_center = f_src_center,
      .f_dst_center = f_dst_center,
      .axis_x = axis_x,
      .space = bm->multiresSpace,
      .axis_y = axis_y,
      .v1 = v1,
      .v4 = v4,
      .e1 = e1,
      .e2 = e2,
      .res = res,
      .d = 1.0f / (float)(res - 1),
  };
  TaskParallelSettings settings;
  BLI_parallel_range_settings_defaults(&settings);
  settings.use_threading = (res > 5);
  BLI_task_parallel_range(0, res, &data, loop_interp_multires_cb, &settings);
}

/**
 * project the multires grid in target onto f_src's set of multires grids
 */
void BM_loop_interp_multires(BMesh *bm, BMLoop *l_dst, const BMFace *f_src)
{
  const int cd_loop_mdisp_offset = CustomData_get_offset(&bm->ldata, CD_MDISPS);

  if (cd_loop_mdisp_offset != -1) {
    float f_dst_center[3];
    float f_src_center[3];

    BM_face_calc_center_median(l_dst->f, f_dst_center);
    BM_face_calc_center_median(f_src, f_src_center);

    BM_loop_interp_multires_ex(bm, l_dst, f_src, f_dst_center, f_src_center, cd_loop_mdisp_offset);
  }
}

void BM_face_interp_multires_ex(BMesh *bm,
                                BMFace *f_dst,
                                const BMFace *f_src,
                                const float f_dst_center[3],
                                const float f_src_center[3],
                                const int cd_loop_mdisp_offset)
{
  BMLoop *l_iter, *l_first;
  l_iter = l_first = BM_FACE_FIRST_LOOP(f_dst);
  do {
    BM_loop_interp_multires_ex(
        bm, l_iter, f_src, f_dst_center, f_src_center, cd_loop_mdisp_offset);
  } while ((l_iter = l_iter->next) != l_first);

  BM_face_multires_bounds_smooth(bm, f_dst);
}

void BM_face_interp_multires(BMesh *bm, BMFace *f_dst, const BMFace *f_src)
{
  const int cd_loop_mdisp_offset = CustomData_get_offset(&bm->ldata, CD_MDISPS);

  if (cd_loop_mdisp_offset != -1) {
    float f_dst_center[3];
    float f_src_center[3];

    BM_face_calc_center_median(f_dst, f_dst_center);
    BM_face_calc_center_median(f_src, f_src_center);

    BM_face_interp_multires_ex(bm, f_dst, f_src, f_dst_center, f_src_center, cd_loop_mdisp_offset);
  }
}

// smooth with weight falloff towards center of grids
static void bm_multires_smooth(BMesh *bm, BMFace *f, bool no_boundary)
{
  const int cd_loop_mdisp_offset = CustomData_get_offset(&bm->ldata, CD_MDISPS);
  float(*orig)[3] = NULL;
  BLI_array_staticdeclare(orig, 256 * 256);

  if (cd_loop_mdisp_offset < 0) {
    return;
  }

  float cent[3];
  zero_v3(cent);

  int ctot = 0;
  BMLoop *cl = f->l_first;
  do {
    add_v3_v3(cent, cl->v->co);
    cl = cl->next;
    ctot++;
  } while (cl != f->l_first);
  mul_v3_fl(cent, 1.0f / (float)ctot);

  const int offs[][2] = {
      {0, 0},
      //    {-1, -1},
      {-1, 0},
      //    {-1, 1},
      {0, 1},
      // {1, 1},
      {1, 0},
      //    {1, -1},
      {0, -1},
  };

  int totoff = sizeof(offs) / sizeof(*offs);

#ifndef ABS
#  define ABS(a) ((a) < 0 ? -(a) : (a))
#endif

  // int space = bm->multiresSpace;
  BMLoop *l = f->l_first;
  do {
    MDisps *md = BM_ELEM_CD_GET_VOID_P(l, cd_loop_mdisp_offset);
    if (!md->disps)
      continue;

    int res = (int)floor(sqrt((double)md->totdisp) + 0.000001);

    int start = no_boundary ? 1 : 0;
    int end = no_boundary ? res - 1 : res;
    float df = 1.0f / (float)(res - 1);
    float u = 0.0;

    BLI_array_clear(orig);
    BLI_array_reserve(orig, md->totdisp * 3);
    memcpy(orig, md->disps, sizeof(float) * 3 * md->totdisp);

    for (int x = start; x < end; x++, u += df) {
      float v = 0.0;

      for (int y = start; y < end; y++, v += df) {
        float co[3];
        float tot = 0.0f;

        zero_v3(co);

        int idx1 = y * res + x;

        for (int oi = 0; oi < totoff; oi++) {
          int ox = x + offs[oi][0];
          int oy = y + offs[oi][1];
          MDisps *md2 = md;

          if (1 && (ox < 0 || oy < 0 || ox >= res || oy >= res)) {
            BMLoop *l2 = NULL;
            BMLoop *ls = l;

            if (ox < 0 && oy < 0) {
              l2 = ls->next->next;
              ox = ABS(ox);
              oy = ABS(oy);
            }
            else if (ox < 0 && oy >= 0 && oy < res) {
              l2 = ls->prev;
              int t = oy;

              oy = -ox;
              ox = t;
            }
            else if (oy < 0 && ox >= 0 && ox < res) {
              l2 = ls->next;
              int t = oy;

              oy = ox;
              ox = -t;
            }
            else if (ox >= res && oy >= 0 && oy < res) {
              l2 = ls->radial_next->next;

              if (ls->v == l2->v) {
                int t = oy;

                oy = 2 * res - ox - 1;
                ox = t;
              }
              else {
                l2 = l2->prev;
                ox = res - ox;
              }

              // XXX disables this branch
              // ox = oy = -1;
            }
            else if (oy >= res && ox >= 0 && ox < res) {
              l2 = ls->prev->radial_next;
              if (l2->v == ls->v) {
                int t = ox;

                ox = 2 * res - oy - 1;
                oy = t;
              }
              else {
                l2 = l2->next;
                oy = 2 * res - oy - 1;
              }
              // XXX disables this branch
              // ox = oy = -1;
            }
            else {
              printf("ignoring non-4-valence multires corner %d %d %d %d : %d %d %d\t",
                     ox,
                     oy,
                     offs[oi][0],
                     offs[oi][1],
                     x,
                     y,
                     res);
              l2 = NULL;
            }

            if (l2) {
              // ox = res - ox - 1;
              // oy = res - oy - 1;
              md2 = BM_ELEM_CD_GET_VOID_P(l2, cd_loop_mdisp_offset);
            }
          }

          if (!md2->disps || oy < 0 || oy >= res || ox < 0 || ox >= res) {
            continue;
          }

          int idx2 = oy * res + ox;
          float *oco2 = md == md2 ? orig[idx2] : md2->disps[idx2];
          float co2[3];

          copy_v3_v3(co2, oco2);

          float dx = (float)offs[oi][0];
          float dy = (float)offs[oi][1];

          float w = 2.0f - dx * dx + dy * dy;

          if (no_boundary && (ox == 0 || oy == 0 || ox == res - 1 || oy == res - 1)) {
            //  w = 2.0;
          }
          else if (ox == x && oy == y) {
            // blend less away from edges
            float au = fabs(u - 0.5) * 2.0, av = fabs(v - 0.5) * 2.0;
            float w2 = au * au + av * av;

            w = 4.0 * w2;
          }
          w = 1.0;

          mul_v3_fl(co2, w);

          tot += w;
          add_v3_v3(co, co2);
        }

        /*
        float vec[3];
        copy_v3_v3(vec, f->no);
        mul_v3_fl(vec, 0.4);

        add_v3_v3(md->disps[idx1], vec);
        sub_v3_v3(md->disps[idx1], cent);
        mul_v3_fl(md->disps[idx1], 1.2);
        add_v3_v3(md->disps[idx1], cent);

        continue;
        //*/

        if (tot > 0.0f) {
          mul_v3_fl(co, 1.0f / tot);
          copy_v3_v3(md->disps[idx1], co);
        }
      }
    }

    l = l->next;
  } while (l != f->l_first);

  BLI_array_free(orig);
}

struct Object *multires_dump_grids_bmesh(struct Object *bmob, BMesh *bm);

void bmo_test_mres_smooth_exec(BMesh *bm, BMOperator *op)
{
  BMIter iter;
  BMFace *f;

  if (!CustomData_has_layer(&bm->ldata, CD_MDISPS)) {
    return;
  }

  BM_ITER_MESH (f, &iter, bm, BM_FACES_OF_MESH) {
    bool ok = !!BM_elem_flag_test(f, BM_ELEM_SELECT);
    ok = ok && !BM_elem_flag_test(f, BM_ELEM_HIDDEN);

    if (!ok) {
      continue;
    }

    // bm_multires_smooth(bm, f, true);
    // BM_multires_smooth(bm, f, false);
    // BM_multires_smooth(bm, f, false);
    // for (int i=0; i<5; i++) {
    BM_face_multires_bounds_smooth(bm, f);
    // }
  }

  multires_dump_grids_bmesh(NULL, bm);
}

void BM_face_multires_bounds_smooth(BMesh *bm, BMFace *f)
{
  return;
  if (bm->multiresSpace == MULTIRES_SPACE_ABSOLUTE) {
    BM_face_multires_stitch(bm, f);

    // for (int i=0; i<5; i++) {
    // bm_multires_smooth(bm, f, true);
    //}
  }
}

/**
 * smooths boundaries between multires grids,
 * including some borders in adjacent faces
 */
void BM_face_multires_stitch(BMesh *bm, BMFace *f)
{
  BMLoop *l;
  BMIter liter;
  float co[3];
  const int cd_loop_mdisp_offset = CustomData_get_offset(&bm->ldata, CD_MDISPS);
  int sides = 0;

  if (cd_loop_mdisp_offset == -1) {
    return;
  }

  BM_ITER_ELEM (l, &liter, f, BM_LOOPS_OF_FACE) {
    MDisps *mdl1 = BM_ELEM_CD_GET_VOID_P(l, cd_loop_mdisp_offset);
    MDisps *mdl2;
    float co1[3], co2[3];
    int x, y;

    /**
     * mdisps is a grid of displacements, ordered thus:
     * <pre>
     *                    v4/next
     *                      |
     *  |      v1/cent-----mid2 ---> x
     *  |         |         |
     *  |         |         |
     * v2/prev---mid1-----v3/cur
     *            |
     *            V
     *            y
     * </pre>
     */

    if (l->radial_next == l) {
      continue;
    }

    if (l->radial_next->v == l->v) {
      mdl2 = BM_ELEM_CD_GET_VOID_P(l->radial_next, cd_loop_mdisp_offset);
    }
    else {
      mdl2 = BM_ELEM_CD_GET_VOID_P(l->radial_next->next, cd_loop_mdisp_offset);
    }

    sides = (int)floor(sqrt(mdl1->totdisp) + FLT_EPSILON);
    for (y = 0; y < sides; y++) {
      int a1, a2, o1, o2;

      if (l->v != l->radial_next->v) {
        a1 = sides * y + sides - 2;
        a2 = (sides - 2) * sides + y;

        o1 = sides * y + sides - 1;
        o2 = (sides - 1) * sides + y;
      }
      else {
        o1 = sides * y + sides - 1;
        o2 = sides * y + sides - 1;
      }

      mid_v3_v3v3(co, mdl1->disps[o1], mdl2->disps[o2]);

      copy_v3_v3(mdl1->disps[o1], co);
      copy_v3_v3(mdl2->disps[o2], co);
    }

    BMLoop *l2 = l->prev->radial_next;
    bool reverse = false;

    if (l2->v != l->v) {
      reverse = true;
      l2 = l2->next;
    }

    mdl2 = BM_ELEM_CD_GET_VOID_P(l2, cd_loop_mdisp_offset);
    y = sides - 1;

    if (!mdl2->disps) {
      continue;
    }

    for (x = 0; x < sides; x++) {
      int x2, y2, o1, o2;

      if (!reverse) {
        x2 = sides - 1;
        y2 = x;
      }
      else {
        x2 = x;
        y2 = y;
      }

      o1 = y * sides + x;
      o2 = y2 * sides + x2;

      mid_v3_v3v3(co, mdl1->disps[o1], mdl2->disps[o2]);
      copy_v3_v3(mdl1->disps[o1], co);
      copy_v3_v3(mdl2->disps[o2], co);
    }
  }

  // do exterior corners
  BM_ITER_ELEM (l, &liter, f, BM_LOOPS_OF_FACE) {
    MDisps *md1 = BM_ELEM_CD_GET_VOID_P(l, cd_loop_mdisp_offset);
    BMIter l2iter;
    BMLoop *l2;
    int x = sides - 1, y = sides - 1;
    int idx = y * sides + x;
    int tot = 1;

    zero_v3(co);

    if (!md1->disps) {
      continue;
    }

    add_v3_v3(co, md1->disps[idx]);

    BM_ITER_ELEM (l2, &l2iter, l->v, BM_LOOPS_OF_VERT) {
      if (l2->v != l->v) {  // winding is flipped
        l2 = l2->next;
      }

      MDisps *md2 = BM_ELEM_CD_GET_VOID_P(l2, cd_loop_mdisp_offset);

      if (l == l2 || !md2->disps) {
        continue;
      }

      add_v3_v3(co, md2->disps[idx]);
      tot++;
    }

    mul_v3_fl(co, 1.0f / (float)tot);

    BM_ITER_ELEM (l2, &l2iter, l->v, BM_LOOPS_OF_VERT) {
      if (l2->v != l->v) {  // winding is flipped
        l2 = l2->next;
      }

      MDisps *md2 = BM_ELEM_CD_GET_VOID_P(l2, cd_loop_mdisp_offset);

      if (l == l2 || !md2->disps) {
        continue;
      }

      copy_v3_v3(md2->disps[idx], co);
    }
  }

  // do interior corners
  int tot = 0;
  zero_v3(co);

  BM_ITER_ELEM (l, &liter, f, BM_LOOPS_OF_FACE) {
    MDisps *md1 = BM_ELEM_CD_GET_VOID_P(l, cd_loop_mdisp_offset);

    if (!md1->disps) {
      continue;
    }

    add_v3_v3(co, md1->disps[0]);
    tot++;
  }

  if (tot) {
    mul_v3_fl(co, 1.0f / (float)tot);
  }

  BM_ITER_ELEM (l, &liter, f, BM_LOOPS_OF_FACE) {
    MDisps *md1 = BM_ELEM_CD_GET_VOID_P(l, cd_loop_mdisp_offset);

    if (!md1->disps) {
      continue;
    }

    copy_v3_v3(md1->disps[0], co);
  }
}

/**
 * projects a single loop, target, onto f_src for customdata interpolation. multires is handled.
 * if do_vertex is true, target's vert data will also get interpolated.
 */
void BM_loop_interp_from_face(
    BMesh *bm, BMLoop *l_dst, const BMFace *f_src, const bool do_vertex, const bool do_multires)
{
  BMLoop *l_iter;
  BMLoop *l_first;
  const void **vblocks = do_vertex ? BLI_array_alloca(vblocks, f_src->len) : NULL;
  const void **blocks = BLI_array_alloca(blocks, f_src->len);
  float(*cos_2d)[2] = BLI_array_alloca(cos_2d, f_src->len);
  float *w = BLI_array_alloca(w, f_src->len);
  float axis_mat[3][3]; /* use normal to transform into 2d xy coords */
  float co[2];
  int i;

  /* Convert the 3d coords into 2d for projection. */
  float axis_dominant[3];
  if (!is_zero_v3(f_src->no)) {
    BLI_assert(BM_face_is_normal_valid(f_src));
    copy_v3_v3(axis_dominant, f_src->no);
  }
  else {
    /* Rare case in which all the vertices of the face are aligned.
     * Get a random axis that is orthogonal to the tangent. */
    float vec[3];
    BM_face_calc_tangent_auto(f_src, vec);
    ortho_v3_v3(axis_dominant, vec);
    normalize_v3(axis_dominant);
  }
  axis_dominant_v3_to_m3(axis_mat, axis_dominant);

  i = 0;
  l_iter = l_first = BM_FACE_FIRST_LOOP(f_src);
  do {
    mul_v2_m3v3(cos_2d[i], axis_mat, l_iter->v->co);
    blocks[i] = l_iter->head.data;

    if (do_vertex) {
      vblocks[i] = l_iter->v->head.data;
    }
  } while ((void)i++, (l_iter = l_iter->next) != l_first);

  mul_v2_m3v3(co, axis_mat, l_dst->v->co);

  /* interpolate */
  interp_weights_poly_v2(w, cos_2d, f_src->len, co);
  CustomData_bmesh_interp(&bm->ldata, blocks, w, NULL, f_src->len, l_dst->head.data);
  if (do_vertex) {
    CustomData_bmesh_interp(&bm->vdata, vblocks, w, NULL, f_src->len, l_dst->v->head.data);
  }

  if (do_multires) {
    BM_loop_interp_multires(bm, l_dst, f_src);
  }
}

void BM_vert_interp_from_face(BMesh *bm, BMVert *v_dst, const BMFace *f_src)
{
  BMLoop *l_iter;
  BMLoop *l_first;
  const void **blocks = BLI_array_alloca(blocks, f_src->len);
  float(*cos_2d)[2] = BLI_array_alloca(cos_2d, f_src->len);
  float *w = BLI_array_alloca(w, f_src->len);
  float axis_mat[3][3]; /* use normal to transform into 2d xy coords */
  float co[2];
  int i;

  /* convert the 3d coords into 2d for projection */
  BLI_assert(BM_face_is_normal_valid(f_src));
  axis_dominant_v3_to_m3(axis_mat, f_src->no);

  i = 0;
  l_iter = l_first = BM_FACE_FIRST_LOOP(f_src);
  do {
    mul_v2_m3v3(cos_2d[i], axis_mat, l_iter->v->co);
    blocks[i] = l_iter->v->head.data;
  } while ((void)i++, (l_iter = l_iter->next) != l_first);

  mul_v2_m3v3(co, axis_mat, v_dst->co);

  /* interpolate */
  interp_weights_poly_v2(w, cos_2d, f_src->len, co);
  CustomData_bmesh_interp(&bm->vdata, blocks, w, NULL, f_src->len, v_dst->head.data);
}

static void update_data_blocks(BMesh *bm, CustomData *olddata, CustomData *data)
{
  BMIter iter;
  BLI_mempool *oldpool = olddata->pool;
  void *block;

  CustomDataLayer **nocopy_layers = NULL;
  BLI_array_staticdeclare(nocopy_layers, 1024);

  // temporarily clear CD_FLAG_ELEM_NOCOPY flags
  for (int i = 0; i < data->totlayer; i++) {
    if (data->layers[i].flag & CD_FLAG_ELEM_NOCOPY) {
      data->layers[i].flag &= ~CD_FLAG_ELEM_NOCOPY;
      BLI_array_append(nocopy_layers, data->layers + i);
    }
  }

  if (data == &bm->vdata) {
    BMVert *eve;

    CustomData_bmesh_init_pool(data, bm->totvert, BM_VERT);

    BM_ITER_MESH (eve, &iter, bm, BM_VERTS_OF_MESH) {
      block = NULL;
      CustomData_bmesh_set_default(data, &block);
      CustomData_bmesh_copy_data(olddata, data, eve->head.data, &block);
      CustomData_bmesh_free_block(olddata, &eve->head.data);
      eve->head.data = block;
    }
  }
  else if (data == &bm->edata) {
    BMEdge *eed;

    CustomData_bmesh_init_pool(data, bm->totedge, BM_EDGE);

    BM_ITER_MESH (eed, &iter, bm, BM_EDGES_OF_MESH) {
      block = NULL;
      CustomData_bmesh_set_default(data, &block);
      CustomData_bmesh_copy_data(olddata, data, eed->head.data, &block);
      CustomData_bmesh_free_block(olddata, &eed->head.data);
      eed->head.data = block;
    }
  }
  else if (data == &bm->ldata) {
    BMIter liter;
    BMFace *efa;
    BMLoop *l;

    CustomData_bmesh_init_pool(data, bm->totloop, BM_LOOP);
    BM_ITER_MESH (efa, &iter, bm, BM_FACES_OF_MESH) {
      BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
        block = NULL;
        CustomData_bmesh_set_default(data, &block);
        CustomData_bmesh_copy_data(olddata, data, l->head.data, &block);
        CustomData_bmesh_free_block(olddata, &l->head.data);
        l->head.data = block;
      }
    }
  }
  else if (data == &bm->pdata) {
    BMFace *efa;

    CustomData_bmesh_init_pool(data, bm->totface, BM_FACE);

    BM_ITER_MESH (efa, &iter, bm, BM_FACES_OF_MESH) {
      block = NULL;
      CustomData_bmesh_set_default(data, &block);
      CustomData_bmesh_copy_data(olddata, data, efa->head.data, &block);
      CustomData_bmesh_free_block(olddata, &efa->head.data);
      efa->head.data = block;
    }
  }
  else {
    /* should never reach this! */
    BLI_assert(0);
  }

  for (int i = 0; i < BLI_array_len(nocopy_layers); i++) {
    nocopy_layers[i]->flag |= CD_FLAG_ELEM_NOCOPY;
  }

  BLI_array_free(nocopy_layers);

  if (oldpool) {
    /* this should never happen but can when dissolve fails - T28960. */
    BLI_assert(data->pool != oldpool);

    BLI_mempool_destroy(oldpool);
  }
}

void BM_data_layers_ensure(BMesh *bm, CustomData *data, BMCustomLayerReq *layers, int totlayer)
{
  bool modified = false;
  CustomData old = *data;
  CustomData temp;
  CustomDataMask mask = 0;

  if (old.layers) {
    old.layers = MEM_dupallocN(old.layers);
  }

  memset(&temp, 0, sizeof(temp));
  CustomData_reset(&temp);

  for (int i = 0; i < totlayer; i++) {
    BMCustomLayerReq *req = layers + i;
    int idx;

    mask |= 1ULL << (CustomDataMask)req->type;

    if (req->name) {
      idx = CustomData_get_named_layer_index(data, req->type, req->name);
    }
    else {
      idx = CustomData_get_layer_index(data, req->type);
    }

    if (idx < 0) {
      modified = true;

      if (req->name) {
        CustomData_add_layer_named(&temp, req->type, CD_ASSIGN, NULL, 0, req->name);
      }
      else {
        CustomData_add_layer(&temp, req->type, CD_ASSIGN, NULL, 0);
      }
    }
  }

  int htype;
  if (data == &bm->vdata) {
    htype = BM_VERT;
  }
  else if (data == &bm->edata) {
    htype = BM_EDGE;
  }
  else if (data == &bm->ldata) {
    htype = BM_LOOP;
  }
  else if (data == &bm->pdata) {
    htype = BM_FACE;
  }
  else {
    printf("error in %s!\n", __func__);
    CustomData_free(&temp, 0);
    return;
  }

  if (modified) {
    CustomData_merge(&temp, data, mask, CD_ASSIGN, 0);
  }

  for (int i = 0; i < totlayer; i++) {
    BMCustomLayerReq *req = layers + i;
    int idx;

    mask |= 1LL << req->type;

    if (req->name) {
      idx = CustomData_get_named_layer_index(data, req->type, req->name);
    }
    else {
      idx = CustomData_get_layer_index(data, req->type);
    }

    data->layers[idx].flag |= req->flag;
  }

  if (modified) {
    /* the pool is now owned by olddata and must not be shared */
    data->pool = NULL;

    update_data_blocks(bm, &old, data);
    bm_update_idmap_cdlayers(bm);
  }

  if (old.layers) {
    MEM_freeN(old.layers);
  }

  CustomData_free(&temp, 0);
}

void BM_data_layer_add(BMesh *bm, CustomData *data, int type)
{
  CustomData olddata;

  olddata = *data;
  olddata.layers = (olddata.layers) ? MEM_dupallocN(olddata.layers) : NULL;

  /* the pool is now owned by olddata and must not be shared */
  data->pool = NULL;

  CustomData_add_layer(data, type, CD_DEFAULT, NULL, 0);

  update_data_blocks(bm, &olddata, data);
  if (olddata.layers) {
    MEM_freeN(olddata.layers);
  }

  bm_update_idmap_cdlayers(bm);
}

void BM_data_layer_add_named(BMesh *bm, CustomData *data, int type, const char *name)
{
  CustomData olddata;

  olddata = *data;
  olddata.layers = (olddata.layers) ? MEM_dupallocN(olddata.layers) : NULL;

  /* the pool is now owned by olddata and must not be shared */
  data->pool = NULL;

  CustomData_add_layer_named(data, type, CD_DEFAULT, NULL, 0, name);

  update_data_blocks(bm, &olddata, data);
  if (olddata.layers) {
    MEM_freeN(olddata.layers);
  }

  bm_update_idmap_cdlayers(bm);
}

void BM_data_layer_free(BMesh *bm, CustomData *data, int type)
{
  CustomData olddata;
  bool has_layer;

  olddata = *data;
  olddata.layers = (olddata.layers) ? MEM_dupallocN(olddata.layers) : NULL;

  /* the pool is now owned by olddata and must not be shared */
  data->pool = NULL;

  has_layer = CustomData_free_layer_active(data, type, 0);
  /* Assert because its expensive to realloc - better not do if layer isn't present. */
  BLI_assert(has_layer != false);
  UNUSED_VARS_NDEBUG(has_layer);

  update_data_blocks(bm, &olddata, data);
  if (olddata.layers) {
    MEM_freeN(olddata.layers);
  }

  bm_update_idmap_cdlayers(bm);
}

void BM_data_layer_free_n(BMesh *bm, CustomData *data, int type, int n)
{
  CustomData olddata;
  bool has_layer;

  olddata = *data;
  olddata.layers = (olddata.layers) ? MEM_dupallocN(olddata.layers) : NULL;

  /* the pool is now owned by olddata and must not be shared */
  data->pool = NULL;

  has_layer = CustomData_free_layer(data, type, 0, CustomData_get_layer_index_n(data, type, n));
  /* Assert because its expensive to realloc - better not do if layer isn't present. */
  BLI_assert(has_layer != false);
  UNUSED_VARS_NDEBUG(has_layer);

  update_data_blocks(bm, &olddata, data);
  if (olddata.layers) {
    MEM_freeN(olddata.layers);
  }

  bm_update_idmap_cdlayers(bm);
}

void BM_data_layer_copy(BMesh *bm, CustomData *data, int type, int src_n, int dst_n)
{
  BMIter iter;

  if (&bm->vdata == data) {
    BMVert *eve;

    BM_ITER_MESH (eve, &iter, bm, BM_VERTS_OF_MESH) {
      void *ptr = CustomData_bmesh_get_n(data, eve->head.data, type, src_n);
      CustomData_bmesh_set_n(data, eve->head.data, type, dst_n, ptr);
    }
  }
  else if (&bm->edata == data) {
    BMEdge *eed;

    BM_ITER_MESH (eed, &iter, bm, BM_EDGES_OF_MESH) {
      void *ptr = CustomData_bmesh_get_n(data, eed->head.data, type, src_n);
      CustomData_bmesh_set_n(data, eed->head.data, type, dst_n, ptr);
    }
  }
  else if (&bm->pdata == data) {
    BMFace *efa;

    BM_ITER_MESH (efa, &iter, bm, BM_FACES_OF_MESH) {
      void *ptr = CustomData_bmesh_get_n(data, efa->head.data, type, src_n);
      CustomData_bmesh_set_n(data, efa->head.data, type, dst_n, ptr);
    }
  }
  else if (&bm->ldata == data) {
    BMIter liter;
    BMFace *efa;
    BMLoop *l;

    BM_ITER_MESH (efa, &iter, bm, BM_FACES_OF_MESH) {
      BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
        void *ptr = CustomData_bmesh_get_n(data, l->head.data, type, src_n);
        CustomData_bmesh_set_n(data, l->head.data, type, dst_n, ptr);
      }
    }
  }
  else {
    /* should never reach this! */
    BLI_assert(0);
  }
}

float BM_elem_float_data_get(CustomData *cd, void *element, int type)
{
  const float *f = CustomData_bmesh_get(cd, ((BMHeader *)element)->data, type);
  return f ? *f : 0.0f;
}

void BM_elem_float_data_set(CustomData *cd, void *element, int type, const float val)
{
  float *f = CustomData_bmesh_get(cd, ((BMHeader *)element)->data, type);
  if (f) {
    *f = val;
  }
}

/* -------------------------------------------------------------------- */
/** \name Loop interpolation functions: BM_vert_loop_groups_data_layer_***
 *
 * Handling loop custom-data such as UV's, while keeping contiguous fans is rather tedious.
 * Especially when a verts loops can have multiple CustomData layers,
 * and each layer can have multiple (different) contiguous fans.
 * Said differently, a single vertices loops may span multiple UV islands.
 *
 * These functions snapshot vertices loops, storing each contiguous fan in its own group.
 * The caller can manipulate the loops, then re-combine the CustomData values.
 *
 * While these functions don't explicitly handle multiple layers at once,
 * the caller can simply store its own list.
 *
 * \note Currently they are averaged back together (weighted by loop angle)
 * but we could copy add other methods to re-combine CustomData-Loop-Fans.
 *
 * \{ */

struct LoopWalkCtx {
  /* same for all groups */
  int type;
  int cd_layer_offset;
  const float *loop_weights;
  MemArena *arena;

  /* --- Per loop fan vars --- */

  /* reference for this contiguous fan */
  const void *data_ref;
  int data_len;

  /* accumulate 'LoopGroupCD.weight' to make unit length */
  float weight_accum;

  /* both arrays the size of the 'BM_vert_face_count(v)'
   * each contiguous fan gets a slide of these arrays */
  void **data_array;
  int *data_index_array;
  float *weight_array;
};

/* Store vars to pass into 'CustomData_bmesh_interp' */
struct LoopGroupCD {
  /* direct customdata pointer array */
  void **data;
  /* weights (aligned with 'data') */
  float *data_weights;
  /* index-in-face */
  int *data_index;
  /* number of loops in the fan */
  int data_len;
};

static void bm_loop_walk_add(struct LoopWalkCtx *lwc, BMLoop *l)
{
  const int i = BM_elem_index_get(l);
  const float w = lwc->loop_weights[i];
  BM_elem_flag_disable(l, BM_ELEM_INTERNAL_TAG);
  lwc->data_array[lwc->data_len] = BM_ELEM_CD_GET_VOID_P(l, lwc->cd_layer_offset);
  lwc->data_index_array[lwc->data_len] = i;
  lwc->weight_array[lwc->data_len] = w;
  lwc->weight_accum += w;

  lwc->data_len += 1;
}

/**
 * called recursively, keep stack-usage minimal.
 *
 * \note called for fan matching so we're pretty much safe not to break the stack
 */
static void bm_loop_walk_data(struct LoopWalkCtx *lwc, BMLoop *l_walk)
{
  int i;

  BLI_assert(CustomData_data_equals(
      lwc->type, lwc->data_ref, BM_ELEM_CD_GET_VOID_P(l_walk, lwc->cd_layer_offset)));
  BLI_assert(BM_elem_flag_test(l_walk, BM_ELEM_INTERNAL_TAG));

  bm_loop_walk_add(lwc, l_walk);

  /* recurse around this loop-fan (in both directions) */
  for (i = 0; i < 2; i++) {
    BMLoop *l_other = ((i == 0) ? l_walk : l_walk->prev)->radial_next;
    if (l_other->radial_next != l_other) {
      if (l_other->v != l_walk->v) {
        l_other = l_other->next;
      }
      BLI_assert(l_other->v == l_walk->v);
      if (BM_elem_flag_test(l_other, BM_ELEM_INTERNAL_TAG)) {
        if (CustomData_data_equals(
                lwc->type, lwc->data_ref, BM_ELEM_CD_GET_VOID_P(l_other, lwc->cd_layer_offset))) {
          bm_loop_walk_data(lwc, l_other);
        }
      }
    }
  }
}

LinkNode *BM_vert_loop_groups_data_layer_create(
    BMesh *bm, BMVert *v, const int layer_n, const float *loop_weights, MemArena *arena)
{
  struct LoopWalkCtx lwc;
  LinkNode *groups = NULL;
  BMLoop *l;
  BMIter liter;
  int loop_num;

  lwc.type = bm->ldata.layers[layer_n].type;
  lwc.cd_layer_offset = bm->ldata.layers[layer_n].offset;
  lwc.loop_weights = loop_weights;
  lwc.arena = arena;

  /* Enable 'BM_ELEM_INTERNAL_TAG', leaving the flag clean on completion. */
  loop_num = 0;
  BM_ITER_ELEM (l, &liter, v, BM_LOOPS_OF_VERT) {
    BM_elem_flag_enable(l, BM_ELEM_INTERNAL_TAG);
    BM_elem_index_set(l, loop_num); /* set_dirty! */
    loop_num++;
  }
  bm->elem_index_dirty |= BM_LOOP;

  lwc.data_len = 0;
  lwc.data_array = BLI_memarena_alloc(lwc.arena, sizeof(void *) * loop_num);
  lwc.data_index_array = BLI_memarena_alloc(lwc.arena, sizeof(int) * loop_num);
  lwc.weight_array = BLI_memarena_alloc(lwc.arena, sizeof(float) * loop_num);

  BM_ITER_ELEM (l, &liter, v, BM_LOOPS_OF_VERT) {
    if (BM_elem_flag_test(l, BM_ELEM_INTERNAL_TAG)) {
      struct LoopGroupCD *lf = BLI_memarena_alloc(lwc.arena, sizeof(*lf));
      int len_prev = lwc.data_len;

      lwc.data_ref = BM_ELEM_CD_GET_VOID_P(l, lwc.cd_layer_offset);

      /* assign len-last */
      lf->data = &lwc.data_array[lwc.data_len];
      lf->data_index = &lwc.data_index_array[lwc.data_len];
      lf->data_weights = &lwc.weight_array[lwc.data_len];
      lwc.weight_accum = 0.0f;

      /* new group */
      bm_loop_walk_data(&lwc, l);
      lf->data_len = lwc.data_len - len_prev;

      if (LIKELY(lwc.weight_accum != 0.0f)) {
        mul_vn_fl(lf->data_weights, lf->data_len, 1.0f / lwc.weight_accum);
      }
      else {
        copy_vn_fl(lf->data_weights, lf->data_len, 1.0f / (float)lf->data_len);
      }

      BLI_linklist_prepend_arena(&groups, lf, lwc.arena);
    }
  }

  BLI_assert(lwc.data_len == loop_num);

  return groups;
}

static void bm_vert_loop_groups_data_layer_merge__single(BMesh *bm,
                                                         void *lf_p,
                                                         int layer_n,
                                                         void *data_tmp)
{
  struct LoopGroupCD *lf = lf_p;
  const int type = bm->ldata.layers[layer_n].type;
  int i;
  const float *data_weights;

  data_weights = lf->data_weights;

  CustomData_bmesh_interp_n(
      &bm->ldata, (const void **)lf->data, data_weights, NULL, lf->data_len, data_tmp, layer_n);

  for (i = 0; i < lf->data_len; i++) {
    CustomData_copy_elements(type, data_tmp, lf->data[i], 1);
  }
}

static void bm_vert_loop_groups_data_layer_merge_weights__single(
    BMesh *bm, void *lf_p, const int layer_n, void *data_tmp, const float *loop_weights)
{
  struct LoopGroupCD *lf = lf_p;
  const int type = bm->ldata.layers[layer_n].type;
  int i;
  const float *data_weights;

  /* re-weight */
  float *temp_weights = BLI_array_alloca(temp_weights, lf->data_len);
  float weight_accum = 0.0f;

  for (i = 0; i < lf->data_len; i++) {
    float w = loop_weights[lf->data_index[i]] * lf->data_weights[i];
    temp_weights[i] = w;
    weight_accum += w;
  }

  if (LIKELY(weight_accum != 0.0f)) {
    mul_vn_fl(temp_weights, lf->data_len, 1.0f / weight_accum);
    data_weights = temp_weights;
  }
  else {
    data_weights = lf->data_weights;
  }

  CustomData_bmesh_interp_n(
      &bm->ldata, (const void **)lf->data, data_weights, NULL, lf->data_len, data_tmp, layer_n);

  for (i = 0; i < lf->data_len; i++) {
    CustomData_copy_elements(type, data_tmp, lf->data[i], 1);
  }
}

/**
 * Take existing custom data and merge each fan's data.
 */
void BM_vert_loop_groups_data_layer_merge(BMesh *bm, LinkNode *groups, const int layer_n)
{
  const int type = bm->ldata.layers[layer_n].type;
  const int size = CustomData_sizeof(type);
  void *data_tmp = alloca(size);

  do {
    bm_vert_loop_groups_data_layer_merge__single(bm, groups->link, layer_n, data_tmp);
  } while ((groups = groups->next));
}

/**
 * A version of #BM_vert_loop_groups_data_layer_merge
 * that takes an array of loop-weights (aligned with #BM_LOOPS_OF_VERT iterator)
 */
void BM_vert_loop_groups_data_layer_merge_weights(BMesh *bm,
                                                  LinkNode *groups,
                                                  const int layer_n,
                                                  const float *loop_weights)
{
  const int type = bm->ldata.layers[layer_n].type;
  const int size = CustomData_sizeof(type);
  void *data_tmp = alloca(size);

  do {
    bm_vert_loop_groups_data_layer_merge_weights__single(
        bm, groups->link, layer_n, data_tmp, loop_weights);
  } while ((groups = groups->next));
}

/** \} */
