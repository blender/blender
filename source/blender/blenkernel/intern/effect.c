/*  effect.c
 * 
 * 
 * $Id$
 *
 * ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

#include <math.h>
#include <stdlib.h>

#include "MEM_guardedalloc.h"

#include "DNA_curve_types.h"
#include "DNA_effect_types.h"
#include "DNA_group_types.h"
#include "DNA_ipo_types.h"
#include "DNA_key_types.h"
#include "DNA_lattice_types.h"
#include "DNA_listBase.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_material_types.h"
#include "DNA_object_types.h"
#include "DNA_object_force.h"
#include "DNA_texture_types.h"
#include "DNA_scene_types.h"

#include "BLI_arithb.h"
#include "BLI_blenlib.h"
#include "BLI_jitter.h"
#include "BLI_rand.h"

#include "BKE_action.h"
#include "BKE_anim.h"		/* needed for where_on_path */
#include "BKE_armature.h"
#include "BKE_bad_level_calls.h"
#include "BKE_blender.h"
#include "BKE_constraint.h"
#include "BKE_deform.h"
#include "BKE_depsgraph.h"
#include "BKE_displist.h"
#include "BKE_DerivedMesh.h"
#include "BKE_effect.h"
#include "BKE_global.h"
#include "BKE_group.h"
#include "BKE_ipo.h"
#include "BKE_key.h"
#include "BKE_lattice.h"
#include "BKE_mesh.h"
#include "BKE_material.h"
#include "BKE_main.h"
#include "BKE_modifier.h"
#include "BKE_object.h"
#include "BKE_scene.h"
#include "BKE_screen.h"
#include "BKE_utildefines.h"

#include "PIL_time.h"
#include "RE_render_ext.h"

/* fluid sim particle import */
#ifndef DISABLE_ELBEEM
#include "DNA_object_fluidsim.h"
#include "LBM_fluidsim.h"
#include "elbeem.h"
#include <zlib.h>
#include <string.h>
#endif // DISABLE_ELBEEM

/* temporal struct, used for reading return of mesh_get_mapped_verts_nors() */
typedef struct VeNoCo {
	float co[3], no[3];
} VeNoCo;

Effect *add_effect(int type)
{
	Effect *eff=0;
	PartEff *paf;
	int a;
	
	switch(type) {
	case EFF_PARTICLE:
		paf= MEM_callocN(sizeof(PartEff), "neweff");
		eff= (Effect *)paf;
		
		paf->sta= 1.0;
		paf->end= 100.0;
		paf->lifetime= 50.0;
		for(a=0; a<PAF_MAXMULT; a++) {
			paf->life[a]= 50.0;
			paf->child[a]= 4;
			paf->mat[a]= 1;
		}
		
		paf->totpart= 1000;
		paf->totkey= 8;
		paf->staticstep= 5;
		paf->defvec[2]= 1.0f;
		paf->nabla= 0.05f;
		paf->disp = 100;
		paf->speedtex = 8;
		paf->omat = 1;
		paf->flag= PAF_FACE;

		break;
	}
	
	eff->type= eff->buttype= type;
	eff->flag |= SELECT;
	
	return eff;
}

void free_effect(Effect *eff)
{
	PartEff *paf;
	
	if(eff->type==EFF_PARTICLE) {
		paf= (PartEff *)eff;
		if(paf->keys) MEM_freeN(paf->keys);
	}
	MEM_freeN(eff);	
}


void free_effects(ListBase *lb)
{
	Effect *eff;
	
	eff= lb->first;
	while(eff) {
		BLI_remlink(lb, eff);
		free_effect(eff);
		eff= lb->first;
	}
}

Effect *copy_effect(Effect *eff) 
{
	Effect *effn;

	effn= MEM_dupallocN(eff);
	if(effn->type==EFF_PARTICLE) ((PartEff *)effn)->keys= 0;

	return effn;	
}

void copy_act_effect(Object *ob)
{
	/* return a copy of the active effect */
	Effect *effn, *eff;
	
	eff= ob->effect.first;
	while(eff) {
		if(eff->flag & SELECT) {
			
			effn= copy_effect(eff);
			BLI_addtail(&ob->effect, effn);
			
			eff->flag &= ~SELECT;
			return;
			
		}
		eff= eff->next;
	}
	
	/* when it comes here: add new effect */
	eff= add_effect(EFF_PARTICLE);
	BLI_addtail(&ob->effect, eff);
			
}

void copy_effects(ListBase *lbn, ListBase *lb)
{
	Effect *eff, *effn;

	lbn->first= lbn->last= 0;

	eff= lb->first;
	while(eff) {
		effn= copy_effect(eff);
		BLI_addtail(lbn, effn);
		
		eff= eff->next;
	}
	
}

void deselectall_eff(Object *ob)
{
	Effect *eff= ob->effect.first;
	
	while(eff) {
		eff->flag &= ~SELECT;
		eff= eff->next;
	}
}

/* ***************** PARTICLES ***************** */

static Particle *new_particle(PartEff *paf)
{
	static Particle *pa;
	static int cur;

	/* we agree: when paf->keys==0: alloc */	
	if(paf->keys==NULL) {
		pa= paf->keys= MEM_callocN( paf->totkey*paf->totpart*sizeof(Particle), "particlekeys" );
		cur= 0;
	}
	else {
		if(cur && cur<paf->totpart) pa+=paf->totkey;
		cur++;
	}
	return pa;
}

PartEff *give_parteff(Object *ob)
{
	PartEff *paf;
	
	paf= ob->effect.first;
	while(paf) {
		if(paf->type==EFF_PARTICLE) return paf;
		paf= paf->next;
	}
	return 0;
}

void where_is_particle(PartEff *paf, Particle *pa, float ctime, float *vec)
{
	Particle *p[4];
	float dt, t[4];
	int a;
	
	if(paf->totkey==1 || ctime < pa->time) {
		VECCOPY(vec, pa->co);
		return;
	}
	
	/* first find the first particlekey */
	a= (int)((paf->totkey-1)*(ctime-pa->time)/pa->lifetime);
	if(a>=paf->totkey) a= paf->totkey-1;
	else if(a<0) a= 0;
	
	pa+= a;
	
	if(a>0) p[0]= pa-1; else p[0]= pa;
	p[1]= pa;
	
	if(a+1<paf->totkey) p[2]= pa+1; else p[2]= pa;
	if(a+2<paf->totkey) p[3]= pa+2; else p[3]= p[2];
	
	if(p[1]==p[2] || p[2]->time == p[1]->time) dt= 0.0;
	else dt= (ctime-p[1]->time)/(p[2]->time - p[1]->time);

	if(paf->flag & PAF_BSPLINE) set_four_ipo(dt, t, KEY_BSPLINE);
	else set_four_ipo(dt, t, KEY_CARDINAL);

	vec[0]= t[0]*p[0]->co[0] + t[1]*p[1]->co[0] + t[2]*p[2]->co[0] + t[3]*p[3]->co[0];
	vec[1]= t[0]*p[0]->co[1] + t[1]*p[1]->co[1] + t[2]*p[2]->co[1] + t[3]*p[3]->co[1];
	vec[2]= t[0]*p[0]->co[2] + t[1]*p[1]->co[2] + t[2]*p[2]->co[2] + t[3]*p[3]->co[2];

}

static void particle_tex(MTex *mtex, PartEff *paf, float *co, float *no)
{				
	float tin, tr, tg, tb, ta;
	float old;
	
	externtex(mtex, co, &tin, &tr, &tg, &tb, &ta);

	if(paf->texmap==PAF_TEXINT) {
		tin*= paf->texfac;
		no[0]+= tin*paf->defvec[0];
		no[1]+= tin*paf->defvec[1];
		no[2]+= tin*paf->defvec[2];
	}
	else if(paf->texmap==PAF_TEXRGB) {
		no[0]+= (tr-0.5f)*paf->texfac;
		no[1]+= (tg-0.5f)*paf->texfac;
		no[2]+= (tb-0.5f)*paf->texfac;
	}
	else {	/* PAF_TEXGRAD */
		
		old= tin;
		co[0]+= paf->nabla;
		externtex(mtex, co, &tin, &tr, &tg, &tb, &ta);
		no[0]+= (old-tin)*paf->texfac;
		
		co[0]-= paf->nabla;
		co[1]+= paf->nabla;
		externtex(mtex, co, &tin, &tr, &tg, &tb, &ta);
		no[1]+= (old-tin)*paf->texfac;
		
		co[1]-= paf->nabla;
		co[2]+= paf->nabla;
		externtex(mtex, co, &tin, &tr, &tg, &tb, &ta);
		no[2]+= (old-tin)*paf->texfac;
		
	}
}

/* -------------------------- Effectors ------------------ */

static void add_to_effectorcache(ListBase *lb, Object *ob, Object *obsrc)
{
	pEffectorCache *ec;
	PartDeflect *pd= ob->pd;
			
	if(pd->forcefield == PFIELD_GUIDE) {
		if(ob->type==OB_CURVE && obsrc->type==OB_MESH) {	/* guides only do mesh particles */
			Curve *cu= ob->data;
			if(cu->flag & CU_PATH) {
				if(cu->path==NULL || cu->path->data==NULL)
					makeDispListCurveTypes(ob, 0);
				if(cu->path && cu->path->data) {
					ec= MEM_callocN(sizeof(pEffectorCache), "effector cache");
					ec->ob= ob;
					BLI_addtail(lb, ec);
				}
			}
		}
	}
	else if(pd->forcefield) {
		ec= MEM_callocN(sizeof(pEffectorCache), "effector cache");
		ec->ob= ob;
		BLI_addtail(lb, ec);
	}
}

/* returns ListBase handle with objects taking part in the effecting */
ListBase *pdInitEffectors(Object *obsrc, Group *group)
{
	static ListBase listb={NULL, NULL};
	pEffectorCache *ec;
	Base *base;
	unsigned int layer= obsrc->lay;
	
	if(group) {
		GroupObject *go;
		
		for(go= group->gobject.first; go; go= go->next) {
			if( (go->ob->lay & layer) && go->ob->pd && go->ob!=obsrc) {
				add_to_effectorcache(&listb, go->ob, obsrc);
			}
		}
	}
	else {
		for(base = G.scene->base.first; base; base= base->next) {
			if( (base->lay & layer) && base->object->pd && base->object!=obsrc) {
				add_to_effectorcache(&listb, base->object, obsrc);
			}
		}
	}
	
	/* make a full copy */
	for(ec= listb.first; ec; ec= ec->next) {
		ec->obcopy= *(ec->ob);
	}

	if(listb.first)
		return &listb;
	
	return NULL;
}

void pdEndEffectors(ListBase *lb)
{
	if(lb) {
		pEffectorCache *ec;
		/* restore full copy */
		for(ec= lb->first; ec; ec= ec->next)
			*(ec->ob)= ec->obcopy;

		BLI_freelistN(lb);
	}
}

/* local for this c file, only for guides now */
static void precalc_effectors(Object *ob, PartEff *paf, Particle *pa, ListBase *lb)
{
	pEffectorCache *ec;
	
	for(ec= lb->first; ec; ec= ec->next) {
		PartDeflect *pd= ec->ob->pd;
		
		ec->oldspeed[0]= ec->oldspeed[1]= ec->oldspeed[2]= 0.0f;
		
		if(pd->forcefield==PFIELD_GUIDE && ec->ob->type==OB_CURVE) {
			float vec[4], dir[3];
			
			if(!(paf->flag & PAF_STATIC))
				where_is_object_time(ec->ob, pa->time);
			
			/* scale corrects speed vector to curve size */
			if(paf->totkey>1) ec->scale= (paf->totkey-1)/pa->lifetime;
			else ec->scale= 1.0f;
			
			/* time_scale is for random life */
			if(pa->lifetime>paf->lifetime)
				ec->time_scale= paf->lifetime/pa->lifetime;
			else
				ec->time_scale= pa->lifetime/paf->lifetime;

			/* distance of first path point to particle origin */
			where_on_path(ec->ob, 0.0f, vec, dir);
			VECCOPY(ec->oldloc, vec);	/* store local coord for differences */
			Mat4MulVecfl(ec->ob->obmat, vec);
			
			/* for static we need to move to global space */
			if(paf->flag & PAF_STATIC) {
				VECCOPY(dir, pa->co);
				Mat4MulVecfl(ob->obmat, dir);
				ec->guide_dist= VecLenf(vec, dir);
			}
			else 
				ec->guide_dist= VecLenf(vec, pa->co);
		}
	}
}


/*  -------- pdDoEffectors() --------
    generic force/speed system, now used for particles and softbodies
	lb			= listbase with objects that take part in effecting
	opco		= global coord, as input
    force		= force accumulator
    speed		= actual current speed which can be altered
	cur_time	= "external" time in frames, is constant for static particles
	loc_time	= "local" time in frames, range <0-1> for the lifetime of particle
    par_layer	= layer the caller is in
	flags		= only used for softbody wind now
	guide		= old speed of particle

*/
void pdDoEffectors(ListBase *lb, float *opco, float *force, float *speed, float cur_time, float loc_time, unsigned int flags)
{
/*
	Modifies the force on a particle according to its
	relation with the effector object
	Different kind of effectors include:
		Forcefields: Gravity-like attractor
		(force power is related to the inverse of distance to the power of a falloff value)
		Vortex fields: swirling effectors
		(particles rotate around Z-axis of the object. otherwise, same relation as)
		(Forcefields, but this is not done through a force/acceleration)
		Guide: particles on a path
		(particles are guided along a curve bezier or old nurbs)
		(is independent of other effectors)
*/
	Object *ob;
	pEffectorCache *ec;
	PartDeflect *pd;
	float vect_to_vert[3];
	float f_force, force_vec[3];
	float *obloc;
	float distance, force_val, ffall_val;
	float guidecollect[3], guidedist= 0.0f;
	int cur_frame;
	
	guidecollect[0]= guidecollect[1]= guidecollect[2]=0.0f;

	/* Cycle through collected objects, get total of (1/(gravity_strength * dist^gravity_power)) */
	/* Check for min distance here? (yes would be cool to add that, ton) */
	
	for(ec = lb->first; ec; ec= ec->next) {
		/* object effectors were fully checked to be OK to evaluate! */
		ob= ec->ob;
		pd= ob->pd;
			
		/* Get IPO force strength and fall off values here */
		if (has_ipo_code(ob->ipo, OB_PD_FSTR))
			force_val = IPO_GetFloatValue(ob->ipo, OB_PD_FSTR, cur_time);
		else 
			force_val = pd->f_strength;
		
		if (has_ipo_code(ob->ipo, OB_PD_FFALL)) 
			ffall_val = IPO_GetFloatValue(ob->ipo, OB_PD_FFALL, cur_time);
		else 
			ffall_val = pd->f_power;
			
		/* Need to set r.cfra for paths (investigate, ton) (uses ob->ctime now, ton) */
		if(ob->ctime!=cur_time) {
			cur_frame = G.scene->r.cfra;
			G.scene->r.cfra = (int)cur_time;
			where_is_object_time(ob, cur_time);
			G.scene->r.cfra = cur_frame;
		}
			
		/* use center of object for distance calculus */
		obloc= ob->obmat[3];
		VECSUB(vect_to_vert, obloc, opco);
		distance = VecLength(vect_to_vert);
			
		if((pd->flag & PFIELD_USEMAX) && distance>pd->maxdist && pd->forcefield != PFIELD_GUIDE)
			;	/* don't do anything */
		else if(pd->forcefield == PFIELD_WIND) {
			VECCOPY(force_vec, ob->obmat[2]);
			
			/* wind works harder perpendicular to normal, would be nice for softbody later (ton) */
			
			/* Limit minimum distance to vertex so that */
			/* the force is not too big */
			if (distance < 0.001) distance = 0.001f;
			f_force = (force_val)*(1/(1000 * (float)pow((double)distance, (double)ffall_val)));
			/* this option for softbody only */
			if(flags && PE_WIND_AS_SPEED){
				speed[0] -= (force_vec[0] * f_force );
				speed[1] -= (force_vec[1] * f_force );
				speed[2] -= (force_vec[2] * f_force );
			}
			else{
				force[0] += force_vec[0]*f_force;
				force[1] += force_vec[1]*f_force;
				force[2] += force_vec[2]*f_force;
			}
		}
		else if(pd->forcefield == PFIELD_FORCE) {
			
			/* only use center of object */
			obloc= ob->obmat[3];

			/* Now calculate the gravitational force */
			VECSUB(vect_to_vert, obloc, opco);
			distance = VecLength(vect_to_vert);

			/* Limit minimum distance to vertex so that */
			/* the force is not too big */
			if (distance < 0.001) distance = 0.001f;
			f_force = (force_val)*(1.0/(1000.0 * (float)pow((double)distance, (double)ffall_val)));
			force[0] += (vect_to_vert[0] * f_force );
			force[1] += (vect_to_vert[1] * f_force );
			force[2] += (vect_to_vert[2] * f_force );
		}
		else if(pd->forcefield == PFIELD_VORTEX) {
			float vortexvec[3];
			
			/* only use center of object */
			obloc= ob->obmat[3];

			/* Now calculate the vortex force */
			VECSUB(vect_to_vert, obloc, opco);
			distance = VecLength(vect_to_vert);

			Crossf(force_vec, ob->obmat[2], vect_to_vert);
			Normalize(force_vec);

			/* Limit minimum distance to vertex so that */
			/* the force is not too big */
			if (distance < 0.001) distance = 0.001f;
			f_force = (force_val)*(1.0/(100.0 * (float)pow((double)distance, (double)ffall_val)));
			vortexvec[0]= -(force_vec[0] * f_force );
			vortexvec[1]= -(force_vec[1] * f_force );
			vortexvec[2]= -(force_vec[2] * f_force );
			
			/* this option for softbody only */
			if(flags &&PE_WIND_AS_SPEED) {
				speed[0]+= vortexvec[0];
				speed[1]+= vortexvec[1];
				speed[2]+= vortexvec[2];
			}
			else {
				/* since vortex alters the speed, we have to correct for the previous vortex result */
				speed[0]+= vortexvec[0] - ec->oldspeed[0];
				speed[1]+= vortexvec[1] - ec->oldspeed[1];
				speed[2]+= vortexvec[2] - ec->oldspeed[2];
				
				VECCOPY(ec->oldspeed, vortexvec);
			}
		}
		else if(pd->forcefield == PFIELD_GUIDE) {
			float guidevec[4], guidedir[3];
			float mindist= force_val; /* force_val is actually mindist in the UI */
			
			distance= ec->guide_dist;
			
			/* WARNING: bails out with continue here */
			if((pd->flag & PFIELD_USEMAX) && distance>pd->maxdist) continue;
			
			/* calculate contribution factor for this guide */
			if(distance<=mindist) f_force= 1.0f;
			else if(pd->flag & PFIELD_USEMAX) {
				if(distance>pd->maxdist || mindist>=pd->maxdist) f_force= 0.0f;
				else {
					f_force= 1.0f - (distance-mindist)/(pd->maxdist - mindist);
					if(ffall_val!=0.0f)
						f_force = (float)pow(f_force, ffall_val+1.0);
				}
			}
			else {
				f_force= 1.0f/(1.0f + distance-mindist);
				if(ffall_val!=0.0f)
					f_force = (float)pow(f_force, ffall_val+1.0);
			}
			
			/* now derive path point from loc_time */
			if(pd->flag & PFIELD_GUIDE_PATH_ADD)
				where_on_path(ob, f_force*loc_time*ec->time_scale, guidevec, guidedir);
			else
				where_on_path(ob, loc_time*ec->time_scale, guidevec, guidedir);
			
			VECSUB(guidedir, guidevec, ec->oldloc);
			VECCOPY(ec->oldloc, guidevec);
			
			Mat4Mul3Vecfl(ob->obmat, guidedir);
			VecMulf(guidedir, ec->scale);	/* correction for lifetime and speed */
			
			/* we subtract the speed we gave it previous step */
			VECCOPY(guidevec, guidedir);
			VECSUB(guidedir, guidedir, ec->oldspeed);
			VECCOPY(ec->oldspeed, guidevec);
			
			/* if it fully contributes, we stop */
			if(f_force==1.0) {
				VECCOPY(guidecollect, guidedir);
				guidedist= 1.0f;
				break;
			}
			else if(guidedist<1.0f) {
				VecMulf(guidedir, f_force);
				VECADD(guidecollect, guidecollect, guidedir);
				guidedist += f_force;
			}					
		}
	}

	/* all guides are accumulated here */
	if(guidedist!=0.0f) {
		if(guidedist!=1.0f) VecMulf(guidecollect, 1.0f/guidedist);
		VECADD(speed, speed, guidecollect);
	}
}

static void cache_object_vertices(Object *ob)
{
	Mesh *me;
	MVert *mvert;
	float *fp;
	int a;
	
	me= ob->data;
	if(me->totvert==0) return;

	fp= ob->sumohandle= MEM_mallocN(3*sizeof(float)*me->totvert, "cache particles");
	mvert= me->mvert;
	a= me->totvert;
	while(a--) {
		VECCOPY(fp, mvert->co);
		Mat4MulVecfl(ob->obmat, fp);
		mvert++;
		fp+= 3;
	}
}

static int pdDoDeflection(RNG *rng, float opco[3], float npco[3], float opno[3],
        float npno[3], float life, float force[3], int def_depth,
        float cur_time, unsigned int par_layer, int *last_object,
		int *last_face, int *same_face)
{
	/* Particle deflection code */
	/* The code is in two sections: the first part checks whether a particle has            */
	/* intersected a face of a deflector mesh, given its old and new co-ords, opco and npco */
	/* and which face it hit first                                                          */
	/* The second part calculates the new co-ordinates given that collision and updates     */
	/* the new co-ordinates accordingly */
	Base *base;
	Object *ob, *deflection_object = NULL;
	Mesh *def_mesh;
	MFace *mface, *deflection_face = NULL;
	float *v1, *v2, *v3, *v4, *vcache=NULL;
	float nv1[3], nv2[3], nv3[3], nv4[3], edge1[3], edge2[3];
	float dv1[3] = {0}, dv2[3] = {0}, dv3[3] = {0};
	float vect_to_int[3], refl_vel[3];
	float d_intersect_co[3], d_intersect_vect[3], d_nvect[3], d_i_co_above[3];
	float forcec[3];
	float k_point3, dist_to_plane;
	float first_dist, ref_plane_mag;
	float dk_plane=0, dk_point1=0;
	float icalctop, icalcbot, n_mag;
	float mag_iv, x_m,y_m,z_m;
	float damping, perm_thresh;
	float perm_val, rdamp_val;
	int a, deflected=0, deflected_now=0;
	float t,t2, min_t;
	float mat[3][3], obloc[3] = {0};
	int cur_frame;
	float time_before, time_after;
	float force_mag_norm;
	int d_object=0, d_face=0, ds_object=0, ds_face=0;

	first_dist = 200000;
	min_t = 200000;

	/* The first part of the code, finding the first intersected face*/
	base= G.scene->base.first;
	while (base) {
		/*Only proceed for mesh object in same layer */
		if(base->object->type==OB_MESH && (base->lay & par_layer)) {
			ob= base->object;
			/* only with deflecting set */
			if(ob->pd && ob->pd->deflect) {
				def_mesh= ob->data;
			
				d_object = d_object + 1;

				d_face = d_face + 1;
				mface= def_mesh->mface;
				a = def_mesh->totface;
				
				
				if(ob->parent==NULL && ob->ipo==NULL) {	// static
					if(ob->sumohandle==NULL) cache_object_vertices(ob);
					vcache= ob->sumohandle;
				}
				else {
					/*Find out where the object is at this time*/
					cur_frame = G.scene->r.cfra;
					G.scene->r.cfra = (int)cur_time;
					where_is_object_time(ob, cur_time);
					G.scene->r.cfra = cur_frame;
					
					/*Pass the values from ob->obmat to mat*/
					/*and the location values to obloc           */
					Mat3CpyMat4(mat,ob->obmat);
					obloc[0] = ob->obmat[3][0];
					obloc[1] = ob->obmat[3][1];
					obloc[2] = ob->obmat[3][2];
					vcache= NULL;

				}
				
				while (a--) {

					if(vcache) {
						v1= vcache+ 3*(mface->v1);
						VECCOPY(nv1, v1);
						v1= vcache+ 3*(mface->v2);
						VECCOPY(nv2, v1);
						v1= vcache+ 3*(mface->v3);
						VECCOPY(nv3, v1);
						v1= vcache+ 3*(mface->v4);
						VECCOPY(nv4, v1);
					}
					else {
						/* Calculate the global co-ordinates of the vertices*/
						v1= (def_mesh->mvert+(mface->v1))->co;
						v2= (def_mesh->mvert+(mface->v2))->co;
						v3= (def_mesh->mvert+(mface->v3))->co;
						v4= (def_mesh->mvert+(mface->v4))->co;
	
						VECCOPY(nv1, v1);
						VECCOPY(nv2, v2);
						VECCOPY(nv3, v3);
						VECCOPY(nv4, v4);
	
						/*Apply the objects deformation matrix*/
						Mat3MulVecfl(mat, nv1);
						Mat3MulVecfl(mat, nv2);
						Mat3MulVecfl(mat, nv3);
						Mat3MulVecfl(mat, nv4);
	
						VECADD(nv1, nv1, obloc);
						VECADD(nv2, nv2, obloc);
						VECADD(nv3, nv3, obloc);
						VECADD(nv4, nv4, obloc);
					}
					
					deflected_now = 0;

						
						
//					t= 0.5;	// this is labda of line, can use it optimize quad intersection
// sorry but no .. see below (BM)					
					if( LineIntersectsTriangle(opco, npco, nv1, nv2, nv3, &t, NULL) ) {
						if (t < min_t) {
							deflected = 1;
							deflected_now = 1;
						}
					}
//					else if (mface->v4 && (t>=0.0 && t<=1.0)) {
// no, you can't skip testing the other triangle
// it might give a smaller t on (close to) the edge .. this is numerics not esoteric maths :)
// note: the 2 triangles don't need to share a plane ! (BM)
					if (mface->v4) {
						if( LineIntersectsTriangle(opco, npco, nv1, nv3, nv4, &t2, NULL) ) {
							if (t2 < min_t) {
								deflected = 1;
								deflected_now = 2;
							}
	  					}
					}
					
					if ((deflected_now > 0) && ((t < min_t) ||(t2 < min_t))) {
                    	min_t = t;
                    	ds_object = d_object;
						ds_face = d_face;
						deflection_object = ob;
						deflection_face = mface;
						if (deflected_now==1) {
                    	min_t = t;
							VECCOPY(dv1, nv1);
							VECCOPY(dv2, nv2);
							VECCOPY(dv3, nv3);
						}
						else {
                    	min_t = t2;
							VECCOPY(dv1, nv1);
							VECCOPY(dv2, nv3);
							VECCOPY(dv3, nv4);
						}
					}
					mface++;
				}
			}
		}
		base = base->next;
	}


	/* Here's the point to do the permeability calculation */
	/* Set deflected to 0 if a random number is below the value */
	/* Get the permeability IPO here*/
	if (deflected) {
		
		if (has_ipo_code(deflection_object->ipo, OB_PD_PERM)) 
			perm_val = IPO_GetFloatValue(deflection_object->ipo, OB_PD_PERM, cur_time);
		else 
			perm_val = deflection_object->pd->pdef_perm;

		perm_thresh = rng_getFloat(rng) - perm_val;
		if (perm_thresh < 0 ) {
			deflected = 0;
		}
	}

	/* Now for the second part of the deflection code - work out the new speed */
	/* and position of the particle if a collision occurred */
	if (deflected) {
    	VECSUB(edge1, dv1, dv2);
		VECSUB(edge2, dv3, dv2);
		Crossf(d_nvect, edge2, edge1);
		n_mag = Normalize(d_nvect);
		dk_plane = INPR(d_nvect, nv1);
		dk_point1 = INPR(d_nvect,opco);

		VECSUB(d_intersect_vect, npco, opco);

		d_intersect_co[0] = opco[0] + (min_t * (npco[0] - opco[0]));
		d_intersect_co[1] = opco[1] + (min_t * (npco[1] - opco[1]));
		d_intersect_co[2] = opco[2] + (min_t * (npco[2] - opco[2]));
		
		d_i_co_above[0] = (d_intersect_co[0] + (0.001f * d_nvect[0]));
		d_i_co_above[1] = (d_intersect_co[1] + (0.001f * d_nvect[1]));
		d_i_co_above[2] = (d_intersect_co[2] + (0.001f * d_nvect[2]));
		mag_iv = Normalize(d_intersect_vect);
		VECCOPY(npco, d_intersect_co);
		
		VECSUB(vect_to_int, opco, d_intersect_co);
		first_dist = Normalize(vect_to_int);

		/* Work out the lengths of time before and after collision*/
		time_before = (life*(first_dist / (mag_iv)));
		time_after =  (life*((mag_iv - first_dist) / (mag_iv)));

		/* We have to recalculate what the speed would have been at the */
		/* point of collision, not the key frame time */
		npno[0]= opno[0] + time_before*force[0];
		npno[1]= opno[1] + time_before*force[1];
		npno[2]= opno[2] + time_before*force[2];


		/* Reflect the speed vector in the face */
		x_m = (2 * npno[0] * d_nvect[0]);
		y_m = (2 * npno[1] * d_nvect[1]);
		z_m = (2 * npno[2] * d_nvect[2]);
		refl_vel[0] = npno[0] - (d_nvect[0] * (x_m + y_m + z_m));
		refl_vel[1] = npno[1] - (d_nvect[1] * (x_m + y_m + z_m));
		refl_vel[2] = npno[2] - (d_nvect[2] * (x_m + y_m + z_m));

		/*A random variation in the damping factor........ */
		/*Get the IPO values for damping here*/
		
		if (has_ipo_code(deflection_object->ipo, OB_PD_SDAMP)) 
			damping = IPO_GetFloatValue(deflection_object->ipo, OB_PD_SDAMP, cur_time);
		else 
			damping = deflection_object->pd->pdef_damp;
		
		if (has_ipo_code(deflection_object->ipo, OB_PD_RDAMP)) 
			rdamp_val = IPO_GetFloatValue(deflection_object->ipo, OB_PD_RDAMP, cur_time);
		else 
			rdamp_val = deflection_object->pd->pdef_rdamp;

		damping = damping + ((1.0f - damping) * rng_getFloat(rng) *rdamp_val);
		damping = damping * damping;
        ref_plane_mag = INPR(refl_vel,d_nvect);

		if (damping > 0.999) damping = 0.999f;

		/* Now add in the damping force - only damp in the direction of */
		/* the faces normal vector */
		npno[0] = (refl_vel[0] - (d_nvect[0] * ref_plane_mag * damping));
		npno[1] = (refl_vel[1] - (d_nvect[1] * ref_plane_mag * damping));
		npno[2] = (refl_vel[2] - (d_nvect[2] * ref_plane_mag * damping));

		/* Now reset opno */
		VECCOPY(opno,npno);
		VECCOPY(forcec, force);

		/* If the particle has bounced more than four times on the same */
		/* face within this cycle (depth > 4, same face > 4 )           */
		/* Then set the force to be only that component of the force    */
		/* in the same direction as the face normal                     */
		/* i.e. subtract the component of the force in the direction    */
		/* of the face normal from the actual force                     */
		if ((ds_object == *last_object) && (ds_face == *last_face)) {
			/* Increment same_face */
			*same_face = *same_face + 1;
			if ((*same_face > 3) && (def_depth > 3)) {
            	force_mag_norm = INPR(forcec, d_nvect);
            	forcec[0] = forcec[0] - (d_nvect[0] * force_mag_norm);
                forcec[1] = forcec[1] - (d_nvect[1] * force_mag_norm);
                forcec[2] = forcec[2] - (d_nvect[2] * force_mag_norm);
			}
		}
		else *same_face = 1;

		*last_object = ds_object;
		*last_face = ds_face;

		/* We have the particles speed at the point of collision    */
		/* Now we want the particles speed at the current key frame */

		npno[0]= npno[0] + time_after*forcec[0];
		npno[1]= npno[1] + time_after*forcec[1];
		npno[2]= npno[2] + time_after*forcec[2];

		/* Now we have to recalculate pa->co for the remainder*/
		/* of the time since the intersect*/
		npco[0]= npco[0] + time_after*npno[0];
		npco[1]= npco[1] + time_after*npno[1];
		npco[2]= npco[2] + time_after*npno[2];

		/* And set the old co-ordinates back to the point just above the intersection */
		VECCOPY(opco, d_i_co_above);

		/* Finally update the time */
		life = time_after;
		cur_time += time_before;

		/* The particle may have fallen through the face again by now!!*/
		/* So check if the particle has changed sides of the plane compared*/
		/* the co-ordinates at the last keyframe*/
		/* But only do this as a last resort, if we've got to the end of the */
		/* number of collisions allowed */
		if (def_depth==9) {
			k_point3 = INPR(d_nvect,npco);
			if (((dk_plane > k_point3) && (dk_plane < dk_point1))||((dk_plane < k_point3) && (dk_plane > dk_point1))) {

				/* Yup, the pesky particle may have fallen through a hole!!! */
                /* So we'll cheat a bit and move the particle along the normal vector */
                /* until it's just the other side of the plane */
                icalctop = (dk_plane - d_nvect[0]*npco[0] - d_nvect[1]*npco[1] - d_nvect[2]*npco[2]);
                icalcbot = (d_nvect[0]*d_nvect[0] + d_nvect[1]*d_nvect[1] + d_nvect[2]*d_nvect[2]);
                dist_to_plane = icalctop / icalcbot;

                /*  Now just increase the distance a little to place */
                /* the point the other side of the plane */
                dist_to_plane *= 1.1f;
                npco[0]= npco[0] + (dist_to_plane * d_nvect[0]);
                npco[1]= npco[1] + (dist_to_plane * d_nvect[1]);
                npco[2]= npco[2] + (dist_to_plane * d_nvect[2]);

			}
		}
	}
	return deflected;
}

/*
	rng= random number generator
	ob = object that spawns the particles
	depth = for fireworks
	nr = index nr of current particle
	paf = the particle system
	part = current particle
	force = force vector
	deform = flag to indicate lattice deform
 */
static void make_particle_keys(RNG *rng, Object *ob, int depth, int nr, PartEff *paf, Particle *part, float *force, int deform, MTex *mtex, ListBase *effectorbase)
{
	Particle *pa, *opa = NULL;
	float damp, deltalife, life;
	float cur_time, maxspeed= paf->maxlen/(float)paf->totkey;
	float opco[3], opno[3], npco[3], npno[3], new_force[3], new_speed[3];
	int b, rt1, rt2, deflected, deflection, finish_defs, def_count;
	int last_ob, last_fc, same_fc;

	damp= 1.0f-paf->damp;
	pa= part;

	/* start speed: random */
	if(paf->randfac!=0.0) {
		pa->no[0]+= paf->randfac*(rng_getFloat(rng) - 0.5f);
		pa->no[1]+= paf->randfac*(rng_getFloat(rng) - 0.5f);
		pa->no[2]+= paf->randfac*(rng_getFloat(rng) - 0.5f);
	}

	/* start speed: texture */
	if(mtex && paf->texfac!=0.0) {
		particle_tex(mtex, paf, pa->co, pa->no);
	}

	/* effectors here? */
	if(effectorbase)
		precalc_effectors(ob, paf, pa, effectorbase);
	
	if(paf->totkey>1) deltalife= pa->lifetime/(paf->totkey-1);
	else deltalife= pa->lifetime;

	/* longer lifetime results in longer distance covered */
	VecMulf(pa->no, deltalife);
	
	opa= pa;
	pa++;

	for(b=1; b<paf->totkey; b++) {

		/* new time */
		pa->time= opa->time+deltalife;
		cur_time = pa->time;

		/* set initial variables                                */
		VECCOPY(opco, opa->co);
		VECCOPY(new_force, force);
		VECCOPY(new_speed, opa->no);
		VecMulf(new_speed, 1.0f/deltalife);
		//new_speed[0] = new_speed[1] = new_speed[2] = 0.0f;

		/* handle differences between static (local coords, fixed frame) and dynamic */
		if(effectorbase) {
			float loc_time= ((float)b)/(float)(paf->totkey-1);
			
			if(paf->flag & PAF_STATIC) {
				float opco1[3], new_force1[3];
				
				/* move co and force to global coords */
				VECCOPY(opco1, opco);
				Mat4MulVecfl(ob->obmat, opco1);
				VECCOPY(new_force1, new_force);
				Mat4Mul3Vecfl(ob->obmat, new_force1);
				Mat4Mul3Vecfl(ob->obmat, new_speed);
				
				cur_time = G.scene->r.cfra;
				
				/* force fields */
				pdDoEffectors(effectorbase, opco1, new_force1, new_speed, cur_time, loc_time, 0);
				
				/* move co, force and newspeed back to local */
				VECCOPY(opco, opco1);
				Mat4MulVecfl(ob->imat, opco);
				VECCOPY(new_force, new_force1);
				Mat4Mul3Vecfl(ob->imat, new_force);
				Mat4Mul3Vecfl(ob->imat, new_speed);
			}
			else {
				 /* force fields */
				pdDoEffectors(effectorbase, opco, new_force, new_speed, cur_time, loc_time, 0);
			}
		}
		
		/* new speed */
		pa->no[0]= deltalife * (new_speed[0] + new_force[0]);
		pa->no[1]= deltalife * (new_speed[1] + new_force[1]);
		pa->no[2]= deltalife * (new_speed[2] + new_force[2]);
		
		/* speed limitor */
		if((paf->flag & PAF_STATIC) && maxspeed!=0.0f) {
			float len= VecLength(pa->no);
			if(len > maxspeed)
				VecMulf(pa->no, maxspeed/len);
		}
			
		/* new location */
		pa->co[0]= opa->co[0] + pa->no[0];
		pa->co[1]= opa->co[1] + pa->no[1];
		pa->co[2]= opa->co[2] + pa->no[2];

		/* Particle deflection code */
		if((paf->flag & PAF_STATIC)==0) {
			deflection = 0;
			finish_defs = 1;
			def_count = 0;

			VECCOPY(opno, opa->no);
			VECCOPY(npco, pa->co);
			VECCOPY(npno, pa->no);

			life = deltalife;
			cur_time -= deltalife;

			last_ob = -1;
			last_fc = -1;
			same_fc = 0;

			/* First call the particle deflection check for the particle moving   */
			/* between the old co-ordinates and the new co-ordinates              */
			/* If a deflection occurs, call the code again, this time between the */
			/* intersection point and the updated new co-ordinates                */
			/* Bail out if we've done the calculation 10 times - this seems ok     */
			/* for most scenes I've tested */
			while (finish_defs) {
				deflected =  pdDoDeflection(rng, opco, npco, opno, npno, life, new_force,
								def_count, cur_time, ob->lay,
								&last_ob, &last_fc, &same_fc);
				if (deflected) {
					def_count = def_count + 1;
					deflection = 1;
					if (def_count==10) finish_defs = 0;
				}
				else {
					finish_defs = 0;
				}
			}

			/* Only update the particle positions and speed if we had a deflection */
			if (deflection) {
				pa->co[0] = npco[0];
				pa->co[1] = npco[1];
				pa->co[2] = npco[2];
				pa->no[0] = npno[0];
				pa->no[1] = npno[1];
				pa->no[2] = npno[2];
			}
		}
		
		/* speed: texture */
		if(mtex && paf->texfac!=0.0) {
			particle_tex(mtex, paf, pa->co, pa->no);
		}
		if(damp!=1.0) {
			pa->no[0]*= damp;
			pa->no[1]*= damp;
			pa->no[2]*= damp;
		}
		
		opa= pa;
		pa++;
		/* opa is used later on too! */
	}

	if(deform) {
		/* deform all keys */
		pa= part;
		b= paf->totkey;
		while(b--) {
			calc_latt_deform(pa->co, 1.0f);
			pa++;
		}
	}
	
	/* the big multiplication */
	if(depth<PAF_MAXMULT && paf->mult[depth]!=0.0) {
		
		/* new 'child' emerges from an average 'mult' part from 
			the particles */
		damp = (float)nr;
		rt1= (int)(damp*paf->mult[depth]);
		rt2= (int)((damp+1.0)*paf->mult[depth]);
		if(rt1!=rt2) {
			
			for(b=0; b<paf->child[depth]; b++) {
				pa= new_particle(paf);
				*pa= *opa;
				pa->lifetime= paf->life[depth];
				if(paf->randlife!=0.0) {
					pa->lifetime*= 1.0f + paf->randlife*(rng_getFloat(rng) - 0.5f);
				}
				pa->mat_nr= paf->mat[depth];

				make_particle_keys(rng, ob, depth+1, b, paf, pa, force, deform, mtex, effectorbase);
			}
		}
	}
}

static void init_mv_jit(float *jit, int num, int seed2)
{
	RNG *rng;
	float *jit2, x, rad1, rad2, rad3;
	int i, num2;

	if(num==0) return;

	rad1= (float)(1.0/sqrt((float)num));
	rad2= (float)(1.0/((float)num));
	rad3= (float)sqrt((float)num)/((float)num);

	rng = rng_new(31415926 + num + seed2);
	x= 0;
        num2 = 2 * num;
	for(i=0; i<num2; i+=2) {
	
		jit[i]= x + rad1*(0.5f - rng_getFloat(rng));
		jit[i+1]= i/(2.0f*num) + rad1*(0.5f - rng_getFloat(rng));
		
		jit[i]-= (float)floor(jit[i]);
		jit[i+1]-= (float)floor(jit[i+1]);
		
		x+= rad3;
		x -= (float)floor(x);
	}

	jit2= MEM_mallocN(12 + 2*sizeof(float)*num, "initjit");

	for (i=0 ; i<4 ; i++) {
		BLI_jitterate1(jit, jit2, num, rad1);
		BLI_jitterate1(jit, jit2, num, rad1);
		BLI_jitterate2(jit, jit2, num, rad2);
	}
	MEM_freeN(jit2);
	rng_free(rng);
}

#define JIT_RAND	32

/* for a position within a face, tot is total amount of faces */
static void give_mesh_particle_coord(PartEff *paf, VeNoCo *noco, MFace *mface, int partnr, int subnr, float *co, float *no)
{
	static float *jit= NULL;
	static float *trands= NULL;
	static int jitlevel= 1;
	float *v1, *v2, *v3, *v4;
	float u, v;
	float *n1, *n2, *n3, *n4;
	
	/* free signal */
	if(paf==NULL) {
		if(jit) MEM_freeN(jit);
		jit= NULL;
		if(trands) MEM_freeN(trands);
		trands= NULL;
		return;
	}
	
	/* first time initialize jitter or trand, partnr then is total amount of particles, subnr total amount of faces */
	if(trands==NULL && jit==NULL) {
		RNG *rng = rng_new(31415926 + paf->seed);
		int i, tot;

		if(paf->flag & PAF_TRAND)
			tot= partnr;
		else 
			tot= JIT_RAND;	/* arbitrary... allows JIT_RAND times more particles in a face for jittered distro */
			
		trands= MEM_callocN(2+2*tot*sizeof(float), "trands");
		for(i=0; i<tot; i++) {
			trands[2*i]= rng_getFloat(rng);
			trands[2*i+1]= rng_getFloat(rng);
		}
		rng_free(rng);

		if((paf->flag & PAF_TRAND)==0) {
			jitlevel= paf->userjit;
			
			if(jitlevel == 0) {
				jitlevel= partnr/subnr;
				if(paf->flag & PAF_EDISTR) jitlevel*= 2;	/* looks better in general, not very scietific */
				if(jitlevel<3) jitlevel= 3;
				if(jitlevel>100) jitlevel= 100;
			}
			
			jit= MEM_callocN(2+ jitlevel*2*sizeof(float), "jit");
			init_mv_jit(jit, jitlevel, paf->seed);
			BLI_array_randomize(jit, 2*sizeof(float), jitlevel, paf->seed); /* for custom jit or even distribution */
		}
		return;
	}
	
	if(paf->flag & PAF_TRAND) {
		u= trands[2*partnr];
		v= trands[2*partnr+1];
	}
	else {
		/* jittered distribution gets fixed random offset */
		if(subnr>=jitlevel) {
			int jitrand= (subnr/jitlevel) % JIT_RAND;
		
			subnr %= jitlevel;
			u= jit[2*subnr] + trands[2*jitrand];
			v= jit[2*subnr+1] + trands[2*jitrand+1];
			if(u > 1.0f) u-= 1.0f;
			if(v > 1.0f) v-= 1.0f;
		}
		else {
			u= jit[2*subnr];
			v= jit[2*subnr+1];
		}
	}
	
	v1= (noco+(mface->v1))->co;
	v2= (noco+(mface->v2))->co;
	v3= (noco+(mface->v3))->co;
	n1= (noco+(mface->v1))->no;
	n2= (noco+(mface->v2))->no;
	n3= (noco+(mface->v3))->no;
	
	if(mface->v4) {
		float uv= u*v;
		float muv= (1.0f-u)*(v);
		float umv= (u)*(1.0f-v);
		float mumv= (1.0f-u)*(1.0f-v);
		
		v4= (noco+(mface->v4))->co;
		n4= (noco+(mface->v4))->no;
		
		co[0]= mumv*v1[0] + muv*v2[0] + uv*v3[0] + umv*v4[0];
		co[1]= mumv*v1[1] + muv*v2[1] + uv*v3[1] + umv*v4[1];
		co[2]= mumv*v1[2] + muv*v2[2] + uv*v3[2] + umv*v4[2];

		no[0]= mumv*n1[0] + muv*n2[0] + uv*n3[0] + umv*n4[0];
		no[1]= mumv*n1[1] + muv*n2[1] + uv*n3[1] + umv*n4[1];
		no[2]= mumv*n1[2] + muv*n2[2] + uv*n3[2] + umv*n4[2];
	}
	else {
		/* mirror triangle uv coordinates when on other side */
		if(u + v > 1.0f) {
			u= 1.0f-u;
			v= 1.0f-v;
		}
		co[0]= v1[0] + u*(v3[0]-v1[0]) + v*(v2[0]-v1[0]);
		co[1]= v1[1] + u*(v3[1]-v1[1]) + v*(v2[1]-v1[1]);
		co[2]= v1[2] + u*(v3[2]-v1[2]) + v*(v2[2]-v1[2]);
		
		no[0]= n1[0] + u*(n3[0]-n1[0]) + v*(n2[0]-n1[0]);
		no[1]= n1[1] + u*(n3[1]-n1[1]) + v*(n2[1]-n1[1]);
		no[2]= n1[2] + u*(n3[2]-n1[2]) + v*(n2[2]-n1[2]);
	}
}


/* Gets a MDeformVert's weight in group (0 if not in group) */
/* note; this call could be in mesh.c or deform.c, but OK... it's in armature.c too! (ton) */
static float vert_weight(MDeformVert *dvert, int group)
{
	MDeformWeight *dw;
	int i;
	
	if(dvert) {
		dw= dvert->dw;
		for(i= dvert->totweight; i>0; i--, dw++) {
			if(dw->def_nr == group) return dw->weight;
			if(i==1) break; /*otherwise dw will point to somewhere it shouldn't*/
		}
	}
	return 0.0;
}

/* Gets a faces average weight in a group, helper for below, face and weights are always set */
static float face_weight(MFace *face, float *weights)
{
	float tweight;
	
	tweight = weights[face->v1] + weights[face->v2] + weights[face->v3];
	
	if(face->v4) {
		tweight += weights[face->v4];
		tweight /= 4.0;
	}
	else {
		tweight /= 3.0;
	}

	return tweight;
}

/* helper function for build_particle_system() */
static void make_weight_tables(PartEff *paf, Mesh *me, int totpart, VeNoCo *vertlist, int totvert, MFace *facelist, int totface, float **vweights, float **fweights)
{
	MFace *mface;
	float *foweights=NULL, *voweights=NULL;
	float totvweight=0.0f, totfweight=0.0f;
	int a;
	
	if((paf->flag & PAF_FACE)==0) totface= 0;
	
	/* collect emitting vertices & faces if vert groups used */
	if(paf->vertgroup && me->dvert) {
		
		/* allocate weights array for all vertices, also for lookup of faces later on. note it's a malloc */
		*vweights= voweights= MEM_mallocN( totvert*sizeof(float), "pafvoweights" );
		totvweight= 0.0f;
		for(a=0; a<totvert; a++) {
			voweights[a]= vert_weight(me->dvert+a, paf->vertgroup-1);
			totvweight+= voweights[a];
		}
		
		if(totface) {
			/* allocate weights array for faces, note it's a malloc */
			*fweights= foweights= MEM_mallocN(totface*sizeof(float), "paffoweights" );
			for(a=0, mface=facelist; a<totface; a++, mface++) {
				foweights[a] = face_weight(mface, voweights);
			}
		}
	}
	
	/* make weights for faces or for even area distribution */
	if(totface && (paf->flag & PAF_EDISTR)) {
		float maxfarea= 0.0f, curfarea;
		
		/* two cases for area distro, second case we already have group weights */
		if(foweights==NULL) {
			/* allocate weights array for faces, note it's a malloc */
			*fweights= foweights= MEM_mallocN(totface*sizeof(float), "paffoweights" );
			
			for(a=0, mface=facelist; a<totface; a++, mface++) {
				if (mface->v4)
					curfarea= AreaQ3Dfl(vertlist[mface->v1].co, vertlist[mface->v2].co, vertlist[mface->v3].co, vertlist[mface->v4].co);
				else
					curfarea= AreaT3Dfl(vertlist[mface->v1].co,  vertlist[mface->v2].co, vertlist[mface->v3].co);
				if(curfarea>maxfarea)
					maxfarea = curfarea;
				foweights[a]= curfarea;
			}
		}
		else {
			for(a=0, mface=facelist; a<totface; a++, mface++) {
				if(foweights[a]!=0.0f) {
					if (mface->v4)
						curfarea= AreaQ3Dfl(vertlist[mface->v1].co, vertlist[mface->v2].co, vertlist[mface->v3].co, vertlist[mface->v4].co);
					else
						curfarea= AreaT3Dfl(vertlist[mface->v1].co,  vertlist[mface->v2].co, vertlist[mface->v3].co);
					if(curfarea>maxfarea)
						maxfarea = curfarea;
					foweights[a]*= curfarea;
				}
			}
		}
		
		/* normalize weights for max face area, calculate tot */
		if(maxfarea!=0.0f) {
			maxfarea= 1.0f/maxfarea;
			for(a=0; a< totface; a++) {
				if(foweights[a]!=0.0) {
					foweights[a] *= maxfarea;
					totfweight+= foweights[a];
				}
			}
		}
	}
	else if(foweights) {
		/* only add totfweight value */
		for(a=0; a< totface; a++) {
			if(foweights[a]!=0.0) {
				totfweight+= foweights[a];
			}
		}
	}
	
	/* if weight arrays, we turn these arrays into the amount of particles */
	if(totvert && voweights) {
		float mult= (float)totpart/totvweight;
		
		for(a=0; a< totvert; a++) {
			if(voweights[a]!=0.0)
				voweights[a] *= mult;
		}
	}
	
	if(totface && foweights) {
		float mult= (float)totpart/totfweight;
		
		for(a=0; a< totface; a++) {
			if(foweights[a]!=0.0)
				foweights[a] *= mult;
		}
	}
}

/* helper function for build_particle_system() */
static void make_length_tables(PartEff *paf, Mesh *me, int totvert, MFace *facelist, int totface, float **vlengths, float **flengths)
{
	MFace *mface;
	float *folengths=NULL, *volengths=NULL;
	int a;
	
	if((paf->flag & PAF_FACE)==0) totface= 0;
	
	/* collect emitting vertices & faces if vert groups used */
	if(paf->vertgroup_v && me->dvert) {
		
		/* allocate lengths array for all vertices, also for lookup of faces later on. note it's a malloc */
		*vlengths= volengths= MEM_mallocN( totvert*sizeof(float), "pafvolengths" );
		for(a=0; a<totvert; a++) {
			volengths[a]= vert_weight(me->dvert+a, paf->vertgroup_v-1);
		}
		
		if(totface) {
			/* allocate lengths array for faces, note it's a malloc */
			*flengths= folengths= MEM_mallocN(totface*sizeof(float), "paffolengths" );
			for(a=0, mface=facelist; a<totface; a++, mface++) {
				folengths[a] = face_weight(mface, volengths);
			}
		}
	}
}


/* for paf start to end, store all matrices for objects */
typedef struct pMatrixCache {
	float obmat[4][4];
	float imat[3][3];
} pMatrixCache;


/* WARN: this function stores data in ob->id.idnew! */
/* error: this function changes ob->recalc of other objects... */
static pMatrixCache *cache_object_matrices(Object *ob, int start, int end)
{
	pMatrixCache *mcache, *mc;
	Group *group= NULL;
	Object *obcopy;
	Base *base;
	float framelenold, cfrao, sfo;
	
	/* object can be linked in group... stupid exception */
	if(NULL==object_in_scene(ob, G.scene))
		group= find_group(ob);
	
	mcache= mc= MEM_mallocN( (end-start+1)*sizeof(pMatrixCache), "ob matrix cache");
	
	framelenold= G.scene->r.framelen;
	G.scene->r.framelen= 1.0f;
	cfrao= G.scene->r.cfra;
	sfo= ob->sf;
	ob->sf= 0.0f;

	/* clear storage, copy recalc tag (bad loop) */
	for(obcopy= G.main->object.first; obcopy; obcopy= obcopy->id.next) {
		obcopy->id.newid= NULL;
		obcopy->recalco= obcopy->recalc;
		obcopy->recalc= 0;
	}
	
	/* all objects get tagged recalc that influence this object (does group too) */
	/* note that recalco has the real recalc tags, set by callers of this function */
	ob->recalc |= OB_RECALC_OB; /* make sure a recalc gets flushed */
	DAG_object_update_flags(G.scene, ob, -1);
	
	for(G.scene->r.cfra= start; G.scene->r.cfra<=end; G.scene->r.cfra++, mc++) {
		
		if(group) {
			GroupObject *go;

			for(go= group->gobject.first; go; go= go->next) {
				if(go->ob->recalc) {
					where_is_object(go->ob);
					
					do_ob_key(go->ob);
					if(go->ob->type==OB_ARMATURE) {
						do_all_pose_actions(go->ob);	// only does this object actions
						where_is_pose(go->ob);
					}
				}
			}
		}
		else {
			for(base= G.scene->base.first; base; base= base->next) {
				if(base->object->recalc) {
					if(base->object->id.newid==NULL)
						base->object->id.newid= MEM_dupallocN(base->object);
					
					where_is_object(base->object);
					
					do_ob_key(base->object);
					if(base->object->type==OB_ARMATURE) {
						do_all_pose_actions(base->object);	// only does this object actions
						where_is_pose(base->object);
					}
				}
			}
		}		
		Mat4CpyMat4(mc->obmat, ob->obmat);
		Mat4Invert(ob->imat, ob->obmat);
		Mat3CpyMat4(mc->imat, ob->imat);
		Mat3Transp(mc->imat);
	}
	
	/* restore */
	G.scene->r.cfra= cfrao;
	G.scene->r.framelen= framelenold;
	ob->sf= sfo;
	
	if(group) {
		GroupObject *go;
		
		for(go= group->gobject.first; go; go= go->next) {
			if(go->ob->recalc) {
				where_is_object(go->ob);
				
				do_ob_key(go->ob);
				if(go->ob->type==OB_ARMATURE) {
					do_all_pose_actions(go->ob);	// only does this object actions
					where_is_pose(go->ob);
				}
			}
		}
	}
	else {
		for(base= G.scene->base.first; base; base= base->next) {
			if(base->object->recalc) {
				
				if(base->object->id.newid) {
					obcopy= (Object *)base->object->id.newid;
					*(base->object) = *(obcopy); 
					MEM_freeN(obcopy);
					base->object->id.newid= NULL;
				}
				
				do_ob_key(base->object);
				if(base->object->type==OB_ARMATURE) {
					do_all_pose_actions(base->object);	// only does this object actions
					where_is_pose(base->object);
				}
			}
		}
	}
	
	/* copy recalc tag (bad loop) */
	for(obcopy= G.main->object.first; obcopy; obcopy= obcopy->id.next)
		obcopy->recalc= obcopy->recalco;
	
	return mcache;
}

/* for fluidsim win32 debug messages */
#if defined(WIN32) && (!(defined snprintf))
#define snprintf _snprintf
#endif

/* main particle building function 
   one day particles should become dynamic (realtime) with the current method as a 'bake' (ton) */
void build_particle_system(Object *ob)
{
	RNG *rng;
	PartEff *paf;
	Particle *pa;
	Mesh *me;
	Base *base;
	MTex *mtexmove=0, *mtextime=0;
	Material *ma;
	MFace *facelist= NULL;
	pMatrixCache *mcache=NULL, *mcnow, *mcprev;
	ListBase *effectorbase;
	VeNoCo *vertexcosnos;
	double startseconds= PIL_check_seconds_timer();
	float ftime, dtime, force[3], vec[3], fac, co[3], no[3];
	float *voweights= NULL, *foweights= NULL, maxw=1.0f;
	float *volengths= NULL, *folengths= NULL;
	int deform=0, a, totpart, paf_sta, paf_end;
	int waitcursor_set= 0, totvert, totface, curface, curvert;
#ifndef DISABLE_ELBEEM
	int readMask, activeParts, fileParts;
#endif
	
	/* return conditions */
	if(ob->type!=OB_MESH) return;
	me= ob->data;

	paf= give_parteff(ob);
	if(paf==NULL) return;
	
	if(G.rendering==0 && paf->disp==0) return;
	
	if(paf->keys) MEM_freeN(paf->keys);	/* free as early as possible, for returns */
	paf->keys= NULL;
	
	//printf("build particles\n");
	
	/* fluid sim particle import handling, actual loading of particles from file */
	#ifndef DISABLE_ELBEEM
	if( (1) && (ob->fluidsimFlag & OB_FLUIDSIM_ENABLE) &&  // broken, disabled for now!
	    (ob->fluidsimSettings) && 
		  (ob->fluidsimSettings->type == OB_FLUIDSIM_PARTICLE)) {
		char *suffix  = "fluidsurface_particles_#";
		char *suffix2 = ".gz";
		char filename[256];
		char debugStrBuffer[256];
		int  curFrame = G.scene->r.cfra -1; // warning - sync with derived mesh fsmesh loading
		int  j, numFileParts;
		gzFile gzf;
		float vel[3];

		if(ob==G.obedit) { // off...
			paf->totpart = 0; // 1 or 0?
			return;
		}

		// ok, start loading
		strcpy(filename, ob->fluidsimSettings->surfdataPath);
		strcat(filename, suffix);
		BLI_convertstringcode(filename, G.sce, curFrame); // fixed #frame-no 
		strcat(filename, suffix2);

		gzf = gzopen(filename, "rb");
		if (!gzf) {
			snprintf(debugStrBuffer,256,"readFsPartData::error - Unable to open file for reading '%s' \n", filename); 
			//elbeemDebugOut(debugStrBuffer);
			paf->totpart = 0;
			return;
		}

		gzread(gzf, &totpart, sizeof(totpart));
		numFileParts = totpart;
		totpart = (G.rendering)?totpart:(paf->disp*totpart)/100;
		paf->totpart= totpart;
		paf->totkey= 1;
		/* initialize particles */
		new_particle(paf); 
		ftime = 0.0; // unused...

		// set up reading mask
		readMask = ob->fluidsimSettings->typeFlags;
		activeParts=0;
		fileParts=0;
		
		for(a=0; a<totpart; a++) {
			int ptype=0;
			short shsize=0;
			float convertSize=0.0;
			gzread(gzf, &ptype, sizeof( ptype )); 
			if(ptype&readMask) {
				activeParts++;
				pa= new_particle(paf);
				pa->time= ftime;
				pa->lifetime= ftime + 10000.; // add large number to make sure they are displayed, G.scene->r.efra +1.0;
				pa->co[0] = 0.0;
				pa->co[1] = 
				pa->co[2] = 1.0*(float)a / (float)totpart;
				pa->no[0] = pa->no[1] = pa->no[2] = 0.0;
				pa->mat_nr= paf->omat;
				gzread(gzf, &convertSize, sizeof( float )); 
				// convert range of  1.0-10.0 to shorts 1000-10000)
				shsize = (short)(convertSize*1000.0);
				pa->rt = shsize;

				for(j=0; j<3; j++) {
					float wrf;
					gzread(gzf, &wrf, sizeof( wrf )); 
					pa->co[j] = wrf;
					//fprintf(stderr,"Rj%d ",j);
				}
				for(j=0; j<3; j++) {
					float wrf;
					gzread(gzf, &wrf, sizeof( wrf )); 
					vel[j] = wrf;
				}
				//if(a<25) fprintf(stderr,"FSPARTICLE debug set %s , a%d = %f,%f,%f , life=%f \n", filename, a, pa->co[0],pa->co[1],pa->co[2], pa->lifetime );
			} else {
				// skip...
				for(j=0; j<2*3+1; j++) {
					float wrf; gzread(gzf, &wrf, sizeof( wrf )); 
				}
			}
			fileParts++;
		}
		gzclose( gzf );

		totpart = paf->totpart = activeParts;
		snprintf(debugStrBuffer,256,"readFsPartData::done - particles:%d, active:%d, file:%d, mask:%d  \n", paf->totpart,activeParts,fileParts,readMask);
		elbeemDebugOut(debugStrBuffer);
		return;
	} // fluid sim particles done
	#endif // DISABLE_ELBEEM
	
	if(paf->end < paf->sta) return;
	
	if( (paf->flag & PAF_OFACE) && (paf->flag & PAF_FACE)==0) return;
	
	if(me->totvert==0) return;
	
	if(ob==G.obedit) return;
	totpart= (G.rendering)?paf->totpart:(paf->disp*paf->totpart)/100;
	if(totpart==0) return;
	
	/* No returns after this line! */
	
	/* material */
	ma= give_current_material(ob, paf->omat);
	if(ma) {
		if(paf->speedtex)
			mtexmove= ma->mtex[paf->speedtex-1];
		mtextime= ma->mtex[paf->timetex-1];
	}

	disable_speed_curve(1);	/* check this... */

	/* initialize particles */
	new_particle(paf);

	/* reset deflector cache, sumohandle is free, but its still sorta abuse... (ton) */
	for(base= G.scene->base.first; base; base= base->next)
		base->object->sumohandle= NULL;

	/* all object positions from start to end */
	paf_sta= (int)floor(paf->sta);
	paf_end= (int)ceil(paf->end);
	if((paf->flag & PAF_STATIC)==0)
		mcache= cache_object_matrices(ob, paf_sta, paf_end);
	
	/* mult generations? */
	for(a=0; a<PAF_MAXMULT; a++) {
		if(paf->mult[a]!=0.0) {
			/* interesting formula! this way after 'x' generations the total is paf->totpart */
			totpart= (int)(totpart / (1.0+paf->mult[a]*paf->child[a]));
		}
		else break;
	}

	/* for static particles, calculate system on current frame (? ton) */
	if(ma) do_mat_ipo(ma);
	
	/* matrix invert for static too */
	Mat4Invert(ob->imat, ob->obmat);
	Mat4CpyMat4(paf->imat, ob->imat);	/* used for duplicators */
	
	/* new random generator */
	rng = rng_new(paf->seed);
	
	/* otherwise it goes way too fast */
	force[0]= paf->force[0]*0.05f;
	force[1]= paf->force[1]*0.05f;
	force[2]= paf->force[2]*0.05f;
	
	if( paf->flag & PAF_STATIC ) deform= 0;
	else {
		Object *parlatt= modifiers_isDeformedByLattice(ob);
		if(parlatt) {
			deform= 1;
			init_latt_deform(parlatt, 0);
		}
	}
	
	/* get the effectors */
	effectorbase= pdInitEffectors(ob, paf->group);
	
	/* init geometry, return is 6 x float * me->totvert in size */
	vertexcosnos= (VeNoCo *)mesh_get_mapped_verts_nors(ob);
	facelist= me->mface;
	totvert= me->totvert;
	totface= me->totface;
	
	/* if vertexweights or even distribution, it makes weight tables, also checks where it emits from */
	make_weight_tables(paf, me, totpart, vertexcosnos, totvert, facelist, totface, &voweights, &foweights);
	
	/* vertexweights can define lengths too */
	make_length_tables(paf, me, totvert, facelist, totface, &volengths, &folengths);
	
	/* now define where to emit from, if there are face weights we skip vertices */
	if(paf->flag & PAF_OFACE) totvert= 0;
	if((paf->flag & PAF_FACE)==0) totface= 0;
	if(foweights) totvert= 0;
	
	/* initialize give_mesh_particle_coord */
	if(totface)
		give_mesh_particle_coord(paf, vertexcosnos, facelist, totpart, totface, NULL, NULL);
	
	/* correction for face timing when using weighted average */
	if(totface && foweights) {
		maxw= (paf->end-paf->sta)/foweights[0];
	}
	else if(totvert && voweights) {
		maxw= (paf->end-paf->sta)/voweights[0];
	}
	
	/* for loop below */
	if (paf->flag & PAF_STATIC) {
		ftime = G.scene->r.cfra;
		dtime= 0.0f;
	} else {
		ftime= paf->sta;
		dtime= (paf->end - paf->sta)/(float)totpart;
	}
	
	curface= curvert= 0;
	for(a=0; a<totpart; a++, ftime+=dtime) {
		
		/* we set waitcursor only when a half second expired, particles now are realtime updated */
		if(waitcursor_set==0 && (a % 256)==255) {
			double seconds= PIL_check_seconds_timer();
			if(seconds - startseconds > 0.5) {
				waitcursor(1);
				waitcursor_set= 1;
			}
		}
		
		pa= new_particle(paf);
		pa->time= ftime;

		/* get coordinates from faces, only when vertices set to zero */
		if(totvert==0 && totface) {
			int curjit;
			
			/* use weight table, we have to do faces in order to be able to use jitter table... */
			if(foweights) {
				
				if(foweights[curface] < 1.0f) {
					float remainder= 0.0f;
					
					while(remainder + foweights[curface] < 1.0f && curface<totface-1) {
						remainder += foweights[curface];
						curface++;
					}
					/* if this is the last face, the foweights[] can be zero, so we don't add a particle extra */
					if(curface!=totface-1)
						foweights[curface] += remainder;
					
					maxw= (paf->end-paf->sta)/foweights[curface];
				}

				if(foweights[curface]==0.0f)
					break;	/* WARN skips here out of particle generating */
				else {
					if(foweights[curface] >= 1.0f)		/* note the >= here, this because of the < 1.0f above, it otherwise will stick to 1 face forever */
						foweights[curface] -= 1.0f;
					
					curjit= (int) foweights[curface];
					give_mesh_particle_coord(paf, vertexcosnos, facelist+curface, a, curjit, co, no);
					
					/* time correction to make particles appear evenly, maxw does interframe (0-1) */
					pa->time= paf->sta + maxw*foweights[curface];
				}
			}
			else {
				curface= a % totface;
				curjit= a/totface;
				give_mesh_particle_coord(paf, vertexcosnos, facelist+curface, a, curjit, co, no);
			}
		}
		/* get coordinates from vertices */
		if(totvert) {
			/* use weight table */
			if(voweights) {
				
				if(voweights[curvert] < 1.0f) {
					float remainder= 0.0f;
					
					while(remainder + voweights[curvert] < 1.0f && curvert<totvert-1) {
						remainder += voweights[curvert];
						curvert++;
					}
					voweights[curvert] += remainder;
					maxw= (paf->end-paf->sta)/voweights[curvert];
				}
				
				if(voweights[curvert]==0.0f)
					break;	/* WARN skips here out of particle generating */
				else {
					if(voweights[curvert] > 1.0f)
						voweights[curvert] -= 1.0f;
					
					/* time correction to make particles appear evenly */
					pa->time= paf->sta + maxw*voweights[curvert];
				}
			}
			else {
				curvert= a % totvert;
				if(a >= totvert && totface)
					totvert= 0;
			}
			
			VECCOPY(co, vertexcosnos[curvert].co);
			VECCOPY(no, vertexcosnos[curvert].no);
		}
		
		VECCOPY(pa->co, co);
		
		/* dynamic options */
		if((paf->flag & PAF_STATIC)==0) {
			int cur;
			
			/* particle retiming with texture */
			if(mtextime && (paf->flag2 & PAF_TEXTIME)) {
				float tin, tr, tg, tb, ta, orco[3];
				
				/* calculate normalized orco */
				orco[0] = (co[0]-me->loc[0])/me->size[0];
				orco[1] = (co[1]-me->loc[1])/me->size[1];
				orco[2] = (co[2]-me->loc[2])/me->size[2];
				externtex(mtextime, orco, &tin, &tr, &tg, &tb, &ta);
				
				if(paf->flag2neg & PAF_TEXTIME)
					pa->time = paf->sta + (paf->end - paf->sta)*tin;
				else
					pa->time = paf->sta + (paf->end - paf->sta)*(1.0f-tin);
			}

			/* set ob at correct time, we use cached matrices */
			cur= (int)floor(pa->time) + 1 ;		/* + 1 has a reason: (obmat/prevobmat) otherwise comet-tails start too late */
			
			if(cur <= paf_end) mcnow= mcache + cur - paf_sta;
			else mcnow= mcache + paf_end - paf_sta;
			
			if(cur > paf_sta) mcprev= mcnow-1;
			else mcprev= mcache;
			
			/* move to global space */
			Mat4MulVecfl(mcnow->obmat, pa->co);
		
			VECCOPY(vec, co);
			Mat4MulVecfl(mcprev->obmat, vec);
			
			/* first start speed: object */
			VECSUB(pa->no, pa->co, vec);

			VecMulf(pa->no, paf->obfac);
			
			/* calculate the correct inter-frame */	
			fac= (pa->time- (float)floor(pa->time));
			pa->co[0]= fac*pa->co[0] + (1.0f-fac)*vec[0];
			pa->co[1]= fac*pa->co[1] + (1.0f-fac)*vec[1];
			pa->co[2]= fac*pa->co[2] + (1.0f-fac)*vec[2];

			/* start speed: normal */
			if(paf->normfac!=0.0) {
				/* imat is transpose ! */
				VECCOPY(vec, no);
				Mat3MulVecfl(mcnow->imat, vec);
			
				Normalize(vec);
				VecMulf(vec, paf->normfac);
				VECADD(pa->no, pa->no, vec);
			}
		}
		else {
			if(paf->normfac!=0.0) {
				VECCOPY(pa->no, no);
				Normalize(pa->no);
				VecMulf(pa->no, paf->normfac);
			}
		}
		
		pa->lifetime= paf->lifetime;
		if(paf->randlife!=0.0) {
			pa->lifetime*= 1.0f + paf->randlife*(rng_getFloat(rng) - 0.5f);
		}
		pa->mat_nr= paf->omat;
		
		if(folengths)
			pa->lifetime*= folengths[curface];

		make_particle_keys(rng, ob, 0, a, paf, pa, force, deform, mtexmove, effectorbase);
	}
	
	/* free stuff */
	give_mesh_particle_coord(NULL, NULL, NULL, 0, 0, NULL, NULL);
	MEM_freeN(vertexcosnos);
	if(voweights) MEM_freeN(voweights);
	if(foweights) MEM_freeN(foweights);
	if(volengths) MEM_freeN(volengths);
	if(folengths) MEM_freeN(folengths);
	if(mcache) MEM_freeN(mcache);
	rng_free(rng);

	if(deform) end_latt_deform();
	
	if(effectorbase)
		pdEndEffectors(effectorbase);	
	
	/* reset deflector cache */
	for(base= G.scene->base.first; base; base= base->next) {
		if(base->object->sumohandle) {
			
			MEM_freeN(base->object->sumohandle);
			base->object->sumohandle= NULL;
		}
	}

	disable_speed_curve(0);
	
	if(waitcursor_set) waitcursor(0);
}

