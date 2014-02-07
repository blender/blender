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

#include "DNA_key_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_anim_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_bitmap.h"
#include "BLI_math.h"
#include "BLI_rand.h"
#include "BLI_ghash.h"

#include "BLF_translation.h"

#include "BKE_context.h"
#include "BKE_curve.h"
#include "BKE_depsgraph.h"
#include "BKE_displist.h"
#include "BKE_fcurve.h"
#include "BKE_global.h"
#include "BKE_key.h"
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
	int actvert;
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

typedef enum eVisible_Types {
	HIDDEN = true,
	VISIBLE = false,
} eVisible_Types;

typedef enum eEndPoint_Types {
	FIRST = true,
	LAST = false,
} eEndPoint_Types;

typedef enum eCurveElem_Types {
	CURVE_VERTEX = 0,
	CURVE_SEGMENT,
} eCurveElem_Types;

void selectend_nurb(Object *obedit, enum eEndPoint_Types selfirst, bool doswap, bool selstatus);
static void select_adjacent_cp(ListBase *editnurb, short next, const bool cont, const bool selstatus);
static void adduplicateflagNurb(Object *obedit, ListBase *newnurb, const short flag, const bool split);
static int curve_delete_segments(Object *obedit, const bool split);

ListBase *object_editcurve_get(Object *ob)
{
	if (ob && ELEM(ob->type, OB_CURVE, OB_SURF)) {
		Curve *cu = ob->data;
		return &cu->editnurb->nurbs;
	}
	return NULL;
}

/* ******************* SELECTION FUNCTIONS ********************* */


/* returns 1 in case (de)selection was successful */
static bool select_beztriple(BezTriple *bezt, bool selstatus, short flag, eVisible_Types hidden)
{	
	if ((bezt->hide == 0) || (hidden == HIDDEN)) {
		if (selstatus == SELECT) { /* selects */
			bezt->f1 |= flag;
			bezt->f2 |= flag;
			bezt->f3 |= flag;
			return true;
		}
		else { /* deselects */
			bezt->f1 &= ~flag;
			bezt->f2 &= ~flag;
			bezt->f3 &= ~flag;
			return true;
		}
	}
	
	return false;
}

/* returns 1 in case (de)selection was successful */
static bool select_bpoint(BPoint *bp, bool selstatus, short flag, bool hidden)
{	
	if ((bp->hide == 0) || (hidden == 1)) {
		if (selstatus == SELECT) {
			bp->f1 |= flag;
			return true;
		}
		else {
			bp->f1 &= ~flag;
			return true;
		}
	}

	return false;
}

static bool swap_selection_beztriple(BezTriple *bezt)
{
	if (bezt->f2 & SELECT)
		return select_beztriple(bezt, DESELECT, SELECT, VISIBLE);
	else
		return select_beztriple(bezt, SELECT, SELECT, VISIBLE);
}

static bool swap_selection_bpoint(BPoint *bp)
{
	if (bp->f1 & SELECT)
		return select_bpoint(bp, DESELECT, SELECT, VISIBLE);
	else
		return select_bpoint(bp, SELECT, SELECT, VISIBLE);
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
	return BLI_ghash_popkey(editnurb->keyindex, cv, NULL);
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

	BLI_ghash_remove(editnurb->keyindex, cv, NULL, MEM_freeN);
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
			BLI_ghash_remove(editnurb->keyindex, bezt, NULL, MEM_freeN);
			bezt++;
		}
	}
	else {
		BPoint *bp = nu->bp;
		a = nu->pntsu * nu->pntsv;

		while (a--) {
			BLI_ghash_remove(editnurb->keyindex, bp, NULL, MEM_freeN);
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
				else {
					fp += a * 12;
				}
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
				else {
					fp += a * 4;
				}
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

	gh = BLI_ghash_ptr_new_ex("dupli_keyIndex gh", BLI_ghash_size(keyindex));

	for (hashIter = BLI_ghashIterator_new(keyindex);
	     BLI_ghashIterator_done(hashIter) == false;
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

static bool curve_is_animated(Curve *cu)
{
	AnimData *ad = BKE_animdata_from_id(&cu->id);

	return ad && (ad->action || ad->drivers.first);
}

static void fcurve_path_rename(AnimData *adt, const char *orig_rna_path, char *rna_path,
                               ListBase *orig_curves, ListBase *curves)
{
	FCurve *fcu, *nfcu, *nextfcu;
	int len = strlen(orig_rna_path);

	for (fcu = orig_curves->first; fcu; fcu = nextfcu) {
		nextfcu = fcu->next;
		if (!strncmp(fcu->rna_path, orig_rna_path, len)) {
			char *spath, *suffix = fcu->rna_path + len;
			nfcu = copy_fcurve(fcu);
			spath = nfcu->rna_path;
			nfcu->rna_path = BLI_sprintfN("%s%s", rna_path, suffix);
			BLI_addtail(curves, nfcu);

			if (fcu->grp) {
				action_groups_remove_channel(adt->action, fcu);
				action_groups_add_channel(adt->action, fcu->grp, nfcu);
			}
			else if ((adt->action) && (&adt->action->curves == orig_curves))
				BLI_remlink(&adt->action->curves, fcu);
			else
				BLI_remlink(&adt->drivers, fcu);

			free_fcurve(fcu);

			MEM_freeN(spath);
		}
	}
}

static void fcurve_remove(AnimData *adt, ListBase *orig_curves, FCurve *fcu)
{
	if (orig_curves == &adt->drivers) BLI_remlink(&adt->drivers, fcu);
	else action_groups_remove_channel(adt->action, fcu);

	free_fcurve(fcu);
}

static void curve_rename_fcurves(Curve *cu, ListBase *orig_curves)
{
	int nu_index = 0, a, pt_index;
	EditNurb *editnurb = cu->editnurb;
	Nurb *nu = editnurb->nurbs.first;
	CVKeyIndex *keyIndex;
	char rna_path[64], orig_rna_path[64];
	AnimData *adt = BKE_animdata_from_id(&cu->id);
	ListBase curves = {NULL, NULL};
	FCurve *fcu, *next;

	for (nu = editnurb->nurbs.first, nu_index = 0;  nu != NULL;  nu = nu->next, nu_index++) {
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
						fcurve_path_rename(adt, orig_handle_path, handle_path, orig_curves, &curves);

						BLI_snprintf(orig_handle_path, sizeof(orig_rna_path), "%s.handle_right", orig_rna_path);
						BLI_snprintf(handle_path, sizeof(rna_path), "%s.handle_left", rna_path);
						fcurve_path_rename(adt, orig_handle_path, handle_path, orig_curves, &curves);
					}

					fcurve_path_rename(adt, orig_rna_path, rna_path, orig_curves, &curves);

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
					fcurve_path_rename(adt, orig_rna_path, rna_path, orig_curves, &curves);

					keyIndex->nu_index = nu_index;
					keyIndex->pt_index = pt_index;
				}

				bp++;
				pt_index++;
			}
		}
	}

	/* remove paths for removed control points
	 * need this to make further step with copying non-cv related curves copying
	 * not touching cv's f-curves */
	for (fcu = orig_curves->first; fcu; fcu = next) {
		next = fcu->next;

		if (!strncmp(fcu->rna_path, "splines", 7)) {
			char *ch = strchr(fcu->rna_path, '.');

			if (ch && (!strncmp(ch, ".bezier_points", 14) || !strncmp(ch, ".points", 7)))
				fcurve_remove(adt, orig_curves, fcu);
		}
	}

	for (nu = editnurb->nurbs.first, nu_index = 0;  nu != NULL;  nu = nu->next, nu_index++) {
		keyIndex = NULL;
		if (nu->pntsu) {
			if (nu->bezt) keyIndex = getCVKeyIndex(editnurb, &nu->bezt[0]);
			else keyIndex = getCVKeyIndex(editnurb, &nu->bp[0]);
		}

		if (keyIndex) {
			BLI_snprintf(rna_path, sizeof(rna_path), "splines[%d]", nu_index);
			BLI_snprintf(orig_rna_path, sizeof(orig_rna_path), "splines[%d]", keyIndex->nu_index);
			fcurve_path_rename(adt, orig_rna_path, rna_path, orig_curves, &curves);
		}
	}

	/* the remainders in orig_curves can be copied back (like follow path) */
	/* (if it's not path to spline) */
	for (fcu = orig_curves->first; fcu; fcu = next) {
		next = fcu->next;

		if (!strncmp(fcu->rna_path, "splines", 7)) fcurve_remove(adt, orig_curves, fcu);
		else BLI_addtail(&curves, fcu);
	}

	*orig_curves = curves;
}

/* return 0 if animation data wasn't changed, 1 otherwise */
int ED_curve_updateAnimPaths(Curve *cu)
{
	AnimData *adt = BKE_animdata_from_id(&cu->id);
	EditNurb *editnurb = cu->editnurb;

	if (!editnurb->keyindex)
		return 0;

	if (!curve_is_animated(cu)) return 0;

	if (adt->action)
		curve_rename_fcurves(cu, &adt->action->curves);

	curve_rename_fcurves(cu, &adt->drivers);

	return 1;
}

/* ********************* LOAD and MAKE *************** */

/* load editNurb in object */
void load_editNurb(Object *obedit)
{
	ListBase *editnurb = object_editcurve_get(obedit);

	if (obedit == NULL) return;

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
}

/* make copy in cu->editnurb */
void make_editNurb(Object *obedit)
{
	Curve *cu = (Curve *)obedit->data;
	EditNurb *editnurb = cu->editnurb;
	Nurb *nu, *newnu;
	KeyBlock *actkey;

	if (ELEM(obedit->type, OB_CURVE, OB_SURF)) {
		actkey = BKE_keyblock_from_object(obedit);

		if (actkey) {
			// XXX strcpy(G.editModeTitleExtra, "(Key) ");
			undo_editmode_clear();
			BKE_key_convert_to_curve(actkey, cu, &cu->nurb);
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
		while (nu) {
			newnu = BKE_nurb_duplicate(nu);
			BKE_nurb_test2D(newnu); // after join, or any other creation of curve
			BLI_addtail(&editnurb->nurbs, newnu);
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

void ED_curve_deselect_all(EditNurb *editnurb)
{
	Nurb *nu;
	int a;

	for (nu = editnurb->nurbs.first; nu; nu = nu->next) {
		if (nu->bezt) {
			BezTriple *bezt;
			for (bezt = nu->bezt, a = 0; a < nu->pntsu; a++, bezt++) {
				bezt->f1 &= ~SELECT;
				bezt->f2 &= ~SELECT;
				bezt->f3 &= ~SELECT;
			}
		}
		else if (nu->bp) {
			BPoint *bp;
			for (bp = nu->bp, a = 0; a < nu->pntsu * nu->pntsv; a++, bp++) {
				bp->f1 &= ~SELECT;
			}
		}
	}
}

void ED_curve_select_all(EditNurb *editnurb)
{
	Nurb *nu;
	int a;
	for (nu = editnurb->nurbs.first; nu; nu = nu->next) {
		if (nu->bezt) {
			BezTriple *bezt;
			for (bezt = nu->bezt, a = 0; a < nu->pntsu; a++, bezt++) {
				if (bezt->hide == 0) {
					bezt->f1 |= SELECT;
					bezt->f2 |= SELECT;
					bezt->f3 |= SELECT;
				}
			}
		}
		else if (nu->bp) {
			BPoint *bp;
			for (bp = nu->bp, a = 0; a < nu->pntsu * nu->pntsv; a++, bp++) {
				if (bp->hide == 0)
					bp->f1 |= SELECT;
			}
		}
	}
}

void ED_curve_select_swap(EditNurb *editnurb, bool hide_handles)
{
	Nurb *nu;
	BPoint *bp;
	BezTriple *bezt;
	int a;

	for (nu = editnurb->nurbs.first; nu; nu = nu->next) {
		if (nu->type == CU_BEZIER) {
			bezt = nu->bezt;
			a = nu->pntsu;
			while (a--) {
				if (bezt->hide == 0) {
					bezt->f2 ^= SELECT; /* always do the center point */
					if (!hide_handles) {
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

/******************** transform operator **********************/

void ED_curve_transform(Curve *cu, float mat[4][4])
{
	Nurb *nu;
	BPoint *bp;
	BezTriple *bezt;
	int a;

	float scale = mat4_to_scale(mat);

	for (nu = cu->nurb.first; nu; nu = nu->next) {
		if (nu->type == CU_BEZIER) {
			a = nu->pntsu;
			for (bezt = nu->bezt; a--; bezt++) {
				mul_m4_v3(mat, bezt->vec[0]);
				mul_m4_v3(mat, bezt->vec[1]);
				mul_m4_v3(mat, bezt->vec[2]);
				bezt->radius *= scale;
			}
			BKE_nurb_handles_calc(nu);
		}
		else {
			a = nu->pntsu * nu->pntsv;
			for (bp = nu->bp; a--; bp++)
				mul_m4_v3(mat, bp->vec);
		}
	}
	DAG_id_tag_update(&cu->id, 0);
}

/******************** separate operator ***********************/

static int separate_exec(bContext *C, wmOperator *op)
{
	Main *bmain = CTX_data_main(C);
	Scene *scene = CTX_data_scene(C);
	Object *oldob, *newob;
	Base *oldbase, *newbase;
	Curve *oldcu, *newcu;
	EditNurb *newedit;
	ListBase newnurb = {NULL, NULL};

	oldbase = CTX_data_active_base(C);
	oldob = oldbase->object;
	oldcu = oldob->data;

	if (oldcu->key) {
		BKE_report(op->reports, RPT_ERROR, "Cannot separate a curve with vertex keys");
		return OPERATOR_CANCELLED;
	}

	WM_cursor_wait(1);

	/* 1. duplicate geometry and check for valid selection for separate */
	adduplicateflagNurb(oldob, &newnurb, SELECT, true);

	if (BLI_listbase_is_empty(&newnurb)) {
		WM_cursor_wait(0);
		BKE_report(op->reports, RPT_ERROR, "Cannot separate current selection");
		return OPERATOR_CANCELLED;
	}

	/* 2. duplicate the object and data */
	newbase = ED_object_add_duplicate(bmain, scene, oldbase, 0); /* 0 = fully linked */
	DAG_relations_tag_update(bmain);

	newob = newbase->object;
	newcu = newob->data = BKE_curve_copy(oldcu);
	newcu->editnurb = NULL;
	oldcu->id.us--; /* because new curve is a copy: reduce user count */

	/* 3. put new object in editmode, clear it and set separated nurbs */
	make_editNurb(newob);
	newedit = newcu->editnurb;
	BKE_nurbList_free(&newedit->nurbs);
	BKE_curve_editNurb_keyIndex_free(newedit);
	newedit->keyindex = NULL;
	BLI_movelisttolist(&newedit->nurbs, &newnurb);

	/* 4. put old object out of editmode and delete separated geometry */
	load_editNurb(newob);
	free_editNurb(newob);
	curve_delete_segments(oldob, true);

	DAG_id_tag_update(&oldob->id, OB_RECALC_DATA);  /* this is the original one */
	DAG_id_tag_update(&newob->id, OB_RECALC_DATA);  /* this is the separated one */

	WM_event_add_notifier(C, NC_GEOM | ND_DATA, oldob->data);
	WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, newob);

	WM_cursor_wait(0);

	return OPERATOR_FINISHED;
}

void CURVE_OT_separate(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Separate";
	ot->idname = "CURVE_OT_separate";
	ot->description = "Separate selected points from connected unselected points into a new object";
	
	/* api callbacks */
	ot->exec = separate_exec;
	ot->poll = ED_operator_editsurfcurve;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/******************** split operator ***********************/

static int curve_split_exec(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	ListBase *editnurb = object_editcurve_get(obedit);
	ListBase newnurb = {NULL, NULL};

	adduplicateflagNurb(obedit, &newnurb, SELECT, true);

	if (BLI_listbase_is_empty(&newnurb) == false) {
		curve_delete_segments(obedit, true);
		BLI_movelisttolist(editnurb, &newnurb);

		if (ED_curve_updateAnimPaths(obedit->data))
			WM_event_add_notifier(C, NC_OBJECT | ND_KEYS, obedit);

		WM_event_add_notifier(C, NC_GEOM | ND_DATA, obedit->data);
		DAG_id_tag_update(obedit->data, 0);
	}
	else {
		BKE_report(op->reports, RPT_ERROR, "Cannot split current selection");
		return OPERATOR_CANCELLED;
	}

	return OPERATOR_FINISHED;
}

void CURVE_OT_split(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Split";
	ot->idname = "CURVE_OT_split";
	ot->description = "Split off selected points from connected unselected points";

	/* api callbacks */
	ot->exec = curve_split_exec;
	ot->poll = ED_operator_editsurfcurve;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* ******************* FLAGS ********************* */

static short isNurbselUV(Nurb *nu, int *u, int *v, int flag)
{
	/* return (u != -1): 1 row in u-direction selected. U has value between 0-pntsv
	 * return (v != -1): 1 column in v-direction selected. V has value between 0-pntsu
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
		else if (sel > 1) {
			return 0;  /* because sel == 1 is still ok */
		}
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
		else if (sel > 1) {
			return 0;
		}
	}

	if (*u == -1 && *v > -1) return 1;
	if (*v == -1 && *u > -1) return 1;
	return 0;
}

/* return true if U direction is selected and number of selected columns v */
static bool isNurbselU(Nurb *nu, int *v, int flag)
{
	BPoint *bp;
	int a, b, sel;

	*v = 0;

	for (b = 0, bp = nu->bp; b < nu->pntsv; b++) {
		sel = 0;
		for (a = 0; a < nu->pntsu; a++, bp++) {
			if (bp->f1 & flag) sel++;
		}
		if (sel == nu->pntsu) {
			(*v)++;
		}
		else if (sel >= 1) {
			*v = 0;
			return 0;
		}
	}

	return 1;
}

/* return true if V direction is selected and number of selected rows u */
static bool isNurbselV(Nurb *nu, int *u, int flag)
{
	BPoint *bp;
	int a, b, sel;

	*u = 0;

	for (a = 0; a < nu->pntsu; a++) {
		bp = &nu->bp[a];
		sel = 0;
		for (b = 0; b < nu->pntsv; b++, bp += nu->pntsu) {
			if (bp->f1 & flag) sel++;
		}
		if (sel == nu->pntsv) {
			(*u)++;
		}
		else if (sel >= 1) {
			*u = 0;
			return 0;
		}
	}

	return 1;
}

static void rotateflagNurb(ListBase *editnurb, short flag, const float cent[3], float rotmat[3][3])
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

void ed_editnurb_translate_flag(ListBase *editnurb, short flag, const float vec[3])
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

static void ed_surf_delete_selected(Object *obedit)
{
	Curve *cu = obedit->data;
	ListBase *editnurb = object_editcurve_get(obedit);
	Nurb *nu, *next;
	BPoint *bp, *bpn, *newbp;
	int a, b, newu, newv;

	BLI_assert(obedit->type != OB_SURF);

	nu = editnurb->first;
	while (nu) {
		next = nu->next;

		/* is entire nurb selected */
		bp = nu->bp;
		a = nu->pntsu * nu->pntsv;
		while (a) {
			a--;
			if (bp->f1 & SELECT) {
				/* pass */
			}
			else {
				break;
			}
			bp++;
		}
		if (a == 0) {
			BLI_remlink(editnurb, nu);
			keyIndex_delNurb(cu->editnurb, nu);
			BKE_nurb_free(nu); nu = NULL;
		}
		else {
			if (isNurbselU(nu, &newv, SELECT)) {
				/* U direction selected */
				newv = nu->pntsv - newv;
				if (newv != nu->pntsv) {
					/* delete */
					bp = nu->bp;
					bpn = newbp = (BPoint *)MEM_mallocN(newv * nu->pntsu * sizeof(BPoint), "deleteNurb");
					for (b = 0; b < nu->pntsv; b++) {
						if ((bp->f1 & SELECT) == 0) {
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
			}
			else if (isNurbselV(nu, &newu, SELECT)) {
				/* V direction selected */
				newu = nu->pntsu - newu;
				if (newu != nu->pntsu) {
					/* delete */
					bp = nu->bp;
					bpn = newbp = (BPoint *)MEM_mallocN(newu * nu->pntsv * sizeof(BPoint), "deleteNurb");
					for (b = 0; b < nu->pntsv; b++) {
						for (a = 0; a < nu->pntsu; a++, bp++) {
							if ((bp->f1 & SELECT) == 0) {
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
}

static void ed_curve_delete_selected(Object *obedit)
{
	Curve *cu = obedit->data;
	EditNurb *editnurb = cu->editnurb;
	ListBase *nubase = &editnurb->nurbs;
	Nurb *nu, *next;
	BezTriple *bezt, *bezt1;
	BPoint *bp, *bp1;
	int a, type, nuindex = 0;

	/* first loop, can we remove entire pieces? */
	nu = nubase->first;
	while (nu) {
		next = nu->next;
		if (nu->type == CU_BEZIER) {
			bezt = nu->bezt;
			a = nu->pntsu;
			if (a) {
				while (a) {
					if (BEZSELECTED_HIDDENHANDLES(cu, bezt)) {
						/* pass */
					}
					else {
						break;
					}
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
					if (bp->f1 & SELECT) {
						/* pass */
					}
					else {
						break;
					}
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
				if (BEZSELECTED_HIDDENHANDLES(cu, bezt)) {
					memmove(bezt, bezt + 1, (nu->pntsu - a - 1) * sizeof(BezTriple));
					keyIndex_delBezt(editnurb, bezt + delta);
					keyIndex_updateBezt(editnurb, bezt + 1, bezt, nu->pntsu - a - 1);
					nu->pntsu--;
					a--;
					type = 1;
					delta++;
				}
				else {
					bezt++;
				}
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

/* only for OB_SURF */
bool ed_editnurb_extrude_flag(EditNurb *editnurb, short flag)
{
	Nurb *nu;
	BPoint *bp, *bpn, *newbp;
	int a, u, v, len;
	bool ok = false;

	nu = editnurb->nurbs.first;
	while (nu) {

		if (nu->pntsv == 1) {
			bp = nu->bp;
			a = nu->pntsu;
			while (a) {
				if (bp->f1 & flag) {
					/* pass */
				}
				else {
					break;
				}
				bp++;
				a--;
			}
			if (a == 0) {
				ok = true;
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

			if (isNurbselUV(nu, &u, &v, flag)) {

				/* deselect all */
				bp = nu->bp;
				a = nu->pntsu * nu->pntsv;
				while (a--) {
					select_bpoint(bp, DESELECT, flag, HIDDEN);
					bp++;
				}

				if (u == 0 || u == nu->pntsv - 1) {      /* row in u-direction selected */
					ok = true;
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
					ok = true;
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

static void adduplicateflagNurb(Object *obedit, ListBase *newnurb,
                                const short flag, const bool split)
{
	ListBase *editnurb = object_editcurve_get(obedit);
	Nurb *nu = editnurb->last, *newnu;
	BezTriple *bezt, *bezt1;
	BPoint *bp, *bp1, *bp2, *bp3;
	Curve *cu = (Curve *)obedit->data;
	int a, b, c, starta, enda, diffa, cyclicu, cyclicv, newu, newv;
	char *usel;

	while (nu) {
		cyclicu = cyclicv = 0;
		if (nu->type == CU_BEZIER) {
			for (a = 0, bezt = nu->bezt; a < nu->pntsu; a++, bezt++) {
				enda = -1;
				starta = a;
				while ((bezt->f1 & flag) || (bezt->f2 & flag) || (bezt->f3 & flag)) {
					if (!split) select_beztriple(bezt, DESELECT, flag, HIDDEN);
					enda = a;
					if (a >= nu->pntsu - 1) break;
					a++;
					bezt++;
				}
				if (enda >= starta) {
					newu = diffa = enda - starta + 1; /* set newu and diffa, may use both */

					if (starta == 0 && newu != nu->pntsu && (nu->flagu & CU_NURB_CYCLIC)) {
						cyclicu = newu;
					}
					else {
						if (enda == nu->pntsu - 1) newu += cyclicu;

						newnu = BKE_nurb_copy(nu, newu, 1);
						BLI_addtail(newnurb, newnu);
						memcpy(newnu->bezt, &nu->bezt[starta], diffa * sizeof(BezTriple));
						if (newu != diffa) {
							memcpy(&newnu->bezt[diffa], nu->bezt, cyclicu * sizeof(BezTriple));
							cyclicu = 0;
						}

						if (newu != nu->pntsu) newnu->flagu &= ~CU_NURB_CYCLIC;

						for (b = 0, bezt1 = newnu->bezt; b < newnu->pntsu; b++, bezt1++) {
							select_beztriple(bezt1, SELECT, flag, HIDDEN);
						}
					}
				}
			}

			if (cyclicu != 0) {
				newnu = BKE_nurb_copy(nu, cyclicu, 1);
				BLI_addtail(newnurb, newnu);
				memcpy(newnu->bezt, nu->bezt, cyclicu * sizeof(BezTriple));
				newnu->flagu &= ~CU_NURB_CYCLIC;

				for (b = 0, bezt1 = newnu->bezt; b < newnu->pntsu; b++, bezt1++) {
					select_beztriple(bezt1, SELECT, flag, HIDDEN);
				}
			}
		}
		else if (nu->pntsv == 1) {    /* because UV Nurb has a different method for dupli */
			for (a = 0, bp = nu->bp; a < nu->pntsu; a++, bp++) {
				enda = -1;
				starta = a;
				while (bp->f1 & flag) {
					if (!split) select_bpoint(bp, DESELECT, flag, HIDDEN);
					enda = a;
					if (a >= nu->pntsu - 1) break;
					a++;
					bp++;
				}
				if (enda >= starta) {
					newu = diffa = enda - starta + 1; /* set newu and diffa, may use both */

					if (starta == 0 && newu != nu->pntsu && (nu->flagu & CU_NURB_CYCLIC)) {
						cyclicu = newu;
					}
					else {
						if (enda == nu->pntsu - 1) newu += cyclicu;

						newnu = BKE_nurb_copy(nu, newu, 1);
						BLI_addtail(newnurb, newnu);
						memcpy(newnu->bp, &nu->bp[starta], diffa * sizeof(BPoint));
						if (newu != diffa) {
							memcpy(&newnu->bp[diffa], nu->bp, cyclicu * sizeof(BPoint));
							cyclicu = 0;
						}

						if (newu != nu->pntsu) newnu->flagu &= ~CU_NURB_CYCLIC;

						for (b = 0, bp1 = newnu->bp; b < newnu->pntsu; b++, bp1++) {
							select_bpoint(bp1, SELECT, flag, HIDDEN);
						}
					}
				}
			}

			if (cyclicu != 0) {
				newnu = BKE_nurb_copy(nu, cyclicu, 1);
				BLI_addtail(newnurb, newnu);
				memcpy(newnu->bp, nu->bp, cyclicu * sizeof(BPoint));
				newnu->flagu &= ~CU_NURB_CYCLIC;

				for (b = 0, bp1 = newnu->bp; b < newnu->pntsu; b++, bp1++) {
					select_bpoint(bp1, SELECT, flag, HIDDEN);
				}
			}
		}
		else {
			if (isNurbsel(nu)) {
				/* a rectangular area in nurb has to be selected and if splitting must be in U or V direction */
				usel = MEM_callocN(nu->pntsu, "adduplicateN3");
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
				MEM_freeN(usel);

				if ((newu == 0 || newv == 0) ||
				    (split && !isNurbselU(nu, &newv, SELECT) && !isNurbselV(nu, &newu, SELECT)))
				{
					if (G.debug & G_DEBUG)
						printf("Can't duplicate Nurb\n");
				}
				else {
					for (a = 0, bp1 = nu->bp; a < nu->pntsu * nu->pntsv; a++, bp1++) {
						newv = newu = 0;

						if ((bp1->f1 & flag) && !(bp1->f1 & SURF_SEEN)) {
							/* point selected, now loop over points in U and V directions */
							for (b = a % nu->pntsu, bp2 = bp1; b < nu->pntsu; b++, bp2++) {
								if (bp2->f1 & flag) {
									newu++;
									for (c = a / nu->pntsu, bp3 = bp2; c < nu->pntsv; c++, bp3 += nu->pntsu) {
										if (bp3->f1 & flag) {
											bp3->f1 |= SURF_SEEN; /* flag as seen so skipped on future iterations */
											if (newu == 1) newv++;
										}
										else {
											break;
										}
									}
								}
								else {
									break;
								}
							}
						}

						if ((newu + newv) > 2) {
							/* ignore single points */
							if (a == 0) {
								/* check if need to save cyclic selection and continue if so */
								if (newu == nu->pntsu && (nu->flagv & CU_NURB_CYCLIC)) cyclicv = newv;
								if (newv == nu->pntsv && (nu->flagu & CU_NURB_CYCLIC)) cyclicu = newu;
								if (cyclicu != 0 || cyclicv != 0) continue;
							}

							if (a + newu == nu->pntsu && cyclicu != 0) {
								/* cyclic in U direction */
								newnu = BKE_nurb_copy(nu, newu + cyclicu, newv);
								for (b = 0; b < newv; b++) {
									memcpy(&newnu->bp[b * newnu->pntsu], &nu->bp[b * nu->pntsu + a], newu * sizeof(BPoint));
									memcpy(&newnu->bp[b * newnu->pntsu + newu], &nu->bp[b * nu->pntsu], cyclicu * sizeof(BPoint));
								}
								cyclicu = cyclicv = 0;
							}
							else if ((a / nu->pntsu) + newv == nu->pntsv && cyclicv != 0) {
								/* cyclic in V direction */
								newnu = BKE_nurb_copy(nu, newu, newv + cyclicv);
								memcpy(newnu->bp, &nu->bp[a], newu * newv * sizeof(BPoint));
								memcpy(&newnu->bp[newu * newv], nu->bp, newu * cyclicv * sizeof(BPoint));
								cyclicu = cyclicv = 0;
							}
							else {
								newnu = BKE_nurb_copy(nu, newu, newv);
								for (b = 0; b < newv; b++) {
									memcpy(&newnu->bp[b * newu], &nu->bp[b * nu->pntsu + a], newu * sizeof(BPoint));
								}
							}
							BLI_addtail(newnurb, newnu);

							if (newu != nu->pntsu) newnu->flagu &= ~CU_NURB_CYCLIC;
							if (newv != nu->pntsv) newnu->flagv &= ~CU_NURB_CYCLIC;
						}
					}

					if (cyclicu != 0 || cyclicv != 0) {
						/* copy start of a cyclic surface, or copying all selected points */
						newu = cyclicu == 0 ? nu->pntsu : cyclicu;
						newv = cyclicv == 0 ? nu->pntsv : cyclicv;

						newnu = BKE_nurb_copy(nu, newu, newv);
						for (b = 0; b < newv; b++) {
							memcpy(&newnu->bp[b * newu], &nu->bp[b * nu->pntsu], newu * sizeof(BPoint));
						}
						BLI_addtail(newnurb, newnu);

						if (newu != nu->pntsu) newnu->flagu &= ~CU_NURB_CYCLIC;
						if (newv != nu->pntsv) newnu->flagv &= ~CU_NURB_CYCLIC;
					}

					for (b = 0, bp1 = nu->bp; b < nu->pntsu * nu->pntsv; b++, bp1++) {
						bp1->f1 &= ~SURF_SEEN;
						if (!split) select_bpoint(bp1, DESELECT, flag, HIDDEN);
					}
				}
			}
		}
		nu = nu->prev;
	}

	if (BLI_listbase_is_empty(newnurb) == false) {
		cu->actnu = cu->actvert = CU_ACT_NONE;

		for (nu = newnurb->first; nu; nu = nu->next) {
			if (nu->type == CU_BEZIER) {
				if (split) {
					/* recalc first and last */
					BKE_nurb_handle_calc_simple(nu, &nu->bezt[0]);
					BKE_nurb_handle_calc_simple(nu, &nu->bezt[nu->pntsu - 1]);
				}
			}
			else {
				/* knots done after duplicate as pntsu may change */
				nu->knotsu = nu->knotsv = NULL;
				BKE_nurb_order_clamp_u(nu);
				BKE_nurb_knot_calc_u(nu);

				if (obedit->type == OB_SURF) {
					for (a = 0, bp = nu->bp; a < nu->pntsu * nu->pntsv; a++, bp++) {
						bp->f1 &= ~SURF_SEEN;
					}

					BKE_nurb_order_clamp_v(nu);
					BKE_nurb_knot_calc_v(nu);
				}
			}
		}
	}
}

/**************** switch direction operator ***************/

static int switch_direction_exec(bContext *C, wmOperator *UNUSED(op))
{
	Object *obedit = CTX_data_edit_object(C);
	Curve *cu = (Curve *)obedit->data;
	EditNurb *editnurb = cu->editnurb;
	Nurb *nu;

	for (nu = editnurb->nurbs.first; nu; nu = nu->next) {
		if (isNurbsel(nu)) {
			BKE_nurb_direction_switch(nu);
			keyData_switchDirectionNurb(cu, nu);
		}
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
	int a, i;
	bool changed = false;
	
	for (nu = editnurb->first; nu; nu = nu->next) {
		if (nu->bezt) {
			changed = false;
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
					changed = true;
				}
			}
			MEM_freeN(beztOrig);
			if (changed) {
				BKE_nurb_handles_calc(nu);
			}
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

/* -------------------------------------------------------------------- */
/* Smooth radius/weight/tilt
 *
 * TODO: make smoothing distance based
 * TODO: support cyclic curves
 */

static void curve_smooth_value(ListBase *editnurb,
                               const int bezt_offsetof, const int bp_offset)
{
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
#define BEZT_VALUE(bezt) (*((float *)((char *)bezt + bezt_offsetof)))

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
					start_rad = end_rad = FLT_MAX;

					if (start_sel == end_sel) {
						/* simple, only 1 point selected */
						if (start_sel > 0)                         start_rad = BEZT_VALUE(&nu->bezt[start_sel - 1]);
						if (end_sel != -1 && end_sel < nu->pntsu)  end_rad   = BEZT_VALUE(&nu->bezt[start_sel + 1]);

						if      (start_rad != FLT_MAX && end_rad >= FLT_MAX) BEZT_VALUE(&nu->bezt[start_sel]) = (start_rad + end_rad) / 2.0f;
						else if (start_rad != FLT_MAX)                       BEZT_VALUE(&nu->bezt[start_sel]) = start_rad;
						else if (end_rad   != FLT_MAX)                       BEZT_VALUE(&nu->bezt[start_sel]) = end_rad;
					}
					else {
						/* if endpoints selected, then use them */
						if (start_sel == 0) {
							start_rad = BEZT_VALUE(&nu->bezt[start_sel]);
							start_sel++; /* we don't want to edit the selected endpoint */
						}
						else {
							start_rad = BEZT_VALUE(&nu->bezt[start_sel - 1]);
						}
						if (end_sel == nu->pntsu - 1) {
							end_rad = BEZT_VALUE(&nu->bezt[end_sel]);
							end_sel--; /* we don't want to edit the selected endpoint */
						}
						else {
							end_rad = BEZT_VALUE(&nu->bezt[end_sel + 1]);
						}

						/* Now Blend between the points */
						range = (float)(end_sel - start_sel) + 2.0f;
						for (bezt = &nu->bezt[start_sel], a = start_sel; a <= end_sel; a++, bezt++) {
							fac = (float)(1 + a - start_sel) / range;
							BEZT_VALUE(bezt) = start_rad * (1.0f - fac) + end_rad * fac;
						}
					}
				}
			}
#undef BEZT_VALUE
		}
		else if (nu->bp) {
#define BP_VALUE(bp) (*((float *)((char *)bp + bp_offset)))

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
					start_rad = end_rad = FLT_MAX;

					if (start_sel == end_sel) {
						/* simple, only 1 point selected */
						if (start_sel > 0) start_rad = BP_VALUE(&nu->bp[start_sel - 1]);
						if (end_sel != -1 && end_sel < nu->pntsu) end_rad = BP_VALUE(&nu->bp[start_sel + 1]);

						if      (start_rad != FLT_MAX && end_rad != FLT_MAX) BP_VALUE(&nu->bp[start_sel]) = (start_rad + end_rad) / 2;
						else if (start_rad != FLT_MAX)                       BP_VALUE(&nu->bp[start_sel]) = start_rad;
						else if (end_rad   != FLT_MAX)                       BP_VALUE(&nu->bp[start_sel]) = end_rad;
					}
					else {
						/* if endpoints selected, then use them */
						if (start_sel == 0) {
							start_rad = BP_VALUE(&nu->bp[start_sel]);
							start_sel++; /* we don't want to edit the selected endpoint */
						}
						else {
							start_rad = BP_VALUE(&nu->bp[start_sel - 1]);
						}
						if (end_sel == nu->pntsu - 1) {
							end_rad = BP_VALUE(&nu->bp[end_sel]);
							end_sel--; /* we don't want to edit the selected endpoint */
						}
						else {
							end_rad = BP_VALUE(&nu->bp[end_sel + 1]);
						}

						/* Now Blend between the points */
						range = (float)(end_sel - start_sel) + 2.0f;
						for (bp = &nu->bp[start_sel], a = start_sel; a <= end_sel; a++, bp++) {
							fac = (float)(1 + a - start_sel) / range;
							BP_VALUE(bp) = start_rad * (1.0f - fac) + end_rad * fac;
						}
					}
				}
			}
#undef BP_VALUE
		}
	}
}

static int curve_smooth_weight_exec(bContext *C, wmOperator *UNUSED(op))
{
	Object *obedit = CTX_data_edit_object(C);
	ListBase *editnurb = object_editcurve_get(obedit);

	curve_smooth_value(editnurb, offsetof(BezTriple, weight), offsetof(BPoint, weight));

	WM_event_add_notifier(C, NC_GEOM | ND_DATA, obedit->data);
	DAG_id_tag_update(obedit->data, 0);

	return OPERATOR_FINISHED;
}

void CURVE_OT_smooth_weight(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Smooth Curve Weight";
	ot->description = "Interpolate weight of selected points";
	ot->idname = "CURVE_OT_smooth_weight";

	/* api clastbacks */
	ot->exec = curve_smooth_weight_exec;
	ot->poll = ED_operator_editsurfcurve;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static int curve_smooth_radius_exec(bContext *C, wmOperator *UNUSED(op))
{
	Object *obedit = CTX_data_edit_object(C);
	ListBase *editnurb = object_editcurve_get(obedit);
	
	curve_smooth_value(editnurb, offsetof(BezTriple, radius), offsetof(BPoint, radius));

	WM_event_add_notifier(C, NC_GEOM | ND_DATA, obedit->data);
	DAG_id_tag_update(obedit->data, 0);

	return OPERATOR_FINISHED;
}

void CURVE_OT_smooth_radius(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Smooth Curve Radius";
	ot->description = "Interpolate radii of selected points";
	ot->idname = "CURVE_OT_smooth_radius";
	
	/* api clastbacks */
	ot->exec = curve_smooth_radius_exec;
	ot->poll = ED_operator_editsurfcurve;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static int curve_smooth_tilt_exec(bContext *C, wmOperator *UNUSED(op))
{
	Object *obedit = CTX_data_edit_object(C);
	ListBase *editnurb = object_editcurve_get(obedit);

	curve_smooth_value(editnurb, offsetof(BezTriple, alfa), offsetof(BPoint, alfa));

	WM_event_add_notifier(C, NC_GEOM | ND_DATA, obedit->data);
	DAG_id_tag_update(obedit->data, 0);

	return OPERATOR_FINISHED;
}

void CURVE_OT_smooth_tilt(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Smooth Curve Tilt";
	ot->description = "Interpolate tilt of selected points";
	ot->idname = "CURVE_OT_smooth_tilt";

	/* api clastbacks */
	ot->exec = curve_smooth_tilt_exec;
	ot->poll = ED_operator_editsurfcurve;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/***************** selection utility *************************/

/* next == 1 -> select next         */
/* next == -1 -> select previous    */
/* cont == 1 -> select continuously */
/* selstatus, inverts behavior		*/
static void select_adjacent_cp(ListBase *editnurb, short next,
                               const bool cont, const bool selstatus)
{
	Nurb *nu;
	BezTriple *bezt;
	BPoint *bp;
	int a;
	short lastsel = false;
	
	if (next == 0) return;
	
	for (nu = editnurb->first; nu; nu = nu->next) {
		lastsel = false;
		if (nu->type == CU_BEZIER) {
			a = nu->pntsu;
			bezt = nu->bezt;
			if (next < 0) bezt = &nu->bezt[a - 1];
			while (a--) {
				if (a - abs(next) < 0) break;
				if ((lastsel == 0) && (bezt->hide == 0) && ((bezt->f2 & SELECT) || (selstatus == DESELECT))) {
					bezt += next;
					if (!(bezt->f2 & SELECT) || (selstatus == DESELECT)) {
						short sel = select_beztriple(bezt, selstatus, SELECT, VISIBLE);
						if ((sel == 1) && (cont == 0)) lastsel = true;
					}
				}
				else {
					bezt += next;
					lastsel = false;
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
				if ((lastsel == 0) && (bp->hide == 0) && ((bp->f1 & SELECT) || (selstatus == DESELECT))) {
					bp += next;
					if (!(bp->f1 & SELECT) || (selstatus == DESELECT)) {
						short sel = select_bpoint(bp, selstatus, SELECT, VISIBLE);
						if ((sel == 1) && (cont == 0)) lastsel = true;
					}
				}
				else {
					bp += next;
					lastsel = false;
				}
				/* move around in zigzag way so that we go through each */
				bp -= (next - next / abs(next));
			}
		}
	}
}

/**************** select start/end operators **************/

/* (de)selects first or last of visible part of each Nurb depending on selFirst
 * selFirst: defines the end of which to select
 * doswap: defines if selection state of each first/last control point is swapped
 * selstatus: selection status in case doswap is false
 */
void selectend_nurb(Object *obedit, eEndPoint_Types selfirst, bool doswap, bool selstatus)
{
	ListBase *editnurb = object_editcurve_get(obedit);
	Nurb *nu;
	BPoint *bp;
	BezTriple *bezt;
	Curve *cu;
	int a;

	if (obedit == NULL) return;

	cu = (Curve *)obedit->data;
	cu->actvert = CU_ACT_NONE;

	for (nu = editnurb->first; nu; nu = nu->next) {
		if (nu->type == CU_BEZIER) {
			a = nu->pntsu;
			
			/* which point? */
			if (selfirst == LAST) { /* select last */
				bezt = &nu->bezt[a - 1];
			}
			else { /* select first */
				bezt = nu->bezt;
			}
			
			while (a--) {
				bool sel;
				if (doswap) sel = swap_selection_beztriple(bezt);
				else sel = select_beztriple(bezt, selstatus, SELECT, VISIBLE);
				
				if (sel == true) break;
			}
		}
		else {
			a = nu->pntsu * nu->pntsv;
			
			/* which point? */
			if (selfirst == LAST) { /* select last */
				bp = &nu->bp[a - 1];
			}
			else { /* select first */
				bp = nu->bp;
			}

			while (a--) {
				if (bp->hide == 0) {
					bool sel;
					if (doswap) sel = swap_selection_bpoint(bp);
					else sel = select_bpoint(bp, selstatus, SELECT, VISIBLE);
					
					if (sel == true) break;
				}
			}
		}
	}
}

static int de_select_first_exec(bContext *C, wmOperator *UNUSED(op))
{
	Object *obedit = CTX_data_edit_object(C);

	selectend_nurb(obedit, FIRST, true, DESELECT);
	WM_event_add_notifier(C, NC_GEOM | ND_SELECT, obedit->data);
	BKE_curve_nurb_vert_active_validate(obedit->data);

	return OPERATOR_FINISHED;
}

void CURVE_OT_de_select_first(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "(De)select First";
	ot->idname = "CURVE_OT_de_select_first";
	ot->description = "(De)select first of visible part of each NURBS";
	
	/* api cfirstbacks */
	ot->exec = de_select_first_exec;
	ot->poll = ED_operator_editcurve;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static int de_select_last_exec(bContext *C, wmOperator *UNUSED(op))
{
	Object *obedit = CTX_data_edit_object(C);

	selectend_nurb(obedit, LAST, true, DESELECT);
	WM_event_add_notifier(C, NC_GEOM | ND_SELECT, obedit->data);
	BKE_curve_nurb_vert_active_validate(obedit->data);

	return OPERATOR_FINISHED;
}

void CURVE_OT_de_select_last(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "(De)select Last";
	ot->idname = "CURVE_OT_de_select_last";
	ot->description = "(De)select last of visible part of each NURBS";
	
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
	Curve *cu = obedit->data;
	ListBase *editnurb = object_editcurve_get(obedit);
	int action = RNA_enum_get(op->ptr, "action");

	if (action == SEL_TOGGLE) {
		action = SEL_SELECT;
		if (nurb_has_selected_cps(editnurb))
			action = SEL_DESELECT;
	}

	switch (action) {
		case SEL_SELECT:
			ED_curve_select_all(cu->editnurb);
			break;
		case SEL_DESELECT:
			ED_curve_deselect_all(cu->editnurb);
			break;
		case SEL_INVERT:
			ED_curve_select_swap(cu->editnurb, (cu->drawflag & CU_HIDE_HANDLES) != 0);
			break;
	}
	
	WM_event_add_notifier(C, NC_GEOM | ND_SELECT, obedit->data);
	BKE_curve_nurb_vert_active_validate(cu);

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
	int a, sel;
	const bool invert = RNA_boolean_get(op->ptr, "unselected");

	for (nu = editnurb->first; nu; nu = nu->next) {
		if (nu->type == CU_BEZIER) {
			bezt = nu->bezt;
			a = nu->pntsu;
			sel = 0;
			while (a--) {
				if (invert == 0 && BEZSELECTED_HIDDENHANDLES(cu, bezt)) {
					select_beztriple(bezt, DESELECT, SELECT, HIDDEN);
					bezt->hide = 1;
				}
				else if (invert && !BEZSELECTED_HIDDENHANDLES(cu, bezt)) {
					select_beztriple(bezt, DESELECT, SELECT, HIDDEN);
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
					select_bpoint(bp, DESELECT, SELECT, HIDDEN);
					bp->hide = 1;
				}
				else if (invert && (bp->f1 & SELECT) == 0) {
					select_bpoint(bp, DESELECT, SELECT, HIDDEN);
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
	BKE_curve_nurb_vert_active_validate(obedit->data);

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
					select_beztriple(bezt, SELECT, SELECT, HIDDEN);
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
					select_bpoint(bp, SELECT, SELECT, HIDDEN);
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
	BezTriple *bezt, *beztnew, *beztn;
	BPoint *bp, *prevbp, *bpnew, *bpn;
	float vec[15];
	int a, b, sel, amount, *usel, *vsel, i;
	float factor;

	// printf("*** subdivideNurb: entering subdivide\n");

	for (nu = editnurb->nurbs.first; nu; nu = nu->next) {
		amount = 0;
		if (nu->type == CU_BEZIER) {
			BezTriple *nextbezt;

			/*
			 * Insert a point into a 2D Bezier curve.
			 * Endpoints are preserved. Otherwise, all selected and inserted points are
			 * newly created. Old points are discarded.
			 */
			/* count */
			a = nu->pntsu;
			bezt = nu->bezt;
			while (a--) {
				nextbezt = BKE_nurb_bezt_get_next(nu, bezt);
				if (nextbezt == NULL) {
					break;
				}

				if (BEZSELECTED_HIDDENHANDLES(cu, bezt) && BEZSELECTED_HIDDENHANDLES(cu, nextbezt)) {
					amount += number_cuts;
				}
				bezt++;
			}

			if (amount) {
				/* insert */
				beztnew = (BezTriple *)MEM_mallocN((amount + nu->pntsu) * sizeof(BezTriple), "subdivNurb");
				beztn = beztnew;
				a = nu->pntsu;
				bezt = nu->bezt;
				while (a--) {
					memcpy(beztn, bezt, sizeof(BezTriple));
					keyIndex_updateBezt(editnurb, bezt, beztn, 1);
					beztn++;

					nextbezt = BKE_nurb_bezt_get_next(nu, bezt);
					if (nextbezt == NULL) {
						break;
					}

					if (BEZSELECTED_HIDDENHANDLES(cu, bezt) && BEZSELECTED_HIDDENHANDLES(cu, nextbezt)) {
						float prevvec[3][3];

						memcpy(prevvec, bezt->vec, sizeof(float) * 9);

						for (i = 0; i < number_cuts; i++) {
							factor = 1.0f / (number_cuts + 1 - i);

							memcpy(beztn, nextbezt, sizeof(BezTriple));

							/* midpoint subdividing */
							interp_v3_v3v3(vec, prevvec[1], prevvec[2], factor);
							interp_v3_v3v3(vec + 3, prevvec[2], nextbezt->vec[0], factor);
							interp_v3_v3v3(vec + 6, nextbezt->vec[0], nextbezt->vec[1], factor);

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
							else                                                                { copy_v3_v3(nextbezt->vec[0], vec + 6); }

							beztn->radius = (bezt->radius + nextbezt->radius) / 2;
							beztn->weight = (bezt->weight + nextbezt->weight) / 2;

							memcpy(prevvec, beztn->vec, sizeof(float) * 9);
							beztn++;
						}
					}

					bezt++;
				}

				MEM_freeN(nu->bezt);
				nu->bezt = beztnew;
				nu->pntsu += amount;

				BKE_nurb_handles_calc(nu);
			}
		} /* End of 'if (nu->type == CU_BEZIER)' */
		else if (nu->pntsv == 1) {
			BPoint *nextbp;

			/*
			 * All flat lines (ie. co-planar), except flat Nurbs. Flat NURB curves
			 * are handled together with the regular NURB plane division, as it
			 * should be. I split it off just now, let's see if it is
			 * stable... nzc 30-5-'00
			 */
			/* count */
			a = nu->pntsu;
			bp = nu->bp;
			while (a--) {
				nextbp = BKE_nurb_bpoint_get_next(nu, bp);
				if (nextbp == NULL) {
					break;
				}

				if ((bp->f1 & SELECT) && (nextbp->f1 & SELECT)) {
					amount += number_cuts;
				}
				bp++;
			}

			if (amount) {
				/* insert */
				bpnew = (BPoint *)MEM_mallocN((amount + nu->pntsu) * sizeof(BPoint), "subdivNurb2");
				bpn = bpnew;

				a = nu->pntsu;
				bp = nu->bp;

				while (a--) {
					/* Copy "old" point. */
					memcpy(bpn, bp, sizeof(BPoint));
					keyIndex_updateBP(editnurb, bp, bpn, 1);
					bpn++;

					nextbp = BKE_nurb_bpoint_get_next(nu, bp);
					if (nextbp == NULL) {
						break;
					}

					if ((bp->f1 & SELECT) && (nextbp->f1 & SELECT)) {
						// printf("*** subdivideNurb: insert 'linear' point\n");
						for (i = 0; i < number_cuts; i++) {
							factor = (float)(i + 1) / (number_cuts + 1);

							memcpy(bpn, nextbp, sizeof(BPoint));
							interp_v4_v4v4(bpn->vec, bp->vec, nextbp->vec, factor);
							bpn++;
						}

					}
					bp++;
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
			if (sel == (nu->pntsu * nu->pntsv)) {  /* subdivide entire nurb */
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
			} /* End of 'if (sel == nu->pntsu * nu->pntsv)' (subdivide entire NURB) */
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

static void findnearestNurbvert__doClosest(void *userData, Nurb *nu, BPoint *bp, BezTriple *bezt, int beztindex, const float screen_co[2])
{
	struct { BPoint *bp; BezTriple *bezt; Nurb *nurb; float dist; int hpoint, select; float mval_fl[2]; } *data = userData;

	short flag;
	float dist_test;

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

	dist_test = len_manhattan_v2v2(data->mval_fl, screen_co);
	if ((flag & SELECT) == data->select) dist_test += 5.0f;
	if (bezt && beztindex == 1) dist_test += 3.0f;  /* middle points get a small disadvantage */

	if (dist_test < data->dist) {
		data->dist = dist_test;

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
	struct { BPoint *bp; BezTriple *bezt; Nurb *nurb; float dist; int hpoint, select; float mval_fl[2]; } data = {NULL};

	data.dist = 100;
	data.hpoint = 0;
	data.select = sel;
	data.mval_fl[0] = mval[0];
	data.mval_fl[1] = mval[1];

	ED_view3d_init_mats_rv3d(vc->obedit, vc->rv3d);
	nurbs_foreachScreenVert(vc, findnearestNurbvert__doClosest, &data, V3D_PROJ_TEST_CLIP_DEFAULT);

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
				if ((bezt1->f1 & SELECT) || (bezt1->f2 & SELECT) || (bezt1->f3 & SELECT)) {
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

static int set_spline_type_exec(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	ListBase *editnurb = object_editcurve_get(obedit);
	Nurb *nu;
	bool changed = false;
	const bool use_handles = RNA_boolean_get(op->ptr, "use_handles");
	const int type = RNA_enum_get(op->ptr, "type");

	if (type == CU_CARDINAL || type == CU_BSPLINE) {
		BKE_report(op->reports, RPT_ERROR, "Not yet implemented");
		return OPERATOR_CANCELLED;
	}
	
	for (nu = editnurb->first; nu; nu = nu->next) {
		if (isNurbsel(nu)) {
			if (BKE_nurb_type_convert(nu, type, use_handles) == false)
				BKE_report(op->reports, RPT_ERROR, "No conversion possible");
			else
				changed = true;
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
	RNA_def_boolean(ot->srna, "use_handles", 0, "Handles", "Use handles when converting bezier curves into polygons");
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

/***************** recalculate handles operator **********************/

static int curve_normals_make_consistent_exec(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	ListBase *editnurb = object_editcurve_get(obedit);
	const bool calc_length = RNA_boolean_get(op->ptr, "calc_length");

	BKE_nurbList_handles_recalculate(editnurb, calc_length, SELECT);

	WM_event_add_notifier(C, NC_GEOM | ND_DATA, obedit->data);
	DAG_id_tag_update(obedit->data, 0);

	return OPERATOR_FINISHED;
}

void CURVE_OT_normals_make_consistent(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Recalc Normals";
	ot->description = "Recalculate the direction of selected handles";
	ot->idname = "CURVE_OT_normals_make_consistent";

	/* api callbacks */
	ot->exec = curve_normals_make_consistent_exec;
	ot->poll = ED_operator_editcurve;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* props */
	RNA_def_boolean(ot->srna, "calc_length", false, "Length", "Recalculate handle length");
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
	
	SWAP(int, nu->pntsu, nu->pntsv);
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

static bool is_u_selected(Nurb *nu, int u)
{
	BPoint *bp;
	int v;
	
	/* what about resolu == 2? */
	bp = &nu->bp[u];
	for (v = 0; v < nu->pntsv - 1; v++, bp += nu->pntsu) {
		if ((v != 0) && (bp->f1 & SELECT)) {
			return TRUE;
		}
	}
	
	return FALSE;
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
	
	if (is_u_selected(nu1, nu1->pntsu - 1)) {
		/* pass */
	}
	else {
		/* For 2D curves blender uses (orderv = 0). It doesn't make any sense mathematically. */
		/* but after rotating (orderu = 0) will be confusing. */
		if (nu1->orderv == 0) nu1->orderv = 1;

		rotate_direction_nurb(nu1);
		if (is_u_selected(nu1, nu1->pntsu - 1)) {
			/* pass */
		}
		else {
			rotate_direction_nurb(nu1);
			if (is_u_selected(nu1, nu1->pntsu - 1)) {
				/* pass */
			}
			else {
				rotate_direction_nurb(nu1);
				if (is_u_selected(nu1, nu1->pntsu - 1)) {
					/* pass */
				}
				else {
					/* rotate again, now its OK! */
					if (nu1->pntsv != 1) rotate_direction_nurb(nu1);
					return;
				}
			}
		}
	}
	
	/* 2nd nurbs: u = 0 selected */
	if (is_u_selected(nu2, 0)) {
		/* pass */
	}
	else {
		if (nu2->orderv == 0) nu2->orderv = 1;
		rotate_direction_nurb(nu2);
		if (is_u_selected(nu2, 0)) {
			/* pass */
		}
		else {
			rotate_direction_nurb(nu2);
			if (is_u_selected(nu2, 0)) {
				/* pass */
			}
			else {
				rotate_direction_nurb(nu2);
				if (is_u_selected(nu2, 0)) {
					/* pass */
				}
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
		BKE_report(op->reports, RPT_ERROR, "Resolution does not match");
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
				select_bpoint(bp, SELECT, SELECT, HIDDEN);
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
		if (nus1->nu->pntsu == nus2->nu->pntsu || nus1->nu->pntsu == nus2->nu->pntsv) {
			/* pass */
		}
		else {
			ok = 0;
		}
	}
	else if (nus2->nu->pntsv == 1) {
		if (nus2->nu->pntsu == nus1->nu->pntsu || nus2->nu->pntsu == nus1->nu->pntsv) {
			/* pass */
		}
		else {
			ok = 0;
		}
	}
	else if (nus1->nu->pntsu == nus2->nu->pntsu || nus1->nu->pntsv == nus2->nu->pntsv) {
		/* pass */
	}
	else if (nus1->nu->pntsu == nus2->nu->pntsv || nus1->nu->pntsv == nus2->nu->pntsu) {
		/* pass */
	}
	else {
		ok = 0;
	}
	
	if (ok == 0) {
		BKE_report(op->reports, RPT_ERROR, "Resolution does not match");
		BLI_freelistN(&nsortbase);
		return OPERATOR_CANCELLED;
	}

	while (nus2) {
		merge_2_nurb(op, editnurb, nus1->nu, nus2->nu);
		nus2 = nus2->next;
	}
	
	BLI_freelistN(&nsortbase);
	
	BKE_curve_nurb_active_set(obedit->data, NULL);

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
	int ok = 0;
	/* int a; */ /* UNUSED */

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
					if ( (nu->bp->f1 & SELECT) || (nu->bp[nu->pntsu - 1].f1 & SELECT)) {
						/* pass */
					}
					else {
						break;
					}
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
				if (BEZSELECTED_HIDDENHANDLES(cu, &(nu->bezt[nu->pntsu - 1]))) {
					/* Last point is selected, preferred for nu2 */
					if (nu2 == NULL) {
						nu2 = nu;
					}
					else if (nu1 == NULL) {
						nu1 = nu;

						/* Just in case both of first/last CV are selected check
						 * whether we really need to switch the direction.
						 */
						if (!BEZSELECTED_HIDDENHANDLES(cu, nu1->bezt)) {
							BKE_nurb_direction_switch(nu1);
							keyData_switchDirectionNurb(cu, nu1);
						}
					}
				}
				else if (BEZSELECTED_HIDDENHANDLES(cu, nu->bezt)) {
					/* First point is selected, preferred for nu1 */
					if (nu1 == NULL) {
						nu1 = nu;
					}
					else if (nu2 == NULL) {
						nu2 = nu;

						/* Just in case both of first/last CV are selected check
						 * whether we really need to switch the direction.
						 */
						if (!BEZSELECTED_HIDDENHANDLES(cu, &(nu->bezt[nu2->pntsu - 1]))) {
							BKE_nurb_direction_switch(nu2);
							keyData_switchDirectionNurb(cu, nu2);
						}
					}
				}
			}
			else if (nu->pntsv == 1) {
				/* Same logic as above: if first point is selected spline is
				 * preferred for nu1, if last point is selected spline is
				 * preferred for u2u.
				 */

				bp = nu->bp;
				if (bp[nu->pntsu - 1].f1 & SELECT)  {
					if (nu2 == NULL) {
						nu2 = nu;
					}
					else if (nu1 == NULL) {
						nu1 = nu;

						if ((bp->f1 & SELECT) == 0) {
							BKE_nurb_direction_switch(nu);
							keyData_switchDirectionNurb(cu, nu);
						}
					}
				}
				else if (bp->f1 & SELECT) {
					if (nu1 == NULL) {
						nu1 = nu;
					}
					else if (nu2 == NULL) {
						nu2 = nu;

						if ((bp[nu->pntsu - 1].f1 & SELECT) == 0) {
							BKE_nurb_direction_switch(nu);
							keyData_switchDirectionNurb(cu, nu);
						}
					}
				}
			}
		}

		if (nu1 && nu2) {
			/* Got second spline, no need to loop over rest of the splines. */
			break;
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

				/* a = nu1->pntsu + nu1->orderu; */ /* UNUSED */

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

			BKE_curve_nurb_active_set(cu, nu1);   /* for selected */
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
		BKE_report(op->reports, RPT_ERROR, "Cannot make segment");
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

bool mouse_nurb(bContext *C, const int mval[2], bool extend, bool deselect, bool toggle)
{
	Object *obedit = CTX_data_edit_object(C);
	Curve *cu = obedit->data;
	ListBase *editnurb = object_editcurve_get(obedit);
	ViewContext vc;
	Nurb *nu;
	BezTriple *bezt = NULL;
	BPoint *bp = NULL;
	const void *vert = BKE_curve_vert_active_get(cu);
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
					select_beztriple(bezt, SELECT, SELECT, HIDDEN);
					BKE_curve_nurb_vert_active_set(cu, nu, bezt);
				}
				else {
					if (hand == 0) bezt->f1 |= SELECT;
					else bezt->f3 |= SELECT;

					cu->actvert = CU_ACT_NONE;
				}
			}
			else {
				BKE_curve_nurb_vert_active_set(cu, nu, bp);
				select_bpoint(bp, SELECT, SELECT, HIDDEN);
			}
		}
		else if (deselect) {
			if (bezt) {
				if (hand == 1) {
					select_beztriple(bezt, DESELECT, SELECT, HIDDEN);
					if (bezt == vert) cu->actvert = CU_ACT_NONE;
				}
				else if (hand == 0) {
					bezt->f1 &= ~SELECT;
				}
				else {
					bezt->f3 &= ~SELECT;
				}
			}
			else {
				select_bpoint(bp, DESELECT, SELECT, HIDDEN);
				if (bp == vert) cu->actvert = CU_ACT_NONE;
			}
		}
		else if (toggle) {
			if (bezt) {
				if (hand == 1) {
					if (bezt->f2 & SELECT) {
						select_beztriple(bezt, DESELECT, SELECT, HIDDEN);
						if (bezt == vert) cu->actvert = CU_ACT_NONE;
					}
					else {
						select_beztriple(bezt, SELECT, SELECT, HIDDEN);
						BKE_curve_nurb_vert_active_set(cu, nu, bezt);
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
					select_bpoint(bp, DESELECT, SELECT, HIDDEN);
					if (bp == vert) cu->actvert = CU_ACT_NONE;
				}
				else {
					select_bpoint(bp, SELECT, SELECT, HIDDEN);
					BKE_curve_nurb_vert_active_set(cu, nu, bp);
				}
			}
		}
		else {
			BKE_nurbList_flag_set(editnurb, 0);

			if (bezt) {

				if (hand == 1) {
					select_beztriple(bezt, SELECT, SELECT, HIDDEN);
					BKE_curve_nurb_vert_active_set(cu, nu, bezt);
				}
				else {
					if (hand == 0) bezt->f1 |= SELECT;
					else bezt->f3 |= SELECT;

					cu->actvert = CU_ACT_NONE;
				}
			}
			else {
				BKE_curve_nurb_vert_active_set(cu, nu, bp);
				select_bpoint(bp, SELECT, SELECT, HIDDEN);
			}
		}

		if (nu != BKE_curve_nurb_active_get(cu)) {
			cu->actvert = CU_ACT_NONE;
			BKE_curve_nurb_active_set(cu, nu);
		}

		WM_event_add_notifier(C, NC_GEOM | ND_SELECT, obedit->data);

		return true;
	}
	
	return false;
}

/******************** spin operator ***********************/

/* 'cent' is in object space and 'dvec' in worldspace.
 */
bool ed_editnurb_spin(float viewmat[4][4], Object *obedit, const float axis[3], const float cent[3])
{
	Curve *cu = (Curve *)obedit->data;
	ListBase *editnurb = object_editcurve_get(obedit);
	Nurb *nu;
	float cmat[3][3], tmat[3][3], imat[3][3];
	float bmat[3][3], rotmat[3][3], scalemat1[3][3], scalemat2[3][3];
	float persmat[3][3], persinv[3][3];
	bool ok, changed = false;
	int a;

	copy_m3_m4(persmat, viewmat);
	invert_m3_m3(persinv, persmat);

	/* imat and center and size */
	copy_m3_m4(bmat, obedit->obmat);
	invert_m3_m3(imat, bmat);
	
	axis_angle_to_mat3(cmat, axis, M_PI / 4.0);
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

	ok = true;

	for (a = 0; a < 7; a++) {
		ok = ed_editnurb_extrude_flag(cu->editnurb, 1);

		if (ok == false)
			return changed;

		changed = true;

		rotateflagNurb(editnurb, SELECT, cent, rotmat);

		if ((a & 1) == 0) {
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
	
	if (!ed_editnurb_spin(viewmat, obedit, axis, cent)) {
		BKE_report(op->reports, RPT_ERROR, "Cannot spin");
		return OPERATOR_CANCELLED;
	}

	if (ED_curve_updateAnimPaths(obedit->data))
		WM_event_add_notifier(C, NC_OBJECT | ND_KEYS, obedit);

	WM_event_add_notifier(C, NC_GEOM | ND_DATA, obedit->data);
	DAG_id_tag_update(obedit->data, 0);

	return OPERATOR_FINISHED;
}

static int spin_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
	Scene *scene = CTX_data_scene(C);
	View3D *v3d = CTX_wm_view3d(C);
	RegionView3D *rv3d = ED_view3d_context_rv3d(C);
	float axis[3] = {0.0f, 0.0f, 1.0f};
	
	if (rv3d)
		copy_v3_v3(axis, rv3d->viewinv[2]);
	
	RNA_float_set_array(op->ptr, "center", ED_view3d_cursor3d_get(scene, v3d));
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
	RNA_def_float_vector(ot->srna, "axis", 3, NULL, -FLT_MAX, FLT_MAX, "Axis", "Axis in global view space", -1.0f, 1.0f);
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
				else {
					memcpy(newnu, nu, sizeof(Nurb));
				}

				BLI_addtail(&editnurb->nurbs, newnu);
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

				newnu = (Nurb *)MEM_mallocN(sizeof(Nurb), "addvert_Nurb newnu");
				memcpy(newnu, nu, sizeof(Nurb));
				BLI_addtail(&editnurb->nurbs, newnu);
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
			newnu->bezt = newbezt;
			newnu->pntsu = 1;

			nu = newnu;
			bezt = newbezt;
			ok = 1;
		}
		else {
			bezt = NULL;
		}

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
			bp = newbp - 1;
			ok = 1;
		}
		else if (bp == nu->bp) {   /* first */
			bp->f1 = 0;
			newbp = (BPoint *)MEM_callocN((nu->pntsu + 1) * sizeof(BPoint), "addvert_Nurb3");
			ED_curve_bpcpy(editnurb, newbp + 1, bp, nu->pntsu);
			*newbp = *bp;
			newbp->f1 |= SELECT;
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

			newnu = (Nurb *)MEM_mallocN(sizeof(Nurb), "addvert_Nurb newnu");
			memcpy(newnu, nu, sizeof(Nurb));
			BLI_addtail(&editnurb->nurbs, newnu);
			newnu->bp = newbp;
			newnu->orderu = 2;
			newnu->pntsu = 1;
			newnu->knotsu = newnu->knotsv = NULL;

			nu = newnu;
			bp = newbp;
			ok = 1;
		}
		else {
			bp = NULL;
		}

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
			else {
				BKE_nurb_knot_calc_u(newnu);
			}
		}
	}

	if (ok) {
		if (nu->bezt) {
			BKE_curve_nurb_vert_active_set(cu, nu, newbezt);
		}
		else {
			BKE_curve_nurb_vert_active_set(cu, nu, newbp);
		}

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

static int add_vertex_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
	ViewContext vc;

	view3d_set_viewcontext(C, &vc);

	if (vc.rv3d && !RNA_struct_property_is_set(op->ptr, "location")) {
		Curve *cu;
		float location[3];
		const bool use_proj = ((vc.scene->toolsettings->snap_flag & SCE_SNAP) &&
		                       (vc.scene->toolsettings->snap_mode == SCE_SNAP_MODE_FACE));

		Nurb *nu;
		BezTriple *bezt;
		BPoint *bp;

		cu = vc.obedit->data;

		findselectedNurbvert(&cu->editnurb->nurbs, &nu, &bezt, &bp);

		if (bezt) {
			mul_v3_m4v3(location, vc.obedit->obmat, bezt->vec[1]);
		}
		else if (bp) {
			mul_v3_m4v3(location, vc.obedit->obmat, bp->vec);
		}
		else {
			copy_v3_v3(location, ED_view3d_cursor3d_get(vc.scene, vc.v3d));
		}

		ED_view3d_win_to_3d_int(vc.ar, location, event->mval, location);

		if (use_proj) {
			const float mval[2] = {UNPACK2(event->mval)};
			float no_dummy[3];
			float dist_px_dummy;
			snapObjectsContext(C, mval, &dist_px_dummy, location, no_dummy, SNAP_NOT_OBEDIT);
		}

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
		if (ed_editnurb_extrude_flag(editnurb, 1)) { /* '1'= flag */
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
					if (BEZSELECTED_HIDDENHANDLES(cu, bezt)) {
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

static int toggle_cyclic_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
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
					pup = uiPupMenuBegin(C, IFACE_("Direction"), ICON_NONE);
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
				if ((bezt->f1 & SELECT) || (bezt->f2 & SELECT) || (bezt->f3 & SELECT)) {
					a = nu->pntsu;
					bezt = nu->bezt;
					while (a--) {
						select_beztriple(bezt, SELECT, SELECT, VISIBLE);
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
						select_bpoint(bp, SELECT, SELECT, VISIBLE);
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

static int select_linked_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
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

static int select_linked_pick_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
	Object *obedit = CTX_data_edit_object(C);
	ViewContext vc;
	Nurb *nu;
	BezTriple *bezt;
	BPoint *bp;
	int a;
	const bool select = !RNA_boolean_get(op->ptr, "deselect");

	view3d_operator_needs_opengl(C);
	view3d_set_viewcontext(C, &vc);

	findnearestNurbvert(&vc, 1, event->mval, &nu, &bezt, &bp);

	if (bezt) {
		a = nu->pntsu;
		bezt = nu->bezt;
		while (a--) {
			select_beztriple(bezt, select, SELECT, VISIBLE);
			bezt++;
		}
	}
	else if (bp) {
		a = nu->pntsu * nu->pntsv;
		bp = nu->bp;
		while (a--) {
			select_bpoint(bp, select, SELECT, VISIBLE);
			bp++;
		}
	}

	WM_event_add_notifier(C, NC_GEOM | ND_SELECT, obedit->data);
	if (!select) {
		BKE_curve_nurb_vert_active_validate(obedit->data);
	}

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
	Nurb *nu = NULL;
	BPoint *bp = NULL;
	int u = 0, v = 0, a, b;

	if (!BKE_curve_nurb_vert_active_get(cu, &nu, (void *)&bp))
		return OPERATOR_CANCELLED;

	if (last == bp) {
		direction = 1 - direction;
		BKE_nurbList_flag_set(editnurb, 0);
	}
	last = bp;

	u = cu->actvert % nu->pntsu;
	v = cu->actvert / nu->pntsu;
	bp = nu->bp;
	for (a = 0; a < nu->pntsv; a++) {
		for (b = 0; b < nu->pntsu; b++, bp++) {
			if (direction) {
				if (a == v) select_bpoint(bp, SELECT, SELECT, VISIBLE);
			}
			else {
				if (b == u) select_bpoint(bp, SELECT, SELECT, VISIBLE);
			}
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
	
	/* note that NURBS surface is a special case because we mimic */
	/* the behavior of "select more" of mesh tools.	      */
	/* The algorithm is designed to work in planar cases so it    */
	/* may not be optimal always (example: end of NURBS sphere)   */
	if (obedit->type == OB_SURF) {
		for (nu = editnurb->first; nu; nu = nu->next) {
			BLI_bitmap *selbpoints;
			a = nu->pntsu * nu->pntsv;
			bp = nu->bp;
			selbpoints = BLI_BITMAP_NEW(a, "selectlist");
			while (a > 0) {
				if ((!BLI_BITMAP_GET(selbpoints, a)) && (bp->hide == 0) && (bp->f1 & SELECT)) {
					/* upper control point */
					if (a % nu->pntsu != 0) {
						tempbp = bp - 1;
						if (!(tempbp->f1 & SELECT)) select_bpoint(tempbp, SELECT, SELECT, VISIBLE);
					}

					/* left control point. select only if it is not selected already */
					if (a - nu->pntsu > 0) {
						sel = 0;
						tempbp = bp + nu->pntsu;
						if (!(tempbp->f1 & SELECT)) sel = select_bpoint(tempbp, SELECT, SELECT, VISIBLE);
						/* make sure selected bpoint is discarded */
						if (sel == 1) BLI_BITMAP_SET(selbpoints, a - nu->pntsu);
					}
					
					/* right control point */
					if (a + nu->pntsu < nu->pntsu * nu->pntsv) {
						tempbp = bp - nu->pntsu;
						if (!(tempbp->f1 & SELECT)) select_bpoint(tempbp, SELECT, SELECT, VISIBLE);
					}
				
					/* lower control point. skip next bp in case selection was made */
					if (a % nu->pntsu != 1) {
						sel = 0;
						tempbp = bp + 1;
						if (!(tempbp->f1 & SELECT)) sel = select_bpoint(tempbp, SELECT, SELECT, VISIBLE);
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
	int sel = 0;
	short lastsel = false;
	
	if (obedit->type == OB_SURF) {
		for (nu = editnurb->first; nu; nu = nu->next) {
			BLI_bitmap *selbpoints;
			a = nu->pntsu * nu->pntsv;
			bp = nu->bp;
			selbpoints = BLI_BITMAP_NEW(a, "selectlist");
			while (a--) {
				if ((bp->hide == 0) && (bp->f1 & SELECT)) {
					sel = 0;

					/* check if neighbors have been selected */
					/* edges of surface are an exception */
					if ((a + 1) % nu->pntsu == 0) {
						sel++;
					}
					else {
						bp--;
						if (BLI_BITMAP_GET(selbpoints, a + 1) || ((bp->hide == 0) && (bp->f1 & SELECT))) sel++;
						bp++;
					}
					
					if ((a + 1) % nu->pntsu == 1) {
						sel++;
					}
					else {
						bp++;
						if ((bp->hide == 0) && (bp->f1 & SELECT)) sel++;
						bp--;
					}
					
					if (a + 1 > nu->pntsu * nu->pntsv - nu->pntsu) {
						sel++;
					}
					else {
						bp -= nu->pntsu;
						if (BLI_BITMAP_GET(selbpoints, a + nu->pntsu) || ((bp->hide == 0) && (bp->f1 & SELECT))) sel++;
						bp += nu->pntsu;
					}

					if (a < nu->pntsu) {
						sel++;
					}
					else {
						bp += nu->pntsu;
						if ((bp->hide == 0) && (bp->f1 & SELECT)) sel++;
						bp -= nu->pntsu;
					}

					if (sel != 4) {
						select_bpoint(bp, DESELECT, SELECT, VISIBLE);
						BLI_BITMAP_SET(selbpoints, a);
					}
				}
				else {
					lastsel = false;
				}

				bp++;
			}
			
			MEM_freeN(selbpoints);
		}
	}
	else {
		for (nu = editnurb->first; nu; nu = nu->next) {
			lastsel = false;
			/* check what type of curve/nurb it is */
			if (nu->type == CU_BEZIER) {
				a = nu->pntsu;
				bezt = nu->bezt;
				while (a--) {
					if ((bezt->hide == 0) && (bezt->f2 & SELECT)) {
						sel = (lastsel == 1);

						/* check if neighbors have been selected */
						/* first and last are exceptions */
						if (a == nu->pntsu - 1) {
							sel++;
						}
						else {
							bezt--;
							if ((bezt->hide == 0) && (bezt->f2 & SELECT)) sel++;
							bezt++;
						}
						
						if (a == 0) {
							sel++;
						}
						else {
							bezt++;
							if ((bezt->hide == 0) && (bezt->f2 & SELECT)) sel++;
							bezt--;
						}

						if (sel != 2) {
							select_beztriple(bezt, DESELECT, SELECT, VISIBLE);
							lastsel = true;
						}
						else {
							lastsel = false;
						}
					}
					else {
						lastsel = false;
					}

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
						if (a == nu->pntsu * nu->pntsv - 1) {
							sel++;
						}
						else {
							bp--;
							if ((bp->hide == 0) && (bp->f1 & SELECT)) sel++;
							bp++;
						}
						
						if (a == 0) {
							sel++;
						}
						else {
							bp++;
							if ((bp->hide == 0) && (bp->f1 & SELECT)) sel++;
							bp--;
						}

						if (sel != 2) {
							select_bpoint(bp, DESELECT, SELECT, VISIBLE);
							lastsel = true;
						}
						else {
							lastsel = false;
						}
					}
					else {
						lastsel = false;
					}

					bp++;
				}
			}
		}
	}
	
	WM_event_add_notifier(C, NC_GEOM | ND_SELECT, obedit->data);
	BKE_curve_nurb_vert_active_validate(obedit->data);

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

static void curve_select_random(ListBase *editnurb, float randfac, bool select)
{
	Nurb *nu;
	BezTriple *bezt;
	BPoint *bp;
	int a;
	
	for (nu = editnurb->first; nu; nu = nu->next) {
		if (nu->type == CU_BEZIER) {
			bezt = nu->bezt;
			a = nu->pntsu;
			while (a--) {
				if (!bezt->hide) {
					if (BLI_frand() < randfac) {
						select_beztriple(bezt, select, SELECT, VISIBLE);
					}
				}
				bezt++;
			}
		}
		else {
			bp = nu->bp;
			a = nu->pntsu * nu->pntsv;
			
			while (a--) {
				if (!bp->hide) {
					if (BLI_frand() < randfac) {
						select_bpoint(bp, select, SELECT, VISIBLE);
					}
				}
				bp++;
			}
		}
	}
}

static int curve_select_random_exec(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	ListBase *editnurb = object_editcurve_get(obedit);
	const bool select = (RNA_enum_get(op->ptr, "action") == SEL_SELECT);
	const float randfac = RNA_float_get(op->ptr, "percent") / 100.0f;

	curve_select_random(editnurb, randfac, select);
	BKE_curve_nurb_vert_active_validate(obedit->data);

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
	ot->exec = curve_select_random_exec;
	ot->poll = ED_operator_editsurfcurve;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* properties */
	RNA_def_float_percentage(ot->srna, "percent", 50.f, 0.0f, 100.0f, "Percent", "Percentage of elements to select randomly", 0.f, 100.0f);
	WM_operator_properties_select_action_simple(ot, SEL_SELECT);
}

/********************* every nth number of point *******************/

static void select_nth_bezt(Nurb *nu, BezTriple *bezt, int nth)
{
	int a, start;

	start = bezt - nu->bezt;
	a = nu->pntsu;
	bezt = &nu->bezt[a - 1];

	while (a--) {
		if (abs(start - a) % nth) {
			select_beztriple(bezt, DESELECT, SELECT, HIDDEN);
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
			select_bpoint(bp, DESELECT, SELECT, HIDDEN);
		}

		pnt--;
		if (pnt < 0) {
			pnt = nu->pntsu - 1;
			row--;
		}

		bp--;
	}
}

bool ED_curve_select_nth(Curve *cu, int nth)
{
	Nurb *nu = NULL;
	void *vert = NULL;

	if (!BKE_curve_nurb_vert_active_get(cu, &nu, &vert))
		return false;

	if (nu->bezt) {
		select_nth_bezt(nu, vert, nth);
	}
	else {
		select_nth_bp(nu, vert, nth);
	}

	return true;
}

static int select_nth_exec(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	int nth = RNA_int_get(op->ptr, "nth");

	if (!ED_curve_select_nth(obedit->data, nth)) {
		if (obedit->type == OB_SURF) {
			BKE_report(op->reports, RPT_ERROR, "Surface has not got active point");
		}
		else {
			BKE_report(op->reports, RPT_ERROR, "Curve has not got active point");
		}

		return OPERATOR_CANCELLED;
	}

	WM_event_add_notifier(C, NC_GEOM | ND_SELECT, obedit->data);

	return OPERATOR_FINISHED;
}

void CURVE_OT_select_nth(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Checker Deselect";
	ot->description = "Deselect every other vertex";
	ot->idname = "CURVE_OT_select_nth";

	/* api callbacks */
	ot->exec = select_nth_exec;
	ot->poll = ED_operator_editsurfcurve;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	RNA_def_int(ot->srna, "nth", 2, 2, INT_MAX, "Nth Selection", "", 2, 100);
}

/********************** add duplicate operator *********************/

static int duplicate_exec(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	ListBase newnurb = {NULL, NULL};

	adduplicateflagNurb(obedit, &newnurb, SELECT, false);

	if (BLI_listbase_is_empty(&newnurb) == false) {
		BLI_movelisttolist(object_editcurve_get(obedit), &newnurb);
		WM_event_add_notifier(C, NC_GEOM | ND_SELECT, obedit->data);
	}
	else {
		BKE_report(op->reports, RPT_ERROR, "Cannot duplicate current selection");
		return OPERATOR_CANCELLED;
	}

	return OPERATOR_FINISHED;
}

void CURVE_OT_duplicate(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Duplicate Curve";
	ot->description = "Duplicate selected control points";
	ot->idname = "CURVE_OT_duplicate";
	
	/* api callbacks */
	ot->exec = duplicate_exec;
	ot->poll = ED_operator_editsurfcurve;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/********************** delete operator *********************/

static int curve_delete_vertices(Object *obedit)
{
	if (obedit->type == OB_SURF) {
		ed_surf_delete_selected(obedit);
	}
	else {
		ed_curve_delete_selected(obedit);
	}

	return OPERATOR_FINISHED;
}

static int curve_delete_segments(Object *obedit, const bool split)
{
	Curve *cu = obedit->data;
	EditNurb *editnurb = cu->editnurb;
	ListBase *nubase = &editnurb->nurbs, newnurb = {NULL, NULL};
	Nurb *nu, *nu1;
	BezTriple *bezt, *bezt1, *bezt2;
	BPoint *bp, *bp1, *bp2;
	int a, b, starta, enda, cut, cyclicut;

	for (nu = nubase->first; nu; nu = nu->next) {
		nu1 = NULL;
		starta = enda = cut = -1;
		cyclicut = 0;

		if (nu->type == CU_BEZIER) {
			for (a = 0, bezt = nu->bezt; a < nu->pntsu; a++, bezt++) {
				if (!BEZSELECTED_HIDDENHANDLES(cu, bezt)) {
					enda = a;
					if (starta == -1) starta = a;
					if (a < nu->pntsu - 1) continue;
				}
				else if (a < nu->pntsu - 1 && !BEZSELECTED_HIDDENHANDLES(cu, bezt + 1)) {
					/* if just single selected point then continue */
					continue;
				}

				if (starta >= 0) {
					/* got selected segment, now check where and copy */
					if (starta <= 1 && a == nu->pntsu - 1) {
						/* copying all points in spline */
						if (starta == 1 && enda != a) nu->flagu &= ~CU_NURB_CYCLIC;

						starta = 0;
						enda = a;
						cut = enda - starta + 1;
						nu1 = BKE_nurb_copy(nu, cut, 1);
					}
					else if (starta == 0) {
						/* if start of curve copy next end point */
						enda++;
						cut = enda - starta + 1;
						bezt1 = &nu->bezt[nu->pntsu - 1];
						bezt2 = &nu->bezt[nu->pntsu - 2];

						if ((nu->flagu & CU_NURB_CYCLIC) &&
						    BEZSELECTED_HIDDENHANDLES(cu, bezt1) &&
						    BEZSELECTED_HIDDENHANDLES(cu, bezt2))
						{
							/* check if need to join start of spline to end */
							nu1 = BKE_nurb_copy(nu, cut + 1, 1);
							ED_curve_beztcpy(editnurb, &nu1->bezt[1], nu->bezt, cut);
							starta = nu->pntsu - 1;
							cut = 1;
						}
						else {
							if (nu->flagu & CU_NURB_CYCLIC) cyclicut = cut;
							else nu1 = BKE_nurb_copy(nu, cut, 1);
						}
					}
					else if (enda == nu->pntsu - 1) {
						/* if end of curve copy previous start point */
						starta--;
						cut = enda - starta + 1;
						bezt1 = nu->bezt;
						bezt2 = &nu->bezt[1];

						if ((nu->flagu & CU_NURB_CYCLIC) &&
						    BEZSELECTED_HIDDENHANDLES(cu, bezt1) &&
						    BEZSELECTED_HIDDENHANDLES(cu, bezt2))
						{
							/* check if need to join start of spline to end */
							nu1 = BKE_nurb_copy(nu, cut + 1, 1);
							ED_curve_beztcpy(editnurb, &nu1->bezt[cut], nu->bezt, 1);
						}
						else if (cyclicut != 0) {
							/* if cyclicut exists it is a cyclic spline, start and end should be connected */
							nu1 = BKE_nurb_copy(nu, cut + cyclicut, 1);
							ED_curve_beztcpy(editnurb, &nu1->bezt[cut], nu->bezt, cyclicut);
							cyclicut = 0;
						}
						else {
							nu1 = BKE_nurb_copy(nu, cut, 1);
						}
					}
					else {
						/* mid spline selection, copy adjacent start and end */
						starta--;
						enda++;
						cut = enda - starta + 1;
						nu1 = BKE_nurb_copy(nu, cut, 1);
					}

					if (nu1 != NULL) {
						ED_curve_beztcpy(editnurb, nu1->bezt, &nu->bezt[starta], cut);
						BLI_addtail(&newnurb, nu1);

						if (starta != 0 || enda != nu->pntsu - 1) nu1->flagu &= ~CU_NURB_CYCLIC;
						nu1 = NULL;
					}
					starta = enda = -1;
				}
			}

			if (!split && cut != -1 && nu->pntsu > 2 && !(nu->flagu & CU_NURB_CYCLIC)) {
				/* start and points copied if connecting segment was deleted and not cylic spline */
				bezt1 = nu->bezt;
				bezt2 = &nu->bezt[1];

				if (BEZSELECTED_HIDDENHANDLES(cu, bezt1) &&
				    BEZSELECTED_HIDDENHANDLES(cu, bezt2))
				{
					nu1 = BKE_nurb_copy(nu, 1, 1);
					ED_curve_beztcpy(editnurb, nu1->bezt, bezt1, 1);
					BLI_addtail(&newnurb, nu1);
				}

				bezt1 = &nu->bezt[nu->pntsu - 1];
				bezt2 = &nu->bezt[nu->pntsu - 2];

				if (BEZSELECTED_HIDDENHANDLES(cu, bezt1) &&
				    BEZSELECTED_HIDDENHANDLES(cu, bezt2))
				{
					nu1 = BKE_nurb_copy(nu, 1, 1);
					ED_curve_beztcpy(editnurb, nu1->bezt, bezt1, 1);
					BLI_addtail(&newnurb, nu1);
				}
			}
		}
		else if (nu->pntsv >= 1) {
			int u, v;

			if (isNurbselV(nu, &u, SELECT)) {
				for (a = 0, bp = nu->bp; a < nu->pntsu; a++, bp++) {
					if (!(bp->f1 & SELECT)) {
						enda = a;
						if (starta == -1) starta = a;
						if (a < nu->pntsu - 1) continue;
					}
					else if (a < nu->pntsu - 1 && !((bp + 1)->f1 & SELECT)) {
						/* if just single selected point then continue */
						continue;
					}

					if (starta >= 0) {
						/* got selected segment, now check where and copy */
						if (starta <= 1 && a == nu->pntsu - 1) {
							/* copying all points in spline */
							if (starta == 1 && enda != a) nu->flagu &= ~CU_NURB_CYCLIC;

							starta = 0;
							enda = a;
							cut = enda - starta + 1;
							nu1 = BKE_nurb_copy(nu, cut, nu->pntsv);
						}
						else if (starta == 0) {
							/* if start of curve copy next end point */
							enda++;
							cut = enda - starta + 1;
							bp1 = &nu->bp[nu->pntsu - 1];
							bp2 = &nu->bp[nu->pntsu - 2];

							if ((nu->flagu & CU_NURB_CYCLIC) && (bp1->f1 & SELECT) && (bp2->f1 & SELECT)) {
								/* check if need to join start of spline to end */
								nu1 = BKE_nurb_copy(nu, cut + 1, nu->pntsv);
								for (b = 0; b < nu->pntsv; b++) {
									ED_curve_bpcpy(editnurb, &nu1->bp[b * nu1->pntsu + 1], &nu->bp[b * nu->pntsu], cut);
								}
								starta = nu->pntsu - 1;
								cut = 1;
							}
							else {
								if (nu->flagu & CU_NURB_CYCLIC) cyclicut = cut;
								else nu1 = BKE_nurb_copy(nu, cut, nu->pntsv);
							}
						}
						else if (enda == nu->pntsu - 1) {
							/* if end of curve copy previous start point */
							starta--;
							cut = enda - starta + 1;
							bp1 = nu->bp;
							bp2 = &nu->bp[1];

							if ((nu->flagu & CU_NURB_CYCLIC) && (bp1->f1 & SELECT) && (bp2->f1 & SELECT)) {
								/* check if need to join start of spline to end */
								nu1 = BKE_nurb_copy(nu, cut + 1, nu->pntsv);
								for (b = 0; b < nu->pntsv; b++) {
									ED_curve_bpcpy(editnurb, &nu1->bp[b * nu1->pntsu + cut], &nu->bp[b * nu->pntsu], 1);
								}
							}
							else if (cyclicut != 0) {
								/* if cyclicut exists it is a cyclic spline, start and end should be connected */
								nu1 = BKE_nurb_copy(nu, cut + cyclicut, nu->pntsv);
								for (b = 0; b < nu->pntsv; b++) {
									ED_curve_bpcpy(editnurb, &nu1->bp[b * nu1->pntsu + cut], &nu->bp[b * nu->pntsu], cyclicut);
								}
							}
							else {
								nu1 = BKE_nurb_copy(nu, cut, nu->pntsv);
							}
						}
						else {
							/* mid spline selection, copy adjacent start and end */
							starta--;
							enda++;
							cut = enda - starta + 1;
							nu1 = BKE_nurb_copy(nu, cut, nu->pntsv);
						}

						if (nu1 != NULL) {
							for (b = 0; b < nu->pntsv; b++) {
								ED_curve_bpcpy(editnurb, &nu1->bp[b * nu1->pntsu], &nu->bp[b * nu->pntsu + starta], cut);
							}
							BLI_addtail(&newnurb, nu1);

							if (starta != 0 || enda != nu->pntsu - 1) nu1->flagu &= ~CU_NURB_CYCLIC;
							nu1 = NULL;
						}
						starta = enda = -1;
					}
				}

				if (!split && cut != -1 && nu->pntsu > 2 && !(nu->flagu & CU_NURB_CYCLIC)) {
					/* start and points copied if connecting segment was deleted and not cylic spline */
					bp1 = nu->bp;
					bp2 = &nu->bp[1];

					if ((bp1->f1 & SELECT) && (bp2->f1 & SELECT)) {
						nu1 = BKE_nurb_copy(nu, 1, nu->pntsv);
						for (b = 0; b < nu->pntsv; b++) {
							ED_curve_bpcpy(editnurb, &nu1->bp[b], &nu->bp[b * nu->pntsu], 1);
						}
						BLI_addtail(&newnurb, nu1);
					}

					bp1 = &nu->bp[nu->pntsu - 1];
					bp2 = &nu->bp[nu->pntsu - 2];

					if ((bp1->f1 & SELECT) && (bp2->f1 & SELECT)) {
						nu1 = BKE_nurb_copy(nu, 1, nu->pntsv);
						for (b = 0; b < nu->pntsv; b++) {
							ED_curve_bpcpy(editnurb, &nu1->bp[b], &nu->bp[b * nu->pntsu + nu->pntsu - 1], 1);
						}
						BLI_addtail(&newnurb, nu1);
					}
				}
			}
			else if (isNurbselU(nu, &v, SELECT)) {
				for (a = 0, bp = nu->bp; a < nu->pntsv; a++, bp += nu->pntsu) {
					if (!(bp->f1 & SELECT)) {
						enda = a;
						if (starta == -1) starta = a;
						if (a < nu->pntsv - 1) continue;
					}
					else if (a < nu->pntsv - 1 && !((bp + nu->pntsu)->f1 & SELECT)) {
						/* if just single selected point then continue */
						continue;
					}

					if (starta >= 0) {
						/* got selected segment, now check where and copy */
						if (starta <= 1 && a == nu->pntsv - 1) {
							/* copying all points in spline */
							if (starta == 1 && enda != a) nu->flagv &= ~CU_NURB_CYCLIC;

							starta = 0;
							enda = a;
							cut = enda - starta + 1;
							nu1 = BKE_nurb_copy(nu, nu->pntsu, cut);
						}
						else if (starta == 0) {
							/* if start of curve copy next end point */
							enda++;
							cut = enda - starta + 1;
							bp1 = &nu->bp[nu->pntsv * nu->pntsu - nu->pntsu];
							bp2 = &nu->bp[nu->pntsv * nu->pntsu - (nu->pntsu * 2)];

							if ((nu->flagv & CU_NURB_CYCLIC) && (bp1->f1 & SELECT) && (bp2->f1 & SELECT)) {
								/* check if need to join start of spline to end */
								nu1 = BKE_nurb_copy(nu, nu->pntsu, cut + 1);
								ED_curve_bpcpy(editnurb, &nu1->bp[nu->pntsu], nu->bp, cut * nu->pntsu);
								starta = nu->pntsv - 1;
								cut = 1;
							}
							else {
								if (nu->flagv & CU_NURB_CYCLIC) cyclicut = cut;
								else nu1 = BKE_nurb_copy(nu, nu->pntsu, cut);
							}
						}
						else if (enda == nu->pntsv - 1) {
							/* if end of curve copy previous start point */
							starta--;
							cut = enda - starta + 1;
							bp1 = nu->bp;
							bp2 = &nu->bp[nu->pntsu];

							if ((nu->flagv & CU_NURB_CYCLIC) && (bp1->f1 & SELECT) && (bp2->f1 & SELECT)) {
								/* check if need to join start of spline to end */
								nu1 = BKE_nurb_copy(nu, nu->pntsu, cut + 1);
								ED_curve_bpcpy(editnurb, &nu1->bp[cut * nu->pntsu], nu->bp, nu->pntsu);
							}
							else if (cyclicut != 0) {
								/* if cyclicut exists it is a cyclic spline, start and end should be connected */
								nu1 = BKE_nurb_copy(nu, nu->pntsu, cut + cyclicut);
								ED_curve_bpcpy(editnurb, &nu1->bp[cut * nu->pntsu], nu->bp, nu->pntsu * cyclicut);
								cyclicut = 0;
							}
							else {
								nu1 = BKE_nurb_copy(nu, nu->pntsu, cut);
							}
						}
						else {
							/* mid spline selection, copy adjacent start and end */
							starta--;
							enda++;
							cut = enda - starta + 1;
							nu1 = BKE_nurb_copy(nu, nu->pntsu, cut);
						}

						if (nu1 != NULL) {
							ED_curve_bpcpy(editnurb, nu1->bp, &nu->bp[starta * nu->pntsu], cut * nu->pntsu);
							BLI_addtail(&newnurb, nu1);

							if (starta != 0 || enda != nu->pntsv - 1) nu1->flagv &= ~CU_NURB_CYCLIC;
							nu1 = NULL;
						}
						starta = enda = -1;
					}
				}

				if (!split && cut != -1 && nu->pntsv > 2 && !(nu->flagv & CU_NURB_CYCLIC)) {
					/* start and points copied if connecting segment was deleted and not cylic spline */
					bp1 = nu->bp;
					bp2 = &nu->bp[nu->pntsu];

					if ((bp1->f1 & SELECT) && (bp2->f1 & SELECT)) {
						nu1 = BKE_nurb_copy(nu, nu->pntsu, 1);
						ED_curve_bpcpy(editnurb, nu1->bp, nu->bp, nu->pntsu);
						BLI_addtail(&newnurb, nu1);
					}

					bp1 = &nu->bp[nu->pntsu * nu->pntsv - nu->pntsu];
					bp2 = &nu->bp[nu->pntsu * nu->pntsv - (nu->pntsu * 2)];

					if ((bp1->f1 & SELECT) && (bp2->f1 & SELECT)) {
						nu1 = BKE_nurb_copy(nu, nu->pntsu, 1);
						ED_curve_bpcpy(editnurb, nu1->bp, &nu->bp[nu->pntsu * nu->pntsv - nu->pntsu], nu->pntsu);
						BLI_addtail(&newnurb, nu1);
					}
				}
			}
			else {
				/* selection not valid, just copy nurb to new list */
				nu1 = BKE_nurb_copy(nu, nu->pntsu, nu->pntsv);
				ED_curve_bpcpy(editnurb, nu1->bp, nu->bp, nu->pntsu * nu->pntsv);
				BLI_addtail(&newnurb, nu1);
			}
		}
	}

	for (nu = newnurb.first; nu; nu = nu->next) {
		if (nu->type == CU_BEZIER) {
			if (split) {
				/* deselect for split operator */
				for (b = 0, bezt1 = nu->bezt; b < nu->pntsu; b++, bezt1++) {
					select_beztriple(bezt1, DESELECT, SELECT, true);
				}
			}

			BKE_nurb_handles_calc(nu);
		}
		else {
			if (split) {
				/* deselect for split operator */
				for (b = 0, bp1 = nu->bp; b < nu->pntsu * nu->pntsv; b++, bp1++) {
					select_bpoint(bp1, DESELECT, SELECT, HIDDEN);
				}
			}

			nu->knotsu = nu->knotsv = NULL;
			BKE_nurb_order_clamp_u(nu);
			BKE_nurb_knot_calc_u(nu);

			if (nu->pntsv > 1) {
				BKE_nurb_order_clamp_v(nu);
				BKE_nurb_knot_calc_v(nu);
			}
		}
	}

	keyIndex_delNurbList(editnurb, nubase);
	BKE_nurbList_free(nubase);
	BLI_movelisttolist(nubase, &newnurb);

	return OPERATOR_FINISHED;
}

static int curve_delete_exec(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	Curve *cu = (Curve *)obedit->data;
	eCurveElem_Types type = RNA_enum_get(op->ptr, "type");
	int retval = OPERATOR_CANCELLED;

	if (type == CURVE_VERTEX) retval = curve_delete_vertices(obedit);
	else if (type == CURVE_SEGMENT) retval = curve_delete_segments(obedit, false);
	else BLI_assert(0);

	if (retval == OPERATOR_FINISHED) {
		cu->actnu = cu->actvert = CU_ACT_NONE;

		if (ED_curve_updateAnimPaths(obedit->data)) WM_event_add_notifier(C, NC_OBJECT | ND_KEYS, obedit);

		WM_event_add_notifier(C, NC_GEOM | ND_DATA, obedit->data);
		DAG_id_tag_update(obedit->data, 0);

		return retval;
	}

	return retval;
}

static EnumPropertyItem curve_delete_type_items[] = {
	{CURVE_VERTEX, "VERT", 0, "Vertices", ""},
	{CURVE_SEGMENT, "SEGMENT", 0, "Segments", ""},
	{0, NULL, 0, NULL, NULL}
};

static EnumPropertyItem *rna_curve_delete_type_itemf(bContext *C, PointerRNA *UNUSED(ptr),
                                                            PropertyRNA *UNUSED(prop), bool *r_free)
{
	EnumPropertyItem *item = NULL;
	int totitem = 0;

	if (!C) /* needed for docs and i18n tools */
		return curve_delete_type_items;

	RNA_enum_items_add_value(&item, &totitem, curve_delete_type_items, CURVE_VERTEX);
	RNA_enum_items_add_value(&item, &totitem, curve_delete_type_items, CURVE_SEGMENT);
	RNA_enum_item_end(&item, &totitem);
	*r_free = true;

	return item;
}

void CURVE_OT_delete(wmOperatorType *ot)
{
	PropertyRNA *prop;

	/* identifiers */
	ot->name = "Delete";
	ot->description = "Delete selected control points or segments";
	ot->idname = "CURVE_OT_delete";
	
	/* api callbacks */
	ot->exec = curve_delete_exec;
	ot->invoke = WM_menu_invoke;
	ot->poll = ED_operator_editsurfcurve;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* properties */
	prop = RNA_def_enum(ot->srna, "type", curve_delete_type_items, 0, "Type", "Which elements to delete");
	RNA_def_enum_funcs(prop, rna_curve_delete_type_itemf);

	ot->prop = prop;
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
/* TODO: shape keys - as with meshes */
int join_curve_exec(bContext *C, wmOperator *op)
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
	bool ok = false;

	CTX_DATA_BEGIN(C, Base *, base, selected_editable_bases)
	{
		if (base->object == ob) {
			ok = true;
			break;
		}
	}
	CTX_DATA_END;

	/* that way the active object is always selected */
	if (ok == false) {
		BKE_report(op->reports, RPT_WARNING, "Active object is not a selected curve");
		return OPERATOR_CANCELLED;
	}

	BLI_listbase_clear(&tempbase);
	
	/* trasnform all selected curves inverse in obact */
	invert_m4_m4(imat, ob->obmat);
	
	CTX_DATA_BEGIN(C, Base *, base, selected_editable_bases)
	{
		if (base->object->type == ob->type) {
			if (base->object != ob) {
			
				cu = base->object->data;
			
				if (cu->nurb.first) {
					/* watch it: switch order here really goes wrong */
					mul_m4_m4m4(cmat, imat, base->object->obmat);
					
					nu = cu->nurb.first;
					while (nu) {
						newnu = BKE_nurb_duplicate(nu);
						if (ob->totcol) { /* TODO, merge material lists */
							CLAMP(newnu->mat_nr, 0, ob->totcol - 1);
						}
						else {
							newnu->mat_nr = 0;
						}
						BLI_addtail(&tempbase, newnu);
						
						if ((bezt = newnu->bezt)) {
							a = newnu->pntsu;
							while (a--) {
								mul_m4_v3(cmat, bezt->vec[0]);
								mul_m4_v3(cmat, bezt->vec[1]);
								mul_m4_v3(cmat, bezt->vec[2]);
								bezt++;
							}
							BKE_nurb_handles_calc(newnu);
						}
						if ((bp = newnu->bp)) {
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
	
	DAG_relations_tag_update(bmain);   // because we removed object(s), call before editmode!
	
	ED_object_editmode_enter(C, EM_WAITCURSOR);
	ED_object_editmode_exit(C, EM_FREEDATA | EM_WAITCURSOR | EM_DO_UNDO);

	WM_event_add_notifier(C, NC_SCENE | ND_OB_ACTIVE, scene);

	return OPERATOR_FINISHED;
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
		BLI_ghash_free(editnurb->keyindex, NULL, MEM_freeN);
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

		if (editnurb->keyindex) {
			keyIndex_updateNurb(editnurb, nu, newnu);
		}

		BLI_addtail(editbase, newnu);
	}

	cu->actvert = undoCurve->actvert;
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

		if (undoCurve->undoIndex) {
			keyIndex_updateNurb(&tmpEditnurb, nu, newnu);
		}

		BLI_addtail(&undoCurve->nubase, newnu);
	}

	undoCurve->actvert = cu->actvert;
	undoCurve->actnu = cu->actnu;

	return undoCurve;
}

static void free_undoCurve(void *ucv)
{
	UndoCurve *undoCurve = ucv;

	BKE_nurbList_free(&undoCurve->nubase);

	if (undoCurve->undoIndex)
		BLI_ghash_free(undoCurve->undoIndex, NULL, MEM_freeN);

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

bool ED_curve_active_center(Curve *cu, float center[3])
{
	Nurb *nu = NULL;
	void *vert = NULL;

	if (!BKE_curve_nurb_vert_active_get(cu, &nu, &vert))
		return false;

	if (nu->type == CU_BEZIER) {
		BezTriple *bezt = (BezTriple *)vert;
		copy_v3_v3(center, bezt->vec[1]);
	}
	else {
		BPoint *bp = (BPoint *)vert;
		copy_v3_v3(center, bp->vec);
	}

	return true;
}

/******************** Match texture space operator ***********************/

static int match_texture_space_poll(bContext *C)
{
	Object *object = CTX_data_active_object(C);

	return object && ELEM3(object->type, OB_CURVE, OB_SURF, OB_FONT);
}

static int match_texture_space_exec(bContext *C, wmOperator *UNUSED(op))
{
	Scene *scene = CTX_data_scene(C);
	Object *object = CTX_data_active_object(C);
	Curve *curve = (Curve *) object->data;
	float min[3], max[3], size[3], loc[3];
	int a;

	if (object->curve_cache == NULL) {
		BKE_displist_make_curveTypes(scene, object, FALSE);
	}

	INIT_MINMAX(min, max);
	BKE_displist_minmax(&object->curve_cache->disp, min, max);

	mid_v3_v3v3(loc, min, max);

	size[0] = (max[0] - min[0]) / 2.0f;
	size[1] = (max[1] - min[1]) / 2.0f;
	size[2] = (max[2] - min[2]) / 2.0f;

	for (a = 0; a < 3; a++) {
		if (size[a] == 0.0f) size[a] = 1.0f;
		else if (size[a] > 0.0f && size[a] < 0.00001f) size[a] = 0.00001f;
		else if (size[a] < 0.0f && size[a] > -0.00001f) size[a] = -0.00001f;
	}

	copy_v3_v3(curve->loc, loc);
	copy_v3_v3(curve->size, size);
	zero_v3(curve->rot);

	curve->texflag &= ~CU_AUTOSPACE;

	WM_event_add_notifier(C, NC_GEOM | ND_DATA, curve);

	return OPERATOR_FINISHED;
}

void CURVE_OT_match_texture_space(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Match Texture Space";
	ot->idname = "CURVE_OT_match_texture_space";
	ot->description = "Match texture space to object's bounding box";

	/* api callbacks */
	ot->exec = match_texture_space_exec;
	ot->poll = match_texture_space_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}
