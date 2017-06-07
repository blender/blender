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
 * The Original Code is Copyright (C) 20014 by Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Sergey Sharybin.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/blenkernel/intern/object_update.c
 *  \ingroup bke
 */

#include "DNA_anim_types.h"
#include "DNA_constraint_types.h"
#include "DNA_group_types.h"
#include "DNA_key_types.h"
#include "DNA_material_types.h"
#include "DNA_scene_types.h"

#include "BLI_blenlib.h"
#include "BLI_utildefines.h"
#include "BLI_math.h"
#include "BLI_threads.h"

#include "BKE_global.h"
#include "BKE_armature.h"
#include "BKE_action.h"
#include "BKE_constraint.h"
#include "BKE_depsgraph.h"
#include "BKE_DerivedMesh.h"
#include "BKE_animsys.h"
#include "BKE_displist.h"
#include "BKE_effect.h"
#include "BKE_key.h"
#include "BKE_lamp.h"
#include "BKE_lattice.h"
#include "BKE_editmesh.h"
#include "BKE_object.h"
#include "BKE_particle.h"
#include "BKE_pointcache.h"
#include "BKE_scene.h"
#include "BKE_material.h"
#include "BKE_image.h"

#include "DEG_depsgraph.h"

#ifdef WITH_LEGACY_DEPSGRAPH
#  define DEBUG_PRINT if (!DEG_depsgraph_use_legacy() && G.debug & G_DEBUG_DEPSGRAPH) printf
#else
#  define DEBUG_PRINT if (G.debug & G_DEBUG_DEPSGRAPH) printf
#endif

static ThreadMutex material_lock = BLI_MUTEX_INITIALIZER;

void BKE_object_eval_local_transform(EvaluationContext *UNUSED(eval_ctx),
                                     Scene *UNUSED(scene),
                                     Object *ob)
{
	DEBUG_PRINT("%s on %s\n", __func__, ob->id.name);

	/* calculate local matrix */
	BKE_object_to_mat4(ob, ob->obmat);
}

/* Evaluate parent */
/* NOTE: based on solve_parenting(), but with the cruft stripped out */
void BKE_object_eval_parent(EvaluationContext *UNUSED(eval_ctx),
                            Scene *scene,
                            Object *ob)
{
	Object *par = ob->parent;

	float totmat[4][4];
	float tmat[4][4];
	float locmat[4][4];

	DEBUG_PRINT("%s on %s\n", __func__, ob->id.name);

	/* get local matrix (but don't calculate it, as that was done already!) */
	// XXX: redundant?
	copy_m4_m4(locmat, ob->obmat);

	/* get parent effect matrix */
	BKE_object_get_parent_matrix(scene, ob, par, totmat);

	/* total */
	mul_m4_m4m4(tmat, totmat, ob->parentinv);
	mul_m4_m4m4(ob->obmat, tmat, locmat);

	/* origin, for help line */
	if ((ob->partype & PARTYPE) == PARSKEL) {
		copy_v3_v3(ob->orig, par->obmat[3]);
	}
	else {
		copy_v3_v3(ob->orig, totmat[3]);
	}
}

void BKE_object_eval_constraints(EvaluationContext *UNUSED(eval_ctx),
                                 Scene *scene,
                                 Object *ob)
{
	bConstraintOb *cob;
	float ctime = BKE_scene_frame_get(scene);

	DEBUG_PRINT("%s on %s\n", __func__, ob->id.name);

	/* evaluate constraints stack */
	/* TODO: split this into:
	 * - pre (i.e. BKE_constraints_make_evalob, per-constraint (i.e.
	 * - inner body of BKE_constraints_solve),
	 * - post (i.e. BKE_constraints_clear_evalob)
	 *
	 * Not sure why, this is from Joshua - sergey
	 *
	 */
	cob = BKE_constraints_make_evalob(scene, ob, NULL, CONSTRAINT_OBTYPE_OBJECT);
	BKE_constraints_solve(&ob->constraints, cob, ctime);
	BKE_constraints_clear_evalob(cob);
}

void BKE_object_eval_done(EvaluationContext *UNUSED(eval_ctx), Object *ob)
{
	DEBUG_PRINT("%s on %s\n", __func__, ob->id.name);

	/* Set negative scale flag in object. */
	if (is_negative_m4(ob->obmat)) ob->transflag |= OB_NEG_SCALE;
	else ob->transflag &= ~OB_NEG_SCALE;
}

void BKE_object_handle_data_update(EvaluationContext *eval_ctx,
                                   Scene *scene,
                                   Object *ob)
{
	ID *data_id = (ID *)ob->data;
	AnimData *adt = BKE_animdata_from_id(data_id);
	Key *key;
	float ctime = BKE_scene_frame_get(scene);

	if (G.debug & G_DEBUG_DEPSGRAPH)
		printf("recalcdata %s\n", ob->id.name + 2);

	/* TODO(sergey): Only used by legacy depsgraph. */
	if (adt) {
		/* evaluate drivers - datalevel */
		/* XXX: for mesh types, should we push this to derivedmesh instead? */
		BKE_animsys_evaluate_animdata(scene, data_id, adt, ctime, ADT_RECALC_DRIVERS);
	}

	/* TODO(sergey): Only used by legacy depsgraph. */
	key = BKE_key_from_object(ob);
	if (key && key->block.first) {
		if (!(ob->shapeflag & OB_SHAPE_LOCK))
			BKE_animsys_evaluate_animdata(scene, &key->id, key->adt, ctime, ADT_RECALC_DRIVERS);
	}

	/* includes all keys and modifiers */
	switch (ob->type) {
		case OB_MESH:
		{
			BMEditMesh *em = (ob == scene->obedit) ? BKE_editmesh_from_object(ob) : NULL;
			uint64_t data_mask = scene->customdata_mask | CD_MASK_BAREMESH;
#ifdef WITH_FREESTYLE
			/* make sure Freestyle edge/face marks appear in DM for render (see T40315) */
			if (eval_ctx->mode != DAG_EVAL_VIEWPORT) {
				data_mask |= CD_MASK_FREESTYLE_EDGE | CD_MASK_FREESTYLE_FACE;
			}
#endif
			if (em) {
				makeDerivedMesh(scene, ob, em,  data_mask, false); /* was CD_MASK_BAREMESH */
			}
			else {
				makeDerivedMesh(scene, ob, NULL, data_mask, false);
			}
			break;
		}
		case OB_ARMATURE:
			if (ID_IS_LINKED_DATABLOCK(ob) && ob->proxy_from) {
				if (BKE_pose_copy_result(ob->pose, ob->proxy_from->pose) == false) {
					printf("Proxy copy error, lib Object: %s proxy Object: %s\n",
					       ob->id.name + 2, ob->proxy_from->id.name + 2);
				}
			}
			else {
				BKE_pose_where_is(scene, ob);
			}
			break;

		case OB_MBALL:
			BKE_displist_make_mball(eval_ctx, scene, ob);
			break;

		case OB_CURVE:
		case OB_SURF:
		case OB_FONT:
			BKE_displist_make_curveTypes(scene, ob, 0);
			break;

		case OB_LATTICE:
			BKE_lattice_modifiers_calc(scene, ob);
			break;

		case OB_EMPTY:
			if (ob->empty_drawtype == OB_EMPTY_IMAGE && ob->data)
				if (BKE_image_is_animated(ob->data))
					BKE_image_user_check_frame_calc(ob->iuser, (int)ctime, 0);
			break;
	}

	/* related materials */
	/* XXX: without depsgraph tagging, this will always need to be run, which will be slow!
	 * However, not doing anything (or trying to hack around this lack) is not an option
	 * anymore, especially due to Cycles [#31834]
	 */
	if (ob->totcol) {
		int a;
		if (ob->totcol != 0) {
			BLI_mutex_lock(&material_lock);
			for (a = 1; a <= ob->totcol; a++) {
				Material *ma = give_current_material(ob, a);
				if (ma) {
					/* recursively update drivers for this material */
					material_drivers_update(scene, ma, ctime);
				}
			}
			BLI_mutex_unlock(&material_lock);
		}
	}
	else if (ob->type == OB_LAMP)
		lamp_drivers_update(scene, ob->data, ctime);

	/* particles */
	if (ob != scene->obedit && ob->particlesystem.first) {
		ParticleSystem *tpsys, *psys;
		DerivedMesh *dm;
		ob->transflag &= ~OB_DUPLIPARTS;
		psys = ob->particlesystem.first;
		while (psys) {
			/* ensure this update always happens even if psys is disabled */
			if (psys->recalc & PSYS_RECALC_TYPE) {
				psys_changed_type(ob, psys);
			}

			if (psys_check_enabled(ob, psys, eval_ctx->mode == DAG_EVAL_RENDER)) {
				/* check use of dupli objects here */
				if (psys->part && (psys->part->draw_as == PART_DRAW_REND || eval_ctx->mode == DAG_EVAL_RENDER) &&
				    ((psys->part->ren_as == PART_DRAW_OB && psys->part->dup_ob) ||
				     (psys->part->ren_as == PART_DRAW_GR && psys->part->dup_group)))
				{
					ob->transflag |= OB_DUPLIPARTS;
				}

				particle_system_update(scene, ob, psys, (eval_ctx->mode == DAG_EVAL_RENDER));
				psys = psys->next;
			}
			else if (psys->flag & PSYS_DELETE) {
				tpsys = psys->next;
				BLI_remlink(&ob->particlesystem, psys);
				psys_free(ob, psys);
				psys = tpsys;
			}
			else
				psys = psys->next;
		}

		if (eval_ctx->mode == DAG_EVAL_RENDER && ob->transflag & OB_DUPLIPARTS) {
			/* this is to make sure we get render level duplis in groups:
			 * the derivedmesh must be created before init_render_mesh,
			 * since object_duplilist does dupliparticles before that */
			CustomDataMask data_mask = CD_MASK_BAREMESH | CD_MASK_MFACE | CD_MASK_MTFACE | CD_MASK_MCOL;
			dm = mesh_create_derived_render(scene, ob, data_mask);
			dm->release(dm);

			for (psys = ob->particlesystem.first; psys; psys = psys->next)
				psys_get_modifier(ob, psys)->flag &= ~eParticleSystemFlag_psys_updated;
		}
	}

	/* quick cache removed */
}

void BKE_object_eval_uber_transform(EvaluationContext *UNUSED(eval_ctx),
                                    Scene *UNUSED(scene),
                                    Object *ob)
{
	/* TODO(sergey): Currently it's a duplicate of logic in BKE_object_handle_update_ex(). */
	// XXX: it's almost redundant now...

	/* Handle proxy copy for target, */
	if (ID_IS_LINKED_DATABLOCK(ob) && ob->proxy_from) {
		if (ob->proxy_from->proxy_group) {
			/* Transform proxy into group space. */
			Object *obg = ob->proxy_from->proxy_group;
			float imat[4][4];
			invert_m4_m4(imat, obg->obmat);
			mul_m4_m4m4(ob->obmat, imat, ob->proxy_from->obmat);
			/* Should always be true. */
			if (obg->dup_group) {
				add_v3_v3(ob->obmat[3], obg->dup_group->dupli_ofs);
			}
		}
		else
			copy_m4_m4(ob->obmat, ob->proxy_from->obmat);
	}

	ob->recalc &= ~(OB_RECALC_OB | OB_RECALC_TIME);
	if (ob->data == NULL) {
		ob->recalc &= ~OB_RECALC_DATA;
	}
}

void BKE_object_eval_uber_data(EvaluationContext *eval_ctx,
                               Scene *scene,
                               Object *ob)
{
	DEBUG_PRINT("%s on %s\n", __func__, ob->id.name);
	BLI_assert(ob->type != OB_ARMATURE);
	BKE_object_handle_data_update(eval_ctx, scene, ob);

	ob->recalc &= ~(OB_RECALC_DATA | OB_RECALC_TIME);
}

void BKE_object_eval_cloth(EvaluationContext *UNUSED(eval_ctx), Scene *scene, Object *object)
{
	DEBUG_PRINT("%s on %s\n", __func__, object->id.name);
	BKE_ptcache_object_reset(scene, object, PTCACHE_RESET_DEPSGRAPH);
}
