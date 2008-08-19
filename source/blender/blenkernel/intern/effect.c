/*  effect.c
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
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
#include "BKE_collision.h"
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

/* ***************** PARTICLES ***************** */

/* deprecated, only keep this for readfile.c */
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
		
		if(pd->forcefield == PFIELD_WIND)
		{
			pd->rng = rng_new(1);
			rng_srandom(pd->rng, (unsigned int)(ceil(PIL_check_seconds_timer()))); // use better seed
		}
	
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
		{
			if(ec->ob->pd && (ec->ob->pd->forcefield == PFIELD_WIND))
				rng_free(ec->ob->pd->rng);
			
			*(ec->ob)= ec->obcopy;
		}

		BLI_freelistN(lb);
	}
}


/************************************************/
/*			Effectors		*/
/************************************************/

// triangle - ray callback function
static void eff_tri_ray_hit(void *userdata, int index, const BVHTreeRay *ray, BVHTreeRayHit *hit)
{	
	// whenever we hit a bounding box, we don't check further
	hit->dist = -1;
	hit->index = 1;
}

// get visibility of a wind ray
static float eff_calc_visibility(Object *ob, float *co, float *dir)
{
	CollisionModifierData **collobjs = NULL;
	int numcollobj = 0, i;
	float norm[3], len = 0.0;
	float visibility = 1.0;
	
	collobjs = get_collisionobjects(ob, &numcollobj);
	
	if(!collobjs)
		return 0;
	
	VECCOPY(norm, dir);
	VecMulf(norm, -1.0);
	len = Normalize(norm);
	
	// check all collision objects
	for(i = 0; i < numcollobj; i++)
	{
		CollisionModifierData *collmd = collobjs[i];
		
		if(collmd->bvhtree)
		{
			BVHTreeRayHit hit;
			
			hit.index = -1;
			hit.dist = len + FLT_EPSILON;
			
			// check if the way is blocked
			if(BLI_bvhtree_ray_cast(collmd->bvhtree, co, norm, &hit, eff_tri_ray_hit, NULL)>=0)
			{
				// visibility is only between 0 and 1, calculated from 1-absorption
				visibility *= MAX2(0.0, MIN2(1.0, (1.0-((float)collmd->absorption)*0.01)));
				
				if(visibility <= 0.0f)
					break;
			}
		}
	}
	
	MEM_freeN(collobjs);
	
	return visibility;
}

// noise function for wind e.g.
static float wind_func(struct RNG *rng, float strength)
{
	int random = (rng_getInt(rng)+1) % 65535; // max 2357
	float force = rng_getFloat(rng) + 1.0f;
	float ret;
	float sign = 0;
	
	sign = (random > 32000.0) ? 1.0: -1.0; // dividing by 2 is not giving equal sign distribution
	
	ret = sign*((float)random / force)*strength/65535.0f;
	
	return ret;
}


static float falloff_func(float fac, int usemin, float mindist, int usemax, float maxdist, float power)
{
	if(!usemin)
		mindist= 0.0f;

	if(fac < mindist) {
		return 1.0f;
	}
	else if(usemax) {
		if(fac>maxdist || (maxdist-mindist)<=0.0f)
			return 0.0f;

		fac= (fac-mindist)/(maxdist-mindist);
		return 1.0f - (float)pow((double)fac, (double)power);
	}
	else
		return pow((double)1.0f+fac-mindist, (double)-power);
}

static float falloff_func_dist(PartDeflect *pd, float fac)
{
	return falloff_func(fac, pd->flag&PFIELD_USEMIN, pd->mindist, pd->flag&PFIELD_USEMAX, pd->maxdist, pd->f_power);
}

static float falloff_func_rad(PartDeflect *pd, float fac)
{
	return falloff_func(fac, pd->flag&PFIELD_USEMINR, pd->minrad, pd->flag&PFIELD_USEMAXR, pd->maxrad, pd->f_power_r);
}

float effector_falloff(PartDeflect *pd, float *eff_velocity, float *vec_to_part)
{
	float eff_dir[3], temp[3];
	float falloff=1.0, fac, r_fac;
	
	VecCopyf(eff_dir,eff_velocity);
	Normalize(eff_dir);

	if(pd->flag & PFIELD_POSZ && Inpf(eff_dir,vec_to_part)<0.0f)
		falloff=0.0f;
	else switch(pd->falloff){
		case PFIELD_FALL_SPHERE:
			fac=VecLength(vec_to_part);
			falloff= falloff_func_dist(pd, fac);
			break;

		case PFIELD_FALL_TUBE:
			fac=Inpf(vec_to_part,eff_dir);
			falloff= falloff_func_dist(pd, ABS(fac));
			if(falloff == 0.0f)
				break;

			VECADDFAC(temp,vec_to_part,eff_dir,-fac);
			r_fac=VecLength(temp);
			falloff*= falloff_func_rad(pd, r_fac);
			break;
		case PFIELD_FALL_CONE:
			fac=Inpf(vec_to_part,eff_dir);
			falloff= falloff_func_dist(pd, ABS(fac));
			if(falloff == 0.0f)
				break;

			r_fac=saacos(fac/VecLength(vec_to_part))*180.0f/(float)M_PI;
			falloff*= falloff_func_rad(pd, r_fac);

			break;
	}

	return falloff;
}

void do_physical_effector(Object *ob, float *opco, short type, float force_val, float distance, float falloff, float size, float damp, float *eff_velocity, float *vec_to_part, float *velocity, float *field, int planar, struct RNG *rng, float noise_factor)
{
	float mag_vec[3]={0,0,0};
	float temp[3], temp2[3];
	float eff_vel[3];
	float noise = 0, visibility;
	
	// calculate visibility
	visibility = eff_calc_visibility(ob, opco, vec_to_part);
	if(visibility <= 0.0)
		return;
	falloff *= visibility;

	VecCopyf(eff_vel,eff_velocity);
	Normalize(eff_vel);

	switch(type){
		case PFIELD_WIND:
			VECCOPY(mag_vec,eff_vel);
			
			// add wind noise here, only if we have wind
			if((noise_factor > 0.0f) && (force_val > FLT_EPSILON))
				noise = wind_func(rng, noise_factor);
			
			VecMulf(mag_vec,(force_val+noise)*falloff);
			VecAddf(field,field,mag_vec);
			break;

		case PFIELD_FORCE:
			if(planar)
				Projf(mag_vec,vec_to_part,eff_vel);
			else
				VecCopyf(mag_vec,vec_to_part);

			VecMulf(mag_vec,force_val*falloff);
			VecAddf(field,field,mag_vec);
			break;

		case PFIELD_VORTEX:
			Crossf(mag_vec,eff_vel,vec_to_part);
			Normalize(mag_vec);

			VecMulf(mag_vec,force_val*distance*falloff);
			VecAddf(field,field,mag_vec);

			break;
		case PFIELD_MAGNET:
			if(planar)
				VecCopyf(temp,eff_vel);
			else
				/* magnetic field of a moving charge */
				Crossf(temp,eff_vel,vec_to_part);

			Crossf(temp2,velocity,temp);
			VecAddf(mag_vec,mag_vec,temp2);

			VecMulf(mag_vec,force_val*falloff);
			VecAddf(field,field,mag_vec);
			break;
		case PFIELD_HARMONIC:
			if(planar)
				Projf(mag_vec,vec_to_part,eff_vel);
			else
				VecCopyf(mag_vec,vec_to_part);

			VecMulf(mag_vec,force_val*falloff);
			VecSubf(field,field,mag_vec);

			VecCopyf(mag_vec,velocity);
			/* 1.9 is an experimental value to get critical damping at damp=1.0 */
			VecMulf(mag_vec,damp*1.9f*(float)sqrt(force_val));
			VecSubf(field,field,mag_vec);
			break;
		case PFIELD_NUCLEAR:
			/*pow here is root of cosine expression below*/
			//rad=(float)pow(2.0,-1.0/power)*distance/size;
			//VECCOPY(mag_vec,vec_to_part);
			//Normalize(mag_vec);
			//VecMulf(mag_vec,(float)cos(3.0*M_PI/2.0*(1.0-1.0/(pow(rad,power)+1.0)))/(rad+0.2f));
			//VECADDFAC(field,field,mag_vec,force_val);
			break;
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
	float *obloc;
	
	float distance, vec_to_part[3];
	float falloff;

	/* Cycle through collected objects, get total of (1/(gravity_strength * dist^gravity_power)) */
	/* Check for min distance here? (yes would be cool to add that, ton) */
	
	for(ec = lb->first; ec; ec= ec->next) {
		/* object effectors were fully checked to be OK to evaluate! */
		ob= ec->ob;
		pd= ob->pd;
			
		/* Get IPO force strength and fall off values here */
		where_is_object_time(ob,cur_time);
			
		/* use center of object for distance calculus */
		obloc= ob->obmat[3];
		VECSUB(vect_to_vert, obloc, opco);
		distance = VecLength(vect_to_vert);
		
		VecSubf(vec_to_part, opco, ob->obmat[3]);
		distance = VecLength(vec_to_part);

		falloff=effector_falloff(pd,ob->obmat[2],vec_to_part);		
		
		if(falloff<=0.0f)
			;	/* don't do anything */
		else {
			float field[3]={0,0,0}, tmp[3];
			VECCOPY(field, force);
			do_physical_effector(ob, opco, pd->forcefield,pd->f_strength,distance,
								falloff,pd->f_dist,pd->f_damp,ob->obmat[2],vec_to_part,
								speed,force,pd->flag&PFIELD_PLANAR, pd->rng, pd->f_noise);
			
			// for softbody backward compatibility
			if(flags & PE_WIND_AS_SPEED){
				VECSUB(tmp, force, field);
				VECSUB(speed, speed, tmp);
			}
		}
	}
}
