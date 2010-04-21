/* object.c
 *
 * 
 * $Id$
 *
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
#include "DNA_lamp_types.h"
#include "DNA_lattice_types.h"
#include "DNA_material_types.h"
#include "DNA_meta_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_sequence_types.h"
#include "DNA_space_types.h"
#include "DNA_view3d_types.h"
#include "DNA_world_types.h"

#include "BLI_blenlib.h"
#include "BLI_editVert.h"
#include "BLI_math.h"
#include "BLI_pbvh.h"

#include "BKE_utildefines.h"

#include "BKE_main.h"
#include "BKE_global.h"

#include "BKE_armature.h"
#include "BKE_action.h"
#include "BKE_bullet.h"
#include "BKE_colortools.h"
#include "BKE_deform.h"
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
#include "BKE_lattice.h"
#include "BKE_library.h"
#include "BKE_mesh.h"
#include "BKE_mball.h"
#include "BKE_modifier.h"
#include "BKE_object.h"
#include "BKE_paint.h"
#include "BKE_particle.h"
#include "BKE_pointcache.h"
#include "BKE_property.h"
#include "BKE_sca.h"
#include "BKE_scene.h"
#include "BKE_sequencer.h"
#include "BKE_softbody.h"

#include "LBM_fluidsim.h"

#ifndef DISABLE_PYTHON
#include "BPY_extern.h"
#endif

#include "GPU_material.h"

/* Local function protos */
static void solve_parenting (Scene *scene, Object *ob, Object *par, float obmat[][4], float slowmat[][4], int simul);

float originmat[3][3];	/* after where_is_object(), can be used in other functions (bad!) */

void clear_workob(Object *workob)
{
	memset(workob, 0, sizeof(Object));
	
	workob->size[0]= workob->size[1]= workob->size[2]= 1.0f;
	workob->rotmode= ROT_MODE_EUL;
}

void copy_baseflags(struct Scene *scene)
{
	Base *base= scene->base.first;
	
	while(base) {
		base->object->flag= base->flag;
		base= base->next;
	}
}

void copy_objectflags(struct Scene *scene)
{
	Base *base= scene->base.first;
	
	while(base) {
		base->flag= base->object->flag;
		base= base->next;
	}
}

void update_base_layer(struct Scene *scene, Object *ob)
{
	Base *base= scene->base.first;

	while (base) {
		if (base->object == ob) base->lay= ob->lay;
		base= base->next;
	}
}

void object_free_particlesystems(Object *ob)
{
	while(ob->particlesystem.first){
		ParticleSystem *psys = ob->particlesystem.first;
		
		BLI_remlink(&ob->particlesystem,psys);
		
		psys_free(ob,psys);
	}
}

void object_free_softbody(Object *ob)
{
	if(ob->soft) {
		sbFree(ob->soft);
		ob->soft= NULL;
	}
}

void object_free_bulletsoftbody(Object *ob)
{
	if(ob->bsoft) {
		bsbFree(ob->bsoft);
		ob->bsoft= NULL;
	}
}

void object_free_modifiers(Object *ob)
{
	while (ob->modifiers.first) {
		ModifierData *md = ob->modifiers.first;
		
		BLI_remlink(&ob->modifiers, md);
		
		modifier_free(md);
	}

	/* particle modifiers were freed, so free the particlesystems as well */
	object_free_particlesystems(ob);

	/* same for softbody */
	object_free_softbody(ob);
}

void object_link_modifiers(struct Object *ob, struct Object *from)
{
	ModifierData *md;
	object_free_modifiers(ob);

	for (md=from->modifiers.first; md; md=md->next) {
		ModifierData *nmd = NULL;

		if(ELEM4(md->type, eModifierType_Hook, eModifierType_Softbody, eModifierType_ParticleInstance, eModifierType_Collision)) continue;

		nmd = modifier_new(md->type);
		modifier_copyData(md, nmd);
		BLI_addtail(&ob->modifiers, nmd);
	}

	copy_object_particlesystems(from, ob);
	copy_object_softbody(from, ob);

	// TODO: smoke?, cloth?
}

/* here we will collect all local displist stuff */
/* also (ab)used in depsgraph */
void object_free_display(Object *ob)
{
	if(ob->derivedDeform) {
		ob->derivedDeform->needsFree = 1;
		ob->derivedDeform->release(ob->derivedDeform);
		ob->derivedDeform= NULL;
	}
	if(ob->derivedFinal) {
		ob->derivedFinal->needsFree = 1;
		ob->derivedFinal->release(ob->derivedFinal);
		ob->derivedFinal= NULL;
	}
	
	freedisplist(&ob->disp);
}

void free_sculptsession(Object *ob)
{
	if(ob && ob->sculpt) {
		SculptSession *ss = ob->sculpt;
		DerivedMesh *dm= ob->derivedFinal;

		if(ss->pbvh)
			BLI_pbvh_free(ss->pbvh);
		if(dm && dm->getPBVH)
			dm->getPBVH(NULL, dm); /* signal to clear */

		if(ss->texcache)
			MEM_freeN(ss->texcache);

		if(ss->layer_co)
			MEM_freeN(ss->layer_co);

		MEM_freeN(ss);

		ob->sculpt = NULL;
	}
}

/* do not free object itself */
void free_object(Object *ob)
{
	int a;
	
	object_free_display(ob);
	
	/* disconnect specific data */
	if(ob->data) {
		ID *id= ob->data;
		id->us--;
		if(id->us==0) {
			if(ob->type==OB_MESH) unlink_mesh(ob->data);
			else if(ob->type==OB_CURVE) unlink_curve(ob->data);
			else if(ob->type==OB_MBALL) unlink_mball(ob->data);
		}
		ob->data= 0;
	}
	
	for(a=0; a<ob->totcol; a++) {
		if(ob->mat[a]) ob->mat[a]->id.us--;
	}
	if(ob->mat) MEM_freeN(ob->mat);
	if(ob->matbits) MEM_freeN(ob->matbits);
	ob->mat= 0;
	ob->matbits= 0;
	if(ob->bb) MEM_freeN(ob->bb); 
	ob->bb= 0;
	if(ob->path) free_path(ob->path); 
	ob->path= 0;
	if(ob->adt) BKE_free_animdata((ID *)ob);
	if(ob->poselib) ob->poselib->id.us--;
	if(ob->dup_group) ob->dup_group->id.us--;
	if(ob->gpd) ob->gpd->id.us--;
	if(ob->defbase.first)
		BLI_freelistN(&ob->defbase);
	if(ob->pose)
		free_pose(ob->pose);
	if(ob->mpath)
		animviz_free_motionpath(ob->mpath);
	free_properties(&ob->prop);
	object_free_modifiers(ob);
	
	free_sensors(&ob->sensors);
	free_controllers(&ob->controllers);
	free_actuators(&ob->actuators);
	
	free_constraints(&ob->constraints);
	
	free_partdeflect(ob->pd);

	if(ob->soft) sbFree(ob->soft);
	if(ob->bsoft) bsbFree(ob->bsoft);
	if(ob->gpulamp.first) GPU_lamp_free(ob);

	free_sculptsession(ob);

	if(ob->pc_ids.first) BLI_freelistN(&ob->pc_ids);
}

static void unlink_object__unlinkModifierLinks(void *userData, Object *ob, Object **obpoin)
{
	Object *unlinkOb = userData;

	if (*obpoin==unlinkOb) {
		*obpoin = NULL;
		ob->recalc |= OB_RECALC;
	}
}
void unlink_object(Scene *scene, Object *ob)
{
	Object *obt;
	Material *mat;
	World *wrld;
	bScreen *sc;
	Scene *sce;
	Curve *cu;
	Tex *tex;
	Group *group;
	Camera *camera;
	bConstraint *con;
	//bActionStrip *strip; // XXX animsys 
	ModifierData *md;
	int a;
	
	unlink_controllers(&ob->controllers);
	unlink_actuators(&ob->actuators);
	
	/* check all objects: parents en bevels and fields, also from libraries */
	// FIXME: need to check all animation blocks (drivers)
	obt= G.main->object.first;
	while(obt) {
		if(obt->proxy==ob)
			obt->proxy= NULL;
		if(obt->proxy_from==ob) {
			obt->proxy_from= NULL;
			obt->recalc |= OB_RECALC_OB;
		}
		if(obt->proxy_group==ob)
			obt->proxy_group= NULL;
		
		if(obt->parent==ob) {
			obt->parent= NULL;
			obt->recalc |= OB_RECALC;
		}
		
		modifiers_foreachObjectLink(obt, unlink_object__unlinkModifierLinks, ob);
		
		if ELEM(obt->type, OB_CURVE, OB_FONT) {
			cu= obt->data;

			if(cu->bevobj==ob) {
				cu->bevobj= NULL;
				obt->recalc |= OB_RECALC;
			}
			if(cu->taperobj==ob) {
				cu->taperobj= NULL;
				obt->recalc |= OB_RECALC;
			}
			if(cu->textoncurve==ob) {
				cu->textoncurve= NULL;
				obt->recalc |= OB_RECALC;
			}
		}
		else if(obt->type==OB_ARMATURE && obt->pose) {
			bPoseChannel *pchan;
			for(pchan= obt->pose->chanbase.first; pchan; pchan= pchan->next) {
				for (con = pchan->constraints.first; con; con=con->next) {
					bConstraintTypeInfo *cti= constraint_get_typeinfo(con);
					ListBase targets = {NULL, NULL};
					bConstraintTarget *ct;
					
					if (cti && cti->get_constraint_targets) {
						cti->get_constraint_targets(con, &targets);
						
						for (ct= targets.first; ct; ct= ct->next) {
							if (ct->tar == ob) {
								ct->tar = NULL;
								strcpy(ct->subtarget, "");
								obt->recalc |= OB_RECALC_DATA;
							}
						}
						
						if (cti->flush_constraint_targets)
							cti->flush_constraint_targets(con, &targets, 0);
					}
				}
				if(pchan->custom==ob)
					pchan->custom= NULL;
			}
		}
		
		sca_remove_ob_poin(obt, ob);
		
		for (con = obt->constraints.first; con; con=con->next) {
			bConstraintTypeInfo *cti= constraint_get_typeinfo(con);
			ListBase targets = {NULL, NULL};
			bConstraintTarget *ct;
			
			if (cti && cti->get_constraint_targets) {
				cti->get_constraint_targets(con, &targets);
				
				for (ct= targets.first; ct; ct= ct->next) {
					if (ct->tar == ob) {
						ct->tar = NULL;
						strcpy(ct->subtarget, "");
						obt->recalc |= OB_RECALC_DATA;
					}
				}
				
				if (cti->flush_constraint_targets)
					cti->flush_constraint_targets(con, &targets, 0);
			}
		}
		
		/* object is deflector or field */
		if(ob->pd) {
			if(obt->soft)
				obt->recalc |= OB_RECALC_DATA;

			/* cloth */
			for(md=obt->modifiers.first; md; md=md->next)
				if(md->type == eModifierType_Cloth)
					obt->recalc |= OB_RECALC_DATA;
		}
		
		/* strips */
#if 0 // XXX old animation system
		for(strip= obt->nlastrips.first; strip; strip= strip->next) {
			if(strip->object==ob)
				strip->object= NULL;
			
			if(strip->modifiers.first) {
				bActionModifier *amod;
				for(amod= strip->modifiers.first; amod; amod= amod->next)
					if(amod->ob==ob)
						amod->ob= NULL;
			}
		}
#endif // XXX old animation system

		/* particle systems */
		if(obt->particlesystem.first) {
			ParticleSystem *tpsys= obt->particlesystem.first;
			for(; tpsys; tpsys=tpsys->next) {
				BoidState *state = NULL;
				BoidRule *rule = NULL;

				ParticleTarget *pt = tpsys->targets.first;
				for(; pt; pt=pt->next) {
					if(pt->ob==ob) {
						pt->ob = NULL;
						obt->recalc |= OB_RECALC_DATA;
						break;
					}
				}

				if(tpsys->target_ob==ob) {
					tpsys->target_ob= NULL;
					obt->recalc |= OB_RECALC_DATA;
				}

				if(tpsys->part->dup_ob==ob)
					tpsys->part->dup_ob= NULL;

				if(tpsys->part->phystype==PART_PHYS_BOIDS) {
					ParticleData *pa;
					BoidParticle *bpa;
					int p;

					for(p=0,pa=tpsys->particles; p<tpsys->totpart; p++,pa++) {
						bpa = pa->boid;
						if(bpa->ground == ob)
							bpa->ground = NULL;
					}
				}
				if(tpsys->part->boids) {
					for(state = tpsys->part->boids->states.first; state; state=state->next) {
						for(rule = state->rules.first; rule; rule=rule->next) {
							if(rule->type==eBoidRuleType_Avoid) {
								BoidRuleGoalAvoid *gabr = (BoidRuleGoalAvoid*)rule;
								if(gabr->ob==ob)
									gabr->ob= NULL;
							}
							else if(rule->type==eBoidRuleType_FollowLeader) {
								BoidRuleFollowLeader *flbr = (BoidRuleFollowLeader*)rule;
								if(flbr->ob==ob)
									flbr->ob= NULL;
							}
						}
					}
				}
			}
			if(ob->pd)
				obt->recalc |= OB_RECALC_DATA;
		}

		obt= obt->id.next;
	}
	
	/* materials */
	mat= G.main->mat.first;
	while(mat) {
	
		for(a=0; a<MAX_MTEX; a++) {
			if(mat->mtex[a] && ob==mat->mtex[a]->object) {
				/* actually, test for lib here... to do */
				mat->mtex[a]->object= NULL;
			}
		}

		mat= mat->id.next;
	}
	
	/* textures */
	tex= G.main->tex.first;
	while(tex) {
		if(tex->env) {
			if(tex->env->object == ob) tex->env->object= NULL;
		}
		tex= tex->id.next;
	}
	
	/* mballs (scene==NULL when called from library.c) */
	if(scene && ob->type==OB_MBALL) {
		obt= find_basis_mball(scene, ob);
		if(obt) freedisplist(&obt->disp);
	}
	
	/* worlds */
	wrld= G.main->world.first;
	while(wrld) {
		if(wrld->id.lib==NULL) {
			for(a=0; a<MAX_MTEX; a++) {
				if(wrld->mtex[a] && ob==wrld->mtex[a]->object)
					wrld->mtex[a]->object= NULL;
			}
		}
		
		wrld= wrld->id.next;
	}
		
	/* scenes */
	sce= G.main->scene.first;
	while(sce) {
		if(sce->id.lib==NULL) {
			if(sce->camera==ob) sce->camera= NULL;
			if(sce->toolsettings->skgen_template==ob) sce->toolsettings->skgen_template = NULL;
			if(sce->toolsettings->particle.object==ob) sce->toolsettings->particle.object= NULL;

#ifdef DURIAN_CAMERA_SWITCH
			{
				TimeMarker *m;

				for (m= sce->markers.first; m; m= m->next) {
					if(m->camera==ob)
						m->camera= NULL;
				}
			}
#endif
			if(sce->ed) {
				Sequence *seq;
				SEQ_BEGIN(sce->ed, seq)
					if(seq->scene_camera==ob) {
						seq->scene_camera= NULL;
					}
				SEQ_END
			}
		}

		sce= sce->id.next;
	}
	
#if 0 // XXX old animation system
	/* ipos */
	ipo= G.main->ipo.first;
	while(ipo) {
		if(ipo->id.lib==NULL) {
			IpoCurve *icu;
			for(icu= ipo->curve.first; icu; icu= icu->next) {
				if(icu->driver && icu->driver->ob==ob)
					icu->driver->ob= NULL;
			}
		}
		ipo= ipo->id.next;
	}
#endif // XXX old animation system
	
	/* screens */
	sc= G.main->screen.first;
	while(sc) {
		ScrArea *sa= sc->areabase.first;
		while(sa) {
			SpaceLink *sl;

			for (sl= sa->spacedata.first; sl; sl= sl->next) {
				if(sl->spacetype==SPACE_VIEW3D) {
					View3D *v3d= (View3D*) sl;

					if(v3d->camera==ob) {
						v3d->camera= NULL;
						// XXX if(v3d->persp==V3D_CAMOB) v3d->persp= V3D_PERSP;
					}
					if(v3d->localvd && v3d->localvd->camera==ob ) {
						v3d->localvd->camera= NULL;
						// XXX if(v3d->localvd->persp==V3D_CAMOB) v3d->localvd->persp= V3D_PERSP;
					}
				}
				else if(sl->spacetype==SPACE_OUTLINER) {
					SpaceOops *so= (SpaceOops *)sl;

					if(so->treestore) {
						TreeStoreElem *tselem= so->treestore->data;
						int a;
						for(a=0; a<so->treestore->usedelem; a++, tselem++) {
							if(tselem->id==(ID *)ob) tselem->id= NULL;
						}
					}
				}
			}

			sa= sa->next;
		}
		sc= sc->id.next;
	}

	/* groups */
	group= G.main->group.first;
	while(group) {
		rem_from_group(group, ob, NULL, NULL);
		group= group->id.next;
	}
	
	/* cameras */
	camera= G.main->camera.first;
	while(camera) {
		if (camera->dof_ob==ob) {
			camera->dof_ob = NULL;
		}
		camera= camera->id.next;
	}
}

int exist_object(Object *obtest)
{
	Object *ob;
	
	if(obtest==NULL) return 0;
	
	ob= G.main->object.first;
	while(ob) {
		if(ob==obtest) return 1;
		ob= ob->id.next;
	}
	return 0;
}

void *add_camera(char *name)
{
	Camera *cam;
	
	cam=  alloc_libblock(&G.main->camera, ID_CA, name);

	cam->lens= 35.0f;
	cam->clipsta= 0.1f;
	cam->clipend= 100.0f;
	cam->drawsize= 0.5f;
	cam->ortho_scale= 6.0;
	cam->flag |= CAM_SHOWTITLESAFE;
	cam->passepartalpha = 0.2f;
	
	return cam;
}

Camera *copy_camera(Camera *cam)
{
	Camera *camn;
	
	camn= copy_libblock(cam);
	camn->adt= BKE_copy_animdata(cam->adt);
	
	return camn;
}



void make_local_camera(Camera *cam)
{
	Object *ob;
	Camera *camn;
	int local=0, lib=0;

	/* - only lib users: do nothing
		* - only local users: set flag
		* - mixed: make copy
		*/
	
	if(cam->id.lib==0) return;
	if(cam->id.us==1) {
		cam->id.lib= 0;
		cam->id.flag= LIB_LOCAL;
		new_id(0, (ID *)cam, 0);
		return;
	}
	
	ob= G.main->object.first;
	while(ob) {
		if(ob->data==cam) {
			if(ob->id.lib) lib= 1;
			else local= 1;
		}
		ob= ob->id.next;
	}
	
	if(local && lib==0) {
		cam->id.lib= 0;
		cam->id.flag= LIB_LOCAL;
		new_id(0, (ID *)cam, 0);
	}
	else if(local && lib) {
		camn= copy_camera(cam);
		camn->id.us= 0;
		
		ob= G.main->object.first;
		while(ob) {
			if(ob->data==cam) {
				
				if(ob->id.lib==0) {
					ob->data= camn;
					camn->id.us++;
					cam->id.us--;
				}
			}
			ob= ob->id.next;
		}
	}
}

/* get the camera's dof value, takes the dof object into account */
float dof_camera(Object *ob)
{
	Camera *cam = (Camera *)ob->data; 
	if (ob->type != OB_CAMERA)
		return 0.0f;
	if (cam->dof_ob) {	
		/* too simple, better to return the distance on the view axis only
		 * return len_v3v3(ob->obmat[3], cam->dof_ob->obmat[3]); */
		float mat[4][4], imat[4][4], obmat[4][4];
		
		copy_m4_m4(obmat, ob->obmat);
		normalize_m4(obmat);
		invert_m4_m4(imat, obmat);
		mul_m4_m4m4(mat, cam->dof_ob->obmat, imat);
		return (float)fabs(mat[3][2]);
	}
	return cam->YF_dofdist;
}

void *add_lamp(char *name)
{
	Lamp *la;
	
	la=  alloc_libblock(&G.main->lamp, ID_LA, name);
	
	la->r= la->g= la->b= la->k= 1.0f;
	la->haint= la->energy= 1.0f;
	la->dist= 25.0f;
	la->spotsize= 45.0f;
	la->spotblend= 0.15f;
	la->att2= 1.0f;
	la->mode= LA_SHAD_BUF;
	la->bufsize= 512;
	la->clipsta= 0.5f;
	la->clipend= 40.0f;
	la->shadspotsize= 45.0f;
	la->samp= 3;
	la->bias= 1.0f;
	la->soft= 3.0f;
	la->compressthresh= 0.05f;
	la->ray_samp= la->ray_sampy= la->ray_sampz= 1; 
	la->area_size=la->area_sizey=la->area_sizez= 1.0f; 
	la->buffers= 1;
	la->buftype= LA_SHADBUF_HALFWAY;
	la->ray_samp_method = LA_SAMP_HALTON;
	la->adapt_thresh = 0.001f;
	la->preview=NULL;
	la->falloff_type = LA_FALLOFF_INVSQUARE;
	la->curfalloff = curvemapping_add(1, 0.0f, 1.0f, 1.0f, 0.0f);
	la->sun_effect_type = 0;
	la->horizon_brightness = 1.0;
	la->spread = 1.0;
	la->sun_brightness = 1.0;
	la->sun_size = 1.0;
	la->backscattered_light = 1.0f;
	la->atm_turbidity = 2.0f;
	la->atm_inscattering_factor = 1.0f;
	la->atm_extinction_factor = 1.0f;
	la->atm_distance_factor = 1.0f;
	la->sun_intensity = 1.0f;
	la->skyblendtype= MA_RAMP_ADD;
	la->skyblendfac= 1.0f;
	la->sky_colorspace= BLI_XYZ_CIE;
	la->sky_exposure= 1.0f;
	
	curvemapping_initialize(la->curfalloff);
	return la;
}

Lamp *copy_lamp(Lamp *la)
{
	Lamp *lan;
	int a;
	
	lan= copy_libblock(la);

	for(a=0; a<MAX_MTEX; a++) {
		if(lan->mtex[a]) {
			lan->mtex[a]= MEM_mallocN(sizeof(MTex), "copylamptex");
			memcpy(lan->mtex[a], la->mtex[a], sizeof(MTex));
			id_us_plus((ID *)lan->mtex[a]->tex);
		}
	}
	
	lan->curfalloff = curvemapping_copy(la->curfalloff);
	
#if 0 // XXX old animation system
	id_us_plus((ID *)lan->ipo);
#endif // XXX old animation system

	if (la->preview) lan->preview = BKE_previewimg_copy(la->preview);
	
	return lan;
}

void make_local_lamp(Lamp *la)
{
	Object *ob;
	Lamp *lan;
	int local=0, lib=0;

	/* - only lib users: do nothing
		* - only local users: set flag
		* - mixed: make copy
		*/
	
	if(la->id.lib==0) return;
	if(la->id.us==1) {
		la->id.lib= 0;
		la->id.flag= LIB_LOCAL;
		new_id(0, (ID *)la, 0);
		return;
	}
	
	ob= G.main->object.first;
	while(ob) {
		if(ob->data==la) {
			if(ob->id.lib) lib= 1;
			else local= 1;
		}
		ob= ob->id.next;
	}
	
	if(local && lib==0) {
		la->id.lib= 0;
		la->id.flag= LIB_LOCAL;
		new_id(0, (ID *)la, 0);
	}
	else if(local && lib) {
		lan= copy_lamp(la);
		lan->id.us= 0;
		
		ob= G.main->object.first;
		while(ob) {
			if(ob->data==la) {
				
				if(ob->id.lib==0) {
					ob->data= lan;
					lan->id.us++;
					la->id.us--;
				}
			}
			ob= ob->id.next;
		}
	}
}

void free_camera(Camera *ca)
{
	BKE_free_animdata((ID *)ca);
}

void free_lamp(Lamp *la)
{
	MTex *mtex;
	int a;

	for(a=0; a<MAX_MTEX; a++) {
		mtex= la->mtex[a];
		if(mtex && mtex->tex) mtex->tex->id.us--;
		if(mtex) MEM_freeN(mtex);
	}
	
	BKE_free_animdata((ID *)la);

	curvemapping_free(la->curfalloff);
	
	BKE_previewimg_free(&la->preview);
	BKE_icon_delete(&la->id);
	la->id.icon_id = 0;
}

void *add_wave()
{
	return 0;
}


/* *************************************************** */

static void *add_obdata_from_type(int type)
{
	switch (type) {
	case OB_MESH: return add_mesh("Mesh");
	case OB_CURVE: return add_curve("Curve", OB_CURVE);
	case OB_SURF: return add_curve("Surf", OB_SURF);
	case OB_FONT: return add_curve("Text", OB_FONT);
	case OB_MBALL: return add_mball("Meta");
	case OB_CAMERA: return add_camera("Camera");
	case OB_LAMP: return add_lamp("Lamp");
	case OB_LATTICE: return add_lattice("Lattice");
	case OB_WAVE: return add_wave();
	case OB_ARMATURE: return add_armature("Armature");
	case OB_EMPTY: return NULL;
	default:
		printf("add_obdata_from_type: Internal error, bad type: %d\n", type);
		return NULL;
	}
}

static char *get_obdata_defname(int type)
{
	switch (type) {
	case OB_MESH: return "Mesh";
	case OB_CURVE: return "Curve";
	case OB_SURF: return "Surf";
	case OB_FONT: return "Font";
	case OB_MBALL: return "Mball";
	case OB_CAMERA: return "Camera";
	case OB_LAMP: return "Lamp";
	case OB_LATTICE: return "Lattice";
	case OB_WAVE: return "Wave";
	case OB_ARMATURE: return "Armature";
	case OB_EMPTY: return "Empty";
	default:
		printf("get_obdata_defname: Internal error, bad type: %d\n", type);
		return "Empty";
	}
}

/* more general add: creates minimum required data, but without vertices etc. */
Object *add_only_object(int type, char *name)
{
	Object *ob;

	ob= alloc_libblock(&G.main->object, ID_OB, name);

	/* default object vars */
	ob->type= type;
	
	ob->col[0]= ob->col[1]= ob->col[2]= 1.0;
	ob->col[3]= 1.0;
	
	ob->size[0]= ob->size[1]= ob->size[2]= 1.0;
	
	/* objects should default to having Euler XYZ rotations, 
	 * but rotations default to quaternions 
	 */
	ob->rotmode= ROT_MODE_EUL;
	/* axis-angle must not have a 0,0,0 axis, so set y-axis as default... */
	ob->rotAxis[1]= ob->drotAxis[1]= 1.0f;
	/* quaternions should be 1,0,0,0 by default.... */
	ob->quat[0]= ob->dquat[0]= 1.0f;
	/* rotation locks should be 4D for 4 component rotations by default... */
	ob->protectflag = OB_LOCK_ROT4D;
	
	unit_m4(ob->constinv);
	unit_m4(ob->parentinv);
	unit_m4(ob->obmat);
	ob->dt= OB_TEXTURE;
	ob->empty_drawtype= OB_ARROWS;
	ob->empty_drawsize= 1.0;

	if(type==OB_CAMERA || type==OB_LAMP) {
		ob->trackflag= OB_NEGZ;
		ob->upflag= OB_POSY;
	}
	else {
		ob->trackflag= OB_POSY;
		ob->upflag= OB_POSZ;
	}
	
	ob->dupon= 1; ob->dupoff= 0;
	ob->dupsta= 1; ob->dupend= 100;
	ob->dupfacesca = 1.0;

	/* Game engine defaults*/
	ob->mass= ob->inertia= 1.0f;
	ob->formfactor= 0.4f;
	ob->damping= 0.04f;
	ob->rdamping= 0.1f;
	ob->anisotropicFriction[0] = 1.0f;
	ob->anisotropicFriction[1] = 1.0f;
	ob->anisotropicFriction[2] = 1.0f;
	ob->gameflag= OB_PROP|OB_COLLISION;
	ob->margin = 0.0;
	/* ob->pad3 == Contact Processing Threshold */
	ob->m_contactProcessingThreshold = 1.;
	
	/* NT fluid sim defaults */
	ob->fluidsimFlag = 0;
	ob->fluidsimSettings = NULL;

	ob->pc_ids.first = ob->pc_ids.last = NULL;
	
	/* Animation Visualisation defaults */
	animviz_settings_init(&ob->avs);

	return ob;
}

/* general add: to scene, with layer from area and default name */
/* creates minimum required data, but without vertices etc. */
Object *add_object(struct Scene *scene, int type)
{
	Object *ob;
	Base *base;
	char name[32];

	strcpy(name, get_obdata_defname(type));
	ob = add_only_object(type, name);

	ob->data= add_obdata_from_type(type);

	ob->lay= scene->lay;
	
	base= scene_add_base(scene, ob);
	scene_select_base(scene, base);
	ob->recalc |= OB_RECALC;

	return ob;
}

SoftBody *copy_softbody(SoftBody *sb)
{
	SoftBody *sbn;
	
	if (sb==NULL) return(NULL);
	
	sbn= MEM_dupallocN(sb);
	sbn->totspring= sbn->totpoint= 0;
	sbn->bpoint= NULL;
	sbn->bspring= NULL;
	
	sbn->keys= NULL;
	sbn->totkey= sbn->totpointkey= 0;
	
	sbn->scratch= NULL;

	sbn->pointcache= BKE_ptcache_copy_list(&sbn->ptcaches, &sb->ptcaches);

	if(sb->effector_weights)
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

ParticleSystem *copy_particlesystem(ParticleSystem *psys)
{
	ParticleSystem *psysn;
	ParticleData *pa;
	int p;

	psysn= MEM_dupallocN(psys);
	psysn->particles= MEM_dupallocN(psys->particles);
	psysn->child= MEM_dupallocN(psys->child);

	if(psys->part->type == PART_HAIR) {
		for(p=0, pa=psysn->particles; p<psysn->totpart; p++, pa++)
			pa->hair = MEM_dupallocN(pa->hair);
	}

	if(psysn->particles && (psysn->particles->keys || psysn->particles->boid)) {
		ParticleKey *key = psysn->particles->keys;
		BoidParticle *boid = psysn->particles->boid;

		if(key)
			key = MEM_dupallocN(key);
		
		if(boid)
			boid = MEM_dupallocN(boid);
		
		for(p=0, pa=psysn->particles; p<psysn->totpart; p++, pa++) {
			if(boid)
				pa->boid = boid++;
			if(key) {
				pa->keys = key;
				key += pa->totkey;
			}
		}
	}

	if(psys->clmd) {
		psysn->clmd = (ClothModifierData *)modifier_new(eModifierType_Cloth);
		modifier_copyData((ModifierData*)psys->clmd, (ModifierData*)psysn->clmd);
		psys->hair_in_dm = psys->hair_out_dm = NULL;
	}

	BLI_duplicatelist(&psysn->targets, &psys->targets);
	
	psysn->pathcache= NULL;
	psysn->childcache= NULL;
	psysn->edit= NULL;
	psysn->frand= NULL;
	psysn->pdd= NULL;
	
	psysn->pathcachebufs.first = psysn->pathcachebufs.last = NULL;
	psysn->childcachebufs.first = psysn->childcachebufs.last = NULL;
	psysn->renderdata = NULL;
	
	psysn->pointcache= BKE_ptcache_copy_list(&psysn->ptcaches, &psys->ptcaches);

	/* XXX - from reading existing code this seems correct but intended usage of
	 * pointcache should /w cloth should be added in 'ParticleSystem' - campbell */
	if(psysn->clmd) {
		psysn->clmd->point_cache= psysn->pointcache;
	}

	id_us_plus((ID *)psysn->part);

	return psysn;
}

void copy_object_particlesystems(Object *obn, Object *ob)
{
	ParticleSystemModifierData *psmd;
	ParticleSystem *psys, *npsys;
	ModifierData *md;

	obn->particlesystem.first= obn->particlesystem.last= NULL;
	for(psys=ob->particlesystem.first; psys; psys=psys->next) {
		npsys= copy_particlesystem(psys);

		BLI_addtail(&obn->particlesystem, npsys);

		/* need to update particle modifiers too */
		for(md=obn->modifiers.first; md; md=md->next) {
			if(md->type==eModifierType_ParticleSystem) {
				psmd= (ParticleSystemModifierData*)md;
				if(psmd->psys==psys)
					psmd->psys= npsys;
			}
		}
	}
}

void copy_object_softbody(Object *obn, Object *ob)
{
	if(ob->soft)
		obn->soft= copy_softbody(ob->soft);
}

static void copy_object_pose(Object *obn, Object *ob)
{
	bPoseChannel *chan;
	
	/* note: need to clear obn->pose pointer first, so that copy_pose works (otherwise there's a crash) */
	obn->pose= NULL;
	copy_pose(&obn->pose, ob->pose, 1);	/* 1 = copy constraints */

	for (chan = obn->pose->chanbase.first; chan; chan=chan->next){
		bConstraint *con;
		
		chan->flag &= ~(POSE_LOC|POSE_ROT|POSE_SIZE);
		
		for (con= chan->constraints.first; con; con= con->next) {
			bConstraintTypeInfo *cti= constraint_get_typeinfo(con);
			ListBase targets = {NULL, NULL};
			bConstraintTarget *ct;
			
#if 0 // XXX old animation system
			/* note that we can't change lib linked ipo blocks. for making
			 * proxies this still works correct however because the object
			 * is changed to object->proxy_from when evaluating the driver. */
			if(con->ipo && !con->ipo->id.lib) {
				IpoCurve *icu;
				
				con->ipo= copy_ipo(con->ipo);
				
				for(icu= con->ipo->curve.first; icu; icu= icu->next) {
					if(icu->driver && icu->driver->ob==ob)
						icu->driver->ob= obn;
				}
			}
#endif // XXX old animation system
			
			if (cti && cti->get_constraint_targets) {
				cti->get_constraint_targets(con, &targets);
				
				for (ct= targets.first; ct; ct= ct->next) {
					if (ct->tar == ob)
						ct->tar = obn;
				}
				
				if (cti->flush_constraint_targets)
					cti->flush_constraint_targets(con, &targets, 0);
			}
		}
	}
}

Object *copy_object(Object *ob)
{
	Object *obn;
	ModifierData *md;
	int a;

	obn= copy_libblock(ob);
	
	if(ob->totcol) {
		obn->mat= MEM_dupallocN(ob->mat);
		obn->matbits= MEM_dupallocN(ob->matbits);
		obn->totcol= ob->totcol;
	}
	
	if(ob->bb) obn->bb= MEM_dupallocN(ob->bb);
	obn->path= NULL;
	obn->flag &= ~OB_FROMGROUP;
	
	obn->modifiers.first = obn->modifiers.last= NULL;
	
	for (md=ob->modifiers.first; md; md=md->next) {
		ModifierData *nmd = modifier_new(md->type);
		modifier_copyData(md, nmd);
		BLI_addtail(&obn->modifiers, nmd);
	}

	obn->prop.first = obn->prop.last = NULL;
	copy_properties(&obn->prop, &ob->prop);
	
	copy_sensors(&obn->sensors, &ob->sensors);
	copy_controllers(&obn->controllers, &ob->controllers);
	copy_actuators(&obn->actuators, &ob->actuators);
	
	if(ob->pose) {
		copy_object_pose(obn, ob);
		/* backwards compat... non-armatures can get poses in older files? */
		if(ob->type==OB_ARMATURE)
			armature_rebuild_pose(obn, obn->data);
	}
	defgroup_copy_list(&obn->defbase, &ob->defbase);
	copy_constraints(&obn->constraints, &ob->constraints);

	obn->mode = 0;
	obn->sculpt = NULL;

	/* increase user numbers */
	id_us_plus((ID *)obn->data);
	id_us_plus((ID *)obn->dup_group);
	

	for(a=0; a<obn->totcol; a++) id_us_plus((ID *)obn->mat[a]);
	
	obn->disp.first= obn->disp.last= NULL;
	
	if(ob->pd){
		obn->pd= MEM_dupallocN(ob->pd);
		if(obn->pd->tex)
			id_us_plus(&(obn->pd->tex->id));
		if(obn->pd->rng)
			obn->pd->rng = MEM_dupallocN(ob->pd->rng);
	}
	obn->soft= copy_softbody(ob->soft);
	obn->bsoft = copy_bulletsoftbody(ob->bsoft);

	copy_object_particlesystems(obn, ob);
	
	obn->derivedDeform = NULL;
	obn->derivedFinal = NULL;

	obn->gpulamp.first = obn->gpulamp.last = NULL;
	obn->pc_ids.first = obn->pc_ids.last = NULL;
	
	return obn;
}

void expand_local_object(Object *ob)
{
	//bActionStrip *strip;
	ParticleSystem *psys;
	int a;

#if 0 // XXX old animation system
	id_lib_extern((ID *)ob->action);
	id_lib_extern((ID *)ob->ipo);
#endif // XXX old animation system
	id_lib_extern((ID *)ob->data);
	id_lib_extern((ID *)ob->dup_group);
	
	for(a=0; a<ob->totcol; a++) {
		id_lib_extern((ID *)ob->mat[a]);
	}
#if 0 // XXX old animation system
	for (strip=ob->nlastrips.first; strip; strip=strip->next) {
		id_lib_extern((ID *)strip->act);
	}
#endif // XXX old animation system
	for(psys=ob->particlesystem.first; psys; psys=psys->next)
		id_lib_extern((ID *)psys->part);
}

void make_local_object(Object *ob)
{
	Object *obn;
	Scene *sce;
	Base *base;
	int local=0, lib=0;

	/* - only lib users: do nothing
		* - only local users: set flag
		* - mixed: make copy
		*/
	
	if(ob->id.lib==NULL) return;
	
	ob->proxy= ob->proxy_from= NULL;
	
	if(ob->id.us==1) {
		ob->id.lib= NULL;
		ob->id.flag= LIB_LOCAL;
		new_id(0, (ID *)ob, 0);

	}
	else {
		sce= G.main->scene.first;
		while(sce) {
			base= sce->base.first;
			while(base) {
				if(base->object==ob) {
					if(sce->id.lib) lib++;
					else local++;
					break;
				}
				base= base->next;
			}
			sce= sce->id.next;
		}
		
		if(local && lib==0) {
			ob->id.lib= 0;
			ob->id.flag= LIB_LOCAL;
			new_id(0, (ID *)ob, 0);
		}
		else if(local && lib) {
			obn= copy_object(ob);
			obn->id.us= 0;
			
			sce= G.main->scene.first;
			while(sce) {
				if(sce->id.lib==0) {
					base= sce->base.first;
					while(base) {
						if(base->object==ob) {
							base->object= obn;
							obn->id.us++;
							ob->id.us--;
						}
						base= base->next;
					}
				}
				sce= sce->id.next;
			}
		}
	}
	
	expand_local_object(ob);
}

/*
 * Returns true if the Object is a from an external blend file (libdata)
 */
int object_is_libdata(Object *ob)
{
	if (!ob) return 0;
	if (ob->proxy) return 0;
	if (ob->id.lib) return 1;
	return 0;
}

/* Returns true if the Object data is a from an external blend file (libdata) */
int object_data_is_libdata(Object *ob)
{
	if(!ob) return 0;
	if(ob->proxy && (ob->data==NULL || ((ID *)ob->data)->lib==NULL)) return 0;
	if(ob->id.lib) return 1;
	if(ob->data==NULL) return 0;
	if(((ID *)ob->data)->lib) return 1;

	return 0;
}

/* *************** PROXY **************** */

/* when you make proxy, ensure the exposed layers are extern */
static void armature_set_id_extern(Object *ob)
{
	bArmature *arm= ob->data;
	bPoseChannel *pchan;
	int lay= arm->layer_protected;
	
	for (pchan = ob->pose->chanbase.first; pchan; pchan=pchan->next) {
		if(!(pchan->bone->layer & lay))
			id_lib_extern((ID *)pchan->custom);
	}
			
}

void object_copy_proxy_drivers(Object *ob, Object *target)
{
	if ((target->adt) && (target->adt->drivers.first)) {
		FCurve *fcu;
		
		/* add new animdata block */
		if(!ob->adt)
			ob->adt= BKE_id_add_animdata(&ob->id);
		
		/* make a copy of all the drivers (for now), then correct any links that need fixing */
		free_fcurves(&ob->adt->drivers);
		copy_fcurves(&ob->adt->drivers, &target->adt->drivers);
		
		for (fcu= ob->adt->drivers.first; fcu; fcu= fcu->next) {
			ChannelDriver *driver= fcu->driver;
			DriverVar *dvar;
			
			for (dvar= driver->variables.first; dvar; dvar= dvar->next) {
				/* all drivers */
				DRIVER_TARGETS_LOOPER(dvar) 
				{
					if ((Object *)dtar->id == target)
						dtar->id= (ID *)ob;
					else
						id_lib_extern((ID *)dtar->id);
				}
				DRIVER_TARGETS_LOOPER_END
			}
		}
	}
}

/* proxy rule: lib_object->proxy_from == the one we borrow from, set temporally while object_update */
/*             local_object->proxy == pointer to library object, saved in files and read */
/*             local_object->proxy_group == pointer to group dupli-object, saved in files and read */

void object_make_proxy(Object *ob, Object *target, Object *gob)
{
	/* paranoia checks */
	if(ob->id.lib || target->id.lib==NULL) {
		printf("cannot make proxy\n");
		return;
	}
	
	ob->proxy= target;
	ob->proxy_group= gob;
	id_lib_extern(&target->id);
	
	ob->recalc= target->recalc= OB_RECALC;
	
	/* copy transform */
	if(gob) {
		VECCOPY(ob->loc, gob->loc);
		VECCOPY(ob->rot, gob->rot);
		VECCOPY(ob->size, gob->size);
		
		group_tag_recalc(gob->dup_group);
	}
	else {
		VECCOPY(ob->loc, target->loc);
		VECCOPY(ob->rot, target->rot);
		VECCOPY(ob->size, target->size);
	}
	
	ob->parent= target->parent;	/* libdata */
	copy_m4_m4(ob->parentinv, target->parentinv);
	
	/* copy animdata stuff - drivers only for now... */
	object_copy_proxy_drivers(ob, target);

	/* skip constraints? */
	// FIXME: this is considered by many as a bug
	
	/* set object type and link to data */
	ob->type= target->type;
	ob->data= target->data;
	id_us_plus((ID *)ob->data);		/* ensures lib data becomes LIB_EXTERN */
	
	/* copy material and index information */
	ob->actcol= ob->totcol= 0;
	if(ob->mat) MEM_freeN(ob->mat);
	if(ob->matbits) MEM_freeN(ob->matbits);
	ob->mat = NULL;
	ob->matbits= NULL;
	if ((target->totcol) && (target->mat) && ELEM5(ob->type, OB_MESH, OB_CURVE, OB_SURF, OB_FONT, OB_MBALL)) { //XXX OB_SUPPORT_MATERIAL
		int i;
		ob->colbits = target->colbits;
		
		ob->actcol= target->actcol;
		ob->totcol= target->totcol;
		
		ob->mat = MEM_dupallocN(target->mat);
		ob->matbits = MEM_dupallocN(target->matbits);
		for(i=0; i<target->totcol; i++) {
			/* dont need to run test_object_materials since we know this object is new and not used elsewhere */
			id_us_plus((ID *)ob->mat[i]); 
		}
	}
	
	/* type conversions */
	if(target->type == OB_ARMATURE) {
		copy_object_pose(ob, target);	/* data copy, object pointers in constraints */
		rest_pose(ob->pose);			/* clear all transforms in channels */
		armature_rebuild_pose(ob, ob->data);	/* set all internal links */
		
		armature_set_id_extern(ob);
	}
	
	/* copy drawtype info */
	ob->dt= target->dt;
}


/* *************** CALC ****************** */

/* there is also a timing calculation in drawobject() */

float bluroffs= 0.0f, fieldoffs= 0.0f;
int no_speed_curve= 0;

/* ugly calls from render */
void set_mblur_offs(float blur)
{
	bluroffs= blur;
}

void set_field_offs(float field)
{
	fieldoffs= field;
}

void disable_speed_curve(int val)
{
	no_speed_curve= val;
}

// XXX THIS CRUFT NEEDS SERIOUS RECODING ASAP!
/* ob can be NULL */
float bsystem_time(struct Scene *scene, Object *ob, float cfra, float ofs)
{
	/* returns float ( see frame_to_float in ipo.c) */
	
	/* bluroffs and fieldoffs are ugly globals that are set by render */
	cfra+= bluroffs+fieldoffs;

	/* global time */
	if (scene)
		cfra*= scene->r.framelen;	
	
#if 0 // XXX old animation system
	if (ob) {
		/* ofset frames */
		if ((ob->ipoflag & OB_OFFS_PARENT) && (ob->partype & PARSLOW)==0) 
			cfra-= give_timeoffset(ob);
	}
#endif // XXX old animation system
	
	cfra-= ofs;

	return cfra;
}

void object_scale_to_mat3(Object *ob, float mat[][3])
{
	float vec[3];
	
	vec[0]= ob->size[0]+ob->dsize[0];
	vec[1]= ob->size[1]+ob->dsize[1];
	vec[2]= ob->size[2]+ob->dsize[2];
	size_to_mat3( mat,vec);
}

// TODO: this should take rotation orders into account later...
void object_rot_to_mat3(Object *ob, float mat[][3])
{
	float rmat[3][3], dmat[3][3];
	
	/* initialise the delta-rotation matrix, which will get (pre)multiplied 
	 * with the rotation matrix to yield the appropriate rotation
	 */
	unit_m3(dmat);
	
	/* rotations may either be quats, eulers (with various rotation orders), or axis-angle */
	if (ob->rotmode > 0) {
		/* euler rotations (will cause gimble lock, but this can be alleviated a bit with rotation orders) */
		eulO_to_mat3( rmat,ob->rot, ob->rotmode);
		eulO_to_mat3( dmat,ob->drot, ob->rotmode);
	}
	else if (ob->rotmode == ROT_MODE_AXISANGLE) {
		/* axis-angle -  not really that great for 3D-changing orientations */
		axis_angle_to_mat3( rmat,ob->rotAxis, ob->rotAngle);
		axis_angle_to_mat3( dmat,ob->drotAxis, ob->drotAngle);
	}
	else {
		/* quats are normalised before use to eliminate scaling issues */
		normalize_qt(ob->quat);
		quat_to_mat3( rmat,ob->quat);
		quat_to_mat3( dmat,ob->dquat);
	}
	
	/* combine these rotations */
	// XXX is this correct? if errors, change the order of multiplication...
	mul_m3_m3m3(mat, dmat, rmat);
}

void object_mat3_to_rot(Object *ob, float mat[][3], int use_compat)
{
	if (ob->rotmode == ROT_MODE_QUAT)
		mat3_to_quat(ob->quat, mat);
	else if (ob->rotmode == ROT_MODE_AXISANGLE)
		mat3_to_axis_angle(ob->rotAxis, &ob->rotAngle, mat);
	else {
		if(use_compat) {
			float eul[3];
			VECCOPY(eul, ob->rot);
			mat3_to_compatible_eulO(ob->rot, eul, ob->rotmode, mat);
		}
		else
			mat3_to_eulO(ob->rot, ob->rotmode, mat);
	}
}

/* see pchan_apply_mat4() for the equivalent 'pchan' function */
void object_apply_mat4(Object *ob, float mat[][4])
{
	float mat3[3][3];
	VECCOPY(ob->loc, mat[3]);
	mat4_to_size(ob->size, mat);
	copy_m3_m4(mat3, mat);
	object_mat3_to_rot(ob, mat3, 0);
}

void object_to_mat3(Object *ob, float mat[][3])	/* no parent */
{
	float smat[3][3];
	float rmat[3][3];
	/*float q1[4];*/
	
	/* size */
	object_scale_to_mat3(ob, smat);

	/* rot */
	object_rot_to_mat3(ob, rmat);
	mul_m3_m3m3(mat, rmat, smat);
}

void object_to_mat4(Object *ob, float mat[][4])
{
	float tmat[3][3];
	
	object_to_mat3(ob, tmat);
	
	copy_m4_m3(mat, tmat);
	
	mat[3][0]= ob->loc[0] + ob->dloc[0];
	mat[3][1]= ob->loc[1] + ob->dloc[1];
	mat[3][2]= ob->loc[2] + ob->dloc[2];
}

int enable_cu_speed= 1;

static void ob_parcurve(Scene *scene, Object *ob, Object *par, float mat[][4])
{
	Curve *cu;
	float q[4], vec[4], dir[3], quat[4], radius, x1, ctime;
	float timeoffs = 0.0, sf_orig = 0.0;
	
	unit_m4(mat);
	
	cu= par->data;
	if(cu->path==NULL || cu->path->data==NULL) /* only happens on reload file, but violates depsgraph still... fix! */
		makeDispListCurveTypes(scene, par, 0);
	if(cu->path==NULL) return;
	
	/* exception, timeoffset is regarded as distance offset */
	if(cu->flag & CU_OFFS_PATHDIST) {
		timeoffs = give_timeoffset(ob);
		SWAP(float, sf_orig, ob->sf);
	}
	
	/* catch exceptions: feature for nla stride editing */
	if(ob->ipoflag & OB_DISABLE_PATH) {
		ctime= 0.0f;
	}
	/* catch exceptions: curve paths used as a duplicator */
	else if(enable_cu_speed) {
		/* ctime is now a proper var setting of Curve which gets set by Animato like any other var that's animated,
		 * but this will only work if it actually is animated... 
		 *
		 * we divide the curvetime calculated in the previous step by the length of the path, to get a time
		 * factor, which then gets clamped to lie within 0.0 - 1.0 range
		 */
		ctime= cu->ctime / cu->pathlen;
		CLAMP(ctime, 0.0, 1.0);
	}
	else {
		ctime= scene->r.cfra - give_timeoffset(ob);
		ctime /= cu->pathlen;
		
		CLAMP(ctime, 0.0, 1.0);
	}
	
	/* time calculus is correct, now apply distance offset */
	if(cu->flag & CU_OFFS_PATHDIST) {
		ctime += timeoffs/cu->path->totdist;

		/* restore */
		SWAP(float, sf_orig, ob->sf);
	}
	
	
	/* vec: 4 items! */
	 if( where_on_path(par, ctime, vec, dir, NULL, &radius, NULL) ) {

		if(cu->flag & CU_FOLLOW) {
			vec_to_quat( quat,dir, ob->trackflag, ob->upflag);
			
			/* the tilt */
			normalize_v3(dir);
			q[0]= (float)cos(0.5*vec[3]);
			x1= (float)sin(0.5*vec[3]);
			q[1]= -x1*dir[0];
			q[2]= -x1*dir[1];
			q[3]= -x1*dir[2];
			mul_qt_qtqt(quat, q, quat);
			
			quat_to_mat4( mat,quat);
		}
		
		if(cu->flag & CU_PATH_RADIUS) {
			float tmat[4][4], rmat[4][4];
			scale_m4_fl(tmat, radius);
			mul_m4_m4m4(rmat, mat, tmat);
			copy_m4_m4(mat, rmat);
		}

		VECCOPY(mat[3], vec);
		
	}
}

static void ob_parbone(Object *ob, Object *par, float mat[][4])
{	
	bPoseChannel *pchan;
	float vec[3];
	
	if (par->type!=OB_ARMATURE) {
		unit_m4(mat);
		return;
	}
	
	/* Make sure the bone is still valid */
	pchan= get_pose_channel(par->pose, ob->parsubstr);
	if (!pchan){
		printf ("Object %s with Bone parent: bone %s doesn't exist\n", ob->id.name+2, ob->parsubstr);
		unit_m4(mat);
		return;
	}

	/* get bone transform */
	copy_m4_m4(mat, pchan->pose_mat);

	/* but for backwards compatibility, the child has to move to the tail */
	VECCOPY(vec, mat[1]);
	mul_v3_fl(vec, pchan->bone->length);
	add_v3_v3(mat[3], vec);
}

static void give_parvert(Object *par, int nr, float *vec)
{
	EditMesh *em;
	int a, count;
	
	vec[0]=vec[1]=vec[2]= 0.0f;
	
	if(par->type==OB_MESH) {
		Mesh *me= par->data;
		em = BKE_mesh_get_editmesh(me);

		if(em) {
			EditVert *eve;
			
			for(eve= em->verts.first; eve; eve= eve->next) {
				if(eve->keyindex==nr) {
					memcpy(vec, eve->co, sizeof(float)*3);
					break;
				}
			}
			BKE_mesh_end_editmesh(me, em);
		}
		else {
			DerivedMesh *dm = par->derivedFinal;
			
			if(dm) {
				MVert *mvert= dm->getVertArray(dm);
				int *index = (int *)dm->getVertDataArray(dm, CD_ORIGINDEX);
				int i, count = 0, vindex, numVerts = dm->getNumVerts(dm);

				/* get the average of all verts with (original index == nr) */
				for(i = 0; i < numVerts; i++) {
					vindex= (index)? index[i]: i;

					if(vindex == nr) {
						add_v3_v3(vec, mvert[i].co);
						count++;
					}
				}

				if (count==0) {
					/* keep as 0,0,0 */
				} else if(count > 0) {
					mul_v3_fl(vec, 1.0f / count);
				} else {
					/* use first index if its out of range */
					dm->getVertCo(dm, 0, vec);
				}
			}
		}
	}
	else if (ELEM(par->type, OB_CURVE, OB_SURF)) {
		Nurb *nu;
		Curve *cu;
		BPoint *bp;
		BezTriple *bezt;
		int found= 0;
		
		cu= par->data;
		if(cu->editnurb)
			nu= cu->editnurb->first;
		else
			nu= cu->nurb.first;
		
		count= 0;
		while(nu && !found) {
			if(nu->type == CU_BEZIER) {
				bezt= nu->bezt;
				a= nu->pntsu;
				while(a--) {
					if(count==nr) {
						found= 1;
						VECCOPY(vec, bezt->vec[1]);
						break;
					}
					count++;
					bezt++;
				}
			}
			else {
				bp= nu->bp;
				a= nu->pntsu*nu->pntsv;
				while(a--) {
					if(count==nr) {
						found= 1;
						memcpy(vec, bp->vec, sizeof(float)*3);
						break;
					}
					count++;
					bp++;
				}
			}
			nu= nu->next;
		}

	}
	else if(par->type==OB_LATTICE) {
		Lattice *latt= par->data;
		BPoint *bp;
		DispList *dl = find_displist(&par->disp, DL_VERTS);
		float *co = dl?dl->verts:NULL;
		
		if(latt->editlatt) latt= latt->editlatt;
		
		a= latt->pntsu*latt->pntsv*latt->pntsw;
		count= 0;
		bp= latt->def;
		while(a--) {
			if(count==nr) {
				if(co)
					memcpy(vec, co, 3*sizeof(float));
				else
					memcpy(vec, bp->vec, 3*sizeof(float));
				break;
			}
			count++;
			if(co) co+= 3;
			else bp++;
		}
	}
}

static void ob_parvert3(Object *ob, Object *par, float mat[][4])
{
	float cmat[3][3], v1[3], v2[3], v3[3], q[4];

	/* in local ob space */
	unit_m4(mat);
	
	if (ELEM4(par->type, OB_MESH, OB_SURF, OB_CURVE, OB_LATTICE)) {
		
		give_parvert(par, ob->par1, v1);
		give_parvert(par, ob->par2, v2);
		give_parvert(par, ob->par3, v3);
				
		tri_to_quat( q,v1, v2, v3);
		quat_to_mat3( cmat,q);
		copy_m4_m3(mat, cmat);
		
		if(ob->type==OB_CURVE) {
			VECCOPY(mat[3], v1);
		}
		else {
			add_v3_v3v3(mat[3], v1, v2);
			add_v3_v3(mat[3], v3);
			mul_v3_fl(mat[3], 0.3333333f);
		}
	}
}

// XXX what the hell is this?
static int no_parent_ipo=0;
void set_no_parent_ipo(int val)
{
	no_parent_ipo= val;
}

void where_is_object_time(Scene *scene, Object *ob, float ctime)
{
	float *fp1, *fp2, slowmat[4][4] = MAT4_UNITY;
	float stime=ctime, fac1, fac2;
	int a;
	
	/* new version: correct parent+vertexparent and track+parent */
	/* this one only calculates direct attached parent and track */
	/* is faster, but should keep track of timeoffs */
	
	if(ob==NULL) return;
	
	/* execute drivers only, as animation has already been done */
	BKE_animsys_evaluate_animdata(&ob->id, ob->adt, ctime, ADT_RECALC_DRIVERS);
	
	if(ob->parent) {
		Object *par= ob->parent;
		
		// XXX depreceated - animsys
		if(ob->ipoflag & OB_OFFS_PARENT) ctime-= give_timeoffset(ob);
		
		/* hurms, code below conflicts with depgraph... (ton) */
		/* and even worse, it gives bad effects for NLA stride too (try ctime != par->ctime, with MBlur) */
		if(no_parent_ipo==0 && stime != par->ctime) {
			// only for ipo systems? 
			Object tmp= *par;
			
			if(par->proxy_from);	// was a copied matrix, no where_is! bad...
			else where_is_object_time(scene, par, ctime);

			solve_parenting(scene, ob, par, ob->obmat, slowmat, 0);

			*par= tmp;
		}
		else
			solve_parenting(scene, ob, par, ob->obmat, slowmat, 0);
		
		if(ob->partype & PARSLOW) {
			// include framerate
			fac1= ( 1.0f / (1.0f + (float)fabs(give_timeoffset(ob))) );
			if(fac1 >= 1.0f) return;
			fac2= 1.0f-fac1;
			
			fp1= ob->obmat[0];
			fp2= slowmat[0];
			for(a=0; a<16; a++, fp1++, fp2++) {
				fp1[0]= fac1*fp1[0] + fac2*fp2[0];
			}
		}
	}
	else {
		object_to_mat4(ob, ob->obmat);
	}

	/* solve constraints */
	if (ob->constraints.first && !(ob->flag & OB_NO_CONSTRAINTS)) {
		bConstraintOb *cob;
		
		cob= constraints_make_evalob(scene, ob, NULL, CONSTRAINT_OBTYPE_OBJECT);
		
		/* constraints need ctime, not stime. Some call where_is_object_time and bsystem_time */
		solve_constraints (&ob->constraints, cob, ctime);
		
		constraints_clear_evalob(cob);
	}
	
	/* set negative scale flag in object */
	if(is_negative_m4(ob->obmat))	ob->transflag |= OB_NEG_SCALE;
	else							ob->transflag &= ~OB_NEG_SCALE;
}

static void solve_parenting (Scene *scene, Object *ob, Object *par, float obmat[][4], float slowmat[][4], int simul)
{
	float totmat[4][4];
	float tmat[4][4];
	float locmat[4][4];
	float vec[3];
	int ok;
	
	object_to_mat4(ob, locmat);
	
	if(ob->partype & PARSLOW) copy_m4_m4(slowmat, obmat);

	switch(ob->partype & PARTYPE) {
	case PAROBJECT:
		ok= 0;
		if(par->type==OB_CURVE) {
			if( ((Curve *)par->data)->flag & CU_PATH ) {
				ob_parcurve(scene, ob, par, tmat);
				ok= 1;
			}
		}
		
		if(ok) mul_serie_m4(totmat, par->obmat, tmat, 
			NULL, NULL, NULL, NULL, NULL, NULL);
		else copy_m4_m4(totmat, par->obmat);
		
		break;
	case PARBONE:
		ob_parbone(ob, par, tmat);
		mul_serie_m4(totmat, par->obmat, tmat,         
			NULL, NULL, NULL, NULL, NULL, NULL);
		break;
		
	case PARVERT1:
		unit_m4(totmat);
		if (simul){
			VECCOPY(totmat[3], par->obmat[3]);
		}
		else{
			give_parvert(par, ob->par1, vec);
			mul_v3_m4v3(totmat[3], par->obmat, vec);
		}
		break;
	case PARVERT3:
		ob_parvert3(ob, par, tmat);
		
		mul_serie_m4(totmat, par->obmat, tmat,         
			NULL, NULL, NULL, NULL, NULL, NULL);
		break;
		
	case PARSKEL:
		copy_m4_m4(totmat, par->obmat);
		break;
	}
	
	// total 
	mul_serie_m4(tmat, totmat, ob->parentinv,         
		NULL, NULL, NULL, NULL, NULL, NULL);
	mul_serie_m4(obmat, tmat, locmat,         
		NULL, NULL, NULL, NULL, NULL, NULL);
	
	if (simul) {

	}
	else{
		// external usable originmat 
		copy_m3_m4(originmat, tmat);
		
		// origin, voor help line
		if( (ob->partype & 15)==PARSKEL ) {
			VECCOPY(ob->orig, par->obmat[3]);
		}
		else {
			VECCOPY(ob->orig, totmat[3]);
		}
	}

}

void where_is_object(struct Scene *scene, Object *ob)
{
	where_is_object_time(scene, ob, (float)scene->r.cfra);
}


void where_is_object_simul(Scene *scene, Object *ob)
/* was written for the old game engine (until 2.04) */
/* It seems that this function is only called
for a lamp that is the child of another object */
{
	Object *par;
	//Ipo *ipo;
	float *fp1, *fp2;
	float slowmat[4][4];
	float fac1, fac2;
	int a;
	
	/* NO TIMEOFFS */
	if(ob->parent) {
		par= ob->parent;
		
		solve_parenting(scene, ob, par, ob->obmat, slowmat, 1);

		if(ob->partype & PARSLOW) {

			fac1= (float)(1.0/(1.0+ fabs(give_timeoffset(ob))));
			fac2= 1.0f-fac1;
			fp1= ob->obmat[0];
			fp2= slowmat[0];
			for(a=0; a<16; a++, fp1++, fp2++) {
				fp1[0]= fac1*fp1[0] + fac2*fp2[0];
			}
		}
		
	}
	else {
		object_to_mat4(ob, ob->obmat);
	}
	
	/* solve constraints */
	if (ob->constraints.first) {
		bConstraintOb *cob;
		
		cob= constraints_make_evalob(scene, ob, NULL, CONSTRAINT_OBTYPE_OBJECT);
		solve_constraints(&ob->constraints, cob, (float)scene->r.cfra);
		constraints_clear_evalob(cob);
	}
}

/* for calculation of the inverse parent transform, only used for editor */
void what_does_parent(Scene *scene, Object *ob, Object *workob)
{
	clear_workob(workob);
	
	unit_m4(workob->obmat);
	unit_m4(workob->parentinv);
	unit_m4(workob->constinv);
	workob->parent= ob->parent;

	workob->trackflag= ob->trackflag;
	workob->upflag= ob->upflag;
	
	workob->partype= ob->partype;
	workob->par1= ob->par1;
	workob->par2= ob->par2;
	workob->par3= ob->par3;

	workob->constraints.first = ob->constraints.first;
	workob->constraints.last = ob->constraints.last;

	strcpy(workob->parsubstr, ob->parsubstr); 

	where_is_object(scene, workob);
}

BoundBox *unit_boundbox()
{
	BoundBox *bb;
	float min[3] = {-1.0f,-1.0f,-1.0f}, max[3] = {-1.0f,-1.0f,-1.0f};

	bb= MEM_callocN(sizeof(BoundBox), "bb");
	boundbox_set_from_min_max(bb, min, max);
	
	return bb;
}

void boundbox_set_from_min_max(BoundBox *bb, float min[3], float max[3])
{
	bb->vec[0][0]=bb->vec[1][0]=bb->vec[2][0]=bb->vec[3][0]= min[0];
	bb->vec[4][0]=bb->vec[5][0]=bb->vec[6][0]=bb->vec[7][0]= max[0];
	
	bb->vec[0][1]=bb->vec[1][1]=bb->vec[4][1]=bb->vec[5][1]= min[1];
	bb->vec[2][1]=bb->vec[3][1]=bb->vec[6][1]=bb->vec[7][1]= max[1];

	bb->vec[0][2]=bb->vec[3][2]=bb->vec[4][2]=bb->vec[7][2]= min[2];
	bb->vec[1][2]=bb->vec[2][2]=bb->vec[5][2]=bb->vec[6][2]= max[2];
}

BoundBox *object_get_boundbox(Object *ob)
{
	BoundBox *bb= NULL;
	
	if(ob->type==OB_MESH) {
		bb = mesh_get_bb(ob);
	}
	else if (ELEM3(ob->type, OB_CURVE, OB_SURF, OB_FONT)) {
		bb= ob->bb ? ob->bb : ( (Curve *)ob->data )->bb;
	}
	else if(ob->type==OB_MBALL) {
		bb= ob->bb;
	}
	return bb;
}

/* used to temporally disable/enable boundbox */
void object_boundbox_flag(Object *ob, int flag, int set)
{
	BoundBox *bb= object_get_boundbox(ob);
	if(bb) {
		if(set) bb->flag |= flag;
		else bb->flag &= ~flag;
	}
}

void object_get_dimensions(Object *ob, float *value)
{
	BoundBox *bb = NULL;
	
	bb= object_get_boundbox(ob);
	if (bb) {
		float scale[3];
		
		mat4_to_size( scale,ob->obmat);
		
		value[0] = fabs(scale[0]) * (bb->vec[4][0] - bb->vec[0][0]);
		value[1] = fabs(scale[1]) * (bb->vec[2][1] - bb->vec[0][1]);
		value[2] = fabs(scale[2]) * (bb->vec[1][2] - bb->vec[0][2]);
	} else {
		value[0] = value[1] = value[2] = 0.f;
	}
}

void object_set_dimensions(Object *ob, const float *value)
{
	BoundBox *bb = NULL;
	
	bb= object_get_boundbox(ob);
	if (bb) {
		float scale[3], len[3];
		
		mat4_to_size( scale,ob->obmat);
		
		len[0] = bb->vec[4][0] - bb->vec[0][0];
		len[1] = bb->vec[2][1] - bb->vec[0][1];
		len[2] = bb->vec[1][2] - bb->vec[0][2];
		
		if (len[0] > 0.f) ob->size[0] = value[0] / len[0];
		if (len[1] > 0.f) ob->size[1] = value[1] / len[1];
		if (len[2] > 0.f) ob->size[2] = value[2] / len[2];
	}
}

void minmax_object(Object *ob, float *min, float *max)
{
	BoundBox bb;
	Mesh *me;
	Curve *cu;
	float vec[3];
	int a;
	
	switch(ob->type) {
		
	case OB_CURVE:
	case OB_FONT:
	case OB_SURF:
		cu= ob->data;
		
		if(cu->bb==NULL) tex_space_curve(cu);
		bb= *(cu->bb);
		
		for(a=0; a<8; a++) {
			mul_m4_v3(ob->obmat, bb.vec[a]);
			DO_MINMAX(bb.vec[a], min, max);
		}
		break;
	case OB_ARMATURE:
		if(ob->pose) {
			bPoseChannel *pchan;
			for(pchan= ob->pose->chanbase.first; pchan; pchan= pchan->next) {
				VECCOPY(vec, pchan->pose_head);
				mul_m4_v3(ob->obmat, vec);
				DO_MINMAX(vec, min, max);
				VECCOPY(vec, pchan->pose_tail);
				mul_m4_v3(ob->obmat, vec);
				DO_MINMAX(vec, min, max);
			}
			break;
		}
		/* no break, get_mesh will give NULL and it passes on to default */
	case OB_MESH:
		me= get_mesh(ob);
		
		if(me) {
			bb = *mesh_get_bb(ob);
			
			for(a=0; a<8; a++) {
				mul_m4_v3(ob->obmat, bb.vec[a]);
				DO_MINMAX(bb.vec[a], min, max);
			}
		}
		if(min[0] < max[0] ) break;
		
		/* else here no break!!!, mesh can be zero sized */
		
	default:
		DO_MINMAX(ob->obmat[3], min, max);

		VECCOPY(vec, ob->obmat[3]);
		add_v3_v3(vec, ob->size);
		DO_MINMAX(vec, min, max);

		VECCOPY(vec, ob->obmat[3]);
		sub_v3_v3v3(vec, vec, ob->size);
		DO_MINMAX(vec, min, max);
		break;
	}
}

int minmax_object_duplis(Scene *scene, Object *ob, float *min, float *max)
{
	int ok= 0;
	if ((ob->transflag & OB_DUPLI)==0) {
		return ok;
	} else {
		ListBase *lb;
		DupliObject *dob;
		
		lb= object_duplilist(scene, ob);
		for(dob= lb->first; dob; dob= dob->next) {
			if(dob->no_draw == 0) {
				BoundBox *bb= object_get_boundbox(dob->ob);

				if(bb) {
					int i;
					for(i=0; i<8; i++) {
						float vec[3];
						mul_v3_m4v3(vec, dob->mat, bb->vec[i]);
						DO_MINMAX(vec, min, max);
					}

					ok= 1;
				}
			}
		}
		free_object_duplilist(lb);	/* does restore */
	}

	return ok;
}

/* copied from DNA_object_types.h */
typedef struct ObTfmBack {
	float loc[3], dloc[3], orig[3];
	float size[3], dsize[3];	/* scale and delta scale */
	float rot[3], drot[3];		/* euler rotation */
	float quat[4], dquat[4];	/* quaternion rotation */
	float rotAxis[3], drotAxis[3];	/* axis angle rotation - axis part */
	float rotAngle, drotAngle;	/* axis angle rotation - angle part */
	float obmat[4][4];		/* final worldspace matrix with constraints & animsys applied */
	float parentinv[4][4]; /* inverse result of parent, so that object doesn't 'stick' to parent */
	float constinv[4][4]; /* inverse result of constraints. doesn't include effect of parent or object local transform */
	float imat[4][4];	/* inverse matrix of 'obmat' for during render, old game engine, temporally: ipokeys of transform  */
} ObTfmBack;

void *object_tfm_backup(Object *ob)
{
	ObTfmBack *obtfm= MEM_mallocN(sizeof(ObTfmBack), "ObTfmBack");
	copy_v3_v3(obtfm->loc, ob->loc);
	copy_v3_v3(obtfm->dloc, ob->dloc);
	copy_v3_v3(obtfm->orig, ob->orig);
	copy_v3_v3(obtfm->size, ob->size);
	copy_v3_v3(obtfm->dsize, ob->dsize);
	copy_v3_v3(obtfm->rot, ob->rot);
	copy_v3_v3(obtfm->drot, ob->drot);
	copy_qt_qt(obtfm->quat, ob->quat);
	copy_qt_qt(obtfm->dquat, ob->dquat);
	copy_v3_v3(obtfm->rotAxis, ob->rotAxis);
	copy_v3_v3(obtfm->drotAxis, ob->drotAxis);
	obtfm->rotAngle= ob->rotAngle;
	obtfm->drotAngle= ob->drotAngle;
	copy_m4_m4(obtfm->obmat, ob->obmat);
	copy_m4_m4(obtfm->parentinv, ob->parentinv);
	copy_m4_m4(obtfm->constinv, ob->constinv);
	copy_m4_m4(obtfm->imat, ob->imat);

	return (void *)obtfm;
}

void object_tfm_restore(Object *ob, void *obtfm_pt)
{
	ObTfmBack *obtfm= (ObTfmBack *)obtfm_pt;
	copy_v3_v3(ob->loc, obtfm->loc);
	copy_v3_v3(ob->dloc, obtfm->dloc);
	copy_v3_v3(ob->orig, obtfm->orig);
	copy_v3_v3(ob->size, obtfm->size);
	copy_v3_v3(ob->dsize, obtfm->dsize);
	copy_v3_v3(ob->rot, obtfm->rot);
	copy_v3_v3(ob->drot, obtfm->drot);
	copy_qt_qt(ob->quat, obtfm->quat);
	copy_qt_qt(ob->dquat, obtfm->dquat);
	copy_v3_v3(ob->rotAxis, obtfm->rotAxis);
	copy_v3_v3(ob->drotAxis, obtfm->drotAxis);
	ob->rotAngle= obtfm->rotAngle;
	ob->drotAngle= obtfm->drotAngle;
	copy_m4_m4(ob->obmat, obtfm->obmat);
	copy_m4_m4(ob->parentinv, obtfm->parentinv);
	copy_m4_m4(ob->constinv, obtfm->constinv);
	copy_m4_m4(ob->imat, obtfm->imat);
}

/* proxy rule: lib_object->proxy_from == the one we borrow from, only set temporal and cleared here */
/*           local_object->proxy      == pointer to library object, saved in files and read */

/* function below is polluted with proxy exceptions, cleanup will follow! */

/* the main object update call, for object matrix, constraints, keys and displist (modifiers) */
/* requires flags to be set! */
void object_handle_update(Scene *scene, Object *ob)
{
	if(ob->recalc & OB_RECALC) {
		/* speed optimization for animation lookups */
		if(ob->pose)
			make_pose_channels_hash(ob->pose);

		/* XXX new animsys warning: depsgraph tag OB_RECALC_DATA should not skip drivers, 
		   which is only in where_is_object now */
		if(ob->recalc & OB_RECALC) {
			
			if (G.f & G_DEBUG)
				printf("recalcob %s\n", ob->id.name+2);
			
			/* handle proxy copy for target */
			if(ob->id.lib && ob->proxy_from) {
				// printf("ob proxy copy, lib ob %s proxy %s\n", ob->id.name, ob->proxy_from->id.name);
				if(ob->proxy_from->proxy_group) {/* transform proxy into group space */
					Object *obg= ob->proxy_from->proxy_group;
					invert_m4_m4(obg->imat, obg->obmat);
					mul_m4_m4m4(ob->obmat, ob->proxy_from->obmat, obg->imat);
				}
				else
					copy_m4_m4(ob->obmat, ob->proxy_from->obmat);
			}
			else
				where_is_object(scene, ob);
		}
		
		if(ob->recalc & OB_RECALC_DATA) {
			ID *data_id= (ID *)ob->data;
			AnimData *adt= BKE_animdata_from_id(data_id);
			float ctime= (float)scene->r.cfra; // XXX this is bad...
			ListBase pidlist;
			PTCacheID *pid;
			
			if (G.f & G_DEBUG)
				printf("recalcdata %s\n", ob->id.name+2);
			
			/* includes all keys and modifiers */
			if(ob->type==OB_MESH) {
				EditMesh *em = BKE_mesh_get_editmesh(ob->data);
				
				/* evaluate drivers */
				// XXX: should we push this to derivedmesh instead?
				BKE_animsys_evaluate_animdata(data_id, adt, ctime, ADT_RECALC_DRIVERS);
				
					// here was vieweditdatamask? XXX
				if(em) {
					makeDerivedMesh(scene, ob, em, CD_MASK_BAREMESH);
					BKE_mesh_end_editmesh(ob->data, em);
				} else
					makeDerivedMesh(scene, ob, NULL, CD_MASK_BAREMESH);
			}
			else if(ob->type==OB_MBALL) {
				makeDispListMBall(scene, ob);
			} 
			else if(ELEM3(ob->type, OB_CURVE, OB_SURF, OB_FONT)) {
				makeDispListCurveTypes(scene, ob, 0);
			}
			else if(ELEM(ob->type, OB_CAMERA, OB_LAMP)) {
				/* evaluate drivers */
				BKE_animsys_evaluate_animdata(data_id, adt, ctime, ADT_RECALC_DRIVERS);
			}
			else if(ob->type==OB_LATTICE) {
				lattice_calc_modifiers(scene, ob);
			}
			else if(ob->type==OB_ARMATURE) {
				/* this happens for reading old files and to match library armatures with poses */
				// XXX this won't screw up the pose set already...
				if(ob->pose==NULL || (ob->pose->flag & POSE_RECALC))
					armature_rebuild_pose(ob, ob->data);
				
				/* evaluate drivers */
				BKE_animsys_evaluate_animdata(data_id, adt, ctime, ADT_RECALC_DRIVERS);
				
				if(ob->id.lib && ob->proxy_from) {
					copy_pose_result(ob->pose, ob->proxy_from->pose);
					// printf("pose proxy copy, lib ob %s proxy %s\n", ob->id.name, ob->proxy_from->id.name);
				}
				else {
					where_is_pose(scene, ob);
				}
			}

			if(ob->particlesystem.first) {
				ParticleSystem *tpsys, *psys;
				DerivedMesh *dm;
				ob->transflag &= ~OB_DUPLIPARTS;
				
				psys= ob->particlesystem.first;
				while(psys) {
					if(psys_check_enabled(ob, psys)) {
						/* check use of dupli objects here */
						if(psys->part && psys->part->draw_as == PART_DRAW_REND &&
							((psys->part->ren_as == PART_DRAW_OB && psys->part->dup_ob)
							|| (psys->part->ren_as == PART_DRAW_GR && psys->part->dup_group)))
							ob->transflag |= OB_DUPLIPARTS;

						particle_system_update(scene, ob, psys);
						psys= psys->next;
					}
					else if(psys->flag & PSYS_DELETE) {
						tpsys=psys->next;
						BLI_remlink(&ob->particlesystem, psys);
						psys_free(ob,psys);
						psys= tpsys;
					}
					else
						psys= psys->next;
				}

				if(G.rendering && ob->transflag & OB_DUPLIPARTS) {
					/* this is to make sure we get render level duplis in groups:
					 * the derivedmesh must be created before init_render_mesh,
					 * since object_duplilist does dupliparticles before that */
					dm = mesh_create_derived_render(scene, ob, CD_MASK_BAREMESH|CD_MASK_MTFACE|CD_MASK_MCOL);
					dm->release(dm);

					for(psys=ob->particlesystem.first; psys; psys=psys->next)
						psys_get_modifier(ob, psys)->flag &= ~eParticleSystemFlag_psys_updated;
				}
			}

			/* check if quick cache is needed */
			BKE_ptcache_ids_from_object(&pidlist, ob, scene, MAX_DUPLI_RECUR);

			for(pid=pidlist.first; pid; pid=pid->next) {
				if((pid->cache->flag & PTCACHE_BAKED)
					|| (pid->cache->flag & PTCACHE_QUICK_CACHE)==0)
					continue;

				if(pid->cache->flag & PTCACHE_OUTDATED || (pid->cache->flag & PTCACHE_SIMULATION_VALID)==0) {
					scene->physics_settings.quick_cache_step =
						scene->physics_settings.quick_cache_step ?
						MIN2(scene->physics_settings.quick_cache_step, pid->cache->step) :
						pid->cache->step;
				}
			}

			BLI_freelistN(&pidlist);
		}

		/* the no-group proxy case, we call update */
		if(ob->proxy && ob->proxy_group==NULL) {
			/* set pointer in library proxy target, for copying, but restore it */
			ob->proxy->proxy_from= ob;
			// printf("call update, lib ob %s proxy %s\n", ob->proxy->id.name, ob->id.name);
			object_handle_update(scene, ob->proxy);
		}
	
		ob->recalc &= ~OB_RECALC;
	}

	/* the case when this is a group proxy, object_update is called in group.c */
	if(ob->proxy) {
		ob->proxy->proxy_from= ob;
		// printf("set proxy pointer for later group stuff %s\n", ob->id.name);
	}
}

float give_timeoffset(Object *ob) {
	if ((ob->ipoflag & OB_OFFS_PARENTADD) && ob->parent) {
		return ob->sf + give_timeoffset(ob->parent);
	} else {
		return ob->sf;
	}
}

int give_obdata_texspace(Object *ob, short **texflag, float **loc, float **size, float **rot) {
	
	if (ob->data==NULL)
		return 0;
	
	switch (GS(((ID *)ob->data)->name)) {
	case ID_ME:
	{
		Mesh *me= ob->data;
		if (texflag)	*texflag = &me->texflag;
		if (loc)		*loc = me->loc;
		if (size)		*size = me->size;
		if (rot)		*rot = me->rot;
		break;
	}
	case ID_CU:
	{
		Curve *cu= ob->data;
		if (texflag)	*texflag = &cu->texflag;
		if (loc)		*loc = cu->loc;
		if (size)		*size = cu->size;
		if (rot)		*rot = cu->rot;
		break;
	}
	case ID_MB:
	{
		MetaBall *mb= ob->data;
		if (texflag)	*texflag = &mb->texflag;
		if (loc)		*loc = mb->loc;
		if (size)		*size = mb->size;
		if (rot)		*rot = mb->rot;
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
int ray_hit_boundbox(struct BoundBox *bb, float ray_start[3], float ray_normal[3])
{
	static int triangle_indexes[12][3] = {{0, 1, 2}, {0, 2, 3},
										  {3, 2, 6}, {3, 6, 7},
										  {1, 2, 6}, {1, 6, 5}, 
										  {5, 6, 7}, {4, 5, 7},
										  {0, 3, 7}, {0, 4, 7},
										  {0, 1, 5}, {0, 4, 5}};
	int result = 0;
	int i;
	
	for (i = 0; i < 12 && result == 0; i++)
	{
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
	if(GET_INT_FROM_POINTER(ad->data) > GET_INT_FROM_POINTER(bd->data))
		return 1;
	else return 0;
}

int object_insert_ptcache(Object *ob) 
{
	LinkData *link = NULL;
	int i = 0;

	BLI_sortlist(&ob->pc_ids, pc_cmp);

	for(link=ob->pc_ids.first, i = 0; link; link=link->next, i++) 
	{
		int index = GET_INT_FROM_POINTER(link->data);

		if(i < index)
			break;
	}

	link = MEM_callocN(sizeof(LinkData), "PCLink");
	link->data = SET_INT_IN_POINTER(i);
	BLI_addtail(&ob->pc_ids, link);

	return i;
}

/* 'lens' may be set for envmap only */
void object_camera_matrix(
		RenderData *rd, Object *camera, int winx, int winy, short field_second,
		float winmat[][4], rctf *viewplane, float *clipsta, float *clipend, float *lens, float *ycor,
		float *viewdx, float *viewdy
) {
	Camera *cam=NULL;
	float pixsize;
	float shiftx=0.0, shifty=0.0, winside, viewfac;

	rd->mode &= ~R_ORTHO;

	/* question mark */
	(*ycor)= rd->yasp / rd->xasp;
	if(rd->mode & R_FIELDS)
		(*ycor) *= 2.0f;

	if(camera->type==OB_CAMERA) {
		cam= camera->data;

		if(cam->type==CAM_ORTHO) rd->mode |= R_ORTHO;
		if(cam->flag & CAM_PANORAMA) rd->mode |= R_PANORAMA;

		/* solve this too... all time depending stuff is in convertblender.c?
		 * Need to update the camera early because it's used for projection matrices
		 * and other stuff BEFORE the animation update loop is done
		 * */
#if 0 // XXX old animation system
		if(cam->ipo) {
			calc_ipo(cam->ipo, frame_to_float(re->scene, re->r.cfra));
			execute_ipo(&cam->id, cam->ipo);
		}
#endif // XXX old animation system
		shiftx=cam->shiftx;
		shifty=cam->shifty;
		(*lens)= cam->lens;
		(*clipsta)= cam->clipsta;
		(*clipend)= cam->clipend;
	}
	else if(camera->type==OB_LAMP) {
		Lamp *la= camera->data;
		float fac= cos( M_PI*la->spotsize/360.0 );
		float phi= acos(fac);

		(*lens)= 16.0*fac/sin(phi);
		if((*lens)==0.0f)
			(*lens)= 35.0;
		(*clipsta)= la->clipsta;
		(*clipend)= la->clipend;
	}
	else {	/* envmap exception... */;
		if((*lens)==0.0f)
			(*lens)= 16.0;

		if((*clipsta)==0.0f || (*clipend)==0.0f) {
			(*clipsta)= 0.1f;
			(*clipend)= 1000.0f;
		}
	}

	/* ortho only with camera available */
	if(cam && rd->mode & R_ORTHO) {
		if(rd->xasp*winx >= rd->yasp*winy) {
			viewfac= winx;
		}
		else {
			viewfac= (*ycor) * winy;
		}
		/* ortho_scale == 1.0 means exact 1 to 1 mapping */
		pixsize= cam->ortho_scale/viewfac;
	}
	else {
		if(rd->xasp*winx >= rd->yasp*winy)	viewfac= ((*lens) * winx)/32.0;
		else								viewfac= (*ycor) * ((*lens) * winy)/32.0;
		pixsize= (*clipsta) / viewfac;
	}

	/* viewplane fully centered, zbuffer fills in jittered between -.5 and +.5 */
	winside= MAX2(winx, winy);
	viewplane->xmin= -0.5f*(float)winx + shiftx*winside;
	viewplane->ymin= -0.5f*(*ycor)*(float)winy + shifty*winside;
	viewplane->xmax=  0.5f*(float)winx + shiftx*winside;
	viewplane->ymax=  0.5f*(*ycor)*(float)winy + shifty*winside;

	if(field_second) {
		if(rd->mode & R_ODDFIELD) {
			viewplane->ymin-= 0.5 * (*ycor);
			viewplane->ymax-= 0.5 * (*ycor);
		}
		else {
			viewplane->ymin+= 0.5 * (*ycor);
			viewplane->ymax+= 0.5 * (*ycor);
		}
	}
	/* the window matrix is used for clipping, and not changed during OSA steps */
	/* using an offset of +0.5 here would give clip errors on edges */
	viewplane->xmin *= pixsize;
	viewplane->xmax *= pixsize;
	viewplane->ymin *= pixsize;
	viewplane->ymax *= pixsize;

	(*viewdx)= pixsize;
	(*viewdy)= (*ycor) * pixsize;

	if(rd->mode & R_ORTHO)
		orthographic_m4(winmat, viewplane->xmin, viewplane->xmax, viewplane->ymin, viewplane->ymax, *clipsta, *clipend);
	else
		perspective_m4(winmat, viewplane->xmin, viewplane->xmax, viewplane->ymin, viewplane->ymax, *clipsta, *clipend);

}

#if 0
static int pc_findindex(ListBase *listbase, int index)
{
	LinkData *link= NULL;
	int number= 0;
	
	if (listbase == NULL) return -1;
	
	link= listbase->first;
	while (link) {
		if ((int)link->data == index)
			return number;
		
		number++;
		link= link->next;
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
static KeyBlock *insert_meshkey(Scene *scene, Object *ob, char *name, int from_mix)
{
	Mesh *me= ob->data;
	Key *key= me->key;
	KeyBlock *kb;
	int newkey= 0;

	if(key == NULL) {
		key= me->key= add_key((ID *)me);
		key->type= KEY_RELATIVE;
		newkey= 1;
	}

	kb= add_keyblock(key, name);

	if(newkey || from_mix==FALSE) {
		/* create from mesh */
		mesh_to_key(me, kb);
	}
	else {
		/* copy from current values */
		kb->data= do_ob_key(scene, ob);
		kb->totelem= me->totvert;
	}

	return kb;
}
/************************* Lattice ************************/
static KeyBlock *insert_lattkey(Scene *scene, Object *ob, char *name, int from_mix)
{
	Lattice *lt= ob->data;
	Key *key= lt->key;
	KeyBlock *kb;
	int newkey= 0;

	if(key==NULL) {
		key= lt->key= add_key( (ID *)lt);
		key->type= KEY_RELATIVE;
		newkey= 1;
	}

	kb= add_keyblock(key, name);

	if(newkey || from_mix==FALSE) {
		/* create from lattice */
		latt_to_key(lt, kb);
	}
	else {
		/* copy from current values */
		kb->totelem= lt->pntsu*lt->pntsv*lt->pntsw;
		kb->data= do_ob_key(scene, ob);
	}

	return kb;
}
/************************* Curve ************************/
static KeyBlock *insert_curvekey(Scene *scene, Object *ob, char *name, int from_mix)
{
	Curve *cu= ob->data;
	Key *key= cu->key;
	KeyBlock *kb;
	ListBase *lb= (cu->editnurb)? cu->editnurb: &cu->nurb;
	int newkey= 0;

	if(key==NULL) {
		key= cu->key= add_key( (ID *)cu);
		key->type = KEY_RELATIVE;
		newkey= 1;
	}

	kb= add_keyblock(key, name);

	if(newkey || from_mix==FALSE) {
		/* create from curve */
		curve_to_key(cu, kb, lb);
	}
	else {
		/* copy from current values */
		kb->totelem= count_curveverts(lb);
		kb->data= do_ob_key(scene, ob);
	}

	return kb;
}

KeyBlock *object_insert_shape_key(Scene *scene, Object *ob, char *name, int from_mix)
{
	if(ob->type==OB_MESH)					 return insert_meshkey(scene, ob, name, from_mix);
	else if ELEM(ob->type, OB_CURVE, OB_SURF)return insert_curvekey(scene, ob, name, from_mix);
	else if(ob->type==OB_LATTICE)			 return insert_lattkey(scene, ob, name, from_mix);
	else									 return NULL;
}

