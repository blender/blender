/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bmesh
 */

#include "MEM_guardedalloc.h"

#include "DNA_scene_types.h"

#include "BLI_listbase.h"
#include "BLI_math_bits.h"

#include "bmesh.hh"
#include "bmesh_structure.hh"

/* -------------------------------------------------------------------- */
/** \name Internal Utilities
 * \{ */

static void bm_mesh_uvselect_disable_all(BMesh *bm)
{
  /* In practically all cases it's best to check #BM_ELEM_HIDDEN
   * In this case the intent is to re-generate the selection, so clear all. */
  BMIter iter;
  BMFace *f;
  BM_ITER_MESH (f, &iter, bm, BM_FACES_OF_MESH) {
    BMLoop *l_iter, *l_first;
    l_iter = l_first = BM_FACE_FIRST_LOOP(f);
    do {
      BM_elem_flag_disable(l_iter, BM_ELEM_SELECT_UV | BM_ELEM_SELECT_UV_EDGE);
    } while ((l_iter = l_iter->next) != l_first);
    BM_elem_flag_disable(f, BM_ELEM_SELECT_UV);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name UV Selection Functions (low level)
 * \{ */

bool BM_loop_vert_uvselect_test(const BMLoop *l)
{
  return (!BM_elem_flag_test(l->f, BM_ELEM_HIDDEN) && BM_elem_flag_test(l, BM_ELEM_SELECT_UV));
}
bool BM_loop_edge_uvselect_test(const BMLoop *l)
{
  return (!BM_elem_flag_test(l->f, BM_ELEM_HIDDEN) &&
          BM_elem_flag_test(l, BM_ELEM_SELECT_UV_EDGE));
}

bool BM_face_uvselect_test(const BMFace *f)
{
  return (!BM_elem_flag_test(f, BM_ELEM_HIDDEN) && BM_elem_flag_test(f, BM_ELEM_SELECT_UV));
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name UV Selection Connectivity Checks
 * \{ */

bool BM_loop_vert_uvselect_check_other_loop_vert(BMLoop *l,
                                                 const char hflag,
                                                 const int cd_loop_uv_offset)
{
  BLI_assert(ELEM(hflag, BM_ELEM_SELECT_UV, BM_ELEM_TAG));
  BMVert *v = l->v;
  BLI_assert(v->e);
  const BMEdge *e_iter, *e_first;
  e_iter = e_first = v->e;
  do {
    if (e_iter->l == nullptr) {
      continue;
    }
    BMLoop *l_first = e_iter->l;
    BMLoop *l_iter = l_first;
    do {
      if (BM_elem_flag_test(l_iter->f, BM_ELEM_HIDDEN)) {
        continue;
      }
      if (l_iter->v != v) {
        continue;
      }
      if (l_iter != l) {
        if (BM_elem_flag_test(l_iter, hflag)) {
          if (BM_loop_uv_share_vert_check(l, l_iter, cd_loop_uv_offset)) {
            return true;
          }
        }
      }
    } while ((l_iter = l_iter->radial_next) != l_first);
  } while ((e_iter = bmesh_disk_edge_next(e_iter, v)) != e_first);
  return false;
}

bool BM_loop_vert_uvselect_check_other_loop_edge(BMLoop *l,
                                                 const char hflag,
                                                 const int cd_loop_uv_offset)
{
  BLI_assert(ELEM(hflag, BM_ELEM_SELECT_UV_EDGE, BM_ELEM_TAG));
  BMVert *v = l->v;
  BLI_assert(v->e);
  const BMEdge *e_iter, *e_first;
  e_iter = e_first = v->e;
  do {
    if (e_iter->l == nullptr) {
      continue;
    }
    BMLoop *l_first = e_iter->l;
    BMLoop *l_iter = l_first;
    do {
      if (BM_elem_flag_test(l_iter->f, BM_ELEM_HIDDEN)) {
        continue;
      }
      if (l_iter->v != v) {
        continue;
      }
      /* Connected to a selected edge. */
      if (l_iter != l) {
        if (BM_elem_flag_test(l_iter, hflag) || BM_elem_flag_test(l_iter->prev, hflag)) {
          if (BM_loop_uv_share_vert_check(l, l_iter, cd_loop_uv_offset)) {
            return true;
          }
        }
      }
    } while ((l_iter = l_iter->radial_next) != l_first);
  } while ((e_iter = bmesh_disk_edge_next(e_iter, v)) != e_first);
  return false;
}

bool BM_loop_vert_uvselect_check_other_edge(BMLoop *l,
                                            const char hflag,
                                            const int cd_loop_uv_offset)
{
  BLI_assert(ELEM(hflag, BM_ELEM_SELECT, BM_ELEM_TAG));
  BMVert *v = l->v;
  BLI_assert(v->e);
  const BMEdge *e_iter, *e_first;
  e_iter = e_first = v->e;
  do {
    if (e_iter->l == nullptr) {
      continue;
    }
    BMLoop *l_first = e_iter->l;
    BMLoop *l_iter = l_first;
    do {
      if (BM_elem_flag_test(l_iter->f, BM_ELEM_HIDDEN)) {
        continue;
      }
      if (l_iter->v != v) {
        continue;
      }
      /* Connected to a selected edge. */
      if (l_iter != l) {
        if (((!BM_elem_flag_test(l_iter->e, BM_ELEM_HIDDEN)) &&
             BM_elem_flag_test(l_iter->e, hflag)) ||
            ((!BM_elem_flag_test(l_iter->prev->e, BM_ELEM_HIDDEN)) &&
             BM_elem_flag_test(l_iter->prev->e, hflag)))
        {
          if (BM_loop_uv_share_vert_check(l, l_iter, cd_loop_uv_offset)) {
            return true;
          }
        }
      }
    } while ((l_iter = l_iter->radial_next) != l_first);
  } while ((e_iter = bmesh_disk_edge_next(e_iter, v)) != e_first);
  return false;
}

bool BM_loop_vert_uvselect_check_other_face(BMLoop *l,
                                            const char hflag,
                                            const int cd_loop_uv_offset)
{
  BLI_assert(ELEM(hflag, BM_ELEM_SELECT, BM_ELEM_SELECT_UV, BM_ELEM_TAG));
  BMVert *v = l->v;
  BLI_assert(v->e);
  const BMEdge *e_iter, *e_first;
  e_iter = e_first = v->e;
  do {
    if (e_iter->l == nullptr) {
      continue;
    }
    BMLoop *l_first = e_iter->l;
    BMLoop *l_iter = l_first;
    do {
      if (BM_elem_flag_test(l_iter->f, BM_ELEM_HIDDEN)) {
        continue;
      }
      if (l_iter->v != v) {
        continue;
      }
      if (l_iter != l) {
        if (BM_elem_flag_test(l_iter->f, hflag)) {
          if (BM_loop_uv_share_vert_check(l, l_iter, cd_loop_uv_offset)) {
            return true;
          }
        }
      }
    } while ((l_iter = l_iter->radial_next) != l_first);
  } while ((e_iter = bmesh_disk_edge_next(e_iter, v)) != e_first);
  return false;
}

bool BM_loop_edge_uvselect_check_other_loop_edge(BMLoop *l,
                                                 const char hflag,
                                                 const int cd_loop_uv_offset)
{
  BLI_assert(ELEM(hflag, BM_ELEM_SELECT, BM_ELEM_SELECT_UV_EDGE, BM_ELEM_TAG));
  BMLoop *l_iter = l;
  do {
    if (BM_elem_flag_test(l_iter->f, BM_ELEM_HIDDEN)) {
      continue;
    }
    if (l_iter != l) {
      if (BM_elem_flag_test(l_iter, hflag)) {
        if (BM_loop_uv_share_edge_check(l, l_iter, cd_loop_uv_offset)) {
          return true;
        }
      }
    }
  } while ((l_iter = l_iter->radial_next) != l);
  return false;
}

bool BM_loop_edge_uvselect_check_other_face(BMLoop *l,
                                            const char hflag,
                                            const int cd_loop_uv_offset)
{
  BLI_assert(ELEM(hflag, BM_ELEM_SELECT, BM_ELEM_SELECT_UV));
  BMLoop *l_iter = l;
  do {
    if (BM_elem_flag_test(l_iter->f, BM_ELEM_HIDDEN)) {
      continue;
    }
    if (l_iter != l) {
      if (BM_elem_flag_test(l_iter->f, hflag)) {
        if (BM_loop_uv_share_edge_check(l, l_iter, cd_loop_uv_offset)) {
          return true;
        }
      }
    }
  } while ((l_iter = l_iter->radial_next) != l);
  return false;
}

bool BM_face_uvselect_check_edges_all(BMFace *f)
{
  if (BM_elem_flag_test(f, BM_ELEM_HIDDEN)) {
    return false;
  }
  BMLoop *l_iter, *l_first;
  l_iter = l_first = BM_FACE_FIRST_LOOP(f);
  do {
    if (!BM_elem_flag_test(l_iter, BM_ELEM_SELECT_UV_EDGE)) {
      return false;
    }
  } while ((l_iter = l_iter->next) != l_first);
  return true;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name UV Selection Functions
 * \{ */

void BM_loop_vert_uvselect_set_noflush(BMesh *bm, BMLoop *l, bool select)
{
  /* Only select if it's valid, otherwise the result wont be used. */
  BLI_assert(bm->uv_select_sync_valid);
  UNUSED_VARS_NDEBUG(bm);

  /* Selecting when hidden must be prevented by the caller.
   * Allow de-selecting as this may be useful at times. */
  BLI_assert(!BM_elem_flag_test(l->f, BM_ELEM_HIDDEN) || (select == false));

  /* NOTE: don't do any flushing here as it's too expensive to walk over connected geometry.
   * These can be handled in separate operations. */
  BM_elem_flag_set(l, BM_ELEM_SELECT_UV, select);
}

void BM_loop_edge_uvselect_set_noflush(BMesh *bm, BMLoop *l, bool select)
{
  /* Only select if it's valid, otherwise the result wont be used. */
  BLI_assert(bm->uv_select_sync_valid);
  UNUSED_VARS_NDEBUG(bm);

  /* Selecting when hidden must be prevented by the caller.
   * Allow de-selecting as this may be useful at times. */
  BLI_assert(!BM_elem_flag_test(l->f, BM_ELEM_HIDDEN) || (select == false));

  /* NOTE: don't do any flushing here as it's too expensive to walk over connected geometry.
   * These can be handled in separate operations. */
  BM_elem_flag_set(l, BM_ELEM_SELECT_UV_EDGE, select);
}

void BM_loop_edge_uvselect_set(BMesh *bm, BMLoop *l, bool select)
{
  BM_loop_edge_uvselect_set_noflush(bm, l, select);

  BM_loop_vert_uvselect_set_noflush(bm, l, select);
  BM_loop_vert_uvselect_set_noflush(bm, l->next, select);
}

void BM_face_uvselect_set_noflush(BMesh *bm, BMFace *f, bool select)
{
  /* Only select if it's valid, otherwise the result wont be used. */
  BLI_assert(bm->uv_select_sync_valid);
  UNUSED_VARS_NDEBUG(bm);

  /* Selecting when hidden must be prevented by the caller.
   * Allow de-selecting as this may be useful at times. */
  BLI_assert(!BM_elem_flag_test(f, BM_ELEM_HIDDEN) || (select == false));

  /* NOTE: don't do any flushing here as it's too expensive to walk over connected geometry.
   * These can be handled in separate operations. */
  BM_elem_flag_set(f, BM_ELEM_SELECT_UV, select);
}

void BM_face_uvselect_set(BMesh *bm, BMFace *f, bool select)
{
  BM_face_uvselect_set_noflush(bm, f, select);
  BMLoop *l_iter, *l_first;
  l_iter = l_first = BM_FACE_FIRST_LOOP(f);
  do {
    BM_loop_vert_uvselect_set_noflush(bm, l_iter, select);
    BM_loop_edge_uvselect_set_noflush(bm, l_iter, select);
  } while ((l_iter = l_iter->next) != l_first);
}

bool BM_mesh_uvselect_clear(BMesh *bm)
{
  if (bm->uv_select_sync_valid == false) {
    return false;
  }
  bm->uv_select_sync_valid = false;
  return true;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name UV Selection Functions (Shared)
 * \{ */

void BM_loop_vert_uvselect_set_shared(BMesh *bm,
                                      BMLoop *l,
                                      bool select,
                                      const int cd_loop_uv_offset)
{
  BM_loop_vert_uvselect_set_noflush(bm, l, select);

  BMVert *v = l->v;
  BLI_assert(v->e);
  const BMEdge *e_iter, *e_first;
  e_iter = e_first = v->e;
  do {
    if (e_iter->l == nullptr) {
      continue;
    }
    BMLoop *l_first = e_iter->l;
    BMLoop *l_iter = l_first;
    do {
      if (BM_elem_flag_test(l_iter->f, BM_ELEM_HIDDEN)) {
        continue;
      }
      if (l_iter->v != v) {
        continue;
      }
      if (l_iter != l) {
        if (BM_elem_flag_test_bool(l_iter, BM_ELEM_SELECT_UV) != select) {
          if (BM_loop_uv_share_vert_check(l, l_iter, cd_loop_uv_offset)) {
            BM_loop_vert_uvselect_set_noflush(bm, l_iter, select);
          }
        }
      }
    } while ((l_iter = l_iter->radial_next) != l_first);
  } while ((e_iter = bmesh_disk_edge_next(e_iter, v)) != e_first);
}

void BM_loop_edge_uvselect_set_shared(BMesh *bm,
                                      BMLoop *l,
                                      bool select,
                                      const int cd_loop_uv_offset)
{
  BM_loop_edge_uvselect_set_noflush(bm, l, select);

  BMLoop *l_iter = l->radial_next;
  /* Check it's not a boundary. */
  if (l_iter != l) {
    do {
      if (BM_elem_flag_test(l_iter->f, BM_ELEM_HIDDEN)) {
        continue;
      }
      if (BM_elem_flag_test_bool(l_iter, BM_ELEM_SELECT_UV_EDGE) != select) {
        if (BM_loop_uv_share_edge_check(l, l_iter, cd_loop_uv_offset)) {
          BM_loop_edge_uvselect_set_noflush(bm, l_iter, select);
        }
      }
    } while ((l_iter = l_iter->radial_next) != l);
  }
}

void BM_face_uvselect_set_shared(BMesh *bm, BMFace *f, bool select, const int cd_loop_uv_offset)
{
  BM_face_uvselect_set_noflush(bm, f, select);
  BMLoop *l_iter, *l_first;
  l_iter = l_first = BM_FACE_FIRST_LOOP(f);
  do {
    BM_loop_vert_uvselect_set_shared(bm, l_iter, select, cd_loop_uv_offset);
    BM_loop_edge_uvselect_set_shared(bm, l_iter, select, cd_loop_uv_offset);
  } while ((l_iter = l_iter->next) != l_first);
}

void BM_mesh_uvselect_set_elem_shared(BMesh *bm,
                                      bool select,
                                      const int cd_loop_uv_offset,
                                      const blender::Span<BMLoop *> loop_verts,
                                      const blender::Span<BMLoop *> loop_edges,
                                      const blender::Span<BMFace *> faces)
{
  /* TODO: this could be optimized to reduce traversal of connected UV's for every element. */

  for (BMLoop *l_vert : loop_verts) {
    BM_loop_vert_uvselect_set_shared(bm, l_vert, select, cd_loop_uv_offset);
  }
  for (BMLoop *l_edge : loop_edges) {
    BM_loop_edge_uvselect_set_shared(bm, l_edge, select, cd_loop_uv_offset);

    if (select) {
      BM_loop_vert_uvselect_set_shared(bm, l_edge, select, cd_loop_uv_offset);
      BM_loop_vert_uvselect_set_shared(bm, l_edge->next, select, cd_loop_uv_offset);
    }
  }
  for (BMFace *f : faces) {
    if (select) {
      BM_face_uvselect_set_shared(bm, f, select, cd_loop_uv_offset);
    }
    else {
      BM_face_uvselect_set_noflush(bm, f, select);
    }
  }

  /* Only de-select shared elements if they are no longer connected to a selection. */
  if (!select) {
    for (BMLoop *l_edge : loop_edges) {
      if (BM_elem_flag_test(l_edge->f, BM_ELEM_HIDDEN)) {
        continue;
      }
      /* If any of the vertices from the edges are no longer connected to a selected edge
       * de-select the entire vertex.. */
      for (BMLoop *l_edge_vert : {l_edge, l_edge->next}) {
        if (!BM_loop_vert_uvselect_check_other_loop_edge(
                l_edge_vert, BM_ELEM_SELECT_UV_EDGE, cd_loop_uv_offset))
        {
          BM_loop_vert_uvselect_set_shared(bm, l_edge_vert, false, cd_loop_uv_offset);
        }
      }
    }

    /* De-select edge pass. */
    for (BMFace *f : faces) {
      if (BM_elem_flag_test(f, BM_ELEM_HIDDEN)) {
        continue;
      }

      BMLoop *l_iter, *l_first;
      l_iter = l_first = BM_FACE_FIRST_LOOP(f);
      do {
        if (!BM_elem_flag_test(l_iter, BM_ELEM_SELECT_UV)) {
          /* Already handled. */
          continue;
        }
        if (!BM_loop_edge_uvselect_check_other_face(l_iter, BM_ELEM_SELECT_UV, cd_loop_uv_offset))
        {
          BM_loop_edge_uvselect_set_shared(bm, l_iter, false, cd_loop_uv_offset);
        }
      } while ((l_iter = l_iter->next) != l_first);
    }

    /* De-select vert pass. */
    for (BMFace *f : faces) {
      if (BM_elem_flag_test(f, BM_ELEM_HIDDEN)) {
        continue;
      }
      BMLoop *l_iter, *l_first;
      l_iter = l_first = BM_FACE_FIRST_LOOP(f);
      do {
        if (!BM_elem_flag_test(l_iter, BM_ELEM_SELECT_UV_EDGE)) {
          /* Already handled. */
          continue;
        }
        if (!BM_loop_vert_uvselect_check_other_loop_edge(
                l_iter, BM_ELEM_SELECT_UV_EDGE, cd_loop_uv_offset))
        {
          BM_loop_vert_uvselect_set_shared(bm, l_iter, false, cd_loop_uv_offset);
        }
      } while ((l_iter = l_iter->next) != l_first);
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name UV Selection Picking Versions of Selection Functions
 *
 * These functions differ in that they perform all necessary flushing but do so only on
 * local elements. This is only practical with a small number of elements since it'd
 * be inefficient on large selections.
 *
 * Note that we *could* also support selecting face-corners from the 3D viewport
 * using these functions, however that's not yet supported.
 *
 * Selection Modes & Flushing
 * ==========================
 *
 * Picking an edge in face-select mode or a vertex in edge-select mode is not supported.
 * This is logical because the user cannot select a single vertex in face select mode.
 * As these functions are exposed publicly for picking, this makes some sense.
 *
 * Internally however, these functions are currently used by #BM_mesh_uvselect_set_elem_from_mesh,
 * which corrects "isolated" elements which should not be selected based on the selection-mode.
 * \{ */

static void bm_vert_uvselect_set_pick(BMesh *bm,
                                      BMVert *v,
                                      const bool select,
                                      const BMUVSelectPickParams & /*uv_pick_params*/,
                                      bool caller_handles_edge_or_face_mode)
{
  if (caller_handles_edge_or_face_mode == false) {
    /* With de-selection, isolated vertices/edges wont be de-selected.
     * In practice users should not be picking edges when in face select mode. */
    BLI_assert_msg(bm->selectmode & (SCE_SELECT_VERTEX),
                   "Picking verts in edge or face-select mode is not supported.");
  }
  /* NOTE: it doesn't make sense to check `uv_pick_params.shared` in this context because,
   * unlike edges and faces, a vertex is logically connected to all corners that use it,
   * so there is no way to use the UV coordinates to differentiate one UV region from another. */

  if (BM_elem_flag_test(v, BM_ELEM_HIDDEN)) {
    return;
  }

  /* Must be connected to edges. */
  if (v->e == nullptr) {
    return;
  }

  if (select) {
    const BMEdge *e_iter, *e_first;
    e_iter = e_first = v->e;
    do {
      if (e_iter->l == nullptr) {
        continue;
      }
      BMLoop *l_radial_iter, *l_radial_first;
      l_radial_iter = l_radial_first = e_iter->l;
      do {
        if (BM_elem_flag_test(l_radial_iter->f, BM_ELEM_HIDDEN)) {
          continue;
        }
        if (v != l_radial_iter->v) {
          continue;
        }
        /* Select vertex. */
        BM_loop_vert_uvselect_set_noflush(bm, l_radial_iter, true);

        /* Select edges if adjacent vertices are selected. */
        if (BM_elem_flag_test(l_radial_iter->next, BM_ELEM_SELECT_UV)) {
          BM_loop_edge_uvselect_set_noflush(bm, l_radial_iter, true);
        }
        if (BM_elem_flag_test(l_radial_iter->prev, BM_ELEM_SELECT_UV)) {
          BM_loop_edge_uvselect_set_noflush(bm, l_radial_iter->prev, true);
        }
        /* Select face if all edges are selected. */
        if (!BM_elem_flag_test(l_radial_iter->f, BM_ELEM_HIDDEN) &&
            !BM_elem_flag_test(l_radial_iter->f, BM_ELEM_SELECT_UV))
        {
          if (BM_face_uvselect_check_edges_all(l_radial_iter->f)) {
            BM_face_uvselect_set_noflush(bm, l_radial_iter->f, true);
          }
        }
      } while ((l_radial_iter = l_radial_iter->radial_next) != l_radial_first);
    } while ((e_iter = bmesh_disk_edge_next(e_iter, v)) != e_first);
  }
  else {
    const BMEdge *e_iter, *e_first;
    e_iter = e_first = v->e;
    do {
      if (e_iter->l == nullptr) {
        continue;
      }
      BMLoop *l_radial_iter, *l_radial_first;
      l_radial_iter = l_radial_first = e_iter->l;
      do {
        if (BM_elem_flag_test(l_radial_iter->f, BM_ELEM_HIDDEN)) {
          continue;
        }
        if (v != l_radial_iter->v) {
          continue;
        }
        /* Deselect vertex. */
        BM_loop_vert_uvselect_set_noflush(bm, l_radial_iter, false);
        /* Deselect edges. */
        BM_loop_edge_uvselect_set_noflush(bm, l_radial_iter, false);
        BM_loop_edge_uvselect_set_noflush(bm, l_radial_iter->prev, false);
        /* Deselect connected face. */
        BM_face_uvselect_set_noflush(bm, l_radial_iter->f, false);
      } while ((l_radial_iter = l_radial_iter->radial_next) != l_radial_first);
    } while ((e_iter = bmesh_disk_edge_next(e_iter, v)) != e_first);
  }
}

static void bm_edge_uvselect_set_pick(BMesh *bm,
                                      BMEdge *e,
                                      const bool select,
                                      const BMUVSelectPickParams &uv_pick_params,
                                      const bool caller_handles_face_mode)
{
  if (caller_handles_face_mode == false) {
    /* With de-selection, isolated vertices/edges wont be de-selected.
     * In practice users should not be picking edges when in face select mode. */
    BLI_assert_msg(bm->selectmode & (SCE_SELECT_VERTEX | SCE_SELECT_EDGE),
                   "Picking edges in face-select mode is not supported.");
  }

  if (BM_elem_flag_test(e, BM_ELEM_HIDDEN)) {
    return;
  }

  /* Must be connected to faces. */
  if (e->l == nullptr) {
    return;
  }

  if (uv_pick_params.shared == false) {
    BMLoop *l_iter, *l_first;

    if (select) {
      bool any_faces_unselected = false;
      l_iter = l_first = e->l;
      do {
        if (BM_elem_flag_test(l_iter->f, BM_ELEM_HIDDEN)) {
          continue;
        }

        BM_loop_edge_uvselect_set_noflush(bm, l_iter, true);

        BM_loop_vert_uvselect_set_noflush(bm, l_iter, true);
        BM_loop_vert_uvselect_set_noflush(bm, l_iter->next, true);

        if (any_faces_unselected == false) {
          if (!BM_elem_flag_test(l_iter->f, BM_ELEM_SELECT_UV)) {
            any_faces_unselected = true;
          }
        }
      } while ((l_iter = l_iter->radial_next) != l_first);

      /* Flush selection to faces when all edges in connected faces are now selected. */
      if (any_faces_unselected) {
        l_iter = l_first = e->l;
        do {
          if (BM_elem_flag_test(l_iter->f, BM_ELEM_HIDDEN)) {
            continue;
          }
          if (!BM_elem_flag_test(l_iter->f, BM_ELEM_SELECT_UV)) {
            if (BM_face_uvselect_check_edges_all(l_iter->f)) {
              BM_face_uvselect_set_noflush(bm, l_iter->f, true);
            }
          }
        } while ((l_iter = l_iter->radial_next) != l_first);
      }
    }
    else {
      l_iter = l_first = e->l;
      do {
        if (BM_elem_flag_test(l_iter->f, BM_ELEM_HIDDEN)) {
          continue;
        }
        BM_loop_edge_uvselect_set_noflush(bm, l_iter, false);
        if (!BM_elem_flag_test(l_iter->prev, BM_ELEM_SELECT_UV_EDGE)) {
          BM_loop_vert_uvselect_set_noflush(bm, l_iter, false);
        }
        if (!BM_elem_flag_test(l_iter->next, BM_ELEM_SELECT_UV_EDGE)) {
          BM_loop_vert_uvselect_set_noflush(bm, l_iter->next, false);
        }
        BM_face_uvselect_set_noflush(bm, l_iter->f, false);
      } while ((l_iter = l_iter->radial_next) != l_first);
    }
    return;
  }

  /* NOTE(@ideasman42): this is awkward as the edge may reference multiple island bounds.
   * - De-selecting will de-select all which makes sense.
   * - Selecting will also select all which is not likely to be all that useful for users.
   *
   * We could attempt to use the surrounding selection to *guess* which UV island selection
   * to extend but this seems error prone as it depends on the order elements are selected
   * so it's it only likely to work in some situations.
   *
   * To *properly* solve this we would be better off to support picking edge+face (loop)
   * combinations from the 3D viewport, so picking the edge would determine the loop which would
   * be selected, but this is a much bigger change.
   *
   * In practice users are likely to prefer face selection when working with UV islands anyway. */

  BMLoop *l_iter, *l_first;

  if (select) {
    bool any_faces_unselected = false;
    l_iter = l_first = e->l;
    do {
      if (BM_elem_flag_test(l_iter->f, BM_ELEM_HIDDEN)) {
        continue;
      }

      BM_loop_edge_uvselect_set_noflush(bm, l_iter, true);

      BM_loop_vert_uvselect_set_noflush(bm, l_iter, true);
      BM_loop_vert_uvselect_set_noflush(bm, l_iter->next, true);

      if (any_faces_unselected == false) {
        if (!BM_elem_flag_test(l_iter->f, BM_ELEM_SELECT_UV)) {
          any_faces_unselected = true;
        }
      }
    } while ((l_iter = l_iter->radial_next) != l_first);

    /* Flush selection to faces when all edges in connected faces are now selected. */
    if (any_faces_unselected) {
      l_iter = l_first = e->l;
      do {
        if (BM_elem_flag_test(l_iter->f, BM_ELEM_HIDDEN)) {
          continue;
        }
        if (!BM_elem_flag_test(l_iter->f, BM_ELEM_SELECT_UV)) {
          if (BM_face_uvselect_check_edges_all(l_iter->f)) {
            BM_face_uvselect_set_noflush(bm, l_iter->f, true);
          }
        }
      } while ((l_iter = l_iter->radial_next) != l_first);
    }
  }
  else {
    l_iter = l_first = e->l;
    do {
      if (BM_elem_flag_test(l_iter->f, BM_ELEM_HIDDEN)) {
        continue;
      }
      BM_loop_edge_uvselect_set_noflush(bm, l_iter, false);
      if (!BM_elem_flag_test(l_iter->prev, BM_ELEM_SELECT_UV_EDGE)) {
        BM_loop_vert_uvselect_set_noflush(bm, l_iter, false);
      }
      if (!BM_elem_flag_test(l_iter->next, BM_ELEM_SELECT_UV_EDGE)) {
        BM_loop_vert_uvselect_set_noflush(bm, l_iter->next, false);
      }
      BM_face_uvselect_set_noflush(bm, l_iter->f, false);
    } while ((l_iter = l_iter->radial_next) != l_first);

    /* Ensure connected vertices remain selected when they are connected to selected edges. */
    l_iter = l_first = e->l;
    do {
      if (BM_elem_flag_test(l_iter->f, BM_ELEM_HIDDEN)) {
        continue;
      }
      for (BMLoop *l_edge_vert : {l_iter, l_iter->next}) {
        if (BM_elem_flag_test(l_edge_vert, BM_ELEM_SELECT_UV)) {
          /* This was not de-selected. */
          continue;
        }
        if (BM_loop_vert_uvselect_check_other_loop_edge(
                l_edge_vert, BM_ELEM_SELECT_UV_EDGE, uv_pick_params.cd_loop_uv_offset))
        {
          BM_loop_vert_uvselect_set_noflush(bm, l_edge_vert, true);
        }
        else {
          /* It's possible there are isolated selected vertices,
           * although in edge select mode this should not happen. */
          BM_loop_vert_uvselect_set_shared(
              bm, l_edge_vert, false, uv_pick_params.cd_loop_uv_offset);
        }
      }
    } while ((l_iter = l_iter->radial_next) != l_first);
  }
}

static void bm_face_uvselect_set_pick(BMesh *bm,
                                      BMFace *f,
                                      const bool select,
                                      const BMUVSelectPickParams &uv_pick_params)
{
  /* Picking faces is valid in all selection modes. */
  if (BM_elem_flag_test(f, BM_ELEM_HIDDEN)) {
    return;
  }

  BMLoop *l_iter, *l_first;

  if (uv_pick_params.shared == false) {
    BM_face_uvselect_set(bm, f, select);
    return;
  }

  if (select) {
    BM_face_uvselect_set_noflush(bm, f, true);

    /* Setting these values first. */
    l_iter = l_first = BM_FACE_FIRST_LOOP(f);
    do {
      BM_loop_vert_uvselect_set_noflush(bm, l_iter, true);
      BM_loop_edge_uvselect_set_noflush(bm, l_iter, true);
    } while ((l_iter = l_iter->next) != l_first);

    /* Set other values. */
    l_iter = l_first = BM_FACE_FIRST_LOOP(f);
    do {
      BM_loop_vert_uvselect_set_shared(bm, l_iter, true, uv_pick_params.cd_loop_uv_offset);
      BM_loop_edge_uvselect_set_shared(bm, l_iter, true, uv_pick_params.cd_loop_uv_offset);
    } while ((l_iter = l_iter->next) != l_first);
  }
  else {
    BM_face_uvselect_set_noflush(bm, f, false);

    l_iter = l_first = BM_FACE_FIRST_LOOP(f);
    do {
      BM_loop_vert_uvselect_set_noflush(bm, l_iter, false);
      BM_loop_edge_uvselect_set_noflush(bm, l_iter, false);
      /* Vertex. */
      if (BM_loop_vert_uvselect_check_other_face(
              l_iter, BM_ELEM_SELECT_UV, uv_pick_params.cd_loop_uv_offset))
      {
        BM_loop_vert_uvselect_set_noflush(bm, l_iter, true);
      }
      else {
        BM_loop_vert_uvselect_set_shared(bm, l_iter, false, uv_pick_params.cd_loop_uv_offset);
      }
      /* Edge. */
      if (BM_loop_edge_uvselect_check_other_face(
              l_iter, BM_ELEM_SELECT_UV, uv_pick_params.cd_loop_uv_offset))
      {
        BM_loop_edge_uvselect_set_noflush(bm, l_iter, true);
      }
      else {
        BM_loop_edge_uvselect_set_shared(bm, l_iter, false, uv_pick_params.cd_loop_uv_offset);
      }
    } while ((l_iter = l_iter->next) != l_first);
  }
}

void BM_vert_uvselect_set_pick(BMesh *bm,
                               BMVert *v,
                               bool select,
                               const BMUVSelectPickParams &params)
{
  const bool caller_handles_edge_or_face_mode = false;
  bm_vert_uvselect_set_pick(bm, v, select, params, caller_handles_edge_or_face_mode);
}
void BM_edge_uvselect_set_pick(BMesh *bm,
                               BMEdge *e,
                               bool select,
                               const BMUVSelectPickParams &params)
{
  const bool caller_handles_face_mode = false;
  bm_edge_uvselect_set_pick(bm, e, select, params, caller_handles_face_mode);
}
void BM_face_uvselect_set_pick(BMesh *bm,
                               BMFace *f,
                               bool select,
                               const BMUVSelectPickParams &params)
{
  /* Picking faces is valid in all modes. */
  bm_face_uvselect_set_pick(bm, f, select, params);
}

/**
 * Ensure isolated elements aren't selected which should be unselected based on `select_mode`.
 *
 * Regarding Picking
 * =================
 *
 * Run this when picking a vertex in edge selection mode or an edge in face select mode.
 *
 * This is not supported by individual picking, however when operating on many elements,
 * it's useful to be able to support this so users of the API can select vertices for example
 * Without it failing entirely because the users has the mesh in edge/face selection mode.
 */
static void bm_mesh_uvselect_mode_flush_down_deselect_only(BMesh *bm,
                                                           const short select_mode,
                                                           const int cd_loop_uv_offset,
                                                           const bool shared,
                                                           const bool check_verts,
                                                           const bool check_edges)
{
  if (!(check_verts || check_edges)) {
    return;
  }

  /* No additional work needed. */
  bool do_check = false;
  if (select_mode & SCE_SELECT_VERTEX) {
    /* Pass. */
  }
  else if (select_mode & SCE_SELECT_EDGE) {
    if (check_verts) {
      do_check = true;
    }
  }
  else if (select_mode & SCE_SELECT_FACE) {
    if (check_verts || check_edges) {
      do_check = true;
    }
  }

  if (do_check == false) {
    return;
  }

  /* This requires a fairly specific kind of flushing.
   * - It's only necessary to flush down (faces -> edges, edges -> verts).
   * - Only select/deselect is needed.
   * Do this inline.
   */
  if (select_mode & SCE_SELECT_EDGE) {
    /* Deselect isolated vertices. */
    BMIter iter;
    BMFace *f;
    BM_ITER_MESH (f, &iter, bm, BM_FACES_OF_MESH) {
      if (BM_elem_flag_test(f, BM_ELEM_HIDDEN)) {
        continue;
      }
      /* Only handle faces that are partially selected. */
      if (BM_elem_flag_test(f, BM_ELEM_SELECT_UV)) {
        continue;
      }
      BMLoop *l_iter, *l_first;
      l_iter = l_first = BM_FACE_FIRST_LOOP(f);
      do {
        if (BM_elem_flag_test(l_iter, BM_ELEM_SELECT_UV) &&
            /* Skip the UV check if either edge is selected. */
            !(BM_elem_flag_test(l_iter, BM_ELEM_SELECT_UV_EDGE) ||
              BM_elem_flag_test(l_iter->prev, BM_ELEM_SELECT_UV_EDGE)))
        {
          if ((shared == false) || !BM_loop_vert_uvselect_check_other_loop_edge(
                                       l_iter, BM_ELEM_SELECT_UV_EDGE, cd_loop_uv_offset))
          {
            BM_elem_flag_disable(l_iter, BM_ELEM_SELECT_UV);
          }
        }
      } while ((l_iter = l_iter->next) != l_first);
    }
  }
  else if (select_mode & SCE_SELECT_FACE) {
    /* Deselect isolated vertices & edges. */
    BMIter iter;
    BMFace *f;
    BM_ITER_MESH (f, &iter, bm, BM_FACES_OF_MESH) {
      if (BM_elem_flag_test(f, BM_ELEM_HIDDEN)) {
        continue;
      }
      /* Only handle faces that are partially selected. */
      if (BM_elem_flag_test(f, BM_ELEM_SELECT_UV)) {
        continue;
      }
      BMLoop *l_iter, *l_first;
      l_iter = l_first = BM_FACE_FIRST_LOOP(f);
      do {
        if (BM_elem_flag_test(l_iter, BM_ELEM_SELECT_UV_EDGE)) {
          if (!BM_loop_edge_uvselect_check_other_face(
                  l_iter, BM_ELEM_SELECT_UV, cd_loop_uv_offset))
          {
            BM_elem_flag_disable(l_iter, BM_ELEM_SELECT_UV_EDGE);
          }
        }
      } while ((l_iter = l_iter->next) != l_first);
      bool e_prev_select = BM_elem_flag_test(l_iter->prev, BM_ELEM_SELECT_UV_EDGE);
      l_iter = l_first;
      do {
        const bool e_iter_select = BM_elem_flag_test(l_iter, BM_ELEM_SELECT_UV_EDGE);
        /* Skip the UV check if either edge is selected. */
        if (BM_elem_flag_test(l_iter, BM_ELEM_SELECT_UV) && !(e_prev_select || e_iter_select)) {
          if ((shared == false) || !BM_loop_vert_uvselect_check_other_face(
                                       l_iter, BM_ELEM_SELECT_UV, cd_loop_uv_offset))
          {
            BM_elem_flag_disable(l_iter, BM_ELEM_SELECT_UV);
          }
        }
        e_prev_select = e_iter_select;
      } while ((l_iter = l_iter->next) != l_first);
    }
  }
}

void BM_mesh_uvselect_set_elem_from_mesh(BMesh *bm,
                                         const bool select,
                                         const BMUVSelectPickParams &params,
                                         const blender::VectorList<BMVert *> &verts,
                                         const blender::VectorList<BMEdge *> &edges,
                                         const blender::VectorList<BMFace *> &faces)
{
  const bool check_verts = !verts.is_empty();
  const bool check_edges = !edges.is_empty();

  /* TODO(@ideasman42): select picking may be slow because it does flushing too.
   * Although in practice it seems fast-enough. This should be handled more efficiently. */

  for (BMVert *v : verts) {
    bm_vert_uvselect_set_pick(bm, v, select, params, true);
  }
  for (BMEdge *e : edges) {
    bm_edge_uvselect_set_pick(bm, e, select, params, true);
  }
  for (BMFace *f : faces) {
    bm_face_uvselect_set_pick(bm, f, select, params);
  }

  bm_mesh_uvselect_mode_flush_down_deselect_only(
      bm, bm->selectmode, params.cd_loop_uv_offset, params.shared, check_verts, check_edges);
}

void BM_mesh_uvselect_set_elem_from_mesh(BMesh *bm,
                                         bool select,
                                         const BMUVSelectPickParams &params,
                                         blender::Span<BMVert *> verts,
                                         blender::Span<BMEdge *> edges,
                                         blender::Span<BMFace *> faces)
{
  const bool check_verts = !verts.is_empty();
  const bool check_edges = !edges.is_empty();

  for (BMVert *v : verts) {
    BM_vert_uvselect_set_pick(bm, v, select, params);
  }
  for (BMEdge *e : edges) {
    BM_edge_uvselect_set_pick(bm, e, select, params);
  }
  for (BMFace *f : faces) {
    BM_face_uvselect_set_pick(bm, f, select, params);
  }

  bm_mesh_uvselect_mode_flush_down_deselect_only(
      bm, bm->selectmode, params.cd_loop_uv_offset, params.shared, check_verts, check_edges);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name UV Selection Flushing (Only Select/De-Select)
 * \{ */

void BM_mesh_uvselect_flush_from_loop_verts_only_select(BMesh *bm)
{
  BMIter iter;
  BMFace *f;
  BM_ITER_MESH (f, &iter, bm, BM_FACES_OF_MESH) {
    if (BM_elem_flag_test(f, BM_ELEM_HIDDEN)) {
      continue;
    }

    BMLoop *l_iter, *l_first;
    l_iter = l_first = BM_FACE_FIRST_LOOP(f);
    bool all_select = true;
    do {
      if (BM_elem_flag_test(l_iter, BM_ELEM_SELECT_UV) &&
          BM_elem_flag_test(l_iter->next, BM_ELEM_SELECT_UV))
      {
        BM_loop_edge_uvselect_set_noflush(bm, l_iter, true);
      }
      else {
        all_select = false;
      }
    } while ((l_iter = l_iter->next) != l_first);
    if (all_select) {
      BM_face_uvselect_set_noflush(bm, f, true);
    }
  }
}

void BM_mesh_uvselect_flush_from_loop_verts_only_deselect(BMesh *bm)
{
  BMIter iter;
  BMFace *f;
  BM_ITER_MESH (f, &iter, bm, BM_FACES_OF_MESH) {
    if (BM_elem_flag_test(f, BM_ELEM_HIDDEN)) {
      continue;
    }

    BMLoop *l_iter, *l_first;
    l_iter = l_first = BM_FACE_FIRST_LOOP(f);
    bool all_select = true;
    do {
      if (BM_elem_flag_test(l_iter, BM_ELEM_SELECT_UV) &&
          BM_elem_flag_test(l_iter->next, BM_ELEM_SELECT_UV))
      {
        /* Pass. */
      }
      else {
        BM_loop_edge_uvselect_set_noflush(bm, l_iter, false);
        all_select = false;
      }
    } while ((l_iter = l_iter->next) != l_first);
    if (all_select == false) {
      BM_face_uvselect_set_noflush(bm, f, false);
    }
  }
}

void BM_mesh_uvselect_flush_from_loop_edges_only_select(BMesh *bm)
{
  BMIter iter;
  BMFace *f;
  BM_ITER_MESH (f, &iter, bm, BM_FACES_OF_MESH) {
    if (BM_elem_flag_test(f, BM_ELEM_HIDDEN)) {
      continue;
    }

    BMLoop *l_iter, *l_first;
    l_iter = l_first = BM_FACE_FIRST_LOOP(f);
    bool all_select = true;
    do {
      if (BM_elem_flag_test(l_iter, BM_ELEM_SELECT_UV_EDGE)) {
        BM_loop_edge_uvselect_set(bm, l_iter, true);
      }
      else {
        all_select = false;
      }
    } while ((l_iter = l_iter->next) != l_first);
    if (all_select) {
      BM_face_uvselect_set_noflush(bm, f, true);
    }
  }
}

void BM_mesh_uvselect_flush_from_loop_edges_only_deselect(BMesh *bm)
{
  BMIter iter;
  BMFace *f;
  BM_ITER_MESH (f, &iter, bm, BM_FACES_OF_MESH) {
    if (BM_elem_flag_test(f, BM_ELEM_HIDDEN)) {
      continue;
    }

    BMLoop *l_iter, *l_first;
    l_iter = l_first = BM_FACE_FIRST_LOOP(f);
    bool all_select = true;
    do {
      BM_loop_vert_uvselect_set_noflush(bm,
                                        l_iter,
                                        (BM_elem_flag_test(l_iter->prev, BM_ELEM_SELECT_UV_EDGE) ||
                                         BM_elem_flag_test(l_iter, BM_ELEM_SELECT_UV_EDGE)));

      if (BM_elem_flag_test(l_iter, BM_ELEM_SELECT_UV_EDGE)) {
        /* Pass. */
      }
      else {
        BM_loop_edge_uvselect_set_noflush(bm, l_iter, false);
        all_select = false;
      }
    } while ((l_iter = l_iter->next) != l_first);
    if (all_select == false) {
      BM_face_uvselect_set_noflush(bm, f, false);
    }
  }
}

void BM_mesh_uvselect_flush_from_faces_only_select(BMesh *bm)
{
  BMIter iter;
  BMFace *f;
  BM_ITER_MESH (f, &iter, bm, BM_FACES_OF_MESH) {
    if (BM_elem_flag_test(f, BM_ELEM_HIDDEN)) {
      continue;
    }

    if (!BM_elem_flag_test(f, BM_ELEM_SELECT_UV)) {
      continue;
    }
    BMLoop *l_iter, *l_first;
    l_iter = l_first = BM_FACE_FIRST_LOOP(f);
    do {
      BM_loop_vert_uvselect_set_noflush(bm, l_iter, true);
      BM_loop_edge_uvselect_set_noflush(bm, l_iter, true);
    } while ((l_iter = l_iter->next) != l_first);
  }
}

void BM_mesh_uvselect_flush_from_faces_only_deselect(BMesh *bm)
{
  BMIter iter;
  BMFace *f;
  BM_ITER_MESH (f, &iter, bm, BM_FACES_OF_MESH) {
    if (BM_elem_flag_test(f, BM_ELEM_HIDDEN)) {
      continue;
    }

    if (BM_elem_flag_test(f, BM_ELEM_SELECT_UV)) {
      continue;
    }
    BMLoop *l_iter, *l_first;
    l_iter = l_first = BM_FACE_FIRST_LOOP(f);
    do {
      BM_loop_vert_uvselect_set_noflush(bm, l_iter, false);
      BM_loop_edge_uvselect_set_noflush(bm, l_iter, false);
    } while ((l_iter = l_iter->next) != l_first);
  }
}

void BM_mesh_uvselect_flush_shared_only_select(BMesh *bm, const int cd_loop_uv_offset)
{
  BLI_assert(cd_loop_uv_offset >= 0);
  BMIter iter;
  BMFace *f;
  BM_ITER_MESH (f, &iter, bm, BM_FACES_OF_MESH) {
    if (BM_elem_flag_test(f, BM_ELEM_HIDDEN)) {
      continue;
    }

    BMLoop *l_iter, *l_first;
    l_iter = l_first = BM_FACE_FIRST_LOOP(f);
    do {
      if (!BM_elem_flag_test(l_iter, BM_ELEM_SELECT_UV)) {
        if (BM_loop_vert_uvselect_check_other_loop_vert(
                l_iter, BM_ELEM_SELECT_UV, cd_loop_uv_offset))
        {
          BM_loop_vert_uvselect_set_noflush(bm, l_iter, true);
        }
      }
      if (!BM_elem_flag_test(l_iter, BM_ELEM_SELECT_UV_EDGE)) {
        if (BM_loop_edge_uvselect_check_other_loop_edge(
                l_iter, BM_ELEM_SELECT_UV_EDGE, cd_loop_uv_offset))
        {
          BM_loop_edge_uvselect_set_noflush(bm, l_iter, true);
        }
      }
    } while ((l_iter = l_iter->next) != l_first);
  }
}

void BM_mesh_uvselect_flush_shared_only_deselect(BMesh *bm, const int cd_loop_uv_offset)
{
  BLI_assert(cd_loop_uv_offset >= 0);
  BMIter iter;
  BMFace *f;
  BM_ITER_MESH (f, &iter, bm, BM_FACES_OF_MESH) {
    if (BM_elem_flag_test(f, BM_ELEM_HIDDEN)) {
      continue;
    }

    BMLoop *l_iter, *l_first;
    l_iter = l_first = BM_FACE_FIRST_LOOP(f);
    do {
      if (BM_elem_flag_test(l_iter, BM_ELEM_SELECT_UV)) {
        if (!BM_loop_vert_uvselect_check_other_loop_vert(
                l_iter, BM_ELEM_SELECT_UV, cd_loop_uv_offset))
        {
          BM_loop_vert_uvselect_set_noflush(bm, l_iter, false);
        }
      }
      if (BM_elem_flag_test(l_iter, BM_ELEM_SELECT_UV_EDGE)) {
        if (!BM_loop_edge_uvselect_check_other_loop_edge(
                l_iter, BM_ELEM_SELECT_UV_EDGE, cd_loop_uv_offset))
        {
          BM_loop_edge_uvselect_set_noflush(bm, l_iter, false);
        }
      }
    } while ((l_iter = l_iter->next) != l_first);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name UV Selection Flushing (Between Elements)
 * \{ */

void BM_mesh_uvselect_flush_from_loop_verts(BMesh *bm)
{
  BMIter iter;
  BMFace *f;
  BM_ITER_MESH (f, &iter, bm, BM_FACES_OF_MESH) {
    if (BM_elem_flag_test(f, BM_ELEM_HIDDEN)) {
      continue;
    }

    bool select_all = true;
    BMLoop *l_iter, *l_first;
    l_iter = l_first = BM_FACE_FIRST_LOOP(f);
    do {
      const bool select = (BM_elem_flag_test(l_iter, BM_ELEM_SELECT_UV) &&
                           BM_elem_flag_test(l_iter->next, BM_ELEM_SELECT_UV));
      BM_loop_edge_uvselect_set_noflush(bm, l_iter, select);
      if (select == false) {
        select_all = false;
      }
    } while ((l_iter = l_iter->next) != l_first);
    BM_face_uvselect_set_noflush(bm, f, select_all);
  }
}

void BM_mesh_uvselect_flush_from_loop_edges(BMesh *bm, bool flush_down)
{
  BMIter iter;
  BMFace *f;

  /* Clear vert/face select. */
  BM_ITER_MESH (f, &iter, bm, BM_FACES_OF_MESH) {
    if (BM_elem_flag_test(f, BM_ELEM_HIDDEN)) {
      continue;
    }

    if (flush_down) {
      BMLoop *l_iter, *l_first;
      l_iter = l_first = BM_FACE_FIRST_LOOP(f);
      do {
        BM_loop_vert_uvselect_set_noflush(bm, l_iter, false);
      } while ((l_iter = l_iter->next) != l_first);
    }
    BM_face_uvselect_set_noflush(bm, f, false);
  }

  BM_ITER_MESH (f, &iter, bm, BM_FACES_OF_MESH) {
    if (BM_elem_flag_test(f, BM_ELEM_HIDDEN)) {
      continue;
    }

    bool select_all = true;
    BMLoop *l_iter, *l_first;
    l_iter = l_first = BM_FACE_FIRST_LOOP(f);
    do {
      const bool select_edge = BM_elem_flag_test(l_iter, BM_ELEM_SELECT_UV_EDGE);
      if (select_edge) {
        if (flush_down) {
          BM_loop_vert_uvselect_set_noflush(bm, l_iter, true);
          BM_loop_vert_uvselect_set_noflush(bm, l_iter->next, true);
        }
      }
      else {
        select_all = false;
      }
    } while ((l_iter = l_iter->next) != l_first);
    if (select_all) {
      BM_face_uvselect_set_noflush(bm, f, true);
    }
  }
}

void BM_mesh_uvselect_flush_from_faces(BMesh *bm, bool flush_down)
{
  if (!flush_down) {
    return; /* NOP. */
  }

  BMIter iter;
  BMFace *f;
  BM_ITER_MESH (f, &iter, bm, BM_FACES_OF_MESH) {
    if (BM_elem_flag_test(f, BM_ELEM_HIDDEN)) {
      continue;
    }

    const bool select_face = BM_elem_flag_test(f, BM_ELEM_SELECT_UV);
    BMLoop *l_iter, *l_first;
    l_iter = l_first = BM_FACE_FIRST_LOOP(f);
    do {
      BM_loop_vert_uvselect_set_noflush(bm, l_iter, select_face);
      BM_loop_edge_uvselect_set_noflush(bm, l_iter, select_face);
    } while ((l_iter = l_iter->next) != l_first);
  }
}

void BM_mesh_uvselect_flush_from_verts(BMesh *bm, const bool select)
{
  if (select) {
    BM_mesh_uvselect_flush_from_loop_verts_only_select(bm);
  }
  else {
    BM_mesh_uvselect_flush_from_loop_verts_only_deselect(bm);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name UV Selection Flushing (Selection Mode Aware)
 * \{ */

void BM_mesh_uvselect_mode_flush_ex(BMesh *bm, const short selectmode, const bool flush_down)
{
  if (selectmode & SCE_SELECT_VERTEX) {
    BM_mesh_uvselect_flush_from_loop_verts(bm);
  }
  else if (selectmode & SCE_SELECT_EDGE) {
    BM_mesh_uvselect_flush_from_loop_edges(bm, flush_down);
  }
  else {
    BM_mesh_uvselect_flush_from_faces(bm, flush_down);
  }
}

void BM_mesh_uvselect_mode_flush(BMesh *bm)
{
  BM_mesh_uvselect_mode_flush_ex(bm, bm->selectmode, false);
}

void BM_mesh_uvselect_mode_flush_only_select(BMesh *bm)
{
  if (bm->selectmode & SCE_SELECT_VERTEX) {
    BM_mesh_uvselect_flush_from_loop_verts_only_select(bm);
  }
  else if (bm->selectmode & SCE_SELECT_EDGE) {
    BM_mesh_uvselect_flush_from_loop_edges_only_select(bm);
  }
  else {
    /* Pass (nothing to do for faces). */
  }
}

void BM_mesh_uvselect_mode_flush_update(BMesh *bm,
                                        const short selectmode_old,
                                        const short selectmode_new,
                                        const int cd_loop_uv_offset)
{

  if (highest_order_bit_s(selectmode_old) >= highest_order_bit_s(selectmode_new)) {

    if ((selectmode_old & SCE_SELECT_VERTEX) == 0 && (selectmode_new & SCE_SELECT_VERTEX)) {
      /* When changing from edge/face to vertex selection,
       * new edges/faces may be selected based on the vertex selection. */
      BM_mesh_uvselect_flush_from_loop_verts(bm);
    }
    else if ((selectmode_old & SCE_SELECT_EDGE) == 0 && (selectmode_new & SCE_SELECT_EDGE)) {
      /* When changing from face to edge selection,
       * new faces may be selected based on the edge selection. */
      BM_mesh_uvselect_flush_from_loop_edges(bm, false);
    }

    /* Pass, no need to do anything when moving from edge to vertex mode (for e.g.). */
    return;
  }

  bool do_flush_deselect_down = false;
  if (selectmode_old & SCE_SELECT_VERTEX) {
    if ((selectmode_new & SCE_SELECT_VERTEX) == 0) {
      do_flush_deselect_down = true;
    }
  }
  else if (selectmode_old & SCE_SELECT_EDGE) {
    if ((selectmode_new & SCE_SELECT_EDGE) == 0) {
      do_flush_deselect_down = true;
    }
  }

  if (do_flush_deselect_down == false) {
    return;
  }

  /* Perform two passes:
   *
   * - De-select all elements where the underlying elements are not selected.
   * - De select any isolated elements.
   *
   *   NOTE: As the mesh will have already had it's isolated elements de-selected,
   *   it may seem like this pass shouldn't be needed in UV space,
   *   however a vert/edge may be isolated in UV space while being connected to a
   *   selected edge/face in 3D space.
   */

  /* First pass: match underlying mesh. */
  BMIter iter;
  BMFace *f;
  BM_ITER_MESH (f, &iter, bm, BM_FACES_OF_MESH) {
    if (BM_elem_flag_test(f, BM_ELEM_HIDDEN)) {
      continue;
    }

    BMLoop *l_iter, *l_first;
    l_iter = l_first = BM_FACE_FIRST_LOOP(f);
    bool select_face = true;
    do {
      if (BM_elem_flag_test(l_iter, BM_ELEM_SELECT_UV)) {
        if (!BM_elem_flag_test(l_iter->v, BM_ELEM_SELECT)) {
          BM_elem_flag_disable(l_iter, BM_ELEM_SELECT_UV);
        }
      }
      if (BM_elem_flag_test(l_iter, BM_ELEM_SELECT_UV_EDGE)) {
        if (!BM_elem_flag_test(l_iter->e, BM_ELEM_SELECT)) {
          BM_elem_flag_disable(l_iter, BM_ELEM_SELECT_UV_EDGE);
          select_face = false;
        }
      }
      else {
        select_face = false;
      }
    } while ((l_iter = l_iter->next) != l_first);

    if (select_face == false) {
      BM_elem_flag_disable(f, BM_ELEM_SELECT_UV);
    }
  }

  /* Second Pass: Ensure isolated elements are not selected. */
  if (cd_loop_uv_offset != -1) {
    const bool shared = true;
    const bool check_verts = (bm->totvertsel != 0);
    const bool check_edges = (bm->totedgesel != 0);
    bm_mesh_uvselect_mode_flush_down_deselect_only(
        bm, selectmode_new, cd_loop_uv_offset, shared, check_verts, check_edges);
  }
}

void BM_mesh_uvselect_flush_post_subdivide(BMesh *bm, const int cd_loop_uv_offset)
{
  {
    BMIter iter;
    BMFace *f;
    BM_ITER_MESH (f, &iter, bm, BM_FACES_OF_MESH) {
      if (BM_elem_flag_test(f, BM_ELEM_SELECT)) {
        BM_face_uvselect_set(bm, f, true);
      }
    }
  }

  const bool use_edges = bm->selectmode & (SCE_SELECT_VERTEX | SCE_SELECT_EDGE);
  if (use_edges) {
    BMIter iter;
    BMEdge *e;
    BM_ITER_MESH (e, &iter, bm, BM_EDGES_OF_MESH) {
      if (e->l == nullptr) {
        continue;
      }
      if (BM_elem_flag_test(e, BM_ELEM_SELECT) &&
          /* This will have been handled if an attached face is selected. */
          !BM_edge_is_any_face_flag_test(e, BM_ELEM_SELECT))
      {
        BMLoop *l_radial_iter, *l_radial_first;
        l_radial_iter = l_radial_first = e->l;
        do {
          BM_loop_edge_uvselect_set(bm, l_radial_iter, true);
        } while ((l_radial_iter = l_radial_iter->radial_next) != l_radial_first);
      }
    }
  }

  /* Now select any "shared" UV's that are connected to an edge or face. */
  if (cd_loop_uv_offset != -1) {
    BMIter iter;
    BMFace *f;
    BM_ITER_MESH (f, &iter, bm, BM_FACES_OF_MESH) {
      if (BM_elem_flag_test(f, BM_ELEM_HIDDEN)) {
        continue;
      }
      if (BM_elem_flag_test(f, BM_ELEM_SELECT_UV)) {
        continue;
      }
      BMLoop *l_iter, *l_first;

      /* Setting these values first. */
      l_iter = l_first = BM_FACE_FIRST_LOOP(f);
      do {
        /* With vertex select mode, only handle vertices, then flush to edges -> faces. */
        if ((bm->selectmode & SCE_SELECT_VERTEX) == 0) {
          /* Check edges first, since a selected edge also indicates a selected vertex. */
          if (!BM_elem_flag_test(l_iter, BM_ELEM_SELECT_UV_EDGE) &&
              BM_loop_edge_uvselect_check_other_loop_edge(
                  l_iter, BM_ELEM_SELECT_UV_EDGE, cd_loop_uv_offset))
          {
            /* Check the other radial edge. */
            BM_loop_edge_uvselect_set(bm, l_iter, true);
          }
        }
        /* Check the other radial vertex (a selected edge will have done this). */
        if (!BM_elem_flag_test(l_iter, BM_ELEM_SELECT_UV)) {
          if (BM_loop_vert_uvselect_check_other_loop_vert(
                  l_iter, BM_ELEM_SELECT_UV, cd_loop_uv_offset))
          {
            BM_loop_vert_uvselect_set_noflush(bm, l_iter, true);
          }
        }
      } while ((l_iter = l_iter->next) != l_first);
    }
  }

  /* It's possible selecting a vertex or edge will cause other elements to have become selected.
   * Flush up if necessary. */
  BM_mesh_uvselect_mode_flush_only_select(bm);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name UV Selection Flushing (From/To Mesh)
 * \{ */

/* Sticky Vertex. */

static void bm_mesh_uvselect_flush_from_mesh_sticky_vert_for_vert_mode(BMesh *bm)
{
  BMIter iter;
  BMFace *f;

  /* UV select flags may be dirty, overwrite all. */
  BM_ITER_MESH (f, &iter, bm, BM_FACES_OF_MESH) {
    if (BM_elem_flag_test(f, BM_ELEM_HIDDEN)) {
      continue;
    }
    BMLoop *l_iter, *l_first;
    l_iter = l_first = BM_FACE_FIRST_LOOP(f);
    do {
      const bool v_select = BM_elem_flag_test(l_iter->v, BM_ELEM_SELECT);
      const bool e_select = BM_elem_flag_test(l_iter->e, BM_ELEM_SELECT);
      BM_elem_flag_set(l_iter, BM_ELEM_SELECT_UV, v_select);
      BM_elem_flag_set(l_iter, BM_ELEM_SELECT_UV_EDGE, e_select);
    } while ((l_iter = l_iter->next) != l_first);
    BM_elem_flag_set(f, BM_ELEM_SELECT_UV, BM_elem_flag_test(f, BM_ELEM_SELECT));
  }
  bm->uv_select_sync_valid = true;
}

static void bm_mesh_uvselect_flush_from_mesh_sticky_vert_for_edge_mode(BMesh *bm)
{
  BMIter iter;
  BMFace *f;

  /* Clearing all makes the following logic simpler as
   * since we only need to select UV's connected to selected edges. */
  bm_mesh_uvselect_disable_all(bm);

  BM_ITER_MESH (f, &iter, bm, BM_FACES_OF_MESH) {
    if (BM_elem_flag_test(f, BM_ELEM_HIDDEN)) {
      continue;
    }

    BMLoop *l_iter, *l_first;
    l_iter = l_first = BM_FACE_FIRST_LOOP(f);
    do {
      if (BM_elem_flag_test(l_iter->e, BM_ELEM_SELECT)) {
        BM_elem_flag_enable(l_iter, BM_ELEM_SELECT_UV_EDGE);
        for (BMLoop *l_edge_vert : {l_iter, l_iter->next}) {
          if (!BM_elem_flag_test(l_edge_vert, BM_ELEM_SELECT_UV)) {
            BM_elem_flag_enable(l_edge_vert, BM_ELEM_SELECT_UV);
          }
        }
      }
    } while ((l_iter = l_iter->next) != l_first);

    if (BM_elem_flag_test(f, BM_ELEM_SELECT)) {
      BM_elem_flag_enable(f, BM_ELEM_SELECT_UV);
    }
  }
  bm->uv_select_sync_valid = true;
}

static void bm_mesh_uvselect_flush_from_mesh_sticky_vert_for_face_mode(BMesh *bm)
{
  BMIter iter;
  BMFace *f;

  /* Clearing all makes the following logic simpler as
   * since we only need to select UV's connected to selected edges. */
  bm_mesh_uvselect_disable_all(bm);

  BM_ITER_MESH (f, &iter, bm, BM_FACES_OF_MESH) {
    if (BM_elem_flag_test(f, BM_ELEM_HIDDEN)) {
      continue;
    }

    if (BM_elem_flag_test(f, BM_ELEM_SELECT)) {
      BMLoop *l_iter, *l_first;
      l_iter = l_first = BM_FACE_FIRST_LOOP(f);
      do {
        BM_elem_flag_enable(l_iter, BM_ELEM_SELECT_UV);
        BM_elem_flag_enable(l_iter, BM_ELEM_SELECT_UV_EDGE);

      } while ((l_iter = l_iter->next) != l_first);
      BM_elem_flag_enable(f, BM_ELEM_SELECT_UV);
    }
  }
  bm->uv_select_sync_valid = true;
}

/* Sticky Location. */

static void bm_mesh_uvselect_flush_from_mesh_sticky_location_for_vert_mode(
    BMesh *bm, const int /*cd_loop_uv_offset*/)
{
  /* In this particular case use the same logic for sticky vertices,
   * unlike faces & edges we can't know which island a selected vertex belongs to.
   *
   * NOTE: arguably this is only true for an isolated vertex selection,
   * if there are surrounding selected edges/faces the vertex could only select UV's
   * connected to those selected regions. However, if this logic was followed (at-run-time)
   * it would mean that de-selecting a face could suddenly cause the vertex
   * (attached to that face on another UV island) to become selected.
   * Since that would be unexpected for users - just use this simple logic here. */
  bm_mesh_uvselect_flush_from_mesh_sticky_vert_for_vert_mode(bm);
}

static void bm_mesh_uvselect_flush_from_mesh_sticky_location_for_edge_mode(
    BMesh *bm, const int cd_loop_uv_offset)
{
  BMIter iter;
  BMFace *f;

  /* UV select flags may be dirty, overwrite all. */
  BM_ITER_MESH (f, &iter, bm, BM_FACES_OF_MESH) {
    if (BM_elem_flag_test(f, BM_ELEM_HIDDEN)) {
      continue;
    }

    BMLoop *l_iter, *l_first;
    l_iter = l_first = BM_FACE_FIRST_LOOP(f);
    bool e_prev_select = BM_elem_flag_test(l_iter->prev->e, BM_ELEM_SELECT);
    do {
      const bool e_iter_select = BM_elem_flag_test(l_iter->e, BM_ELEM_SELECT);
      const bool v_iter_select = (BM_elem_flag_test(l_iter->v, BM_ELEM_SELECT) &&
                                  ((e_prev_select || e_iter_select) ||
                                   /* This is a more expensive check, order last. */
                                   BM_loop_vert_uvselect_check_other_edge(
                                       l_iter, BM_ELEM_SELECT, cd_loop_uv_offset)));

      BM_elem_flag_set(l_iter, BM_ELEM_SELECT_UV, v_iter_select);
      BM_elem_flag_set(l_iter, BM_ELEM_SELECT_UV_EDGE, e_iter_select);
      e_prev_select = e_iter_select;
    } while ((l_iter = l_iter->next) != l_first);

    const bool f_select = BM_elem_flag_test(f, BM_ELEM_SELECT);
    BM_elem_flag_set(f, BM_ELEM_SELECT_UV, f_select);
  }
  bm->uv_select_sync_valid = true;
}

static void bm_mesh_uvselect_flush_from_mesh_sticky_location_for_face_mode(
    BMesh *bm, const int cd_loop_uv_offset)
{
  BMIter iter;
  BMFace *f;

  /* UV select flags may be dirty, overwrite all. */
  BM_ITER_MESH (f, &iter, bm, BM_FACES_OF_MESH) {
    if (BM_elem_flag_test(f, BM_ELEM_HIDDEN)) {
      continue;
    }

    if (BM_elem_flag_test(f, BM_ELEM_SELECT)) {
      BMLoop *l_iter, *l_first;
      l_iter = l_first = BM_FACE_FIRST_LOOP(f);
      do {
        BM_elem_flag_enable(l_iter, BM_ELEM_SELECT_UV | BM_ELEM_SELECT_UV_EDGE);
      } while ((l_iter = l_iter->next) != l_first);
      BM_elem_flag_enable(f, BM_ELEM_SELECT_UV);
    }
    else {
      BMLoop *l_iter, *l_first;
      l_iter = l_first = BM_FACE_FIRST_LOOP(f);
      do {
        const bool v_iter_select = (BM_elem_flag_test(l_iter->v, BM_ELEM_SELECT) &&
                                    BM_loop_vert_uvselect_check_other_face(
                                        l_iter, BM_ELEM_SELECT, cd_loop_uv_offset));
        const bool e_iter_select = (BM_elem_flag_test(l_iter->e, BM_ELEM_SELECT) &&
                                    BM_loop_edge_uvselect_check_other_face(
                                        l_iter, BM_ELEM_SELECT, cd_loop_uv_offset));

        BM_elem_flag_set(l_iter, BM_ELEM_SELECT_UV, v_iter_select);
        BM_elem_flag_set(l_iter, BM_ELEM_SELECT_UV_EDGE, e_iter_select);
      } while ((l_iter = l_iter->next) != l_first);
      BM_elem_flag_disable(f, BM_ELEM_SELECT_UV);
    }
  }
  bm->uv_select_sync_valid = true;
}

/* Public API. */

void BM_mesh_uvselect_sync_from_mesh_sticky_location(BMesh *bm, const int cd_loop_uv_offset)
{
  if (bm->selectmode & SCE_SELECT_VERTEX) {
    bm_mesh_uvselect_flush_from_mesh_sticky_location_for_vert_mode(bm, cd_loop_uv_offset);
  }
  else if (bm->selectmode & SCE_SELECT_EDGE) {
    bm_mesh_uvselect_flush_from_mesh_sticky_location_for_edge_mode(bm, cd_loop_uv_offset);
  }
  else { /* `SCE_SELECT_FACE` */
    bm_mesh_uvselect_flush_from_mesh_sticky_location_for_face_mode(bm, cd_loop_uv_offset);
  }

  BLI_assert(bm->uv_select_sync_valid);
}

void BM_mesh_uvselect_sync_from_mesh_sticky_disabled(BMesh *bm)
{
  /* The mode is ignored when sticky selection is disabled,
   * Always use the selection from the mesh. */
  bm_mesh_uvselect_flush_from_mesh_sticky_vert_for_vert_mode(bm);
  BLI_assert(bm->uv_select_sync_valid);
}

void BM_mesh_uvselect_sync_from_mesh_sticky_vert(BMesh *bm)
{
  if (bm->selectmode & SCE_SELECT_VERTEX) {
    bm_mesh_uvselect_flush_from_mesh_sticky_vert_for_vert_mode(bm);
  }
  else if (bm->selectmode & SCE_SELECT_EDGE) {
    bm_mesh_uvselect_flush_from_mesh_sticky_vert_for_edge_mode(bm);
  }
  else { /* `SCE_SELECT_FACE` */
    bm_mesh_uvselect_flush_from_mesh_sticky_vert_for_face_mode(bm);
  }
  BLI_assert(bm->uv_select_sync_valid);
}

void BM_mesh_uvselect_sync_to_mesh(BMesh *bm)
{
  BLI_assert(bm->uv_select_sync_valid);

  /* Prevent clearing the selection from removing all selection history.
   * This will be validated after flushing. */
  BM_SELECT_HISTORY_BACKUP(bm);

  BM_mesh_elem_hflag_disable_all(bm, BM_VERT | BM_EDGE | BM_FACE, BM_ELEM_SELECT, false);

  if (bm->selectmode & SCE_SELECT_VERTEX) {
    /* Simple, no need to worry about edge selection. */

    /* Copy loop-vert to vert, then flush. */
    BMIter iter;
    BMFace *f;
    BM_ITER_MESH (f, &iter, bm, BM_FACES_OF_MESH) {
      if (BM_elem_flag_test(f, BM_ELEM_HIDDEN)) {
        continue;
      }

      BMLoop *l_iter, *l_first;
      l_iter = l_first = BM_FACE_FIRST_LOOP(f);
      do {
        if (BM_elem_flag_test(l_iter, BM_ELEM_SELECT_UV)) {
          BM_vert_select_set(bm, l_iter->v, true);
        }
      } while ((l_iter = l_iter->next) != l_first);
    }

    BM_mesh_select_flush_from_verts(bm, true);
  }
  else if (bm->selectmode & SCE_SELECT_EDGE) {
    BMIter iter;
    BMFace *f;
    BM_ITER_MESH (f, &iter, bm, BM_FACES_OF_MESH) {
      if (BM_elem_flag_test(f, BM_ELEM_HIDDEN)) {
        continue;
      }

      BMLoop *l_iter, *l_first;
      l_iter = l_first = BM_FACE_FIRST_LOOP(f);
      /* Technically this should only need to check the edge
       * because when a vertex isn't selected, it's connected edges shouldn't be.
       * Check both in the unlikely case of an invalid selection. */
      bool face_select = true;

      do {
        /* This requires the edges to have already been flushed to the vertices (assert next). */
        if (BM_elem_flag_test(l_iter, BM_ELEM_SELECT_UV)) {
          BM_vert_select_set(bm, l_iter->v, true);
        }
        else {
          face_select = false;
        }

        if (BM_elem_flag_test(l_iter, BM_ELEM_SELECT_UV_EDGE)) {
          /* If this fails, we've missed flushing. */
          BLI_assert(BM_elem_flag_test(l_iter, BM_ELEM_SELECT_UV) &&
                     BM_elem_flag_test(l_iter->next, BM_ELEM_SELECT_UV));
          BM_edge_select_set(bm, l_iter->e, true);
        }
        else {
          face_select = false;
        }
      } while ((l_iter = l_iter->next) != l_first);
      if (face_select) {
        BM_face_select_set_noflush(bm, f, true);
      }
    }

    /* It's possible that a face which is *not* UV-selected
     * ends up with all it's edges selected.
     * Perform the edge to face flush inline. */
    BM_ITER_MESH (f, &iter, bm, BM_FACES_OF_MESH) {
      if (BM_elem_flag_test(f, BM_ELEM_HIDDEN)) {
        continue;
      }

      /* If the face is hidden, we can't selected,
       * If the face is already selected, it can be skipped here. */
      if (BM_elem_flag_test(f, BM_ELEM_HIDDEN | BM_ELEM_SELECT)) {
        continue;
      }
      bool face_select = true;
      BMLoop *l_iter, *l_first;
      l_iter = l_first = BM_FACE_FIRST_LOOP(f);
      do {
        if (!BM_elem_flag_test(l_iter->e, BM_ELEM_SELECT)) {
          face_select = false;
          break;
        }
      } while ((l_iter = l_iter->next) != l_first);

      if (face_select) {
        BM_face_select_set_noflush(bm, f, true);
      }
    }
  }
  else { /* `bm->selectmode & SCE_SELECT_FACE` */
    BMIter iter;
    BMFace *f;
    BM_ITER_MESH (f, &iter, bm, BM_FACES_OF_MESH) {
      if (BM_elem_flag_test(f, BM_ELEM_HIDDEN)) {
        continue;
      }

      BMLoop *l_iter, *l_first;
      l_iter = l_first = BM_FACE_FIRST_LOOP(f);
      bool face_select = true;
      do {
        if (!BM_elem_flag_test(l_iter, BM_ELEM_SELECT_UV) ||
            !BM_elem_flag_test(l_iter, BM_ELEM_SELECT_UV_EDGE))
        {
          face_select = false;
          break;
        }
      } while ((l_iter = l_iter->next) != l_first);
      if (face_select) {
        BM_face_select_set(bm, f, true);
      }
    }
  }

  BM_SELECT_HISTORY_RESTORE(bm);

  BM_select_history_validate(bm);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name UV Selection Validation
 *
 * Split the validity checks into categories.
 *
 * - UV selection and viewport selection are in sync.
 *   Where a selected UV-vertex must have it's viewport-vertex selected too.
 *   Where a selected viewport-vertex must have at least one selected UV.
 *
 *   This is core to UV sync-select functioning properly.
 *
 *   Failure to properly sync is likely to result in bugs where UV's aren't handled properly
 *   although it should not cause crashes.
 *
 * - UV selection flushing.
 *   Where the relationship between selected elements makes sense.
 *   - An face cannot be selected when one of it's vertices is de-selected.
 *   - An edge cannot be selected if one of it's vertices is de-selected.
 *   ... etc ...
 *   This is much the same as selection flushing for viewport selection.
 *
 * - Contiguous UV selection
 *   Where co-located UV's are all either selected or de-selected.
 *
 *   Failure to select co-located UV's is *not* an error (on a data-correctness level) rather,
 *   it's something that's applied on a "tool" level - depending on UV sticky options.
 *   Depending on the tools, it may be intended that UV selection be contiguous across UV's.
 * \{ */

/* Asserting can be useful to inspect the values while debugging. */
#if 0 /* Useful when debugging. */
#  define MAYBE_ASSERT BLI_assert(0)
#elif 0 /* Can also be useful. */
#  define MAYBE_ASSERT printf(AT "\n")
#else
#  define MAYBE_ASSERT
#endif

#define INCF_MAYBE_ASSERT(var) \
  { \
    MAYBE_ASSERT; \
    (var) += 1; \
  } \
  ((void)0)

static void bm_mesh_loop_clear_tag(BMesh *bm)
{
  BMIter fiter;
  BMFace *f;
  BM_ITER_MESH (f, &fiter, bm, BM_FACES_OF_MESH) {
    BMLoop *l_iter, *l_first;
    l_iter = l_first = BM_FACE_FIRST_LOOP(f);
    do {
      BM_elem_flag_disable(l_iter, BM_ELEM_TAG);
    } while ((l_iter = l_iter->next) != l_first);
  }
}

/**
 * Check UV vertices and edges are synchronized with the viewport selection.
 *
 * UV face selection isn't checked here since this is handled as part of flushing checks.
 */
static bool bm_mesh_uvselect_check_viewport_sync(BMesh *bm, UVSelectValidateInfo_Sync &info_sub)
{
  bool is_valid = true;

  /* Vertices. */
  {
    int &error_count = info_sub.count_uv_vert_any_selected_with_vert_unselected;
    BLI_assert(error_count == 0);
    BMIter fiter;
    BMFace *f;
    BM_ITER_MESH (f, &fiter, bm, BM_FACES_OF_MESH) {
      if (BM_elem_flag_test(f, BM_ELEM_HIDDEN)) {
        continue;
      }

      BMLoop *l_iter, *l_first;
      l_iter = l_first = BM_FACE_FIRST_LOOP(f);
      do {
        if (BM_elem_flag_test(l_iter, BM_ELEM_SELECT_UV)) {
          if (!BM_elem_flag_test(l_iter->v, BM_ELEM_SELECT)) {
            INCF_MAYBE_ASSERT(error_count);
          }
        }
      } while ((l_iter = l_iter->next) != l_first);
    }
    if (error_count) {
      is_valid = false;
    }
  }

  {
    int &error_count = info_sub.count_uv_vert_none_selected_with_vert_selected;
    BLI_assert(error_count == 0);
    BMIter viter;
    BMIter liter;

    BMVert *v;
    BM_ITER_MESH (v, &viter, bm, BM_VERTS_OF_MESH) {
      if (BM_elem_flag_test(v, BM_ELEM_HIDDEN)) {
        continue;
      }
      if (!BM_elem_flag_test(v, BM_ELEM_SELECT)) {
        continue;
      }

      bool any_loop_selected = false;
      {
        BMLoop *l;
        BM_ITER_ELEM (l, &liter, v, BM_LOOPS_OF_VERT) {
          if (BM_elem_flag_test(l, BM_ELEM_SELECT_UV)) {
            any_loop_selected = true;
            break;
          }
        }
      }

      if (any_loop_selected == false) {
        INCF_MAYBE_ASSERT(error_count);
      }
    }
    if (error_count) {
      is_valid = false;
    }
  }

  /* Edges. */
  {
    int &error_count = info_sub.count_uv_edge_any_selected_with_edge_unselected;
    BLI_assert(error_count == 0);
    BMIter fiter;
    BMFace *f;
    BM_ITER_MESH (f, &fiter, bm, BM_FACES_OF_MESH) {
      if (BM_elem_flag_test(f, BM_ELEM_HIDDEN)) {
        continue;
      }

      BMLoop *l_iter, *l_first;
      l_iter = l_first = BM_FACE_FIRST_LOOP(f);
      do {
        if (BM_elem_flag_test(l_iter, BM_ELEM_SELECT_UV_EDGE)) {
          if (!BM_elem_flag_test(l_iter->v, BM_ELEM_SELECT)) {
            INCF_MAYBE_ASSERT(error_count);
          }
        }
      } while ((l_iter = l_iter->next) != l_first);
    }
    if (error_count) {
      is_valid = false;
    }
  }

  /* When vertex selection is enabled, it's possible for UV's
   * that don't form a selected UV edge to form a selected viewport edge.
   * So, it only makes sense to perform this check in edge selection mode. */
  if ((bm->selectmode & SCE_SELECT_VERTEX) == 0) {
    int &error_count = info_sub.count_uv_edge_none_selected_with_edge_selected;
    BLI_assert(error_count == 0);
    BMIter eiter;

    BMEdge *e;
    BM_ITER_MESH (e, &eiter, bm, BM_EDGES_OF_MESH) {
      if (BM_elem_flag_test(e, BM_ELEM_HIDDEN)) {
        continue;
      }
      if (!BM_elem_flag_test(e, BM_ELEM_SELECT)) {
        continue;
      }
      if (e->l == nullptr) {
        continue;
      }
      bool any_loop_selected = false;
      BMLoop *l_iter = e->l;
      do {
        if (BM_elem_flag_test(l_iter->f, BM_ELEM_HIDDEN)) {
          continue;
        }
        if (BM_elem_flag_test(l_iter, BM_ELEM_SELECT_UV_EDGE)) {
          any_loop_selected = true;
          break;
        }
      } while ((l_iter = l_iter->radial_next) != e->l);
      if (any_loop_selected == false) {
        INCF_MAYBE_ASSERT(error_count);
      }
    }
    if (error_count) {
      is_valid = false;
    }
  }

  return is_valid;
}

static bool bm_mesh_uvselect_check_flush(BMesh *bm, UVSelectValidateInfo_Flush &info_sub)
{
  bool is_valid = true;

  /* Vertices are flushed to edges. */
  {
    int &error_count_selected = info_sub.count_uv_edge_selected_with_any_verts_unselected;
    int &error_count_unselected = info_sub.count_uv_edge_unselected_with_all_verts_selected;
    BLI_assert(error_count_selected == 0 && error_count_unselected == 0);
    BMIter fiter;
    BMFace *f;
    BM_ITER_MESH (f, &fiter, bm, BM_FACES_OF_MESH) {
      if (BM_elem_flag_test(f, BM_ELEM_HIDDEN)) {
        continue;
      }
      BMLoop *l_iter, *l_first;
      l_iter = l_first = BM_FACE_FIRST_LOOP(f);
      do {
        const bool v_curr_select = BM_elem_flag_test(l_iter, BM_ELEM_SELECT_UV);
        const bool v_next_select = BM_elem_flag_test(l_iter->next, BM_ELEM_SELECT_UV);
        if (BM_elem_flag_test(l_iter, BM_ELEM_SELECT_UV_EDGE)) {
          if (!v_curr_select || !v_next_select) {
            INCF_MAYBE_ASSERT(error_count_selected);
          }
        }
        else {
          if (v_curr_select && v_next_select) {
            /* Only an error in with vertex selection mode. */
            if (bm->selectmode & SCE_SELECT_VERTEX) {
              INCF_MAYBE_ASSERT(error_count_unselected);
            }
          }
        }
      } while ((l_iter = l_iter->next) != l_first);
    }
    if (error_count_selected || error_count_unselected) {
      is_valid = false;
    }
  }

  /* Vertices & edges are flushed to faces. */
  {
    int &error_count_verts_selected = info_sub.count_uv_face_selected_with_any_verts_unselected;
    int &error_count_verts_unselected = info_sub.count_uv_face_unselected_with_all_verts_selected;

    int &error_count_edges_selected = info_sub.count_uv_face_selected_with_any_edges_unselected;
    int &error_count_edges_unselected = info_sub.count_uv_face_unselected_with_all_edges_selected;

    BLI_assert(error_count_verts_selected == 0 && error_count_verts_unselected == 0);
    BLI_assert(error_count_edges_selected == 0 && error_count_edges_unselected == 0);
    BMIter fiter;
    BMFace *f;
    BM_ITER_MESH (f, &fiter, bm, BM_FACES_OF_MESH) {
      if (BM_elem_flag_test(f, BM_ELEM_HIDDEN)) {
        continue;
      }
      int uv_vert_select = 0;
      int uv_edge_select = 0;
      BMLoop *l_iter, *l_first;
      l_iter = l_first = BM_FACE_FIRST_LOOP(f);
      do {
        if (BM_elem_flag_test(l_iter, BM_ELEM_SELECT_UV)) {
          uv_vert_select += 1;
        }

        if (BM_elem_flag_test(l_iter, BM_ELEM_SELECT_UV_EDGE)) {
          uv_edge_select += 1;
        }
      } while ((l_iter = l_iter->next) != l_first);

      if (BM_elem_flag_test(f, BM_ELEM_SELECT_UV)) {
        if (uv_vert_select != f->len) {
          INCF_MAYBE_ASSERT(error_count_verts_selected);
        }
        if (uv_edge_select != f->len) {
          INCF_MAYBE_ASSERT(error_count_edges_selected);
        }
      }
      else {
        /* Only an error with vertex or edge selection modes. */
        if (bm->selectmode & SCE_SELECT_VERTEX) {
          if (uv_vert_select == f->len) {
            INCF_MAYBE_ASSERT(error_count_verts_unselected);
          }
        }
        else if (bm->selectmode & SCE_SELECT_EDGE) {
          if (uv_edge_select == f->len) {
            INCF_MAYBE_ASSERT(error_count_edges_unselected);
          }
        }
      }
    }

    if (error_count_verts_selected || error_count_verts_unselected) {
      is_valid = false;
    }
    if (error_count_edges_selected || error_count_edges_unselected) {
      is_valid = false;
    }
  }

  return is_valid;
}

static bool bm_mesh_uvselect_check_contiguous(BMesh *bm,
                                              const int cd_loop_uv_offset,
                                              UVSelectValidateInfo_Contiguous &info_sub)
{
  bool is_valid = true;
  enum {
    UV_IS_SELECTED = 1 << 0,
    UV_IS_UNSELECTED = 1 << 1,
  };

  BLI_assert(cd_loop_uv_offset != -1);

  /* Handle vertices. */
  {
    int &error_count = info_sub.count_uv_vert_non_contiguous_selected;
    BLI_assert(error_count == 0);

    bm_mesh_loop_clear_tag(bm);

    auto loop_vert_select_test_fn = [&cd_loop_uv_offset](BMLoop *l_base) -> int {
      BMIter liter;
      BMLoop *l_other;

      BM_elem_flag_enable(l_base, BM_ELEM_TAG);

      int select_test = 0;

      BM_ITER_ELEM (l_other, &liter, l_base->v, BM_LOOPS_OF_VERT) {
        /* Ignore all hidden. */
        if (BM_elem_flag_test(l_other->f, BM_ELEM_HIDDEN)) {
          continue;
        }
        if (BM_elem_flag_test(l_other, BM_ELEM_TAG)) {
          continue;
        }
        if (!BM_loop_uv_share_vert_check(l_base, l_other, cd_loop_uv_offset)) {
          continue;
        }
        select_test |= BM_elem_flag_test(l_other, BM_ELEM_SELECT_UV) ? UV_IS_SELECTED :
                                                                       UV_IS_UNSELECTED;
        if (select_test == (UV_IS_SELECTED | UV_IS_UNSELECTED)) {
          break;
        }
      }
      return select_test;
    };
    BMIter fiter;
    BMFace *f;
    BM_ITER_MESH (f, &fiter, bm, BM_FACES_OF_MESH) {
      if (BM_elem_flag_test(f, BM_ELEM_HIDDEN)) {
        continue;
      }
      BMLoop *l_iter, *l_first;
      l_iter = l_first = BM_FACE_FIRST_LOOP(f);
      do {
        if (BM_elem_flag_test(l_iter, BM_ELEM_TAG)) {
          continue;
        }
        if (loop_vert_select_test_fn(l_iter) == (UV_IS_SELECTED | UV_IS_UNSELECTED)) {
          INCF_MAYBE_ASSERT(error_count);
        }
      } while ((l_iter = l_iter->next) != l_first);
    }
    if (error_count) {
      is_valid = false;
    }
  }

  /* Handle edges. */
  {
    int &error_count = info_sub.count_uv_edge_non_contiguous_selected;
    BLI_assert(error_count == 0);
    bm_mesh_loop_clear_tag(bm);

    auto loop_edge_select_test_fn = [&cd_loop_uv_offset](BMLoop *l_base) -> int {
      BM_elem_flag_enable(l_base, BM_ELEM_TAG);

      int select_test = 0;
      if (l_base->radial_next != l_base) {
        BMLoop *l_other = l_base->radial_next;
        do {
          /* Ignore all hidden. */
          if (BM_elem_flag_test(l_other->f, BM_ELEM_HIDDEN)) {
            continue;
          }
          if (BM_elem_flag_test(l_other, BM_ELEM_TAG)) {
            continue;
          }
          if (!BM_loop_uv_share_edge_check(l_base, l_other, cd_loop_uv_offset)) {
            continue;
          }

          select_test |= BM_elem_flag_test(l_other, BM_ELEM_SELECT_UV_EDGE) ? UV_IS_SELECTED :
                                                                              UV_IS_UNSELECTED;
          if (select_test == (UV_IS_SELECTED | UV_IS_UNSELECTED)) {
            break;
          }
        } while ((l_other = l_other->radial_next) != l_base);
      }
      return select_test;
    };
    BMIter fiter;
    BMFace *f;
    BM_ITER_MESH (f, &fiter, bm, BM_FACES_OF_MESH) {
      if (BM_elem_flag_test(f, BM_ELEM_HIDDEN)) {
        continue;
      }
      BMLoop *l_iter, *l_first;
      l_iter = l_first = BM_FACE_FIRST_LOOP(f);
      do {
        if (BM_elem_flag_test(l_iter, BM_ELEM_TAG)) {
          continue;
        }
        if (loop_edge_select_test_fn(l_iter) == (UV_IS_SELECTED | UV_IS_UNSELECTED)) {
          INCF_MAYBE_ASSERT(error_count);
        }
      } while ((l_iter = l_iter->next) != l_first);
    }
    if (error_count) {
      is_valid = false;
    }
  }
  return is_valid;
}

/**
 * Checks using both flush & contiguous.
 */
static bool bm_mesh_uvselect_check_flush_and_contiguous(
    BMesh *bm, const int cd_loop_uv_offset, UVSelectValidateInfo_FlushAndContiguous &info_sub)
{
  bool is_valid = true;

  /* Check isolated selection. */
  if ((bm->selectmode & SCE_SELECT_EDGE) && (bm->selectmode & SCE_SELECT_VERTEX) == 0) {
    int &error_count = info_sub.count_uv_vert_isolated_in_edge_or_face_mode;
    BLI_assert(error_count == 0);

    if (bm->selectmode & SCE_SELECT_EDGE) {
      /* All selected UV's must have at least one selected edge. */
      BMIter fiter;
      BMFace *f;
      BM_ITER_MESH (f, &fiter, bm, BM_FACES_OF_MESH) {
        if (BM_elem_flag_test(f, BM_ELEM_HIDDEN)) {
          continue;
        }
        BMLoop *l_iter, *l_first;
        l_iter = l_first = BM_FACE_FIRST_LOOP(f);
        do {
          /* Only check selected vertices. */
          if (BM_elem_flag_test(l_iter, BM_ELEM_SELECT_UV)) {
            if (!BM_elem_flag_test(l_iter, BM_ELEM_SELECT_UV_EDGE) &&
                !BM_elem_flag_test(l_iter->prev, BM_ELEM_SELECT_UV_EDGE) &&
                !BM_loop_vert_uvselect_check_other_loop_edge(
                    l_iter, BM_ELEM_SELECT_UV_EDGE, cd_loop_uv_offset))
            {
              INCF_MAYBE_ASSERT(error_count);
            }
          }
        } while ((l_iter = l_iter->next) != l_first);
      }
    }
    if (error_count) {
      is_valid = false;
    }
  }

  if ((bm->selectmode & SCE_SELECT_FACE) &&
      (bm->selectmode & (SCE_SELECT_VERTEX | SCE_SELECT_EDGE)) == 0)
  {
    int &error_count_vert = info_sub.count_uv_vert_isolated_in_face_mode;
    int &error_count_edge = info_sub.count_uv_edge_isolated_in_face_mode;
    BLI_assert(error_count_vert == 0 && error_count_edge == 0);

    /* All selected UV's must have at least one selected edge. */
    BMIter fiter;
    BMFace *f;
    BM_ITER_MESH (f, &fiter, bm, BM_FACES_OF_MESH) {
      if (BM_elem_flag_test(f, BM_ELEM_HIDDEN)) {
        continue;
      }
      /* If this face is selected, there is no need to search over it's verts. */
      if (BM_elem_flag_test(f, BM_ELEM_SELECT_UV)) {
        continue;
      }
      BMLoop *l_iter, *l_first;
      l_iter = l_first = BM_FACE_FIRST_LOOP(f);
      do {
        /* Only check selected vertices. */
        if (BM_elem_flag_test(l_iter, BM_ELEM_SELECT_UV)) {
          if (!BM_loop_vert_uvselect_check_other_face(
                  l_iter, BM_ELEM_SELECT_UV, cd_loop_uv_offset))
          {
            INCF_MAYBE_ASSERT(error_count_vert);
          }
        }
        /* Only check selected edges. */
        if (BM_elem_flag_test(l_iter, BM_ELEM_SELECT_UV_EDGE)) {
          if (!BM_loop_edge_uvselect_check_other_face(
                  l_iter, BM_ELEM_SELECT_UV, cd_loop_uv_offset))
          {
            INCF_MAYBE_ASSERT(error_count_edge);
          }
        }
      } while ((l_iter = l_iter->next) != l_first);
    }

    if (error_count_vert || error_count_edge) {
      is_valid = false;
    }
  }
  return is_valid;
}

#undef INCF_MAYBE_ASSERT
#undef MAYBE_ASSERT

bool BM_mesh_uvselect_is_valid(BMesh *bm,
                               const int cd_loop_uv_offset,
                               const bool check_sync,
                               const bool check_flush,
                               const bool check_contiguous,
                               UVSelectValidateInfo *info_p)
{
  /* Correctness is as follows:
   *
   * - UV selection must match the viewport selection.
   *   - If a vertex is selected at least one if it's UV verts must be selected.
   *   - If an edge is selected at least one of it's UV verts must be selected.
   *
   * - UV selection must be flushed.
   *
   * Notes:
   * - When all vertices of a face are selected in the viewport
   *   (and therefor the face) is selected, it's possible the UV face is *not* selected,
   *   because the vertices in the viewport may be selected because of other selected UV's,
   *   not part of the UV's associated with the face.
   *
   *   Therefor it is possible for a viewport face to be selected
   *   with an unselected UV face.
   */

  UVSelectValidateInfo _info_fallback = {};
  UVSelectValidateInfo &info = info_p ? *info_p : _info_fallback;

  bool is_valid = true;
  if (check_sync) {
    BLI_assert(bm->uv_select_sync_valid);
    if (!bm_mesh_uvselect_check_viewport_sync(bm, info.sync)) {
      is_valid = false;
    }
  }

  if (check_flush) {
    if (!bm_mesh_uvselect_check_flush(bm, info.flush)) {
      is_valid = false;
    }
  }

  if (check_contiguous) {
    if (!bm_mesh_uvselect_check_contiguous(bm, cd_loop_uv_offset, info.contiguous)) {
      is_valid = false;
    }
  }

  if (check_flush && check_contiguous) {
    if (!bm_mesh_uvselect_check_flush_and_contiguous(bm, cd_loop_uv_offset, info.flush_contiguous))
    {
      is_valid = false;
    }
  }
  return is_valid;
}

/** \} */
