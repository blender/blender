/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bmesh
 *
 * Selection routines for bmesh structures.
 * This is actually all old code ripped from
 * editmesh_lib.c and slightly modified to work
 * for bmesh's. This also means that it has some
 * of the same problems.... something that
 * that should be addressed eventually.
 */

#include <cstddef>

#include "MEM_guardedalloc.h"

#include "DNA_scene_types.h"

#include "BLI_listbase.h"
#include "BLI_math_vector.h"
#include "BLI_task.h"

#include "bmesh.hh"
#include "bmesh_query_uv.hh"
#include "bmesh_structure.hh"

/* For '_FLAG_OVERLAP'. */
#include "bmesh_private.hh"

/* -------------------------------------------------------------------- */
/** \name Recounting total selection.
 * \{ */

struct SelectionCountChunkData {
  int selection_len;
};

static void recount_totsels_range_vert_func(void * /*userdata*/,
                                            MempoolIterData *iter,
                                            const TaskParallelTLS *__restrict tls)
{
  SelectionCountChunkData *count = static_cast<SelectionCountChunkData *>(tls->userdata_chunk);
  const BMVert *eve = (const BMVert *)iter;
  if (BM_elem_flag_test(eve, BM_ELEM_SELECT)) {
    count->selection_len += 1;
  }
}

static void recount_totsels_range_edge_func(void * /*userdata*/,
                                            MempoolIterData *iter,
                                            const TaskParallelTLS *__restrict tls)
{
  SelectionCountChunkData *count = static_cast<SelectionCountChunkData *>(tls->userdata_chunk);
  const BMEdge *eed = (const BMEdge *)iter;
  if (BM_elem_flag_test(eed, BM_ELEM_SELECT)) {
    count->selection_len += 1;
  }
}

static void recount_totsels_range_face_func(void * /*userdata*/,
                                            MempoolIterData *iter,
                                            const TaskParallelTLS *__restrict tls)
{
  SelectionCountChunkData *count = static_cast<SelectionCountChunkData *>(tls->userdata_chunk);
  const BMFace *efa = (const BMFace *)iter;
  if (BM_elem_flag_test(efa, BM_ELEM_SELECT)) {
    count->selection_len += 1;
  }
}

static void recount_totsels_reduce(const void *__restrict /*userdata*/,
                                   void *__restrict chunk_join,
                                   void *__restrict chunk)
{
  SelectionCountChunkData *dst = static_cast<SelectionCountChunkData *>(chunk_join);
  const SelectionCountChunkData *src = static_cast<const SelectionCountChunkData *>(chunk);
  dst->selection_len += src->selection_len;
}

static TaskParallelMempoolFunc recount_totsels_get_range_func(BMIterType iter_type)
{
  BLI_assert(ELEM(iter_type, BM_VERTS_OF_MESH, BM_EDGES_OF_MESH, BM_FACES_OF_MESH));

  TaskParallelMempoolFunc range_func = nullptr;
  if (iter_type == BM_VERTS_OF_MESH) {
    range_func = recount_totsels_range_vert_func;
  }
  else if (iter_type == BM_EDGES_OF_MESH) {
    range_func = recount_totsels_range_edge_func;
  }
  else if (iter_type == BM_FACES_OF_MESH) {
    range_func = recount_totsels_range_face_func;
  }
  return range_func;
}

static int recount_totsel(BMesh *bm, BMIterType iter_type)
{
  const int MIN_ITER_SIZE = 1024;

  TaskParallelSettings settings;
  BLI_parallel_range_settings_defaults(&settings);
  settings.func_reduce = recount_totsels_reduce;
  settings.min_iter_per_thread = MIN_ITER_SIZE;

  SelectionCountChunkData count = {0};
  settings.userdata_chunk = &count;
  settings.userdata_chunk_size = sizeof(count);

  TaskParallelMempoolFunc range_func = recount_totsels_get_range_func(iter_type);
  BM_iter_parallel(bm, iter_type, range_func, nullptr, &settings);
  return count.selection_len;
}

static void recount_totvertsel(BMesh *bm)
{
  bm->totvertsel = recount_totsel(bm, BM_VERTS_OF_MESH);
}

static void recount_totedgesel(BMesh *bm)
{
  bm->totedgesel = recount_totsel(bm, BM_EDGES_OF_MESH);
}

static void recount_totfacesel(BMesh *bm)
{
  bm->totfacesel = recount_totsel(bm, BM_FACES_OF_MESH);
}

static void recount_totsels(BMesh *bm)
{
  recount_totvertsel(bm);
  recount_totedgesel(bm);
  recount_totfacesel(bm);
}

#ifndef NDEBUG
static bool recount_totsels_are_ok(BMesh *bm)
{
  return bm->totvertsel == recount_totsel(bm, BM_VERTS_OF_MESH) &&
         bm->totedgesel == recount_totsel(bm, BM_EDGES_OF_MESH) &&
         bm->totfacesel == recount_totsel(bm, BM_FACES_OF_MESH);
}
#endif

/** \} */

/* -------------------------------------------------------------------- */
/** \name BMesh helper functions for selection & hide flushing.
 * \{ */

static bool bm_vert_is_edge_select_any_other(const BMVert *v, const BMEdge *e_first)
{
  const BMEdge *e_iter = e_first;

  /* start by stepping over the current edge */
  while ((e_iter = bmesh_disk_edge_next(e_iter, v)) != e_first) {
    if (BM_elem_flag_test(e_iter, BM_ELEM_SELECT)) {
      return true;
    }
  }
  return false;
}

#if 0
static bool bm_vert_is_edge_select_any(const BMVert *v)
{
  if (v->e) {
    const BMEdge *e_iter, *e_first;
    e_iter = e_first = v->e;
    do {
      if (BM_elem_flag_test(e_iter, BM_ELEM_SELECT)) {
        return true;
      }
    } while ((e_iter = bmesh_disk_edge_next(e_iter, v)) != e_first);
  }
  return false;
}
#endif

static bool bm_vert_is_edge_visible_any(const BMVert *v)
{
  if (v->e) {
    const BMEdge *e_iter, *e_first;
    e_iter = e_first = v->e;
    do {
      if (!BM_elem_flag_test(e_iter, BM_ELEM_HIDDEN)) {
        return true;
      }
    } while ((e_iter = bmesh_disk_edge_next(e_iter, v)) != e_first);
  }
  return false;
}

static bool bm_edge_is_face_select_any_other(BMLoop *l_first)
{
  const BMLoop *l_iter = l_first;

  /* start by stepping over the current face */
  while ((l_iter = l_iter->radial_next) != l_first) {
    if (BM_elem_flag_test(l_iter->f, BM_ELEM_SELECT)) {
      return true;
    }
  }
  return false;
}

#if 0
static bool bm_edge_is_face_select_any(const BMEdge *e)
{
  if (e->l) {
    const BMLoop *l_iter, *l_first;
    l_iter = l_first = e->l;
    do {
      if (BM_elem_flag_test(l_iter->f, BM_ELEM_SELECT)) {
        return true;
      }
    } while ((l_iter = l_iter->radial_next) != l_first);
  }
  return false;
}
#endif

static bool bm_edge_is_face_visible_any(const BMEdge *e)
{
  if (e->l) {
    const BMLoop *l_iter, *l_first;
    l_iter = l_first = e->l;
    do {
      if (!BM_elem_flag_test(l_iter->f, BM_ELEM_HIDDEN)) {
        return true;
      }
    } while ((l_iter = l_iter->radial_next) != l_first);
  }
  return false;
}

/** \} */

bool BM_mesh_select_is_mixed(const BMesh *bm)
{
  if (bm->selectmode & SCE_SELECT_VERTEX) {
    return !ELEM(bm->totvertsel, 0, bm->totvert);
  }
  if (bm->selectmode & SCE_SELECT_EDGE) {
    return !ELEM(bm->totedgesel, 0, bm->totedge);
  }
  return !ELEM(bm->totfacesel, 0, bm->totface);
}

void BM_mesh_select_mode_clean_ex(BMesh *bm, const short selectmode)
{
  if (selectmode & SCE_SELECT_VERTEX) {
    /* pass */
  }
  else if (selectmode & SCE_SELECT_EDGE) {
    BMIter iter;

    if (bm->totvertsel) {
      BMVert *v;
      BM_ITER_MESH (v, &iter, bm, BM_VERTS_OF_MESH) {
        BM_elem_flag_disable(v, BM_ELEM_SELECT);
      }
      bm->totvertsel = 0;
    }

    if (bm->totedgesel) {
      BMEdge *e;
      BM_ITER_MESH (e, &iter, bm, BM_EDGES_OF_MESH) {
        if (BM_elem_flag_test(e, BM_ELEM_SELECT)) {
          BM_vert_select_set(bm, e->v1, true);
          BM_vert_select_set(bm, e->v2, true);
        }
      }
    }
  }
  else if (selectmode & SCE_SELECT_FACE) {
    BMIter iter;

    if (bm->totvertsel) {
      BMVert *v;
      BM_ITER_MESH (v, &iter, bm, BM_VERTS_OF_MESH) {
        BM_elem_flag_disable(v, BM_ELEM_SELECT);
      }
      bm->totvertsel = 0;
    }

    if (bm->totedgesel) {
      BMEdge *e;
      BM_ITER_MESH (e, &iter, bm, BM_EDGES_OF_MESH) {
        BM_elem_flag_disable(e, BM_ELEM_SELECT);
      }
      bm->totedgesel = 0;
    }

    if (bm->totfacesel) {
      BMFace *f;
      BM_ITER_MESH (f, &iter, bm, BM_FACES_OF_MESH) {
        if (BM_elem_flag_test(f, BM_ELEM_SELECT)) {
          BMLoop *l_iter, *l_first;
          l_iter = l_first = BM_FACE_FIRST_LOOP(f);
          do {
            BM_edge_select_set(bm, l_iter->e, true);
          } while ((l_iter = l_iter->next) != l_first);
        }
      }
    }
  }
}

void BM_mesh_select_mode_clean(BMesh *bm)
{
  BM_mesh_select_mode_clean_ex(bm, bm->selectmode);
}

/* -------------------------------------------------------------------- */
/** \name Select mode flush selection
 * \{ */

struct SelectionFlushChunkData {
  int delta_selection_len;
};

static void bm_mesh_select_mode_flush_vert_to_edge_iter_fn(void * /*userdata*/,
                                                           MempoolIterData *iter,
                                                           const TaskParallelTLS *__restrict tls)
{
  SelectionFlushChunkData *chunk_data = static_cast<SelectionFlushChunkData *>(
      tls->userdata_chunk);
  BMEdge *e = (BMEdge *)iter;
  const bool is_selected = BM_elem_flag_test(e, BM_ELEM_SELECT);
  const bool is_hidden = BM_elem_flag_test(e, BM_ELEM_HIDDEN);
  if (!is_hidden &&
      (BM_elem_flag_test(e->v1, BM_ELEM_SELECT) && BM_elem_flag_test(e->v2, BM_ELEM_SELECT)))
  {
    BM_elem_flag_enable(e, BM_ELEM_SELECT);
    chunk_data->delta_selection_len += is_selected ? 0 : 1;
  }
  else {
    BM_elem_flag_disable(e, BM_ELEM_SELECT);
    chunk_data->delta_selection_len += is_selected ? -1 : 0;
  }
}

static void bm_mesh_select_mode_flush_edge_to_face_iter_fn(void * /*userdata*/,
                                                           MempoolIterData *iter,
                                                           const TaskParallelTLS *__restrict tls)
{
  SelectionFlushChunkData *chunk_data = static_cast<SelectionFlushChunkData *>(
      tls->userdata_chunk);
  BMFace *f = (BMFace *)iter;
  BMLoop *l_iter;
  BMLoop *l_first;
  const bool is_selected = BM_elem_flag_test(f, BM_ELEM_SELECT);
  bool ok = true;
  if (!BM_elem_flag_test(f, BM_ELEM_HIDDEN)) {
    l_iter = l_first = BM_FACE_FIRST_LOOP(f);
    do {
      if (!BM_elem_flag_test(l_iter->e, BM_ELEM_SELECT)) {
        ok = false;
        break;
      }
    } while ((l_iter = l_iter->next) != l_first);
  }
  else {
    ok = false;
  }

  BM_elem_flag_set(f, BM_ELEM_SELECT, ok);
  if (is_selected && !ok) {
    chunk_data->delta_selection_len -= 1;
  }
  else if (ok && !is_selected) {
    chunk_data->delta_selection_len += 1;
  }
}

static void bm_mesh_select_mode_flush_reduce_fn(const void *__restrict /*userdata*/,
                                                void *__restrict chunk_join,
                                                void *__restrict chunk)
{
  SelectionFlushChunkData *dst = static_cast<SelectionFlushChunkData *>(chunk_join);
  const SelectionFlushChunkData *src = static_cast<const SelectionFlushChunkData *>(chunk);
  dst->delta_selection_len += src->delta_selection_len;
}

static void bm_mesh_select_mode_flush_vert_to_edge(BMesh *bm)
{
  SelectionFlushChunkData chunk_data = {0};

  TaskParallelSettings settings;
  BLI_parallel_range_settings_defaults(&settings);
  settings.use_threading = bm->totedge >= BM_THREAD_LIMIT;
  settings.userdata_chunk = &chunk_data;
  settings.userdata_chunk_size = sizeof(chunk_data);
  settings.func_reduce = bm_mesh_select_mode_flush_reduce_fn;

  BM_iter_parallel(
      bm, BM_EDGES_OF_MESH, bm_mesh_select_mode_flush_vert_to_edge_iter_fn, nullptr, &settings);
  bm->totedgesel += chunk_data.delta_selection_len;
}

static void bm_mesh_select_mode_flush_edge_to_face(BMesh *bm)
{
  SelectionFlushChunkData chunk_data = {0};

  TaskParallelSettings settings;
  BLI_parallel_range_settings_defaults(&settings);
  settings.use_threading = bm->totface >= BM_THREAD_LIMIT;
  settings.userdata_chunk = &chunk_data;
  settings.userdata_chunk_size = sizeof(chunk_data);
  settings.func_reduce = bm_mesh_select_mode_flush_reduce_fn;

  BM_iter_parallel(
      bm, BM_FACES_OF_MESH, bm_mesh_select_mode_flush_edge_to_face_iter_fn, nullptr, &settings);
  bm->totfacesel += chunk_data.delta_selection_len;
}

/**
 * Flush down from edges to verts.
 *
 * \note This is not typically needed as settling the selection flushes down.
 */
static void bm_mesh_select_mode_flush_edge_to_vert(BMesh *bm)
{
  BMIter iter;
  BMEdge *e;
  bool any_select = false;
  BM_ITER_MESH (e, &iter, bm, BM_EDGES_OF_MESH) {
    if (BM_elem_flag_test(e, BM_ELEM_HIDDEN)) {
      continue;
    }
    if (BM_elem_flag_test(e, BM_ELEM_SELECT)) {
      any_select = true;
    }
    else {
      BM_vert_select_set(bm, e->v1, false);
      BM_vert_select_set(bm, e->v2, false);
    }
  }
  if (any_select) {
    BM_ITER_MESH (e, &iter, bm, BM_EDGES_OF_MESH) {
      if (BM_elem_flag_test(e, BM_ELEM_HIDDEN)) {
        continue;
      }
      if (BM_elem_flag_test(e, BM_ELEM_SELECT)) {
        BM_vert_select_set(bm, e->v1, true);
        BM_vert_select_set(bm, e->v2, true);
      }
    }
  }
}

/**
 * Flush down from faces to verts & edges.
 *
 * \note This is not typically needed as settling the selection flushes down.
 */
static void bm_mesh_select_mode_flush_face_to_vert_and_edge(BMesh *bm)
{
  BMIter iter;
  BMFace *f;
  bool any_select = false;
  BM_ITER_MESH (f, &iter, bm, BM_FACES_OF_MESH) {
    if (BM_elem_flag_test(f, BM_ELEM_HIDDEN)) {
      continue;
    }
    if (BM_elem_flag_test(f, BM_ELEM_SELECT)) {
      any_select = true;
    }
    else {
      BMLoop *l_first = BM_FACE_FIRST_LOOP(f);
      BMLoop *l_iter = l_first;
      do {
        BM_vert_select_set(bm, l_iter->v, false);
        BM_edge_select_set_noflush(bm, l_iter->e, false);
      } while ((l_iter = l_iter->next) != l_first);
    }
  }
  if (any_select) {
    BM_ITER_MESH (f, &iter, bm, BM_FACES_OF_MESH) {
      if (BM_elem_flag_test(f, BM_ELEM_HIDDEN)) {
        continue;
      }
      if (BM_elem_flag_test(f, BM_ELEM_SELECT)) {
        BMLoop *l_first = BM_FACE_FIRST_LOOP(f);
        BMLoop *l_iter = l_first;
        do {
          BM_vert_select_set(bm, l_iter->v, true);
          BM_edge_select_set_noflush(bm, l_iter->e, true);
        } while ((l_iter = l_iter->next) != l_first);
      }
    }
  }
}

void BM_mesh_select_mode_flush_ex(BMesh *bm, const short selectmode, BMSelectFlushFlag flag)
{
  const bool flush_down = flag_is_set(flag, BMSelectFlushFlag::Down);
  if (flush_down) {
    if (selectmode & SCE_SELECT_VERTEX) {
      /* Pass. */
    }
    else if (selectmode & SCE_SELECT_EDGE) {
      bm_mesh_select_mode_flush_edge_to_vert(bm);
    }
    else if (selectmode & SCE_SELECT_FACE) {
      bm_mesh_select_mode_flush_face_to_vert_and_edge(bm);
    }
  }

  /* Always flush up. */
  {
    if (selectmode & SCE_SELECT_VERTEX) {
      bm_mesh_select_mode_flush_vert_to_edge(bm);
    }

    if (selectmode & (SCE_SELECT_VERTEX | SCE_SELECT_EDGE)) {
      bm_mesh_select_mode_flush_edge_to_face(bm);
    }
  }

  /* Remove any deselected elements from the BMEditSelection */
  BM_select_history_validate(bm);

  if (flag_is_set(flag, BMSelectFlushFlag::RecalcLenVert)) {
    recount_totvertsel(bm);
  }
  if (flag_is_set(flag, BMSelectFlushFlag::RecalcLenEdge)) {
    recount_totedgesel(bm);
  }
  if (flag_is_set(flag, BMSelectFlushFlag::RecalcLenFace)) {
    recount_totfacesel(bm);
  }
  BLI_assert(recount_totsels_are_ok(bm));
}

void BM_mesh_select_mode_flush(BMesh *bm)
{
  BM_mesh_select_mode_flush_ex(bm, bm->selectmode, BMSelectFlushFlag_Default);
}

/** \} */

void BM_mesh_select_flush_from_verts(BMesh *bm, const bool select)
{
  if (select) {
    BMEdge *e;
    BMLoop *l_iter;
    BMLoop *l_first;
    BMFace *f;

    BMIter eiter;
    BMIter fiter;

    bool ok;

    BM_ITER_MESH (e, &eiter, bm, BM_EDGES_OF_MESH) {
      if (BM_elem_flag_test(e->v1, BM_ELEM_SELECT) && BM_elem_flag_test(e->v2, BM_ELEM_SELECT) &&
          !BM_elem_flag_test(e, BM_ELEM_HIDDEN))
      {
        BM_elem_flag_enable(e, BM_ELEM_SELECT);
      }
    }
    BM_ITER_MESH (f, &fiter, bm, BM_FACES_OF_MESH) {
      ok = true;
      if (!BM_elem_flag_test(f, BM_ELEM_HIDDEN)) {
        l_iter = l_first = BM_FACE_FIRST_LOOP(f);
        do {
          if (!BM_elem_flag_test(l_iter->v, BM_ELEM_SELECT)) {
            ok = false;
            break;
          }
        } while ((l_iter = l_iter->next) != l_first);
      }
      else {
        ok = false;
      }

      if (ok) {
        BM_elem_flag_enable(f, BM_ELEM_SELECT);
      }
    }
  }
  else {
    BMIter eiter;
    BMEdge *e;

    BM_ITER_MESH (e, &eiter, bm, BM_EDGES_OF_MESH) {
      if (!BM_elem_flag_test(e, BM_ELEM_HIDDEN)) {
        if (BM_elem_flag_test(e, BM_ELEM_SELECT)) {
          if (!BM_elem_flag_test(e->v1, BM_ELEM_SELECT) ||
              !BM_elem_flag_test(e->v2, BM_ELEM_SELECT))
          {
            BM_elem_flag_disable(e, BM_ELEM_SELECT);
          }
        }

        if (e->l && !BM_elem_flag_test(e, BM_ELEM_SELECT)) {
          BMLoop *l_iter;
          BMLoop *l_first;

          l_iter = l_first = e->l;
          do {
            BM_elem_flag_disable(l_iter->f, BM_ELEM_SELECT);
          } while ((l_iter = l_iter->radial_next) != l_first);
        }
      }
    }
    /* Remove any deselected elements from the #BMEditSelection. */
    BM_select_history_validate(bm);
  }

  recount_totsels(bm);
}

void BM_vert_select_set(BMesh *bm, BMVert *v, const bool select)
{
  BLI_assert(v->head.htype == BM_VERT);

  if (BM_elem_flag_test(v, BM_ELEM_HIDDEN)) {
    return;
  }

  if (select) {
    if (!BM_elem_flag_test(v, BM_ELEM_SELECT)) {
      BM_elem_flag_enable(v, BM_ELEM_SELECT);
      bm->totvertsel += 1;
    }
  }
  else {
    if (BM_elem_flag_test(v, BM_ELEM_SELECT)) {
      bm->totvertsel -= 1;
      BM_elem_flag_disable(v, BM_ELEM_SELECT);
    }
  }
}

void BM_edge_select_set(BMesh *bm, BMEdge *e, const bool select)
{
  BLI_assert(e->head.htype == BM_EDGE);

  if (BM_elem_flag_test(e, BM_ELEM_HIDDEN)) {
    return;
  }

  if (select) {
    if (!BM_elem_flag_test(e, BM_ELEM_SELECT)) {
      BM_elem_flag_enable(e, BM_ELEM_SELECT);
      bm->totedgesel += 1;
    }
    BM_vert_select_set(bm, e->v1, true);
    BM_vert_select_set(bm, e->v2, true);
  }
  else {
    if (BM_elem_flag_test(e, BM_ELEM_SELECT)) {
      BM_elem_flag_disable(e, BM_ELEM_SELECT);
      bm->totedgesel -= 1;
    }

    if ((bm->selectmode & SCE_SELECT_VERTEX) == 0) {
      int i;

      /* check if the vert is used by a selected edge */
      for (i = 0; i < 2; i++) {
        BMVert *v = *((&e->v1) + i);
        if (bm_vert_is_edge_select_any_other(v, e) == false) {
          BM_vert_select_set(bm, v, false);
        }
      }
    }
    else {
      BM_vert_select_set(bm, e->v1, false);
      BM_vert_select_set(bm, e->v2, false);
    }
  }
}

void BM_face_select_set(BMesh *bm, BMFace *f, const bool select)
{
  BMLoop *l_iter;
  BMLoop *l_first;

  BLI_assert(f->head.htype == BM_FACE);

  if (BM_elem_flag_test(f, BM_ELEM_HIDDEN)) {
    return;
  }

  if (select) {
    if (!BM_elem_flag_test(f, BM_ELEM_SELECT)) {
      BM_elem_flag_enable(f, BM_ELEM_SELECT);
      bm->totfacesel += 1;
    }

    l_iter = l_first = BM_FACE_FIRST_LOOP(f);
    do {
      BM_vert_select_set(bm, l_iter->v, true);
      BM_edge_select_set(bm, l_iter->e, true);
    } while ((l_iter = l_iter->next) != l_first);
  }
  else {

    if (BM_elem_flag_test(f, BM_ELEM_SELECT)) {
      BM_elem_flag_disable(f, BM_ELEM_SELECT);
      bm->totfacesel -= 1;
    }
    /**
     * \note This allows a temporarily invalid state - where for eg
     * an edge bay be de-selected, but an adjacent face remains selected.
     *
     * Rely on #BM_mesh_select_mode_flush to correct these cases.
     *
     * \note flushing based on mode, see #46494
     */
    if (bm->selectmode & SCE_SELECT_VERTEX) {
      l_iter = l_first = BM_FACE_FIRST_LOOP(f);
      do {
        BM_vert_select_set(bm, l_iter->v, false);
        BM_edge_select_set_noflush(bm, l_iter->e, false);
      } while ((l_iter = l_iter->next) != l_first);
    }
    else {
      /**
       * \note use #BM_edge_select_set_noflush,
       * vertex flushing is handled last.
       */
      if (bm->selectmode & SCE_SELECT_EDGE) {
        l_iter = l_first = BM_FACE_FIRST_LOOP(f);
        do {
          BM_edge_select_set_noflush(bm, l_iter->e, false);
        } while ((l_iter = l_iter->next) != l_first);
      }
      else {
        l_iter = l_first = BM_FACE_FIRST_LOOP(f);
        do {
          if (bm_edge_is_face_select_any_other(l_iter) == false) {
            BM_edge_select_set_noflush(bm, l_iter->e, false);
          }
        } while ((l_iter = l_iter->next) != l_first);
      }

      /* flush down to verts */
      l_iter = l_first = BM_FACE_FIRST_LOOP(f);
      do {
        if (bm_vert_is_edge_select_any_other(l_iter->v, l_iter->e) == false) {
          BM_vert_select_set(bm, l_iter->v, false);
        }
      } while ((l_iter = l_iter->next) != l_first);
    }
  }
}

/* -------------------------------------------------------------------- */
/** \name Non Flushing Versions Element Selection
 * \{ */

void BM_edge_select_set_noflush(BMesh *bm, BMEdge *e, const bool select)
{
  BLI_assert(e->head.htype == BM_EDGE);

  if (BM_elem_flag_test(e, BM_ELEM_HIDDEN)) {
    return;
  }

  if (select) {
    if (!BM_elem_flag_test(e, BM_ELEM_SELECT)) {
      BM_elem_flag_enable(e, BM_ELEM_SELECT);
      bm->totedgesel += 1;
    }
  }
  else {
    if (BM_elem_flag_test(e, BM_ELEM_SELECT)) {
      BM_elem_flag_disable(e, BM_ELEM_SELECT);
      bm->totedgesel -= 1;
    }
  }
}

void BM_face_select_set_noflush(BMesh *bm, BMFace *f, const bool select)
{
  BLI_assert(f->head.htype == BM_FACE);

  if (BM_elem_flag_test(f, BM_ELEM_HIDDEN)) {
    return;
  }

  if (select) {
    if (!BM_elem_flag_test(f, BM_ELEM_SELECT)) {
      BM_elem_flag_enable(f, BM_ELEM_SELECT);
      bm->totfacesel += 1;
    }
  }
  else {
    if (BM_elem_flag_test(f, BM_ELEM_SELECT)) {
      BM_elem_flag_disable(f, BM_ELEM_SELECT);
      bm->totfacesel -= 1;
    }
  }
}

/** \} */

void BM_mesh_select_mode_set(BMesh *bm, int selectmode)
{
  BMIter iter;
  BMElem *ele;

  bm->selectmode = selectmode;

  if (bm->selectmode & SCE_SELECT_VERTEX) {
/* disabled because selection flushing handles these */
#if 0
    BM_ITER_MESH (ele, &iter, bm, BM_EDGES_OF_MESH) {
      BM_elem_flag_disable(ele, BM_ELEM_SELECT);
    }
    BM_ITER_MESH (ele, &iter, bm, BM_FACES_OF_MESH) {
      BM_elem_flag_disable(ele, BM_ELEM_SELECT);
    }
#endif
    BM_mesh_select_mode_flush(bm);
  }
  else if (bm->selectmode & SCE_SELECT_EDGE) {
/* disabled because selection flushing handles these */
#if 0
    BM_ITER_MESH (ele, &iter, bm, BM_VERTS_OF_MESH) {
      BM_elem_flag_disable(ele, BM_ELEM_SELECT);
    }
#endif

    BM_ITER_MESH (ele, &iter, bm, BM_EDGES_OF_MESH) {
      if (BM_elem_flag_test(ele, BM_ELEM_SELECT)) {
        BM_edge_select_set(bm, (BMEdge *)ele, true);
      }
    }
    BM_mesh_select_mode_flush(bm);
  }
  else if (bm->selectmode & SCE_SELECT_FACE) {
/* disabled because selection flushing handles these */
#if 0
    BM_ITER_MESH (ele, &iter, bm, BM_EDGES_OF_MESH) {
      BM_elem_flag_disable(ele, BM_ELEM_SELECT);
    }
#endif
    BM_ITER_MESH (ele, &iter, bm, BM_FACES_OF_MESH) {
      if (BM_elem_flag_test(ele, BM_ELEM_SELECT)) {
        BM_face_select_set(bm, (BMFace *)ele, true);
      }
    }
    BM_mesh_select_mode_flush(bm);
  }
}

/**
 * counts number of elements with flag enabled/disabled
 */
static int bm_mesh_flag_count(BMesh *bm,
                              const char htype,
                              const char hflag,
                              const bool respecthide,
                              const bool test_for_enabled)
{
  BMElem *ele;
  BMIter iter;
  int tot = 0;

  BLI_assert((htype & ~BM_ALL_NOLOOP) == 0);

  if (htype & BM_VERT) {
    BM_ITER_MESH (ele, &iter, bm, BM_VERTS_OF_MESH) {
      if (respecthide && BM_elem_flag_test(ele, BM_ELEM_HIDDEN)) {
        continue;
      }
      if (BM_elem_flag_test_bool(ele, hflag) == test_for_enabled) {
        tot++;
      }
    }
  }
  if (htype & BM_EDGE) {
    BM_ITER_MESH (ele, &iter, bm, BM_EDGES_OF_MESH) {
      if (respecthide && BM_elem_flag_test(ele, BM_ELEM_HIDDEN)) {
        continue;
      }
      if (BM_elem_flag_test_bool(ele, hflag) == test_for_enabled) {
        tot++;
      }
    }
  }
  if (htype & BM_FACE) {
    BM_ITER_MESH (ele, &iter, bm, BM_FACES_OF_MESH) {
      if (respecthide && BM_elem_flag_test(ele, BM_ELEM_HIDDEN)) {
        continue;
      }
      if (BM_elem_flag_test_bool(ele, hflag) == test_for_enabled) {
        tot++;
      }
    }
  }

  return tot;
}

int BM_mesh_elem_hflag_count_enabled(BMesh *bm,
                                     const char htype,
                                     const char hflag,
                                     const bool respecthide)
{
  return bm_mesh_flag_count(bm, htype, hflag, respecthide, true);
}

int BM_mesh_elem_hflag_count_disabled(BMesh *bm,
                                      const char htype,
                                      const char hflag,
                                      const bool respecthide)
{
  return bm_mesh_flag_count(bm, htype, hflag, respecthide, false);
}

void BM_elem_select_set(BMesh *bm, BMElem *ele, const bool select)
{
  switch (ele->head.htype) {
    case BM_VERT:
      BM_vert_select_set(bm, (BMVert *)ele, select);
      break;
    case BM_EDGE:
      BM_edge_select_set(bm, (BMEdge *)ele, select);
      break;
    case BM_FACE:
      BM_face_select_set(bm, (BMFace *)ele, select);
      break;
    default:
      BLI_assert(0);
      break;
  }
}

void BM_mesh_active_face_set(BMesh *bm, BMFace *f)
{
  bm->act_face = f;
}

int BM_mesh_active_face_index_get(BMesh *bm, bool is_sloppy, bool is_selected)
{
  const BMFace *f = BM_mesh_active_face_get(bm, is_sloppy, is_selected);
  return f ? BM_elem_index_get(f) : -1;
}

int BM_mesh_active_edge_index_get(BMesh *bm)
{
  const BMEdge *e = BM_mesh_active_edge_get(bm);
  return e ? BM_elem_index_get(e) : -1;
}

int BM_mesh_active_vert_index_get(BMesh *bm)
{
  const BMVert *v = BM_mesh_active_vert_get(bm);
  return v ? BM_elem_index_get(v) : -1;
}

BMFace *BM_mesh_active_face_get(BMesh *bm, const bool is_sloppy, const bool is_selected)
{
  if (bm->act_face && (!is_selected || BM_elem_flag_test(bm->act_face, BM_ELEM_SELECT))) {
    return bm->act_face;
  }
  if (is_sloppy) {
    BMIter iter;
    BMFace *f = nullptr;
    BMEditSelection *ese;

    /* Find the latest non-hidden face from the BMEditSelection */
    ese = static_cast<BMEditSelection *>(bm->selected.last);
    for (; ese; ese = ese->prev) {
      if (ese->htype == BM_FACE) {
        f = (BMFace *)ese->ele;

        if (BM_elem_flag_test(f, BM_ELEM_HIDDEN)) {
          f = nullptr;
        }
        else if (is_selected && !BM_elem_flag_test(f, BM_ELEM_SELECT)) {
          f = nullptr;
        }
        else {
          break;
        }
      }
    }
    /* Last attempt: try to find any selected face */
    if (f == nullptr) {
      BM_ITER_MESH (f, &iter, bm, BM_FACES_OF_MESH) {
        if (BM_elem_flag_test(f, BM_ELEM_SELECT)) {
          break;
        }
      }
    }
    return f; /* can still be null */
  }
  return nullptr;
}

BMEdge *BM_mesh_active_edge_get(BMesh *bm)
{
  if (bm->selected.last) {
    BMEditSelection *ese = static_cast<BMEditSelection *>(bm->selected.last);

    if (ese && ese->htype == BM_EDGE) {
      return (BMEdge *)ese->ele;
    }
  }

  return nullptr;
}

BMVert *BM_mesh_active_vert_get(BMesh *bm)
{
  if (bm->selected.last) {
    BMEditSelection *ese = static_cast<BMEditSelection *>(bm->selected.last);

    if (ese && ese->htype == BM_VERT) {
      return (BMVert *)ese->ele;
    }
  }

  return nullptr;
}

BMElem *BM_mesh_active_elem_get(BMesh *bm)
{
  if (bm->selected.last) {
    BMEditSelection *ese = static_cast<BMEditSelection *>(bm->selected.last);

    if (ese) {
      return ese->ele;
    }
  }

  return nullptr;
}

void BM_editselection_center(BMEditSelection *ese, float r_center[3])
{
  if (ese->htype == BM_VERT) {
    BMVert *eve = (BMVert *)ese->ele;
    copy_v3_v3(r_center, eve->co);
  }
  else if (ese->htype == BM_EDGE) {
    BMEdge *eed = (BMEdge *)ese->ele;
    mid_v3_v3v3(r_center, eed->v1->co, eed->v2->co);
  }
  else if (ese->htype == BM_FACE) {
    BMFace *efa = (BMFace *)ese->ele;
    BM_face_calc_center_median(efa, r_center);
  }
}

void BM_editselection_normal(BMEditSelection *ese, float r_normal[3])
{
  if (ese->htype == BM_VERT) {
    BMVert *eve = (BMVert *)ese->ele;
    copy_v3_v3(r_normal, eve->no);
  }
  else if (ese->htype == BM_EDGE) {
    BMEdge *eed = (BMEdge *)ese->ele;
    float plane[3]; /* need a plane to correct the normal */
    float vec[3];   /* temp vec storage */

    add_v3_v3v3(r_normal, eed->v1->no, eed->v2->no);
    sub_v3_v3v3(plane, eed->v2->co, eed->v1->co);

    /* the 2 vertex normals will be close but not at right angles to the edge
     * for rotate about edge we want them to be at right angles, so we need to
     * do some extra calculation to correct the vert normals,
     * we need the plane for this */
    cross_v3_v3v3(vec, r_normal, plane);
    cross_v3_v3v3(r_normal, plane, vec);
    normalize_v3(r_normal);
  }
  else if (ese->htype == BM_FACE) {
    BMFace *efa = (BMFace *)ese->ele;
    copy_v3_v3(r_normal, efa->no);
  }
}

void BM_editselection_plane(BMEditSelection *ese, float r_plane[3])
{
  if (ese->htype == BM_VERT) {
    BMVert *eve = (BMVert *)ese->ele;
    float vec[3] = {0.0f, 0.0f, 0.0f};

    if (ese->prev) { /* use previously selected data to make a useful vertex plane */
      BM_editselection_center(ese->prev, vec);
      sub_v3_v3v3(r_plane, vec, eve->co);
    }
    else {
      /* make a fake plane that's at right-angles to the normal
       * we can't make a cross-vector from a vec that's the same as the vec
       * unlikely but possible, so make sure if the normal is (0, 0, 1)
       * that vec isn't the same or in the same direction even. */
      if (eve->no[0] < 0.5f) {
        vec[0] = 1.0f;
      }
      else if (eve->no[1] < 0.5f) {
        vec[1] = 1.0f;
      }
      else {
        vec[2] = 1.0f;
      }
      cross_v3_v3v3(r_plane, eve->no, vec);
    }
    normalize_v3(r_plane);
  }
  else if (ese->htype == BM_EDGE) {
    BMEdge *eed = (BMEdge *)ese->ele;

    if (BM_edge_is_boundary(eed)) {
      sub_v3_v3v3(r_plane, eed->l->v->co, eed->l->next->v->co);
    }
    else {
      /* the plane is simple, it runs along the edge
       * however selecting different edges can swap the direction of the y axis.
       * this makes it less likely for the y axis of the gizmo
       * (running along the edge).. to flip less often.
       * at least its more predictable */
      if (eed->v2->co[1] > eed->v1->co[1]) { /* check which to do first */
        sub_v3_v3v3(r_plane, eed->v2->co, eed->v1->co);
      }
      else {
        sub_v3_v3v3(r_plane, eed->v1->co, eed->v2->co);
      }
    }

    normalize_v3(r_plane);
  }
  else if (ese->htype == BM_FACE) {
    BMFace *efa = (BMFace *)ese->ele;
    BM_face_calc_tangent_auto(efa, r_plane);
  }
}

static BMEditSelection *bm_select_history_create(BMHeader *ele)
{
  BMEditSelection *ese = MEM_callocN<BMEditSelection>("BMEdit Selection");
  ese->htype = ele->htype;
  ese->ele = (BMElem *)ele;
  return ese;
}

/* --- Macro wrapped functions. --- */

bool _bm_select_history_check(BMesh *bm, const BMHeader *ele)
{
  return (BLI_findptr(&bm->selected, ele, offsetof(BMEditSelection, ele)) != nullptr);
}

bool _bm_select_history_remove(BMesh *bm, BMHeader *ele)
{
  BMEditSelection *ese = static_cast<BMEditSelection *>(
      BLI_findptr(&bm->selected, ele, offsetof(BMEditSelection, ele)));
  if (ese) {
    BLI_freelinkN(&bm->selected, ese);
    return true;
  }
  return false;
}

void _bm_select_history_store_notest(BMesh *bm, BMHeader *ele)
{
  BMEditSelection *ese = bm_select_history_create(ele);
  BLI_addtail(&(bm->selected), ese);
}

void _bm_select_history_store_head_notest(BMesh *bm, BMHeader *ele)
{
  BMEditSelection *ese = bm_select_history_create(ele);
  BLI_addhead(&(bm->selected), ese);
}

void _bm_select_history_store(BMesh *bm, BMHeader *ele)
{
  if (!BM_select_history_check(bm, (BMElem *)ele)) {
    BM_select_history_store_notest(bm, (BMElem *)ele);
  }
}

void _bm_select_history_store_head(BMesh *bm, BMHeader *ele)
{
  if (!BM_select_history_check(bm, (BMElem *)ele)) {
    BM_select_history_store_head_notest(bm, (BMElem *)ele);
  }
}

void _bm_select_history_store_after_notest(BMesh *bm, BMEditSelection *ese_ref, BMHeader *ele)
{
  BMEditSelection *ese = bm_select_history_create(ele);
  BLI_insertlinkafter(&(bm->selected), ese_ref, ese);
}

void _bm_select_history_store_after(BMesh *bm, BMEditSelection *ese_ref, BMHeader *ele)
{
  if (!BM_select_history_check(bm, (BMElem *)ele)) {
    BM_select_history_store_after_notest(bm, ese_ref, (BMElem *)ele);
  }
}
/* --- End macro wrapped functions --- */

void BM_select_history_clear(BMesh *bm)
{
  BLI_freelistN(&bm->selected);
}

void BM_select_history_validate(BMesh *bm)
{
  BMEditSelection *ese, *ese_next;

  for (ese = static_cast<BMEditSelection *>(bm->selected.first); ese; ese = ese_next) {
    ese_next = ese->next;
    if (!BM_elem_flag_test(ese->ele, BM_ELEM_SELECT)) {
      BLI_freelinkN(&(bm->selected), ese);
    }
  }
}

char BM_select_history_htype_all(const BMesh *bm)
{
  char htype_selected = 0;
  LISTBASE_FOREACH (const BMEditSelection *, ese, &bm->selected) {
    htype_selected |= ese->htype;
    /* Early exit if all types found. */
    if (htype_selected == (BM_VERT | BM_EDGE | BM_FACE)) {
      break;
    }
  }
  return htype_selected;
}

bool BM_select_history_active_get(BMesh *bm, BMEditSelection *ese)
{
  BMEditSelection *ese_last = static_cast<BMEditSelection *>(bm->selected.last);
  BMFace *efa = BM_mesh_active_face_get(bm, false, true);

  ese->next = ese->prev = nullptr;

  if (ese_last) {
    /* If there is an active face, use it over the last selected face. */
    if (ese_last->htype == BM_FACE) {
      if (efa) {
        ese->ele = (BMElem *)efa;
      }
      else {
        ese->ele = ese_last->ele;
      }
      ese->htype = BM_FACE;
    }
    else {
      ese->ele = ese_last->ele;
      ese->htype = ese_last->htype;
    }
  }
  else if (efa) {
    /* no edit-selection, fall back to active face */
    ese->ele = (BMElem *)efa;
    ese->htype = BM_FACE;
  }
  else {
    ese->ele = nullptr;
    return false;
  }

  return true;
}

GHash *BM_select_history_map_create(BMesh *bm)
{
  if (BLI_listbase_is_empty(&bm->selected)) {
    return nullptr;
  }

  GHash *map = BLI_ghash_ptr_new(__func__);

  LISTBASE_FOREACH (BMEditSelection *, ese, &bm->selected) {
    BLI_ghash_insert(map, ese->ele, ese);
  }

  return map;
}

void BM_select_history_merge_from_targetmap(
    BMesh *bm, GHash *vert_map, GHash *edge_map, GHash *face_map, const bool use_chain)
{

#ifndef NDEBUG
  LISTBASE_FOREACH (BMEditSelection *, ese, &bm->selected) {
    BLI_assert(BM_ELEM_API_FLAG_TEST(ese->ele, _FLAG_OVERLAP) == 0);
  }
#endif

  LISTBASE_FOREACH (BMEditSelection *, ese, &bm->selected) {
    BM_ELEM_API_FLAG_ENABLE(ese->ele, _FLAG_OVERLAP);

    /* Only loop when (use_chain == true). */
    GHash *map = nullptr;
    switch (ese->ele->head.htype) {
      case BM_VERT:
        map = vert_map;
        break;
      case BM_EDGE:
        map = edge_map;
        break;
      case BM_FACE:
        map = face_map;
        break;
      default:
        BMESH_ASSERT(0);
        break;
    }
    if (map != nullptr) {
      BMElem *ele_dst = ese->ele;
      while (true) {
        BMElem *ele_dst_next = static_cast<BMElem *>(BLI_ghash_lookup(map, ele_dst));
        BLI_assert(ele_dst != ele_dst_next);
        if (ele_dst_next == nullptr) {
          break;
        }
        ele_dst = ele_dst_next;
        /* Break loop on circular reference (should never happen). */
        if (UNLIKELY(ele_dst == ese->ele)) {
          BLI_assert(0);
          break;
        }
        if (use_chain == false) {
          break;
        }
      }
      ese->ele = ele_dst;
    }
  }

  /* Remove overlapping duplicates. */
  for (BMEditSelection *ese = static_cast<BMEditSelection *>(bm->selected.first), *ese_next; ese;
       ese = ese_next)
  {
    ese_next = ese->next;
    if (BM_ELEM_API_FLAG_TEST(ese->ele, _FLAG_OVERLAP)) {
      BM_ELEM_API_FLAG_DISABLE(ese->ele, _FLAG_OVERLAP);
    }
    else {
      BLI_freelinkN(&bm->selected, ese);
    }
  }
}

void BM_mesh_elem_hflag_disable_test(BMesh *bm,
                                     const char htype,
                                     const char hflag,
                                     const bool respecthide,
                                     const bool overwrite,
                                     const char hflag_test)
{
  const char iter_types[3] = {BM_VERTS_OF_MESH, BM_EDGES_OF_MESH, BM_FACES_OF_MESH};

  const char flag_types[3] = {BM_VERT, BM_EDGE, BM_FACE};

  const char hflag_nosel = hflag & ~BM_ELEM_SELECT;

  int i;

  BLI_assert((htype & ~BM_ALL_NOLOOP) == 0);

  if (hflag & BM_ELEM_SELECT) {
    BM_select_history_clear(bm);
  }

  if ((htype == (BM_VERT | BM_EDGE | BM_FACE)) && (hflag == BM_ELEM_SELECT) &&
      (respecthide == false) && (hflag_test == 0))
  {
    /* fast path for deselect all, avoid topology loops
     * since we know all will be de-selected anyway. */
    for (i = 0; i < 3; i++) {
      BMIter iter;
      BMElem *ele;

      ele = static_cast<BMElem *>(BM_iter_new(&iter, bm, iter_types[i], nullptr));
      for (; ele; ele = static_cast<BMElem *>(BM_iter_step(&iter))) {
        BM_elem_flag_disable(ele, BM_ELEM_SELECT);
      }
    }

    bm->totvertsel = bm->totedgesel = bm->totfacesel = 0;
  }
  else {
    for (i = 0; i < 3; i++) {
      BMIter iter;
      BMElem *ele;

      if (htype & flag_types[i]) {
        ele = static_cast<BMElem *>(BM_iter_new(&iter, bm, iter_types[i], nullptr));
        for (; ele; ele = static_cast<BMElem *>(BM_iter_step(&iter))) {

          if (UNLIKELY(respecthide && BM_elem_flag_test(ele, BM_ELEM_HIDDEN))) {
            /* pass */
          }
          else if (!hflag_test || BM_elem_flag_test(ele, hflag_test)) {
            if (hflag & BM_ELEM_SELECT) {
              BM_elem_select_set(bm, ele, false);
            }
            BM_elem_flag_disable(ele, hflag);
          }
          else if (overwrite) {
            /* no match! */
            if (hflag & BM_ELEM_SELECT) {
              BM_elem_select_set(bm, ele, true);
            }
            BM_elem_flag_enable(ele, hflag_nosel);
          }
        }
      }
    }
  }
}

void BM_mesh_elem_hflag_enable_test(BMesh *bm,
                                    const char htype,
                                    const char hflag,
                                    const bool respecthide,
                                    const bool overwrite,
                                    const char hflag_test)
{
  const char iter_types[3] = {BM_VERTS_OF_MESH, BM_EDGES_OF_MESH, BM_FACES_OF_MESH};

  const char flag_types[3] = {BM_VERT, BM_EDGE, BM_FACE};

  /* use the nosel version when setting so under no
   * condition may a hidden face become selected.
   * Applying other flags to hidden faces is OK. */
  const char hflag_nosel = hflag & ~BM_ELEM_SELECT;

  BMIter iter;
  BMElem *ele;
  int i;

  BLI_assert((htype & ~BM_ALL_NOLOOP) == 0);

  /* NOTE: better not attempt a fast path for selection as done with de-select
   * because hidden geometry and different selection modes can give different results,
   * we could of course check for no hidden faces and then use
   * quicker method but its not worth it. */

  for (i = 0; i < 3; i++) {
    if (htype & flag_types[i]) {
      ele = static_cast<BMElem *>(BM_iter_new(&iter, bm, iter_types[i], nullptr));
      for (; ele; ele = static_cast<BMElem *>(BM_iter_step(&iter))) {

        if (UNLIKELY(respecthide && BM_elem_flag_test(ele, BM_ELEM_HIDDEN))) {
          /* pass */
        }
        else if (!hflag_test || BM_elem_flag_test(ele, hflag_test)) {
          /* match! */
          if (hflag & BM_ELEM_SELECT) {
            BM_elem_select_set(bm, ele, true);
          }
          BM_elem_flag_enable(ele, hflag_nosel);
        }
        else if (overwrite) {
          /* no match! */
          if (hflag & BM_ELEM_SELECT) {
            BM_elem_select_set(bm, ele, false);
          }
          BM_elem_flag_disable(ele, hflag);
        }
      }
    }
  }
}

void BM_mesh_elem_hflag_disable_all(BMesh *bm,
                                    const char htype,
                                    const char hflag,
                                    const bool respecthide)
{
  /* call with 0 hflag_test */
  BM_mesh_elem_hflag_disable_test(bm, htype, hflag, respecthide, false, 0);
}

void BM_mesh_elem_hflag_enable_all(BMesh *bm,
                                   const char htype,
                                   const char hflag,
                                   const bool respecthide)
{
  /* call with 0 hflag_test */
  BM_mesh_elem_hflag_enable_test(bm, htype, hflag, respecthide, false, 0);
}

/***************** Mesh Hiding stuff *********** */

/**
 * Hide unless any connected elements are visible.
 * Run this after hiding a connected edge or face.
 */
static void vert_flush_hide_set(BMVert *v)
{
  BM_elem_flag_set(v, BM_ELEM_HIDDEN, !bm_vert_is_edge_visible_any(v));
}

/**
 * Hide unless any connected elements are visible.
 * Run this after hiding a connected face.
 */
static void edge_flush_hide_set(BMEdge *e)
{
  BM_elem_flag_set(e, BM_ELEM_HIDDEN, !bm_edge_is_face_visible_any(e));
}

void BM_vert_hide_set(BMVert *v, const bool hide)
{
  /* vert hiding: vert + surrounding edges and faces */
  BLI_assert(v->head.htype == BM_VERT);
  if (hide) {
    BLI_assert(!BM_elem_flag_test(v, BM_ELEM_SELECT));
  }

  BM_elem_flag_set(v, BM_ELEM_HIDDEN, hide);

  if (v->e) {
    BMEdge *e_iter, *e_first;
    e_iter = e_first = v->e;
    do {
      BM_elem_flag_set(e_iter, BM_ELEM_HIDDEN, hide);
      if (e_iter->l) {
        const BMLoop *l_radial_iter, *l_radial_first;
        l_radial_iter = l_radial_first = e_iter->l;
        do {
          BM_elem_flag_set(l_radial_iter->f, BM_ELEM_HIDDEN, hide);
        } while ((l_radial_iter = l_radial_iter->radial_next) != l_radial_first);
      }
    } while ((e_iter = bmesh_disk_edge_next(e_iter, v)) != e_first);
  }
}

void BM_edge_hide_set(BMEdge *e, const bool hide)
{
  BLI_assert(e->head.htype == BM_EDGE);
  if (hide) {
    BLI_assert(!BM_elem_flag_test(e, BM_ELEM_SELECT));
  }

  /* edge hiding: faces around the edge */
  if (e->l) {
    const BMLoop *l_iter, *l_first;
    l_iter = l_first = e->l;
    do {
      BM_elem_flag_set(l_iter->f, BM_ELEM_HIDDEN, hide);
    } while ((l_iter = l_iter->radial_next) != l_first);
  }

  BM_elem_flag_set(e, BM_ELEM_HIDDEN, hide);

  /* hide vertices if necessary */
  if (hide) {
    vert_flush_hide_set(e->v1);
    vert_flush_hide_set(e->v2);
  }
  else {
    BM_elem_flag_disable(e->v1, BM_ELEM_HIDDEN);
    BM_elem_flag_disable(e->v2, BM_ELEM_HIDDEN);
  }
}

void BM_face_hide_set(BMFace *f, const bool hide)
{
  BLI_assert(f->head.htype == BM_FACE);
  if (hide) {
    BLI_assert(!BM_elem_flag_test(f, BM_ELEM_SELECT));
  }

  BM_elem_flag_set(f, BM_ELEM_HIDDEN, hide);

  if (hide) {
    BMLoop *l_first = BM_FACE_FIRST_LOOP(f);
    BMLoop *l_iter;

    l_iter = l_first;
    do {
      edge_flush_hide_set(l_iter->e);
    } while ((l_iter = l_iter->next) != l_first);

    l_iter = l_first;
    do {
      vert_flush_hide_set(l_iter->v);
    } while ((l_iter = l_iter->next) != l_first);
  }
  else {
    BMLoop *l_first = BM_FACE_FIRST_LOOP(f);
    BMLoop *l_iter;

    l_iter = l_first;
    do {
      BM_elem_flag_disable(l_iter->e, BM_ELEM_HIDDEN);
      BM_elem_flag_disable(l_iter->v, BM_ELEM_HIDDEN);
    } while ((l_iter = l_iter->next) != l_first);
  }
}

void _bm_elem_hide_set(BMesh *bm, BMHeader *head, const bool hide)
{
  /* Follow convention of always deselecting before
   * hiding an element */
  switch (head->htype) {
    case BM_VERT:
      if (hide) {
        BM_vert_select_set(bm, (BMVert *)head, false);
      }
      BM_vert_hide_set((BMVert *)head, hide);
      break;
    case BM_EDGE:
      if (hide) {
        BM_edge_select_set(bm, (BMEdge *)head, false);
      }
      BM_edge_hide_set((BMEdge *)head, hide);
      break;
    case BM_FACE:
      if (hide) {
        BM_face_select_set(bm, (BMFace *)head, false);
      }
      BM_face_hide_set((BMFace *)head, hide);
      break;
    default:
      BMESH_ASSERT(0);
      break;
  }
}
