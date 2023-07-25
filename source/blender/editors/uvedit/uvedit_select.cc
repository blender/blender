/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup eduv
 */

#include <cmath>
#include <cstdlib>
#include <cstring>

#include "MEM_guardedalloc.h"

#include "DNA_image_types.h"
#include "DNA_material_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_node_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_space_types.h"

#include "BLI_alloca.h"
#include "BLI_blenlib.h"
#include "BLI_hash.h"
#include "BLI_heap.h"
#include "BLI_kdopbvh.h"
#include "BLI_kdtree.h"
#include "BLI_lasso_2d.h"
#include "BLI_math.h"
#include "BLI_memarena.h"
#include "BLI_polyfill_2d.h"
#include "BLI_polyfill_2d_beautify.h"
#include "BLI_utildefines.h"

#include "BKE_context.h"
#include "BKE_customdata.h"
#include "BKE_editmesh.h"
#include "BKE_layer.h"
#include "BKE_material.h"
#include "BKE_mesh.h"
#include "BKE_mesh_mapping.h"
#include "BKE_report.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#include "ED_image.h"
#include "ED_mesh.h"
#include "ED_screen.h"
#include "ED_select_utils.h"
#include "ED_uvedit.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "WM_api.h"
#include "WM_types.h"

#include "UI_view2d.h"

#include "uvedit_intern.h"

static void uv_select_all_perform(const Scene *scene, Object *obedit, int action);

static void uv_select_all_perform_multi_ex(const Scene *scene,
                                           Object **objects,
                                           const uint objects_len,
                                           int action,
                                           const Object *ob_exclude);
static void uv_select_all_perform_multi(const Scene *scene,
                                        Object **objects,
                                        const uint objects_len,
                                        int action);

static void uv_select_flush_from_tag_face(const Scene *scene, Object *obedit, const bool select);
static void uv_select_flush_from_tag_loop(const Scene *scene, Object *obedit, const bool select);
static void uv_select_flush_from_loop_edge_flag(const Scene *scene, BMEditMesh *em);

static void uv_select_tag_update_for_object(Depsgraph *depsgraph,
                                            const ToolSettings *ts,
                                            Object *obedit);

enum eUVSelectSimilar {
  UV_SSIM_AREA_UV = 1000,
  UV_SSIM_AREA_3D,
  UV_SSIM_FACE,
  UV_SSIM_LENGTH_UV,
  UV_SSIM_LENGTH_3D,
  UV_SSIM_MATERIAL,
  UV_SSIM_OBJECT,
  UV_SSIM_PIN,
  UV_SSIM_SIDES,
  UV_SSIM_WINDING,
};

/* -------------------------------------------------------------------- */
/** \name Active Selection Tracking
 *
 * Currently we don't store loops in the selection history,
 * store face/edge/vert combinations (needed for UV path selection).
 * \{ */

void ED_uvedit_active_vert_loop_set(BMesh *bm, BMLoop *l)
{
  BM_select_history_clear(bm);
  BM_select_history_remove(bm, (BMElem *)l->f);
  BM_select_history_remove(bm, (BMElem *)l->v);
  BM_select_history_store_notest(bm, (BMElem *)l->f);
  BM_select_history_store_notest(bm, (BMElem *)l->v);
}

BMLoop *ED_uvedit_active_vert_loop_get(BMesh *bm)
{
  BMEditSelection *ese = static_cast<BMEditSelection *>(bm->selected.last);
  if (ese && ese->prev) {
    BMEditSelection *ese_prev = ese->prev;
    if ((ese->htype == BM_VERT) && (ese_prev->htype == BM_FACE)) {
      /* May be null. */
      return BM_face_vert_share_loop((BMFace *)ese_prev->ele, (BMVert *)ese->ele);
    }
  }
  return nullptr;
}

void ED_uvedit_active_edge_loop_set(BMesh *bm, BMLoop *l)
{
  BM_select_history_clear(bm);
  BM_select_history_remove(bm, (BMElem *)l->f);
  BM_select_history_remove(bm, (BMElem *)l->e);
  BM_select_history_store_notest(bm, (BMElem *)l->f);
  BM_select_history_store_notest(bm, (BMElem *)l->e);
}

BMLoop *ED_uvedit_active_edge_loop_get(BMesh *bm)
{
  BMEditSelection *ese = static_cast<BMEditSelection *>(bm->selected.last);
  if (ese && ese->prev) {
    BMEditSelection *ese_prev = ese->prev;
    if ((ese->htype == BM_EDGE) && (ese_prev->htype == BM_FACE)) {
      /* May be null. */
      return BM_face_edge_share_loop((BMFace *)ese_prev->ele, (BMEdge *)ese->ele);
    }
  }
  return nullptr;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Visibility and Selection Utilities
 * \{ */

char ED_uvedit_select_mode_get(const Scene *scene)
{
  const ToolSettings *ts = scene->toolsettings;
  char uv_selectmode = UV_SELECT_VERTEX;

  if (ts->uv_flag & UV_SYNC_SELECTION) {
    if (ts->selectmode & SCE_SELECT_VERTEX) {
      uv_selectmode = UV_SELECT_VERTEX;
    }
    else if (ts->selectmode & SCE_SELECT_EDGE) {
      uv_selectmode = UV_SELECT_EDGE;
    }
    else if (ts->selectmode & SCE_SELECT_FACE) {
      uv_selectmode = UV_SELECT_FACE;
    }
  }
  else {
    if (ts->uv_selectmode & UV_SELECT_VERTEX) {
      uv_selectmode = UV_SELECT_VERTEX;
    }
    else if (ts->uv_selectmode & UV_SELECT_EDGE) {
      uv_selectmode = UV_SELECT_EDGE;
    }
    else if (ts->uv_selectmode & UV_SELECT_FACE) {
      uv_selectmode = UV_SELECT_FACE;
    }
  }
  return uv_selectmode;
}

void ED_uvedit_select_sync_flush(const ToolSettings *ts, BMEditMesh *em, const bool select)
{
  /* bmesh API handles flushing but not on de-select */
  if (ts->uv_flag & UV_SYNC_SELECTION) {
    if (ts->selectmode != SCE_SELECT_FACE) {
      if (select == false) {
        EDBM_deselect_flush(em);
      }
      else {
        EDBM_select_flush(em);
      }
    }

    if (select == false) {
      BM_select_history_validate(em->bm);
    }
  }
}

static void uvedit_vertex_select_tagged(BMEditMesh *em,
                                        Scene *scene,
                                        bool select,
                                        const BMUVOffsets offsets)
{
  BMFace *efa;
  BMLoop *l;
  BMIter iter, liter;

  BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
    BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
      if (BM_elem_flag_test(l->v, BM_ELEM_TAG)) {
        uvedit_uv_select_set(scene, em->bm, l, select, false, offsets);
      }
    }
  }
}

bool uvedit_face_visible_test_ex(const ToolSettings *ts, BMFace *efa)
{
  if (ts->uv_flag & UV_SYNC_SELECTION) {
    return (BM_elem_flag_test(efa, BM_ELEM_HIDDEN) == 0);
  }
  return (BM_elem_flag_test(efa, BM_ELEM_HIDDEN) == 0 && BM_elem_flag_test(efa, BM_ELEM_SELECT));
}
bool uvedit_face_visible_test(const Scene *scene, BMFace *efa)
{
  return uvedit_face_visible_test_ex(scene->toolsettings, efa);
}

bool uvedit_face_select_test_ex(const ToolSettings *ts, BMFace *efa, const BMUVOffsets offsets)
{
  BLI_assert(offsets.select_vert >= 0);
  BLI_assert(offsets.select_edge >= 0);
  if (ts->uv_flag & UV_SYNC_SELECTION) {
    return BM_elem_flag_test(efa, BM_ELEM_SELECT);
  }

  BMLoop *l;
  BMIter liter;

  BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
    if (ts->uv_selectmode & UV_SELECT_VERTEX) {
      if (!BM_ELEM_CD_GET_BOOL(l, offsets.select_vert)) {
        return false;
      }
    }
    else {
      if (!BM_ELEM_CD_GET_BOOL(l, offsets.select_edge)) {
        return false;
      }
    }
  }
  return true;
}
bool uvedit_face_select_test(const Scene *scene, BMFace *efa, const BMUVOffsets offsets)
{
  return uvedit_face_select_test_ex(scene->toolsettings, efa, offsets);
}

void uvedit_face_select_set_with_sticky(const Scene *scene,
                                        BMEditMesh *em,
                                        BMFace *efa,
                                        const bool select,
                                        const bool do_history,
                                        const BMUVOffsets offsets)
{
  const ToolSettings *ts = scene->toolsettings;
  const char sticky = ts->uv_sticky;
  if (ts->uv_flag & UV_SYNC_SELECTION) {
    uvedit_face_select_set(scene, em->bm, efa, select, do_history, offsets);
    return;
  }
  if (!uvedit_face_visible_test(scene, efa)) {
    return;
  }
  /* NOTE: Previously face selections done in sticky vertex mode selected stray UV vertices
   * (not part of any face selections). This now uses the sticky location mode logic instead. */
  switch (sticky) {
    case SI_STICKY_DISABLE: {
      uvedit_face_select_set(scene, em->bm, efa, select, do_history, offsets);
      break;
    }
    default: {
      /* SI_STICKY_LOC and SI_STICKY_VERTEX modes. */
      uvedit_face_select_shared_vert(scene, em, efa, select, do_history, offsets);
    }
  }
}

void uvedit_face_select_shared_vert(const Scene *scene,
                                    BMEditMesh *em,
                                    BMFace *efa,
                                    const bool select,
                                    const bool do_history,
                                    const BMUVOffsets offsets)
{
  BMLoop *l;
  BMIter liter;

  BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
    if (select) {
      BM_ELEM_CD_SET_BOOL(l, offsets.select_edge, true);
      uvedit_uv_select_shared_vert(scene, em, l, select, SI_STICKY_LOC, do_history, offsets);
    }
    else {
      BM_ELEM_CD_SET_BOOL(l, offsets.select_edge, false);
      if (!uvedit_vert_is_face_select_any_other(scene, l, offsets)) {
        uvedit_uv_select_shared_vert(scene, em, l, select, SI_STICKY_LOC, do_history, offsets);
      }
    }
  }
}

void uvedit_face_select_set(const Scene *scene,
                            BMesh *bm,
                            BMFace *efa,
                            const bool select,
                            const bool do_history,
                            const BMUVOffsets offsets)
{
  if (select) {
    uvedit_face_select_enable(scene, bm, efa, do_history, offsets);
  }
  else {
    uvedit_face_select_disable(scene, bm, efa, offsets);
  }
}

void uvedit_face_select_enable(
    const Scene *scene, BMesh *bm, BMFace *efa, const bool do_history, const BMUVOffsets offsets)
{
  BLI_assert(offsets.select_vert >= 0);
  BLI_assert(offsets.select_edge >= 0);
  const ToolSettings *ts = scene->toolsettings;

  if (ts->uv_flag & UV_SYNC_SELECTION) {
    BM_face_select_set(bm, efa, true);
    if (do_history) {
      BM_select_history_store(bm, (BMElem *)efa);
    }
  }
  else {
    BMLoop *l;
    BMIter liter;

    BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
      BM_ELEM_CD_SET_BOOL(l, offsets.select_vert, true);
      BM_ELEM_CD_SET_BOOL(l, offsets.select_edge, true);
    }
  }
}

void uvedit_face_select_disable(const Scene *scene,
                                BMesh *bm,
                                BMFace *efa,
                                const BMUVOffsets offsets)
{
  BLI_assert(offsets.select_vert >= 0);
  BLI_assert(offsets.select_edge >= 0);
  const ToolSettings *ts = scene->toolsettings;

  if (ts->uv_flag & UV_SYNC_SELECTION) {
    BM_face_select_set(bm, efa, false);
  }
  else {
    BMLoop *l;
    BMIter liter;

    BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
      BM_ELEM_CD_SET_BOOL(l, offsets.select_vert, false);
      BM_ELEM_CD_SET_BOOL(l, offsets.select_edge, false);
    }
  }
}

bool uvedit_edge_select_test_ex(const ToolSettings *ts, BMLoop *l, const BMUVOffsets offsets)
{
  BLI_assert(offsets.select_vert >= 0);
  BLI_assert(offsets.select_edge >= 0);
  if (ts->uv_flag & UV_SYNC_SELECTION) {
    if (ts->selectmode & SCE_SELECT_FACE) {
      return BM_elem_flag_test(l->f, BM_ELEM_SELECT);
    }
    if (ts->selectmode == SCE_SELECT_EDGE) {
      return BM_elem_flag_test(l->e, BM_ELEM_SELECT);
    }
    return BM_elem_flag_test(l->v, BM_ELEM_SELECT) &&
           BM_elem_flag_test(l->next->v, BM_ELEM_SELECT);
  }

  if (ts->uv_selectmode & UV_SELECT_VERTEX) {
    return BM_ELEM_CD_GET_BOOL(l, offsets.select_vert) &&
           BM_ELEM_CD_GET_BOOL(l->next, offsets.select_vert);
  }
  return BM_ELEM_CD_GET_BOOL(l, offsets.select_edge);
}

bool uvedit_edge_select_test(const Scene *scene, BMLoop *l, const BMUVOffsets offsets)
{
  return uvedit_edge_select_test_ex(scene->toolsettings, l, offsets);
}

void uvedit_edge_select_set_with_sticky(const Scene *scene,
                                        BMEditMesh *em,
                                        BMLoop *l,
                                        const bool select,
                                        const bool do_history,
                                        const BMUVOffsets offsets)
{
  const ToolSettings *ts = scene->toolsettings;
  if (ts->uv_flag & UV_SYNC_SELECTION) {
    uvedit_edge_select_set(scene, em->bm, l, select, do_history, offsets);
    return;
  }

  const int sticky = ts->uv_sticky;
  switch (sticky) {
    case SI_STICKY_DISABLE: {
      if (uvedit_face_visible_test(scene, l->f)) {
        uvedit_edge_select_set(scene, em->bm, l, select, do_history, offsets);
      }
      break;
    }
    case SI_STICKY_VERTEX: {
      uvedit_edge_select_shared_vert(scene, em, l, select, SI_STICKY_VERTEX, do_history, offsets);
      break;
    }
    default: {
      /* SI_STICKY_LOC (Fallback) */
      uvedit_edge_select_shared_vert(scene, em, l, select, SI_STICKY_LOC, do_history, offsets);
      break;
    }
  }
}

void uvedit_edge_select_shared_vert(const Scene *scene,
                                    BMEditMesh *em,
                                    BMLoop *l,
                                    const bool select,
                                    const int sticky_flag,
                                    const bool do_history,
                                    const BMUVOffsets offsets)
{
  BLI_assert(ELEM(sticky_flag, SI_STICKY_LOC, SI_STICKY_VERTEX));
  /* Set edge flags. Rely on this for face visibility checks */
  uvedit_edge_select_set_noflush(scene, l, select, sticky_flag, offsets);

  /* Vert selections. */
  BMLoop *l_iter = l;
  do {
    if (select && BM_ELEM_CD_GET_BOOL(l_iter, offsets.select_edge)) {
      uvedit_uv_select_shared_vert(scene, em, l_iter, true, SI_STICKY_LOC, do_history, offsets);
      uvedit_uv_select_shared_vert(
          scene, em, l_iter->next, true, SI_STICKY_LOC, do_history, offsets);
    }

    else if (!select && !BM_ELEM_CD_GET_BOOL(l_iter, offsets.select_edge)) {
      if (!uvedit_vert_is_edge_select_any_other(scene, l, offsets)) {
        uvedit_uv_select_shared_vert(scene, em, l_iter, false, SI_STICKY_LOC, do_history, offsets);
      }
      if (!uvedit_vert_is_edge_select_any_other(scene, l->next, offsets)) {
        uvedit_uv_select_shared_vert(
            scene, em, l_iter->next, false, SI_STICKY_LOC, do_history, offsets);
      }
    }
  } while (((l_iter = l_iter->radial_next) != l) && (sticky_flag != SI_STICKY_LOC));
}

void uvedit_edge_select_set_noflush(const Scene *scene,
                                    BMLoop *l,
                                    const bool select,
                                    const int sticky_flag,
                                    const BMUVOffsets offsets)
{
  BLI_assert(offsets.uv >= 0);
  BLI_assert(offsets.select_edge >= 0);
  BMLoop *l_iter = l;
  do {
    if (uvedit_face_visible_test(scene, l_iter->f)) {
      if ((sticky_flag == SI_STICKY_VERTEX) || BM_loop_uv_share_edge_check(l, l_iter, offsets.uv))
      {
        BM_ELEM_CD_SET_BOOL(l_iter, offsets.select_edge, select);
      }
    }
  } while (((l_iter = l_iter->radial_next) != l) && (sticky_flag != SI_STICKY_DISABLE));
}

void uvedit_edge_select_set(const Scene *scene,
                            BMesh *bm,
                            BMLoop *l,
                            const bool select,
                            const bool do_history,
                            const BMUVOffsets offsets)
{
  if (select) {
    uvedit_edge_select_enable(scene, bm, l, do_history, offsets);
  }
  else {
    uvedit_edge_select_disable(scene, bm, l, offsets);
  }
}

void uvedit_edge_select_enable(
    const Scene *scene, BMesh *bm, BMLoop *l, const bool do_history, const BMUVOffsets offsets)

{
  const ToolSettings *ts = scene->toolsettings;
  BLI_assert(offsets.select_vert >= 0);
  BLI_assert(offsets.select_edge >= 0);

  if (ts->uv_flag & UV_SYNC_SELECTION) {
    if (ts->selectmode & SCE_SELECT_FACE) {
      BM_face_select_set(bm, l->f, true);
    }
    else if (ts->selectmode & SCE_SELECT_EDGE) {
      BM_edge_select_set(bm, l->e, true);
    }
    else {
      BM_vert_select_set(bm, l->e->v1, true);
      BM_vert_select_set(bm, l->e->v2, true);
    }

    if (do_history) {
      BM_select_history_store(bm, (BMElem *)l->e);
    }
  }
  else {
    BM_ELEM_CD_SET_BOOL(l, offsets.select_vert, true);
    BM_ELEM_CD_SET_BOOL(l, offsets.select_edge, true);
    BM_ELEM_CD_SET_BOOL(l->next, offsets.select_vert, true);
  }
}

void uvedit_edge_select_disable(const Scene *scene,
                                BMesh *bm,
                                BMLoop *l,
                                const BMUVOffsets offsets)
{
  const ToolSettings *ts = scene->toolsettings;
  BLI_assert(offsets.select_vert >= 0);
  BLI_assert(offsets.select_edge >= 0);

  if (ts->uv_flag & UV_SYNC_SELECTION) {
    if (ts->selectmode & SCE_SELECT_FACE) {
      BM_face_select_set(bm, l->f, false);
    }
    else if (ts->selectmode & SCE_SELECT_EDGE) {
      BM_edge_select_set(bm, l->e, false);
    }
    else {
      BM_vert_select_set(bm, l->e->v1, false);
      BM_vert_select_set(bm, l->e->v2, false);
    }
  }
  else {
    BM_ELEM_CD_SET_BOOL(l, offsets.select_edge, false);
    if ((ts->uv_selectmode & UV_SELECT_VERTEX) == 0) {
      /* Deselect UV vertex if not part of another edge selection */
      if (!BM_ELEM_CD_GET_BOOL(l->next, offsets.select_edge)) {
        BM_ELEM_CD_SET_BOOL(l->next, offsets.select_vert, false);
      }
      if (!BM_ELEM_CD_GET_BOOL(l->prev, offsets.select_edge)) {
        BM_ELEM_CD_SET_BOOL(l, offsets.select_vert, false);
      }
    }
    else {
      BM_ELEM_CD_SET_BOOL(l, offsets.select_vert, false);
      BM_ELEM_CD_SET_BOOL(l->next, offsets.select_vert, false);
    }
  }
}

bool uvedit_uv_select_test_ex(const ToolSettings *ts, BMLoop *l, const BMUVOffsets offsets)
{
  BLI_assert(offsets.select_vert >= 0);
  if (ts->uv_flag & UV_SYNC_SELECTION) {
    if (ts->selectmode & SCE_SELECT_FACE) {
      return BM_elem_flag_test_bool(l->f, BM_ELEM_SELECT);
    }
    if (ts->selectmode & SCE_SELECT_EDGE) {
      /* Are you looking for `uvedit_edge_select_test(...)` instead? */
    }
    return BM_elem_flag_test_bool(l->v, BM_ELEM_SELECT);
  }

  if (ts->selectmode & SCE_SELECT_FACE) {
    /* Are you looking for `uvedit_face_select_test(...)` instead? */
  }

  if (ts->selectmode & SCE_SELECT_EDGE) {
    /* Are you looking for `uvedit_edge_select_test(...)` instead? */
  }

  return BM_ELEM_CD_GET_BOOL(l, offsets.select_vert);
}

bool uvedit_uv_select_test(const Scene *scene, BMLoop *l, const BMUVOffsets offsets)
{
  return uvedit_uv_select_test_ex(scene->toolsettings, l, offsets);
}

void uvedit_uv_select_set_with_sticky(const Scene *scene,
                                      BMEditMesh *em,
                                      BMLoop *l,
                                      const bool select,
                                      const bool do_history,
                                      const BMUVOffsets offsets)
{
  const ToolSettings *ts = scene->toolsettings;
  if (ts->uv_flag & UV_SYNC_SELECTION) {
    uvedit_uv_select_set(scene, em->bm, l, select, do_history, offsets);
    return;
  }

  const int sticky = ts->uv_sticky;
  switch (sticky) {
    case SI_STICKY_DISABLE: {
      if (uvedit_face_visible_test(scene, l->f)) {
        uvedit_uv_select_set(scene, em->bm, l, select, do_history, offsets);
      }
      break;
    }
    case SI_STICKY_VERTEX: {
      uvedit_uv_select_shared_vert(scene, em, l, select, SI_STICKY_VERTEX, do_history, offsets);
      break;
    }
    default: {
      /* SI_STICKY_LOC. */
      uvedit_uv_select_shared_vert(scene, em, l, select, SI_STICKY_LOC, do_history, offsets);
      break;
    }
  }
}

void uvedit_uv_select_shared_vert(const Scene *scene,
                                  BMEditMesh *em,
                                  BMLoop *l,
                                  const bool select,
                                  const int sticky_flag,
                                  const bool do_history,
                                  const BMUVOffsets offsets)
{
  BLI_assert(ELEM(sticky_flag, SI_STICKY_LOC, SI_STICKY_VERTEX));
  BLI_assert(offsets.uv >= 0);

  BMEdge *e_first, *e_iter;
  e_first = e_iter = l->e;
  do {
    BMLoop *l_radial_iter = e_iter->l;
    if (!l_radial_iter) {
      continue; /* Skip wire edges with no loops. */
    }
    do {
      if (l_radial_iter->v == l->v) {
        if (uvedit_face_visible_test(scene, l_radial_iter->f)) {
          bool do_select = false;
          if (sticky_flag == SI_STICKY_VERTEX) {
            do_select = true;
          }
          else if (BM_loop_uv_share_vert_check(l, l_radial_iter, offsets.uv)) {
            do_select = true;
          }

          if (do_select) {
            uvedit_uv_select_set(scene, em->bm, l_radial_iter, select, do_history, offsets);
          }
        }
      }
    } while ((l_radial_iter = l_radial_iter->radial_next) != e_iter->l);
  } while ((e_iter = BM_DISK_EDGE_NEXT(e_iter, l->v)) != e_first);
}

void uvedit_uv_select_set(const Scene *scene,
                          BMesh *bm,
                          BMLoop *l,
                          const bool select,
                          const bool do_history,
                          const BMUVOffsets offsets)
{
  if (select) {
    uvedit_uv_select_enable(scene, bm, l, do_history, offsets);
  }
  else {
    uvedit_uv_select_disable(scene, bm, l, offsets);
  }
}

void uvedit_uv_select_enable(
    const Scene *scene, BMesh *bm, BMLoop *l, const bool do_history, const BMUVOffsets offsets)
{
  const ToolSettings *ts = scene->toolsettings;
  BLI_assert(offsets.select_vert >= 0);

  if (ts->selectmode & SCE_SELECT_EDGE) {
    /* Are you looking for `uvedit_edge_select_set(...)` instead? */
  }

  if (ts->uv_flag & UV_SYNC_SELECTION) {
    if (ts->selectmode & SCE_SELECT_FACE) {
      BM_face_select_set(bm, l->f, true);
    }
    else {
      BM_vert_select_set(bm, l->v, true);
    }

    if (do_history) {
      BM_select_history_store(bm, (BMElem *)l->v);
    }
  }
  else {
    BM_ELEM_CD_SET_BOOL(l, offsets.select_vert, true);
  }
}

void uvedit_uv_select_disable(const Scene *scene, BMesh *bm, BMLoop *l, const BMUVOffsets offsets)
{
  const ToolSettings *ts = scene->toolsettings;
  BLI_assert(offsets.select_vert >= 0);

  if (ts->uv_flag & UV_SYNC_SELECTION) {
    if (ts->selectmode & SCE_SELECT_FACE) {
      BM_face_select_set(bm, l->f, false);
    }
    else {
      BM_vert_select_set(bm, l->v, false);
    }
  }
  else {
    BM_ELEM_CD_SET_BOOL(l, offsets.select_vert, false);
  }
}

static BMLoop *uvedit_loop_find_other_radial_loop_with_visible_face(const Scene *scene,
                                                                    BMLoop *l_src,
                                                                    const BMUVOffsets offsets)
{
  BLI_assert(offsets.uv >= 0);
  BMLoop *l_other = nullptr;
  BMLoop *l_iter = l_src->radial_next;
  if (l_iter != l_src) {
    do {
      if (uvedit_face_visible_test(scene, l_iter->f) &&
          BM_loop_uv_share_edge_check(l_src, l_iter, offsets.uv))
      {
        /* Check UVs are contiguous. */
        if (l_other == nullptr) {
          l_other = l_iter;
        }
        else {
          /* Only use when there is a single alternative. */
          l_other = nullptr;
          break;
        }
      }
    } while ((l_iter = l_iter->radial_next) != l_src);
  }
  return l_other;
}

static BMLoop *uvedit_loop_find_other_boundary_loop_with_visible_face(const Scene *scene,
                                                                      BMLoop *l_edge,
                                                                      BMVert *v_pivot,
                                                                      const BMUVOffsets offsets)
{
  BLI_assert(uvedit_loop_find_other_radial_loop_with_visible_face(scene, l_edge, offsets) ==
             nullptr);

  BMLoop *l_step = l_edge;
  l_step = (l_step->v == v_pivot) ? l_step->prev : l_step->next;
  BMLoop *l_step_last = nullptr;
  do {
    BLI_assert(BM_vert_in_edge(l_step->e, v_pivot));
    l_step_last = l_step;
    l_step = uvedit_loop_find_other_radial_loop_with_visible_face(scene, l_step, offsets);
    if (l_step) {
      l_step = (l_step->v == v_pivot) ? l_step->prev : l_step->next;
    }
  } while (l_step != nullptr);

  if (l_step_last != nullptr) {
    BLI_assert(uvedit_loop_find_other_radial_loop_with_visible_face(scene, l_step_last, offsets) ==
               nullptr);
  }

  return l_step_last;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Find Nearest Elements
 * \{ */

UvNearestHit uv_nearest_hit_init_dist_px(const View2D *v2d, const float dist_px)
{
  UvNearestHit hit = {0};
  hit.dist_sq = square_f(U.pixelsize * dist_px);
  hit.scale[0] = UI_view2d_scale_get_x(v2d);
  hit.scale[1] = UI_view2d_scale_get_y(v2d);
  return hit;
}

UvNearestHit uv_nearest_hit_init_max(const View2D *v2d)
{
  UvNearestHit hit = {0};
  hit.dist_sq = FLT_MAX;
  hit.scale[0] = UI_view2d_scale_get_x(v2d);
  hit.scale[1] = UI_view2d_scale_get_y(v2d);
  return hit;
}

bool uv_find_nearest_edge(
    Scene *scene, Object *obedit, const float co[2], const float penalty, UvNearestHit *hit)
{
  BLI_assert((hit->scale[0] > 0.0f) && (hit->scale[1] > 0.0f));
  BMEditMesh *em = BKE_editmesh_from_object(obedit);
  BMFace *efa;
  BMLoop *l;
  BMIter iter, liter;
  float *luv, *luv_next;
  int i;
  bool found = false;

  const BMUVOffsets offsets = BM_uv_map_get_offsets(em->bm);
  BLI_assert(offsets.uv >= 0);

  BM_mesh_elem_index_ensure(em->bm, BM_VERT);

  BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
    if (!uvedit_face_visible_test(scene, efa)) {
      continue;
    }
    BM_ITER_ELEM_INDEX (l, &liter, efa, BM_LOOPS_OF_FACE, i) {
      luv = BM_ELEM_CD_GET_FLOAT_P(l, offsets.uv);
      luv_next = BM_ELEM_CD_GET_FLOAT_P(l->next, offsets.uv);

      float delta[2];
      closest_to_line_segment_v2(delta, co, luv, luv_next);

      sub_v2_v2(delta, co);
      mul_v2_v2(delta, hit->scale);

      float dist_test_sq = len_squared_v2(delta);

      /* Ensures that successive selection attempts will select other edges sharing the same
       * UV coordinates as the previous selection. */
      if ((penalty != 0.0f) && uvedit_edge_select_test(scene, l, offsets)) {
        dist_test_sq = square_f(sqrtf(dist_test_sq) + penalty);
      }
      if (dist_test_sq < hit->dist_sq) {
        hit->ob = obedit;
        hit->efa = efa;

        hit->l = l;

        hit->dist_sq = dist_test_sq;
        found = true;
      }
    }
  }
  return found;
}

bool uv_find_nearest_edge_multi(Scene *scene,
                                Object **objects,
                                const uint objects_len,
                                const float co[2],
                                const float penalty,
                                UvNearestHit *hit)
{
  bool found = false;
  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    Object *obedit = objects[ob_index];
    if (uv_find_nearest_edge(scene, obedit, co, penalty, hit)) {
      found = true;
    }
  }
  return found;
}

bool uv_find_nearest_face_ex(
    Scene *scene, Object *obedit, const float co[2], UvNearestHit *hit, const bool only_in_face)
{
  BLI_assert((hit->scale[0] > 0.0f) && (hit->scale[1] > 0.0f));
  BMEditMesh *em = BKE_editmesh_from_object(obedit);
  bool found = false;

  const int cd_loop_uv_offset = CustomData_get_offset(&em->bm->ldata, CD_PROP_FLOAT2);

  BMIter iter;
  BMFace *efa;

  BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
    if (!uvedit_face_visible_test(scene, efa)) {
      continue;
    }

    float cent[2];
    BM_face_uv_calc_center_median(efa, cd_loop_uv_offset, cent);

    float delta[2];
    sub_v2_v2v2(delta, co, cent);
    mul_v2_v2(delta, hit->scale);

    const float dist_test_sq = len_squared_v2(delta);

    if (dist_test_sq < hit->dist_sq) {

      if (only_in_face) {
        if (!BM_face_uv_point_inside_test(efa, co, cd_loop_uv_offset)) {
          continue;
        }
      }

      hit->ob = obedit;
      hit->efa = efa;
      hit->dist_sq = dist_test_sq;
      found = true;
    }
  }
  return found;
}

bool uv_find_nearest_face(Scene *scene, Object *obedit, const float co[2], UvNearestHit *hit)
{
  return uv_find_nearest_face_ex(scene, obedit, co, hit, false);
}

bool uv_find_nearest_face_multi_ex(Scene *scene,
                                   Object **objects,
                                   const uint objects_len,
                                   const float co[2],
                                   UvNearestHit *hit,
                                   const bool only_in_face)
{
  bool found = false;
  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    Object *obedit = objects[ob_index];
    if (uv_find_nearest_face_ex(scene, obedit, co, hit, only_in_face)) {
      found = true;
    }
  }
  return found;
}

bool uv_find_nearest_face_multi(
    Scene *scene, Object **objects, const uint objects_len, const float co[2], UvNearestHit *hit)
{
  return uv_find_nearest_face_multi_ex(scene, objects, objects_len, co, hit, false);
}

static bool uv_nearest_between(const BMLoop *l, const float co[2], const int cd_loop_uv_offset)
{
  const float *uv_prev = BM_ELEM_CD_GET_FLOAT_P(l->prev, cd_loop_uv_offset);
  const float *uv_curr = BM_ELEM_CD_GET_FLOAT_P(l, cd_loop_uv_offset);
  const float *uv_next = BM_ELEM_CD_GET_FLOAT_P(l->next, cd_loop_uv_offset);

  return ((line_point_side_v2(uv_prev, uv_curr, co) > 0.0f) &&
          (line_point_side_v2(uv_next, uv_curr, co) <= 0.0f));
}

bool uv_find_nearest_vert(
    Scene *scene, Object *obedit, float const co[2], const float penalty_dist, UvNearestHit *hit)
{
  BLI_assert((hit->scale[0] > 0.0f) && (hit->scale[1] > 0.0f));
  bool found = false;

  BMEditMesh *em = BKE_editmesh_from_object(obedit);
  BMFace *efa;
  BMIter iter;

  BM_mesh_elem_index_ensure(em->bm, BM_VERT);

  const BMUVOffsets offsets = BM_uv_map_get_offsets(em->bm);
  BLI_assert(offsets.uv >= 0);

  BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
    if (!uvedit_face_visible_test(scene, efa)) {
      continue;
    }

    BMIter liter;
    BMLoop *l;
    int i;
    BM_ITER_ELEM_INDEX (l, &liter, efa, BM_LOOPS_OF_FACE, i) {
      float *luv = BM_ELEM_CD_GET_FLOAT_P(l, offsets.uv);

      float delta[2];

      sub_v2_v2v2(delta, co, luv);
      mul_v2_v2(delta, hit->scale);

      float dist_test_sq = len_squared_v2(delta);

      /* Ensures that successive selection attempts will select other vertices sharing the same
       * UV coordinates */
      if ((penalty_dist != 0.0f) && uvedit_uv_select_test(scene, l, offsets)) {
        dist_test_sq = square_f(sqrtf(dist_test_sq) + penalty_dist);
      }

      if (dist_test_sq <= hit->dist_sq) {
        if (dist_test_sq == hit->dist_sq) {
          if (!uv_nearest_between(l, co, offsets.uv)) {
            continue;
          }
        }

        hit->dist_sq = dist_test_sq;

        hit->ob = obedit;
        hit->efa = efa;
        hit->l = l;
        found = true;
      }
    }
  }

  return found;
}

bool uv_find_nearest_vert_multi(Scene *scene,
                                Object **objects,
                                const uint objects_len,
                                float const co[2],
                                const float penalty_dist,
                                UvNearestHit *hit)
{
  bool found = false;
  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    Object *obedit = objects[ob_index];
    if (uv_find_nearest_vert(scene, obedit, co, penalty_dist, hit)) {
      found = true;
    }
  }
  return found;
}

static bool uvedit_nearest_uv(const Scene *scene,
                              Object *obedit,
                              const float co[2],
                              const float scale[2],
                              const bool ignore_selected,
                              float *dist_sq,
                              float r_uv[2])
{
  BMEditMesh *em = BKE_editmesh_from_object(obedit);
  BMIter iter;
  BMFace *efa;
  const float *uv_best = nullptr;
  float dist_best = *dist_sq;
  const BMUVOffsets offsets = BM_uv_map_get_offsets(em->bm);
  BLI_assert(offsets.uv >= 0);
  BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
    if (!uvedit_face_visible_test(scene, efa)) {
      continue;
    }
    BMLoop *l_iter, *l_first;
    l_iter = l_first = BM_FACE_FIRST_LOOP(efa);
    do {
      if (ignore_selected && uvedit_uv_select_test(scene, l_iter, offsets)) {
        continue;
      }

      const float *uv = BM_ELEM_CD_GET_FLOAT_P(l_iter, offsets.uv);
      float co_tmp[2];
      mul_v2_v2v2(co_tmp, scale, uv);
      const float dist_test = len_squared_v2v2(co, co_tmp);
      if (dist_best > dist_test) {
        dist_best = dist_test;
        uv_best = uv;
      }
    } while ((l_iter = l_iter->next) != l_first);
  }

  if (uv_best != nullptr) {
    copy_v2_v2(r_uv, uv_best);
    *dist_sq = dist_best;
    return true;
  }
  return false;
}

bool ED_uvedit_nearest_uv_multi(const View2D *v2d,
                                const Scene *scene,
                                Object **objects,
                                const uint objects_len,
                                const int mval[2],
                                const bool ignore_selected,
                                float *dist_sq,
                                float r_uv[2])
{
  bool found = false;

  float scale[2], offset[2];
  UI_view2d_scale_get(v2d, &scale[0], &scale[1]);
  UI_view2d_view_to_region_fl(v2d, 0.0f, 0.0f, &offset[0], &offset[1]);

  float co[2];

  const float mval_fl[2] = {float(mval[0]), float(mval[1])};
  sub_v2_v2v2(co, mval_fl, offset);

  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    Object *obedit = objects[ob_index];
    if (uvedit_nearest_uv(scene, obedit, co, scale, ignore_selected, dist_sq, r_uv)) {
      found = true;
    }
  }
  return found;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Find Nearest to Element
 *
 * These functions are quite specialized, useful when sync select is enabled
 * and we want to pick an active UV vertex/edge from the active element which may
 * have multiple UVs split out.
 * \{ */

BMLoop *uv_find_nearest_loop_from_vert(Scene *scene, Object *obedit, BMVert *v, const float co[2])
{
  BMEditMesh *em = BKE_editmesh_from_object(obedit);
  const int cd_loop_uv_offset = CustomData_get_offset(&em->bm->ldata, CD_PROP_FLOAT2);

  BMIter liter;
  BMLoop *l;
  BMLoop *l_found = nullptr;
  float dist_best_sq = FLT_MAX;

  BM_ITER_ELEM (l, &liter, v, BM_LOOPS_OF_VERT) {
    if (!uvedit_face_visible_test(scene, l->f)) {
      continue;
    }

    const float *luv = BM_ELEM_CD_GET_FLOAT_P(l, cd_loop_uv_offset);
    const float dist_test_sq = len_squared_v2v2(co, luv);
    if (dist_test_sq < dist_best_sq) {
      dist_best_sq = dist_test_sq;
      l_found = l;
    }
  }
  return l_found;
}

BMLoop *uv_find_nearest_loop_from_edge(Scene *scene, Object *obedit, BMEdge *e, const float co[2])
{
  BMEditMesh *em = BKE_editmesh_from_object(obedit);
  const int cd_loop_uv_offset = CustomData_get_offset(&em->bm->ldata, CD_PROP_FLOAT2);

  BMIter eiter;
  BMLoop *l;
  BMLoop *l_found = nullptr;
  float dist_best_sq = FLT_MAX;

  BM_ITER_ELEM (l, &eiter, e, BM_LOOPS_OF_EDGE) {
    if (!uvedit_face_visible_test(scene, l->f)) {
      continue;
    }
    const float *luv = BM_ELEM_CD_GET_FLOAT_P(l, cd_loop_uv_offset);
    const float *luv_next = BM_ELEM_CD_GET_FLOAT_P(l->next, cd_loop_uv_offset);
    const float dist_test_sq = dist_squared_to_line_segment_v2(co, luv, luv_next);
    if (dist_test_sq < dist_best_sq) {
      dist_best_sq = dist_test_sq;
      l_found = l;
    }
  }
  return l_found;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Helper functions for UV selection.
 * \{ */

bool uvedit_vert_is_edge_select_any_other(const Scene *scene, BMLoop *l, const BMUVOffsets offsets)
{
  BLI_assert(offsets.uv >= 0);
  BMEdge *e_iter = l->e;
  do {
    BMLoop *l_radial_iter = e_iter->l, *l_other;
    do {
      if (uvedit_face_visible_test(scene, l_radial_iter->f)) {
        /* Use #l_other to check if the uvs are connected (share the same uv coordinates)
         * and #l_radial_iter for the actual edge selection test. */
        l_other = (l_radial_iter->v != l->v) ? l_radial_iter->next : l_radial_iter;
        if (BM_loop_uv_share_vert_check(l, l_other, offsets.uv) &&
            uvedit_edge_select_test(scene, l_radial_iter, offsets))
        {
          return true;
        }
      }
    } while ((l_radial_iter = l_radial_iter->radial_next) != e_iter->l);
  } while ((e_iter = BM_DISK_EDGE_NEXT(e_iter, l->v)) != l->e);

  return false;
}

bool uvedit_vert_is_face_select_any_other(const Scene *scene, BMLoop *l, const BMUVOffsets offsets)
{
  BLI_assert(offsets.uv >= 0);
  BMIter liter;
  BMLoop *l_iter;
  BM_ITER_ELEM (l_iter, &liter, l->v, BM_LOOPS_OF_VERT) {
    if (!uvedit_face_visible_test(scene, l_iter->f) || (l_iter->f == l->f)) {
      continue;
    }
    if (BM_loop_uv_share_vert_check(l, l_iter, offsets.uv) &&
        uvedit_face_select_test(scene, l_iter->f, offsets))
    {
      return true;
    }
  }
  return false;
}

bool uvedit_vert_is_all_other_faces_selected(const Scene *scene,
                                             BMLoop *l,
                                             const BMUVOffsets offsets)
{
  BLI_assert(offsets.uv >= 0);
  BMIter liter;
  BMLoop *l_iter;
  BM_ITER_ELEM (l_iter, &liter, l->v, BM_LOOPS_OF_VERT) {
    if (!uvedit_face_visible_test(scene, l_iter->f) || (l_iter->f == l->f)) {
      continue;
    }
    if (BM_loop_uv_share_vert_check(l, l_iter, offsets.uv) &&
        !uvedit_face_select_test(scene, l_iter->f, offsets))
    {
      return false;
    }
  }
  return true;
}

static void bm_clear_uv_vert_selection(const Scene *scene, BMesh *bm, const BMUVOffsets offsets)
{
  if (offsets.select_vert == -1) {
    return;
  }
  BMFace *efa;
  BMLoop *l;
  BMIter iter, liter;
  BM_ITER_MESH (efa, &iter, bm, BM_FACES_OF_MESH) {
    if (!uvedit_face_visible_test(scene, efa)) {
      continue;
    }
    BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
      BM_ELEM_CD_SET_BOOL(l, offsets.select_vert, false);
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name UV Select-Mode Flushing
 *
 * \{ */

void ED_uvedit_selectmode_flush(const Scene *scene, BMEditMesh *em)
{
  const ToolSettings *ts = scene->toolsettings;
  const char *active_uv_name = CustomData_get_active_layer_name(&em->bm->ldata, CD_PROP_FLOAT2);
  BM_uv_map_ensure_vert_select_attr(em->bm, active_uv_name);
  BM_uv_map_ensure_edge_select_attr(em->bm, active_uv_name);
  const BMUVOffsets offsets = BM_uv_map_get_offsets(em->bm);

  BLI_assert((ts->uv_flag & UV_SYNC_SELECTION) == 0);
  UNUSED_VARS_NDEBUG(ts);

  /* Vertex Mode only. */
  if (ts->uv_selectmode & UV_SELECT_VERTEX) {
    BMFace *efa;
    BMLoop *l;
    BMIter iter, liter;
    BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
      if (!uvedit_face_visible_test(scene, efa)) {
        continue;
      }
      BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
        bool edge_selected = BM_ELEM_CD_GET_BOOL(l, offsets.select_vert) &&
                             BM_ELEM_CD_GET_BOOL(l->next, offsets.select_vert);
        BM_ELEM_CD_SET_BOOL(l, offsets.select_edge, edge_selected);
      }
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name UV Flush selection (up/down)
 * \{ */

void uvedit_select_flush(const Scene *scene, BMEditMesh *em)
{
  /* Careful when using this in face select mode.
   * For face selections with sticky mode enabled, this can create invalid selection states. */
  const ToolSettings *ts = scene->toolsettings;
  const char *active_uv_name = CustomData_get_active_layer_name(&em->bm->ldata, CD_PROP_FLOAT2);
  BM_uv_map_ensure_edge_select_attr(em->bm, active_uv_name);
  BM_uv_map_ensure_vert_select_attr(em->bm, active_uv_name);
  const BMUVOffsets offsets = BM_uv_map_get_offsets(em->bm);

  BLI_assert((ts->uv_flag & UV_SYNC_SELECTION) == 0);
  UNUSED_VARS_NDEBUG(ts);

  BMFace *efa;
  BMLoop *l;
  BMIter iter, liter;
  BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
    if (!uvedit_face_visible_test(scene, efa)) {
      continue;
    }
    BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
      if (BM_ELEM_CD_GET_BOOL(l, offsets.select_vert) &&
          BM_ELEM_CD_GET_BOOL(l->next, offsets.select_vert))
      {
        BM_ELEM_CD_SET_BOOL(l, offsets.select_edge, true);
      }
    }
  }
}

void uvedit_deselect_flush(const Scene *scene, BMEditMesh *em)
{
  const ToolSettings *ts = scene->toolsettings;
  const char *active_uv_name = CustomData_get_active_layer_name(&em->bm->ldata, CD_PROP_FLOAT2);
  BM_uv_map_ensure_edge_select_attr(em->bm, active_uv_name);
  BM_uv_map_ensure_vert_select_attr(em->bm, active_uv_name);
  const BMUVOffsets offsets = BM_uv_map_get_offsets(em->bm);

  BLI_assert((ts->uv_flag & UV_SYNC_SELECTION) == 0);
  UNUSED_VARS_NDEBUG(ts);

  BMFace *efa;
  BMLoop *l;
  BMIter iter, liter;
  BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
    if (!uvedit_face_visible_test(scene, efa)) {
      continue;
    }
    BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
      if (!BM_ELEM_CD_GET_BOOL(l, offsets.select_vert) ||
          !BM_ELEM_CD_GET_BOOL(l->next, offsets.select_vert))
      {
        BM_ELEM_CD_SET_BOOL(l, offsets.select_edge, false);
      }
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Edge Loop Select
 * \{ */

/** Mode for selecting edge loops at boundaries. */
enum eUVEdgeLoopBoundaryMode {
  /** Delimit at face corners (don't walk over multiple edges in the same face). */
  UV_EDGE_LOOP_BOUNDARY_LOOP = 1,
  /** Don't delimit, walk over the all connected boundary loops. */
  UV_EDGE_LOOP_BOUNDARY_ALL = 2,
};

static BMLoop *bm_select_edgeloop_double_side_next(const Scene *scene,
                                                   BMLoop *l_step,
                                                   BMVert *v_from,
                                                   const BMUVOffsets offsets)
{
  if (l_step->f->len == 4) {
    BMVert *v_from_next = BM_edge_other_vert(l_step->e, v_from);
    BMLoop *l_step_over = (v_from == l_step->v) ? l_step->next : l_step->prev;
    l_step_over = uvedit_loop_find_other_radial_loop_with_visible_face(
        scene, l_step_over, offsets);
    if (l_step_over) {
      return (l_step_over->v == v_from_next) ? l_step_over->prev : l_step_over->next;
    }
  }
  return nullptr;
}

static BMLoop *bm_select_edgeloop_single_side_next(const Scene *scene,
                                                   BMLoop *l_step,
                                                   BMVert *v_from,
                                                   const BMUVOffsets offsets)
{
  BMVert *v_from_next = BM_edge_other_vert(l_step->e, v_from);
  return uvedit_loop_find_other_boundary_loop_with_visible_face(
      scene, l_step, v_from_next, offsets);
}

/* TODO(@ideasman42): support this in the BMesh API, as we have for clearing other types. */
static void bm_loop_tags_clear(BMesh *bm)
{
  BMIter iter;
  BMFace *f;
  BM_ITER_MESH (f, &iter, bm, BM_FACES_OF_MESH) {
    BMIter liter;
    BMLoop *l_iter;
    BM_ITER_ELEM (l_iter, &liter, f, BM_LOOPS_OF_FACE) {
      BM_elem_flag_disable(l_iter, BM_ELEM_TAG);
    }
  }
}

/**
 * Tag all loops which should be selected, the caller must select.
 */
static void uv_select_edgeloop_double_side_tag(const Scene *scene,
                                               BMEditMesh *em,
                                               BMLoop *l_init_pair[2],
                                               const BMUVOffsets offsets)
{
  bm_loop_tags_clear(em->bm);

  for (int side = 0; side < 2; side++) {
    BMLoop *l_step_pair[2] = {l_init_pair[0], l_init_pair[1]};
    BMVert *v_from = side ? l_step_pair[0]->e->v1 : l_step_pair[0]->e->v2;
    /* Disable since we start from the same edge. */
    BM_elem_flag_disable(l_step_pair[0], BM_ELEM_TAG);
    BM_elem_flag_disable(l_step_pair[1], BM_ELEM_TAG);
    while ((l_step_pair[0] != nullptr) && (l_step_pair[1] != nullptr)) {
      if (!uvedit_face_visible_test(scene, l_step_pair[0]->f) ||
          !uvedit_face_visible_test(scene, l_step_pair[1]->f) ||
          /* Check loops have not diverged. */
          (uvedit_loop_find_other_radial_loop_with_visible_face(scene, l_step_pair[0], offsets) !=
           l_step_pair[1]))
      {
        break;
      }

      BLI_assert(l_step_pair[0]->e == l_step_pair[1]->e);

      BM_elem_flag_enable(l_step_pair[0], BM_ELEM_TAG);
      BM_elem_flag_enable(l_step_pair[1], BM_ELEM_TAG);

      BMVert *v_from_next = BM_edge_other_vert(l_step_pair[0]->e, v_from);
      /* Walk over both sides, ensure they keep on the same edge. */
      for (int i = 0; i < ARRAY_SIZE(l_step_pair); i++) {
        l_step_pair[i] = bm_select_edgeloop_double_side_next(
            scene, l_step_pair[i], v_from, offsets);
      }

      if ((l_step_pair[0] && BM_elem_flag_test(l_step_pair[0], BM_ELEM_TAG)) ||
          (l_step_pair[1] && BM_elem_flag_test(l_step_pair[1], BM_ELEM_TAG)))
      {
        break;
      }
      v_from = v_from_next;
    }
  }
}

/**
 * Tag all loops which should be selected, the caller must select.
 *
 * \param r_count_by_select: Count the number of unselected and selected loops,
 * this is needed to implement cycling between #eUVEdgeLoopBoundaryMode.
 */
static void uv_select_edgeloop_single_side_tag(const Scene *scene,
                                               BMEditMesh *em,
                                               BMLoop *l_init,
                                               const BMUVOffsets offsets,
                                               enum eUVEdgeLoopBoundaryMode boundary_mode,
                                               int r_count_by_select[2])
{
  if (r_count_by_select) {
    r_count_by_select[0] = r_count_by_select[1] = 0;
  }

  bm_loop_tags_clear(em->bm);

  for (int side = 0; side < 2; side++) {
    BMLoop *l_step = l_init;
    BMVert *v_from = side ? l_step->e->v1 : l_step->e->v2;
    /* Disable since we start from the same edge. */
    BM_elem_flag_disable(l_step, BM_ELEM_TAG);
    while (l_step != nullptr) {

      if (!uvedit_face_visible_test(scene, l_step->f) ||
          /* Check the boundary is still a boundary. */
          (uvedit_loop_find_other_radial_loop_with_visible_face(scene, l_step, offsets) !=
           nullptr))
      {
        break;
      }

      if (r_count_by_select != nullptr) {
        r_count_by_select[uvedit_edge_select_test(scene, l_step, offsets)] += 1;
        /* Early exit when mixed could be optional if needed. */
        if (r_count_by_select[0] && r_count_by_select[1]) {
          r_count_by_select[0] = r_count_by_select[1] = -1;
          break;
        }
      }

      BM_elem_flag_enable(l_step, BM_ELEM_TAG);

      BMVert *v_from_next = BM_edge_other_vert(l_step->e, v_from);
      BMFace *f_step_prev = l_step->f;

      l_step = bm_select_edgeloop_single_side_next(scene, l_step, v_from, offsets);

      if (l_step && BM_elem_flag_test(l_step, BM_ELEM_TAG)) {
        break;
      }
      if (boundary_mode == UV_EDGE_LOOP_BOUNDARY_LOOP) {
        /* Don't allow walking over the face. */
        if (f_step_prev == l_step->f) {
          break;
        }
      }
      v_from = v_from_next;
    }
  }
}

static int uv_select_edgeloop(Scene *scene, Object *obedit, UvNearestHit *hit, const bool extend)
{
  const ToolSettings *ts = scene->toolsettings;
  BMEditMesh *em = BKE_editmesh_from_object(obedit);
  bool select;

  const char *active_uv_name = CustomData_get_active_layer_name(&em->bm->ldata, CD_PROP_FLOAT2);
  BM_uv_map_ensure_vert_select_attr(em->bm, active_uv_name);
  BM_uv_map_ensure_edge_select_attr(em->bm, active_uv_name);
  const BMUVOffsets offsets = BM_uv_map_get_offsets(em->bm);

  if (extend) {
    select = !uvedit_edge_select_test(scene, hit->l, offsets);
  }
  else {
    select = true;
  }

  BMLoop *l_init_pair[2] = {
      hit->l,
      uvedit_loop_find_other_radial_loop_with_visible_face(scene, hit->l, offsets),
  };

  /* When selecting boundaries, support cycling between selection modes. */
  enum eUVEdgeLoopBoundaryMode boundary_mode = UV_EDGE_LOOP_BOUNDARY_LOOP;

  /* Tag all loops that are part of the edge loop (select after).
   * This is done so we can */
  if (l_init_pair[1] == nullptr) {
    int count_by_select[2];
    /* If the loops selected toggle the boundaries. */
    uv_select_edgeloop_single_side_tag(
        scene, em, l_init_pair[0], offsets, boundary_mode, count_by_select);
    if (count_by_select[!select] == 0) {
      boundary_mode = UV_EDGE_LOOP_BOUNDARY_ALL;

      /* If the boundary is selected, toggle back to the loop. */
      uv_select_edgeloop_single_side_tag(
          scene, em, l_init_pair[0], offsets, boundary_mode, count_by_select);
      if (count_by_select[!select] == 0) {
        boundary_mode = UV_EDGE_LOOP_BOUNDARY_LOOP;
      }
    }
  }

  if (l_init_pair[1] == nullptr) {
    uv_select_edgeloop_single_side_tag(scene, em, l_init_pair[0], offsets, boundary_mode, nullptr);
  }
  else {
    uv_select_edgeloop_double_side_tag(scene, em, l_init_pair, offsets);
  }

  /* Apply the selection. */
  if (!extend) {
    uv_select_all_perform(scene, obedit, SEL_DESELECT);
  }

  /* Select all tagged loops. */
  {
    BMIter iter;
    BMFace *f;
    BM_ITER_MESH (f, &iter, em->bm, BM_FACES_OF_MESH) {
      BMIter liter;
      BMLoop *l_iter;
      BM_ITER_ELEM (l_iter, &liter, f, BM_LOOPS_OF_FACE) {
        if (BM_elem_flag_test(l_iter, BM_ELEM_TAG)) {
          if (ts->uv_selectmode == UV_SELECT_VERTEX) {
            uvedit_uv_select_set_with_sticky(scene, em, l_iter, select, false, offsets);
            uvedit_uv_select_set_with_sticky(scene, em, l_iter->next, select, false, offsets);
          }
          else {
            uvedit_edge_select_set_with_sticky(scene, em, l_iter, select, false, offsets);
          }
        }
      }
    }
  }

  return select ? 1 : -1;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Face Loop Select
 * \{ */

static int uv_select_faceloop(Scene *scene, Object *obedit, UvNearestHit *hit, const bool extend)
{
  BMEditMesh *em = BKE_editmesh_from_object(obedit);
  bool select;

  const char *active_uv_name = CustomData_get_active_layer_name(&em->bm->ldata, CD_PROP_FLOAT2);
  BM_uv_map_ensure_vert_select_attr(em->bm, active_uv_name);
  BM_uv_map_ensure_edge_select_attr(em->bm, active_uv_name);
  const BMUVOffsets offsets = BM_uv_map_get_offsets(em->bm);

  if (!extend) {
    uv_select_all_perform(scene, obedit, SEL_DESELECT);
  }

  BM_mesh_elem_hflag_disable_all(em->bm, BM_FACE, BM_ELEM_TAG, false);

  if (extend) {
    select = !uvedit_face_select_test(scene, hit->l->f, offsets);
  }
  else {
    select = true;
  }

  BMLoop *l_pair[2] = {
      hit->l,
      uvedit_loop_find_other_radial_loop_with_visible_face(scene, hit->l, offsets),
  };

  for (int side = 0; side < 2; side++) {
    BMLoop *l_step = l_pair[side];
    while (l_step) {
      if (!uvedit_face_visible_test(scene, l_step->f)) {
        break;
      }

      uvedit_face_select_set_with_sticky(scene, em, l_step->f, select, false, offsets);

      BM_elem_flag_enable(l_step->f, BM_ELEM_TAG);
      if (l_step->f->len == 4) {
        BMLoop *l_step_opposite = l_step->next->next;
        l_step = uvedit_loop_find_other_radial_loop_with_visible_face(
            scene, l_step_opposite, offsets);
      }
      else {
        l_step = nullptr;
      }

      /* Break iteration when `l_step`:
       * - is the first loop where we started from.
       * - tagged using #BM_ELEM_TAG (meaning this loop has been visited in this iteration). */
      if (l_step && BM_elem_flag_test(l_step->f, BM_ELEM_TAG)) {
        break;
      }
    }
  }

  return (select) ? 1 : -1;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Edge Ring Select
 * \{ */

static int uv_select_edgering(Scene *scene, Object *obedit, UvNearestHit *hit, const bool extend)
{
  const ToolSettings *ts = scene->toolsettings;
  BMEditMesh *em = BKE_editmesh_from_object(obedit);
  const bool use_face_select = (ts->uv_flag & UV_SYNC_SELECTION) ?
                                   (ts->selectmode & SCE_SELECT_FACE) :
                                   (ts->uv_selectmode & UV_SELECT_FACE);
  const bool use_vertex_select = (ts->uv_flag & UV_SYNC_SELECTION) ?
                                     (ts->selectmode & SCE_SELECT_VERTEX) :
                                     (ts->uv_selectmode & UV_SELECT_VERTEX);
  bool select;

  const char *active_uv_name = CustomData_get_active_layer_name(&em->bm->ldata, CD_PROP_FLOAT2);
  BM_uv_map_ensure_vert_select_attr(em->bm, active_uv_name);
  BM_uv_map_ensure_edge_select_attr(em->bm, active_uv_name);
  const BMUVOffsets offsets = BM_uv_map_get_offsets(em->bm);

  if (!extend) {
    uv_select_all_perform(scene, obedit, SEL_DESELECT);
  }

  BM_mesh_elem_hflag_disable_all(em->bm, BM_EDGE, BM_ELEM_TAG, false);

  if (extend) {
    select = !uvedit_edge_select_test(scene, hit->l, offsets);
  }
  else {
    select = true;
  }

  BMLoop *l_pair[2] = {
      hit->l,
      uvedit_loop_find_other_radial_loop_with_visible_face(scene, hit->l, offsets),
  };

  for (int side = 0; side < 2; side++) {
    BMLoop *l_step = l_pair[side];
    /* Disable since we start from the same edge. */
    BM_elem_flag_disable(hit->l->e, BM_ELEM_TAG);
    while (l_step) {
      if (!uvedit_face_visible_test(scene, l_step->f)) {
        break;
      }

      if (use_face_select) {
        /* While selecting face loops is now done in a separate function #uv_select_faceloop(),
         * this check is still kept for edge ring selection, to keep it consistent with how edge
         * ring selection works in face mode in the 3D viewport. */
        uvedit_face_select_set_with_sticky(scene, em, l_step->f, select, false, offsets);
      }
      else if (use_vertex_select) {
        uvedit_uv_select_set_with_sticky(scene, em, l_step, select, false, offsets);
        uvedit_uv_select_set_with_sticky(scene, em, l_step->next, select, false, offsets);
      }
      else {
        /* Edge select mode */
        uvedit_edge_select_set_with_sticky(scene, em, l_step, select, false, offsets);
      }

      BM_elem_flag_enable(l_step->e, BM_ELEM_TAG);
      if (l_step->f->len == 4) {
        BMLoop *l_step_opposite = l_step->next->next;
        l_step = uvedit_loop_find_other_radial_loop_with_visible_face(
            scene, l_step_opposite, offsets);
        if (l_step == nullptr) {
          /* Ensure we touch the opposite edge if we can't walk over it. */
          l_step = l_step_opposite;
        }
      }
      else {
        l_step = nullptr;
      }

      /* Break iteration when `l_step`:
       * - Is the first loop where we started from.
       * - Tagged using #BM_ELEM_TAG (meaning this loop has been visited in this iteration).
       * - Has its corresponding UV edge selected/unselected based on #select. */
      if (l_step && BM_elem_flag_test(l_step->e, BM_ELEM_TAG)) {
        /* Previously this check was not done and this resulted in the final edge in the edge ring
         * cycle to be skipped during selection (caused by old sticky selection behavior). */
        if (select && uvedit_edge_select_test(scene, l_step, offsets)) {
          break;
        }
        if (!select && !uvedit_edge_select_test(scene, l_step, offsets)) {
          break;
        }
      }
    }
  }

  return (select) ? 1 : -1;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Select Linked
 * \{ */

static void uv_select_linked_multi(Scene *scene,
                                   Object **objects,
                                   const uint objects_len,
                                   UvNearestHit *hit,
                                   const bool extend,
                                   bool deselect,
                                   const bool toggle,
                                   const bool select_faces)
{
  const bool uv_sync_select = (scene->toolsettings->uv_flag & UV_SYNC_SELECTION);

  /* loop over objects, or just use hit->ob */
  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    if (hit && ob_index != 0) {
      break;
    }
    Object *obedit = hit ? hit->ob : objects[ob_index];

    BMFace *efa;
    BMLoop *l;
    BMIter iter, liter;
    UvMapVert *vlist, *iterv, *startv;
    int i, stacksize = 0, *stack;
    uint a;
    char *flag;

    BMEditMesh *em = BKE_editmesh_from_object(obedit);
    const char *active_uv_name = CustomData_get_active_layer_name(&em->bm->ldata, CD_PROP_FLOAT2);
    BLI_assert(active_uv_name != nullptr);
    BM_uv_map_ensure_vert_select_attr(em->bm, active_uv_name);
    BM_uv_map_ensure_edge_select_attr(em->bm, active_uv_name);
    const BMUVOffsets offsets = BM_uv_map_get_offsets(em->bm);

    BM_mesh_elem_table_ensure(em->bm, BM_FACE); /* we can use this too */

    /* NOTE: we had 'use winding' so we don't consider overlapping islands as connected, see #44320
     * this made *every* projection split the island into front/back islands.
     * Keep 'use_winding' to false, see: #50970.
     *
     * Better solve this by having a delimit option for select-linked operator,
     * keeping island-select working as is. */
    UvVertMap *vmap = BM_uv_vert_map_create(em->bm, !uv_sync_select);
    if (vmap == nullptr) {
      continue;
    }

    stack = static_cast<int *>(MEM_mallocN(sizeof(*stack) * (em->bm->totface + 1), "UvLinkStack"));
    flag = static_cast<char *>(MEM_callocN(sizeof(*flag) * em->bm->totface, "UvLinkFlag"));

    if (hit == nullptr) {
      /* Use existing selection */
      BM_ITER_MESH_INDEX (efa, &iter, em->bm, BM_FACES_OF_MESH, a) {
        if (uvedit_face_visible_test(scene, efa)) {
          if (select_faces) {
            if (BM_elem_flag_test(efa, BM_ELEM_SELECT)) {
              stack[stacksize] = a;
              stacksize++;
              flag[a] = 1;
            }
          }
          else {
            BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
              if (uvedit_uv_select_test(scene, l, offsets)) {
                bool add_to_stack = true;
                if (uv_sync_select) {
                  /* Special case, vertex/edge & sync select being enabled.
                   *
                   * Without this, a second linked select will 'grow' each time as each new
                   * selection reaches the boundaries of islands that share vertices but not UVs.
                   *
                   * Rules applied here:
                   * - This loops face isn't selected.
                   * - The only other fully selected face is connected or,
                   * - There are no connected fully selected faces UV-connected to this loop.
                   */
                  BLI_assert(!select_faces);
                  if (uvedit_face_select_test(scene, l->f, offsets)) {
                    /* pass */
                  }
                  else {
                    BMIter liter_other;
                    BMLoop *l_other;
                    BM_ITER_ELEM (l_other, &liter_other, l->v, BM_LOOPS_OF_VERT) {
                      if ((l != l_other) && !BM_loop_uv_share_vert_check(l, l_other, offsets.uv) &&
                          uvedit_face_select_test(scene, l_other->f, offsets))
                      {
                        add_to_stack = false;
                        break;
                      }
                    }
                  }
                }

                if (add_to_stack) {
                  stack[stacksize] = a;
                  stacksize++;
                  flag[a] = 1;
                  break;
                }
              }
            }
          }
        }
      }
    }
    else {
      BM_ITER_MESH_INDEX (efa, &iter, em->bm, BM_FACES_OF_MESH, a) {
        if (efa == hit->efa) {
          stack[stacksize] = a;
          stacksize++;
          flag[a] = 1;
          break;
        }
      }
    }

    while (stacksize > 0) {

      stacksize--;
      a = stack[stacksize];

      efa = BM_face_at_index(em->bm, a);

      BM_ITER_ELEM_INDEX (l, &liter, efa, BM_LOOPS_OF_FACE, i) {

        /* make_uv_vert_map_EM sets verts tmp.l to the indices */
        vlist = BM_uv_vert_map_at_index(vmap, BM_elem_index_get(l->v));

        startv = vlist;

        for (iterv = vlist; iterv; iterv = iterv->next) {
          if (iterv->separate) {
            startv = iterv;
          }
          if (iterv->face_index == a) {
            break;
          }
        }

        for (iterv = startv; iterv; iterv = iterv->next) {
          if ((startv != iterv) && (iterv->separate)) {
            break;
          }
          if (!flag[iterv->face_index]) {
            flag[iterv->face_index] = 1;
            stack[stacksize] = iterv->face_index;
            stacksize++;
          }
        }
      }
    }

    /* Toggling - if any of the linked vertices is selected (and visible), we deselect. */
    if ((toggle == true) && (extend == false) && (deselect == false)) {
      BM_ITER_MESH_INDEX (efa, &iter, em->bm, BM_FACES_OF_MESH, a) {
        bool found_selected = false;
        if (!flag[a]) {
          continue;
        }

        if (select_faces) {
          if (BM_elem_flag_test(efa, BM_ELEM_SELECT) && !BM_elem_flag_test(efa, BM_ELEM_HIDDEN)) {
            found_selected = true;
          }
        }
        else {
          BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
            if (uvedit_uv_select_test(scene, l, offsets)) {
              found_selected = true;
              break;
            }
          }

          if (found_selected) {
            deselect = true;
            break;
          }
        }
      }
    }

#define SET_SELECTION(value) \
  if (select_faces) { \
    BM_face_select_set(em->bm, efa, value); \
  } \
  else { \
    uvedit_face_select_set(scene, em->bm, efa, value, false, offsets); \
  } \
  (void)0

    BM_ITER_MESH_INDEX (efa, &iter, em->bm, BM_FACES_OF_MESH, a) {
      if (!flag[a]) {
        if (!extend && !deselect && !toggle) {
          SET_SELECTION(false);
        }
        continue;
      }

      if (!deselect) {
        SET_SELECTION(true);
      }
      else {
        SET_SELECTION(false);
      }
    }

#undef SET_SELECTION

    MEM_freeN(stack);
    MEM_freeN(flag);
    BM_uv_vert_map_free(vmap);

    if (uv_sync_select) {
      if (deselect) {
        EDBM_deselect_flush(em);
      }
      else {
        if (!select_faces) {
          EDBM_selectmode_flush(em);
        }
      }
    }
  }
}

const float *uvedit_first_selected_uv_from_vertex(Scene *scene,
                                                  BMVert *eve,
                                                  const BMUVOffsets offsets)
{
  BMIter liter;
  BMLoop *l;

  BM_ITER_ELEM (l, &liter, eve, BM_LOOPS_OF_VERT) {
    if (!uvedit_face_visible_test(scene, l->f)) {
      continue;
    }

    if (uvedit_uv_select_test(scene, l, offsets)) {
      float *luv = BM_ELEM_CD_GET_FLOAT_P(l, offsets.uv);
      return luv;
    }
  }

  return nullptr;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Select More/Less Operator
 * \{ */

static int uv_select_more_less(bContext *C, const bool select)
{
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);

  BMFace *efa;
  BMLoop *l;
  BMIter iter, liter;
  const ToolSettings *ts = scene->toolsettings;

  uint objects_len = 0;
  Object **objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data_with_uvs(
      scene, view_layer, ((View3D *)nullptr), &objects_len);

  const bool is_uv_face_selectmode = (ts->uv_selectmode == UV_SELECT_FACE);

  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    Object *obedit = objects[ob_index];
    BMEditMesh *em = BKE_editmesh_from_object(obedit);

    bool changed = false;

    const char *active_uv_name = CustomData_get_active_layer_name(&em->bm->ldata, CD_PROP_FLOAT2);
    BM_uv_map_ensure_vert_select_attr(em->bm, active_uv_name);
    BM_uv_map_ensure_edge_select_attr(em->bm, active_uv_name);
    const BMUVOffsets offsets = BM_uv_map_get_offsets(em->bm);

    if (ts->uv_flag & UV_SYNC_SELECTION) {
      if (select) {
        EDBM_select_more(em, true);
      }
      else {
        EDBM_select_less(em, true);
      }

      DEG_id_tag_update(static_cast<ID *>(obedit->data), ID_RECALC_SELECT);
      WM_event_add_notifier(C, NC_GEOM | ND_SELECT, obedit->data);
      continue;
    }

    if (is_uv_face_selectmode) {

      /* clear tags */
      BM_mesh_elem_hflag_disable_all(em->bm, BM_FACE, BM_ELEM_TAG, false);

      /* mark loops to be selected */
      BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
        if (uvedit_face_visible_test(scene, efa)) {

          if (select) {
#define NEIGHBORING_FACE_IS_SEL 1
#define CURR_FACE_IS_UNSEL 2

            int sel_state = 0;

            BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
              if (BM_ELEM_CD_GET_BOOL(l, offsets.select_vert)) {
                sel_state |= NEIGHBORING_FACE_IS_SEL;
              }
              else {
                sel_state |= CURR_FACE_IS_UNSEL;
              }

              if (!BM_ELEM_CD_GET_BOOL(l, offsets.select_edge)) {
                sel_state |= CURR_FACE_IS_UNSEL;
              }

              /* If the current face is not selected and at least one neighboring face is
               * selected, then tag the current face to grow selection. */
              if (sel_state == (NEIGHBORING_FACE_IS_SEL | CURR_FACE_IS_UNSEL)) {
                BM_elem_flag_enable(efa, BM_ELEM_TAG);
                changed = true;
                break;
              }
            }

#undef NEIGHBORING_FACE_IS_SEL
#undef CURR_FACE_IS_UNSEL
          }
          else {
            if (!uvedit_face_select_test(scene, efa, offsets)) {
              continue;
            }
            BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
              /* Deselect face when at least one of the surrounding faces is not selected */
              if (!uvedit_vert_is_all_other_faces_selected(scene, l, offsets)) {
                BM_elem_flag_enable(efa, BM_ELEM_TAG);
                changed = true;
                break;
              }
            }
          }
        }
      }
    }
    else {

      /* clear tags */
      BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
        BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
          BM_elem_flag_disable(l, BM_ELEM_TAG);
        }
      }

      /* mark loops to be selected */
      BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
        if (uvedit_face_visible_test(scene, efa)) {
          BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {

            if (BM_ELEM_CD_GET_BOOL(l, offsets.select_vert) == select) {
              BM_elem_flag_enable(l->next, BM_ELEM_TAG);
              BM_elem_flag_enable(l->prev, BM_ELEM_TAG);
              changed = true;
            }
          }
        }
      }
    }

    if (changed) {
      if (is_uv_face_selectmode) {
        /* Select tagged faces. */
        uv_select_flush_from_tag_face(scene, obedit, select);
      }
      else {
        /* Select tagged loops. */
        uv_select_flush_from_tag_loop(scene, obedit, select);
        /* Set/unset edge flags based on selected verts. */
        if (select) {
          uvedit_select_flush(scene, em);
        }
        else {
          uvedit_deselect_flush(scene, em);
        }
      }
      DEG_id_tag_update(static_cast<ID *>(obedit->data), ID_RECALC_SELECT);
      WM_event_add_notifier(C, NC_GEOM | ND_SELECT, obedit->data);
    }
  }
  MEM_freeN(objects);

  return OPERATOR_FINISHED;
}

static int uv_select_more_exec(bContext *C, wmOperator * /*op*/)
{
  return uv_select_more_less(C, true);
}

void UV_OT_select_more(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Select More";
  ot->description = "Select more UV vertices connected to initial selection";
  ot->idname = "UV_OT_select_more";
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* api callbacks */
  ot->exec = uv_select_more_exec;
  ot->poll = ED_operator_uvedit_space_image;
}

static int uv_select_less_exec(bContext *C, wmOperator * /*op*/)
{
  return uv_select_more_less(C, false);
}

void UV_OT_select_less(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Select Less";
  ot->description = "Deselect UV vertices at the boundary of each selection region";
  ot->idname = "UV_OT_select_less";
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* api callbacks */
  ot->exec = uv_select_less_exec;
  ot->poll = ED_operator_uvedit_space_image;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name (De)Select All Operator
 * \{ */

bool uvedit_select_is_any_selected(const Scene *scene, Object *obedit)
{
  const ToolSettings *ts = scene->toolsettings;
  BMEditMesh *em = BKE_editmesh_from_object(obedit);
  BMFace *efa;
  BMLoop *l;
  BMIter iter, liter;

  if (ts->uv_flag & UV_SYNC_SELECTION) {
    return (em->bm->totvertsel || em->bm->totedgesel || em->bm->totfacesel);
  }

  const BMUVOffsets offsets = BM_uv_map_get_offsets(em->bm);

  BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
    if (!uvedit_face_visible_test(scene, efa)) {
      continue;
    }
    BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
      if (BM_ELEM_CD_GET_BOOL(l, offsets.select_vert)) {
        return true;
      }
    }
  }
  return false;
}

bool uvedit_select_is_any_selected_multi(const Scene *scene,
                                         Object **objects,
                                         const uint objects_len)
{
  bool found = false;
  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    Object *obedit = objects[ob_index];
    if (uvedit_select_is_any_selected(scene, obedit)) {
      found = true;
      break;
    }
  }
  return found;
}

static void uv_select_all(const Scene *scene, BMEditMesh *em, bool select_all)
{
  BMFace *efa;
  BMLoop *l;
  BMIter iter, liter;
  const char *active_uv_name = CustomData_get_active_layer_name(&em->bm->ldata, CD_PROP_FLOAT2);
  BM_uv_map_ensure_vert_select_attr(em->bm, active_uv_name);
  BM_uv_map_ensure_edge_select_attr(em->bm, active_uv_name);
  const BMUVOffsets offsets = BM_uv_map_get_offsets(em->bm);

  BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
    if (!uvedit_face_visible_test(scene, efa)) {
      continue;
    }
    BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
      BM_ELEM_CD_SET_BOOL(l, offsets.select_vert, select_all);
      BM_ELEM_CD_SET_BOOL(l, offsets.select_edge, select_all);
    }
  }
}

static void uv_select_invert(const Scene *scene, BMEditMesh *em)
{
  const ToolSettings *ts = scene->toolsettings;
  BLI_assert((ts->uv_flag & UV_SYNC_SELECTION) == 0);

  const char *active_uv_name = CustomData_get_active_layer_name(&em->bm->ldata, CD_PROP_FLOAT2);
  BM_uv_map_ensure_vert_select_attr(em->bm, active_uv_name);
  BM_uv_map_ensure_edge_select_attr(em->bm, active_uv_name);
  const BMUVOffsets offsets = BM_uv_map_get_offsets(em->bm);
  BMFace *efa;
  BMLoop *l;
  BMIter iter, liter;
  char uv_selectmode = ts->uv_selectmode;
  BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
    if (!uvedit_face_visible_test(scene, efa)) {
      continue;
    }
    BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
      if (ELEM(uv_selectmode, UV_SELECT_EDGE, UV_SELECT_FACE)) {
        /* Use UV edge selection to find vertices and edges that must be selected. */
        bool es = BM_ELEM_CD_GET_BOOL(l, offsets.select_edge);
        BM_ELEM_CD_SET_BOOL(l, offsets.select_edge, !es);
        BM_ELEM_CD_SET_BOOL(l, offsets.select_vert, false);
      }
      /* Use UV vertex selection to find vertices and edges that must be selected. */
      else if (ELEM(uv_selectmode, UV_SELECT_VERTEX, UV_SELECT_ISLAND)) {
        bool vs = BM_ELEM_CD_GET_BOOL(l, offsets.select_vert);
        BM_ELEM_CD_SET_BOOL(l, offsets.select_vert, !vs);
        BM_ELEM_CD_SET_BOOL(l, offsets.select_edge, false);
      }
    }
  }

  /* Flush based on uv vert/edge flags and current UV select mode */
  if (ELEM(uv_selectmode, UV_SELECT_EDGE, UV_SELECT_FACE)) {
    uv_select_flush_from_loop_edge_flag(scene, em);
  }
  else if (ELEM(uv_selectmode, UV_SELECT_VERTEX, UV_SELECT_ISLAND)) {
    uvedit_select_flush(scene, em);
  }
}

static void uv_select_all_perform(const Scene *scene, Object *obedit, int action)
{
  const ToolSettings *ts = scene->toolsettings;
  BMEditMesh *em = BKE_editmesh_from_object(obedit);

  if (action == SEL_TOGGLE) {
    action = uvedit_select_is_any_selected(scene, obedit) ? SEL_DESELECT : SEL_SELECT;
  }

  if (ts->uv_flag & UV_SYNC_SELECTION) {
    switch (action) {
      case SEL_TOGGLE:
        EDBM_select_toggle_all(em);
        break;
      case SEL_SELECT:
        EDBM_flag_enable_all(em, BM_ELEM_SELECT);
        break;
      case SEL_DESELECT:
        EDBM_flag_disable_all(em, BM_ELEM_SELECT);
        break;
      case SEL_INVERT:
        EDBM_select_swap(em);
        EDBM_selectmode_flush(em);
        break;
    }
  }
  else {
    switch (action) {
      case SEL_SELECT:
        uv_select_all(scene, em, true);
        break;
      case SEL_DESELECT:
        uv_select_all(scene, em, false);
        break;
      case SEL_INVERT:
        uv_select_invert(scene, em);
        break;
    }
  }
}

static void uv_select_all_perform_multi_ex(const Scene *scene,
                                           Object **objects,
                                           const uint objects_len,
                                           int action,
                                           const Object *ob_exclude)
{
  if (action == SEL_TOGGLE) {
    action = uvedit_select_is_any_selected_multi(scene, objects, objects_len) ? SEL_DESELECT :
                                                                                SEL_SELECT;
  }

  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    Object *obedit = objects[ob_index];
    if (ob_exclude && (obedit == ob_exclude)) {
      continue;
    }
    uv_select_all_perform(scene, obedit, action);
  }
}

static void uv_select_all_perform_multi(const Scene *scene,
                                        Object **objects,
                                        const uint objects_len,
                                        int action)
{
  uv_select_all_perform_multi_ex(scene, objects, objects_len, action, nullptr);
}

static int uv_select_all_exec(bContext *C, wmOperator *op)
{
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  Scene *scene = CTX_data_scene(C);
  const ToolSettings *ts = scene->toolsettings;
  ViewLayer *view_layer = CTX_data_view_layer(C);

  int action = RNA_enum_get(op->ptr, "action");

  uint objects_len = 0;
  Object **objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data_with_uvs(
      scene, view_layer, ((View3D *)nullptr), &objects_len);

  uv_select_all_perform_multi(scene, objects, objects_len, action);

  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    Object *obedit = objects[ob_index];
    uv_select_tag_update_for_object(depsgraph, ts, obedit);
  }

  MEM_freeN(objects);

  return OPERATOR_FINISHED;
}

void UV_OT_select_all(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "(De)select All";
  ot->description = "Change selection of all UV vertices";
  ot->idname = "UV_OT_select_all";
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* api callbacks */
  ot->exec = uv_select_all_exec;
  ot->poll = ED_operator_uvedit;

  WM_operator_properties_select_all(ot);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Mouse Select Operator
 * \{ */

static bool uv_mouse_select_multi(bContext *C,
                                  Object **objects,
                                  uint objects_len,
                                  const float co[2],
                                  const SelectPick_Params *params)
{
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  const ARegion *region = CTX_wm_region(C);
  Scene *scene = CTX_data_scene(C);
  const ToolSettings *ts = scene->toolsettings;
  UvNearestHit hit = uv_nearest_hit_init_dist_px(&region->v2d, 75.0f);
  int selectmode, sticky;
  bool found_item = false;
  /* 0 == don't flush, 1 == sel, -1 == deselect;  only use when selection sync is enabled. */
  int flush = 0;

  /* Penalty (in pixels) applied to elements that are already selected
   * so elements that aren't already selected are prioritized. */
  const float penalty_dist = 3.0f * U.pixelsize;

  /* retrieve operation mode */
  if (ts->uv_flag & UV_SYNC_SELECTION) {
    if (ts->selectmode & SCE_SELECT_FACE) {
      selectmode = UV_SELECT_FACE;
    }
    else if (ts->selectmode & SCE_SELECT_EDGE) {
      selectmode = UV_SELECT_EDGE;
    }
    else {
      selectmode = UV_SELECT_VERTEX;
    }

    sticky = SI_STICKY_DISABLE;
  }
  else {
    selectmode = ts->uv_selectmode;
    sticky = ts->uv_sticky;
  }

  /* find nearest element */
  if (selectmode == UV_SELECT_VERTEX) {
    /* find vertex */
    found_item = uv_find_nearest_vert_multi(scene, objects, objects_len, co, penalty_dist, &hit);
    if (found_item) {
      if ((ts->uv_flag & UV_SYNC_SELECTION) == 0) {
        BMesh *bm = BKE_editmesh_from_object(hit.ob)->bm;
        ED_uvedit_active_vert_loop_set(bm, hit.l);
      }
    }
  }
  else if (selectmode == UV_SELECT_EDGE) {
    /* find edge */
    found_item = uv_find_nearest_edge_multi(scene, objects, objects_len, co, penalty_dist, &hit);
    if (found_item) {
      if ((ts->uv_flag & UV_SYNC_SELECTION) == 0) {
        BMesh *bm = BKE_editmesh_from_object(hit.ob)->bm;
        ED_uvedit_active_edge_loop_set(bm, hit.l);
      }
    }
  }
  else if (selectmode == UV_SELECT_FACE) {
    /* find face */
    found_item = uv_find_nearest_face_multi(scene, objects, objects_len, co, &hit);

    if (!found_item) {
      /* Fallback, perform a second pass without a limited threshold,
       * which succeeds as long as the cursor is inside the UV face.
       * Useful when zoomed in, to select faces with distant screen-space face centers. */
      hit.dist_sq = FLT_MAX;
      found_item = uv_find_nearest_face_multi_ex(scene, objects, objects_len, co, &hit, true);
    }

    if (found_item) {
      BMesh *bm = BKE_editmesh_from_object(hit.ob)->bm;
      BM_mesh_active_face_set(bm, hit.efa);
    }
  }
  else if (selectmode == UV_SELECT_ISLAND) {
    found_item = uv_find_nearest_edge_multi(scene, objects, objects_len, co, 0.0f, &hit);

    if (!found_item) {
      /* Without this, we can be within the face of an island but too far from an edge,
       * see face selection comment for details. */
      hit.dist_sq = FLT_MAX;
      found_item = uv_find_nearest_face_multi_ex(scene, objects, objects_len, co, &hit, true);
    }
  }

  bool found = found_item;
  bool changed = false;

  bool is_selected = false;
  if (found) {
    Object *obedit = hit.ob;
    BMEditMesh *em = BKE_editmesh_from_object(obedit);
    const char *active_uv_name = CustomData_get_active_layer_name(&em->bm->ldata, CD_PROP_FLOAT2);
    BM_uv_map_ensure_vert_select_attr(em->bm, active_uv_name);
    BM_uv_map_ensure_edge_select_attr(em->bm, active_uv_name);
    const BMUVOffsets offsets = BM_uv_map_get_offsets(em->bm);
    if (selectmode == UV_SELECT_FACE) {
      is_selected = uvedit_face_select_test(scene, hit.efa, offsets);
    }
    else if (selectmode == UV_SELECT_EDGE) {
      is_selected = uvedit_edge_select_test(scene, hit.l, offsets);
    }
    else {
      /* Vertex or island. For island (if we were using #uv_find_nearest_face_multi_ex, see above),
       * `hit.l` is null, use `hit.efa` instead. */
      if (hit.l != nullptr) {
        is_selected = uvedit_uv_select_test(scene, hit.l, offsets);
      }
      else {
        is_selected = uvedit_face_select_test(scene, hit.efa, offsets);
      }
    }
  }

  if (params->sel_op == SEL_OP_SET) {
    if ((found && params->select_passthrough) && is_selected) {
      found = false;
    }
    else if (found || params->deselect_all) {
      /* Deselect everything. */
      uv_select_all_perform_multi(scene, objects, objects_len, SEL_DESELECT);
      for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
        Object *obedit = objects[ob_index];
        uv_select_tag_update_for_object(depsgraph, ts, obedit);
      }
      changed = true;
    }
  }

  if (found) {
    Object *obedit = hit.ob;
    BMEditMesh *em = BKE_editmesh_from_object(obedit);
    const char *active_uv_name = CustomData_get_active_layer_name(&em->bm->ldata, CD_PROP_FLOAT2);
    BM_uv_map_ensure_vert_select_attr(em->bm, active_uv_name);
    BM_uv_map_ensure_edge_select_attr(em->bm, active_uv_name);
    const BMUVOffsets offsets = BM_uv_map_get_offsets(em->bm);

    if (selectmode == UV_SELECT_ISLAND) {
      const bool extend = params->sel_op == SEL_OP_ADD;
      const bool deselect = params->sel_op == SEL_OP_SUB;
      const bool toggle = params->sel_op == SEL_OP_XOR;
      /* Current behavior of 'extend'
       * is actually toggling, so pass extend flag as 'toggle' here */
      uv_select_linked_multi(scene, objects, objects_len, &hit, extend, deselect, toggle, false);
      /* TODO: check if this actually changed. */
      changed = true;
    }
    else {
      BLI_assert(ELEM(selectmode, UV_SELECT_VERTEX, UV_SELECT_EDGE, UV_SELECT_FACE));
      bool select_value = false;
      switch (params->sel_op) {
        case SEL_OP_ADD: {
          select_value = true;
          break;
        }
        case SEL_OP_SUB: {
          select_value = false;
          break;
        }
        case SEL_OP_XOR: {
          select_value = !is_selected;
          break;
        }
        case SEL_OP_SET: {
          /* Deselect has already been performed. */
          select_value = true;
          break;
        }
        case SEL_OP_AND: {
          BLI_assert_unreachable(); /* Doesn't make sense for picking. */
          break;
        }
      }

      if (selectmode == UV_SELECT_FACE) {
        uvedit_face_select_set_with_sticky(scene, em, hit.efa, select_value, true, offsets);
        flush = 1;
      }
      else if (selectmode == UV_SELECT_EDGE) {
        uvedit_edge_select_set_with_sticky(scene, em, hit.l, select_value, true, offsets);
        flush = 1;
      }
      else if (selectmode == UV_SELECT_VERTEX) {
        uvedit_uv_select_set_with_sticky(scene, em, hit.l, select_value, true, offsets);
        flush = 1;
      }
      else {
        BLI_assert_unreachable();
      }

      /* De-selecting an edge may deselect a face too - validate. */
      if (ts->uv_flag & UV_SYNC_SELECTION) {
        if (select_value == false) {
          BM_select_history_validate(em->bm);
        }
      }

      /* (de)select sticky UV nodes. */
      if (sticky != SI_STICKY_DISABLE) {
        flush = select_value ? 1 : -1;
      }

      changed = true;
    }

    if (ts->uv_flag & UV_SYNC_SELECTION) {
      if (flush != 0) {
        EDBM_selectmode_flush(em);
      }
    }
    else {
      /* Setting the selection implies a single element, which doesn't need to be flushed. */
      if (params->sel_op != SEL_OP_SET) {
        ED_uvedit_selectmode_flush(scene, em);
      }
    }
  }

  if (changed && found) {
    /* Only update the `hit` object as de-selecting all will have refreshed the others. */
    Object *obedit = hit.ob;
    uv_select_tag_update_for_object(depsgraph, ts, obedit);
  }

  return changed || found;
}
static bool uv_mouse_select(bContext *C, const float co[2], const SelectPick_Params *params)
{
  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  uint objects_len = 0;
  Object **objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data_with_uvs(
      scene, view_layer, ((View3D *)nullptr), &objects_len);
  bool changed = uv_mouse_select_multi(C, objects, objects_len, co, params);
  MEM_freeN(objects);
  return changed;
}

static int uv_select_exec(bContext *C, wmOperator *op)
{
  float co[2];

  RNA_float_get_array(op->ptr, "location", co);

  SelectPick_Params params{};
  ED_select_pick_params_from_operator(op->ptr, &params);

  const bool changed = uv_mouse_select(C, co, &params);

  if (changed) {
    return OPERATOR_FINISHED | OPERATOR_PASS_THROUGH;
  }
  return OPERATOR_CANCELLED | OPERATOR_PASS_THROUGH;
}

static int uv_select_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  const ARegion *region = CTX_wm_region(C);
  float co[2];

  UI_view2d_region_to_view(&region->v2d, event->mval[0], event->mval[1], &co[0], &co[1]);
  RNA_float_set_array(op->ptr, "location", co);

  const int retval = uv_select_exec(C, op);

  return WM_operator_flag_only_pass_through_on_press(retval, event);
}

void UV_OT_select(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Select";
  ot->description = "Select UV vertices";
  ot->idname = "UV_OT_select";
  ot->flag = OPTYPE_UNDO;

  /* api callbacks */
  ot->exec = uv_select_exec;
  ot->invoke = uv_select_invoke;
  ot->poll = ED_operator_uvedit; /* requires space image */
  ot->get_name = ED_select_pick_get_name;

  /* properties */
  PropertyRNA *prop;

  WM_operator_properties_mouse_select(ot);

  prop = RNA_def_float_vector(
      ot->srna,
      "location",
      2,
      nullptr,
      -FLT_MAX,
      FLT_MAX,
      "Location",
      "Mouse location in normalized coordinates, 0.0 to 1.0 is within the image bounds",
      -100.0f,
      100.0f);
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Shared Edge Loop/Ring Select Operator Functions
 * \{ */

enum eUVLoopGenericType {
  UV_LOOP_SELECT = 1,
  UV_RING_SELECT = 2,
};

static int uv_mouse_select_loop_generic_multi(bContext *C,
                                              Object **objects,
                                              uint objects_len,
                                              const float co[2],
                                              const bool extend,
                                              enum eUVLoopGenericType loop_type)
{
  const ARegion *region = CTX_wm_region(C);
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  Scene *scene = CTX_data_scene(C);
  const ToolSettings *ts = scene->toolsettings;
  UvNearestHit hit = uv_nearest_hit_init_max(&region->v2d);
  bool found_item = false;
  /* 0 == don't flush, 1 == sel, -1 == deselect;  only use when selection sync is enabled. */
  int flush = 0;

  /* Find edge. */
  found_item = uv_find_nearest_edge_multi(scene, objects, objects_len, co, 0.0f, &hit);
  if (!found_item) {
    return OPERATOR_CANCELLED;
  }

  Object *obedit = hit.ob;
  BMEditMesh *em = BKE_editmesh_from_object(obedit);

  /* Do selection. */
  if (!extend) {
    uv_select_all_perform_multi_ex(scene, objects, objects_len, SEL_DESELECT, obedit);
  }

  if (loop_type == UV_LOOP_SELECT) {
    if (ED_uvedit_select_mode_get(scene) == UV_SELECT_FACE) {
      flush = uv_select_faceloop(scene, obedit, &hit, extend);
    }
    else {
      flush = uv_select_edgeloop(scene, obedit, &hit, extend);
    }
  }
  else if (loop_type == UV_RING_SELECT) {
    flush = uv_select_edgering(scene, obedit, &hit, extend);
  }
  else {
    BLI_assert_unreachable();
  }

  if (ts->uv_flag & UV_SYNC_SELECTION) {
    if (flush == 1) {
      EDBM_select_flush(em);
    }
    else if (flush == -1) {
      EDBM_deselect_flush(em);
    }
  }
  else {
    ED_uvedit_selectmode_flush(scene, em);
  }

  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    Object *obiter = objects[ob_index];
    uv_select_tag_update_for_object(depsgraph, ts, obiter);
  }

  return OPERATOR_PASS_THROUGH | OPERATOR_FINISHED;
}
static int uv_mouse_select_loop_generic(bContext *C,
                                        const float co[2],
                                        const bool extend,
                                        enum eUVLoopGenericType loop_type)
{
  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  uint objects_len = 0;
  Object **objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data_with_uvs(
      scene, view_layer, ((View3D *)nullptr), &objects_len);
  int ret = uv_mouse_select_loop_generic_multi(C, objects, objects_len, co, extend, loop_type);
  MEM_freeN(objects);
  return ret;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Edge Loop Select Operator
 * \{ */

static int uv_select_loop_exec(bContext *C, wmOperator *op)
{
  float co[2];

  RNA_float_get_array(op->ptr, "location", co);
  const bool extend = RNA_boolean_get(op->ptr, "extend");

  return uv_mouse_select_loop_generic(C, co, extend, UV_LOOP_SELECT);
}

static int uv_select_loop_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  const ARegion *region = CTX_wm_region(C);
  float co[2];

  UI_view2d_region_to_view(&region->v2d, event->mval[0], event->mval[1], &co[0], &co[1]);
  RNA_float_set_array(op->ptr, "location", co);

  const int retval = uv_select_loop_exec(C, op);

  return WM_operator_flag_only_pass_through_on_press(retval, event);
}

void UV_OT_select_loop(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Loop Select";
  ot->description = "Select a loop of connected UV vertices";
  ot->idname = "UV_OT_select_loop";
  ot->flag = OPTYPE_UNDO;

  /* api callbacks */
  ot->exec = uv_select_loop_exec;
  ot->invoke = uv_select_loop_invoke;
  ot->poll = ED_operator_uvedit; /* requires space image */

  /* properties */
  PropertyRNA *prop;
  prop = RNA_def_boolean(ot->srna,
                         "extend",
                         false,
                         "Extend",
                         "Extend selection rather than clearing the existing selection");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
  prop = RNA_def_float_vector(
      ot->srna,
      "location",
      2,
      nullptr,
      -FLT_MAX,
      FLT_MAX,
      "Location",
      "Mouse location in normalized coordinates, 0.0 to 1.0 is within the image bounds",
      -100.0f,
      100.0f);
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Edge Ring Select Operator
 * \{ */

static int uv_select_edge_ring_exec(bContext *C, wmOperator *op)
{
  float co[2];
  RNA_float_get_array(op->ptr, "location", co);
  const bool extend = RNA_boolean_get(op->ptr, "extend");
  return uv_mouse_select_loop_generic(C, co, extend, UV_RING_SELECT);
}

static int uv_select_edge_ring_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  const ARegion *region = CTX_wm_region(C);
  float co[2];

  UI_view2d_region_to_view(&region->v2d, event->mval[0], event->mval[1], &co[0], &co[1]);
  RNA_float_set_array(op->ptr, "location", co);

  const int retval = uv_select_edge_ring_exec(C, op);

  return WM_operator_flag_only_pass_through_on_press(retval, event);
}

void UV_OT_select_edge_ring(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Edge Ring Select";
  ot->description = "Select an edge ring of connected UV vertices";
  ot->idname = "UV_OT_select_edge_ring";
  ot->flag = OPTYPE_UNDO;

  /* api callbacks */
  ot->exec = uv_select_edge_ring_exec;
  ot->invoke = uv_select_edge_ring_invoke;
  ot->poll = ED_operator_uvedit; /* requires space image */

  /* properties */
  PropertyRNA *prop;
  prop = RNA_def_boolean(ot->srna,
                         "extend",
                         false,
                         "Extend",
                         "Extend selection rather than clearing the existing selection");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
  prop = RNA_def_float_vector(
      ot->srna,
      "location",
      2,
      nullptr,
      -FLT_MAX,
      FLT_MAX,
      "Location",
      "Mouse location in normalized coordinates, 0.0 to 1.0 is within the image bounds",
      -100.0f,
      100.0f);
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Select Linked Operator
 * \{ */

static int uv_select_linked_internal(bContext *C, wmOperator *op, const wmEvent *event, bool pick)
{
  const ARegion *region = CTX_wm_region(C);
  Scene *scene = CTX_data_scene(C);
  const ToolSettings *ts = scene->toolsettings;
  ViewLayer *view_layer = CTX_data_view_layer(C);
  bool extend = true;
  bool deselect = false;
  bool select_faces = (ts->uv_flag & UV_SYNC_SELECTION) && (ts->selectmode & SCE_SELECT_FACE);

  UvNearestHit hit = uv_nearest_hit_init_max(&region->v2d);

  if (pick) {
    extend = RNA_boolean_get(op->ptr, "extend");
    deselect = RNA_boolean_get(op->ptr, "deselect");
  }

  uint objects_len = 0;
  Object **objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data_with_uvs(
      scene, view_layer, ((View3D *)nullptr), &objects_len);

  if (pick) {
    float co[2];

    if (event) {
      /* invoke */
      UI_view2d_region_to_view(&region->v2d, event->mval[0], event->mval[1], &co[0], &co[1]);
      RNA_float_set_array(op->ptr, "location", co);
    }
    else {
      /* exec */
      RNA_float_get_array(op->ptr, "location", co);
    }

    if (!uv_find_nearest_edge_multi(scene, objects, objects_len, co, 0.0f, &hit)) {
      MEM_freeN(objects);
      return OPERATOR_CANCELLED;
    }
  }

  if (!extend && !deselect) {
    uv_select_all_perform_multi(scene, objects, objects_len, SEL_DESELECT);
  }

  uv_select_linked_multi(
      scene, objects, objects_len, pick ? &hit : nullptr, extend, deselect, false, select_faces);

  /* weak!, but works */
  Object **objects_free = objects;
  if (pick) {
    objects = &hit.ob;
    objects_len = 1;
  }

  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    Object *obedit = objects[ob_index];
    DEG_id_tag_update(static_cast<ID *>(obedit->data), ID_RECALC_COPY_ON_WRITE | ID_RECALC_SELECT);
    WM_event_add_notifier(C, NC_GEOM | ND_SELECT, obedit->data);
  }

  MEM_SAFE_FREE(objects_free);

  return OPERATOR_FINISHED;
}

static int uv_select_linked_exec(bContext *C, wmOperator *op)
{
  return uv_select_linked_internal(C, op, nullptr, false);
}

void UV_OT_select_linked(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Select Linked";
  ot->description = "Select all UV vertices linked to the active UV map";
  ot->idname = "UV_OT_select_linked";

  /* api callbacks */
  ot->exec = uv_select_linked_exec;
  ot->poll = ED_operator_uvedit; /* requires space image */

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Select Linked (Cursor Pick) Operator
 * \{ */

static int uv_select_linked_pick_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  return uv_select_linked_internal(C, op, event, true);
}

static int uv_select_linked_pick_exec(bContext *C, wmOperator *op)
{
  return uv_select_linked_internal(C, op, nullptr, true);
}

void UV_OT_select_linked_pick(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Select Linked Pick";
  ot->description = "Select all UV vertices linked under the mouse";
  ot->idname = "UV_OT_select_linked_pick";

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* api callbacks */
  ot->invoke = uv_select_linked_pick_invoke;
  ot->exec = uv_select_linked_pick_exec;
  ot->poll = ED_operator_uvedit; /* requires space image */

  /* properties */
  PropertyRNA *prop;
  prop = RNA_def_boolean(ot->srna,
                         "extend",
                         false,
                         "Extend",
                         "Extend selection rather than clearing the existing selection");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
  prop = RNA_def_boolean(ot->srna,
                         "deselect",
                         false,
                         "Deselect",
                         "Deselect linked UV vertices rather than selecting them");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
  prop = RNA_def_float_vector(
      ot->srna,
      "location",
      2,
      nullptr,
      -FLT_MAX,
      FLT_MAX,
      "Location",
      "Mouse location in normalized coordinates, 0.0 to 1.0 is within the image bounds",
      -100.0f,
      100.0f);
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Select Split Operator
 * \{ */

/**
 * \note This is based on similar use case to #MESH_OT_split(), which has a similar effect
 * but in this case they are not joined to begin with (only having the behavior of being joined)
 * so its best to call this #uv_select_split() instead of just split(), but assigned to the same
 * key as #MESH_OT_split - Campbell.
 */
static int uv_select_split_exec(bContext *C, wmOperator *op)
{
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  const ToolSettings *ts = scene->toolsettings;

  BMFace *efa;
  BMLoop *l;
  BMIter iter, liter;

  if (ts->uv_flag & UV_SYNC_SELECTION) {
    BKE_report(op->reports, RPT_ERROR, "Cannot split selection when sync selection is enabled");
    return OPERATOR_CANCELLED;
  }

  bool changed_multi = false;

  uint objects_len = 0;
  Object **objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data_with_uvs(
      scene, view_layer, ((View3D *)nullptr), &objects_len);

  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    Object *obedit = objects[ob_index];
    BMesh *bm = BKE_editmesh_from_object(obedit)->bm;

    bool changed = false;

    const char *active_uv_name = CustomData_get_active_layer_name(&bm->ldata, CD_PROP_FLOAT2);
    BM_uv_map_ensure_vert_select_attr(bm, active_uv_name);
    BM_uv_map_ensure_edge_select_attr(bm, active_uv_name);
    const BMUVOffsets offsets = BM_uv_map_get_offsets(bm);

    BM_ITER_MESH (efa, &iter, bm, BM_FACES_OF_MESH) {
      bool is_sel = false;
      bool is_unsel = false;

      if (!uvedit_face_visible_test(scene, efa)) {
        continue;
      }

      /* are we all selected? */
      BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {

        if (BM_ELEM_CD_GET_BOOL(l, offsets.select_vert) ||
            BM_ELEM_CD_GET_BOOL(l, offsets.select_edge)) {
          is_sel = true;
        }
        if (!BM_ELEM_CD_GET_BOOL(l, offsets.select_vert) ||
            !BM_ELEM_CD_GET_BOOL(l, offsets.select_edge)) {
          is_unsel = true;
        }

        /* we have mixed selection, bail out */
        if (is_sel && is_unsel) {
          break;
        }
      }

      if (is_sel && is_unsel) {
        BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
          BM_ELEM_CD_SET_BOOL(l, offsets.select_vert, false);
          BM_ELEM_CD_SET_BOOL(l, offsets.select_edge, false);
        }

        changed = true;
      }
    }

    if (changed) {
      changed_multi = true;
      WM_event_add_notifier(C, NC_SPACE | ND_SPACE_IMAGE, nullptr);
      uv_select_tag_update_for_object(depsgraph, ts, obedit);
    }
  }
  MEM_freeN(objects);

  return changed_multi ? OPERATOR_FINISHED : OPERATOR_CANCELLED;
}

void UV_OT_select_split(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Select Split";
  ot->description = "Select only entirely selected faces";
  ot->idname = "UV_OT_select_split";
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* api callbacks */
  ot->exec = uv_select_split_exec;
  ot->poll = ED_operator_uvedit; /* requires space image */
}

static void uv_select_tag_update_for_object(Depsgraph *depsgraph,
                                            const ToolSettings *ts,
                                            Object *obedit)
{
  if (ts->uv_flag & UV_SYNC_SELECTION) {
    DEG_id_tag_update(static_cast<ID *>(obedit->data), ID_RECALC_SELECT);
    WM_main_add_notifier(NC_GEOM | ND_SELECT, obedit->data);
  }
  else {
    Object *obedit_eval = DEG_get_evaluated_object(depsgraph, obedit);
    BKE_mesh_batch_cache_dirty_tag(static_cast<Mesh *>(obedit_eval->data),
                                   BKE_MESH_BATCH_DIRTY_UVEDIT_SELECT);
    /* Only for region redraw. */
    WM_main_add_notifier(NC_GEOM | ND_SELECT, obedit->data);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Select/Tag Flushing Utils
 *
 * Utility functions to flush the uv-selection from tags.
 * \{ */

/**
 * helper function for #uv_select_flush_from_tag_loop and uv_select_flush_from_tag_face
 */
static void uv_select_flush_from_tag_sticky_loc_internal(const Scene *scene,
                                                         BMEditMesh *em,
                                                         UvVertMap *vmap,
                                                         const uint efa_index,
                                                         BMLoop *l,
                                                         const bool select,
                                                         const BMUVOffsets offsets)
{
  UvMapVert *start_vlist = nullptr, *vlist_iter;
  BMFace *efa_vlist;

  uvedit_uv_select_set(scene, em->bm, l, select, false, offsets);

  vlist_iter = BM_uv_vert_map_at_index(vmap, BM_elem_index_get(l->v));

  while (vlist_iter) {
    if (vlist_iter->separate) {
      start_vlist = vlist_iter;
    }

    if (efa_index == vlist_iter->face_index) {
      break;
    }

    vlist_iter = vlist_iter->next;
  }

  vlist_iter = start_vlist;
  while (vlist_iter) {
    if (vlist_iter != start_vlist && vlist_iter->separate) {
      break;
    }

    if (efa_index != vlist_iter->face_index) {
      BMLoop *l_other;
      efa_vlist = BM_face_at_index(em->bm, vlist_iter->face_index);
      /* tf_vlist = BM_ELEM_CD_GET_VOID_P(efa_vlist, cd_poly_tex_offset); */ /* UNUSED */

      l_other = static_cast<BMLoop *>(
          BM_iter_at_index(em->bm, BM_LOOPS_OF_FACE, efa_vlist, vlist_iter->loop_of_face_index));

      uvedit_uv_select_set(scene, em->bm, l_other, select, false, offsets);
    }
    vlist_iter = vlist_iter->next;
  }
}

/**
 * Flush the selection from face tags based on sticky and selection modes.
 *
 * needed because setting the selection of a face is done in a number of places but it also
 * needs to respect the sticky modes for the UV verts, so dealing with the sticky modes
 * is best done in a separate function.
 *
 * \note This function is very similar to #uv_select_flush_from_tag_loop,
 * be sure to update both upon changing.
 */
static void uv_select_flush_from_tag_face(const Scene *scene, Object *obedit, const bool select)
{
  /* Selecting UV Faces with some modes requires us to change
   * the selection in other faces (depending on the sticky mode).
   *
   * This only needs to be done when the Mesh is not used for
   * selection (so for sticky modes, vertex or location based). */

  const ToolSettings *ts = scene->toolsettings;
  BMEditMesh *em = BKE_editmesh_from_object(obedit);
  BMFace *efa;
  BMLoop *l;
  BMIter iter, liter;
  const char *active_uv_name = CustomData_get_active_layer_name(&em->bm->ldata, CD_PROP_FLOAT2);
  BM_uv_map_ensure_vert_select_attr(em->bm, active_uv_name);
  BM_uv_map_ensure_edge_select_attr(em->bm, active_uv_name);
  const BMUVOffsets offsets = BM_uv_map_get_offsets(em->bm);

  if ((ts->uv_flag & UV_SYNC_SELECTION) == 0 &&
      ELEM(ts->uv_sticky, SI_STICKY_VERTEX, SI_STICKY_LOC)) {

    uint efa_index;

    BM_mesh_elem_table_ensure(em->bm, BM_FACE);
    UvVertMap *vmap = BM_uv_vert_map_create(em->bm, false);
    if (vmap == nullptr) {
      return;
    }

    BM_ITER_MESH_INDEX (efa, &iter, em->bm, BM_FACES_OF_MESH, efa_index) {
      if (BM_elem_flag_test(efa, BM_ELEM_TAG)) {
        BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
          if (select) {
            BM_ELEM_CD_SET_BOOL(l, offsets.select_edge, true);
            uv_select_flush_from_tag_sticky_loc_internal(
                scene, em, vmap, efa_index, l, select, offsets);
          }
          else {
            BM_ELEM_CD_SET_BOOL(l, offsets.select_edge, false);
            if (!uvedit_vert_is_face_select_any_other(scene, l, offsets)) {
              uv_select_flush_from_tag_sticky_loc_internal(
                  scene, em, vmap, efa_index, l, select, offsets);
            }
          }
        }
      }
    }
    BM_uv_vert_map_free(vmap);
  }
  else { /* SI_STICKY_DISABLE or ts->uv_flag & UV_SYNC_SELECTION */
    BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
      if (BM_elem_flag_test(efa, BM_ELEM_TAG)) {
        uvedit_face_select_set(scene, em->bm, efa, select, false, offsets);
      }
    }
  }
}

/**
 * Flush the selection from loop tags based on sticky and selection modes.
 *
 * needed because setting the selection of a face is done in a number of places but it also needs
 * to respect the sticky modes for the UV verts, so dealing with the sticky modes is best done
 * in a separate function.
 *
 * \note This function is very similar to #uv_select_flush_from_tag_face,
 * be sure to update both upon changing.
 */
static void uv_select_flush_from_tag_loop(const Scene *scene, Object *obedit, const bool select)
{
  /* Selecting UV Loops with some modes requires us to change
   * the selection in other faces (depending on the sticky mode).
   *
   * This only needs to be done when the Mesh is not used for
   * selection (so for sticky modes, vertex or location based). */

  const ToolSettings *ts = scene->toolsettings;
  BMEditMesh *em = BKE_editmesh_from_object(obedit);
  BMFace *efa;
  BMLoop *l;
  BMIter iter, liter;

  const char *active_uv_name = CustomData_get_active_layer_name(&em->bm->ldata, CD_PROP_FLOAT2);
  BM_uv_map_ensure_vert_select_attr(em->bm, active_uv_name);
  BM_uv_map_ensure_edge_select_attr(em->bm, active_uv_name);
  const BMUVOffsets offsets = BM_uv_map_get_offsets(em->bm);

  if ((ts->uv_flag & UV_SYNC_SELECTION) == 0 && ts->uv_sticky == SI_STICKY_VERTEX) {
    /* Tag all verts as untouched, then touch the ones that have a face center
     * in the loop and select all UVs that use a touched vert. */
    BM_mesh_elem_hflag_disable_all(em->bm, BM_VERT, BM_ELEM_TAG, false);

    BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
      BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
        if (BM_elem_flag_test(l, BM_ELEM_TAG)) {
          BM_elem_flag_enable(l->v, BM_ELEM_TAG);
        }
      }
    }

    /* now select tagged verts */
    BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
      BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
        if (BM_elem_flag_test(l->v, BM_ELEM_TAG)) {
          uvedit_uv_select_set(scene, em->bm, l, select, false, offsets);
        }
      }
    }
  }
  else if ((ts->uv_flag & UV_SYNC_SELECTION) == 0 && ts->uv_sticky == SI_STICKY_LOC) {
    uint efa_index;

    BM_mesh_elem_table_ensure(em->bm, BM_FACE);
    UvVertMap *vmap = BM_uv_vert_map_create(em->bm, false);
    if (vmap == nullptr) {
      return;
    }

    BM_ITER_MESH_INDEX (efa, &iter, em->bm, BM_FACES_OF_MESH, efa_index) {
      BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
        if (BM_elem_flag_test(l, BM_ELEM_TAG)) {
          uv_select_flush_from_tag_sticky_loc_internal(
              scene, em, vmap, efa_index, l, select, offsets);
        }
      }
    }
    BM_uv_vert_map_free(vmap);
  }
  else { /* SI_STICKY_DISABLE or ts->uv_flag & UV_SYNC_SELECTION */
    BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
      BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
        if (BM_elem_flag_test(l, BM_ELEM_TAG)) {
          uvedit_uv_select_set(scene, em->bm, l, select, false, offsets);
        }
      }
    }
  }
}

/**
 * Flush the selection from UV edges based on sticky modes.
 *
 * Useful when performing edge selections in different sticky modes, since setting the required
 * edge selection is done manually or using #uvedit_edge_select_set_noflush,
 * but dealing with sticky modes for vertex selections is best done in a separate function.
 *
 * \note Current behavior is selecting only; deselecting can be added but the behavior isn't
 * required anywhere.
 */
static void uv_select_flush_from_loop_edge_flag(const Scene *scene, BMEditMesh *em)
{
  const ToolSettings *ts = scene->toolsettings;
  BMFace *efa;
  BMLoop *l;
  BMIter iter, liter;

  const char *active_uv_name = CustomData_get_active_layer_name(&em->bm->ldata, CD_PROP_FLOAT2);
  BM_uv_map_ensure_vert_select_attr(em->bm, active_uv_name);
  BM_uv_map_ensure_edge_select_attr(em->bm, active_uv_name);
  const BMUVOffsets offsets = BM_uv_map_get_offsets(em->bm);

  if ((ts->uv_flag & UV_SYNC_SELECTION) == 0 &&
      ELEM(ts->uv_sticky, SI_STICKY_LOC, SI_STICKY_VERTEX)) {
    /* Use UV edge selection to identify which verts must to be selected */
    uint efa_index;
    /* Clear UV vert flags */
    bm_clear_uv_vert_selection(scene, em->bm, offsets);

    BM_mesh_elem_table_ensure(em->bm, BM_FACE);
    UvVertMap *vmap = BM_uv_vert_map_create(em->bm, false);
    if (vmap == nullptr) {
      return;
    }
    BM_ITER_MESH_INDEX (efa, &iter, em->bm, BM_FACES_OF_MESH, efa_index) {
      if (!uvedit_face_visible_test(scene, efa)) {
        /* This visibility check could be removed? Simply relying on edge flags to ensure
         * visibility might be sufficient. */
        continue;
      }
      BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
        /* Select verts based on UV edge flag. */
        if (BM_ELEM_CD_GET_BOOL(l, offsets.select_edge)) {
          uv_select_flush_from_tag_sticky_loc_internal(
              scene, em, vmap, efa_index, l, true, offsets);
          uv_select_flush_from_tag_sticky_loc_internal(
              scene, em, vmap, efa_index, l->next, true, offsets);
        }
      }
    }
    BM_uv_vert_map_free(vmap);
  }
  else {
    BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
      BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {

        if (BM_ELEM_CD_GET_BOOL(l, offsets.select_edge)) {
          BM_ELEM_CD_SET_BOOL(l, offsets.select_vert, true);
          BM_ELEM_CD_SET_BOOL(l->next, offsets.select_vert, true);
        }
        else if (!BM_ELEM_CD_GET_BOOL(l->prev, offsets.select_edge)) {
          BM_ELEM_CD_SET_BOOL(l->next, offsets.select_vert, false);
        }
      }
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Box Select Operator
 * \{ */

static int uv_box_select_exec(bContext *C, wmOperator *op)
{
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  Scene *scene = CTX_data_scene(C);
  const ToolSettings *ts = scene->toolsettings;
  ViewLayer *view_layer = CTX_data_view_layer(C);
  const ARegion *region = CTX_wm_region(C);
  BMFace *efa;
  BMLoop *l;
  BMIter iter, liter;
  float *luv;
  rctf rectf;
  bool pinned;
  const bool use_face_center = ((ts->uv_flag & UV_SYNC_SELECTION) ?
                                    (ts->selectmode == SCE_SELECT_FACE) :
                                    (ts->uv_selectmode == UV_SELECT_FACE));
  const bool use_edge = ((ts->uv_flag & UV_SYNC_SELECTION) ?
                             (ts->selectmode == SCE_SELECT_EDGE) :
                             (ts->uv_selectmode == UV_SELECT_EDGE));
  const bool use_select_linked = !(ts->uv_flag & UV_SYNC_SELECTION) &&
                                 (ts->uv_selectmode == UV_SELECT_ISLAND);

  /* get rectangle from operator */
  WM_operator_properties_border_to_rctf(op, &rectf);
  UI_view2d_region_to_view_rctf(&region->v2d, &rectf, &rectf);

  const eSelectOp sel_op = eSelectOp(RNA_enum_get(op->ptr, "mode"));
  const bool select = (sel_op != SEL_OP_SUB);
  const bool use_pre_deselect = SEL_OP_USE_PRE_DESELECT(sel_op);

  pinned = RNA_boolean_get(op->ptr, "pinned");

  bool changed_multi = false;

  uint objects_len = 0;
  Object **objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data_with_uvs(
      scene, view_layer, ((View3D *)nullptr), &objects_len);

  if (use_pre_deselect) {
    uv_select_all_perform_multi(scene, objects, objects_len, SEL_DESELECT);
  }

  /* don't indent to avoid diff noise! */
  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    Object *obedit = objects[ob_index];
    BMEditMesh *em = BKE_editmesh_from_object(obedit);

    bool changed = false;

    const char *active_uv_name = CustomData_get_active_layer_name(&em->bm->ldata, CD_PROP_FLOAT2);
    BM_uv_map_ensure_vert_select_attr(em->bm, active_uv_name);
    BM_uv_map_ensure_edge_select_attr(em->bm, active_uv_name);
    BM_uv_map_ensure_pin_attr(em->bm, active_uv_name);
    const BMUVOffsets offsets = BM_uv_map_get_offsets(em->bm);

    /* do actual selection */
    if (use_face_center && !pinned) {
      /* handle face selection mode */
      float cent[2];

      BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
        /* assume not touched */
        BM_elem_flag_disable(efa, BM_ELEM_TAG);

        if (uvedit_face_visible_test(scene, efa)) {
          BM_face_uv_calc_center_median(efa, offsets.uv, cent);
          if (BLI_rctf_isect_pt_v(&rectf, cent)) {
            BM_elem_flag_enable(efa, BM_ELEM_TAG);
            changed = true;
          }
        }
      }

      /* (de)selects all tagged faces and deals with sticky modes */
      if (changed) {
        uv_select_flush_from_tag_face(scene, obedit, select);
      }
    }
    else if (use_edge && !pinned) {
      bool do_second_pass = true;
      BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
        if (!uvedit_face_visible_test(scene, efa)) {
          continue;
        }

        BMLoop *l_prev = BM_FACE_FIRST_LOOP(efa)->prev;
        float *luv_prev = BM_ELEM_CD_GET_FLOAT_P(l_prev, offsets.uv);

        BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
          luv = BM_ELEM_CD_GET_FLOAT_P(l, offsets.uv);
          if (BLI_rctf_isect_pt_v(&rectf, luv) && BLI_rctf_isect_pt_v(&rectf, luv_prev)) {
            uvedit_edge_select_set_with_sticky(scene, em, l_prev, select, false, offsets);
            changed = true;
            do_second_pass = false;
          }
          l_prev = l;
          luv_prev = luv;
        }
      }
      /* Do a second pass if no complete edges could be selected.
       * This matches wire-frame edit-mesh selection in the 3D view. */
      if (do_second_pass) {
        /* Second pass to check if edges partially overlap with the selection area (box). */
        BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
          if (!uvedit_face_visible_test(scene, efa)) {
            continue;
          }
          BMLoop *l_prev = BM_FACE_FIRST_LOOP(efa)->prev;
          float *luv_prev = BM_ELEM_CD_GET_FLOAT_P(l_prev, offsets.uv);

          BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
            luv = BM_ELEM_CD_GET_FLOAT_P(l, offsets.uv);
            if (BLI_rctf_isect_segment(&rectf, luv_prev, luv)) {
              uvedit_edge_select_set_with_sticky(scene, em, l_prev, select, false, offsets);
              changed = true;
            }
            l_prev = l;
            luv_prev = luv;
          }
        }
      }
    }
    else {
      /* other selection modes */
      changed = true;
      BM_mesh_elem_hflag_disable_all(em->bm, BM_VERT, BM_ELEM_TAG, false);

      BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
        if (!uvedit_face_visible_test(scene, efa)) {
          continue;
        }
        bool has_selected = false;
        BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
          luv = BM_ELEM_CD_GET_FLOAT_P(l, offsets.uv);
          if (select != uvedit_uv_select_test(scene, l, offsets)) {
            if (!pinned || (ts->uv_flag & UV_SYNC_SELECTION)) {
              /* UV_SYNC_SELECTION - can't do pinned selection */
              if (BLI_rctf_isect_pt_v(&rectf, luv)) {
                uvedit_uv_select_set(scene, em->bm, l, select, false, offsets);
                BM_elem_flag_enable(l->v, BM_ELEM_TAG);
                has_selected = true;
              }
            }
            else if (pinned) {
              if (BM_ELEM_CD_GET_BOOL(l, offsets.pin) && BLI_rctf_isect_pt_v(&rectf, luv)) {
                uvedit_uv_select_set(scene, em->bm, l, select, false, offsets);
                BM_elem_flag_enable(l->v, BM_ELEM_TAG);
              }
            }
          }
        }
        if (has_selected && use_select_linked) {
          UvNearestHit hit = {};
          hit.ob = obedit;
          hit.efa = efa;
          uv_select_linked_multi(scene, objects, objects_len, &hit, true, !select, false, false);
        }
      }

      if (ts->uv_sticky == SI_STICKY_VERTEX) {
        uvedit_vertex_select_tagged(em, scene, select, offsets);
      }
    }

    if (changed || use_pre_deselect) {
      changed_multi = true;
      if (ts->uv_flag & UV_SYNC_SELECTION) {
        ED_uvedit_select_sync_flush(ts, em, select);
      }
      else {
        ED_uvedit_selectmode_flush(scene, em);
      }
      uv_select_tag_update_for_object(depsgraph, ts, obedit);
    }
  }

  MEM_freeN(objects);

  return changed_multi ? OPERATOR_FINISHED : OPERATOR_CANCELLED;
}

void UV_OT_select_box(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Box Select";
  ot->description = "Select UV vertices using box selection";
  ot->idname = "UV_OT_select_box";

  /* api callbacks */
  ot->invoke = WM_gesture_box_invoke;
  ot->exec = uv_box_select_exec;
  ot->modal = WM_gesture_box_modal;
  ot->poll = ED_operator_uvedit_space_image; /* requires space image */
  ot->cancel = WM_gesture_box_cancel;

  /* flags */
  ot->flag = OPTYPE_UNDO;

  /* properties */
  RNA_def_boolean(ot->srna, "pinned", false, "Pinned", "Border select pinned UVs only");

  WM_operator_properties_gesture_box(ot);
  WM_operator_properties_select_operation_simple(ot);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Circle Select Operator
 * \{ */

static bool uv_circle_select_is_point_inside(const float uv[2],
                                             const float offset[2],
                                             const float ellipse[2])
{
  /* normalized ellipse: ell[0] = scaleX, ell[1] = scaleY */
  const float co[2] = {
      (uv[0] - offset[0]) * ellipse[0],
      (uv[1] - offset[1]) * ellipse[1],
  };
  return len_squared_v2(co) < 1.0f;
}

static bool uv_circle_select_is_edge_inside(const float uv_a[2],
                                            const float uv_b[2],
                                            const float offset[2],
                                            const float ellipse[2])
{
  /* normalized ellipse: ell[0] = scaleX, ell[1] = scaleY */
  const float co_a[2] = {
      (uv_a[0] - offset[0]) * ellipse[0],
      (uv_a[1] - offset[1]) * ellipse[1],
  };
  const float co_b[2] = {
      (uv_b[0] - offset[0]) * ellipse[0],
      (uv_b[1] - offset[1]) * ellipse[1],
  };
  const float co_zero[2] = {0.0f, 0.0f};
  return dist_squared_to_line_segment_v2(co_zero, co_a, co_b) < 1.0f;
}

static int uv_circle_select_exec(bContext *C, wmOperator *op)
{
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  SpaceImage *sima = CTX_wm_space_image(C);
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  const ToolSettings *ts = scene->toolsettings;
  const ARegion *region = CTX_wm_region(C);
  BMFace *efa;
  BMLoop *l;
  BMIter iter, liter;
  float *luv;
  int x, y, radius, width, height;
  float zoomx, zoomy;
  float offset[2], ellipse[2];

  const bool use_face_center = ((ts->uv_flag & UV_SYNC_SELECTION) ?
                                    (ts->selectmode == SCE_SELECT_FACE) :
                                    (ts->uv_selectmode == UV_SELECT_FACE));
  const bool use_edge = ((ts->uv_flag & UV_SYNC_SELECTION) ?
                             (ts->selectmode == SCE_SELECT_EDGE) :
                             (ts->uv_selectmode == UV_SELECT_EDGE));
  const bool use_select_linked = !(ts->uv_flag & UV_SYNC_SELECTION) &&
                                 (ts->uv_selectmode == UV_SELECT_ISLAND);

  /* get operator properties */
  x = RNA_int_get(op->ptr, "x");
  y = RNA_int_get(op->ptr, "y");
  radius = RNA_int_get(op->ptr, "radius");

  /* compute ellipse size and location, not a circle since we deal
   * with non square image. ellipse is normalized, r = 1.0. */
  ED_space_image_get_size(sima, &width, &height);
  ED_space_image_get_zoom(sima, region, &zoomx, &zoomy);

  ellipse[0] = width * zoomx / radius;
  ellipse[1] = height * zoomy / radius;

  UI_view2d_region_to_view(&region->v2d, x, y, &offset[0], &offset[1]);

  bool changed_multi = false;

  uint objects_len = 0;
  Object **objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data_with_uvs(
      scene, view_layer, ((View3D *)nullptr), &objects_len);

  const eSelectOp sel_op = ED_select_op_modal(
      eSelectOp(RNA_enum_get(op->ptr, "mode")),
      WM_gesture_is_modal_first(static_cast<wmGesture *>(op->customdata)));
  const bool select = (sel_op != SEL_OP_SUB);
  const bool use_pre_deselect = SEL_OP_USE_PRE_DESELECT(sel_op);

  if (use_pre_deselect) {
    uv_select_all_perform_multi(scene, objects, objects_len, SEL_DESELECT);
  }

  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    Object *obedit = objects[ob_index];
    BMEditMesh *em = BKE_editmesh_from_object(obedit);

    bool changed = false;

    const char *active_uv_name = CustomData_get_active_layer_name(&em->bm->ldata, CD_PROP_FLOAT2);
    BM_uv_map_ensure_vert_select_attr(em->bm, active_uv_name);
    BM_uv_map_ensure_edge_select_attr(em->bm, active_uv_name);
    const BMUVOffsets offsets = BM_uv_map_get_offsets(em->bm);

    /* do selection */
    if (use_face_center) {
      BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
        BM_elem_flag_disable(efa, BM_ELEM_TAG);
        /* assume not touched */
        if (select != uvedit_face_select_test(scene, efa, offsets)) {
          float cent[2];
          BM_face_uv_calc_center_median(efa, offsets.uv, cent);
          if (uv_circle_select_is_point_inside(cent, offset, ellipse)) {
            BM_elem_flag_enable(efa, BM_ELEM_TAG);
            changed = true;
          }
        }
      }

      /* (de)selects all tagged faces and deals with sticky modes */
      if (changed) {
        uv_select_flush_from_tag_face(scene, obedit, select);
      }
    }
    else if (use_edge) {
      BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
        if (!uvedit_face_visible_test(scene, efa)) {
          continue;
        }

        BMLoop *l_prev = BM_FACE_FIRST_LOOP(efa)->prev;
        float *luv_prev = BM_ELEM_CD_GET_FLOAT_P(l_prev, offsets.uv);

        BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
          luv = BM_ELEM_CD_GET_FLOAT_P(l, offsets.uv);
          if (uv_circle_select_is_edge_inside(luv, luv_prev, offset, ellipse)) {
            uvedit_edge_select_set_with_sticky(scene, em, l_prev, select, false, offsets);
            changed = true;
          }
          l_prev = l;
          luv_prev = luv;
        }
      }
    }
    else {
      BM_mesh_elem_hflag_disable_all(em->bm, BM_VERT, BM_ELEM_TAG, false);

      BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
        if (!uvedit_face_visible_test(scene, efa)) {
          continue;
        }
        bool has_selected = false;
        BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
          if (select != uvedit_uv_select_test(scene, l, offsets)) {
            luv = BM_ELEM_CD_GET_FLOAT_P(l, offsets.uv);
            if (uv_circle_select_is_point_inside(luv, offset, ellipse)) {
              changed = true;
              uvedit_uv_select_set(scene, em->bm, l, select, false, offsets);
              BM_elem_flag_enable(l->v, BM_ELEM_TAG);
              has_selected = true;
            }
          }
        }
        if (has_selected && use_select_linked) {
          UvNearestHit hit = {};
          hit.ob = obedit;
          hit.efa = efa;
          uv_select_linked_multi(scene, objects, objects_len, &hit, true, !select, false, false);
        }
      }

      if (ts->uv_sticky == SI_STICKY_VERTEX) {
        uvedit_vertex_select_tagged(em, scene, select, offsets);
      }
    }

    if (changed || use_pre_deselect) {
      changed_multi = true;
      if (ts->uv_flag & UV_SYNC_SELECTION) {
        ED_uvedit_select_sync_flush(ts, em, select);
      }
      else {
        ED_uvedit_selectmode_flush(scene, em);
      }
      uv_select_tag_update_for_object(depsgraph, ts, obedit);
    }
  }
  MEM_freeN(objects);

  return changed_multi ? OPERATOR_FINISHED : OPERATOR_CANCELLED;
}

void UV_OT_select_circle(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Circle Select";
  ot->description = "Select UV vertices using circle selection";
  ot->idname = "UV_OT_select_circle";

  /* api callbacks */
  ot->invoke = WM_gesture_circle_invoke;
  ot->modal = WM_gesture_circle_modal;
  ot->exec = uv_circle_select_exec;
  ot->poll = ED_operator_uvedit_space_image; /* requires space image */
  ot->cancel = WM_gesture_circle_cancel;
  ot->get_name = ED_select_circle_get_name;

  /* flags */
  ot->flag = OPTYPE_UNDO;

  /* properties */
  WM_operator_properties_gesture_circle(ot);
  WM_operator_properties_select_operation_simple(ot);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Lasso Select Operator
 * \{ */

static bool do_lasso_select_mesh_uv_is_point_inside(const ARegion *region,
                                                    const rcti *clip_rect,
                                                    const int mcoords[][2],
                                                    const int mcoords_len,
                                                    const float co_test[2])
{
  int co_screen[2];
  if (UI_view2d_view_to_region_clip(
          &region->v2d, co_test[0], co_test[1], &co_screen[0], &co_screen[1]) &&
      BLI_rcti_isect_pt_v(clip_rect, co_screen) &&
      BLI_lasso_is_point_inside(mcoords, mcoords_len, co_screen[0], co_screen[1], V2D_IS_CLIPPED))
  {
    return true;
  }
  return false;
}

static bool do_lasso_select_mesh_uv_is_edge_inside(const ARegion *region,
                                                   const rcti *clip_rect,
                                                   const int mcoords[][2],
                                                   const int mcoords_len,
                                                   const float co_test_a[2],
                                                   const float co_test_b[2])
{
  int co_screen_a[2], co_screen_b[2];
  if (UI_view2d_view_to_region_segment_clip(
          &region->v2d, co_test_a, co_test_b, co_screen_a, co_screen_b) &&
      BLI_rcti_isect_segment(clip_rect, co_screen_a, co_screen_b) &&
      BLI_lasso_is_edge_inside(
          mcoords, mcoords_len, UNPACK2(co_screen_a), UNPACK2(co_screen_b), V2D_IS_CLIPPED))
  {
    return true;
  }
  return false;
}

static bool do_lasso_select_mesh_uv(bContext *C,
                                    const int mcoords[][2],
                                    const int mcoords_len,
                                    const eSelectOp sel_op)
{
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  const ARegion *region = CTX_wm_region(C);
  Scene *scene = CTX_data_scene(C);
  const ToolSettings *ts = scene->toolsettings;
  ViewLayer *view_layer = CTX_data_view_layer(C);
  const bool use_face_center = ((ts->uv_flag & UV_SYNC_SELECTION) ?
                                    (ts->selectmode == SCE_SELECT_FACE) :
                                    (ts->uv_selectmode == UV_SELECT_FACE));
  const bool use_edge = ((ts->uv_flag & UV_SYNC_SELECTION) ?
                             (ts->selectmode == SCE_SELECT_EDGE) :
                             (ts->uv_selectmode == UV_SELECT_EDGE));
  const bool use_select_linked = !(ts->uv_flag & UV_SYNC_SELECTION) &&
                                 (ts->uv_selectmode == UV_SELECT_ISLAND);

  const bool select = (sel_op != SEL_OP_SUB);
  const bool use_pre_deselect = SEL_OP_USE_PRE_DESELECT(sel_op);

  BMIter iter, liter;

  BMFace *efa;
  BMLoop *l;
  bool changed_multi = false;
  rcti rect;

  BLI_lasso_boundbox(&rect, mcoords, mcoords_len);

  uint objects_len = 0;
  Object **objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data_with_uvs(
      scene, view_layer, ((View3D *)nullptr), &objects_len);

  if (use_pre_deselect) {
    uv_select_all_perform_multi(scene, objects, objects_len, SEL_DESELECT);
  }

  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    Object *obedit = objects[ob_index];

    bool changed = false;

    BMEditMesh *em = BKE_editmesh_from_object(obedit);

    const char *active_uv_name = CustomData_get_active_layer_name(&em->bm->ldata, CD_PROP_FLOAT2);
    BM_uv_map_ensure_vert_select_attr(em->bm, active_uv_name);
    BM_uv_map_ensure_edge_select_attr(em->bm, active_uv_name);
    const BMUVOffsets offsets = BM_uv_map_get_offsets(em->bm);

    if (use_face_center) { /* Face Center Select. */
      BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
        BM_elem_flag_disable(efa, BM_ELEM_TAG);
        /* assume not touched */
        if (select != uvedit_face_select_test(scene, efa, offsets)) {
          float cent[2];
          BM_face_uv_calc_center_median(efa, offsets.uv, cent);
          if (do_lasso_select_mesh_uv_is_point_inside(region, &rect, mcoords, mcoords_len, cent)) {
            BM_elem_flag_enable(efa, BM_ELEM_TAG);
            changed = true;
          }
        }
      }

      /* (de)selects all tagged faces and deals with sticky modes */
      if (changed) {
        uv_select_flush_from_tag_face(scene, obedit, select);
      }
    }
    else if (use_edge) {
      bool do_second_pass = true;
      BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
        if (!uvedit_face_visible_test(scene, efa)) {
          continue;
        }

        BMLoop *l_prev = BM_FACE_FIRST_LOOP(efa)->prev;
        float *luv_prev = BM_ELEM_CD_GET_FLOAT_P(l_prev, offsets.uv);

        BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
          float *luv = BM_ELEM_CD_GET_FLOAT_P(l, offsets.uv);
          if (do_lasso_select_mesh_uv_is_point_inside(region, &rect, mcoords, mcoords_len, luv) &&
              do_lasso_select_mesh_uv_is_point_inside(
                  region, &rect, mcoords, mcoords_len, luv_prev))
          {
            uvedit_edge_select_set_with_sticky(scene, em, l_prev, select, false, offsets);
            do_second_pass = false;
            changed = true;
          }
          l_prev = l;
          luv_prev = luv;
        }
      }
      /* Do a second pass if no complete edges could be selected.
       * This matches wire-frame edit-mesh selection in the 3D view. */
      if (do_second_pass) {
        /* Second pass to check if edges partially overlap with the selection area (lasso). */
        BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
          if (!uvedit_face_visible_test(scene, efa)) {
            continue;
          }
          BMLoop *l_prev = BM_FACE_FIRST_LOOP(efa)->prev;
          float *luv_prev = BM_ELEM_CD_GET_FLOAT_P(l_prev, offsets.uv);

          BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
            float *luv = BM_ELEM_CD_GET_FLOAT_P(l, offsets.uv);
            if (do_lasso_select_mesh_uv_is_edge_inside(
                    region, &rect, mcoords, mcoords_len, luv, luv_prev)) {
              uvedit_edge_select_set_with_sticky(scene, em, l_prev, select, false, offsets);
              changed = true;
            }
            l_prev = l;
            luv_prev = luv;
          }
        }
      }
    }
    else { /* Vert Selection. */
      BM_mesh_elem_hflag_disable_all(em->bm, BM_VERT, BM_ELEM_TAG, false);

      BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
        if (!uvedit_face_visible_test(scene, efa)) {
          continue;
        }
        bool has_selected = false;
        BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
          if (select != uvedit_uv_select_test(scene, l, offsets)) {
            float *luv = BM_ELEM_CD_GET_FLOAT_P(l, offsets.uv);
            if (do_lasso_select_mesh_uv_is_point_inside(region, &rect, mcoords, mcoords_len, luv))
            {
              uvedit_uv_select_set(scene, em->bm, l, select, false, offsets);
              changed = true;
              BM_elem_flag_enable(l->v, BM_ELEM_TAG);
              has_selected = true;
            }
          }
        }
        if (has_selected && use_select_linked) {
          UvNearestHit hit = {};
          hit.ob = obedit;
          hit.efa = efa;
          uv_select_linked_multi(scene, objects, objects_len, &hit, true, !select, false, false);
        }
      }

      if (ts->uv_sticky == SI_STICKY_VERTEX) {
        uvedit_vertex_select_tagged(em, scene, select, offsets);
      }
    }

    if (changed || use_pre_deselect) {
      changed_multi = true;
      if (ts->uv_flag & UV_SYNC_SELECTION) {
        ED_uvedit_select_sync_flush(ts, em, select);
      }
      else {
        ED_uvedit_selectmode_flush(scene, em);
      }
      uv_select_tag_update_for_object(depsgraph, ts, obedit);
    }
  }
  MEM_freeN(objects);

  return changed_multi;
}

static int uv_lasso_select_exec(bContext *C, wmOperator *op)
{
  int mcoords_len;
  const int(*mcoords)[2] = WM_gesture_lasso_path_to_array(C, op, &mcoords_len);

  if (mcoords) {
    const eSelectOp sel_op = eSelectOp(RNA_enum_get(op->ptr, "mode"));
    bool changed = do_lasso_select_mesh_uv(C, mcoords, mcoords_len, sel_op);
    MEM_freeN((void *)mcoords);

    return changed ? OPERATOR_FINISHED : OPERATOR_CANCELLED;
  }

  return OPERATOR_PASS_THROUGH;
}

void UV_OT_select_lasso(wmOperatorType *ot)
{
  ot->name = "Lasso Select UV";
  ot->description = "Select UVs using lasso selection";
  ot->idname = "UV_OT_select_lasso";

  ot->invoke = WM_gesture_lasso_invoke;
  ot->modal = WM_gesture_lasso_modal;
  ot->exec = uv_lasso_select_exec;
  ot->poll = ED_operator_uvedit_space_image;
  ot->cancel = WM_gesture_lasso_cancel;

  /* flags */
  ot->flag = OPTYPE_UNDO | OPTYPE_DEPENDS_ON_CURSOR;

  /* properties */
  WM_operator_properties_gesture_lasso(ot);
  WM_operator_properties_select_operation_simple(ot);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Select Pinned UVs Operator
 * \{ */

static int uv_select_pinned_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);
  const ToolSettings *ts = scene->toolsettings;

  /* Use this operator only in vertex mode, since it is not guaranteed that pinned vertices may
   * form higher selection states (like edges/faces/islands) in other modes. */
  if ((ts->uv_flag & UV_SYNC_SELECTION) == 0) {
    if (ts->uv_selectmode != UV_SELECT_VERTEX) {
      BKE_report(op->reports, RPT_ERROR, "Pinned vertices can be selected in Vertex Mode only");
      return OPERATOR_CANCELLED;
    }
  }

  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  BMFace *efa;
  BMLoop *l;
  BMIter iter, liter;

  uint objects_len = 0;
  Object **objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data_with_uvs(
      scene, view_layer, ((View3D *)nullptr), &objects_len);

  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    Object *obedit = objects[ob_index];
    BMEditMesh *em = BKE_editmesh_from_object(obedit);

    bool changed = false;
    const char *active_uv_name = CustomData_get_active_layer_name(&em->bm->ldata, CD_PROP_FLOAT2);
    BM_uv_map_ensure_vert_select_attr(em->bm, active_uv_name);
    BM_uv_map_ensure_edge_select_attr(em->bm, active_uv_name);
    BM_uv_map_ensure_pin_attr(em->bm, active_uv_name);
    const BMUVOffsets offsets = BM_uv_map_get_offsets(em->bm);

    BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
      if (!uvedit_face_visible_test(scene, efa)) {
        continue;
      }

      BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {

        if (BM_ELEM_CD_GET_BOOL(l, offsets.pin)) {
          uvedit_uv_select_enable(scene, em->bm, l, false, offsets);
          changed = true;
        }
      }
    }
    if ((ts->uv_flag & UV_SYNC_SELECTION) == 0) {
      ED_uvedit_selectmode_flush(scene, em);
    }

    if (changed) {
      uv_select_tag_update_for_object(depsgraph, ts, obedit);
    }
  }
  MEM_freeN(objects);

  return OPERATOR_FINISHED;
}

void UV_OT_select_pinned(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Selected Pinned";
  ot->description = "Select all pinned UV vertices";
  ot->idname = "UV_OT_select_pinned";
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* api callbacks */
  ot->exec = uv_select_pinned_exec;
  ot->poll = ED_operator_uvedit;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Select Overlap Operator
 * \{ */

BLI_INLINE uint overlap_hash(const void *overlap_v)
{
  const BVHTreeOverlap *overlap = static_cast<const BVHTreeOverlap *>(overlap_v);

  /* Designed to treat (A,B) and (B,A) as the same. */
  int x = overlap->indexA;
  int y = overlap->indexB;
  if (x > y) {
    SWAP(int, x, y);
  }
  return BLI_hash_int_2d(x, y);
}

BLI_INLINE bool overlap_cmp(const void *a_v, const void *b_v)
{
  const BVHTreeOverlap *a = static_cast<const BVHTreeOverlap *>(a_v);
  const BVHTreeOverlap *b = static_cast<const BVHTreeOverlap *>(b_v);
  return !((a->indexA == b->indexA && a->indexB == b->indexB) ||
           (a->indexA == b->indexB && a->indexB == b->indexA));
}

struct UVOverlapData {
  int ob_index;
  int face_index;
  float tri[3][2];
};

/**
 * Specialized 2D triangle intersection for detecting UV overlap:
 *
 * \return
 * - false when single corners or edges touch (common for UV coordinates).
 * - true when all corners touch (an exactly overlapping triangle).
 */
static bool overlap_tri_tri_uv_test(const float t1[3][2],
                                    const float t2[3][2],
                                    const float endpoint_bias)
{
  float vi[2];

  /* Don't use 'isect_tri_tri_v2' here
   * because it's important to ignore overlap at end-points. */
  if (isect_seg_seg_v2_point_ex(t1[0], t1[1], t2[0], t2[1], endpoint_bias, vi) == 1 ||
      isect_seg_seg_v2_point_ex(t1[0], t1[1], t2[1], t2[2], endpoint_bias, vi) == 1 ||
      isect_seg_seg_v2_point_ex(t1[0], t1[1], t2[2], t2[0], endpoint_bias, vi) == 1 ||
      isect_seg_seg_v2_point_ex(t1[1], t1[2], t2[0], t2[1], endpoint_bias, vi) == 1 ||
      isect_seg_seg_v2_point_ex(t1[1], t1[2], t2[1], t2[2], endpoint_bias, vi) == 1 ||
      isect_seg_seg_v2_point_ex(t1[1], t1[2], t2[2], t2[0], endpoint_bias, vi) == 1 ||
      isect_seg_seg_v2_point_ex(t1[2], t1[0], t2[0], t2[1], endpoint_bias, vi) == 1 ||
      isect_seg_seg_v2_point_ex(t1[2], t1[0], t2[1], t2[2], endpoint_bias, vi) == 1)
  {
    return true;
  }

  /* When none of the segments intersect, checking if either of the triangles corners
   * is inside the others is almost always sufficient to test if the two triangles intersect.
   *
   * However, the `endpoint_bias` on segment intersections causes _exact_ overlapping
   * triangles not to be detected.
   *
   * Resolve this problem at the small cost of calculating the triangle center, see #85508. */
  mid_v2_v2v2v2(vi, UNPACK3(t1));
  if (isect_point_tri_v2(vi, UNPACK3(t2)) != 0) {
    return true;
  }
  mid_v2_v2v2v2(vi, UNPACK3(t2));
  if (isect_point_tri_v2(vi, UNPACK3(t1)) != 0) {
    return true;
  }

  return false;
}

static int uv_select_overlap(bContext *C, const bool extend)
{
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);

  uint objects_len = 0;
  Object **objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data_with_uvs(
      scene, view_layer, ((View3D *)nullptr), &objects_len);

  /* Calculate maximum number of tree nodes and prepare initial selection. */
  uint uv_tri_len = 0;
  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    Object *obedit = objects[ob_index];
    BMEditMesh *em = BKE_editmesh_from_object(obedit);

    BM_mesh_elem_table_ensure(em->bm, BM_FACE);
    BM_mesh_elem_index_ensure(em->bm, BM_VERT | BM_FACE);
    BM_mesh_elem_hflag_disable_all(em->bm, BM_FACE, BM_ELEM_TAG, false);
    if (!extend) {
      uv_select_all_perform(scene, obedit, SEL_DESELECT);
    }

    BMIter iter;
    BMFace *efa;
    BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
      if (!uvedit_face_visible_test_ex(scene->toolsettings, efa)) {
        continue;
      }
      uv_tri_len += efa->len - 2;
    }
  }

  UVOverlapData *overlap_data = static_cast<UVOverlapData *>(
      MEM_mallocN(sizeof(UVOverlapData) * uv_tri_len, "UvOverlapData"));
  BVHTree *uv_tree = BLI_bvhtree_new(uv_tri_len, 0.0f, 4, 6);

  /* Use a global data index when inserting into the BVH. */
  int data_index = 0;

  int face_len_alloc = 3;
  float(*uv_verts)[2] = static_cast<float(*)[2]>(
      MEM_mallocN(sizeof(*uv_verts) * face_len_alloc, "UvOverlapCoords"));
  uint(*indices)[3] = static_cast<uint(*)[3]>(
      MEM_mallocN(sizeof(*indices) * (face_len_alloc - 2), "UvOverlapTris"));

  MemArena *arena = BLI_memarena_new(BLI_MEMARENA_STD_BUFSIZE, __func__);
  Heap *heap = BLI_heap_new_ex(BLI_POLYFILL_ALLOC_NGON_RESERVE);

  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    Object *obedit = objects[ob_index];
    BMEditMesh *em = BKE_editmesh_from_object(obedit);
    BMIter iter, liter;
    BMFace *efa;
    BMLoop *l;

    const int cd_loop_uv_offset = CustomData_get_offset(&em->bm->ldata, CD_PROP_FLOAT2);

    /* Triangulate each UV face and store it inside the BVH. */
    int face_index;
    BM_ITER_MESH_INDEX (efa, &iter, em->bm, BM_FACES_OF_MESH, face_index) {

      if (!uvedit_face_visible_test_ex(scene->toolsettings, efa)) {
        continue;
      }

      const uint face_len = efa->len;
      const uint tri_len = face_len - 2;

      if (face_len_alloc < face_len) {
        MEM_freeN(uv_verts);
        MEM_freeN(indices);
        uv_verts = static_cast<float(*)[2]>(
            MEM_mallocN(sizeof(*uv_verts) * face_len, "UvOverlapCoords"));
        indices = static_cast<uint(*)[3]>(
            MEM_mallocN(sizeof(*indices) * tri_len, "UvOverlapTris"));
        face_len_alloc = face_len;
      }

      int vert_index;
      BM_ITER_ELEM_INDEX (l, &liter, efa, BM_LOOPS_OF_FACE, vert_index) {
        float *luv = BM_ELEM_CD_GET_FLOAT_P(l, cd_loop_uv_offset);
        copy_v2_v2(uv_verts[vert_index], luv);
      }

      /* The UV coordinates winding could be positive of negative,
       * determine it automatically. */
      const int coords_sign = 0;
      BLI_polyfill_calc_arena(uv_verts, face_len, coords_sign, indices, arena);

      /* A beauty fill is necessary to remove degenerate triangles that may be produced from the
       * above poly-fill (see #103913), otherwise the overlap tests can fail. */
      BLI_polyfill_beautify(uv_verts, face_len, indices, arena, heap);

      for (int t = 0; t < tri_len; t++) {
        overlap_data[data_index].ob_index = ob_index;
        overlap_data[data_index].face_index = face_index;

        /* BVH needs 3D, overlap data uses 2D. */
        const float tri[3][3] = {
            {UNPACK2(uv_verts[indices[t][0]]), 0.0f},
            {UNPACK2(uv_verts[indices[t][1]]), 0.0f},
            {UNPACK2(uv_verts[indices[t][2]]), 0.0f},
        };

        copy_v2_v2(overlap_data[data_index].tri[0], tri[0]);
        copy_v2_v2(overlap_data[data_index].tri[1], tri[1]);
        copy_v2_v2(overlap_data[data_index].tri[2], tri[2]);

        BLI_bvhtree_insert(uv_tree, data_index, &tri[0][0], 3);
        data_index++;
      }

      BLI_memarena_clear(arena);
      BLI_heap_clear(heap, nullptr);
    }
  }
  BLI_assert(data_index == uv_tri_len);

  BLI_memarena_free(arena);
  BLI_heap_free(heap, nullptr);
  MEM_freeN(uv_verts);
  MEM_freeN(indices);

  BLI_bvhtree_balance(uv_tree);

  uint tree_overlap_len;
  BVHTreeOverlap *overlap = BLI_bvhtree_overlap_self(uv_tree, &tree_overlap_len, nullptr, nullptr);

  if (overlap != nullptr) {
    GSet *overlap_set = BLI_gset_new_ex(overlap_hash, overlap_cmp, __func__, tree_overlap_len);

    for (int i = 0; i < tree_overlap_len; i++) {
      /* Skip overlaps against yourself. */
      if (overlap[i].indexA == overlap[i].indexB) {
        continue;
      }

      /* Skip overlaps that have already been tested. */
      if (!BLI_gset_add(overlap_set, &overlap[i])) {
        continue;
      }

      const UVOverlapData *o_a = &overlap_data[overlap[i].indexA];
      const UVOverlapData *o_b = &overlap_data[overlap[i].indexB];
      Object *obedit_a = objects[o_a->ob_index];
      Object *obedit_b = objects[o_b->ob_index];
      BMEditMesh *em_a = BKE_editmesh_from_object(obedit_a);
      BMEditMesh *em_b = BKE_editmesh_from_object(obedit_b);
      BMFace *face_a = em_a->bm->ftable[o_a->face_index];
      BMFace *face_b = em_b->bm->ftable[o_b->face_index];
      const char *uv_a_name = CustomData_get_active_layer_name(&em_a->bm->ldata, CD_PROP_FLOAT2);
      const char *uv_b_name = CustomData_get_active_layer_name(&em_b->bm->ldata, CD_PROP_FLOAT2);
      BM_uv_map_ensure_vert_select_attr(em_a->bm, uv_a_name);
      BM_uv_map_ensure_vert_select_attr(em_b->bm, uv_b_name);
      BM_uv_map_ensure_edge_select_attr(em_a->bm, uv_a_name);
      BM_uv_map_ensure_edge_select_attr(em_b->bm, uv_b_name);
      const BMUVOffsets offsets_a = BM_uv_map_get_offsets(em_a->bm);
      const BMUVOffsets offsets_b = BM_uv_map_get_offsets(em_b->bm);

      /* Skip if both faces are already selected. */
      if (uvedit_face_select_test(scene, face_a, offsets_a) &&
          uvedit_face_select_test(scene, face_b, offsets_b))
      {
        continue;
      }

      /* Main tri-tri overlap test. */
      const float endpoint_bias = -1e-4f;
      if (overlap_tri_tri_uv_test(o_a->tri, o_b->tri, endpoint_bias)) {
        uvedit_face_select_enable(scene, em_a->bm, face_a, false, offsets_a);
        uvedit_face_select_enable(scene, em_b->bm, face_b, false, offsets_b);
      }
    }

    BLI_gset_free(overlap_set, nullptr);
    MEM_freeN(overlap);
  }

  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    uv_select_tag_update_for_object(depsgraph, scene->toolsettings, objects[ob_index]);
  }

  BLI_bvhtree_free(uv_tree);

  MEM_freeN(overlap_data);
  MEM_freeN(objects);

  return OPERATOR_FINISHED;
}

static int uv_select_overlap_exec(bContext *C, wmOperator *op)
{
  bool extend = RNA_boolean_get(op->ptr, "extend");
  return uv_select_overlap(C, extend);
}

void UV_OT_select_overlap(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Select Overlap";
  ot->description = "Select all UV faces which overlap each other";
  ot->idname = "UV_OT_select_overlap";
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* api callbacks */
  ot->exec = uv_select_overlap_exec;
  ot->poll = ED_operator_uvedit;

  /* properties */
  RNA_def_boolean(ot->srna,
                  "extend",
                  false,
                  "Extend",
                  "Extend selection rather than clearing the existing selection");
}

/** \} */
/** \name Select Similar Operator
 * \{ */

static float get_uv_vert_needle(const eUVSelectSimilar type,
                                BMVert *vert,
                                const float ob_m3[3][3],
                                BMLoop *loop,
                                const BMUVOffsets offsets)
{
  BLI_assert(offsets.pin >= 0);
  BLI_assert(offsets.uv >= 0);

  float result = 0.0f;
  switch (type) {
    case UV_SSIM_AREA_UV: {
      BMFace *f;
      BMIter iter;
      BM_ITER_ELEM (f, &iter, vert, BM_FACES_OF_VERT) {
        result += BM_face_calc_area_uv(f, offsets.uv);
      }
    } break;
    case UV_SSIM_AREA_3D: {
      BMFace *f;
      BMIter iter;
      BM_ITER_ELEM (f, &iter, vert, BM_FACES_OF_VERT) {
        result += BM_face_calc_area_with_mat3(f, ob_m3);
      }
    } break;
    case UV_SSIM_SIDES: {
      BMEdge *e;
      BMIter iter;
      BM_ITER_ELEM (e, &iter, vert, BM_EDGES_OF_VERT) {
        result += 1.0f;
      }
    } break;
    case UV_SSIM_PIN:
      return BM_ELEM_CD_GET_BOOL(loop, offsets.pin) ? 1.0f : 0.0f;
    default:
      BLI_assert_unreachable();
      return false;
  }

  return result;
}

static float get_uv_edge_needle(const eUVSelectSimilar type,
                                BMEdge *edge,
                                const float ob_m3[3][3],
                                BMLoop *loop_a,
                                BMLoop *loop_b,
                                const BMUVOffsets offsets)
{
  BLI_assert(offsets.pin >= 0);
  BLI_assert(offsets.uv >= 0);
  float result = 0.0f;
  switch (type) {
    case UV_SSIM_AREA_UV: {
      BMFace *f;
      BMIter iter;
      BM_ITER_ELEM (f, &iter, edge, BM_FACES_OF_EDGE) {
        result += BM_face_calc_area_uv(f, offsets.uv);
      }
    } break;
    case UV_SSIM_AREA_3D: {
      BMFace *f;
      BMIter iter;
      BM_ITER_ELEM (f, &iter, edge, BM_FACES_OF_EDGE) {
        result += BM_face_calc_area_with_mat3(f, ob_m3);
      }
    } break;
    case UV_SSIM_LENGTH_UV: {
      float *luv_a = BM_ELEM_CD_GET_FLOAT_P(loop_a, offsets.uv);
      float *luv_b = BM_ELEM_CD_GET_FLOAT_P(loop_b, offsets.uv);
      return len_v2v2(luv_a, luv_b);
    } break;
    case UV_SSIM_LENGTH_3D:
      return len_v3v3(edge->v1->co, edge->v2->co);
    case UV_SSIM_SIDES: {
      BMEdge *e;
      BMIter iter;
      BM_ITER_ELEM (e, &iter, edge, BM_FACES_OF_EDGE) {
        result += 1.0f;
      }
    } break;
    case UV_SSIM_PIN:
      if (BM_ELEM_CD_GET_BOOL(loop_a, offsets.pin)) {
        result += 1.0f;
      }
      if (BM_ELEM_CD_GET_BOOL(loop_b, offsets.pin)) {
        result += 1.0f;
      }
      break;
    default:
      BLI_assert_unreachable();
      return false;
  }

  return result;
}

static float get_uv_face_needle(const eUVSelectSimilar type,
                                BMFace *face,
                                int ob_index,
                                const float ob_m3[3][3],
                                const BMUVOffsets offsets)
{
  BLI_assert(offsets.pin >= 0);
  BLI_assert(offsets.uv >= 0);
  float result = 0.0f;
  switch (type) {
    case UV_SSIM_AREA_UV:
      return BM_face_calc_area_uv(face, offsets.uv);
    case UV_SSIM_AREA_3D:
      return BM_face_calc_area_with_mat3(face, ob_m3);
    case UV_SSIM_SIDES:
      return face->len;
    case UV_SSIM_OBJECT:
      return ob_index;
    case UV_SSIM_PIN: {
      BMLoop *l;
      BMIter liter;
      BM_ITER_ELEM (l, &liter, face, BM_LOOPS_OF_FACE) {
        if (BM_ELEM_CD_GET_BOOL(l, offsets.pin)) {
          result += 1.0f;
        }
      }
    } break;
    case UV_SSIM_MATERIAL:
      return face->mat_nr;
    case UV_SSIM_WINDING:
      return signum_i(BM_face_calc_area_uv_signed(face, offsets.uv));
    default:
      BLI_assert_unreachable();
      return false;
  }
  return result;
}

static float get_uv_island_needle(const eUVSelectSimilar type,
                                  const FaceIsland *island,
                                  const float ob_m3[3][3],
                                  const BMUVOffsets offsets)

{
  BLI_assert(offsets.uv >= 0);
  float result = 0.0f;
  switch (type) {
    case UV_SSIM_AREA_UV:
      for (int i = 0; i < island->faces_len; i++) {
        result += BM_face_calc_area_uv(island->faces[i], offsets.uv);
      }
      break;
    case UV_SSIM_AREA_3D:
      for (int i = 0; i < island->faces_len; i++) {
        result += BM_face_calc_area_with_mat3(island->faces[i], ob_m3);
      }
      break;
    case UV_SSIM_FACE:
      return island->faces_len;
    default:
      BLI_assert_unreachable();
      return false;
  }
  return result;
}

static int uv_select_similar_vert_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  ToolSettings *ts = CTX_data_tool_settings(C);

  const eUVSelectSimilar type = eUVSelectSimilar(RNA_enum_get(op->ptr, "type"));
  const float threshold = RNA_float_get(op->ptr, "threshold");
  const eSimilarCmp compare = eSimilarCmp(RNA_enum_get(op->ptr, "compare"));

  uint objects_len = 0;
  Object **objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data_with_uvs(
      scene, view_layer, ((View3D *)nullptr), &objects_len);

  int max_verts_selected_all = 0;
  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    Object *ob = objects[ob_index];
    BMEditMesh *em = BKE_editmesh_from_object(ob);
    BMFace *face;
    BMIter iter;
    BM_ITER_MESH (face, &iter, em->bm, BM_FACES_OF_MESH) {
      if (!uvedit_face_visible_test(scene, face)) {
        continue;
      }
      max_verts_selected_all += face->len;
    }
    /* TODO: Get a tighter bounds */
  }

  int tree_index = 0;
  KDTree_1d *tree_1d = BLI_kdtree_1d_new(max_verts_selected_all);

  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    Object *ob = objects[ob_index];
    BMEditMesh *em = BKE_editmesh_from_object(ob);
    BMesh *bm = em->bm;
    if (bm->totvertsel == 0) {
      continue;
    }

    const BMUVOffsets offsets = BM_uv_map_get_offsets(bm);
    float ob_m3[3][3];
    copy_m3_m4(ob_m3, ob->object_to_world);

    BMFace *face;
    BMIter iter;
    BM_ITER_MESH (face, &iter, em->bm, BM_FACES_OF_MESH) {
      if (!uvedit_face_visible_test(scene, face)) {
        continue;
      }
      BMLoop *l;
      BMIter liter;
      BM_ITER_ELEM (l, &liter, face, BM_LOOPS_OF_FACE) {
        if (!uvedit_uv_select_test(scene, l, offsets)) {
          continue;
        }
        float needle = get_uv_vert_needle(type, l->v, ob_m3, l, offsets);
        BLI_kdtree_1d_insert(tree_1d, tree_index++, &needle);
      }
    }
  }

  if (tree_1d != nullptr) {
    BLI_kdtree_1d_deduplicate(tree_1d);
    BLI_kdtree_1d_balance(tree_1d);
  }

  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    Object *ob = objects[ob_index];
    BMEditMesh *em = BKE_editmesh_from_object(ob);
    BMesh *bm = em->bm;
    if (bm->totvertsel == 0) {
      continue;
    }

    bool changed = false;

    const BMUVOffsets offsets = BM_uv_map_get_offsets(bm);
    float ob_m3[3][3];
    copy_m3_m4(ob_m3, ob->object_to_world);

    BMFace *face;
    BMIter iter;
    BM_ITER_MESH (face, &iter, em->bm, BM_FACES_OF_MESH) {
      if (!uvedit_face_visible_test(scene, face)) {
        continue;
      }
      BMLoop *l;
      BMIter liter;
      BM_ITER_ELEM (l, &liter, face, BM_LOOPS_OF_FACE) {
        if (uvedit_uv_select_test(scene, l, offsets)) {
          continue; /* Already selected. */
        }
        const float needle = get_uv_vert_needle(type, l->v, ob_m3, l, offsets);
        bool select = ED_select_similar_compare_float_tree(tree_1d, needle, threshold, compare);
        if (select) {
          uvedit_uv_select_set(scene, em->bm, l, select, false, offsets);
          changed = true;
        }
      }
      if (changed) {
        uv_select_tag_update_for_object(depsgraph, ts, ob);
      }
    }
  }

  MEM_SAFE_FREE(objects);
  BLI_kdtree_1d_free(tree_1d);
  return OPERATOR_FINISHED;
}

static int uv_select_similar_edge_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  ToolSettings *ts = CTX_data_tool_settings(C);

  const eUVSelectSimilar type = eUVSelectSimilar(RNA_enum_get(op->ptr, "type"));
  const float threshold = RNA_float_get(op->ptr, "threshold");
  const eSimilarCmp compare = eSimilarCmp(RNA_enum_get(op->ptr, "compare"));

  uint objects_len = 0;
  Object **objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data_with_uvs(
      scene, view_layer, ((View3D *)nullptr), &objects_len);

  int max_edges_selected_all = 0;
  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    Object *ob = objects[ob_index];
    BMEditMesh *em = BKE_editmesh_from_object(ob);
    BMFace *face;
    BMIter iter;
    BM_ITER_MESH (face, &iter, em->bm, BM_FACES_OF_MESH) {
      if (!uvedit_face_visible_test(scene, face)) {
        continue;
      }
      max_edges_selected_all += face->len;
    }
    /* TODO: Get a tighter bounds. */
  }

  int tree_index = 0;
  KDTree_1d *tree_1d = BLI_kdtree_1d_new(max_edges_selected_all);

  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    Object *ob = objects[ob_index];
    BMEditMesh *em = BKE_editmesh_from_object(ob);
    BMesh *bm = em->bm;
    if (bm->totvertsel == 0) {
      continue;
    }

    const BMUVOffsets offsets = BM_uv_map_get_offsets(bm);
    float ob_m3[3][3];
    copy_m3_m4(ob_m3, ob->object_to_world);

    BMFace *face;
    BMIter iter;
    BM_ITER_MESH (face, &iter, em->bm, BM_FACES_OF_MESH) {
      if (!uvedit_face_visible_test(scene, face)) {
        continue;
      }
      BMLoop *l;
      BMIter liter;
      BM_ITER_ELEM (l, &liter, face, BM_LOOPS_OF_FACE) {
        if (!uvedit_edge_select_test(scene, l, offsets)) {
          continue;
        }

        float needle = get_uv_edge_needle(type, l->e, ob_m3, l, l->next, offsets);
        if (tree_1d) {
          BLI_kdtree_1d_insert(tree_1d, tree_index++, &needle);
        }
      }
    }
  }

  if (tree_1d != nullptr) {
    BLI_kdtree_1d_deduplicate(tree_1d);
    BLI_kdtree_1d_balance(tree_1d);
  }

  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    Object *ob = objects[ob_index];
    BMEditMesh *em = BKE_editmesh_from_object(ob);
    BMesh *bm = em->bm;
    if (bm->totvertsel == 0) {
      continue;
    }

    bool changed = false;
    const BMUVOffsets offsets = BM_uv_map_get_offsets(bm);
    float ob_m3[3][3];
    copy_m3_m4(ob_m3, ob->object_to_world);

    BMFace *face;
    BMIter iter;
    BM_ITER_MESH (face, &iter, em->bm, BM_FACES_OF_MESH) {
      if (!uvedit_face_visible_test(scene, face)) {
        continue;
      }
      BMLoop *l;
      BMIter liter;
      BM_ITER_ELEM (l, &liter, face, BM_LOOPS_OF_FACE) {
        if (uvedit_edge_select_test(scene, l, offsets)) {
          continue; /* Already selected. */
        }

        float needle = get_uv_edge_needle(type, l->e, ob_m3, l, l->next, offsets);
        bool select = ED_select_similar_compare_float_tree(tree_1d, needle, threshold, compare);
        if (select) {
          uvedit_edge_select_set(scene, em->bm, l, select, false, offsets);
          changed = true;
        }
      }
      if (changed) {
        uv_select_tag_update_for_object(depsgraph, ts, ob);
      }
    }
  }

  MEM_SAFE_FREE(objects);
  BLI_kdtree_1d_free(tree_1d);
  return OPERATOR_FINISHED;
}

static int uv_select_similar_face_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  ToolSettings *ts = CTX_data_tool_settings(C);

  const eUVSelectSimilar type = eUVSelectSimilar(RNA_enum_get(op->ptr, "type"));
  const float threshold = RNA_float_get(op->ptr, "threshold");
  const eSimilarCmp compare = eSimilarCmp(RNA_enum_get(op->ptr, "compare"));

  uint objects_len = 0;
  Object **objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data_with_uvs(
      scene, view_layer, ((View3D *)nullptr), &objects_len);

  int max_faces_selected_all = 0;
  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    Object *ob = objects[ob_index];
    BMEditMesh *em = BKE_editmesh_from_object(ob);
    max_faces_selected_all += em->bm->totfacesel;
    /* TODO: Get a tighter bounds */
  }

  int tree_index = 0;
  KDTree_1d *tree_1d = BLI_kdtree_1d_new(max_faces_selected_all);

  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    Object *ob = objects[ob_index];

    BMEditMesh *em = BKE_editmesh_from_object(ob);
    BMesh *bm = em->bm;

    float ob_m3[3][3];
    copy_m3_m4(ob_m3, ob->object_to_world);

    const BMUVOffsets offsets = BM_uv_map_get_offsets(bm);

    BMFace *face;
    BMIter iter;
    BM_ITER_MESH (face, &iter, bm, BM_FACES_OF_MESH) {
      if (!uvedit_face_visible_test(scene, face)) {
        continue;
      }
      if (!uvedit_face_select_test(scene, face, offsets)) {
        continue;
      }

      float needle = get_uv_face_needle(type, face, ob_index, ob_m3, offsets);
      if (tree_1d) {
        BLI_kdtree_1d_insert(tree_1d, tree_index++, &needle);
      }
    }
  }

  if (tree_1d != nullptr) {
    BLI_kdtree_1d_deduplicate(tree_1d);
    BLI_kdtree_1d_balance(tree_1d);
  }

  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    Object *ob = objects[ob_index];

    BMEditMesh *em = BKE_editmesh_from_object(ob);
    BMesh *bm = em->bm;
    bool changed = false;
    bool do_history = false;
    const BMUVOffsets offsets = BM_uv_map_get_offsets(bm);

    float ob_m3[3][3];
    copy_m3_m4(ob_m3, ob->object_to_world);

    BMFace *face;
    BMIter iter;
    BM_ITER_MESH (face, &iter, bm, BM_FACES_OF_MESH) {
      if (!uvedit_face_visible_test(scene, face)) {
        continue;
      }
      if (uvedit_face_select_test(scene, face, offsets)) {
        continue;
      }

      float needle = get_uv_face_needle(type, face, ob_index, ob_m3, offsets);

      bool select = ED_select_similar_compare_float_tree(tree_1d, needle, threshold, compare);
      if (select) {
        uvedit_face_select_set(scene, bm, face, select, do_history, offsets);
        changed = true;
      }
    }
    if (changed) {
      uv_select_tag_update_for_object(depsgraph, ts, ob);
    }
  }

  MEM_SAFE_FREE(objects);
  BLI_kdtree_1d_free(tree_1d);
  return OPERATOR_FINISHED;
}

static bool uv_island_selected(const Scene *scene, FaceIsland *island)
{
  BLI_assert(island && island->faces_len);
  return uvedit_face_select_test(scene, island->faces[0], island->offsets);
}

static int uv_select_similar_island_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  ToolSettings *ts = CTX_data_tool_settings(C);

  const eUVSelectSimilar type = eUVSelectSimilar(RNA_enum_get(op->ptr, "type"));
  const float threshold = RNA_float_get(op->ptr, "threshold");
  const eSimilarCmp compare = eSimilarCmp(RNA_enum_get(op->ptr, "compare"));

  uint objects_len = 0;
  Object **objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data_with_uvs(
      scene, view_layer, ((View3D *)nullptr), &objects_len);

  ListBase *island_list_ptr = static_cast<ListBase *>(
      MEM_callocN(sizeof(*island_list_ptr) * objects_len, __func__));
  int island_list_len = 0;

  const bool face_selected = !(scene->toolsettings->uv_flag & UV_SYNC_SELECTION);

  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    Object *obedit = objects[ob_index];
    BMEditMesh *em = BKE_editmesh_from_object(obedit);
    const BMUVOffsets offsets = BM_uv_map_get_offsets(em->bm);
    if (offsets.uv == -1) {
      continue;
    }

    float aspect_y = 1.0f; /* Placeholder value, aspect doesn't change connectivity. */
    island_list_len += bm_mesh_calc_uv_islands(
        scene, em->bm, &island_list_ptr[ob_index], face_selected, false, false, aspect_y, offsets);
  }

  FaceIsland **island_array = static_cast<FaceIsland **>(
      MEM_callocN(sizeof(*island_array) * island_list_len, __func__));

  int tree_index = 0;
  KDTree_1d *tree_1d = BLI_kdtree_1d_new(island_list_len);

  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    Object *obedit = objects[ob_index];
    BMEditMesh *em = BKE_editmesh_from_object(obedit);
    const int cd_loop_uv_offset = CustomData_get_offset(&em->bm->ldata, CD_PROP_FLOAT2);
    if (cd_loop_uv_offset == -1) {
      continue;
    }

    float ob_m3[3][3];
    copy_m3_m4(ob_m3, obedit->object_to_world);

    int index;
    LISTBASE_FOREACH_INDEX (FaceIsland *, island, &island_list_ptr[ob_index], index) {
      island_array[index] = island;
      if (!uv_island_selected(scene, island)) {
        continue;
      }
      float needle = get_uv_island_needle(type, island, ob_m3, island->offsets);
      if (tree_1d) {
        BLI_kdtree_1d_insert(tree_1d, tree_index++, &needle);
      }
    }
  }

  if (tree_1d != nullptr) {
    BLI_kdtree_1d_deduplicate(tree_1d);
    BLI_kdtree_1d_balance(tree_1d);
  }

  int tot_island_index = 0;
  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    Object *obedit = objects[ob_index];
    BMEditMesh *em = BKE_editmesh_from_object(obedit);
    const int cd_loop_uv_offset = CustomData_get_offset(&em->bm->ldata, CD_PROP_FLOAT2);
    if (cd_loop_uv_offset == -1) {
      continue;
    }
    float ob_m3[3][3];
    copy_m3_m4(ob_m3, obedit->object_to_world);

    bool changed = false;
    int index;
    LISTBASE_FOREACH_INDEX (FaceIsland *, island, &island_list_ptr[ob_index], index) {
      island_array[tot_island_index++] = island; /* To deallocate later. */
      if (uv_island_selected(scene, island)) {
        continue;
      }
      float needle = get_uv_island_needle(type, island, ob_m3, island->offsets);
      bool select = ED_select_similar_compare_float_tree(tree_1d, needle, threshold, compare);
      if (!select) {
        continue;
      }
      bool do_history = false;
      for (int j = 0; j < island->faces_len; j++) {
        uvedit_face_select_set(
            scene, em->bm, island->faces[j], select, do_history, island->offsets);
      }
      changed = true;
    }

    if (changed) {
      uv_select_tag_update_for_object(depsgraph, ts, obedit);
    }
  }

  BLI_assert(tot_island_index == island_list_len);
  for (int i = 0; i < island_list_len; i++) {
    MEM_SAFE_FREE(island_array[i]->faces);
    MEM_SAFE_FREE(island_array[i]);
  }

  MEM_SAFE_FREE(island_array);
  MEM_SAFE_FREE(island_list_ptr);
  MEM_SAFE_FREE(objects);
  BLI_kdtree_1d_free(tree_1d);

  return OPERATOR_FINISHED;
}

/* Select similar UV faces/edges/verts based on current selection. */
static int uv_select_similar_exec(bContext *C, wmOperator *op)
{
  ToolSettings *ts = CTX_data_tool_settings(C);
  PropertyRNA *prop = RNA_struct_find_property(op->ptr, "threshold");

  if (!RNA_property_is_set(op->ptr, prop)) {
    RNA_property_float_set(op->ptr, prop, ts->select_thresh);
  }
  else {
    ts->select_thresh = RNA_property_float_get(op->ptr, prop);
  }

  int selectmode = (ts->uv_flag & UV_SYNC_SELECTION) ? ts->selectmode : ts->uv_selectmode;
  if (selectmode & UV_SELECT_EDGE) {
    return uv_select_similar_edge_exec(C, op);
  }
  if (selectmode & UV_SELECT_FACE) {
    return uv_select_similar_face_exec(C, op);
  }
  if (selectmode & UV_SELECT_ISLAND) {
    return uv_select_similar_island_exec(C, op);
  }

  return uv_select_similar_vert_exec(C, op);
}

static EnumPropertyItem uv_select_similar_type_items[] = {
    {UV_SSIM_PIN, "PIN", 0, "Pinned", ""},
    {UV_SSIM_LENGTH_UV, "LENGTH", 0, "Length", ""},
    {UV_SSIM_LENGTH_3D, "LENGTH_3D", 0, "Length 3D", ""},
    {UV_SSIM_AREA_UV, "AREA", 0, "Area", ""},
    {UV_SSIM_AREA_3D, "AREA_3D", 0, "Area 3D", ""},
    {UV_SSIM_MATERIAL, "MATERIAL", 0, "Material", ""},
    {UV_SSIM_OBJECT, "OBJECT", 0, "Object", ""},
    {UV_SSIM_SIDES, "SIDES", 0, "Polygon Sides", ""},
    {UV_SSIM_WINDING, "WINDING", 0, "Winding", ""},
    {UV_SSIM_FACE, "FACE", 0, "Amount of Faces in Island", ""},
    {0}};

static EnumPropertyItem prop_similar_compare_types[] = {{SIM_CMP_EQ, "EQUAL", 0, "Equal", ""},
                                                        {SIM_CMP_GT, "GREATER", 0, "Greater", ""},
                                                        {SIM_CMP_LT, "LESS", 0, "Less", ""},
                                                        {0}};

static const EnumPropertyItem *uv_select_similar_type_itemf(bContext *C,
                                                            PointerRNA * /*ptr*/,
                                                            PropertyRNA * /*prop*/,
                                                            bool *r_free)
{
  EnumPropertyItem *item = nullptr;
  int totitem = 0;

  const ToolSettings *ts = CTX_data_tool_settings(C);
  if (ts) {
    int selectmode = (ts->uv_flag & UV_SYNC_SELECTION) ? ts->selectmode : ts->uv_selectmode;
    if (selectmode & UV_SELECT_VERTEX) {
      RNA_enum_items_add_value(&item, &totitem, uv_select_similar_type_items, UV_SSIM_PIN);
    }
    else if (selectmode & UV_SELECT_EDGE) {
      RNA_enum_items_add_value(&item, &totitem, uv_select_similar_type_items, UV_SSIM_LENGTH_UV);
      RNA_enum_items_add_value(&item, &totitem, uv_select_similar_type_items, UV_SSIM_LENGTH_3D);
      RNA_enum_items_add_value(&item, &totitem, uv_select_similar_type_items, UV_SSIM_PIN);
    }
    else if (selectmode & UV_SELECT_FACE) {
      RNA_enum_items_add_value(&item, &totitem, uv_select_similar_type_items, UV_SSIM_AREA_UV);
      RNA_enum_items_add_value(&item, &totitem, uv_select_similar_type_items, UV_SSIM_AREA_3D);
      RNA_enum_items_add_value(&item, &totitem, uv_select_similar_type_items, UV_SSIM_MATERIAL);
      RNA_enum_items_add_value(&item, &totitem, uv_select_similar_type_items, UV_SSIM_OBJECT);
      RNA_enum_items_add_value(&item, &totitem, uv_select_similar_type_items, UV_SSIM_SIDES);
      RNA_enum_items_add_value(&item, &totitem, uv_select_similar_type_items, UV_SSIM_WINDING);
    }
    else if (selectmode & UV_SELECT_ISLAND) {
      RNA_enum_items_add_value(&item, &totitem, uv_select_similar_type_items, UV_SSIM_AREA_UV);
      RNA_enum_items_add_value(&item, &totitem, uv_select_similar_type_items, UV_SSIM_AREA_3D);
      RNA_enum_items_add_value(&item, &totitem, uv_select_similar_type_items, UV_SSIM_FACE);
    }
  }
  else {
    RNA_enum_items_add_value(&item, &totitem, uv_select_similar_type_items, UV_SSIM_PIN);
  }

  RNA_enum_item_end(&item, &totitem);
  *r_free = true;
  return item;
}

void UV_OT_select_similar(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Select Similar";
  ot->description = "Select similar UVs by property types";
  ot->idname = "UV_OT_select_similar";

  /* api callbacks */
  ot->invoke = WM_menu_invoke;
  ot->exec = uv_select_similar_exec;
  ot->poll = ED_operator_uvedit_space_image;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  PropertyRNA *prop = ot->prop = RNA_def_enum(
      ot->srna, "type", uv_select_similar_type_items, SIMVERT_NORMAL, "Type", "");
  RNA_def_enum_funcs(prop, uv_select_similar_type_itemf);
  RNA_def_enum(ot->srna, "compare", prop_similar_compare_types, SIM_CMP_EQ, "Compare", "");
  RNA_def_float(ot->srna, "threshold", 0.0f, 0.0f, 1.0f, "Threshold", "", 0.0f, 1.0f);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Selected Elements as Arrays (Vertex, Edge & Faces)
 *
 * These functions return single elements per connected vertex/edge.
 * So an edge that has two connected edge loops only assigns one loop in the array.
 * \{ */

BMFace **ED_uvedit_selected_faces(const Scene *scene, BMesh *bm, int len_max, int *r_faces_len)
{
  const BMUVOffsets offsets = BM_uv_map_get_offsets(bm);

  CLAMP_MAX(len_max, bm->totface);
  int faces_len = 0;
  BMFace **faces = static_cast<BMFace **>(MEM_mallocN(sizeof(*faces) * len_max, __func__));

  BMIter iter;
  BMFace *f;
  BM_ITER_MESH (f, &iter, bm, BM_FACES_OF_MESH) {
    if (uvedit_face_visible_test(scene, f)) {
      if (uvedit_face_select_test(scene, f, offsets)) {
        faces[faces_len++] = f;
        if (faces_len == len_max) {
          goto finally;
        }
      }
    }
  }

finally:
  *r_faces_len = faces_len;
  if (faces_len != len_max) {
    faces = static_cast<BMFace **>(MEM_reallocN(faces, sizeof(*faces) * faces_len));
  }
  return faces;
}

BMLoop **ED_uvedit_selected_edges(const Scene *scene, BMesh *bm, int len_max, int *r_edges_len)
{
  const BMUVOffsets offsets = BM_uv_map_get_offsets(bm);
  BLI_assert(offsets.uv >= 0);

  CLAMP_MAX(len_max, bm->totloop);
  int edges_len = 0;
  BMLoop **edges = static_cast<BMLoop **>(MEM_mallocN(sizeof(*edges) * len_max, __func__));

  BMIter iter;
  BMFace *f;

  /* Clear tag. */
  BM_ITER_MESH (f, &iter, bm, BM_FACES_OF_MESH) {
    BMIter liter;
    BMLoop *l_iter;
    BM_ITER_ELEM (l_iter, &liter, f, BM_LOOPS_OF_FACE) {
      BM_elem_flag_disable(l_iter, BM_ELEM_TAG);
    }
  }

  BM_ITER_MESH (f, &iter, bm, BM_FACES_OF_MESH) {
    if (uvedit_face_visible_test(scene, f)) {
      BMIter liter;
      BMLoop *l_iter;
      BM_ITER_ELEM (l_iter, &liter, f, BM_LOOPS_OF_FACE) {
        if (!BM_elem_flag_test(l_iter, BM_ELEM_TAG)) {
          if (uvedit_edge_select_test(scene, l_iter, offsets)) {
            BM_elem_flag_enable(l_iter, BM_ELEM_TAG);

            edges[edges_len++] = l_iter;
            if (edges_len == len_max) {
              goto finally;
            }

            /* Tag other connected loops so we don't consider them separate edges. */
            if (l_iter != l_iter->radial_next) {
              BMLoop *l_radial_iter = l_iter->radial_next;
              do {
                if (BM_loop_uv_share_edge_check(l_iter, l_radial_iter, offsets.uv)) {
                  BM_elem_flag_enable(l_radial_iter, BM_ELEM_TAG);
                }
              } while ((l_radial_iter = l_radial_iter->radial_next) != l_iter);
            }
          }
        }
      }
    }
  }

finally:
  *r_edges_len = edges_len;
  if (edges_len != len_max) {
    edges = static_cast<BMLoop **>(MEM_reallocN(edges, sizeof(*edges) * edges_len));
  }
  return edges;
}

BMLoop **ED_uvedit_selected_verts(const Scene *scene, BMesh *bm, int len_max, int *r_verts_len)
{
  const BMUVOffsets offsets = BM_uv_map_get_offsets(bm);
  BLI_assert(offsets.select_vert >= 0);
  BLI_assert(offsets.uv >= 0);

  CLAMP_MAX(len_max, bm->totloop);
  int verts_len = 0;
  BMLoop **verts = static_cast<BMLoop **>(MEM_mallocN(sizeof(*verts) * len_max, __func__));

  BMIter iter;
  BMFace *f;

  /* Clear tag. */
  BM_ITER_MESH (f, &iter, bm, BM_FACES_OF_MESH) {
    BMIter liter;
    BMLoop *l_iter;
    BM_ITER_ELEM (l_iter, &liter, f, BM_LOOPS_OF_FACE) {
      BM_elem_flag_disable(l_iter, BM_ELEM_TAG);
    }
  }

  BM_ITER_MESH (f, &iter, bm, BM_FACES_OF_MESH) {
    if (uvedit_face_visible_test(scene, f)) {
      BMIter liter;
      BMLoop *l_iter;
      BM_ITER_ELEM (l_iter, &liter, f, BM_LOOPS_OF_FACE) {
        if (!BM_elem_flag_test(l_iter, BM_ELEM_TAG)) {
          if (BM_ELEM_CD_GET_BOOL(l_iter, offsets.select_vert)) {
            BM_elem_flag_enable(l_iter->v, BM_ELEM_TAG);

            verts[verts_len++] = l_iter;
            if (verts_len == len_max) {
              goto finally;
            }

            /* Tag other connected loops so we don't consider them separate vertices. */
            BMIter liter_disk;
            BMLoop *l_disk_iter;
            BM_ITER_ELEM (l_disk_iter, &liter_disk, l_iter->v, BM_LOOPS_OF_VERT) {
              if (BM_loop_uv_share_vert_check(l_iter, l_disk_iter, offsets.uv)) {
                BM_elem_flag_enable(l_disk_iter, BM_ELEM_TAG);
              }
            }
          }
        }
      }
    }
  }

finally:
  *r_verts_len = verts_len;
  if (verts_len != len_max) {
    verts = static_cast<BMLoop **>(MEM_reallocN(verts, sizeof(*verts) * verts_len));
  }
  return verts;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Select Mode UV Vert/Edge/Face/Island Operator
 * \{ */

/**
 * Deselects UVs that are not part of a complete island selection.
 *
 * Use only when sync select disabled.
 */
static void uv_isolate_selected_islands(const Scene *scene,
                                        BMEditMesh *em,
                                        const BMUVOffsets offsets)
{
  BLI_assert((scene->toolsettings->uv_flag & UV_SYNC_SELECTION) == 0);
  BMFace *efa;
  BMIter iter, liter;
  UvElementMap *elementmap = BM_uv_element_map_create(em->bm, scene, false, false, true, true);
  if (elementmap == nullptr) {
    return;
  }
  BLI_assert(offsets.select_vert >= 0);
  BLI_assert(offsets.select_edge >= 0);

  int num_islands = elementmap->total_islands;
  /* Boolean array that tells if island with index i is completely selected or not. */
  bool *is_island_not_selected = static_cast<bool *>(
      MEM_callocN(sizeof(bool) * (num_islands), __func__));

  BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
    BMLoop *l;
    if (!uvedit_face_visible_test(scene, efa)) {
      BM_elem_flag_disable(efa, BM_ELEM_TAG);
      continue;
    }
    BM_elem_flag_enable(efa, BM_ELEM_TAG);
    BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
      if (!uvedit_edge_select_test(scene, l, offsets)) {
        UvElement *element = BM_uv_element_get(elementmap, l);
        if (element) {
          is_island_not_selected[element->island] = true;
        }
      }
    }
  }

  BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
    BMLoop *l;
    if (!BM_elem_flag_test(efa, BM_ELEM_TAG)) {
      continue;
    }
    BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
      UvElement *element = BM_uv_element_get(elementmap, l);
      /* Deselect all elements of islands which are not completely selected. */
      if (element && is_island_not_selected[element->island]) {
        BM_ELEM_CD_SET_BOOL(l, offsets.select_vert, false);
        BM_ELEM_CD_SET_BOOL(l, offsets.select_edge, false);
      }
    }
  }

  BM_uv_element_map_free(elementmap);
  MEM_freeN(is_island_not_selected);
}

void ED_uvedit_selectmode_clean(const Scene *scene, Object *obedit)
{
  const ToolSettings *ts = scene->toolsettings;
  BLI_assert((ts->uv_flag & UV_SYNC_SELECTION) == 0);
  BMEditMesh *em = BKE_editmesh_from_object(obedit);
  char sticky = ts->uv_sticky;
  const char *active_uv_name = CustomData_get_active_layer_name(&em->bm->ldata, CD_PROP_FLOAT2);
  BM_uv_map_ensure_vert_select_attr(em->bm, active_uv_name);
  BM_uv_map_ensure_edge_select_attr(em->bm, active_uv_name);
  const BMUVOffsets offsets = BM_uv_map_get_offsets(em->bm);
  BMFace *efa;
  BMLoop *l;
  BMIter iter, liter;

  if (ts->uv_selectmode == UV_SELECT_VERTEX) {
    /* Vertex mode. */
    if (sticky != SI_STICKY_DISABLE) {
      bm_loop_tags_clear(em->bm);
      BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
        if (!uvedit_face_visible_test(scene, efa)) {
          continue;
        }
        BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
          if (uvedit_uv_select_test(scene, l, offsets)) {
            BM_elem_flag_enable(l, BM_ELEM_TAG);
          }
        }
      }
      uv_select_flush_from_tag_loop(scene, obedit, true);
    }
  }

  else if (ts->uv_selectmode == UV_SELECT_EDGE) {
    /* Edge mode. */
    if (sticky != SI_STICKY_DISABLE) {
      BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
        if (!uvedit_face_visible_test(scene, efa)) {
          continue;
        }
        BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
          if (uvedit_edge_select_test(scene, l, offsets)) {
            uvedit_edge_select_set_noflush(scene, l, true, sticky, offsets);
          }
        }
      }
    }
    uv_select_flush_from_loop_edge_flag(scene, em);
  }

  else if (ts->uv_selectmode == UV_SELECT_FACE) {
    /* Face mode. */
    BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
      BM_elem_flag_disable(efa, BM_ELEM_TAG);
      if (uvedit_face_visible_test(scene, efa)) {
        if (uvedit_face_select_test(scene, efa, offsets)) {
          BM_elem_flag_enable(efa, BM_ELEM_TAG);
        }
        uvedit_face_select_set(scene, em->bm, efa, false, false, offsets);
      }
    }
    uv_select_flush_from_tag_face(scene, obedit, true);
  }

  else if (ts->uv_selectmode == UV_SELECT_ISLAND) {
    /* Island mode. */
    uv_isolate_selected_islands(scene, em, offsets);
  }

  ED_uvedit_selectmode_flush(scene, em);
}

void ED_uvedit_selectmode_clean_multi(bContext *C)
{
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);

  uint objects_len = 0;
  Object **objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data_with_uvs(
      scene, view_layer, ((View3D *)nullptr), &objects_len);

  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    Object *obedit = objects[ob_index];
    ED_uvedit_selectmode_clean(scene, obedit);

    uv_select_tag_update_for_object(depsgraph, scene->toolsettings, obedit);
  }
  MEM_freeN(objects);
}

static int uv_select_mode_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);
  ToolSettings *ts = scene->toolsettings;
  const char new_uv_selectmode = RNA_enum_get(op->ptr, "type");

  /* Early exit if no change in current selection mode */
  if (new_uv_selectmode == ts->uv_selectmode) {
    return OPERATOR_CANCELLED;
  }

  /* Set new UV select mode. */
  ts->uv_selectmode = new_uv_selectmode;

  /* Handle UV selection states according to new select mode and sticky mode. */
  ED_uvedit_selectmode_clean_multi(C);

  DEG_id_tag_update(&scene->id, ID_RECALC_COPY_ON_WRITE | ID_RECALC_SELECT);
  WM_main_add_notifier(NC_SCENE | ND_TOOLSETTINGS, nullptr);

  return OPERATOR_FINISHED;
}

static int uv_select_mode_invoke(bContext *C, wmOperator *op, const wmEvent * /*event*/)
{
  const ToolSettings *ts = CTX_data_tool_settings(C);
  const SpaceImage *sima = CTX_wm_space_image(C);

  /* Could be removed? - Already done in poll callback. */
  if ((!sima) || (sima->mode != SI_MODE_UV)) {
    return OPERATOR_CANCELLED;
  }
  /* Pass through when UV sync selection is enabled.
   * Allow for mesh select-mode key-map. */
  if (ts->uv_flag & UV_SYNC_SELECTION) {
    return OPERATOR_PASS_THROUGH;
  }

  return uv_select_mode_exec(C, op);
}

void UV_OT_select_mode(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "UV Select Mode";
  ot->description = "Change UV selection mode";
  ot->idname = "UV_OT_select_mode";

  /* api callbacks */
  ot->invoke = uv_select_mode_invoke;
  ot->exec = uv_select_mode_exec;
  ot->poll = ED_operator_uvedit_space_image;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* RNA props */
  PropertyRNA *prop;
  ot->prop = prop = RNA_def_enum(
      ot->srna, "type", rna_enum_mesh_select_mode_uv_items, 0, "Type", "");
  RNA_def_property_flag(prop, PropertyFlag(PROP_HIDDEN | PROP_SKIP_SAVE));
}

/** \} */
