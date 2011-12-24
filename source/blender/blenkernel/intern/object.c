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

#include "BLI_blenlib.h"
#include "BLI_bpath.h"
#include "BLI_editVert.h"
#include "BLI_math.h"
#include "BLI_pbvh.h"
#include "BLI_utildefines.h"

#include "BKE_main.h"
#include "BKE_global.h"
#include "BKE_idprop.h"
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
#include "BKE_lamp.h"
#include "BKE_lattice.h"
#include "BKE_library.h"
#include "BKE_mesh.h"
#include "BKE_mball.h"
#include "BKE_modifier.h"
#include "BKE_node.h"
#include "BKE_object.h"
#include "BKE_paint.h"
#include "BKE_particle.h"
#include "BKE_pointcache.h"
#include "BKE_property.h"
#include "BKE_sca.h"
#include "BKE_scene.h"
#include "BKE_sequencer.h"
#include "BKE_speaker.h"
#include "BKE_softbody.h"
#include "BKE_material.h"
#include "BKE_camera.h"

#include "LBM_fluidsim.h"

#ifdef WITH_PYTHON
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
	workob->dscale[0]= workob->dscale[1]= workob->dscale[2]= 1.0f;
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

	copy_object_particlesystems(ob, from);
	copy_object_softbody(ob, from);

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

void free_sculptsession_deformMats(SculptSession *ss)
{
	if(ss->orig_cos) MEM_freeN(ss->orig_cos);
	if(ss->deform_cos) MEM_freeN(ss->deform_cos);
	if(ss->deform_imats) MEM_freeN(ss->deform_imats);

	ss->orig_cos = NULL;
	ss->deform_cos = NULL;
	ss->deform_imats = NULL;
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

		if(ss->orig_cos)
			MEM_freeN(ss->orig_cos);
		if(ss->deform_cos)
			MEM_freeN(ss->deform_cos);
		if(ss->deform_imats)
			MEM_freeN(ss->deform_imats);

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
		ob->data= NULL;
	}
	
	for(a=0; a<ob->totcol; a++) {
		if(ob->mat[a]) ob->mat[a]->id.us--;
	}
	if(ob->mat) MEM_freeN(ob->mat);
	if(ob->matbits) MEM_freeN(ob->matbits);
	ob->mat= NULL;
	ob->matbits= NULL;
	if(ob->bb) MEM_freeN(ob->bb); 
	ob->bb= NULL;
	if(ob->adt) BKE_free_animdata((ID *)ob);
	if(ob->poselib) ob->poselib->id.us--;
	if(ob->gpd) ((ID *)ob->gpd)->us--;
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
		ob->recalc |= OB_RECALC_OB|OB_RECALC_DATA|OB_RECALC_TIME; // XXX: should this just be OB_RECALC_DATA?
	}
}

void unlink_object(Object *ob)
{
	Main *bmain= G.main;
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
	ARegion *ar;
	RegionView3D *rv3d;
	int a, found;
	
	unlink_controllers(&ob->controllers);
	unlink_actuators(&ob->actuators);
	
	/* check all objects: parents en bevels and fields, also from libraries */
	// FIXME: need to check all animation blocks (drivers)
	obt= bmain->object.first;
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
			obt->recalc |= OB_RECALC_OB|OB_RECALC_DATA|OB_RECALC_TIME;
		}
		
		modifiers_foreachObjectLink(obt, unlink_object__unlinkModifierLinks, ob);
		
		if ELEM(obt->type, OB_CURVE, OB_FONT) {
			cu= obt->data;

			if(cu->bevobj==ob) {
				cu->bevobj= NULL;
				obt->recalc |= OB_RECALC_OB|OB_RECALC_DATA|OB_RECALC_TIME;
			}
			if(cu->taperobj==ob) {
				cu->taperobj= NULL;
				obt->recalc |= OB_RECALC_OB|OB_RECALC_DATA|OB_RECALC_TIME;
			}
			if(cu->textoncurve==ob) {
				cu->textoncurve= NULL;
				obt->recalc |= OB_RECALC_OB|OB_RECALC_DATA|OB_RECALC_TIME;
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
								ct->subtarget[0]= '\0';
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
		} else if(ELEM(OB_MBALL, ob->type, obt->type)) {
			if(is_mball_basis_for(obt, ob))
				obt->recalc|= OB_RECALC_DATA;
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
						ct->subtarget[0]= '\0';
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
	mat= bmain->mat.first;
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
	for(tex= bmain->tex.first; tex; tex= tex->id.next) {
		if(tex->env && (ob==tex->env->object)) tex->env->object= NULL;
		if(tex->pd  && (ob==tex->pd->object))  tex->pd->object= NULL;
		if(tex->vd  && (ob==tex->vd->object))  tex->vd->object= NULL;
	}

	/* worlds */
	wrld= bmain->world.first;
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
	sce= bmain->scene.first;
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
	ipo= bmain->ipo.first;
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
	sc= bmain->screen.first;
	while(sc) {
		ScrArea *sa= sc->areabase.first;
		while(sa) {
			SpaceLink *sl;

			for (sl= sa->spacedata.first; sl; sl= sl->next) {
				if(sl->spacetype==SPACE_VIEW3D) {
					View3D *v3d= (View3D*) sl;

					found= 0;
					if(v3d->camera==ob) {
						v3d->camera= NULL;
						found= 1;
					}
					if(v3d->localvd && v3d->localvd->camera==ob ) {
						v3d->localvd->camera= NULL;
						found += 2;
					}

					if (found) {
						if (sa->spacetype == SPACE_VIEW3D) {
							for (ar= sa->regionbase.first; ar; ar= ar->next) {
								if (ar->regiontype==RGN_TYPE_WINDOW) {
									rv3d= (RegionView3D *)ar->regiondata;
									if (found == 1 || found == 3) {
										if (rv3d->persp == RV3D_CAMOB)
											rv3d->persp= RV3D_PERSP;
									}
									if (found == 2 || found == 3) {
										if (rv3d->localvd && rv3d->localvd->persp == RV3D_CAMOB)
											rv3d->localvd->persp= RV3D_PERSP;
									}
								}
							}
						}
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
				else if(sl->spacetype==SPACE_BUTS) {
					SpaceButs *sbuts= (SpaceButs *)sl;

					if(sbuts->pinid==(ID *)ob) {
						sbuts->flag&= ~SB_PIN_CONTEXT;
						sbuts->pinid= NULL;
					}
				}
			}

			sa= sa->next;
		}
		sc= sc->id.next;
	}

	/* groups */
	group= bmain->group.first;
	while(group) {
		rem_from_group(group, ob, NULL, NULL);
		group= group->id.next;
	}
	
	/* cameras */
	camera= bmain->camera.first;
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
	case OB_ARMATURE: return add_armature("Armature");
	case OB_SPEAKER: return add_speaker("Speaker");
	case OB_EMPTY: return NULL;
	default:
		printf("add_obdata_from_type: Internal error, bad type: %d\n", type);
		return NULL;
	}
}

static const char *get_obdata_defname(int type)
{
	switch (type) {
	case OB_MESH: return "Mesh";
	case OB_CURVE: return "Curve";
	case OB_SURF: return "Surf";
	case OB_FONT: return "Text";
	case OB_MBALL: return "Mball";
	case OB_CAMERA: return "Camera";
	case OB_LAMP: return "Lamp";
	case OB_LATTICE: return "Lattice";
	case OB_ARMATURE: return "Armature";
	case OB_SPEAKER: return "Speaker";
	case OB_EMPTY: return "Empty";
	default:
		printf("get_obdata_defname: Internal error, bad type: %d\n", type);
		return "Empty";
	}
}

/* more general add: creates minimum required data, but without vertices etc. */
Object *add_only_object(int type, const char *name)
{
	Object *ob;

	ob= alloc_libblock(&G.main->object, ID_OB, name);

	/* default object vars */
	ob->type= type;
	
	ob->col[0]= ob->col[1]= ob->col[2]= 1.0;
	ob->col[3]= 1.0;
	
	ob->size[0]= ob->size[1]= ob->size[2]= 1.0;
	ob->dscale[0]= ob->dscale[1]= ob->dscale[2]= 1.0;
	
	/* objects should default to having Euler XYZ rotations, 
	 * but rotations default to quaternions 
	 */
	ob->rotmode= ROT_MODE_EUL;

	unit_axis_angle(ob->rotAxis, &ob->rotAngle);
	unit_axis_angle(ob->drotAxis, &ob->drotAngle);

	unit_qt(ob->quat);
	unit_qt(ob->dquat);

	/* rotation locks should be 4D for 4 component rotations by default... */
	ob->protectflag = OB_LOCK_ROT4D;
	
	unit_m4(ob->constinv);
	unit_m4(ob->parentinv);
	unit_m4(ob->obmat);
	ob->dt= OB_TEXTURE;
	ob->empty_drawtype= OB_PLAINAXES;
	ob->empty_drawsize= 1.0;

	if(type==OB_CAMERA || type==OB_LAMP || type==OB_SPEAKER) {
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
	ob->init_state=1;
	ob->state=1;
	/* ob->pad3 == Contact Processing Threshold */
	ob->m_contactProcessingThreshold = 1.;
	ob->obstacleRad = 1.;
	
	/* NT fluid sim defaults */
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

	BLI_strncpy(name, get_obdata_defname(type), sizeof(name));
	ob = add_only_object(type, name);

	ob->data= add_obdata_from_type(type);

	ob->lay= scene->lay;
	
	base= scene_add_base(scene, ob);
	scene_select_base(scene, base);
	ob->recalc |= OB_RECALC_OB|OB_RECALC_DATA|OB_RECALC_TIME;

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

static ParticleSystem *copy_particlesystem(ParticleSystem *psys)
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
	psysn->effectors= NULL;
	
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
	ParticleSystem *psys, *npsys;
	ModifierData *md;

	obn->particlesystem.first= obn->particlesystem.last= NULL;
	for(psys=ob->particlesystem.first; psys; psys=psys->next) {
		npsys= copy_particlesystem(psys);

		BLI_addtail(&obn->particlesystem, npsys);

		/* need to update particle modifiers too */
		for(md=obn->modifiers.first; md; md=md->next) {
			if(md->type==eModifierType_ParticleSystem) {
				ParticleSystemModifierData *psmd= (ParticleSystemModifierData*)md;
				if(psmd->psys==psys)
					psmd->psys= npsys;
			}
			else if(md->type==eModifierType_DynamicPaint) {
				DynamicPaintModifierData *pmd= (DynamicPaintModifierData*)md;
				if (pmd->brush) {
					if(pmd->brush->psys==psys) {
						pmd->brush->psys= npsys;
					}
				}
			}
			else if (md->type==eModifierType_Smoke) {
				SmokeModifierData *smd = (SmokeModifierData*) md;

				if(smd->type==MOD_SMOKE_TYPE_FLOW) {
					if (smd->flow) {
						if (smd->flow->psys == psys)
							smd->flow->psys= npsys;
					}
				}
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

static int object_pose_context(Object *ob)
{
	if(	(ob) &&
		(ob->type == OB_ARMATURE) &&
		(ob->pose) &&
		(ob->mode & OB_MODE_POSE)
	) {
		return 1;
	}
	else {
		return 0;
	}
}

//Object *object_pose_armature_get(Object *ob)
Object *object_pose_armature_get(struct Object *ob)
{
	if(ob==NULL)
		return NULL;

	if(object_pose_context(ob))
		return ob;

	ob= modifiers_isDeformedByArmature(ob);

	if(object_pose_context(ob))
		return ob;

	return NULL;
}

static void copy_object_transform(Object *ob_tar, Object *ob_src)
{
	copy_v3_v3(ob_tar->loc, ob_src->loc);
	copy_v3_v3(ob_tar->rot, ob_src->rot);
	copy_v3_v3(ob_tar->quat, ob_src->quat);
	copy_v3_v3(ob_tar->rotAxis, ob_src->rotAxis);
	ob_tar->rotAngle= ob_src->rotAngle;
	ob_tar->rotmode= ob_src->rotmode;
	copy_v3_v3(ob_tar->size, ob_src->size);
}

Object *copy_object(Object *ob)
{
	Object *obn;
	ModifierData *md;
	int a;

	obn= copy_libblock(&ob->id);
	
	if(ob->totcol) {
		obn->mat= MEM_dupallocN(ob->mat);
		obn->matbits= MEM_dupallocN(ob->matbits);
		obn->totcol= ob->totcol;
	}
	
	if(ob->bb) obn->bb= MEM_dupallocN(ob->bb);
	obn->flag &= ~OB_FROMGROUP;
	
	obn->modifiers.first = obn->modifiers.last= NULL;
	
	for (md=ob->modifiers.first; md; md=md->next) {
		ModifierData *nmd = modifier_new(md->type);
		BLI_strncpy(nmd->name, md->name, sizeof(nmd->name));
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
	copy_constraints(&obn->constraints, &ob->constraints, TRUE);

	obn->mode = 0;
	obn->sculpt = NULL;

	/* increase user numbers */
	id_us_plus((ID *)obn->data);
	id_us_plus((ID *)obn->gpd);
	id_lib_extern((ID *)obn->dup_group);

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

	obn->mpath= NULL;
	
	return obn;
}

static void extern_local_object(Object *ob)
{
	//bActionStrip *strip;
	ParticleSystem *psys;

#if 0 // XXX old animation system
	id_lib_extern((ID *)ob->action);
	id_lib_extern((ID *)ob->ipo);
#endif // XXX old animation system
	id_lib_extern((ID *)ob->data);
	id_lib_extern((ID *)ob->dup_group);
	id_lib_extern((ID *)ob->poselib);
	id_lib_extern((ID *)ob->gpd);

	extern_local_matarar(ob->mat, ob->totcol);

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
	Main *bmain= G.main;
	Scene *sce;
	Base *base;
	int is_local= FALSE, is_lib= FALSE;

	/* - only lib users: do nothing
	 * - only local users: set flag
	 * - mixed: make copy
	 */

	if(ob->id.lib==NULL) return;
	
	ob->proxy= ob->proxy_from= NULL;
	
	if(ob->id.us==1) {
		id_clear_lib_data(bmain, &ob->id);
		extern_local_object(ob);
	}
	else {
		for(sce= bmain->scene.first; sce && ELEM(0, is_lib, is_local); sce= sce->id.next) {
			if(object_in_scene(ob, sce)) {
				if(sce->id.lib) is_lib= TRUE;
				else is_local= TRUE;
			}
		}

		if(is_local && is_lib == FALSE) {
			id_clear_lib_data(bmain, &ob->id);
			extern_local_object(ob);
		}
		else if(is_local && is_lib) {
			Object *ob_new= copy_object(ob);

			ob_new->id.us= 0;
			
			/* Remap paths of new ID using old library as base. */
			BKE_id_lib_local_paths(bmain, ob->id.lib, &ob_new->id);

			sce= bmain->scene.first;
			while(sce) {
				if(sce->id.lib==NULL) {
					base= sce->base.first;
					while(base) {
						if(base->object==ob) {
							base->object= ob_new;
							ob_new->id.us++;
							ob->id.us--;
						}
						base= base->next;
					}
				}
				sce= sce->id.next;
			}
		}
	}
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
	unsigned int lay= arm->layer_protected;
	
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
					if(dtar->id) {
						if ((Object *)dtar->id == target)
							dtar->id= (ID *)ob;
						else {
							/* only on local objects because this causes indirect links a -> b -> c,blend to point directly to a.blend
							 * when a.blend has a proxy thats linked into c.blend  */
							if(ob->id.lib==NULL)
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
	
	ob->recalc= target->recalc= OB_RECALC_OB|OB_RECALC_DATA|OB_RECALC_TIME;
	
	/* copy transform
	 * - gob means this proxy comes from a group, just apply the matrix
	 *   so the object wont move from its dupli-transform.
	 *
	 * - no gob means this is being made from a linked object,
	 *   this is closer to making a copy of the object - in-place. */
	if(gob) {
		ob->rotmode= target->rotmode;
		mult_m4_m4m4(ob->obmat, gob->obmat, target->obmat);
		if(gob->dup_group) { /* should always be true */
			float tvec[3];
			copy_v3_v3(tvec, gob->dup_group->dupli_ofs);
			mul_mat3_m4_v3(ob->obmat, tvec);
			sub_v3_v3(ob->obmat[3], tvec);
		}
		object_apply_mat4(ob, ob->obmat, FALSE, TRUE);
	}
	else {
		copy_object_transform(ob, target);
		ob->parent= target->parent;	/* libdata */
		copy_m4_m4(ob->parentinv, target->parentinv);
	}
	
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
	if ((target->totcol) && (target->mat) && OB_TYPE_SUPPORT_MATERIAL(ob->type)) {
		int i;
		
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
	else if (target->type == OB_EMPTY) {
		ob->empty_drawtype = target->empty_drawtype;
		ob->empty_drawsize = target->empty_drawsize;
	}

	/* copy IDProperties */
	if(ob->id.properties) {
		IDP_FreeProperty(ob->id.properties);
		MEM_freeN(ob->id.properties);
		ob->id.properties= NULL;
	}
	if(target->id.properties) {
		ob->id.properties= IDP_CopyProperty(target->id.properties);
	}

	/* copy drawtype info */
	ob->dt= target->dt;
}


/* *************** CALC ****************** */

void object_scale_to_mat3(Object *ob, float mat[][3])
{
	float vec[3];
	mul_v3_v3v3(vec, ob->size, ob->dscale);
	size_to_mat3( mat,vec);
}

void object_rot_to_mat3(Object *ob, float mat[][3])
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
		/* axis-angle -  not really that great for 3D-changing orientations */
		axis_angle_to_mat3(rmat, ob->rotAxis, ob->rotAngle);
		axis_angle_to_mat3(dmat, ob->drotAxis, ob->drotAngle);
	}
	else {
		/* quats are normalised before use to eliminate scaling issues */
		float tquat[4];
		
		normalize_qt_qt(tquat, ob->quat);
		quat_to_mat3(rmat, tquat);
		
		normalize_qt_qt(tquat, ob->dquat);
		quat_to_mat3(dmat, tquat);
	}
	
	/* combine these rotations */
	mul_m3_m3m3(mat, dmat, rmat);
}

void object_mat3_to_rot(Object *ob, float mat[][3], short use_compat)
{
	switch(ob->rotmode) {
	case ROT_MODE_QUAT:
		{
			float dquat[4];
			mat3_to_quat(ob->quat, mat);
			normalize_qt_qt(dquat, ob->dquat);
			invert_qt(dquat);
			mul_qt_qtqt(ob->quat, dquat, ob->quat);
		}
		break;
	case ROT_MODE_AXISANGLE:
		mat3_to_axis_angle(ob->rotAxis, &ob->rotAngle, mat);
		sub_v3_v3(ob->rotAxis, ob->drotAxis);
		ob->rotAngle -= ob->drotAngle;
		break;
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

			if(use_compat)	mat3_to_compatible_eulO(ob->rot, ob->rot, ob->rotmode, tmat);
			else			mat3_to_eulO(ob->rot, ob->rotmode, tmat);
		}
	}
}

void object_tfm_protected_backup(const Object *ob,
                                 ObjectTfmProtectedChannels *obtfm)
{

#define TFMCPY(   _v) (obtfm->_v = ob->_v)
#define TFMCPY3D( _v) copy_v3_v3(obtfm->_v, ob->_v)
#define TFMCPY4D( _v) copy_v4_v4(obtfm->_v, ob->_v)

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

void object_tfm_protected_restore(Object *ob,
                                  const ObjectTfmProtectedChannels *obtfm,
                                  const short protectflag)
{
	unsigned int i;

	for (i= 0; i < 3; i++) {
		if (protectflag & (OB_LOCK_LOCX<<i)) {
			ob->loc[i]=  obtfm->loc[i];
			ob->dloc[i]= obtfm->dloc[i];
		}

		if (protectflag & (OB_LOCK_SCALEX<<i)) {
			ob->size[i]=  obtfm->size[i];
			ob->dscale[i]= obtfm->dscale[i];
		}

		if (protectflag & (OB_LOCK_ROTX<<i)) {
			ob->rot[i]=  obtfm->rot[i];
			ob->drot[i]= obtfm->drot[i];

			ob->quat[i + 1]=  obtfm->quat[i + 1];
			ob->dquat[i + 1]= obtfm->dquat[i + 1];

			ob->rotAxis[i]=  obtfm->rotAxis[i];
			ob->drotAxis[i]= obtfm->drotAxis[i];
		}
	}

	if ((protectflag & OB_LOCK_ROT4D) && (protectflag & OB_LOCK_ROTW)) {
		ob->quat[0]=  obtfm->quat[0];
		ob->dquat[0]= obtfm->dquat[0];

		ob->rotAngle=  obtfm->rotAngle;
		ob->drotAngle= obtfm->drotAngle;
	}
}

/* see pchan_apply_mat4() for the equivalent 'pchan' function */
void object_apply_mat4(Object *ob, float mat[][4], const short use_compat, const short use_parent)
{
	float rot[3][3];

	if(use_parent && ob->parent) {
		float rmat[4][4], diff_mat[4][4], imat[4][4];
		mult_m4_m4m4(diff_mat, ob->parent->obmat, ob->parentinv);
		invert_m4_m4(imat, diff_mat);
		mult_m4_m4m4(rmat, imat, mat); /* get the parent relative matrix */
		object_apply_mat4(ob, rmat, use_compat, FALSE);
		
		/* same as below, use rmat rather than mat */
		mat4_to_loc_rot_size(ob->loc, rot, ob->size, rmat);
		object_mat3_to_rot(ob, rot, use_compat);
	}
	else {
		mat4_to_loc_rot_size(ob->loc, rot, ob->size, mat);
		object_mat3_to_rot(ob, rot, use_compat);
	}
	
	sub_v3_v3(ob->loc, ob->dloc);

	if (ob->dscale[0] != 0.0f) ob->size[0] /= ob->dscale[0];
	if (ob->dscale[1] != 0.0f) ob->size[1] /= ob->dscale[1];
	if (ob->dscale[2] != 0.0f) ob->size[2] /= ob->dscale[2];

	/* object_mat3_to_rot handles delta rotations */
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

	add_v3_v3v3(mat[3], ob->loc, ob->dloc);
}

/* extern */
int enable_cu_speed= 1;

static void ob_parcurve(Scene *scene, Object *ob, Object *par, float mat[][4])
{
	Curve *cu;
	float vec[4], dir[3], quat[4], radius, ctime;
	float timeoffs = 0.0, sf_orig = 0.0;
	
	unit_m4(mat);
	
	cu= par->data;
	if(cu->path==NULL || cu->path->data==NULL) /* only happens on reload file, but violates depsgraph still... fix! */
		makeDispListCurveTypes(scene, par, 0);
	if(cu->path==NULL) return;
	
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
		if (IS_EQF(cu->pathlen, 0.0f) == 0)
			ctime= cu->ctime / cu->pathlen;
		else
			ctime= cu->ctime;

		CLAMP(ctime, 0.0f, 1.0f);
	}
	else {
		ctime= scene->r.cfra;
		if (IS_EQF(cu->pathlen, 0.0f) == 0)
			ctime /= cu->pathlen;
		
		CLAMP(ctime, 0.0f, 1.0f);
	}
	
	/* time calculus is correct, now apply distance offset */
	if(cu->flag & CU_OFFS_PATHDIST) {
		ctime += timeoffs/cu->path->totdist;

		/* restore */
		SWAP(float, sf_orig, ob->sf);
	}
	
	
	/* vec: 4 items! */
	if( where_on_path(par, ctime, vec, dir, cu->flag & CU_FOLLOW ? quat:NULL, &radius, NULL) ) {

		if(cu->flag & CU_FOLLOW) {
#if 0
			float x1, q[4];
			vec_to_quat( quat,dir, ob->trackflag, ob->upflag);
			
			/* the tilt */
			normalize_v3(dir);
			q[0]= (float)cos(0.5*vec[3]);
			x1= (float)sin(0.5*vec[3]);
			q[1]= -x1*dir[0];
			q[2]= -x1*dir[1];
			q[3]= -x1*dir[2];
			mul_qt_qtqt(quat, q, quat);
#else
			quat_apply_track(quat, ob->trackflag, ob->upflag);
#endif
			normalize_qt(quat);
			quat_to_mat4(mat, quat);
		}
		
		if(cu->flag & CU_PATH_RADIUS) {
			float tmat[4][4], rmat[4][4];
			scale_m4_fl(tmat, radius);
			mult_m4_m4m4(rmat, tmat, mat);
			copy_m4_m4(mat, rmat);
		}

		copy_v3_v3(mat[3], vec);
		
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
	copy_v3_v3(vec, mat[1]);
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
		DerivedMesh *dm;

		em = BKE_mesh_get_editmesh(me);
		dm = (em)? em->derivedFinal: par->derivedFinal;
			
		if(dm) {
			MVert *mvert= dm->getVertArray(dm);
			int *index = (int *)dm->getVertDataArray(dm, CD_ORIGINDEX);
			int i, vindex, numVerts = dm->getNumVerts(dm);

			/* get the average of all verts with (original index == nr) */
			count= 0;
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
		else fprintf(stderr, "%s: DerivedMesh is needed to solve parenting, object position can be wrong now\n", __func__);

		if(em)
			BKE_mesh_end_editmesh(me, em);
	}
	else if (ELEM(par->type, OB_CURVE, OB_SURF)) {
		Nurb *nu;
		Curve *cu;
		BPoint *bp;
		BezTriple *bezt;
		int found= 0;
		ListBase *nurbs;

		cu= par->data;
		nurbs= BKE_curve_nurbs(cu);
		nu= nurbs->first;

		count= 0;
		while(nu && !found) {
			if(nu->type == CU_BEZIER) {
				bezt= nu->bezt;
				a= nu->pntsu;
				while(a--) {
					if(count==nr) {
						found= 1;
						copy_v3_v3(vec, bezt->vec[1]);
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
		
		if(latt->editlatt) latt= latt->editlatt->latt;
		
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
			copy_v3_v3(mat[3], v1);
		}
		else {
			add_v3_v3v3(mat[3], v1, v2);
			add_v3_v3(mat[3], v3);
			mul_v3_fl(mat[3], 0.3333333f);
		}
	}
}

static int where_is_object_parslow(Object *ob, float obmat[4][4], float slowmat[4][4])
{
	float *fp1, *fp2;
	float fac1, fac2;
	int a;

	// include framerate
	fac1= ( 1.0f / (1.0f + fabsf(ob->sf)) );
	if(fac1 >= 1.0f) return 0;
	fac2= 1.0f-fac1;

	fp1= obmat[0];
	fp2= slowmat[0];
	for(a=0; a<16; a++, fp1++, fp2++) {
		fp1[0]= fac1*fp1[0] + fac2*fp2[0];
	}

	return 1;
}

void where_is_object_time(Scene *scene, Object *ob, float ctime)
{
	float slowmat[4][4] = MAT4_UNITY;
	float stime=ctime;
	
	/* new version: correct parent+vertexparent and track+parent */
	/* this one only calculates direct attached parent and track */
	/* is faster, but should keep track of timeoffs */
	
	if(ob==NULL) return;
	
	/* execute drivers only, as animation has already been done */
	BKE_animsys_evaluate_animdata(scene, &ob->id, ob->adt, ctime, ADT_RECALC_DRIVERS);
	
	if(ob->parent) {
		Object *par= ob->parent;
		
		/* hurms, code below conflicts with depgraph... (ton) */
		/* and even worse, it gives bad effects for NLA stride too (try ctime != par->ctime, with MBlur) */
		if(stime != par->ctime) {
			// only for ipo systems? 
			Object tmp= *par;
			
			if(par->proxy_from);	// was a copied matrix, no where_is! bad...
			else where_is_object_time(scene, par, ctime);
			
			solve_parenting(scene, ob, par, ob->obmat, slowmat, 0);
			
			*par= tmp;
		}
		else
			solve_parenting(scene, ob, par, ob->obmat, slowmat, 0);
		
		/* "slow parent" is definitely not threadsafe, and may also give bad results jumping around 
		 * An old-fashioned hack which probably doesn't really cut it anymore
		 */
		if(ob->partype & PARSLOW) {
			if(!where_is_object_parslow(ob, ob->obmat, slowmat))
				return;
		}
	}
	else {
		object_to_mat4(ob, ob->obmat);
	}

	/* solve constraints */
	if (ob->constraints.first && !(ob->transflag & OB_NO_CONSTRAINTS)) {
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

/* get object transformation matrix without recalculating dependencies and
   constraints -- assume dependencies are already solved by depsgraph.
   no changes to object and it's parent would be done.
   used for bundles orientation in 3d space relative to parented blender camera */
void where_is_object_mat(Scene *scene, Object *ob, float obmat[4][4])
{
	float slowmat[4][4] = MAT4_UNITY;

	if(ob->parent) {
		Object *par= ob->parent;

		solve_parenting(scene, ob, par, obmat, slowmat, 1);

		if(ob->partype & PARSLOW)
			where_is_object_parslow(ob, obmat, slowmat);
	}
	else {
		object_to_mat4(ob, obmat);
	}
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
			copy_v3_v3(totmat[3], par->obmat[3]);
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
		if( (ob->partype & PARTYPE)==PARSKEL ) {
			copy_v3_v3(ob->orig, par->obmat[3]);
		}
		else {
			copy_v3_v3(ob->orig, totmat[3]);
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
	float *fp1, *fp2;
	float slowmat[4][4];
	float fac1, fac2;
	int a;
	
	/* NO TIMEOFFS */
	if(ob->parent) {
		par= ob->parent;
		
		solve_parenting(scene, ob, par, ob->obmat, slowmat, 1);
		
		if(ob->partype & PARSLOW) {
			fac1= (float)(1.0/(1.0+ fabs(ob->sf)));
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

	BLI_strncpy(workob->parsubstr, ob->parsubstr, sizeof(workob->parsubstr));

	where_is_object(scene, workob);
}

BoundBox *unit_boundbox(void)
{
	BoundBox *bb;
	float min[3] = {-1.0f,-1.0f,-1.0f}, max[3] = {-1.0f,-1.0f,-1.0f};

	bb= MEM_callocN(sizeof(BoundBox), "OB-BoundBox");
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
		
		value[0] = fabsf(scale[0]) * (bb->vec[4][0] - bb->vec[0][0]);
		value[1] = fabsf(scale[1]) * (bb->vec[2][1] - bb->vec[0][1]);
		value[2] = fabsf(scale[2]) * (bb->vec[1][2] - bb->vec[0][2]);
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

void minmax_object(Object *ob, float min[3], float max[3])
{
	BoundBox bb;
	float vec[3];
	int a;
	short change= FALSE;
	
	switch(ob->type) {
	case OB_CURVE:
	case OB_FONT:
	case OB_SURF:
		{
			Curve *cu= ob->data;

			if(cu->bb==NULL) tex_space_curve(cu);
			bb= *(cu->bb);

			for(a=0; a<8; a++) {
				mul_m4_v3(ob->obmat, bb.vec[a]);
				DO_MINMAX(bb.vec[a], min, max);
			}
			change= TRUE;
		}
		break;
	case OB_LATTICE:
		{
			Lattice *lt= ob->data;
			BPoint *bp= lt->def;
			int u, v, w;

			for(w=0; w<lt->pntsw; w++) {
				for(v=0; v<lt->pntsv; v++) {
					for(u=0; u<lt->pntsu; u++, bp++) {
						mul_v3_m4v3(vec, ob->obmat, bp->vec);
						DO_MINMAX(vec, min, max);
					}
				}
			}
			change= TRUE;
		}
		break;
	case OB_ARMATURE:
		if(ob->pose) {
			bPoseChannel *pchan;
			for(pchan= ob->pose->chanbase.first; pchan; pchan= pchan->next) {
				mul_v3_m4v3(vec, ob->obmat, pchan->pose_head);
				DO_MINMAX(vec, min, max);
				mul_v3_m4v3(vec, ob->obmat, pchan->pose_tail);
				DO_MINMAX(vec, min, max);
			}
			change= TRUE;
		}
		break;
	case OB_MESH:
		{
			Mesh *me= get_mesh(ob);

			if(me) {
				bb = *mesh_get_bb(ob);

				for(a=0; a<8; a++) {
					mul_m4_v3(ob->obmat, bb.vec[a]);
					DO_MINMAX(bb.vec[a], min, max);
				}
				change= TRUE;
			}
		}
		break;
	}

	if(change == FALSE) {
		DO_MINMAX(ob->obmat[3], min, max);

		copy_v3_v3(vec, ob->obmat[3]);
		add_v3_v3(vec, ob->size);
		DO_MINMAX(vec, min, max);

		copy_v3_v3(vec, ob->obmat[3]);
		sub_v3_v3(vec, ob->size);
		DO_MINMAX(vec, min, max);
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

void BKE_object_foreach_display_point(
        Object *ob, float obmat[4][4],
        void (*func_cb)(const float[3], void *), void *user_data)
{
	float co[3];

	if (ob->derivedFinal) {
		DerivedMesh *dm= ob->derivedFinal;
		MVert *mv= dm->getVertArray(dm);
		int totvert= dm->getNumVerts(dm);
		int i;

		for (i= 0; i < totvert; i++, mv++) {
			mul_v3_m4v3(co, obmat, mv->co);
			func_cb(co, user_data);
		}
	}
	else if (ob->disp.first) {
		DispList *dl;

		for (dl=ob->disp.first; dl; dl=dl->next) {
			float *v3= dl->verts;
			int totvert= dl->nr;
			int i;

			for (i= 0; i < totvert; i++, v3+=3) {
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

	for(base= FIRSTBASE; base; base = base->next) {
		if(BASE_VISIBLE(v3d, base) && (base->flag & flag) == flag) {
			ob= base->object;

			if ((ob->transflag & OB_DUPLI)==0) {
				BKE_object_foreach_display_point(ob, ob->obmat, func_cb, user_data);
			}
			else {
				ListBase *lb;
				DupliObject *dob;

				lb= object_duplilist(scene, ob);
				for(dob= lb->first; dob; dob= dob->next) {
					if(dob->no_draw == 0) {
						BKE_object_foreach_display_point(dob->ob, dob->mat, func_cb, user_data);
					}
				}
				free_object_duplilist(lb);	/* does restore */
			}
		}
	}
}

/* copied from DNA_object_types.h */
typedef struct ObTfmBack {
	float loc[3], dloc[3], orig[3];
	float size[3], dscale[3];	/* scale and delta scale */
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
	copy_v3_v3(obtfm->dscale, ob->dscale);
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
	copy_v3_v3(ob->dscale, obtfm->dscale);
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

int BKE_object_parent_loop_check(const Object *par, const Object *ob)
{
	/* test if 'ob' is a parent somewhere in par's parents */
	if(par == NULL) return 0;
	if(ob == par) return 1;
	return BKE_object_parent_loop_check(par->parent, ob);
}

/* proxy rule: lib_object->proxy_from == the one we borrow from, only set temporal and cleared here */
/*           local_object->proxy      == pointer to library object, saved in files and read */

/* function below is polluted with proxy exceptions, cleanup will follow! */

/* the main object update call, for object matrix, constraints, keys and displist (modifiers) */
/* requires flags to be set! */
void object_handle_update(Scene *scene, Object *ob)
{
	if(ob->recalc & OB_RECALC_ALL) {
		/* speed optimization for animation lookups */
		if(ob->pose)
			make_pose_channels_hash(ob->pose);

		if(ob->recalc & OB_RECALC_DATA) {
			if(ob->type==OB_ARMATURE) {
				/* this happens for reading old files and to match library armatures
				   with poses we do it ahead of where_is_object to ensure animation
				   is evaluated on the rebuilt pose, otherwise we get incorrect poses
				   on file load */
				if(ob->pose==NULL || (ob->pose->flag & POSE_RECALC))
					armature_rebuild_pose(ob, ob->data);
			}
		}

		/* XXX new animsys warning: depsgraph tag OB_RECALC_DATA should not skip drivers, 
		   which is only in where_is_object now */
		// XXX: should this case be OB_RECALC_OB instead?
		if(ob->recalc & OB_RECALC_ALL) {
			
			if (G.f & G_DEBUG)
				printf("recalcob %s\n", ob->id.name+2);
			
			/* handle proxy copy for target */
			if(ob->id.lib && ob->proxy_from) {
				// printf("ob proxy copy, lib ob %s proxy %s\n", ob->id.name, ob->proxy_from->id.name);
				if(ob->proxy_from->proxy_group) {/* transform proxy into group space */
					Object *obg= ob->proxy_from->proxy_group;
					invert_m4_m4(obg->imat, obg->obmat);
					mult_m4_m4m4(ob->obmat, obg->imat, ob->proxy_from->obmat);
					if(obg->dup_group) { /* should always be true */
						add_v3_v3(ob->obmat[3], obg->dup_group->dupli_ofs);
					}
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

			if(adt) {
				/* evaluate drivers */
				// XXX: for mesh types, should we push this to derivedmesh instead?
				BKE_animsys_evaluate_animdata(scene, data_id, adt, ctime, ADT_RECALC_DRIVERS);
			}

			/* includes all keys and modifiers */
			switch(ob->type) {
			case OB_MESH:
				{
#if 0				// XXX, comment for 2.56a release, background wont set 'scene->customdata_mask'
					EditMesh *em = (ob == scene->obedit)? BKE_mesh_get_editmesh(ob->data): NULL;
					BLI_assert((scene->customdata_mask & CD_MASK_BAREMESH) == CD_MASK_BAREMESH);
					if(em) {
						makeDerivedMesh(scene, ob, em,  scene->customdata_mask); /* was CD_MASK_BAREMESH */
						BKE_mesh_end_editmesh(ob->data, em);
					} else
						makeDerivedMesh(scene, ob, NULL, scene->customdata_mask);

#else				/* ensure CD_MASK_BAREMESH for now */
					EditMesh *em = (ob == scene->obedit)? BKE_mesh_get_editmesh(ob->data): NULL;
					uint64_t data_mask= scene->customdata_mask | ob->customdata_mask | CD_MASK_BAREMESH;
					if(em) {
						makeDerivedMesh(scene, ob, em,  data_mask); /* was CD_MASK_BAREMESH */
						BKE_mesh_end_editmesh(ob->data, em);
					} else
						makeDerivedMesh(scene, ob, NULL, data_mask);
#endif

				}
				break;

			case OB_ARMATURE:
				if(ob->id.lib && ob->proxy_from) {
					// printf("pose proxy copy, lib ob %s proxy %s\n", ob->id.name, ob->proxy_from->id.name);
					copy_pose_result(ob->pose, ob->proxy_from->pose);
				}
				else {
					where_is_pose(scene, ob);
				}
				break;

			case OB_MBALL:
				makeDispListMBall(scene, ob);
				break;

			case OB_CURVE:
			case OB_SURF:
			case OB_FONT:
				makeDispListCurveTypes(scene, ob, 0);
				break;
				
			case OB_LATTICE:
				lattice_calc_modifiers(scene, ob);
				break;
			}


			if(ob->particlesystem.first) {
				ParticleSystem *tpsys, *psys;
				DerivedMesh *dm;
				ob->transflag &= ~OB_DUPLIPARTS;
				
				psys= ob->particlesystem.first;
				while(psys) {
					if(psys_check_enabled(ob, psys)) {
						/* check use of dupli objects here */
						if(psys->part && (psys->part->draw_as == PART_DRAW_REND || G.rendering) &&
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
	
		ob->recalc &= ~OB_RECALC_ALL;
	}

	/* the case when this is a group proxy, object_update is called in group.c */
	if(ob->proxy) {
		ob->proxy->proxy_from= ob;
		// printf("set proxy pointer for later group stuff %s\n", ob->id.name);
	}
}

void object_sculpt_modifiers_changed(Object *ob)
{
	SculptSession *ss= ob->sculpt;

	if(!ss->cache) {
		/* we free pbvh on changes, except during sculpt since it can't deal with
		   changing PVBH node organization, we hope topology does not change in
		   the meantime .. weak */
		if(ss->pbvh) {
				BLI_pbvh_free(ss->pbvh);
				ss->pbvh= NULL;
		}

		free_sculptsession_deformMats(ob->sculpt);
	} else {
		PBVHNode **nodes;
		int n, totnode;

		BLI_pbvh_search_gather(ss->pbvh, NULL, NULL, &nodes, &totnode);

		for(n = 0; n < totnode; n++)
			BLI_pbvh_node_mark_update(nodes[n]);

		MEM_freeN(nodes);
	}
}

int give_obdata_texspace(Object *ob, short **texflag, float **loc, float **size, float **rot)
{
	
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
static KeyBlock *insert_meshkey(Scene *scene, Object *ob, const char *name, int from_mix)
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

	if(newkey || from_mix==FALSE) {
		/* create from mesh */
		kb= add_keyblock(key, name);
		mesh_to_key(me, kb);
	}
	else {
		/* copy from current values */
		float *data= do_ob_key(scene, ob);

		/* create new block with prepared data */
		kb= add_keyblock(key, name);
		kb->data= data;
		kb->totelem= me->totvert;
	}

	return kb;
}
/************************* Lattice ************************/
static KeyBlock *insert_lattkey(Scene *scene, Object *ob, const char *name, int from_mix)
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

	if(newkey || from_mix==FALSE) {
		kb= add_keyblock(key, name);
		if (!newkey) {
			KeyBlock *basekb= (KeyBlock *)key->block.first;
			kb->data= MEM_dupallocN(basekb->data);
			kb->totelem= basekb->totelem;
		}
		else {
			latt_to_key(lt, kb);
		}
	}
	else {
		/* copy from current values */
		float *data= do_ob_key(scene, ob);

		/* create new block with prepared data */
		kb= add_keyblock(key, name);
		kb->totelem= lt->pntsu*lt->pntsv*lt->pntsw;
		kb->data= data;
	}

	return kb;
}
/************************* Curve ************************/
static KeyBlock *insert_curvekey(Scene *scene, Object *ob, const char *name, int from_mix)
{
	Curve *cu= ob->data;
	Key *key= cu->key;
	KeyBlock *kb;
	ListBase *lb= BKE_curve_nurbs(cu);
	int newkey= 0;

	if(key==NULL) {
		key= cu->key= add_key( (ID *)cu);
		key->type = KEY_RELATIVE;
		newkey= 1;
	}

	if(newkey || from_mix==FALSE) {
		/* create from curve */
		kb= add_keyblock(key, name);
		if (!newkey) {
			KeyBlock *basekb= (KeyBlock *)key->block.first;
			kb->data= MEM_dupallocN(basekb->data);
			kb->totelem= basekb->totelem;
		}
		else {
			curve_to_key(cu, kb, lb);
		}
	}
	else {
		/* copy from current values */
		float *data= do_ob_key(scene, ob);

		/* create new block with prepared data */
		kb= add_keyblock(key, name);
		kb->totelem= count_curveverts(lb);
		kb->data= data;
	}

	return kb;
}

KeyBlock *object_insert_shape_key(Scene *scene, Object *ob, const char *name, int from_mix)
{
	if(ob->type==OB_MESH)					 return insert_meshkey(scene, ob, name, from_mix);
	else if ELEM(ob->type, OB_CURVE, OB_SURF)return insert_curvekey(scene, ob, name, from_mix);
	else if(ob->type==OB_LATTICE)			 return insert_lattkey(scene, ob, name, from_mix);
	else									 return NULL;
}

/* most important if this is modified it should _always_ return True, in certain
 * cases false positives are hard to avoid (shape keys for eg)
 */
int object_is_modified(Scene *scene, Object *ob)
{
	int flag= 0;

	if(ob_get_key(ob)) {
		flag |= eModifierMode_Render;
	}
	else {
		ModifierData *md;
		/* cloth */
		for(md=modifiers_getVirtualModifierList(ob); md && (flag != (eModifierMode_Render | eModifierMode_Realtime)); md=md->next) {
			if((flag & eModifierMode_Render) == 0	&& modifier_isEnabled(scene, md, eModifierMode_Render))		flag |= eModifierMode_Render;
			if((flag & eModifierMode_Realtime) == 0	&& modifier_isEnabled(scene, md, eModifierMode_Realtime))	flag |= eModifierMode_Realtime;
		}
	}

	return flag;
}

static void copy_object__forwardModifierLinks(void *UNUSED(userData), Object *UNUSED(ob), ID **idpoin)
{
	/* this is copied from ID_NEW; it might be better to have a macro */
	if(*idpoin && (*idpoin)->newid) *idpoin = (*idpoin)->newid;
}

void object_relink(Object *ob)
{
	if(ob->id.lib)
		return;

	relink_constraints(&ob->constraints);
	if (ob->pose){
		bPoseChannel *chan;
		for (chan = ob->pose->chanbase.first; chan; chan=chan->next){
			relink_constraints(&chan->constraints);
		}
	}
	modifiers_foreachIDLink(ob, copy_object__forwardModifierLinks, NULL);

	if(ob->adt)
		BKE_relink_animdata(ob->adt);

	ID_NEW(ob->parent);

	ID_NEW(ob->proxy);
	ID_NEW(ob->proxy_group);
}

MovieClip *object_get_movieclip(Scene *scene, Object *ob, int use_default)
{
	MovieClip *clip= use_default ? scene->clip : NULL;
	bConstraint *con= ob->constraints.first, *scon= NULL;

	while(con){
		if(con->type==CONSTRAINT_TYPE_CAMERASOLVER){
			if(scon==NULL || (scon->flag&CONSTRAINT_OFF))
				scon= con;
		}

		con= con->next;
	}

	if(scon) {
		bCameraSolverConstraint *solver= scon->data;
		if((solver->flag&CAMERASOLVER_ACTIVECLIP)==0)
			clip= solver->clip;
		else
			clip= scene->clip;
	}

	return clip;
}
