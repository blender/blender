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
 */

/** \file
 * \ingroup edmesh
 */

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_bitmap.h"

#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"

#include "DNA_meshdata_types.h"
#include "DNA_mesh_types.h"
#include "DNA_object_types.h"

#include "BKE_context.h"
#include "BKE_customdata.h"
#include "BKE_global.h"
#include "BKE_mesh.h"

#include "BIF_gl.h"

#include "ED_mesh.h"
#include "ED_screen.h"
#include "ED_select_utils.h"
#include "ED_view3d.h"

#include "WM_api.h"
#include "WM_types.h"

#include "GPU_draw.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

/* own include */

/* copy the face flags, most importantly selection from the mesh to the final derived mesh,
 * use in object mode when selecting faces (while painting) */
void paintface_flush_flags(struct bContext *C, Object *ob, short flag)
{
  Mesh *me = BKE_mesh_from_object(ob);
  MPoly *polys, *mp_orig;
  const int *index_array = NULL;
  int totpoly;
  int i;

  BLI_assert((flag & ~(SELECT | ME_HIDE)) == 0);

  if (me == NULL) {
    return;
  }

  /* note, call #BKE_mesh_flush_hidden_from_verts_ex first when changing hidden flags */

  /* we could call this directly in all areas that change selection,
   * since this could become slow for realtime updates (circle-select for eg) */
  if (flag & SELECT) {
    BKE_mesh_flush_select_from_polys(me);
  }

  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  Object *ob_eval = DEG_get_evaluated_object(depsgraph, ob);

  if (ob_eval == NULL) {
    return;
  }

  Mesh *me_orig = ob_eval->runtime.mesh_orig;
  Mesh *me_eval = ob_eval->runtime.mesh_eval;
  bool updated = false;

  if (me_orig != NULL && me_eval != NULL && me_orig->totpoly == me->totpoly) {
    /* Update the COW copy of the mesh. */
    for (i = 0; i < me->totpoly; i++) {
      me_orig->mpoly[i].flag = me->mpoly[i].flag;
    }

    /* If the mesh has only deform modifiers, the evaluated mesh shares arrays. */
    if (me_eval->mpoly == me_orig->mpoly) {
      updated = true;
    }
    /* Mesh polys => Final derived polys */
    else if ((index_array = CustomData_get_layer(&me_eval->pdata, CD_ORIGINDEX))) {
      polys = me_eval->mpoly;
      totpoly = me_eval->totpoly;

      /* loop over final derived polys */
      for (i = 0; i < totpoly; i++) {
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

    DEG_id_tag_update(ob->data, ID_RECALC_SELECT);
  }
  else {
    DEG_id_tag_update(ob->data, ID_RECALC_COPY_ON_WRITE | ID_RECALC_SELECT);
  }

  WM_event_add_notifier(C, NC_GEOM | ND_SELECT, ob->data);
}

void paintface_hide(bContext *C, Object *ob, const bool unselected)
{
  Mesh *me;
  MPoly *mpoly;
  int a;

  me = BKE_mesh_from_object(ob);
  if (me == NULL || me->totpoly == 0) {
    return;
  }

  mpoly = me->mpoly;
  a = me->totpoly;
  while (a--) {
    if ((mpoly->flag & ME_HIDE) == 0) {
      if (((mpoly->flag & ME_FACE_SEL) == 0) == unselected) {
        mpoly->flag |= ME_HIDE;
      }
    }

    if (mpoly->flag & ME_HIDE) {
      mpoly->flag &= ~ME_FACE_SEL;
    }

    mpoly++;
  }

  BKE_mesh_flush_hidden_from_polys(me);

  paintface_flush_flags(C, ob, SELECT | ME_HIDE);
}

void paintface_reveal(bContext *C, Object *ob, const bool select)
{
  Mesh *me;
  MPoly *mpoly;
  int a;

  me = BKE_mesh_from_object(ob);
  if (me == NULL || me->totpoly == 0) {
    return;
  }

  mpoly = me->mpoly;
  a = me->totpoly;
  while (a--) {
    if (mpoly->flag & ME_HIDE) {
      SET_FLAG_FROM_TEST(mpoly->flag, select, ME_FACE_SEL);
      mpoly->flag &= ~ME_HIDE;
    }
    mpoly++;
  }

  BKE_mesh_flush_hidden_from_polys(me);

  paintface_flush_flags(C, ob, SELECT | ME_HIDE);
}

/* Set tface seams based on edge data, uses hash table to find seam edges. */

static void select_linked_tfaces_with_seams(Mesh *me, const unsigned int index, const bool select)
{
  MPoly *mp;
  MLoop *ml;
  int a, b;
  bool do_it = true;
  bool mark = false;

  BLI_bitmap *edge_tag = BLI_BITMAP_NEW(me->totedge, __func__);
  BLI_bitmap *poly_tag = BLI_BITMAP_NEW(me->totpoly, __func__);

  if (index != (unsigned int)-1) {
    /* only put face under cursor in array */
    mp = &me->mpoly[index];
    BKE_mesh_poly_edgebitmap_insert(edge_tag, mp, me->mloop + mp->loopstart);
    BLI_BITMAP_ENABLE(poly_tag, index);
  }
  else {
    /* fill array by selection */
    mp = me->mpoly;
    for (a = 0; a < me->totpoly; a++, mp++) {
      if (mp->flag & ME_HIDE) {
        /* pass */
      }
      else if (mp->flag & ME_FACE_SEL) {
        BKE_mesh_poly_edgebitmap_insert(edge_tag, mp, me->mloop + mp->loopstart);
        BLI_BITMAP_ENABLE(poly_tag, a);
      }
    }
  }

  while (do_it) {
    do_it = false;

    /* expand selection */
    mp = me->mpoly;
    for (a = 0; a < me->totpoly; a++, mp++) {
      if (mp->flag & ME_HIDE) {
        continue;
      }

      if (!BLI_BITMAP_TEST(poly_tag, a)) {
        mark = false;

        ml = me->mloop + mp->loopstart;
        for (b = 0; b < mp->totloop; b++, ml++) {
          if ((me->medge[ml->e].flag & ME_SEAM) == 0) {
            if (BLI_BITMAP_TEST(edge_tag, ml->e)) {
              mark = true;
              break;
            }
          }
        }

        if (mark) {
          BLI_BITMAP_ENABLE(poly_tag, a);
          BKE_mesh_poly_edgebitmap_insert(edge_tag, mp, me->mloop + mp->loopstart);
          do_it = true;
        }
      }
    }
  }

  MEM_freeN(edge_tag);

  for (a = 0, mp = me->mpoly; a < me->totpoly; a++, mp++) {
    if (BLI_BITMAP_TEST(poly_tag, a)) {
      SET_FLAG_FROM_TEST(mp->flag, select, ME_FACE_SEL);
    }
  }

  MEM_freeN(poly_tag);
}

void paintface_select_linked(bContext *C, Object *ob, const int mval[2], const bool select)
{
  Mesh *me;
  unsigned int index = (unsigned int)-1;

  me = BKE_mesh_from_object(ob);
  if (me == NULL || me->totpoly == 0) {
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
  Mesh *me;
  MPoly *mpoly;
  int a;

  me = BKE_mesh_from_object(ob);
  if (me == NULL) {
    return false;
  }

  if (action == SEL_TOGGLE) {
    action = SEL_SELECT;

    mpoly = me->mpoly;
    a = me->totpoly;
    while (a--) {
      if ((mpoly->flag & ME_HIDE) == 0 && mpoly->flag & ME_FACE_SEL) {
        action = SEL_DESELECT;
        break;
      }
      mpoly++;
    }
  }

  bool changed = false;

  mpoly = me->mpoly;
  a = me->totpoly;
  while (a--) {
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
    mpoly++;
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
  const Mesh *me;
  const MPoly *mp;
  const MLoop *ml;
  const MVert *mvert;
  int a, b;
  bool ok = false;
  float vec[3], bmat[3][3];

  me = BKE_mesh_from_object(ob);
  if (!me || !me->mloopuv) {
    return ok;
  }

  copy_m3_m4(bmat, ob->obmat);

  mvert = me->mvert;
  mp = me->mpoly;
  for (a = me->totpoly; a > 0; a--, mp++) {
    if (mp->flag & ME_HIDE || !(mp->flag & ME_FACE_SEL)) {
      continue;
    }

    ml = me->mloop + mp->totloop;
    for (b = 0; b < mp->totloop; b++, ml++) {
      mul_v3_m3v3(vec, bmat, mvert[ml->v].co);
      add_v3_v3v3(vec, vec, ob->obmat[3]);
      minmax_v3v3_v3(r_min, r_max, vec);
    }

    ok = true;
  }

  return ok;
}

bool paintface_mouse_select(
    struct bContext *C, Object *ob, const int mval[2], bool extend, bool deselect, bool toggle)
{
  Mesh *me;
  MPoly *mpoly_sel;
  uint index;

  /* Get the face under the cursor */
  me = BKE_mesh_from_object(ob);

  if (!ED_mesh_pick_face(C, ob, mval, ED_MESH_PICK_DEFAULT_FACE_DIST, &index)) {
    return false;
  }

  if (index >= me->totpoly) {
    return false;
  }

  mpoly_sel = me->mpoly + index;
  if (mpoly_sel->flag & ME_HIDE) {
    return false;
  }

  /* clear flags */
  if (!extend && !deselect && !toggle) {
    paintface_deselect_all_visible(C, ob, SEL_DESELECT, false);
  }

  me->act_face = (int)index;

  if (extend) {
    mpoly_sel->flag |= ME_FACE_SEL;
  }
  else if (deselect) {
    mpoly_sel->flag &= ~ME_FACE_SEL;
  }
  else if (toggle) {
    if (mpoly_sel->flag & ME_FACE_SEL) {
      mpoly_sel->flag &= ~ME_FACE_SEL;
    }
    else {
      mpoly_sel->flag |= ME_FACE_SEL;
    }
  }
  else {
    mpoly_sel->flag |= ME_FACE_SEL;
  }

  /* image window redraw */

  paintface_flush_flags(C, ob, SELECT);
  ED_region_tag_redraw(CTX_wm_region(C));  // XXX - should redraw all 3D views
  return true;
}

/*  (similar to void paintface_flush_flags(Object *ob))
 * copy the vertex flags, most importantly selection from the mesh to the final derived mesh,
 * use in object mode when selecting vertices (while painting) */
void paintvert_flush_flags(Object *ob)
{
  Mesh *me = BKE_mesh_from_object(ob);
  Mesh *me_eval = ob->runtime.mesh_eval;
  MVert *mvert_eval, *mv;
  const int *index_array = NULL;
  int totvert;
  int i;

  if (me == NULL) {
    return;
  }

  /* we could call this directly in all areas that change selection,
   * since this could become slow for realtime updates (circle-select for eg) */
  BKE_mesh_flush_select_from_verts(me);

  if (me_eval == NULL) {
    return;
  }

  index_array = CustomData_get_layer(&me_eval->vdata, CD_ORIGINDEX);

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

void paintvert_tag_select_update(struct bContext *C, struct Object *ob)
{
  DEG_id_tag_update(ob->data, ID_RECALC_COPY_ON_WRITE | ID_RECALC_SELECT);
  WM_event_add_notifier(C, NC_GEOM | ND_SELECT, ob->data);
}

/*  note: if the caller passes false to flush_flags,
 *  then they will need to run paintvert_flush_flags(ob) themselves */
bool paintvert_deselect_all_visible(Object *ob, int action, bool flush_flags)
{
  Mesh *me;
  MVert *mvert;
  int a;

  me = BKE_mesh_from_object(ob);
  if (me == NULL) {
    return false;
  }

  if (action == SEL_TOGGLE) {
    action = SEL_SELECT;

    mvert = me->mvert;
    a = me->totvert;
    while (a--) {
      if ((mvert->flag & ME_HIDE) == 0 && mvert->flag & SELECT) {
        action = SEL_DESELECT;
        break;
      }
      mvert++;
    }
  }

  bool changed = false;
  mvert = me->mvert;
  a = me->totvert;
  while (a--) {
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
    mvert++;
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
  MVert *mv;
  MDeformVert *dv;
  int a, tot;

  if (me == NULL || me->dvert == NULL) {
    return;
  }

  if (!extend) {
    paintvert_deselect_all_visible(ob, SEL_DESELECT, false);
  }

  dv = me->dvert;
  tot = me->totvert;

  for (a = 0, mv = me->mvert; a < tot; a++, mv++, dv++) {
    if ((mv->flag & ME_HIDE) == 0) {
      if (dv->dw == NULL) {
        /* if null weight then not grouped */
        mv->flag |= SELECT;
      }
    }
  }

  if (flush_flags) {
    paintvert_flush_flags(ob);
  }
}
