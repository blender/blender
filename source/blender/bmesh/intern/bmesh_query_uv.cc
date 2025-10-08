/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bmesh
 */

#include "BLI_array.hh"
#include "BLI_math_geom.h"
#include "BLI_math_vector.h"
#include "BLI_math_vector_types.hh"
#include "BLI_string_ref.hh"

#include "BKE_attribute.h"
#include "BKE_customdata.hh"

#include "bmesh.hh"

BMUVOffsets BM_uv_map_offsets_from_layer(const BMesh *bm, const int layer)
{
  using namespace blender;
  using namespace blender::bke;
  const int layer_index = CustomData_get_layer_index_n(&bm->ldata, CD_PROP_FLOAT2, layer);
  if (layer_index == -1) {
    return BMUVOFFSETS_NONE;
  }

  const StringRef name = bm->ldata.layers[layer_index].name;
  char buffer[MAX_CUSTOMDATA_LAYER_NAME];

  BMUVOffsets offsets;
  offsets.uv = bm->ldata.layers[layer_index].offset;
  offsets.pin = CustomData_get_offset_named(
      &bm->ldata, CD_PROP_BOOL, BKE_uv_map_pin_name_get(name, buffer));

  return offsets;
}

BMUVOffsets BM_uv_map_offsets_get(const BMesh *bm)
{
  const int layer = CustomData_get_active_layer(&bm->ldata, CD_PROP_FLOAT2);
  if (layer == -1) {
    return BMUVOFFSETS_NONE;
  }
  return BM_uv_map_offsets_from_layer(bm, layer);
}

static void uv_aspect(const BMLoop *l,
                      const float aspect[2],
                      const int cd_loop_uv_offset,
                      float r_uv[2])
{
  const float *uv = BM_ELEM_CD_GET_FLOAT_P(l, cd_loop_uv_offset);
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
    mul_v2_fl(r_cent, 1.0f / totw);
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
    const float *luv = BM_ELEM_CD_GET_FLOAT_P(l_iter, cd_loop_uv_offset);
    add_v2_v2(r_cent, luv);
  } while ((l_iter = l_iter->next) != l_first);

  mul_v2_fl(r_cent, 1.0f / float(f->len));
}

float BM_face_uv_calc_cross(const BMFace *f, const int cd_loop_uv_offset)
{
  blender::Array<blender::float2, BM_DEFAULT_NGON_STACK_SIZE> uvs(f->len);
  const BMLoop *l_iter;
  const BMLoop *l_first;
  int i = 0;
  l_iter = l_first = BM_FACE_FIRST_LOOP(f);
  do {
    uvs[i++] = BM_ELEM_CD_GET_FLOAT2_P(l_iter, cd_loop_uv_offset);
  } while ((l_iter = l_iter->next) != l_first);
  return cross_poly_v2(reinterpret_cast<const float (*)[2]>(uvs.data()), f->len);
}

void BM_face_uv_minmax(const BMFace *f, float min[2], float max[2], const int cd_loop_uv_offset)
{
  const BMLoop *l_iter;
  const BMLoop *l_first;
  l_iter = l_first = BM_FACE_FIRST_LOOP(f);
  do {
    const float *luv = BM_ELEM_CD_GET_FLOAT_P(l_iter, cd_loop_uv_offset);
    minmax_v2v2_v2(min, max, luv);
  } while ((l_iter = l_iter->next) != l_first);
}

bool BM_loop_uv_share_edge_check(const BMLoop *l_a, const BMLoop *l_b, const int cd_loop_uv_offset)
{
  BLI_assert(l_a->e == l_b->e);
  const float *luv_a_curr = BM_ELEM_CD_GET_FLOAT_P(l_a, cd_loop_uv_offset);
  const float *luv_a_next = BM_ELEM_CD_GET_FLOAT_P(l_a->next, cd_loop_uv_offset);
  const float *luv_b_curr = BM_ELEM_CD_GET_FLOAT_P(l_b, cd_loop_uv_offset);
  const float *luv_b_next = BM_ELEM_CD_GET_FLOAT_P(l_b->next, cd_loop_uv_offset);
  if (l_a->v != l_b->v) {
    std::swap(luv_b_curr, luv_b_next);
  }
  return (equals_v2v2(luv_a_curr, luv_b_curr) && equals_v2v2(luv_a_next, luv_b_next));
}

bool BM_loop_uv_share_vert_check(const BMLoop *l_a, const BMLoop *l_b, const int cd_loop_uv_offset)
{
  BLI_assert(l_a->v == l_b->v);
  const float *luv_a = BM_ELEM_CD_GET_FLOAT_P(l_a, cd_loop_uv_offset);
  const float *luv_b = BM_ELEM_CD_GET_FLOAT_P(l_b, cd_loop_uv_offset);
  if (!equals_v2v2(luv_a, luv_b)) {
    return false;
  }
  return true;
}

bool BM_edge_uv_share_vert_check(const BMEdge *e,
                                 const BMLoop *l_a,
                                 const BMLoop *l_b,
                                 const int cd_loop_uv_offset)
{
  BLI_assert(l_a->v == l_b->v);
  if (!BM_loop_uv_share_vert_check(l_a, l_b, cd_loop_uv_offset)) {
    return false;
  }

  /* No need for null checks, these will always succeed. */
  const BMLoop *l_other_a = BM_loop_other_vert_loop_by_edge(const_cast<BMLoop *>(l_a),
                                                            const_cast<BMEdge *>(e));
  const BMLoop *l_other_b = BM_loop_other_vert_loop_by_edge(const_cast<BMLoop *>(l_b),
                                                            const_cast<BMEdge *>(e));

  {
    const float *luv_other_a = BM_ELEM_CD_GET_FLOAT_P(l_other_a, cd_loop_uv_offset);
    const float *luv_other_b = BM_ELEM_CD_GET_FLOAT_P(l_other_b, cd_loop_uv_offset);
    if (!equals_v2v2(luv_other_a, luv_other_b)) {
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
    projverts[i] = BM_ELEM_CD_GET_FLOAT2_P(l_iter, cd_loop_uv_offset);
  }

  return isect_point_poly_v2(co, reinterpret_cast<const float (*)[2]>(projverts.data()), f->len);
}
