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

/** \file blender/blenkernel/intern/object.c
 *  \ingroup bke
 */


#include <string.h>
#include <math.h>
#include <stdio.h>

#include "MEM_guardedalloc.h"

#include "DNA_anim_types.h"
#include "DNA_armature_types.h"
#include "DNA_camera_types.h"
#include "DNA_constraint_types.h"
#include "DNA_group_types.h"
#include "DNA_key_types.h"
#include "DNA_lattice_types.h"
#include "DNA_material_types.h"
#include "DNA_meta_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_movieclip_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_sequence_types.h"
#include "DNA_smoke_types.h"
#include "DNA_sound_types.h"
#include "DNA_space_types.h"
#include "DNA_view3d_types.h"
#include "DNA_world_types.h"
#include "DNA_object_types.h"
#include "DNA_property_types.h"
#include "DNA_rigidbody_types.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_threads.h"
#include "BLI_utildefines.h"
#include "BLI_linklist.h"
#include "BLI_kdtree.h"

#include "BLF_translation.h"

#include "BKE_pbvh.h"
#include "BKE_main.h"
#include "BKE_global.h"
#include "BKE_idprop.h"
#include "BKE_armature.h"
#include "BKE_action.h"
#include "BKE_bullet.h"
#include "BKE_colortools.h"
#include "BKE_deform.h"
#include "BKE_depsgraph.h"
#include "BKE_DerivedMesh.h"
#include "BKE_animsys.h"
#include "BKE_anim.h"
#include "BKE_constraint.h"
#include "BKE_curve.h"
#include "BKE_displist.h"
#include "BKE_effect.h"
#include "BKE_fcurve.h"
#include "BKE_group.h"
#include "BKE_icons.h"
#include "BKE_key.h"
#include "BKE_lamp.h"
#include "BKE_lattice.h"
#include "BKE_library.h"
#include "BKE_linestyle.h"
#include "BKE_mesh.h"
#include "BKE_editmesh.h"
#include "BKE_mball.h"
#include "BKE_modifier.h"
#include "BKE_node.h"
#include "BKE_object.h"
#include "BKE_paint.h"
#include "BKE_particle.h"
#include "BKE_pointcache.h"
#include "BKE_property.h"
#include "BKE_rigidbody.h"
#include "BKE_sca.h"
#include "BKE_scene.h"
#include "BKE_sequencer.h"
#include "BKE_speaker.h"
#include "BKE_softbody.h"
#include "BKE_material.h"
#include "BKE_camera.h"

#ifdef WITH_MOD_FLUID
#include "LBM_fluidsim.h"
#endif

#ifdef WITH_PYTHON
#include "BPY_extern.h"
#endif

#include "GPU_material.h"

/* Vertex parent modifies original BMesh which is not safe for threading.
 * Ideally such a modification should be handled as a separate DAG update
 * callback for mesh datablock, but for until it is actually supported use
 * simpler solution with a mutex lock.
 *                                               - sergey -
 */
#define VPARENT_THREADING_HACK

#ifdef VPARENT_THREADING_HACK
static ThreadMutex vparent_lock = BLI_MUTEX_INITIALIZER;
#endif

void BKE_object_workob_clear(Object *workob)
{
	memset(workob, 0, sizeof(Object));
	
	workob->size[0] = workob->size[1] = workob->size[2] = 1.0f;
	workob->dscale[0] = workob->dscale[1] = workob->dscale[2] = 1.0f;
	workob->rotmode = ROT_MODE_EUL;
}

void BKE_object_update_base_layer(struct Scene *scene, Object *ob)
{
	Base *base = scene->base.first;

	while (base) {
		if (base->object == ob) base->lay = ob->lay;
		base = base->next;
	}
}

void BKE_object_free_particlesystems(Object *ob)
{
	ParticleSystem *psys;

	while ((psys = BLI_pophead(&ob->particlesystem))) {
		psys_free(ob, psys);
	}
}

void BKE_object_free_softbody(Object *ob)
{
	if (ob->soft) {
		sbFree(ob->soft);
		ob->soft = NULL;
	}
}

void BKE_object_free_bulletsoftbody(Object *ob)
{
	if (ob->bsoft) {
		bsbFree(ob->bsoft);
		ob->bsoft = NULL;
	}
}

void BKE_object_free_curve_cache(Object *ob)
{
	if (ob->curve_cache) {
		BKE_displist_free(&ob->curve_cache->disp);
		BLI_freelistN(&ob->curve_cache->bev);
		if (ob->curve_cache->path) {
			free_path(ob->curve_cache->path);
		}
		MEM_freeN(ob->curve_cache);
		ob->curve_cache = NULL;
	}
}

void BKE_object_free_modifiers(Object *ob)
{
	ModifierData *md;

	while ((md = BLI_pophead(&ob->modifiers))) {
		modifier_free(md);
	}

	/* particle modifiers were freed, so free the particlesystems as well */
	BKE_object_free_particlesystems(ob);

	/* same for softbody */
	BKE_object_free_softbody(ob);
}

void BKE_object_modifier_hook_reset(Object *ob, HookModifierData *hmd)
{
	/* reset functionality */
	if (hmd->object) {
		bPoseChannel *pchan = BKE_pose_channel_find_name(hmd->object->pose, hmd->subtarget);

		if (hmd->subtarget[0] && pchan) {
			float imat[4][4], mat[4][4];

			/* calculate the world-space matrix for the pose-channel target first, then carry on as usual */
			mul_m4_m4m4(mat, hmd->object->obmat, pchan->pose_mat);

			invert_m4_m4(imat, mat);
			mul_m4_m4m4(hmd->parentinv, imat, ob->obmat);
		}
		else {
			invert_m4_m4(hmd->object->imat, hmd->object->obmat);
			mul_m4_m4m4(hmd->parentinv, hmd->object->imat, ob->obmat);
		}
	}
}

bool BKE_object_support_modifier_type_check(Object *ob, int modifier_type)
{
	ModifierTypeInfo *mti;

	mti = modifierType_getInfo(modifier_type);

	if (!((mti->flags & eModifierTypeFlag_AcceptsCVs) ||
	      (ob->type == OB_MESH && (mti->flags & eModifierTypeFlag_AcceptsMesh))))
	{
		return false;
	}

	return true;
}

void BKE_object_link_modifiers(struct Object *ob_dst, struct Object *ob_src)
{
	ModifierData *md;
	BKE_object_free_modifiers(ob_dst);

	if (!ELEM5(ob_dst->type, OB_MESH, OB_CURVE, OB_SURF, OB_FONT, OB_LATTICE)) {
		/* only objects listed above can have modifiers and linking them to objects
		 * which doesn't have modifiers stack is quite silly */
		return;
	}

	for (md = ob_src->modifiers.first; md; md = md->next) {
		ModifierData *nmd = NULL;

		if (ELEM4(md->type,
		          eModifierType_Hook,
		          eModifierType_Softbody,
		          eModifierType_ParticleInstance,
		          eModifierType_Collision))
		{
			continue;
		}

		if (!BKE_object_support_modifier_type_check(ob_dst, md->type))
			continue;
		
		if (md->type == eModifierType_Skin) {
			/* ensure skin-node customdata exists */
			modifier_skin_customdata_ensure(ob_dst);
		}

		nmd = modifier_new(md->type);
		BLI_strncpy(nmd->name, md->name, sizeof(nmd->name));
		modifier_copyData(md, nmd);
		BLI_addtail(&ob_dst->modifiers, nmd);
		modifier_unique_name(&ob_dst->modifiers, nmd);
	}

	BKE_object_copy_particlesystems(ob_dst, ob_src);
	BKE_object_copy_softbody(ob_dst, ob_src);

	/* TODO: smoke?, cloth? */
}

/* free data derived from mesh, called when mesh changes or is freed */
void BKE_object_free_derived_caches(Object *ob)
{
	/* also serves as signal to remake texspace */
	if (ob->type == OB_MESH) {
		Mesh *me = ob->data;

		if (me->bb) {
			me->bb->flag |= BOUNDBOX_DIRTY;
		}
	}
	else if (ELEM3(ob->type, OB_SURF, OB_CURVE, OB_FONT)) {
		Curve *cu = ob->data;

		if (cu->bb) {
			cu->bb->flag |= BOUNDBOX_DIRTY;
		}
	}

	if (ob->bb) {
		MEM_freeN(ob->bb);
		ob->bb = NULL;
	}

	if (ob->derivedFinal) {
		ob->derivedFinal->needsFree = 1;
		ob->derivedFinal->release(ob->derivedFinal);
		ob->derivedFinal = NULL;
	}
	if (ob->derivedDeform) {
		ob->derivedDeform->needsFree = 1;
		ob->derivedDeform->release(ob->derivedDeform);
		ob->derivedDeform = NULL;
	}
	
	if (ob->curve_cache) {
		BKE_displist_free(&ob->curve_cache->disp);
		BLI_freelistN(&ob->curve_cache->bev);
		if (ob->curve_cache->path) {
			free_path(ob->curve_cache->path);
			ob->curve_cache->path = NULL;
		}
	}
}

/* do not free object itself */
void BKE_object_free_ex(Object *ob, bool do_id_user)
{
	int a;
	
	BKE_object_free_derived_caches(ob);
	
	/* disconnect specific data, but not for lib data (might be indirect data, can get relinked) */
	if (ob->data) {
		ID *id = ob->data;
		id->us--;
		if (id->us == 0 && id->lib == NULL) {
			switch (ob->type) {
				case OB_MESH:
					BKE_mesh_unlink((Mesh *)id);
					break;
				case OB_CURVE:
					BKE_curve_unlink((Curve *)id);
					break;
				case OB_MBALL:
					BKE_mball_unlink((MetaBall *)id);
					break;
			}
		}
		ob->data = NULL;
	}

	if (ob->mat) {
		for (a = 0; a < ob->totcol; a++) {
			if (ob->mat[a]) ob->mat[a]->id.us--;
		}
		MEM_freeN(ob->mat);
	}
	if (ob->matbits) MEM_freeN(ob->matbits);
	ob->mat = NULL;
	ob->matbits = NULL;
	if (ob->bb) MEM_freeN(ob->bb); 
	ob->bb = NULL;
	if (ob->adt) BKE_free_animdata((ID *)ob);
	if (ob->poselib) ob->poselib->id.us--;
	if (ob->gpd) ((ID *)ob->gpd)->us--;
	if (ob->defbase.first)
		BLI_freelistN(&ob->defbase);
	if (ob->pose)
		BKE_pose_free_ex(ob->pose, do_id_user);
	if (ob->mpath)
		animviz_free_motionpath(ob->mpath);
	BKE_bproperty_free_list(&ob->prop);
	BKE_object_free_modifiers(ob);
	
	free_sensors(&ob->sensors);
	free_controllers(&ob->controllers);
	free_actuators(&ob->actuators);
	
	BKE_free_constraints(&ob->constraints);
	
	free_partdeflect(ob->pd);
	BKE_rigidbody_free_object(ob);
	BKE_rigidbody_free_constraint(ob);

	if (ob->soft) sbFree(ob->soft);
	if (ob->bsoft) bsbFree(ob->bsoft);
	if (ob->gpulamp.first) GPU_lamp_free(ob);

	free_sculptsession(ob);

	if (ob->pc_ids.first) BLI_freelistN(&ob->pc_ids);

	BLI_freelistN(&ob->lodlevels);

	/* Free runtime curves data. */
	if (ob->curve_cache) {
		BLI_freelistN(&ob->curve_cache->bev);
		if (ob->curve_cache->path)
			free_path(ob->curve_cache->path);
		MEM_freeN(ob->curve_cache);
	}
}

void BKE_object_free(Object *ob)
{
	BKE_object_free_ex(ob, true);
}

static void unlink_object__unlinkModifierLinks(void *userData, Object *ob, Object **obpoin)
{
	Object *unlinkOb = userData;

	if (*obpoin == unlinkOb) {
		*obpoin = NULL;
		// XXX: should this just be OB_RECALC_DATA?
		DAG_id_tag_update(&ob->id, OB_RECALC_OB | OB_RECALC_DATA | OB_RECALC_TIME);
	}
}

void BKE_object_unlink(Object *ob)
{
	Main *bmain = G.main;
	Object *obt;
	Material *mat;
	World *wrld;
	bScreen *sc;
	Scene *sce;
	SceneRenderLayer *srl;
	FreestyleLineSet *lineset;
	Curve *cu;
	Tex *tex;
	Group *group;
	Camera *camera;
	bConstraint *con;
	//bActionStrip *strip; // XXX animsys 
	ModifierData *md;
	ARegion *ar;
	RegionView3D *rv3d;
	LodLevel *lod;
	int a, found;
	
	unlink_controllers(&ob->controllers);
	unlink_actuators(&ob->actuators);
	
	/* check all objects: parents en bevels and fields, also from libraries */
	/* FIXME: need to check all animation blocks (drivers) */
	obt = bmain->object.first;
	while (obt) {
		if (obt->proxy == ob)
			obt->proxy = NULL;
		if (obt->proxy_from == ob) {
			obt->proxy_from = NULL;
			DAG_id_tag_update(&obt->id, OB_RECALC_OB);
		}
		if (obt->proxy_group == ob)
			obt->proxy_group = NULL;
		
		if (obt->parent == ob) {
			obt->parent = NULL;
			DAG_id_tag_update(&obt->id, OB_RECALC_OB | OB_RECALC_DATA | OB_RECALC_TIME);
		}
		
		modifiers_foreachObjectLink(obt, unlink_object__unlinkModifierLinks, ob);
		
		if (ELEM(obt->type, OB_CURVE, OB_FONT)) {
			cu = obt->data;

			if (cu->bevobj == ob) {
				cu->bevobj = NULL;
				DAG_id_tag_update(&obt->id, OB_RECALC_OB | OB_RECALC_DATA | OB_RECALC_TIME);
			}
			if (cu->taperobj == ob) {
				cu->taperobj = NULL;
				DAG_id_tag_update(&obt->id, OB_RECALC_OB | OB_RECALC_DATA | OB_RECALC_TIME);
			}
			if (cu->textoncurve == ob) {
				cu->textoncurve = NULL;
				DAG_id_tag_update(&obt->id, OB_RECALC_OB | OB_RECALC_DATA | OB_RECALC_TIME);
			}
		}
		else if (obt->type == OB_ARMATURE && obt->pose) {
			bPoseChannel *pchan;
			for (pchan = obt->pose->chanbase.first; pchan; pchan = pchan->next) {
				for (con = pchan->constraints.first; con; con = con->next) {
					bConstraintTypeInfo *cti = BKE_constraint_get_typeinfo(con);
					ListBase targets = {NULL, NULL};
					bConstraintTarget *ct;
					
					if (cti && cti->get_constraint_targets) {
						cti->get_constraint_targets(con, &targets);
						
						for (ct = targets.first; ct; ct = ct->next) {
							if (ct->tar == ob) {
								ct->tar = NULL;
								ct->subtarget[0] = '\0';
								DAG_id_tag_update(&obt->id, OB_RECALC_DATA);
							}
						}
						
						if (cti->flush_constraint_targets)
							cti->flush_constraint_targets(con, &targets, 0);
					}
				}
				if (pchan->custom == ob)
					pchan->custom = NULL;
			}
		}
		else if (ELEM(OB_MBALL, ob->type, obt->type)) {
			if (BKE_mball_is_basis_for(obt, ob))
				DAG_id_tag_update(&obt->id, OB_RECALC_DATA);
		}
		
		sca_remove_ob_poin(obt, ob);
		
		for (con = obt->constraints.first; con; con = con->next) {
			bConstraintTypeInfo *cti = BKE_constraint_get_typeinfo(con);
			ListBase targets = {NULL, NULL};
			bConstraintTarget *ct;
			
			if (cti && cti->get_constraint_targets) {
				cti->get_constraint_targets(con, &targets);
				
				for (ct = targets.first; ct; ct = ct->next) {
					if (ct->tar == ob) {
						ct->tar = NULL;
						ct->subtarget[0] = '\0';
						DAG_id_tag_update(&obt->id, OB_RECALC_DATA);
					}
				}
				
				if (cti->flush_constraint_targets)
					cti->flush_constraint_targets(con, &targets, 0);
			}
		}
		
		/* object is deflector or field */
		if (ob->pd) {
			if (obt->soft)
				DAG_id_tag_update(&obt->id, OB_RECALC_DATA);

			/* cloth */
			for (md = obt->modifiers.first; md; md = md->next)
				if (md->type == eModifierType_Cloth)
					DAG_id_tag_update(&obt->id, OB_RECALC_DATA);
		}
		
		/* strips */
#if 0 // XXX old animation system
		for (strip = obt->nlastrips.first; strip; strip = strip->next) {
			if (strip->object == ob)
				strip->object = NULL;
			
			if (strip->modifiers.first) {
				bActionModifier *amod;
				for (amod = strip->modifiers.first; amod; amod = amod->next)
					if (amod->ob == ob)
						amod->ob = NULL;
			}
		}
#endif // XXX old animation system

		/* particle systems */
		if (obt->particlesystem.first) {
			ParticleSystem *tpsys = obt->particlesystem.first;
			for (; tpsys; tpsys = tpsys->next) {
				BoidState *state = NULL;
				BoidRule *rule = NULL;

				ParticleTarget *pt = tpsys->targets.first;
				for (; pt; pt = pt->next) {
					if (pt->ob == ob) {
						pt->ob = NULL;
						DAG_id_tag_update(&obt->id, OB_RECALC_DATA);
						break;
					}
				}

				if (tpsys->target_ob == ob) {
					tpsys->target_ob = NULL;
					DAG_id_tag_update(&obt->id, OB_RECALC_DATA);
				}

				if (tpsys->part->dup_ob == ob)
					tpsys->part->dup_ob = NULL;

				if (tpsys->part->phystype == PART_PHYS_BOIDS) {
					ParticleData *pa;
					BoidParticle *bpa;
					int p;

					for (p = 0, pa = tpsys->particles; p < tpsys->totpart; p++, pa++) {
						bpa = pa->boid;
						if (bpa->ground == ob)
							bpa->ground = NULL;
					}
				}
				if (tpsys->part->boids) {
					for (state = tpsys->part->boids->states.first; state; state = state->next) {
						for (rule = state->rules.first; rule; rule = rule->next) {
							if (rule->type == eBoidRuleType_Avoid) {
								BoidRuleGoalAvoid *gabr = (BoidRuleGoalAvoid *)rule;
								if (gabr->ob == ob)
									gabr->ob = NULL;
							}
							else if (rule->type == eBoidRuleType_FollowLeader) {
								BoidRuleFollowLeader *flbr = (BoidRuleFollowLeader *)rule;
								if (flbr->ob == ob)
									flbr->ob = NULL;
							}
						}
					}
				}
				
				if (tpsys->parent == ob)
					tpsys->parent = NULL;
			}
			if (ob->pd)
				DAG_id_tag_update(&obt->id, OB_RECALC_DATA);
		}

		/* levels of detail */
		for (lod = obt->lodlevels.first; lod; lod = lod->next) {
			if (lod->source == ob)
				lod->source = NULL;
		}

		obt = obt->id.next;
	}
	
	/* materials */
	mat = bmain->mat.first;
	while (mat) {
	
		for (a = 0; a < MAX_MTEX; a++) {
			if (mat->mtex[a] && ob == mat->mtex[a]->object) {
				/* actually, test for lib here... to do */
				mat->mtex[a]->object = NULL;
			}
		}

		mat = mat->id.next;
	}
	
	/* textures */
	for (tex = bmain->tex.first; tex; tex = tex->id.next) {
		if (tex->env && (ob == tex->env->object)) tex->env->object = NULL;
		if (tex->pd  && (ob == tex->pd->object)) tex->pd->object = NULL;
		if (tex->vd  && (ob == tex->vd->object)) tex->vd->object = NULL;
	}

	/* worlds */
	wrld = bmain->world.first;
	while (wrld) {
		if (wrld->id.lib == NULL) {
			for (a = 0; a < MAX_MTEX; a++) {
				if (wrld->mtex[a] && ob == wrld->mtex[a]->object)
					wrld->mtex[a]->object = NULL;
			}
		}
		
		wrld = wrld->id.next;
	}
		
	/* scenes */
	sce = bmain->scene.first;
	while (sce) {
		if (sce->id.lib == NULL) {
			if (sce->camera == ob) sce->camera = NULL;
			if (sce->toolsettings->skgen_template == ob) sce->toolsettings->skgen_template = NULL;
			if (sce->toolsettings->particle.object == ob) sce->toolsettings->particle.object = NULL;

#ifdef DURIAN_CAMERA_SWITCH
			{
				TimeMarker *m;

				for (m = sce->markers.first; m; m = m->next) {
					if (m->camera == ob)
						m->camera = NULL;
				}
			}
#endif
			if (sce->ed) {
				Sequence *seq;
				SEQ_BEGIN(sce->ed, seq)
				{
					if (seq->scene_camera == ob) {
						seq->scene_camera = NULL;
					}
				}
				SEQ_END
			}

			for (srl = sce->r.layers.first; srl; srl = srl->next) {
				for (lineset = (FreestyleLineSet *)srl->freestyleConfig.linesets.first;
				     lineset; lineset = lineset->next)
				{
					if (lineset->linestyle) {
						BKE_unlink_linestyle_target_object(lineset->linestyle, ob);
					}
				}
			}
		}

		sce = sce->id.next;
	}
	
	/* screens */
	sc = bmain->screen.first;
	while (sc) {
		ScrArea *sa = sc->areabase.first;
		while (sa) {
			SpaceLink *sl;

			for (sl = sa->spacedata.first; sl; sl = sl->next) {
				if (sl->spacetype == SPACE_VIEW3D) {
					View3D *v3d = (View3D *) sl;

					/* found doesn't need to be set here */
					if (v3d->ob_centre == ob) {
						v3d->ob_centre = NULL;
						v3d->ob_centre_bone[0] = '\0';
					}
					if (v3d->localvd && v3d->localvd->ob_centre == ob) {
						v3d->localvd->ob_centre = NULL;
						v3d->localvd->ob_centre_bone[0] = '\0';
					}

					found = 0;
					if (v3d->camera == ob) {
						v3d->camera = NULL;
						found = 1;
					}
					if (v3d->localvd && v3d->localvd->camera == ob) {
						v3d->localvd->camera = NULL;
						found += 2;
					}

					if (found) {
						if (sa->spacetype == SPACE_VIEW3D) {
							for (ar = sa->regionbase.first; ar; ar = ar->next) {
								if (ar->regiontype == RGN_TYPE_WINDOW) {
									rv3d = (RegionView3D *)ar->regiondata;
									if (found == 1 || found == 3) {
										if (rv3d->persp == RV3D_CAMOB)
											rv3d->persp = RV3D_PERSP;
									}
									if (found == 2 || found == 3) {
										if (rv3d->localvd && rv3d->localvd->persp == RV3D_CAMOB)
											rv3d->localvd->persp = RV3D_PERSP;
									}
								}
							}
						}
					}
				}
				else if (sl->spacetype == SPACE_OUTLINER) {
					SpaceOops *so = (SpaceOops *)sl;

					if (so->treestore) {
						TreeStoreElem *tselem;
						BLI_mempool_iter iter;
						BLI_mempool_iternew(so->treestore, &iter);
						while ((tselem = BLI_mempool_iterstep(&iter))) {
							if (tselem->id == (ID *)ob) tselem->id = NULL;
						}
					}
				}
				else if (sl->spacetype == SPACE_BUTS) {
					SpaceButs *sbuts = (SpaceButs *)sl;

					if (sbuts->pinid == (ID *)ob) {
						sbuts->flag &= ~SB_PIN_CONTEXT;
						sbuts->pinid = NULL;
					}
				}
				else if (sl->spacetype == SPACE_NODE) {
					SpaceNode *snode = (SpaceNode *)sl;

					if (snode->from == (ID *)ob) {
						snode->flag &= ~SNODE_PIN;
						snode->from = NULL;
					}
				}
			}

			sa = sa->next;
		}
		sc = sc->id.next;
	}

	/* groups */
	group = bmain->group.first;
	while (group) {
		BKE_group_object_unlink(group, ob, NULL, NULL);
		group = group->id.next;
	}
	
	/* cameras */
	camera = bmain->camera.first;
	while (camera) {
		if (camera->dof_ob == ob) {
			camera->dof_ob = NULL;
		}
		camera = camera->id.next;
	}
}

/* actual check for internal data, not context or flags */
bool BKE_object_is_in_editmode(Object *ob)
{
	if (ob->data == NULL)
		return false;
	
	if (ob->type == OB_MESH) {
		Mesh *me = ob->data;
		if (me->edit_btmesh)
			return true;
	}
	else if (ob->type == OB_ARMATURE) {
		bArmature *arm = ob->data;
		
		if (arm->edbo)
			return true;
	}
	else if (ob->type == OB_FONT) {
		Curve *cu = ob->data;
		
		if (cu->editfont)
			return true;
	}
	else if (ob->type == OB_MBALL) {
		MetaBall *mb = ob->data;
		
		if (mb->editelems)
			return true;
	}
	else if (ob->type == OB_LATTICE) {
		Lattice *lt = ob->data;
		
		if (lt->editlatt)
			return true;
	}
	else if (ob->type == OB_SURF || ob->type == OB_CURVE) {
		Curve *cu = ob->data;

		if (cu->editnurb)
			return true;
	}
	return false;
}

bool BKE_object_is_in_editmode_vgroup(Object *ob)
{
	return (OB_TYPE_SUPPORT_VGROUP(ob->type) &&
	        BKE_object_is_in_editmode(ob));
}

bool BKE_object_is_in_wpaint_select_vert(Object *ob)
{
	if (ob->type == OB_MESH) {
		Mesh *me = ob->data;
		return ( (ob->mode & OB_MODE_WEIGHT_PAINT) &&
		         (me->edit_btmesh == NULL) &&
		         (ME_EDIT_PAINT_SEL_MODE(me) == SCE_SELECT_VERTEX) );
	}

	return false;
}

bool BKE_object_exists_check(Object *obtest)
{
	Object *ob;
	
	if (obtest == NULL) return false;
	
	ob = G.main->object.first;
	while (ob) {
		if (ob == obtest) return true;
		ob = ob->id.next;
	}
	return false;
}

/* *************************************************** */

void *BKE_object_obdata_add_from_type(Main *bmain, int type)
{
	switch (type) {
		case OB_MESH:      return BKE_mesh_add(bmain, "Mesh");
		case OB_CURVE:     return BKE_curve_add(bmain, "Curve", OB_CURVE);
		case OB_SURF:      return BKE_curve_add(bmain, "Surf", OB_SURF);
		case OB_FONT:      return BKE_curve_add(bmain, "Text", OB_FONT);
		case OB_MBALL:     return BKE_mball_add(bmain, "Meta");
		case OB_CAMERA:    return BKE_camera_add(bmain, "Camera");
		case OB_LAMP:      return BKE_lamp_add(bmain, "Lamp");
		case OB_LATTICE:   return BKE_lattice_add(bmain, "Lattice");
		case OB_ARMATURE:  return BKE_armature_add(bmain, "Armature");
		case OB_SPEAKER:   return BKE_speaker_add(bmain, "Speaker");
		case OB_EMPTY:     return NULL;
		default:
			printf("BKE_object_obdata_add_from_type: Internal error, bad type: %d\n", type);
			return NULL;
	}
}

static const char *get_obdata_defname(int type)
{
	switch (type) {
		case OB_MESH: return DATA_("Mesh");
		case OB_CURVE: return DATA_("Curve");
		case OB_SURF: return DATA_("Surf");
		case OB_FONT: return DATA_("Text");
		case OB_MBALL: return DATA_("Mball");
		case OB_CAMERA: return DATA_("Camera");
		case OB_LAMP: return DATA_("Lamp");
		case OB_LATTICE: return DATA_("Lattice");
		case OB_ARMATURE: return DATA_("Armature");
		case OB_SPEAKER: return DATA_("Speaker");
		case OB_EMPTY: return DATA_("Empty");
		default:
			printf("get_obdata_defname: Internal error, bad type: %d\n", type);
			return DATA_("Empty");
	}
}

/* more general add: creates minimum required data, but without vertices etc. */
Object *BKE_object_add_only_object(Main *bmain, int type, const char *name)
{
	Object *ob;

	if (!name)
		name = get_obdata_defname(type);

	ob = BKE_libblock_alloc(&bmain->object, ID_OB, name);

	/* default object vars */
	ob->type = type;
	
	ob->col[0] = ob->col[1] = ob->col[2] = 1.0;
	ob->col[3] = 1.0;
	
	ob->size[0] = ob->size[1] = ob->size[2] = 1.0;
	ob->dscale[0] = ob->dscale[1] = ob->dscale[2] = 1.0;
	
	/* objects should default to having Euler XYZ rotations, 
	 * but rotations default to quaternions 
	 */
	ob->rotmode = ROT_MODE_EUL;

	unit_axis_angle(ob->rotAxis, &ob->rotAngle);
	unit_axis_angle(ob->drotAxis, &ob->drotAngle);

	unit_qt(ob->quat);
	unit_qt(ob->dquat);

	/* rotation locks should be 4D for 4 component rotations by default... */
	ob->protectflag = OB_LOCK_ROT4D;
	
	unit_m4(ob->constinv);
	unit_m4(ob->parentinv);
	unit_m4(ob->obmat);
	ob->dt = OB_TEXTURE;
	ob->empty_drawtype = OB_PLAINAXES;
	ob->empty_drawsize = 1.0;

	if (ELEM3(type, OB_LAMP, OB_CAMERA, OB_SPEAKER)) {
		ob->trackflag = OB_NEGZ;
		ob->upflag = OB_POSY;
	}
	else {
		ob->trackflag = OB_POSY;
		ob->upflag = OB_POSZ;
	}
	
	ob->dupon = 1; ob->dupoff = 0;
	ob->dupsta = 1; ob->dupend = 100;
	ob->dupfacesca = 1.0;

	/* Game engine defaults*/
	ob->mass = ob->inertia = 1.0f;
	ob->formfactor = 0.4f;
	ob->damping = 0.04f;
	ob->rdamping = 0.1f;
	ob->anisotropicFriction[0] = 1.0f;
	ob->anisotropicFriction[1] = 1.0f;
	ob->anisotropicFriction[2] = 1.0f;
	ob->gameflag = OB_PROP | OB_COLLISION;
	ob->margin = 0.04f;
	ob->init_state = 1;
	ob->state = 1;
	/* ob->pad3 == Contact Processing Threshold */
	ob->m_contactProcessingThreshold = 1.0f;
	ob->obstacleRad = 1.0f;
	ob->step_height = 0.15f;
	ob->jump_speed = 10.0f;
	ob->fall_speed = 55.0f;
	ob->col_group = 0x01;
	ob->col_mask = 0xff;

	/* NT fluid sim defaults */
	ob->fluidsimSettings = NULL;

	ob->pc_ids.first = ob->pc_ids.last = NULL;
	
	/* Animation Visualization defaults */
	animviz_settings_init(&ob->avs);

	return ob;
}

/* general add: to scene, with layer from area and default name */
/* creates minimum required data, but without vertices etc. */
Object *BKE_object_add(Main *bmain, Scene *scene, int type)
{
	Object *ob;
	Base *base;
	char name[MAX_ID_NAME];

	BLI_strncpy(name, get_obdata_defname(type), sizeof(name));
	ob = BKE_object_add_only_object(bmain, type, name);

	ob->data = BKE_object_obdata_add_from_type(bmain, type);

	ob->lay = scene->lay;
	
	base = BKE_scene_base_add(scene, ob);
	BKE_scene_base_deselect_all(scene);
	BKE_scene_base_select(scene, base);
	DAG_id_tag_update_ex(bmain, &ob->id, OB_RECALC_OB | OB_RECALC_DATA | OB_RECALC_TIME);

	return ob;
}

void BKE_object_lod_add(Object *ob)
{
	LodLevel *lod = MEM_callocN(sizeof(LodLevel), "LoD Level");
	LodLevel *last = ob->lodlevels.last;

	/* If the lod list is empty, initialize it with the base lod level */
	if (!last) {
		LodLevel *base = MEM_callocN(sizeof(LodLevel), "Base LoD Level");
		BLI_addtail(&ob->lodlevels, base);
		base->flags = OB_LOD_USE_MESH | OB_LOD_USE_MAT;
		base->source = ob;
		last = ob->currentlod = base;
	}
	
	lod->distance = last->distance + 25.0f;
	lod->flags = OB_LOD_USE_MESH | OB_LOD_USE_MAT;

	BLI_addtail(&ob->lodlevels, lod);
}

static int lod_cmp(void *a, void *b)
{
	LodLevel *loda = (LodLevel *)a;
	LodLevel *lodb = (LodLevel *)b;

	if (loda->distance < lodb->distance) return -1;
	return loda->distance > lodb->distance;
}

void BKE_object_lod_sort(Object *ob)
{
	BLI_sortlist(&ob->lodlevels, lod_cmp);
}

bool BKE_object_lod_remove(Object *ob, int level)
{
	LodLevel *rem;

	if (level < 1 || level > BLI_countlist(&ob->lodlevels) - 1)
		return false;

	rem = BLI_findlink(&ob->lodlevels, level);

	if (rem == ob->currentlod) {
		ob->currentlod = rem->prev;
	}

	BLI_remlink(&ob->lodlevels, rem);
	MEM_freeN(rem);

	/* If there are no user defined lods, remove the base lod as well */
	if (BLI_countlist(&ob->lodlevels) == 1) {
		LodLevel *base = ob->lodlevels.first;
		BLI_remlink(&ob->lodlevels, base);
		MEM_freeN(base);
		ob->currentlod = NULL;
	}

	return true;
}

static LodLevel *lod_level_select(Object *ob, const float cam_loc[3])
{
	LodLevel *current = ob->currentlod;
	float ob_loc[3], delta[3];
	float distance2;

	if (!current) return NULL;

	copy_v3_v3(ob_loc, ob->obmat[3]);
	sub_v3_v3v3(delta, ob_loc, cam_loc);
	distance2 = len_squared_v3(delta);

	if (distance2 < current->distance * current->distance) {
		/* check for higher LoD */
		while (current->prev && distance2 < (current->distance * current->distance)) {
			current = current->prev;
		}
	}
	else {
		/* check for lower LoD */
		while (current->next && distance2 > (current->next->distance * current->next->distance)) {
			current = current->next;
		}
	}

	return current;
}

bool BKE_object_lod_is_usable(Object *ob, Scene *scene)
{
	bool active = (scene) ? ob == OBACT : 0;
	return (ob->mode == OB_MODE_OBJECT || !active);
}

bool BKE_object_lod_update(Object *ob, float camera_position[3])
{
	LodLevel *cur_level = ob->currentlod;
	LodLevel *new_level = lod_level_select(ob, camera_position);

	if (new_level != cur_level) {
		ob->currentlod = new_level;
		return true;
	}

	return false;
}

static Object *lod_ob_get(Object *ob, Scene *scene, int flag)
{
	LodLevel *current = ob->currentlod;

	if (!current || !BKE_object_lod_is_usable(ob, scene))
		return ob;

	while (current->prev && (!(current->flags & flag) || !current->source || current->source->type != OB_MESH)) {
		current = current->prev;
	}

	return current->source;
}

struct Object *BKE_object_lod_meshob_get(Object *ob, Scene *scene)
{
	return lod_ob_get(ob, scene, OB_LOD_USE_MESH);
}

struct Object *BKE_object_lod_matob_get(Object *ob, Scene *scene)
{
	return lod_ob_get(ob, scene, OB_LOD_USE_MAT);
}

SoftBody *copy_softbody(SoftBody *sb, int copy_caches)
{
	SoftBody *sbn;
	
	if (sb == NULL) return(NULL);
	
	sbn = MEM_dupallocN(sb);

	if (copy_caches == FALSE) {
		sbn->totspring = sbn->totpoint = 0;
		sbn->bpoint = NULL;
		sbn->bspring = NULL;
	}
	else {
		sbn->totspring = sb->totspring;
		sbn->totpoint = sb->totpoint;

		if (sbn->bpoint) {
			int i;

			sbn->bpoint = MEM_dupallocN(sbn->bpoint);

			for (i = 0; i < sbn->totpoint; i++) {
				if (sbn->bpoint[i].springs)
					sbn->bpoint[i].springs = MEM_dupallocN(sbn->bpoint[i].springs);
			}
		}

		if (sb->bspring)
			sbn->bspring = MEM_dupallocN(sb->bspring);
	}
	
	sbn->keys = NULL;
	sbn->totkey = sbn->totpointkey = 0;
	
	sbn->scratch = NULL;

	sbn->pointcache = BKE_ptcache_copy_list(&sbn->ptcaches, &sb->ptcaches, copy_caches);

	if (sb->effector_weights)
		sbn->effector_weights = MEM_dupallocN(sb->effector_weights);

	return sbn;
}

BulletSoftBody *copy_bulletsoftbody(BulletSoftBody *bsb)
{
	BulletSoftBody *bsbn;

	if (bsb == NULL)
		return NULL;
	bsbn = MEM_dupallocN(bsb);
	/* no pointer in this structure yet */
	return bsbn;
}

static ParticleSystem *copy_particlesystem(ParticleSystem *psys)
{
	ParticleSystem *psysn;
	ParticleData *pa;
	int p;

	psysn = MEM_dupallocN(psys);
	psysn->particles = MEM_dupallocN(psys->particles);
	psysn->child = MEM_dupallocN(psys->child);

	if (psys->part->type == PART_HAIR) {
		for (p = 0, pa = psysn->particles; p < psysn->totpart; p++, pa++)
			pa->hair = MEM_dupallocN(pa->hair);
	}

	if (psysn->particles && (psysn->particles->keys || psysn->particles->boid)) {
		ParticleKey *key = psysn->particles->keys;
		BoidParticle *boid = psysn->particles->boid;

		if (key)
			key = MEM_dupallocN(key);
		
		if (boid)
			boid = MEM_dupallocN(boid);
		
		for (p = 0, pa = psysn->particles; p < psysn->totpart; p++, pa++) {
			if (boid)
				pa->boid = boid++;
			if (key) {
				pa->keys = key;
				key += pa->totkey;
			}
		}
	}

	if (psys->clmd) {
		psysn->clmd = (ClothModifierData *)modifier_new(eModifierType_Cloth);
		modifier_copyData((ModifierData *)psys->clmd, (ModifierData *)psysn->clmd);
		psys->hair_in_dm = psys->hair_out_dm = NULL;
	}

	BLI_duplicatelist(&psysn->targets, &psys->targets);

	psysn->pathcache = NULL;
	psysn->childcache = NULL;
	psysn->edit = NULL;
	psysn->frand = NULL;
	psysn->pdd = NULL;
	psysn->effectors = NULL;
	
	psysn->pathcachebufs.first = psysn->pathcachebufs.last = NULL;
	psysn->childcachebufs.first = psysn->childcachebufs.last = NULL;
	psysn->renderdata = NULL;
	
	psysn->pointcache = BKE_ptcache_copy_list(&psysn->ptcaches, &psys->ptcaches, FALSE);

	/* XXX - from reading existing code this seems correct but intended usage of
	 * pointcache should /w cloth should be added in 'ParticleSystem' - campbell */
	if (psysn->clmd) {
		psysn->clmd->point_cache = psysn->pointcache;
	}

	id_us_plus((ID *)psysn->part);

	return psysn;
}

void BKE_object_copy_particlesystems(Object *obn, Object *ob)
{
	ParticleSystem *psys, *npsys;
	ModifierData *md;

	if (obn->type != OB_MESH) {
		/* currently only mesh objects can have soft body */
		return;
	}

	obn->particlesystem.first = obn->particlesystem.last = NULL;
	for (psys = ob->particlesystem.first; psys; psys = psys->next) {
		npsys = copy_particlesystem(psys);

		BLI_addtail(&obn->particlesystem, npsys);

		/* need to update particle modifiers too */
		for (md = obn->modifiers.first; md; md = md->next) {
			if (md->type == eModifierType_ParticleSystem) {
				ParticleSystemModifierData *psmd = (ParticleSystemModifierData *)md;
				if (psmd->psys == psys)
					psmd->psys = npsys;
			}
			else if (md->type == eModifierType_DynamicPaint) {
				DynamicPaintModifierData *pmd = (DynamicPaintModifierData *)md;
				if (pmd->brush) {
					if (pmd->brush->psys == psys) {
						pmd->brush->psys = npsys;
					}
				}
			}
			else if (md->type == eModifierType_Smoke) {
				SmokeModifierData *smd = (SmokeModifierData *) md;
				
				if (smd->type == MOD_SMOKE_TYPE_FLOW) {
					if (smd->flow) {
						if (smd->flow->psys == psys)
							smd->flow->psys = npsys;
					}
				}
			}
		}
	}
}

void BKE_object_copy_softbody(Object *obn, Object *ob)
{
	if (ob->soft)
		obn->soft = copy_softbody(ob->soft, FALSE);
}

static void copy_object_pose(Object *obn, Object *ob)
{
	bPoseChannel *chan;
	
	/* note: need to clear obn->pose pointer first, so that BKE_pose_copy_data works (otherwise there's a crash) */
	obn->pose = NULL;
	BKE_pose_copy_data(&obn->pose, ob->pose, 1);    /* 1 = copy constraints */

	for (chan = obn->pose->chanbase.first; chan; chan = chan->next) {
		bConstraint *con;
		
		chan->flag &= ~(POSE_LOC | POSE_ROT | POSE_SIZE);
		
		if (chan->custom) {
			id_us_plus(&chan->custom->id);
		}
		
		for (con = chan->constraints.first; con; con = con->next) {
			bConstraintTypeInfo *cti = BKE_constraint_get_typeinfo(con);
			ListBase targets = {NULL, NULL};
			bConstraintTarget *ct;
			
			if (cti && cti->get_constraint_targets) {
				cti->get_constraint_targets(con, &targets);
				
				for (ct = targets.first; ct; ct = ct->next) {
					if (ct->tar == ob)
						ct->tar = obn;
				}
				
				if (cti->flush_constraint_targets)
					cti->flush_constraint_targets(con, &targets, 0);
			}
		}
	}
}

static void copy_object_lod(Object *obn, Object *ob)
{
	BLI_duplicatelist(&obn->lodlevels, &ob->lodlevels);

	if (obn->lodlevels.first)
		((LodLevel *)obn->lodlevels.first)->source = obn;

	obn->currentlod = (LodLevel *)obn->lodlevels.first;
}

bool BKE_object_pose_context_check(Object *ob)
{
	if ((ob) &&
	    (ob->type == OB_ARMATURE) &&
	    (ob->pose) &&
	    (ob->mode & OB_MODE_POSE))
	{
		return 1;
	}
	else {
		return 0;
	}
}

Object *BKE_object_pose_armature_get(Object *ob)
{
	if (ob == NULL)
		return NULL;

	if (BKE_object_pose_context_check(ob))
		return ob;

	ob = modifiers_isDeformedByArmature(ob);

	if (BKE_object_pose_context_check(ob))
		return ob;

	return NULL;
}

void BKE_object_transform_copy(Object *ob_tar, const Object *ob_src)
{
	copy_v3_v3(ob_tar->loc, ob_src->loc);
	copy_v3_v3(ob_tar->rot, ob_src->rot);
	copy_v3_v3(ob_tar->quat, ob_src->quat);
	copy_v3_v3(ob_tar->rotAxis, ob_src->rotAxis);
	ob_tar->rotAngle = ob_src->rotAngle;
	ob_tar->rotmode = ob_src->rotmode;
	copy_v3_v3(ob_tar->size, ob_src->size);
}

Object *BKE_object_copy_ex(Main *bmain, Object *ob, int copy_caches)
{
	Object *obn;
	ModifierData *md;
	int a;

	obn = BKE_libblock_copy_ex(bmain, &ob->id);
	
	if (ob->totcol) {
		obn->mat = MEM_dupallocN(ob->mat);
		obn->matbits = MEM_dupallocN(ob->matbits);
		obn->totcol = ob->totcol;
	}
	
	if (ob->bb) obn->bb = MEM_dupallocN(ob->bb);
	obn->flag &= ~OB_FROMGROUP;
	
	obn->modifiers.first = obn->modifiers.last = NULL;
	
	for (md = ob->modifiers.first; md; md = md->next) {
		ModifierData *nmd = modifier_new(md->type);
		BLI_strncpy(nmd->name, md->name, sizeof(nmd->name));
		modifier_copyData(md, nmd);
		BLI_addtail(&obn->modifiers, nmd);
	}

	obn->prop.first = obn->prop.last = NULL;
	BKE_bproperty_copy_list(&obn->prop, &ob->prop);
	
	copy_sensors(&obn->sensors, &ob->sensors);
	copy_controllers(&obn->controllers, &ob->controllers);
	copy_actuators(&obn->actuators, &ob->actuators);
	
	if (ob->pose) {
		copy_object_pose(obn, ob);
		/* backwards compat... non-armatures can get poses in older files? */
		if (ob->type == OB_ARMATURE)
			BKE_pose_rebuild(obn, obn->data);
	}
	defgroup_copy_list(&obn->defbase, &ob->defbase);
	BKE_copy_constraints(&obn->constraints, &ob->constraints, TRUE);

	obn->mode = 0;
	obn->sculpt = NULL;

	/* Proxies are not to be copied. */
	obn->proxy_from = NULL;
	obn->proxy_group = NULL;
	obn->proxy = NULL;

	/* increase user numbers */
	id_us_plus((ID *)obn->data);
	id_us_plus((ID *)obn->gpd);
	id_lib_extern((ID *)obn->dup_group);

	for (a = 0; a < obn->totcol; a++) id_us_plus((ID *)obn->mat[a]);
	
	if (ob->pd) {
		obn->pd = MEM_dupallocN(ob->pd);
		if (obn->pd->tex)
			id_us_plus(&(obn->pd->tex->id));
		if (obn->pd->rng)
			obn->pd->rng = MEM_dupallocN(ob->pd->rng);
	}
	obn->soft = copy_softbody(ob->soft, copy_caches);
	obn->bsoft = copy_bulletsoftbody(ob->bsoft);
	obn->rigidbody_object = BKE_rigidbody_copy_object(ob);
	obn->rigidbody_constraint = BKE_rigidbody_copy_constraint(ob);

	BKE_object_copy_particlesystems(obn, ob);
	
	obn->derivedDeform = NULL;
	obn->derivedFinal = NULL;

	obn->gpulamp.first = obn->gpulamp.last = NULL;
	obn->pc_ids.first = obn->pc_ids.last = NULL;

	obn->mpath = NULL;

	copy_object_lod(obn, ob);
	

	/* Copy runtime surve data. */
	obn->curve_cache = NULL;

	return obn;
}

/* copy objects, will re-initialize cached simulation data */
Object *BKE_object_copy(Object *ob)
{
	return BKE_object_copy_ex(G.main, ob, FALSE);
}

static void extern_local_object(Object *ob)
{
	ParticleSystem *psys;

	id_lib_extern((ID *)ob->data);
	id_lib_extern((ID *)ob->dup_group);
	id_lib_extern((ID *)ob->poselib);
	id_lib_extern((ID *)ob->gpd);

	extern_local_matarar(ob->mat, ob->totcol);

	for (psys = ob->particlesystem.first; psys; psys = psys->next)
		id_lib_extern((ID *)psys->part);
}

void BKE_object_make_local(Object *ob)
{
	Main *bmain = G.main;
	Scene *sce;
	Base *base;
	int is_local = FALSE, is_lib = FALSE;

	/* - only lib users: do nothing
	 * - only local users: set flag
	 * - mixed: make copy
	 */

	if (ob->id.lib == NULL) return;
	
	ob->proxy = ob->proxy_from = NULL;
	
	if (ob->id.us == 1) {
		id_clear_lib_data(bmain, &ob->id);
		extern_local_object(ob);
	}
	else {
		for (sce = bmain->scene.first; sce && ELEM(0, is_lib, is_local); sce = sce->id.next) {
			if (BKE_scene_base_find(sce, ob)) {
				if (sce->id.lib) is_lib = TRUE;
				else is_local = TRUE;
			}
		}

		if (is_local && is_lib == FALSE) {
			id_clear_lib_data(bmain, &ob->id);
			extern_local_object(ob);
		}
		else if (is_local && is_lib) {
			Object *ob_new = BKE_object_copy(ob);

			ob_new->id.us = 0;
			
			/* Remap paths of new ID using old library as base. */
			BKE_id_lib_local_paths(bmain, ob->id.lib, &ob_new->id);

			sce = bmain->scene.first;
			while (sce) {
				if (sce->id.lib == NULL) {
					base = sce->base.first;
					while (base) {
						if (base->object == ob) {
							base->object = ob_new;
							ob_new->id.us++;
							ob->id.us--;
						}
						base = base->next;
					}
				}
				sce = sce->id.next;
			}
		}
	}
}

/*
 * Returns true if the Object is a from an external blend file (libdata)
 */
bool BKE_object_is_libdata(Object *ob)
{
	if (!ob) return false;
	if (ob->proxy) return false;
	if (ob->id.lib) return true;
	return false;
}

/* Returns true if the Object data is a from an external blend file (libdata) */
bool BKE_object_obdata_is_libdata(Object *ob)
{
	if (!ob) return false;
	if (ob->proxy && (ob->data == NULL || ((ID *)ob->data)->lib == NULL)) return false;
	if (ob->id.lib) return true;
	if (ob->data == NULL) return false;
	if (((ID *)ob->data)->lib) return true;

	return false;
}

/* *************** PROXY **************** */

/* when you make proxy, ensure the exposed layers are extern */
static void armature_set_id_extern(Object *ob)
{
	bArmature *arm = ob->data;
	bPoseChannel *pchan;
	unsigned int lay = arm->layer_protected;
	
	for (pchan = ob->pose->chanbase.first; pchan; pchan = pchan->next) {
		if (!(pchan->bone->layer & lay))
			id_lib_extern((ID *)pchan->custom);
	}
			
}

void BKE_object_copy_proxy_drivers(Object *ob, Object *target)
{
	if ((target->adt) && (target->adt->drivers.first)) {
		FCurve *fcu;
		
		/* add new animdata block */
		if (!ob->adt)
			ob->adt = BKE_id_add_animdata(&ob->id);
		
		/* make a copy of all the drivers (for now), then correct any links that need fixing */
		free_fcurves(&ob->adt->drivers);
		copy_fcurves(&ob->adt->drivers, &target->adt->drivers);
		
		for (fcu = ob->adt->drivers.first; fcu; fcu = fcu->next) {
			ChannelDriver *driver = fcu->driver;
			DriverVar *dvar;
			
			for (dvar = driver->variables.first; dvar; dvar = dvar->next) {
				/* all drivers */
				DRIVER_TARGETS_LOOPER(dvar) 
				{
					if (dtar->id) {
						if ((Object *)dtar->id == target)
							dtar->id = (ID *)ob;
						else {
							/* only on local objects because this causes indirect links
							 * 'a -> b -> c', blend to point directly to a.blend
							 * when a.blend has a proxy thats linked into c.blend  */
							if (ob->id.lib == NULL)
								id_lib_extern((ID *)dtar->id);
						}
					}
				}
				DRIVER_TARGETS_LOOPER_END
			}
		}
	}
}

/* proxy rule: lib_object->proxy_from == the one we borrow from, set temporally while object_update */
/*             local_object->proxy == pointer to library object, saved in files and read */
/*             local_object->proxy_group == pointer to group dupli-object, saved in files and read */

void BKE_object_make_proxy(Object *ob, Object *target, Object *gob)
{
	/* paranoia checks */
	if (ob->id.lib || target->id.lib == NULL) {
		printf("cannot make proxy\n");
		return;
	}
	
	ob->proxy = target;
	ob->proxy_group = gob;
	id_lib_extern(&target->id);
	
	DAG_id_tag_update(&ob->id, OB_RECALC_OB | OB_RECALC_DATA | OB_RECALC_TIME);
	DAG_id_tag_update(&target->id, OB_RECALC_OB | OB_RECALC_DATA | OB_RECALC_TIME);
	
	/* copy transform
	 * - gob means this proxy comes from a group, just apply the matrix
	 *   so the object wont move from its dupli-transform.
	 *
	 * - no gob means this is being made from a linked object,
	 *   this is closer to making a copy of the object - in-place. */
	if (gob) {
		ob->rotmode = target->rotmode;
		mul_m4_m4m4(ob->obmat, gob->obmat, target->obmat);
		if (gob->dup_group) { /* should always be true */
			float tvec[3];
			copy_v3_v3(tvec, gob->dup_group->dupli_ofs);
			mul_mat3_m4_v3(ob->obmat, tvec);
			sub_v3_v3(ob->obmat[3], tvec);
		}
		BKE_object_apply_mat4(ob, ob->obmat, FALSE, TRUE);
	}
	else {
		BKE_object_transform_copy(ob, target);
		ob->parent = target->parent; /* libdata */
		copy_m4_m4(ob->parentinv, target->parentinv);
	}
	
	/* copy animdata stuff - drivers only for now... */
	BKE_object_copy_proxy_drivers(ob, target);

	/* skip constraints? */
	/* FIXME: this is considered by many as a bug */
	
	/* set object type and link to data */
	ob->type = target->type;
	ob->data = target->data;
	id_us_plus((ID *)ob->data);     /* ensures lib data becomes LIB_EXTERN */
	
	/* copy material and index information */
	ob->actcol = ob->totcol = 0;
	if (ob->mat) MEM_freeN(ob->mat);
	if (ob->matbits) MEM_freeN(ob->matbits);
	ob->mat = NULL;
	ob->matbits = NULL;
	if ((target->totcol) && (target->mat) && OB_TYPE_SUPPORT_MATERIAL(ob->type)) {
		int i;
		
		ob->actcol = target->actcol;
		ob->totcol = target->totcol;
		
		ob->mat = MEM_dupallocN(target->mat);
		ob->matbits = MEM_dupallocN(target->matbits);
		for (i = 0; i < target->totcol; i++) {
			/* don't need to run test_object_materials since we know this object is new and not used elsewhere */
			id_us_plus((ID *)ob->mat[i]); 
		}
	}
	
	/* type conversions */
	if (target->type == OB_ARMATURE) {
		copy_object_pose(ob, target);   /* data copy, object pointers in constraints */
		BKE_pose_rest(ob->pose);            /* clear all transforms in channels */
		BKE_pose_rebuild(ob, ob->data); /* set all internal links */
		
		armature_set_id_extern(ob);
	}
	else if (target->type == OB_EMPTY) {
		ob->empty_drawtype = target->empty_drawtype;
		ob->empty_drawsize = target->empty_drawsize;
	}

	/* copy IDProperties */
	if (ob->id.properties) {
		IDP_FreeProperty(ob->id.properties);
		MEM_freeN(ob->id.properties);
		ob->id.properties = NULL;
	}
	if (target->id.properties) {
		ob->id.properties = IDP_CopyProperty(target->id.properties);
	}

	/* copy drawtype info */
	ob->dt = target->dt;
}


/* *************** CALC ****************** */

void BKE_object_scale_to_mat3(Object *ob, float mat[3][3])
{
	float vec[3];
	mul_v3_v3v3(vec, ob->size, ob->dscale);
	size_to_mat3(mat, vec);
}

void BKE_object_rot_to_mat3(Object *ob, float mat[3][3], bool use_drot)
{
	float rmat[3][3], dmat[3][3];
	
	/* 'dmat' is the delta-rotation matrix, which will get (pre)multiplied
	 * with the rotation matrix to yield the appropriate rotation
	 */

	/* rotations may either be quats, eulers (with various rotation orders), or axis-angle */
	if (ob->rotmode > 0) {
		/* euler rotations (will cause gimble lock, but this can be alleviated a bit with rotation orders) */
		eulO_to_mat3(rmat, ob->rot, ob->rotmode);
		eulO_to_mat3(dmat, ob->drot, ob->rotmode);
	}
	else if (ob->rotmode == ROT_MODE_AXISANGLE) {
		/* axis-angle - not really that great for 3D-changing orientations */
		axis_angle_to_mat3(rmat, ob->rotAxis, ob->rotAngle);
		axis_angle_to_mat3(dmat, ob->drotAxis, ob->drotAngle);
	}
	else {
		/* quats are normalized before use to eliminate scaling issues */
		float tquat[4];
		
		normalize_qt_qt(tquat, ob->quat);
		quat_to_mat3(rmat, tquat);
		
		normalize_qt_qt(tquat, ob->dquat);
		quat_to_mat3(dmat, tquat);
	}
	
	/* combine these rotations */
	if (use_drot)
		mul_m3_m3m3(mat, dmat, rmat);
	else
		copy_m3_m3(mat, rmat);
}

void BKE_object_mat3_to_rot(Object *ob, float mat[3][3], bool use_compat)
{
	switch (ob->rotmode) {
		case ROT_MODE_QUAT:
		{
			float dquat[4];
			mat3_to_quat(ob->quat, mat);
			normalize_qt_qt(dquat, ob->dquat);
			invert_qt(dquat);
			mul_qt_qtqt(ob->quat, dquat, ob->quat);
			break;
		}
		case ROT_MODE_AXISANGLE:
		{
			mat3_to_axis_angle(ob->rotAxis, &ob->rotAngle, mat);
			sub_v3_v3(ob->rotAxis, ob->drotAxis);
			ob->rotAngle -= ob->drotAngle;
			break;
		}
		default: /* euler */
		{
			float quat[4];
			float dquat[4];
			float tmat[3][3];

			/* without drot we could apply 'mat' directly */
			mat3_to_quat(quat, mat);
			eulO_to_quat(dquat, ob->drot, ob->rotmode);
			invert_qt(dquat);
			mul_qt_qtqt(quat, dquat, quat);
			quat_to_mat3(tmat, quat);
			/* end drot correction */

			if (use_compat) mat3_to_compatible_eulO(ob->rot, ob->rot, ob->rotmode, tmat);
			else            mat3_to_eulO(ob->rot, ob->rotmode, tmat);
			break;
		}
	}
}

void BKE_object_tfm_protected_backup(const Object *ob,
                                     ObjectTfmProtectedChannels *obtfm)
{

#define TFMCPY(_v) (obtfm->_v = ob->_v)
#define TFMCPY3D(_v) copy_v3_v3(obtfm->_v, ob->_v)
#define TFMCPY4D(_v) copy_v4_v4(obtfm->_v, ob->_v)

	TFMCPY3D(loc);
	TFMCPY3D(dloc);
	TFMCPY3D(size);
	TFMCPY3D(dscale);
	TFMCPY3D(rot);
	TFMCPY3D(drot);
	TFMCPY4D(quat);
	TFMCPY4D(dquat);
	TFMCPY3D(rotAxis);
	TFMCPY3D(drotAxis);
	TFMCPY(rotAngle);
	TFMCPY(drotAngle);

#undef TFMCPY
#undef TFMCPY3D
#undef TFMCPY4D

}

void BKE_object_tfm_protected_restore(Object *ob,
                                      const ObjectTfmProtectedChannels *obtfm,
                                      const short protectflag)
{
	unsigned int i;

	for (i = 0; i < 3; i++) {
		if (protectflag & (OB_LOCK_LOCX << i)) {
			ob->loc[i] =  obtfm->loc[i];
			ob->dloc[i] = obtfm->dloc[i];
		}

		if (protectflag & (OB_LOCK_SCALEX << i)) {
			ob->size[i] =  obtfm->size[i];
			ob->dscale[i] = obtfm->dscale[i];
		}

		if (protectflag & (OB_LOCK_ROTX << i)) {
			ob->rot[i] =  obtfm->rot[i];
			ob->drot[i] = obtfm->drot[i];

			ob->quat[i + 1] =  obtfm->quat[i + 1];
			ob->dquat[i + 1] = obtfm->dquat[i + 1];

			ob->rotAxis[i] =  obtfm->rotAxis[i];
			ob->drotAxis[i] = obtfm->drotAxis[i];
		}
	}

	if ((protectflag & OB_LOCK_ROT4D) && (protectflag & OB_LOCK_ROTW)) {
		ob->quat[0] =  obtfm->quat[0];
		ob->dquat[0] = obtfm->dquat[0];

		ob->rotAngle =  obtfm->rotAngle;
		ob->drotAngle = obtfm->drotAngle;
	}
}

void BKE_object_to_mat3(Object *ob, float mat[3][3]) /* no parent */
{
	float smat[3][3];
	float rmat[3][3];
	/*float q1[4];*/
	
	/* size */
	BKE_object_scale_to_mat3(ob, smat);

	/* rot */
	BKE_object_rot_to_mat3(ob, rmat, TRUE);
	mul_m3_m3m3(mat, rmat, smat);
}

void BKE_object_to_mat4(Object *ob, float mat[4][4])
{
	float tmat[3][3];
	
	BKE_object_to_mat3(ob, tmat);
	
	copy_m4_m3(mat, tmat);

	add_v3_v3v3(mat[3], ob->loc, ob->dloc);
}

void BKE_object_matrix_local_get(struct Object *ob, float mat[4][4])
{
	if (ob->parent) {
		float invmat[4][4]; /* for inverse of parent's matrix */
		invert_m4_m4(invmat, ob->parent->obmat);
		mul_m4_m4m4(mat, invmat, ob->obmat);
	}
	else {
		copy_m4_m4(mat, ob->obmat);
	}
}

/* extern */
int enable_cu_speed = 1;

static void ob_parcurve(Scene *scene, Object *ob, Object *par, float mat[4][4])
{
	Curve *cu;
	float vec[4], dir[3], quat[4], radius, ctime;
	float timeoffs = 0.0, sf_orig = 0.0;
	
	unit_m4(mat);
	
	cu = par->data;
	if (ELEM3(NULL, par->curve_cache, par->curve_cache->path, par->curve_cache->path->data)) /* only happens on reload file, but violates depsgraph still... fix! */
		BKE_displist_make_curveTypes(scene, par, 0);
	if (par->curve_cache->path == NULL) return;
	
	/* catch exceptions: feature for nla stride editing */
	if (ob->ipoflag & OB_DISABLE_PATH) {
		ctime = 0.0f;
	}
	/* catch exceptions: curve paths used as a duplicator */
	else if (enable_cu_speed) {
		/* ctime is now a proper var setting of Curve which gets set by Animato like any other var that's animated,
		 * but this will only work if it actually is animated... 
		 *
		 * we divide the curvetime calculated in the previous step by the length of the path, to get a time
		 * factor, which then gets clamped to lie within 0.0 - 1.0 range
		 */
		if (IS_EQF(cu->pathlen, 0.0f) == 0)
			ctime = cu->ctime / cu->pathlen;
		else
			ctime = cu->ctime;

		CLAMP(ctime, 0.0f, 1.0f);
	}
	else {
		ctime = BKE_scene_frame_get(scene);
		if (IS_EQF(cu->pathlen, 0.0f) == 0)
			ctime /= cu->pathlen;
		
		CLAMP(ctime, 0.0f, 1.0f);
	}
	
	/* time calculus is correct, now apply distance offset */
	if (cu->flag & CU_OFFS_PATHDIST) {
		ctime += timeoffs / par->curve_cache->path->totdist;

		/* restore */
		SWAP(float, sf_orig, ob->sf);
	}
	
	
	/* vec: 4 items! */
	if (where_on_path(par, ctime, vec, dir, cu->flag & CU_FOLLOW ? quat : NULL, &radius, NULL)) {

		if (cu->flag & CU_FOLLOW) {
#if 0
			float si, q[4];
			vec_to_quat(quat, dir, ob->trackflag, ob->upflag);
			
			/* the tilt */
			normalize_v3(dir);
			q[0] = cosf(0.5 * vec[3]);
			si = sinf(0.5 * vec[3]);
			q[1] = -si * dir[0];
			q[2] = -si * dir[1];
			q[3] = -si * dir[2];
			mul_qt_qtqt(quat, q, quat);
#else
			quat_apply_track(quat, ob->trackflag, ob->upflag);
#endif
			normalize_qt(quat);
			quat_to_mat4(mat, quat);
		}
		
		if (cu->flag & CU_PATH_RADIUS) {
			float tmat[4][4], rmat[4][4];
			scale_m4_fl(tmat, radius);
			mul_m4_m4m4(rmat, tmat, mat);
			copy_m4_m4(mat, rmat);
		}

		copy_v3_v3(mat[3], vec);
		
	}
}

static void ob_parbone(Object *ob, Object *par, float mat[4][4])
{	
	bPoseChannel *pchan;
	float vec[3];
	
	if (par->type != OB_ARMATURE) {
		unit_m4(mat);
		return;
	}
	
	/* Make sure the bone is still valid */
	pchan = BKE_pose_channel_find_name(par->pose, ob->parsubstr);
	if (!pchan || !pchan->bone) {
		printf("Object %s with Bone parent: bone %s doesn't exist\n", ob->id.name + 2, ob->parsubstr);
		unit_m4(mat);
		return;
	}

	/* get bone transform */
	if (pchan->bone->flag & BONE_RELATIVE_PARENTING) {
		/* the new option uses the root - expected bahaviour, but differs from old... */
		/* XXX check on version patching? */
		copy_m4_m4(mat, pchan->chan_mat);
	}
	else {
		copy_m4_m4(mat, pchan->pose_mat);

		/* but for backwards compatibility, the child has to move to the tail */
		copy_v3_v3(vec, mat[1]);
		mul_v3_fl(vec, pchan->bone->length);
		add_v3_v3(mat[3], vec);
	}
}

static void give_parvert(Object *par, int nr, float vec[3])
{
	zero_v3(vec);
	
	if (par->type == OB_MESH) {
		Mesh *me = par->data;
		BMEditMesh *em = me->edit_btmesh;
		DerivedMesh *dm;

		dm = (em) ? em->derivedFinal : par->derivedFinal;
			
		if (dm) {
			int count = 0;
			int numVerts = dm->getNumVerts(dm);

			if (nr < numVerts) {
				/* avoid dm->getVertDataArray() since it allocates arrays in the dm (not thread safe) */
				int i;

				if (em && dm->type == DM_TYPE_EDITBMESH) {
					if (em->bm->elem_table_dirty & BM_VERT) {
#ifdef VPARENT_THREADING_HACK
						BLI_mutex_lock(&vparent_lock);
						if (em->bm->elem_table_dirty & BM_VERT) {
							BM_mesh_elem_table_ensure(em->bm, BM_VERT);
						}
						BLI_mutex_unlock(&vparent_lock);
#else
						BLI_assert(!"Not safe for threading");
						BM_mesh_elem_table_ensure(em->bm, BM_VERT);
#endif
					}
				}

				/* get the average of all verts with (original index == nr) */
				if (CustomData_has_layer(&dm->vertData, CD_ORIGINDEX)) {
					for (i = 0; i < numVerts; i++) {
						const int *index = dm->getVertData(dm, i, CD_ORIGINDEX);
						if (*index == nr) {
							float co[3];
							dm->getVertCo(dm, i, co);
							add_v3_v3(vec, co);
							count++;
						}
					}
				}
				else {
					if (nr < numVerts) {
						float co[3];
						dm->getVertCo(dm, nr, co);
						add_v3_v3(vec, co);
						count++;
					}
				}
			}

			if (count == 0) {
				/* keep as 0, 0, 0 */
			}
			else if (count > 0) {
				mul_v3_fl(vec, 1.0f / count);
			}
			else {
				/* use first index if its out of range */
				dm->getVertCo(dm, 0, vec);
			}
		}
		else {
			fprintf(stderr,
			        "%s: DerivedMesh is needed to solve parenting, "
			        "object position can be wrong now\n", __func__);
		}
	}
	else if (ELEM(par->type, OB_CURVE, OB_SURF)) {
		Curve *cu       = par->data;
		ListBase *nurb  = BKE_curve_nurbs_get(cu);

		BKE_nurbList_index_get_co(nurb, nr, vec);
	}
	else if (par->type == OB_LATTICE) {
		Lattice *latt  = par->data;
		DispList *dl   = par->curve_cache ? BKE_displist_find(&par->curve_cache->disp, DL_VERTS) : NULL;
		float (*co)[3] = dl ? (float (*)[3])dl->verts : NULL;
		int tot;

		if (latt->editlatt) latt = latt->editlatt->latt;

		tot = latt->pntsu * latt->pntsv * latt->pntsw;

		/* ensure dl is correct size */
		BLI_assert(dl == NULL || dl->nr == tot);

		if (nr < tot) {
			if (co) {
				copy_v3_v3(vec, co[nr]);
			}
			else {
				copy_v3_v3(vec, latt->def[nr].vec);
			}
		}
	}
}

static void ob_parvert3(Object *ob, Object *par, float mat[4][4])
{

	/* in local ob space */
	if (OB_TYPE_SUPPORT_PARVERT(par->type)) {
		float cmat[3][3], v1[3], v2[3], v3[3], q[4];

		give_parvert(par, ob->par1, v1);
		give_parvert(par, ob->par2, v2);
		give_parvert(par, ob->par3, v3);

		tri_to_quat(q, v1, v2, v3);
		quat_to_mat3(cmat, q);
		copy_m4_m3(mat, cmat);

		mid_v3_v3v3v3(mat[3], v1, v2, v3);
	}
	else {
		unit_m4(mat);
	}
}

static void ob_get_parent_matrix(Scene *scene, Object *ob, Object *par, float parentmat[4][4])
{
	float tmat[4][4];
	float vec[3];
	int ok;

	switch (ob->partype & PARTYPE) {
		case PAROBJECT:
			ok = 0;
			if (par->type == OB_CURVE) {
				if (scene && ((Curve *)par->data)->flag & CU_PATH) {
					ob_parcurve(scene, ob, par, tmat);
					ok = 1;
				}
			}
			
			if (ok) mul_m4_m4m4(parentmat, par->obmat, tmat);
			else copy_m4_m4(parentmat, par->obmat);
			
			break;
		case PARBONE:
			ob_parbone(ob, par, tmat);
			mul_m4_m4m4(parentmat, par->obmat, tmat);
			break;
		
		case PARVERT1:
			unit_m4(parentmat);
			give_parvert(par, ob->par1, vec);
			mul_v3_m4v3(parentmat[3], par->obmat, vec);
			break;
		case PARVERT3:
			ob_parvert3(ob, par, tmat);
			
			mul_m4_m4m4(parentmat, par->obmat, tmat);
			break;
		
		case PARSKEL:
			copy_m4_m4(parentmat, par->obmat);
			break;
	}

}

/**
 * \param r_originmat  Optional matrix that stores the space the object is in (without its own matrix applied)
 */
static void solve_parenting(Scene *scene, Object *ob, Object *par, float obmat[4][4], float slowmat[4][4],
                            float r_originmat[3][3], const bool set_origin)
{
	float totmat[4][4];
	float tmat[4][4];
	float locmat[4][4];
	
	BKE_object_to_mat4(ob, locmat);
	
	if (ob->partype & PARSLOW) copy_m4_m4(slowmat, obmat);

	ob_get_parent_matrix(scene, ob, par, totmat);
	
	/* total */
	mul_m4_m4m4(tmat, totmat, ob->parentinv);
	mul_m4_m4m4(obmat, tmat, locmat);
	
	if (r_originmat) {
		/* usable originmat */
		copy_m3_m4(r_originmat, tmat);
	}
	
	/* origin, for help line */
	if (set_origin) {
		if ((ob->partype & PARTYPE) == PARSKEL) {
			copy_v3_v3(ob->orig, par->obmat[3]);
		}
		else {
			copy_v3_v3(ob->orig, totmat[3]);
		}
	}
}

static int where_is_object_parslow(Object *ob, float obmat[4][4], float slowmat[4][4])
{
	float *fp1, *fp2;
	float fac1, fac2;
	int a;

	/* include framerate */
	fac1 = (1.0f / (1.0f + fabsf(ob->sf)) );
	if (fac1 >= 1.0f) return 0;
	fac2 = 1.0f - fac1;

	fp1 = obmat[0];
	fp2 = slowmat[0];
	for (a = 0; a < 16; a++, fp1++, fp2++) {
		fp1[0] = fac1 * fp1[0] + fac2 * fp2[0];
	}

	return 1;
}

/* note, scene is the active scene while actual_scene is the scene the object resides in */
void BKE_object_where_is_calc_time_ex(Scene *scene, Object *ob, float ctime,
                                      RigidBodyWorld *rbw, float r_originmat[3][3])
{
	if (ob == NULL) return;
	
	/* execute drivers only, as animation has already been done */
	BKE_animsys_evaluate_animdata(scene, &ob->id, ob->adt, ctime, ADT_RECALC_DRIVERS);
	
	if (ob->parent) {
		Object *par = ob->parent;
		float slowmat[4][4] = MAT4_UNITY;
		
		/* calculate parent matrix */
		solve_parenting(scene, ob, par, ob->obmat, slowmat, r_originmat, true);
		
		/* "slow parent" is definitely not threadsafe, and may also give bad results jumping around 
		 * An old-fashioned hack which probably doesn't really cut it anymore
		 */
		if (ob->partype & PARSLOW) {
			if (!where_is_object_parslow(ob, ob->obmat, slowmat))
				return;
		}
	}
	else {
		BKE_object_to_mat4(ob, ob->obmat);
	}

	/* try to fall back to the scene rigid body world if none given */
	rbw = rbw ? rbw : scene->rigidbody_world;
	/* read values pushed into RBO from sim/cache... */
	BKE_rigidbody_sync_transforms(rbw, ob, ctime);
	
	/* solve constraints */
	if (ob->constraints.first && !(ob->transflag & OB_NO_CONSTRAINTS)) {
		bConstraintOb *cob;
		cob = BKE_constraints_make_evalob(scene, ob, NULL, CONSTRAINT_OBTYPE_OBJECT);
		BKE_solve_constraints(&ob->constraints, cob, ctime);
		BKE_constraints_clear_evalob(cob);
	}
	
	/* set negative scale flag in object */
	if (is_negative_m4(ob->obmat)) ob->transflag |= OB_NEG_SCALE;
	else ob->transflag &= ~OB_NEG_SCALE;
}

void BKE_object_where_is_calc_time(Scene *scene, Object *ob, float ctime)
{
	BKE_object_where_is_calc_time_ex(scene, ob, ctime, NULL, NULL);
}

/* get object transformation matrix without recalculating dependencies and
 * constraints -- assume dependencies are already solved by depsgraph.
 * no changes to object and it's parent would be done.
 * used for bundles orientation in 3d space relative to parented blender camera */
void BKE_object_where_is_calc_mat4(Scene *scene, Object *ob, float obmat[4][4])
{
	float slowmat[4][4] = MAT4_UNITY;

	if (ob->parent) {
		Object *par = ob->parent;
		
		solve_parenting(scene, ob, par, obmat, slowmat, NULL, false);
		
		if (ob->partype & PARSLOW)
			where_is_object_parslow(ob, obmat, slowmat);
	}
	else {
		BKE_object_to_mat4(ob, obmat);
	}
}

void BKE_object_where_is_calc_ex(Scene *scene, RigidBodyWorld *rbw, Object *ob, float r_originmat[3][3])
{
	BKE_object_where_is_calc_time_ex(scene, ob, BKE_scene_frame_get(scene), rbw, r_originmat);
}
void BKE_object_where_is_calc(Scene *scene, Object *ob)
{
	BKE_object_where_is_calc_time_ex(scene, ob, BKE_scene_frame_get(scene), NULL, NULL);
}

/* for calculation of the inverse parent transform, only used for editor */
void BKE_object_workob_calc_parent(Scene *scene, Object *ob, Object *workob)
{
	BKE_object_workob_clear(workob);
	
	unit_m4(workob->obmat);
	unit_m4(workob->parentinv);
	unit_m4(workob->constinv);
	workob->parent = ob->parent;

	workob->trackflag = ob->trackflag;
	workob->upflag = ob->upflag;
	
	workob->partype = ob->partype;
	workob->par1 = ob->par1;
	workob->par2 = ob->par2;
	workob->par3 = ob->par3;

	workob->constraints.first = ob->constraints.first;
	workob->constraints.last = ob->constraints.last;

	BLI_strncpy(workob->parsubstr, ob->parsubstr, sizeof(workob->parsubstr));

	BKE_object_where_is_calc(scene, workob);
}

/* see BKE_pchan_apply_mat4() for the equivalent 'pchan' function */
void BKE_object_apply_mat4(Object *ob, float mat[4][4], const bool use_compat, const bool use_parent)
{
	float rot[3][3];

	if (use_parent && ob->parent) {
		float rmat[4][4], diff_mat[4][4], imat[4][4], parent_mat[4][4];

		ob_get_parent_matrix(NULL, ob, ob->parent, parent_mat);

		mul_m4_m4m4(diff_mat, parent_mat, ob->parentinv);
		invert_m4_m4(imat, diff_mat);
		mul_m4_m4m4(rmat, imat, mat); /* get the parent relative matrix */
		BKE_object_apply_mat4(ob, rmat, use_compat, FALSE);

		/* same as below, use rmat rather than mat */
		mat4_to_loc_rot_size(ob->loc, rot, ob->size, rmat);
		BKE_object_mat3_to_rot(ob, rot, use_compat);
	}
	else {
		mat4_to_loc_rot_size(ob->loc, rot, ob->size, mat);
		BKE_object_mat3_to_rot(ob, rot, use_compat);
	}

	sub_v3_v3(ob->loc, ob->dloc);

	if (ob->dscale[0] != 0.0f) ob->size[0] /= ob->dscale[0];
	if (ob->dscale[1] != 0.0f) ob->size[1] /= ob->dscale[1];
	if (ob->dscale[2] != 0.0f) ob->size[2] /= ob->dscale[2];

	/* BKE_object_mat3_to_rot handles delta rotations */
}

BoundBox *BKE_boundbox_alloc_unit(void)
{
	BoundBox *bb;
	const float min[3] = {-1.0f, -1.0f, -1.0f}, max[3] = {-1.0f, -1.0f, -1.0f};

	bb = MEM_callocN(sizeof(BoundBox), "OB-BoundBox");
	BKE_boundbox_init_from_minmax(bb, min, max);
	
	return bb;
}

void BKE_boundbox_init_from_minmax(BoundBox *bb, const float min[3], const float max[3])
{
	bb->vec[0][0] = bb->vec[1][0] = bb->vec[2][0] = bb->vec[3][0] = min[0];
	bb->vec[4][0] = bb->vec[5][0] = bb->vec[6][0] = bb->vec[7][0] = max[0];
	
	bb->vec[0][1] = bb->vec[1][1] = bb->vec[4][1] = bb->vec[5][1] = min[1];
	bb->vec[2][1] = bb->vec[3][1] = bb->vec[6][1] = bb->vec[7][1] = max[1];

	bb->vec[0][2] = bb->vec[3][2] = bb->vec[4][2] = bb->vec[7][2] = min[2];
	bb->vec[1][2] = bb->vec[2][2] = bb->vec[5][2] = bb->vec[6][2] = max[2];
}

BoundBox *BKE_object_boundbox_get(Object *ob)
{
	BoundBox *bb = NULL;
	
	if (ob->type == OB_MESH) {
		bb = BKE_mesh_boundbox_get(ob);
	}
	else if (ELEM3(ob->type, OB_CURVE, OB_SURF, OB_FONT)) {
		bb = BKE_curve_boundbox_get(ob);
	}
	else if (ob->type == OB_MBALL) {
		bb = ob->bb;
	}
	return bb;
}

/* used to temporally disable/enable boundbox */
void BKE_object_boundbox_flag(Object *ob, int flag, int set)
{
	BoundBox *bb = BKE_object_boundbox_get(ob);
	if (bb) {
		if (set) bb->flag |= flag;
		else bb->flag &= ~flag;
	}
}

void BKE_object_dimensions_get(Object *ob, float vec[3])
{
	BoundBox *bb = NULL;
	
	bb = BKE_object_boundbox_get(ob);
	if (bb) {
		float scale[3];
		
		mat4_to_size(scale, ob->obmat);
		
		vec[0] = fabsf(scale[0]) * (bb->vec[4][0] - bb->vec[0][0]);
		vec[1] = fabsf(scale[1]) * (bb->vec[2][1] - bb->vec[0][1]);
		vec[2] = fabsf(scale[2]) * (bb->vec[1][2] - bb->vec[0][2]);
	}
	else {
		zero_v3(vec);
	}
}

void BKE_object_dimensions_set(Object *ob, const float *value)
{
	BoundBox *bb = NULL;
	
	bb = BKE_object_boundbox_get(ob);
	if (bb) {
		float scale[3], len[3];
		
		mat4_to_size(scale, ob->obmat);
		
		len[0] = bb->vec[4][0] - bb->vec[0][0];
		len[1] = bb->vec[2][1] - bb->vec[0][1];
		len[2] = bb->vec[1][2] - bb->vec[0][2];
		
		if (len[0] > 0.f) ob->size[0] = value[0] / len[0];
		if (len[1] > 0.f) ob->size[1] = value[1] / len[1];
		if (len[2] > 0.f) ob->size[2] = value[2] / len[2];
	}
}

void BKE_object_minmax(Object *ob, float min_r[3], float max_r[3], const bool use_hidden)
{
	BoundBox bb;
	float vec[3];
	int a;
	bool changed = false;
	
	switch (ob->type) {
		case OB_CURVE:
		case OB_FONT:
		case OB_SURF:
		{
			bb = *BKE_curve_boundbox_get(ob);

			for (a = 0; a < 8; a++) {
				mul_m4_v3(ob->obmat, bb.vec[a]);
				minmax_v3v3_v3(min_r, max_r, bb.vec[a]);
			}
			changed = true;
			break;
		}
		case OB_LATTICE:
		{
			Lattice *lt = ob->data;
			BPoint *bp = lt->def;
			int u, v, w;

			for (w = 0; w < lt->pntsw; w++) {
				for (v = 0; v < lt->pntsv; v++) {
					for (u = 0; u < lt->pntsu; u++, bp++) {
						mul_v3_m4v3(vec, ob->obmat, bp->vec);
						minmax_v3v3_v3(min_r, max_r, vec);
					}
				}
			}
			changed = true;
			break;
		}
		case OB_ARMATURE:
		{
			if (ob->pose) {
				bArmature *arm = ob->data;
				bPoseChannel *pchan;

				for (pchan = ob->pose->chanbase.first; pchan; pchan = pchan->next) {
					/* XXX pchan->bone may be NULL for duplicated bones, see duplicateEditBoneObjects() comment
					 *     (editarmature.c:2592)... Skip in this case too! */
					if (pchan->bone && !((use_hidden == FALSE) && (PBONE_VISIBLE(arm, pchan->bone) == FALSE))) {
						mul_v3_m4v3(vec, ob->obmat, pchan->pose_head);
						minmax_v3v3_v3(min_r, max_r, vec);
						mul_v3_m4v3(vec, ob->obmat, pchan->pose_tail);
						minmax_v3v3_v3(min_r, max_r, vec);

						changed = true;
					}
				}
			}
			break;
		}
		case OB_MESH:
		{
			Mesh *me = BKE_mesh_from_object(ob);

			if (me) {
				bb = *BKE_mesh_boundbox_get(ob);

				for (a = 0; a < 8; a++) {
					mul_m4_v3(ob->obmat, bb.vec[a]);
					minmax_v3v3_v3(min_r, max_r, bb.vec[a]);
				}
				changed = true;
			}
			break;
		}
		case OB_MBALL:
		{
			float ob_min[3], ob_max[3];

			changed = BKE_mball_minmax_ex(ob->data, ob_min, ob_max, ob->obmat, 0);
			if (changed) {
				minmax_v3v3_v3(min_r, max_r, ob_min);
				minmax_v3v3_v3(min_r, max_r, ob_max);
			}
			break;
		}
	}

	if (changed == false) {
		float size[3];

		copy_v3_v3(size, ob->size);
		if (ob->type == OB_EMPTY) {
			mul_v3_fl(size, ob->empty_drawsize);
		}

		minmax_v3v3_v3(min_r, max_r, ob->obmat[3]);

		copy_v3_v3(vec, ob->obmat[3]);
		add_v3_v3(vec, size);
		minmax_v3v3_v3(min_r, max_r, vec);

		copy_v3_v3(vec, ob->obmat[3]);
		sub_v3_v3(vec, size);
		minmax_v3v3_v3(min_r, max_r, vec);
	}
}

bool BKE_object_minmax_dupli(Scene *scene, Object *ob, float r_min[3], float r_max[3], const bool use_hidden)
{
	bool ok = false;
	if ((ob->transflag & OB_DUPLI) == 0) {
		return ok;
	}
	else {
		ListBase *lb;
		DupliObject *dob;
		lb = object_duplilist(G.main->eval_ctx, scene, ob);
		for (dob = lb->first; dob; dob = dob->next) {
			if ((use_hidden == false) && (dob->no_draw != 0)) {
				/* pass */
			}
			else {
				BoundBox *bb = BKE_object_boundbox_get(dob->ob);

				if (bb) {
					int i;
					for (i = 0; i < 8; i++) {
						float vec[3];
						mul_v3_m4v3(vec, dob->mat, bb->vec[i]);
						minmax_v3v3_v3(r_min, r_max, vec);
					}

					ok = true;
				}
			}
		}
		free_object_duplilist(lb);  /* does restore */
	}

	return ok;
}

void BKE_object_foreach_display_point(
        Object *ob, float obmat[4][4],
        void (*func_cb)(const float[3], void *), void *user_data)
{
	float co[3];

	if (ob->derivedFinal) {
		DerivedMesh *dm = ob->derivedFinal;
		MVert *mv = dm->getVertArray(dm);
		int totvert = dm->getNumVerts(dm);
		int i;

		for (i = 0; i < totvert; i++, mv++) {
			mul_v3_m4v3(co, obmat, mv->co);
			func_cb(co, user_data);
		}
	}
	else if (ob->curve_cache && ob->curve_cache->disp.first) {
		DispList *dl;

		for (dl = ob->curve_cache->disp.first; dl; dl = dl->next) {
			float *v3 = dl->verts;
			int totvert = dl->nr;
			int i;

			for (i = 0; i < totvert; i++, v3 += 3) {
				mul_v3_m4v3(co, obmat, v3);
				func_cb(co, user_data);
			}
		}
	}
}

void BKE_scene_foreach_display_point(
        Scene *scene, View3D *v3d, const short flag,
        void (*func_cb)(const float[3], void *), void *user_data)
{
	Base *base;
	Object *ob;

	for (base = FIRSTBASE; base; base = base->next) {
		if (BASE_VISIBLE_BGMODE(v3d, scene, base) && (base->flag & flag) == flag) {
			ob = base->object;

			if ((ob->transflag & OB_DUPLI) == 0) {
				BKE_object_foreach_display_point(ob, ob->obmat, func_cb, user_data);
			}
			else {
				ListBase *lb;
				DupliObject *dob;

				lb = object_duplilist(G.main->eval_ctx, scene, ob);
				for (dob = lb->first; dob; dob = dob->next) {
					if (dob->no_draw == 0) {
						BKE_object_foreach_display_point(dob->ob, dob->mat, func_cb, user_data);
					}
				}
				free_object_duplilist(lb);  /* does restore */
			}
		}
	}
}

/* copied from DNA_object_types.h */
typedef struct ObTfmBack {
	float loc[3], dloc[3], orig[3];
	float size[3], dscale[3];   /* scale and delta scale */
	float rot[3], drot[3];      /* euler rotation */
	float quat[4], dquat[4];    /* quaternion rotation */
	float rotAxis[3], drotAxis[3];  /* axis angle rotation - axis part */
	float rotAngle, drotAngle;  /* axis angle rotation - angle part */
	float obmat[4][4];      /* final worldspace matrix with constraints & animsys applied */
	float parentinv[4][4]; /* inverse result of parent, so that object doesn't 'stick' to parent */
	float constinv[4][4]; /* inverse result of constraints. doesn't include effect of parent or object local transform */
	float imat[4][4];   /* inverse matrix of 'obmat' for during render, old game engine, temporally: ipokeys of transform  */
} ObTfmBack;

void *BKE_object_tfm_backup(Object *ob)
{
	ObTfmBack *obtfm = MEM_mallocN(sizeof(ObTfmBack), "ObTfmBack");
	copy_v3_v3(obtfm->loc, ob->loc);
	copy_v3_v3(obtfm->dloc, ob->dloc);
	copy_v3_v3(obtfm->orig, ob->orig);
	copy_v3_v3(obtfm->size, ob->size);
	copy_v3_v3(obtfm->dscale, ob->dscale);
	copy_v3_v3(obtfm->rot, ob->rot);
	copy_v3_v3(obtfm->drot, ob->drot);
	copy_qt_qt(obtfm->quat, ob->quat);
	copy_qt_qt(obtfm->dquat, ob->dquat);
	copy_v3_v3(obtfm->rotAxis, ob->rotAxis);
	copy_v3_v3(obtfm->drotAxis, ob->drotAxis);
	obtfm->rotAngle = ob->rotAngle;
	obtfm->drotAngle = ob->drotAngle;
	copy_m4_m4(obtfm->obmat, ob->obmat);
	copy_m4_m4(obtfm->parentinv, ob->parentinv);
	copy_m4_m4(obtfm->constinv, ob->constinv);
	copy_m4_m4(obtfm->imat, ob->imat);

	return (void *)obtfm;
}

void BKE_object_tfm_restore(Object *ob, void *obtfm_pt)
{
	ObTfmBack *obtfm = (ObTfmBack *)obtfm_pt;
	copy_v3_v3(ob->loc, obtfm->loc);
	copy_v3_v3(ob->dloc, obtfm->dloc);
	copy_v3_v3(ob->orig, obtfm->orig);
	copy_v3_v3(ob->size, obtfm->size);
	copy_v3_v3(ob->dscale, obtfm->dscale);
	copy_v3_v3(ob->rot, obtfm->rot);
	copy_v3_v3(ob->drot, obtfm->drot);
	copy_qt_qt(ob->quat, obtfm->quat);
	copy_qt_qt(ob->dquat, obtfm->dquat);
	copy_v3_v3(ob->rotAxis, obtfm->rotAxis);
	copy_v3_v3(ob->drotAxis, obtfm->drotAxis);
	ob->rotAngle = obtfm->rotAngle;
	ob->drotAngle = obtfm->drotAngle;
	copy_m4_m4(ob->obmat, obtfm->obmat);
	copy_m4_m4(ob->parentinv, obtfm->parentinv);
	copy_m4_m4(ob->constinv, obtfm->constinv);
	copy_m4_m4(ob->imat, obtfm->imat);
}

bool BKE_object_parent_loop_check(const Object *par, const Object *ob)
{
	/* test if 'ob' is a parent somewhere in par's parents */
	if (par == NULL) return false;
	if (ob == par) return true;
	return BKE_object_parent_loop_check(par->parent, ob);
}

/* proxy rule: lib_object->proxy_from == the one we borrow from, only set temporal and cleared here */
/*           local_object->proxy      == pointer to library object, saved in files and read */

/* function below is polluted with proxy exceptions, cleanup will follow! */

/* the main object update call, for object matrix, constraints, keys and displist (modifiers) */
/* requires flags to be set! */
/* Ideally we shouldn't have to pass the rigid body world, but need bigger restructuring to avoid id */
void BKE_object_handle_update_ex(EvaluationContext *eval_ctx,
                                 Scene *scene, Object *ob,
                                 RigidBodyWorld *rbw)
{
	if (ob->recalc & OB_RECALC_ALL) {
		/* speed optimization for animation lookups */
		if (ob->pose)
			BKE_pose_channels_hash_make(ob->pose);

		if (ob->recalc & OB_RECALC_DATA) {
			if (ob->type == OB_ARMATURE) {
				/* this happens for reading old files and to match library armatures
				 * with poses we do it ahead of BKE_object_where_is_calc to ensure animation
				 * is evaluated on the rebuilt pose, otherwise we get incorrect poses
				 * on file load */
				if (ob->pose == NULL || (ob->pose->flag & POSE_RECALC))
					BKE_pose_rebuild(ob, ob->data);
			}
		}

		/* XXX new animsys warning: depsgraph tag OB_RECALC_DATA should not skip drivers, 
		 * which is only in BKE_object_where_is_calc now */
		/* XXX: should this case be OB_RECALC_OB instead? */
		if (ob->recalc & OB_RECALC_ALL) {
			
			if (G.debug & G_DEBUG)
				printf("recalcob %s\n", ob->id.name + 2);
			
			/* handle proxy copy for target */
			if (ob->id.lib && ob->proxy_from) {
				// printf("ob proxy copy, lib ob %s proxy %s\n", ob->id.name, ob->proxy_from->id.name);
				if (ob->proxy_from->proxy_group) { /* transform proxy into group space */
					Object *obg = ob->proxy_from->proxy_group;
					invert_m4_m4(obg->imat, obg->obmat);
					mul_m4_m4m4(ob->obmat, obg->imat, ob->proxy_from->obmat);
					if (obg->dup_group) { /* should always be true */
						add_v3_v3(ob->obmat[3], obg->dup_group->dupli_ofs);
					}
				}
				else
					copy_m4_m4(ob->obmat, ob->proxy_from->obmat);
			}
			else
				BKE_object_where_is_calc_ex(scene, rbw, ob, NULL);
		}
		
		if (ob->recalc & OB_RECALC_DATA) {
			ID *data_id = (ID *)ob->data;
			AnimData *adt = BKE_animdata_from_id(data_id);
			Key *key;
			float ctime = BKE_scene_frame_get(scene);
			
			if (G.debug & G_DEBUG)
				printf("recalcdata %s\n", ob->id.name + 2);

			if (adt) {
				/* evaluate drivers - datalevel */
				/* XXX: for mesh types, should we push this to derivedmesh instead? */
				BKE_animsys_evaluate_animdata(scene, data_id, adt, ctime, ADT_RECALC_DRIVERS);
			}
			
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
					if (em) {
						makeDerivedMesh(scene, ob, em,  data_mask, 0); /* was CD_MASK_BAREMESH */
					}
					else {
						makeDerivedMesh(scene, ob, NULL, data_mask, 0);
					}
					break;
				}
				case OB_ARMATURE:
					if (ob->id.lib && ob->proxy_from) {
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
			}
			
			/* related materials */
			/* XXX: without depsgraph tagging, this will always need to be run, which will be slow! 
			 * However, not doing anything (or trying to hack around this lack) is not an option 
			 * anymore, especially due to Cycles [#31834] 
			 */
			if (ob->totcol) {
				int a;
				
				for (a = 1; a <= ob->totcol; a++) {
					Material *ma = give_current_material(ob, a);
					
					if (ma) {
						/* recursively update drivers for this material */
						material_drivers_update(scene, ma, ctime);
					}
				}
			}
			else if (ob->type == OB_LAMP)
				lamp_drivers_update(scene, ob->data, ctime);
			
			/* particles */
			if (ob->particlesystem.first) {
				ParticleSystem *tpsys, *psys;
				DerivedMesh *dm;
				ob->transflag &= ~OB_DUPLIPARTS;
				
				psys = ob->particlesystem.first;
				while (psys) {
					if (psys_check_enabled(ob, psys)) {
						/* check use of dupli objects here */
						if (psys->part && (psys->part->draw_as == PART_DRAW_REND || eval_ctx->for_render) &&
						    ((psys->part->ren_as == PART_DRAW_OB && psys->part->dup_ob) ||
						     (psys->part->ren_as == PART_DRAW_GR && psys->part->dup_group)))
						{
							ob->transflag |= OB_DUPLIPARTS;
						}

						particle_system_update(scene, ob, psys);
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

				if (eval_ctx->for_render && ob->transflag & OB_DUPLIPARTS) {
					/* this is to make sure we get render level duplis in groups:
					 * the derivedmesh must be created before init_render_mesh,
					 * since object_duplilist does dupliparticles before that */
					dm = mesh_create_derived_render(scene, ob, CD_MASK_BAREMESH | CD_MASK_MTFACE | CD_MASK_MCOL);
					dm->release(dm);

					for (psys = ob->particlesystem.first; psys; psys = psys->next)
						psys_get_modifier(ob, psys)->flag &= ~eParticleSystemFlag_psys_updated;
				}
			}
			
			/* quick cache removed */
		}

		ob->recalc &= ~OB_RECALC_ALL;
	}

	/* the case when this is a group proxy, object_update is called in group.c */
	if (ob->proxy) {
		/* set pointer in library proxy target, for copying, but restore it */
		ob->proxy->proxy_from = ob;
		// printf("set proxy pointer for later group stuff %s\n", ob->id.name);

		/* the no-group proxy case, we call update */
		if (ob->proxy_group == NULL) {
			// printf("call update, lib ob %s proxy %s\n", ob->proxy->id.name, ob->id.name);
			BKE_object_handle_update(eval_ctx, scene, ob->proxy);
		}
	}
}
/* WARNING: "scene" here may not be the scene object actually resides in. 
 * When dealing with background-sets, "scene" is actually the active scene.
 * e.g. "scene" <-- set 1 <-- set 2 ("ob" lives here) <-- set 3 <-- ... <-- set n
 * rigid bodies depend on their world so use BKE_object_handle_update_ex() to also pass along the corrent rigid body world
 */
void BKE_object_handle_update(EvaluationContext *eval_ctx, Scene *scene, Object *ob)
{
	BKE_object_handle_update_ex(eval_ctx, scene, ob, NULL);
}

void BKE_object_sculpt_modifiers_changed(Object *ob)
{
	SculptSession *ss = ob->sculpt;

	if (ss) {
		if (!ss->cache) {
			/* we free pbvh on changes, except during sculpt since it can't deal with
			 * changing PVBH node organization, we hope topology does not change in
			 * the meantime .. weak */
			if (ss->pbvh) {
				BKE_pbvh_free(ss->pbvh);
				ss->pbvh = NULL;
			}

			free_sculptsession_deformMats(ob->sculpt);
		}
		else {
			PBVHNode **nodes;
			int n, totnode;

			BKE_pbvh_search_gather(ss->pbvh, NULL, NULL, &nodes, &totnode);

			for (n = 0; n < totnode; n++)
				BKE_pbvh_node_mark_update(nodes[n]);

			MEM_freeN(nodes);
		}
	}
}

int BKE_object_obdata_texspace_get(Object *ob, short **r_texflag, float **r_loc, float **r_size, float **r_rot)
{
	
	if (ob->data == NULL)
		return 0;
	
	switch (GS(((ID *)ob->data)->name)) {
		case ID_ME:
		{
			Mesh *me = ob->data;
			if (r_texflag) *r_texflag = &me->texflag;
			if (r_loc) *r_loc = me->loc;
			if (r_size) *r_size = me->size;
			if (r_rot) *r_rot = me->rot;
			break;
		}
		case ID_CU:
		{
			Curve *cu = ob->data;
			if (r_texflag) *r_texflag = &cu->texflag;
			if (r_loc) *r_loc = cu->loc;
			if (r_size) *r_size = cu->size;
			if (r_rot) *r_rot = cu->rot;
			break;
		}
		case ID_MB:
		{
			MetaBall *mb = ob->data;
			if (r_texflag) *r_texflag = &mb->texflag;
			if (r_loc) *r_loc = mb->loc;
			if (r_size) *r_size = mb->size;
			if (r_rot) *r_rot = mb->rot;
			break;
		}
		default:
			return 0;
	}
	return 1;
}

/*
 * Test a bounding box for ray intersection
 * assumes the ray is already local to the boundbox space
 */
bool BKE_boundbox_ray_hit_check(struct BoundBox *bb, const float ray_start[3], const float ray_normal[3])
{
	const int triangle_indexes[12][3] = {
	    {0, 1, 2}, {0, 2, 3},
	    {3, 2, 6}, {3, 6, 7},
	    {1, 2, 6}, {1, 6, 5},
	    {5, 6, 7}, {4, 5, 7},
	    {0, 3, 7}, {0, 4, 7},
	    {0, 1, 5}, {0, 4, 5}};

	bool result = false;
	int i;
	
	for (i = 0; i < 12 && result == 0; i++) {
		float lambda;
		int v1, v2, v3;
		v1 = triangle_indexes[i][0];
		v2 = triangle_indexes[i][1];
		v3 = triangle_indexes[i][2];
		result = isect_ray_tri_v3(ray_start, ray_normal, bb->vec[v1], bb->vec[v2], bb->vec[v3], &lambda, NULL);
	}
	
	return result;
}

static int pc_cmp(void *a, void *b)
{
	LinkData *ad = a, *bd = b;
	if (GET_INT_FROM_POINTER(ad->data) > GET_INT_FROM_POINTER(bd->data))
		return 1;
	else return 0;
}

int BKE_object_insert_ptcache(Object *ob) 
{
	LinkData *link = NULL;
	int i = 0;

	BLI_sortlist(&ob->pc_ids, pc_cmp);

	for (link = ob->pc_ids.first, i = 0; link; link = link->next, i++) {
		int index = GET_INT_FROM_POINTER(link->data);

		if (i < index)
			break;
	}

	link = MEM_callocN(sizeof(LinkData), "PCLink");
	link->data = SET_INT_IN_POINTER(i);
	BLI_addtail(&ob->pc_ids, link);

	return i;
}

#if 0
static int pc_findindex(ListBase *listbase, int index)
{
	LinkData *link = NULL;
	int number = 0;
	
	if (listbase == NULL) return -1;
	
	link = listbase->first;
	while (link) {
		if ((int)link->data == index)
			return number;
		
		number++;
		link = link->next;
	}
	
	return -1;
}

void object_delete_ptcache(Object *ob, int index) 
{
	int list_index = pc_findindex(&ob->pc_ids, index);
	LinkData *link = BLI_findlink(&ob->pc_ids, list_index);
	BLI_freelinkN(&ob->pc_ids, link);
}
#endif

/* shape key utility function */

/************************* Mesh ************************/
static KeyBlock *insert_meshkey(Scene *scene, Object *ob, const char *name, int from_mix)
{
	Mesh *me = ob->data;
	Key *key = me->key;
	KeyBlock *kb;
	int newkey = 0;

	if (key == NULL) {
		key = me->key = BKE_key_add((ID *)me);
		key->type = KEY_RELATIVE;
		newkey = 1;
	}

	if (newkey || from_mix == FALSE) {
		/* create from mesh */
		kb = BKE_keyblock_add_ctime(key, name, FALSE);
		BKE_key_convert_from_mesh(me, kb);
	}
	else {
		/* copy from current values */
		int totelem;
		float *data = BKE_key_evaluate_object(scene, ob, &totelem);

		/* create new block with prepared data */
		kb = BKE_keyblock_add_ctime(key, name, FALSE);
		kb->data = data;
		kb->totelem = totelem;
	}

	return kb;
}
/************************* Lattice ************************/
static KeyBlock *insert_lattkey(Scene *scene, Object *ob, const char *name, int from_mix)
{
	Lattice *lt = ob->data;
	Key *key = lt->key;
	KeyBlock *kb;
	int newkey = 0;

	if (key == NULL) {
		key = lt->key = BKE_key_add((ID *)lt);
		key->type = KEY_RELATIVE;
		newkey = 1;
	}

	if (newkey || from_mix == FALSE) {
		kb = BKE_keyblock_add_ctime(key, name, FALSE);
		if (!newkey) {
			KeyBlock *basekb = (KeyBlock *)key->block.first;
			kb->data = MEM_dupallocN(basekb->data);
			kb->totelem = basekb->totelem;
		}
		else {
			BKE_key_convert_from_lattice(lt, kb);
		}
	}
	else {
		/* copy from current values */
		int totelem;
		float *data = BKE_key_evaluate_object(scene, ob, &totelem);

		/* create new block with prepared data */
		kb = BKE_keyblock_add_ctime(key, name, FALSE);
		kb->totelem = totelem;
		kb->data = data;
	}

	return kb;
}
/************************* Curve ************************/
static KeyBlock *insert_curvekey(Scene *scene, Object *ob, const char *name, int from_mix)
{
	Curve *cu = ob->data;
	Key *key = cu->key;
	KeyBlock *kb;
	ListBase *lb = BKE_curve_nurbs_get(cu);
	int newkey = 0;

	if (key == NULL) {
		key = cu->key = BKE_key_add((ID *)cu);
		key->type = KEY_RELATIVE;
		newkey = 1;
	}

	if (newkey || from_mix == FALSE) {
		/* create from curve */
		kb = BKE_keyblock_add_ctime(key, name, FALSE);
		if (!newkey) {
			KeyBlock *basekb = (KeyBlock *)key->block.first;
			kb->data = MEM_dupallocN(basekb->data);
			kb->totelem = basekb->totelem;
		}
		else {
			BKE_key_convert_from_curve(cu, kb, lb);
		}
	}
	else {
		/* copy from current values */
		int totelem;
		float *data = BKE_key_evaluate_object(scene, ob, &totelem);

		/* create new block with prepared data */
		kb = BKE_keyblock_add_ctime(key, name, FALSE);
		kb->totelem = totelem;
		kb->data = data;
	}

	return kb;
}

KeyBlock *BKE_object_insert_shape_key(Scene *scene, Object *ob, const char *name, int from_mix)
{	
	switch (ob->type) {
		case OB_MESH:
			return insert_meshkey(scene, ob, name, from_mix);
		case OB_CURVE:
		case OB_SURF:
			return insert_curvekey(scene, ob, name, from_mix);
		case OB_LATTICE:
			return insert_lattkey(scene, ob, name, from_mix);
		default:
			return NULL;
	}

}

bool BKE_object_is_child_recursive(Object *ob_parent, Object *ob_child)
{
	for (ob_child = ob_child->parent; ob_child; ob_child = ob_child->parent) {
		if (ob_child == ob_parent) {
			return true;
		}
	}
	return false;
}

/* most important if this is modified it should _always_ return True, in certain
 * cases false positives are hard to avoid (shape keys for example) */
int BKE_object_is_modified(Scene *scene, Object *ob)
{
	int flag = 0;

	if (BKE_key_from_object(ob)) {
		flag |= eModifierMode_Render;
	}
	else {
		ModifierData *md;
		VirtualModifierData virtualModifierData;
		/* cloth */
		for (md = modifiers_getVirtualModifierList(ob, &virtualModifierData);
		     md && (flag != (eModifierMode_Render | eModifierMode_Realtime));
		     md = md->next)
		{
			if ((flag & eModifierMode_Render) == 0 && modifier_isEnabled(scene, md, eModifierMode_Render))
				flag |= eModifierMode_Render;

			if ((flag & eModifierMode_Realtime) == 0 && modifier_isEnabled(scene, md, eModifierMode_Realtime))
				flag |= eModifierMode_Realtime;
		}
	}

	return flag;
}

/* test if object is affected by deforming modifiers (for motion blur). again
 * most important is to avoid false positives, this is to skip computations
 * and we can still if there was actual deformation afterwards */
int BKE_object_is_deform_modified(Scene *scene, Object *ob)
{
	ModifierData *md;
	VirtualModifierData virtualModifierData;
	int flag = 0;

	/* cloth */
	for (md = modifiers_getVirtualModifierList(ob, &virtualModifierData);
	     md && (flag != (eModifierMode_Render | eModifierMode_Realtime));
	     md = md->next)
	{
		ModifierTypeInfo *mti = modifierType_getInfo(md->type);

		if (mti->type == eModifierTypeType_OnlyDeform) {
			if (!(flag & eModifierMode_Render) && modifier_isEnabled(scene, md, eModifierMode_Render))
				flag |= eModifierMode_Render;

			if (!(flag & eModifierMode_Realtime) && modifier_isEnabled(scene, md, eModifierMode_Realtime))
				flag |= eModifierMode_Realtime;
		}
	}

	return flag;
}

/* See if an object is using an animated modifier */
bool BKE_object_is_animated(Scene *scene, Object *ob)
{
	ModifierData *md;
	VirtualModifierData virtualModifierData;

	for (md = modifiers_getVirtualModifierList(ob, &virtualModifierData); md; md = md->next)
		if (modifier_dependsOnTime(md) &&
		    (modifier_isEnabled(scene, md, eModifierMode_Realtime) ||
		     modifier_isEnabled(scene, md, eModifierMode_Render)))
		{
			return true;
		}
	return false;
}

static void copy_object__forwardModifierLinks(void *UNUSED(userData), Object *UNUSED(ob), ID **idpoin)
{
	/* this is copied from ID_NEW; it might be better to have a macro */
	if (*idpoin && (*idpoin)->newid) *idpoin = (*idpoin)->newid;
}

void BKE_object_relink(Object *ob)
{
	if (ob->id.lib)
		return;

	BKE_relink_constraints(&ob->constraints);
	if (ob->pose) {
		bPoseChannel *chan;
		for (chan = ob->pose->chanbase.first; chan; chan = chan->next) {
			BKE_relink_constraints(&chan->constraints);
		}
	}
	modifiers_foreachIDLink(ob, copy_object__forwardModifierLinks, NULL);

	if (ob->adt)
		BKE_relink_animdata(ob->adt);
	
	if (ob->rigidbody_constraint)
		BKE_rigidbody_relink_constraint(ob->rigidbody_constraint);

	ID_NEW(ob->parent);

	ID_NEW(ob->proxy);
	ID_NEW(ob->proxy_group);
}

MovieClip *BKE_object_movieclip_get(Scene *scene, Object *ob, bool use_default)
{
	MovieClip *clip = use_default ? scene->clip : NULL;
	bConstraint *con = ob->constraints.first, *scon = NULL;

	while (con) {
		if (con->type == CONSTRAINT_TYPE_CAMERASOLVER) {
			if (scon == NULL || (scon->flag & CONSTRAINT_OFF))
				scon = con;
		}

		con = con->next;
	}

	if (scon) {
		bCameraSolverConstraint *solver = scon->data;
		if ((solver->flag & CAMERASOLVER_ACTIVECLIP) == 0)
			clip = solver->clip;
		else
			clip = scene->clip;
	}

	return clip;
}


/*
 * Find an associated Armature object
 */
static Object *obrel_armature_find(Object *ob)
{
	Object *ob_arm = NULL;

	if (ob->parent && ob->partype == PARSKEL && ob->parent->type == OB_ARMATURE) {
		ob_arm = ob->parent;
	}
	else {
		ModifierData *mod;
		for (mod = (ModifierData *)ob->modifiers.first; mod; mod = mod->next) {
			if (mod->type == eModifierType_Armature) {
				ob_arm = ((ArmatureModifierData *)mod)->object;
			}
		}
	}

	return ob_arm;
}

static int obrel_list_test(Object *ob)
{
	return ob && !(ob->id.flag & LIB_DOIT);
}

static void obrel_list_add(LinkNode **links, Object *ob)
{
	BLI_linklist_prepend(links, ob);
	ob->id.flag |= LIB_DOIT;
}

/*
 * Iterates over all objects of the given scene.
 * Depending on the eObjectSet flag:
 * collect either OB_SET_ALL, OB_SET_VISIBLE or OB_SET_SELECTED objects.
 * If OB_SET_VISIBLE or OB_SET_SELECTED are collected, 
 * then also add related objects according to the given includeFilters.
 */
LinkNode *BKE_object_relational_superset(struct Scene *scene, eObjectSet objectSet, eObRelationTypes includeFilter)
{
	LinkNode *links = NULL;

	Base *base;

	/* Remove markers from all objects */
	for (base = scene->base.first; base; base = base->next) {
		base->object->id.flag &= ~LIB_DOIT;
	}

	/* iterate over all selected and visible objects */
	for (base = scene->base.first; base; base = base->next) {
		if (objectSet == OB_SET_ALL) {
			/* as we get all anyways just add it */
			Object *ob = base->object;
			obrel_list_add(&links, ob);
		}
		else {
			if ((objectSet == OB_SET_SELECTED && TESTBASELIB_BGMODE(((View3D *)NULL), scene, base)) ||
			    (objectSet == OB_SET_VISIBLE  && BASE_EDITABLE_BGMODE(((View3D *)NULL), scene, base)))
			{
				Object *ob = base->object;

				if (obrel_list_test(ob))
					obrel_list_add(&links, ob);

				/* parent relationship */
				if (includeFilter & (OB_REL_PARENT | OB_REL_PARENT_RECURSIVE)) {
					Object *parent = ob->parent;
					if (obrel_list_test(parent)) {

						obrel_list_add(&links, parent);

						/* recursive parent relationship */
						if (includeFilter & OB_REL_PARENT_RECURSIVE) {
							parent = parent->parent;
							while (obrel_list_test(parent)) {

								obrel_list_add(&links, parent);
								parent = parent->parent;
							}
						}
					}
				}

				/* child relationship */
				if (includeFilter & (OB_REL_CHILDREN | OB_REL_CHILDREN_RECURSIVE)) {
					Base *local_base;
					for (local_base = scene->base.first; local_base; local_base = local_base->next) {
						if (BASE_EDITABLE_BGMODE(((View3D *)NULL), scene, local_base)) {

							Object *child = local_base->object;
							if (obrel_list_test(child)) {
								if ((includeFilter & OB_REL_CHILDREN_RECURSIVE && BKE_object_is_child_recursive(ob, child)) ||
								    (includeFilter & OB_REL_CHILDREN && child->parent && child->parent == ob))
								{
									obrel_list_add(&links, child);
								}
							}
						}
					}
				}


				/* include related armatures */
				if (includeFilter & OB_REL_MOD_ARMATURE) {
					Object *arm = obrel_armature_find(ob);
					if (obrel_list_test(arm)) {
						obrel_list_add(&links, arm);
					}
				}

			}
		}
	}

	return links;
}

/**
 * return all groups this object is apart of, caller must free.
 */
struct LinkNode *BKE_object_groups(Object *ob)
{
	LinkNode *group_linknode = NULL;
	Group *group = NULL;
	while ((group = BKE_group_object_find(group, ob))) {
		BLI_linklist_prepend(&group_linknode, group);
	}

	return group_linknode;
}

void BKE_object_groups_clear(Scene *scene, Base *base, Object *object)
{
	Group *group = NULL;

	BLI_assert((base == NULL) || (base->object == object));

	if (scene && base == NULL) {
		base = BKE_scene_base_find(scene, object);
	}

	while ((group = BKE_group_object_find(group, base->object))) {
		BKE_group_object_unlink(group, object, scene, base);
	}
}

/**
 * Return a KDTree from the deformed object (in worldspace)
 *
 * \note Only mesh objects currently support deforming, others are TODO.
 *
 * \param ob
 * \param r_tot
 * \return The kdtree or NULL if it can't be created.
 */
KDTree *BKE_object_as_kdtree(Object *ob, int *r_tot)
{
	KDTree *tree = NULL;
	unsigned int tot = 0;

	switch (ob->type) {
		case OB_MESH:
		{
			Mesh *me = ob->data;
			unsigned int i;

			DerivedMesh *dm = ob->derivedDeform ? ob->derivedDeform : ob->derivedFinal;
			int *index;

			if (dm && (index = CustomData_get_layer(&dm->vertData, CD_ORIGINDEX))) {
				MVert *mvert = dm->getVertArray(dm);
				unsigned int totvert = dm->getNumVerts(dm);

				/* tree over-allocs in case where some verts have ORIGINDEX_NONE */
				tot = 0;
				tree = BLI_kdtree_new(totvert);

				/* we don't how how many verts from the DM we can use */
				for (i = 0; i < totvert; i++) {
					if (index[i] != ORIGINDEX_NONE) {
						float co[3];
						mul_v3_m4v3(co, ob->obmat, mvert[i].co);
						BLI_kdtree_insert(tree, index[i], co, NULL);
						tot++;
					}
				}
			}
			else {
				MVert *mvert = me->mvert;

				tot = me->totvert;
				tree = BLI_kdtree_new(tot);

				for (i = 0; i < tot; i++) {
					float co[3];
					mul_v3_m4v3(co, ob->obmat, mvert[i].co);
					BLI_kdtree_insert(tree, i, co, NULL);
				}
			}

			BLI_kdtree_balance(tree);
			break;
		}
		case OB_CURVE:
		case OB_SURF:
		{
			/* TODO: take deformation into account */
			Curve *cu = ob->data;
			unsigned int i, a;

			Nurb *nu;

			tot = BKE_nurbList_verts_count_without_handles(&cu->nurb);
			tree = BLI_kdtree_new(tot);
			i = 0;

			nu = cu->nurb.first;
			while (nu) {
				if (nu->bezt) {
					BezTriple *bezt;

					bezt = nu->bezt;
					a = nu->pntsu;
					while (a--) {
						float co[3];
						mul_v3_m4v3(co, ob->obmat, bezt->vec[1]);
						BLI_kdtree_insert(tree, i++, co, NULL);
						bezt++;
					}
				}
				else {
					BPoint *bp;

					bp = nu->bp;
					a = nu->pntsu * nu->pntsv;
					while (a--) {
						float co[3];
						mul_v3_m4v3(co, ob->obmat, bp->vec);
						BLI_kdtree_insert(tree, i++, co, NULL);
						bp++;
					}
				}
				nu = nu->next;
			}

			BLI_kdtree_balance(tree);
			break;
		}
		case OB_LATTICE:
		{
			/* TODO: take deformation into account */
			Lattice *lt = ob->data;
			BPoint *bp;
			unsigned int i;

			tot = lt->pntsu * lt->pntsv * lt->pntsw;
			tree = BLI_kdtree_new(tot);
			i = 0;

			for (bp = lt->def; i < tot; bp++) {
				float co[3];
				mul_v3_m4v3(co, ob->obmat, bp->vec);
				BLI_kdtree_insert(tree, i++, co, NULL);
			}

			BLI_kdtree_balance(tree);
			break;
		}
	}

	*r_tot = tot;
	return tree;
}
