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
 * The Original Code is Copyright (C) 2007 by Janne Karhu.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Raul Fernandez Hernandez (Farsthary), Stephen Swhitehorn.
 *
 * Adaptive time step
 * Classical SPH
 * Copyright 2011-2012 AutoCRC
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/blenkernel/intern/particle_system.c
 *  \ingroup bke
 */


#include <stddef.h>

#include <stdlib.h>
#include <math.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "DNA_anim_types.h"
#include "DNA_boid_types.h"
#include "DNA_particle_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_force_types.h"
#include "DNA_object_types.h"
#include "DNA_curve_types.h"
#include "DNA_scene_types.h"
#include "DNA_texture_types.h"
#include "DNA_listBase.h"

#include "BLI_utildefines.h"
#include "BLI_edgehash.h"
#include "BLI_rand.h"
#include "BLI_jitter_2d.h"
#include "BLI_math.h"
#include "BLI_blenlib.h"
#include "BLI_kdtree.h"
#include "BLI_kdopbvh.h"
#include "BLI_sort.h"
#include "BLI_task.h"
#include "BLI_threads.h"
#include "BLI_linklist.h"

#include "BKE_animsys.h"
#include "BKE_boids.h"
#include "BKE_cdderivedmesh.h"
#include "BKE_collision.h"
#include "BKE_colortools.h"
#include "BKE_effect.h"
#include "BKE_library_query.h"
#include "BKE_main.h"
#include "BKE_particle.h"

#include "BKE_DerivedMesh.h"
#include "BKE_object.h"
#include "BKE_material.h"
#include "BKE_cloth.h"
#include "BKE_lattice.h"
#include "BKE_pointcache.h"
#include "BKE_mesh.h"
#include "BKE_modifier.h"
#include "BKE_scene.h"
#include "BKE_bvhutils.h"
#include "BKE_depsgraph.h"

#include "PIL_time.h"

#include "RE_shader_ext.h"
#include "DEG_depsgraph.h"

/* fluid sim particle import */
#ifdef WITH_MOD_FLUID
#include "DNA_object_fluidsim_types.h"
#include "LBM_fluidsim.h"
#include <zlib.h>
#include <string.h>

#endif // WITH_MOD_FLUID

static ThreadRWMutex psys_bvhtree_rwlock = BLI_RWLOCK_INITIALIZER;

/************************************************/
/*			Reacting to system events			*/
/************************************************/

static int particles_are_dynamic(ParticleSystem *psys)
{
	if (psys->pointcache->flag & PTCACHE_BAKED)
		return 0;

	if (psys->part->type == PART_HAIR)
		return psys->flag & PSYS_HAIR_DYNAMICS;
	else
		return ELEM(psys->part->phystype, PART_PHYS_NEWTON, PART_PHYS_BOIDS, PART_PHYS_FLUID);
}

float psys_get_current_display_percentage(ParticleSystem *psys)
{
	ParticleSettings *part=psys->part;

	if ((psys->renderdata && !particles_are_dynamic(psys)) ||  /* non-dynamic particles can be rendered fully */
	    (part->child_nbr && part->childtype)  ||    /* display percentage applies to children */
	    (psys->pointcache->flag & PTCACHE_BAKING))  /* baking is always done with full amount */
	{
		return 1.0f;
	}

	return psys->part->disp/100.0f;
}

static int tot_particles(ParticleSystem *psys, PTCacheID *pid)
{
	if (pid && psys->pointcache->flag & PTCACHE_EXTERNAL)
		return pid->cache->totpoint;
	else if (psys->part->distr == PART_DISTR_GRID && psys->part->from != PART_FROM_VERT)
		return psys->part->grid_res * psys->part->grid_res * psys->part->grid_res - psys->totunexist;
	else
		return psys->part->totpart - psys->totunexist;
}

void psys_reset(ParticleSystem *psys, int mode)
{
	PARTICLE_P;

	if (ELEM(mode, PSYS_RESET_ALL, PSYS_RESET_DEPSGRAPH)) {
		if (mode == PSYS_RESET_ALL || !(psys->flag & PSYS_EDITED)) {
			/* don't free if not absolutely necessary */
			if (psys->totpart != tot_particles(psys, NULL)) {
				psys_free_particles(psys);
				psys->totpart= 0;
			}

			psys->totkeyed= 0;
			psys->flag &= ~(PSYS_HAIR_DONE|PSYS_KEYED);

			if (psys->edit && psys->free_edit) {
				psys->free_edit(psys->edit);
				psys->edit = NULL;
				psys->free_edit = NULL;
			}
		}
	}
	else if (mode == PSYS_RESET_CACHE_MISS) {
		/* set all particles to be skipped */
		LOOP_PARTICLES {
			pa->flag |= PARS_NO_DISP;
		}
	}

	/* reset children */
	if (psys->child) {
		MEM_freeN(psys->child);
		psys->child= NULL;
	}

	psys->totchild= 0;

	/* reset path cache */
	psys_free_path_cache(psys, psys->edit);

	/* reset point cache */
	BKE_ptcache_invalidate(psys->pointcache);

	if (psys->fluid_springs) {
		MEM_freeN(psys->fluid_springs);
		psys->fluid_springs = NULL;
	}

	psys->tot_fluidsprings = psys->alloc_fluidsprings = 0;
}

static void realloc_particles(ParticleSimulationData *sim, int new_totpart)
{
	ParticleSystem *psys = sim->psys;
	ParticleSettings *part = psys->part;
	ParticleData *newpars = NULL;
	BoidParticle *newboids = NULL;
	PARTICLE_P;
	int totpart, totsaved = 0;

	if (new_totpart<0) {
		if ((part->distr == PART_DISTR_GRID) && (part->from != PART_FROM_VERT)) {
			totpart= part->grid_res;
			totpart*=totpart*totpart;
		}
		else
			totpart=part->totpart;
	}
	else
		totpart=new_totpart;

	if (totpart != psys->totpart) {
		if (psys->edit && psys->free_edit) {
			psys->free_edit(psys->edit);
			psys->edit = NULL;
			psys->free_edit = NULL;
		}

		if (totpart) {
			newpars= MEM_callocN(totpart*sizeof(ParticleData), "particles");
			if (newpars == NULL)
				return;

			if (psys->part->phystype == PART_PHYS_BOIDS) {
				newboids= MEM_callocN(totpart*sizeof(BoidParticle), "boid particles");

				if (newboids == NULL) {
					/* allocation error! */
					if (newpars)
						MEM_freeN(newpars);
					return;
				}
			}
		}

		if (psys->particles) {
			totsaved=MIN2(psys->totpart,totpart);
			/*save old pars*/
			if (totsaved) {
				memcpy(newpars,psys->particles,totsaved*sizeof(ParticleData));

				if (psys->particles->boid)
					memcpy(newboids, psys->particles->boid, totsaved*sizeof(BoidParticle));
			}

			if (psys->particles->keys)
				MEM_freeN(psys->particles->keys);

			if (psys->particles->boid)
				MEM_freeN(psys->particles->boid);

			for (p=0, pa=newpars; p<totsaved; p++, pa++) {
				if (pa->keys) {
					pa->keys= NULL;
					pa->totkey= 0;
				}
			}

			for (p=totsaved, pa=psys->particles+totsaved; p<psys->totpart; p++, pa++)
				if (pa->hair) MEM_freeN(pa->hair);

			MEM_freeN(psys->particles);
			psys_free_pdd(psys);
		}

		psys->particles=newpars;
		psys->totpart=totpart;

		if (newboids) {
			LOOP_PARTICLES {
				pa->boid = newboids++;
			}
		}
	}

	if (psys->child) {
		MEM_freeN(psys->child);
		psys->child=NULL;
		psys->totchild=0;
	}
}

int psys_get_child_number(Scene *scene, ParticleSystem *psys)
{
	int nbr;

	if (!psys->part->childtype)
		return 0;

	if (psys->renderdata)
		nbr= psys->part->ren_child_nbr;
	else
		nbr= psys->part->child_nbr;

	return get_render_child_particle_number(&scene->r, nbr, psys->renderdata != NULL);
}

int psys_get_tot_child(Scene *scene, ParticleSystem *psys)
{
	return psys->totpart*psys_get_child_number(scene, psys);
}

/************************************************/
/*			Distribution						*/
/************************************************/

void psys_calc_dmcache(Object *ob, DerivedMesh *dm_final, DerivedMesh *dm_deformed, ParticleSystem *psys)
{
	/* use for building derived mesh mapping info:
	 *
	 * node: the allocated links - total derived mesh element count
	 * nodearray: the array of nodes aligned with the base mesh's elements, so
	 *            each original elements can reference its derived elements
	 */
	Mesh *me= (Mesh*)ob->data;
	bool use_modifier_stack= psys->part->use_modifier_stack;
	PARTICLE_P;

	/* CACHE LOCATIONS */
	if (!dm_final->deformedOnly) {
		/* Will use later to speed up subsurf/derivedmesh */
		LinkNode *node, *nodedmelem, **nodearray;
		int totdmelem, totelem, i, *origindex, *origindex_poly = NULL;

		if (psys->part->from == PART_FROM_VERT) {
			totdmelem= dm_final->getNumVerts(dm_final);

			if (use_modifier_stack) {
				totelem= totdmelem;
				origindex= NULL;
			}
			else {
				totelem= me->totvert;
				origindex= dm_final->getVertDataArray(dm_final, CD_ORIGINDEX);
			}
		}
		else { /* FROM_FACE/FROM_VOLUME */
			totdmelem= dm_final->getNumTessFaces(dm_final);

			if (use_modifier_stack) {
				totelem= totdmelem;
				origindex= NULL;
				origindex_poly= NULL;
			}
			else {
				totelem = dm_deformed->getNumTessFaces(dm_deformed);
				origindex = dm_final->getTessFaceDataArray(dm_final, CD_ORIGINDEX);

				/* for face lookups we need the poly origindex too */
				origindex_poly= dm_final->getPolyDataArray(dm_final, CD_ORIGINDEX);
				if (origindex_poly == NULL) {
					origindex= NULL;
				}
			}
		}

		nodedmelem= MEM_callocN(sizeof(LinkNode)*totdmelem, "psys node elems");
		nodearray= MEM_callocN(sizeof(LinkNode *)*totelem, "psys node array");

		for (i=0, node=nodedmelem; i<totdmelem; i++, node++) {
			int origindex_final;
			node->link = POINTER_FROM_INT(i);

			/* may be vertex or face origindex */
			if (use_modifier_stack) {
				origindex_final = i;
			}
			else {
				origindex_final = origindex ? origindex[i] : ORIGINDEX_NONE;

				/* if we have a poly source, do an index lookup */
				if (origindex_poly && origindex_final != ORIGINDEX_NONE) {
					origindex_final = origindex_poly[origindex_final];
				}
			}

			if (origindex_final != ORIGINDEX_NONE && origindex_final < totelem) {
				if (nodearray[origindex_final]) {
					/* prepend */
					node->next = nodearray[origindex_final];
					nodearray[origindex_final] = node;
				}
				else {
					nodearray[origindex_final] = node;
				}
			}
		}

		/* cache the verts/faces! */
		LOOP_PARTICLES {
			if (pa->num < 0) {
				pa->num_dmcache = DMCACHE_NOTFOUND;
				continue;
			}

			if (use_modifier_stack) {
				if (pa->num < totelem)
					pa->num_dmcache = DMCACHE_ISCHILD;
				else
					pa->num_dmcache = DMCACHE_NOTFOUND;
			}
			else {
				if (psys->part->from == PART_FROM_VERT) {
					if (pa->num < totelem && nodearray[pa->num])
						pa->num_dmcache= POINTER_AS_INT(nodearray[pa->num]->link);
					else
						pa->num_dmcache = DMCACHE_NOTFOUND;
				}
				else { /* FROM_FACE/FROM_VOLUME */
					pa->num_dmcache = psys_particle_dm_face_lookup(dm_final, dm_deformed, pa->num, pa->fuv, nodearray);
				}
			}
		}

		MEM_freeN(nodearray);
		MEM_freeN(nodedmelem);
	}
	else {
		/* TODO PARTICLE, make the following line unnecessary, each function
		 * should know to use the num or num_dmcache, set the num_dmcache to
		 * an invalid value, just in case */

		LOOP_PARTICLES {
			pa->num_dmcache = DMCACHE_NOTFOUND;
		}
	}
}

/* threaded child particle distribution and path caching */
void psys_thread_context_init(ParticleThreadContext *ctx, ParticleSimulationData *sim)
{
	memset(ctx, 0, sizeof(ParticleThreadContext));
	ctx->sim = *sim;
	ctx->dm = ctx->sim.psmd->dm_final;
	ctx->ma = give_current_material(sim->ob, sim->psys->part->omat);
}

#define MAX_PARTICLES_PER_TASK 256 /* XXX arbitrary - maybe use at least number of points instead for better balancing? */

BLI_INLINE int ceil_ii(int a, int b)
{
	return (a + b - 1) / b;
}

void psys_tasks_create(ParticleThreadContext *ctx, int startpart, int endpart, ParticleTask **r_tasks, int *r_numtasks)
{
	ParticleTask *tasks;
	int numtasks = ceil_ii((endpart - startpart), MAX_PARTICLES_PER_TASK);
	float particles_per_task = (float)(endpart - startpart) / (float)numtasks, p, pnext;
	int i;

	tasks = MEM_callocN(sizeof(ParticleTask) * numtasks, "ParticleThread");
	*r_numtasks = numtasks;
	*r_tasks = tasks;

	p = (float)startpart;
	for (i = 0; i < numtasks; i++, p = pnext) {
		pnext = p + particles_per_task;

		tasks[i].ctx = ctx;
		tasks[i].begin = (int)p;
		tasks[i].end = min_ii((int)pnext, endpart);
	}
}

void psys_tasks_free(ParticleTask *tasks, int numtasks)
{
	int i;

	/* threads */
	for (i = 0; i < numtasks; ++i) {
		if (tasks[i].rng)
			BLI_rng_free(tasks[i].rng);
		if (tasks[i].rng_path)
			BLI_rng_free(tasks[i].rng_path);
	}

	MEM_freeN(tasks);
}

void psys_thread_context_free(ParticleThreadContext *ctx)
{
	/* path caching */
	if (ctx->vg_length)
		MEM_freeN(ctx->vg_length);
	if (ctx->vg_clump)
		MEM_freeN(ctx->vg_clump);
	if (ctx->vg_kink)
		MEM_freeN(ctx->vg_kink);
	if (ctx->vg_rough1)
		MEM_freeN(ctx->vg_rough1);
	if (ctx->vg_rough2)
		MEM_freeN(ctx->vg_rough2);
	if (ctx->vg_roughe)
		MEM_freeN(ctx->vg_roughe);
	if (ctx->vg_twist)
		MEM_freeN(ctx->vg_twist);

	if (ctx->sim.psys->lattice_deform_data) {
		end_latt_deform(ctx->sim.psys->lattice_deform_data);
		ctx->sim.psys->lattice_deform_data = NULL;
	}

	/* distribution */
	if (ctx->jit) MEM_freeN(ctx->jit);
	if (ctx->jitoff) MEM_freeN(ctx->jitoff);
	if (ctx->weight) MEM_freeN(ctx->weight);
	if (ctx->index) MEM_freeN(ctx->index);
	if (ctx->skip) MEM_freeN(ctx->skip);
	if (ctx->seams) MEM_freeN(ctx->seams);
	//if (ctx->vertpart) MEM_freeN(ctx->vertpart);
	BLI_kdtree_free(ctx->tree);

	if (ctx->clumpcurve != NULL) {
		curvemapping_free(ctx->clumpcurve);
	}
	if (ctx->roughcurve != NULL) {
		curvemapping_free(ctx->roughcurve);
	}
	if (ctx->twistcurve != NULL) {
		curvemapping_free(ctx->twistcurve);
	}
}

static void initialize_particle_texture(ParticleSimulationData *sim, ParticleData *pa, int p)
{
	ParticleSystem *psys = sim->psys;
	ParticleSettings *part = psys->part;
	ParticleTexture ptex;

	psys_get_texture(sim, pa, &ptex, PAMAP_INIT, 0.f);

	switch (part->type) {
		case PART_EMITTER:
			if (ptex.exist < psys_frand(psys, p + 125)) {
				pa->flag |= PARS_UNEXIST;
			}
			pa->time = part->sta + (part->end - part->sta)*ptex.time;
			break;
		case PART_HAIR:
			if (ptex.exist < psys_frand(psys, p + 125)) {
				pa->flag |= PARS_UNEXIST;
			}
			pa->time = 0.f;
			break;
		case PART_FLUID:
			break;
	}
}

/* set particle parameters that don't change during particle's life */
void initialize_particle(ParticleSimulationData *sim, ParticleData *pa)
{
	ParticleSettings *part = sim->psys->part;
	float birth_time = (float)(pa - sim->psys->particles) / (float)sim->psys->totpart;

	pa->flag &= ~PARS_UNEXIST;
	pa->time = part->sta + (part->end - part->sta) * birth_time;

	pa->hair_index = 0;
	/* we can't reset to -1 anymore since we've figured out correct index in distribute_particles */
	/* usage other than straight after distribute has to handle this index by itself - jahka*/
	//pa->num_dmcache = DMCACHE_NOTFOUND; /* assume we don't have a derived mesh face */
}

static void initialize_all_particles(ParticleSimulationData *sim)
{
	ParticleSystem *psys = sim->psys;
	ParticleSettings *part = psys->part;
	/* Grid distributionsets UNEXIST flag, need to take care of
	 * it here because later this flag is being reset.
	 *
	 * We can't do it for any distribution, because it'll then
	 * conflict with texture influence, which does not free
	 * unexisting particles and only sets flag.
	 *
	 * It's not so bad, because only grid distribution sets
	 * UNEXIST flag.
	 */
	const bool emit_from_volume_grid = (part->distr == PART_DISTR_GRID) &&
	                                   (!ELEM(part->from, PART_FROM_VERT, PART_FROM_CHILD));
	PARTICLE_P;
	LOOP_PARTICLES {
		if (!(emit_from_volume_grid && (pa->flag & PARS_UNEXIST) != 0)) {
			initialize_particle(sim, pa);
		}
	}
}

static void free_unexisting_particles(ParticleSimulationData *sim)
{
	ParticleSystem *psys = sim->psys;
	PARTICLE_P;

	psys->totunexist = 0;

	LOOP_PARTICLES {
		if (pa->flag & PARS_UNEXIST) {
			psys->totunexist++;
		}
	}

	if (psys->totpart && psys->totunexist == psys->totpart) {
		if (psys->particles->boid)
			MEM_freeN(psys->particles->boid);

		MEM_freeN(psys->particles);
		psys->particles = NULL;
		psys->totpart = psys->totunexist = 0;
	}

	if (psys->totunexist) {
		int newtotpart = psys->totpart - psys->totunexist;
		ParticleData *npa, *newpars;

		npa = newpars = MEM_callocN(newtotpart * sizeof(ParticleData), "particles");

		for (p=0, pa=psys->particles; p<newtotpart; p++, pa++, npa++) {
			while (pa->flag & PARS_UNEXIST)
				pa++;

			memcpy(npa, pa, sizeof(ParticleData));
		}

		if (psys->particles->boid)
			MEM_freeN(psys->particles->boid);
		MEM_freeN(psys->particles);
		psys->particles = newpars;
		psys->totpart -= psys->totunexist;

		if (psys->particles->boid) {
			BoidParticle *newboids = MEM_callocN(psys->totpart * sizeof(BoidParticle), "boid particles");

			LOOP_PARTICLES {
				pa->boid = newboids++;
			}

		}
	}
}

static void get_angular_velocity_vector(short avemode, ParticleKey *state, float vec[3])
{
	switch (avemode) {
		case PART_AVE_VELOCITY:
			copy_v3_v3(vec, state->vel);
			break;
		case PART_AVE_HORIZONTAL:
		{
			float zvec[3];
			zvec[0] = zvec[1] = 0;
			zvec[2] = 1.f;
			cross_v3_v3v3(vec, state->vel, zvec);
			break;
		}
		case PART_AVE_VERTICAL:
		{
			float zvec[3], temp[3];
			zvec[0] = zvec[1] = 0;
			zvec[2] = 1.f;
			cross_v3_v3v3(temp, state->vel, zvec);
			cross_v3_v3v3(vec, temp, state->vel);
			break;
		}
		case PART_AVE_GLOBAL_X:
			vec[0] = 1.f;
			vec[1] = vec[2] = 0;
			break;
		case PART_AVE_GLOBAL_Y:
			vec[1] = 1.f;
			vec[0] = vec[2] = 0;
			break;
		case PART_AVE_GLOBAL_Z:
			vec[2] = 1.f;
			vec[0] = vec[1] = 0;
			break;
	}
}

void psys_get_birth_coords(ParticleSimulationData *sim, ParticleData *pa, ParticleKey *state, float dtime, float cfra)
{
	Object *ob = sim->ob;
	ParticleSystem *psys = sim->psys;
	ParticleSettings *part = psys->part;
	ParticleTexture ptex;
	float fac, phasefac, nor[3] = {0,0,0},loc[3],vel[3] = {0.0,0.0,0.0},rot[4],q2[4];
	float r_vel[3],r_ave[3],r_rot[4],vec[3],p_vel[3] = {0.0,0.0,0.0};
	float x_vec[3] = {1.0,0.0,0.0}, utan[3] = {0.0,1.0,0.0}, vtan[3] = {0.0,0.0,1.0}, rot_vec[3] = {0.0,0.0,0.0};
	float q_phase[4];

	const bool use_boids = ((part->phystype == PART_PHYS_BOIDS) &&
	                        (pa->boid != NULL));
	const bool use_tangents = ((use_boids == false) &&
	                           ((part->tanfac != 0.0f) || (part->rotmode == PART_ROT_NOR_TAN)));

	int p = pa - psys->particles;

	/* get birth location from object		*/
	if (use_tangents)
		psys_particle_on_emitter(sim->psmd, part->from,pa->num, pa->num_dmcache, pa->fuv,pa->foffset,loc,nor,utan,vtan,0,0);
	else
		psys_particle_on_emitter(sim->psmd, part->from,pa->num, pa->num_dmcache, pa->fuv,pa->foffset,loc,nor,0,0,0,0);

	/* get possible textural influence */
	psys_get_texture(sim, pa, &ptex, PAMAP_IVEL, cfra);

	/* particles live in global space so	*/
	/* let's convert:						*/
	/* -location							*/
	mul_m4_v3(ob->obmat, loc);

	/* -normal								*/
	mul_mat3_m4_v3(ob->obmat, nor);
	normalize_v3(nor);

	/* -tangent								*/
	if (use_tangents) {
		//float phase=vg_rot?2.0f*(psys_particle_value_from_verts(sim->psmd->dm,part->from,pa,vg_rot)-0.5f):0.0f;
		float phase=0.0f;
		mul_v3_fl(vtan,-cosf((float)M_PI*(part->tanphase+phase)));
		fac= -sinf((float)M_PI*(part->tanphase+phase));
		madd_v3_v3fl(vtan, utan, fac);

		mul_mat3_m4_v3(ob->obmat,vtan);

		copy_v3_v3(utan, nor);
		mul_v3_fl(utan,dot_v3v3(vtan,nor));
		sub_v3_v3(vtan, utan);

		normalize_v3(vtan);
	}


	/* -velocity (boids need this even if there's no random velocity) */
	if (part->randfac != 0.0f || (part->phystype==PART_PHYS_BOIDS && pa->boid)) {
		r_vel[0] = 2.0f * (psys_frand(psys, p + 10) - 0.5f);
		r_vel[1] = 2.0f * (psys_frand(psys, p + 11) - 0.5f);
		r_vel[2] = 2.0f * (psys_frand(psys, p + 12) - 0.5f);

		mul_mat3_m4_v3(ob->obmat, r_vel);
		normalize_v3(r_vel);
	}

	/* -angular velocity					*/
	if (part->avemode==PART_AVE_RAND) {
		r_ave[0] = 2.0f * (psys_frand(psys, p + 13) - 0.5f);
		r_ave[1] = 2.0f * (psys_frand(psys, p + 14) - 0.5f);
		r_ave[2] = 2.0f * (psys_frand(psys, p + 15) - 0.5f);

		mul_mat3_m4_v3(ob->obmat,r_ave);
		normalize_v3(r_ave);
	}

	/* -rotation							*/
	if (part->randrotfac != 0.0f) {
		r_rot[0] = 2.0f * (psys_frand(psys, p + 16) - 0.5f);
		r_rot[1] = 2.0f * (psys_frand(psys, p + 17) - 0.5f);
		r_rot[2] = 2.0f * (psys_frand(psys, p + 18) - 0.5f);
		r_rot[3] = 2.0f * (psys_frand(psys, p + 19) - 0.5f);
		normalize_qt(r_rot);

		mat4_to_quat(rot,ob->obmat);
		mul_qt_qtqt(r_rot,r_rot,rot);
	}

	if (use_boids) {
		float dvec[3], q[4], mat[3][3];

		copy_v3_v3(state->co,loc);

		/* boids don't get any initial velocity  */
		zero_v3(state->vel);

		/* boids store direction in ave */
		if (fabsf(nor[2])==1.0f) {
			sub_v3_v3v3(state->ave, loc, ob->obmat[3]);
			normalize_v3(state->ave);
		}
		else {
			copy_v3_v3(state->ave, nor);
		}

		/* calculate rotation matrix */
		project_v3_v3v3(dvec, r_vel, state->ave);
		sub_v3_v3v3(mat[0], state->ave, dvec);
		normalize_v3(mat[0]);
		negate_v3_v3(mat[2], r_vel);
		normalize_v3(mat[2]);
		cross_v3_v3v3(mat[1], mat[2], mat[0]);

		/* apply rotation */
		mat3_to_quat_is_ok( q,mat);
		copy_qt_qt(state->rot, q);
	}
	else {
		/* conversion done so now we apply new:	*/
		/* -velocity from:						*/

		/*		*reactions						*/
		if (dtime > 0.f) {
			sub_v3_v3v3(vel, pa->state.vel, pa->prev_state.vel);
		}

		/*		*emitter velocity				*/
		if (dtime != 0.f && part->obfac != 0.f) {
			sub_v3_v3v3(vel, loc, state->co);
			mul_v3_fl(vel, part->obfac/dtime);
		}

		/*		*emitter normal					*/
		if (part->normfac != 0.f)
			madd_v3_v3fl(vel, nor, part->normfac);

		/*		*emitter tangent				*/
		if (sim->psmd && part->tanfac != 0.f)
			madd_v3_v3fl(vel, vtan, part->tanfac);

		/*		*emitter object orientation		*/
		if (part->ob_vel[0] != 0.f) {
			normalize_v3_v3(vec, ob->obmat[0]);
			madd_v3_v3fl(vel, vec, part->ob_vel[0]);
		}
		if (part->ob_vel[1] != 0.f) {
			normalize_v3_v3(vec, ob->obmat[1]);
			madd_v3_v3fl(vel, vec, part->ob_vel[1]);
		}
		if (part->ob_vel[2] != 0.f) {
			normalize_v3_v3(vec, ob->obmat[2]);
			madd_v3_v3fl(vel, vec, part->ob_vel[2]);
		}

		/*		*texture						*/
		/* TODO	*/

		/*		*random							*/
		if (part->randfac != 0.f)
			madd_v3_v3fl(vel, r_vel, part->randfac);

		/*		*particle						*/
		if (part->partfac != 0.f)
			madd_v3_v3fl(vel, p_vel, part->partfac);

		mul_v3_v3fl(state->vel, vel, ptex.ivel);

		/* -location from emitter				*/
		copy_v3_v3(state->co,loc);

		/* -rotation							*/
		unit_qt(state->rot);

		if (part->rotmode) {
			bool use_global_space;

			/* create vector into which rotation is aligned */
			switch (part->rotmode) {
				case PART_ROT_NOR:
				case PART_ROT_NOR_TAN:
					copy_v3_v3(rot_vec, nor);
					use_global_space = false;
					break;
				case PART_ROT_VEL:
					copy_v3_v3(rot_vec, vel);
					use_global_space = true;
					break;
				case PART_ROT_GLOB_X:
				case PART_ROT_GLOB_Y:
				case PART_ROT_GLOB_Z:
					rot_vec[part->rotmode - PART_ROT_GLOB_X] = 1.0f;
					use_global_space = true;
					break;
				case PART_ROT_OB_X:
				case PART_ROT_OB_Y:
				case PART_ROT_OB_Z:
					copy_v3_v3(rot_vec, ob->obmat[part->rotmode - PART_ROT_OB_X]);
					use_global_space = false;
					break;
				default:
					use_global_space = true;
					break;
			}

			/* create rotation quat */


			if (use_global_space) {
				negate_v3(rot_vec);
				vec_to_quat(q2, rot_vec, OB_POSX, OB_POSZ);

				/* randomize rotation quat */
				if (part->randrotfac != 0.0f) {
					interp_qt_qtqt(rot, q2, r_rot, part->randrotfac);
				}
				else {
					copy_qt_qt(rot, q2);
				}
			}
			else {
				/* calculate rotation in local-space */
				float q_obmat[4];
				float q_imat[4];

				mat4_to_quat(q_obmat, ob->obmat);
				invert_qt_qt_normalized(q_imat, q_obmat);


				if (part->rotmode != PART_ROT_NOR_TAN) {
					float rot_vec_local[3];

					/* rot_vec */
					negate_v3(rot_vec);
					copy_v3_v3(rot_vec_local, rot_vec);
					mul_qt_v3(q_imat, rot_vec_local);
					normalize_v3(rot_vec_local);

					vec_to_quat(q2, rot_vec_local, OB_POSX, OB_POSZ);
				}
				else {
					/* (part->rotmode == PART_ROT_NOR_TAN) */
					float tmat[3][3];

					/* note: utan_local is not taken from 'utan', we calculate from rot_vec/vtan */
					/* note: it looks like rotation phase may be applied twice (once with vtan, again below)
					 * however this isn't the case - campbell */
					float *rot_vec_local = tmat[0];
					float *vtan_local    = tmat[1];
					float *utan_local    = tmat[2];

					/* use tangents */
					BLI_assert(use_tangents == true);

					/* rot_vec */
					copy_v3_v3(rot_vec_local, rot_vec);
					mul_qt_v3(q_imat, rot_vec_local);

					/* vtan_local */
					copy_v3_v3(vtan_local, vtan);  /* flips, cant use */
					mul_qt_v3(q_imat, vtan_local);

					/* ensure orthogonal matrix (rot_vec aligned) */
					cross_v3_v3v3(utan_local, vtan_local, rot_vec_local);
					cross_v3_v3v3(vtan_local, utan_local, rot_vec_local);

					/* note: no need to normalize */
					mat3_to_quat(q2, tmat);
				}

				/* randomize rotation quat */
				if (part->randrotfac != 0.0f) {
					mul_qt_qtqt(r_rot, r_rot, q_imat);
					interp_qt_qtqt(rot, q2, r_rot, part->randrotfac);
				}
				else {
					copy_qt_qt(rot, q2);
				}

				mul_qt_qtqt(rot, q_obmat, rot);
			}

			/* rotation phase */
			phasefac = part->phasefac;
			if (part->randphasefac != 0.0f)
				phasefac += part->randphasefac * psys_frand(psys, p + 20);
			axis_angle_to_quat( q_phase,x_vec, phasefac*(float)M_PI);

			/* combine base rotation & phase */
			mul_qt_qtqt(state->rot, rot, q_phase);
		}

		/* -angular velocity					*/

		zero_v3(state->ave);

		if (part->avemode) {
			if (part->avemode == PART_AVE_RAND)
				copy_v3_v3(state->ave, r_ave);
			else
				get_angular_velocity_vector(part->avemode, state, state->ave);

			normalize_v3(state->ave);
			mul_v3_fl(state->ave, part->avefac);
		}
	}
}

/* recursively evaluate emitter parent anim at cfra */
static void evaluate_emitter_anim(Scene *scene, Object *ob, float cfra)
{
	if (ob->parent)
		evaluate_emitter_anim(scene, ob->parent, cfra);

	/* we have to force RECALC_ANIM here since where_is_objec_time only does drivers */
	BKE_animsys_evaluate_animdata(scene, &ob->id, ob->adt, cfra, ADT_RECALC_ANIM);
	BKE_object_where_is_calc_time(scene, ob, cfra);
}

/* sets particle to the emitter surface with initial velocity & rotation */
void reset_particle(ParticleSimulationData *sim, ParticleData *pa, float dtime, float cfra)
{
	ParticleSystem *psys = sim->psys;
	ParticleSettings *part;
	ParticleTexture ptex;
	int p = pa - psys->particles;
	part=psys->part;

	/* get precise emitter matrix if particle is born */
	if (part->type != PART_HAIR && dtime > 0.f && pa->time < cfra && pa->time >= sim->psys->cfra) {
		evaluate_emitter_anim(sim->scene, sim->ob, pa->time);

		psys->flag |= PSYS_OB_ANIM_RESTORE;
	}

	psys_get_birth_coords(sim, pa, &pa->state, dtime, cfra);

	/* Initialize particle settings which depends on texture.
	 *
	 * We could only do it now because we'll need to know coordinate
	 * before sampling the texture.
	 */
	initialize_particle_texture(sim, pa, p);

	if (part->phystype==PART_PHYS_BOIDS && pa->boid) {
		BoidParticle *bpa = pa->boid;

		/* and gravity in r_ve */
		bpa->gravity[0] = bpa->gravity[1] = 0.0f;
		bpa->gravity[2] = -1.0f;
		if ((sim->scene->physics_settings.flag & PHYS_GLOBAL_GRAVITY) &&
		    (sim->scene->physics_settings.gravity[2] != 0.0f))
		{
			bpa->gravity[2] = sim->scene->physics_settings.gravity[2];
		}

		bpa->data.health = part->boids->health;
		bpa->data.mode = eBoidMode_InAir;
		bpa->data.state_id = ((BoidState*)part->boids->states.first)->id;
		bpa->data.acc[0]=bpa->data.acc[1]=bpa->data.acc[2]=0.0f;
	}

	if (part->type == PART_HAIR) {
		pa->lifetime = 100.0f;
	}
	else {
		/* initialize the lifetime, in case the texture coordinates
		 * are from Particles/Strands, which would cause undefined values
		 */
		pa->lifetime = part->lifetime * (1.0f - part->randlife * psys_frand(psys, p + 21));
		pa->dietime = pa->time + pa->lifetime;

		/* get possible textural influence */
		psys_get_texture(sim, pa, &ptex, PAMAP_LIFE, cfra);

		pa->lifetime = part->lifetime * ptex.life;

		if (part->randlife != 0.0f)
			pa->lifetime *= 1.0f - part->randlife * psys_frand(psys, p + 21);
	}

	pa->dietime = pa->time + pa->lifetime;

	if ((sim->psys->pointcache) &&
	    (sim->psys->pointcache->flag & PTCACHE_BAKED) &&
	    (sim->psys->pointcache->mem_cache.first))
	{
		float dietime = psys_get_dietime_from_cache(sim->psys->pointcache, p);
		pa->dietime = MIN2(pa->dietime, dietime);
	}

	if (pa->time > cfra)
		pa->alive = PARS_UNBORN;
	else if (pa->dietime <= cfra)
		pa->alive = PARS_DEAD;
	else
		pa->alive = PARS_ALIVE;

	pa->state.time = cfra;
}
static void reset_all_particles(ParticleSimulationData *sim, float dtime, float cfra, int from)
{
	ParticleData *pa;
	int p, totpart=sim->psys->totpart;

	for (p=from, pa=sim->psys->particles+from; p<totpart; p++, pa++)
		reset_particle(sim, pa, dtime, cfra);
}
/************************************************/
/*			Particle targets					*/
/************************************************/
ParticleSystem *psys_get_target_system(Object *ob, ParticleTarget *pt)
{
	ParticleSystem *psys = NULL;

	if (pt->ob == NULL || pt->ob == ob)
		psys = BLI_findlink(&ob->particlesystem, pt->psys-1);
	else
		psys = BLI_findlink(&pt->ob->particlesystem, pt->psys-1);

	if (psys)
		pt->flag |= PTARGET_VALID;
	else
		pt->flag &= ~PTARGET_VALID;

	return psys;
}
/************************************************/
/*			Keyed particles						*/
/************************************************/
/* Counts valid keyed targets */
void psys_count_keyed_targets(ParticleSimulationData *sim)
{
	ParticleSystem *psys = sim->psys, *kpsys;
	ParticleTarget *pt = psys->targets.first;
	int keys_valid = 1;
	psys->totkeyed = 0;

	for (; pt; pt=pt->next) {
		kpsys = psys_get_target_system(sim->ob, pt);

		if (kpsys && kpsys->totpart) {
			psys->totkeyed += keys_valid;
			if (psys->flag & PSYS_KEYED_TIMING && pt->duration != 0.0f)
				psys->totkeyed += 1;
		}
		else {
			keys_valid = 0;
		}
	}

	psys->totkeyed *= psys->flag & PSYS_KEYED_TIMING ? 1 : psys->part->keyed_loops;
}

static void set_keyed_keys(ParticleSimulationData *sim)
{
	ParticleSystem *psys = sim->psys;
	ParticleSimulationData ksim= {0};
	ParticleTarget *pt;
	PARTICLE_P;
	ParticleKey *key;
	int totpart = psys->totpart, k, totkeys = psys->totkeyed;
	int keyed_flag = 0;

	ksim.scene= sim->scene;

	/* no proper targets so let's clear and bail out */
	if (psys->totkeyed==0) {
		free_keyed_keys(psys);
		psys->flag &= ~PSYS_KEYED;
		return;
	}

	if (totpart && psys->particles->totkey != totkeys) {
		free_keyed_keys(psys);

		key = MEM_callocN(totpart*totkeys*sizeof(ParticleKey), "Keyed keys");

		LOOP_PARTICLES {
			pa->keys = key;
			pa->totkey = totkeys;
			key += totkeys;
		}
	}

	psys->flag &= ~PSYS_KEYED;


	pt = psys->targets.first;
	for (k=0; k<totkeys; k++) {
		ksim.ob = pt->ob ? pt->ob : sim->ob;
		ksim.psys = BLI_findlink(&ksim.ob->particlesystem, pt->psys - 1);
		keyed_flag = (ksim.psys->flag & PSYS_KEYED);
		ksim.psys->flag &= ~PSYS_KEYED;

		LOOP_PARTICLES {
			key = pa->keys + k;
			key->time = -1.0; /* use current time */

			psys_get_particle_state(&ksim, p%ksim.psys->totpart, key, 1);

			if (psys->flag & PSYS_KEYED_TIMING) {
				key->time = pa->time + pt->time;
				if (pt->duration != 0.0f && k+1 < totkeys) {
					copy_particle_key(key+1, key, 1);
					(key+1)->time = pa->time + pt->time + pt->duration;
				}
			}
			else if (totkeys > 1)
				key->time = pa->time + (float)k / (float)(totkeys - 1) * pa->lifetime;
			else
				key->time = pa->time;
		}

		if (psys->flag & PSYS_KEYED_TIMING && pt->duration != 0.0f)
			k++;

		ksim.psys->flag |= keyed_flag;

		pt = (pt->next && pt->next->flag & PTARGET_VALID) ? pt->next : psys->targets.first;
	}

	psys->flag |= PSYS_KEYED;
}

/************************************************/
/*			Point Cache							*/
/************************************************/
void psys_make_temp_pointcache(Object *ob, ParticleSystem *psys)
{
	PointCache *cache = psys->pointcache;

	if (cache->flag & PTCACHE_DISK_CACHE && BLI_listbase_is_empty(&cache->mem_cache)) {
		PTCacheID pid;
		BKE_ptcache_id_from_particles(&pid, ob, psys);
		cache->flag &= ~PTCACHE_DISK_CACHE;
		BKE_ptcache_disk_to_mem(&pid);
		cache->flag |= PTCACHE_DISK_CACHE;
	}
}
static void psys_clear_temp_pointcache(ParticleSystem *psys)
{
	if (psys->pointcache->flag & PTCACHE_DISK_CACHE)
		BKE_ptcache_free_mem(&psys->pointcache->mem_cache);
}
void psys_get_pointcache_start_end(Scene *scene, ParticleSystem *psys, int *sfra, int *efra)
{
	ParticleSettings *part = psys->part;

	*sfra = max_ii(1, (int)part->sta);
	*efra = min_ii((int)(part->end + part->lifetime + 1.0f), max_ii(scene->r.pefra, scene->r.efra));
}

/************************************************/
/*			Effectors							*/
/************************************************/
static void psys_update_particle_bvhtree(ParticleSystem *psys, float cfra)
{
	if (psys) {
		PARTICLE_P;
		int totpart = 0;
		bool need_rebuild;

		BLI_rw_mutex_lock(&psys_bvhtree_rwlock, THREAD_LOCK_READ);
		need_rebuild = !psys->bvhtree || psys->bvhtree_frame != cfra;
		BLI_rw_mutex_unlock(&psys_bvhtree_rwlock);

		if (need_rebuild) {
			LOOP_SHOWN_PARTICLES {
				totpart++;
			}

			BLI_rw_mutex_lock(&psys_bvhtree_rwlock, THREAD_LOCK_WRITE);

			BLI_bvhtree_free(psys->bvhtree);
			psys->bvhtree = BLI_bvhtree_new(totpart, 0.0, 4, 6);

			LOOP_SHOWN_PARTICLES {
				if (pa->alive == PARS_ALIVE) {
					if (pa->state.time == cfra)
						BLI_bvhtree_insert(psys->bvhtree, p, pa->prev_state.co, 1);
					else
						BLI_bvhtree_insert(psys->bvhtree, p, pa->state.co, 1);
				}
			}
			BLI_bvhtree_balance(psys->bvhtree);

			psys->bvhtree_frame = cfra;

			BLI_rw_mutex_unlock(&psys_bvhtree_rwlock);
		}
	}
}
void psys_update_particle_tree(ParticleSystem *psys, float cfra)
{
	if (psys) {
		PARTICLE_P;
		int totpart = 0;

		if (!psys->tree || psys->tree_frame != cfra) {
			LOOP_SHOWN_PARTICLES {
				totpart++;
			}

			BLI_kdtree_free(psys->tree);
			psys->tree = BLI_kdtree_new(psys->totpart);

			LOOP_SHOWN_PARTICLES {
				if (pa->alive == PARS_ALIVE) {
					if (pa->state.time == cfra)
						BLI_kdtree_insert(psys->tree, p, pa->prev_state.co);
					else
						BLI_kdtree_insert(psys->tree, p, pa->state.co);
				}
			}
			BLI_kdtree_balance(psys->tree);

			psys->tree_frame = cfra;
		}
	}
}

static void psys_update_effectors(ParticleSimulationData *sim)
{
	pdEndEffectors(&sim->psys->effectors);
	sim->psys->effectors = pdInitEffectors(sim->scene, sim->ob, sim->psys,
	                                       sim->psys->part->effector_weights, true);
	precalc_guides(sim, sim->psys->effectors);
}

static void integrate_particle(ParticleSettings *part, ParticleData *pa, float dtime, float *external_acceleration,
                               void (*force_func)(void *forcedata, ParticleKey *state, float *force, float *impulse),
                               void *forcedata)
{
#define ZERO_F43 {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}}

	ParticleKey states[5];
	float force[3], acceleration[3], impulse[3], dx[4][3] = ZERO_F43, dv[4][3] = ZERO_F43, oldpos[3];
	float pa_mass= (part->flag & PART_SIZEMASS ? part->mass * pa->size : part->mass);
	int i, steps=1;
	int integrator = part->integrator;

#undef ZERO_F43

	copy_v3_v3(oldpos, pa->state.co);

	/* Verlet integration behaves strangely with moving emitters, so do first step with euler. */
	if (pa->prev_state.time < 0.f && integrator == PART_INT_VERLET)
		integrator = PART_INT_EULER;

	switch (integrator) {
		case PART_INT_EULER:
			steps=1;
			break;
		case PART_INT_MIDPOINT:
			steps=2;
			break;
		case PART_INT_RK4:
			steps=4;
			break;
		case PART_INT_VERLET:
			steps=1;
			break;
	}

	for (i=0; i<steps; i++) {
		copy_particle_key(states + i, &pa->state, 1);
	}

	states->time = 0.f;

	for (i=0; i<steps; i++) {
		zero_v3(force);
		zero_v3(impulse);

		force_func(forcedata, states+i, force, impulse);

		/* force to acceleration*/
		mul_v3_v3fl(acceleration, force, 1.0f/pa_mass);

		if (external_acceleration)
			add_v3_v3(acceleration, external_acceleration);

		/* calculate next state */
		add_v3_v3(states[i].vel, impulse);

		switch (integrator) {
			case PART_INT_EULER:
				madd_v3_v3v3fl(pa->state.co, states->co, states->vel, dtime);
				madd_v3_v3v3fl(pa->state.vel, states->vel, acceleration, dtime);
				break;
			case PART_INT_MIDPOINT:
				if (i==0) {
					madd_v3_v3v3fl(states[1].co, states->co, states->vel, dtime*0.5f);
					madd_v3_v3v3fl(states[1].vel, states->vel, acceleration, dtime*0.5f);
					states[1].time = dtime*0.5f;
					/*fra=sim->psys->cfra+0.5f*dfra;*/
				}
				else {
					madd_v3_v3v3fl(pa->state.co, states->co, states[1].vel, dtime);
					madd_v3_v3v3fl(pa->state.vel, states->vel, acceleration, dtime);
				}
				break;
			case PART_INT_RK4:
				switch (i) {
					case 0:
						copy_v3_v3(dx[0], states->vel);
						mul_v3_fl(dx[0], dtime);
						copy_v3_v3(dv[0], acceleration);
						mul_v3_fl(dv[0], dtime);

						madd_v3_v3v3fl(states[1].co, states->co, dx[0], 0.5f);
						madd_v3_v3v3fl(states[1].vel, states->vel, dv[0], 0.5f);
						states[1].time = dtime*0.5f;
						/*fra=sim->psys->cfra+0.5f*dfra;*/
						break;
					case 1:
						madd_v3_v3v3fl(dx[1], states->vel, dv[0], 0.5f);
						mul_v3_fl(dx[1], dtime);
						copy_v3_v3(dv[1], acceleration);
						mul_v3_fl(dv[1], dtime);

						madd_v3_v3v3fl(states[2].co, states->co, dx[1], 0.5f);
						madd_v3_v3v3fl(states[2].vel, states->vel, dv[1], 0.5f);
						states[2].time = dtime*0.5f;
						break;
					case 2:
						madd_v3_v3v3fl(dx[2], states->vel, dv[1], 0.5f);
						mul_v3_fl(dx[2], dtime);
						copy_v3_v3(dv[2], acceleration);
						mul_v3_fl(dv[2], dtime);

						add_v3_v3v3(states[3].co, states->co, dx[2]);
						add_v3_v3v3(states[3].vel, states->vel, dv[2]);
						states[3].time = dtime;
						/*fra=cfra;*/
						break;
					case 3:
						add_v3_v3v3(dx[3], states->vel, dv[2]);
						mul_v3_fl(dx[3], dtime);
						copy_v3_v3(dv[3], acceleration);
						mul_v3_fl(dv[3], dtime);

						madd_v3_v3v3fl(pa->state.co, states->co, dx[0], 1.0f/6.0f);
						madd_v3_v3fl(pa->state.co, dx[1], 1.0f/3.0f);
						madd_v3_v3fl(pa->state.co, dx[2], 1.0f/3.0f);
						madd_v3_v3fl(pa->state.co, dx[3], 1.0f/6.0f);

						madd_v3_v3v3fl(pa->state.vel, states->vel, dv[0], 1.0f/6.0f);
						madd_v3_v3fl(pa->state.vel, dv[1], 1.0f/3.0f);
						madd_v3_v3fl(pa->state.vel, dv[2], 1.0f/3.0f);
						madd_v3_v3fl(pa->state.vel, dv[3], 1.0f/6.0f);
				}
				break;
			case PART_INT_VERLET:   /* Verlet integration */
				madd_v3_v3v3fl(pa->state.vel, pa->prev_state.vel, acceleration, dtime);
				madd_v3_v3v3fl(pa->state.co, pa->prev_state.co, pa->state.vel, dtime);

				sub_v3_v3v3(pa->state.vel, pa->state.co, oldpos);
				mul_v3_fl(pa->state.vel, 1.0f/dtime);
				break;
		}
	}
}

/*********************************************************************************************************
 *                    SPH fluid physics
 *
 * In theory, there could be unlimited implementation of SPH simulators
 *
 * This code uses in some parts adapted algorithms from the pseudo code as outlined in the Research paper:
 *
 * Titled: Particle-based Viscoelastic Fluid Simulation.
 * Authors: Simon Clavet, Philippe Beaudoin and Pierre Poulin
 * Website: http://www.iro.umontreal.ca/labs/infographie/papers/Clavet-2005-PVFS/
 *
 * Presented at Siggraph, (2005)
 *
 * ********************************************************************************************************/
#define PSYS_FLUID_SPRINGS_INITIAL_SIZE 256
static ParticleSpring *sph_spring_add(ParticleSystem *psys, ParticleSpring *spring)
{
	/* Are more refs required? */
	if (psys->alloc_fluidsprings == 0 || psys->fluid_springs == NULL) {
		psys->alloc_fluidsprings = PSYS_FLUID_SPRINGS_INITIAL_SIZE;
		psys->fluid_springs = (ParticleSpring*)MEM_callocN(psys->alloc_fluidsprings * sizeof(ParticleSpring), "Particle Fluid Springs");
	}
	else if (psys->tot_fluidsprings == psys->alloc_fluidsprings) {
		/* Double the number of refs allocated */
		psys->alloc_fluidsprings *= 2;
		psys->fluid_springs = (ParticleSpring*)MEM_reallocN(psys->fluid_springs, psys->alloc_fluidsprings * sizeof(ParticleSpring));
	}

	memcpy(psys->fluid_springs + psys->tot_fluidsprings, spring, sizeof(ParticleSpring));
	psys->tot_fluidsprings++;

	return psys->fluid_springs + psys->tot_fluidsprings - 1;
}
static void sph_spring_delete(ParticleSystem *psys, int j)
{
	if (j != psys->tot_fluidsprings - 1)
		psys->fluid_springs[j] = psys->fluid_springs[psys->tot_fluidsprings - 1];

	psys->tot_fluidsprings--;

	if (psys->tot_fluidsprings < psys->alloc_fluidsprings/2 && psys->alloc_fluidsprings > PSYS_FLUID_SPRINGS_INITIAL_SIZE) {
		psys->alloc_fluidsprings /= 2;
		psys->fluid_springs = (ParticleSpring*)MEM_reallocN(psys->fluid_springs,  psys->alloc_fluidsprings * sizeof(ParticleSpring));
	}
}
static void sph_springs_modify(ParticleSystem *psys, float dtime)
{
	SPHFluidSettings *fluid = psys->part->fluid;
	ParticleData *pa1, *pa2;
	ParticleSpring *spring = psys->fluid_springs;

	float h, d, Rij[3], rij, Lij;
	int i;

	float yield_ratio = fluid->yield_ratio;
	float plasticity = fluid->plasticity_constant;
	/* scale things according to dtime */
	float timefix = 25.f * dtime;

	if ((fluid->flag & SPH_VISCOELASTIC_SPRINGS)==0 || fluid->spring_k == 0.f)
		return;

	/* Loop through the springs */
	for (i=0; i<psys->tot_fluidsprings; i++, spring++) {
		pa1 = psys->particles + spring->particle_index[0];
		pa2 = psys->particles + spring->particle_index[1];

		sub_v3_v3v3(Rij, pa2->prev_state.co, pa1->prev_state.co);
		rij = normalize_v3(Rij);

		/* adjust rest length */
		Lij = spring->rest_length;
		d = yield_ratio * timefix * Lij;

		if (rij > Lij + d) // Stretch
			spring->rest_length += plasticity * (rij - Lij - d) * timefix;
		else if (rij < Lij - d) // Compress
			spring->rest_length -= plasticity * (Lij - d - rij) * timefix;

		h = 4.f*pa1->size;

		if (spring->rest_length > h)
			spring->delete_flag = 1;
	}

	/* Loop through springs backwaqrds - for efficient delete function */
	for (i=psys->tot_fluidsprings-1; i >= 0; i--) {
		if (psys->fluid_springs[i].delete_flag)
			sph_spring_delete(psys, i);
	}
}
static EdgeHash *sph_springhash_build(ParticleSystem *psys)
{
	EdgeHash *springhash = NULL;
	ParticleSpring *spring;
	int i = 0;

	springhash = BLI_edgehash_new_ex(__func__, psys->tot_fluidsprings);

	for (i=0, spring=psys->fluid_springs; i<psys->tot_fluidsprings; i++, spring++)
		BLI_edgehash_insert(springhash, spring->particle_index[0], spring->particle_index[1], POINTER_FROM_INT(i+1));

	return springhash;
}

#define SPH_NEIGHBORS 512
typedef struct SPHNeighbor {
	ParticleSystem *psys;
	int index;
} SPHNeighbor;

typedef struct SPHRangeData {
	SPHNeighbor neighbors[SPH_NEIGHBORS];
	int tot_neighbors;

	float* data;

	ParticleSystem *npsys;
	ParticleData *pa;

	float h;
	float mass;
	float massfac;
	int use_size;
} SPHRangeData;

static void sph_evaluate_func(BVHTree *tree, ParticleSystem **psys, float co[3], SPHRangeData *pfr, float interaction_radius, BVHTree_RangeQuery callback)
{
	int i;

	pfr->tot_neighbors = 0;

	for (i=0; i < 10 && psys[i]; i++) {
		pfr->npsys    = psys[i];
		pfr->massfac  = psys[i]->part->mass / pfr->mass;
		pfr->use_size = psys[i]->part->flag & PART_SIZEMASS;

		if (tree) {
			BLI_bvhtree_range_query(tree, co, interaction_radius, callback, pfr);
			break;
		}
		else {
			BLI_rw_mutex_lock(&psys_bvhtree_rwlock, THREAD_LOCK_READ);

			BLI_bvhtree_range_query(psys[i]->bvhtree, co, interaction_radius, callback, pfr);

			BLI_rw_mutex_unlock(&psys_bvhtree_rwlock);
		}
	}
}
static void sph_density_accum_cb(void *userdata, int index, const float co[3], float squared_dist)
{
	SPHRangeData *pfr = (SPHRangeData *)userdata;
	ParticleData *npa = pfr->npsys->particles + index;
	float q;
	float dist;

	UNUSED_VARS(co);

	if (npa == pfr->pa || squared_dist < FLT_EPSILON)
		return;

	/* Ugh! One particle has too many neighbors! If some aren't taken into
	 * account, the forces will be biased by the tree search order. This
	 * effectively adds energy to the system, and results in a churning motion.
	 * But, we have to stop somewhere, and it's not the end of the world.
	 *  - jahka and z0r
	 */
	if (pfr->tot_neighbors >= SPH_NEIGHBORS)
		return;

	pfr->neighbors[pfr->tot_neighbors].index = index;
	pfr->neighbors[pfr->tot_neighbors].psys = pfr->npsys;
	pfr->tot_neighbors++;

	dist = sqrtf(squared_dist);
	q = (1.f - dist/pfr->h) * pfr->massfac;

	if (pfr->use_size)
		q *= npa->size;

	pfr->data[0] += q*q;
	pfr->data[1] += q*q*q;
}

/*
 * Find the Courant number for an SPH particle (used for adaptive time step).
 */
static void sph_particle_courant(SPHData *sphdata, SPHRangeData *pfr)
{
	ParticleData *pa, *npa;
	int i;
	float flow[3], offset[3], dist;

	zero_v3(flow);

	dist = 0.0f;
	if (pfr->tot_neighbors > 0) {
		pa = pfr->pa;
		for (i=0; i < pfr->tot_neighbors; i++) {
			npa = pfr->neighbors[i].psys->particles + pfr->neighbors[i].index;
			sub_v3_v3v3(offset, pa->prev_state.co, npa->prev_state.co);
			dist += len_v3(offset);
			add_v3_v3(flow, npa->prev_state.vel);
		}
		dist += sphdata->psys[0]->part->fluid->radius; // TODO: remove this? - z0r
		sphdata->element_size = dist / pfr->tot_neighbors;
		mul_v3_v3fl(sphdata->flow, flow, 1.0f / pfr->tot_neighbors);
	}
	else {
		sphdata->element_size = FLT_MAX;
		copy_v3_v3(sphdata->flow, flow);
	}
}
static void sph_force_cb(void *sphdata_v, ParticleKey *state, float *force, float *UNUSED(impulse))
{
	SPHData *sphdata = (SPHData *)sphdata_v;
	ParticleSystem **psys = sphdata->psys;
	ParticleData *pa = sphdata->pa;
	SPHFluidSettings *fluid = psys[0]->part->fluid;
	ParticleSpring *spring = NULL;
	SPHRangeData pfr;
	SPHNeighbor *pfn;
	float *gravity = sphdata->gravity;
	EdgeHash *springhash = sphdata->eh;

	float q, u, rij, dv[3];
	float pressure, near_pressure;

	float visc = fluid->viscosity_omega;
	float stiff_visc = fluid->viscosity_beta * (fluid->flag & SPH_FAC_VISCOSITY ? fluid->viscosity_omega : 1.f);

	float inv_mass = 1.0f / sphdata->mass;
	float spring_constant = fluid->spring_k;

	/* 4.0 seems to be a pretty good value */
	float interaction_radius = fluid->radius * (fluid->flag & SPH_FAC_RADIUS ? 4.0f * pa->size : 1.0f);
	float h = interaction_radius * sphdata->hfac;
	float rest_density = fluid->rest_density * (fluid->flag & SPH_FAC_DENSITY ? 4.77f : 1.f); /* 4.77 is an experimentally determined density factor */
	float rest_length = fluid->rest_length * (fluid->flag & SPH_FAC_REST_LENGTH ? 2.588f * pa->size : 1.f);

	float stiffness = fluid->stiffness_k;
	float stiffness_near_fac = fluid->stiffness_knear * (fluid->flag & SPH_FAC_REPULSION ? fluid->stiffness_k : 1.f);

	ParticleData *npa;
	float vec[3];
	float vel[3];
	float co[3];
	float data[2];
	float density, near_density;

	int i, spring_index, index = pa - psys[0]->particles;

	data[0] = data[1] = 0;
	pfr.data = data;
	pfr.h = h;
	pfr.pa = pa;
	pfr.mass = sphdata->mass;

	sph_evaluate_func( NULL, psys, state->co, &pfr, interaction_radius, sph_density_accum_cb);

	density = data[0];
	near_density = data[1];

	pressure =  stiffness * (density - rest_density);
	near_pressure = stiffness_near_fac * near_density;

	pfn = pfr.neighbors;
	for (i=0; i<pfr.tot_neighbors; i++, pfn++) {
		npa = pfn->psys->particles + pfn->index;

		madd_v3_v3v3fl(co, npa->prev_state.co, npa->prev_state.vel, state->time);

		sub_v3_v3v3(vec, co, state->co);
		rij = normalize_v3(vec);

		q = (1.f - rij/h) * pfn->psys->part->mass * inv_mass;

		if (pfn->psys->part->flag & PART_SIZEMASS)
			q *= npa->size;

		copy_v3_v3(vel, npa->prev_state.vel);

		/* Double Density Relaxation */
		madd_v3_v3fl(force, vec, -(pressure + near_pressure*q)*q);

		/* Viscosity */
		if (visc > 0.f  || stiff_visc > 0.f) {
			sub_v3_v3v3(dv, vel, state->vel);
			u = dot_v3v3(vec, dv);

			if (u < 0.f && visc > 0.f)
				madd_v3_v3fl(force, vec, 0.5f * q * visc * u );

			if (u > 0.f && stiff_visc > 0.f)
				madd_v3_v3fl(force, vec, 0.5f * q * stiff_visc * u );
		}

		if (spring_constant > 0.f) {
			/* Viscoelastic spring force */
			if (pfn->psys == psys[0] && fluid->flag & SPH_VISCOELASTIC_SPRINGS && springhash) {
				/* BLI_edgehash_lookup appears to be thread-safe. - z0r */
				spring_index = POINTER_AS_INT(BLI_edgehash_lookup(springhash, index, pfn->index));

				if (spring_index) {
					spring = psys[0]->fluid_springs + spring_index - 1;

					madd_v3_v3fl(force, vec, -10.f * spring_constant * (1.f - rij/h) * (spring->rest_length - rij));
				}
				else if (fluid->spring_frames == 0 || (pa->prev_state.time-pa->time) <= fluid->spring_frames) {
					ParticleSpring temp_spring;
					temp_spring.particle_index[0] = index;
					temp_spring.particle_index[1] = pfn->index;
					temp_spring.rest_length = (fluid->flag & SPH_CURRENT_REST_LENGTH) ? rij : rest_length;
					temp_spring.delete_flag = 0;

					/* sph_spring_add is not thread-safe. - z0r */
					sph_spring_add(psys[0], &temp_spring);
				}
			}
			else {/* PART_SPRING_HOOKES - Hooke's spring force */
				madd_v3_v3fl(force, vec, -10.f * spring_constant * (1.f - rij/h) * (rest_length - rij));
			}
		}
	}

	/* Artificial buoyancy force in negative gravity direction  */
	if (fluid->buoyancy > 0.f && gravity)
		madd_v3_v3fl(force, gravity, fluid->buoyancy * (density-rest_density));

	if (sphdata->pass == 0 && psys[0]->part->time_flag & PART_TIME_AUTOSF)
		sph_particle_courant(sphdata, &pfr);
	sphdata->pass++;
}

static void sphclassical_density_accum_cb(void *userdata, int index, const float co[3], float UNUSED(squared_dist))
{
	SPHRangeData *pfr = (SPHRangeData *)userdata;
	ParticleData *npa = pfr->npsys->particles + index;
	float q;
	float qfac = 21.0f / (256.f * (float)M_PI);
	float rij, rij_h;
	float vec[3];

	/* Exclude particles that are more than 2h away. Can't use squared_dist here
	 * because it is not accurate enough. Use current state, i.e. the output of
	 * basic_integrate() - z0r */
	sub_v3_v3v3(vec, npa->state.co, co);
	rij = len_v3(vec);
	rij_h = rij / pfr->h;
	if (rij_h > 2.0f)
		return;

	/* Smoothing factor. Utilise the Wendland kernel. gnuplot:
	 *     q1(x) = (2.0 - x)**4 * ( 1.0 + 2.0 * x)
	 *     plot [0:2] q1(x) */
	q  = qfac / pow3f(pfr->h) * pow4f(2.0f - rij_h) * ( 1.0f + 2.0f * rij_h);
	q *= pfr->npsys->part->mass;

	if (pfr->use_size)
		q *= pfr->pa->size;

	pfr->data[0] += q;
	pfr->data[1] += q / npa->sphdensity;
}

static void sphclassical_neighbour_accum_cb(void *userdata, int index, const float co[3], float UNUSED(squared_dist))
{
	SPHRangeData *pfr = (SPHRangeData *)userdata;
	ParticleData *npa = pfr->npsys->particles + index;
	float rij, rij_h;
	float vec[3];

	if (pfr->tot_neighbors >= SPH_NEIGHBORS)
		return;

	/* Exclude particles that are more than 2h away. Can't use squared_dist here
	 * because it is not accurate enough. Use current state, i.e. the output of
	 * basic_integrate() - z0r */
	sub_v3_v3v3(vec, npa->state.co, co);
	rij = len_v3(vec);
	rij_h = rij / pfr->h;
	if (rij_h > 2.0f)
		return;

	pfr->neighbors[pfr->tot_neighbors].index = index;
	pfr->neighbors[pfr->tot_neighbors].psys = pfr->npsys;
	pfr->tot_neighbors++;
}
static void sphclassical_force_cb(void *sphdata_v, ParticleKey *state, float *force, float *UNUSED(impulse))
{
	SPHData *sphdata = (SPHData *)sphdata_v;
	ParticleSystem **psys = sphdata->psys;
	ParticleData *pa = sphdata->pa;
	SPHFluidSettings *fluid = psys[0]->part->fluid;
	SPHRangeData pfr;
	SPHNeighbor *pfn;
	float *gravity = sphdata->gravity;

	float dq, u, rij, dv[3];
	float pressure, npressure;

	float visc = fluid->viscosity_omega;

	float interaction_radius;
	float h, hinv;
	/* 4.77 is an experimentally determined density factor */
	float rest_density = fluid->rest_density * (fluid->flag & SPH_FAC_DENSITY ? 4.77f : 1.0f);

	// Use speed of sound squared
	float stiffness = pow2f(fluid->stiffness_k);

	ParticleData *npa;
	float vec[3];
	float co[3];
	float pressureTerm;

	int i;

	float qfac2 = 42.0f / (256.0f * (float)M_PI);
	float rij_h;

	/* 4.0 here is to be consistent with previous formulation/interface */
	interaction_radius = fluid->radius * (fluid->flag & SPH_FAC_RADIUS ? 4.0f * pa->size : 1.0f);
	h = interaction_radius * sphdata->hfac;
	hinv = 1.0f / h;

	pfr.h = h;
	pfr.pa = pa;

	sph_evaluate_func(NULL, psys, state->co, &pfr, interaction_radius, sphclassical_neighbour_accum_cb);
	pressure =  stiffness * (pow7f(pa->sphdensity / rest_density) - 1.0f);

	/* multiply by mass so that we return a force, not accel */
	qfac2 *= sphdata->mass / pow3f(pfr.h);

	pfn = pfr.neighbors;
	for (i = 0; i < pfr.tot_neighbors; i++, pfn++) {
		npa = pfn->psys->particles + pfn->index;
		if (npa == pa) {
			/* we do not contribute to ourselves */
			continue;
		}

		/* Find vector to neighbor. Exclude particles that are more than 2h
		 * away. Can't use current state here because it may have changed on
		 * another thread - so do own mini integration. Unlike basic_integrate,
		 * SPH integration depends on neighboring particles. - z0r */
		madd_v3_v3v3fl(co, npa->prev_state.co, npa->prev_state.vel, state->time);
		sub_v3_v3v3(vec, co, state->co);
		rij = normalize_v3(vec);
		rij_h = rij / pfr.h;
		if (rij_h > 2.0f)
			continue;

		npressure = stiffness * (pow7f(npa->sphdensity / rest_density) - 1.0f);

		/* First derivative of smoothing factor. Utilise the Wendland kernel.
		 * gnuplot:
		 *     q2(x) = 2.0 * (2.0 - x)**4 - 4.0 * (2.0 - x)**3 * (1.0 + 2.0 * x)
		 *     plot [0:2] q2(x)
		 * Particles > 2h away are excluded above. */
		dq = qfac2 * (2.0f * pow4f(2.0f - rij_h) - 4.0f * pow3f(2.0f - rij_h) * (1.0f + 2.0f * rij_h)  );

		if (pfn->psys->part->flag & PART_SIZEMASS)
			dq *= npa->size;

		pressureTerm = pressure / pow2f(pa->sphdensity) + npressure / pow2f(npa->sphdensity);

		/* Note that 'minus' is removed, because vec = vecBA, not vecAB.
		 * This applies to the viscosity calculation below, too. */
		madd_v3_v3fl(force, vec, pressureTerm * dq);

		/* Viscosity */
		if (visc > 0.0f) {
			sub_v3_v3v3(dv, npa->prev_state.vel, pa->prev_state.vel);
			u = dot_v3v3(vec, dv);
			/* Apply parameters */
			u *= -dq * hinv * visc / (0.5f * npa->sphdensity + 0.5f * pa->sphdensity);
			madd_v3_v3fl(force, vec, u);
		}
	}

	/* Artificial buoyancy force in negative gravity direction  */
	if (fluid->buoyancy > 0.f && gravity)
		madd_v3_v3fl(force, gravity, fluid->buoyancy * (pa->sphdensity - rest_density));

	if (sphdata->pass == 0 && psys[0]->part->time_flag & PART_TIME_AUTOSF)
		sph_particle_courant(sphdata, &pfr);
	sphdata->pass++;
}

static void sphclassical_calc_dens(ParticleData *pa, float UNUSED(dfra), SPHData *sphdata)
{
	ParticleSystem **psys = sphdata->psys;
	SPHFluidSettings *fluid = psys[0]->part->fluid;
	/* 4.0 seems to be a pretty good value */
	float interaction_radius  = fluid->radius * (fluid->flag & SPH_FAC_RADIUS ? 4.0f * psys[0]->part->size : 1.0f);
	SPHRangeData pfr;
	float data[2];

	data[0] = 0;
	data[1] = 0;
	pfr.data = data;
	pfr.h = interaction_radius * sphdata->hfac;
	pfr.pa = pa;
	pfr.mass = sphdata->mass;

	sph_evaluate_func( NULL, psys, pa->state.co, &pfr, interaction_radius, sphclassical_density_accum_cb);
	pa->sphdensity = min_ff(max_ff(data[0], fluid->rest_density * 0.9f), fluid->rest_density * 1.1f);
}

void psys_sph_init(ParticleSimulationData *sim, SPHData *sphdata)
{
	ParticleTarget *pt;
	int i;

	// Add other coupled particle systems.
	sphdata->psys[0] = sim->psys;
	for (i=1, pt=sim->psys->targets.first; i<10; i++, pt=(pt?pt->next:NULL))
		sphdata->psys[i] = pt ? psys_get_target_system(sim->ob, pt) : NULL;

	if (psys_uses_gravity(sim))
		sphdata->gravity = sim->scene->physics_settings.gravity;
	else
		sphdata->gravity = NULL;
	sphdata->eh = sph_springhash_build(sim->psys);

	// These per-particle values should be overridden later, but just for
	// completeness we give them default values now.
	sphdata->pa = NULL;
	sphdata->mass = 1.0f;

	if (sim->psys->part->fluid->solver == SPH_SOLVER_DDR) {
		sphdata->force_cb = sph_force_cb;
		sphdata->density_cb = sph_density_accum_cb;
		sphdata->hfac = 1.0f;
	}
	else {
		/* SPH_SOLVER_CLASSICAL */
		sphdata->force_cb = sphclassical_force_cb;
		sphdata->density_cb = sphclassical_density_accum_cb;
		sphdata->hfac = 0.5f;
	}

}

void psys_sph_finalise(SPHData *sphdata)
{
	if (sphdata->eh) {
		BLI_edgehash_free(sphdata->eh, NULL);
		sphdata->eh = NULL;
	}
}
/* Sample the density field at a point in space. */
void psys_sph_density(BVHTree *tree, SPHData *sphdata, float co[3], float vars[2])
{
	ParticleSystem **psys = sphdata->psys;
	SPHFluidSettings *fluid = psys[0]->part->fluid;
	/* 4.0 seems to be a pretty good value */
	float interaction_radius  = fluid->radius * (fluid->flag & SPH_FAC_RADIUS ? 4.0f * psys[0]->part->size : 1.0f);
	SPHRangeData pfr;
	float density[2];

	density[0] = density[1] = 0.0f;
	pfr.data = density;
	pfr.h = interaction_radius * sphdata->hfac;
	pfr.mass = sphdata->mass;

	sph_evaluate_func(tree, psys, co, &pfr, interaction_radius, sphdata->density_cb);

	vars[0] = pfr.data[0];
	vars[1] = pfr.data[1];
}

static void sph_integrate(ParticleSimulationData *sim, ParticleData *pa, float dfra, SPHData *sphdata)
{
	ParticleSettings *part = sim->psys->part;
	// float timestep = psys_get_timestep(sim); // UNUSED
	float pa_mass = part->mass * (part->flag & PART_SIZEMASS ? pa->size : 1.f);
	float dtime = dfra*psys_get_timestep(sim);
	// int steps = 1; // UNUSED
	float effector_acceleration[3];

	sphdata->pa = pa;
	sphdata->mass = pa_mass;
	sphdata->pass = 0;
	//sphdata.element_size and sphdata.flow are set in the callback.

	/* restore previous state and treat gravity & effectors as external acceleration*/
	sub_v3_v3v3(effector_acceleration, pa->state.vel, pa->prev_state.vel);
	mul_v3_fl(effector_acceleration, 1.f/dtime);

	copy_particle_key(&pa->state, &pa->prev_state, 0);

	integrate_particle(part, pa, dtime, effector_acceleration, sphdata->force_cb, sphdata);
}

/************************************************/
/*			Basic physics						*/
/************************************************/
typedef struct EfData {
	ParticleTexture ptex;
	ParticleSimulationData *sim;
	ParticleData *pa;
} EfData;
static void basic_force_cb(void *efdata_v, ParticleKey *state, float *force, float *impulse)
{
	EfData *efdata = (EfData *)efdata_v;
	ParticleSimulationData *sim = efdata->sim;
	ParticleSettings *part = sim->psys->part;
	ParticleData *pa = efdata->pa;
	EffectedPoint epoint;

	/* add effectors */
	pd_point_from_particle(efdata->sim, efdata->pa, state, &epoint);
	if (part->type != PART_HAIR || part->effector_weights->flag & EFF_WEIGHT_DO_HAIR)
		pdDoEffectors(sim->psys->effectors, sim->colliders, part->effector_weights, &epoint, force, impulse);

	mul_v3_fl(force, efdata->ptex.field);
	mul_v3_fl(impulse, efdata->ptex.field);

	/* calculate air-particle interaction */
	if (part->dragfac != 0.0f)
		madd_v3_v3fl(force, state->vel, -part->dragfac * pa->size * pa->size * len_v3(state->vel));

	/* brownian force */
	if (part->brownfac != 0.0f) {
		force[0] += (BLI_frand()-0.5f) * part->brownfac;
		force[1] += (BLI_frand()-0.5f) * part->brownfac;
		force[2] += (BLI_frand()-0.5f) * part->brownfac;
	}

	if (part->flag & PART_ROT_DYN && epoint.ave)
		copy_v3_v3(pa->state.ave, epoint.ave);
}
/* gathers all forces that effect particles and calculates a new state for the particle */
static void basic_integrate(ParticleSimulationData *sim, int p, float dfra, float cfra)
{
	ParticleSettings *part = sim->psys->part;
	ParticleData *pa = sim->psys->particles + p;
	ParticleKey tkey;
	float dtime=dfra*psys_get_timestep(sim), time;
	float *gravity = NULL, gr[3];
	EfData efdata;

	psys_get_texture(sim, pa, &efdata.ptex, PAMAP_PHYSICS, cfra);

	efdata.pa = pa;
	efdata.sim = sim;

	/* add global acceleration (gravitation) */
	if (psys_uses_gravity(sim) &&
		/* normal gravity is too strong for hair so it's disabled by default */
		(part->type != PART_HAIR || part->effector_weights->flag & EFF_WEIGHT_DO_HAIR))
	{
		zero_v3(gr);
		madd_v3_v3fl(gr, sim->scene->physics_settings.gravity, part->effector_weights->global_gravity * efdata.ptex.gravity);
		gravity = gr;
	}

	/* maintain angular velocity */
	copy_v3_v3(pa->state.ave, pa->prev_state.ave);

	integrate_particle(part, pa, dtime, gravity, basic_force_cb, &efdata);

	/* damp affects final velocity */
	if (part->dampfac != 0.f)
		mul_v3_fl(pa->state.vel, 1.f - part->dampfac * efdata.ptex.damp * 25.f * dtime);

	//copy_v3_v3(pa->state.ave, states->ave);

	/* finally we do guides */
	time=(cfra-pa->time)/pa->lifetime;
	CLAMP(time, 0.0f, 1.0f);

	copy_v3_v3(tkey.co,pa->state.co);
	copy_v3_v3(tkey.vel,pa->state.vel);
	tkey.time=pa->state.time;

	if (part->type != PART_HAIR) {
		if (do_guides(sim->psys->part, sim->psys->effectors, &tkey, p, time)) {
			copy_v3_v3(pa->state.co,tkey.co);
			/* guides don't produce valid velocity */
			sub_v3_v3v3(pa->state.vel, tkey.co, pa->prev_state.co);
			mul_v3_fl(pa->state.vel,1.0f/dtime);
			pa->state.time=tkey.time;
		}
	}
}
static void basic_rotate(ParticleSettings *part, ParticleData *pa, float dfra, float timestep)
{
	float rotfac, rot1[4], rot2[4] = {1.0,0.0,0.0,0.0}, dtime=dfra*timestep, extrotfac;

	if ((part->flag & PART_ROTATIONS) == 0) {
		unit_qt(pa->state.rot);
		return;
	}

	if (part->flag & PART_ROT_DYN) {
		extrotfac = len_v3(pa->state.ave);
	}
	else {
		extrotfac = 0.0f;
	}

	if ((part->flag & PART_ROT_DYN) && ELEM(part->avemode, PART_AVE_VELOCITY, PART_AVE_HORIZONTAL, PART_AVE_VERTICAL)) {
		float angle;
		float len1 = len_v3(pa->prev_state.vel);
		float len2 = len_v3(pa->state.vel);
		float vec[3];

		if (len1 == 0.0f || len2 == 0.0f) {
			zero_v3(pa->state.ave);
		}
		else {
			cross_v3_v3v3(pa->state.ave, pa->prev_state.vel, pa->state.vel);
			normalize_v3(pa->state.ave);
			angle = dot_v3v3(pa->prev_state.vel, pa->state.vel) / (len1 * len2);
			mul_v3_fl(pa->state.ave, saacos(angle) / dtime);
		}

		get_angular_velocity_vector(part->avemode, &pa->state, vec);
		axis_angle_to_quat(rot2, vec, dtime*part->avefac);
	}

	rotfac = len_v3(pa->state.ave);
	if (rotfac == 0.0f || (part->flag & PART_ROT_DYN)==0 || extrotfac == 0.0f) {
		unit_qt(rot1);
	}
	else {
		axis_angle_to_quat(rot1,pa->state.ave,rotfac*dtime);
	}
	mul_qt_qtqt(pa->state.rot,rot1,pa->prev_state.rot);
	mul_qt_qtqt(pa->state.rot,rot2,pa->state.rot);

	/* keep rotation quat in good health */
	normalize_qt(pa->state.rot);
}

/************************************************
 *			Collisions
 *
 * The algorithm is roughly:
 *  1. Use a BVH tree to search for faces that a particle may collide with.
 *  2. Use Newton's method to find the exact time at which the collision occurs.
 *     https://en.wikipedia.org/wiki/Newton's_method
 *
 ************************************************/
#define COLLISION_MIN_RADIUS 0.001f
#define COLLISION_MIN_DISTANCE 0.0001f
#define COLLISION_ZERO 0.00001f
#define COLLISION_INIT_STEP 0.00008f
typedef float (*NRDistanceFunc)(float *p, float radius, ParticleCollisionElement *pce, float *nor);
static float nr_signed_distance_to_plane(float *p, float radius, ParticleCollisionElement *pce, float *nor)
{
	float p0[3], e1[3], e2[3], d;

	sub_v3_v3v3(e1, pce->x1, pce->x0);
	sub_v3_v3v3(e2, pce->x2, pce->x0);
	sub_v3_v3v3(p0, p, pce->x0);

	cross_v3_v3v3(nor, e1, e2);
	normalize_v3(nor);

	d = dot_v3v3(p0, nor);

	if (pce->inv_nor == -1) {
		if (d < 0.f)
			pce->inv_nor = 1;
		else
			pce->inv_nor = 0;
	}

	if (pce->inv_nor == 1) {
		negate_v3(nor);
		d = -d;
	}

	return d - radius;
}
static float nr_distance_to_edge(float *p, float radius, ParticleCollisionElement *pce, float *UNUSED(nor))
{
	float v0[3], v1[3], v2[3], c[3];

	sub_v3_v3v3(v0, pce->x1, pce->x0);
	sub_v3_v3v3(v1, p, pce->x0);
	sub_v3_v3v3(v2, p, pce->x1);

	cross_v3_v3v3(c, v1, v2);

	return fabsf(len_v3(c)/len_v3(v0)) - radius;
}
static float nr_distance_to_vert(float *p, float radius, ParticleCollisionElement *pce, float *UNUSED(nor))
{
	return len_v3v3(p, pce->x0) - radius;
}
static void collision_interpolate_element(ParticleCollisionElement *pce, float t, float fac, ParticleCollision *col)
{
	/* t is the current time for newton rhapson */
	/* fac is the starting factor for current collision iteration */
	/* the col->fac's are factors for the particle subframe step start and end during collision modifier step */
	float f = fac + t*(1.f-fac);
	float mul = col->fac1 + f * (col->fac2-col->fac1);
	if (pce->tot > 0) {
		madd_v3_v3v3fl(pce->x0, pce->x[0], pce->v[0], mul);

		if (pce->tot > 1) {
			madd_v3_v3v3fl(pce->x1, pce->x[1], pce->v[1], mul);

			if (pce->tot > 2)
				madd_v3_v3v3fl(pce->x2, pce->x[2], pce->v[2], mul);
		}
	}
}
static void collision_point_velocity(ParticleCollisionElement *pce)
{
	float v[3];

	copy_v3_v3(pce->vel, pce->v[0]);

	if (pce->tot > 1) {
		sub_v3_v3v3(v, pce->v[1], pce->v[0]);
		madd_v3_v3fl(pce->vel, v, pce->uv[0]);

		if (pce->tot > 2) {
			sub_v3_v3v3(v, pce->v[2], pce->v[0]);
			madd_v3_v3fl(pce->vel, v, pce->uv[1]);
		}
	}
}
static float collision_point_distance_with_normal(float p[3], ParticleCollisionElement *pce, float fac, ParticleCollision *col, float *nor)
{
	if (fac >= 0.f)
		collision_interpolate_element(pce, 0.f, fac, col);

	switch (pce->tot) {
		case 1:
		{
			sub_v3_v3v3(nor, p, pce->x0);
			return normalize_v3(nor);
		}
		case 2:
		{
			float u, e[3], vec[3];
			sub_v3_v3v3(e, pce->x1, pce->x0);
			sub_v3_v3v3(vec, p, pce->x0);
			u = dot_v3v3(vec, e) / dot_v3v3(e, e);

			madd_v3_v3v3fl(nor, vec, e, -u);
			return normalize_v3(nor);
		}
		case 3:
			return nr_signed_distance_to_plane(p, 0.f, pce, nor);
	}
	return 0;
}
static void collision_point_on_surface(float p[3], ParticleCollisionElement *pce, float fac, ParticleCollision *col, float *co)
{
	collision_interpolate_element(pce, 0.f, fac, col);

	switch (pce->tot) {
		case 1:
		{
			sub_v3_v3v3(co, p, pce->x0);
			normalize_v3(co);
			madd_v3_v3v3fl(co, pce->x0, co, col->radius);
			break;
		}
		case 2:
		{
			float u, e[3], vec[3], nor[3];
			sub_v3_v3v3(e, pce->x1, pce->x0);
			sub_v3_v3v3(vec, p, pce->x0);
			u = dot_v3v3(vec, e) / dot_v3v3(e, e);

			madd_v3_v3v3fl(nor, vec, e, -u);
			normalize_v3(nor);

			madd_v3_v3v3fl(co, pce->x0, e, pce->uv[0]);
			madd_v3_v3fl(co, nor, col->radius);
			break;
		}
		case 3:
		{
			float p0[3], e1[3], e2[3], nor[3];

			sub_v3_v3v3(e1, pce->x1, pce->x0);
			sub_v3_v3v3(e2, pce->x2, pce->x0);
			sub_v3_v3v3(p0, p, pce->x0);

			cross_v3_v3v3(nor, e1, e2);
			normalize_v3(nor);

			if (pce->inv_nor == 1)
				negate_v3(nor);

			madd_v3_v3v3fl(co, pce->x0, nor, col->radius);
			madd_v3_v3fl(co, e1, pce->uv[0]);
			madd_v3_v3fl(co, e2, pce->uv[1]);
			break;
		}
	}
}
/* find first root in range [0-1] starting from 0 */
static float collision_newton_rhapson(ParticleCollision *col, float radius, ParticleCollisionElement *pce, NRDistanceFunc distance_func)
{
	float t0, t1, dt_init, d0, d1, dd, n[3];
	int iter;

	pce->inv_nor = -1;

	if (col->inv_total_time > 0.0f) {
		/* Initial step size should be small, but not too small or floating point
		 * precision errors will appear. - z0r */
		dt_init = COLLISION_INIT_STEP * col->inv_total_time;
	}
	else {
		dt_init = 0.001f;
	}

	/* start from the beginning */
	t0 = 0.f;
	collision_interpolate_element(pce, t0, col->f, col);
	d0 = distance_func(col->co1, radius, pce, n);
	t1 = dt_init;
	d1 = 0.f;

	for (iter=0; iter<10; iter++) {//, itersum++) {
		/* get current location */
		collision_interpolate_element(pce, t1, col->f, col);
		interp_v3_v3v3(pce->p, col->co1, col->co2, t1);

		d1 = distance_func(pce->p, radius, pce, n);

		/* particle already inside face, so report collision */
		if (iter == 0 && d0 < 0.f && d0 > -radius) {
			copy_v3_v3(pce->p, col->co1);
			copy_v3_v3(pce->nor, n);
			pce->inside = 1;
			return 0.f;
		}

		/* Zero gradient (no movement relative to element). Can't step from
		 * here. */
		if (d1 == d0) {
			/* If first iteration, try from other end where the gradient may be
			 * greater. Note: code duplicated below. */
			if (iter == 0) {
				t0 = 1.f;
				collision_interpolate_element(pce, t0, col->f, col);
				d0 = distance_func(col->co2, radius, pce, n);
				t1 = 1.0f - dt_init;
				d1 = 0.f;
				continue;
			}
			else
				return -1.f;
		}

		dd = (t1-t0)/(d1-d0);

		t0 = t1;
		d0 = d1;

		t1 -= d1*dd;

		/* Particle moving away from plane could also mean a strangely rotating
		 * face, so check from end. Note: code duplicated above. */
		if (iter == 0 && t1 < 0.f) {
			t0 = 1.f;
			collision_interpolate_element(pce, t0, col->f, col);
			d0 = distance_func(col->co2, radius, pce, n);
			t1 = 1.0f - dt_init;
			d1 = 0.f;
			continue;
		}
		else if (iter == 1 && (t1 < -COLLISION_ZERO || t1 > 1.f))
			return -1.f;

		if (d1 <= COLLISION_ZERO && d1 >= -COLLISION_ZERO) {
			if (t1 >= -COLLISION_ZERO && t1 <= 1.f) {
				if (distance_func == nr_signed_distance_to_plane)
					copy_v3_v3(pce->nor, n);

				CLAMP(t1, 0.f, 1.f);

				return t1;
			}
			else
				return -1.f;
		}
	}
	return -1.0;
}
static int collision_sphere_to_tri(ParticleCollision *col, float radius, ParticleCollisionElement *pce, float *t)
{
	ParticleCollisionElement *result = &col->pce;
	float ct, u, v;

	pce->inv_nor = -1;
	pce->inside = 0;

	ct = collision_newton_rhapson(col, radius, pce, nr_signed_distance_to_plane);

	if (ct >= 0.f && ct < *t && (result->inside==0 || pce->inside==1) ) {
		float e1[3], e2[3], p0[3];
		float e1e1, e1e2, e1p0, e2e2, e2p0, inv;

		sub_v3_v3v3(e1, pce->x1, pce->x0);
		sub_v3_v3v3(e2, pce->x2, pce->x0);
		/* XXX: add radius correction here? */
		sub_v3_v3v3(p0, pce->p, pce->x0);

		e1e1 = dot_v3v3(e1, e1);
		e1e2 = dot_v3v3(e1, e2);
		e1p0 = dot_v3v3(e1, p0);
		e2e2 = dot_v3v3(e2, e2);
		e2p0 = dot_v3v3(e2, p0);

		inv = 1.f/(e1e1 * e2e2 - e1e2 * e1e2);
		u = (e2e2 * e1p0 - e1e2 * e2p0) * inv;
		v = (e1e1 * e2p0 - e1e2 * e1p0) * inv;

		if (u>=0.f && u<=1.f && v>=0.f && u+v<=1.f) {
			*result = *pce;

			/* normal already calculated in pce */

			result->uv[0] = u;
			result->uv[1] = v;

			*t = ct;
			return 1;
		}
	}
	return 0;
}
static int collision_sphere_to_edges(ParticleCollision *col, float radius, ParticleCollisionElement *pce, float *t)
{
	ParticleCollisionElement edge[3], *cur = NULL, *hit = NULL;
	ParticleCollisionElement *result = &col->pce;

	float ct;
	int i;

	for (i=0; i<3; i++) {
		cur = edge+i;
		cur->x[0] = pce->x[i]; cur->x[1] = pce->x[(i+1)%3];
		cur->v[0] = pce->v[i]; cur->v[1] = pce->v[(i+1)%3];
		cur->tot = 2;
		cur->inside = 0;

		ct = collision_newton_rhapson(col, radius, cur, nr_distance_to_edge);

		if (ct >= 0.f && ct < *t) {
			float u, e[3], vec[3];

			sub_v3_v3v3(e, cur->x1, cur->x0);
			sub_v3_v3v3(vec, cur->p, cur->x0);
			u = dot_v3v3(vec, e) / dot_v3v3(e, e);

			if (u < 0.f || u > 1.f)
				break;

			*result = *cur;

			madd_v3_v3v3fl(result->nor, vec, e, -u);
			normalize_v3(result->nor);

			result->uv[0] = u;


			hit = cur;
			*t = ct;
		}

	}

	return hit != NULL;
}
static int collision_sphere_to_verts(ParticleCollision *col, float radius, ParticleCollisionElement *pce, float *t)
{
	ParticleCollisionElement vert[3], *cur = NULL, *hit = NULL;
	ParticleCollisionElement *result = &col->pce;

	float ct;
	int i;

	for (i=0; i<3; i++) {
		cur = vert+i;
		cur->x[0] = pce->x[i];
		cur->v[0] = pce->v[i];
		cur->tot = 1;
		cur->inside = 0;

		ct = collision_newton_rhapson(col, radius, cur, nr_distance_to_vert);

		if (ct >= 0.f && ct < *t) {
			*result = *cur;

			sub_v3_v3v3(result->nor, cur->p, cur->x0);
			normalize_v3(result->nor);

			hit = cur;
			*t = ct;
		}

	}

	return hit != NULL;
}
/* Callback for BVHTree near test */
void BKE_psys_collision_neartest_cb(void *userdata, int index, const BVHTreeRay *ray, BVHTreeRayHit *hit)
{
	ParticleCollision *col = (ParticleCollision *) userdata;
	ParticleCollisionElement pce;
	const MVertTri *vt = &col->md->tri[index];
	MVert *x = col->md->x;
	MVert *v = col->md->current_v;
	float t = hit->dist/col->original_ray_length;
	int collision = 0;

	pce.x[0] = x[vt->tri[0]].co;
	pce.x[1] = x[vt->tri[1]].co;
	pce.x[2] = x[vt->tri[2]].co;

	pce.v[0] = v[vt->tri[0]].co;
	pce.v[1] = v[vt->tri[1]].co;
	pce.v[2] = v[vt->tri[2]].co;

	pce.tot = 3;
	pce.inside = 0;
	pce.index = index;

	collision = collision_sphere_to_tri(col, ray->radius, &pce, &t);
	if (col->pce.inside == 0) {
		collision += collision_sphere_to_edges(col, ray->radius, &pce, &t);
		collision += collision_sphere_to_verts(col, ray->radius, &pce, &t);
	}

	if (collision) {
		hit->dist = col->original_ray_length * t;
		hit->index = index;

		collision_point_velocity(&col->pce);

		col->hit = col->current;
	}
}
static int collision_detect(ParticleData *pa, ParticleCollision *col, BVHTreeRayHit *hit, ListBase *colliders)
{
	const int raycast_flag = BVH_RAYCAST_DEFAULT & ~(BVH_RAYCAST_WATERTIGHT);
	ColliderCache *coll;
	float ray_dir[3];

	if (BLI_listbase_is_empty(colliders))
		return 0;

	sub_v3_v3v3(ray_dir, col->co2, col->co1);
	hit->index = -1;
	hit->dist = col->original_ray_length = normalize_v3(ray_dir);
	col->pce.inside = 0;

	/* even if particle is stationary we want to check for moving colliders */
	/* if hit.dist is zero the bvhtree_ray_cast will just ignore everything */
	if (hit->dist == 0.0f)
		hit->dist = col->original_ray_length = 0.000001f;

	for (coll = colliders->first; coll; coll=coll->next) {
		/* for boids: don't check with current ground object; also skip if permeated */
		bool skip = false;

		for (int i = 0; i < col->skip_count; i++) {
			if (coll->ob == col->skip[i]) {
				skip = true;
				break;
			}
		}

		if (skip)
			continue;

		/* particles should not collide with emitter at birth */
		if (coll->ob == col->emitter && pa->time < col->cfra && pa->time >= col->old_cfra)
			continue;

		col->current = coll->ob;
		col->md = coll->collmd;
		col->fac1 = (col->old_cfra - coll->collmd->time_x) / (coll->collmd->time_xnew - coll->collmd->time_x);
		col->fac2 = (col->cfra - coll->collmd->time_x) / (coll->collmd->time_xnew - coll->collmd->time_x);

		if (col->md && col->md->bvhtree) {
			BLI_bvhtree_ray_cast_ex(
			        col->md->bvhtree, col->co1, ray_dir, col->radius, hit,
			        BKE_psys_collision_neartest_cb, col, raycast_flag);
		}
	}

	return hit->index >= 0;
}
static int collision_response(ParticleData *pa, ParticleCollision *col, BVHTreeRayHit *hit, int kill, int dynamic_rotation)
{
	ParticleCollisionElement *pce = &col->pce;
	PartDeflect *pd = col->hit->pd;
	float co[3];										/* point of collision */
	float x = hit->dist/col->original_ray_length;		/* location factor of collision between this iteration */
	float f = col->f + x * (1.0f - col->f);				/* time factor of collision between timestep */
	float dt1 = (f - col->f) * col->total_time;			/* time since previous collision (in seconds) */
	float dt2 = (1.0f - f) * col->total_time;			/* time left after collision (in seconds) */
	int through = (BLI_frand() < pd->pdef_perm) ? 1 : 0; /* did particle pass through the collision surface? */

	/* calculate exact collision location */
	interp_v3_v3v3(co, col->co1, col->co2, x);

	/* particle dies in collision */
	if (through == 0 && (kill || pd->flag & PDEFLE_KILL_PART)) {
		pa->alive = PARS_DYING;
		pa->dietime = col->old_cfra + (col->cfra - col->old_cfra) * f;

		copy_v3_v3(pa->state.co, co);
		interp_v3_v3v3(pa->state.vel, pa->prev_state.vel, pa->state.vel, f);
		interp_qt_qtqt(pa->state.rot, pa->prev_state.rot, pa->state.rot, f);
		interp_v3_v3v3(pa->state.ave, pa->prev_state.ave, pa->state.ave, f);

		/* particle is dead so we don't need to calculate further */
		return 0;
	}
	/* figure out velocity and other data after collision */
	else {
		float v0[3];	/* velocity directly before collision to be modified into velocity directly after collision */
		float v0_nor[3];/* normal component of v0 */
		float v0_tan[3];/* tangential component of v0 */
		float vc_tan[3];/* tangential component of collision surface velocity */
		float v0_dot, vc_dot;
		float damp = pd->pdef_damp + pd->pdef_rdamp * 2 * (BLI_frand() - 0.5f);
		float frict = pd->pdef_frict + pd->pdef_rfrict * 2 * (BLI_frand() - 0.5f);
		float distance, nor[3], dot;

		CLAMP(damp,0.0f, 1.0f);
		CLAMP(frict,0.0f, 1.0f);

		/* get exact velocity right before collision */
		madd_v3_v3v3fl(v0, col->ve1, col->acc, dt1);

		/* convert collider velocity from 1/framestep to 1/s TODO: here we assume 1 frame step for collision modifier */
		mul_v3_fl(pce->vel, col->inv_timestep);

		/* calculate tangential particle velocity */
		v0_dot = dot_v3v3(pce->nor, v0);
		madd_v3_v3v3fl(v0_tan, v0, pce->nor, -v0_dot);

		/* calculate tangential collider velocity */
		vc_dot = dot_v3v3(pce->nor, pce->vel);
		madd_v3_v3v3fl(vc_tan, pce->vel, pce->nor, -vc_dot);

		/* handle friction effects (tangential and angular velocity) */
		if (frict > 0.0f) {
			/* angular <-> linear velocity */
			if (dynamic_rotation) {
				float vr_tan[3], v1_tan[3], ave[3];

				/* linear velocity of particle surface */
				cross_v3_v3v3(vr_tan, pce->nor, pa->state.ave);
				mul_v3_fl(vr_tan, pa->size);

				/* change to coordinates that move with the collision plane */
				sub_v3_v3v3(v1_tan, v0_tan, vc_tan);

				/* The resulting velocity is a weighted average of particle cm & surface
				 * velocity. This weight (related to particle's moment of inertia) could
				 * be made a parameter for angular <-> linear conversion.
				 */
				madd_v3_v3fl(v1_tan, vr_tan, -0.4);
				mul_v3_fl(v1_tan, 1.0f/1.4f); /* 1/(1+0.4) */

				/* rolling friction is around 0.01 of sliding friction (could be made a parameter) */
				mul_v3_fl(v1_tan, 1.0f - 0.01f * frict);

				/* surface_velocity is opposite to cm velocity */
				negate_v3_v3(vr_tan, v1_tan);

				/* get back to global coordinates */
				add_v3_v3(v1_tan, vc_tan);

				/* convert to angular velocity*/
				cross_v3_v3v3(ave, vr_tan, pce->nor);
				mul_v3_fl(ave, 1.0f/MAX2(pa->size, 0.001f));

				/* only friction will cause change in linear & angular velocity */
				interp_v3_v3v3(pa->state.ave, pa->state.ave, ave, frict);
				interp_v3_v3v3(v0_tan, v0_tan, v1_tan, frict);
			}
			else {
				/* just basic friction (unphysical due to the friction model used in Blender) */
				interp_v3_v3v3(v0_tan, v0_tan, vc_tan, frict);
			}
		}

		/* stickiness was possibly added before, so cancel that before calculating new normal velocity */
		/* otherwise particles go flying out of the surface because of high reversed sticky velocity */
		if (v0_dot < 0.0f) {
			v0_dot += pd->pdef_stickness;
			if (v0_dot > 0.0f)
				v0_dot = 0.0f;
		}

		/* damping and flipping of velocity around normal */
		v0_dot *= 1.0f - damp;
		vc_dot *= through ? damp : 1.0f;

		/* calculate normal particle velocity */
		/* special case for object hitting the particle from behind */
		if (through==0 && ((vc_dot>0.0f && v0_dot>0.0f && vc_dot>v0_dot) || (vc_dot<0.0f && v0_dot<0.0f && vc_dot<v0_dot)))
			mul_v3_v3fl(v0_nor, pce->nor, vc_dot);
		else if (v0_dot > 0.f)
			mul_v3_v3fl(v0_nor, pce->nor, vc_dot + v0_dot);
		else
			mul_v3_v3fl(v0_nor, pce->nor, vc_dot + (through ? 1.0f : -1.0f) * v0_dot);

		/* combine components together again */
		add_v3_v3v3(v0, v0_nor, v0_tan);

		if (col->boid) {
			/* keep boids above ground */
			BoidParticle *bpa = pa->boid;
			if (bpa->data.mode == eBoidMode_OnLand || co[2] <= col->boid_z) {
				co[2] = col->boid_z;
				v0[2] = 0.0f;
			}
		}

		/* re-apply acceleration to final location and velocity */
		madd_v3_v3v3fl(pa->state.co, co, v0, dt2);
		madd_v3_v3fl(pa->state.co, col->acc, 0.5f*dt2*dt2);
		madd_v3_v3v3fl(pa->state.vel, v0, col->acc, dt2);

		/* make sure particle stays on the right side of the surface */
		if (!through) {
			distance = collision_point_distance_with_normal(co, pce, -1.f, col, nor);

			if (distance < col->radius + COLLISION_MIN_DISTANCE)
				madd_v3_v3fl(co, nor, col->radius + COLLISION_MIN_DISTANCE - distance);

			dot = dot_v3v3(nor, v0);
			if (dot < 0.f)
				madd_v3_v3fl(v0, nor, -dot);

			distance = collision_point_distance_with_normal(pa->state.co, pce, 1.f, col, nor);

			if (distance < col->radius + COLLISION_MIN_DISTANCE)
				madd_v3_v3fl(pa->state.co, nor, col->radius + COLLISION_MIN_DISTANCE - distance);

			dot = dot_v3v3(nor, pa->state.vel);
			if (dot < 0.f)
				madd_v3_v3fl(pa->state.vel, nor, -dot);
		}

		/* add stickiness to surface */
		madd_v3_v3fl(pa->state.vel, pce->nor, -pd->pdef_stickness);

		/* set coordinates for next iteration */
		copy_v3_v3(col->co1, co);
		copy_v3_v3(col->co2, pa->state.co);

		copy_v3_v3(col->ve1, v0);
		copy_v3_v3(col->ve2, pa->state.vel);

		col->f = f;
	}

	/* if permeability random roll succeeded, disable collider for this sim step */
	if (through) {
		col->skip[col->skip_count++] = col->hit;
	}

	return 1;
}
static void collision_fail(ParticleData *pa, ParticleCollision *col)
{
	/* final chance to prevent total failure, so stick to the surface and hope for the best */
	collision_point_on_surface(col->co1, &col->pce, 1.f, col, pa->state.co);

	copy_v3_v3(pa->state.vel, col->pce.vel);
	mul_v3_fl(pa->state.vel, col->inv_timestep);


	/* printf("max iterations\n"); */
}

/* Particle - Mesh collision detection and response
 * Features:
 * -friction and damping
 * -angular momentum <-> linear momentum
 * -high accuracy by re-applying particle acceleration after collision
 * -handles moving, rotating and deforming meshes
 * -uses Newton-Rhapson iteration to find the collisions
 * -handles spherical particles and (nearly) point like particles
 */
static void collision_check(ParticleSimulationData *sim, int p, float dfra, float cfra)
{
	ParticleSettings *part = sim->psys->part;
	ParticleData *pa = sim->psys->particles + p;
	ParticleCollision col;
	BVHTreeRayHit hit;
	int collision_count=0;

	float timestep = psys_get_timestep(sim);

	memset(&col, 0, sizeof(ParticleCollision));

	col.total_time = timestep * dfra;
	col.inv_total_time = 1.0f/col.total_time;
	col.inv_timestep = 1.0f/timestep;

	col.cfra = cfra;
	col.old_cfra = sim->psys->cfra;

	/* get acceleration (from gravity, forcefields etc. to be re-applied in collision response) */
	sub_v3_v3v3(col.acc, pa->state.vel, pa->prev_state.vel);
	mul_v3_fl(col.acc, 1.f/col.total_time);

	/* set values for first iteration */
	copy_v3_v3(col.co1, pa->prev_state.co);
	copy_v3_v3(col.co2, pa->state.co);
	copy_v3_v3(col.ve1, pa->prev_state.vel);
	copy_v3_v3(col.ve2, pa->state.vel);
	col.f = 0.0f;

	col.radius = ((part->flag & PART_SIZE_DEFL) || (part->phystype == PART_PHYS_BOIDS)) ? pa->size : COLLISION_MIN_RADIUS;

	/* override for boids */
	if (part->phystype == PART_PHYS_BOIDS && part->boids->options & BOID_ALLOW_LAND) {
		col.boid = 1;
		col.boid_z = pa->state.co[2];
		col.skip[col.skip_count++] = pa->boid->ground;
	}

	/* 10 iterations to catch multiple collisions */
	while (collision_count < PARTICLE_COLLISION_MAX_COLLISIONS) {
		if (collision_detect(pa, &col, &hit, sim->colliders)) {

			collision_count++;

			if (collision_count == PARTICLE_COLLISION_MAX_COLLISIONS)
				collision_fail(pa, &col);
			else if (collision_response(pa, &col, &hit, part->flag & PART_DIE_ON_COL, part->flag & PART_ROT_DYN)==0)
				return;
		}
		else
			return;
	}
}
/************************************************/
/*			Hair								*/
/************************************************/
/* check if path cache or children need updating and do it if needed */
static void psys_update_path_cache(ParticleSimulationData *sim, float cfra, const bool use_render_params)
{
	ParticleSystem *psys = sim->psys;
	ParticleSettings *part = psys->part;
	ParticleEditSettings *pset = &sim->scene->toolsettings->particle;
	Base *base;
	int distr=0, alloc=0, skip=0;

	if ((psys->part->childtype && psys->totchild != psys_get_tot_child(sim->scene, psys)) || psys->recalc&PSYS_RECALC_RESET)
		alloc=1;

	if (alloc || psys->recalc&PSYS_RECALC_CHILD || (psys->vgroup[PSYS_VG_DENSITY] && (sim->ob && sim->ob->mode & OB_MODE_WEIGHT_PAINT)))
		distr=1;

	if (distr) {
		if (alloc)
			realloc_particles(sim, sim->psys->totpart);

		if (psys_get_tot_child(sim->scene, psys)) {
			/* don't generate children while computing the hair keys */
			if (!(psys->part->type == PART_HAIR) || (psys->flag & PSYS_HAIR_DONE)) {
				distribute_particles(sim, PART_FROM_CHILD);

				if (part->childtype==PART_CHILD_FACES && part->parents != 0.0f)
					psys_find_parents(sim, use_render_params);
			}
		}
		else
			psys_free_children(psys);
	}

	if ((part->type==PART_HAIR || psys->flag&PSYS_KEYED || psys->pointcache->flag & PTCACHE_BAKED)==0)
		skip = 1; /* only hair, keyed and baked stuff can have paths */
	else if (part->ren_as != PART_DRAW_PATH && !(part->type==PART_HAIR && ELEM(part->ren_as, PART_DRAW_OB, PART_DRAW_GR)))
		skip = 1; /* particle visualization must be set as path */
	else if (!psys->renderdata) {
		if (part->draw_as != PART_DRAW_REND)
			skip = 1; /* draw visualization */
		else if (psys->pointcache->flag & PTCACHE_BAKING)
			skip = 1; /* no need to cache paths while baking dynamics */
		else if (psys_in_edit_mode(sim->scene, psys)) {
			if ((pset->flag & PE_DRAW_PART)==0)
				skip = 1;
			else if (part->childtype==0 && (psys->flag & PSYS_HAIR_DYNAMICS && psys->pointcache->flag & PTCACHE_BAKED)==0)
				skip = 1; /* in edit mode paths are needed for child particles and dynamic hair */
		}
	}


	/* particle instance modifier with "path" option need cached paths even if particle system doesn't */
	for (base = sim->scene->base.first; base; base= base->next) {
		ModifierData *md = modifiers_findByType(base->object, eModifierType_ParticleInstance);
		if (md) {
			ParticleInstanceModifierData *pimd = (ParticleInstanceModifierData *)md;
			if (pimd->flag & eParticleInstanceFlag_Path && pimd->ob == sim->ob && pimd->psys == (psys - (ParticleSystem*)sim->ob->particlesystem.first)) {
				skip = 0;
				break;
			}
		}
	}

	if (!skip) {
		psys_cache_paths(sim, cfra, use_render_params);

		/* for render, child particle paths are computed on the fly */
		if (part->childtype) {
			if (!psys->totchild)
				skip = 1;
			else if (psys->part->type == PART_HAIR && (psys->flag & PSYS_HAIR_DONE)==0)
				skip = 1;

			if (!skip)
				psys_cache_child_paths(sim, cfra, 0, use_render_params);
		}
	}
	else if (psys->pathcache)
		psys_free_path_cache(psys, NULL);
}

static bool psys_hair_use_simulation(ParticleData *pa, float max_length)
{
	/* Minimum segment length relative to average length.
	 * Hairs with segments below this length will be excluded from the simulation,
	 * because otherwise the solver will become unstable.
	 * The hair system should always make sure the hair segments have reasonable length ratios,
	 * but this can happen in old files when e.g. cutting hair.
	 */
	const float min_length = 0.1f * max_length;

	HairKey *key;
	int k;

	if (pa->totkey < 2)
		return false;

	for (k=1, key=pa->hair+1; k<pa->totkey; k++,key++) {
		float length = len_v3v3(key->co, (key-1)->co);
		if (length < min_length)
			return false;
	}

	return true;
}

static MDeformVert *hair_set_pinning(MDeformVert *dvert, float weight)
{
	if (dvert) {
		if (!dvert->totweight) {
			dvert->dw = MEM_callocN(sizeof(MDeformWeight), "deformWeight");
			dvert->totweight = 1;
		}

		dvert->dw->weight = weight;
		dvert++;
	}
	return dvert;
}

static void hair_create_input_dm(ParticleSimulationData *sim, int totpoint, int totedge, DerivedMesh **r_dm, ClothHairData **r_hairdata)
{
	ParticleSystem *psys = sim->psys;
	ParticleSettings *part = psys->part;
	DerivedMesh *dm;
	ClothHairData *hairdata;
	MVert *mvert;
	MEdge *medge;
	MDeformVert *dvert;
	HairKey *key;
	PARTICLE_P;
	int k, hair_index;
	float hairmat[4][4];
	float max_length;
	float hair_radius;

	dm = *r_dm;
	if (!dm) {
		*r_dm = dm = CDDM_new(totpoint, totedge, 0, 0, 0);
		DM_add_vert_layer(dm, CD_MDEFORMVERT, CD_CALLOC, NULL);
	}
	mvert = CDDM_get_verts(dm);
	medge = CDDM_get_edges(dm);
	dvert = DM_get_vert_data_layer(dm, CD_MDEFORMVERT);

	hairdata = *r_hairdata;
	if (!hairdata) {
		*r_hairdata = hairdata = MEM_mallocN(sizeof(ClothHairData) * totpoint, "hair data");
	}

	/* calculate maximum segment length */
	max_length = 0.0f;
	LOOP_PARTICLES {
		if (!(pa->flag & PARS_UNEXIST)) {
			for (k=1, key=pa->hair+1; k<pa->totkey; k++,key++) {
				float length = len_v3v3(key->co, (key-1)->co);
				if (max_length < length)
					max_length = length;
			}
		}
	}

	psys->clmd->sim_parms->vgroup_mass = 1;

	/* XXX placeholder for more flexible future hair settings */
	hair_radius = part->size;

	/* make vgroup for pin roots etc.. */
	hair_index = 1;
	LOOP_PARTICLES {
		if (!(pa->flag & PARS_UNEXIST)) {
			float root_mat[4][4];
			float bending_stiffness;
			bool use_hair;

			pa->hair_index = hair_index;
			use_hair = psys_hair_use_simulation(pa, max_length);

			psys_mat_hair_to_object(sim->ob, sim->psmd->dm_final, psys->part->from, pa, hairmat);
			mul_m4_m4m4(root_mat, sim->ob->obmat, hairmat);
			normalize_m4(root_mat);

			bending_stiffness = CLAMPIS(1.0f - part->bending_random * psys_frand(psys, p + 666), 0.0f, 1.0f);

			for (k=0, key=pa->hair; k<pa->totkey; k++,key++) {
				ClothHairData *hair;
				float *co, *co_next;

				co = key->co;
				co_next = (key+1)->co;

				/* create fake root before actual root to resist bending */
				if (k==0) {
					hair = &psys->clmd->hairdata[pa->hair_index - 1];
					copy_v3_v3(hair->loc, root_mat[3]);
					copy_m3_m4(hair->rot, root_mat);

					hair->radius = hair_radius;
					hair->bending_stiffness = bending_stiffness;

					add_v3_v3v3(mvert->co, co, co);
					sub_v3_v3(mvert->co, co_next);
					mul_m4_v3(hairmat, mvert->co);

					medge->v1 = pa->hair_index - 1;
					medge->v2 = pa->hair_index;

					dvert = hair_set_pinning(dvert, 1.0f);

					mvert++;
					medge++;
				}

				/* store root transform in cloth data */
				hair = &psys->clmd->hairdata[pa->hair_index + k];
				copy_v3_v3(hair->loc, root_mat[3]);
				copy_m3_m4(hair->rot, root_mat);

				hair->radius = hair_radius;
				hair->bending_stiffness = bending_stiffness;

				copy_v3_v3(mvert->co, co);
				mul_m4_v3(hairmat, mvert->co);

				if (k) {
					medge->v1 = pa->hair_index + k - 1;
					medge->v2 = pa->hair_index + k;
				}

				/* roots and disabled hairs should be 1.0, the rest can be anything from 0.0 to 1.0 */
				if (use_hair)
					dvert = hair_set_pinning(dvert, key->weight);
				else
					dvert = hair_set_pinning(dvert, 1.0f);

				mvert++;
				if (k)
					medge++;
			}

			hair_index += pa->totkey + 1;
		}
	}
}

static void do_hair_dynamics(ParticleSimulationData *sim)
{
	ParticleSystem *psys = sim->psys;
	PARTICLE_P;
	EffectorWeights *clmd_effweights;
	int totpoint;
	int totedge;
	float (*deformedVerts)[3];
	bool realloc_roots;

	if (!psys->clmd) {
		psys->clmd = (ClothModifierData*)modifier_new(eModifierType_Cloth);
		psys->clmd->sim_parms->goalspring = 0.0f;
		psys->clmd->sim_parms->vel_damping = 1.0f;
		psys->clmd->sim_parms->flags |= CLOTH_SIMSETTINGS_FLAG_GOAL|CLOTH_SIMSETTINGS_FLAG_NO_SPRING_COMPRESS;
		psys->clmd->coll_parms->flags &= ~CLOTH_COLLSETTINGS_FLAG_SELF;
	}

	/* count simulated points */
	totpoint = 0;
	totedge = 0;
	LOOP_PARTICLES {
		if (!(pa->flag & PARS_UNEXIST)) {
			/* "out" dm contains all hairs */
			totedge += pa->totkey;
			totpoint += pa->totkey + 1; /* +1 for virtual root point */
		}
	}

	realloc_roots = false; /* whether hair root info array has to be reallocated */
	if (psys->hair_in_dm) {
		DerivedMesh *dm = psys->hair_in_dm;
		if (totpoint != dm->getNumVerts(dm) || totedge != dm->getNumEdges(dm)) {
			dm->release(dm);
			psys->hair_in_dm = NULL;
			realloc_roots = true;
		}
	}

	if (!psys->hair_in_dm || !psys->clmd->hairdata || realloc_roots) {
		if (psys->clmd->hairdata) {
			MEM_freeN(psys->clmd->hairdata);
			psys->clmd->hairdata = NULL;
		}
	}

	hair_create_input_dm(sim, totpoint, totedge, &psys->hair_in_dm, &psys->clmd->hairdata);

	if (psys->hair_out_dm)
		psys->hair_out_dm->release(psys->hair_out_dm);

	psys->clmd->point_cache = psys->pointcache;
	/* for hair sim we replace the internal cloth effector weights temporarily
	 * to use the particle settings
	 */
	clmd_effweights = psys->clmd->sim_parms->effector_weights;
	psys->clmd->sim_parms->effector_weights = psys->part->effector_weights;

	deformedVerts = MEM_mallocN(sizeof(*deformedVerts) * psys->hair_in_dm->getNumVerts(psys->hair_in_dm), "do_hair_dynamics vertexCos");
	psys->hair_out_dm = CDDM_copy(psys->hair_in_dm);
	psys->hair_out_dm->getVertCos(psys->hair_out_dm, deformedVerts);

	clothModifier_do(psys->clmd, sim->scene, sim->ob, psys->hair_in_dm, deformedVerts);

	CDDM_apply_vert_coords(psys->hair_out_dm, deformedVerts);

	MEM_freeN(deformedVerts);

	/* restore cloth effector weights */
	psys->clmd->sim_parms->effector_weights = clmd_effweights;
}
static void hair_step(ParticleSimulationData *sim, float cfra, const bool use_render_params)
{
	ParticleSystem *psys = sim->psys;
	ParticleSettings *part = psys->part;
	PARTICLE_P;
	float disp = psys_get_current_display_percentage(psys);

	LOOP_PARTICLES {
		pa->size = part->size;
		if (part->randsize > 0.0f)
			pa->size *= 1.0f - part->randsize * psys_frand(psys, p + 1);

		if (psys_frand(psys, p) > disp)
			pa->flag |= PARS_NO_DISP;
		else
			pa->flag &= ~PARS_NO_DISP;
	}

	if (psys->recalc & PSYS_RECALC_RESET) {
		/* need this for changing subsurf levels */
		psys_calc_dmcache(sim->ob, sim->psmd->dm_final, sim->psmd->dm_deformed, psys);

		if (psys->clmd)
			cloth_free_modifier(psys->clmd);
	}

	/* dynamics with cloth simulation, psys->particles can be NULL with 0 particles [#25519] */
	if (psys->part->type==PART_HAIR && psys->flag & PSYS_HAIR_DYNAMICS && psys->particles)
		do_hair_dynamics(sim);

	/* following lines were removed r29079 but cause bug [#22811], see report for details */
	psys_update_effectors(sim);
	psys_update_path_cache(sim, cfra, use_render_params);

	psys->flag |= PSYS_HAIR_UPDATED;
}

static void save_hair(ParticleSimulationData *sim, float UNUSED(cfra))
{
	Object *ob = sim->ob;
	ParticleSystem *psys = sim->psys;
	HairKey *key, *root;
	PARTICLE_P;

	invert_m4_m4(ob->imat, ob->obmat);

	psys->lattice_deform_data= psys_create_lattice_deform_data(sim);

	if (psys->totpart==0) return;

	/* save new keys for elements if needed */
	LOOP_PARTICLES {
		/* first time alloc */
		if (pa->totkey==0 || pa->hair==NULL) {
			pa->hair = MEM_callocN((psys->part->hair_step + 1) * sizeof(HairKey), "HairKeys");
			pa->totkey = 0;
		}

		key = root = pa->hair;
		key += pa->totkey;

		/* convert from global to geometry space */
		copy_v3_v3(key->co, pa->state.co);
		mul_m4_v3(ob->imat, key->co);

		if (pa->totkey) {
			sub_v3_v3(key->co, root->co);
			psys_vec_rot_to_face(sim->psmd->dm_final, pa, key->co);
		}

		key->time = pa->state.time;

		key->weight = 1.0f - key->time / 100.0f;

		pa->totkey++;

		/* root is always in the origin of hair space so we set it to be so after the last key is saved*/
		if (pa->totkey == psys->part->hair_step + 1) {
			zero_v3(root->co);
		}

	}
}

/* Code for an adaptive time step based on the Courant-Friedrichs-Lewy
 * condition. */
static const float MIN_TIMESTEP = 1.0f / 101.0f;
/* Tolerance of 1.5 means the last subframe neither favors growing nor
 * shrinking (e.g if it were 1.3, the last subframe would tend to be too
 * small). */
static const float TIMESTEP_EXPANSION_FACTOR = 0.1f;
static const float TIMESTEP_EXPANSION_TOLERANCE = 1.5f;

/* Calculate the speed of the particle relative to the local scale of the
 * simulation. This should be called once per particle during a simulation
 * step, after the velocity has been updated. element_size defines the scale of
 * the simulation, and is typically the distance to neighboring particles. */
static void update_courant_num(ParticleSimulationData *sim, ParticleData *pa,
                               float dtime, SPHData *sphdata, SpinLock *spin)
{
	float relative_vel[3];

	sub_v3_v3v3(relative_vel, pa->prev_state.vel, sphdata->flow);

	const float courant_num = len_v3(relative_vel) * dtime / sphdata->element_size;
	if (sim->courant_num < courant_num) {
		BLI_spin_lock(spin);
		if (sim->courant_num < courant_num) {
			sim->courant_num = courant_num;
		}
		BLI_spin_unlock(spin);
	}
}
static float get_base_time_step(ParticleSettings *part)
{
	return 1.0f / (float) (part->subframes + 1);
}
/* Update time step size to suit current conditions. */
static void update_timestep(ParticleSystem *psys, ParticleSimulationData *sim)
{
	float dt_target;
	if (sim->courant_num == 0.0f)
		dt_target = 1.0f;
	else
		dt_target = psys->dt_frac * (psys->part->courant_target / sim->courant_num);

	/* Make sure the time step is reasonable. For some reason, the CLAMP macro
	 * doesn't work here. The time step becomes too large. - z0r */
	if (dt_target < MIN_TIMESTEP)
		dt_target = MIN_TIMESTEP;
	else if (dt_target > get_base_time_step(psys->part))
		dt_target = get_base_time_step(psys->part);

	/* Decrease time step instantly, but increase slowly. */
	if (dt_target > psys->dt_frac)
		psys->dt_frac = interpf(dt_target, psys->dt_frac, TIMESTEP_EXPANSION_FACTOR);
	else
		psys->dt_frac = dt_target;
}

static float sync_timestep(ParticleSystem *psys, float t_frac)
{
	/* Sync with frame end if it's close. */
	if (t_frac == 1.0f)
		return psys->dt_frac;
	else if (t_frac + (psys->dt_frac * TIMESTEP_EXPANSION_TOLERANCE) >= 1.0f)
		return 1.0f - t_frac;
	else
		return psys->dt_frac;
}

/************************************************/
/*			System Core							*/
/************************************************/

typedef struct DynamicStepSolverTaskData {
	ParticleSimulationData *sim;

	float cfra;
	float timestep;
	float dtime;

	SpinLock spin;
} DynamicStepSolverTaskData;

static void dynamics_step_sph_ddr_task_cb_ex(
        void *__restrict userdata,
        const int p,
        const ParallelRangeTLS *__restrict tls)
{
	DynamicStepSolverTaskData *data = userdata;
	ParticleSimulationData *sim = data->sim;
	ParticleSystem *psys = sim->psys;
	ParticleSettings *part = psys->part;

	SPHData *sphdata = tls->userdata_chunk;

	ParticleData *pa;

	if ((pa = psys->particles + p)->state.time <= 0.0f) {
		return;
	}

	/* do global forces & effectors */
	basic_integrate(sim, p, pa->state.time, data->cfra);

	/* actual fluids calculations */
	sph_integrate(sim, pa, pa->state.time, sphdata);

	if (sim->colliders)
		collision_check(sim, p, pa->state.time, data->cfra);

	/* SPH particles are not physical particles, just interpolation
	 * particles,  thus rotation has not a direct sense for them */
	basic_rotate(part, pa, pa->state.time, data->timestep);

	if (part->time_flag & PART_TIME_AUTOSF) {
		update_courant_num(sim, pa, data->dtime, sphdata, &data->spin);
	}
}

static void dynamics_step_sph_classical_basic_integrate_task_cb_ex(
        void *__restrict userdata,
        const int p,
        const ParallelRangeTLS *__restrict UNUSED(tls))
{
	DynamicStepSolverTaskData *data = userdata;
	ParticleSimulationData *sim = data->sim;
	ParticleSystem *psys = sim->psys;

	ParticleData *pa;

	if ((pa = psys->particles + p)->state.time <= 0.0f) {
		return;
	}

	basic_integrate(sim, p, pa->state.time, data->cfra);
}

static void dynamics_step_sph_classical_calc_density_task_cb_ex(
        void *__restrict userdata,
        const int p,
        const ParallelRangeTLS *__restrict tls)
{
	DynamicStepSolverTaskData *data = userdata;
	ParticleSimulationData *sim = data->sim;
	ParticleSystem *psys = sim->psys;

	SPHData *sphdata = tls->userdata_chunk;

	ParticleData *pa;

	if ((pa = psys->particles + p)->state.time <= 0.0f) {
		return;
	}

	sphclassical_calc_dens(pa, pa->state.time, sphdata);
}

static void dynamics_step_sph_classical_integrate_task_cb_ex(
        void *__restrict userdata,
        const int p,
        const ParallelRangeTLS *__restrict tls)
{
	DynamicStepSolverTaskData *data = userdata;
	ParticleSimulationData *sim = data->sim;
	ParticleSystem *psys = sim->psys;
	ParticleSettings *part = psys->part;

	SPHData *sphdata = tls->userdata_chunk;

	ParticleData *pa;

	if ((pa = psys->particles + p)->state.time <= 0.0f) {
		return;
	}

	/* actual fluids calculations */
	sph_integrate(sim, pa, pa->state.time, sphdata);

	if (sim->colliders)
		collision_check(sim, p, pa->state.time, data->cfra);

	/* SPH particles are not physical particles, just interpolation
	 * particles,  thus rotation has not a direct sense for them */
	basic_rotate(part, pa, pa->state.time, data->timestep);

	if (part->time_flag & PART_TIME_AUTOSF) {
		update_courant_num(sim, pa, data->dtime, sphdata, &data->spin);
	}
}

/* unbaked particles are calculated dynamically */
static void dynamics_step(ParticleSimulationData *sim, float cfra)
{
	ParticleSystem *psys = sim->psys;
	ParticleSettings *part=psys->part;
	RNG *rng;
	BoidBrainData bbd;
	ParticleTexture ptex;
	PARTICLE_P;
	float timestep;
	/* frame & time changes */
	float dfra, dtime;
	float birthtime, dietime;

	/* where have we gone in time since last time */
	dfra= cfra - psys->cfra;

	timestep = psys_get_timestep(sim);
	dtime= dfra*timestep;

	if (dfra < 0.0f) {
		LOOP_EXISTING_PARTICLES {
			psys_get_texture(sim, pa, &ptex, PAMAP_SIZE, cfra);
			pa->size = part->size*ptex.size;
			if (part->randsize > 0.0f)
				pa->size *= 1.0f - part->randsize * psys_frand(psys, p + 1);

			reset_particle(sim, pa, dtime, cfra);
		}
		return;
	}

	BLI_srandom(31415926 + (int)cfra + psys->seed);
	/* for now do both, boids us 'rng' */
	rng = BLI_rng_new_srandom(31415926 + (int)cfra + psys->seed);

	psys_update_effectors(sim);

	if (part->type != PART_HAIR)
		sim->colliders = get_collider_cache(sim->scene, sim->ob, part->collision_group);

	/* initialize physics type specific stuff */
	switch (part->phystype) {
		case PART_PHYS_BOIDS:
		{
			ParticleTarget *pt = psys->targets.first;
			bbd.sim = sim;
			bbd.part = part;
			bbd.cfra = cfra;
			bbd.dfra = dfra;
			bbd.timestep = timestep;
			bbd.rng = rng;

			psys_update_particle_tree(psys, cfra);

			boids_precalc_rules(part, cfra);

			for (; pt; pt=pt->next) {
				ParticleSystem *psys_target = psys_get_target_system(sim->ob, pt);
				if (psys_target && psys_target != psys) {
					psys_update_particle_tree(psys_target, cfra);
				}
			}
			break;
		}
		case PART_PHYS_FLUID:
		{
			ParticleTarget *pt = psys->targets.first;
			psys_update_particle_bvhtree(psys, cfra);

			for (; pt; pt=pt->next) {  /* Updating others systems particle tree for fluid-fluid interaction */
				if (pt->ob)
					psys_update_particle_bvhtree(BLI_findlink(&pt->ob->particlesystem, pt->psys-1), cfra);
			}
			break;
		}
	}
	/* initialize all particles for dynamics */
	LOOP_SHOWN_PARTICLES {
		copy_particle_key(&pa->prev_state,&pa->state,1);

		psys_get_texture(sim, pa, &ptex, PAMAP_SIZE, cfra);

		pa->size = part->size*ptex.size;
		if (part->randsize > 0.0f)
			pa->size *= 1.0f - part->randsize * psys_frand(psys, p + 1);

		birthtime = pa->time;
		dietime = pa->dietime;

		/* store this, so we can do multiple loops over particles */
		pa->state.time = dfra;

		if (dietime <= cfra && psys->cfra < dietime) {
			/* particle dies some time between this and last step */
			pa->state.time = dietime - ((birthtime > psys->cfra) ? birthtime : psys->cfra);
			pa->alive = PARS_DYING;
		}
		else if (birthtime <= cfra && birthtime >= psys->cfra) {
			/* particle is born some time between this and last step*/
			reset_particle(sim, pa, dfra*timestep, cfra);
			pa->alive = PARS_ALIVE;
			pa->state.time = cfra - birthtime;
		}
		else if (dietime < cfra) {
			/* nothing to be done when particle is dead */
		}

		/* only reset unborn particles if they're shown or if the particle is born soon*/
		if (pa->alive==PARS_UNBORN && (part->flag & PART_UNBORN || (cfra + psys->pointcache->step > pa->time))) {
			reset_particle(sim, pa, dtime, cfra);
		}
		else if (part->phystype == PART_PHYS_NO) {
			reset_particle(sim, pa, dtime, cfra);
		}

		if (ELEM(pa->alive, PARS_ALIVE, PARS_DYING)==0 || (pa->flag & (PARS_UNEXIST|PARS_NO_DISP)))
			pa->state.time = -1.f;
	}

	switch (part->phystype) {
		case PART_PHYS_NEWTON:
		{
			LOOP_DYNAMIC_PARTICLES {
				/* do global forces & effectors */
				basic_integrate(sim, p, pa->state.time, cfra);

				/* deflection */
				if (sim->colliders)
					collision_check(sim, p, pa->state.time, cfra);

				/* rotations */
				basic_rotate(part, pa, pa->state.time, timestep);
			}
			break;
		}
		case PART_PHYS_BOIDS:
		{
			LOOP_DYNAMIC_PARTICLES {
				bbd.goal_ob = NULL;

				boid_brain(&bbd, p, pa);

				if (pa->alive != PARS_DYING) {
					boid_body(&bbd, pa);

					/* deflection */
					if (sim->colliders)
						collision_check(sim, p, pa->state.time, cfra);
				}
			}
			break;
		}
		case PART_PHYS_FLUID:
		{
			SPHData sphdata;
			psys_sph_init(sim, &sphdata);

			DynamicStepSolverTaskData task_data = {
			    .sim = sim, .cfra = cfra, .timestep = timestep, .dtime = dtime,
			};

			BLI_spin_init(&task_data.spin);

			if (part->fluid->solver == SPH_SOLVER_DDR) {
				/* Apply SPH forces using double-density relaxation algorithm
				 * (Clavat et. al.) */

				ParallelRangeSettings settings;
				BLI_parallel_range_settings_defaults(&settings);
				settings.use_threading = (psys->totpart > 100);
				settings.userdata_chunk = &sphdata;
				settings.userdata_chunk_size = sizeof(sphdata);
				BLI_task_parallel_range(
				        0, psys->totpart,
				        &task_data,
				        dynamics_step_sph_ddr_task_cb_ex,
				        &settings);

				sph_springs_modify(psys, timestep);
			}
			else {
				/* SPH_SOLVER_CLASSICAL */
				/* Apply SPH forces using classical algorithm (due to Gingold
				 * and Monaghan). Note that, unlike double-density relaxation,
				 * this algorithm is separated into distinct loops. */

				{
					ParallelRangeSettings settings;
					BLI_parallel_range_settings_defaults(&settings);
					settings.use_threading = (psys->totpart > 100);
					BLI_task_parallel_range(
					        0, psys->totpart,
					        &task_data,
					        dynamics_step_sph_classical_basic_integrate_task_cb_ex,
					        &settings);
				}

				/* calculate summation density */
				/* Note that we could avoid copying sphdata for each thread here (it's only read here),
				 * but doubt this would gain us anything except confusion... */
				{
					ParallelRangeSettings settings;
					BLI_parallel_range_settings_defaults(&settings);
					settings.use_threading = (psys->totpart > 100);
					settings.userdata_chunk = &sphdata;
					settings.userdata_chunk_size = sizeof(sphdata);
					BLI_task_parallel_range(
					        0, psys->totpart,
					        &task_data,
					        dynamics_step_sph_classical_calc_density_task_cb_ex,
					        &settings);
				}

				/* do global forces & effectors */
				{
					ParallelRangeSettings settings;
					BLI_parallel_range_settings_defaults(&settings);
					settings.use_threading = (psys->totpart > 100);
					settings.userdata_chunk = &sphdata;
					settings.userdata_chunk_size = sizeof(sphdata);
					BLI_task_parallel_range(
					        0, psys->totpart,
					        &task_data,
					        dynamics_step_sph_classical_integrate_task_cb_ex,
					        &settings);
				}
			}

			BLI_spin_end(&task_data.spin);

			psys_sph_finalise(&sphdata);
			break;
		}
	}

	/* finalize particle state and time after dynamics */
	LOOP_DYNAMIC_PARTICLES {
		if (pa->alive == PARS_DYING) {
			pa->alive=PARS_DEAD;
			pa->state.time=pa->dietime;
		}
		else
			pa->state.time=cfra;
	}

	free_collider_cache(&sim->colliders);
	BLI_rng_free(rng);
}
static void update_children(ParticleSimulationData *sim)
{
	if ((sim->psys->part->type == PART_HAIR) && (sim->psys->flag & PSYS_HAIR_DONE)==0)
	/* don't generate children while growing hair - waste of time */
		psys_free_children(sim->psys);
	else if (sim->psys->part->childtype) {
		if (sim->psys->totchild != psys_get_tot_child(sim->scene, sim->psys))
			distribute_particles(sim, PART_FROM_CHILD);
		else {
			/* Children are up to date, nothing to do. */
		}
	}
	else
		psys_free_children(sim->psys);
}
/* updates cached particles' alive & other flags etc..*/
static void cached_step(ParticleSimulationData *sim, float cfra)
{
	ParticleSystem *psys = sim->psys;
	ParticleSettings *part = psys->part;
	ParticleTexture ptex;
	PARTICLE_P;
	float disp, dietime;

	psys_update_effectors(sim);

	disp= psys_get_current_display_percentage(psys);

	LOOP_PARTICLES {
		psys_get_texture(sim, pa, &ptex, PAMAP_SIZE, cfra);
		pa->size = part->size*ptex.size;
		if (part->randsize > 0.0f)
			pa->size *= 1.0f - part->randsize * psys_frand(psys, p + 1);

		psys->lattice_deform_data = psys_create_lattice_deform_data(sim);

		dietime = pa->dietime;

		/* update alive status and push events */
		if (pa->time > cfra) {
			pa->alive = PARS_UNBORN;
			if (part->flag & PART_UNBORN && (psys->pointcache->flag & PTCACHE_EXTERNAL) == 0)
				reset_particle(sim, pa, 0.0f, cfra);
		}
		else if (dietime <= cfra)
			pa->alive = PARS_DEAD;
		else
			pa->alive = PARS_ALIVE;

		if (psys->lattice_deform_data) {
			end_latt_deform(psys->lattice_deform_data);
			psys->lattice_deform_data = NULL;
		}

		if (psys_frand(psys, p) > disp)
			pa->flag |= PARS_NO_DISP;
		else
			pa->flag &= ~PARS_NO_DISP;
	}
}

static void particles_fluid_step(
        Main *bmain, ParticleSimulationData *sim, int UNUSED(cfra), const bool use_render_params)
{
	ParticleSystem *psys = sim->psys;
	if (psys->particles) {
		MEM_freeN(psys->particles);
		psys->particles = 0;
		psys->totpart = 0;
	}

	/* fluid sim particle import handling, actual loading of particles from file */
#ifdef WITH_MOD_FLUID
	{
		FluidsimModifierData *fluidmd = (FluidsimModifierData *)modifiers_findByType(sim->ob, eModifierType_Fluidsim);

		if ( fluidmd && fluidmd->fss) {
			FluidsimSettings *fss= fluidmd->fss;
			ParticleSettings *part = psys->part;
			ParticleData *pa=NULL;
			char filename[256];
			char debugStrBuffer[256];
			int  curFrame = sim->scene->r.cfra -1; // warning - sync with derived mesh fsmesh loading
			int  p, j, totpart;
			int readMask, activeParts = 0, fileParts = 0;
			gzFile gzf;

// XXX			if (ob==G.obedit) // off...
//				return;

			// ok, start loading
			BLI_join_dirfile(filename, sizeof(filename), fss->surfdataPath, OB_FLUIDSIM_SURF_PARTICLES_FNAME);

			BLI_path_abs(filename, modifier_path_relbase(bmain, sim->ob));

			BLI_path_frame(filename, curFrame, 0); // fixed #frame-no

			gzf = BLI_gzopen(filename, "rb");
			if (!gzf) {
				BLI_snprintf(debugStrBuffer, sizeof(debugStrBuffer),"readFsPartData::error - Unable to open file for reading '%s'\n", filename);
				// XXX bad level call elbeemDebugOut(debugStrBuffer);
				return;
			}

			gzread(gzf, &totpart, sizeof(totpart));
			totpart = (use_render_params) ? totpart:(part->disp*totpart) / 100;

			part->totpart= totpart;
			part->sta=part->end = 1.0f;
			part->lifetime = sim->scene->r.efra + 1;

			/* allocate particles */
			realloc_particles(sim, part->totpart);

			// set up reading mask
			readMask = fss->typeFlags;

			for (p=0, pa=psys->particles; p<totpart; p++, pa++) {
				int ptype=0;

				gzread(gzf, &ptype, sizeof( ptype ));
				if (ptype & readMask) {
					activeParts++;

					gzread(gzf, &(pa->size), sizeof(float));

					pa->size /= 10.0f;

					for (j=0; j<3; j++) {
						float wrf;
						gzread(gzf, &wrf, sizeof( wrf ));
						pa->state.co[j] = wrf;
						//fprintf(stderr,"Rj%d ",j);
					}
					for (j=0; j<3; j++) {
						float wrf;
						gzread(gzf, &wrf, sizeof( wrf ));
						pa->state.vel[j] = wrf;
					}

					zero_v3(pa->state.ave);
					unit_qt(pa->state.rot);

					pa->time = 1.f;
					pa->dietime = sim->scene->r.efra + 1;
					pa->lifetime = sim->scene->r.efra;
					pa->alive = PARS_ALIVE;
					//if (a < 25) fprintf(stderr,"FSPARTICLE debug set %s, a%d = %f,%f,%f, life=%f\n", filename, a, pa->co[0],pa->co[1],pa->co[2], pa->lifetime );
				}
				else {
					// skip...
					for (j=0; j<2*3+1; j++) {
						float wrf; gzread(gzf, &wrf, sizeof( wrf ));
					}
				}
				fileParts++;
			}
			gzclose(gzf);

			totpart = psys->totpart = activeParts;
			BLI_snprintf(debugStrBuffer,sizeof(debugStrBuffer),"readFsPartData::done - particles:%d, active:%d, file:%d, mask:%d\n", psys->totpart,activeParts,fileParts,readMask);
			// bad level call
			// XXX elbeemDebugOut(debugStrBuffer);

		} // fluid sim particles done
	}
#else
	UNUSED_VARS(bmain, use_render_params);
#endif // WITH_MOD_FLUID
}

static int emit_particles(ParticleSimulationData *sim, PTCacheID *pid, float UNUSED(cfra))
{
	ParticleSystem *psys = sim->psys;
	int oldtotpart = psys->totpart;
	int totpart = tot_particles(psys, pid);

	if (totpart != oldtotpart)
		realloc_particles(sim, totpart);

	return totpart - oldtotpart;
}

/* Calculates the next state for all particles of the system
 * In particles code most fra-ending are frames, time-ending are fra*timestep (seconds)
 * 1. Emit particles
 * 2. Check cache (if used) and return if frame is cached
 * 3. Do dynamics
 * 4. Save to cache */
static void system_step(ParticleSimulationData *sim, float cfra, const bool use_render_params)
{
	ParticleSystem *psys = sim->psys;
	ParticleSettings *part = psys->part;
	PointCache *cache = psys->pointcache;
	PTCacheID ptcacheid, *pid = NULL;
	PARTICLE_P;
	float disp, cache_cfra = cfra; /*, *vg_vel= 0, *vg_tan= 0, *vg_rot= 0, *vg_size= 0; */
	int startframe = 0, endframe = 100, oldtotpart = 0;

	/* cache shouldn't be used for hair or "continue physics" */
	if (part->type != PART_HAIR) {
		psys_clear_temp_pointcache(psys);

		/* set suitable cache range automatically */
		if ((cache->flag & (PTCACHE_BAKING|PTCACHE_BAKED))==0)
			psys_get_pointcache_start_end(sim->scene, psys, &cache->startframe, &cache->endframe);

		pid = &ptcacheid;
		BKE_ptcache_id_from_particles(pid, sim->ob, psys);

		BKE_ptcache_id_time(pid, sim->scene, 0.0f, &startframe, &endframe, NULL);

		/* clear everything on start frame, or when psys needs full reset! */
		if ((cfra == startframe) || (psys->recalc & PSYS_RECALC_RESET)) {
			BKE_ptcache_id_reset(sim->scene, pid, PTCACHE_RESET_OUTDATED);
			BKE_ptcache_validate(cache, startframe);
			cache->flag &= ~PTCACHE_REDO_NEEDED;
		}

		CLAMP(cache_cfra, startframe, endframe);
	}

/* 1. emit particles and redo particles if needed */
	oldtotpart = psys->totpart;
	if (emit_particles(sim, pid, cfra) || psys->recalc & PSYS_RECALC_RESET) {
		distribute_particles(sim, part->from);
		initialize_all_particles(sim);
		/* reset only just created particles (on startframe all particles are recreated) */
		reset_all_particles(sim, 0.0, cfra, oldtotpart);
		free_unexisting_particles(sim);

		if (psys->fluid_springs) {
			MEM_freeN(psys->fluid_springs);
			psys->fluid_springs = NULL;
		}

		psys->tot_fluidsprings = psys->alloc_fluidsprings = 0;

		/* flag for possible explode modifiers after this system */
		sim->psmd->flag |= eParticleSystemFlag_Pars;

		BKE_ptcache_id_clear(pid, PTCACHE_CLEAR_AFTER, cfra);
	}

/* 2. try to read from the cache */
	if (pid) {
		int cache_result = BKE_ptcache_read(pid, cache_cfra, true);

		if (ELEM(cache_result, PTCACHE_READ_EXACT, PTCACHE_READ_INTERPOLATED)) {
			cached_step(sim, cfra);
			update_children(sim);
			psys_update_path_cache(sim, cfra, use_render_params);

			BKE_ptcache_validate(cache, (int)cache_cfra);

			if (cache_result == PTCACHE_READ_INTERPOLATED && cache->flag & PTCACHE_REDO_NEEDED)
				BKE_ptcache_write(pid, (int)cache_cfra);

			return;
		}
		/* Cache is supposed to be baked, but no data was found so bail out */
		else if (cache->flag & PTCACHE_BAKED) {
			psys_reset(psys, PSYS_RESET_CACHE_MISS);
			return;
		}
		else if (cache_result == PTCACHE_READ_OLD) {
			psys->cfra = (float)cache->simframe;
			cached_step(sim, psys->cfra);
		}

		/* if on second frame, write cache for first frame */
		if (psys->cfra == startframe && (cache->flag & PTCACHE_OUTDATED || cache->last_exact==0))
			BKE_ptcache_write(pid, startframe);
	}
	else
		BKE_ptcache_invalidate(cache);

/* 3. do dynamics */
	/* set particles to be not calculated TODO: can't work with pointcache */
	disp= psys_get_current_display_percentage(psys);

	LOOP_PARTICLES {
		if (psys_frand(psys, p) > disp)
			pa->flag |= PARS_NO_DISP;
		else
			pa->flag &= ~PARS_NO_DISP;
	}

	if (psys->totpart) {
		int dframe, totframesback = 0;
		float t_frac, dt_frac;

		/* handle negative frame start at the first frame by doing
		 * all the steps before the first frame */
		if ((int)cfra == startframe && part->sta < startframe)
			totframesback = (startframe - (int)part->sta);

		if (!(part->time_flag & PART_TIME_AUTOSF)) {
			/* Constant time step */
			psys->dt_frac = get_base_time_step(part);
		}
		else if ((int)cfra == startframe) {
			/* Variable time step; initialise to subframes */
			psys->dt_frac = get_base_time_step(part);
		}
		else if (psys->dt_frac < MIN_TIMESTEP) {
			/* Variable time step; subsequent frames */
			psys->dt_frac = MIN_TIMESTEP;
		}

		for (dframe=-totframesback; dframe<=0; dframe++) {
			/* simulate each subframe */
			dt_frac = psys->dt_frac;
			for (t_frac = dt_frac; t_frac <= 1.0f; t_frac += dt_frac) {
				sim->courant_num = 0.0f;
				dynamics_step(sim, cfra+dframe+t_frac - 1.f);
				psys->cfra = cfra+dframe+t_frac - 1.f;
#if 0
				printf("%f,%f,%f,%f\n", cfra+dframe+t_frac - 1.f, t_frac, dt_frac, sim->courant_num);
#endif
				if (part->time_flag & PART_TIME_AUTOSF)
					update_timestep(psys, sim);
				/* Even without AUTOSF dt_frac may not add up to 1.0 due to float precision. */
				dt_frac = sync_timestep(psys, t_frac);
			}
		}
	}

/* 4. only write cache starting from second frame */
	if (pid) {
		BKE_ptcache_validate(cache, (int)cache_cfra);
		if ((int)cache_cfra != startframe)
			BKE_ptcache_write(pid, (int)cache_cfra);
	}

	update_children(sim);

/* cleanup */
	if (psys->lattice_deform_data) {
		end_latt_deform(psys->lattice_deform_data);
		psys->lattice_deform_data = NULL;
	}
}

/* system type has changed so set sensible defaults and clear non applicable flags */
void psys_changed_type(Object *ob, ParticleSystem *psys)
{
	ParticleSettings *part = psys->part;
	PTCacheID pid;

	BKE_ptcache_id_from_particles(&pid, ob, psys);

	if (part->phystype != PART_PHYS_KEYED)
		psys->flag &= ~PSYS_KEYED;

	if (part->type == PART_HAIR) {
		if (ELEM(part->ren_as, PART_DRAW_NOT, PART_DRAW_PATH, PART_DRAW_OB, PART_DRAW_GR)==0)
			part->ren_as = PART_DRAW_PATH;

		if (part->distr == PART_DISTR_GRID)
			part->distr = PART_DISTR_JIT;

		if (ELEM(part->draw_as, PART_DRAW_NOT, PART_DRAW_REND, PART_DRAW_PATH)==0)
			part->draw_as = PART_DRAW_REND;

		CLAMP(part->path_start, 0.0f, 100.0f);
		CLAMP(part->path_end, 0.0f, 100.0f);

		BKE_ptcache_id_clear(&pid, PTCACHE_CLEAR_ALL, 0);
	}
	else {
		free_hair(ob, psys, 1);

		CLAMP(part->path_start, 0.0f, MAX2(100.0f, part->end + part->lifetime));
		CLAMP(part->path_end, 0.0f, MAX2(100.0f, part->end + part->lifetime));
	}

	psys_reset(psys, PSYS_RESET_ALL);
}
void psys_check_boid_data(ParticleSystem *psys)
{
		BoidParticle *bpa;
		PARTICLE_P;

		pa = psys->particles;

		if (!pa)
			return;

		if (psys->part && psys->part->phystype==PART_PHYS_BOIDS) {
			if (!pa->boid) {
				bpa = MEM_callocN(psys->totpart * sizeof(BoidParticle), "Boid Data");

				LOOP_PARTICLES {
					pa->boid = bpa++;
				}
			}
		}
		else if (pa->boid) {
			MEM_freeN(pa->boid);
			LOOP_PARTICLES {
				pa->boid = NULL;
			}
		}
}

void BKE_particlesettings_fluid_default_settings(ParticleSettings *part)
{
	SPHFluidSettings *fluid = part->fluid;

	fluid->spring_k = 0.f;
	fluid->plasticity_constant = 0.1f;
	fluid->yield_ratio = 0.1f;
	fluid->rest_length = 1.f;
	fluid->viscosity_omega = 2.f;
	fluid->viscosity_beta = 0.1f;
	fluid->stiffness_k = 1.f;
	fluid->stiffness_knear = 1.f;
	fluid->rest_density = 1.f;
	fluid->buoyancy = 0.f;
	fluid->radius = 1.f;
	fluid->flag |= SPH_FAC_REPULSION|SPH_FAC_DENSITY|SPH_FAC_RADIUS|SPH_FAC_VISCOSITY|SPH_FAC_REST_LENGTH;
}

static void psys_prepare_physics(ParticleSimulationData *sim)
{
	ParticleSettings *part = sim->psys->part;

	if (ELEM(part->phystype, PART_PHYS_NO, PART_PHYS_KEYED)) {
		PTCacheID pid;
		BKE_ptcache_id_from_particles(&pid, sim->ob, sim->psys);
		BKE_ptcache_id_clear(&pid, PTCACHE_CLEAR_ALL, 0);
	}
	else {
		free_keyed_keys(sim->psys);
		sim->psys->flag &= ~PSYS_KEYED;
	}

	/* RNA Update must ensure this is true. */
	if (part->phystype == PART_PHYS_BOIDS) {
		BLI_assert(part->boids != NULL);
	}
	else if (part->phystype == PART_PHYS_FLUID) {
		BLI_assert(part->fluid != NULL);
	}

	psys_check_boid_data(sim->psys);
}
static int hair_needs_recalc(ParticleSystem *psys)
{
	if (!(psys->flag & PSYS_EDITED) && (!psys->edit || !psys->edit->edited) &&
	    ((psys->flag & PSYS_HAIR_DONE)==0 || psys->recalc & PSYS_RECALC_RESET || (psys->part->flag & PART_HAIR_REGROW && !psys->edit)))
	{
		return 1;
	}

	return 0;
}

/* main particle update call, checks that things are ok on the large scale and
 * then advances in to actual particle calculations depending on particle type */
void particle_system_update(Main *bmain, Scene *scene, Object *ob, ParticleSystem *psys, const bool use_render_params)
{
	ParticleSimulationData sim= {0};
	ParticleSettings *part = psys->part;
	float cfra;

	/* drawdata is outdated after ANY change */
	if (psys->pdd) psys->pdd->flag &= ~PARTICLE_DRAW_DATA_UPDATED;

	if (!psys_check_enabled(ob, psys, use_render_params))
		return;

	cfra= BKE_scene_frame_get(scene);

	sim.scene= scene;
	sim.ob= ob;
	sim.psys= psys;
	sim.psmd= psys_get_modifier(ob, psys);

	/* system was already updated from modifier stack */
	if (sim.psmd->flag & eParticleSystemFlag_psys_updated) {
		sim.psmd->flag &= ~eParticleSystemFlag_psys_updated;
		/* make sure it really was updated to cfra */
		if (psys->cfra == cfra)
			return;
	}

	if (!sim.psmd->dm_final)
		return;

	if (part->from != PART_FROM_VERT) {
		DM_ensure_tessface(sim.psmd->dm_final);
	}

	/* execute drivers only, as animation has already been done */
	BKE_animsys_evaluate_animdata(scene, &part->id, part->adt, cfra, ADT_RECALC_DRIVERS);

	/* to verify if we need to restore object afterwards */
	psys->flag &= ~PSYS_OB_ANIM_RESTORE;

	if (psys->recalc & PSYS_RECALC_TYPE)
		psys_changed_type(sim.ob, sim.psys);

	if (psys->recalc & PSYS_RECALC_RESET)
		psys->totunexist = 0;

	/* setup necessary physics type dependent additional data if it doesn't yet exist */
	psys_prepare_physics(&sim);

	switch (part->type) {
		case PART_HAIR:
		{
			/* nothing to do so bail out early */
			if (psys->totpart == 0 && part->totpart == 0) {
				psys_free_path_cache(psys, NULL);
				free_hair(ob, psys, 0);
				psys->flag |= PSYS_HAIR_DONE;
			}
			/* (re-)create hair */
			else if (hair_needs_recalc(psys)) {
				float hcfra=0.0f;
				int i, recalc = psys->recalc;

				free_hair(ob, psys, 0);

				if (psys->edit && psys->free_edit) {
					psys->free_edit(psys->edit);
					psys->edit = NULL;
					psys->free_edit = NULL;
				}

				/* first step is negative so particles get killed and reset */
				psys->cfra= 1.0f;

				for (i=0; i<=part->hair_step; i++) {
					hcfra=100.0f*(float)i/(float)psys->part->hair_step;
					if ((part->flag & PART_HAIR_REGROW)==0)
						BKE_animsys_evaluate_animdata(scene, &part->id, part->adt, hcfra, ADT_RECALC_ANIM);
					system_step(&sim, hcfra, use_render_params);
					psys->cfra = hcfra;
					psys->recalc = 0;
					save_hair(&sim, hcfra);
				}

				psys->flag |= PSYS_HAIR_DONE;
				psys->recalc = recalc;
			}
			else if (psys->flag & PSYS_EDITED)
				psys->flag |= PSYS_HAIR_DONE;

			if (psys->flag & PSYS_HAIR_DONE)
				hair_step(&sim, cfra, use_render_params);
			break;
		}
		case PART_FLUID:
		{
			particles_fluid_step(bmain, &sim, (int)cfra, use_render_params);
			break;
		}
		default:
		{
			switch (part->phystype) {
				case PART_PHYS_NO:
				case PART_PHYS_KEYED:
				{
					PARTICLE_P;
					float disp = psys_get_current_display_percentage(psys);
					bool free_unexisting = false;

					/* Particles without dynamics haven't been reset yet because they don't use pointcache */
					if (psys->recalc & PSYS_RECALC_RESET)
						psys_reset(psys, PSYS_RESET_ALL);

					if (emit_particles(&sim, NULL, cfra) || (psys->recalc & PSYS_RECALC_RESET)) {
						free_keyed_keys(psys);
						distribute_particles(&sim, part->from);
						initialize_all_particles(&sim);
						free_unexisting = true;

						/* flag for possible explode modifiers after this system */
						sim.psmd->flag |= eParticleSystemFlag_Pars;
					}

					LOOP_EXISTING_PARTICLES {
						pa->size = part->size;
						if (part->randsize > 0.0f)
							pa->size *= 1.0f - part->randsize * psys_frand(psys, p + 1);

						reset_particle(&sim, pa, 0.0, cfra);

						if (psys_frand(psys, p) > disp)
							pa->flag |= PARS_NO_DISP;
						else
							pa->flag &= ~PARS_NO_DISP;
					}

					/* free unexisting after resetting particles */
					if (free_unexisting)
						free_unexisting_particles(&sim);

					if (part->phystype == PART_PHYS_KEYED) {
						psys_count_keyed_targets(&sim);
						set_keyed_keys(&sim);
						psys_update_path_cache(&sim, (int)cfra, use_render_params);
					}
					break;
				}
				default:
				{
					/* the main dynamic particle system step */
					system_step(&sim, cfra, use_render_params);
					break;
				}
			}
			break;
		}
	}

	/* make sure emitter is left at correct time (particle emission can change this) */
	if (psys->flag & PSYS_OB_ANIM_RESTORE) {
		evaluate_emitter_anim(scene, ob, cfra);
		psys->flag &= ~PSYS_OB_ANIM_RESTORE;
	}

	psys->cfra = cfra;
	psys->recalc = 0;

	/* save matrix for duplicators, at rendertime the actual dupliobject's matrix is used so don't update! */
	if (psys->renderdata==0)
		invert_m4_m4(psys->imat, ob->obmat);
}

/* ID looper */

void BKE_particlesystem_id_loop(ParticleSystem *psys, ParticleSystemIDFunc func, void *userdata)
{
	ParticleTarget *pt;

	func(psys, (ID **)&psys->part, userdata, IDWALK_CB_USER | IDWALK_CB_NEVER_NULL);
	func(psys, (ID **)&psys->target_ob, userdata, IDWALK_CB_NOP);
	func(psys, (ID **)&psys->parent, userdata, IDWALK_CB_NOP);

	for (pt = psys->targets.first; pt; pt = pt->next) {
		func(psys, (ID **)&pt->ob, userdata, IDWALK_CB_NOP);
	}

	/* Even though psys->part should never be NULL, this can happen as an exception during deletion.
	 * See ID_REMAP_SKIP/FORCE/FLAG_NEVER_NULL_USAGE in BKE_library_remap. */
	if (psys->part && psys->part->phystype == PART_PHYS_BOIDS) {
		ParticleData *pa;
		int p;

		for (p = 0, pa = psys->particles; p < psys->totpart; p++, pa++) {
			func(psys, (ID **)&pa->boid->ground, userdata, IDWALK_CB_NOP);
		}
	}
}

/* **** Depsgraph evaluation **** */

void BKE_particle_system_eval_init(EvaluationContext *UNUSED(eval_ctx),
                                   Scene *scene,
                                   Object *ob)
{
	DEG_debug_print_eval(__func__, ob->id.name, ob);
	BKE_ptcache_object_reset(scene, ob, PTCACHE_RESET_DEPSGRAPH);
}
