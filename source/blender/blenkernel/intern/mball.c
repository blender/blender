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
 * Contributor(s): Jiri Hnidek <jiri.hnidek@vslib.cz>.
 *
 * ***** END GPL LICENSE BLOCK *****
 *
 * MetaBalls are created from a single Object (with a name without number in it),
 * here the DispList and BoundBox also is located.
 * All objects with the same name (but with a number in it) are added to this.
 *
 * texture coordinates are patched within the displist
 */

/** \file blender/blenkernel/intern/mball.c
 *  \ingroup bke
 */

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <ctype.h>
#include <float.h>

#include "MEM_guardedalloc.h"

#include "DNA_material_types.h"
#include "DNA_object_types.h"
#include "DNA_meta_types.h"
#include "DNA_scene_types.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_string_utils.h"
#include "BLI_utildefines.h"

#include "BKE_global.h"
#include "BKE_main.h"

#include "BKE_animsys.h"
#include "BKE_curve.h"
#include "BKE_depsgraph.h"
#include "BKE_scene.h"
#include "BKE_library.h"
#include "BKE_library_query.h"
#include "BKE_library_remap.h"
#include "BKE_displist.h"
#include "BKE_mball.h"
#include "BKE_object.h"
#include "BKE_material.h"

/* Functions */

/** Free (or release) any data used by this mball (does not free the mball itself). */
void BKE_mball_free(MetaBall *mb)
{
	BKE_animdata_free((ID *)mb, false);

	MEM_SAFE_FREE(mb->mat);

	BLI_freelistN(&mb->elems);
	if (mb->disp.first) BKE_displist_free(&mb->disp);
}

void BKE_mball_init(MetaBall *mb)
{
	BLI_assert(MEMCMP_STRUCT_OFS_IS_ZERO(mb, id));

	mb->size[0] = mb->size[1] = mb->size[2] = 1.0;
	mb->texflag = MB_AUTOSPACE;
	
	mb->wiresize = 0.4f;
	mb->rendersize = 0.2f;
	mb->thresh = 0.6f;
}

MetaBall *BKE_mball_add(Main *bmain, const char *name)
{
	MetaBall *mb;

	mb = BKE_libblock_alloc(bmain, ID_MB, name);

	BKE_mball_init(mb);

	return mb;
}

MetaBall *BKE_mball_copy(Main *bmain, const MetaBall *mb)
{
	MetaBall *mbn;
	int a;
	
	mbn = BKE_libblock_copy(bmain, &mb->id);

	BLI_duplicatelist(&mbn->elems, &mb->elems);
	
	mbn->mat = MEM_dupallocN(mb->mat);
	for (a = 0; a < mbn->totcol; a++) {
		id_us_plus((ID *)mbn->mat[a]);
	}

	mbn->editelems = NULL;
	mbn->lastelem = NULL;
	
	BKE_id_copy_ensure_local(bmain, &mb->id, &mbn->id);

	return mbn;
}

void BKE_mball_make_local(Main *bmain, MetaBall *mb, const bool lib_local)
{
	BKE_id_make_local_generic(bmain, &mb->id, true, lib_local);
}

/* most simple meta-element adding function
 * don't do context manipulation here (rna uses) */
MetaElem *BKE_mball_element_add(MetaBall *mb, const int type)
{
	MetaElem *ml = MEM_callocN(sizeof(MetaElem), "metaelem");

	unit_qt(ml->quat);

	ml->rad = 2.0;
	ml->s = 2.0;
	ml->flag = MB_SCALE_RAD;

	switch (type) {
		case MB_BALL:
			ml->type = MB_BALL;
			ml->expx = ml->expy = ml->expz = 1.0;

			break;
		case MB_TUBE:
			ml->type = MB_TUBE;
			ml->expx = ml->expy = ml->expz = 1.0;

			break;
		case MB_PLANE:
			ml->type = MB_PLANE;
			ml->expx = ml->expy = ml->expz = 1.0;

			break;
		case MB_ELIPSOID:
			ml->type = MB_ELIPSOID;
			ml->expx = 1.2f;
			ml->expy = 0.8f;
			ml->expz = 1.0;

			break;
		case MB_CUBE:
			ml->type = MB_CUBE;
			ml->expx = ml->expy = ml->expz = 1.0;

			break;
		default:
			break;
	}

	BLI_addtail(&mb->elems, ml);

	return ml;
}
/** Compute bounding box of all MetaElems/MetaBalls.
 *
 * Bounding box is computed from polygonized surface. Object *ob is
 * basic MetaBall (usually with name Meta). All other MetaBalls (with
 * names Meta.001, Meta.002, etc) are included in this Bounding Box.
 */
void BKE_mball_texspace_calc(Object *ob)
{
	DispList *dl;
	BoundBox *bb;
	float *data, min[3], max[3] /*, loc[3], size[3] */;
	int tot;
	bool do_it = false;

	if (ob->bb == NULL) ob->bb = MEM_callocN(sizeof(BoundBox), "mb boundbox");
	bb = ob->bb;
	
	/* Weird one, this. */
/*      INIT_MINMAX(min, max); */
	(min)[0] = (min)[1] = (min)[2] = 1.0e30f;
	(max)[0] = (max)[1] = (max)[2] = -1.0e30f;

	dl = ob->curve_cache->disp.first;
	while (dl) {
		tot = dl->nr;
		if (tot) do_it = true;
		data = dl->verts;
		while (tot--) {
			/* Also weird... but longer. From utildefines. */
			minmax_v3v3_v3(min, max, data);
			data += 3;
		}
		dl = dl->next;
	}

	if (!do_it) {
		min[0] = min[1] = min[2] = -1.0f;
		max[0] = max[1] = max[2] = 1.0f;
	}
#if 0
	loc[0] = (min[0] + max[0]) / 2.0f;
	loc[1] = (min[1] + max[1]) / 2.0f;
	loc[2] = (min[2] + max[2]) / 2.0f;

	size[0] = (max[0] - min[0]) / 2.0f;
	size[1] = (max[1] - min[1]) / 2.0f;
	size[2] = (max[2] - min[2]) / 2.0f;
#endif
	BKE_boundbox_init_from_minmax(bb, min, max);

	bb->flag &= ~BOUNDBOX_DIRTY;
}

float *BKE_mball_make_orco(Object *ob, ListBase *dispbase)
{
	BoundBox *bb;
	DispList *dl;
	float *data, *orco, *orcodata;
	float loc[3], size[3];
	int a;

	/* restore size and loc */
	bb = ob->bb;
	loc[0] = (bb->vec[0][0] + bb->vec[4][0]) / 2.0f;
	size[0] = bb->vec[4][0] - loc[0];
	loc[1] = (bb->vec[0][1] + bb->vec[2][1]) / 2.0f;
	size[1] = bb->vec[2][1] - loc[1];
	loc[2] = (bb->vec[0][2] + bb->vec[1][2]) / 2.0f;
	size[2] = bb->vec[1][2] - loc[2];

	dl = dispbase->first;
	orcodata = MEM_mallocN(sizeof(float) * 3 * dl->nr, "MballOrco");

	data = dl->verts;
	orco = orcodata;
	a = dl->nr;
	while (a--) {
		orco[0] = (data[0] - loc[0]) / size[0];
		orco[1] = (data[1] - loc[1]) / size[1];
		orco[2] = (data[2] - loc[2]) / size[2];

		data += 3;
		orco += 3;
	}

	return orcodata;
}

/* Note on mball basis stuff 2.5x (this is a can of worms)
 * This really needs a rewrite/refactor its totally broken in anything other then basic cases
 * Multiple Scenes + Set Scenes & mixing mball basis SHOULD work but fails to update the depsgraph on rename
 * and linking into scenes or removal of basis mball. so take care when changing this code.
 * 
 * Main idiot thing here is that the system returns find_basis_mball() objects which fail a is_basis_mball() test.
 *
 * Not only that but the depsgraph and their areas depend on this behavior!, so making small fixes here isn't worth it.
 * - Campbell
 */


/** \brief Test, if Object *ob is basic MetaBall.
 *
 * It test last character of Object ID name. If last character
 * is digit it return 0, else it return 1.
 */
bool BKE_mball_is_basis(Object *ob)
{
	/* just a quick test */
	const int len = strlen(ob->id.name);
	return (!isdigit(ob->id.name[len - 1]));
}

/* return nonzero if ob1 is a basis mball for ob */
bool BKE_mball_is_basis_for(Object *ob1, Object *ob2)
{
	int basis1nr, basis2nr;
	char basis1name[MAX_ID_NAME], basis2name[MAX_ID_NAME];

	BLI_split_name_num(basis1name, &basis1nr, ob1->id.name + 2, '.');
	BLI_split_name_num(basis2name, &basis2nr, ob2->id.name + 2, '.');

	if (STREQ(basis1name, basis2name)) {
		return BKE_mball_is_basis(ob1);
	}
	else {
		return false;
	}
}

/* \brief copy some properties from object to other metaball object with same base name
 *
 * When some properties (wiresize, threshold, update flags) of metaball are changed, then this properties
 * are copied to all metaballs in same "group" (metaballs with same base name: MBall,
 * MBall.001, MBall.002, etc). The most important is to copy properties to the base metaball,
 * because this metaball influence polygonisation of metaballs. */
void BKE_mball_properties_copy(Scene *scene, Object *active_object)
{
	Scene *sce_iter = scene;
	Base *base;
	Object *ob;
	MetaBall *active_mball = (MetaBall *)active_object->data;
	int basisnr, obnr;
	char basisname[MAX_ID_NAME], obname[MAX_ID_NAME];
	SceneBaseIter iter;
	EvaluationContext *eval_ctx = G.main->eval_ctx;

	BLI_split_name_num(basisname, &basisnr, active_object->id.name + 2, '.');

	BKE_scene_base_iter_next(eval_ctx, &iter, &sce_iter, 0, NULL, NULL);
	while (BKE_scene_base_iter_next(eval_ctx, &iter, &sce_iter, 1, &base, &ob)) {
		if (ob->type == OB_MBALL) {
			if (ob != active_object) {
				BLI_split_name_num(obname, &obnr, ob->id.name + 2, '.');

				/* Object ob has to be in same "group" ... it means, that it has to have
				 * same base of its name */
				if (STREQ(obname, basisname)) {
					MetaBall *mb = ob->data;

					/* Copy properties from selected/edited metaball */
					mb->wiresize = active_mball->wiresize;
					mb->rendersize = active_mball->rendersize;
					mb->thresh = active_mball->thresh;
					mb->flag = active_mball->flag;
				}
			}
		}
	}
}

/** \brief This function finds basic MetaBall.
 *
 * Basic MetaBall doesn't include any number at the end of
 * its name. All MetaBalls with same base of name can be
 * blended. MetaBalls with different basic name can't be
 * blended.
 *
 * warning!, is_basis_mball() can fail on returned object, see long note above.
 */
Object *BKE_mball_basis_find(Scene *scene, Object *basis)
{
	Scene *sce_iter = scene;
	Base *base;
	Object *ob, *bob = basis;
	int basisnr, obnr;
	char basisname[MAX_ID_NAME], obname[MAX_ID_NAME];
	SceneBaseIter iter;
	EvaluationContext *eval_ctx = G.main->eval_ctx;

	BLI_split_name_num(basisname, &basisnr, basis->id.name + 2, '.');

	BKE_scene_base_iter_next(eval_ctx, &iter, &sce_iter, 0, NULL, NULL);
	while (BKE_scene_base_iter_next(eval_ctx, &iter, &sce_iter, 1, &base, &ob)) {
		if ((ob->type == OB_MBALL) && !(base->flag & OB_FROMDUPLI)) {
			if (ob != bob) {
				BLI_split_name_num(obname, &obnr, ob->id.name + 2, '.');

				/* object ob has to be in same "group" ... it means, that it has to have same base of its name */
				if (STREQ(obname, basisname)) {
					if (obnr < basisnr) {
						basis = ob;
						basisnr = obnr;
					}
				}
			}
		}
	}

	return basis;
}

bool BKE_mball_minmax_ex(MetaBall *mb, float min[3], float max[3],
                         float obmat[4][4], const short flag)
{
	const float scale = obmat ? mat4_to_scale(obmat) : 1.0f;
	MetaElem *ml;
	bool changed = false;
	float centroid[3], vec[3];

	INIT_MINMAX(min, max);

	for (ml = mb->elems.first; ml; ml = ml->next) {
		if ((ml->flag & flag) == flag) {
			const float scale_mb = (ml->rad * 0.5f) * scale;
			int i;

			if (obmat) {
				mul_v3_m4v3(centroid, obmat, &ml->x);
			}
			else {
				copy_v3_v3(centroid, &ml->x);
			}

			/* TODO, non circle shapes cubes etc, probably nobody notices - campbell */
			for (i = -1; i != 3; i += 2) {
				copy_v3_v3(vec, centroid);
				add_v3_fl(vec, scale_mb * i);
				minmax_v3v3_v3(min, max, vec);
			}
			changed = true;
		}
	}

	return changed;
}


/* basic vertex data functions */
bool BKE_mball_minmax(MetaBall *mb, float min[3], float max[3])
{
	MetaElem *ml;

	INIT_MINMAX(min, max);

	for (ml = mb->elems.first; ml; ml = ml->next) {
		minmax_v3v3_v3(min, max, &ml->x);
	}

	return (BLI_listbase_is_empty(&mb->elems) == false);
}

bool BKE_mball_center_median(MetaBall *mb, float r_cent[3])
{
	MetaElem *ml;
	int total = 0;

	zero_v3(r_cent);

	for (ml = mb->elems.first; ml; ml = ml->next) {
		add_v3_v3(r_cent, &ml->x);
		total++;
	}

	if (total) {
		mul_v3_fl(r_cent, 1.0f / (float)total);
	}

	return (total != 0);
}

bool BKE_mball_center_bounds(MetaBall *mb, float r_cent[3])
{
	float min[3], max[3];

	if (BKE_mball_minmax(mb, min, max)) {
		mid_v3_v3v3(r_cent, min, max);
		return true;
	}

	return false;
}

void BKE_mball_transform(MetaBall *mb, float mat[4][4])
{
	MetaElem *me;
	float quat[4];
	const float scale = mat4_to_scale(mat);
	const float scale_sqrt = sqrtf(scale);

	mat4_to_quat(quat, mat);

	for (me = mb->elems.first; me; me = me->next) {
		mul_m4_v3(mat, &me->x);
		mul_qt_qtqt(me->quat, quat, me->quat);
		me->rad *= scale;
		/* hrmf, probably elems shouldn't be
		 * treating scale differently - campbell */
		if (!MB_TYPE_SIZE_SQUARED(me->type)) {
			mul_v3_fl(&me->expx, scale);
		}
		else {
			mul_v3_fl(&me->expx, scale_sqrt);
		}
	}
}

void BKE_mball_translate(MetaBall *mb, const float offset[3])
{
	MetaElem *ml;

	for (ml = mb->elems.first; ml; ml = ml->next) {
		add_v3_v3(&ml->x, offset);
	}
}

/* *** select funcs *** */
void BKE_mball_select_all(struct MetaBall *mb)
{
	MetaElem *ml;

	for (ml = mb->editelems->first; ml; ml = ml->next) {
		ml->flag |= SELECT;
	}
}

void BKE_mball_deselect_all(MetaBall *mb)
{
	MetaElem *ml;

	for (ml = mb->editelems->first; ml; ml = ml->next) {
		ml->flag &= ~SELECT;
	}
}

void BKE_mball_select_swap(struct MetaBall *mb)
{
	MetaElem *ml;

	for (ml = mb->editelems->first; ml; ml = ml->next) {
		ml->flag ^= SELECT;
	}
}

/* **** Depsgraph evaluation **** */

void BKE_mball_eval_geometry(EvaluationContext *UNUSED(eval_ctx),
                             MetaBall *UNUSED(mball))
{
}
