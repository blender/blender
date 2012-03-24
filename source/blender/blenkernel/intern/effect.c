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

/** \file blender/blenkernel/intern/effect.c
 *  \ingroup bke
 */


#include <stddef.h>

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
#include "DNA_particle_types.h"
#include "DNA_texture_types.h"
#include "DNA_scene_types.h"

#include "BLI_math.h"
#include "BLI_blenlib.h"
#include "BLI_jitter.h"
#include "BLI_rand.h"
#include "BLI_utildefines.h"

#include "PIL_time.h"

#include "BKE_action.h"
#include "BKE_anim.h"		/* needed for where_on_path */
#include "BKE_armature.h"
#include "BKE_blender.h"
#include "BKE_collision.h"
#include "BKE_constraint.h"
#include "BKE_deform.h"
#include "BKE_depsgraph.h"
#include "BKE_displist.h"
#include "BKE_DerivedMesh.h"
#include "BKE_cdderivedmesh.h"
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
#include "BKE_particle.h"
#include "BKE_scene.h"


#include "RE_render_ext.h"
#include "RE_shader_ext.h"

/* fluid sim particle import */
#ifdef WITH_MOD_FLUID
#include "DNA_object_fluidsim.h"
#include "LBM_fluidsim.h"
#include <zlib.h>
#include <string.h>
#endif // WITH_MOD_FLUID

//XXX #include "BIF_screen.h"

EffectorWeights *BKE_add_effector_weights(Group *group)
{
	EffectorWeights *weights = MEM_callocN(sizeof(EffectorWeights), "EffectorWeights");
	int i;

	for (i=0; i<NUM_PFIELD_TYPES; i++)
		weights->weight[i] = 1.0f;

	weights->global_gravity = 1.0f;

	weights->group = group;

	return weights;
}
PartDeflect *object_add_collision_fields(int type)
{
	PartDeflect *pd;

	pd= MEM_callocN(sizeof(PartDeflect), "PartDeflect");

	pd->forcefield = type;
	pd->pdef_sbdamp = 0.1f;
	pd->pdef_sbift  = 0.2f;
	pd->pdef_sboft  = 0.02f;
	pd->seed = ((unsigned int)(ceil(PIL_check_seconds_timer()))+1) % 128;
	pd->f_strength = 1.0f;
	pd->f_damp = 1.0f;

	/* set sensible defaults based on type */
	switch(type) {
		case PFIELD_VORTEX:
			pd->shape = PFIELD_SHAPE_PLANE;
			break;
		case PFIELD_WIND:
			pd->shape = PFIELD_SHAPE_PLANE;
			pd->f_flow = 1.0f; /* realistic wind behavior */
			break;
		case PFIELD_TEXTURE:
			pd->f_size = 1.0f;
			break;
	}
	pd->flag = PFIELD_DO_LOCATION|PFIELD_DO_ROTATION;

	return pd;
}

/* temporal struct, used for reading return of mesh_get_mapped_verts_nors() */

typedef struct VeNoCo {
	float co[3], no[3];
} VeNoCo;

/* ***************** PARTICLES ***************** */

/* -------------------------- Effectors ------------------ */
void free_partdeflect(PartDeflect *pd)
{
	if (!pd)
		return;

	if (pd->tex)
		pd->tex->id.us--;

	if (pd->rng)
		rng_free(pd->rng);

	MEM_freeN(pd);
}

static void precalculate_effector(EffectorCache *eff)
{
	unsigned int cfra = (unsigned int)(eff->scene->r.cfra >= 0 ? eff->scene->r.cfra : -eff->scene->r.cfra);
	if (!eff->pd->rng)
		eff->pd->rng = rng_new(eff->pd->seed + cfra);
	else
		rng_srandom(eff->pd->rng, eff->pd->seed + cfra);

	if (eff->pd->forcefield == PFIELD_GUIDE && eff->ob->type==OB_CURVE) {
		Curve *cu= eff->ob->data;
		if (cu->flag & CU_PATH) {
			if (cu->path==NULL || cu->path->data==NULL)
				makeDispListCurveTypes(eff->scene, eff->ob, 0);

			if (cu->path && cu->path->data) {
				where_on_path(eff->ob, 0.0, eff->guide_loc, eff->guide_dir, NULL, &eff->guide_radius, NULL);
				mul_m4_v3(eff->ob->obmat, eff->guide_loc);
				mul_mat3_m4_v3(eff->ob->obmat, eff->guide_dir);
			}
		}
	}
	else if (eff->pd->shape == PFIELD_SHAPE_SURFACE) {
		eff->surmd = (SurfaceModifierData *)modifiers_findByType ( eff->ob, eModifierType_Surface );
		if (eff->ob->type == OB_CURVE)
			eff->flag |= PE_USE_NORMAL_DATA;
	}
	else if (eff->psys)
		psys_update_particle_tree(eff->psys, eff->scene->r.cfra);

	/* Store object velocity */
	if (eff->ob) {
		float old_vel[3];

		where_is_object_time(eff->scene, eff->ob, cfra - 1.0f);
		copy_v3_v3(old_vel, eff->ob->obmat[3]);	
		where_is_object_time(eff->scene, eff->ob, cfra);
		sub_v3_v3v3(eff->velocity, eff->ob->obmat[3], old_vel);
	}
}
static EffectorCache *new_effector_cache(Scene *scene, Object *ob, ParticleSystem *psys, PartDeflect *pd)
{
	EffectorCache *eff = MEM_callocN(sizeof(EffectorCache), "EffectorCache");
	eff->scene = scene;
	eff->ob = ob;
	eff->psys = psys;
	eff->pd = pd;
	eff->frame = -1;

	precalculate_effector(eff);

	return eff;
}
static void add_object_to_effectors(ListBase **effectors, Scene *scene, EffectorWeights *weights, Object *ob, Object *ob_src)
{
	EffectorCache *eff = NULL;

	if ( ob == ob_src || weights->weight[ob->pd->forcefield] == 0.0f )
		return;

	if (ob->pd->shape == PFIELD_SHAPE_POINTS && !ob->derivedFinal )
		return;

	if (*effectors == NULL)
		*effectors = MEM_callocN(sizeof(ListBase), "effectors list");

	eff = new_effector_cache(scene, ob, NULL, ob->pd);

	/* make sure imat is up to date */
	invert_m4_m4(ob->imat, ob->obmat);

	BLI_addtail(*effectors, eff);
}
static void add_particles_to_effectors(ListBase **effectors, Scene *scene, EffectorWeights *weights, Object *ob, ParticleSystem *psys, ParticleSystem *psys_src)
{
	ParticleSettings *part= psys->part;

	if ( !psys_check_enabled(ob, psys) )
		return;

	if ( psys == psys_src && (part->flag & PART_SELF_EFFECT) == 0)
		return;

	if ( part->pd && part->pd->forcefield && weights->weight[part->pd->forcefield] != 0.0f) {
		if (*effectors == NULL)
			*effectors = MEM_callocN(sizeof(ListBase), "effectors list");

		BLI_addtail(*effectors, new_effector_cache(scene, ob, psys, part->pd));
	}

	if (part->pd2 && part->pd2->forcefield && weights->weight[part->pd2->forcefield] != 0.0f) {
		if (*effectors == NULL)
			*effectors = MEM_callocN(sizeof(ListBase), "effectors list");

		BLI_addtail(*effectors, new_effector_cache(scene, ob, psys, part->pd2));
	}
}

/* returns ListBase handle with objects taking part in the effecting */
ListBase *pdInitEffectors(Scene *scene, Object *ob_src, ParticleSystem *psys_src, EffectorWeights *weights)
{
	Base *base;
	unsigned int layer= ob_src->lay;
	ListBase *effectors = NULL;
	
	if (weights->group) {
		GroupObject *go;
		
		for (go= weights->group->gobject.first; go; go= go->next) {
			if ( (go->ob->lay & layer) ) {
				if ( go->ob->pd && go->ob->pd->forcefield )
					add_object_to_effectors(&effectors, scene, weights, go->ob, ob_src);

				if ( go->ob->particlesystem.first ) {
					ParticleSystem *psys= go->ob->particlesystem.first;

					for ( ; psys; psys=psys->next )
						add_particles_to_effectors(&effectors, scene, weights, go->ob, psys, psys_src);
				}
			}
		}
	}
	else {
		for (base = scene->base.first; base; base= base->next) {
			if ( (base->lay & layer) ) {
				if ( base->object->pd && base->object->pd->forcefield )
				add_object_to_effectors(&effectors, scene, weights, base->object, ob_src);

				if ( base->object->particlesystem.first ) {
					ParticleSystem *psys= base->object->particlesystem.first;

					for ( ; psys; psys=psys->next )
						add_particles_to_effectors(&effectors, scene, weights, base->object, psys, psys_src);
				}
			}
		}
	}
	return effectors;
}

void pdEndEffectors(ListBase **effectors)
{
	if (*effectors) {
		EffectorCache *eff = (*effectors)->first;

		for (; eff; eff=eff->next) {
			if (eff->guide_data)
				MEM_freeN(eff->guide_data);
		}

		BLI_freelistN(*effectors);
		MEM_freeN(*effectors);
		*effectors = NULL;
	}
}


void pd_point_from_particle(ParticleSimulationData *sim, ParticleData *pa, ParticleKey *state, EffectedPoint *point)
{
	ParticleSettings *part = sim->psys->part;
	point->loc = state->co;
	point->vel = state->vel;
	point->index = pa - sim->psys->particles;
	point->size = pa->size;
	point->charge = 0.0f;
	
	if (part->pd && part->pd->forcefield == PFIELD_CHARGE)
		point->charge += part->pd->f_strength;

	if (part->pd2 && part->pd2->forcefield == PFIELD_CHARGE)
		point->charge += part->pd2->f_strength;

	point->vel_to_sec = 1.0f;
	point->vel_to_frame = psys_get_timestep(sim);

	point->flag = 0;

	if (sim->psys->part->flag & PART_ROT_DYN) {
		point->ave = state->ave;
		point->rot = state->rot;
	}
	else
		point->ave = point->rot = NULL;

	point->psys = sim->psys;
}

void pd_point_from_loc(Scene *scene, float *loc, float *vel, int index, EffectedPoint *point)
{
	point->loc = loc;
	point->vel = vel;
	point->index = index;
	point->size = 0.0f;

	point->vel_to_sec = (float)scene->r.frs_sec;
	point->vel_to_frame = 1.0f;

	point->flag = 0;

	point->ave = point->rot = NULL;
	point->psys = NULL;
}
void pd_point_from_soft(Scene *scene, float *loc, float *vel, int index, EffectedPoint *point)
{
	point->loc = loc;
	point->vel = vel;
	point->index = index;
	point->size = 0.0f;

	point->vel_to_sec = (float)scene->r.frs_sec;
	point->vel_to_frame = 1.0f;

	point->flag = PE_WIND_AS_SPEED;

	point->ave = point->rot = NULL;

	point->psys = NULL;
}
/************************************************/
/*			Effectors		*/
/************************************************/

// triangle - ray callback function
static void eff_tri_ray_hit(void *UNUSED(userData), int UNUSED(index), const BVHTreeRay *UNUSED(ray), BVHTreeRayHit *hit)
{	
	// whenever we hit a bounding box, we don't check further
	hit->dist = -1;
	hit->index = 1;
}

// get visibility of a wind ray
static float eff_calc_visibility(ListBase *colliders, EffectorCache *eff, EffectorData *efd, EffectedPoint *point)
{
	ListBase *colls = colliders;
	ColliderCache *col;
	float norm[3], len = 0.0;
	float visibility = 1.0, absorption = 0.0;
	
	if (!(eff->pd->flag & PFIELD_VISIBILITY))
		return visibility;

	if (!colls)
		colls = get_collider_cache(eff->scene, eff->ob, NULL);

	if (!colls)
		return visibility;

	negate_v3_v3(norm, efd->vec_to_point);
	len = normalize_v3(norm);
	
	// check all collision objects
	for (col = colls->first; col; col = col->next)
	{
		CollisionModifierData *collmd = col->collmd;

		if (col->ob == eff->ob)
			continue;
		
		if (collmd->bvhtree) {
			BVHTreeRayHit hit;
			
			hit.index = -1;
			hit.dist = len + FLT_EPSILON;
			
			// check if the way is blocked
			if (BLI_bvhtree_ray_cast(collmd->bvhtree, point->loc, norm, 0.0f, &hit, eff_tri_ray_hit, NULL)>=0) {
				absorption= col->ob->pd->absorption;

				// visibility is only between 0 and 1, calculated from 1-absorption
				visibility *= CLAMPIS(1.0f-absorption, 0.0f, 1.0f);
				
				if (visibility <= 0.0f)
					break;
			}
		}
	}

	if (!colliders)
		free_collider_cache(&colls);
	
	return visibility;
}

// noise function for wind e.g.
static float wind_func(struct RNG *rng, float strength)
{
	int random = (rng_getInt(rng)+1) % 128; // max 2357
	float force = rng_getFloat(rng) + 1.0f;
	float ret;
	float sign = 0;
	
	sign = ((float)random > 64.0f) ? 1.0f: -1.0f; // dividing by 2 is not giving equal sign distribution
	
	ret = sign*((float)random / force)*strength/128.0f;
	
	return ret;
}

/* maxdist: zero effect from this distance outwards (if usemax) */
/* mindist: full effect up to this distance (if usemin) */
/* power: falloff with formula 1/r^power */
static float falloff_func(float fac, int usemin, float mindist, int usemax, float maxdist, float power)
{
	/* first quick checks */
	if (usemax && fac > maxdist)
		return 0.0f;

	if (usemin && fac < mindist)
		return 1.0f;

	if (!usemin)
		mindist = 0.0;

	return pow((double)(1.0f+fac-mindist), (double)(-power));
}

static float falloff_func_dist(PartDeflect *pd, float fac)
{
	return falloff_func(fac, pd->flag&PFIELD_USEMIN, pd->mindist, pd->flag&PFIELD_USEMAX, pd->maxdist, pd->f_power);
}

static float falloff_func_rad(PartDeflect *pd, float fac)
{
	return falloff_func(fac, pd->flag&PFIELD_USEMINR, pd->minrad, pd->flag&PFIELD_USEMAXR, pd->maxrad, pd->f_power_r);
}

float effector_falloff(EffectorCache *eff, EffectorData *efd, EffectedPoint *UNUSED(point), EffectorWeights *weights)
{
	float temp[3];
	float falloff = weights ? weights->weight[0] * weights->weight[eff->pd->forcefield] : 1.0f;
	float fac, r_fac;

	fac = dot_v3v3(efd->nor, efd->vec_to_point2);

	if (eff->pd->zdir == PFIELD_Z_POS && fac < 0.0f)
		falloff=0.0f;
	else if (eff->pd->zdir == PFIELD_Z_NEG && fac > 0.0f)
		falloff=0.0f;
	else switch(eff->pd->falloff) {
		case PFIELD_FALL_SPHERE:
			falloff*= falloff_func_dist(eff->pd, efd->distance);
			break;

		case PFIELD_FALL_TUBE:
			falloff*= falloff_func_dist(eff->pd, ABS(fac));
			if (falloff == 0.0f)
				break;

			madd_v3_v3v3fl(temp, efd->vec_to_point, efd->nor, -fac);
			r_fac= len_v3(temp);
			falloff*= falloff_func_rad(eff->pd, r_fac);
			break;
		case PFIELD_FALL_CONE:
			falloff*= falloff_func_dist(eff->pd, ABS(fac));
			if (falloff == 0.0f)
				break;

			r_fac= RAD2DEGF(saacos(fac/len_v3(efd->vec_to_point)));
			falloff*= falloff_func_rad(eff->pd, r_fac);

			break;
	}

	return falloff;
}

int closest_point_on_surface(SurfaceModifierData *surmd, const float co[3], float surface_co[3], float surface_nor[3], float surface_vel[3])
{
	BVHTreeNearest nearest;

	nearest.index = -1;
	nearest.dist = FLT_MAX;

	BLI_bvhtree_find_nearest(surmd->bvhtree->tree, co, &nearest, surmd->bvhtree->nearest_callback, surmd->bvhtree);

	if (nearest.index != -1) {
		copy_v3_v3(surface_co, nearest.co);

		if (surface_nor) {
			copy_v3_v3(surface_nor, nearest.no);
		}

		if (surface_vel) {
			MFace *mface = CDDM_get_tessface(surmd->dm, nearest.index);
			
			copy_v3_v3(surface_vel, surmd->v[mface->v1].co);
			add_v3_v3(surface_vel, surmd->v[mface->v2].co);
			add_v3_v3(surface_vel, surmd->v[mface->v3].co);
			if (mface->v4)
				add_v3_v3(surface_vel, surmd->v[mface->v4].co);

			mul_v3_fl(surface_vel, mface->v4 ? 0.25f : 0.333f);
		}
		return 1;
	}

	return 0;
}
int get_effector_data(EffectorCache *eff, EffectorData *efd, EffectedPoint *point, int real_velocity)
{
	float cfra = eff->scene->r.cfra;
	int ret = 0;

	if (eff->pd && eff->pd->shape==PFIELD_SHAPE_SURFACE && eff->surmd) {
		/* closest point in the object surface is an effector */
		float vec[3];

		/* using velocity corrected location allows for easier sliding over effector surface */
		copy_v3_v3(vec, point->vel);
		mul_v3_fl(vec, point->vel_to_frame);
		add_v3_v3(vec, point->loc);

		ret = closest_point_on_surface(eff->surmd, vec, efd->loc, efd->nor, real_velocity ? efd->vel : NULL);

		efd->size = 0.0f;
	}
	else if (eff->pd && eff->pd->shape==PFIELD_SHAPE_POINTS) {

		if (eff->ob->derivedFinal) {
			DerivedMesh *dm = eff->ob->derivedFinal;

			dm->getVertCo(dm, *efd->index, efd->loc);
			dm->getVertNo(dm, *efd->index, efd->nor);

			mul_m4_v3(eff->ob->obmat, efd->loc);
			mul_mat3_m4_v3(eff->ob->obmat, efd->nor);

			normalize_v3(efd->nor);

			efd->size = 0.0f;

			/**/
			ret = 1;
		}
	}
	else if (eff->psys) {
		ParticleData *pa = eff->psys->particles + *efd->index;
		ParticleKey state;

		/* exclude the particle itself for self effecting particles */
		if (eff->psys == point->psys && *efd->index == point->index) {
			/* pass */
		}
		else {
			ParticleSimulationData sim= {NULL};
			sim.scene= eff->scene;
			sim.ob= eff->ob;
			sim.psys= eff->psys;

			/* TODO: time from actual previous calculated frame (step might not be 1) */
			state.time = cfra - 1.0f;
			ret = psys_get_particle_state(&sim, *efd->index, &state, 0);

			/* TODO */
			//if(eff->pd->forcefiled == PFIELD_HARMONIC && ret==0) {
			//	if (pa->dietime < eff->psys->cfra)
			//		eff->flag |= PE_VELOCITY_TO_IMPULSE;
			//}

			copy_v3_v3(efd->loc, state.co);

			/* rather than use the velocity use rotated x-axis (defaults to velocity) */
			efd->nor[0] = 1.f;
			efd->nor[1] = efd->nor[2] = 0.f;
			mul_qt_v3(state.rot, efd->nor);
		
			if (real_velocity)
				copy_v3_v3(efd->vel, state.vel);

			efd->size = pa->size;
		}
	}
	else {
		/* use center of object for distance calculus */
		Object *ob = eff->ob;
		Object obcopy = *ob;

		/* use z-axis as normal*/
		normalize_v3_v3(efd->nor, ob->obmat[2]);

		if (eff->pd && eff->pd->shape == PFIELD_SHAPE_PLANE) {
			float temp[3], translate[3];
			sub_v3_v3v3(temp, point->loc, ob->obmat[3]);
			project_v3_v3v3(translate, temp, efd->nor);

			/* for vortex the shape chooses between old / new force */
			if (eff->pd->forcefield == PFIELD_VORTEX)
				add_v3_v3v3(efd->loc, ob->obmat[3], translate);
			else /* normally efd->loc is closest point on effector xy-plane */
				sub_v3_v3v3(efd->loc, point->loc, translate);
		}
		else {
			copy_v3_v3(efd->loc, ob->obmat[3]);
		}

		if (real_velocity)
			copy_v3_v3(efd->vel, eff->velocity);

		*eff->ob = obcopy;

		efd->size = 0.0f;

		ret = 1;
	}

	if (ret) {
		sub_v3_v3v3(efd->vec_to_point, point->loc, efd->loc);
		efd->distance = len_v3(efd->vec_to_point);

		/* rest length for harmonic effector, will have to see later if this could be extended to other effectors */
		if (eff->pd && eff->pd->forcefield == PFIELD_HARMONIC && eff->pd->f_size)
			mul_v3_fl(efd->vec_to_point, (efd->distance-eff->pd->f_size)/efd->distance);

		if (eff->flag & PE_USE_NORMAL_DATA) {
			copy_v3_v3(efd->vec_to_point2, efd->vec_to_point);
			copy_v3_v3(efd->nor2, efd->nor);
		}
		else {
			/* for some effectors we need the object center every time */
			sub_v3_v3v3(efd->vec_to_point2, point->loc, eff->ob->obmat[3]);
			normalize_v3_v3(efd->nor2, eff->ob->obmat[2]);
		}
	}

	return ret;
}
static void get_effector_tot(EffectorCache *eff, EffectorData *efd, EffectedPoint *point, int *tot, int *p, int *step)
{
	if (eff->pd->shape == PFIELD_SHAPE_POINTS) {
		efd->index = p;

		*p = 0;
		*tot = eff->ob->derivedFinal ? eff->ob->derivedFinal->numVertData : 1;

		if (*tot && eff->pd->forcefield == PFIELD_HARMONIC && point->index >= 0) {
			*p = point->index % *tot;
			*tot = *p+1;
		}
	}
	else if (eff->psys) {
		efd->index = p;

		*p = 0;
		*tot = eff->psys->totpart;
		
		if (eff->pd->forcefield == PFIELD_CHARGE) {
			/* Only the charge of the effected particle is used for 
			 * interaction, not fall-offs. If the fall-offs aren't the	
			 * same this will be unphysical, but for animation this		
			 * could be the wanted behavior. If you want physical
			 * correctness the fall-off should be spherical 2.0 anyways.
			 */
			efd->charge = eff->pd->f_strength;
		}
		else if (eff->pd->forcefield == PFIELD_HARMONIC && (eff->pd->flag & PFIELD_MULTIPLE_SPRINGS)==0) {
			/* every particle is mapped to only one harmonic effector particle */
			*p= point->index % eff->psys->totpart;
			*tot= *p + 1;
		}

		if (eff->psys->part->effector_amount) {
			int totpart = eff->psys->totpart;
			int amount = eff->psys->part->effector_amount;

			*step = (totpart > amount) ? totpart/amount : 1;
		}
	}
	else {
		*p = 0;
		*tot = 1;
	}
}
static void do_texture_effector(EffectorCache *eff, EffectorData *efd, EffectedPoint *point, float *total_force)
{
	TexResult result[4];
	float tex_co[3], strength, force[3];
	float nabla = eff->pd->tex_nabla;
	int hasrgb;
	short mode = eff->pd->tex_mode;

	if (!eff->pd->tex)
		return;

	result[0].nor = result[1].nor = result[2].nor = result[3].nor = NULL;

	strength= eff->pd->f_strength * efd->falloff;

	copy_v3_v3(tex_co,point->loc);

	if (eff->pd->flag & PFIELD_TEX_2D) {
		float fac=-dot_v3v3(tex_co, efd->nor);
		madd_v3_v3fl(tex_co, efd->nor, fac);
	}

	if (eff->pd->flag & PFIELD_TEX_OBJECT) {
		mul_m4_v3(eff->ob->imat, tex_co);
	}

	hasrgb = multitex_ext(eff->pd->tex, tex_co, NULL,NULL, 0, result);

	if (hasrgb && mode==PFIELD_TEX_RGB) {
		force[0] = (0.5f - result->tr) * strength;
		force[1] = (0.5f - result->tg) * strength;
		force[2] = (0.5f - result->tb) * strength;
	}
	else {
		strength/=nabla;

		tex_co[0] += nabla;
		multitex_ext(eff->pd->tex, tex_co, NULL, NULL, 0, result+1);

		tex_co[0] -= nabla;
		tex_co[1] += nabla;
		multitex_ext(eff->pd->tex, tex_co, NULL, NULL, 0, result+2);

		tex_co[1] -= nabla;
		tex_co[2] += nabla;
		multitex_ext(eff->pd->tex, tex_co, NULL, NULL, 0, result+3);

		if (mode == PFIELD_TEX_GRAD || !hasrgb) { /* if we don't have rgb fall back to grad */
			force[0] = (result[0].tin - result[1].tin) * strength;
			force[1] = (result[0].tin - result[2].tin) * strength;
			force[2] = (result[0].tin - result[3].tin) * strength;
		}
		else { /*PFIELD_TEX_CURL*/
			float dbdy, dgdz, drdz, dbdx, dgdx, drdy;

			dbdy = result[2].tb - result[0].tb;
			dgdz = result[3].tg - result[0].tg;
			drdz = result[3].tr - result[0].tr;
			dbdx = result[1].tb - result[0].tb;
			dgdx = result[1].tg - result[0].tg;
			drdy = result[2].tr - result[0].tr;

			force[0] = (dbdy - dgdz) * strength;
			force[1] = (drdz - dbdx) * strength;
			force[2] = (dgdx - drdy) * strength;
		}
	}

	if (eff->pd->flag & PFIELD_TEX_2D) {
		float fac = -dot_v3v3(force, efd->nor);
		madd_v3_v3fl(force, efd->nor, fac);
	}

	add_v3_v3(total_force, force);
}
static void do_physical_effector(EffectorCache *eff, EffectorData *efd, EffectedPoint *point, float *total_force)
{
	PartDeflect *pd = eff->pd;
	RNG *rng = pd->rng;
	float force[3]={0,0,0};
	float temp[3];
	float fac;
	float strength = pd->f_strength;
	float damp = pd->f_damp;
	float noise_factor = pd->f_noise;

	if (noise_factor > 0.0f) {
		strength += wind_func(rng, noise_factor);

		if (ELEM(pd->forcefield, PFIELD_HARMONIC, PFIELD_DRAG))
			damp += wind_func(rng, noise_factor);
	}

	copy_v3_v3(force, efd->vec_to_point);

	switch(pd->forcefield) {
		case PFIELD_WIND:
			copy_v3_v3(force, efd->nor);
			mul_v3_fl(force, strength * efd->falloff);
			break;
		case PFIELD_FORCE:
			normalize_v3(force);
			mul_v3_fl(force, strength * efd->falloff);
			break;
		case PFIELD_VORTEX:
			/* old vortex force */
			if (pd->shape == PFIELD_SHAPE_POINT) {
				cross_v3_v3v3(force, efd->nor, efd->vec_to_point);
				normalize_v3(force);
				mul_v3_fl(force, strength * efd->distance * efd->falloff);
			}
			else {
				/* new vortex force */
				cross_v3_v3v3(temp, efd->nor2, efd->vec_to_point2);
				mul_v3_fl(temp, strength * efd->falloff);
				
				cross_v3_v3v3(force, efd->nor2, temp);
				mul_v3_fl(force, strength * efd->falloff);
				
				madd_v3_v3fl(temp, point->vel, -point->vel_to_sec);
				add_v3_v3(force, temp);
			}
			break;
		case PFIELD_MAGNET:
			if (eff->pd->shape == PFIELD_SHAPE_POINT)
				/* magnetic field of a moving charge */
				cross_v3_v3v3(temp, efd->nor, efd->vec_to_point);
			else
				copy_v3_v3(temp, efd->nor);

			normalize_v3(temp);
			mul_v3_fl(temp, strength * efd->falloff);
			cross_v3_v3v3(force, point->vel, temp);
			mul_v3_fl(force, point->vel_to_sec);
			break;
		case PFIELD_HARMONIC:
			mul_v3_fl(force, -strength * efd->falloff);
			copy_v3_v3(temp, point->vel);
			mul_v3_fl(temp, -damp * 2.0f * (float)sqrt(fabs(strength)) * point->vel_to_sec);
			add_v3_v3(force, temp);
			break;
		case PFIELD_CHARGE:
			mul_v3_fl(force, point->charge * strength * efd->falloff);
			break;
		case PFIELD_LENNARDJ:
			fac = pow((efd->size + point->size) / efd->distance, 6.0);
			
			fac = - fac * (1.0f - fac) / efd->distance;

			/* limit the repulsive term drastically to avoid huge forces */
			fac = ((fac>2.0f) ? 2.0f : fac);

			mul_v3_fl(force, strength * fac);
			break;
		case PFIELD_BOID:
			/* Boid field is handled completely in boids code. */
			return;
		case PFIELD_TURBULENCE:
			if (pd->flag & PFIELD_GLOBAL_CO) {
				copy_v3_v3(temp, point->loc);
			}
			else {
				add_v3_v3v3(temp, efd->vec_to_point2, efd->nor2);
			}
			force[0] = -1.0f + 2.0f * BLI_gTurbulence(pd->f_size, temp[0], temp[1], temp[2], 2,0,2);
			force[1] = -1.0f + 2.0f * BLI_gTurbulence(pd->f_size, temp[1], temp[2], temp[0], 2,0,2);
			force[2] = -1.0f + 2.0f * BLI_gTurbulence(pd->f_size, temp[2], temp[0], temp[1], 2,0,2);
			mul_v3_fl(force, strength * efd->falloff);
			break;
		case PFIELD_DRAG:
			copy_v3_v3(force, point->vel);
			fac = normalize_v3(force) * point->vel_to_sec;

			strength = MIN2(strength, 2.0f);
			damp = MIN2(damp, 2.0f);

			mul_v3_fl(force, -efd->falloff * fac * (strength * fac + damp));
			break;
	}

	if (pd->flag & PFIELD_DO_LOCATION) {
		madd_v3_v3fl(total_force, force, 1.0f/point->vel_to_sec);

		if (ELEM(pd->forcefield, PFIELD_HARMONIC, PFIELD_DRAG)==0 && pd->f_flow != 0.0f) {
			madd_v3_v3fl(total_force, point->vel, -pd->f_flow * efd->falloff);
		}
	}

	if (pd->flag & PFIELD_DO_ROTATION && point->ave && point->rot) {
		float xvec[3] = {1.0f, 0.0f, 0.0f};
		float dave[3];
		mul_qt_v3(point->rot, xvec);
		cross_v3_v3v3(dave, xvec, force);
		if (pd->f_flow != 0.0f) {
			madd_v3_v3fl(dave, point->ave, -pd->f_flow * efd->falloff);
		}
		add_v3_v3(point->ave, dave);
	}
}

/*  -------- pdDoEffectors() --------
 * generic force/speed system, now used for particles and softbodies
 * scene       = scene where it runs in, for time and stuff
 * lb			= listbase with objects that take part in effecting
 * opco		= global coord, as input
 * force		= force accumulator
 * speed		= actual current speed which can be altered
 * cur_time	= "external" time in frames, is constant for static particles
 * loc_time	= "local" time in frames, range <0-1> for the lifetime of particle
 * par_layer	= layer the caller is in
 * flags		= only used for softbody wind now
 * guide		= old speed of particle
 */
void pdDoEffectors(ListBase *effectors, ListBase *colliders, EffectorWeights *weights, EffectedPoint *point, float *force, float *impulse)
{
/*
 * Modifies the force on a particle according to its
 * relation with the effector object
 * Different kind of effectors include:
 *     Forcefields: Gravity-like attractor
 *     (force power is related to the inverse of distance to the power of a falloff value)
 *     Vortex fields: swirling effectors
 *     (particles rotate around Z-axis of the object. otherwise, same relation as)
 *     (Forcefields, but this is not done through a force/acceleration)
 *     Guide: particles on a path
 *     (particles are guided along a curve bezier or old nurbs)
 *     (is independent of other effectors)
 */
	EffectorCache *eff;
	EffectorData efd;
	int p=0, tot = 1, step = 1;

	/* Cycle through collected objects, get total of (1/(gravity_strength * dist^gravity_power)) */
	/* Check for min distance here? (yes would be cool to add that, ton) */
	
	if (effectors) for(eff = effectors->first; eff; eff=eff->next) {
		/* object effectors were fully checked to be OK to evaluate! */

		get_effector_tot(eff, &efd, point, &tot, &p, &step);

		for (; p<tot; p+=step) {
			if (get_effector_data(eff, &efd, point, 0)) {
				efd.falloff= effector_falloff(eff, &efd, point, weights);
				
				if (efd.falloff > 0.0f)
					efd.falloff *= eff_calc_visibility(colliders, eff, &efd, point);

				if (efd.falloff <= 0.0f)
					;	/* don't do anything */
				else if (eff->pd->forcefield == PFIELD_TEXTURE)
					do_texture_effector(eff, &efd, point, force);
				else {
					float temp1[3]={0,0,0}, temp2[3];
					copy_v3_v3(temp1, force);

					do_physical_effector(eff, &efd, point, force);
					
					// for softbody backward compatibility
					if (point->flag & PE_WIND_AS_SPEED && impulse) {
						sub_v3_v3v3(temp2, force, temp1);
						sub_v3_v3v3(impulse, impulse, temp2);
					}
				}
			}
			else if (eff->flag & PE_VELOCITY_TO_IMPULSE && impulse) {
				/* special case for harmonic effector */
				add_v3_v3v3(impulse, impulse, efd.vel);
			}
		}
	}
}
