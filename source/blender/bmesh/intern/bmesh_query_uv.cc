/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bmesh
 */

#include "MEM_guardedalloc.h"

#include "BLI_array.hh"
#include "BLI_linklist.h"
#include "BLI_math.h"
#include "BLI_math_vec_types.hh"
#include "BLI_utildefines_stack.h"

#include "BKE_customdata.h"

#include "DNA_meshdata_types.h"

#include "bmesh.h"
#include "intern/bmesh_private.h"

static void uv_aspect(const BMLoop *l,
                      const float aspect[2],
                      const int cd_loop_uv_offset,
                      float r_uv[2])
{
  const float *uv = ((const MLoopUV *)BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset))->uv;
  r_uv[0] = uv[0] * aspect[0];
  r_uv[1] = uv[1] * aspect[1];
}

/**
 * Typically we avoid hiding arguments,
 * make this an exception since it reads poorly with so many repeated arguments.
 */
#define UV_ASPECT(l, r_uv) uv_aspect(l, aspect, cd_loop_uv_offset, r_uv)

void BM_face_uv_calc_center_median_weighted(const BMFace *f,
                                            const float aspect[2],
                                            const int cd_loop_uv_offset,
                                            float r_cent[2])
{
  const BMLoop *l_iter;
  const BMLoop *l_first;
  float totw = 0.0f;
  float w_prev;

  zero_v2(r_cent);

  l_iter = l_first = BM_FACE_FIRST_LOOP(f);

  float uv_prev[2], uv_curr[2];
  UV_ASPECT(l_iter->prev, uv_prev);
  UV_ASPECT(l_iter, uv_curr);
  w_prev = len_v2v2(uv_prev, uv_curr);
  do {
    float uv_next[2];
    UV_ASPECT(l_iter->next, uv_next);
    const float w_curr = len_v2v2(uv_curr, uv_next);
    const float w = (w_curr + w_prev);
    madd_v2_v2fl(r_cent, uv_curr, w);
    totw += w;
    w_prev = w_curr;
    copy_v2_v2(uv_curr, uv_next);
  } while ((l_iter = l_iter->next) != l_first);

  if (totw != 0.0f) {
    mul_v2_fl(r_cent, 1.0f / (float)totw);
  }
  /* Reverse aspect. */
  r_cent[0] /= aspect[0];
  r_cent[1] /= aspect[1];
}

#undef UV_ASPECT

void BM_face_uv_calc_center_median(const BMFace *f, const int cd_loop_uv_offset, float r_cent[2])
{
  const BMLoop *l_iter;
  const BMLoop *l_first;
  zero_v2(r_cent);
  l_iter = l_first = BM_FACE_FIRST_LOOP(f);
  do {
    const MLoopUV *luv = (const MLoopUV *)BM_ELEM_CD_GET_VOID_P(l_iter, cd_loop_uv_offset);
    add_v2_v2(r_cent, luv->uv);
  } while ((l_iter = l_iter->next) != l_first);

  mul_v2_fl(r_cent, 1.0f / (float)f->len);
}

float BM_face_uv_calc_cross(const BMFace *f, const int cd_loop_uv_offset)
{
  blender::Array<blender::float2, BM_DEFAULT_NGON_STACK_SIZE> uvs(f->len);
  const BMLoop *l_iter;
  const BMLoop *l_first;
  int i = 0;
  l_iter = l_first = BM_FACE_FIRST_LOOP(f);
  do {
    const MLoopUV *luv = (const MLoopUV *)BM_ELEM_CD_GET_VOID_P(l_iter, cd_loop_uv_offset);
    uvs[i++] = luv->uv;
  } while ((l_iter = l_iter->next) != l_first);
  return cross_poly_v2(reinterpret_cast<const float(*)[2]>(uvs.data()), f->len);
}

void BM_face_uv_minmax(const BMFace *f, float min[2], float max[2], const int cd_loop_uv_offset)
{
  const BMLoop *l_iter;
  const BMLoop *l_first;
  l_iter = l_first = BM_FACE_FIRST_LOOP(f);
  do {
    const MLoopUV *luv = (const MLoopUV *)BM_ELEM_CD_GET_VOID_P(l_iter, cd_loop_uv_offset);
    minmax_v2v2_v2(min, max, luv->uv);
  } while ((l_iter = l_iter->next) != l_first);
}

void BM_face_uv_transform(BMFace *f, const float matrix[2][2], const int cd_loop_uv_offset)
{
  BMLoop *l_iter;
  BMLoop *l_first;
  l_iter = l_first = BM_FACE_FIRST_LOOP(f);
  do {
    MLoopUV *luv = (MLoopUV *)BM_ELEM_CD_GET_VOID_P(l_iter, cd_loop_uv_offset);
    mul_m2_v2(matrix, luv->uv);
  } while ((l_iter = l_iter->next) != l_first);
}

bool BM_loop_uv_share_edge_check(BMLoop *l_a, BMLoop *l_b, const int cd_loop_uv_offset)
{
  BLI_assert(l_a->e == l_b->e);
  MLoopUV *luv_a_curr = (MLoopUV *)BM_ELEM_CD_GET_VOID_P(l_a, cd_loop_uv_offset);
  MLoopUV *luv_a_next = (MLoopUV *)BM_ELEM_CD_GET_VOID_P(l_a->next, cd_loop_uv_offset);
  MLoopUV *luv_b_curr = (MLoopUV *)BM_ELEM_CD_GET_VOID_P(l_b, cd_loop_uv_offset);
  MLoopUV *luv_b_next = (MLoopUV *)BM_ELEM_CD_GET_VOID_P(l_b->next, cd_loop_uv_offset);
  if (l_a->v != l_b->v) {
    std::swap(luv_b_curr, luv_b_next);
  }
  return (equals_v2v2(luv_a_curr->uv, luv_b_curr->uv) &&
          equals_v2v2(luv_a_next->uv, luv_b_next->uv));
}

bool BM_loop_uv_share_vert_check(BMLoop *l_a, BMLoop *l_b, const int cd_loop_uv_offset)
{
  BLI_assert(l_a->v == l_b->v);
  const MLoopUV *luv_a = (const MLoopUV *)BM_ELEM_CD_GET_VOID_P(l_a, cd_loop_uv_offset);
  const MLoopUV *luv_b = (const MLoopUV *)BM_ELEM_CD_GET_VOID_P(l_b, cd_loop_uv_offset);
  if (!equals_v2v2(luv_a->uv, luv_b->uv)) {
    return false;
  }
  return true;
}

bool BM_edge_uv_share_vert_check(BMEdge *e, BMLoop *l_a, BMLoop *l_b, const int cd_loop_uv_offset)
{
  BLI_assert(l_a->v == l_b->v);
  if (!BM_loop_uv_share_vert_check(l_a, l_b, cd_loop_uv_offset)) {
    return false;
  }

  /* No need for NULL checks, these will always succeed. */
  const BMLoop *l_other_a = BM_loop_other_vert_loop_by_edge(l_a, e);
  const BMLoop *l_other_b = BM_loop_other_vert_loop_by_edge(l_b, e);

  {
    const MLoopUV *luv_other_a = (const MLoopUV *)BM_ELEM_CD_GET_VOID_P(l_other_a,
                                                                        cd_loop_uv_offset);
    const MLoopUV *luv_other_b = (const MLoopUV *)BM_ELEM_CD_GET_VOID_P(l_other_b,
                                                                        cd_loop_uv_offset);
    if (!equals_v2v2(luv_other_a->uv, luv_other_b->uv)) {
      return false;
    }
  }

  return true;
}

bool BM_face_uv_point_inside_test(const BMFace *f, const float co[2], const int cd_loop_uv_offset)
{
  blender::Array<blender::float2, BM_DEFAULT_NGON_STACK_SIZE> projverts(f->len);

  BMLoop *l_iter;
  int i;

  BLI_assert(BM_face_is_normal_valid(f));

  for (i = 0, l_iter = BM_FACE_FIRST_LOOP(f); i < f->len; i++, l_iter = l_iter->next) {
    projverts[i] = ((const MLoopUV *)BM_ELEM_CD_GET_VOID_P(l_iter, cd_loop_uv_offset))->uv;
  }

  return isect_point_poly_v2(
      co, reinterpret_cast<const float(*)[2]>(projverts.data()), f->len, false);
}
