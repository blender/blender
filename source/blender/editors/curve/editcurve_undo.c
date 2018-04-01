/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
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
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/curve/editcurve_undo.c
 *  \ingroup edcurve
 */

#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_anim_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_ghash.h"
#include "BLI_array_utils.h"

#include "BKE_context.h"
#include "BKE_curve.h"
#include "BKE_fcurve.h"
#include "BKE_library.h"
#include "BKE_animsys.h"
#include "BKE_undo_system.h"

#include "DEG_depsgraph.h"

#include "ED_object.h"
#include "ED_util.h"
#include "ED_curve.h"

#include "WM_types.h"
#include "WM_api.h"

#include "curve_intern.h"

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
	size_t undo_size;
} UndoCurve;

static void undocurve_to_editcurve(UndoCurve *ucu, Curve *cu)
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
			free_fcurves(&ad->action->curves);
			copy_fcurves(&ad->action->curves, &ucu->fcurves);
		}

		free_fcurves(&ad->drivers);
		copy_fcurves(&ad->drivers, &ucu->drivers);
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
	ED_curve_updateAnimPaths(cu);
}

static void undocurve_from_editcurve(UndoCurve *ucu, Curve *cu)
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
		if (ad->action)
			copy_fcurves(&ucu->fcurves, &ad->action->curves);

		copy_fcurves(&ucu->drivers, &ad->drivers);
	}

	/* copy  */
	for (nu = nubase->first; nu; nu = nu->next) {
		newnu = BKE_nurb_duplicate(nu);

		if (ucu->undoIndex) {
			ED_curve_keyindex_update_nurb(&tmpEditnurb, nu, newnu);
		}

		BLI_addtail(&ucu->nubase, newnu);

		ucu->undo_size += (
		        (nu->bezt ? (sizeof(BezTriple) * nu->pntsu) : 0) +
		        (nu->bp ? (sizeof(BPoint) * (nu->pntsu * nu->pntsv)) : 0) +
		        (nu->knotsu ? (sizeof(float) * KNOTSU(nu)) : 0) +
		        (nu->knotsv ? (sizeof(float) * KNOTSV(nu)) : 0) +
		        sizeof(Nurb));
	}

	ucu->actvert = cu->actvert;
	ucu->actnu = cu->actnu;
	ucu->flag = cu->flag;
}

static void undocurve_free_data(UndoCurve *uc)
{
	BKE_nurbList_free(&uc->nubase);

	BKE_curve_editNurb_keyIndex_free(&uc->undoIndex);

	free_fcurves(&uc->fcurves);
	free_fcurves(&uc->drivers);
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
 * \{ */

typedef struct CurveUndoStep {
	UndoStep step;
	/* note: will split out into list for multi-object-editmode. */
	UndoRefID_Object obedit_ref;
	UndoCurve data;
} CurveUndoStep;

static bool curve_undosys_poll(bContext *C)
{
	Object *obedit = editcurve_object_from_context(C);
	return (obedit != NULL);
}

static bool curve_undosys_step_encode(struct bContext *C, UndoStep *us_p)
{
	CurveUndoStep *us = (CurveUndoStep *)us_p;
	us->obedit_ref.ptr = editcurve_object_from_context(C);
	undocurve_from_editcurve(&us->data, us->obedit_ref.ptr->data);
	us->step.data_size = us->data.undo_size;
	return true;
}

static void curve_undosys_step_decode(struct bContext *C, UndoStep *us_p, int UNUSED(dir))
{
	/* TODO(campbell): undo_system: use low-level API to set mode. */
	ED_object_mode_set(C, OB_MODE_EDIT);
	BLI_assert(curve_undosys_poll(C));

	CurveUndoStep *us = (CurveUndoStep *)us_p;
	Object *obedit = us->obedit_ref.ptr;
	undocurve_to_editcurve(&us->data, obedit->data);
	DEG_id_tag_update(&obedit->id, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_GEOM | ND_DATA, NULL);
}

static void curve_undosys_step_free(UndoStep *us_p)
{
	CurveUndoStep *us = (CurveUndoStep *)us_p;
	undocurve_free_data(&us->data);
}

static void curve_undosys_foreach_ID_ref(
        UndoStep *us_p, UndoTypeForEachIDRefFn foreach_ID_ref_fn, void *user_data)
{
	CurveUndoStep *us = (CurveUndoStep *)us_p;
	foreach_ID_ref_fn(user_data, ((UndoRefID *)&us->obedit_ref));
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

	ut->mode = BKE_UNDOTYPE_MODE_STORE;
	ut->use_context = true;

	ut->step_size = sizeof(CurveUndoStep);
}

/** \} */
