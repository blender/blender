/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

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
#include "BKE_object.h"
#include "BKE_undo_system.h"

#include "DEG_depsgraph.h"

#include "ED_curve.h"
#include "ED_undo.h"

#include "WM_api.hh"
#include "WM_types.hh"

#include "curve_intern.h"

/** We only need this locally. */
static CLG_LogRef LOG = {"ed.undo.curve"};

/* -------------------------------------------------------------------- */
/** \name Undo Conversion
 * \{ */

struct UndoCurve {
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
};

static void undocurve_to_editcurve(Main *bmain, UndoCurve *ucu, Curve *cu, short *r_shapenr)
{
  ListBase *undobase = &ucu->nubase;
  ListBase *editbase = BKE_curve_editNurbs_get(cu);
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

  /* Copy. */
  LISTBASE_FOREACH (Nurb *, nu, undobase) {
    Nurb *newnu = BKE_nurb_duplicate(nu);

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

  /* Copy. */
  LISTBASE_FOREACH (Nurb *, nu, nubase) {
    Nurb *newnu = BKE_nurb_duplicate(nu);

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
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  BKE_view_layer_synced_ensure(scene, view_layer);
  Object *obedit = BKE_view_layer_edit_object_get(view_layer);
  if (obedit && ELEM(obedit->type, OB_CURVES_LEGACY, OB_SURF)) {
    Curve *cu = static_cast<Curve *>(obedit->data);
    if (BKE_curve_editNurbs_get(cu) != nullptr) {
      return obedit;
    }
  }
  return nullptr;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Implements ED Undo System
 *
 * \note This is similar for all edit-mode types.
 * \{ */

struct CurveUndoStep_Elem {
  UndoRefID_Object obedit_ref;
  UndoCurve data;
};

struct CurveUndoStep {
  UndoStep step;
  CurveUndoStep_Elem *elems;
  uint elems_len;
};

static bool curve_undosys_poll(bContext *C)
{
  Object *obedit = editcurve_object_from_context(C);
  return (obedit != nullptr);
}

static bool curve_undosys_step_encode(bContext *C, Main *bmain, UndoStep *us_p)
{
  CurveUndoStep *us = (CurveUndoStep *)us_p;

  /* Important not to use the 3D view when getting objects because all objects
   * outside of this list will be moved out of edit-mode when reading back undo steps. */
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  uint objects_len = 0;
  Object **objects = ED_undo_editmode_objects_from_view_layer(scene, view_layer, &objects_len);

  us->elems = static_cast<CurveUndoStep_Elem *>(
      MEM_callocN(sizeof(*us->elems) * objects_len, __func__));
  us->elems_len = objects_len;

  for (uint i = 0; i < objects_len; i++) {
    Object *ob = objects[i];
    Curve *cu = static_cast<Curve *>(ob->data);
    CurveUndoStep_Elem *elem = &us->elems[i];

    elem->obedit_ref.ptr = ob;
    undocurve_from_editcurve(&elem->data, static_cast<Curve *>(ob->data), ob->shapenr);
    cu->editnurb->needs_flush_to_id = 1;
    us->step.data_size += elem->data.undo_size;
  }
  MEM_freeN(objects);

  bmain->is_memfile_undo_flush_needed = true;

  return true;
}

static void curve_undosys_step_decode(
    bContext *C, Main *bmain, UndoStep *us_p, const eUndoStepDir /*dir*/, bool /*is_final*/)
{
  CurveUndoStep *us = (CurveUndoStep *)us_p;

  ED_undo_object_editmode_restore_helper(
      C, &us->elems[0].obedit_ref.ptr, us->elems_len, sizeof(*us->elems));

  BLI_assert(BKE_object_is_in_editmode(us->elems[0].obedit_ref.ptr));

  for (uint i = 0; i < us->elems_len; i++) {
    CurveUndoStep_Elem *elem = &us->elems[i];
    Object *obedit = elem->obedit_ref.ptr;
    Curve *cu = static_cast<Curve *>(obedit->data);
    if (cu->editnurb == nullptr) {
      /* Should never fail, may not crash but can give odd behavior. */
      CLOG_ERROR(&LOG,
                 "name='%s', failed to enter edit-mode for object '%s', undo state invalid",
                 us_p->name,
                 obedit->id.name);
      continue;
    }
    undocurve_to_editcurve(
        bmain, &elem->data, static_cast<Curve *>(obedit->data), &obedit->shapenr);
    cu->editnurb->needs_flush_to_id = 1;
    DEG_id_tag_update(&cu->id, ID_RECALC_GEOMETRY);
  }

  /* The first element is always active */
  ED_undo_object_set_active_or_warn(
      CTX_data_scene(C), CTX_data_view_layer(C), us->elems[0].obedit_ref.ptr, us_p->name, &LOG);

  /* Check after setting active. */
  BLI_assert(curve_undosys_poll(C));

  bmain->is_memfile_undo_flush_needed = true;

  WM_event_add_notifier(C, NC_GEOM | ND_DATA, nullptr);
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

void ED_curve_undosys_type(UndoType *ut)
{
  ut->name = "Edit Curve";
  ut->poll = curve_undosys_poll;
  ut->step_encode = curve_undosys_step_encode;
  ut->step_decode = curve_undosys_step_decode;
  ut->step_free = curve_undosys_step_free;

  ut->step_foreach_ID_ref = curve_undosys_foreach_ID_ref;

  ut->flags = UNDOTYPE_FLAG_NEED_CONTEXT_FOR_ENCODE;

  ut->step_size = sizeof(CurveUndoStep);
}

/** \} */
