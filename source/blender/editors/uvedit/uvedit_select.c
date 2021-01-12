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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 */

/** \file
 * \ingroup eduv
 */

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "DNA_image_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_node_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_space_types.h"

#include "BLI_alloca.h"
#include "BLI_blenlib.h"
#include "BLI_hash.h"
#include "BLI_kdopbvh.h"
#include "BLI_lasso_2d.h"
#include "BLI_math.h"
#include "BLI_polyfill_2d.h"
#include "BLI_utildefines.h"

#include "BKE_context.h"
#include "BKE_customdata.h"
#include "BKE_editmesh.h"
#include "BKE_layer.h"
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

#include "WM_api.h"
#include "WM_types.h"

#include "UI_view2d.h"

#include "uvedit_intern.h"

static void uv_select_all_perform(Scene *scene, Object *obedit, int action);

static void uv_select_all_perform_multi_ex(
    Scene *scene, Object **objects, const uint objects_len, int action, const Object *ob_exclude);
static void uv_select_all_perform_multi(Scene *scene,
                                        Object **objects,
                                        const uint objects_len,
                                        int action);

static void uv_select_flush_from_tag_face(SpaceImage *sima,
                                          Scene *scene,
                                          Object *obedit,
                                          const bool select);
static void uv_select_flush_from_tag_loop(SpaceImage *sima,
                                          Scene *scene,
                                          Object *obedit,
                                          const bool select);
static void uv_select_tag_update_for_object(Depsgraph *depsgraph,
                                            const ToolSettings *ts,
                                            Object *obedit);

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
  BMEditSelection *ese = bm->selected.last;
  if (ese && ese->prev) {
    BMEditSelection *ese_prev = ese->prev;
    if ((ese->htype == BM_VERT) && (ese_prev->htype == BM_FACE)) {
      /* May be NULL. */
      return BM_face_vert_share_loop((BMFace *)ese_prev->ele, (BMVert *)ese->ele);
    }
  }
  return NULL;
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
  BMEditSelection *ese = bm->selected.last;
  if (ese && ese->prev) {
    BMEditSelection *ese_prev = ese->prev;
    if ((ese->htype == BM_EDGE) && (ese_prev->htype == BM_FACE)) {
      /* May be NULL. */
      return BM_face_edge_share_loop((BMFace *)ese_prev->ele, (BMEdge *)ese->ele);
    }
  }
  return NULL;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Visibility and Selection Utilities
 * \{ */

/**
 * Intentionally don't return #UV_SELECT_ISLAND as it's not an element type.
 * In this case return #UV_SELECT_VERTEX as a fallback.
 */
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
                                        int cd_loop_uv_offset)
{
  BMFace *efa;
  BMLoop *l;
  BMIter iter, liter;

  BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
    BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
      if (BM_elem_flag_test(l->v, BM_ELEM_TAG)) {
        uvedit_uv_select_set(scene, em, l, select, false, cd_loop_uv_offset);
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

bool uvedit_face_select_test_ex(const ToolSettings *ts, BMFace *efa, const int cd_loop_uv_offset)
{
  if (ts->uv_flag & UV_SYNC_SELECTION) {
    return (BM_elem_flag_test(efa, BM_ELEM_SELECT));
  }

  BMLoop *l;
  MLoopUV *luv;
  BMIter liter;

  BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
    luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
    if (!(luv->flag & MLOOPUV_VERTSEL)) {
      return false;
    }
  }
  return true;
}
bool uvedit_face_select_test(const Scene *scene, BMFace *efa, const int cd_loop_uv_offset)
{
  return uvedit_face_select_test_ex(scene->toolsettings, efa, cd_loop_uv_offset);
}

void uvedit_face_select_set_with_sticky(const SpaceImage *sima,
                                        const Scene *scene,
                                        BMEditMesh *em,
                                        BMFace *efa,
                                        const bool select,
                                        const bool do_history,
                                        const int cd_loop_uv_offset)
{
  const ToolSettings *ts = scene->toolsettings;
  if (ts->uv_flag & UV_SYNC_SELECTION) {
    uvedit_face_select_set(scene, em, efa, select, do_history, cd_loop_uv_offset);
    return;
  }

  BMLoop *l_iter, *l_first;
  l_iter = l_first = BM_FACE_FIRST_LOOP(efa);
  do {
    uvedit_uv_select_set_with_sticky(
        sima, scene, em, l_iter, select, do_history, cd_loop_uv_offset);
  } while ((l_iter = l_iter->next) != l_first);
}

void uvedit_face_select_set(const struct Scene *scene,
                            struct BMEditMesh *em,
                            struct BMFace *efa,
                            const bool select,
                            const bool do_history,
                            const int cd_loop_uv_offset)
{
  if (select) {
    uvedit_face_select_enable(scene, em, efa, do_history, cd_loop_uv_offset);
  }
  else {
    uvedit_face_select_disable(scene, em, efa, cd_loop_uv_offset);
  }
}

void uvedit_face_select_enable(const Scene *scene,
                               BMEditMesh *em,
                               BMFace *efa,
                               const bool do_history,
                               const int cd_loop_uv_offset)
{
  const ToolSettings *ts = scene->toolsettings;

  if (ts->uv_flag & UV_SYNC_SELECTION) {
    BM_face_select_set(em->bm, efa, true);
    if (do_history) {
      BM_select_history_store(em->bm, (BMElem *)efa);
    }
  }
  else {
    BMLoop *l;
    MLoopUV *luv;
    BMIter liter;

    BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
      luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
      luv->flag |= MLOOPUV_VERTSEL;
    }
  }
}

void uvedit_face_select_disable(const Scene *scene,
                                BMEditMesh *em,
                                BMFace *efa,
                                const int cd_loop_uv_offset)
{
  const ToolSettings *ts = scene->toolsettings;

  if (ts->uv_flag & UV_SYNC_SELECTION) {
    BM_face_select_set(em->bm, efa, false);
  }
  else {
    BMLoop *l;
    MLoopUV *luv;
    BMIter liter;

    BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
      luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
      luv->flag &= ~MLOOPUV_VERTSEL;
    }
  }
}

bool uvedit_edge_select_test_ex(const ToolSettings *ts, BMLoop *l, const int cd_loop_uv_offset)
{
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

  MLoopUV *luv1, *luv2;

  luv1 = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
  luv2 = BM_ELEM_CD_GET_VOID_P(l->next, cd_loop_uv_offset);

  return (luv1->flag & MLOOPUV_VERTSEL) && (luv2->flag & MLOOPUV_VERTSEL);
}
bool uvedit_edge_select_test(const Scene *scene, BMLoop *l, const int cd_loop_uv_offset)
{
  return uvedit_edge_select_test_ex(scene->toolsettings, l, cd_loop_uv_offset);
}

void uvedit_edge_select_set_with_sticky(const struct SpaceImage *sima,
                                        const Scene *scene,
                                        BMEditMesh *em,
                                        BMLoop *l,
                                        const bool select,
                                        const bool do_history,
                                        const uint cd_loop_uv_offset)
{
  const ToolSettings *ts = scene->toolsettings;
  if (ts->uv_flag & UV_SYNC_SELECTION) {
    uvedit_edge_select_set(scene, em, l, select, do_history, cd_loop_uv_offset);
    return;
  }

  uvedit_uv_select_set_with_sticky(sima, scene, em, l, select, do_history, cd_loop_uv_offset);
  uvedit_uv_select_set_with_sticky(
      sima, scene, em, l->next, select, do_history, cd_loop_uv_offset);
}

void uvedit_edge_select_set(const Scene *scene,
                            BMEditMesh *em,
                            BMLoop *l,
                            const bool select,
                            const bool do_history,
                            const int cd_loop_uv_offset)

{
  if (select) {
    uvedit_edge_select_enable(scene, em, l, do_history, cd_loop_uv_offset);
  }
  else {
    uvedit_edge_select_disable(scene, em, l, cd_loop_uv_offset);
  }
}

void uvedit_edge_select_enable(const Scene *scene,
                               BMEditMesh *em,
                               BMLoop *l,
                               const bool do_history,
                               const int cd_loop_uv_offset)

{
  const ToolSettings *ts = scene->toolsettings;

  if (ts->uv_flag & UV_SYNC_SELECTION) {
    if (ts->selectmode & SCE_SELECT_FACE) {
      BM_face_select_set(em->bm, l->f, true);
    }
    else if (ts->selectmode & SCE_SELECT_EDGE) {
      BM_edge_select_set(em->bm, l->e, true);
    }
    else {
      BM_vert_select_set(em->bm, l->e->v1, true);
      BM_vert_select_set(em->bm, l->e->v2, true);
    }

    if (do_history) {
      BM_select_history_store(em->bm, (BMElem *)l->e);
    }
  }
  else {
    MLoopUV *luv1, *luv2;

    luv1 = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
    luv2 = BM_ELEM_CD_GET_VOID_P(l->next, cd_loop_uv_offset);

    luv1->flag |= MLOOPUV_VERTSEL;
    luv2->flag |= MLOOPUV_VERTSEL;
  }
}

void uvedit_edge_select_disable(const Scene *scene,
                                BMEditMesh *em,
                                BMLoop *l,
                                const int cd_loop_uv_offset)

{
  const ToolSettings *ts = scene->toolsettings;

  if (ts->uv_flag & UV_SYNC_SELECTION) {
    if (ts->selectmode & SCE_SELECT_FACE) {
      BM_face_select_set(em->bm, l->f, false);
    }
    else if (ts->selectmode & SCE_SELECT_EDGE) {
      BM_edge_select_set(em->bm, l->e, false);
    }
    else {
      BM_vert_select_set(em->bm, l->e->v1, false);
      BM_vert_select_set(em->bm, l->e->v2, false);
    }
  }
  else {
    MLoopUV *luv1, *luv2;

    luv1 = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
    luv2 = BM_ELEM_CD_GET_VOID_P(l->next, cd_loop_uv_offset);

    luv1->flag &= ~MLOOPUV_VERTSEL;
    luv2->flag &= ~MLOOPUV_VERTSEL;
  }
}

bool uvedit_uv_select_test_ex(const ToolSettings *ts, BMLoop *l, const int cd_loop_uv_offset)
{
  if (ts->uv_flag & UV_SYNC_SELECTION) {
    if (ts->selectmode & SCE_SELECT_FACE) {
      return BM_elem_flag_test_bool(l->f, BM_ELEM_SELECT);
    }
    return BM_elem_flag_test_bool(l->v, BM_ELEM_SELECT);
  }

  MLoopUV *luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
  return (luv->flag & MLOOPUV_VERTSEL) != 0;
}
bool uvedit_uv_select_test(const Scene *scene, BMLoop *l, const int cd_loop_uv_offset)
{
  return uvedit_uv_select_test_ex(scene->toolsettings, l, cd_loop_uv_offset);
}

void uvedit_uv_select_set_with_sticky(const struct SpaceImage *sima,
                                      const Scene *scene,
                                      BMEditMesh *em,
                                      BMLoop *l,
                                      const bool select,
                                      const bool do_history,
                                      const uint cd_loop_uv_offset)
{
  const ToolSettings *ts = scene->toolsettings;
  if (ts->uv_flag & UV_SYNC_SELECTION) {
    uvedit_uv_select_set(scene, em, l, select, do_history, cd_loop_uv_offset);
    return;
  }

  const int sticky = sima->sticky;
  switch (sticky) {
    case SI_STICKY_DISABLE: {
      uvedit_uv_select_set(scene, em, l, select, do_history, cd_loop_uv_offset);
      break;
    }
    default: {
      /* #SI_STICKY_VERTEX or #SI_STICKY_LOC. */
      const MLoopUV *luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
      BMEdge *e_first, *e_iter;
      e_first = e_iter = l->e;
      do {
        if (e_iter->l) {
          BMLoop *l_radial_iter = e_iter->l;
          do {
            if (l_radial_iter->v == l->v) {
              if (uvedit_face_visible_test(scene, l_radial_iter->f)) {
                bool do_select = false;
                if (sticky == SI_STICKY_VERTEX) {
                  do_select = true;
                }
                else {
                  const MLoopUV *luv_other = BM_ELEM_CD_GET_VOID_P(l_radial_iter,
                                                                   cd_loop_uv_offset);
                  if (equals_v2v2(luv_other->uv, luv->uv)) {
                    do_select = true;
                  }
                }

                if (do_select) {
                  uvedit_uv_select_set(
                      scene, em, l_radial_iter, select, do_history, cd_loop_uv_offset);
                }
              }
            }
          } while ((l_radial_iter = l_radial_iter->radial_next) != e_iter->l);
        }
      } while ((e_iter = BM_DISK_EDGE_NEXT(e_iter, l->v)) != e_first);
    }
  }
}

void uvedit_uv_select_set(const Scene *scene,
                          BMEditMesh *em,
                          BMLoop *l,
                          const bool select,
                          const bool do_history,
                          const int cd_loop_uv_offset)
{
  if (select) {
    uvedit_uv_select_enable(scene, em, l, do_history, cd_loop_uv_offset);
  }
  else {
    uvedit_uv_select_disable(scene, em, l, cd_loop_uv_offset);
  }
}

void uvedit_uv_select_enable(const Scene *scene,
                             BMEditMesh *em,
                             BMLoop *l,
                             const bool do_history,
                             const int cd_loop_uv_offset)
{
  const ToolSettings *ts = scene->toolsettings;

  if (ts->uv_flag & UV_SYNC_SELECTION) {
    if (ts->selectmode & SCE_SELECT_FACE) {
      BM_face_select_set(em->bm, l->f, true);
    }
    else {
      BM_vert_select_set(em->bm, l->v, true);
    }

    if (do_history) {
      BM_select_history_store(em->bm, (BMElem *)l->v);
    }
  }
  else {
    MLoopUV *luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
    luv->flag |= MLOOPUV_VERTSEL;
  }
}

void uvedit_uv_select_disable(const Scene *scene,
                              BMEditMesh *em,
                              BMLoop *l,
                              const int cd_loop_uv_offset)
{
  const ToolSettings *ts = scene->toolsettings;

  if (ts->uv_flag & UV_SYNC_SELECTION) {
    if (ts->selectmode & SCE_SELECT_FACE) {
      BM_face_select_set(em->bm, l->f, false);
    }
    else {
      BM_vert_select_set(em->bm, l->v, false);
    }
  }
  else {
    MLoopUV *luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
    luv->flag &= ~MLOOPUV_VERTSEL;
  }
}

static BMLoop *uvedit_loop_find_other_radial_loop_with_visible_face(const Scene *scene,
                                                                    BMLoop *l_src,
                                                                    const int cd_loop_uv_offset)
{
  BMLoop *l_other = NULL;
  BMLoop *l_iter = l_src->radial_next;
  if (l_iter != l_src) {
    do {
      if (uvedit_face_visible_test(scene, l_iter->f) &&
          BM_loop_uv_share_edge_check(l_src, l_iter, cd_loop_uv_offset)) {
        /* Check UV's are contiguous. */
        if (l_other == NULL) {
          l_other = l_iter;
        }
        else {
          /* Only use when there is a single alternative. */
          l_other = NULL;
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
                                                                      const int cd_loop_uv_offset)
{
  BLI_assert(uvedit_loop_find_other_radial_loop_with_visible_face(
                 scene, l_edge, cd_loop_uv_offset) == NULL);

  BMLoop *l_step = l_edge;
  l_step = (l_step->v == v_pivot) ? l_step->prev : l_step->next;
  BMLoop *l_step_last = NULL;
  do {
    BLI_assert(BM_vert_in_edge(l_step->e, v_pivot));
    l_step_last = l_step;
    l_step = uvedit_loop_find_other_radial_loop_with_visible_face(
        scene, l_step, cd_loop_uv_offset);
    if (l_step) {
      l_step = (l_step->v == v_pivot) ? l_step->prev : l_step->next;
    }
  } while (l_step != NULL);

  BM_elem_flag_set(l_step_last->e, BM_ELEM_SMOOTH, false);

  if (l_step_last != NULL) {
    BLI_assert(uvedit_loop_find_other_radial_loop_with_visible_face(
                   scene, l_step_last, cd_loop_uv_offset) == NULL);
  }

  return l_step_last;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Find Nearest Elements
 * \{ */

bool uv_find_nearest_edge(Scene *scene, Object *obedit, const float co[2], UvNearestHit *hit)
{
  BLI_assert((hit->scale[0] > 0.0f) && (hit->scale[1] > 0.0f));
  BMEditMesh *em = BKE_editmesh_from_object(obedit);
  BMFace *efa;
  BMLoop *l;
  BMIter iter, liter;
  MLoopUV *luv, *luv_next;
  int i;
  bool found = false;

  const int cd_loop_uv_offset = CustomData_get_offset(&em->bm->ldata, CD_MLOOPUV);

  BM_mesh_elem_index_ensure(em->bm, BM_VERT);

  BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
    if (!uvedit_face_visible_test(scene, efa)) {
      continue;
    }
    BM_ITER_ELEM_INDEX (l, &liter, efa, BM_LOOPS_OF_FACE, i) {
      luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
      luv_next = BM_ELEM_CD_GET_VOID_P(l->next, cd_loop_uv_offset);

      float delta[2];
      closest_to_line_segment_v2(delta, co, luv->uv, luv_next->uv);

      sub_v2_v2(delta, co);
      mul_v2_v2(delta, hit->scale);

      const float dist_test_sq = len_squared_v2(delta);

      if (dist_test_sq < hit->dist_sq) {
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
                                UvNearestHit *hit_final)
{
  bool found = false;
  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    Object *obedit = objects[ob_index];
    if (uv_find_nearest_edge(scene, obedit, co, hit_final)) {
      hit_final->ob = obedit;
      found = true;
    }
  }
  return found;
}

bool uv_find_nearest_face(Scene *scene, Object *obedit, const float co[2], UvNearestHit *hit)
{
  BLI_assert((hit->scale[0] > 0.0f) && (hit->scale[1] > 0.0f));
  BMEditMesh *em = BKE_editmesh_from_object(obedit);
  bool found = false;

  const int cd_loop_uv_offset = CustomData_get_offset(&em->bm->ldata, CD_MLOOPUV);

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
      hit->efa = efa;
      hit->dist_sq = dist_test_sq;
      found = true;
    }
  }
  return found;
}

bool uv_find_nearest_face_multi(Scene *scene,
                                Object **objects,
                                const uint objects_len,
                                const float co[2],
                                UvNearestHit *hit_final)
{
  bool found = false;
  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    Object *obedit = objects[ob_index];
    if (uv_find_nearest_face(scene, obedit, co, hit_final)) {
      hit_final->ob = obedit;
      found = true;
    }
  }
  return found;
}

static bool uv_nearest_between(const BMLoop *l, const float co[2], const int cd_loop_uv_offset)
{
  const float *uv_prev = ((MLoopUV *)BM_ELEM_CD_GET_VOID_P(l->prev, cd_loop_uv_offset))->uv;
  const float *uv_curr = ((MLoopUV *)BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset))->uv;
  const float *uv_next = ((MLoopUV *)BM_ELEM_CD_GET_VOID_P(l->next, cd_loop_uv_offset))->uv;

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

  const int cd_loop_uv_offset = CustomData_get_offset(&em->bm->ldata, CD_MLOOPUV);

  BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
    if (!uvedit_face_visible_test(scene, efa)) {
      continue;
    }

    BMIter liter;
    BMLoop *l;
    int i;
    BM_ITER_ELEM_INDEX (l, &liter, efa, BM_LOOPS_OF_FACE, i) {
      MLoopUV *luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);

      float delta[2];

      sub_v2_v2v2(delta, co, luv->uv);
      mul_v2_v2(delta, hit->scale);

      float dist_test_sq = len_squared_v2(delta);

      if ((penalty_dist != 0.0f) && uvedit_uv_select_test(scene, l, cd_loop_uv_offset)) {
        dist_test_sq = square_f(sqrtf(dist_test_sq) + penalty_dist);
      }

      if (dist_test_sq <= hit->dist_sq) {
        if (dist_test_sq == hit->dist_sq) {
          if (!uv_nearest_between(l, co, cd_loop_uv_offset)) {
            continue;
          }
        }

        hit->dist_sq = dist_test_sq;

        hit->l = l;
        hit->efa = efa;
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
                                UvNearestHit *hit_final)
{
  bool found = false;
  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    Object *obedit = objects[ob_index];
    if (uv_find_nearest_vert(scene, obedit, co, penalty_dist, hit_final)) {
      hit_final->ob = obedit;
      found = true;
    }
  }
  return found;
}

bool ED_uvedit_nearest_uv(
    const Scene *scene, Object *obedit, const float co[2], float *dist_sq, float r_uv[2])
{
  BMEditMesh *em = BKE_editmesh_from_object(obedit);
  BMIter iter;
  BMFace *efa;
  const float *uv_best = NULL;
  float dist_best = *dist_sq;
  const int cd_loop_uv_offset = CustomData_get_offset(&em->bm->ldata, CD_MLOOPUV);
  BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
    if (!uvedit_face_visible_test(scene, efa)) {
      continue;
    }
    BMLoop *l_iter, *l_first;
    l_iter = l_first = BM_FACE_FIRST_LOOP(efa);
    do {
      const float *uv = ((const MLoopUV *)BM_ELEM_CD_GET_VOID_P(l_iter, cd_loop_uv_offset))->uv;
      const float dist_test = len_squared_v2v2(co, uv);
      if (dist_best > dist_test) {
        dist_best = dist_test;
        uv_best = uv;
      }
    } while ((l_iter = l_iter->next) != l_first);
  }

  if (uv_best != NULL) {
    copy_v2_v2(r_uv, uv_best);
    *dist_sq = dist_best;
    return true;
  }
  return false;
}

bool ED_uvedit_nearest_uv_multi(const Scene *scene,
                                Object **objects,
                                const uint objects_len,
                                const float co[2],
                                float *dist_sq,
                                float r_uv[2])
{
  bool found = false;
  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    Object *obedit = objects[ob_index];
    if (ED_uvedit_nearest_uv(scene, obedit, co, dist_sq, r_uv)) {
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
 * have multiple UV's split out.
 * \{ */

BMLoop *uv_find_nearest_loop_from_vert(struct Scene *scene,
                                       struct Object *obedit,
                                       struct BMVert *v,
                                       const float co[2])
{
  BMEditMesh *em = BKE_editmesh_from_object(obedit);
  const uint cd_loop_uv_offset = CustomData_get_offset(&em->bm->ldata, CD_MLOOPUV);

  BMIter liter;
  BMLoop *l;
  BMLoop *l_found = NULL;
  float dist_best_sq = FLT_MAX;

  BM_ITER_ELEM (l, &liter, v, BM_LOOPS_OF_VERT) {
    if (!uvedit_face_visible_test(scene, l->f)) {
      continue;
    }

    const MLoopUV *luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
    const float dist_test_sq = len_squared_v2v2(co, luv->uv);
    if (dist_test_sq < dist_best_sq) {
      dist_best_sq = dist_test_sq;
      l_found = l;
    }
  }
  return l_found;
}

BMLoop *uv_find_nearest_loop_from_edge(struct Scene *scene,
                                       struct Object *obedit,
                                       struct BMEdge *e,
                                       const float co[2])
{
  BMEditMesh *em = BKE_editmesh_from_object(obedit);
  const uint cd_loop_uv_offset = CustomData_get_offset(&em->bm->ldata, CD_MLOOPUV);

  BMIter eiter;
  BMLoop *l;
  BMLoop *l_found = NULL;
  float dist_best_sq = FLT_MAX;

  BM_ITER_ELEM (l, &eiter, e, BM_LOOPS_OF_EDGE) {
    if (!uvedit_face_visible_test(scene, l->f)) {
      continue;
    }
    const MLoopUV *luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
    const MLoopUV *luv_next = BM_ELEM_CD_GET_VOID_P(l->next, cd_loop_uv_offset);
    const float dist_test_sq = dist_squared_to_line_segment_v2(co, luv->uv, luv_next->uv);
    if (dist_test_sq < dist_best_sq) {
      dist_best_sq = dist_test_sq;
      l_found = l;
    }
  }
  return l_found;
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
                                                   const int cd_loop_uv_offset)
{
  if (l_step->f->len == 4) {
    BMVert *v_from_next = BM_edge_other_vert(l_step->e, v_from);
    BMLoop *l_step_over = (v_from == l_step->v) ? l_step->next : l_step->prev;
    l_step_over = uvedit_loop_find_other_radial_loop_with_visible_face(
        scene, l_step_over, cd_loop_uv_offset);
    if (l_step_over) {
      return (l_step_over->v == v_from_next) ? l_step_over->prev : l_step_over->next;
    }
  }
  return NULL;
}

static BMLoop *bm_select_edgeloop_single_side_next(const Scene *scene,
                                                   BMLoop *l_step,
                                                   BMVert *v_from,
                                                   const int cd_loop_uv_offset)
{
  BMVert *v_from_next = BM_edge_other_vert(l_step->e, v_from);
  return uvedit_loop_find_other_boundary_loop_with_visible_face(
      scene, l_step, v_from_next, cd_loop_uv_offset);
}

/* TODO(campbell): support this in the BMesh API, as we have for clearing other types. */
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
                                               const int cd_loop_uv_offset)
{
  bm_loop_tags_clear(em->bm);

  for (int side = 0; side < 2; side++) {
    BMLoop *l_step_pair[2] = {l_init_pair[0], l_init_pair[1]};
    BMVert *v_from = side ? l_step_pair[0]->e->v1 : l_step_pair[0]->e->v2;
    /* Disable since we start from the same edge. */
    BM_elem_flag_disable(l_step_pair[0], BM_ELEM_TAG);
    BM_elem_flag_disable(l_step_pair[1], BM_ELEM_TAG);
    while ((l_step_pair[0] != NULL) && (l_step_pair[1] != NULL)) {
      if (!uvedit_face_visible_test(scene, l_step_pair[0]->f) ||
          !uvedit_face_visible_test(scene, l_step_pair[1]->f) ||
          /* Check loops have not diverged. */
          (uvedit_loop_find_other_radial_loop_with_visible_face(
               scene, l_step_pair[0], cd_loop_uv_offset) != l_step_pair[1])) {
        break;
      }

      BLI_assert(l_step_pair[0]->e == l_step_pair[1]->e);

      BM_elem_flag_enable(l_step_pair[0], BM_ELEM_TAG);
      BM_elem_flag_enable(l_step_pair[1], BM_ELEM_TAG);

      BMVert *v_from_next = BM_edge_other_vert(l_step_pair[0]->e, v_from);
      /* Walk over both sides, ensure they keep on the same edge. */
      for (int i = 0; i < ARRAY_SIZE(l_step_pair); i++) {
        l_step_pair[i] = bm_select_edgeloop_double_side_next(
            scene, l_step_pair[i], v_from, cd_loop_uv_offset);
      }

      if ((l_step_pair[0] && BM_elem_flag_test(l_step_pair[0], BM_ELEM_TAG)) ||
          (l_step_pair[1] && BM_elem_flag_test(l_step_pair[1], BM_ELEM_TAG))) {
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
                                               const int cd_loop_uv_offset,
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
    while (l_step != NULL) {

      if (!uvedit_face_visible_test(scene, l_step->f) ||
          /* Check the boundary is still a  boundary. */
          (uvedit_loop_find_other_radial_loop_with_visible_face(
               scene, l_step, cd_loop_uv_offset) != NULL)) {
        break;
      }

      if (r_count_by_select != NULL) {
        r_count_by_select[uvedit_edge_select_test(scene, l_step, cd_loop_uv_offset)] += 1;
        /* Early exit when mixed could be optional if needed. */
        if (r_count_by_select[0] && r_count_by_select[1]) {
          r_count_by_select[0] = r_count_by_select[1] = -1;
          break;
        }
      }

      BM_elem_flag_enable(l_step, BM_ELEM_TAG);

      BMVert *v_from_next = BM_edge_other_vert(l_step->e, v_from);
      BMFace *f_step_prev = l_step->f;

      l_step = bm_select_edgeloop_single_side_next(scene, l_step, v_from, cd_loop_uv_offset);

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

static int uv_select_edgeloop(
    SpaceImage *sima, Scene *scene, Object *obedit, UvNearestHit *hit, const bool extend)
{
  BMEditMesh *em = BKE_editmesh_from_object(obedit);
  bool select;

  const int cd_loop_uv_offset = CustomData_get_offset(&em->bm->ldata, CD_MLOOPUV);

  if (extend) {
    select = !(uvedit_uv_select_test(scene, hit->l, cd_loop_uv_offset));
  }
  else {
    select = true;
  }

  BMLoop *l_init_pair[2] = {
      hit->l,
      uvedit_loop_find_other_radial_loop_with_visible_face(scene, hit->l, cd_loop_uv_offset),
  };

  /* When selecting boundaries, support cycling between selection modes. */
  enum eUVEdgeLoopBoundaryMode boundary_mode = UV_EDGE_LOOP_BOUNDARY_LOOP;

  /* Tag all loops that are part of the edge loop (select after).
   * This is done so we can */
  if (l_init_pair[1] == NULL) {
    int count_by_select[2];
    /* If the loops selected toggle the boundaries. */
    uv_select_edgeloop_single_side_tag(
        scene, em, l_init_pair[0], cd_loop_uv_offset, boundary_mode, count_by_select);
    if (count_by_select[!select] == 0) {
      boundary_mode = UV_EDGE_LOOP_BOUNDARY_ALL;

      /* If the boundary is selected, toggle back to the loop. */
      uv_select_edgeloop_single_side_tag(
          scene, em, l_init_pair[0], cd_loop_uv_offset, boundary_mode, count_by_select);
      if (count_by_select[!select] == 0) {
        boundary_mode = UV_EDGE_LOOP_BOUNDARY_LOOP;
      }
    }
  }

  if (l_init_pair[1] == NULL) {
    uv_select_edgeloop_single_side_tag(
        scene, em, l_init_pair[0], cd_loop_uv_offset, boundary_mode, NULL);
  }
  else {
    uv_select_edgeloop_double_side_tag(scene, em, l_init_pair, cd_loop_uv_offset);
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
          uvedit_edge_select_set_with_sticky(
              sima, scene, em, l_iter, select, false, cd_loop_uv_offset);
        }
      }
    }
  }

  return (select) ? 1 : -1;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Edge Ring Select
 * \{ */

static int uv_select_edgering(
    const SpaceImage *sima, Scene *scene, Object *obedit, UvNearestHit *hit, const bool extend)
{
  const ToolSettings *ts = scene->toolsettings;
  BMEditMesh *em = BKE_editmesh_from_object(obedit);
  const bool use_face_select = (ts->uv_flag & UV_SYNC_SELECTION) ?
                                   (ts->selectmode & SCE_SELECT_FACE) :
                                   (ts->uv_selectmode & UV_SELECT_FACE);
  bool select;

  const int cd_loop_uv_offset = CustomData_get_offset(&em->bm->ldata, CD_MLOOPUV);

  if (!extend) {
    uv_select_all_perform(scene, obedit, SEL_DESELECT);
  }

  BM_mesh_elem_hflag_disable_all(em->bm, BM_EDGE, BM_ELEM_TAG, false);

  if (extend) {
    select = !(uvedit_uv_select_test(scene, hit->l, cd_loop_uv_offset));
  }
  else {
    select = true;
  }

  BMLoop *l_pair[2] = {
      hit->l,
      uvedit_loop_find_other_radial_loop_with_visible_face(scene, hit->l, cd_loop_uv_offset),
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
        uvedit_face_select_set_with_sticky(
            sima, scene, em, l_step->f, select, false, cd_loop_uv_offset);
      }
      else {
        uvedit_edge_select_set_with_sticky(
            sima, scene, em, l_step, select, false, cd_loop_uv_offset);
      }

      BM_elem_flag_enable(l_step->e, BM_ELEM_TAG);
      if (l_step->f->len == 4) {
        BMLoop *l_step_opposite = l_step->next->next;
        l_step = uvedit_loop_find_other_radial_loop_with_visible_face(
            scene, l_step_opposite, cd_loop_uv_offset);
        if (l_step == NULL) {
          /* Ensure we touch the opposite edge if we cant walk over it. */
          l_step = l_step_opposite;
        }
      }
      else {
        l_step = NULL;
      }

      if (l_step && BM_elem_flag_test(l_step->e, BM_ELEM_TAG)) {
        break;
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
                                   UvNearestHit *hit_final,
                                   const bool extend,
                                   bool deselect,
                                   const bool toggle,
                                   const bool select_faces)
{
  const bool uv_sync_select = (scene->toolsettings->uv_flag & UV_SYNC_SELECTION);

  /* loop over objects, or just use hit_final->ob */
  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    if (hit_final && ob_index != 0) {
      break;
    }
    Object *obedit = hit_final ? hit_final->ob : objects[ob_index];

    BMFace *efa;
    BMLoop *l;
    BMIter iter, liter;
    UvVertMap *vmap;
    UvMapVert *vlist, *iterv, *startv;
    int i, stacksize = 0, *stack;
    uint a;
    char *flag;

    BMEditMesh *em = BKE_editmesh_from_object(obedit);
    const int cd_loop_uv_offset = CustomData_get_offset(&em->bm->ldata, CD_MLOOPUV);

    BM_mesh_elem_table_ensure(em->bm, BM_FACE); /* we can use this too */

    /* Note, we had 'use winding' so we don't consider overlapping islands as connected, see T44320
     * this made *every* projection split the island into front/back islands.
     * Keep 'use_winding' to false, see: T50970.
     *
     * Better solve this by having a delimit option for select-linked operator,
     * keeping island-select working as is. */
    vmap = BM_uv_vert_map_create(em->bm, !uv_sync_select, false);

    if (vmap == NULL) {
      continue;
    }

    stack = MEM_mallocN(sizeof(*stack) * (em->bm->totface + 1), "UvLinkStack");
    flag = MEM_callocN(sizeof(*flag) * em->bm->totface, "UvLinkFlag");

    if (hit_final == NULL) {
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
              if (uvedit_uv_select_test(scene, l, cd_loop_uv_offset)) {
                bool add_to_stack = true;
                if (uv_sync_select && !select_faces) {
                  /* Special case, vertex/edge & sync select being enabled.
                   *
                   * Without this, a second linked select will 'grow' each time as each new
                   * selection reaches the boundaries of islands that share vertices but not UV's.
                   *
                   * Rules applied here:
                   * - This loops face isn't selected.
                   * - The only other fully selected face is connected or,
                   * - There are no connected fully selected faces UV-connected to this loop.
                   */
                  if (uvedit_face_select_test(scene, l->f, cd_loop_uv_offset)) {
                    /* pass */
                  }
                  else {
                    BMIter liter_other;
                    BMLoop *l_other;
                    BM_ITER_ELEM (l_other, &liter_other, l->v, BM_LOOPS_OF_VERT) {
                      if ((l != l_other) &&
                          !BM_loop_uv_share_vert_check(l, l_other, cd_loop_uv_offset) &&
                          uvedit_face_select_test(scene, l_other->f, cd_loop_uv_offset)) {
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
        if (efa == hit_final->efa) {
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
          if (iterv->poly_index == a) {
            break;
          }
        }

        for (iterv = startv; iterv; iterv = iterv->next) {
          if ((startv != iterv) && (iterv->separate)) {
            break;
          }
          if (!flag[iterv->poly_index]) {
            flag[iterv->poly_index] = 1;
            stack[stacksize] = iterv->poly_index;
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
            if (uvedit_uv_select_test(scene, l, cd_loop_uv_offset)) {
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
    uvedit_face_select_set(scene, em, efa, value, false, cd_loop_uv_offset); \
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

/**
 * \warning This returns first selected UV,
 * not ideal in many cases since there could be multiple.
 */
const float *uvedit_first_selected_uv_from_vertex(Scene *scene,
                                                  BMVert *eve,
                                                  const int cd_loop_uv_offset)
{
  BMIter liter;
  BMLoop *l;

  BM_ITER_ELEM (l, &liter, eve, BM_LOOPS_OF_VERT) {
    if (!uvedit_face_visible_test(scene, l->f)) {
      continue;
    }

    if (uvedit_uv_select_test(scene, l, cd_loop_uv_offset)) {
      MLoopUV *luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
      return luv->uv;
    }
  }

  return NULL;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Select More/Less Operator
 * \{ */

static int uv_select_more_less(bContext *C, const bool select)
{
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  SpaceImage *sima = CTX_wm_space_image(C);

  BMFace *efa;
  BMLoop *l;
  BMIter iter, liter;
  const ToolSettings *ts = scene->toolsettings;

  uint objects_len = 0;
  Object **objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data_with_uvs(
      view_layer, ((View3D *)NULL), &objects_len);

  const bool is_uv_face_selectmode = (ts->uv_selectmode == UV_SELECT_FACE);

  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    Object *obedit = objects[ob_index];
    BMEditMesh *em = BKE_editmesh_from_object(obedit);

    bool changed = false;

    const int cd_loop_uv_offset = CustomData_get_offset(&em->bm->ldata, CD_MLOOPUV);

    if (ts->uv_flag & UV_SYNC_SELECTION) {
      if (select) {
        EDBM_select_more(em, true);
      }
      else {
        EDBM_select_less(em, true);
      }

      DEG_id_tag_update(obedit->data, ID_RECALC_SELECT);
      WM_event_add_notifier(C, NC_GEOM | ND_SELECT, obedit->data);
      continue;
    }

    if (is_uv_face_selectmode) {

      /* clear tags */
      BM_mesh_elem_hflag_disable_all(em->bm, BM_FACE, BM_ELEM_TAG, false);

      /* mark loops to be selected */
      BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
        if (uvedit_face_visible_test(scene, efa)) {

#define IS_SEL 1
#define IS_UNSEL 2

          int sel_state = 0;

          BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
            MLoopUV *luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
            if (luv->flag & MLOOPUV_VERTSEL) {
              sel_state |= IS_SEL;
            }
            else {
              sel_state |= IS_UNSEL;
            }

            /* if we have a mixed selection, tag to grow it */
            if (sel_state == (IS_SEL | IS_UNSEL)) {
              BM_elem_flag_enable(efa, BM_ELEM_TAG);
              changed = true;
              break;
            }
          }

#undef IS_SEL
#undef IS_UNSEL
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

            MLoopUV *luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);

            if (((luv->flag & MLOOPUV_VERTSEL) != 0) == select) {
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
        uv_select_flush_from_tag_face(sima, scene, obedit, select);
      }
      else {
        /* Select tagged loops. */
        uv_select_flush_from_tag_loop(sima, scene, obedit, select);
      }
      DEG_id_tag_update(obedit->data, ID_RECALC_SELECT);
      WM_event_add_notifier(C, NC_GEOM | ND_SELECT, obedit->data);
    }
  }
  MEM_freeN(objects);

  return OPERATOR_FINISHED;
}

static int uv_select_more_exec(bContext *C, wmOperator *UNUSED(op))
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

static int uv_select_less_exec(bContext *C, wmOperator *UNUSED(op))
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

bool uvedit_select_is_any_selected(Scene *scene, Object *obedit)
{
  const ToolSettings *ts = scene->toolsettings;
  BMEditMesh *em = BKE_editmesh_from_object(obedit);
  BMFace *efa;
  BMLoop *l;
  BMIter iter, liter;
  MLoopUV *luv;

  if (ts->uv_flag & UV_SYNC_SELECTION) {
    return (em->bm->totvertsel || em->bm->totedgesel || em->bm->totfacesel);
  }

  const int cd_loop_uv_offset = CustomData_get_offset(&em->bm->ldata, CD_MLOOPUV);
  BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
    if (!uvedit_face_visible_test(scene, efa)) {
      continue;
    }
    BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
      luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
      if (luv->flag & MLOOPUV_VERTSEL) {
        return true;
      }
    }
  }
  return false;
}

bool uvedit_select_is_any_selected_multi(Scene *scene, Object **objects, const uint objects_len)
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

static void uv_select_all_perform(Scene *scene, Object *obedit, int action)
{
  const ToolSettings *ts = scene->toolsettings;
  BMEditMesh *em = BKE_editmesh_from_object(obedit);
  BMFace *efa;
  BMLoop *l;
  BMIter iter, liter;
  MLoopUV *luv;

  const int cd_loop_uv_offset = CustomData_get_offset(&em->bm->ldata, CD_MLOOPUV);

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
    BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
      if (!uvedit_face_visible_test(scene, efa)) {
        continue;
      }

      BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
        luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);

        switch (action) {
          case SEL_SELECT:
            luv->flag |= MLOOPUV_VERTSEL;
            break;
          case SEL_DESELECT:
            luv->flag &= ~MLOOPUV_VERTSEL;
            break;
          case SEL_INVERT:
            luv->flag ^= MLOOPUV_VERTSEL;
            break;
        }
      }
    }
  }
}

static void uv_select_all_perform_multi_ex(
    Scene *scene, Object **objects, const uint objects_len, int action, const Object *ob_exclude)
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

static void uv_select_all_perform_multi(Scene *scene,
                                        Object **objects,
                                        const uint objects_len,
                                        int action)
{
  uv_select_all_perform_multi_ex(scene, objects, objects_len, action, NULL);
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
      view_layer, ((View3D *)NULL), &objects_len);

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

static int uv_mouse_select_multi(bContext *C,
                                 Object **objects,
                                 uint objects_len,
                                 const float co[2],
                                 const bool extend,
                                 const bool deselect_all)
{
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  SpaceImage *sima = CTX_wm_space_image(C);
  const ARegion *region = CTX_wm_region(C);
  Scene *scene = CTX_data_scene(C);
  const ToolSettings *ts = scene->toolsettings;
  UvNearestHit hit = UV_NEAREST_HIT_INIT_DIST_PX(&region->v2d, 75.0f);
  int selectmode, sticky;
  bool found_item = false;
  /* 0 == don't flush, 1 == sel, -1 == desel;  only use when selection sync is enabled */
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
    sticky = (sima) ? sima->sticky : SI_STICKY_DISABLE;
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
    found_item = uv_find_nearest_edge_multi(scene, objects, objects_len, co, &hit);
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
    if (found_item) {
      BMesh *bm = BKE_editmesh_from_object(hit.ob)->bm;
      BM_mesh_active_face_set(bm, hit.efa);
    }
  }
  else if (selectmode == UV_SELECT_ISLAND) {
    found_item = uv_find_nearest_edge_multi(scene, objects, objects_len, co, &hit);
  }

  if (!found_item) {
    if (deselect_all) {
      uv_select_all_perform_multi(scene, objects, objects_len, SEL_DESELECT);

      for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
        Object *obedit = objects[ob_index];
        uv_select_tag_update_for_object(depsgraph, ts, obedit);
      }

      return OPERATOR_PASS_THROUGH | OPERATOR_FINISHED;
    }
    return OPERATOR_CANCELLED;
  }

  Object *obedit = hit.ob;
  BMEditMesh *em = BKE_editmesh_from_object(obedit);
  const int cd_loop_uv_offset = CustomData_get_offset(&em->bm->ldata, CD_MLOOPUV);

  /* do selection */
  if (selectmode == UV_SELECT_ISLAND) {
    if (!extend) {
      uv_select_all_perform_multi_ex(scene, objects, objects_len, SEL_DESELECT, obedit);
    }
    /* Current behavior of 'extend'
     * is actually toggling, so pass extend flag as 'toggle' here */
    uv_select_linked_multi(scene, objects, objects_len, &hit, false, false, extend, false);
  }
  else if (extend) {
    bool select = true;
    if (selectmode == UV_SELECT_VERTEX) {
      /* (de)select uv vertex */
      select = !uvedit_uv_select_test(scene, hit.l, cd_loop_uv_offset);
      uvedit_uv_select_set_with_sticky(sima, scene, em, hit.l, select, true, cd_loop_uv_offset);
      flush = 1;
    }
    else if (selectmode == UV_SELECT_EDGE) {
      /* (de)select edge */
      select = !(uvedit_edge_select_test(scene, hit.l, cd_loop_uv_offset));
      uvedit_edge_select_set_with_sticky(sima, scene, em, hit.l, select, true, cd_loop_uv_offset);
      flush = 1;
    }
    else if (selectmode == UV_SELECT_FACE) {
      /* (de)select face */
      select = !(uvedit_face_select_test(scene, hit.efa, cd_loop_uv_offset));
      uvedit_face_select_set_with_sticky(
          sima, scene, em, hit.efa, select, true, cd_loop_uv_offset);
      flush = -1;
    }

    /* de-selecting an edge may deselect a face too - validate */
    if (ts->uv_flag & UV_SYNC_SELECTION) {
      if (select == false) {
        BM_select_history_validate(em->bm);
      }
    }

    /* (de)select sticky uv nodes */
    if (sticky != SI_STICKY_DISABLE) {
      flush = select ? 1 : -1;
    }
  }
  else {
    const bool select = true;
    /* deselect all */
    uv_select_all_perform_multi(scene, objects, objects_len, SEL_DESELECT);

    if (selectmode == UV_SELECT_VERTEX) {
      /* select vertex */
      uvedit_uv_select_set_with_sticky(sima, scene, em, hit.l, select, true, cd_loop_uv_offset);
      flush = 1;
    }
    else if (selectmode == UV_SELECT_EDGE) {
      /* select edge */
      uvedit_edge_select_set_with_sticky(sima, scene, em, hit.l, select, true, cd_loop_uv_offset);
      flush = 1;
    }
    else if (selectmode == UV_SELECT_FACE) {
      /* select face */
      uvedit_face_select_set_with_sticky(
          sima, scene, em, hit.efa, select, true, cd_loop_uv_offset);
      flush = 1;
    }
  }

  if (ts->uv_flag & UV_SYNC_SELECTION) {
    if (flush != 0) {
      EDBM_selectmode_flush(em);
    }
  }

  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    Object *obiter = objects[ob_index];
    uv_select_tag_update_for_object(depsgraph, ts, obiter);
  }

  return OPERATOR_PASS_THROUGH | OPERATOR_FINISHED;
}
static int uv_mouse_select(bContext *C,
                           const float co[2],
                           const bool extend,
                           const bool deselect_all)
{
  ViewLayer *view_layer = CTX_data_view_layer(C);
  uint objects_len = 0;
  Object **objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data_with_uvs(
      view_layer, ((View3D *)NULL), &objects_len);
  int ret = uv_mouse_select_multi(C, objects, objects_len, co, extend, deselect_all);
  MEM_freeN(objects);
  return ret;
}

static int uv_select_exec(bContext *C, wmOperator *op)
{
  float co[2];

  RNA_float_get_array(op->ptr, "location", co);
  const bool extend = RNA_boolean_get(op->ptr, "extend");
  const bool deselect_all = RNA_boolean_get(op->ptr, "deselect_all");

  return uv_mouse_select(C, co, extend, deselect_all);
}

static int uv_select_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  const ARegion *region = CTX_wm_region(C);
  float co[2];

  UI_view2d_region_to_view(&region->v2d, event->mval[0], event->mval[1], &co[0], &co[1]);
  RNA_float_set_array(op->ptr, "location", co);

  return uv_select_exec(C, op);
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

  /* properties */
  PropertyRNA *prop;
  RNA_def_boolean(ot->srna,
                  "extend",
                  0,
                  "Extend",
                  "Extend selection rather than clearing the existing selection");
  prop = RNA_def_boolean(ot->srna,
                         "deselect_all",
                         false,
                         "Deselect On Nothing",
                         "Deselect all when nothing under the cursor");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);

  RNA_def_float_vector(
      ot->srna,
      "location",
      2,
      NULL,
      -FLT_MAX,
      FLT_MAX,
      "Location",
      "Mouse location in normalized coordinates, 0.0 to 1.0 is within the image bounds",
      -100.0f,
      100.0f);
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
  SpaceImage *sima = CTX_wm_space_image(C);
  const ARegion *region = CTX_wm_region(C);
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  Scene *scene = CTX_data_scene(C);
  const ToolSettings *ts = scene->toolsettings;
  UvNearestHit hit = UV_NEAREST_HIT_INIT_MAX(&region->v2d);
  bool found_item = false;
  /* 0 == don't flush, 1 == sel, -1 == desel;  only use when selection sync is enabled */
  int flush = 0;

  /* Find edge. */
  found_item = uv_find_nearest_edge_multi(scene, objects, objects_len, co, &hit);
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
    flush = uv_select_edgeloop(sima, scene, obedit, &hit, extend);
  }
  else if (loop_type == UV_RING_SELECT) {
    flush = uv_select_edgering(sima, scene, obedit, &hit, extend);
  }
  else {
    BLI_assert(0);
  }

  if (ts->uv_flag & UV_SYNC_SELECTION) {
    if (flush == 1) {
      EDBM_select_flush(em);
    }
    else if (flush == -1) {
      EDBM_deselect_flush(em);
    }
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
  ViewLayer *view_layer = CTX_data_view_layer(C);
  uint objects_len = 0;
  Object **objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data_with_uvs(
      view_layer, ((View3D *)NULL), &objects_len);
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

  Scene *scene = CTX_data_scene(C);
  enum eUVLoopGenericType type = UV_LOOP_SELECT;
  if (ED_uvedit_select_mode_get(scene) == UV_SELECT_FACE) {
    /* For now ring-select and face-loop is the same thing,
     * if we support real edge selection this will no longer be the case. */
    type = UV_RING_SELECT;
  }

  return uv_mouse_select_loop_generic(C, co, extend, type);
}

static int uv_select_loop_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  const ARegion *region = CTX_wm_region(C);
  float co[2];

  UI_view2d_region_to_view(&region->v2d, event->mval[0], event->mval[1], &co[0], &co[1]);
  RNA_float_set_array(op->ptr, "location", co);

  return uv_select_loop_exec(C, op);
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
  RNA_def_boolean(ot->srna,
                  "extend",
                  0,
                  "Extend",
                  "Extend selection rather than clearing the existing selection");
  RNA_def_float_vector(
      ot->srna,
      "location",
      2,
      NULL,
      -FLT_MAX,
      FLT_MAX,
      "Location",
      "Mouse location in normalized coordinates, 0.0 to 1.0 is within the image bounds",
      -100.0f,
      100.0f);
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

  return uv_select_edge_ring_exec(C, op);
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
  RNA_def_boolean(ot->srna,
                  "extend",
                  0,
                  "Extend",
                  "Extend selection rather than clearing the existing selection");
  RNA_def_float_vector(
      ot->srna,
      "location",
      2,
      NULL,
      -FLT_MAX,
      FLT_MAX,
      "Location",
      "Mouse location in normalized coordinates, 0.0 to 1.0 is within the image bounds",
      -100.0f,
      100.0f);
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

  UvNearestHit hit = UV_NEAREST_HIT_INIT_MAX(&region->v2d);

  if (pick) {
    extend = RNA_boolean_get(op->ptr, "extend");
    deselect = RNA_boolean_get(op->ptr, "deselect");
  }

  uint objects_len = 0;
  Object **objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data_with_uvs(
      view_layer, ((View3D *)NULL), &objects_len);

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

    if (!uv_find_nearest_edge_multi(scene, objects, objects_len, co, &hit)) {
      MEM_freeN(objects);
      return OPERATOR_CANCELLED;
    }
  }

  if (!extend && !deselect) {
    uv_select_all_perform_multi(scene, objects, objects_len, SEL_DESELECT);
  }

  uv_select_linked_multi(
      scene, objects, objects_len, pick ? &hit : NULL, extend, deselect, false, select_faces);

  /* weak!, but works */
  Object **objects_free = objects;
  if (pick) {
    objects = &hit.ob;
    objects_len = 1;
  }

  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    Object *obedit = objects[ob_index];
    DEG_id_tag_update(obedit->data, ID_RECALC_COPY_ON_WRITE | ID_RECALC_SELECT);
    WM_event_add_notifier(C, NC_GEOM | ND_SELECT, obedit->data);
  }

  MEM_SAFE_FREE(objects_free);

  return OPERATOR_FINISHED;
}

static int uv_select_linked_exec(bContext *C, wmOperator *op)
{
  return uv_select_linked_internal(C, op, NULL, false);
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
  return uv_select_linked_internal(C, op, NULL, true);
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
  RNA_def_boolean(ot->srna,
                  "extend",
                  0,
                  "Extend",
                  "Extend selection rather than clearing the existing selection");
  RNA_def_boolean(ot->srna,
                  "deselect",
                  0,
                  "Deselect",
                  "Deselect linked UV vertices rather than selecting them");
  RNA_def_float_vector(
      ot->srna,
      "location",
      2,
      NULL,
      -FLT_MAX,
      FLT_MAX,
      "Location",
      "Mouse location in normalized coordinates, 0.0 to 1.0 is within the image bounds",
      -100.0f,
      100.0f);
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
  MLoopUV *luv;

  if (ts->uv_flag & UV_SYNC_SELECTION) {
    BKE_report(op->reports, RPT_ERROR, "Cannot split selection when sync selection is enabled");
    return OPERATOR_CANCELLED;
  }

  bool changed_multi = false;

  uint objects_len = 0;
  Object **objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data_with_uvs(
      view_layer, ((View3D *)NULL), &objects_len);

  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    Object *obedit = objects[ob_index];
    BMesh *bm = BKE_editmesh_from_object(obedit)->bm;

    bool changed = false;

    const int cd_loop_uv_offset = CustomData_get_offset(&bm->ldata, CD_MLOOPUV);

    BM_ITER_MESH (efa, &iter, bm, BM_FACES_OF_MESH) {
      bool is_sel = false;
      bool is_unsel = false;

      if (!uvedit_face_visible_test(scene, efa)) {
        continue;
      }

      /* are we all selected? */
      BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
        luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);

        if (luv->flag & MLOOPUV_VERTSEL) {
          is_sel = true;
        }
        else {
          is_unsel = true;
        }

        /* we have mixed selection, bail out */
        if (is_sel && is_unsel) {
          break;
        }
      }

      if (is_sel && is_unsel) {
        BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
          luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
          luv->flag &= ~MLOOPUV_VERTSEL;
        }

        changed = true;
      }
    }

    if (changed) {
      changed_multi = true;
      WM_event_add_notifier(C, NC_SPACE | ND_SPACE_IMAGE, NULL);
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
    DEG_id_tag_update(obedit->data, ID_RECALC_SELECT);
    WM_main_add_notifier(NC_GEOM | ND_SELECT, obedit->data);
  }
  else {
    Object *obedit_eval = DEG_get_evaluated_object(depsgraph, obedit);
    BKE_mesh_batch_cache_dirty_tag(obedit_eval->data, BKE_MESH_BATCH_DIRTY_UVEDIT_SELECT);
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
static void uv_select_flush_from_tag_sticky_loc_internal(Scene *scene,
                                                         BMEditMesh *em,
                                                         UvVertMap *vmap,
                                                         const uint efa_index,
                                                         BMLoop *l,
                                                         const bool select,
                                                         const int cd_loop_uv_offset)
{
  UvMapVert *start_vlist = NULL, *vlist_iter;
  BMFace *efa_vlist;

  uvedit_uv_select_set(scene, em, l, select, false, cd_loop_uv_offset);

  vlist_iter = BM_uv_vert_map_at_index(vmap, BM_elem_index_get(l->v));

  while (vlist_iter) {
    if (vlist_iter->separate) {
      start_vlist = vlist_iter;
    }

    if (efa_index == vlist_iter->poly_index) {
      break;
    }

    vlist_iter = vlist_iter->next;
  }

  vlist_iter = start_vlist;
  while (vlist_iter) {

    if (vlist_iter != start_vlist && vlist_iter->separate) {
      break;
    }

    if (efa_index != vlist_iter->poly_index) {
      BMLoop *l_other;
      efa_vlist = BM_face_at_index(em->bm, vlist_iter->poly_index);
      /* tf_vlist = BM_ELEM_CD_GET_VOID_P(efa_vlist, cd_poly_tex_offset); */ /* UNUSED */

      l_other = BM_iter_at_index(
          em->bm, BM_LOOPS_OF_FACE, efa_vlist, vlist_iter->loop_of_poly_index);

      uvedit_uv_select_set(scene, em, l_other, select, false, cd_loop_uv_offset);
    }
    vlist_iter = vlist_iter->next;
  }
}

/**
 * Flush the selection from face tags based on sticky and selection modes.
 *
 * needed because settings the selection a face is done in a number of places but it also
 * needs to respect the sticky modes for the UV verts, so dealing with the sticky modes
 * is best done in a separate function.
 *
 * \note This function is very similar to #uv_select_flush_from_tag_loop,
 * be sure to update both upon changing.
 */
static void uv_select_flush_from_tag_face(SpaceImage *sima,
                                          Scene *scene,
                                          Object *obedit,
                                          const bool select)
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
  const int cd_loop_uv_offset = CustomData_get_offset(&em->bm->ldata, CD_MLOOPUV);

  if ((ts->uv_flag & UV_SYNC_SELECTION) == 0 && sima->sticky == SI_STICKY_VERTEX) {
    /* Tag all verts as untouched, then touch the ones that have a face center
     * in the loop and select all MLoopUV's that use a touched vert. */
    BM_mesh_elem_hflag_disable_all(em->bm, BM_VERT, BM_ELEM_TAG, false);

    BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
      if (BM_elem_flag_test(efa, BM_ELEM_TAG)) {
        BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
          BM_elem_flag_enable(l->v, BM_ELEM_TAG);
        }
      }
    }

    /* now select tagged verts */
    BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
      /* tf = BM_ELEM_CD_GET_VOID_P(efa, cd_poly_tex_offset); */ /* UNUSED */

      BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
        if (BM_elem_flag_test(l->v, BM_ELEM_TAG)) {
          uvedit_uv_select_set(scene, em, l, select, false, cd_loop_uv_offset);
        }
      }
    }
  }
  else if ((ts->uv_flag & UV_SYNC_SELECTION) == 0 && sima->sticky == SI_STICKY_LOC) {
    struct UvVertMap *vmap;
    uint efa_index;

    BM_mesh_elem_table_ensure(em->bm, BM_FACE);
    vmap = BM_uv_vert_map_create(em->bm, false, false);
    if (vmap == NULL) {
      return;
    }

    BM_ITER_MESH_INDEX (efa, &iter, em->bm, BM_FACES_OF_MESH, efa_index) {
      if (BM_elem_flag_test(efa, BM_ELEM_TAG)) {
        /* tf = BM_ELEM_CD_GET_VOID_P(efa, cd_poly_tex_offset); */ /* UNUSED */

        BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
          uv_select_flush_from_tag_sticky_loc_internal(
              scene, em, vmap, efa_index, l, select, cd_loop_uv_offset);
        }
      }
    }
    BM_uv_vert_map_free(vmap);
  }
  else { /* SI_STICKY_DISABLE or ts->uv_flag & UV_SYNC_SELECTION */
    BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
      if (BM_elem_flag_test(efa, BM_ELEM_TAG)) {
        uvedit_face_select_set(scene, em, efa, select, false, cd_loop_uv_offset);
      }
    }
  }
}

/**
 * Flush the selection from loop tags based on sticky and selection modes.
 *
 * needed because settings the selection a face is done in a number of places but it also needs
 * to respect the sticky modes for the UV verts, so dealing with the sticky modes is best done
 * in a separate function.
 *
 * \note This function is very similar to #uv_select_flush_from_tag_loop,
 * be sure to update both upon changing.
 */
static void uv_select_flush_from_tag_loop(SpaceImage *sima,
                                          Scene *scene,
                                          Object *obedit,
                                          const bool select)
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

  const int cd_loop_uv_offset = CustomData_get_offset(&em->bm->ldata, CD_MLOOPUV);

  if ((ts->uv_flag & UV_SYNC_SELECTION) == 0 && sima->sticky == SI_STICKY_VERTEX) {
    /* Tag all verts as untouched, then touch the ones that have a face center
     * in the loop and select all MLoopUV's that use a touched vert. */
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
      /* tf = BM_ELEM_CD_GET_VOID_P(efa, cd_poly_tex_offset); */ /* UNUSED */

      BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
        if (BM_elem_flag_test(l->v, BM_ELEM_TAG)) {
          uvedit_uv_select_set(scene, em, l, select, false, cd_loop_uv_offset);
        }
      }
    }
  }
  else if ((ts->uv_flag & UV_SYNC_SELECTION) == 0 && sima->sticky == SI_STICKY_LOC) {
    struct UvVertMap *vmap;
    uint efa_index;

    BM_mesh_elem_table_ensure(em->bm, BM_FACE);
    vmap = BM_uv_vert_map_create(em->bm, false, false);
    if (vmap == NULL) {
      return;
    }

    BM_ITER_MESH_INDEX (efa, &iter, em->bm, BM_FACES_OF_MESH, efa_index) {
      /* tf = BM_ELEM_CD_GET_VOID_P(efa, cd_poly_tex_offset); */ /* UNUSED */

      BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
        if (BM_elem_flag_test(l, BM_ELEM_TAG)) {
          uv_select_flush_from_tag_sticky_loc_internal(
              scene, em, vmap, efa_index, l, select, cd_loop_uv_offset);
        }
      }
    }
    BM_uv_vert_map_free(vmap);
  }
  else { /* SI_STICKY_DISABLE or ts->uv_flag & UV_SYNC_SELECTION */
    BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
      BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
        if (BM_elem_flag_test(l, BM_ELEM_TAG)) {
          uvedit_uv_select_set(scene, em, l, select, false, cd_loop_uv_offset);
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
  SpaceImage *sima = CTX_wm_space_image(C);
  Scene *scene = CTX_data_scene(C);
  const ToolSettings *ts = scene->toolsettings;
  ViewLayer *view_layer = CTX_data_view_layer(C);
  const ARegion *region = CTX_wm_region(C);
  BMFace *efa;
  BMLoop *l;
  BMIter iter, liter;
  MLoopUV *luv;
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

  const eSelectOp sel_op = RNA_enum_get(op->ptr, "mode");
  const bool select = (sel_op != SEL_OP_SUB);
  const bool use_pre_deselect = SEL_OP_USE_PRE_DESELECT(sel_op);

  pinned = RNA_boolean_get(op->ptr, "pinned");

  bool changed_multi = false;

  uint objects_len = 0;
  Object **objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data_with_uvs(
      view_layer, ((View3D *)NULL), &objects_len);

  if (use_pre_deselect) {
    uv_select_all_perform_multi(scene, objects, objects_len, SEL_DESELECT);
  }

  /* don't indent to avoid diff noise! */
  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    Object *obedit = objects[ob_index];
    BMEditMesh *em = BKE_editmesh_from_object(obedit);

    bool changed = false;

    const int cd_loop_uv_offset = CustomData_get_offset(&em->bm->ldata, CD_MLOOPUV);

    /* do actual selection */
    if (use_face_center && !pinned) {
      /* handle face selection mode */
      float cent[2];

      BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
        /* assume not touched */
        BM_elem_flag_disable(efa, BM_ELEM_TAG);

        if (uvedit_face_visible_test(scene, efa)) {
          BM_face_uv_calc_center_median(efa, cd_loop_uv_offset, cent);
          if (BLI_rctf_isect_pt_v(&rectf, cent)) {
            BM_elem_flag_enable(efa, BM_ELEM_TAG);
            changed = true;
          }
        }
      }

      /* (de)selects all tagged faces and deals with sticky modes */
      if (changed) {
        uv_select_flush_from_tag_face(sima, scene, obedit, select);
      }
    }
    else if (use_edge && !pinned) {
      BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
        if (!uvedit_face_visible_test(scene, efa)) {
          continue;
        }

        BMLoop *l_prev = BM_FACE_FIRST_LOOP(efa)->prev;
        MLoopUV *luv_prev = BM_ELEM_CD_GET_VOID_P(l_prev, cd_loop_uv_offset);

        BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
          luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
          if (BLI_rctf_isect_pt_v(&rectf, luv->uv) && BLI_rctf_isect_pt_v(&rectf, luv_prev->uv)) {
            uvedit_edge_select_set_with_sticky(
                sima, scene, em, l_prev, select, false, cd_loop_uv_offset);
            changed = true;
          }
          l_prev = l;
          luv_prev = luv;
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
          luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
          if ((select) != (uvedit_uv_select_test(scene, l, cd_loop_uv_offset))) {
            if (!pinned || (ts->uv_flag & UV_SYNC_SELECTION)) {
              /* UV_SYNC_SELECTION - can't do pinned selection */
              if (BLI_rctf_isect_pt_v(&rectf, luv->uv)) {
                uvedit_uv_select_set(scene, em, l, select, false, cd_loop_uv_offset);
                BM_elem_flag_enable(l->v, BM_ELEM_TAG);
                has_selected = true;
              }
            }
            else if (pinned) {
              if ((luv->flag & MLOOPUV_PINNED) && BLI_rctf_isect_pt_v(&rectf, luv->uv)) {
                uvedit_uv_select_set(scene, em, l, select, false, cd_loop_uv_offset);
                BM_elem_flag_enable(l->v, BM_ELEM_TAG);
              }
            }
          }
        }
        if (has_selected && use_select_linked) {
          UvNearestHit hit = {
              .ob = obedit,
              .efa = efa,
          };
          uv_select_linked_multi(scene, objects, objects_len, &hit, true, !select, false, false);
        }
      }

      if (sima->sticky == SI_STICKY_VERTEX) {
        uvedit_vertex_select_tagged(em, scene, select, cd_loop_uv_offset);
      }
    }

    if (changed || use_pre_deselect) {
      changed_multi = true;

      ED_uvedit_select_sync_flush(ts, em, select);
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
  RNA_def_boolean(ot->srna, "pinned", 0, "Pinned", "Border select pinned UVs only");

  WM_operator_properties_gesture_box(ot);
  WM_operator_properties_select_operation_simple(ot);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Circle Select Operator
 * \{ */

static int uv_circle_select_is_point_inside(const float uv[2],
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

static int uv_circle_select_is_edge_inside(const float uv_a[2],
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
  return dist_squared_to_line_segment_v2((const float[2]){0.0f, 0.0f}, co_a, co_b) < 1.0f;
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
  MLoopUV *luv;
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
      view_layer, ((View3D *)NULL), &objects_len);

  const eSelectOp sel_op = ED_select_op_modal(RNA_enum_get(op->ptr, "mode"),
                                              WM_gesture_is_modal_first(op->customdata));
  const bool select = (sel_op != SEL_OP_SUB);
  const bool use_pre_deselect = SEL_OP_USE_PRE_DESELECT(sel_op);

  if (use_pre_deselect) {
    uv_select_all_perform_multi(scene, objects, objects_len, SEL_DESELECT);
  }

  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    Object *obedit = objects[ob_index];
    BMEditMesh *em = BKE_editmesh_from_object(obedit);

    bool changed = false;

    const int cd_loop_uv_offset = CustomData_get_offset(&em->bm->ldata, CD_MLOOPUV);

    /* do selection */
    if (use_face_center) {
      BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
        BM_elem_flag_disable(efa, BM_ELEM_TAG);
        /* assume not touched */
        if (select != uvedit_face_select_test(scene, efa, cd_loop_uv_offset)) {
          float cent[2];
          BM_face_uv_calc_center_median(efa, cd_loop_uv_offset, cent);
          if (uv_circle_select_is_point_inside(cent, offset, ellipse)) {
            BM_elem_flag_enable(efa, BM_ELEM_TAG);
            changed = true;
          }
        }
      }

      /* (de)selects all tagged faces and deals with sticky modes */
      if (changed) {
        uv_select_flush_from_tag_face(sima, scene, obedit, select);
      }
    }
    else if (use_edge) {
      BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
        if (!uvedit_face_visible_test(scene, efa)) {
          continue;
        }

        BMLoop *l_prev = BM_FACE_FIRST_LOOP(efa)->prev;
        MLoopUV *luv_prev = BM_ELEM_CD_GET_VOID_P(l_prev, cd_loop_uv_offset);

        BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
          luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
          if (uv_circle_select_is_edge_inside(luv->uv, luv_prev->uv, offset, ellipse)) {
            uvedit_edge_select_set_with_sticky(
                sima, scene, em, l_prev, select, false, cd_loop_uv_offset);
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
          if ((select) != (uvedit_uv_select_test(scene, l, cd_loop_uv_offset))) {
            luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
            if (uv_circle_select_is_point_inside(luv->uv, offset, ellipse)) {
              changed = true;
              uvedit_uv_select_set(scene, em, l, select, false, cd_loop_uv_offset);
              BM_elem_flag_enable(l->v, BM_ELEM_TAG);
              has_selected = true;
            }
          }
        }
        if (has_selected && use_select_linked) {
          UvNearestHit hit = {
              .ob = obedit,
              .efa = efa,
          };
          uv_select_linked_multi(scene, objects, objects_len, &hit, true, !select, false, false);
        }
      }

      if (sima->sticky == SI_STICKY_VERTEX) {
        uvedit_vertex_select_tagged(em, scene, select, cd_loop_uv_offset);
      }
    }

    if (changed || use_pre_deselect) {
      changed_multi = true;

      ED_uvedit_select_sync_flush(ts, em, select);
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
      BLI_lasso_is_point_inside(
          mcoords, mcoords_len, co_screen[0], co_screen[1], V2D_IS_CLIPPED)) {
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
  SpaceImage *sima = CTX_wm_space_image(C);
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
      view_layer, ((View3D *)NULL), &objects_len);

  if (use_pre_deselect) {
    uv_select_all_perform_multi(scene, objects, objects_len, SEL_DESELECT);
  }

  /* don't indent to avoid diff noise! */
  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    Object *obedit = objects[ob_index];

    bool changed = false;

    BMEditMesh *em = BKE_editmesh_from_object(obedit);

    const int cd_loop_uv_offset = CustomData_get_offset(&em->bm->ldata, CD_MLOOPUV);

    if (use_face_center) { /* Face Center Sel */
      BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
        BM_elem_flag_disable(efa, BM_ELEM_TAG);
        /* assume not touched */
        if (select != uvedit_face_select_test(scene, efa, cd_loop_uv_offset)) {
          float cent[2];
          BM_face_uv_calc_center_median(efa, cd_loop_uv_offset, cent);
          if (do_lasso_select_mesh_uv_is_point_inside(region, &rect, mcoords, mcoords_len, cent)) {
            BM_elem_flag_enable(efa, BM_ELEM_TAG);
            changed = true;
          }
        }
      }

      /* (de)selects all tagged faces and deals with sticky modes */
      if (changed) {
        uv_select_flush_from_tag_face(sima, scene, obedit, select);
      }
    }
    else if (use_edge) {
      BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
        if (!uvedit_face_visible_test(scene, efa)) {
          continue;
        }

        BMLoop *l_prev = BM_FACE_FIRST_LOOP(efa)->prev;
        MLoopUV *luv_prev = BM_ELEM_CD_GET_VOID_P(l_prev, cd_loop_uv_offset);

        BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
          MLoopUV *luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
          if (do_lasso_select_mesh_uv_is_point_inside(
                  region, &rect, mcoords, mcoords_len, luv->uv) &&
              do_lasso_select_mesh_uv_is_point_inside(
                  region, &rect, mcoords, mcoords_len, luv_prev->uv)) {
            uvedit_edge_select_set_with_sticky(
                sima, scene, em, l_prev, select, false, cd_loop_uv_offset);
            changed = true;
          }
          l_prev = l;
          luv_prev = luv;
        }
      }
    }
    else { /* Vert Sel */
      BM_mesh_elem_hflag_disable_all(em->bm, BM_VERT, BM_ELEM_TAG, false);

      BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
        if (!uvedit_face_visible_test(scene, efa)) {
          continue;
        }
        bool has_selected = false;
        BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
          if ((select) != (uvedit_uv_select_test(scene, l, cd_loop_uv_offset))) {
            MLoopUV *luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
            if (do_lasso_select_mesh_uv_is_point_inside(
                    region, &rect, mcoords, mcoords_len, luv->uv)) {
              uvedit_uv_select_set(scene, em, l, select, false, cd_loop_uv_offset);
              changed = true;
              BM_elem_flag_enable(l->v, BM_ELEM_TAG);
              has_selected = true;
            }
          }
        }
        if (has_selected && use_select_linked) {
          UvNearestHit hit = {
              .ob = obedit,
              .efa = efa,
          };
          uv_select_linked_multi(scene, objects, objects_len, &hit, true, !select, false, false);
        }
      }

      if (sima->sticky == SI_STICKY_VERTEX) {
        uvedit_vertex_select_tagged(em, scene, select, cd_loop_uv_offset);
      }
    }

    if (changed || use_pre_deselect) {
      changed_multi = true;

      ED_uvedit_select_sync_flush(ts, em, select);
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
    const eSelectOp sel_op = RNA_enum_get(op->ptr, "mode");
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
  ot->flag = OPTYPE_UNDO;

  /* properties */
  WM_operator_properties_gesture_lasso(ot);
  WM_operator_properties_select_operation_simple(ot);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Select Pinned UV's Operator
 * \{ */

static int uv_select_pinned_exec(bContext *C, wmOperator *UNUSED(op))
{
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  Scene *scene = CTX_data_scene(C);
  const ToolSettings *ts = scene->toolsettings;
  ViewLayer *view_layer = CTX_data_view_layer(C);
  BMFace *efa;
  BMLoop *l;
  BMIter iter, liter;
  MLoopUV *luv;

  uint objects_len = 0;
  Object **objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data_with_uvs(
      view_layer, ((View3D *)NULL), &objects_len);

  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    Object *obedit = objects[ob_index];
    BMEditMesh *em = BKE_editmesh_from_object(obedit);

    const int cd_loop_uv_offset = CustomData_get_offset(&em->bm->ldata, CD_MLOOPUV);
    bool changed = false;

    BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
      if (!uvedit_face_visible_test(scene, efa)) {
        continue;
      }

      BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
        luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);

        if (luv->flag & MLOOPUV_PINNED) {
          uvedit_uv_select_enable(scene, em, l, false, cd_loop_uv_offset);
          changed = true;
        }
      }
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
  const BVHTreeOverlap *overlap = overlap_v;

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
  const BVHTreeOverlap *a = a_v;
  const BVHTreeOverlap *b = b_v;
  return !((a->indexA == b->indexA && a->indexB == b->indexB) ||
           (a->indexA == b->indexB && a->indexB == b->indexA));
}

struct UVOverlapData {
  int ob_index;
  int face_index;
  float tri[3][2];
};

static int uv_select_overlap(bContext *C, const bool extend)
{
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);

  uint objects_len = 0;
  Object **objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data_with_uvs(
      view_layer, ((View3D *)NULL), &objects_len);

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

  struct UVOverlapData *overlap_data = MEM_mallocN(sizeof(struct UVOverlapData) * uv_tri_len,
                                                   "UvOverlapData");
  BVHTree *uv_tree = BLI_bvhtree_new(uv_tri_len, 0.0f, 4, 6);

  /* Use a global data index when inserting into the BVH. */
  int data_index = 0;

  int face_len_alloc = 3;
  float(*uv_verts)[2] = MEM_mallocN(sizeof(*uv_verts) * face_len_alloc, "UvOverlapCoords");
  uint(*indices)[3] = MEM_mallocN(sizeof(*indices) * (face_len_alloc - 2), "UvOverlapTris");

  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    Object *obedit = objects[ob_index];
    BMEditMesh *em = BKE_editmesh_from_object(obedit);
    BMIter iter, liter;
    BMFace *efa;
    BMLoop *l;

    const int cd_loop_uv_offset = CustomData_get_offset(&em->bm->ldata, CD_MLOOPUV);

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
        uv_verts = MEM_mallocN(sizeof(*uv_verts) * face_len, "UvOverlapCoords");
        indices = MEM_mallocN(sizeof(*indices) * tri_len, "UvOverlapTris");
        face_len_alloc = face_len;
      }

      int vert_index;
      BM_ITER_ELEM_INDEX (l, &liter, efa, BM_LOOPS_OF_FACE, vert_index) {
        MLoopUV *luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
        copy_v2_v2(uv_verts[vert_index], luv->uv);
      }

      BLI_polyfill_calc(uv_verts, face_len, 0, indices);

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
    }
  }
  BLI_assert(data_index == uv_tri_len);

  MEM_freeN(uv_verts);
  MEM_freeN(indices);

  BLI_bvhtree_balance(uv_tree);

  uint tree_overlap_len;
  BVHTreeOverlap *overlap = BLI_bvhtree_overlap(uv_tree, uv_tree, &tree_overlap_len, NULL, NULL);

  if (overlap != NULL) {
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

      const struct UVOverlapData *o_a = &overlap_data[overlap[i].indexA];
      const struct UVOverlapData *o_b = &overlap_data[overlap[i].indexB];
      Object *obedit_a = objects[o_a->ob_index];
      Object *obedit_b = objects[o_b->ob_index];
      BMEditMesh *em_a = BKE_editmesh_from_object(obedit_a);
      BMEditMesh *em_b = BKE_editmesh_from_object(obedit_b);
      BMFace *face_a = em_a->bm->ftable[o_a->face_index];
      BMFace *face_b = em_b->bm->ftable[o_b->face_index];
      const int cd_loop_uv_offset_a = CustomData_get_offset(&em_a->bm->ldata, CD_MLOOPUV);
      const int cd_loop_uv_offset_b = CustomData_get_offset(&em_b->bm->ldata, CD_MLOOPUV);

      /* Skip if both faces are already selected. */
      if (uvedit_face_select_test(scene, face_a, cd_loop_uv_offset_a) &&
          uvedit_face_select_test(scene, face_b, cd_loop_uv_offset_b)) {
        continue;
      }

      /* Main tri-tri overlap test. */
      const float endpoint_bias = -1e-4f;
      const float(*t1)[2] = o_a->tri;
      const float(*t2)[2] = o_b->tri;
      float vi[2];
      bool result = (
          /* Don't use 'isect_tri_tri_v2' here
           * because it's important to ignore overlap at end-points. */
          isect_seg_seg_v2_point_ex(t1[0], t1[1], t2[0], t2[1], endpoint_bias, vi) == 1 ||
          isect_seg_seg_v2_point_ex(t1[0], t1[1], t2[1], t2[2], endpoint_bias, vi) == 1 ||
          isect_seg_seg_v2_point_ex(t1[0], t1[1], t2[2], t2[0], endpoint_bias, vi) == 1 ||
          isect_seg_seg_v2_point_ex(t1[1], t1[2], t2[0], t2[1], endpoint_bias, vi) == 1 ||
          isect_seg_seg_v2_point_ex(t1[1], t1[2], t2[1], t2[2], endpoint_bias, vi) == 1 ||
          isect_seg_seg_v2_point_ex(t1[1], t1[2], t2[2], t2[0], endpoint_bias, vi) == 1 ||
          isect_seg_seg_v2_point_ex(t1[2], t1[0], t2[0], t2[1], endpoint_bias, vi) == 1 ||
          isect_seg_seg_v2_point_ex(t1[2], t1[0], t2[1], t2[2], endpoint_bias, vi) == 1 ||
          isect_point_tri_v2(t1[0], t2[0], t2[1], t2[2]) != 0 ||
          isect_point_tri_v2(t2[0], t1[0], t1[1], t1[2]) != 0);

      if (result) {
        uvedit_face_select_enable(scene, em_a, face_a, false, cd_loop_uv_offset_a);
        uvedit_face_select_enable(scene, em_b, face_b, false, cd_loop_uv_offset_b);
      }
    }

    BLI_gset_free(overlap_set, NULL);
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
                  0,
                  "Extend",
                  "Extend selection rather than clearing the existing selection");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Selected Elements as Arrays (Vertex, Edge & Faces)
 *
 * These functions return single elements per connected vertex/edge.
 * So an edge that has two connected edge loops only assigns one loop in the array.
 * \{ */

BMFace **ED_uvedit_selected_faces(Scene *scene, BMesh *bm, int len_max, int *r_faces_len)
{
  const int cd_loop_uv_offset = CustomData_get_offset(&bm->ldata, CD_MLOOPUV);
  CLAMP_MAX(len_max, bm->totface);
  int faces_len = 0;
  BMFace **faces = MEM_mallocN(sizeof(*faces) * len_max, __func__);

  BMIter iter;
  BMFace *f;
  BM_ITER_MESH (f, &iter, bm, BM_FACES_OF_MESH) {
    if (uvedit_face_visible_test(scene, f)) {
      if (uvedit_face_select_test(scene, f, cd_loop_uv_offset)) {
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
    faces = MEM_reallocN(faces, sizeof(*faces) * faces_len);
  }
  return faces;
}

BMLoop **ED_uvedit_selected_edges(Scene *scene, BMesh *bm, int len_max, int *r_edges_len)
{
  const int cd_loop_uv_offset = CustomData_get_offset(&bm->ldata, CD_MLOOPUV);
  CLAMP_MAX(len_max, bm->totloop);
  int edges_len = 0;
  BMLoop **edges = MEM_mallocN(sizeof(*edges) * len_max, __func__);

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
          const MLoopUV *luv_curr = BM_ELEM_CD_GET_VOID_P(l_iter, cd_loop_uv_offset);
          const MLoopUV *luv_next = BM_ELEM_CD_GET_VOID_P(l_iter->next, cd_loop_uv_offset);
          if ((luv_curr->flag & MLOOPUV_VERTSEL) && (luv_next->flag & MLOOPUV_VERTSEL)) {
            BM_elem_flag_enable(l_iter, BM_ELEM_TAG);

            edges[edges_len++] = l_iter;
            if (edges_len == len_max) {
              goto finally;
            }

            /* Tag other connected loops so we don't consider them separate edges. */
            if (l_iter != l_iter->radial_next) {
              BMLoop *l_radial_iter = l_iter->radial_next;
              do {
                if (BM_loop_uv_share_edge_check(l_iter, l_radial_iter, cd_loop_uv_offset)) {
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
    edges = MEM_reallocN(edges, sizeof(*edges) * edges_len);
  }
  return edges;
}

BMLoop **ED_uvedit_selected_verts(Scene *scene, BMesh *bm, int len_max, int *r_verts_len)
{
  const int cd_loop_uv_offset = CustomData_get_offset(&bm->ldata, CD_MLOOPUV);
  CLAMP_MAX(len_max, bm->totloop);
  int verts_len = 0;
  BMLoop **verts = MEM_mallocN(sizeof(*verts) * len_max, __func__);

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
          const MLoopUV *luv = BM_ELEM_CD_GET_VOID_P(l_iter, cd_loop_uv_offset);
          if ((luv->flag & MLOOPUV_VERTSEL)) {
            BM_elem_flag_enable(l_iter->v, BM_ELEM_TAG);

            verts[verts_len++] = l_iter;
            if (verts_len == len_max) {
              goto finally;
            }

            /* Tag other connected loops so we don't consider them separate vertices. */
            BMIter liter_disk;
            BMLoop *l_disk_iter;
            BM_ITER_ELEM (l_disk_iter, &liter_disk, l_iter->v, BM_LOOPS_OF_VERT) {
              if (BM_loop_uv_share_vert_check(l_iter, l_disk_iter, cd_loop_uv_offset)) {
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
    verts = MEM_reallocN(verts, sizeof(*verts) * verts_len);
  }
  return verts;
}

/** \} */
