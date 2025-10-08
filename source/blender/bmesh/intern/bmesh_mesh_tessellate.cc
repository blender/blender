/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bmesh
 *
 * This file contains code for polygon tessellation
 * (creating triangles from polygons).
 *
 * \see mesh_tessellate.cc for the #Mesh equivalent of this file.
 */

#include "BLI_heap.h"
#include "BLI_math_geom.h"
#include "BLI_math_matrix.h"
#include "BLI_memarena.h"
#include "BLI_polyfill_2d.h"
#include "BLI_polyfill_2d_beautify.h"
#include "BLI_task.h"

#include "bmesh.hh"

using blender::MutableSpan;

/**
 * On systems with 32+ cores,
 * only a very small number of faces has any advantage single threading (in the 100's).
 * Note that between 500-2000 quads, the difference isn't so much
 * (tessellation isn't a bottleneck in this case anyway).
 * Avoid the slight overhead of using threads in this case.
 */
#define BM_FACE_TESSELLATE_THREADED_LIMIT 1024

/* -------------------------------------------------------------------- */
/** \name Default Mesh Tessellation
 * \{ */

/**
 * \param face_normal: This will be optimized out as a constant.
 */
BLI_INLINE void bmesh_calc_tessellation_for_face_impl(std::array<BMLoop *, 3> *looptris,
                                                      BMFace *efa,
                                                      MemArena **pf_arena_p,
                                                      const bool face_normal)
{
#ifndef NDEBUG
  /* The face normal is used for projecting faces into 2D space for tessellation.
   * Invalid normals may result in invalid tessellation.
   * Either `face_normal` should be true or normals should be updated first. */
  BLI_assert(face_normal || BM_face_is_normal_valid(efa));
#endif

  switch (efa->len) {
    case 3: {
      /* `0 1 2` -> `0 1 2` */
      BMLoop *l;
      BMLoop **l_ptr = looptris[0].data();
      l_ptr[0] = l = BM_FACE_FIRST_LOOP(efa);
      l_ptr[1] = l = l->next;
      l_ptr[2] = l->next;
      if (face_normal) {
        normal_tri_v3(efa->no, l_ptr[0]->v->co, l_ptr[1]->v->co, l_ptr[2]->v->co);
      }
      break;
    }
    case 4: {
      /* `0 1 2 3` -> (`0 1 2`, `0 2 3`) */
      BMLoop *l;
      BMLoop **l_ptr_a = looptris[0].data();
      BMLoop **l_ptr_b = looptris[1].data();
      (l_ptr_a[0] = l_ptr_b[0] = l = BM_FACE_FIRST_LOOP(efa));
      (l_ptr_a[1] = l = l->next);
      (l_ptr_a[2] = l_ptr_b[1] = l = l->next);
      (l_ptr_b[2] = l->next);

      if (face_normal) {
        normal_quad_v3(
            efa->no, l_ptr_a[0]->v->co, l_ptr_a[1]->v->co, l_ptr_a[2]->v->co, l_ptr_b[2]->v->co);
      }

      if (UNLIKELY(is_quad_flip_v3_first_third_fast(
              l_ptr_a[0]->v->co, l_ptr_a[1]->v->co, l_ptr_a[2]->v->co, l_ptr_b[2]->v->co)))
      {
        /* Flip out of degenerate 0-2 state. */
        l_ptr_a[2] = l_ptr_b[2];
        l_ptr_b[0] = l_ptr_a[1];
      }
      break;
    }
    default: {
      if (face_normal) {
        BM_face_calc_normal(efa, efa->no);
      }

      BMLoop *l_iter, *l_first;
      BMLoop **l_arr;

      float axis_mat[3][3];
      float (*projverts)[2];
      uint(*tris)[3];

      const int tris_len = efa->len - 2;

      MemArena *pf_arena = *pf_arena_p;
      if (UNLIKELY(pf_arena == nullptr)) {
        pf_arena = *pf_arena_p = BLI_memarena_new(BLI_MEMARENA_STD_BUFSIZE, __func__);
      }

      tris = static_cast<uint(*)[3]>(BLI_memarena_alloc(pf_arena, sizeof(*tris) * tris_len));
      l_arr = static_cast<BMLoop **>(BLI_memarena_alloc(pf_arena, sizeof(*l_arr) * efa->len));
      projverts = static_cast<float (*)[2]>(
          BLI_memarena_alloc(pf_arena, sizeof(*projverts) * efa->len));

      axis_dominant_v3_to_m3_negate(axis_mat, efa->no);

      int i = 0;
      l_iter = l_first = BM_FACE_FIRST_LOOP(efa);
      do {
        l_arr[i] = l_iter;
        mul_v2_m3v3(projverts[i], axis_mat, l_iter->v->co);
        i++;
      } while ((l_iter = l_iter->next) != l_first);

      BLI_polyfill_calc_arena(projverts, efa->len, 1, tris, pf_arena);

      for (i = 0; i < tris_len; i++) {
        BMLoop **l_ptr = looptris[i].data();
        uint *tri = tris[i];

        l_ptr[0] = l_arr[tri[0]];
        l_ptr[1] = l_arr[tri[1]];
        l_ptr[2] = l_arr[tri[2]];
      }

      BLI_memarena_clear(pf_arena);
      break;
    }
  }
}

static void bmesh_calc_tessellation_for_face(std::array<BMLoop *, 3> *looptris,
                                             BMFace *efa,
                                             MemArena **pf_arena_p)
{
  bmesh_calc_tessellation_for_face_impl(looptris, efa, pf_arena_p, false);
}

static void bmesh_calc_tessellation_for_face_with_normal(std::array<BMLoop *, 3> *looptris,
                                                         BMFace *efa,
                                                         MemArena **pf_arena_p)
{
  bmesh_calc_tessellation_for_face_impl(looptris, efa, pf_arena_p, true);
}

/**
 * \brief BM_mesh_calc_tessellation get the looptris and its number from a certain bmesh
 * \param looptris:
 *
 * \note \a looptris Must be pre-allocated to at least the size of given by: poly_to_tri_count
 */
static void bm_mesh_calc_tessellation__single_threaded(
    BMesh *bm, MutableSpan<std::array<BMLoop *, 3>> looptris, const char face_normals)
{
#ifndef NDEBUG
  const int looptris_tot = poly_to_tri_count(bm->totface, bm->totloop);
#endif

  BMIter iter;
  BMFace *efa;
  int i = 0;

  MemArena *pf_arena = nullptr;

  if (face_normals) {
    BM_ITER_MESH (efa, &iter, bm, BM_FACES_OF_MESH) {
      BLI_assert(efa->len >= 3);
      BM_face_calc_normal(efa, efa->no);
      bmesh_calc_tessellation_for_face_with_normal(looptris.data() + i, efa, &pf_arena);
      i += efa->len - 2;
    }
  }
  else {
    BM_ITER_MESH (efa, &iter, bm, BM_FACES_OF_MESH) {
      BLI_assert(efa->len >= 3);
      bmesh_calc_tessellation_for_face(looptris.data() + i, efa, &pf_arena);
      i += efa->len - 2;
    }
  }

  if (pf_arena) {
    BLI_memarena_free(pf_arena);
    pf_arena = nullptr;
  }

  BLI_assert(i <= looptris_tot);
}

struct TessellationUserTLS {
  MemArena *pf_arena;
};

static void bmesh_calc_tessellation_for_face_fn(void *__restrict userdata,
                                                MempoolIterData *mp_f,
                                                const TaskParallelTLS *__restrict tls)
{
  TessellationUserTLS *tls_data = static_cast<TessellationUserTLS *>(tls->userdata_chunk);
  std::array<BMLoop *, 3> *looptris = static_cast<std::array<BMLoop *, 3> *>(userdata);
  BMFace *f = (BMFace *)mp_f;
  BMLoop *l = BM_FACE_FIRST_LOOP(f);
  const int offset = BM_elem_index_get(l) - (BM_elem_index_get(f) * 2);
  bmesh_calc_tessellation_for_face(looptris + offset, f, &tls_data->pf_arena);
}

static void bmesh_calc_tessellation_for_face_with_normals_fn(void *__restrict userdata,
                                                             MempoolIterData *mp_f,
                                                             const TaskParallelTLS *__restrict tls)
{
  TessellationUserTLS *tls_data = static_cast<TessellationUserTLS *>(tls->userdata_chunk);
  std::array<BMLoop *, 3> *looptris = static_cast<std::array<BMLoop *, 3> *>(userdata);
  BMFace *f = (BMFace *)mp_f;
  BMLoop *l = BM_FACE_FIRST_LOOP(f);
  const int offset = BM_elem_index_get(l) - (BM_elem_index_get(f) * 2);
  bmesh_calc_tessellation_for_face_with_normal(looptris + offset, f, &tls_data->pf_arena);
}

static void bmesh_calc_tessellation_for_face_free_fn(const void *__restrict /*userdata*/,
                                                     void *__restrict tls_v)
{
  TessellationUserTLS *tls_data = static_cast<TessellationUserTLS *>(tls_v);
  if (tls_data->pf_arena) {
    BLI_memarena_free(tls_data->pf_arena);
  }
}

static void bm_mesh_calc_tessellation__multi_threaded(
    BMesh *bm, MutableSpan<std::array<BMLoop *, 3>> looptris, const char face_normals)
{
  BM_mesh_elem_index_ensure(bm, BM_LOOP | BM_FACE);

  TaskParallelSettings settings;
  TessellationUserTLS tls_dummy = {nullptr};
  BLI_parallel_mempool_settings_defaults(&settings);
  settings.userdata_chunk = &tls_dummy;
  settings.userdata_chunk_size = sizeof(tls_dummy);
  settings.func_free = bmesh_calc_tessellation_for_face_free_fn;
  BM_iter_parallel(bm,
                   BM_FACES_OF_MESH,
                   face_normals ? bmesh_calc_tessellation_for_face_with_normals_fn :
                                  bmesh_calc_tessellation_for_face_fn,
                   looptris.data(),
                   &settings);
}

void BM_mesh_calc_tessellation_ex(BMesh *bm,
                                  MutableSpan<std::array<BMLoop *, 3>> looptris,
                                  const BMeshCalcTessellation_Params *params)
{
  if (bm->totface < BM_FACE_TESSELLATE_THREADED_LIMIT) {
    bm_mesh_calc_tessellation__single_threaded(bm, looptris, params->face_normals);
  }
  else {
    bm_mesh_calc_tessellation__multi_threaded(bm, looptris, params->face_normals);
  }
}

void BM_mesh_calc_tessellation(BMesh *bm, MutableSpan<std::array<BMLoop *, 3>> looptris)
{
  BMeshCalcTessellation_Params params{};
  params.face_normals = false;
  BM_mesh_calc_tessellation_ex(bm, looptris, &params);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Default Tessellation (Partial Updates)
 * \{ */

struct PartialTessellationUserData {
  BMFace *const *faces;
  MutableSpan<std::array<BMLoop *, 3>> looptris;
};

struct PartialTessellationUserTLS {
  MemArena *pf_arena;
};

static void bmesh_calc_tessellation_for_face_partial_fn(void *__restrict userdata,
                                                        const int index,
                                                        const TaskParallelTLS *__restrict tls)
{
  PartialTessellationUserTLS *tls_data = static_cast<PartialTessellationUserTLS *>(
      tls->userdata_chunk);
  PartialTessellationUserData *data = static_cast<PartialTessellationUserData *>(userdata);
  BMFace *f = data->faces[index];
  BMLoop *l = BM_FACE_FIRST_LOOP(f);
  const int offset = BM_elem_index_get(l) - (BM_elem_index_get(f) * 2);
  bmesh_calc_tessellation_for_face(data->looptris.data() + offset, f, &tls_data->pf_arena);
}

static void bmesh_calc_tessellation_for_face_partial_with_normals_fn(
    void *__restrict userdata, const int index, const TaskParallelTLS *__restrict tls)
{
  PartialTessellationUserTLS *tls_data = static_cast<PartialTessellationUserTLS *>(
      tls->userdata_chunk);
  PartialTessellationUserData *data = static_cast<PartialTessellationUserData *>(userdata);
  BMFace *f = data->faces[index];
  BMLoop *l = BM_FACE_FIRST_LOOP(f);
  const int offset = BM_elem_index_get(l) - (BM_elem_index_get(f) * 2);
  bmesh_calc_tessellation_for_face_with_normal(
      data->looptris.data() + offset, f, &tls_data->pf_arena);
}

static void bmesh_calc_tessellation_for_face_partial_free_fn(const void *__restrict /*userdata*/,
                                                             void *__restrict tls_v)
{
  PartialTessellationUserTLS *tls_data = static_cast<PartialTessellationUserTLS *>(tls_v);
  if (tls_data->pf_arena) {
    BLI_memarena_free(tls_data->pf_arena);
  }
}

static void bm_mesh_calc_tessellation_with_partial__multi_threaded(
    MutableSpan<std::array<BMLoop *, 3>> looptris,
    const BMPartialUpdate *bmpinfo,
    const BMeshCalcTessellation_Params *params)
{
  const int faces_len = bmpinfo->faces.size();
  BMFace *const *faces = bmpinfo->faces.data();

  PartialTessellationUserData data{};
  data.faces = faces;
  data.looptris = looptris;

  PartialTessellationUserTLS tls_dummy = {nullptr};
  TaskParallelSettings settings;
  BLI_parallel_range_settings_defaults(&settings);
  settings.use_threading = true;
  settings.userdata_chunk = &tls_dummy;
  settings.userdata_chunk_size = sizeof(tls_dummy);
  settings.func_free = bmesh_calc_tessellation_for_face_partial_free_fn;

  BLI_task_parallel_range(0,
                          faces_len,
                          &data,
                          params->face_normals ?
                              bmesh_calc_tessellation_for_face_partial_with_normals_fn :
                              bmesh_calc_tessellation_for_face_partial_fn,
                          &settings);
}

static void bm_mesh_calc_tessellation_with_partial__single_threaded(
    MutableSpan<std::array<BMLoop *, 3>> looptris,
    const BMPartialUpdate *bmpinfo,
    const BMeshCalcTessellation_Params *params)
{
  const int faces_len = bmpinfo->faces.size();
  BMFace *const *faces = bmpinfo->faces.data();

  MemArena *pf_arena = nullptr;

  if (params->face_normals) {
    for (int index = 0; index < faces_len; index++) {
      BMFace *f = faces[index];
      BMLoop *l = BM_FACE_FIRST_LOOP(f);
      const int offset = BM_elem_index_get(l) - (BM_elem_index_get(f) * 2);
      bmesh_calc_tessellation_for_face_with_normal(looptris.data() + offset, f, &pf_arena);
    }
  }
  else {
    for (int index = 0; index < faces_len; index++) {
      BMFace *f = faces[index];
      BMLoop *l = BM_FACE_FIRST_LOOP(f);
      const int offset = BM_elem_index_get(l) - (BM_elem_index_get(f) * 2);
      bmesh_calc_tessellation_for_face(looptris.data() + offset, f, &pf_arena);
    }
  }

  if (pf_arena) {
    BLI_memarena_free(pf_arena);
  }
}

void BM_mesh_calc_tessellation_with_partial_ex(BMesh *bm,
                                               MutableSpan<std::array<BMLoop *, 3>> looptris,
                                               const BMPartialUpdate *bmpinfo,
                                               const BMeshCalcTessellation_Params *params)
{
  BLI_assert(bmpinfo->params.do_tessellate);
  /* While harmless, exit early if there is nothing to do (avoids ensuring the index). */
  if (UNLIKELY(bmpinfo->faces.is_empty())) {
    return;
  }

  BM_mesh_elem_index_ensure(bm, BM_LOOP | BM_FACE);

  if (bmpinfo->faces.size() < BM_FACE_TESSELLATE_THREADED_LIMIT) {
    bm_mesh_calc_tessellation_with_partial__single_threaded(looptris, bmpinfo, params);
  }
  else {
    bm_mesh_calc_tessellation_with_partial__multi_threaded(looptris, bmpinfo, params);
  }
}

void BM_mesh_calc_tessellation_with_partial(BMesh *bm,
                                            MutableSpan<std::array<BMLoop *, 3>> looptris,
                                            const BMPartialUpdate *bmpinfo)
{
  BM_mesh_calc_tessellation_with_partial_ex(bm, looptris, bmpinfo, nullptr);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Beauty Mesh Tessellation
 *
 * Avoid degenerate triangles.
 * \{ */

static int bmesh_calc_tessellation_for_face_beauty(std::array<BMLoop *, 3> *looptris,
                                                   BMFace *efa,
                                                   MemArena **pf_arena_p,
                                                   Heap **pf_heap_p)
{
  switch (efa->len) {
    case 3: {
      BMLoop *l;
      BMLoop **l_ptr = looptris[0].data();
      l_ptr[0] = l = BM_FACE_FIRST_LOOP(efa);
      l_ptr[1] = l = l->next;
      l_ptr[2] = l->next;
      return 1;
    }
    case 4: {
      BMLoop *l_v1 = BM_FACE_FIRST_LOOP(efa);
      BMLoop *l_v2 = l_v1->next;
      BMLoop *l_v3 = l_v2->next;
      BMLoop *l_v4 = l_v1->prev;

/* #BM_verts_calc_rotate_beauty performs excessive checks we don't need!
 * It's meant for rotating edges, it also calculates a new normal.
 *
 * Use #BLI_polyfill_beautify_quad_rotate_calc since we have the normal.
 */
#if 0
      const bool split_13 = (BM_verts_calc_rotate_beauty(
                                 l_v1->v, l_v2->v, l_v3->v, l_v4->v, 0, 0) < 0.0f);
#else
      float axis_mat[3][3], v_quad[4][2];
      axis_dominant_v3_to_m3(axis_mat, efa->no);
      mul_v2_m3v3(v_quad[0], axis_mat, l_v1->v->co);
      mul_v2_m3v3(v_quad[1], axis_mat, l_v2->v->co);
      mul_v2_m3v3(v_quad[2], axis_mat, l_v3->v->co);
      mul_v2_m3v3(v_quad[3], axis_mat, l_v4->v->co);

      const bool split_13 = BLI_polyfill_beautify_quad_rotate_calc(
                                v_quad[0], v_quad[1], v_quad[2], v_quad[3]) < 0.0f;
#endif

      BMLoop **l_ptr_a = looptris[0].data();
      BMLoop **l_ptr_b = looptris[1].data();
      if (split_13) {
        l_ptr_a[0] = l_v1;
        l_ptr_a[1] = l_v2;
        l_ptr_a[2] = l_v3;

        l_ptr_b[0] = l_v1;
        l_ptr_b[1] = l_v3;
        l_ptr_b[2] = l_v4;
      }
      else {
        l_ptr_a[0] = l_v1;
        l_ptr_a[1] = l_v2;
        l_ptr_a[2] = l_v4;

        l_ptr_b[0] = l_v2;
        l_ptr_b[1] = l_v3;
        l_ptr_b[2] = l_v4;
      }
      return 2;
    }
    default: {
      MemArena *pf_arena = *pf_arena_p;
      Heap *pf_heap = *pf_heap_p;
      if (UNLIKELY(pf_arena == nullptr)) {
        pf_arena = *pf_arena_p = BLI_memarena_new(BLI_MEMARENA_STD_BUFSIZE, __func__);
        pf_heap = *pf_heap_p = BLI_heap_new_ex(BLI_POLYFILL_ALLOC_NGON_RESERVE);
      }

      BMLoop *l_iter, *l_first;
      BMLoop **l_arr;

      float axis_mat[3][3];
      float (*projverts)[2];
      uint(*tris)[3];

      const int tris_len = efa->len - 2;

      tris = static_cast<uint(*)[3]>(BLI_memarena_alloc(pf_arena, sizeof(*tris) * tris_len));
      l_arr = static_cast<BMLoop **>(BLI_memarena_alloc(pf_arena, sizeof(*l_arr) * efa->len));
      projverts = static_cast<float (*)[2]>(
          BLI_memarena_alloc(pf_arena, sizeof(*projverts) * efa->len));

      axis_dominant_v3_to_m3_negate(axis_mat, efa->no);

      int i = 0;
      l_iter = l_first = BM_FACE_FIRST_LOOP(efa);
      do {
        l_arr[i] = l_iter;
        mul_v2_m3v3(projverts[i], axis_mat, l_iter->v->co);
        i++;
      } while ((l_iter = l_iter->next) != l_first);

      BLI_polyfill_calc_arena(projverts, efa->len, 1, tris, pf_arena);

      BLI_polyfill_beautify(projverts, efa->len, tris, pf_arena, pf_heap);

      for (i = 0; i < tris_len; i++) {
        BMLoop **l_ptr = looptris[i].data();
        uint *tri = tris[i];

        l_ptr[0] = l_arr[tri[0]];
        l_ptr[1] = l_arr[tri[1]];
        l_ptr[2] = l_arr[tri[2]];
      }

      BLI_memarena_clear(pf_arena);

      return tris_len;
    }
  }
}

void BM_mesh_calc_tessellation_beauty(BMesh *bm, MutableSpan<std::array<BMLoop *, 3>> looptris)
{
#ifndef NDEBUG
  const int looptris_tot = poly_to_tri_count(bm->totface, bm->totloop);
#endif

  BMIter iter;
  BMFace *efa;
  int i = 0;

  MemArena *pf_arena = nullptr;

  /* use_beauty */
  Heap *pf_heap = nullptr;

  BM_ITER_MESH (efa, &iter, bm, BM_FACES_OF_MESH) {
    BLI_assert(efa->len >= 3);
    i += bmesh_calc_tessellation_for_face_beauty(looptris.data() + i, efa, &pf_arena, &pf_heap);
  }

  if (pf_arena) {
    BLI_memarena_free(pf_arena);

    BLI_heap_free(pf_heap, nullptr);
  }

  BLI_assert(i <= looptris_tot);
}

/** \} */
