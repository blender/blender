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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/curve/editcurve.c
 *  \ingroup edcurve
 */

#include <math.h>
#include <string.h>

#ifndef WIN32
#include <unistd.h>
#else
#include <io.h>
#endif
#include <stdlib.h>

#include "DNA_key_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_anim_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_dynstr.h"
#include "BLI_rand.h"
#include "BLI_utildefines.h"
#include "BLI_ghash.h"

#include "BKE_context.h"
#include "BKE_curve.h"
#include "BKE_depsgraph.h"
#include "BKE_fcurve.h"
#include "BKE_global.h"
#include "BKE_key.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_report.h"
#include "BKE_animsys.h"
#include "BKE_action.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_keyframes_edit.h"
#include "ED_object.h"
#include "ED_screen.h"
#include "ED_transform.h"
#include "ED_types.h"
#include "ED_util.h"
#include "ED_view3d.h"
#include "ED_curve.h"

#include "curve_intern.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

/* Undo stuff */
typedef struct {
	ListBase nubase;
	void *lastsel;
	GHash *undoIndex;
	ListBase fcurves, drivers;
	int actnu;
} UndoCurve;

/* Definitions needed for shape keys */
typedef struct {
	void *orig_cv;
	int key_index, nu_index, pt_index;
	int switched;
	Nurb *orig_nu;
} CVKeyIndex;

void selectend_nurb(Object *obedit, short selfirst, short doswap, short selstatus);
static void select_adjacent_cp(ListBase *editnurb, short next, short cont, short selstatus);

/* still need to eradicate a few :( */
#define callocstructN(x, y, name) (x *)MEM_callocN((y) * sizeof(x), name)

static float nurbcircle[8][2] = {
	{0.0, -1.0}, {-1.0, -1.0}, {-1.0, 0.0}, {-1.0,  1.0},
	{0.0,  1.0}, { 1.0,  1.0}, { 1.0, 0.0}, { 1.0, -1.0}
};

ListBase *object_editcurve_get(Object *ob)
{
	if (ob && ELEM(ob->type, OB_CURVE, OB_SURF)) {
		Curve *cu = ob->data;
		return &cu->editnurb->nurbs;
	}
	return NULL;
}

/* this replaces the active flag used in uv/face mode */
static void set_actNurb(Object *obedit, Nurb *nu)
{
	Curve *cu = obedit->data;
	
	if (nu == NULL)
		cu->actnu = -1;
	else {
		ListBase *nurbs = BKE_curve_editNurbs_get(cu);
		cu->actnu = BLI_findindex(nurbs, nu);
	}
}

static Nurb *get_actNurb(Object *obedit)
{
	Curve *cu = obedit->data;
	ListBase *nurbs = BKE_curve_editNurbs_get(cu);

	return BLI_findlink(nurbs, cu->actnu);
}

/* ******************* SELECTION FUNCTIONS ********************* */

#define HIDDEN          1
#define VISIBLE         0

#define FIRST           1
#define LAST            0


/* returns 1 in case (de)selection was successful */
static short select_beztriple(BezTriple *bezt, short selstatus, short flag, short hidden)
{	
	if (bezt) {
		if ((bezt->hide == 0) || (hidden == 1)) {
			if (selstatus == 1) { /* selects */
				bezt->f1 |= flag;
				bezt->f2 |= flag;
				bezt->f3 |= flag;
				return 1;			
			}
			else { /* deselects */	
				bezt->f1 &= ~flag; 
				bezt->f2 &= ~flag; 
				bezt->f3 &= ~flag; 
				return 1;
			}
		}
	}
	
	return 0;
}

/* returns 1 in case (de)selection was successful */
static short select_bpoint(BPoint *bp, short selstatus, short flag, short hidden) 
{	
	if (bp) {
		if ((bp->hide == 0) || (hidden == 1)) {
			if (selstatus == 1) {
				bp->f1 |= flag;
				return 1;
			}
			else {
				bp->f1 &= ~flag;
				return 1;
			}
		}
	}

	return 0;
}

static short swap_selection_beztriple(BezTriple *bezt)
{
	if (bezt->f2 & SELECT)
		return select_beztriple(bezt, DESELECT, 1, VISIBLE);
	else
		return select_beztriple(bezt, SELECT, 1, VISIBLE);
}

static short swap_selection_bpoint(BPoint *bp)
{
	if (bp->f1 & SELECT)
		return select_bpoint(bp, DESELECT, 1, VISIBLE);
	else
		return select_bpoint(bp, SELECT, 1, VISIBLE);
}

int isNurbsel(Nurb *nu)
{
	BezTriple *bezt;
	BPoint *bp;
	int a;

	if (nu->type == CU_BEZIER) {
		bezt = nu->bezt;
		a = nu->pntsu;
		while (a--) {
			if ( (bezt->f1 & SELECT) || (bezt->f2 & SELECT) || (bezt->f3 & SELECT) ) return 1;
			bezt++;
		}
	}
	else {
		bp = nu->bp;
		a = nu->pntsu * nu->pntsv;
		while (a--) {
			if (bp->f1 & SELECT) return 1;
			bp++;
		}
	}
	return 0;
}

static int isNurbsel_count(Curve *cu, Nurb *nu)
{
	BezTriple *bezt;
	BPoint *bp;
	int a, sel = 0;

	if (nu->type == CU_BEZIER) {
		bezt = nu->bezt;
		a = nu->pntsu;
		while (a--) {
			if (BEZSELECTED_HIDDENHANDLES(cu, bezt)) sel++;
			bezt++;
		}
	}
	else {
		bp = nu->bp;
		a = nu->pntsu * nu->pntsv;
		while (a--) {
			if ( (bp->f1 & SELECT) ) sel++;
			bp++;
		}
	}
	return sel;
}

/* ******************* PRINTS ********************* */

void printknots(Object *obedit)
{
	ListBase *editnurb = object_editcurve_get(obedit);
	Nurb *nu;
	int a, num;

	for (nu = editnurb->first; nu; nu = nu->next) {
		if (isNurbsel(nu) && nu->type == CU_NURBS) {
			if (nu->knotsu) {
				num = KNOTSU(nu);
				for (a = 0; a < num; a++) printf("knotu %d: %f\n", a, nu->knotsu[a]);
			}
			if (nu->knotsv) {
				num = KNOTSV(nu);
				for (a = 0; a < num; a++) printf("knotv %d: %f\n", a, nu->knotsv[a]);
			}
		}
	}
}

/* ********************* Shape keys *************** */

static CVKeyIndex *init_cvKeyIndex(void *cv, int key_index, int nu_index, int pt_index, Nurb *orig_nu)
{
	CVKeyIndex *cvIndex = MEM_callocN(sizeof(CVKeyIndex), "init_cvKeyIndex");

	cvIndex->orig_cv = cv;
	cvIndex->key_index = key_index;
	cvIndex->nu_index = nu_index;
	cvIndex->pt_index = pt_index;
	cvIndex->switched = 0;
	cvIndex->orig_nu = orig_nu;

	return cvIndex;
}

static void init_editNurb_keyIndex(EditNurb *editnurb, ListBase *origBase)
{
	Nurb *nu = editnurb->nurbs.first;
	Nurb *orignu = origBase->first;
	GHash *gh;
	BezTriple *bezt, *origbezt;
	BPoint *bp, *origbp;
	CVKeyIndex *keyIndex;
	int a, key_index = 0, nu_index = 0, pt_index = 0;

	if (editnurb->keyindex) return;

	gh = BLI_ghash_ptr_new("editNurb keyIndex");

	while (orignu) {
		if (orignu->bezt) {
			a = orignu->pntsu;
			bezt = nu->bezt;
			origbezt = orignu->bezt;
			pt_index = 0;
			while (a--) {
				keyIndex = init_cvKeyIndex(origbezt, key_index, nu_index, pt_index, orignu);
				BLI_ghash_insert(gh, bezt, keyIndex);
				key_index += 12;
				bezt++;
				origbezt++;
				pt_index++;
			}
		}
		else {
			a = orignu->pntsu * orignu->pntsv;
			bp = nu->bp;
			origbp = orignu->bp;
			pt_index = 0;
			while (a--) {
				keyIndex = init_cvKeyIndex(origbp, key_index, nu_index, pt_index, orignu);
				BLI_ghash_insert(gh, bp, keyIndex);
				key_index += 4;
				bp++;
				origbp++;
				pt_index++;
			}
		}

		nu = nu->next;
		orignu = orignu->next;
		nu_index++;
	}

	editnurb->keyindex = gh;
}

static CVKeyIndex *getCVKeyIndex(EditNurb *editnurb, void *cv)
{
	return BLI_ghash_lookup(editnurb->keyindex, cv);
}

static CVKeyIndex *popCVKeyIndex(EditNurb *editnurb, void *cv)
{
	return BLI_ghash_pop(editnurb->keyindex, cv, NULL);
}

static BezTriple *getKeyIndexOrig_bezt(EditNurb *editnurb, BezTriple *bezt)
{
	CVKeyIndex *index = getCVKeyIndex(editnurb, bezt);

	if (!index) {
		return NULL;
	}

	return (BezTriple *)index->orig_cv;
}

static BPoint *getKeyIndexOrig_bp(EditNurb *editnurb, BPoint *bp)
{
	CVKeyIndex *index = getCVKeyIndex(editnurb, bp);

	if (!index) {
		return NULL;
	}

	return (BPoint *)index->orig_cv;
}

static int getKeyIndexOrig_keyIndex(EditNurb *editnurb, void *cv)
{
	CVKeyIndex *index = getCVKeyIndex(editnurb, cv);

	if (!index) {
		return -1;
	}

	return index->key_index;
}

static void keyIndex_delCV(EditNurb *editnurb, void *cv)
{
	if (!editnurb->keyindex) {
		return;
	}

	BLI_ghash_remove(editnurb->keyindex, cv, NULL, (GHashValFreeFP)MEM_freeN);
}

static void keyIndex_delBezt(EditNurb *editnurb, BezTriple *bezt)
{
	keyIndex_delCV(editnurb, bezt);
}

static void keyIndex_delBP(EditNurb *editnurb, BPoint *bp)
{
	keyIndex_delCV(editnurb, bp);
}

static void keyIndex_delNurb(EditNurb *editnurb, Nurb *nu)
{
	int a;

	if (!editnurb->keyindex) {
		return;
	}

	if (nu->bezt) {
		BezTriple *bezt = nu->bezt;
		a = nu->pntsu;

		while (a--) {
			BLI_ghash_remove(editnurb->keyindex, bezt, NULL, (GHashValFreeFP)MEM_freeN);
			bezt++;
		}
	}
	else {
		BPoint *bp = nu->bp;
		a = nu->pntsu * nu->pntsv;

		while (a--) {
			BLI_ghash_remove(editnurb->keyindex, bp, NULL, (GHashValFreeFP)MEM_freeN);
			bp++;
		}
	}
}

static void keyIndex_delNurbList(EditNurb *editnurb, ListBase *nubase)
{
	Nurb *nu = nubase->first;

	while (nu) {
		keyIndex_delNurb(editnurb, nu);

		nu = nu->next;
	}
}

static void keyIndex_updateCV(EditNurb *editnurb, char *cv,
                              char *newcv, int count, int size)
{
	int i;
	CVKeyIndex *index;

	if (editnurb->keyindex == NULL) {
		/* No shape keys - updating not needed */
		return;
	}

	for (i = 0; i < count; i++) {
		index = popCVKeyIndex(editnurb, cv);

		if (index) {
			BLI_ghash_insert(editnurb->keyindex, newcv, index);
		}

		newcv += size;
		cv += size;
	}
}

static void keyIndex_updateBezt(EditNurb *editnurb, BezTriple *bezt,
                                BezTriple *newbezt, int count)
{
	keyIndex_updateCV(editnurb, (char *)bezt, (char *)newbezt, count, sizeof(BezTriple));
}

static void keyIndex_updateBP(EditNurb *editnurb, BPoint *bp,
                              BPoint *newbp, int count)
{
	keyIndex_updateCV(editnurb, (char *)bp, (char *)newbp, count, sizeof(BPoint));
}

static void keyIndex_updateNurb(EditNurb *editnurb, Nurb *nu, Nurb *newnu)
{
	if (nu->bezt) {
		keyIndex_updateBezt(editnurb, nu->bezt, newnu->bezt, newnu->pntsu);
	}
	else {
		keyIndex_updateBP(editnurb, nu->bp, newnu->bp, newnu->pntsu * newnu->pntsv);
	}
}

static void keyIndex_swap(EditNurb *editnurb, void *a, void *b)
{
	CVKeyIndex *index1 = popCVKeyIndex(editnurb, a);
	CVKeyIndex *index2 = popCVKeyIndex(editnurb, b);

	if (index2) BLI_ghash_insert(editnurb->keyindex, a, index2);
	if (index1) BLI_ghash_insert(editnurb->keyindex, b, index1);
}

static void keyIndex_switchDirection(EditNurb *editnurb, Nurb *nu)
{
	int a;
	CVKeyIndex *index1, *index2;

	if (nu->bezt) {
		BezTriple *bezt1, *bezt2;

		a = nu->pntsu;

		bezt1 = nu->bezt;
		bezt2 = bezt1 + (a - 1);

		if (a & 1) ++a;

		a /= 2;

		while (a--) {
			index1 = getCVKeyIndex(editnurb, bezt1);
			index2 = getCVKeyIndex(editnurb, bezt2);

			if (index1) index1->switched = !index1->switched;

			if (bezt1 != bezt2) {
				keyIndex_swap(editnurb, bezt1, bezt2);

				if (index2) index2->switched = !index2->switched;
			}

			bezt1++;
			bezt2--;
		}
	}
	else {
		BPoint *bp1, *bp2;

		if (nu->pntsv == 1) {
			a = nu->pntsu;
			bp1 = nu->bp;
			bp2 = bp1 + (a - 1);
			a /= 2;
			while (bp1 != bp2 && a > 0) {
				index1 = getCVKeyIndex(editnurb, bp1);
				index2 = getCVKeyIndex(editnurb, bp2);

				if (index1) index1->switched = !index1->switched;

				if (bp1 != bp2) {
					if (index2) index2->switched = !index2->switched;

					keyIndex_swap(editnurb, bp1, bp2);
				}

				a--;
				bp1++;
				bp2--;
			}
		}
		else {
			int b;

			for (b = 0; b < nu->pntsv; b++) {

				bp1 = &nu->bp[b * nu->pntsu];
				a = nu->pntsu;
				bp2 = bp1 + (a - 1);
				a /= 2;

				while (bp1 != bp2 && a > 0) {
					index1 = getCVKeyIndex(editnurb, bp1);
					index2 = getCVKeyIndex(editnurb, bp2);

					if (index1) index1->switched = !index1->switched;

					if (bp1 != bp2) {
						if (index2) index2->switched = !index2->switched;

						keyIndex_swap(editnurb, bp1, bp2);
					}

					a--;
					bp1++;
					bp2--;
				}
			}

		}
	}
}

static void switch_keys_direction(Curve *cu, Nurb *actnu)
{
	KeyBlock *currkey;
	EditNurb *editnurb = cu->editnurb;
	ListBase *nubase = &editnurb->nurbs;
	Nurb *nu;
	float *fp;
	int a;

	currkey = cu->key->block.first;
	while (currkey) {
		fp = currkey->data;

		nu = nubase->first;
		while (nu) {
			if (nu->bezt) {
				BezTriple *bezt = nu->bezt;
				a = nu->pntsu;
				if (nu == actnu) {
					while (a--) {
						if (getKeyIndexOrig_bezt(editnurb, bezt)) {
							swap_v3_v3(fp, fp + 6);
							*(fp + 9) = -*(fp + 9);
							fp += 12;
						}
						bezt++;
					}
				}
				else fp += a * 12;
			}
			else {
				BPoint *bp = nu->bp;
				a = nu->pntsu * nu->pntsv;
				if (nu == actnu) {
					while (a--) {
						if (getKeyIndexOrig_bp(editnurb, bp)) {
							*(fp + 3) = -*(fp + 3);
							fp += 4;
						}
						bp++;
					}
				}
				else fp += a * 4;
			}

			nu = nu->next;
		}

		currkey = currkey->next;
	}
}

static void keyData_switchDirectionNurb(Curve *cu, Nurb *nu)
{
	EditNurb *editnurb = cu->editnurb;

	if (!editnurb->keyindex) {
		/* no shape keys - nothing to do */
		return;
	}

	keyIndex_switchDirection(editnurb, nu);
	if (cu->key)
		switch_keys_direction(cu, nu);
}

static GHash *dupli_keyIndexHash(GHash *keyindex)
{
	GHash *gh;
	GHashIterator *hashIter;

	gh = BLI_ghash_ptr_new("dupli_keyIndex gh");

	for (hashIter = BLI_ghashIterator_new(keyindex);
	     !BLI_ghashIterator_isDone(hashIter);
	     BLI_ghashIterator_step(hashIter))
	{
		void *cv = BLI_ghashIterator_getKey(hashIter);
		CVKeyIndex *index = BLI_ghashIterator_getValue(hashIter);
		CVKeyIndex *newIndex = MEM_callocN(sizeof(CVKeyIndex), "dupli_keyIndexHash index");

		memcpy(newIndex, index, sizeof(CVKeyIndex));

		BLI_ghash_insert(gh, cv, newIndex);
	}

	BLI_ghashIterator_free(hashIter);

	return gh;
}

static void key_to_bezt(float *key, BezTriple *basebezt, BezTriple *bezt)
{
	memcpy(bezt, basebezt, sizeof(BezTriple));
	memcpy(bezt->vec, key, sizeof(float) * 9);
	bezt->alfa = key[9];
}

static void bezt_to_key(BezTriple *bezt, float *key)
{
	memcpy(key, bezt->vec, sizeof(float) * 9);
	key[9] = bezt->alfa;
}

static void calc_keyHandles(ListBase *nurb, float *key)
{
	Nurb *nu;
	int a;
	float *fp = key;
	BezTriple *bezt;

	nu = nurb->first;
	while (nu) {
		if (nu->bezt) {
			BezTriple *prevp, *nextp;
			BezTriple cur, prev, next;
			float *startfp, *prevfp, *nextfp;

			bezt = nu->bezt;
			a = nu->pntsu;
			startfp = fp;

			if (nu->flagu & CU_NURB_CYCLIC) {
				prevp = bezt + (a - 1);
				prevfp = fp + (12 * (a - 1));
			}
			else {
				prevp = NULL;
				prevfp = NULL;
			}

			nextp = bezt + 1;
			nextfp = fp + 12;

			while (a--) {
				key_to_bezt(fp, bezt, &cur);

				if (nextp) key_to_bezt(nextfp, nextp, &next);
				if (prevp) key_to_bezt(prevfp, prevp, &prev);

				BKE_nurb_handle_calc(&cur, prevp ? &prev : NULL, nextp ? &next : NULL, 0);
				bezt_to_key(&cur, fp);

				prevp = bezt;
				prevfp = fp;
				if (a == 1) {
					if (nu->flagu & CU_NURB_CYCLIC) {
						nextp = nu->bezt;
						nextfp = startfp;
					}
					else {
						nextp = NULL;
						nextfp = NULL;
					}
				}
				else {
					nextp++;
					nextfp += 12;
				}

				bezt++;
				fp += 12;
			}
		}
		else {
			a = nu->pntsu * nu->pntsv;
			fp += a * 4;
		}

		nu = nu->next;
	}
}

static void calc_shapeKeys(Object *obedit)
{
	Curve *cu = (Curve *)obedit->data;

	/* are there keys? */
	if (cu->key) {
		int a, i;
		EditNurb *editnurb = cu->editnurb;
		KeyBlock *currkey;
		KeyBlock *actkey = BLI_findlink(&cu->key->block, editnurb->shapenr - 1);
		BezTriple *bezt, *oldbezt;
		BPoint *bp, *oldbp;
		Nurb *nu;
		int totvert = BKE_nurbList_verts_count(&editnurb->nurbs);

		float (*ofs)[3] = NULL;
		float *oldkey, *newkey, *ofp;

		/* editing the base key should update others */
		if (cu->key->type == KEY_RELATIVE) {
			int act_is_basis = 0;
			/* find if this key is a basis for any others */
			for (currkey = cu->key->block.first; currkey; currkey = currkey->next) {
				if (editnurb->shapenr - 1 == currkey->relative) {
					act_is_basis = 1;
					break;
				}
			}

			if (act_is_basis) { /* active key is a base */
				int totvec = 0;

				/* Calculate needed memory to store offset */
				nu = editnurb->nurbs.first;
				while (nu) {
					if (nu->bezt) {
						/* Three vects to store handles and one for alfa */
						totvec += nu->pntsu * 4;
					}
					else {
						totvec += 2 * nu->pntsu * nu->pntsv;
					}

					nu = nu->next;
				}

				ofs = MEM_callocN(sizeof(float) * 3 * totvec,  "currkey->data");
				nu = editnurb->nurbs.first;
				i = 0;
				while (nu) {
					if (nu->bezt) {
						bezt = nu->bezt;
						a = nu->pntsu;
						while (a--) {
							oldbezt = getKeyIndexOrig_bezt(editnurb, bezt);

							if (oldbezt) {
								int j;
								for (j = 0; j < 3; ++j) {
									sub_v3_v3v3(ofs[i], bezt->vec[j], oldbezt->vec[j]);
									i++;
								}
								ofs[i++][0] = bezt->alfa - oldbezt->alfa;
							}
							else {
								i += 4;
							}
							bezt++;
						}
					}
					else {
						bp = nu->bp;
						a = nu->pntsu * nu->pntsv;
						while (a--) {
							oldbp = getKeyIndexOrig_bp(editnurb, bp);
							if (oldbp) {
								sub_v3_v3v3(ofs[i], bp->vec, oldbp->vec);
								ofs[i + 1][0] = bp->alfa - oldbp->alfa;
							}
							i += 2;
							bp++;
						}
					}

					nu = nu->next;
				}
			}
		}

		currkey = cu->key->block.first;
		while (currkey) {
			int apply_offset = (ofs && (currkey != actkey) && (editnurb->shapenr - 1 == currkey->relative));

			float *fp = newkey = MEM_callocN(cu->key->elemsize * totvert,  "currkey->data");
			ofp = oldkey = currkey->data;

			nu = editnurb->nurbs.first;
			i = 0;
			while (nu) {
				if (currkey == actkey) {
					int restore = actkey != cu->key->refkey;

					if (nu->bezt) {
						bezt = nu->bezt;
						a = nu->pntsu;
						while (a--) {
							int j;
							oldbezt = getKeyIndexOrig_bezt(editnurb, bezt);

							for (j = 0; j < 3; ++j, ++i) {
								copy_v3_v3(fp, bezt->vec[j]);

								if (restore && oldbezt) {
									copy_v3_v3(bezt->vec[j], oldbezt->vec[j]);
								}

								fp += 3;
							}
							fp[0] = bezt->alfa;

							if (restore && oldbezt) {
								bezt->alfa = oldbezt->alfa;
							}

							fp += 3; ++i; /* alphas */
							bezt++;
						}
					}
					else {
						bp = nu->bp;
						a = nu->pntsu * nu->pntsv;
						while (a--) {
							oldbp = getKeyIndexOrig_bp(editnurb, bp);

							copy_v3_v3(fp, bp->vec);

							fp[3] = bp->alfa;

							if (restore && oldbp) {
								copy_v3_v3(bp->vec, oldbp->vec);
								bp->alfa = oldbp->alfa;
							}

							fp += 4;
							bp++;
							i += 2;
						}
					}
				}
				else {
					int index;
					float *curofp;

					if (oldkey) {
						if (nu->bezt) {
							bezt = nu->bezt;
							a = nu->pntsu;

							while (a--) {
								index = getKeyIndexOrig_keyIndex(editnurb, bezt);
								if (index >= 0) {
									int j;
									curofp = ofp + index;

									for (j = 0; j < 3; ++j, ++i) {
										copy_v3_v3(fp, curofp);

										if (apply_offset) {
											add_v3_v3(fp, ofs[i]);
										}

										fp += 3; curofp += 3;
									}
									fp[0] = curofp[0];

									if (apply_offset) {
										/* apply alfa offsets */
										add_v3_v3(fp, ofs[i]);
										i++;
									}

									fp += 3; /* alphas */
								}
								else {
									int j;
									for (j = 0; j < 3; ++j, ++i) {
										copy_v3_v3(fp, bezt->vec[j]);
										fp += 3;
									}
									fp[0] = bezt->alfa;

									fp += 3; /* alphas */
								}
								bezt++;
							}
						}
						else {
							bp = nu->bp;
							a = nu->pntsu * nu->pntsv;
							while (a--) {
								index = getKeyIndexOrig_keyIndex(editnurb, bp);

								if (index >= 0) {
									curofp = ofp + index;
									copy_v3_v3(fp, curofp);
									fp[3] = curofp[3];

									if (apply_offset) {
										add_v3_v3(fp, ofs[i]);
										fp[3] += ofs[i + 1][0];
									}
								}
								else {
									copy_v3_v3(fp, bp->vec);
									fp[3] = bp->alfa;
								}

								fp += 4;
								bp++;
								i += 2;
							}
						}
					}
				}

				nu = nu->next;
			}

			if (apply_offset) {
				/* handles could become malicious after offsets applying */
				calc_keyHandles(&editnurb->nurbs, newkey);
			}

			currkey->totelem = totvert;
			if (currkey->data) MEM_freeN(currkey->data);
			currkey->data = newkey;

			currkey = currkey->next;
		}

		if (ofs) MEM_freeN(ofs);
	}
}

/* ********************* Amimation data *************** */

static int curve_is_animated(Curve *cu)
{
	AnimData *ad = BKE_animdata_from_id(&cu->id);

	return ad && (ad->action || ad->drivers.first);
}

static void fcurve_path_rename(AnimData *ad, char *orig_rna_path, char *rna_path, ListBase *orig_curves, ListBase *curves)
{
	FCurve *fcu, *nfcu, *nextfcu;
	int len = strlen(orig_rna_path);

	fcu = orig_curves->first;
	while (fcu) {
		nextfcu = fcu->next;
		if (!strncmp(fcu->rna_path, orig_rna_path, len)) {
			char *spath, *suffix = fcu->rna_path + len;
			nfcu = copy_fcurve(fcu);
			spath = nfcu->rna_path;
			nfcu->rna_path = BLI_sprintfN("%s%s", rna_path, suffix);
			BLI_addtail(curves, nfcu);

			if (fcu->grp) {
				action_groups_remove_channel(ad->action, fcu);
				action_groups_add_channel(ad->action, fcu->grp, nfcu);
			}
			else if (ad->action && &ad->action->curves == orig_curves)
				BLI_remlink(&ad->action->curves, fcu);
			else
				BLI_remlink(&ad->drivers, fcu);

			free_fcurve(fcu);

			MEM_freeN(spath);
		}
		fcu = nextfcu;
	}
}

static void fcurve_remove(AnimData *ad, ListBase *orig_curves, FCurve *fcu)
{
	if (orig_curves == &ad->drivers) BLI_remlink(&ad->drivers, fcu);
	else action_groups_remove_channel(ad->action, fcu);

	free_fcurve(fcu);
}

static void curve_rename_fcurves(Curve *cu, ListBase *orig_curves)
{
	int nu_index = 0, a, pt_index;
	EditNurb *editnurb = cu->editnurb;
	Nurb *nu = editnurb->nurbs.first;
	CVKeyIndex *keyIndex;
	char rna_path[64], orig_rna_path[64];
	AnimData *ad = BKE_animdata_from_id(&cu->id);
	ListBase curves = {NULL, NULL};
	FCurve *fcu, *next;

	while (nu) {
		if (nu->bezt) {
			BezTriple *bezt = nu->bezt;
			a = nu->pntsu;
			pt_index = 0;

			while (a--) {
				keyIndex = getCVKeyIndex(editnurb, bezt);
				if (keyIndex) {
					BLI_snprintf(rna_path, sizeof(rna_path), "splines[%d].bezier_points[%d]", nu_index, pt_index);
					BLI_snprintf(orig_rna_path, sizeof(orig_rna_path), "splines[%d].bezier_points[%d]", keyIndex->nu_index, keyIndex->pt_index);

					if (keyIndex->switched) {
						char handle_path[64], orig_handle_path[64];
						BLI_snprintf(orig_handle_path, sizeof(orig_rna_path), "%s.handle_left", orig_rna_path);
						BLI_snprintf(handle_path, sizeof(rna_path), "%s.handle_right", rna_path);
						fcurve_path_rename(ad, orig_handle_path, handle_path, orig_curves, &curves);

						BLI_snprintf(orig_handle_path, sizeof(orig_rna_path), "%s.handle_right", orig_rna_path);
						BLI_snprintf(handle_path, sizeof(rna_path), "%s.handle_left", rna_path);
						fcurve_path_rename(ad, orig_handle_path, handle_path, orig_curves, &curves);
					}

					fcurve_path_rename(ad, orig_rna_path, rna_path, orig_curves, &curves);

					keyIndex->nu_index = nu_index;
					keyIndex->pt_index = pt_index;
				}

				bezt++;
				pt_index++;
			}
		}
		else {
			BPoint *bp = nu->bp;
			a = nu->pntsu * nu->pntsv;
			pt_index = 0;

			while (a--) {
				keyIndex = getCVKeyIndex(editnurb, bp);
				if (keyIndex) {
					BLI_snprintf(rna_path, sizeof(rna_path), "splines[%d].points[%d]", nu_index, pt_index);
					BLI_snprintf(orig_rna_path, sizeof(orig_rna_path), "splines[%d].points[%d]", keyIndex->nu_index, keyIndex->pt_index);
					fcurve_path_rename(ad, orig_rna_path, rna_path, orig_curves, &curves);

					keyIndex->nu_index = nu_index;
					keyIndex->pt_index = pt_index;
				}

				bp++;
				pt_index++;
			}
		}
		nu = nu->next;
		nu_index++;
	}

	/* remove paths for removed control points
	 * need this to make further step with copying non-cv related curves copying
	 * not touching cv's f-curves */
	for (fcu = orig_curves->first; fcu; fcu = next) {
		next = fcu->next;

		if (!strncmp(fcu->rna_path, "splines", 7)) {
			char *ch = strchr(fcu->rna_path, '.');

			if (ch && (!strncmp(ch, ".bezier_points", 14) || !strncmp(ch, ".points", 7)))
				fcurve_remove(ad, orig_curves, fcu);
		}
	}

	nu_index = 0;
	nu = editnurb->nurbs.first;
	while (nu) {
		keyIndex = NULL;
		if (nu->pntsu) {
			if (nu->bezt) keyIndex = getCVKeyIndex(editnurb, &nu->bezt[0]);
			else keyIndex = getCVKeyIndex(editnurb, &nu->bp[0]);
		}

		if (keyIndex) {
			BLI_snprintf(rna_path, sizeof(rna_path), "splines[%d]", nu_index);
			BLI_snprintf(orig_rna_path, sizeof(orig_rna_path), "splines[%d]", keyIndex->nu_index);
			fcurve_path_rename(ad, orig_rna_path, rna_path, orig_curves, &curves);
		}

		nu_index++;
		nu = nu->next;
	}

	/* the remainders in orig_curves can be copied back (like follow path) */
	/* (if it's not path to spline) */
	for (fcu = orig_curves->first; fcu; fcu = next) {
		next = fcu->next;

		if (!strncmp(fcu->rna_path, "splines", 7)) fcurve_remove(ad, orig_curves, fcu);
		else BLI_addtail(&curves, fcu);
	}

	*orig_curves = curves;
}

/* return 0 if animation data wasn't changed, 1 otherwise */
int ED_curve_updateAnimPaths(Curve *cu)
{
	AnimData *ad = BKE_animdata_from_id(&cu->id);

	if (!curve_is_animated(cu)) return 0;

	if (ad->action)
		curve_rename_fcurves(cu, &ad->action->curves);

	curve_rename_fcurves(cu, &ad->drivers);

	return 1;
}

/* ********************* LOAD and MAKE *************** */

/* load editNurb in object */
void load_editNurb(Object *obedit)
{
	ListBase *editnurb = object_editcurve_get(obedit);

	if (obedit == NULL) return;

	set_actNurb(obedit, NULL);

	if (ELEM(obedit->type, OB_CURVE, OB_SURF)) {
		Curve *cu = obedit->data;
		Nurb *nu, *newnu;
		ListBase newnurb = {NULL, NULL}, oldnurb = cu->nurb;

		for (nu = editnurb->first; nu; nu = nu->next) {
			newnu = BKE_nurb_duplicate(nu);
			BLI_addtail(&newnurb, newnu);

			if (nu->type == CU_NURBS) {
				BKE_nurb_order_clamp_u(nu);
			}
		}

		cu->nurb = newnurb;

		calc_shapeKeys(obedit);
		ED_curve_updateAnimPaths(obedit->data);

		BKE_nurbList_free(&oldnurb);
	}

	set_actNurb(obedit, NULL);
}

/* make copy in cu->editnurb */
void make_editNurb(Object *obedit)
{
	Curve *cu = (Curve *)obedit->data;
	EditNurb *editnurb = cu->editnurb;
	Nurb *nu, *newnu, *nu_act = NULL;
	KeyBlock *actkey;


	set_actNurb(obedit, NULL);

	if (ELEM(obedit->type, OB_CURVE, OB_SURF)) {
		actkey = ob_get_keyblock(obedit);

		if (actkey) {
			// XXX strcpy(G.editModeTitleExtra, "(Key) ");
			undo_editmode_clear();
			key_to_curve(actkey, cu, &cu->nurb);
		}

		if (editnurb) {
			BKE_nurbList_free(&editnurb->nurbs);
			BKE_curve_editNurb_keyIndex_free(editnurb);
			editnurb->keyindex = NULL;
		}
		else {
			editnurb = MEM_callocN(sizeof(EditNurb), "editnurb");
			cu->editnurb = editnurb;
		}

		nu = cu->nurb.first;
		cu->lastsel = NULL;   /* for select row */

		while (nu) {
			newnu = BKE_nurb_duplicate(nu);
			BKE_nurb_test2D(newnu); // after join, or any other creation of curve
			BLI_addtail(&editnurb->nurbs, newnu);

			if (nu_act == NULL && isNurbsel(nu)) {
				nu_act = newnu;
				set_actNurb(obedit, newnu);
			}

			nu = nu->next;
		}

		if (actkey)
			editnurb->shapenr = obedit->shapenr;

		/* animation could be added in editmode even if there was no animdata i
		 * object mode hence we always need CVs index be created */
		init_editNurb_keyIndex(editnurb, &cu->nurb);
	}
}

void free_editNurb(Object *obedit)
{
	Curve *cu = obedit->data;

	BKE_curve_editNurb_free(cu);
}

void CU_deselect_all(Object *obedit)
{
	ListBase *editnurb = object_editcurve_get(obedit);

	if (editnurb) {
		selectend_nurb(obedit, FIRST, 0, DESELECT); /* set first control points as unselected */
		select_adjacent_cp(editnurb, 1, 1, DESELECT); /* cascade selection */
	}
}

void CU_select_all(Object *obedit)
{
	ListBase *editnurb = object_editcurve_get(obedit);

	if (editnurb) {
		selectend_nurb(obedit, FIRST, 0, SELECT); /* set first control points as unselected */
		select_adjacent_cp(editnurb, 1, 1, SELECT); /* cascade selection */
	}
}

void CU_select_swap(Object *obedit)
{
	ListBase *editnurb = object_editcurve_get(obedit);

	if (editnurb) {
		Curve *cu = obedit->data;
		Nurb *nu;
		BPoint *bp;
		BezTriple *bezt;
		int a;

		cu->lastsel = NULL;

		for (nu = editnurb->first; nu; nu = nu->next) {
			if (nu->type == CU_BEZIER) {
				bezt = nu->bezt;
				a = nu->pntsu;
				while (a--) {
					if (bezt->hide == 0) {
						bezt->f2 ^= SELECT; /* always do the center point */
						if ((cu->drawflag & CU_HIDE_HANDLES) == 0) {
							bezt->f1 ^= SELECT;
							bezt->f3 ^= SELECT;
						}
					}
					bezt++;
				}
			}
			else {
				bp = nu->bp;
				a = nu->pntsu * nu->pntsv;
				while (a--) {
					swap_selection_bpoint(bp);
					bp++;
				}
			}
		}
	}
}

/******************** separate operator ***********************/

static int separate_exec(bContext *C, wmOperator *op)
{
	Main *bmain = CTX_data_main(C);
	Scene *scene = CTX_data_scene(C);
	Nurb *nu, *nu1;
	Object *oldob, *newob;
	Base *oldbase, *newbase;
	Curve *oldcu, *newcu;
	EditNurb *oldedit, *newedit;

	oldbase = CTX_data_active_base(C);
	oldob = oldbase->object;
	oldcu = oldob->data;
	oldedit = oldcu->editnurb;

	if (oldcu->key) {
		BKE_report(op->reports, RPT_ERROR, "Can't separate a curve with vertex keys");
		return OPERATOR_CANCELLED;
	}

	WM_cursor_wait(1);
	
	/* 1. duplicate the object and data */
	newbase = ED_object_add_duplicate(bmain, scene, oldbase, 0); /* 0 = fully linked */
	ED_base_object_select(newbase, BA_DESELECT);
	newob = newbase->object;

	newcu = newob->data = BKE_curve_copy(oldcu);
	newcu->editnurb = NULL;
	oldcu->id.us--; /* because new curve is a copy: reduce user count */

	/* 2. put new object in editmode and clear it */
	make_editNurb(newob);
	newedit = newcu->editnurb;
	BKE_nurbList_free(&newedit->nurbs);
	BKE_curve_editNurb_keyIndex_free(newedit);

	/* 3. move over parts from old object */
	for (nu = oldedit->nurbs.first; nu; nu = nu1) {
		nu1 = nu->next;

		if (isNurbsel(nu)) {
			BLI_remlink(&oldedit->nurbs, nu);
			BLI_addtail(&newedit->nurbs, nu);
		}
	}

	/* 4. put old object out of editmode */
	load_editNurb(newob);
	free_editNurb(newob);

	DAG_id_tag_update(&oldob->id, OB_RECALC_DATA);  /* this is the original one */
	DAG_id_tag_update(&newob->id, OB_RECALC_DATA);  /* this is the separated one */

	WM_event_add_notifier(C, NC_GEOM | ND_DATA, oldob->data);

	WM_cursor_wait(0);

	return OPERATOR_FINISHED;
}

void CURVE_OT_separate(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Separate";
	ot->idname = "CURVE_OT_separate";
	ot->description = "Separate (partly) selected curves or surfaces into a new object";
	
	/* api callbacks */
	ot->exec = separate_exec;
	ot->poll = ED_operator_editsurfcurve;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* ******************* FLAGS ********************* */

static short isNurbselUV(Nurb *nu, int *u, int *v, int flag)
{
	/* return u!=-1:     1 row in u-direction selected. U has value between 0-pntsv 
	 * return v!=-1: 1 column in v-direction selected. V has value between 0-pntsu
	 */
	BPoint *bp;
	int a, b, sel;

	*u = *v = -1;

	bp = nu->bp;
	for (b = 0; b < nu->pntsv; b++) {
		sel = 0;
		for (a = 0; a < nu->pntsu; a++, bp++) {
			if (bp->f1 & flag) sel++;
		}
		if (sel == nu->pntsu) {
			if (*u == -1) *u = b;
			else return 0;
		}
		else if (sel > 1) return 0;  /* because sel == 1 is still ok */
	}

	for (a = 0; a < nu->pntsu; a++) {
		sel = 0;
		bp = &nu->bp[a];
		for (b = 0; b < nu->pntsv; b++, bp += nu->pntsu) {
			if (bp->f1 & flag) sel++;
		}
		if (sel == nu->pntsv) {
			if (*v == -1) *v = a;
			else return 0;
		}
		else if (sel > 1) return 0;
	}

	if (*u == -1 && *v > -1) return 1;
	if (*v == -1 && *u > -1) return 1;
	return 0;
}

static void setflagsNurb(ListBase *editnurb, short flag)
{
	Nurb *nu;
	BezTriple *bezt;
	BPoint *bp;
	int a;

	for (nu = editnurb->first; nu; nu = nu->next) {
		if (nu->type == CU_BEZIER) {
			a = nu->pntsu;
			bezt = nu->bezt;
			while (a--) {
				bezt->f1 = bezt->f2 = bezt->f3 = flag;
				bezt++;
			}
		}
		else {
			a = nu->pntsu * nu->pntsv;
			bp = nu->bp;
			while (a--) {
				bp->f1 = flag;
				bp++;
			}
		}
	}
}

static void rotateflagNurb(ListBase *editnurb, short flag, float *cent, float rotmat[][3])
{
	/* all verts with (flag & 'flag') rotate */
	Nurb *nu;
	BPoint *bp;
	int a;

	for (nu = editnurb->first; nu; nu = nu->next) {
		if (nu->type == CU_NURBS) {
			bp = nu->bp;
			a = nu->pntsu * nu->pntsv;

			while (a--) {
				if (bp->f1 & flag) {
					sub_v3_v3(bp->vec, cent);
					mul_m3_v3(rotmat, bp->vec);
					add_v3_v3(bp->vec, cent);
				}
				bp++;
			}
		}
	}
}

static void translateflagNurb(ListBase *editnurb, short flag, const float vec[3])
{
	/* all verts with ('flag' & flag) translate */
	Nurb *nu;
	BezTriple *bezt;
	BPoint *bp;
	int a;

	for (nu = editnurb->first; nu; nu = nu->next) {
		if (nu->type == CU_BEZIER) {
			a = nu->pntsu;
			bezt = nu->bezt;
			while (a--) {
				if (bezt->f1 & flag) add_v3_v3(bezt->vec[0], vec);
				if (bezt->f2 & flag) add_v3_v3(bezt->vec[1], vec);
				if (bezt->f3 & flag) add_v3_v3(bezt->vec[2], vec);
				bezt++;
			}
		}
		else {
			a = nu->pntsu * nu->pntsv;
			bp = nu->bp;
			while (a--) {
				if (bp->f1 & flag) add_v3_v3(bp->vec, vec);
				bp++;
			}
		}

		BKE_nurb_test2D(nu);
	}
}

static void weightflagNurb(ListBase *editnurb, short flag, float w)
{
	Nurb *nu;
	BPoint *bp;
	int a;

	for (nu = editnurb->first; nu; nu = nu->next) {
		if (nu->type == CU_NURBS) {
			a = nu->pntsu * nu->pntsv;
			bp = nu->bp;
			while (a--) {
				if (bp->f1 & flag) {
					/* a mode used to exist for replace/multiple but is was unused */
					bp->vec[3] *= w;
				}
				bp++;
			}
		}
	}
}

static int deleteflagNurb(bContext *C, wmOperator *UNUSED(op), int flag)
{
	Object *obedit = CTX_data_edit_object(C);
	Curve *cu = obedit->data;
	ListBase *editnurb = object_editcurve_get(obedit);
	Nurb *nu, *next;
	BPoint *bp, *bpn, *newbp;
	int a, b, newu, newv, sel;

	if (obedit->type == OB_SURF) ;
	else return OPERATOR_CANCELLED;

	cu->lastsel = NULL;

	nu = editnurb->first;
	while (nu) {
		next = nu->next;

		/* is entire nurb selected */
		bp = nu->bp;
		a = nu->pntsu * nu->pntsv;
		while (a) {
			a--;
			if (bp->f1 & flag) ;
			else break;
			bp++;
		}
		if (a == 0) {
			BLI_remlink(editnurb, nu);
			keyIndex_delNurb(cu->editnurb, nu);
			BKE_nurb_free(nu); nu = NULL;
		}
		else {
			/* is nurb in U direction selected */
			newv = nu->pntsv;
			bp = nu->bp;
			for (b = 0; b < nu->pntsv; b++) {
				sel = 0;
				for (a = 0; a < nu->pntsu; a++, bp++) {
					if (bp->f1 & flag) sel++;
				}
				if (sel == nu->pntsu) {
					newv--;
				}
				else if (sel >= 1) {
					/* don't delete */
					break;
				}
			}
			if (newv != nu->pntsv && b == nu->pntsv) {
				/* delete */
				bp = nu->bp;
				bpn = newbp = (BPoint *)MEM_mallocN(newv * nu->pntsu * sizeof(BPoint), "deleteNurb");
				for (b = 0; b < nu->pntsv; b++) {
					if ((bp->f1 & flag) == 0) {
						memcpy(bpn, bp, nu->pntsu * sizeof(BPoint));
						keyIndex_updateBP(cu->editnurb, bp, bpn, nu->pntsu);
						bpn += nu->pntsu;
					}
					else {
						keyIndex_delBP(cu->editnurb, bp);
					}
					bp += nu->pntsu;
				}
				nu->pntsv = newv;
				MEM_freeN(nu->bp);
				nu->bp = newbp;
				BKE_nurb_order_clamp_v(nu);

				BKE_nurb_knot_calc_v(nu);
			}
			else {
				/* is the nurb in V direction selected */
				newu = nu->pntsu;
				for (a = 0; a < nu->pntsu; a++) {
					bp = &nu->bp[a];
					sel = 0;
					for (b = 0; b < nu->pntsv; b++, bp += nu->pntsu) {
						if (bp->f1 & flag) sel++;
					}
					if (sel == nu->pntsv) {
						newu--;
					}
					else if (sel >= 1) {
						/* don't delete */
						break;
					}
				}
				if (newu != nu->pntsu && a == nu->pntsu) {
					/* delete */
					bp = nu->bp;
					bpn = newbp = (BPoint *)MEM_mallocN(newu * nu->pntsv * sizeof(BPoint), "deleteNurb");
					for (b = 0; b < nu->pntsv; b++) {
						for (a = 0; a < nu->pntsu; a++, bp++) {
							if ((bp->f1 & flag) == 0) {
								*bpn = *bp;
								keyIndex_updateBP(cu->editnurb, bp, bpn, 1);
								bpn++;
							}
							else {
								keyIndex_delBP(cu->editnurb, bp);
							}
						}
					}
					MEM_freeN(nu->bp);
					nu->bp = newbp;
					if (newu == 1 && nu->pntsv > 1) {    /* make a U spline */
						nu->pntsu = nu->pntsv;
						nu->pntsv = 1;
						SWAP(short, nu->orderu, nu->orderv);
						BKE_nurb_order_clamp_u(nu);
						if (nu->knotsv) MEM_freeN(nu->knotsv);
						nu->knotsv = NULL;
					}
					else {
						nu->pntsu = newu;
						BKE_nurb_order_clamp_u(nu);
					}
					BKE_nurb_knot_calc_u(nu);
				}
			}
		}
		nu = next;
	}

	if (ED_curve_updateAnimPaths(obedit->data))
		WM_event_add_notifier(C, NC_OBJECT | ND_KEYS, obedit);

	return OPERATOR_FINISHED;
}

/* only for OB_SURF */
static short extrudeflagNurb(EditNurb *editnurb, int flag)
{
	Nurb *nu;
	BPoint *bp, *bpn, *newbp;
	int ok = 0, a, u, v, len;

	nu = editnurb->nurbs.first;
	while (nu) {

		if (nu->pntsv == 1) {
			bp = nu->bp;
			a = nu->pntsu;
			while (a) {
				if (bp->f1 & flag) ;
				else break;
				bp++;
				a--;
			}
			if (a == 0) {
				ok = 1;
				newbp = (BPoint *)MEM_mallocN(2 * nu->pntsu * sizeof(BPoint), "extrudeNurb1");
				ED_curve_bpcpy(editnurb, newbp, nu->bp, nu->pntsu);
				bp = newbp + nu->pntsu;
				ED_curve_bpcpy(editnurb, bp, nu->bp, nu->pntsu);
				MEM_freeN(nu->bp);
				nu->bp = newbp;
				a = nu->pntsu;
				while (a--) {
					select_bpoint(bp, SELECT, flag, HIDDEN);
					select_bpoint(newbp, DESELECT, flag, HIDDEN);
					bp++; 
					newbp++;
				}

				nu->pntsv = 2;
				nu->orderv = 2;
				BKE_nurb_knot_calc_v(nu);
			}
		}
		else {
			/* which row or column is selected */

			if (isNurbselUV(nu, &u, &v, flag) ) {

				/* deselect all */
				bp = nu->bp;
				a = nu->pntsu * nu->pntsv;
				while (a--) {
					select_bpoint(bp, DESELECT, flag, HIDDEN);
					bp++;
				}

				if (u == 0 || u == nu->pntsv - 1) {      /* row in u-direction selected */
					ok = 1;
					newbp = (BPoint *)MEM_mallocN(nu->pntsu * (nu->pntsv + 1) *
					                              sizeof(BPoint), "extrudeNurb1");
					if (u == 0) {
						len = nu->pntsv * nu->pntsu;
						ED_curve_bpcpy(editnurb, newbp + nu->pntsu, nu->bp, len);
						ED_curve_bpcpy(editnurb, newbp, nu->bp, nu->pntsu);
						bp = newbp;
					}
					else {
						len = nu->pntsv * nu->pntsu;
						ED_curve_bpcpy(editnurb, newbp, nu->bp, len);
						ED_curve_bpcpy(editnurb, newbp + len, &nu->bp[len - nu->pntsu], nu->pntsu);
						bp = newbp + len;
					}

					a = nu->pntsu;
					while (a--) {
						select_bpoint(bp, SELECT, flag, HIDDEN);
						bp++;
					}

					MEM_freeN(nu->bp);
					nu->bp = newbp;
					nu->pntsv++;
					BKE_nurb_knot_calc_v(nu);
				}
				else if (v == 0 || v == nu->pntsu - 1) {     /* column in v-direction selected */
					ok = 1;
					bpn = newbp = (BPoint *)MEM_mallocN((nu->pntsu + 1) * nu->pntsv * sizeof(BPoint), "extrudeNurb1");
					bp = nu->bp;

					for (a = 0; a < nu->pntsv; a++) {
						if (v == 0) {
							*bpn = *bp;
							bpn->f1 |= flag;
							bpn++;
						}
						ED_curve_bpcpy(editnurb, bpn, bp, nu->pntsu);
						bp += nu->pntsu;
						bpn += nu->pntsu;
						if (v == nu->pntsu - 1) {
							*bpn = *(bp - 1);
							bpn->f1 |= flag;
							bpn++;
						}
					}

					MEM_freeN(nu->bp);
					nu->bp = newbp;
					nu->pntsu++;
					BKE_nurb_knot_calc_u(nu);
				}
			}
		}
		nu = nu->next;
	}

	return ok;
}

static void adduplicateflagNurb(Object *obedit, short flag)
{
	ListBase *editnurb = object_editcurve_get(obedit);
	Nurb *nu, *newnu;
	BezTriple *bezt, *bezt1;
	BPoint *bp, *bp1;
	Curve *cu = (Curve *)obedit->data;
	int a, b, starta, enda, newu, newv;
	char *usel;

	cu->lastsel = NULL;

	nu = editnurb->last;
	while (nu) {
		if (nu->type == CU_BEZIER) {
			bezt = nu->bezt;
			for (a = 0; a < nu->pntsu; a++) {
				enda = -1;
				starta = a;
				while ( (bezt->f1 & flag) || (bezt->f2 & flag) || (bezt->f3 & flag) ) {
					select_beztriple(bezt, DESELECT, flag, HIDDEN);
					enda = a;
					if (a >= nu->pntsu - 1) break;
					a++;
					bezt++;
				}
				if (enda >= starta) {
					newnu = (Nurb *)MEM_mallocN(sizeof(Nurb), "adduplicateN");
					memcpy(newnu, nu, sizeof(Nurb));
					BLI_addtail(editnurb, newnu);
					set_actNurb(obedit, newnu);
					newnu->pntsu = enda - starta + 1;
					newnu->bezt = (BezTriple *)MEM_mallocN((enda - starta + 1) * sizeof(BezTriple), "adduplicateN");
					memcpy(newnu->bezt, &nu->bezt[starta], newnu->pntsu * sizeof(BezTriple));

					b = newnu->pntsu;
					bezt1 = newnu->bezt;
					while (b--) {
						select_beztriple(bezt1, SELECT, flag, HIDDEN);
						bezt1++;
					}

					if (nu->flagu & CU_NURB_CYCLIC) {
						if (starta != 0 || enda != nu->pntsu - 1) {
							newnu->flagu &= ~CU_NURB_CYCLIC;
						}
					}
				}
				bezt++;
			}
		}
		else if (nu->pntsv == 1) {    /* because UV Nurb has a different method for dupli */
			bp = nu->bp;
			for (a = 0; a < nu->pntsu; a++) {
				enda = -1;
				starta = a;
				while (bp->f1 & flag) {
					select_bpoint(bp, DESELECT, flag, HIDDEN);
					enda = a;
					if (a >= nu->pntsu - 1) break;
					a++;
					bp++;
				}
				if (enda >= starta) {
					newnu = (Nurb *)MEM_mallocN(sizeof(Nurb), "adduplicateN3");
					memcpy(newnu, nu, sizeof(Nurb));
					set_actNurb(obedit, newnu);
					BLI_addtail(editnurb, newnu);
					newnu->pntsu = enda - starta + 1;
					newnu->bp = (BPoint *)MEM_mallocN((enda - starta + 1) * sizeof(BPoint), "adduplicateN4");
					memcpy(newnu->bp, &nu->bp[starta], newnu->pntsu * sizeof(BPoint));

					b = newnu->pntsu;
					bp1 = newnu->bp;
					while (b--) {
						select_bpoint(bp1, SELECT, flag, HIDDEN);
						bp1++;
					}

					if (nu->flagu & CU_NURB_CYCLIC) {
						if (starta != 0 || enda != nu->pntsu - 1) {
							newnu->flagu &= ~CU_NURB_CYCLIC;
						}
					}

					/* knots */
					newnu->knotsu = NULL;
					BKE_nurb_knot_calc_u(newnu);
				}
				bp++;
			}
		}
		else {
			/* a rectangular area in nurb has to be selected */
			if (isNurbsel(nu)) {
				usel = MEM_callocN(nu->pntsu, "adduplicateN4");
				bp = nu->bp;
				for (a = 0; a < nu->pntsv; a++) {
					for (b = 0; b < nu->pntsu; b++, bp++) {
						if (bp->f1 & flag) usel[b]++;
					}
				}
				newu = 0;
				newv = 0;
				for (a = 0; a < nu->pntsu; a++) {
					if (usel[a]) {
						if (newv == 0 || usel[a] == newv) {
							newv = usel[a];
							newu++;
						}
						else {
							newv = 0;
							break;
						}
					}
				}
				if (newu == 0 || newv == 0) {
					if (G.debug & G_DEBUG)
						printf("Can't duplicate Nurb\n");
				}
				else {

					if (newu == 1) SWAP(short, newu, newv);

					newnu = (Nurb *)MEM_mallocN(sizeof(Nurb), "adduplicateN5");
					memcpy(newnu, nu, sizeof(Nurb));
					BLI_addtail(editnurb, newnu);
					set_actNurb(obedit, newnu);
					newnu->pntsu = newu;
					newnu->pntsv = newv;
					newnu->bp = (BPoint *)MEM_mallocN(newu * newv * sizeof(BPoint), "adduplicateN6");
					BKE_nurb_order_clamp_u(newnu);
					BKE_nurb_order_clamp_v(newnu);
					
					newnu->knotsu = newnu->knotsv = NULL;
					
					bp = newnu->bp;
					bp1 = nu->bp;
					for (a = 0; a < nu->pntsv; a++) {
						for (b = 0; b < nu->pntsu; b++, bp1++) {
							if (bp1->f1 & flag) {
								memcpy(bp, bp1, sizeof(BPoint));
								select_bpoint(bp1, DESELECT, flag, HIDDEN);
								bp++;
							}
						}
					}
					if (BKE_nurb_check_valid_u(newnu)) {
						if (nu->pntsu == newnu->pntsu && nu->knotsu) {
							newnu->knotsu = MEM_dupallocN(nu->knotsu);
						}
						else {
							BKE_nurb_knot_calc_u(newnu);
						}
					}
					if (BKE_nurb_check_valid_v(newnu)) {
						if (nu->pntsv == newnu->pntsv && nu->knotsv) {
							newnu->knotsv = MEM_dupallocN(nu->knotsv);
						}
						else {
							BKE_nurb_knot_calc_v(newnu);
						}
					}
				}
				MEM_freeN(usel);
			}
		}

		nu = nu->prev;
	}
	
	/* actnu changed */
}

/**************** switch direction operator ***************/

static int switch_direction_exec(bContext *C, wmOperator *UNUSED(op))
{
	Object *obedit = CTX_data_edit_object(C);
	Curve *cu = (Curve *)obedit->data;
	EditNurb *editnurb = cu->editnurb;
	Nurb *nu;

	for (nu = editnurb->nurbs.first; nu; nu = nu->next)
		if (isNurbsel(nu)) {
			BKE_nurb_direction_switch(nu);
			keyData_switchDirectionNurb(cu, nu);
		}

	if (ED_curve_updateAnimPaths(obedit->data))
		WM_event_add_notifier(C, NC_OBJECT | ND_KEYS, obedit);

	DAG_id_tag_update(obedit->data, 0);
	WM_event_add_notifier(C, NC_GEOM | ND_DATA, obedit->data);

	return OPERATOR_FINISHED;
}

void CURVE_OT_switch_direction(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Switch Direction";
	ot->description = "Switch direction of selected splines";
	ot->idname = "CURVE_OT_switch_direction";
	
	/* api callbacks */
	ot->exec = switch_direction_exec;
	ot->poll = ED_operator_editsurfcurve;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/****************** set weight operator *******************/

static int set_goal_weight_exec(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	ListBase *editnurb = object_editcurve_get(obedit);
	Nurb *nu;
	BezTriple *bezt;
	BPoint *bp;
	float weight = RNA_float_get(op->ptr, "weight");
	int a;
				
	for (nu = editnurb->first; nu; nu = nu->next) {
		if (nu->bezt) {
			for (bezt = nu->bezt, a = 0; a < nu->pntsu; a++, bezt++) {
				if (bezt->f2 & SELECT)
					bezt->weight = weight;
			}
		}
		else if (nu->bp) {
			for (bp = nu->bp, a = 0; a < nu->pntsu * nu->pntsv; a++, bp++) {
				if (bp->f1 & SELECT)
					bp->weight = weight;
			}
		}
	}	

	DAG_id_tag_update(obedit->data, 0);
	WM_event_add_notifier(C, NC_GEOM | ND_DATA, obedit->data);

	return OPERATOR_FINISHED;
}

void CURVE_OT_spline_weight_set(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Set Goal Weight";
	ot->description = "Set softbody goal weight for selected points";
	ot->idname = "CURVE_OT_spline_weight_set";
	
	/* api callbacks */
	ot->exec = set_goal_weight_exec;
	ot->invoke = WM_operator_props_popup;
	ot->poll = ED_operator_editsurfcurve;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* properties */
	RNA_def_float_factor(ot->srna, "weight", 1.0f, 0.0f, 1.0f, "Weight", "", 0.0f, 1.0f);
}

/******************* set radius operator ******************/

static int set_radius_exec(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	ListBase *editnurb = object_editcurve_get(obedit);
	Nurb *nu;
	BezTriple *bezt;
	BPoint *bp;
	float radius = RNA_float_get(op->ptr, "radius");
	int a;
	
	for (nu = editnurb->first; nu; nu = nu->next) {
		if (nu->bezt) {
			for (bezt = nu->bezt, a = 0; a < nu->pntsu; a++, bezt++) {
				if (bezt->f2 & SELECT)
					bezt->radius = radius;
			}
		}
		else if (nu->bp) {
			for (bp = nu->bp, a = 0; a < nu->pntsu * nu->pntsv; a++, bp++) {
				if (bp->f1 & SELECT)
					bp->radius = radius;
			}
		}
	}	

	WM_event_add_notifier(C, NC_GEOM | ND_DATA, obedit->data);
	DAG_id_tag_update(obedit->data, 0);

	return OPERATOR_FINISHED;
}

void CURVE_OT_radius_set(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Set Curve Radius";
	ot->description = "Set per-point radius which is used for bevel tapering";
	ot->idname = "CURVE_OT_radius_set";
	
	/* api callbacks */
	ot->exec = set_radius_exec;
	ot->invoke = WM_operator_props_popup;
	ot->poll = ED_operator_editsurfcurve;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* properties */
	RNA_def_float(ot->srna, "radius", 1.0f, 0.0f, FLT_MAX, "Radius", "", 0.0001f, 10.0f);
}

/********************* smooth operator ********************/

static int smooth_exec(bContext *C, wmOperator *UNUSED(op))
{
	Object *obedit = CTX_data_edit_object(C);
	ListBase *editnurb = object_editcurve_get(obedit);
	Nurb *nu;
	BezTriple *bezt, *beztOrig;
	BPoint *bp, *bpOrig;
	float val, newval, offset;
	int a, i, change = 0;
	
	for (nu = editnurb->first; nu; nu = nu->next) {
		if (nu->bezt) {
			change = 0;
			beztOrig = MEM_dupallocN(nu->bezt);
			for (bezt = &nu->bezt[1], a = 1; a < nu->pntsu - 1; a++, bezt++) {
				if (bezt->f2 & SELECT) {
					for (i = 0; i < 3; i++) {
						val = bezt->vec[1][i];
						newval = ((beztOrig + (a - 1))->vec[1][i] * 0.5f) + ((beztOrig + (a + 1))->vec[1][i] * 0.5f);
						offset = (val * ((1.0f / 6.0f) * 5.0f)) + (newval * (1.0f / 6.0f)) - val;
						/* offset handles */
						bezt->vec[1][i] += offset;
						bezt->vec[0][i] += offset;
						bezt->vec[2][i] += offset;
					}
					change = 1;
				}
			}
			MEM_freeN(beztOrig);
			if (change)
				BKE_nurb_handles_calc(nu);
		}
		else if (nu->bp) {
			bpOrig = MEM_dupallocN(nu->bp);
			/* Same as above, keep these the same! */
			for (bp = &nu->bp[1], a = 1; a < nu->pntsu - 1; a++, bp++) {
				if (bp->f1 & SELECT) {
					for (i = 0; i < 3; i++) {
						val = bp->vec[i];
						newval = ((bpOrig + (a - 1))->vec[i] * 0.5f) + ((bpOrig + (a + 1))->vec[i] * 0.5f);
						offset = (val * ((1.0f / 6.0f) * 5.0f)) + (newval * (1.0f / 6.0f)) - val;
					
						bp->vec[i] += offset;
					}
				}
			}
			MEM_freeN(bpOrig);
		}
	}

	WM_event_add_notifier(C, NC_GEOM | ND_DATA, obedit->data);
	DAG_id_tag_update(obedit->data, 0);

	return OPERATOR_FINISHED;
}

void CURVE_OT_smooth(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Smooth";
	ot->description = "Flatten angles of selected points";
	ot->idname = "CURVE_OT_smooth";
	
	/* api callbacks */
	ot->exec = smooth_exec;
	ot->poll = ED_operator_editsurfcurve;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/**************** smooth curve radius operator *************/

/* TODO, make smoothing distance based */
static int smooth_radius_exec(bContext *C, wmOperator *UNUSED(op))
{
	Object *obedit = CTX_data_edit_object(C);
	ListBase *editnurb = object_editcurve_get(obedit);
	Nurb *nu;
	BezTriple *bezt;
	BPoint *bp;
	int a;
	
	/* use for smoothing */
	int last_sel;
	int start_sel, end_sel; /* selection indices, inclusive */
	float start_rad, end_rad, fac, range;
	
	for (nu = editnurb->first; nu; nu = nu->next) {
		if (nu->bezt) {
			
			for (last_sel = 0; last_sel < nu->pntsu; last_sel++) {
				/* loop over selection segments of a curve, smooth each */
				
				/* Start BezTriple code, this is duplicated below for points, make sure these functions stay in sync */
				start_sel = -1;
				for (bezt = &nu->bezt[last_sel], a = last_sel; a < nu->pntsu; a++, bezt++) {
					if (bezt->f2 & SELECT) {
						start_sel = a;
						break;
					}
				}
				/* in case there are no other selected verts */
				end_sel = start_sel;
				for (bezt = &nu->bezt[start_sel + 1], a = start_sel + 1; a < nu->pntsu; a++, bezt++) {
					if ((bezt->f2 & SELECT) == 0) {
						break;
					}
					end_sel = a;
				}
				
				if (start_sel == -1) {
					last_sel = nu->pntsu; /* next... */
				}
				else {
					last_sel = end_sel; /* before we modify it */
					
					/* now blend between start and end sel */
					start_rad = end_rad = -1.0;
					
					if (start_sel == end_sel) {
						/* simple, only 1 point selected */
						if (start_sel > 0)                         start_rad = nu->bezt[start_sel - 1].radius;
						if (end_sel != -1 && end_sel < nu->pntsu)  end_rad   = nu->bezt[start_sel + 1].radius;
						
						if      (start_rad >= 0.0f && end_rad >= 0.0f)  nu->bezt[start_sel].radius = (start_rad + end_rad) / 2.0f;
						else if (start_rad >= 0.0f)                     nu->bezt[start_sel].radius = start_rad;
						else if (end_rad >= 0.0f)                       nu->bezt[start_sel].radius = end_rad;
					}
					else {
						/* if endpoints selected, then use them */
						if (start_sel == 0) {
							start_rad = nu->bezt[start_sel].radius;
							start_sel++; /* we don't want to edit the selected endpoint */
						}
						else {
							start_rad = nu->bezt[start_sel - 1].radius;
						}
						if (end_sel == nu->pntsu - 1) {
							end_rad = nu->bezt[end_sel].radius;
							end_sel--; /* we don't want to edit the selected endpoint */
						}
						else {
							end_rad = nu->bezt[end_sel + 1].radius;
						}
						
						/* Now Blend between the points */
						range = (float)(end_sel - start_sel) + 2.0f;
						for (bezt = &nu->bezt[start_sel], a = start_sel; a <= end_sel; a++, bezt++) {
							fac = (float)(1 + a - start_sel) / range;
							bezt->radius = start_rad * (1.0f - fac) + end_rad * fac;
						}
					}
				}
			}
		}
		else if (nu->bp) {
			/* Same as above, keep these the same! */
			for (last_sel = 0; last_sel < nu->pntsu; last_sel++) {
				/* loop over selection segments of a curve, smooth each */
				
				/* Start BezTriple code, this is duplicated below for points, make sure these functions stay in sync */
				start_sel = -1;
				for (bp = &nu->bp[last_sel], a = last_sel; a < nu->pntsu; a++, bp++) {
					if (bp->f1 & SELECT) {
						start_sel = a;
						break;
					}
				}
				/* in case there are no other selected verts */
				end_sel = start_sel;
				for (bp = &nu->bp[start_sel + 1], a = start_sel + 1; a < nu->pntsu; a++, bp++) {
					if ((bp->f1 & SELECT) == 0) {
						break;
					}
					end_sel = a;
				}
				
				if (start_sel == -1) {
					last_sel = nu->pntsu; /* next... */
				}
				else {
					last_sel = end_sel; /* before we modify it */
					
					/* now blend between start and end sel */
					start_rad = end_rad = -1.0;
					
					if (start_sel == end_sel) {
						/* simple, only 1 point selected */
						if (start_sel > 0) start_rad = nu->bp[start_sel - 1].radius;
						if (end_sel != -1 && end_sel < nu->pntsu) end_rad = nu->bp[start_sel + 1].radius;
						
						if      (start_rad >= 0.0f && end_rad >= 0.0f)  nu->bp[start_sel].radius = (start_rad + end_rad) / 2;
						else if (start_rad >= 0.0f)                     nu->bp[start_sel].radius = start_rad;
						else if (end_rad >= 0.0f)                       nu->bp[start_sel].radius = end_rad;
					}
					else {
						/* if endpoints selected, then use them */
						if (start_sel == 0) {
							start_rad = nu->bp[start_sel].radius;
							start_sel++; /* we don't want to edit the selected endpoint */
						}
						else {
							start_rad = nu->bp[start_sel - 1].radius;
						}
						if (end_sel == nu->pntsu - 1) {
							end_rad = nu->bp[end_sel].radius;
							end_sel--; /* we don't want to edit the selected endpoint */
						}
						else {
							end_rad = nu->bp[end_sel + 1].radius;
						}
						
						/* Now Blend between the points */
						range = (float)(end_sel - start_sel) + 2.0f;
						for (bp = &nu->bp[start_sel], a = start_sel; a <= end_sel; a++, bp++) {
							fac = (float)(1 + a - start_sel) / range;
							bp->radius = start_rad * (1.0f - fac) + end_rad * fac;
						}
					}
				}
			}
		}
	}

	WM_event_add_notifier(C, NC_GEOM | ND_DATA, obedit->data);
	DAG_id_tag_update(obedit->data, 0);

	return OPERATOR_FINISHED;
}

void CURVE_OT_smooth_radius(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Smooth Curve Radius";
	ot->description = "Flatten radiuses of selected points";
	ot->idname = "CURVE_OT_smooth_radius";
	
	/* api clastbacks */
	ot->exec = smooth_radius_exec;
	ot->poll = ED_operator_editsurfcurve;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/***************** selection utility *************************/

/* next == 1 -> select next         */
/* next == -1 -> select previous    */
/* cont == 1 -> select continuously */
/* selstatus, inverts behavior		*/
static void select_adjacent_cp(ListBase *editnurb, short next, short cont, short selstatus)
{
	Nurb *nu;
	BezTriple *bezt;
	BPoint *bp;
	int a;
	short lastsel = 0;
	
	if (next == 0) return;
	
	for (nu = editnurb->first; nu; nu = nu->next) {
		lastsel = 0;
		if (nu->type == CU_BEZIER) {			
			a = nu->pntsu;
			bezt = nu->bezt;
			if (next < 0) bezt = &nu->bezt[a - 1];
			while (a--) {
				if (a - abs(next) < 0) break;
				if ((lastsel == 0) && (bezt->hide == 0) && ((bezt->f2 & SELECT) || (selstatus == 0))) {
					bezt += next;
					if (!(bezt->f2 & SELECT) || (selstatus == 0)) {
						short sel = select_beztriple(bezt, selstatus, 1, VISIBLE);
						if ((sel == 1) && (cont == 0)) lastsel = 1;
					}
				}
				else {
					bezt += next;
					lastsel = 0;
				}
				/* move around in zigzag way so that we go through each */				
				bezt -= (next - next / abs(next));
			}
		}
		else {
			a = nu->pntsu * nu->pntsv;
			bp = nu->bp;
			if (next < 0) bp = &nu->bp[a - 1];
			while (a--) {
				if (a - abs(next) < 0) break;
				if ((lastsel == 0) && (bp->hide == 0) && ((bp->f1 & SELECT) || (selstatus == 0))) {
					bp += next;
					if (!(bp->f1 & SELECT) || (selstatus == 0)) {
						short sel = select_bpoint(bp, selstatus, 1, VISIBLE);
						if ((sel == 1) && (cont == 0)) lastsel = 1;
					}			
				}
				else {
					bp += next;
					lastsel = 0;
				}
				/* move around in zigzag way so that we go through each */
				bp -= (next - next / abs(next));
			}
		}
	}
}

/**************** select start/end operators **************/

/* (de)selects first or last of visible part of each Nurb depending on selFirst     */
/* selFirst: defines the end of which to select					    */
/* doswap: defines if selection state of each first/last control point is swapped   */
/* selstatus: selection status in case doswap is false				    */
void selectend_nurb(Object *obedit, short selfirst, short doswap, short selstatus)
{
	ListBase *editnurb = object_editcurve_get(obedit);
	Nurb *nu;
	BPoint *bp;
	BezTriple *bezt;
	Curve *cu;
	int a;

	if (obedit == NULL) return;

	cu = (Curve *)obedit->data;
	cu->lastsel = NULL;

	for (nu = editnurb->first; nu; nu = nu->next) {
		if (nu->type == CU_BEZIER) {
			a = nu->pntsu;
			
			/* which point? */
			if (selfirst == 0) { /* select last */
				bezt = &nu->bezt[a - 1];
			}
			else { /* select first */
				bezt = nu->bezt;
			}
			
			while (a--) {
				short sel;
				if (doswap) sel = swap_selection_beztriple(bezt);
				else sel = select_beztriple(bezt, selstatus, 1, VISIBLE);
				
				if (sel == 1) break;
			}
		}
		else {
			a = nu->pntsu * nu->pntsv;
			
			/* which point? */
			if (selfirst == 0) { /* select last */
				bp = &nu->bp[a - 1];
			}
			else { /* select first */
				bp = nu->bp;
			}

			while (a--) {
				if (bp->hide == 0) {
					short sel;
					if (doswap) sel = swap_selection_bpoint(bp);
					else sel = select_bpoint(bp, selstatus, 1, VISIBLE);
					
					if (sel == 1) break;
				}
			}
		}
	}
}

static int de_select_first_exec(bContext *C, wmOperator *UNUSED(op))
{
	Object *obedit = CTX_data_edit_object(C);

	selectend_nurb(obedit, FIRST, 1, DESELECT);
	WM_event_add_notifier(C, NC_GEOM | ND_SELECT, obedit->data);

	return OPERATOR_FINISHED;
}

void CURVE_OT_de_select_first(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "(De)select First";
	ot->idname = "CURVE_OT_de_select_first";
	ot->description = "(De)select first of visible part of each Nurb";
	
	/* api cfirstbacks */
	ot->exec = de_select_first_exec;
	ot->poll = ED_operator_editcurve;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static int de_select_last_exec(bContext *C, wmOperator *UNUSED(op))
{
	Object *obedit = CTX_data_edit_object(C);

	selectend_nurb(obedit, LAST, 1, DESELECT);
	WM_event_add_notifier(C, NC_GEOM | ND_SELECT, obedit->data);

	return OPERATOR_FINISHED;
}

void CURVE_OT_de_select_last(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "(De)select Last";
	ot->idname = "CURVE_OT_de_select_last";
	ot->description = "(De)select last of visible part of each Nurb";
	
	/* api clastbacks */
	ot->exec = de_select_last_exec;
	ot->poll = ED_operator_editcurve;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/******************* de select all operator ***************/

static short nurb_has_selected_cps(ListBase *editnurb)
{
	Nurb *nu;
	BezTriple *bezt;
	BPoint *bp;
	int a;

	for (nu = editnurb->first; nu; nu = nu->next) {
		if (nu->type == CU_BEZIER) {
			a = nu->pntsu;
			bezt = nu->bezt;
			while (a--) {
				if (bezt->hide == 0) {
					if ((bezt->f1 & SELECT) ||
					    (bezt->f2 & SELECT) ||
					    (bezt->f3 & SELECT))
					{
						return 1;
					}
				}
				bezt++;
			}
		}
		else {
			a = nu->pntsu * nu->pntsv;
			bp = nu->bp;
			while (a--) {
				if ((bp->hide == 0) && (bp->f1 & SELECT)) return 1;
				bp++;
			}
		}
	}
	
	return 0;
}

static int de_select_all_exec(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	ListBase *editnurb = object_editcurve_get(obedit);
	int action = RNA_enum_get(op->ptr, "action");

	if (action == SEL_TOGGLE) {
		action = SEL_SELECT;
		if (nurb_has_selected_cps(editnurb))
			action = SEL_DESELECT;
	}

	switch (action) {
		case SEL_SELECT:
			CU_select_all(obedit);
			break;
		case SEL_DESELECT:
			CU_deselect_all(obedit);
			break;
		case SEL_INVERT:
			CU_select_swap(obedit);
			break;
	}
	
	WM_event_add_notifier(C, NC_GEOM | ND_SELECT, obedit->data);

	return OPERATOR_FINISHED;
}

void CURVE_OT_select_all(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "(De)select All";
	ot->idname = "CURVE_OT_select_all";
	ot->description = "(De)select all control points";
	
	/* api callbacks */
	ot->exec = de_select_all_exec;
	ot->poll = ED_operator_editsurfcurve;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* properties */
	WM_operator_properties_select_all(ot);
}

/********************** hide operator *********************/

static int hide_exec(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	Curve *cu = obedit->data;
	ListBase *editnurb = object_editcurve_get(obedit);
	Nurb *nu;
	BPoint *bp;
	BezTriple *bezt;
	int a, sel, invert = RNA_boolean_get(op->ptr, "unselected");

	for (nu = editnurb->first; nu; nu = nu->next) {
		if (nu->type == CU_BEZIER) {
			bezt = nu->bezt;
			a = nu->pntsu;
			sel = 0;
			while (a--) {
				if (invert == 0 && BEZSELECTED_HIDDENHANDLES(cu, bezt)) {
					select_beztriple(bezt, DESELECT, 1, HIDDEN);
					bezt->hide = 1;
				}
				else if (invert && !BEZSELECTED_HIDDENHANDLES(cu, bezt)) {
					select_beztriple(bezt, DESELECT, 1, HIDDEN);
					bezt->hide = 1;
				}
				if (bezt->hide) sel++;
				bezt++;
			}
			if (sel == nu->pntsu) nu->hide = 1;
		}
		else {
			bp = nu->bp;
			a = nu->pntsu * nu->pntsv;
			sel = 0;
			while (a--) {
				if (invert == 0 && (bp->f1 & SELECT)) {
					select_bpoint(bp, DESELECT, 1, HIDDEN);
					bp->hide = 1;
				}
				else if (invert && (bp->f1 & SELECT) == 0) {
					select_bpoint(bp, DESELECT, 1, HIDDEN);
					bp->hide = 1;
				}
				if (bp->hide) sel++;
				bp++;
			}
			if (sel == nu->pntsu * nu->pntsv) nu->hide = 1;
		}
	}

	DAG_id_tag_update(obedit->data, 0);
	WM_event_add_notifier(C, NC_GEOM | ND_SELECT, obedit->data);

	return OPERATOR_FINISHED;	
}

void CURVE_OT_hide(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Hide Selected";
	ot->idname = "CURVE_OT_hide";
	ot->description = "Hide (un)selected control points";
	
	/* api callbacks */
	ot->exec = hide_exec;
	ot->poll = ED_operator_editsurfcurve;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	/* props */
	RNA_def_boolean(ot->srna, "unselected", 0, "Unselected", "Hide unselected rather than selected");
}

/********************** reveal operator *********************/

static int reveal_exec(bContext *C, wmOperator *UNUSED(op))
{
	Object *obedit = CTX_data_edit_object(C);
	ListBase *editnurb = object_editcurve_get(obedit);
	Nurb *nu;
	BPoint *bp;
	BezTriple *bezt;
	int a;

	for (nu = editnurb->first; nu; nu = nu->next) {
		nu->hide = 0;
		if (nu->type == CU_BEZIER) {
			bezt = nu->bezt;
			a = nu->pntsu;
			while (a--) {
				if (bezt->hide) {
					select_beztriple(bezt, SELECT, 1, HIDDEN);
					bezt->hide = 0;
				}
				bezt++;
			}
		}
		else {
			bp = nu->bp;
			a = nu->pntsu * nu->pntsv;
			while (a--) {
				if (bp->hide) {
					select_bpoint(bp, SELECT, 1, HIDDEN);
					bp->hide = 0;
				}
				bp++;
			}
		}
	}

	DAG_id_tag_update(obedit->data, 0);
	WM_event_add_notifier(C, NC_GEOM | ND_SELECT, obedit->data);

	return OPERATOR_FINISHED;	
}

void CURVE_OT_reveal(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Reveal Hidden";
	ot->idname = "CURVE_OT_reveal";
	ot->description = "Show again hidden control points";
	
	/* api callbacks */
	ot->exec = reveal_exec;
	ot->poll = ED_operator_editsurfcurve;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/********************** subdivide operator *********************/

/** Divide the line segments associated with the currently selected
 * curve nodes (Bezier or NURB). If there are no valid segment
 * selections within the current selection, nothing happens.
 */
static void subdividenurb(Object *obedit, int number_cuts)
{
	Curve *cu = obedit->data;
	EditNurb *editnurb = cu->editnurb;
	Nurb *nu;
	BezTriple *prevbezt, *bezt, *beztnew, *beztn;
	BPoint *bp, *prevbp, *bpnew, *bpn;
	float vec[15];
	int a, b, sel, amount, *usel, *vsel, i;
	float factor;

	// printf("*** subdivideNurb: entering subdivide\n");

	for (nu = editnurb->nurbs.first; nu; nu = nu->next) {
		amount = 0;
		if (nu->type == CU_BEZIER) {
			/*
			 * Insert a point into a 2D Bezier curve.
			 * Endpoints are preserved. Otherwise, all selected and inserted points are
			 * newly created. Old points are discarded.
			 */
			/* count */
			if (nu->flagu & CU_NURB_CYCLIC) {
				a = nu->pntsu;
				bezt = nu->bezt;
				prevbezt = bezt + (a - 1);
			}
			else {
				a = nu->pntsu - 1;
				prevbezt = nu->bezt;
				bezt = prevbezt + 1;
			}
			while (a--) {
				if (BEZSELECTED_HIDDENHANDLES(cu, prevbezt) && BEZSELECTED_HIDDENHANDLES(cu, bezt) ) amount += number_cuts;
				prevbezt = bezt;
				bezt++;
			}

			if (amount) {
				/* insert */
				beztnew = (BezTriple *)MEM_mallocN((amount + nu->pntsu) * sizeof(BezTriple), "subdivNurb");
				beztn = beztnew;
				if (nu->flagu & CU_NURB_CYCLIC) {
					a = nu->pntsu;
					bezt = nu->bezt;
					prevbezt = bezt + (a - 1);
				}
				else {
					a = nu->pntsu - 1;
					prevbezt = nu->bezt;
					bezt = prevbezt + 1;
				}
				while (a--) {
					memcpy(beztn, prevbezt, sizeof(BezTriple));
					keyIndex_updateBezt(editnurb, prevbezt, beztn, 1);
					beztn++;

					if (BEZSELECTED_HIDDENHANDLES(cu, prevbezt) && BEZSELECTED_HIDDENHANDLES(cu, bezt) ) {
						float prevvec[3][3];

						memcpy(prevvec, prevbezt->vec, sizeof(float) * 9);

						for (i = 0; i < number_cuts; i++) {
							factor = 1.0f / (number_cuts + 1 - i);

							memcpy(beztn, bezt, sizeof(BezTriple));

							/* midpoint subdividing */
							interp_v3_v3v3(vec, prevvec[1], prevvec[2], factor);
							interp_v3_v3v3(vec + 3, prevvec[2], bezt->vec[0], factor);
							interp_v3_v3v3(vec + 6, bezt->vec[0], bezt->vec[1], factor);

							interp_v3_v3v3(vec + 9, vec, vec + 3, factor);
							interp_v3_v3v3(vec + 12, vec + 3, vec + 6, factor);

							/* change handle of prev beztn */
							copy_v3_v3((beztn - 1)->vec[2], vec);
							/* new point */
							copy_v3_v3(beztn->vec[0], vec + 9);
							interp_v3_v3v3(beztn->vec[1], vec + 9, vec + 12, factor);
							copy_v3_v3(beztn->vec[2], vec + 12);
							/* handle of next bezt */
							if (a == 0 && i == number_cuts - 1 && (nu->flagu & CU_NURB_CYCLIC)) { copy_v3_v3(beztnew->vec[0], vec + 6); }
							else                                                                { copy_v3_v3(bezt->vec[0], vec + 6); }

							beztn->radius = (prevbezt->radius + bezt->radius) / 2;
							beztn->weight = (prevbezt->weight + bezt->weight) / 2;

							memcpy(prevvec, beztn->vec, sizeof(float) * 9);
							beztn++;
						}
					}

					prevbezt = bezt;
					bezt++;
				}
				/* last point */
				if ((nu->flagu & CU_NURB_CYCLIC) == 0) {
					memcpy(beztn, prevbezt, sizeof(BezTriple));
					keyIndex_updateBezt(editnurb, prevbezt, beztn, 1);
				}

				MEM_freeN(nu->bezt);
				nu->bezt = beztnew;
				nu->pntsu += amount;

				BKE_nurb_handles_calc(nu);
			}
		} /* End of 'if (nu->type == CU_BEZIER)' */
		else if (nu->pntsv == 1) {
			/*
			 * All flat lines (ie. co-planar), except flat Nurbs. Flat NURB curves
			 * are handled together with the regular NURB plane division, as it
			 * should be. I split it off just now, let's see if it is
			 * stable... nzc 30-5-'00
			 */
			/* count */
			if (nu->flagu & CU_NURB_CYCLIC) {
				a = nu->pntsu;
				bp = nu->bp;
				prevbp = bp + (a - 1);
			}
			else {
				a = nu->pntsu - 1;
				prevbp = nu->bp;
				bp = prevbp + 1;
			}
			while (a--) {
				if ( (bp->f1 & SELECT) && (prevbp->f1 & SELECT) ) amount += number_cuts;
				prevbp = bp;
				bp++;
			}

			if (amount) {
				/* insert */
				bpnew = (BPoint *)MEM_mallocN((amount + nu->pntsu) * sizeof(BPoint), "subdivNurb2");
				bpn = bpnew;

				if (nu->flagu & CU_NURB_CYCLIC) {
					a = nu->pntsu;
					bp = nu->bp;
					prevbp = bp + (a - 1);
				}
				else {
					a = nu->pntsu - 1;
					prevbp = nu->bp;
					bp = prevbp + 1;
				}
				while (a--) {
					memcpy(bpn, prevbp, sizeof(BPoint));
					keyIndex_updateBP(editnurb, prevbp, bpn, 1);
					bpn++;

					if ((bp->f1 & SELECT) && (prevbp->f1 & SELECT)) {
						// printf("*** subdivideNurb: insert 'linear' point\n");
						for (i = 0; i < number_cuts; i++) {
							factor = (float)(i + 1) / (number_cuts + 1);

							memcpy(bpn, bp, sizeof(BPoint));
							interp_v4_v4v4(bpn->vec, prevbp->vec, bp->vec, factor);
							bpn++;
						}

					}
					prevbp = bp;
					bp++;
				}
				if ((nu->flagu & CU_NURB_CYCLIC) == 0) { /* last point */
					memcpy(bpn, prevbp, sizeof(BPoint));
					keyIndex_updateBP(editnurb, prevbp, bpn, 1);
				}

				MEM_freeN(nu->bp);
				nu->bp = bpnew;
				nu->pntsu += amount;

				if (nu->type & CU_NURBS) {
					BKE_nurb_knot_calc_u(nu);
				}
			}
		} /* End of 'else if (nu->pntsv == 1)' */
		else if (nu->type == CU_NURBS) {
			/* This is a very strange test ... */
			/**
			 * Subdivide NURB surfaces - nzc 30-5-'00 -
			 *
			 * Subdivision of a NURB curve can be effected by adding a
			 * control point (insertion of a knot), or by raising the
			 * degree of the functions used to build the NURB. The
			 * expression
			 *
			 *     degree = #knots - #controlpoints + 1 (J Walter piece)
			 *     degree = #knots - #controlpoints     (Blender
			 *                                           implementation)
			 *       ( this is confusing.... what is true? Another concern
			 *       is that the JW piece allows the curve to become
			 *       explicitly 1st order derivative discontinuous, while
			 *       this is not what we want here... )
			 *
			 * is an invariant for a single NURB curve. Raising the degree
			 * of the NURB is done elsewhere; the degree is assumed
			 * constant during this operation. Degree is a property shared
			 * by all controlpoints in a curve (even though it is stored
			 * per control point - this can be misleading).
			 * Adding a knot is done by searching for the place in the
			 * knot vector where a certain knot value must be inserted, or
			 * by picking an appropriate knot value between two existing
			 * ones. The number of controlpoints that is influenced by the
			 * insertion depends on the order of the curve. A certain
			 * minimum number of knots is needed to form high-order
			 * curves, as can be seen from the equation above. In Blender,
			 * currently NURBs may be up to 6th order, so we modify at
			 * most 6 points. One point is added. For an n-degree curve,
			 * n points are discarded, and n+1 points inserted
			 * (so effectively, n points are modified).  (that holds for
			 * the JW piece, but it seems not for our NURBs)
			 * In practice, the knot spacing is copied, but the tail
			 * (the points following the insertion point) need to be
			 * offset to keep the knot series ascending. The knot series
			 * is always a series of monotonically ascending integers in
			 * Blender. When not enough control points are available to
			 * fit the order, duplicates of the endpoints are added as
			 * needed.
			 */
			/* selection-arrays */
			usel = MEM_callocN(sizeof(int) * nu->pntsu, "subivideNurb3");
			vsel = MEM_callocN(sizeof(int) * nu->pntsv, "subivideNurb3");
			sel = 0;

			/* Count the number of selected points. */
			bp = nu->bp;
			for (a = 0; a < nu->pntsv; a++) {
				for (b = 0; b < nu->pntsu; b++) {
					if (bp->f1 & SELECT) {
						usel[b]++;
						vsel[a]++;
						sel++;
					}
					bp++;
				}
			}
			if (sel == (nu->pntsu * nu->pntsv) ) {  /* subdivide entire nurb */
				/* Global subdivision is a special case of partial
				 * subdivision. Strange it is considered separately... */

				/* count of nodes (after subdivision) along U axis */
				int countu = nu->pntsu + (nu->pntsu - 1) * number_cuts;

				/* total count of nodes after subdivision */
				int tot = ((number_cuts + 1) * nu->pntsu - number_cuts) * ((number_cuts + 1) * nu->pntsv - number_cuts);

				bpn = bpnew = MEM_mallocN(tot * sizeof(BPoint), "subdivideNurb4");
				bp = nu->bp;
				/* first subdivide rows */
				for (a = 0; a < nu->pntsv; a++) {
					for (b = 0; b < nu->pntsu; b++) {
						*bpn = *bp;
						keyIndex_updateBP(editnurb, bp, bpn, 1);
						bpn++; 
						bp++;
						if (b < nu->pntsu - 1) {
							prevbp = bp - 1;
							for (i = 0; i < number_cuts; i++) {
								factor = (float)(i + 1) / (number_cuts + 1);
								*bpn = *bp;
								interp_v4_v4v4(bpn->vec, prevbp->vec, bp->vec, factor);
								bpn++;
							}
						}
					}
					bpn += number_cuts * countu;
				}
				/* now insert new */
				bpn = bpnew + ((number_cuts + 1) * nu->pntsu - number_cuts);
				bp = bpnew + (number_cuts + 1) * ((number_cuts + 1) * nu->pntsu - number_cuts);
				prevbp = bpnew;
				for (a = 1; a < nu->pntsv; a++) {

					for (b = 0; b < (number_cuts + 1) * nu->pntsu - number_cuts; b++) {
						BPoint *tmp = bpn;
						for (i = 0; i < number_cuts; i++) {
							factor = (float)(i + 1) / (number_cuts + 1);
							*tmp = *bp;
							interp_v4_v4v4(tmp->vec, prevbp->vec, bp->vec, factor);
							tmp += countu;
						}
						bp++; 
						prevbp++;
						bpn++;
					}
					bp += number_cuts * countu;
					bpn += number_cuts * countu;
					prevbp += number_cuts * countu;
				}
				MEM_freeN(nu->bp);
				nu->bp = bpnew;
				nu->pntsu = (number_cuts + 1) * nu->pntsu - number_cuts;
				nu->pntsv = (number_cuts + 1) * nu->pntsv - number_cuts;
				BKE_nurb_knot_calc_u(nu);
				BKE_nurb_knot_calc_v(nu);
			} /* End of 'if (sel == nu->pntsu*nu->pntsv)' (subdivide entire NURB) */
			else {
				/* subdivide in v direction? */
				sel = 0;
				for (a = 0; a < nu->pntsv - 1; a++) {
					if (vsel[a] == nu->pntsu && vsel[a + 1] == nu->pntsu) sel += number_cuts;
				}

				if (sel) {   /* V ! */
					bpn = bpnew = MEM_mallocN((sel + nu->pntsv) * nu->pntsu * sizeof(BPoint), "subdivideNurb4");
					bp = nu->bp;
					for (a = 0; a < nu->pntsv; a++) {
						for (b = 0; b < nu->pntsu; b++) {
							*bpn = *bp;
							keyIndex_updateBP(editnurb, bp, bpn, 1);
							bpn++;
							bp++;
						}
						if ( (a < nu->pntsv - 1) && vsel[a] == nu->pntsu && vsel[a + 1] == nu->pntsu) {
							for (i = 0; i < number_cuts; i++) {
								factor = (float)(i + 1) / (number_cuts + 1);
								prevbp = bp - nu->pntsu;
								for (b = 0; b < nu->pntsu; b++) {
									/*
									 * This simple bisection must be replaces by a
									 * subtle resampling of a number of points. Our
									 * task is made slightly easier because each
									 * point in our curve is a separate data
									 * node. (is it?)
									 */
									*bpn = *prevbp;
									interp_v4_v4v4(bpn->vec, prevbp->vec, bp->vec, factor);
									bpn++;

									prevbp++;
									bp++;
								}
								bp -= nu->pntsu;
							}
						}
					}
					MEM_freeN(nu->bp);
					nu->bp = bpnew;
					nu->pntsv += sel;
					BKE_nurb_knot_calc_v(nu);
				}
				else {
					/* or in u direction? */
					sel = 0;
					for (a = 0; a < nu->pntsu - 1; a++) {
						if (usel[a] == nu->pntsv && usel[a + 1] == nu->pntsv) sel += number_cuts;
					}

					if (sel) {  /* U ! */
						/* Inserting U points is sort of 'default' Flat curves only get */
						/* U points inserted in them.                                   */
						bpn = bpnew = MEM_mallocN((sel + nu->pntsu) * nu->pntsv * sizeof(BPoint), "subdivideNurb4");
						bp = nu->bp;
						for (a = 0; a < nu->pntsv; a++) {
							for (b = 0; b < nu->pntsu; b++) {
								*bpn = *bp;
								keyIndex_updateBP(editnurb, bp, bpn, 1);
								bpn++; 
								bp++;
								if ( (b < nu->pntsu - 1) && usel[b] == nu->pntsv && usel[b + 1] == nu->pntsv) {
									/*
									 * One thing that bugs me here is that the
									 * orders of things are not the same as in
									 * the JW piece. Also, this implies that we
									 * handle at most 3rd order curves? I miss
									 * some symmetry here...
									 */
									for (i = 0; i < number_cuts; i++) {
										factor = (float)(i + 1) / (number_cuts + 1);
										prevbp = bp - 1;
										*bpn = *prevbp;
										interp_v4_v4v4(bpn->vec, prevbp->vec, bp->vec, factor);
										bpn++;
									}
								}
							}
						}
						MEM_freeN(nu->bp);
						nu->bp = bpnew;
						nu->pntsu += sel;
						BKE_nurb_knot_calc_u(nu); /* shift knots forward */
					}
				}
			}
			MEM_freeN(usel); 
			MEM_freeN(vsel);

		} /* End of 'if (nu->type == CU_NURBS)'  */
	}
}

static int subdivide_exec(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	int number_cuts = RNA_int_get(op->ptr, "number_cuts");

	subdividenurb(obedit, number_cuts);

	if (ED_curve_updateAnimPaths(obedit->data))
		WM_event_add_notifier(C, NC_OBJECT | ND_KEYS, obedit);

	WM_event_add_notifier(C, NC_GEOM | ND_DATA, obedit->data);
	DAG_id_tag_update(obedit->data, 0);

	return OPERATOR_FINISHED;	
}

void CURVE_OT_subdivide(wmOperatorType *ot)
{
	PropertyRNA *prop;

	/* identifiers */
	ot->name = "Subdivide";
	ot->description = "Subdivide selected segments";
	ot->idname = "CURVE_OT_subdivide";
	
	/* api callbacks */
	ot->exec = subdivide_exec;
	ot->poll = ED_operator_editsurfcurve;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	prop = RNA_def_int(ot->srna, "number_cuts", 1, 1, INT_MAX, "Number of cuts", "", 1, 10);
	/* avoid re-using last var because it can cause _very_ high poly meshes and annoy users (or worse crash) */
	RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

/******************** find nearest ************************/

static void findnearestNurbvert__doClosest(void *userData, Nurb *nu, BPoint *bp, BezTriple *bezt, int beztindex, int x, int y)
{
	struct { BPoint *bp; BezTriple *bezt; Nurb *nurb; int dist, hpoint, select, mval[2]; } *data = userData;

	short flag;
	short temp;

	if (bp) {
		flag = bp->f1;
	}
	else {
		if (beztindex == 0) {
			flag = bezt->f1;
		}
		else if (beztindex == 1) {
			flag = bezt->f2;
		}
		else {
			flag = bezt->f3;
		}
	}

	temp = abs(data->mval[0] - x) + abs(data->mval[1] - y);
	if ((flag & 1) == data->select) temp += 5;
	if (bezt && beztindex == 1) temp += 3;  /* middle points get a small disadvantage */

	if (temp < data->dist) {
		data->dist = temp;

		data->bp = bp;
		data->bezt = bezt;
		data->nurb = nu;
		data->hpoint = bezt ? beztindex : 0;
	}
}

static short findnearestNurbvert(ViewContext *vc, short sel, const int mval[2], Nurb **nurb, BezTriple **bezt, BPoint **bp)
{
	/* (sel == 1): selected gets a disadvantage */
	/* in nurb and bezt or bp the nearest is written */
	/* return 0 1 2: handlepunt */
	struct { BPoint *bp; BezTriple *bezt; Nurb *nurb; int dist, hpoint, select, mval[2]; } data = {NULL};

	data.dist = 100;
	data.hpoint = 0;
	data.select = sel;
	data.mval[0] = mval[0];
	data.mval[1] = mval[1];

	ED_view3d_init_mats_rv3d(vc->obedit, vc->rv3d);
	nurbs_foreachScreenVert(vc, findnearestNurbvert__doClosest, &data);

	*nurb = data.nurb;
	*bezt = data.bezt;
	*bp = data.bp;

	return data.hpoint;
}

static void findselectedNurbvert(ListBase *editnurb, Nurb **nu, BezTriple **bezt, BPoint **bp)
{
	/* in nu and (bezt or bp) selected are written if there's 1 sel.  */
	/* if more points selected in 1 spline: return only nu, bezt and bp are 0 */
	Nurb *nu1;
	BezTriple *bezt1;
	BPoint *bp1;
	int a;

	*nu = NULL;
	*bezt = NULL;
	*bp = NULL;
	for (nu1 = editnurb->first; nu1; nu1 = nu1->next) {
		if (nu1->type == CU_BEZIER) {
			bezt1 = nu1->bezt;
			a = nu1->pntsu;
			while (a--) {
				if ( (bezt1->f1 & SELECT) || (bezt1->f2 & SELECT) || (bezt1->f3 & SELECT) ) {
					if (*nu != NULL && *nu != nu1) {
						*nu = NULL;
						*bp = NULL;
						*bezt = NULL;
						return;
					}
					else if (*bezt || *bp) {
						*bp = NULL;
						*bezt = NULL;
					}
					else {
						*bezt = bezt1;
						*nu = nu1;
					}
				}
				bezt1++;
			}
		}
		else {
			bp1 = nu1->bp;
			a = nu1->pntsu * nu1->pntsv;
			while (a--) {
				if (bp1->f1 & SELECT) {
					if (*nu != NULL && *nu != nu1) {
						*bp = NULL;
						*bezt = NULL;
						*nu = NULL;
						return;
					}
					else if (*bezt || *bp) {
						*bp = NULL;
						*bezt = NULL;
					}
					else {
						*bp = bp1;
						*nu = nu1;
					}
				}
				bp1++;
			}
		}
	}
}

/***************** set spline type operator *******************/

static int convertspline(short type, Nurb *nu)
{
	BezTriple *bezt;
	BPoint *bp;
	int a, c, nr;

	if (nu->type == CU_POLY) {
		if (type == CU_BEZIER) {  /* to Bezier with vecthandles  */
			nr = nu->pntsu;
			bezt = (BezTriple *)MEM_callocN(nr * sizeof(BezTriple), "setsplinetype2");
			nu->bezt = bezt;
			a = nr;
			bp = nu->bp;
			while (a--) {
				copy_v3_v3(bezt->vec[1], bp->vec);
				bezt->f1 = bezt->f2 = bezt->f3 = bp->f1;
				bezt->h1 = bezt->h2 = HD_VECT;
				bezt->weight = bp->weight;
				bezt->radius = bp->radius;
				bp++;
				bezt++;
			}
			MEM_freeN(nu->bp);
			nu->bp = NULL;
			nu->pntsu = nr;
			nu->type = CU_BEZIER;
			BKE_nurb_handles_calc(nu);
		}
		else if (type == CU_NURBS) {
			nu->type = CU_NURBS;
			nu->orderu = 4;
			nu->flagu &= CU_NURB_CYCLIC; /* disable all flags except for cyclic */
			BKE_nurb_knot_calc_u(nu);
			a = nu->pntsu * nu->pntsv;
			bp = nu->bp;
			while (a--) {
				bp->vec[3] = 1.0;
				bp++;
			}
		}
	}
	else if (nu->type == CU_BEZIER) {   /* Bezier */
		if (type == CU_POLY || type == CU_NURBS) {
			nr = 3 * nu->pntsu;
			nu->bp = MEM_callocN(nr * sizeof(BPoint), "setsplinetype");
			a = nu->pntsu;
			bezt = nu->bezt;
			bp = nu->bp;
			while (a--) {
				if (type == CU_POLY && bezt->h1 == HD_VECT && bezt->h2 == HD_VECT) {
					/* vector handle becomes 1 poly vertice */
					copy_v3_v3(bp->vec, bezt->vec[1]);
					bp->vec[3] = 1.0;
					bp->f1 = bezt->f2;
					nr -= 2;
					bp->radius = bezt->radius;
					bp->weight = bezt->weight;
					bp++;
				}
				else {
					for (c = 0; c < 3; c++) {
						copy_v3_v3(bp->vec, bezt->vec[c]);
						bp->vec[3] = 1.0;
						if (c == 0) bp->f1 = bezt->f1;
						else if (c == 1) bp->f1 = bezt->f2;
						else bp->f1 = bezt->f3;
						bp->radius = bezt->radius;
						bp->weight = bezt->weight;
						bp++;
					}
				}
				bezt++;
			}
			MEM_freeN(nu->bezt); 
			nu->bezt = NULL;
			nu->pntsu = nr;
			nu->pntsv = 1;
			nu->orderu = 4;
			nu->orderv = 1;
			nu->type = type;

#if 0       /* UNUSED */
			if (nu->flagu & CU_NURB_CYCLIC) c = nu->orderu - 1;
			else c = 0;
#endif

			if (type == CU_NURBS) {
				nu->flagu &= CU_NURB_CYCLIC; /* disable all flags except for cyclic */
				nu->flagu |= CU_NURB_BEZIER;
				BKE_nurb_knot_calc_u(nu);
			}
		}
	}
	else if (nu->type == CU_NURBS) {
		if (type == CU_POLY) {
			nu->type = CU_POLY;
			if (nu->knotsu) MEM_freeN(nu->knotsu);  /* python created nurbs have a knotsu of zero */
			nu->knotsu = NULL;
			if (nu->knotsv) MEM_freeN(nu->knotsv);
			nu->knotsv = NULL;
		}
		else if (type == CU_BEZIER) {     /* to Bezier */
			nr = nu->pntsu / 3;

			if (nr < 2) {
				return 1;  /* conversion impossible */
			}
			else {
				bezt = MEM_callocN(nr * sizeof(BezTriple), "setsplinetype2");
				nu->bezt = bezt;
				a = nr;
				bp = nu->bp;
				while (a--) {
					copy_v3_v3(bezt->vec[0], bp->vec);
					bezt->f1 = bp->f1;
					bp++;
					copy_v3_v3(bezt->vec[1], bp->vec);
					bezt->f2 = bp->f1;
					bp++;
					copy_v3_v3(bezt->vec[2], bp->vec);
					bezt->f3 = bp->f1;
					bezt->radius = bp->radius;
					bezt->weight = bp->weight;
					bp++;
					bezt++;
				}
				MEM_freeN(nu->bp);
				nu->bp = NULL;
				MEM_freeN(nu->knotsu);
				nu->knotsu = NULL;
				nu->pntsu = nr;
				nu->type = CU_BEZIER;
			}
		}
	}

	return 0;
}

void ED_nurb_set_spline_type(Nurb *nu, int type)
{
	convertspline(type, nu);
}

static int set_spline_type_exec(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	ListBase *editnurb = object_editcurve_get(obedit);
	Nurb *nu;
	int changed = 0, type = RNA_enum_get(op->ptr, "type");

	if (type == CU_CARDINAL || type == CU_BSPLINE) {
		BKE_report(op->reports, RPT_ERROR, "Not implemented yet");
		return OPERATOR_CANCELLED;
	}
	
	for (nu = editnurb->first; nu; nu = nu->next) {
		if (isNurbsel(nu)) {
			if (convertspline(type, nu))
				BKE_report(op->reports, RPT_ERROR, "No conversion possible");
			else
				changed = 1;
		}
	}

	if (changed) {
		if (ED_curve_updateAnimPaths(obedit->data))
			WM_event_add_notifier(C, NC_OBJECT | ND_KEYS, obedit);

		DAG_id_tag_update(obedit->data, 0);
		WM_event_add_notifier(C, NC_GEOM | ND_DATA, obedit->data);

		return OPERATOR_FINISHED;
	}
	else {
		return OPERATOR_CANCELLED;
	}
}

void CURVE_OT_spline_type_set(wmOperatorType *ot)
{
	static EnumPropertyItem type_items[] = {
		{CU_POLY, "POLY", 0, "Poly", ""},
		{CU_BEZIER, "BEZIER", 0, "Bezier", ""},
//		{CU_CARDINAL, "CARDINAL", 0, "Cardinal", ""},
//		{CU_BSPLINE, "B_SPLINE", 0, "B-Spline", ""},
		{CU_NURBS, "NURBS", 0, "NURBS", ""},
		{0, NULL, 0, NULL, NULL}
	};

	/* identifiers */
	ot->name = "Set Spline Type";
	ot->description = "Set type of active spline";
	ot->idname = "CURVE_OT_spline_type_set";
	
	/* api callbacks */
	ot->exec = set_spline_type_exec;
	ot->invoke = WM_menu_invoke;
	ot->poll = ED_operator_editcurve;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* properties */
	ot->prop = RNA_def_enum(ot->srna, "type", type_items, CU_POLY, "Type", "Spline type");
}

/***************** set handle type operator *******************/

static int set_handle_type_exec(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	ListBase *editnurb = object_editcurve_get(obedit);

	BKE_nurbList_handles_set(editnurb, RNA_enum_get(op->ptr, "type"));

	WM_event_add_notifier(C, NC_GEOM | ND_DATA, obedit->data);
	DAG_id_tag_update(obedit->data, 0);

	return OPERATOR_FINISHED;
}

void CURVE_OT_handle_type_set(wmOperatorType *ot)
{
	/* keep in sync with graphkeys_handle_type_items */
	static EnumPropertyItem editcurve_handle_type_items[] = {
		{HD_AUTO, "AUTOMATIC", 0, "Automatic", ""},
		{HD_VECT, "VECTOR", 0, "Vector", ""},
		{5, "ALIGNED", 0, "Aligned", ""},
		{6, "FREE_ALIGN", 0, "Free", ""},
		{3, "TOGGLE_FREE_ALIGN", 0, "Toggle Free/Align", ""},
		{0, NULL, 0, NULL, NULL}
	};

	/* identifiers */
	ot->name = "Set Handle Type";
	ot->description = "Set type of handles for selected control points";
	ot->idname = "CURVE_OT_handle_type_set";
	
	/* api callbacks */
	ot->invoke = WM_menu_invoke;
	ot->exec = set_handle_type_exec;
	ot->poll = ED_operator_editcurve;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* properties */
	ot->prop = RNA_def_enum(ot->srna, "type", editcurve_handle_type_items, 1, "Type", "Spline type");
}

/***************** make segment operator **********************/

/* ******************** SKINNING LOFTING!!! ******************** */

static void switchdirection_knots(float *base, int tot)
{
	float *fp1, *fp2, *tempf;
	int a;
	
	if (base == NULL || tot == 0) return;
	
	/* reverse knots */
	a = tot;
	fp1 = base;
	fp2 = fp1 + (a - 1);
	a /= 2;
	while (fp1 != fp2 && a > 0) {
		SWAP(float, *fp1, *fp2);
		a--;
		fp1++; 
		fp2--;
	}
	/* and make in increasing order again */
	a = tot;
	fp1 = base;
	fp2 = tempf = MEM_mallocN(sizeof(float) * a, "switchdirect");
	while (a--) {
		fp2[0] = fabs(fp1[1] - fp1[0]);
		fp1++;
		fp2++;
	}

	a = tot - 1;
	fp1 = base;
	fp2 = tempf;
	fp1[0] = 0.0;
	fp1++;
	while (a--) {
		fp1[0] = fp1[-1] + fp2[0];
		fp1++;
		fp2++;
	}
	MEM_freeN(tempf);
}

static void rotate_direction_nurb(Nurb *nu)
{
	BPoint *bp1, *bp2, *temp;
	int u, v;
	
	SWAP(short, nu->pntsu, nu->pntsv);
	SWAP(short, nu->orderu, nu->orderv);
	SWAP(short, nu->resolu, nu->resolv);
	SWAP(short, nu->flagu, nu->flagv);
	
	SWAP(float *, nu->knotsu, nu->knotsv);
	switchdirection_knots(nu->knotsv, KNOTSV(nu));
	
	temp = MEM_dupallocN(nu->bp);
	bp1 = nu->bp;
	for (v = 0; v < nu->pntsv; v++) {
		for (u = 0; u < nu->pntsu; u++, bp1++) {
			bp2 = temp + (nu->pntsu - u - 1) * (nu->pntsv) + v;
			*bp1 = *bp2;
		}
	}

	MEM_freeN(temp);
}

static int is_u_selected(Nurb *nu, int u)
{
	BPoint *bp;
	int v;
	
	/* what about resolu == 2? */
	bp = &nu->bp[u];
	for (v = 0; v < nu->pntsv - 1; v++, bp += nu->pntsu) {
		if (v) if (bp->f1 & SELECT) return 1;
	}
	
	return 0;
}

typedef struct NurbSort {
	struct NurbSort *next, *prev;
	Nurb *nu;
	float vec[3];
} NurbSort;

static ListBase nsortbase = {NULL, NULL};
/*  static NurbSort *nusmain; */ /* this var seems to go unused... at least in this file */

static void make_selection_list_nurb(ListBase *editnurb)
{
	ListBase nbase = {NULL, NULL};
	NurbSort *nus, *nustest, *headdo, *taildo;
	Nurb *nu;
	BPoint *bp;
	float dist, headdist, taildist;
	int a;
	
	for (nu = editnurb->first; nu; nu = nu->next) {
		if (isNurbsel(nu)) {
			
			nus = (NurbSort *)MEM_callocN(sizeof(NurbSort), "sort");
			BLI_addhead(&nbase, nus);
			nus->nu = nu;
			
			bp = nu->bp;
			a = nu->pntsu;
			while (a--) {
				add_v3_v3(nus->vec, bp->vec);
				bp++;
			}
			mul_v3_fl(nus->vec, 1.0f / (float)nu->pntsu);
			
			
		}
	}

	/* just add the first one */
	nus = nbase.first;
	BLI_remlink(&nbase, nus);
	BLI_addtail(&nsortbase, nus);
	
	/* now add, either at head or tail, the closest one */
	while (nbase.first) {
	
		headdist = taildist = 1.0e30;
		headdo = taildo = NULL;

		nustest = nbase.first;
		while (nustest) {
			dist = len_v3v3(nustest->vec, ((NurbSort *)nsortbase.first)->vec);

			if (dist < headdist) {
				headdist = dist;
				headdo = nustest;
			}
			dist = len_v3v3(nustest->vec, ((NurbSort *)nsortbase.last)->vec);

			if (dist < taildist) {
				taildist = dist;
				taildo = nustest;
			}
			nustest = nustest->next;
		}
		
		if (headdist < taildist) {
			BLI_remlink(&nbase, headdo);
			BLI_addhead(&nsortbase, headdo);
		}
		else {
			BLI_remlink(&nbase, taildo);
			BLI_addtail(&nsortbase, taildo);
		}
	}
}

static void merge_2_nurb(wmOperator *op, ListBase *editnurb, Nurb *nu1, Nurb *nu2)
{
	BPoint *bp, *bp1, *bp2, *temp;
	float len1, len2;
	int origu, u, v;
	
	/* first nurbs will be changed to make u = resolu-1 selected */
	/* 2nd nurbs will be changed to make u = 0 selected */

	/* first nurbs: u = resolu-1 selected */
	
	if (is_u_selected(nu1, nu1->pntsu - 1) ) ;
	else {
		/* For 2D curves blender uses (orderv = 0). It doesn't make any sense mathematically. */
		/* but after rotating (orderu = 0) will be confusing. */
		if (nu1->orderv == 0) nu1->orderv = 1;

		rotate_direction_nurb(nu1);
		if (is_u_selected(nu1, nu1->pntsu - 1)) ;
		else {
			rotate_direction_nurb(nu1);
			if (is_u_selected(nu1, nu1->pntsu - 1)) ;
			else {
				rotate_direction_nurb(nu1);
				if (is_u_selected(nu1, nu1->pntsu - 1)) ;
				else {
					/* rotate again, now its OK! */
					if (nu1->pntsv != 1) rotate_direction_nurb(nu1);
					return;
				}
			}
		}
	}
	
	/* 2nd nurbs: u = 0 selected */
	if (is_u_selected(nu2, 0) ) ;
	else {
		if (nu2->orderv == 0) nu2->orderv = 1;
		rotate_direction_nurb(nu2);
		if (is_u_selected(nu2, 0)) ;
		else {
			rotate_direction_nurb(nu2);
			if (is_u_selected(nu2, 0)) ;
			else {
				rotate_direction_nurb(nu2);
				if (is_u_selected(nu2, 0)) ;
				else {
					/* rotate again, now its OK! */
					if (nu1->pntsu == 1) rotate_direction_nurb(nu1);
					if (nu2->pntsv != 1) rotate_direction_nurb(nu2);
					return;
				}
			}
		}
	}
	
	if (nu1->pntsv != nu2->pntsv) {
		BKE_report(op->reports, RPT_ERROR, "Resolution doesn't match");
		return;
	}
	
	/* ok, now nu1 has the rightmost column and nu2 the leftmost column selected */
	/* maybe we need a 'v' flip of nu2? */
	
	bp1 = &nu1->bp[nu1->pntsu - 1];
	bp2 = nu2->bp;
	len1 = 0.0;
	
	for (v = 0; v < nu1->pntsv; v++, bp1 += nu1->pntsu, bp2 += nu2->pntsu) {
		len1 += len_v3v3(bp1->vec, bp2->vec);
	}

	bp1 = &nu1->bp[nu1->pntsu - 1];
	bp2 = &nu2->bp[nu2->pntsu * (nu2->pntsv - 1)];
	len2 = 0.0;
	
	for (v = 0; v < nu1->pntsv; v++, bp1 += nu1->pntsu, bp2 -= nu2->pntsu) {
		len2 += len_v3v3(bp1->vec, bp2->vec);
	}

	/* merge */
	origu = nu1->pntsu;
	nu1->pntsu += nu2->pntsu;
	if (nu1->orderu < 3 && nu1->orderu < nu1->pntsu) nu1->orderu++;
	if (nu1->orderv < 3 && nu1->orderv < nu1->pntsv) nu1->orderv++;
	temp = nu1->bp;
	nu1->bp = MEM_mallocN(nu1->pntsu * nu1->pntsv * sizeof(BPoint), "mergeBP");
	
	bp = nu1->bp;
	bp1 = temp;
	
	for (v = 0; v < nu1->pntsv; v++) {
		
		/* switch direction? */
		if (len1 < len2) bp2 = &nu2->bp[v * nu2->pntsu];
		else             bp2 = &nu2->bp[(nu1->pntsv - v - 1) * nu2->pntsu];

		for (u = 0; u < nu1->pntsu; u++, bp++) {
			if (u < origu) {
				*bp = *bp1; bp1++;
				select_bpoint(bp, SELECT, 1, HIDDEN);
			}
			else {
				*bp = *bp2; bp2++;
			}
		}
	}

	if (nu1->type == CU_NURBS) {
		/* merge knots */
		BKE_nurb_knot_calc_u(nu1);
	
		/* make knots, for merged curved for example */
		BKE_nurb_knot_calc_v(nu1);
	}
	
	MEM_freeN(temp);
	BLI_remlink(editnurb, nu2);
	BKE_nurb_free(nu2);
}

static int merge_nurb(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	ListBase *editnurb = object_editcurve_get(obedit);
	NurbSort *nus1, *nus2;
	int ok = 1;
	
	make_selection_list_nurb(editnurb);
	
	if (nsortbase.first == nsortbase.last) {
		BLI_freelistN(&nsortbase);
		BKE_report(op->reports, RPT_ERROR, "Too few selections to merge");
		return OPERATOR_CANCELLED;
	}
	
	nus1 = nsortbase.first;
	nus2 = nus1->next;

	/* resolution match, to avoid uv rotations */
	if (nus1->nu->pntsv == 1) {
		if (nus1->nu->pntsu == nus2->nu->pntsu || nus1->nu->pntsu == nus2->nu->pntsv) ;
		else ok = 0;
	}
	else if (nus2->nu->pntsv == 1) {
		if (nus2->nu->pntsu == nus1->nu->pntsu || nus2->nu->pntsu == nus1->nu->pntsv) ;
		else ok = 0;
	}
	else if (nus1->nu->pntsu == nus2->nu->pntsu || nus1->nu->pntsv == nus2->nu->pntsv) ;
	else if (nus1->nu->pntsu == nus2->nu->pntsv || nus1->nu->pntsv == nus2->nu->pntsu) ;
	else {
		ok = 0;
	}
	
	if (ok == 0) {
		BKE_report(op->reports, RPT_ERROR, "Resolution doesn't match");
		BLI_freelistN(&nsortbase);
		return OPERATOR_CANCELLED;
	}

	while (nus2) {
		merge_2_nurb(op, editnurb, nus1->nu, nus2->nu);
		nus2 = nus2->next;
	}
	
	BLI_freelistN(&nsortbase);
	
	set_actNurb(obedit, NULL);

	WM_event_add_notifier(C, NC_GEOM | ND_DATA, obedit->data);
	DAG_id_tag_update(obedit->data, 0);
	
	return OPERATOR_FINISHED;
}

static int make_segment_exec(bContext *C, wmOperator *op)
{
	/* joins 2 curves */
	Object *obedit = CTX_data_edit_object(C);
	Curve *cu = obedit->data;
	ListBase *nubase = object_editcurve_get(obedit);
	Nurb *nu, *nu1 = NULL, *nu2 = NULL;
	BPoint *bp;
	int a, ok = 0;

	/* first decide if this is a surface merge! */
	if (obedit->type == OB_SURF) nu = nubase->first;
	else nu = NULL;
	
	while (nu) {
		if (isNurbsel(nu)) {
		
			if (nu->pntsu > 1 && nu->pntsv > 1) break;
			if (isNurbsel_count(cu, nu) > 1) break;
			if (isNurbsel_count(cu, nu) == 1) {
				/* only 1 selected, not first or last, a little complex, but intuitive */
				if (nu->pntsv == 1) {
					if ( (nu->bp->f1 & SELECT) || (nu->bp[nu->pntsu - 1].f1 & SELECT)) ;
					else break;
				}
			}
		}
		nu = nu->next;
	}

	if (nu)
		return merge_nurb(C, op);
	
	/* find both nurbs and points, nu1 will be put behind nu2 */
	for (nu = nubase->first; nu; nu = nu->next) {
		if (nu->pntsu == 1)
			nu->flagu &= ~CU_NURB_CYCLIC;

		if ((nu->flagu & CU_NURB_CYCLIC) == 0) {    /* not cyclic */
			if (nu->type == CU_BEZIER) {
				if (nu1 == NULL) {
					if (BEZSELECTED_HIDDENHANDLES(cu, nu->bezt) ) nu1 = nu;
					else {
						if (BEZSELECTED_HIDDENHANDLES(cu, &(nu->bezt[nu->pntsu - 1])) ) {
							nu1 = nu;
							BKE_nurb_direction_switch(nu);
							keyData_switchDirectionNurb(cu, nu);
						}
					}
				}
				else if (nu2 == NULL) {
					if (BEZSELECTED_HIDDENHANDLES(cu, nu->bezt) ) {
						nu2 = nu;
						BKE_nurb_direction_switch(nu);
						keyData_switchDirectionNurb(cu, nu);
					}
					else {
						if (BEZSELECTED_HIDDENHANDLES(cu, &(nu->bezt[nu->pntsu - 1])) ) {
							nu2 = nu;
						}
					}
				}
				else break;
			}
			else if (nu->pntsv == 1) {
				bp = nu->bp;
				if (nu1 == NULL) {
					if (bp->f1 & SELECT) nu1 = nu;
					else {
						bp = bp + (nu->pntsu - 1);
						if (bp->f1 & SELECT) {
							nu1 = nu;
							BKE_nurb_direction_switch(nu);
							keyData_switchDirectionNurb(cu, nu);
						}
					}
				}
				else if (nu2 == NULL) {
					if (bp->f1 & SELECT) {
						nu2 = nu;
						BKE_nurb_direction_switch(nu);
						keyData_switchDirectionNurb(cu, nu);
					}
					else {
						bp = bp + (nu->pntsu - 1);
						if (bp->f1 & SELECT) {
							nu2 = nu;
						}
					}
				}
				else break;
			}
		}
	}

	if ((nu1 && nu2) && (nu1 != nu2)) {
		if (nu1->type == nu2->type) {
			if (nu1->type == CU_BEZIER) {
				BezTriple *bezt = (BezTriple *)MEM_mallocN((nu1->pntsu + nu2->pntsu) * sizeof(BezTriple), "addsegmentN");
				ED_curve_beztcpy(cu->editnurb, bezt, nu2->bezt, nu2->pntsu);
				ED_curve_beztcpy(cu->editnurb, bezt + nu2->pntsu, nu1->bezt, nu1->pntsu);

				MEM_freeN(nu1->bezt);
				nu1->bezt = bezt;
				nu1->pntsu += nu2->pntsu;
				BLI_remlink(nubase, nu2);
				BKE_nurb_free(nu2); nu2 = NULL;
				BKE_nurb_handles_calc(nu1);
			}
			else {
				bp = (BPoint *)MEM_mallocN((nu1->pntsu + nu2->pntsu) * sizeof(BPoint), "addsegmentN2");
				ED_curve_bpcpy(cu->editnurb, bp, nu2->bp, nu2->pntsu);
				ED_curve_bpcpy(cu->editnurb, bp + nu2->pntsu, nu1->bp, nu1->pntsu);
				MEM_freeN(nu1->bp);
				nu1->bp = bp;

				a = nu1->pntsu + nu1->orderu;

				nu1->pntsu += nu2->pntsu;
				BLI_remlink(nubase, nu2);

				/* now join the knots */
				if (nu1->type == CU_NURBS) {
					if (nu1->knotsu != NULL) {
						MEM_freeN(nu1->knotsu);
						nu1->knotsu = NULL;
					}

					BKE_nurb_knot_calc_u(nu1);
				}
				BKE_nurb_free(nu2); nu2 = NULL;
			}

			set_actNurb(obedit, nu1);   /* for selected */
			ok = 1;
		}
	}
	else if (nu1 && !nu2) {
		if (!(nu1->flagu & CU_NURB_CYCLIC) && nu1->pntsu > 1) {
			if (nu1->type == CU_BEZIER && BEZSELECTED_HIDDENHANDLES(cu, nu1->bezt) &&
			    BEZSELECTED_HIDDENHANDLES(cu, &nu1->bezt[nu1->pntsu - 1]))
			{
				nu1->flagu |= CU_NURB_CYCLIC;
				BKE_nurb_handles_calc(nu1);
				ok = 1;
			}
			else if (nu1->type == CU_NURBS && nu1->bp->f1 & SELECT && (nu1->bp[nu1->pntsu - 1].f1 & SELECT)) {
				nu1->flagu |= CU_NURB_CYCLIC;
				BKE_nurb_knot_calc_u(nu1);
				ok = 1;
			}
		}
	}

	if (!ok) {
		BKE_report(op->reports, RPT_ERROR, "Can't make segment");
		return OPERATOR_CANCELLED;
	}

	if (ED_curve_updateAnimPaths(obedit->data))
		WM_event_add_notifier(C, NC_OBJECT | ND_KEYS, obedit);

	WM_event_add_notifier(C, NC_GEOM | ND_DATA, obedit->data);
	DAG_id_tag_update(obedit->data, 0);

	return OPERATOR_FINISHED;
}

void CURVE_OT_make_segment(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Make Segment";
	ot->idname = "CURVE_OT_make_segment";
	ot->description = "Join two curves by their selected ends";
	
	/* api callbacks */
	ot->exec = make_segment_exec;
	ot->poll = ED_operator_editsurfcurve;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/***************** pick select from 3d view **********************/

int mouse_nurb(bContext *C, const int mval[2], int extend, int deselect, int toggle)
{
	Object *obedit = CTX_data_edit_object(C);
	Curve *cu = obedit->data;
	ListBase *editnurb = object_editcurve_get(obedit);
	ViewContext vc;
	Nurb *nu;
	BezTriple *bezt = NULL;
	BPoint *bp = NULL;
	int location[2];
	short hand;
	
	view3d_operator_needs_opengl(C);
	view3d_set_viewcontext(C, &vc);
	
	location[0] = mval[0];
	location[1] = mval[1];
	hand = findnearestNurbvert(&vc, 1, location, &nu, &bezt, &bp);

	if (bezt || bp) {
		if (extend) {
			if (bezt) {
				if (hand == 1) {
					select_beztriple(bezt, SELECT, 1, HIDDEN);
					cu->lastsel = bezt;
				}
				else {
					if (hand == 0) bezt->f1 |= SELECT;
					else bezt->f3 |= SELECT;

					cu->lastsel = NULL;
				}
			}
			else {
				cu->lastsel = bp;
				select_bpoint(bp, SELECT, 1, HIDDEN);
			}
		}
		else if (deselect) {
			if (bezt) {
				if (hand == 1) {
					select_beztriple(bezt, DESELECT, 1, HIDDEN);
					if (bezt == cu->lastsel) cu->lastsel = NULL;
				}
				else if (hand == 0) {
					bezt->f1 &= ~SELECT;
				}
				else {
					bezt->f3 &= ~SELECT;
				}
			}
			else {
				select_bpoint(bp, DESELECT, 1, HIDDEN);
				if (cu->lastsel == bp) cu->lastsel = NULL;
			}
		}
		else if (toggle) {
			if (bezt) {
				if (hand == 1) {
					if (bezt->f2 & SELECT) {
						select_beztriple(bezt, DESELECT, 1, HIDDEN);
						if (bezt == cu->lastsel) cu->lastsel = NULL;
					}
					else {
						select_beztriple(bezt, SELECT, 1, HIDDEN);
						cu->lastsel = bezt;
					}
				}
				else if (hand == 0) {
					bezt->f1 ^= SELECT;
				}
				else {
					bezt->f3 ^= SELECT;
				}
			}
			else {
				if (bp->f1 & SELECT) {
					select_bpoint(bp, DESELECT, 1, HIDDEN);
					if (cu->lastsel == bp) cu->lastsel = NULL;
				}
				else {
					select_bpoint(bp, SELECT, 1, HIDDEN);
					cu->lastsel = bp;
				}
			}
		}
		else {
			setflagsNurb(editnurb, 0);

			if (bezt) {

				if (hand == 1) {
					select_beztriple(bezt, SELECT, 1, HIDDEN);
					cu->lastsel = bezt;
				}
				else {
					if (hand == 0) bezt->f1 |= SELECT;
					else bezt->f3 |= SELECT;

					cu->lastsel = NULL;
				}
			}
			else {
				cu->lastsel = bp;
				select_bpoint(bp, SELECT, 1, HIDDEN);
			}
		}

		if (nu != get_actNurb(obedit))
			set_actNurb(obedit, nu);

		WM_event_add_notifier(C, NC_GEOM | ND_SELECT, obedit->data);

		return 1;
	}
	
	return 0;
}

/******************** spin operator ***********************/

/* 'cent' is in object space and 'dvec' in worldspace.
 */
static int spin_nurb(float viewmat[][4], Object *obedit, float *axis, float *cent)
{
	Curve *cu = (Curve *)obedit->data;
	ListBase *editnurb = object_editcurve_get(obedit);
	Nurb *nu;
	float si, phi, n[3], q[4], cmat[3][3], tmat[3][3], imat[3][3];
	float bmat[3][3], rotmat[3][3], scalemat1[3][3], scalemat2[3][3];
	float persmat[3][3], persinv[3][3];
	short a, ok, changed = 0;

	copy_m3_m4(persmat, viewmat);
	invert_m3_m3(persinv, persmat);

	/* imat and center and size */
	copy_m3_m4(bmat, obedit->obmat);
	invert_m3_m3(imat, bmat);
	
	normalize_v3_v3(n, axis);
	/* TODO - use math func */
	phi = M_PI / 8.0;
	q[0] = cos(phi);
	si = sin(phi);
	q[1] = n[0] * si;
	q[2] = n[1] * si;
	q[3] = n[2] * si;
	quat_to_mat3(cmat, q);
	mul_m3_m3m3(tmat, cmat, bmat);
	mul_m3_m3m3(rotmat, imat, tmat);

	unit_m3(scalemat1);
	scalemat1[0][0] = M_SQRT2;
	scalemat1[1][1] = M_SQRT2;

	mul_m3_m3m3(tmat, persmat, bmat);
	mul_m3_m3m3(cmat, scalemat1, tmat);
	mul_m3_m3m3(tmat, persinv, cmat);
	mul_m3_m3m3(scalemat1, imat, tmat);

	unit_m3(scalemat2);
	scalemat2[0][0] /= (float)M_SQRT2;
	scalemat2[1][1] /= (float)M_SQRT2;

	mul_m3_m3m3(tmat, persmat, bmat);
	mul_m3_m3m3(cmat, scalemat2, tmat);
	mul_m3_m3m3(tmat, persinv, cmat);
	mul_m3_m3m3(scalemat2, imat, tmat);

	ok = 1;

	for (a = 0; a < 7; a++) {
		ok = extrudeflagNurb(cu->editnurb, 1);

		if (ok == 0)
			return changed;

		changed = 1;

		rotateflagNurb(editnurb, SELECT, cent, rotmat);

		if ((a & SELECT) == 0) {
			rotateflagNurb(editnurb, SELECT, cent, scalemat1);
			weightflagNurb(editnurb, SELECT, 0.25 * M_SQRT2);
		}
		else {
			rotateflagNurb(editnurb, SELECT, cent, scalemat2);
			weightflagNurb(editnurb, SELECT, 4.0 / M_SQRT2);
		}
	}

	if (ok) {
		for (nu = editnurb->first; nu; nu = nu->next) {
			if (isNurbsel(nu)) {
				nu->orderv = 4;
				nu->flagv |= CU_NURB_CYCLIC;
				BKE_nurb_knot_calc_v(nu);
			}
		}
	}

	return changed;
}

static int spin_exec(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	RegionView3D *rv3d = ED_view3d_context_rv3d(C);
	float cent[3], axis[3], viewmat[4][4];
	
	RNA_float_get_array(op->ptr, "center", cent);
	RNA_float_get_array(op->ptr, "axis", axis);
	
	invert_m4_m4(obedit->imat, obedit->obmat);
	mul_m4_v3(obedit->imat, cent);
	
	if (rv3d)
		copy_m4_m4(viewmat, rv3d->viewmat);
	else
		unit_m4(viewmat);
	
	if (!spin_nurb(viewmat, obedit, axis, cent)) {
		BKE_report(op->reports, RPT_ERROR, "Can't spin");
		return OPERATOR_CANCELLED;
	}

	if (ED_curve_updateAnimPaths(obedit->data))
		WM_event_add_notifier(C, NC_OBJECT | ND_KEYS, obedit);

	WM_event_add_notifier(C, NC_GEOM | ND_DATA, obedit->data);
	DAG_id_tag_update(obedit->data, 0);

	return OPERATOR_FINISHED;
}

static int spin_invoke(bContext *C, wmOperator *op, wmEvent *UNUSED(event))
{
	Scene *scene = CTX_data_scene(C);
	View3D *v3d = CTX_wm_view3d(C);
	RegionView3D *rv3d = ED_view3d_context_rv3d(C);
	float axis[3] = {0.0f, 0.0f, 1.0f};
	
	if (rv3d)
		copy_v3_v3(axis, rv3d->viewinv[2]);
	
	RNA_float_set_array(op->ptr, "center", give_cursor(scene, v3d));
	RNA_float_set_array(op->ptr, "axis", axis);
	
	return spin_exec(C, op);
}

void CURVE_OT_spin(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Spin";
	ot->idname = "CURVE_OT_spin";
	ot->description = "Extrude selected boundary row around pivot point and current view axis";
	
	/* api callbacks */
	ot->exec = spin_exec;
	ot->invoke = spin_invoke;
	ot->poll = ED_operator_editsurf;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	RNA_def_float_vector_xyz(ot->srna, "center", 3, NULL, -FLT_MAX, FLT_MAX, "Center", "Center in global view space", -FLT_MAX, FLT_MAX);
	RNA_def_float_vector(ot->srna, "axis", 3, NULL, -1.0f, 1.0f, "Axis", "Axis in global view space", -FLT_MAX, FLT_MAX);
}

/***************** add vertex operator **********************/

static int addvert_Nurb(bContext *C, short mode, float location[3])
{
	Object *obedit = CTX_data_edit_object(C);
	Curve *cu = (Curve *)obedit->data;
	EditNurb *editnurb = cu->editnurb;
	Nurb *nu, *newnu = NULL;
	BezTriple *bezt, *newbezt = NULL;
	BPoint *bp, *newbp = NULL;
	float imat[4][4], temp[3];
	int ok = 0;
	BezTriple *bezt_recalc[3] = {NULL};

	invert_m4_m4(imat, obedit->obmat);

	findselectedNurbvert(&editnurb->nurbs, &nu, &bezt, &bp);

	if ((nu == NULL) || (nu->type == CU_BEZIER && bezt == NULL) || (nu->type != CU_BEZIER && bp == NULL)) {
		if (mode != 'e') {
			if (cu->actnu >= 0)
				nu = BLI_findlink(&editnurb->nurbs, cu->actnu);

			if (!nu || nu->type == CU_BEZIER) {
				newbezt = (BezTriple *)MEM_callocN(sizeof(BezTriple), "addvert_Nurb");
				newbezt->radius = 1;
				newbezt->alfa = 0;
				BEZ_SEL(newbezt);
				newbezt->h2 = newbezt->h1 = HD_AUTO;

				newnu = (Nurb *)MEM_callocN(sizeof(Nurb), "addvert_Nurb newnu");
				if (!nu) {
					/* no selected segment -- create new one which is BEZIER type
					 * type couldn't be determined from Curve bt could be changed
					 * in the future, so shouldn't make much headache */
					newnu->type = CU_BEZIER;
					newnu->resolu = cu->resolu;
					newnu->flag |= CU_SMOOTH;
				}
				else memcpy(newnu, nu, sizeof(Nurb));

				BLI_addtail(&editnurb->nurbs, newnu);
				set_actNurb(obedit, newnu);
				newnu->bezt = newbezt;
				newnu->pntsu = 1;

				temp[0] = 1;
				temp[1] = 0;
				temp[2] = 0;

				copy_v3_v3(newbezt->vec[1], location);
				sub_v3_v3v3(newbezt->vec[0], newbezt->vec[1], temp);
				add_v3_v3v3(newbezt->vec[2], newbezt->vec[1], temp);

				mul_m4_v3(imat, newbezt->vec[0]);
				mul_m4_v3(imat, newbezt->vec[1]);
				mul_m4_v3(imat, newbezt->vec[2]);

				ok = 1;
				nu = newnu;
			}
			else if (nu->pntsv == 1) {
				newbp = (BPoint *)MEM_callocN(sizeof(BPoint), "addvert_Nurb5");
				newbp->radius = 1;
				newbp->alfa = 0;
				newbp->f1 |= SELECT;
				cu->lastsel = newbp;

				newnu = (Nurb *)MEM_mallocN(sizeof(Nurb), "addvert_Nurb newnu");
				memcpy(newnu, nu, sizeof(Nurb));
				BLI_addtail(&editnurb->nurbs, newnu);
				set_actNurb(obedit, newnu);
				newnu->bp = newbp;
				newnu->orderu = 2;
				newnu->pntsu = 1;

				mul_v3_m4v3(newbp->vec, imat, location);
				newbp->vec[3] = 1.0;

				newnu->knotsu = newnu->knotsv = NULL;
				BKE_nurb_knot_calc_u(newnu);

				ok = 1;
				nu = newnu;
			}

		}

		if (!ok)
			return OPERATOR_CANCELLED;
	}

	if (!ok && nu->type == CU_BEZIER) {
		/* which bezpoint? */
		if (bezt == &nu->bezt[nu->pntsu - 1]) {  /* last */
			BEZ_DESEL(bezt);
			newbezt = (BezTriple *)MEM_callocN((nu->pntsu + 1) * sizeof(BezTriple), "addvert_Nurb");
			ED_curve_beztcpy(editnurb, newbezt, nu->bezt, nu->pntsu);
			newbezt[nu->pntsu] = *bezt;
			copy_v3_v3(temp, bezt->vec[1]);
			MEM_freeN(nu->bezt);
			nu->bezt = newbezt;
			newbezt += nu->pntsu;
			BEZ_SEL(newbezt);
			cu->lastsel = newbezt;
			newbezt->h1 = newbezt->h2;
			bezt = &nu->bezt[nu->pntsu - 1];
			ok = 1;

			if (nu->pntsu > 1) {
				bezt_recalc[1] = newbezt;
				bezt_recalc[0] = newbezt - 1;
			}
		}
		else if (bezt == nu->bezt) {   /* first */
			BEZ_DESEL(bezt);
			newbezt = (BezTriple *)MEM_callocN((nu->pntsu + 1) * sizeof(BezTriple), "addvert_Nurb");
			ED_curve_beztcpy(editnurb, newbezt + 1, bezt, nu->pntsu);
			*newbezt = *bezt;
			BEZ_SEL(newbezt);
			cu->lastsel = newbezt;
			newbezt->h2 = newbezt->h1;
			copy_v3_v3(temp, bezt->vec[1]);
			MEM_freeN(nu->bezt);
			nu->bezt = newbezt;
			bezt = newbezt + 1;
			ok = 1;

			if (nu->pntsu > 1) {
				bezt_recalc[1] = newbezt;
				bezt_recalc[2] = newbezt + 1;
			}
		}
		else if (mode != 'e') {
			BEZ_DESEL(bezt);
			newbezt = (BezTriple *)MEM_callocN(sizeof(BezTriple), "addvert_Nurb");
			*newbezt = *bezt;
			BEZ_SEL(newbezt);
			newbezt->h2 = newbezt->h1;
			copy_v3_v3(temp, bezt->vec[1]);

			newnu = (Nurb *)MEM_mallocN(sizeof(Nurb), "addvert_Nurb newnu");
			memcpy(newnu, nu, sizeof(Nurb));
			BLI_addtail(&editnurb->nurbs, newnu);
			set_actNurb(obedit, newnu);
			newnu->bezt = newbezt;
			newnu->pntsu = 1;

			cu->lastsel = newbezt;

			bezt = newbezt;
			ok = 1;
		}
		else bezt = NULL;

		if (bezt) {
			if (!newnu) nu->pntsu++;

			if (mode == 'e') {
				copy_v3_v3(newbezt->vec[0], bezt->vec[0]);
				copy_v3_v3(newbezt->vec[1], bezt->vec[1]);
				copy_v3_v3(newbezt->vec[2], bezt->vec[2]);
			}
			else {
				mul_v3_m4v3(newbezt->vec[1], imat, location);
				sub_v3_v3v3(temp, newbezt->vec[1], temp);

				if (bezt_recalc[1]) {
					const char h1 = bezt_recalc[1]->h1, h2 = bezt_recalc[1]->h2;
					bezt_recalc[1]->h1 = bezt_recalc[1]->h2 = HD_AUTO;
					BKE_nurb_handle_calc(bezt_recalc[1], bezt_recalc[0], bezt_recalc[2], 0);
					bezt_recalc[1]->h1 = h1;
					bezt_recalc[1]->h2 = h2;
				}
				else {
					add_v3_v3v3(newbezt->vec[0], bezt->vec[0], temp);
					add_v3_v3v3(newbezt->vec[2], bezt->vec[2], temp);
				}
				

				if (newnu) BKE_nurb_handles_calc(newnu);
				else BKE_nurb_handles_calc(nu);
			}
		}
	}
	else if (!ok && nu->pntsv == 1) {
		/* which b-point? */
		if (bp == &nu->bp[nu->pntsu - 1]) {  /* last */
			bp->f1 = 0;
			newbp = (BPoint *)MEM_callocN((nu->pntsu + 1) * sizeof(BPoint), "addvert_Nurb4");
			ED_curve_bpcpy(editnurb, newbp, nu->bp, nu->pntsu);
			newbp[nu->pntsu] = *bp;
			MEM_freeN(nu->bp);
			nu->bp = newbp;
			newbp += nu->pntsu;
			newbp->f1 |= SELECT;
			cu->lastsel = newbp;
			bp = newbp - 1;
			ok = 1;
		}
		else if (bp == nu->bp) {   /* first */
			bp->f1 = 0;
			newbp = (BPoint *)MEM_callocN((nu->pntsu + 1) * sizeof(BPoint), "addvert_Nurb3");
			ED_curve_bpcpy(editnurb, newbp + 1, bp, nu->pntsu);
			*newbp = *bp;
			newbp->f1 |= SELECT;
			cu->lastsel = newbp;
			MEM_freeN(nu->bp);
			nu->bp = newbp;
			bp = newbp + 1;
			ok = 1;
		}
		else if (mode != 'e') {
			bp->f1 = 0;
			newbp = (BPoint *)MEM_callocN(sizeof(BPoint), "addvert_Nurb5");
			*newbp = *bp;
			newbp->f1 |= SELECT;
			cu->lastsel = newbp;

			newnu = (Nurb *)MEM_mallocN(sizeof(Nurb), "addvert_Nurb newnu");
			memcpy(newnu, nu, sizeof(Nurb));
			BLI_addtail(&editnurb->nurbs, newnu);
			set_actNurb(obedit, newnu);
			newnu->bp = newbp;
			newnu->orderu = 2;
			newnu->pntsu = 1;
			newnu->knotsu = newnu->knotsv = NULL;

			bp = newbp;
			ok = 1;
		}
		else bp = NULL;

		if (bp) {
			if (mode == 'e') {
				copy_v3_v3(newbp->vec, bp->vec);
			}
			else {
				mul_v3_m4v3(newbp->vec, imat, location);
				newbp->vec[3] = 1.0;

				if (!newnu && nu->orderu < 4 && nu->orderu <= nu->pntsu)
					nu->orderu++;
			}

			if (!newnu) {
				nu->pntsu++;
				BKE_nurb_knot_calc_u(nu);
			}
			else BKE_nurb_knot_calc_u(newnu);
		}
	}

	if (ok) {
		BKE_nurb_test2D(nu);

		if (ED_curve_updateAnimPaths(obedit->data))
			WM_event_add_notifier(C, NC_OBJECT | ND_KEYS, obedit);

		WM_event_add_notifier(C, NC_GEOM | ND_DATA, obedit->data);
		DAG_id_tag_update(obedit->data, 0);

		return OPERATOR_FINISHED;
	}

	return OPERATOR_CANCELLED;
}

static int add_vertex_exec(bContext *C, wmOperator *op)
{
	float location[3];

	RNA_float_get_array(op->ptr, "location", location);
	return addvert_Nurb(C, 0, location);
}

static int add_vertex_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	RegionView3D *rv3d = CTX_wm_region_view3d(C);

	if (rv3d && !RNA_struct_property_is_set(op->ptr, "location")) {
		Curve *cu;
		ViewContext vc;
		float location[3];

		Nurb *nu;
		BezTriple *bezt;
		BPoint *bp;

		view3d_set_viewcontext(C, &vc);

		cu = vc.obedit->data;

		findselectedNurbvert(&cu->editnurb->nurbs, &nu, &bezt, &bp);

		if (bezt) {
			mul_v3_m4v3(location, vc.obedit->obmat, bezt->vec[1]);
		}
		else if (bp) {
			mul_v3_m4v3(location, vc.obedit->obmat, bp->vec);
		}
		else {
			copy_v3_v3(location, give_cursor(vc.scene, vc.v3d));
		}

		view3d_get_view_aligned_coordinate(&vc, location, event->mval, TRUE);
		RNA_float_set_array(op->ptr, "location", location);
	}

	return add_vertex_exec(C, op);
}

void CURVE_OT_vertex_add(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Add Vertex";
	ot->idname = "CURVE_OT_vertex_add";
	ot->description = "Add a new control point (linked to only selected end-curve one, if any)";
	
	/* api callbacks */
	ot->exec = add_vertex_exec;
	ot->invoke = add_vertex_invoke;
	ot->poll = ED_operator_editcurve;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* properties */
	RNA_def_float_vector_xyz(ot->srna, "location", 3, NULL, -FLT_MAX, FLT_MAX, "Location", "Location to add new vertex at", -1e4, 1e4);
}

/***************** extrude operator **********************/

static int extrude_exec(bContext *C, wmOperator *UNUSED(op))
{
	Object *obedit = CTX_data_edit_object(C);
	Curve *cu = obedit->data;
	EditNurb *editnurb = cu->editnurb;
	Nurb *nu;
	
	/* first test: curve? */
	for (nu = editnurb->nurbs.first; nu; nu = nu->next)
		if (nu->pntsv == 1 && isNurbsel_count(cu, nu) == 1)
			break;

	if (obedit->type == OB_CURVE || nu) {
		addvert_Nurb(C, 'e', NULL);
	}
	else {
		if (extrudeflagNurb(editnurb, 1)) { /* '1'= flag */
			if (ED_curve_updateAnimPaths(obedit->data))
				WM_event_add_notifier(C, NC_OBJECT | ND_KEYS, obedit);

			WM_event_add_notifier(C, NC_GEOM | ND_DATA, obedit->data);
			DAG_id_tag_update(obedit->data, 0);
		}
	}

	return OPERATOR_FINISHED;
}

void CURVE_OT_extrude(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Extrude";
	ot->description = "Extrude selected control point(s)";
	ot->idname = "CURVE_OT_extrude";
	
	/* api callbacks */
	ot->exec = extrude_exec;
	ot->poll = ED_operator_editsurfcurve;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* to give to transform */
	RNA_def_enum(ot->srna, "mode", transform_mode_types, TFM_TRANSLATION, "Mode", "");
}

/***************** make cyclic operator **********************/

static int toggle_cyclic_exec(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	Curve *cu = obedit->data;
	ListBase *editnurb = object_editcurve_get(obedit);
	Nurb *nu;
	BezTriple *bezt;
	BPoint *bp;
	int a, direction = RNA_enum_get(op->ptr, "direction");

	for (nu = editnurb->first; nu; nu = nu->next) {
		if (nu->pntsu > 1 || nu->pntsv > 1) {
			if (nu->type == CU_POLY) {
				a = nu->pntsu;
				bp = nu->bp;
				while (a--) {
					if (bp->f1 & SELECT) {
						nu->flagu ^= CU_NURB_CYCLIC;
						break;
					}
					bp++;
				}
			}
			else if (nu->type == CU_BEZIER) {
				a = nu->pntsu;
				bezt = nu->bezt;
				while (a--) {
					if (BEZSELECTED_HIDDENHANDLES(cu, bezt) ) {
						nu->flagu ^= CU_NURB_CYCLIC;
						break;
					}
					bezt++;
				}
				BKE_nurb_handles_calc(nu);
			}
			else if (nu->pntsv == 1 && nu->type == CU_NURBS) {
				if (nu->knotsu) { /* if check_valid_nurb_u fails the knotsu can be NULL */
					a = nu->pntsu;
					bp = nu->bp;
					while (a--) {
						if (bp->f1 & SELECT) {
							nu->flagu ^= CU_NURB_CYCLIC;
							BKE_nurb_knot_calc_u(nu);   /* 1==u  type is ignored for cyclic curves */
							break;
						}
						bp++;
					}
				}
			}
			else if (nu->type == CU_NURBS) {
				a = nu->pntsu * nu->pntsv;
				bp = nu->bp;
				while (a--) {
	
					if (bp->f1 & SELECT) {
						if (direction == 0 && nu->pntsu > 1) {
							nu->flagu ^= CU_NURB_CYCLIC;
							BKE_nurb_knot_calc_u(nu);   /* 1==u  type is ignored for cyclic curves */
						}
						if (direction == 1 && nu->pntsv > 1) {
							nu->flagv ^= CU_NURB_CYCLIC;
							BKE_nurb_knot_calc_v(nu);   /* 2==v  type is ignored for cyclic curves */
						}
						break;
					}
					bp++;
				}
	
			}
		}
	}

	WM_event_add_notifier(C, NC_GEOM | ND_DATA, obedit->data);
	DAG_id_tag_update(obedit->data, 0);

	return OPERATOR_FINISHED;
}

static int toggle_cyclic_invoke(bContext *C, wmOperator *op, wmEvent *UNUSED(event))
{
	Object *obedit = CTX_data_edit_object(C);
	ListBase *editnurb = object_editcurve_get(obedit);
	uiPopupMenu *pup;
	uiLayout *layout;
	Nurb *nu;

	if (obedit->type == OB_SURF) {
		for (nu = editnurb->first; nu; nu = nu->next) {
			if (nu->pntsu > 1 || nu->pntsv > 1) {
				if (nu->type == CU_NURBS) {
					pup = uiPupMenuBegin(C, "Direction", ICON_NONE);
					layout = uiPupMenuLayout(pup);
					uiItemsEnumO(layout, op->type->idname, "direction");
					uiPupMenuEnd(C, pup);
					return OPERATOR_CANCELLED;
				}
			}
		}
	}

	return toggle_cyclic_exec(C, op);
}

void CURVE_OT_cyclic_toggle(wmOperatorType *ot)
{
	static EnumPropertyItem direction_items[] = {
		{0, "CYCLIC_U", 0, "Cyclic U", ""},
		{1, "CYCLIC_V", 0, "Cyclic V", ""},
		{0, NULL, 0, NULL, NULL}
	};

	/* identifiers */
	ot->name = "Toggle Cyclic";
	ot->description = "Make active spline closed/opened loop";
	ot->idname = "CURVE_OT_cyclic_toggle";
	
	/* api callbacks */
	ot->exec = toggle_cyclic_exec;
	ot->invoke = toggle_cyclic_invoke;
	ot->poll = ED_operator_editsurfcurve;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* properties */
	RNA_def_enum(ot->srna, "direction", direction_items, 0, "Direction", "Direction to make surface cyclic in");
}

/***************** select linked operator ******************/

static int select_linked_exec(bContext *C, wmOperator *UNUSED(op))
{
	Object *obedit = CTX_data_edit_object(C);
	Curve *cu = (Curve *)obedit->data;
	EditNurb *editnurb = cu->editnurb;
	ListBase *nurbs = &editnurb->nurbs;
	Nurb *nu;
	BezTriple *bezt;
	BPoint *bp;
	int a;

	for (nu = nurbs->first; nu; nu = nu->next) {
		if (nu->type == CU_BEZIER) {
			bezt = nu->bezt;
			a = nu->pntsu;
			while (a--) {
				if ( (bezt->f1 & SELECT) || (bezt->f2 & SELECT) || (bezt->f3 & SELECT) ) {
					a = nu->pntsu;
					bezt = nu->bezt;
					while (a--) {
						select_beztriple(bezt, SELECT, 1, VISIBLE);
						bezt++;
					}
					break;
				}
				bezt++;
			}
		}
		else {
			bp = nu->bp;
			a = nu->pntsu * nu->pntsv;
			while (a--) {
				if (bp->f1 & SELECT) {
					a = nu->pntsu * nu->pntsv;
					bp = nu->bp;
					while (a--) {
						select_bpoint(bp, SELECT, 1, VISIBLE);
						bp++;
					}
					break;
				}
				bp++;
			}
		}
	}

	WM_event_add_notifier(C, NC_GEOM | ND_SELECT, obedit->data);

	return OPERATOR_FINISHED;
}

static int select_linked_invoke(bContext *C, wmOperator *op, wmEvent *UNUSED(event))
{
	return select_linked_exec(C, op);
}

void CURVE_OT_select_linked(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Select Linked All";
	ot->idname = "CURVE_OT_select_linked";
	ot->description = "Select all control points linked to active one";

	/* api callbacks */
	ot->exec = select_linked_exec;
	ot->invoke = select_linked_invoke;
	ot->poll = ED_operator_editsurfcurve;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* properties */
}


/***************** select linked pick operator ******************/

static int select_linked_pick_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	Object *obedit = CTX_data_edit_object(C);
	ViewContext vc;
	Nurb *nu;
	BezTriple *bezt;
	BPoint *bp;
	int a, deselect;

	deselect = RNA_boolean_get(op->ptr, "deselect");

	view3d_operator_needs_opengl(C);
	view3d_set_viewcontext(C, &vc);

	findnearestNurbvert(&vc, 1, event->mval, &nu, &bezt, &bp);

	if (bezt) {
		a = nu->pntsu;
		bezt = nu->bezt;
		while (a--) {
			if (deselect) select_beztriple(bezt, DESELECT, 1, VISIBLE);
			else select_beztriple(bezt, SELECT, 1, VISIBLE);
			bezt++;
		}
	}
	else if (bp) {
		a = nu->pntsu * nu->pntsv;
		bp = nu->bp;
		while (a--) {
			if (deselect) select_bpoint(bp, DESELECT, 1, VISIBLE);
			else select_bpoint(bp, SELECT, 1, VISIBLE);
			bp++;
		}
	}

	WM_event_add_notifier(C, NC_GEOM | ND_SELECT, obedit->data);

	return OPERATOR_FINISHED;
}

void CURVE_OT_select_linked_pick(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Select Linked";
	ot->idname = "CURVE_OT_select_linked_pick";
	ot->description = "Select all control points linked to already selected ones";

	/* api callbacks */
	ot->invoke = select_linked_pick_invoke;
	ot->poll = ED_operator_editsurfcurve_region_view3d;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* properties */
	RNA_def_boolean(ot->srna, "deselect", 0, "Deselect", "Deselect linked control points rather than selecting them");
}

/***************** select row operator **********************/

static int select_row_exec(bContext *C, wmOperator *UNUSED(op))
{
	Object *obedit = CTX_data_edit_object(C);
	Curve *cu = obedit->data;
	ListBase *editnurb = object_editcurve_get(obedit);
	static BPoint *last = NULL;
	static int direction = 0;
	Nurb *nu;
	BPoint *bp;
	int u = 0, v = 0, a, b, ok = 0;

	if (editnurb->first == NULL)
		return OPERATOR_CANCELLED;
	if (cu->lastsel == NULL)
		return OPERATOR_CANCELLED;

	/* find the correct nurb and toggle with u of v */
	for (nu = editnurb->first; nu; nu = nu->next) {
		bp = nu->bp;
		for (v = 0; v < nu->pntsv; v++) {
			for (u = 0; u < nu->pntsu; u++, bp++) {
				if (bp == cu->lastsel) {
					if (bp->f1 & SELECT) {
						ok = 1;
						break;
					}
				}
			}
			if (ok) break;
		}

		if (ok) {
			if (last == cu->lastsel) {
				direction = 1 - direction;
				setflagsNurb(editnurb, 0);
			}
			last = cu->lastsel;

			bp = nu->bp;
			for (a = 0; a < nu->pntsv; a++) {
				for (b = 0; b < nu->pntsu; b++, bp++) {
					if (direction) {
						if (a == v) select_bpoint(bp, SELECT, 1, VISIBLE);
					}
					else {
						if (b == u) select_bpoint(bp, SELECT, 1, VISIBLE);
					}
				}
			}

			break;
		}
	}
	
	WM_event_add_notifier(C, NC_GEOM | ND_SELECT, obedit->data);

	return OPERATOR_FINISHED;
}

void CURVE_OT_select_row(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Select Control Point Row";
	ot->idname = "CURVE_OT_select_row";
	ot->description = "Select a row of control points including active one";
	
	/* api callbacks */
	ot->exec = select_row_exec;
	ot->poll = ED_operator_editsurf;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/***************** select next operator **********************/

static int select_next_exec(bContext *C, wmOperator *UNUSED(op))
{
	Object *obedit = CTX_data_edit_object(C);
	ListBase *editnurb = object_editcurve_get(obedit);
	
	select_adjacent_cp(editnurb, 1, 0, SELECT);
	WM_event_add_notifier(C, NC_GEOM | ND_SELECT, obedit->data);

	return OPERATOR_FINISHED;
}

void CURVE_OT_select_next(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Select Next";
	ot->idname = "CURVE_OT_select_next";
	ot->description = "Select control points following already selected ones along the curves";
	
	/* api callbacks */
	ot->exec = select_next_exec;
	ot->poll = ED_operator_editcurve;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/***************** select previous operator **********************/

static int select_previous_exec(bContext *C, wmOperator *UNUSED(op))
{
	Object *obedit = CTX_data_edit_object(C);
	ListBase *editnurb = object_editcurve_get(obedit);
	
	select_adjacent_cp(editnurb, -1, 0, SELECT);
	WM_event_add_notifier(C, NC_GEOM | ND_SELECT, obedit->data);

	return OPERATOR_FINISHED;
}

void CURVE_OT_select_previous(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Select Previous";
	ot->idname = "CURVE_OT_select_previous";
	ot->description = "Select control points preceding already selected ones along the curves";
	
	/* api callbacks */
	ot->exec = select_previous_exec;
	ot->poll = ED_operator_editcurve;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/***************** select more operator **********************/

static int select_more_exec(bContext *C, wmOperator *UNUSED(op))
{
	Object *obedit = CTX_data_edit_object(C);
	ListBase *editnurb = object_editcurve_get(obedit);
	Nurb *nu;
	BPoint *bp, *tempbp;
	int a;
	short sel = 0;
	short *selbpoints;
	
	/* note that NURBS surface is a special case because we mimic */
	/* the behavior of "select more" of mesh tools.	      */
	/* The algorithm is designed to work in planar cases so it    */
	/* may not be optimal always (example: end of NURBS sphere)   */
	if (obedit->type == OB_SURF) {
		for (nu = editnurb->first; nu; nu = nu->next) {
			a = nu->pntsu * nu->pntsv;
			bp = nu->bp;
			selbpoints = MEM_callocN(sizeof(short) * a - nu->pntsu, "selectlist");
			while (a > 0) {
				if ((selbpoints[a] != 1) && (bp->hide == 0) && (bp->f1 & SELECT)) {
					/* upper control point */
					if (a % nu->pntsu != 0) {
						tempbp = bp - 1;
						if (!(tempbp->f1 & SELECT)) select_bpoint(tempbp, SELECT, 1, VISIBLE); 
					}

					/* left control point. select only if it is not selected already */
					if (a - nu->pntsu > 0) {
						sel = 0;
						tempbp = bp + nu->pntsu;
						if (!(tempbp->f1 & SELECT)) sel = select_bpoint(tempbp, SELECT, 1, VISIBLE);
						/* make sure selected bpoint is discarded */
						if (sel == 1) selbpoints[a - nu->pntsu] = 1;
					}
					
					/* right control point */
					if (a + nu->pntsu < nu->pntsu * nu->pntsv) {
						tempbp = bp - nu->pntsu;
						if (!(tempbp->f1 & SELECT)) select_bpoint(tempbp, SELECT, 1, VISIBLE); 
					}
				
					/* lower control point. skip next bp in case selection was made */
					if (a % nu->pntsu != 1) {
						sel = 0;
						tempbp = bp + 1;
						if (!(tempbp->f1 & SELECT)) sel = select_bpoint(tempbp, SELECT, 1, VISIBLE);
						if (sel) {
							bp++;	
							a--;
						}
					}				
				}

				bp++;
				a--;
			}
			
			MEM_freeN(selbpoints);
		}
	}
	else {
		select_adjacent_cp(editnurb, 1, 0, SELECT);
		select_adjacent_cp(editnurb, -1, 0, SELECT);
	}
		
	WM_event_add_notifier(C, NC_GEOM | ND_SELECT, obedit->data);

	return OPERATOR_FINISHED;
}

void CURVE_OT_select_more(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Select More";
	ot->idname = "CURVE_OT_select_more";
	ot->description = "Select control points directly linked to already selected ones";
	
	/* api callbacks */
	ot->exec = select_more_exec;
	ot->poll = ED_operator_editsurfcurve;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/******************** select less operator *****************/

/* basic method: deselect if control point doesn't have all neighbors selected */
static int select_less_exec(bContext *C, wmOperator *UNUSED(op))
{
	Object *obedit = CTX_data_edit_object(C);
	ListBase *editnurb = object_editcurve_get(obedit);
	Nurb *nu;
	BPoint *bp;
	BezTriple *bezt;
	int a;
	short sel = 0, lastsel = 0;
	short *selbpoints;
	
	if (obedit->type == OB_SURF) {
		for (nu = editnurb->first; nu; nu = nu->next) {
			a = nu->pntsu * nu->pntsv;
			bp = nu->bp;
			selbpoints = MEM_callocN(sizeof(short) * a, "selectlist");
			while (a--) {
				if ((bp->hide == 0) && (bp->f1 & SELECT)) {
					sel = 0;
									
					/* check if neighbors have been selected */	
					/* edges of surface are an exception */	
					if ((a + 1) % nu->pntsu == 0) sel++;
					else {
						bp--;
						if ((selbpoints[a + 1] == 1) || ((bp->hide == 0) && (bp->f1 & SELECT))) sel++;
						bp++;
					}
					
					if ((a + 1) % nu->pntsu == 1) sel++;
					else {
						bp++;
						if ((bp->hide == 0) && (bp->f1 & SELECT)) sel++;
						bp--;
					}
					
					if (a + 1 > nu->pntsu * nu->pntsv - nu->pntsu) sel++;
					else {
						bp -= nu->pntsu;
						if ((selbpoints[a + nu->pntsu] == 1) || ((bp->hide == 0) && (bp->f1 & SELECT))) sel++;
						bp += nu->pntsu;
					}
									
					if (a < nu->pntsu) sel++;
					else {
						bp += nu->pntsu;
						if ((bp->hide == 0) && (bp->f1 & SELECT)) sel++;
						bp -= nu->pntsu;
					}
													
					if (sel != 4) {
						select_bpoint(bp, DESELECT, 1, VISIBLE); 
						selbpoints[a] = 1;
					}									
				}
				else lastsel = 0;
					
				bp++;
			}
			
			MEM_freeN(selbpoints);
		}
	}
	else {
		for (nu = editnurb->first; nu; nu = nu->next) {
			lastsel = 0;
			/* check what type of curve/nurb it is */
			if (nu->type == CU_BEZIER) {			
				a = nu->pntsu;
				bezt = nu->bezt;
				while (a--) {
					if ((bezt->hide == 0) && (bezt->f2 & SELECT)) {
						if (lastsel == 1) sel = 1;
						else sel = 0;
												
						/* check if neighbors have been selected */						
						/* first and last are exceptions */					
						if (a == nu->pntsu - 1) sel++;
						else { 
							bezt--;
							if ((bezt->hide == 0) && (bezt->f2 & SELECT)) sel++;
							bezt++;
						}
						
						if (a == 0) sel++;
						else {
							bezt++;
							if ((bezt->hide == 0) && (bezt->f2 & SELECT)) sel++;
							bezt--;
						}

						if (sel != 2) {
							select_beztriple(bezt, DESELECT, 1, VISIBLE);	
							lastsel = 1;
						}
						else lastsel = 0;
					}
					else lastsel = 0;
						
					bezt++;	
				}
			}
			else {
				a = nu->pntsu * nu->pntsv;
				bp = nu->bp;
				while (a--) {
					if ((lastsel == 0) && (bp->hide == 0) && (bp->f1 & SELECT)) {
						if (lastsel != 0) sel = 1;
						else sel = 0;
						
						/* first and last are exceptions */					
						if (a == nu->pntsu * nu->pntsv - 1) sel++;
						else { 
							bp--;
							if ((bp->hide == 0) && (bp->f1 & SELECT)) sel++;
							bp++;
						}
						
						if (a == 0) sel++;
						else {
							bp++;
							if ((bp->hide == 0) && (bp->f1 & SELECT)) sel++;
							bp--;
						}
											
						if (sel != 2) {
							select_bpoint(bp, DESELECT, 1, VISIBLE); 	
							lastsel = 1;
						}				
						else lastsel = 0;
					}
					else lastsel = 0;
						
					bp++;
				}
			}
		}
	}
	
	WM_event_add_notifier(C, NC_GEOM | ND_SELECT, obedit->data);

	return OPERATOR_FINISHED;
}

void CURVE_OT_select_less(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Select Less";
	ot->idname = "CURVE_OT_select_less";
	ot->description = "Reduce current selection by deselecting boundary elements";
	
	/* api callbacks */
	ot->exec = select_less_exec;
	ot->poll = ED_operator_editsurfcurve;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/********************** select random *********************/

static void selectrandom_curve(ListBase *editnurb, float randfac)
{
	Nurb *nu;
	BezTriple *bezt;
	BPoint *bp;
	int a;
	
	BLI_srand(BLI_rand()); /* random seed */
	
	for (nu = editnurb->first; nu; nu = nu->next) {
		if (nu->type == CU_BEZIER) {
			bezt = nu->bezt;
			a = nu->pntsu;
			while (a--) {
				if (BLI_frand() < randfac)
					select_beztriple(bezt, SELECT, 1, VISIBLE);
				bezt++;
			}
		}
		else {
			bp = nu->bp;
			a = nu->pntsu * nu->pntsv;
			
			while (a--) {
				if (BLI_frand() < randfac)
					select_bpoint(bp, SELECT, 1, VISIBLE); 
				bp++;
			}
		}		
	}
}

static int select_random_exec(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	ListBase *editnurb = object_editcurve_get(obedit);

	if (!RNA_boolean_get(op->ptr, "extend"))
		CU_deselect_all(obedit);
	
	selectrandom_curve(editnurb, RNA_float_get(op->ptr, "percent") / 100.0f);
	
	WM_event_add_notifier(C, NC_GEOM | ND_SELECT, obedit->data);
	
	return OPERATOR_FINISHED;
}

void CURVE_OT_select_random(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Select Random";
	ot->idname = "CURVE_OT_select_random";
	ot->description = "Randomly select some control points";
	
	/* api callbacks */
	ot->exec = select_random_exec;
	ot->poll = ED_operator_editsurfcurve;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* properties */
	RNA_def_float_percentage(ot->srna, "percent", 50.f, 0.0f, 100.0f, "Percent", "Percentage of elements to select randomly", 0.f, 100.0f);
	RNA_def_boolean(ot->srna, "extend", FALSE, "Extend Selection", "Extend selection instead of deselecting everything first");
}

/********************* every nth number of point *******************/

static int point_on_nurb(Nurb *nu, void *point)
{
	if (nu->bezt) {
		BezTriple *bezt = (BezTriple *)point;
		return bezt >= nu->bezt && bezt < &nu->bezt[nu->pntsu];
	}
	else {
		BPoint *bp = (BPoint *)point;
		return bp >= nu->bp && bp < &nu->bp[nu->pntsu * nu->pntsv];
	}
}

static Nurb *get_lastsel_nurb(Curve *cu)
{
	ListBase *nubase = BKE_curve_editNurbs_get(cu);
	Nurb *nu = nubase->first;

	if (!cu->lastsel)
		return NULL;

	while (nu) {
		if (point_on_nurb(nu, cu->lastsel))
			return nu;

		nu = nu->next;
	}

	return NULL;
}

static void select_nth_bezt(Nurb *nu, BezTriple *bezt, int nth)
{
	int a, start;

	start = bezt - nu->bezt;
	a = nu->pntsu;
	bezt = &nu->bezt[a - 1];

	while (a--) {
		if (abs(start - a) % nth) {
			select_beztriple(bezt, DESELECT, 1, HIDDEN);
		}

		bezt--;
	}
}

static void select_nth_bp(Nurb *nu, BPoint *bp, int nth)
{
	int a, startrow, startpnt;
	int dist, row, pnt;

	startrow = (bp - nu->bp) / nu->pntsu;
	startpnt = (bp - nu->bp) % nu->pntsu;

	a = nu->pntsu * nu->pntsv;
	bp = &nu->bp[a - 1];
	row = nu->pntsv - 1;
	pnt = nu->pntsu - 1;

	while (a--) {
		dist = abs(pnt - startpnt) + abs(row - startrow);
		if (dist % nth) {
			select_bpoint(bp, DESELECT, 1, HIDDEN);
		}

		pnt--;
		if (pnt < 0) {
			pnt = nu->pntsu - 1;
			row--;
		}

		bp--;
	}
}

int CU_select_nth(Object *obedit, int nth)
{
	Curve *cu = (Curve *)obedit->data;
	Nurb *nu;

	nu = get_lastsel_nurb(cu);
	if (!nu)
		return 0;

	if (nu->bezt) {
		select_nth_bezt(nu, cu->lastsel, nth);
	}
	else {
		select_nth_bp(nu, cu->lastsel, nth);
	}

	return 1;
}

static int select_nth_exec(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	int nth = RNA_int_get(op->ptr, "nth");

	if (!CU_select_nth(obedit, nth)) {
		if (obedit->type == OB_SURF) {
			BKE_report(op->reports, RPT_ERROR, "Surface hasn't got active point");
		}
		else {
			BKE_report(op->reports, RPT_ERROR, "Curve hasn't got active point");
		}

		return OPERATOR_CANCELLED;
	}

	WM_event_add_notifier(C, NC_GEOM | ND_SELECT, obedit->data);

	return OPERATOR_FINISHED;
}

void CURVE_OT_select_nth(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Select Nth";
	ot->description = "";
	ot->idname = "CURVE_OT_select_nth";

	/* api callbacks */
	ot->exec = select_nth_exec;
	ot->poll = ED_operator_editsurfcurve;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	RNA_def_int(ot->srna, "nth", 2, 2, 100, "Nth Selection", "", 1, INT_MAX);
}

/********************** add duplicate operator *********************/

static int duplicate_exec(bContext *C, wmOperator *UNUSED(op))
{
	Object *obedit = CTX_data_edit_object(C);

	adduplicateflagNurb(obedit, 1);
	WM_event_add_notifier(C, NC_GEOM | ND_SELECT, obedit->data);

	return OPERATOR_FINISHED;
}

void CURVE_OT_duplicate(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Duplicate Curve";
	ot->description = "Duplicate selected control points and segments between them";
	ot->idname = "CURVE_OT_duplicate";
	
	/* api callbacks */
	ot->exec = duplicate_exec;
	ot->poll = ED_operator_editsurfcurve;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/********************** delete operator *********************/

static int delete_exec(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	Curve *cu = obedit->data;
	EditNurb *editnurb = cu->editnurb;
	ListBase *nubase = &editnurb->nurbs;
	Nurb *nu, *nu1;
	BezTriple *bezt, *bezt1, *bezt2;
	BPoint *bp, *bp1, *bp2;
	int a, cut = 0, type = RNA_enum_get(op->ptr, "type");
	int nuindex = 0;

	if (obedit->type == OB_SURF) {
		if (type == 0) {
			deleteflagNurb(C, op, 1);
		}
		else {
			keyIndex_delNurbList(editnurb, nubase);
			BKE_nurbList_free(nubase);

			if (ED_curve_updateAnimPaths(obedit->data))
				WM_event_add_notifier(C, NC_OBJECT | ND_KEYS, obedit);
		}

		WM_event_add_notifier(C, NC_GEOM | ND_DATA, obedit->data);
		DAG_id_tag_update(obedit->data, 0);
	
		return OPERATOR_FINISHED;
	}

	if (type == 0) {
		/* first loop, can we remove entire pieces? */
		Nurb *next;
		nu = nubase->first;
		while (nu) {
			next = nu->next;
			if (nu->type == CU_BEZIER) {
				bezt = nu->bezt;
				a = nu->pntsu;
				if (a) {
					while (a) {
						if (BEZSELECTED_HIDDENHANDLES(cu, bezt) ) ;
						else break;
						a--;
						bezt++;
					}
					if (a == 0) {
						if (cu->actnu == nuindex)
							cu->actnu = -1;

						BLI_remlink(nubase, nu);
						keyIndex_delNurb(editnurb, nu);
						BKE_nurb_free(nu); nu = NULL;
					}
				}
			}
			else {
				bp = nu->bp;
				a = nu->pntsu * nu->pntsv;
				if (a) {
					while (a) {
						if (bp->f1 & SELECT) ;
						else break;
						a--;
						bp++;
					}
					if (a == 0) {
						if (cu->actnu == nuindex)
							cu->actnu = -1;

						BLI_remlink(nubase, nu);
						keyIndex_delNurb(editnurb, nu);
						BKE_nurb_free(nu); nu = NULL;
					}
				}
			}
			
			/* Never allow the order to exceed the number of points
			 * - note, this is ok but changes unselected nurbs, disable for now */
#if 0
			if ((nu != NULL) && (nu->type == CU_NURBS)) {
				clamp_nurb_order_u(nu);
			}
#endif
			nu = next;
			nuindex++;
		}
		/* 2nd loop, delete small pieces: just for curves */
		nu = nubase->first;
		while (nu) {
			next = nu->next;
			type = 0;
			if (nu->type == CU_BEZIER) {
				int delta = 0;
				bezt = nu->bezt;
				for (a = 0; a < nu->pntsu; a++) {
					if (BEZSELECTED_HIDDENHANDLES(cu, bezt) ) {
						memmove(bezt, bezt + 1, (nu->pntsu - a - 1) * sizeof(BezTriple));
						keyIndex_delBezt(editnurb, bezt + delta);
						keyIndex_updateBezt(editnurb, bezt + 1, bezt, nu->pntsu - a - 1);
						nu->pntsu--;
						a--;
						type = 1;
						delta++;
					}
					else bezt++;
				}
				if (type) {
					bezt1 = (BezTriple *)MEM_mallocN((nu->pntsu) * sizeof(BezTriple), "delNurb");
					memcpy(bezt1, nu->bezt, (nu->pntsu) * sizeof(BezTriple));
					keyIndex_updateBezt(editnurb, nu->bezt, bezt1, nu->pntsu);
					MEM_freeN(nu->bezt);
					nu->bezt = bezt1;
					BKE_nurb_handles_calc(nu);
				}
			}
			else if (nu->pntsv == 1) {
				int delta = 0;
				bp = nu->bp;

				for (a = 0; a < nu->pntsu; a++) {
					if (bp->f1 & SELECT) {
						memmove(bp, bp + 1, (nu->pntsu - a - 1) * sizeof(BPoint));
						keyIndex_delBP(editnurb, bp + delta);
						keyIndex_updateBP(editnurb, bp + 1, bp, nu->pntsu - a - 1);
						nu->pntsu--;
						a--;
						type = 1;
						delta++;
					}
					else {
						bp++;
					}
				}
				if (type) {
					bp1 = (BPoint *)MEM_mallocN(nu->pntsu * sizeof(BPoint), "delNurb2");
					memcpy(bp1, nu->bp, (nu->pntsu) * sizeof(BPoint));
					keyIndex_updateBP(editnurb, nu->bp, bp1, nu->pntsu);
					MEM_freeN(nu->bp);
					nu->bp = bp1;
					
					/* Never allow the order to exceed the number of points
					 * - note, this is ok but changes unselected nurbs, disable for now */
#if 0
					if (nu->type == CU_NURBS) {
						clamp_nurb_order_u(nu);
					}
#endif
				}
				BKE_nurb_order_clamp_u(nu);
				BKE_nurb_knot_calc_u(nu);
			}
			nu = next;
		}
	}
	else if (type == 1) {  /* erase segment */
		/* find the 2 selected points */
		bezt1 = bezt2 = NULL;
		bp1 = bp2 = NULL;
		nu1 = NULL;
		nuindex = 0;
		for (nu = nubase->first; nu; nu = nu->next) {
			if (nu->type == CU_BEZIER) {
				bezt = nu->bezt;
				for (a = 0; a < nu->pntsu - 1; a++) {
					if (BEZSELECTED_HIDDENHANDLES(cu, bezt) ) {
						bezt1 = bezt;
						bezt2 = bezt + 1;
						if ((bezt2->f1 & SELECT) || (bezt2->f2 & SELECT) || (bezt2->f3 & SELECT)) {
							/* pass */
						}
						else {  /* maybe do not make cyclic */
							if (a == 0 && (nu->flagu & CU_NURB_CYCLIC) ) {
								bezt2 = bezt + (nu->pntsu - 1);
								if ( (bezt2->f1 & SELECT) || (bezt2->f2 & SELECT) || (bezt2->f3 & SELECT) ) {
									nu->flagu &= ~CU_NURB_CYCLIC;
									BKE_nurb_handles_calc(nu);
									WM_event_add_notifier(C, NC_GEOM | ND_DATA, obedit->data);
									DAG_id_tag_update(obedit->data, 0);
								}
							}

							return OPERATOR_FINISHED;
						}
						cut = a;
						nu1 = nu;
						break;
					}
					bezt++;
				}
			}
			else if (nu->pntsv == 1) {
				bp = nu->bp;
				for (a = 0; a < nu->pntsu - 1; a++) {
					if (bp->f1 & SELECT) {
						bp1 = bp;
						bp2 = bp + 1;
						if (bp2->f1 & SELECT) {
							/* pass */
						}
						else {  /* maybe do not make cyclic */
							if (a == 0 && (nu->flagu & CU_NURB_CYCLIC) ) {
								bp2 = bp + (nu->pntsu - 1);
								if (bp2->f1 & SELECT) {
									nu->flagu &= ~CU_NURB_CYCLIC;
									WM_event_add_notifier(C, NC_GEOM | ND_DATA, obedit->data);
									DAG_id_tag_update(obedit->data, 0);
								}
							}

							return OPERATOR_FINISHED;
						}
						cut = a;
						nu1 = nu;
						break;
					}
					bp++;
				}
			}
			if (nu1) break;
			nuindex++;
		}
		if (nu1) {
			if (bezt1) {
				if (nu1->pntsu == 2) {    /* remove completely */
					if (cu->actnu == nuindex)
						cu->actnu = -1;

					BLI_remlink(nubase, nu);
					BKE_nurb_free(nu); nu = NULL;
				}
				else if (nu1->flagu & CU_NURB_CYCLIC) { /* cyclic */
					bezt = (BezTriple *)MEM_mallocN((cut + 1) * sizeof(BezTriple), "delNurb1");
					ED_curve_beztcpy(editnurb, bezt, nu1->bezt, cut + 1);
					a = nu1->pntsu - cut - 1;
					ED_curve_beztcpy(editnurb, nu1->bezt, bezt2, a);
					ED_curve_beztcpy(editnurb, &nu1->bezt[a], bezt, cut + 1);

					nu1->flagu &= ~CU_NURB_CYCLIC;
					MEM_freeN(bezt);
					BKE_nurb_handles_calc(nu);
				}
				else {  /* add new curve */

/* seems to be an error here... but where? (a can become zero) */

					nu = (Nurb *)MEM_mallocN(sizeof(Nurb), "delNurb2");
					memcpy(nu, nu1, sizeof(Nurb));
					BLI_addtail(nubase, nu);
					nu->bezt = (BezTriple *)MEM_mallocN((cut + 1) * sizeof(BezTriple), "delNurb3");
					ED_curve_beztcpy(editnurb, nu->bezt, nu1->bezt, cut + 1);
					a = nu1->pntsu - cut - 1;

					bezt = (BezTriple *)MEM_mallocN(a * sizeof(BezTriple), "delNurb4");
					ED_curve_beztcpy(editnurb, bezt, &nu1->bezt[cut + 1], a);
					MEM_freeN(nu1->bezt);
					nu1->bezt = bezt;
					nu1->pntsu = a;
					nu->pntsu = cut + 1;
					
					
					BKE_nurb_handles_calc(nu);
					BKE_nurb_handles_calc(nu1);
				}
			}
			else if (bp1) {
				if (nu1->pntsu == 2) {  /* remove completely */
					if (cu->actnu == nuindex)
						cu->actnu = -1;

					BLI_remlink(nubase, nu);
					BKE_nurb_free(nu); nu = NULL;
				}
				else if (nu1->flagu & CU_NURB_CYCLIC) { /* cyclic */
					bp = (BPoint *)MEM_mallocN((cut + 1) * sizeof(BPoint), "delNurb5");
					ED_curve_bpcpy(editnurb, bp, nu1->bp, cut + 1);
					a = nu1->pntsu - cut - 1;
					ED_curve_bpcpy(editnurb, nu1->bp, bp2, a);
					ED_curve_bpcpy(editnurb, &nu1->bp[a], bp, cut + 1);

					nu1->flagu &= ~CU_NURB_CYCLIC;
					MEM_freeN(bp);
				}
				else {  /* add new curve */
					nu = (Nurb *)MEM_mallocN(sizeof(Nurb), "delNurb6");
					memcpy(nu, nu1, sizeof(Nurb));
					BLI_addtail(nubase, nu);
					nu->bp = (BPoint *)MEM_mallocN((cut + 1) * sizeof(BPoint), "delNurb7");
					ED_curve_bpcpy(editnurb, nu->bp, nu1->bp, cut + 1);
					a = nu1->pntsu - cut - 1;
					bp = (BPoint *)MEM_mallocN(a * sizeof(BPoint), "delNurb8");
					ED_curve_bpcpy(editnurb, bp, &nu1->bp[cut + 1], a);
					MEM_freeN(nu1->bp);
					nu1->bp = bp;
					nu1->pntsu = a;
					nu1->knotsu = NULL;
					nu->pntsu = cut + 1;

					BKE_nurb_order_clamp_u(nu);
					BKE_nurb_knot_calc_u(nu);

					BKE_nurb_order_clamp_u(nu1);
					BKE_nurb_knot_calc_u(nu1);
				}
			}
		}
	}
	else if (type == 2) {
		cu->actnu = -1;
		keyIndex_delNurbList(editnurb, nubase);
		BKE_nurbList_free(nubase);
	}

	if (ED_curve_updateAnimPaths(obedit->data))
		WM_event_add_notifier(C, NC_OBJECT | ND_KEYS, obedit);

	WM_event_add_notifier(C, NC_GEOM | ND_DATA, obedit->data);
	DAG_id_tag_update(obedit->data, 0);
	
	return OPERATOR_FINISHED;
}

static int delete_invoke(bContext *C, wmOperator *op, wmEvent *UNUSED(event))
{
	Object *obedit = CTX_data_edit_object(C);
	uiPopupMenu *pup;
	uiLayout *layout;

	if (obedit->type == OB_SURF) {
		pup = uiPupMenuBegin(C, "Delete", ICON_NONE);
		layout = uiPupMenuLayout(pup);
		uiItemEnumO_ptr(layout, op->type, NULL, 0, "type", 0);
		uiItemEnumO_ptr(layout, op->type, NULL, 0, "type", 2);
		uiPupMenuEnd(C, pup);
	}
	else {
		pup = uiPupMenuBegin(C, "Delete", ICON_NONE);
		layout = uiPupMenuLayout(pup);
		uiItemsEnumO(layout, op->type->idname, "type");
		uiPupMenuEnd(C, pup);
	}

	return OPERATOR_CANCELLED;
}

void CURVE_OT_delete(wmOperatorType *ot)
{
	static EnumPropertyItem type_items[] = {
		{0, "SELECTED", 0, "Select", ""},
		{1, "SEGMENT", 0, "Segment", ""},
		{2, "ALL", 0, "All", ""},
		{0, NULL, 0, NULL, NULL}
	};

	/* identifiers */
	ot->name = "Delete";
	ot->description = "Delete selected control points or segments";
	ot->idname = "CURVE_OT_delete";
	
	/* api callbacks */
	ot->exec = delete_exec;
	ot->invoke = delete_invoke;
	ot->poll = ED_operator_editsurfcurve;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* properties */
	RNA_def_enum(ot->srna, "type", type_items, 0, "Type", "Which elements to delete");
}

/********************** shade smooth/flat operator *********************/

static int shade_smooth_exec(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	ListBase *editnurb = object_editcurve_get(obedit);
	Nurb *nu;
	int clear = (strcmp(op->idname, "CURVE_OT_shade_flat") == 0);
	
	if (obedit->type != OB_CURVE)
		return OPERATOR_CANCELLED;
	
	for (nu = editnurb->first; nu; nu = nu->next) {
		if (isNurbsel(nu)) {
			if (!clear) nu->flag |= CU_SMOOTH;
			else nu->flag &= ~CU_SMOOTH;
		}
	}
	
	WM_event_add_notifier(C, NC_GEOM | ND_DATA, obedit->data);
	DAG_id_tag_update(obedit->data, 0);

	return OPERATOR_FINISHED;
}

void CURVE_OT_shade_smooth(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Shade Smooth";
	ot->idname = "CURVE_OT_shade_smooth";
	ot->description = "Set shading to smooth";
	
	/* api callbacks */
	ot->exec = shade_smooth_exec;
	ot->poll = ED_operator_editsurfcurve;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

void CURVE_OT_shade_flat(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Shade Flat";
	ot->idname = "CURVE_OT_shade_flat";
	ot->description = "Set shading to flat";
	
	/* api callbacks */
	ot->exec = shade_smooth_exec;
	ot->poll = ED_operator_editsurfcurve;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/************** join operator, to be used externally? ****************/

int join_curve_exec(bContext *C, wmOperator *UNUSED(op))
{
	Main *bmain = CTX_data_main(C);
	Scene *scene = CTX_data_scene(C);
	Object *ob = CTX_data_active_object(C);
	Curve *cu;
	Nurb *nu, *newnu;
	BezTriple *bezt;
	BPoint *bp;
	ListBase tempbase;
	float imat[4][4], cmat[4][4];
	int a;

	tempbase.first = tempbase.last = NULL;
	
	/* trasnform all selected curves inverse in obact */
	invert_m4_m4(imat, ob->obmat);
	
	CTX_DATA_BEGIN(C, Base *, base, selected_editable_bases)
	{
		if (base->object->type == ob->type) {
			if (base->object != ob) {
			
				cu = base->object->data;
			
				if (cu->nurb.first) {
					/* watch it: switch order here really goes wrong */
					mult_m4_m4m4(cmat, imat, base->object->obmat);
					
					nu = cu->nurb.first;
					while (nu) {
						newnu = BKE_nurb_duplicate(nu);
						if (ob->totcol) { /* TODO, merge material lists */
							CLAMP(newnu->mat_nr, 0, ob->totcol - 1);
						}
						else newnu->mat_nr = 0;
						BLI_addtail(&tempbase, newnu);
						
						if ( (bezt = newnu->bezt) ) {
							a = newnu->pntsu;
							while (a--) {
								mul_m4_v3(cmat, bezt->vec[0]);
								mul_m4_v3(cmat, bezt->vec[1]);
								mul_m4_v3(cmat, bezt->vec[2]);
								bezt++;
							}
							BKE_nurb_handles_calc(newnu);
						}
						if ( (bp = newnu->bp) ) {
							a = newnu->pntsu * nu->pntsv;
							while (a--) {
								mul_m4_v3(cmat, bp->vec);
								bp++;
							}
						}
						nu = nu->next;
					}
				}
			
				ED_base_object_free_and_unlink(bmain, scene, base);
			}
		}
	}
	CTX_DATA_END;
	
	cu = ob->data;
	BLI_movelisttolist(&cu->nurb, &tempbase);
	
	DAG_scene_sort(bmain, scene);   // because we removed object(s), call before editmode!
	
	ED_object_enter_editmode(C, EM_WAITCURSOR);
	ED_object_exit_editmode(C, EM_FREEDATA | EM_WAITCURSOR | EM_DO_UNDO);

	WM_event_add_notifier(C, NC_SCENE | ND_OB_ACTIVE, scene);

	return OPERATOR_FINISHED;
}

/************ add primitive, used by object/ module ****************/

static const char *get_curve_defname(int type)
{
	int stype = type & CU_PRIMITIVE;

	if ((type & CU_TYPE) == CU_BEZIER) {
		switch (stype) {
			case CU_PRIM_CURVE: return "BezierCurve";
			case CU_PRIM_CIRCLE: return "BezierCircle";
			case CU_PRIM_PATH: return "CurvePath";
			default:
				return "Curve";
		}
	}
	else {
		switch (stype) {
			case CU_PRIM_CURVE: return "NurbsCurve";
			case CU_PRIM_CIRCLE: return "NurbsCircle";
			case CU_PRIM_PATH: return "NurbsPath";
			default:
				return "Curve";
		}
	}
}

static const char *get_surf_defname(int type)
{
	int stype = type & CU_PRIMITIVE;

	switch (stype) {
		case CU_PRIM_CURVE: return "SurfCurve";
		case CU_PRIM_CIRCLE: return "SurfCircle";
		case CU_PRIM_PATCH: return "SurfPatch";
		case CU_PRIM_SPHERE: return "SurfSphere";
		case CU_PRIM_DONUT: return "SurfTorus";
		default:
			return "Surface";
	}
}


Nurb *add_nurbs_primitive(bContext *C, float mat[4][4], int type, int newob)
{
	static int xzproj = 0;   /* this function calls itself... */
	Object *obedit = CTX_data_edit_object(C);
	ListBase *editnurb = object_editcurve_get(obedit);
	View3D *v3d = CTX_wm_view3d(C);
	RegionView3D *rv3d = ED_view3d_context_rv3d(C);
	Nurb *nu = NULL;
	BezTriple *bezt;
	BPoint *bp;
	Curve *cu = (Curve *)obedit->data;
	float vec[3], zvec[3] = {0.0f, 0.0f, 1.0f};
	float umat[4][4] = MAT4_UNITY, viewmat[4][4] = MAT4_UNITY;
	float fac;
	int a, b;
	const float grid = v3d ? v3d->grid : 1.0f;
	const int cutype = (type & CU_TYPE); // poly, bezier, nurbs, etc
	const int stype = (type & CU_PRIMITIVE);
	const int force_3d = ((Curve *)obedit->data)->flag & CU_3D; /* could be adding to an existing 3D curve */

	if (rv3d) {
		copy_m4_m4(viewmat, rv3d->viewmat);
		copy_v3_v3(zvec, rv3d->viewinv[2]);
	}

	setflagsNurb(editnurb, 0);
	
	/* these types call this function to return a Nurb */
	if (stype != CU_PRIM_TUBE && stype != CU_PRIM_DONUT) {
		nu = (Nurb *)MEM_callocN(sizeof(Nurb), "addNurbprim");
		nu->type = cutype;
		nu->resolu = cu->resolu;
		nu->resolv = cu->resolv;
	}

	switch (stype) {
		case CU_PRIM_CURVE: /* curve */
			nu->resolu = cu->resolu;
			if (cutype == CU_BEZIER) {
				if (!force_3d) nu->flag |= CU_2D;
				nu->pntsu = 2;
				nu->bezt = (BezTriple *)MEM_callocN(2 * sizeof(BezTriple), "addNurbprim1");
				bezt = nu->bezt;
				bezt->h1 = bezt->h2 = HD_ALIGN;
				bezt->f1 = bezt->f2 = bezt->f3 = SELECT;
				bezt->radius = 1.0;

				bezt->vec[1][0] += -grid;
				bezt->vec[0][0] += -1.5f * grid;
				bezt->vec[0][1] += -0.5f * grid;
				bezt->vec[2][0] += -0.5f * grid;
				bezt->vec[2][1] +=  0.5f * grid;
				for (a = 0; a < 3; a++) mul_m4_v3(mat, bezt->vec[a]);

				bezt++;
				bezt->h1 = bezt->h2 = HD_ALIGN;
				bezt->f1 = bezt->f2 = bezt->f3 = SELECT;
				bezt->radius = bezt->weight = 1.0;

				bezt->vec[0][0] = 0;
				bezt->vec[0][1] = 0;
				bezt->vec[1][0] = grid;
				bezt->vec[1][1] = 0;
				bezt->vec[2][0] = grid * 2;
				bezt->vec[2][1] = 0;
				for (a = 0; a < 3; a++) mul_m4_v3(mat, bezt->vec[a]);

				BKE_nurb_handles_calc(nu);
			}
			else {

				nu->pntsu = 4;
				nu->pntsv = 1;
				nu->orderu = 4;
				nu->bp = callocstructN(BPoint, 4, "addNurbprim3");

				bp = nu->bp;
				for (a = 0; a < 4; a++, bp++) {
					bp->vec[3] = 1.0;
					bp->f1 = SELECT;
					bp->radius = bp->weight = 1.0;
				}

				bp = nu->bp;
				bp->vec[0] += -1.5f * grid;
				bp++;
				bp->vec[0] += -grid;
				bp->vec[1] +=  grid;
				bp++;
				bp->vec[0] += grid;
				bp->vec[1] += grid;
				bp++;
				bp->vec[0] += 1.5f * grid;

				bp = nu->bp;
				for (a = 0; a < 4; a++, bp++) mul_m4_v3(mat, bp->vec);

				if (cutype == CU_NURBS) {
					nu->knotsu = NULL; /* nurbs_knot_calc_u allocates */
					BKE_nurb_knot_calc_u(nu);
				}
			
			}
			break;
		case CU_PRIM_PATH: /* 5 point path */
			nu->pntsu = 5;
			nu->pntsv = 1;
			nu->orderu = 5;
			nu->flagu = CU_NURB_ENDPOINT; /* endpoint */
			nu->resolu = cu->resolu;
			nu->bp = callocstructN(BPoint, 5, "addNurbprim3");

			bp = nu->bp;
			for (a = 0; a < 5; a++, bp++) {
				bp->vec[3] = 1.0;
				bp->f1 = SELECT;
				bp->radius = bp->weight = 1.0;
			}

			bp = nu->bp;
			bp->vec[0] += -2.0f * grid;
			bp++;
			bp->vec[0] += -grid;
			bp++; bp++;
			bp->vec[0] += grid;
			bp++;
			bp->vec[0] += 2.0f * grid;

			bp = nu->bp;
			for (a = 0; a < 5; a++, bp++) mul_m4_v3(mat, bp->vec);

			if (cutype == CU_NURBS) {
				nu->knotsu = NULL; /* nurbs_knot_calc_u allocates */
				BKE_nurb_knot_calc_u(nu);
			}

			break;
		case CU_PRIM_CIRCLE: /* circle */
			nu->resolu = cu->resolu;
			
			if (cutype == CU_BEZIER) {
				if (!force_3d) nu->flag |= CU_2D;
				nu->pntsu = 4;
				nu->bezt = callocstructN(BezTriple, 4, "addNurbprim1");
				nu->flagu = CU_NURB_CYCLIC;
				bezt = nu->bezt;

				bezt->h1 = bezt->h2 = HD_AUTO;
				bezt->f1 = bezt->f2 = bezt->f3 = SELECT;
				bezt->vec[1][0] += -grid;
				for (a = 0; a < 3; a++) mul_m4_v3(mat, bezt->vec[a]);
				bezt->radius = bezt->weight = 1.0;

				bezt++;
				bezt->h1 = bezt->h2 = HD_AUTO;
				bezt->f1 = bezt->f2 = bezt->f3 = SELECT;
				bezt->vec[1][1] += grid;
				for (a = 0; a < 3; a++) mul_m4_v3(mat, bezt->vec[a]);
				bezt->radius = bezt->weight = 1.0;

				bezt++;
				bezt->h1 = bezt->h2 = HD_AUTO;
				bezt->f1 = bezt->f2 = bezt->f3 = SELECT;
				bezt->vec[1][0] += grid;
				for (a = 0; a < 3; a++) mul_m4_v3(mat, bezt->vec[a]);
				bezt->radius = bezt->weight = 1.0;

				bezt++;
				bezt->h1 = bezt->h2 = HD_AUTO;
				bezt->f1 = bezt->f2 = bezt->f3 = SELECT;
				bezt->vec[1][1] += -grid;
				for (a = 0; a < 3; a++) mul_m4_v3(mat, bezt->vec[a]);
				bezt->radius = bezt->weight = 1.0;
				
				BKE_nurb_handles_calc(nu);
			}
			else if (cutype == CU_NURBS) { /* nurb */
				nu->pntsu = 8;
				nu->pntsv = 1;
				nu->orderu = 4;
				nu->bp = callocstructN(BPoint, 8, "addNurbprim6");
				nu->flagu = CU_NURB_CYCLIC;
				bp = nu->bp;

				for (a = 0; a < 8; a++) {
					bp->f1 = SELECT;
					if (xzproj == 0) {
						bp->vec[0] += nurbcircle[a][0] * grid;
						bp->vec[1] += nurbcircle[a][1] * grid;
					}
					else {
						bp->vec[0] += 0.25f * nurbcircle[a][0] * grid - 0.75f * grid;
						bp->vec[2] += 0.25f * nurbcircle[a][1] * grid;
					}
					if (a & 1) bp->vec[3] = 0.25 * M_SQRT2;
					else bp->vec[3] = 1.0;
					mul_m4_v3(mat, bp->vec);
					bp->radius = bp->weight = 1.0;

					bp++;
				}

				BKE_nurb_knot_calc_u(nu);
			}
			break;
		case CU_PRIM_PATCH: /* 4x4 patch */
			if (cutype == CU_NURBS) { /* nurb */

				nu->pntsu = 4;
				nu->pntsv = 4;
				nu->orderu = 4;
				nu->orderv = 4;
				nu->flag = CU_SMOOTH;
				nu->bp = callocstructN(BPoint, 4 * 4, "addNurbprim6");
				nu->flagu = 0;
				nu->flagv = 0;
				bp = nu->bp;

				for (a = 0; a < 4; a++) {
					for (b = 0; b < 4; b++) {
						bp->f1 = SELECT;
						fac = (float)a - 1.5f;
						bp->vec[0] += fac * grid;
						fac = (float)b - 1.5f;
						bp->vec[1] += fac * grid;
						if (a == 1 || a == 2) if (b == 1 || b == 2) {
								bp->vec[2] += grid;
							}
						mul_m4_v3(mat, bp->vec);
						bp->vec[3] = 1.0;
						bp++;
					}
				}

				BKE_nurb_knot_calc_u(nu);
				BKE_nurb_knot_calc_v(nu);
			}
			break;
		case CU_PRIM_TUBE: /* Cylinder */
			if (cutype == CU_NURBS) {
				nu = add_nurbs_primitive(C, mat, CU_NURBS | CU_PRIM_CIRCLE, 0); /* circle */
				nu->resolu = cu->resolu;
				nu->flag = CU_SMOOTH;
				BLI_addtail(editnurb, nu); /* temporal for extrude and translate */
				vec[0] = vec[1] = 0.0;
				vec[2] = -grid;

				if (newob && (U.flag & USER_ADD_VIEWALIGNED) == 0) {
					/* pass */
				}
				else {
					mul_mat3_m4_v3(mat, vec);
				}

				translateflagNurb(editnurb, 1, vec);
				extrudeflagNurb(cu->editnurb, 1);
				mul_v3_fl(vec, -2.0f);
				translateflagNurb(editnurb, 1, vec);

				BLI_remlink(editnurb, nu);

				a = nu->pntsu * nu->pntsv;
				bp = nu->bp;
				while (a-- > 0) {
					bp->f1 |= SELECT;
					bp++;
				}
			}
			break;
		case CU_PRIM_SPHERE: /* sphere */
			if (cutype == CU_NURBS) {
				float tmp_cent[3] = {0.f, 0.f, 0.f};
				float tmp_vec[3] = {0.f, 0.f, 1.f};

				nu->pntsu = 5;
				nu->pntsv = 1;
				nu->orderu = 3;
				nu->resolu = cu->resolu;
				nu->resolv = cu->resolv;
				nu->flag = CU_SMOOTH;
				nu->bp = callocstructN(BPoint, 5, "addNurbprim6");
				nu->flagu = 0;
				bp = nu->bp;

				for (a = 0; a < 5; a++) {
					bp->f1 = SELECT;
					bp->vec[0] += nurbcircle[a][0] * grid;
					bp->vec[2] += nurbcircle[a][1] * grid;
					if (a & 1) bp->vec[3] = 0.5 * M_SQRT2;
					else bp->vec[3] = 1.0;
					mul_m4_v3(mat, bp->vec);
					bp++;
				}
				nu->flagu = CU_NURB_BEZIER;
				BKE_nurb_knot_calc_u(nu);

				BLI_addtail(editnurb, nu); /* temporal for spin */

				if (newob && (U.flag & USER_ADD_VIEWALIGNED) == 0) spin_nurb(umat, obedit, tmp_vec, tmp_cent);
				else if ((U.flag & USER_ADD_VIEWALIGNED)) spin_nurb(viewmat, obedit, zvec, mat[3]);
				else spin_nurb(umat, obedit, tmp_vec, mat[3]);

				BKE_nurb_knot_calc_v(nu);

				a = nu->pntsu * nu->pntsv;
				bp = nu->bp;
				while (a-- > 0) {
					bp->f1 |= SELECT;
					bp++;
				}
				BLI_remlink(editnurb, nu);
			}
			break;
		case CU_PRIM_DONUT: /* torus */
			if (cutype == CU_NURBS) {
				float tmp_cent[3] = {0.f, 0.f, 0.f};
				float tmp_vec[3] = {0.f, 0.f, 1.f};

				xzproj = 1;
				nu = add_nurbs_primitive(C, mat, CU_NURBS | CU_PRIM_CIRCLE, 0); /* circle */
				xzproj = 0;
				nu->resolu = cu->resolu;
				nu->resolv = cu->resolv;
				nu->flag = CU_SMOOTH;
				BLI_addtail(editnurb, nu); /* temporal for spin */

				/* same as above */
				if (newob && (U.flag & USER_ADD_VIEWALIGNED) == 0) spin_nurb(umat, obedit, tmp_vec, tmp_cent);
				else if ((U.flag & USER_ADD_VIEWALIGNED)) spin_nurb(viewmat, obedit, zvec, mat[3]);
				else spin_nurb(umat, obedit, tmp_vec, mat[3]);


				BLI_remlink(editnurb, nu);

				a = nu->pntsu * nu->pntsv;
				bp = nu->bp;
				while (a-- > 0) {
					bp->f1 |= SELECT;
					bp++;
				}

			}
			break;

		default: /* should never happen */
			BLI_assert(!"invalid nurbs type");
			return NULL;
	}

	BLI_assert(nu != NULL);

	if (nu) { /* should always be set */
		nu->flag |= CU_SMOOTH;

		BKE_nurb_test2D(nu);
	}

	return nu;
}

static int curvesurf_prim_add(bContext *C, wmOperator *op, int type, int isSurf)
{
	Object *obedit = CTX_data_edit_object(C);
	ListBase *editnurb;
	Nurb *nu;
	int newob = 0;
	int enter_editmode, is_aligned;
	unsigned int layer;
	float loc[3], rot[3];
	float mat[4][4];

	if (!ED_object_add_generic_get_opts(C, op, loc, rot, &enter_editmode, &layer, &is_aligned))
		return OPERATOR_CANCELLED;

	if (!isSurf) { /* adding curve */
		if (obedit == NULL || obedit->type != OB_CURVE) {
			Curve *cu;
			
			obedit = ED_object_add_type(C, OB_CURVE, loc, rot, TRUE, layer);
			newob = 1;

			cu = (Curve *)obedit->data;
			cu->flag |= CU_DEFORM_FILL;
			
			if (type & CU_PRIM_PATH)
				cu->flag |= CU_PATH | CU_3D;
		} 
		else DAG_id_tag_update(&obedit->id, OB_RECALC_DATA);
	} 
	else { /* adding surface */
		if (obedit == NULL || obedit->type != OB_SURF) {
			obedit = ED_object_add_type(C, OB_SURF, loc, rot, TRUE, layer);
			newob = 1;
		} 
		else DAG_id_tag_update(&obedit->id, OB_RECALC_DATA);
	}

	/* rename here, the undo stack checks name for valid undo pushes */
	if (newob) {

		if (obedit->type == OB_CURVE) {
			rename_id((ID *)obedit, get_curve_defname(type));
			rename_id((ID *)obedit->data, get_curve_defname(type));
		}
		else {
			rename_id((ID *)obedit, get_surf_defname(type));
			rename_id((ID *)obedit->data, get_surf_defname(type));
		}
	}
	
	/* ED_object_add_type doesnt do an undo, is needed for redo operator on primitive */
	if (newob && enter_editmode)
		ED_undo_push(C, "Enter Editmode");
	
	ED_object_new_primitive_matrix(C, obedit, loc, rot, mat);

	nu = add_nurbs_primitive(C, mat, type, newob);
	editnurb = object_editcurve_get(obedit);
	BLI_addtail(editnurb, nu);

	/* userdef */
	if (newob && !enter_editmode) {
		ED_object_exit_editmode(C, EM_FREEDATA);
	}

	WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, obedit);

	return OPERATOR_FINISHED;
}

static int curve_prim_add(bContext *C, wmOperator *op, int type)
{
	return curvesurf_prim_add(C, op, type, 0);
}

static int surf_prim_add(bContext *C, wmOperator *op, int type)
{
	return curvesurf_prim_add(C, op, type, 1);
}

/* ******************** Curves ******************* */

static int add_primitive_bezier_exec(bContext *C, wmOperator *op)
{
	return curve_prim_add(C, op, CU_BEZIER | CU_PRIM_CURVE);
}

void CURVE_OT_primitive_bezier_curve_add(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Add Bezier";
	ot->description = "Construct a Bezier Curve";
	ot->idname = "CURVE_OT_primitive_bezier_curve_add";
	
	/* api callbacks */
	ot->invoke = ED_object_add_generic_invoke;
	ot->exec = add_primitive_bezier_exec;
	ot->poll = ED_operator_scene_editable;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	ED_object_add_generic_props(ot, TRUE);
}

static int add_primitive_bezier_circle_exec(bContext *C, wmOperator *op)
{
	return curve_prim_add(C, op, CU_BEZIER | CU_PRIM_CIRCLE);
}

void CURVE_OT_primitive_bezier_circle_add(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Add Bezier Circle";
	ot->description = "Construct a Bezier Circle";
	ot->idname = "CURVE_OT_primitive_bezier_circle_add";
	
	/* api callbacks */
	ot->invoke = ED_object_add_generic_invoke;
	ot->exec = add_primitive_bezier_circle_exec;
	ot->poll = ED_operator_scene_editable;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	ED_object_add_generic_props(ot, TRUE);
}

static int add_primitive_nurbs_curve_exec(bContext *C, wmOperator *op)
{
	return curve_prim_add(C, op, CU_NURBS | CU_PRIM_CURVE);
}

void CURVE_OT_primitive_nurbs_curve_add(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Add Nurbs Curve";
	ot->description = "Construct a Nurbs Curve";
	ot->idname = "CURVE_OT_primitive_nurbs_curve_add";
	
	/* api callbacks */
	ot->invoke = ED_object_add_generic_invoke;
	ot->exec = add_primitive_nurbs_curve_exec;
	ot->poll = ED_operator_scene_editable;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	ED_object_add_generic_props(ot, TRUE);
}

static int add_primitive_nurbs_circle_exec(bContext *C, wmOperator *op)
{
	return curve_prim_add(C, op, CU_NURBS | CU_PRIM_CIRCLE);
}

void CURVE_OT_primitive_nurbs_circle_add(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Add Nurbs Circle";
	ot->description = "Construct a Nurbs Circle";
	ot->idname = "CURVE_OT_primitive_nurbs_circle_add";
	
	/* api callbacks */
	ot->invoke = ED_object_add_generic_invoke;
	ot->exec = add_primitive_nurbs_circle_exec;
	ot->poll = ED_operator_scene_editable;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	ED_object_add_generic_props(ot, TRUE);
}

static int add_primitive_curve_path_exec(bContext *C, wmOperator *op)
{
	return curve_prim_add(C, op, CU_NURBS | CU_PRIM_PATH);
}

void CURVE_OT_primitive_nurbs_path_add(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Add Path";
	ot->description = "Construct a Path";
	ot->idname = "CURVE_OT_primitive_nurbs_path_add";
	
	/* api callbacks */
	ot->invoke = ED_object_add_generic_invoke;
	ot->exec = add_primitive_curve_path_exec;
	ot->poll = ED_operator_scene_editable;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	ED_object_add_generic_props(ot, TRUE);
}

/* **************** NURBS surfaces ********************** */
static int add_primitive_nurbs_surface_curve_exec(bContext *C, wmOperator *op)
{
	return surf_prim_add(C, op, CU_PRIM_CURVE | CU_NURBS);
}

void SURFACE_OT_primitive_nurbs_surface_curve_add(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Add Surface Curve";
	ot->description = "Construct a Nurbs surface Curve";
	ot->idname = "SURFACE_OT_primitive_nurbs_surface_curve_add";
	
	/* api callbacks */
	ot->invoke = ED_object_add_generic_invoke;
	ot->exec = add_primitive_nurbs_surface_curve_exec;
	ot->poll = ED_operator_scene_editable;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	ED_object_add_generic_props(ot, TRUE);
}

static int add_primitive_nurbs_surface_circle_exec(bContext *C, wmOperator *op)
{
	return surf_prim_add(C, op, CU_PRIM_CIRCLE | CU_NURBS);
}

void SURFACE_OT_primitive_nurbs_surface_circle_add(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Add Surface Circle";
	ot->description = "Construct a Nurbs surface Circle";
	ot->idname = "SURFACE_OT_primitive_nurbs_surface_circle_add";
	
	/* api callbacks */
	ot->invoke = ED_object_add_generic_invoke;
	ot->exec = add_primitive_nurbs_surface_circle_exec;
	ot->poll = ED_operator_scene_editable;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	ED_object_add_generic_props(ot, TRUE);
}

static int add_primitive_nurbs_surface_surface_exec(bContext *C, wmOperator *op)
{
	return surf_prim_add(C, op, CU_PRIM_PATCH | CU_NURBS);
}

void SURFACE_OT_primitive_nurbs_surface_surface_add(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Add Surface Patch";
	ot->description = "Construct a Nurbs surface Patch";
	ot->idname = "SURFACE_OT_primitive_nurbs_surface_surface_add";
	
	/* api callbacks */
	ot->invoke = ED_object_add_generic_invoke;
	ot->exec = add_primitive_nurbs_surface_surface_exec;
	ot->poll = ED_operator_scene_editable;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	ED_object_add_generic_props(ot, TRUE);
}

static int add_primitive_nurbs_surface_cylinder_exec(bContext *C, wmOperator *op)
{
	return surf_prim_add(C, op, CU_PRIM_TUBE | CU_NURBS);
}

void SURFACE_OT_primitive_nurbs_surface_cylinder_add(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Add Surface Cylinder";
	ot->description = "Construct a Nurbs surface Cylinder";
	ot->idname = "SURFACE_OT_primitive_nurbs_surface_cylinder_add";
	
	/* api callbacks */
	ot->invoke = ED_object_add_generic_invoke;
	ot->exec = add_primitive_nurbs_surface_cylinder_exec;
	ot->poll = ED_operator_scene_editable;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	ED_object_add_generic_props(ot, TRUE);
}

static int add_primitive_nurbs_surface_sphere_exec(bContext *C, wmOperator *op)
{
	return surf_prim_add(C, op, CU_PRIM_SPHERE | CU_NURBS);
}

void SURFACE_OT_primitive_nurbs_surface_sphere_add(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Add Surface Sphere";
	ot->description = "Construct a Nurbs surface Sphere";
	ot->idname = "SURFACE_OT_primitive_nurbs_surface_sphere_add";
	
	/* api callbacks */
	ot->invoke = ED_object_add_generic_invoke;
	ot->exec = add_primitive_nurbs_surface_sphere_exec;
	ot->poll = ED_operator_scene_editable;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	ED_object_add_generic_props(ot, TRUE);
}

static int add_primitive_nurbs_surface_torus_exec(bContext *C, wmOperator *op)
{
	return surf_prim_add(C, op, CU_PRIM_DONUT | CU_NURBS);
}

void SURFACE_OT_primitive_nurbs_surface_torus_add(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Add Surface Torus";
	ot->description = "Construct a Nurbs surface Torus";
	ot->idname = "SURFACE_OT_primitive_nurbs_surface_torus_add";
	
	/* api callbacks */
	ot->invoke = ED_object_add_generic_invoke;
	ot->exec = add_primitive_nurbs_surface_torus_exec;
	ot->poll = ED_operator_scene_editable;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	ED_object_add_generic_props(ot, TRUE);
}

/***************** clear tilt operator ********************/

static int clear_tilt_exec(bContext *C, wmOperator *UNUSED(op))
{
	Object *obedit = CTX_data_edit_object(C);
	Curve *cu = obedit->data;
	ListBase *editnurb = object_editcurve_get(obedit);
	Nurb *nu;
	BezTriple *bezt;
	BPoint *bp;
	int a;

	for (nu = editnurb->first; nu; nu = nu->next) {
		if (nu->bezt) {
			bezt = nu->bezt;
			a = nu->pntsu;
			while (a--) {
				if (BEZSELECTED_HIDDENHANDLES(cu, bezt)) bezt->alfa = 0.0;
				bezt++;
			}
		}
		else if (nu->bp) {
			bp = nu->bp;
			a = nu->pntsu * nu->pntsv;
			while (a--) {
				if (bp->f1 & SELECT) bp->alfa = 0.0f;
				bp++;
			}
		}
	}

	WM_event_add_notifier(C, NC_GEOM | ND_DATA, obedit->data);
	DAG_id_tag_update(obedit->data, 0);

	return OPERATOR_FINISHED;
}

void CURVE_OT_tilt_clear(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Clear Tilt";
	ot->idname = "CURVE_OT_tilt_clear";
	ot->description = "Clear the tilt of selected control points";
	
	/* api callbacks */
	ot->exec = clear_tilt_exec;
	ot->poll = ED_operator_editcurve;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/****************** undo for curves ****************/

static void *undo_check_lastsel(void *lastsel, Nurb *nu, Nurb *newnu)
{
	if (nu->bezt) {
		BezTriple *lastbezt = (BezTriple *)lastsel;
		if (lastbezt >= nu->bezt && lastbezt < &nu->bezt[nu->pntsu]) {
			return &newnu->bezt[lastbezt - nu->bezt];
		}
	}
	else {
		BPoint *lastbp = (BPoint *)lastsel;
		if (lastbp >= nu->bp && lastbp < &nu->bp[nu->pntsu * nu->pntsv]) {
			return &newnu->bp[lastbp - nu->bp];
		}
	}

	return NULL;
}

static void undoCurve_to_editCurve(void *ucu, void *UNUSED(edata), void *cu_v)
{
	Curve *cu = cu_v;
	UndoCurve *undoCurve = ucu;
	ListBase *undobase = &undoCurve->nubase;
	ListBase *editbase = BKE_curve_editNurbs_get(cu);
	Nurb *nu, *newnu;
	EditNurb *editnurb = cu->editnurb;
	void *lastsel = NULL;
	AnimData *ad = BKE_animdata_from_id(&cu->id);

	BKE_nurbList_free(editbase);

	if (undoCurve->undoIndex) {
		BLI_ghash_free(editnurb->keyindex, NULL, (GHashValFreeFP)MEM_freeN);
		editnurb->keyindex = dupli_keyIndexHash(undoCurve->undoIndex);
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

		if (lastsel == NULL) {
			lastsel = undo_check_lastsel(undoCurve->lastsel, nu, newnu);
		}

		if (editnurb->keyindex) {
			keyIndex_updateNurb(editnurb, nu, newnu);
		}

		BLI_addtail(editbase, newnu);
	}

	cu->lastsel = lastsel;
	cu->actnu = undoCurve->actnu;
	ED_curve_updateAnimPaths(cu);
}

static void *editCurve_to_undoCurve(void *UNUSED(edata), void *cu_v)
{
	Curve *cu = cu_v;
	ListBase *nubase = BKE_curve_editNurbs_get(cu);
	UndoCurve *undoCurve;
	EditNurb *editnurb = cu->editnurb, tmpEditnurb;
	Nurb *nu, *newnu;
	void *lastsel = NULL;
	AnimData *ad = BKE_animdata_from_id(&cu->id);

	undoCurve = MEM_callocN(sizeof(UndoCurve), "undoCurve");

	if (editnurb->keyindex) {
		undoCurve->undoIndex = dupli_keyIndexHash(editnurb->keyindex);
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

		if (lastsel == NULL) {
			lastsel = undo_check_lastsel(cu->lastsel, nu, newnu);
		}

		if (undoCurve->undoIndex) {
			keyIndex_updateNurb(&tmpEditnurb, nu, newnu);
		}

		BLI_addtail(&undoCurve->nubase, newnu);
	}

	undoCurve->lastsel = lastsel;
	undoCurve->actnu = cu->actnu;

	return undoCurve;
}

static void free_undoCurve(void *ucv)
{
	UndoCurve *undoCurve = ucv;

	BKE_nurbList_free(&undoCurve->nubase);

	if (undoCurve->undoIndex)
		BLI_ghash_free(undoCurve->undoIndex, NULL, (GHashValFreeFP)MEM_freeN);

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

void ED_curve_beztcpy(EditNurb *editnurb, BezTriple *dst, BezTriple *src, int count)
{
	memcpy(dst, src, count * sizeof(BezTriple));
	keyIndex_updateBezt(editnurb, src, dst, count);
}

void ED_curve_bpcpy(EditNurb *editnurb, BPoint *dst, BPoint *src, int count)
{
	memcpy(dst, src, count * sizeof(BPoint));
	keyIndex_updateBP(editnurb, src, dst, count);
}

int ED_curve_actSelection(Curve *cu, float center[3])
{
	Nurb *nu = get_lastsel_nurb(cu);

	if (!nu)
		return 0;

	if (nu->bezt) {
		BezTriple *bezt = cu->lastsel;

		copy_v3_v3(center, bezt->vec[1]);
	}
	else {
		BPoint *bp = cu->lastsel;

		copy_v3_v3(center, bp->vec);
	}

	return 1;
}
