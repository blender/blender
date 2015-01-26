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

/** \file blender/blenkernel/intern/object_dupli.c
 *  \ingroup bke
 */

#include <limits.h>
#include <stdlib.h>
#include <stddef.h>

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_string_utf8.h"

#include "BLI_math.h"
#include "BLI_rand.h"

#include "DNA_anim_types.h"
#include "DNA_group_types.h"
#include "DNA_mesh_types.h"
#include "DNA_scene_types.h"
#include "DNA_vfont_types.h"

#include "BKE_animsys.h"
#include "BKE_DerivedMesh.h"
#include "BKE_depsgraph.h"
#include "BKE_font.h"
#include "BKE_group.h"
#include "BKE_global.h"
#include "BKE_lattice.h"
#include "BKE_main.h"
#include "BKE_mesh.h"
#include "BKE_object.h"
#include "BKE_particle.h"
#include "BKE_scene.h"
#include "BKE_editmesh.h"
#include "BKE_anim.h"


#include "BLI_strict_flags.h"

/* Dupli-Geometry */

typedef struct DupliContext {
	EvaluationContext *eval_ctx;
	bool do_update;
	bool animated;
	Group *group; /* XXX child objects are selected from this group if set, could be nicer */

	Scene *scene;
	Object *object;
	float space_mat[4][4];
	unsigned int lay;

	int persistent_id[MAX_DUPLI_RECUR];
	int level;
	int index;

	const struct DupliGenerator *gen;

	/* result containers */
	ListBase *duplilist; /* legacy doubly-linked list */
} DupliContext;

typedef struct DupliGenerator {
	short type;				/* dupli type */
	void (*make_duplis)(const DupliContext *ctx);
} DupliGenerator;

static const DupliGenerator *get_dupli_generator(const DupliContext *ctx);

/* create initial context for root object */
static void init_context(DupliContext *r_ctx, EvaluationContext *eval_ctx, Scene *scene, Object *ob, float space_mat[4][4], bool update)
{
	r_ctx->eval_ctx = eval_ctx;
	r_ctx->scene = scene;
	/* don't allow BKE_object_handle_update for viewport during render, can crash */
	r_ctx->do_update = update && !(G.is_rendering && eval_ctx->mode != DAG_EVAL_RENDER);
	r_ctx->animated = false;
	r_ctx->group = NULL;

	r_ctx->object = ob;
	if (space_mat)
		copy_m4_m4(r_ctx->space_mat, space_mat);
	else
		unit_m4(r_ctx->space_mat);
	r_ctx->lay = ob->lay;
	r_ctx->level = 0;

	r_ctx->gen = get_dupli_generator(r_ctx);

	r_ctx->duplilist = NULL;
}

/* create sub-context for recursive duplis */
static void copy_dupli_context(DupliContext *r_ctx, const DupliContext *ctx, Object *ob, float mat[4][4], int index, bool animated)
{
	*r_ctx = *ctx;
	
	r_ctx->animated |= animated; /* object animation makes all children animated */

	/* XXX annoying, previously was done by passing an ID* argument, this at least is more explicit */
	if (ctx->gen->type == OB_DUPLIGROUP)
		r_ctx->group = ctx->object->dup_group;

	r_ctx->object = ob;
	if (mat)
		mul_m4_m4m4(r_ctx->space_mat, (float (*)[4])ctx->space_mat, mat);
	r_ctx->persistent_id[r_ctx->level] = index;
	++r_ctx->level;

	r_ctx->gen = get_dupli_generator(r_ctx);
}

/* generate a dupli instance
 * mat is transform of the object relative to current context (including object obmat)
 */
static DupliObject *make_dupli(const DupliContext *ctx,
                               Object *ob, float mat[4][4], int index,
                               bool animated, bool hide)
{
	DupliObject *dob;
	int i;

	/* add a DupliObject instance to the result container */
	if (ctx->duplilist) {
		dob = MEM_callocN(sizeof(DupliObject), "dupli object");
		BLI_addtail(ctx->duplilist, dob);
	}
	else {
		return NULL;
	}

	dob->ob = ob;
	mul_m4_m4m4(dob->mat, (float (*)[4])ctx->space_mat, mat);
	dob->type = ctx->gen->type;
	dob->animated = animated || ctx->animated; /* object itself or some parent is animated */

	/* set persistent id, which is an array with a persistent index for each level
	 * (particle number, vertex number, ..). by comparing this we can find the same
	 * dupli object between frames, which is needed for motion blur. last level
	 * goes first in the array. */
	dob->persistent_id[0] = index;
	for (i = 1; i < ctx->level + 1; i++)
		dob->persistent_id[i] = ctx->persistent_id[ctx->level - i];
	/* fill rest of values with INT_MAX which index will never have as value */
	for (; i < MAX_DUPLI_RECUR; i++)
		dob->persistent_id[i] = INT_MAX;

	if (hide)
		dob->no_draw = true;
	/* metaballs never draw in duplis, they are instead merged into one by the basis
	 * mball outside of the group. this does mean that if that mball is not in the
	 * scene, they will not show up at all, limitation that should be solved once. */
	if (ob->type == OB_MBALL)
		dob->no_draw = true;

	return dob;
}

/* recursive dupli objects
 * space_mat is the local dupli space (excluding dupli object obmat!)
 */
static void make_recursive_duplis(const DupliContext *ctx, Object *ob, float space_mat[4][4], int index, bool animated)
{
	/* simple preventing of too deep nested groups with MAX_DUPLI_RECUR */
	if (ctx->level < MAX_DUPLI_RECUR) {
		DupliContext rctx;
		copy_dupli_context(&rctx, ctx, ob, space_mat, index, animated);
		if (rctx.gen) {
			rctx.gen->make_duplis(&rctx);
		}
	}
}

/* ---- Child Duplis ---- */

typedef void (*MakeChildDuplisFunc)(const DupliContext *ctx, void *userdata, Object *child);

static bool is_child(const Object *ob, const Object *parent)
{
	const Object *ob_parent = ob->parent;
	while (ob_parent) {
		if (ob_parent == parent)
			return true;
		ob_parent = ob_parent->parent;
	}
	return false;
}

/* create duplis from every child in scene or group */
static void make_child_duplis(const DupliContext *ctx, void *userdata, MakeChildDuplisFunc make_child_duplis_cb)
{
	Object *parent = ctx->object;
	Object *obedit = ctx->scene->obedit;

	if (ctx->group) {
		unsigned int lay = ctx->group->layer;
		GroupObject *go;
		for (go = ctx->group->gobject.first; go; go = go->next) {
			Object *ob = go->ob;

			if ((ob->lay & lay) && ob != obedit && is_child(ob, parent)) {
				/* mballs have a different dupli handling */
				if (ob->type != OB_MBALL)
					ob->flag |= OB_DONE;  /* doesnt render */

				make_child_duplis_cb(ctx, userdata, ob);
			}
		}
	}
	else {
		unsigned int lay = ctx->scene->lay;
		Base *base;
		for (base = ctx->scene->base.first; base; base = base->next) {
			Object *ob = base->object;

			if ((base->lay & lay) && ob != obedit && is_child(ob, parent)) {
				/* mballs have a different dupli handling */
				if (ob->type != OB_MBALL)
					ob->flag |= OB_DONE;  /* doesnt render */

				make_child_duplis_cb(ctx, userdata, ob);
			}
		}
	}
}


/*---- Implementations ----*/

/* OB_DUPLIGROUP */
static void make_duplis_group(const DupliContext *ctx)
{
	bool for_render = (ctx->eval_ctx->mode == DAG_EVAL_RENDER);
	Object *ob = ctx->object;
	Group *group;
	GroupObject *go;
	float group_mat[4][4];
	int id;
	bool animated, hide;

	if (ob->dup_group == NULL) return;
	group = ob->dup_group;

	/* combine group offset and obmat */
	unit_m4(group_mat);
	sub_v3_v3(group_mat[3], group->dupli_ofs);
	mul_m4_m4m4(group_mat, ob->obmat, group_mat);
	/* don't access 'ob->obmat' from now on. */

	/* handles animated groups */

	/* we need to check update for objects that are not in scene... */
	if (ctx->do_update) {
		/* note: update is optional because we don't always need object
		 * transformations to be correct. Also fixes bug [#29616]. */
		BKE_group_handle_recalc_and_update(ctx->eval_ctx, ctx->scene, ob, group);
	}

	animated = BKE_group_is_animated(group, ob);

	for (go = group->gobject.first, id = 0; go; go = go->next, id++) {
		/* note, if you check on layer here, render goes wrong... it still deforms verts and uses parent imat */
		if (go->ob != ob) {
			float mat[4][4];

			/* Special case for instancing dupli-groups, see: T40051
			 * this object may be instanced via dupli-verts/faces, in this case we don't want to render
			 * (blender convention), but _do_ show in the viewport.
			 *
			 * Regular objects work fine but not if we're instancing dupli-groups,
			 * because the rules for rendering aren't applied to objects they instance.
			 * We could recursively pass down the 'hide' flag instead, but that seems unnecessary.
			 */
			if (for_render && go->ob->parent && go->ob->parent->transflag & (OB_DUPLIVERTS | OB_DUPLIFACES)) {
				continue;
			}

			/* group dupli offset, should apply after everything else */
			mul_m4_m4m4(mat, group_mat, go->ob->obmat);

			/* check the group instance and object layers match, also that the object visible flags are ok. */
			hide = (go->ob->lay & group->layer) == 0 ||
			       (for_render ? go->ob->restrictflag & OB_RESTRICT_RENDER : go->ob->restrictflag & OB_RESTRICT_VIEW);

			make_dupli(ctx, go->ob, mat, id, animated, hide);

			/* recursion */
			make_recursive_duplis(ctx, go->ob, group_mat, id, animated);
		}
	}
}

const DupliGenerator gen_dupli_group = {
    OB_DUPLIGROUP,                  /* type */
    make_duplis_group               /* make_duplis */
};

/* OB_DUPLIFRAMES */
static void make_duplis_frames(const DupliContext *ctx)
{
	Scene *scene = ctx->scene;
	Object *ob = ctx->object;
	extern int enable_cu_speed; /* object.c */
	Object copyob;
	int cfrao = scene->r.cfra;
	int dupend = ob->dupend;

	/* dupliframes not supported inside groups */
	if (ctx->group)
		return;
	/* if we don't have any data/settings which will lead to object movement,
	 * don't waste time trying, as it will all look the same...
	 */
	if (ob->parent == NULL && BLI_listbase_is_empty(&ob->constraints) && ob->adt == NULL)
		return;

	/* make a copy of the object's original data (before any dupli-data overwrites it)
	 * as we'll need this to keep track of unkeyed data
	 *	- this doesn't take into account other data that can be reached from the object,
	 *	  for example it's shapekeys or bones, hence the need for an update flush at the end
	 */
	copyob = *ob;

	/* duplicate over the required range */
	if (ob->transflag & OB_DUPLINOSPEED) enable_cu_speed = 0;

	/* special flag to avoid setting recalc flags to notify the depsgraph of
	 * updates, as this is not a permanent change to the object */
	ob->id.flag |= LIB_ANIM_NO_RECALC;

	for (scene->r.cfra = ob->dupsta; scene->r.cfra <= dupend; scene->r.cfra++) {
		int ok = 1;

		/* - dupoff = how often a frames within the range shouldn't be made into duplis
		 * - dupon = the length of each "skipping" block in frames
		 */
		if (ob->dupoff) {
			ok = scene->r.cfra - ob->dupsta;
			ok = ok % (ob->dupon + ob->dupoff);
			ok = (ok < ob->dupon);
		}

		if (ok) {
			/* WARNING: doing animation updates in this way is not terribly accurate, as the dependencies
			 * and/or other objects which may affect this object's transforms are not updated either.
			 * However, this has always been the way that this worked (i.e. pre 2.5), so I guess that it'll be fine!
			 */
			BKE_animsys_evaluate_animdata(scene, &ob->id, ob->adt, (float)scene->r.cfra, ADT_RECALC_ANIM); /* ob-eval will do drivers, so we don't need to do them */
			BKE_object_where_is_calc_time(scene, ob, (float)scene->r.cfra);

			make_dupli(ctx, ob, ob->obmat, scene->r.cfra, false, false);
		}
	}

	enable_cu_speed = 1;

	/* reset frame to original frame, then re-evaluate animation as above
	 * as 2.5 animation data may have far-reaching consequences
	 */
	scene->r.cfra = cfrao;

	BKE_animsys_evaluate_animdata(scene, &ob->id, ob->adt, (float)scene->r.cfra, ADT_RECALC_ANIM); /* ob-eval will do drivers, so we don't need to do them */
	BKE_object_where_is_calc_time(scene, ob, (float)scene->r.cfra);

	/* but, to make sure unkeyed object transforms are still sane,
	 * let's copy object's original data back over
	 */
	*ob = copyob;
}

const DupliGenerator gen_dupli_frames = {
    OB_DUPLIFRAMES,                 /* type */
    make_duplis_frames              /* make_duplis */
};

/* OB_DUPLIVERTS */
typedef struct VertexDupliData {
	DerivedMesh *dm;
	BMEditMesh *edit_btmesh;
	int totvert;
	float (*orco)[3];
	bool use_rotation;

	const DupliContext *ctx;
	Object *inst_ob; /* object to instantiate (argument for vertex map callback) */
	float child_imat[4][4];
} VertexDupliData;

static void get_duplivert_transform(const float co[3], const float nor_f[3], const short nor_s[3],
                                    bool use_rotation, short axis, short upflag, float mat[4][4])
{
	float quat[4];
	const float size[3] = {1.0f, 1.0f, 1.0f};

	if (use_rotation) {
		float nor[3];
		/* construct rotation matrix from normals */
		if (nor_f) {
			nor[0] = -nor_f[0];
			nor[1] = -nor_f[1];
			nor[2] = -nor_f[2];
		}
		else if (nor_s) {
			nor[0] = (float)-nor_s[0];
			nor[1] = (float)-nor_s[1];
			nor[2] = (float)-nor_s[2];
		}
		vec_to_quat(quat, nor, axis, upflag);
	}
	else
		unit_qt(quat);

	loc_quat_size_to_mat4(mat, co, quat, size);
}

static void vertex_dupli__mapFunc(void *userData, int index, const float co[3],
                                  const float nor_f[3], const short nor_s[3])
{
	const VertexDupliData *vdd = userData;
	Object *inst_ob = vdd->inst_ob;
	DupliObject *dob;
	float obmat[4][4], space_mat[4][4];

	/* obmat is transform to vertex */
	get_duplivert_transform(co, nor_f, nor_s, vdd->use_rotation, inst_ob->trackflag, inst_ob->upflag, obmat);
	/* make offset relative to inst_ob using relative child transform */
	mul_mat3_m4_v3((float (*)[4])vdd->child_imat, obmat[3]);
	/* apply obmat _after_ the local vertex transform */
	mul_m4_m4m4(obmat, inst_ob->obmat, obmat);

	/* space matrix is constructed by removing obmat transform,
	 * this yields the worldspace transform for recursive duplis
	 */
	mul_m4_m4m4(space_mat, obmat, inst_ob->imat);

	dob = make_dupli(vdd->ctx, vdd->inst_ob, obmat, index, false, false);

	if (vdd->orco)
		copy_v3_v3(dob->orco, vdd->orco[index]);

	/* recursion */
	make_recursive_duplis(vdd->ctx, vdd->inst_ob, space_mat, index, false);
}

static void make_child_duplis_verts(const DupliContext *ctx, void *userdata, Object *child)
{
	VertexDupliData *vdd = userdata;
	DerivedMesh *dm = vdd->dm;

	vdd->inst_ob = child;
	invert_m4_m4(child->imat, child->obmat);
	/* relative transform from parent to child space */
	mul_m4_m4m4(vdd->child_imat, child->imat, ctx->object->obmat);

	if (vdd->edit_btmesh) {
		dm->foreachMappedVert(dm, vertex_dupli__mapFunc, vdd,
		                      vdd->use_rotation ? DM_FOREACH_USE_NORMAL : 0);
	}
	else {
		int a, totvert = vdd->totvert;
		float vec[3], no[3];

		if (vdd->use_rotation) {
			for (a = 0; a < totvert; a++) {
				dm->getVertCo(dm, a, vec);
				dm->getVertNo(dm, a, no);

				vertex_dupli__mapFunc(vdd, a, vec, no, NULL);
			}
		}
		else {
			for (a = 0; a < totvert; a++) {
				dm->getVertCo(dm, a, vec);

				vertex_dupli__mapFunc(vdd, a, vec, NULL, NULL);
			}
		}
	}
}

static void make_duplis_verts(const DupliContext *ctx)
{
	Scene *scene = ctx->scene;
	Object *parent = ctx->object;
	bool use_texcoords = ELEM(ctx->eval_ctx->mode, DAG_EVAL_RENDER, DAG_EVAL_PREVIEW);
	VertexDupliData vdd;

	vdd.ctx = ctx;
	vdd.use_rotation = parent->transflag & OB_DUPLIROT;

	/* gather mesh info */
	{
		Mesh *me = parent->data;
		BMEditMesh *em = BKE_editmesh_from_object(parent);
		CustomDataMask dm_mask = (use_texcoords ? CD_MASK_BAREMESH | CD_MASK_ORCO : CD_MASK_BAREMESH);

		if (em)
			vdd.dm = editbmesh_get_derived_cage(scene, parent, em, dm_mask);
		else
			vdd.dm = mesh_get_derived_final(scene, parent, dm_mask);
		vdd.edit_btmesh = me->edit_btmesh;

		if (use_texcoords)
			vdd.orco = vdd.dm->getVertDataArray(vdd.dm, CD_ORCO);
		else
			vdd.orco = NULL;

		vdd.totvert = vdd.dm->getNumVerts(vdd.dm);
	}

	make_child_duplis(ctx, &vdd, make_child_duplis_verts);

	vdd.dm->release(vdd.dm);
}

const DupliGenerator gen_dupli_verts = {
    OB_DUPLIVERTS,                  /* type */
    make_duplis_verts               /* make_duplis */
};

/* OB_DUPLIVERTS - FONT */
static Object *find_family_object(const char *family, size_t family_len, unsigned int ch, GHash *family_gh)
{
	Object **ob_pt;
	Object *ob;
	void *ch_key = SET_UINT_IN_POINTER(ch);

	if ((ob_pt = (Object **)BLI_ghash_lookup_p(family_gh, ch_key))) {
		ob = *ob_pt;
	}
	else {
		char ch_utf8[7];
		size_t ch_utf8_len;

		ch_utf8_len = BLI_str_utf8_from_unicode(ch, ch_utf8);
		ch_utf8[ch_utf8_len] = '\0';
		ch_utf8_len += 1;  /* compare with null terminator */

		for (ob = G.main->object.first; ob; ob = ob->id.next) {
			if (STREQLEN(ob->id.name + 2 + family_len, ch_utf8, ch_utf8_len)) {
				if (STREQLEN(ob->id.name + 2, family, family_len)) {
					break;
				}
			}
		}

		/* inserted value can be NULL, just to save searches in future */
		BLI_ghash_insert(family_gh, ch_key, ob);
	}

	return ob;
}

static void make_duplis_font(const DupliContext *ctx)
{
	Object *par = ctx->object;
	GHash *family_gh;
	Object *ob;
	Curve *cu;
	struct CharTrans *ct, *chartransdata = NULL;
	float vec[3], obmat[4][4], pmat[4][4], fsize, xof, yof;
	int text_len, a;
	size_t family_len;
	const wchar_t *text = NULL;
	bool text_free = false;

	/* font dupliverts not supported inside groups */
	if (ctx->group)
		return;

	copy_m4_m4(pmat, par->obmat);

	/* in par the family name is stored, use this to find the other objects */

	BKE_vfont_to_curve_ex(G.main, par, FO_DUPLI, NULL,
	                      &text, &text_len, &text_free, &chartransdata);

	if (text == NULL || chartransdata == NULL) {
		return;
	}

	cu = par->data;
	fsize = cu->fsize;
	xof = cu->xof;
	yof = cu->yof;

	ct = chartransdata;

	/* cache result */
	family_len = strlen(cu->family);
	family_gh = BLI_ghash_int_new_ex(__func__, 256);

	/* advance matching BLI_strncpy_wchar_from_utf8 */
	for (a = 0; a < text_len; a++, ct++) {

		ob = find_family_object(cu->family, family_len, (unsigned int)text[a], family_gh);
		if (ob) {
			vec[0] = fsize * (ct->xof - xof);
			vec[1] = fsize * (ct->yof - yof);
			vec[2] = 0.0;

			mul_m4_v3(pmat, vec);

			copy_m4_m4(obmat, par->obmat);

			if (UNLIKELY(ct->rot != 0.0f)) {
				float rmat[4][4];

				zero_v3(obmat[3]);
				unit_m4(rmat);
				rotate_m4(rmat, 'Z', -ct->rot);
				mul_m4_m4m4(obmat, obmat, rmat);
			}

			copy_v3_v3(obmat[3], vec);

			make_dupli(ctx, ob, obmat, a, false, false);
		}
	}

	if (text_free) {
		MEM_freeN((void *)text);
	}

	BLI_ghash_free(family_gh, NULL, NULL);

	MEM_freeN(chartransdata);
}

const DupliGenerator gen_dupli_verts_font = {
    OB_DUPLIVERTS,                  /* type */
    make_duplis_font                /* make_duplis */
};

/* OB_DUPLIFACES */
typedef struct FaceDupliData {
	DerivedMesh *dm;
	int totface;
	MPoly *mpoly;
	MLoop *mloop;
	MVert *mvert;
	float (*orco)[3];
	MLoopUV *mloopuv;
	bool use_scale;
} FaceDupliData;

static void get_dupliface_transform(MPoly *mpoly, MLoop *mloop, MVert *mvert,
                                    bool use_scale, float scale_fac, float mat[4][4])
{
	float loc[3], quat[4], scale, size[3];
	float f_no[3];

	/* location */
	BKE_mesh_calc_poly_center(mpoly, mloop, mvert, loc);
	/* rotation */
	{
		const float *v1, *v2, *v3;
		BKE_mesh_calc_poly_normal(mpoly, mloop, mvert, f_no);
		v1 = mvert[mloop[0].v].co;
		v2 = mvert[mloop[1].v].co;
		v3 = mvert[mloop[2].v].co;
		tri_to_quat_ex(quat, v1, v2, v3, f_no);
	}
	/* scale */
	if (use_scale) {
		float area = BKE_mesh_calc_poly_area(mpoly, mloop, mvert);
		scale = sqrtf(area) * scale_fac;
	}
	else
		scale = 1.0f;
	size[0] = size[1] = size[2] = scale;

	loc_quat_size_to_mat4(mat, loc, quat, size);
}

static void make_child_duplis_faces(const DupliContext *ctx, void *userdata, Object *inst_ob)
{
	FaceDupliData *fdd = userdata;
	MPoly *mpoly = fdd->mpoly, *mp;
	MLoop *mloop = fdd->mloop;
	MVert *mvert = fdd->mvert;
	float (*orco)[3] = fdd->orco;
	MLoopUV *mloopuv = fdd->mloopuv;
	int a, totface = fdd->totface;
	bool use_texcoords = ELEM(ctx->eval_ctx->mode, DAG_EVAL_RENDER, DAG_EVAL_PREVIEW);
	float child_imat[4][4];
	DupliObject *dob;

	invert_m4_m4(inst_ob->imat, inst_ob->obmat);
	/* relative transform from parent to child space */
	mul_m4_m4m4(child_imat, inst_ob->imat, ctx->object->obmat);

	for (a = 0, mp = mpoly; a < totface; a++, mp++) {
		MLoop *loopstart = mloop + mp->loopstart;
		float space_mat[4][4], obmat[4][4];

		if (UNLIKELY(mp->totloop < 3))
			continue;

		/* obmat is transform to face */
		get_dupliface_transform(mp, loopstart, mvert, fdd->use_scale, ctx->object->dupfacesca, obmat);
		/* make offset relative to inst_ob using relative child transform */
		mul_mat3_m4_v3(child_imat, obmat[3]);

		/* XXX ugly hack to ensure same behavior as in master
		 * this should not be needed, parentinv is not consistent
		 * outside of parenting.
		 */
		{
			float imat[3][3];
			copy_m3_m4(imat, inst_ob->parentinv);
			mul_m4_m3m4(obmat, imat, obmat);
		}

		/* apply obmat _after_ the local face transform */
		mul_m4_m4m4(obmat, inst_ob->obmat, obmat);

		/* space matrix is constructed by removing obmat transform,
		 * this yields the worldspace transform for recursive duplis
		 */
		mul_m4_m4m4(space_mat, obmat, inst_ob->imat);

		dob = make_dupli(ctx, inst_ob, obmat, a, false, false);
		if (use_texcoords) {
			float w = 1.0f / (float)mp->totloop;

			if (orco) {
				int j;
				for (j = 0; j < mp->totloop; j++) {
					madd_v3_v3fl(dob->orco, orco[loopstart[j].v], w);
				}
			}

			if (mloopuv) {
				int j;
				for (j = 0; j < mp->totloop; j++) {
					madd_v2_v2fl(dob->uv, mloopuv[mp->loopstart + j].uv, w);
				}
			}
		}

		/* recursion */
		make_recursive_duplis(ctx, inst_ob, space_mat, a, false);
	}
}

static void make_duplis_faces(const DupliContext *ctx)
{
	Scene *scene = ctx->scene;
	Object *parent = ctx->object;
	bool use_texcoords = ELEM(ctx->eval_ctx->mode, DAG_EVAL_RENDER, DAG_EVAL_PREVIEW);
	FaceDupliData fdd;

	fdd.use_scale = ((parent->transflag & OB_DUPLIFACES_SCALE) != 0);

	/* gather mesh info */
	{
		BMEditMesh *em = BKE_editmesh_from_object(parent);
		CustomDataMask dm_mask = (use_texcoords ? CD_MASK_BAREMESH | CD_MASK_ORCO | CD_MASK_MLOOPUV : CD_MASK_BAREMESH);

		if (em)
			fdd.dm = editbmesh_get_derived_cage(scene, parent, em, dm_mask);
		else
			fdd.dm = mesh_get_derived_final(scene, parent, dm_mask);

		if (use_texcoords) {
			fdd.orco = fdd.dm->getVertDataArray(fdd.dm, CD_ORCO);
			fdd.mloopuv = fdd.dm->getLoopDataArray(fdd.dm, CD_MLOOPUV);
		}
		else {
			fdd.orco = NULL;
			fdd.mloopuv = NULL;
		}

		fdd.totface = fdd.dm->getNumPolys(fdd.dm);
		fdd.mpoly = fdd.dm->getPolyArray(fdd.dm);
		fdd.mloop = fdd.dm->getLoopArray(fdd.dm);
		fdd.mvert = fdd.dm->getVertArray(fdd.dm);
	}

	make_child_duplis(ctx, &fdd, make_child_duplis_faces);

	fdd.dm->release(fdd.dm);
}

const DupliGenerator gen_dupli_faces = {
    OB_DUPLIFACES,                  /* type */
    make_duplis_faces               /* make_duplis */
};

/* OB_DUPLIPARTS */
static void make_duplis_particle_system(const DupliContext *ctx, ParticleSystem *psys)
{
	Scene *scene = ctx->scene;
	Object *par = ctx->object;
	bool for_render = ctx->eval_ctx->mode == DAG_EVAL_RENDER;
	bool use_texcoords = ELEM(ctx->eval_ctx->mode, DAG_EVAL_RENDER, DAG_EVAL_PREVIEW);

	GroupObject *go;
	Object *ob = NULL, **oblist = NULL, obcopy, *obcopylist = NULL;
	DupliObject *dob;
	ParticleDupliWeight *dw;
	ParticleSettings *part;
	ParticleData *pa;
	ChildParticle *cpa = NULL;
	ParticleKey state;
	ParticleCacheKey *cache;
	float ctime, pa_time, scale = 1.0f;
	float tmat[4][4], mat[4][4], pamat[4][4], vec[3], size = 0.0;
	float (*obmat)[4];
	int a, b, hair = 0;
	int totpart, totchild, totgroup = 0 /*, pa_num */;
	const bool dupli_type_hack = !BKE_scene_use_new_shading_nodes(scene);

	int no_draw_flag = PARS_UNEXIST;

	if (psys == NULL) return;

	part = psys->part;

	if (part == NULL)
		return;

	if (!psys_check_enabled(par, psys))
		return;

	if (!for_render)
		no_draw_flag |= PARS_NO_DISP;

	ctime = BKE_scene_frame_get(scene); /* NOTE: in old animsys, used parent object's timeoffset... */

	totpart = psys->totpart;
	totchild = psys->totchild;

	BLI_srandom((unsigned int)(31415926 + psys->seed));

	if ((psys->renderdata || part->draw_as == PART_DRAW_REND) && ELEM(part->ren_as, PART_DRAW_OB, PART_DRAW_GR)) {
		ParticleSimulationData sim = {NULL};
		sim.scene = scene;
		sim.ob = par;
		sim.psys = psys;
		sim.psmd = psys_get_modifier(par, psys);
		/* make sure emitter imat is in global coordinates instead of render view coordinates */
		invert_m4_m4(par->imat, par->obmat);

		/* first check for loops (particle system object used as dupli object) */
		if (part->ren_as == PART_DRAW_OB) {
			if (ELEM(part->dup_ob, NULL, par))
				return;
		}
		else { /*PART_DRAW_GR */
			if (part->dup_group == NULL || BLI_listbase_is_empty(&part->dup_group->gobject))
				return;

			if (BLI_findptr(&part->dup_group->gobject, par, offsetof(GroupObject, ob))) {
				return;
			}
		}

		/* if we have a hair particle system, use the path cache */
		if (part->type == PART_HAIR) {
			if (psys->flag & PSYS_HAIR_DONE)
				hair = (totchild == 0 || psys->childcache) && psys->pathcache;
			if (!hair)
				return;

			/* we use cache, update totchild according to cached data */
			totchild = psys->totchildcache;
			totpart = psys->totcached;
		}

		psys_check_group_weights(part);

		psys->lattice_deform_data = psys_create_lattice_deform_data(&sim);

		/* gather list of objects or single object */
		if (part->ren_as == PART_DRAW_GR) {
			if (ctx->do_update) {
				BKE_group_handle_recalc_and_update(ctx->eval_ctx, scene, par, part->dup_group);
			}

			if (part->draw & PART_DRAW_COUNT_GR) {
				for (dw = part->dupliweights.first; dw; dw = dw->next)
					totgroup += dw->count;
			}
			else {
				for (go = part->dup_group->gobject.first; go; go = go->next)
					totgroup++;
			}

			/* we also copy the actual objects to restore afterwards, since
			 * BKE_object_where_is_calc_time will change the object which breaks transform */
			oblist = MEM_callocN((size_t)totgroup * sizeof(Object *), "dupgroup object list");
			obcopylist = MEM_callocN((size_t)totgroup * sizeof(Object), "dupgroup copy list");

			if (part->draw & PART_DRAW_COUNT_GR && totgroup) {
				dw = part->dupliweights.first;

				for (a = 0; a < totgroup; dw = dw->next) {
					for (b = 0; b < dw->count; b++, a++) {
						oblist[a] = dw->ob;
						obcopylist[a] = *dw->ob;
					}
				}
			}
			else {
				go = part->dup_group->gobject.first;
				for (a = 0; a < totgroup; a++, go = go->next) {
					oblist[a] = go->ob;
					obcopylist[a] = *go->ob;
				}
			}
		}
		else {
			ob = part->dup_ob;
			obcopy = *ob;
		}

		if (totchild == 0 || part->draw & PART_DRAW_PARENT)
			a = 0;
		else
			a = totpart;

		for (pa = psys->particles; a < totpart + totchild; a++, pa++) {
			if (a < totpart) {
				/* handle parent particle */
				if (pa->flag & no_draw_flag)
					continue;

				/* pa_num = pa->num; */ /* UNUSED */
				pa_time = pa->time;
				size = pa->size;
			}
			else {
				/* handle child particle */
				cpa = &psys->child[a - totpart];

				/* pa_num = a; */ /* UNUSED */
				pa_time = psys->particles[cpa->parent].time;
				size = psys_get_child_size(psys, cpa, ctime, NULL);
			}

			/* some hair paths might be non-existent so they can't be used for duplication */
			if (hair && psys->pathcache &&
			    ((a < totpart && psys->pathcache[a]->segments < 0) ||
			     (a >= totpart && psys->childcache[a - totpart]->segments < 0)))
			{
				continue;
			}

			if (part->ren_as == PART_DRAW_GR) {
				/* prevent divide by zero below [#28336] */
				if (totgroup == 0)
					continue;

				/* for groups, pick the object based on settings */
				if (part->draw & PART_DRAW_RAND_GR)
					b = BLI_rand() % totgroup;
				else
					b = a % totgroup;

				ob = oblist[b];
				obmat = oblist[b]->obmat;
			}
			else {
				obmat = ob->obmat;
			}

			if (hair) {
				/* hair we handle separate and compute transform based on hair keys */
				if (a < totpart) {
					cache = psys->pathcache[a];
					psys_get_dupli_path_transform(&sim, pa, NULL, cache, pamat, &scale);
				}
				else {
					cache = psys->childcache[a - totpart];
					psys_get_dupli_path_transform(&sim, NULL, cpa, cache, pamat, &scale);
				}

				copy_v3_v3(pamat[3], cache->co);
				pamat[3][3] = 1.0f;

			}
			else {
				/* first key */
				state.time = ctime;
				if (psys_get_particle_state(&sim, a, &state, 0) == 0) {
					continue;
				}
				else {
					float tquat[4];
					normalize_qt_qt(tquat, state.rot);
					quat_to_mat4(pamat, tquat);
					copy_v3_v3(pamat[3], state.co);
					pamat[3][3] = 1.0f;
				}
			}

			if (part->ren_as == PART_DRAW_GR && psys->part->draw & PART_DRAW_WHOLE_GR) {
				for (go = part->dup_group->gobject.first, b = 0; go; go = go->next, b++) {

					copy_m4_m4(tmat, oblist[b]->obmat);
					/* apply particle scale */
					mul_mat3_m4_fl(tmat, size * scale);
					mul_v3_fl(tmat[3], size * scale);
					/* group dupli offset, should apply after everything else */
					if (!is_zero_v3(part->dup_group->dupli_ofs))
						sub_v3_v3(tmat[3], part->dup_group->dupli_ofs);
					/* individual particle transform */
					mul_m4_m4m4(mat, pamat, tmat);

					dob = make_dupli(ctx, go->ob, mat, a, false, false);
					dob->particle_system = psys;
					if (use_texcoords)
						psys_get_dupli_texture(psys, part, sim.psmd, pa, cpa, dob->uv, dob->orco);
				}
			}
			else {
				/* to give ipos in object correct offset */
				BKE_object_where_is_calc_time(scene, ob, ctime - pa_time);

				copy_v3_v3(vec, obmat[3]);
				obmat[3][0] = obmat[3][1] = obmat[3][2] = 0.0f;

				/* particle rotation uses x-axis as the aligned axis, so pre-rotate the object accordingly */
				if ((part->draw & PART_DRAW_ROTATE_OB) == 0) {
					float xvec[3], q[4], size_mat[4][4], original_size[3];

					mat4_to_size(original_size, obmat);
					size_to_mat4(size_mat, original_size);

					xvec[0] = -1.f;
					xvec[1] = xvec[2] = 0;
					vec_to_quat(q, xvec, ob->trackflag, ob->upflag);
					quat_to_mat4(obmat, q);
					obmat[3][3] = 1.0f;

					/* add scaling if requested */
					if ((part->draw & PART_DRAW_NO_SCALE_OB) == 0)
						mul_m4_m4m4(obmat, obmat, size_mat);
				}
				else if (part->draw & PART_DRAW_NO_SCALE_OB) {
					/* remove scaling */
					float size_mat[4][4], original_size[3];

					mat4_to_size(original_size, obmat);
					size_to_mat4(size_mat, original_size);
					invert_m4(size_mat);

					mul_m4_m4m4(obmat, obmat, size_mat);
				}

				mul_m4_m4m4(tmat, pamat, obmat);
				mul_mat3_m4_fl(tmat, size * scale);

				copy_m4_m4(mat, tmat);

				if (part->draw & PART_DRAW_GLOBAL_OB)
					add_v3_v3v3(mat[3], mat[3], vec);

				dob = make_dupli(ctx, ob, mat, a, false, false);
				dob->particle_system = psys;
				if (use_texcoords)
					psys_get_dupli_texture(psys, part, sim.psmd, pa, cpa, dob->uv, dob->orco);
				/* XXX blender internal needs this to be set to dupligroup to render
				 * groups correctly, but we don't want this hack for cycles */
				if (dupli_type_hack && ctx->group)
					dob->type = OB_DUPLIGROUP;
			}
		}

		/* restore objects since they were changed in BKE_object_where_is_calc_time */
		if (part->ren_as == PART_DRAW_GR) {
			for (a = 0; a < totgroup; a++)
				*(oblist[a]) = obcopylist[a];
		}
		else
			*ob = obcopy;
	}

	/* clean up */
	if (oblist)
		MEM_freeN(oblist);
	if (obcopylist)
		MEM_freeN(obcopylist);

	if (psys->lattice_deform_data) {
		end_latt_deform(psys->lattice_deform_data);
		psys->lattice_deform_data = NULL;
	}
}

static void make_duplis_particles(const DupliContext *ctx)
{
	ParticleSystem *psys;
	int psysid;

	/* particle system take up one level in id, the particles another */
	for (psys = ctx->object->particlesystem.first, psysid = 0; psys; psys = psys->next, psysid++) {
		/* particles create one more level for persistent psys index */
		DupliContext pctx;
		copy_dupli_context(&pctx, ctx, ctx->object, NULL, psysid, false);
		make_duplis_particle_system(&pctx, psys);
	}
}

const DupliGenerator gen_dupli_particles = {
    OB_DUPLIPARTS,                  /* type */
    make_duplis_particles           /* make_duplis */
};

/* ------------- */

/* select dupli generator from given context */
static const DupliGenerator *get_dupli_generator(const DupliContext *ctx)
{
	int transflag = ctx->object->transflag;
	int restrictflag = ctx->object->restrictflag;

	if ((transflag & OB_DUPLI) == 0)
		return NULL;

	/* Should the dupli's be generated for this object? - Respect restrict flags */
	if (ctx->eval_ctx->mode == DAG_EVAL_RENDER ? (restrictflag & OB_RESTRICT_RENDER) : (restrictflag & OB_RESTRICT_VIEW))
		return NULL;

	if (transflag & OB_DUPLIPARTS) {
		return &gen_dupli_particles;
	}
	else if (transflag & OB_DUPLIVERTS) {
		if (ctx->object->type == OB_MESH) {
			return &gen_dupli_verts;
		}
		else if (ctx->object->type == OB_FONT) {
			return &gen_dupli_verts_font;
		}
	}
	else if (transflag & OB_DUPLIFACES) {
		if (ctx->object->type == OB_MESH)
			return &gen_dupli_faces;
	}
	else if (transflag & OB_DUPLIFRAMES) {
		return &gen_dupli_frames;
	}
	else if (transflag & OB_DUPLIGROUP) {
		return &gen_dupli_group;
	}

	return NULL;
}


/* ---- ListBase dupli container implementation ---- */

/* Returns a list of DupliObject */
ListBase *object_duplilist_ex(EvaluationContext *eval_ctx, Scene *scene, Object *ob, bool update)
{
	ListBase *duplilist = MEM_callocN(sizeof(ListBase), "duplilist");
	DupliContext ctx;
	init_context(&ctx, eval_ctx, scene, ob, NULL, update);
	if (ctx.gen) {
		ctx.duplilist = duplilist;
		ctx.gen->make_duplis(&ctx);
	}

	return duplilist;
}

/* note: previously updating was always done, this is why it defaults to be on
 * but there are likely places it can be called without updating */
ListBase *object_duplilist(EvaluationContext *eval_ctx, Scene *sce, Object *ob)
{
	return object_duplilist_ex(eval_ctx, sce, ob, true);
}

void free_object_duplilist(ListBase *lb)
{
	BLI_freelistN(lb);
	MEM_freeN(lb);
}

int count_duplilist(Object *ob)
{
	if (ob->transflag & OB_DUPLI) {
		if (ob->transflag & OB_DUPLIVERTS) {
			if (ob->type == OB_MESH) {
				if (ob->transflag & OB_DUPLIVERTS) {
					ParticleSystem *psys = ob->particlesystem.first;
					int pdup = 0;

					for (; psys; psys = psys->next)
						pdup += psys->totpart;

					if (pdup == 0) {
						Mesh *me = ob->data;
						return me->totvert;
					}
					else
						return pdup;
				}
			}
		}
		else if (ob->transflag & OB_DUPLIFRAMES) {
			int tot = ob->dupend - ob->dupsta;
			tot /= (ob->dupon + ob->dupoff);
			return tot * ob->dupon;
		}
	}
	return 1;
}

DupliApplyData *duplilist_apply(Object *ob, ListBase *duplilist)
{
	DupliApplyData *apply_data = NULL;
	int num_objects = BLI_listbase_count(duplilist);
	
	if (num_objects > 0) {
		DupliObject *dob;
		int i;
		apply_data = MEM_mallocN(sizeof(DupliApplyData), "DupliObject apply data");
		apply_data->num_objects = num_objects;
		apply_data->extra = MEM_mallocN(sizeof(DupliExtraData) * (size_t) num_objects,
		                                "DupliObject apply extra data");

		for (dob = duplilist->first, i = 0; dob; dob = dob->next, ++i) {
			/* copy obmat from duplis */
			copy_m4_m4(apply_data->extra[i].obmat, dob->ob->obmat);
			copy_m4_m4(dob->ob->obmat, dob->mat);
			
			/* copy layers from the main duplicator object */
			apply_data->extra[i].lay = dob->ob->lay;
			dob->ob->lay = ob->lay;
		}
	}
	return apply_data;
}

void duplilist_restore(ListBase *duplilist, DupliApplyData *apply_data)
{
	DupliObject *dob;
	int i;
	/* Restore object matrices.
	 * NOTE: this has to happen in reverse order, since nested
	 * dupli objects can repeatedly override the obmat.
	 */
	for (dob = duplilist->last, i = apply_data->num_objects - 1; dob; dob = dob->prev, --i) {
		copy_m4_m4(dob->ob->obmat, apply_data->extra[i].obmat);
		
		dob->ob->lay = apply_data->extra[i].lay;
	}
}

void duplilist_free_apply_data(DupliApplyData *apply_data)
{
	MEM_freeN(apply_data->extra);
	MEM_freeN(apply_data);
}
