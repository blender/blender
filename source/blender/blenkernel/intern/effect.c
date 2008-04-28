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
		else if((pd->flag & PFIELD_USEMIN) && distance<pd->mindist && pd->forcefield != PFIELD_GUIDE)
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


/* for paf start to end, store all matrices for objects */
typedef struct pMatrixCache {
	float obmat[4][4];
	float imat[3][3];
} pMatrixCache;

/* for fluidsim win32 debug messages */
#if defined(WIN32) && (!(defined snprintf))
#define snprintf _snprintf
#endif
