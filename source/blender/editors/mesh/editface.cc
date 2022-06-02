/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edmesh
 */

#include "MEM_guardedalloc.h"

#include "BLI_bitmap.h"
#include "BLI_blenlib.h"
#include "BLI_math.h"

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"

#include "BKE_context.h"
#include "BKE_customdata.h"
#include "BKE_global.h"
#include "BKE_mesh.h"
#include "BKE_object.h"

#include "ED_mesh.h"
#include "ED_screen.h"
#include "ED_select_utils.h"
#include "ED_view3d.h"

#include "WM_api.h"
#include "WM_types.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

/* own include */

void paintface_flush_flags(bContext *C, Object *ob, short flag)
{
  Mesh *me = BKE_mesh_from_object(ob);
  MPoly *polys, *mp_orig;
  const int *index_array = nullptr;
  int totpoly;

  BLI_assert((flag & ~(SELECT | ME_HIDE)) == 0);

  if (me == nullptr) {
    return;
  }

  /* NOTE: call #BKE_mesh_flush_hidden_from_verts_ex first when changing hidden flags. */

  /* we could call this directly in all areas that change selection,
   * since this could become slow for realtime updates (circle-select for eg) */
  if (flag & SELECT) {
    BKE_mesh_flush_select_from_polys(me);
  }

  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  Object *ob_eval = DEG_get_evaluated_object(depsgraph, ob);

  if (ob_eval == nullptr) {
    return;
  }

  Mesh *me_orig = (Mesh *)ob_eval->runtime.data_orig;
  Mesh *me_eval = (Mesh *)ob_eval->runtime.data_eval;
  bool updated = false;

  if (me_orig != nullptr && me_eval != nullptr && me_orig->totpoly == me->totpoly) {
    /* Update the COW copy of the mesh. */
    for (int i = 0; i < me->totpoly; i++) {
      me_orig->mpoly[i].flag = me->mpoly[i].flag;
    }

    /* If the mesh has only deform modifiers, the evaluated mesh shares arrays. */
    if (me_eval->mpoly == me_orig->mpoly) {
      updated = true;
    }
    /* Mesh polys => Final derived polys */
    else if ((index_array = (const int *)CustomData_get_layer(&me_eval->pdata, CD_ORIGINDEX))) {
      polys = me_eval->mpoly;
      totpoly = me_eval->totpoly;

      /* loop over final derived polys */
      for (int i = 0; i < totpoly; i++) {
        if (index_array[i] != ORIGINDEX_NONE) {
          /* Copy flags onto the final derived poly from the original mesh poly */
          mp_orig = me->mpoly + index_array[i];
          polys[i].flag = mp_orig->flag;
        }
      }

      updated = true;
    }
  }

  if (updated) {
    if (flag & ME_HIDE) {
      BKE_mesh_batch_cache_dirty_tag(me_eval, BKE_MESH_BATCH_DIRTY_ALL);
    }
    else {
      BKE_mesh_batch_cache_dirty_tag(me_eval, BKE_MESH_BATCH_DIRTY_SELECT_PAINT);
    }

    DEG_id_tag_update(static_cast<ID *>(ob->data), ID_RECALC_SELECT);
  }
  else {
    DEG_id_tag_update(static_cast<ID *>(ob->data), ID_RECALC_COPY_ON_WRITE | ID_RECALC_SELECT);
  }

  WM_event_add_notifier(C, NC_GEOM | ND_SELECT, ob->data);
}

void paintface_hide(bContext *C, Object *ob, const bool unselected)
{
  Mesh *me = BKE_mesh_from_object(ob);
  if (me == nullptr || me->totpoly == 0) {
    return;
  }

  for (int i = 0; i < me->totpoly; i++) {
    MPoly *mpoly = &me->mpoly[i];
    if ((mpoly->flag & ME_HIDE) == 0) {
      if (((mpoly->flag & ME_FACE_SEL) == 0) == unselected) {
        mpoly->flag |= ME_HIDE;
      }
    }

    if (mpoly->flag & ME_HIDE) {
      mpoly->flag &= ~ME_FACE_SEL;
    }
  }

  BKE_mesh_flush_hidden_from_polys(me);

  paintface_flush_flags(C, ob, SELECT | ME_HIDE);
}

void paintface_reveal(bContext *C, Object *ob, const bool select)
{
  Mesh *me = BKE_mesh_from_object(ob);
  if (me == nullptr || me->totpoly == 0) {
    return;
  }

  for (int i = 0; i < me->totpoly; i++) {
    MPoly *mpoly = &me->mpoly[i];
    if (mpoly->flag & ME_HIDE) {
      SET_FLAG_FROM_TEST(mpoly->flag, select, ME_FACE_SEL);
      mpoly->flag &= ~ME_HIDE;
    }
  }

  BKE_mesh_flush_hidden_from_polys(me);

  paintface_flush_flags(C, ob, SELECT | ME_HIDE);
}

/* Set object-mode face selection seams based on edge data, uses hash table to find seam edges. */

static void select_linked_tfaces_with_seams(Mesh *me, const uint index, const bool select)
{
  bool do_it = true;
  bool mark = false;

  BLI_bitmap *edge_tag = BLI_BITMAP_NEW(me->totedge, __func__);
  BLI_bitmap *poly_tag = BLI_BITMAP_NEW(me->totpoly, __func__);

  if (index != (uint)-1) {
    /* only put face under cursor in array */
    MPoly *mp = &me->mpoly[index];
    BKE_mesh_poly_edgebitmap_insert(edge_tag, mp, me->mloop + mp->loopstart);
    BLI_BITMAP_ENABLE(poly_tag, index);
  }
  else {
    /* fill array by selection */
    for (int i = 0; i < me->totpoly; i++) {
      MPoly *mp = &me->mpoly[i];
      if (mp->flag & ME_HIDE) {
        /* pass */
      }
      else if (mp->flag & ME_FACE_SEL) {
        BKE_mesh_poly_edgebitmap_insert(edge_tag, mp, me->mloop + mp->loopstart);
        BLI_BITMAP_ENABLE(poly_tag, i);
      }
    }
  }

  while (do_it) {
    do_it = false;

    /* expand selection */
    for (int i = 0; i < me->totpoly; i++) {
      MPoly *mp = &me->mpoly[i];
      if (mp->flag & ME_HIDE) {
        continue;
      }

      if (!BLI_BITMAP_TEST(poly_tag, i)) {
        mark = false;

        MLoop *ml = me->mloop + mp->loopstart;
        for (int b = 0; b < mp->totloop; b++, ml++) {
          if ((me->medge[ml->e].flag & ME_SEAM) == 0) {
            if (BLI_BITMAP_TEST(edge_tag, ml->e)) {
              mark = true;
              break;
            }
          }
        }

        if (mark) {
          BLI_BITMAP_ENABLE(poly_tag, i);
          BKE_mesh_poly_edgebitmap_insert(edge_tag, mp, me->mloop + mp->loopstart);
          do_it = true;
        }
      }
    }
  }

  MEM_freeN(edge_tag);

  for (int i = 0; i < me->totpoly; i++) {
    MPoly *mp = &me->mpoly[i];
    if (BLI_BITMAP_TEST(poly_tag, i)) {
      SET_FLAG_FROM_TEST(mp->flag, select, ME_FACE_SEL);
    }
  }

  MEM_freeN(poly_tag);
}

void paintface_select_linked(bContext *C, Object *ob, const int mval[2], const bool select)
{
  uint index = (uint)-1;

  Mesh *me = BKE_mesh_from_object(ob);
  if (me == nullptr || me->totpoly == 0) {
    return;
  }

  if (mval) {
    if (!ED_mesh_pick_face(C, ob, mval, ED_MESH_PICK_DEFAULT_FACE_DIST, &index)) {
      return;
    }
  }

  select_linked_tfaces_with_seams(me, index, select);

  paintface_flush_flags(C, ob, SELECT);
}

bool paintface_deselect_all_visible(bContext *C, Object *ob, int action, bool flush_flags)
{
  Mesh *me = BKE_mesh_from_object(ob);
  if (me == nullptr) {
    return false;
  }

  if (action == SEL_TOGGLE) {
    action = SEL_SELECT;

    for (int i = 0; i < me->totpoly; i++) {
      MPoly *mpoly = &me->mpoly[i];
      if ((mpoly->flag & ME_HIDE) == 0 && mpoly->flag & ME_FACE_SEL) {
        action = SEL_DESELECT;
        break;
      }
    }
  }

  bool changed = false;

  for (int i = 0; i < me->totpoly; i++) {
    MPoly *mpoly = &me->mpoly[i];
    if ((mpoly->flag & ME_HIDE) == 0) {
      switch (action) {
        case SEL_SELECT:
          if ((mpoly->flag & ME_FACE_SEL) == 0) {
            mpoly->flag |= ME_FACE_SEL;
            changed = true;
          }
          break;
        case SEL_DESELECT:
          if ((mpoly->flag & ME_FACE_SEL) != 0) {
            mpoly->flag &= ~ME_FACE_SEL;
            changed = true;
          }
          break;
        case SEL_INVERT:
          mpoly->flag ^= ME_FACE_SEL;
          changed = true;
          break;
      }
    }
  }

  if (changed) {
    if (flush_flags) {
      paintface_flush_flags(C, ob, SELECT);
    }
  }
  return changed;
}

bool paintface_minmax(Object *ob, float r_min[3], float r_max[3])
{
  bool ok = false;
  float vec[3], bmat[3][3];

  const Mesh *me = BKE_mesh_from_object(ob);
  if (!me || !me->mloopuv) {
    return ok;
  }
  const MVert *mvert = me->mvert;

  copy_m3_m4(bmat, ob->obmat);

  for (int i = 0; i < me->totpoly; i++) {
    MPoly *mp = &me->mpoly[i];
    if (mp->flag & ME_HIDE || !(mp->flag & ME_FACE_SEL)) {
      continue;
    }

    const MLoop *ml = me->mloop + mp->loopstart;
    for (int b = 0; b < mp->totloop; b++, ml++) {
      mul_v3_m3v3(vec, bmat, mvert[ml->v].co);
      add_v3_v3v3(vec, vec, ob->obmat[3]);
      minmax_v3v3_v3(r_min, r_max, vec);
    }

    ok = true;
  }

  return ok;
}

bool paintface_mouse_select(bContext *C,
                            const int mval[2],
                            const SelectPick_Params *params,
                            Object *ob)
{
  MPoly *mpoly_sel = nullptr;
  uint index;
  bool changed = false;
  bool found = false;

  /* Get the face under the cursor */
  Mesh *me = BKE_mesh_from_object(ob);

  if (ED_mesh_pick_face(C, ob, mval, ED_MESH_PICK_DEFAULT_FACE_DIST, &index)) {
    if (index < me->totpoly) {
      mpoly_sel = me->mpoly + index;
      if ((mpoly_sel->flag & ME_HIDE) == 0) {
        found = true;
      }
    }
  }

  if (params->sel_op == SEL_OP_SET) {
    if ((found && params->select_passthrough) && (mpoly_sel->flag & ME_FACE_SEL)) {
      found = false;
    }
    else if (found || params->deselect_all) {
      /* Deselect everything. */
      changed |= paintface_deselect_all_visible(C, ob, SEL_DESELECT, false);
    }
  }

  if (found) {
    me->act_face = (int)index;

    switch (params->sel_op) {
      case SEL_OP_ADD: {
        mpoly_sel->flag |= ME_FACE_SEL;
        break;
      }
      case SEL_OP_SUB: {
        mpoly_sel->flag &= ~ME_FACE_SEL;
        break;
      }
      case SEL_OP_XOR: {
        if (mpoly_sel->flag & ME_FACE_SEL) {
          mpoly_sel->flag &= ~ME_FACE_SEL;
        }
        else {
          mpoly_sel->flag |= ME_FACE_SEL;
        }
        break;
      }
      case SEL_OP_SET: {
        mpoly_sel->flag |= ME_FACE_SEL;
        break;
      }
      case SEL_OP_AND: {
        BLI_assert_unreachable(); /* Doesn't make sense for picking. */
        break;
      }
    }

    /* image window redraw */

    paintface_flush_flags(C, ob, SELECT);
    ED_region_tag_redraw(CTX_wm_region(C)); /* XXX: should redraw all 3D views. */
    changed = true;
  }
  return changed || found;
}

void paintvert_flush_flags(Object *ob)
{
  Mesh *me = BKE_mesh_from_object(ob);
  Mesh *me_eval = BKE_object_get_evaluated_mesh(ob);
  MVert *mvert_eval, *mv;
  const int *index_array = nullptr;
  int totvert;
  int i;

  if (me == nullptr) {
    return;
  }

  /* we could call this directly in all areas that change selection,
   * since this could become slow for realtime updates (circle-select for eg) */
  BKE_mesh_flush_select_from_verts(me);

  if (me_eval == nullptr) {
    return;
  }

  index_array = (const int *)CustomData_get_layer(&me_eval->vdata, CD_ORIGINDEX);

  mvert_eval = me_eval->mvert;
  totvert = me_eval->totvert;

  mv = mvert_eval;

  if (index_array) {
    int orig_index;
    for (i = 0; i < totvert; i++, mv++) {
      orig_index = index_array[i];
      if (orig_index != ORIGINDEX_NONE) {
        mv->flag = me->mvert[index_array[i]].flag;
      }
    }
  }
  else {
    for (i = 0; i < totvert; i++, mv++) {
      mv->flag = me->mvert[i].flag;
    }
  }

  BKE_mesh_batch_cache_dirty_tag(me, BKE_MESH_BATCH_DIRTY_ALL);
}

void paintvert_tag_select_update(bContext *C, Object *ob)
{
  DEG_id_tag_update(static_cast<ID *>(ob->data), ID_RECALC_COPY_ON_WRITE | ID_RECALC_SELECT);
  WM_event_add_notifier(C, NC_GEOM | ND_SELECT, ob->data);
}

bool paintvert_deselect_all_visible(Object *ob, int action, bool flush_flags)
{
  Mesh *me = BKE_mesh_from_object(ob);
  if (me == nullptr) {
    return false;
  }

  if (action == SEL_TOGGLE) {
    action = SEL_SELECT;

    for (int i = 0; i < me->totvert; i++) {
      MVert *mvert = &me->mvert[i];
      if ((mvert->flag & ME_HIDE) == 0 && mvert->flag & SELECT) {
        action = SEL_DESELECT;
        break;
      }
    }
  }

  bool changed = false;
  for (int i = 0; i < me->totvert; i++) {
    MVert *mvert = &me->mvert[i];
    if ((mvert->flag & ME_HIDE) == 0) {
      switch (action) {
        case SEL_SELECT:
          if ((mvert->flag & SELECT) == 0) {
            mvert->flag |= SELECT;
            changed = true;
          }
          break;
        case SEL_DESELECT:
          if ((mvert->flag & SELECT) != 0) {
            mvert->flag &= ~SELECT;
            changed = true;
          }
          break;
        case SEL_INVERT:
          mvert->flag ^= SELECT;
          changed = true;
          break;
      }
    }
  }

  if (changed) {
    /* handle mselect */
    if (action == SEL_SELECT) {
      /* pass */
    }
    else if (ELEM(action, SEL_DESELECT, SEL_INVERT)) {
      BKE_mesh_mselect_clear(me);
    }
    else {
      BKE_mesh_mselect_validate(me);
    }

    if (flush_flags) {
      paintvert_flush_flags(ob);
    }
  }
  return changed;
}

void paintvert_select_ungrouped(Object *ob, bool extend, bool flush_flags)
{
  Mesh *me = BKE_mesh_from_object(ob);

  if (me == nullptr || me->dvert == nullptr) {
    return;
  }

  if (!extend) {
    paintvert_deselect_all_visible(ob, SEL_DESELECT, false);
  }

  for (int i = 0; i < me->totvert; i++) {
    MVert *mv = &me->mvert[i];
    MDeformVert *dv = &me->dvert[i];
    if ((mv->flag & ME_HIDE) == 0) {
      if (dv->dw == nullptr) {
        /* if null weight then not grouped */
        mv->flag |= SELECT;
      }
    }
  }

  if (flush_flags) {
    paintvert_flush_flags(ob);
  }
}
