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
#include "DNA_mesh_types.h"
#include "DNA_scene_types.h"

#include "BLI_blenlib.h"
#include "BLI_utildefines.h"
#include "BLI_math.h"

#include "BKE_global.h"
#include "BKE_armature.h"
#include "BKE_action.h"
#include "BKE_constraint.h"
#include "BKE_curve.h"
#include "BKE_DerivedMesh.h"
#include "BKE_animsys.h"
#include "BKE_displist.h"
#include "BKE_effect.h"
#include "BKE_key.h"
#include "BKE_lamp.h"
#include "BKE_lattice.h"
#include "BKE_library.h"
#include "BKE_editmesh.h"
#include "BKE_object.h"
#include "BKE_particle.h"
#include "BKE_pointcache.h"
#include "BKE_scene.h"
#include "BKE_material.h"
#include "BKE_mball.h"
#include "BKE_mesh.h"
#include "BKE_image.h"

#include "MEM_guardedalloc.h"
#include "DEG_depsgraph.h"


void BKE_object_eval_local_transform(const EvaluationContext *UNUSED(eval_ctx),
                                     Object *ob)
{
	DEG_debug_print_eval(__func__, ob->id.name, ob);

	/* calculate local matrix */
	BKE_object_to_mat4(ob, ob->obmat);
}

/* Evaluate parent */
/* NOTE: based on solve_parenting(), but with the cruft stripped out */
void BKE_object_eval_parent(const EvaluationContext *UNUSED(eval_ctx),
                            Scene *scene,
                            Object *ob)
{
	Object *par = ob->parent;

	float totmat[4][4];
	float tmat[4][4];
	float locmat[4][4];

	DEG_debug_print_eval(__func__, ob->id.name, ob);

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

void BKE_object_eval_constraints(const EvaluationContext *eval_ctx,
                                 Scene *scene,
                                 Object *ob)
{
	bConstraintOb *cob;
	float ctime = BKE_scene_frame_get(scene);

	DEG_debug_print_eval(__func__, ob->id.name, ob);

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
	BKE_constraints_solve(eval_ctx, &ob->constraints, cob, ctime);
	BKE_constraints_clear_evalob(cob);
}

void BKE_object_eval_done(const EvaluationContext *UNUSED(eval_ctx), Object *ob)
{
	DEG_debug_print_eval(__func__, ob->id.name, ob);

	/* Set negative scale flag in object. */
	if (is_negative_m4(ob->obmat)) ob->transflag |= OB_NEG_SCALE;
	else ob->transflag &= ~OB_NEG_SCALE;
}

void BKE_object_handle_data_update(
        const EvaluationContext *eval_ctx,
        Scene *scene,
        Object *ob)
{
	ID *data_id = (ID *)ob->data;
	AnimData *adt = BKE_animdata_from_id(data_id);
	Key *key;
	float ctime = BKE_scene_frame_get(scene);

	if (G.debug & G_DEBUG_DEPSGRAPH_EVAL)
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
			BMEditMesh *em = (eval_ctx->object_mode & OB_MODE_EDIT) ? BKE_editmesh_from_object(ob) : NULL;
			uint64_t data_mask = scene->customdata_mask | CD_MASK_BAREMESH;
#ifdef WITH_FREESTYLE
			/* make sure Freestyle edge/face marks appear in DM for render (see T40315) */
			if (eval_ctx->mode != DAG_EVAL_VIEWPORT) {
				data_mask |= CD_MASK_FREESTYLE_EDGE | CD_MASK_FREESTYLE_FACE;
			}
#endif
			if (em) {
				makeDerivedMesh(eval_ctx, scene, ob, em,  data_mask, false); /* was CD_MASK_BAREMESH */
			}
			else {
				makeDerivedMesh(eval_ctx, scene, ob, NULL, data_mask, false);
			}
			break;
		}
		case OB_ARMATURE:
			if (ID_IS_LINKED(ob) && ob->proxy_from) {
				if (BKE_pose_copy_result(ob->pose, ob->proxy_from->pose) == false) {
					printf("Proxy copy error, lib Object: %s proxy Object: %s\n",
					       ob->id.name + 2, ob->proxy_from->id.name + 2);
				}
			}
			else {
				BKE_pose_where_is(eval_ctx, scene, ob);
			}
			break;

		case OB_MBALL:
			BKE_displist_make_mball(eval_ctx, scene, ob);
			break;

		case OB_CURVE:
		case OB_SURF:
		case OB_FONT:
			BKE_displist_make_curveTypes(eval_ctx, scene, ob, 0);
			break;

		case OB_LATTICE:
			BKE_lattice_modifiers_calc(eval_ctx, scene, ob);
			break;

		case OB_EMPTY:
			if (ob->empty_drawtype == OB_EMPTY_IMAGE && ob->data)
				if (BKE_image_is_animated(ob->data))
					BKE_image_user_check_frame_calc(ob->iuser, (int)ctime, 0);
			break;
	}

	/* particles */
	if ((ob != OBEDIT_FROM_EVAL_CTX(eval_ctx)) && ob->particlesystem.first) {
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

				particle_system_update(eval_ctx, scene, ob, psys, (eval_ctx->mode == DAG_EVAL_RENDER));
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
			dm = mesh_create_derived_render(eval_ctx, scene, ob, data_mask);
			dm->release(dm);

			for (psys = ob->particlesystem.first; psys; psys = psys->next)
				psys_get_modifier(ob, psys)->flag &= ~eParticleSystemFlag_psys_updated;
		}
	}

	/* quick cache removed */
}

bool BKE_object_eval_proxy_copy(const EvaluationContext *UNUSED(eval_ctx),
                                Object *object)
{
	/* Handle proxy copy for target, */
	if (ID_IS_LINKED(object) && object->proxy_from) {
		if (object->proxy_from->proxy_group) {
			/* Transform proxy into group space. */
			Object *obg = object->proxy_from->proxy_group;
			float imat[4][4];
			invert_m4_m4(imat, obg->obmat);
			mul_m4_m4m4(object->obmat, imat, object->proxy_from->obmat);
			/* Should always be true. */
			if (obg->dup_group) {
				add_v3_v3(object->obmat[3], obg->dup_group->dupli_ofs);
			}
		}
		else {
			copy_m4_m4(object->obmat, object->proxy_from->obmat);
		}
		return true;
	}
	return false;
}

void BKE_object_eval_uber_transform(const EvaluationContext *eval_ctx, Object *object)
{
	BKE_object_eval_proxy_copy(eval_ctx, object);
}

void BKE_object_eval_uber_data(const EvaluationContext *eval_ctx,
                               Scene *scene,
                               Object *ob)
{
	DEG_debug_print_eval(__func__, ob->id.name, ob);
	BLI_assert(ob->type != OB_ARMATURE);
	BKE_object_handle_data_update(eval_ctx, scene, ob);

	switch (ob->type) {
		case OB_MESH:
			BKE_mesh_batch_cache_dirty(ob->data, BKE_MESH_BATCH_DIRTY_ALL);
			break;
		case OB_LATTICE:
			BKE_lattice_batch_cache_dirty(ob->data, BKE_LATTICE_BATCH_DIRTY_ALL);
			break;
		case OB_CURVE:
		case OB_FONT:
		case OB_SURF:
			BKE_curve_batch_cache_dirty(ob->data, BKE_CURVE_BATCH_DIRTY_ALL);
			break;
		case OB_MBALL:
			BKE_mball_batch_cache_dirty(ob->data, BKE_MBALL_BATCH_DIRTY_ALL);
			break;
	}

	if (DEG_depsgraph_use_copy_on_write()) {
		if (ob->type == OB_MESH) {
			/* Quick hack to convert evaluated derivedMesh to Mesh. */
			DerivedMesh *dm = ob->derivedFinal;
			if (dm != NULL) {
				Mesh *mesh = (Mesh *)ob->data;
				Mesh *new_mesh = BKE_libblock_alloc_notest(ID_ME);
				BKE_mesh_init(new_mesh);
				/* Copy ID name so GS(new_mesh->id) works correct later on. */
				BLI_strncpy(new_mesh->id.name, mesh->id.name, sizeof(new_mesh->id.name));
				/* Copy materials so render engines can access them. */
				new_mesh->mat = MEM_dupallocN(mesh->mat);
				new_mesh->totcol = mesh->totcol;
				DM_to_mesh(dm, new_mesh, ob, CD_MASK_MESH, true);
				new_mesh->edit_btmesh = mesh->edit_btmesh;
				/* Store result mesh as derived_mesh of object. This way we have
				 * explicit  way to query final object evaluated data and know for sure
				 * who owns the newly created mesh datablock.
				 */
				ob->mesh_evaluated = new_mesh;
				/* TODO(sergey): This is kind of compatibility thing, so all render
				 * engines can use object->data for mesh data for display. This is
				 * something what we might want to change in the future.
				 */
				ob->data = new_mesh;
				/* Special flags to help debugging. */
				new_mesh->id.tag |= LIB_TAG_COPY_ON_WRITE_EVAL;
				/* Save some memory by throwing DerivedMesh away. */
				/* NOTE: Watch out, some tools might need it!
				 * So keep around for now..
				 */
				/* Store original ID as a pointer in evaluated ID.
				 * This way we can restore original object data when we are freeing
				 * evaluated mesh.
				 */
				new_mesh->id.orig_id = &mesh->id;
			}
#if 0
			if (ob->derivedFinal != NULL) {
				ob->derivedFinal->needsFree = 1;
				ob->derivedFinal->release(ob->derivedFinal);
				ob->derivedFinal = NULL;
			}
			if (ob->derivedDeform != NULL) {
				ob->derivedDeform->needsFree = 1;
				ob->derivedDeform->release(ob->derivedDeform);
				ob->derivedDeform = NULL;
			}
#endif
		}
	}
}

void BKE_object_eval_cloth(const EvaluationContext *UNUSED(eval_ctx),
                           Scene *scene,
                           Object *object)
{
	DEG_debug_print_eval(__func__, object->id.name, object);
	BKE_ptcache_object_reset(scene, object, PTCACHE_RESET_DEPSGRAPH);
}

void BKE_object_eval_transform_all(const EvaluationContext *eval_ctx,
                                   Scene *scene,
                                   Object *object)
{
	/* This mimics full transform update chain from new depsgraph. */
	BKE_object_eval_local_transform(eval_ctx, object);
	if (object->parent != NULL) {
		BKE_object_eval_parent(eval_ctx, scene, object);
	}
	if (!BLI_listbase_is_empty(&object->constraints)) {
		BKE_object_eval_constraints(eval_ctx, scene, object);
	}
	BKE_object_eval_uber_transform(eval_ctx, object);
	BKE_object_eval_done(eval_ctx, object);
}

void BKE_object_eval_update_shading(const EvaluationContext *UNUSED(eval_ctx),
                                    Object *object)
{
	DEG_debug_print_eval(__func__, object->id.name, object);
	if (object->type == OB_MESH) {
		BKE_mesh_batch_cache_dirty(object->data, BKE_MESH_BATCH_DIRTY_SHADING);
	}
}

void BKE_object_data_select_update(const EvaluationContext *UNUSED(eval_ctx),
                                   struct ID *object_data)
{
	DEG_debug_print_eval(__func__, object_data->name, object_data);
	switch (GS(object_data->name)) {
		case ID_ME:
			BKE_mesh_batch_cache_dirty((Mesh *)object_data,
			                           BKE_CURVE_BATCH_DIRTY_SELECT);
			break;
		case ID_CU:
			BKE_curve_batch_cache_dirty((Curve *)object_data,
			                            BKE_CURVE_BATCH_DIRTY_SELECT);
			break;
		case ID_LT:
			BKE_lattice_batch_cache_dirty((struct Lattice *)object_data,
			                              BKE_CURVE_BATCH_DIRTY_SELECT);
			break;
		default:
			break;
	}
}

void BKE_object_eval_flush_base_flags(const EvaluationContext *UNUSED(eval_ctx),
                                      Object *object, Base *base, bool is_from_set)
{
	DEG_debug_print_eval(__func__, object->id.name, object);

	/* Make sure we have the base collection settings is already populated.
	 * This will fail when BKE_layer_eval_layer_collection_pre hasn't run yet.
	 *
	 * Which usually means a missing call to DEG_id_tag_update(id, DEG_TAG_BASE_FLAGS_UPDATE).
	 * Either of the entire scene, or of the newly added objects.*/
	BLI_assert(!BLI_listbase_is_empty(&base->collection_properties->data.group));

	/* Copy flags and settings from base. */
	object->base_flag = base->flag;
	if (is_from_set) {
		object->base_flag |= BASE_FROM_SET;
		object->base_flag &= ~(BASE_SELECTED | BASE_SELECTABLED);
	}
	object->base_collection_properties = base->collection_properties;
}
