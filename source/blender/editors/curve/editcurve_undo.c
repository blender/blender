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
 * \ingroup edcurve
 */

#include "MEM_guardedalloc.h"

#include "CLG_log.h"

#include "DNA_anim_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BLI_array_utils.h"
#include "BLI_blenlib.h"
#include "BLI_ghash.h"

#include "BKE_anim_data.h"
#include "BKE_context.h"
#include "BKE_curve.h"
#include "BKE_fcurve.h"
#include "BKE_layer.h"
#include "BKE_main.h"
#include "BKE_undo_system.h"

#include "DEG_depsgraph.h"

#include "ED_curve.h"
#include "ED_undo.h"

#include "WM_api.h"
#include "WM_types.h"

#include "curve_intern.h"

/** We only need this locally. */
static CLG_LogRef LOG = {"ed.undo.curve"};

/* -------------------------------------------------------------------- */
/** \name Undo Conversion
 * \{ */

typedef struct {
  ListBase nubase;
  int actvert;
  GHash *undoIndex;
  ListBase fcurves, drivers;
  int actnu;
  int flag;

  /* Stored in the object, needed since users may change the active key while in edit-mode. */
  struct {
    short shapenr;
  } obedit;

  size_t undo_size;
} UndoCurve;

static void undocurve_to_editcurve(Main *bmain, UndoCurve *ucu, Curve *cu, short *r_shapenr)
{
  ListBase *undobase = &ucu->nubase;
  ListBase *editbase = BKE_curve_editNurbs_get(cu);
  Nurb *nu, *newnu;
  EditNurb *editnurb = cu->editnurb;
  AnimData *ad = BKE_animdata_from_id(&cu->id);

  BKE_nurbList_free(editbase);

  if (ucu->undoIndex) {
    BKE_curve_editNurb_keyIndex_free(&editnurb->keyindex);
    editnurb->keyindex = ED_curve_keyindex_hash_duplicate(ucu->undoIndex);
  }

  if (ad) {
    if (ad->action) {
      BKE_fcurves_free(&ad->action->curves);
      BKE_fcurves_copy(&ad->action->curves, &ucu->fcurves);
    }

    BKE_fcurves_free(&ad->drivers);
    BKE_fcurves_copy(&ad->drivers, &ucu->drivers);
  }

  /* copy  */
  for (nu = undobase->first; nu; nu = nu->next) {
    newnu = BKE_nurb_duplicate(nu);

    if (editnurb->keyindex) {
      ED_curve_keyindex_update_nurb(editnurb, nu, newnu);
    }

    BLI_addtail(editbase, newnu);
  }

  cu->actvert = ucu->actvert;
  cu->actnu = ucu->actnu;
  cu->flag = ucu->flag;
  *r_shapenr = ucu->obedit.shapenr;
  ED_curve_updateAnimPaths(bmain, cu);
}

static void undocurve_from_editcurve(UndoCurve *ucu, Curve *cu, const short shapenr)
{
  BLI_assert(BLI_array_is_zeroed(ucu, 1));
  ListBase *nubase = BKE_curve_editNurbs_get(cu);
  EditNurb *editnurb = cu->editnurb, tmpEditnurb;
  Nurb *nu, *newnu;
  AnimData *ad = BKE_animdata_from_id(&cu->id);

  /* TODO: include size of fcurve & undoIndex */
  // ucu->undo_size = 0;

  if (editnurb->keyindex) {
    ucu->undoIndex = ED_curve_keyindex_hash_duplicate(editnurb->keyindex);
    tmpEditnurb.keyindex = ucu->undoIndex;
  }

  if (ad) {
    if (ad->action) {
      BKE_fcurves_copy(&ucu->fcurves, &ad->action->curves);
    }

    BKE_fcurves_copy(&ucu->drivers, &ad->drivers);
  }

  /* copy  */
  for (nu = nubase->first; nu; nu = nu->next) {
    newnu = BKE_nurb_duplicate(nu);

    if (ucu->undoIndex) {
      ED_curve_keyindex_update_nurb(&tmpEditnurb, nu, newnu);
    }

    BLI_addtail(&ucu->nubase, newnu);

    ucu->undo_size += ((nu->bezt ? (sizeof(BezTriple) * nu->pntsu) : 0) +
                       (nu->bp ? (sizeof(BPoint) * (nu->pntsu * nu->pntsv)) : 0) +
                       (nu->knotsu ? (sizeof(float) * KNOTSU(nu)) : 0) +
                       (nu->knotsv ? (sizeof(float) * KNOTSV(nu)) : 0) + sizeof(Nurb));
  }

  ucu->actvert = cu->actvert;
  ucu->actnu = cu->actnu;
  ucu->flag = cu->flag;

  ucu->obedit.shapenr = shapenr;
}

static void undocurve_free_data(UndoCurve *uc)
{
  BKE_nurbList_free(&uc->nubase);

  BKE_curve_editNurb_keyIndex_free(&uc->undoIndex);

  BKE_fcurves_free(&uc->fcurves);
  BKE_fcurves_free(&uc->drivers);
}

static Object *editcurve_object_from_context(bContext *C)
{
  Object *obedit = CTX_data_edit_object(C);
  if (obedit && ELEM(obedit->type, OB_CURVE, OB_SURF)) {
    Curve *cu = obedit->data;
    if (BKE_curve_editNurbs_get(cu) != NULL) {
      return obedit;
    }
  }
  return NULL;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Implements ED Undo System
 *
 * \note This is similar for all edit-mode types.
 * \{ */

typedef struct CurveUndoStep_Elem {
  UndoRefID_Object obedit_ref;
  UndoCurve data;
} CurveUndoStep_Elem;

typedef struct CurveUndoStep {
  UndoStep step;
  CurveUndoStep_Elem *elems;
  uint elems_len;
} CurveUndoStep;

static bool curve_undosys_poll(bContext *C)
{
  Object *obedit = editcurve_object_from_context(C);
  return (obedit != NULL);
}

static bool curve_undosys_step_encode(struct bContext *C, struct Main *bmain, UndoStep *us_p)
{
  CurveUndoStep *us = (CurveUndoStep *)us_p;

  /* Important not to use the 3D view when getting objects because all objects
   * outside of this list will be moved out of edit-mode when reading back undo steps. */
  ViewLayer *view_layer = CTX_data_view_layer(C);
  uint objects_len = 0;
  Object **objects = ED_undo_editmode_objects_from_view_layer(view_layer, &objects_len);

  us->elems = MEM_callocN(sizeof(*us->elems) * objects_len, __func__);
  us->elems_len = objects_len;

  for (uint i = 0; i < objects_len; i++) {
    Object *ob = objects[i];
    Curve *cu = ob->data;
    CurveUndoStep_Elem *elem = &us->elems[i];

    elem->obedit_ref.ptr = ob;
    undocurve_from_editcurve(&elem->data, ob->data, ob->shapenr);
    cu->editnurb->needs_flush_to_id = 1;
    us->step.data_size += elem->data.undo_size;
  }
  MEM_freeN(objects);

  bmain->is_memfile_undo_flush_needed = true;

  return true;
}

static void curve_undosys_step_decode(
    struct bContext *C, struct Main *bmain, UndoStep *us_p, int UNUSED(dir), bool UNUSED(is_final))
{
  CurveUndoStep *us = (CurveUndoStep *)us_p;

  /* Load all our objects  into edit-mode, clear everything else. */
  ED_undo_object_editmode_restore_helper(
      C, &us->elems[0].obedit_ref.ptr, us->elems_len, sizeof(*us->elems));

  BLI_assert(curve_undosys_poll(C));

  for (uint i = 0; i < us->elems_len; i++) {
    CurveUndoStep_Elem *elem = &us->elems[i];
    Object *obedit = elem->obedit_ref.ptr;
    Curve *cu = obedit->data;
    if (cu->editnurb == NULL) {
      /* Should never fail, may not crash but can give odd behavior. */
      CLOG_ERROR(&LOG,
                 "name='%s', failed to enter edit-mode for object '%s', undo state invalid",
                 us_p->name,
                 obedit->id.name);
      continue;
    }
    undocurve_to_editcurve(bmain, &elem->data, obedit->data, &obedit->shapenr);
    cu->editnurb->needs_flush_to_id = 1;
    DEG_id_tag_update(&obedit->id, ID_RECALC_GEOMETRY);
  }

  /* The first element is always active */
  ED_undo_object_set_active_or_warn(
      CTX_data_view_layer(C), us->elems[0].obedit_ref.ptr, us_p->name, &LOG);

  bmain->is_memfile_undo_flush_needed = true;

  WM_event_add_notifier(C, NC_GEOM | ND_DATA, NULL);
}

static void curve_undosys_step_free(UndoStep *us_p)
{
  CurveUndoStep *us = (CurveUndoStep *)us_p;

  for (uint i = 0; i < us->elems_len; i++) {
    CurveUndoStep_Elem *elem = &us->elems[i];
    undocurve_free_data(&elem->data);
  }
  MEM_freeN(us->elems);
}

static void curve_undosys_foreach_ID_ref(UndoStep *us_p,
                                         UndoTypeForEachIDRefFn foreach_ID_ref_fn,
                                         void *user_data)
{
  CurveUndoStep *us = (CurveUndoStep *)us_p;

  for (uint i = 0; i < us->elems_len; i++) {
    CurveUndoStep_Elem *elem = &us->elems[i];
    foreach_ID_ref_fn(user_data, ((UndoRefID *)&elem->obedit_ref));
  }
}

/* Export for ED_undo_sys. */
void ED_curve_undosys_type(UndoType *ut)
{
  ut->name = "Edit Curve";
  ut->poll = curve_undosys_poll;
  ut->step_encode = curve_undosys_step_encode;
  ut->step_decode = curve_undosys_step_decode;
  ut->step_free = curve_undosys_step_free;

  ut->step_foreach_ID_ref = curve_undosys_foreach_ID_ref;

  ut->use_context = true;

  ut->step_size = sizeof(CurveUndoStep);
}

/** \} */
