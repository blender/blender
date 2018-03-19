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

#include "BKE_context.h"
#include "BKE_curve.h"
#include "BKE_fcurve.h"
#include "BKE_library.h"
#include "BKE_animsys.h"

#include "ED_util.h"
#include "ED_curve.h"

#include "curve_intern.h"

typedef struct {
	ListBase nubase;
	int actvert;
	GHash *undoIndex;
	ListBase fcurves, drivers;
	int actnu;
	int flag;
} UndoCurve;

static void undoCurve_to_editCurve(void *ucu, void *UNUSED(edata), void *cu_v)
{
	Curve *cu = cu_v;
	UndoCurve *undoCurve = ucu;
	ListBase *undobase = &undoCurve->nubase;
	ListBase *editbase = BKE_curve_editNurbs_get(cu);
	Nurb *nu, *newnu;
	EditNurb *editnurb = cu->editnurb;
	AnimData *ad = BKE_animdata_from_id(&cu->id);

	BKE_nurbList_free(editbase);

	if (undoCurve->undoIndex) {
		BKE_curve_editNurb_keyIndex_free(&editnurb->keyindex);
		editnurb->keyindex = ED_curve_keyindex_hash_duplicate(undoCurve->undoIndex);
	}

	if (ad) {
		if (ad->action) {
			free_fcurves(&ad->action->curves);
			copy_fcurves(&ad->action->curves, &undoCurve->fcurves);
		}

		free_fcurves(&ad->drivers);
		copy_fcurves(&ad->drivers, &undoCurve->drivers);
	}

	/* copy  */
	for (nu = undobase->first; nu; nu = nu->next) {
		newnu = BKE_nurb_duplicate(nu);

		if (editnurb->keyindex) {
			ED_curve_keyindex_update_nurb(editnurb, nu, newnu);
		}

		BLI_addtail(editbase, newnu);
	}

	cu->actvert = undoCurve->actvert;
	cu->actnu = undoCurve->actnu;
	cu->flag = undoCurve->flag;
	ED_curve_updateAnimPaths(cu);
}

static void *editCurve_to_undoCurve(void *UNUSED(edata), void *cu_v)
{
	Curve *cu = cu_v;
	ListBase *nubase = BKE_curve_editNurbs_get(cu);
	UndoCurve *undoCurve;
	EditNurb *editnurb = cu->editnurb, tmpEditnurb;
	Nurb *nu, *newnu;
	AnimData *ad = BKE_animdata_from_id(&cu->id);

	undoCurve = MEM_callocN(sizeof(UndoCurve), "undoCurve");

	if (editnurb->keyindex) {
		undoCurve->undoIndex = ED_curve_keyindex_hash_duplicate(editnurb->keyindex);
		tmpEditnurb.keyindex = undoCurve->undoIndex;
	}

	if (ad) {
		if (ad->action)
			copy_fcurves(&undoCurve->fcurves, &ad->action->curves);

		copy_fcurves(&undoCurve->drivers, &ad->drivers);
	}

	/* copy  */
	for (nu = nubase->first; nu; nu = nu->next) {
		newnu = BKE_nurb_duplicate(nu);

		if (undoCurve->undoIndex) {
			ED_curve_keyindex_update_nurb(&tmpEditnurb, nu, newnu);
		}

		BLI_addtail(&undoCurve->nubase, newnu);
	}

	undoCurve->actvert = cu->actvert;
	undoCurve->actnu = cu->actnu;
	undoCurve->flag = cu->flag;

	return undoCurve;
}

static void free_undoCurve(void *ucv)
{
	UndoCurve *undoCurve = ucv;

	BKE_nurbList_free(&undoCurve->nubase);

	BKE_curve_editNurb_keyIndex_free(&undoCurve->undoIndex);

	free_fcurves(&undoCurve->fcurves);
	free_fcurves(&undoCurve->drivers);

	MEM_freeN(undoCurve);
}

static void *get_data(bContext *C)
{
	Object *obedit = CTX_data_edit_object(C);
	return obedit;
}

/* and this is all the undo system needs to know */
void undo_push_curve(bContext *C, const char *name)
{
	undo_editmode_push(C, name, get_data, free_undoCurve, undoCurve_to_editCurve, editCurve_to_undoCurve, NULL);
}
