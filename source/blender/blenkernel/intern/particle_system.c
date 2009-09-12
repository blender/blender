/* particle_system.c
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
 * The Original Code is Copyright (C) 2007 by Janne Karhu.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include "BLI_storage.h" /* _LARGEFILE_SOURCE */

#include <stdlib.h>
#include <math.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "DNA_boid_types.h"
#include "DNA_particle_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_force.h"
#include "DNA_object_types.h"
#include "DNA_material_types.h"
#include "DNA_curve_types.h"
#include "DNA_group_types.h"
#include "DNA_scene_types.h"
#include "DNA_texture_types.h"
#include "DNA_ipo_types.h" // XXX old animation system stuff... to be removed!
#include "DNA_listBase.h"

#include "BLI_rand.h"
#include "BLI_jitter.h"
#include "BLI_arithb.h"
#include "BLI_blenlib.h"
#include "BLI_kdtree.h"
#include "BLI_kdopbvh.h"
#include "BLI_linklist.h"
#include "BLI_threads.h"

#include "BKE_anim.h"
#include "BKE_boids.h"
#include "BKE_cdderivedmesh.h"
#include "BKE_collision.h"
#include "BKE_displist.h"
#include "BKE_effect.h"
#include "BKE_particle.h"
#include "BKE_global.h"
#include "BKE_utildefines.h"
#include "BKE_DerivedMesh.h"
#include "BKE_object.h"
#include "BKE_material.h"
#include "BKE_cloth.h"
#include "BKE_depsgraph.h"
#include "BKE_lattice.h"
#include "BKE_pointcache.h"
#include "BKE_mesh.h"
#include "BKE_modifier.h"
#include "BKE_scene.h"
#include "BKE_bvhutils.h"

#include "PIL_time.h"

#include "RE_shader_ext.h"

/* fluid sim particle import */
#ifndef DISABLE_ELBEEM
#include "DNA_object_fluidsim.h"
#include "LBM_fluidsim.h"
#include <zlib.h>
#include <string.h>

#ifdef WIN32
#ifndef snprintf
#define snprintf _snprintf
#endif
#endif

#endif // DISABLE_ELBEEM

/************************************************/
/*			Reacting to system events			*/
/************************************************/

static int get_current_display_percentage(ParticleSystem *psys)
{
	ParticleSettings *part=psys->part;

	if(psys->renderdata || (part->child_nbr && part->childtype)
		|| (psys->pointcache->flag & PTCACHE_BAKING))
		return 100;

	if(part->phystype==PART_PHYS_KEYED){
		return psys->part->disp;
	}
	else
		return psys->part->disp;
}

void psys_reset(ParticleSystem *psys, int mode)
{
	ParticleSettings *part= psys->part;
	PARTICLE_P;

	if(ELEM(mode, PSYS_RESET_ALL, PSYS_RESET_DEPSGRAPH)) {
		if(mode == PSYS_RESET_ALL || !(psys->flag & PSYS_EDITED)) {
			psys_free_particles(psys);

			psys->totpart= 0;
			psys->totkeyed= 0;
			psys->flag &= ~(PSYS_HAIR_DONE|PSYS_KEYED);

			if(psys->reactevents.first)
				BLI_freelistN(&psys->reactevents);

			if(psys->edit && psys->free_edit) {
				psys->free_edit(psys->edit);
				psys->edit = NULL;
				psys->free_edit = NULL;
			}
		}
	}
	else if(mode == PSYS_RESET_CACHE_MISS) {
		/* set all particles to be skipped */
		LOOP_PARTICLES
			pa->flag |= PARS_NO_DISP;
	}

	/* reset children */
	if(psys->child) {
		MEM_freeN(psys->child);
		psys->child= 0;
	}

	psys->totchild= 0;

	/* reset path cache */
	psys_free_path_cache(psys, psys->edit);

	/* reset point cache */
	psys->pointcache->flag &= ~PTCACHE_SIMULATION_VALID;
	psys->pointcache->simframe= 0;
}

static void realloc_particles(Object *ob, ParticleSystem *psys, int new_totpart)
{
	ParticleData *newpars = NULL;
	BoidParticle *newboids = NULL;
	PARTICLE_P;
	int totpart, totsaved = 0;

	if(new_totpart<0) {
		if(psys->part->distr==PART_DISTR_GRID  && psys->part->from != PART_FROM_VERT) {
			totpart= psys->part->grid_res;
			totpart*=totpart*totpart;
		}
		else
			totpart=psys->part->totpart;
	}
	else
		totpart=new_totpart;

	if(totpart && totpart != psys->totpart) {
		newpars= MEM_callocN(totpart*sizeof(ParticleData), "particles");
	
		if(psys->particles) {
			totsaved=MIN2(psys->totpart,totpart);
			/*save old pars*/
			if(totsaved) {
				memcpy(newpars,psys->particles,totsaved*sizeof(ParticleData));

				if(psys->particles->boid)
					memcpy(newboids, psys->particles->boid, totsaved*sizeof(BoidParticle));
			}

			if(psys->particles->keys)
				MEM_freeN(psys->particles->keys);

			if(psys->particles->boid)
				MEM_freeN(psys->particles->boid);

			for(p=0, pa=newpars; p<totsaved; p++, pa++) {
				if(pa->keys) {
					pa->keys= NULL;
					pa->totkey= 0;
				}
			}

			for(p=totsaved, pa=psys->particles+totsaved; p<psys->totpart; p++, pa++)
				if(pa->hair) MEM_freeN(pa->hair);

			MEM_freeN(psys->particles);
		}
		
		psys->particles=newpars;

		if(newboids) {
			LOOP_PARTICLES
				pa->boid = newboids++;
		}
		
		psys->totpart=totpart;
	}

	if(psys->child) {
		MEM_freeN(psys->child);
		psys->child=0;
		psys->totchild=0;
	}
}

static int get_psys_child_number(struct Scene *scene, ParticleSystem *psys)
{
	int nbr;

	if(!psys->part->childtype)
		return 0;

	if(psys->renderdata) {
		nbr= psys->part->ren_child_nbr;
		return get_render_child_particle_number(&scene->r, nbr);
	}
	else
		return psys->part->child_nbr;
}

static int get_psys_tot_child(struct Scene *scene, ParticleSystem *psys)
{
	return psys->totpart*get_psys_child_number(scene, psys);
}

static void alloc_child_particles(ParticleSystem *psys, int tot)
{
	if(psys->child){
		MEM_freeN(psys->child);
		psys->child=0;
		psys->totchild=0;
	}

	if(psys->part->childtype) {
		psys->totchild= tot;
		if(psys->totchild)
			psys->child= MEM_callocN(psys->totchild*sizeof(ChildParticle), "child_particles");
	}
}

void psys_calc_dmcache(Object *ob, DerivedMesh *dm, ParticleSystem *psys)
{
	/* use for building derived mesh mapping info:

	   node: the allocated links - total derived mesh element count 
	   nodearray: the array of nodes aligned with the base mesh's elements, so
	              each original elements can reference its derived elements
	*/
	Mesh *me= (Mesh*)ob->data;
	PARTICLE_P;
	
	/* CACHE LOCATIONS */
	if(!dm->deformedOnly) {
		/* Will use later to speed up subsurf/derivedmesh */
		LinkNode *node, *nodedmelem, **nodearray;
		int totdmelem, totelem, i, *origindex;

		if(psys->part->from == PART_FROM_VERT) {
			totdmelem= dm->getNumVerts(dm);
			totelem= me->totvert;
			origindex= DM_get_vert_data_layer(dm, CD_ORIGINDEX);
		}
		else { /* FROM_FACE/FROM_VOLUME */
			totdmelem= dm->getNumFaces(dm);
			totelem= me->totface;
			origindex= DM_get_face_data_layer(dm, CD_ORIGINDEX);
		}
	
		nodedmelem= MEM_callocN(sizeof(LinkNode)*totdmelem, "psys node elems");
		nodearray= MEM_callocN(sizeof(LinkNode *)*totelem, "psys node array");
		
		for(i=0, node=nodedmelem; i<totdmelem; i++, origindex++, node++) {
			node->link= SET_INT_IN_POINTER(i);

			if(*origindex != -1) {
				if(nodearray[*origindex]) {
					/* prepend */
					node->next = nodearray[*origindex];
					nodearray[*origindex]= node;
				}
				else
					nodearray[*origindex]= node;
			}
		}
		
		/* cache the verts/faces! */
		LOOP_PARTICLES {
			if(psys->part->from == PART_FROM_VERT) {
				if(nodearray[pa->num])
					pa->num_dmcache= GET_INT_FROM_POINTER(nodearray[pa->num]->link);
			}
			else { /* FROM_FACE/FROM_VOLUME */
				/* Note that somtimes the pa->num is over the nodearray size, this is bad, maybe there is a better place to fix this,
				 * but for now passing NULL is OK. every face will be searched for the particle so its slower - Campbell */
				pa->num_dmcache= psys_particle_dm_face_lookup(ob, dm, pa->num, pa->fuv, pa->num < totelem ? nodearray[pa->num] : NULL);
			}
		}

		MEM_freeN(nodearray);
		MEM_freeN(nodedmelem);
	}
	else {
		/* TODO PARTICLE, make the following line unnecessary, each function
		 * should know to use the num or num_dmcache, set the num_dmcache to
		 * an invalid value, just incase */
		
		LOOP_PARTICLES
			pa->num_dmcache = -1;
	}
}

static void distribute_particles_in_grid(DerivedMesh *dm, ParticleSystem *psys)
{
	ParticleData *pa=0;
	float min[3], max[3], delta[3], d;
	MVert *mv, *mvert = dm->getVertDataArray(dm,0);
	int totvert=dm->getNumVerts(dm), from=psys->part->from;
	int i, j, k, p, res=psys->part->grid_res, size[3], axis;

	mv=mvert;

	/* find bounding box of dm */
	VECCOPY(min,mv->co);
	VECCOPY(max,mv->co);
	mv++;

	for(i=1; i<totvert; i++, mv++){
		min[0]=MIN2(min[0],mv->co[0]);
		min[1]=MIN2(min[1],mv->co[1]);
		min[2]=MIN2(min[2],mv->co[2]);

		max[0]=MAX2(max[0],mv->co[0]);
		max[1]=MAX2(max[1],mv->co[1]);
		max[2]=MAX2(max[2],mv->co[2]);
	}

	VECSUB(delta,max,min);

	/* determine major axis */
	axis = (delta[0]>=delta[1])?0:((delta[1]>=delta[2])?1:2);

	d = delta[axis]/(float)res;

	size[axis]=res;
	size[(axis+1)%3]=(int)ceil(delta[(axis+1)%3]/d);
	size[(axis+2)%3]=(int)ceil(delta[(axis+2)%3]/d);

	/* float errors grrr.. */
	size[(axis+1)%3] = MIN2(size[(axis+1)%3],res);
	size[(axis+2)%3] = MIN2(size[(axis+2)%3],res);

	min[0]+=d/2.0f;
	min[1]+=d/2.0f;
	min[2]+=d/2.0f;

	for(i=0,p=0,pa=psys->particles; i<res; i++){
		for(j=0; j<res; j++){
			for(k=0; k<res; k++,p++,pa++){
				pa->fuv[0]=min[0]+(float)i*d;
				pa->fuv[1]=min[1]+(float)j*d;
				pa->fuv[2]=min[2]+(float)k*d;
				pa->flag |= PARS_UNEXIST;
				pa->loop=0; /* abused in volume calculation */
			}
		}
	}

	/* enable particles near verts/edges/faces/inside surface */
	if(from==PART_FROM_VERT){
		float vec[3];

		pa=psys->particles;

		min[0]-=d/2.0f;
		min[1]-=d/2.0f;
		min[2]-=d/2.0f;

		for(i=0,mv=mvert; i<totvert; i++,mv++){
			VecSubf(vec,mv->co,min);
			vec[0]/=delta[0];
			vec[1]/=delta[1];
			vec[2]/=delta[2];
			(pa	+((int)(vec[0]*(size[0]-1))*res
				+(int)(vec[1]*(size[1]-1)))*res
				+(int)(vec[2]*(size[2]-1)))->flag &= ~PARS_UNEXIST;
		}
	}
	else if(ELEM(from,PART_FROM_FACE,PART_FROM_VOLUME)){
		float co1[3], co2[3];

		MFace *mface=0;
		float v1[3], v2[3], v3[3], v4[4], lambda;
		int a, a1, a2, a0mul, a1mul, a2mul, totface;
		int amax= from==PART_FROM_FACE ? 3 : 1;

		totface=dm->getNumFaces(dm);
		mface=dm->getFaceDataArray(dm,CD_MFACE);
		
		for(a=0; a<amax; a++){
			if(a==0){ a0mul=res*res; a1mul=res; a2mul=1; }
			else if(a==1){ a0mul=res; a1mul=1; a2mul=res*res; }
			else{ a0mul=1; a1mul=res*res; a2mul=res; }

			for(a1=0; a1<size[(a+1)%3]; a1++){
				for(a2=0; a2<size[(a+2)%3]; a2++){
					mface=dm->getFaceDataArray(dm,CD_MFACE);

					pa=psys->particles + a1*a1mul + a2*a2mul;
					VECCOPY(co1,pa->fuv);
					co1[a]-=d/2.0f;
					VECCOPY(co2,co1);
					co2[a]+=delta[a] + 0.001f*d;
					co1[a]-=0.001f*d;
					
					/* lets intersect the faces */
					for(i=0; i<totface; i++,mface++){
						VECCOPY(v1,mvert[mface->v1].co);
						VECCOPY(v2,mvert[mface->v2].co);
						VECCOPY(v3,mvert[mface->v3].co);

						if(AxialLineIntersectsTriangle(a,co1, co2, v2, v3, v1, &lambda)){
							if(from==PART_FROM_FACE)
								(pa+(int)(lambda*size[a])*a0mul)->flag &= ~PARS_UNEXIST;
							else /* store number of intersections */
								(pa+(int)(lambda*size[a])*a0mul)->loop++;
						}
						
						if(mface->v4){
							VECCOPY(v4,mvert[mface->v4].co);

							if(AxialLineIntersectsTriangle(a,co1, co2, v4, v1, v3, &lambda)){
								if(from==PART_FROM_FACE)
									(pa+(int)(lambda*size[a])*a0mul)->flag &= ~PARS_UNEXIST;
								else
									(pa+(int)(lambda*size[a])*a0mul)->loop++;
							}
						}
					}

					if(from==PART_FROM_VOLUME){
						int in=pa->loop%2;
						if(in) pa->loop++;
						for(i=0; i<size[0]; i++){
							if(in || (pa+i*a0mul)->loop%2)
								(pa+i*a0mul)->flag &= ~PARS_UNEXIST;
							/* odd intersections == in->out / out->in */
							/* even intersections -> in stays same */
							in=(in + (pa+i*a0mul)->loop) % 2;
						}
					}
				}
			}
		}
	}

	if(psys->part->flag & PART_GRID_INVERT){
		for(i=0,pa=psys->particles; i<size[0]; i++){
			for(j=0; j<size[1]; j++){
				pa=psys->particles + res*(i*res + j);
				for(k=0; k<size[2]; k++, pa++){
					pa->flag ^= PARS_UNEXIST;
				}
			}
		}
	}
}

/* modified copy from rayshade.c */
static void hammersley_create(float *out, int n, int seed, float amount)
{
	RNG *rng;
    double p, t, offs[2];
    int k, kk;

	rng = rng_new(31415926 + n + seed);
	offs[0]= rng_getDouble(rng) + amount;
	offs[1]= rng_getDouble(rng) + amount;
	rng_free(rng);

    for (k = 0; k < n; k++) {
        t = 0;
        for (p = 0.5, kk = k; kk; p *= 0.5, kk >>= 1)
            if (kk & 1) /* kk mod 2 = 1 */
				t += p;
    
		out[2*k + 0]= fmod((double)k/(double)n + offs[0], 1.0);
		out[2*k + 1]= fmod(t + offs[1], 1.0);
	}
}

/* modified copy from effect.c */
static void init_mv_jit(float *jit, int num, int seed2, float amount)
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
	
		jit[i]= x + amount*rad1*(0.5f - rng_getFloat(rng));
		jit[i+1]= i/(2.0f*num) + amount*rad1*(0.5f - rng_getFloat(rng));
		
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

static void psys_uv_to_w(float u, float v, int quad, float *w)
{
	float vert[4][3], co[3];

	if(!quad) {
		if(u+v > 1.0f)
			v= 1.0f-v;
		else
			u= 1.0f-u;
	}

	vert[0][0]= 0.0f; vert[0][1]= 0.0f; vert[0][2]= 0.0f;
	vert[1][0]= 1.0f; vert[1][1]= 0.0f; vert[1][2]= 0.0f;
	vert[2][0]= 1.0f; vert[2][1]= 1.0f; vert[2][2]= 0.0f;

	co[0]= u;
	co[1]= v;
	co[2]= 0.0f;

	if(quad) {
		vert[3][0]= 0.0f; vert[3][1]= 1.0f; vert[3][2]= 0.0f;
		MeanValueWeights(vert, 4, co, w);
	}
	else {
		MeanValueWeights(vert, 3, co, w);
		w[3]= 0.0f;
	}
}

static int binary_search_distribution(float *sum, int n, float value)
{
	int mid, low=0, high=n;

	while(low <= high) {
		mid= (low + high)/2;
		if(sum[mid] <= value && value <= sum[mid+1])
			return mid;
		else if(sum[mid] > value)
			high= mid - 1;
		else if(sum[mid] < value)
			low= mid + 1;
		else
			return mid;
	}

	return low;
}

/* note: this function must be thread safe, for from == PART_FROM_CHILD */
#define ONLY_WORKING_WITH_PA_VERTS 0
void psys_thread_distribute_particle(ParticleThread *thread, ParticleData *pa, ChildParticle *cpa, int p)
{
	ParticleThreadContext *ctx= thread->ctx;
	Object *ob= ctx->ob;
	DerivedMesh *dm= ctx->dm;
	ParticleData *tpa;
	ParticleSettings *part= ctx->psys->part;
	float *v1, *v2, *v3, *v4, nor[3], orco1[3], co1[3], co2[3], nor1[3], ornor1[3];
	float cur_d, min_d, randu, randv;
	int from= ctx->from;
	int cfrom= ctx->cfrom;
	int distr= ctx->distr;
	int i, intersect, tot;

	if(from == PART_FROM_VERT) {
		/* TODO_PARTICLE - use original index */
		pa->num= ctx->index[p];
		pa->fuv[0] = 1.0f;
		pa->fuv[1] = pa->fuv[2] = pa->fuv[3] = 0.0;
		//pa->verts[0] = pa->verts[1] = pa->verts[2] = 0;

#if ONLY_WORKING_WITH_PA_VERTS
		if(ctx->tree){
			KDTreeNearest ptn[3];
			int w, maxw;

			psys_particle_on_dm(ctx->dm,from,pa->num,pa->num_dmcache,pa->fuv,pa->foffset,co1,0,0,0,orco1,0);
			transform_mesh_orco_verts((Mesh*)ob->data, &orco1, 1, 1);
			maxw = BLI_kdtree_find_n_nearest(ctx->tree,3,orco1,NULL,ptn);

			for(w=0; w<maxw; w++){
				pa->verts[w]=ptn->num;
			}
		}
#endif
	}
	else if(from == PART_FROM_FACE || from == PART_FROM_VOLUME) {
		MFace *mface;

		pa->num = i = ctx->index[p];
		mface = dm->getFaceData(dm,i,CD_MFACE);
		
		switch(distr){
		case PART_DISTR_JIT:
			ctx->jitoff[i] = fmod(ctx->jitoff[i],(float)ctx->jitlevel);
			psys_uv_to_w(ctx->jit[2*(int)ctx->jitoff[i]], ctx->jit[2*(int)ctx->jitoff[i]+1], mface->v4, pa->fuv);
			ctx->jitoff[i]++;
			//ctx->jitoff[i]=(float)fmod(ctx->jitoff[i]+ctx->maxweight/ctx->weight[i],(float)ctx->jitlevel);
			break;
		case PART_DISTR_RAND:
			randu= rng_getFloat(thread->rng);
			randv= rng_getFloat(thread->rng);
			psys_uv_to_w(randu, randv, mface->v4, pa->fuv);
			break;
		}
		pa->foffset= 0.0f;
		
		/*
		pa->verts[0] = mface->v1;
		pa->verts[1] = mface->v2;
		pa->verts[2] = mface->v3;
		*/
		
		/* experimental */
		if(from==PART_FROM_VOLUME){
			MVert *mvert=dm->getVertDataArray(dm,CD_MVERT);

			tot=dm->getNumFaces(dm);

			psys_interpolate_face(mvert,mface,0,0,pa->fuv,co1,nor,0,0,0,0);

			Normalize(nor);
			VecMulf(nor,-100.0);

			VECADD(co2,co1,nor);

			min_d=2.0;
			intersect=0;

			for(i=0,mface=dm->getFaceDataArray(dm,CD_MFACE); i<tot; i++,mface++){
				if(i==pa->num) continue;

				v1=mvert[mface->v1].co;
				v2=mvert[mface->v2].co;
				v3=mvert[mface->v3].co;

				if(LineIntersectsTriangle(co1, co2, v2, v3, v1, &cur_d, 0)){
					if(cur_d<min_d){
						min_d=cur_d;
						pa->foffset=cur_d*50.0f; /* to the middle of volume */
						intersect=1;
					}
				}
				if(mface->v4){
					v4=mvert[mface->v4].co;

					if(LineIntersectsTriangle(co1, co2, v4, v1, v3, &cur_d, 0)){
						if(cur_d<min_d){
							min_d=cur_d;
							pa->foffset=cur_d*50.0f; /* to the middle of volume */
							intersect=1;
						}
					}
				}
			}
			if(intersect==0)
				pa->foffset=0.0;
			else switch(distr){
				case PART_DISTR_JIT:
					pa->foffset*= ctx->jit[2*(int)ctx->jitoff[i]];
					break;
				case PART_DISTR_RAND:
					pa->foffset*=BLI_frand();
					break;
			}
		}
	}
	else if(from == PART_FROM_PARTICLE) {
		//pa->verts[0]=0; /* not applicable */
		//pa->verts[1]=0;
		//pa->verts[2]=0;

		tpa=ctx->tpars+ctx->index[p];
		pa->num=ctx->index[p];
		pa->fuv[0]=tpa->fuv[0];
		pa->fuv[1]=tpa->fuv[1];
		/* abusing foffset a little for timing in near reaction */
		pa->foffset=ctx->weight[ctx->index[p]];
		ctx->weight[ctx->index[p]]+=ctx->maxweight;
	}
	else if(from == PART_FROM_CHILD) {
		MFace *mf;

		if(ctx->index[p] < 0) {
			cpa->num=0;
			cpa->fuv[0]=cpa->fuv[1]=cpa->fuv[2]=cpa->fuv[3]=0.0f;
			cpa->pa[0]=cpa->pa[1]=cpa->pa[2]=cpa->pa[3]=0;
			cpa->rand[0]=cpa->rand[1]=cpa->rand[2]=0.0f;
			return;
		}

		mf= dm->getFaceData(dm, ctx->index[p], CD_MFACE);

		//switch(distr){
		//	case PART_DISTR_JIT:
		//		i=index[p];
		//		psys_uv_to_w(ctx->jit[2*(int)ctx->jitoff[i]], ctx->jit[2*(int)ctx->jitoff[i]+1], mf->v4, cpa->fuv);
		//		ctx->jitoff[i]=(float)fmod(ctx->jitoff[i]+ctx->maxweight/ctx->weight[i],(float)ctx->jitlevel);
		//		break;
		//	case PART_DISTR_RAND:
				randu= rng_getFloat(thread->rng);
				randv= rng_getFloat(thread->rng);
				psys_uv_to_w(randu, randv, mf->v4, cpa->fuv);
		//		break;
		//}

		cpa->rand[0] = rng_getFloat(thread->rng);
		cpa->rand[1] = rng_getFloat(thread->rng);
		cpa->rand[2] = rng_getFloat(thread->rng);
		cpa->num = ctx->index[p];

		if(ctx->tree){
			KDTreeNearest ptn[10];
			int w,maxw, do_seams;
			float maxd,mind,dd,totw=0.0;
			int parent[10];
			float pweight[10];

			do_seams= (part->flag&PART_CHILD_SEAMS && ctx->seams);

			psys_particle_on_dm(dm,cfrom,cpa->num,DMCACHE_ISCHILD,cpa->fuv,cpa->foffset,co1,nor1,0,0,orco1,ornor1);
			transform_mesh_orco_verts((Mesh*)ob->data, &orco1, 1, 1);
			maxw = BLI_kdtree_find_n_nearest(ctx->tree,(do_seams)?10:4,orco1,ornor1,ptn);

			maxd=ptn[maxw-1].dist;
			mind=ptn[0].dist;
			dd=maxd-mind;
			
			/* the weights here could be done better */
			for(w=0; w<maxw; w++){
				parent[w]=ptn[w].index;
				pweight[w]=(float)pow(2.0,(double)(-6.0f*ptn[w].dist/maxd));
				//pweight[w]= (1.0f - ptn[w].dist*ptn[w].dist/(maxd*maxd));
				//pweight[w] *= pweight[w];
			}
			for(;w<10; w++){
				parent[w]=-1;
				pweight[w]=0.0f;
			}
			if(do_seams){
				ParticleSeam *seam=ctx->seams;
				float temp[3],temp2[3],tan[3];
				float inp,cur_len,min_len=10000.0f;
				int min_seam=0, near_vert=0;
				/* find closest seam */
				for(i=0; i<ctx->totseam; i++, seam++){
					VecSubf(temp,co1,seam->v0);
					inp=Inpf(temp,seam->dir)/seam->length2;
					if(inp<0.0f){
						cur_len=VecLenf(co1,seam->v0);
					}
					else if(inp>1.0f){
						cur_len=VecLenf(co1,seam->v1);
					}
					else{
						VecCopyf(temp2,seam->dir);
						VecMulf(temp2,inp);
						cur_len=VecLenf(temp,temp2);
					}
					if(cur_len<min_len){
						min_len=cur_len;
						min_seam=i;
						if(inp<0.0f) near_vert=-1;
						else if(inp>1.0f) near_vert=1;
						else near_vert=0;
					}
				}
				seam=ctx->seams+min_seam;
				
				VecCopyf(temp,seam->v0);
				
				if(near_vert){
					if(near_vert==-1)
						VecSubf(tan,co1,seam->v0);
					else{
						VecSubf(tan,co1,seam->v1);
						VecCopyf(temp,seam->v1);
					}

					Normalize(tan);
				}
				else{
					VecCopyf(tan,seam->tan);
					VecSubf(temp2,co1,temp);
					if(Inpf(tan,temp2)<0.0f)
						VecNegf(tan);
				}
				for(w=0; w<maxw; w++){
					VecSubf(temp2,ptn[w].co,temp);
					if(Inpf(tan,temp2)<0.0f){
						parent[w]=-1;
						pweight[w]=0.0f;
					}
				}

			}

			for(w=0,i=0; w<maxw && i<4; w++){
				if(parent[w]>=0){
					cpa->pa[i]=parent[w];
					cpa->w[i]=pweight[w];
					totw+=pweight[w];
					i++;
				}
			}
			for(;i<4; i++){
				cpa->pa[i]=-1;
				cpa->w[i]=0.0f;
			}

			if(totw>0.0f) for(w=0; w<4; w++)
				cpa->w[w]/=totw;

			cpa->parent=cpa->pa[0];
		}
	}
}

static void *exec_distribution(void *data)
{
	ParticleThread *thread= (ParticleThread*)data;
	ParticleSystem *psys= thread->ctx->psys;
	ParticleData *pa;
	ChildParticle *cpa;
	int p, totpart;

	if(thread->ctx->from == PART_FROM_CHILD) {
		totpart= psys->totchild;
		cpa= psys->child;

		for(p=0; p<totpart; p++, cpa++) {
			if(thread->ctx->skip) /* simplification skip */
				rng_skip(thread->rng, 5*thread->ctx->skip[p]);

			if((p+thread->num) % thread->tot == 0)
				psys_thread_distribute_particle(thread, NULL, cpa, p);
			else /* thread skip */
				rng_skip(thread->rng, 5);
		}
	}
	else {
		totpart= psys->totpart;
		pa= psys->particles + thread->num;
		for(p=thread->num; p<totpart; p+=thread->tot, pa+=thread->tot)
			psys_thread_distribute_particle(thread, pa, NULL, p);
	}

	return 0;
}

/* not thread safe, but qsort doesn't take userdata argument */
static int *COMPARE_ORIG_INDEX = NULL;
static int compare_orig_index(const void *p1, const void *p2)
{
	int index1 = COMPARE_ORIG_INDEX[*(const int*)p1];
	int index2 = COMPARE_ORIG_INDEX[*(const int*)p2];

	if(index1 < index2)
		return -1;
	else if(index1 == index2) {
		/* this pointer comparison appears to make qsort stable for glibc,
		 * and apparently on solaris too, makes the renders reproducable */
		if(p1 < p2)
			return -1;
		else if(p1 == p2)
			return 0;
		else
			return 1;
	}
	else
		return 1;
}

/* creates a distribution of coordinates on a DerivedMesh	*/
/*															*/
/* 1. lets check from what we are emitting					*/
/* 2. now we know that we have something to emit from so	*/
/*	  let's calculate some weights							*/
/* 2.1 from even distribution								*/
/* 2.2 and from vertex groups								*/
/* 3. next we determine the indexes of emitting thing that	*/
/*	  the particles will have								*/
/* 4. let's do jitter if we need it							*/
/* 5. now we're ready to set the indexes & distributions to	*/
/*	  the particles											*/
/* 6. and we're done!										*/

/* This is to denote functionality that does not yet work with mesh - only derived mesh */
int psys_threads_init_distribution(ParticleThread *threads, Scene *scene, DerivedMesh *finaldm, int from)
{
	ParticleThreadContext *ctx= threads[0].ctx;
	Object *ob= ctx->ob;
	ParticleSystem *psys= ctx->psys;
	Object *tob;
	ParticleData *pa=0, *tpars= 0;
	ParticleSettings *part;
	ParticleSystem *tpsys;
	ParticleSeam *seams= 0;
	ChildParticle *cpa=0;
	KDTree *tree=0;
	DerivedMesh *dm= NULL;
	float *jit= NULL;
	int i, seed, p=0, totthread= threads[0].tot;
	int no_distr=0, cfrom=0;
	int tot=0, totpart, *index=0, children=0, totseam=0;
	//int *vertpart=0;
	int jitlevel= 1, distr;
	float *weight=0,*sum=0,*jitoff=0;
	float cur, maxweight=0.0, tweight, totweight, co[3], nor[3], orco[3], ornor[3];
	
	if(ob==0 || psys==0 || psys->part==0)
		return 0;

	part=psys->part;
	totpart=psys->totpart;
	if(totpart==0)
		return 0;

	if (!finaldm->deformedOnly && !CustomData_has_layer( &finaldm->faceData, CD_ORIGINDEX ) ) {
// XXX		error("Can't paint with the current modifier stack, disable destructive modifiers");
		return 0;
	}

	BLI_srandom(31415926 + psys->seed);
	
	if(from==PART_FROM_CHILD){
		distr=PART_DISTR_RAND;
		if(part->from!=PART_FROM_PARTICLE && part->childtype==PART_CHILD_FACES){
			dm= finaldm;
			children=1;

			tree=BLI_kdtree_new(totpart);

			for(p=0,pa=psys->particles; p<totpart; p++,pa++){
				psys_particle_on_dm(dm,part->from,pa->num,pa->num_dmcache,pa->fuv,pa->foffset,co,nor,0,0,orco,ornor);
				transform_mesh_orco_verts((Mesh*)ob->data, &orco, 1, 1);
				BLI_kdtree_insert(tree, p, orco, ornor);
			}

			BLI_kdtree_balance(tree);

			totpart=get_psys_tot_child(scene, psys);
			cfrom=from=PART_FROM_FACE;

			if(part->flag&PART_CHILD_SEAMS){
				MEdge *ed, *medge=dm->getEdgeDataArray(dm,CD_MEDGE);
				MVert *mvert=dm->getVertDataArray(dm,CD_MVERT);
				int totedge=dm->getNumEdges(dm);

				for(p=0, ed=medge; p<totedge; p++,ed++)
					if(ed->flag&ME_SEAM)
						totseam++;

				if(totseam){
					ParticleSeam *cur_seam=seams=MEM_callocN(totseam*sizeof(ParticleSeam),"Child Distribution Seams");
					float temp[3],temp2[3];

					for(p=0, ed=medge; p<totedge; p++,ed++){
						if(ed->flag&ME_SEAM){
							VecCopyf(cur_seam->v0,(mvert+ed->v1)->co);
							VecCopyf(cur_seam->v1,(mvert+ed->v2)->co);

							VecSubf(cur_seam->dir,cur_seam->v1,cur_seam->v0);

							cur_seam->length2=VecLength(cur_seam->dir);
							cur_seam->length2*=cur_seam->length2;

							temp[0]=(float)((mvert+ed->v1)->no[0]);
							temp[1]=(float)((mvert+ed->v1)->no[1]);
							temp[2]=(float)((mvert+ed->v1)->no[2]);
							temp2[0]=(float)((mvert+ed->v2)->no[0]);
							temp2[1]=(float)((mvert+ed->v2)->no[1]);
							temp2[2]=(float)((mvert+ed->v2)->no[2]);

							VecAddf(cur_seam->nor,temp,temp2);
							Normalize(cur_seam->nor);

							Crossf(cur_seam->tan,cur_seam->dir,cur_seam->nor);

							Normalize(cur_seam->tan);

							cur_seam++;
						}
					}
				}
				
			}
		}
		else{
			/* no need to figure out distribution */
			int child_nbr= get_psys_child_number(scene, psys);

			totpart= get_psys_tot_child(scene, psys);
			alloc_child_particles(psys, totpart);
			cpa=psys->child;
			for(i=0; i<child_nbr; i++){
				for(p=0; p<psys->totpart; p++,cpa++){
					float length=2.0;
					cpa->parent=p;
					
					/* create even spherical distribution inside unit sphere */
					while(length>=1.0f){
						cpa->fuv[0]=2.0f*BLI_frand()-1.0f;
						cpa->fuv[1]=2.0f*BLI_frand()-1.0f;
						cpa->fuv[2]=2.0f*BLI_frand()-1.0f;
						length=VecLength(cpa->fuv);
					}

					cpa->rand[0]=BLI_frand();
					cpa->rand[1]=BLI_frand();
					cpa->rand[2]=BLI_frand();

					cpa->num=-1;
				}
			}

			return 0;
		}
	}
	else{
		dm= CDDM_from_mesh((Mesh*)ob->data, ob);

		/* special handling of grid distribution */
		if(part->distr==PART_DISTR_GRID && from != PART_FROM_VERT){
			distribute_particles_in_grid(dm,psys);
			dm->release(dm);
			return 0;
		}

		/* we need orco for consistent distributions */
		DM_add_vert_layer(dm, CD_ORCO, CD_ASSIGN, get_mesh_orco_verts(ob));

		distr=part->distr;
		pa=psys->particles;
		if(from==PART_FROM_VERT){
			MVert *mv= dm->getVertDataArray(dm, CD_MVERT);
			float (*orcodata)[3]= dm->getVertDataArray(dm, CD_ORCO);
			int totvert = dm->getNumVerts(dm);

			tree=BLI_kdtree_new(totvert);

			for(p=0; p<totvert; p++){
				if(orcodata) {
					VECCOPY(co,orcodata[p])
					transform_mesh_orco_verts((Mesh*)ob->data, &co, 1, 1);
				}
				else
					VECCOPY(co,mv[p].co)
				BLI_kdtree_insert(tree,p,co,NULL);
			}

			BLI_kdtree_balance(tree);
		}
	}

	/* 1. */
	switch(from){
		case PART_FROM_VERT:
			tot = dm->getNumVerts(dm);
			break;
		case PART_FROM_VOLUME:
		case PART_FROM_FACE:
			tot = dm->getNumFaces(dm);
			break;
		case PART_FROM_PARTICLE:
			if(psys->target_ob)
				tob=psys->target_ob;
			else
				tob=ob;

			if((tpsys=BLI_findlink(&tob->particlesystem,psys->target_psys-1))){
				tpars=tpsys->particles;
				tot=tpsys->totpart;
			}
			break;
	}

	if(tot==0){
		no_distr=1;
		if(children){
			if(G.f & G_DEBUG)
				fprintf(stderr,"Particle child distribution error: Nothing to emit from!\n");
			if(psys->child) {
				for(p=0,cpa=psys->child; p<totpart; p++,cpa++){
					cpa->fuv[0]=cpa->fuv[1]=cpa->fuv[2]=cpa->fuv[3]= 0.0;
					cpa->foffset= 0.0f;
					cpa->parent=0;
					cpa->pa[0]=cpa->pa[1]=cpa->pa[2]=cpa->pa[3]=0;
					cpa->num= -1;
				}
			}
		}
		else {
			if(G.f & G_DEBUG)
				fprintf(stderr,"Particle distribution error: Nothing to emit from!\n");
			for(p=0,pa=psys->particles; p<totpart; p++,pa++){
				pa->fuv[0]=pa->fuv[1]=pa->fuv[2]= pa->fuv[3]= 0.0;
				pa->foffset= 0.0f;
				pa->num= -1;
			}
		}

		if(dm != finaldm) dm->release(dm);
		return 0;
	}

	/* 2. */

	weight=MEM_callocN(sizeof(float)*tot, "particle_distribution_weights");
	index=MEM_callocN(sizeof(int)*totpart, "particle_distribution_indexes");
	sum=MEM_callocN(sizeof(float)*(tot+1), "particle_distribution_sum");
	jitoff=MEM_callocN(sizeof(float)*tot, "particle_distribution_jitoff");

	/* 2.1 */
	if((part->flag&PART_EDISTR || children) && ELEM(from,PART_FROM_PARTICLE,PART_FROM_VERT)==0){
		MVert *v1, *v2, *v3, *v4;
		float totarea=0.0, co1[3], co2[3], co3[3], co4[3];
		float (*orcodata)[3];
		
		orcodata= dm->getVertDataArray(dm, CD_ORCO);

		for(i=0; i<tot; i++){
			MFace *mf=dm->getFaceData(dm,i,CD_MFACE);

			if(orcodata) {
				VECCOPY(co1, orcodata[mf->v1]);
				VECCOPY(co2, orcodata[mf->v2]);
				VECCOPY(co3, orcodata[mf->v3]);
				transform_mesh_orco_verts((Mesh*)ob->data, &co1, 1, 1);
				transform_mesh_orco_verts((Mesh*)ob->data, &co2, 1, 1);
				transform_mesh_orco_verts((Mesh*)ob->data, &co3, 1, 1);
			}
			else {
				v1= (MVert*)dm->getVertData(dm,mf->v1,CD_MVERT);
				v2= (MVert*)dm->getVertData(dm,mf->v2,CD_MVERT);
				v3= (MVert*)dm->getVertData(dm,mf->v3,CD_MVERT);
				VECCOPY(co1, v1->co);
				VECCOPY(co2, v2->co);
				VECCOPY(co3, v3->co);
			}

			if (mf->v4){
				if(orcodata) {
					VECCOPY(co4, orcodata[mf->v4]);
					transform_mesh_orco_verts((Mesh*)ob->data, &co4, 1, 1);
				}
				else {
					v4= (MVert*)dm->getVertData(dm,mf->v4,CD_MVERT);
					VECCOPY(co4, v4->co);
				}
				cur= AreaQ3Dfl(co1, co2, co3, co4);
			}
			else
				cur= AreaT3Dfl(co1, co2, co3);
			
			if(cur>maxweight)
				maxweight=cur;

			weight[i]= cur;
			totarea+=cur;
		}

		for(i=0; i<tot; i++)
			weight[i] /= totarea;

		maxweight /= totarea;
	}
	else if(from==PART_FROM_PARTICLE){
		float val=(float)tot/(float)totpart;
		for(i=0; i<tot; i++)
			weight[i]=val;
		maxweight=val;
	}
	else{
		float min=1.0f/(float)(MIN2(tot,totpart));
		for(i=0; i<tot; i++)
			weight[i]=min;
		maxweight=min;
	}

	/* 2.2 */
	if(ELEM3(from,PART_FROM_VERT,PART_FROM_FACE,PART_FROM_VOLUME)){
		float *vweight= psys_cache_vgroup(dm,psys,PSYS_VG_DENSITY);

		if(vweight){
			if(from==PART_FROM_VERT) {
				for(i=0;i<tot; i++)
					weight[i]*=vweight[i];
			}
			else { /* PART_FROM_FACE / PART_FROM_VOLUME */
				for(i=0;i<tot; i++){
					MFace *mf=dm->getFaceData(dm,i,CD_MFACE);
					tweight = vweight[mf->v1] + vweight[mf->v2] + vweight[mf->v3];
				
					if(mf->v4) {
						tweight += vweight[mf->v4];
						tweight /= 4.0;
					}
					else {
						tweight /= 3.0;
					}

					weight[i]*=tweight;
				}
			}
			MEM_freeN(vweight);
		}
	}

	/* 3. */
	totweight= 0.0f;
	for(i=0;i<tot; i++)
		totweight += weight[i];

	if(totweight > 0.0f)
		totweight= 1.0f/totweight;

	sum[0]= 0.0f;
	for(i=0;i<tot; i++)
		sum[i+1]= sum[i]+weight[i]*totweight;
	
	if((part->flag&PART_TRAND) || (part->simplify_flag&PART_SIMPLIFY_ENABLE)) {
		float pos;

		for(p=0; p<totpart; p++) {
			pos= BLI_frand();
			index[p]= binary_search_distribution(sum, tot, pos);
			index[p]= MIN2(tot-1, index[p]);
			jitoff[index[p]]= pos;
		}
	}
	else {
		double step, pos;
		
		step= (totpart <= 1)? 0.5: 1.0/(totpart-1);
		pos= 1e-16f; /* tiny offset to avoid zero weight face */
		i= 0;

		for(p=0; p<totpart; p++, pos+=step) {
			while((i < tot) && (pos > sum[i+1]))
				i++;

			index[p]= MIN2(tot-1, i);

			/* avoid zero weight face */
			if(p == totpart-1 && weight[index[p]] == 0.0f)
				index[p]= index[p-1];

			jitoff[index[p]]= pos;
		}
	}

	MEM_freeN(sum);

	/* for hair, sort by origindex, allows optimizations in rendering */
	/* however with virtual parents the children need to be in random order */
	if(part->type == PART_HAIR && !(part->childtype==PART_CHILD_FACES && part->parents!=0.0)) {
		if(from != PART_FROM_PARTICLE) {
			COMPARE_ORIG_INDEX = NULL;

			if(from == PART_FROM_VERT) {
				if(dm->numVertData)
					COMPARE_ORIG_INDEX= dm->getVertDataArray(dm, CD_ORIGINDEX);
			}
			else {
				if(dm->numFaceData)
					COMPARE_ORIG_INDEX= dm->getFaceDataArray(dm, CD_ORIGINDEX);
			}

			if(COMPARE_ORIG_INDEX) {
				qsort(index, totpart, sizeof(int), compare_orig_index);
				COMPARE_ORIG_INDEX = NULL;
			}
		}
	}

	/* weights are no longer used except for FROM_PARTICLE, which needs them zeroed for indexing */
	if(from==PART_FROM_PARTICLE){
		for(i=0; i<tot; i++)
			weight[i]=0.0f;
	}

	/* 4. */
	if(distr==PART_DISTR_JIT && ELEM(from,PART_FROM_FACE,PART_FROM_VOLUME)) {
		jitlevel= part->userjit;
		
		if(jitlevel == 0) {
			jitlevel= totpart/tot;
			if(part->flag & PART_EDISTR) jitlevel*= 2;	/* looks better in general, not very scietific */
			if(jitlevel<3) jitlevel= 3;
			//if(jitlevel>100) jitlevel= 100;
		}
		
		jit= MEM_callocN((2+ jitlevel*2)*sizeof(float), "jit");

		/* for small amounts of particles we use regular jitter since it looks
		 * a bit better, for larger amounts we switch to hammersley sequence 
		 * because it is much faster */
		if(jitlevel < 25)
			init_mv_jit(jit, jitlevel, psys->seed, part->jitfac);
		else
			hammersley_create(jit, jitlevel+1, psys->seed, part->jitfac);
		BLI_array_randomize(jit, 2*sizeof(float), jitlevel, psys->seed); /* for custom jit or even distribution */
	}

	/* 5. */
	ctx->tree= tree;
	ctx->seams= seams;
	ctx->totseam= totseam;
	ctx->psys= psys;
	ctx->index= index;
	ctx->jit= jit;
	ctx->jitlevel= jitlevel;
	ctx->jitoff= jitoff;
	ctx->weight= weight;
	ctx->maxweight= maxweight;
	ctx->from= (children)? PART_FROM_CHILD: from;
	ctx->cfrom= cfrom;
	ctx->distr= distr;
	ctx->dm= dm;
	ctx->tpars= tpars;

	if(children) {
		totpart= psys_render_simplify_distribution(ctx, totpart);
		alloc_child_particles(psys, totpart);
	}

	if(!children || psys->totchild < 10000)
		totthread= 1;
	
	seed= 31415926 + ctx->psys->seed;
	for(i=0; i<totthread; i++) {
		threads[i].rng= rng_new(seed);
		threads[i].tot= totthread;
	}

	return 1;
}

static void distribute_particles_on_dm(DerivedMesh *finaldm, Scene *scene, Object *ob, ParticleSystem *psys, int from)
{
	ListBase threads;
	ParticleThread *pthreads;
	ParticleThreadContext *ctx;
	int i, totthread;

	pthreads= psys_threads_create(scene, ob, psys);

	if(!psys_threads_init_distribution(pthreads, scene, finaldm, from)) {
		psys_threads_free(pthreads);
		return;
	}

	totthread= pthreads[0].tot;
	if(totthread > 1) {
		BLI_init_threads(&threads, exec_distribution, totthread);

		for(i=0; i<totthread; i++)
			BLI_insert_thread(&threads, &pthreads[i]);

		BLI_end_threads(&threads);
	}
	else
		exec_distribution(&pthreads[0]);

	psys_calc_dmcache(ob, finaldm, psys);

	ctx= pthreads[0].ctx;
	if(ctx->dm != finaldm)
		ctx->dm->release(ctx->dm);

	psys_threads_free(pthreads);
}

/* ready for future use, to emit particles without geometry */
static void distribute_particles_on_shape(Object *ob, ParticleSystem *psys, int from)
{
	PARTICLE_P;

	fprintf(stderr,"Shape emission not yet possible!\n");

	LOOP_PARTICLES {
		pa->fuv[0]=pa->fuv[1]=pa->fuv[2]=pa->fuv[3]= 0.0;
		pa->foffset= 0.0f;
		pa->num= -1;
	}
}
static void distribute_particles(Scene *scene, Object *ob, ParticleSystem *psys, int from)
{
	ParticleSystemModifierData *psmd=0;
	int distr_error=0;
	psmd=psys_get_modifier(ob,psys);

	if(psmd){
		if(psmd->dm)
			distribute_particles_on_dm(psmd->dm, scene, ob, psys, from);
		else
			distr_error=1;
	}
	else
		distribute_particles_on_shape(ob,psys,from);

	if(distr_error){
		PARTICLE_P;

		fprintf(stderr,"Particle distribution error!\n");

		LOOP_PARTICLES {
			pa->fuv[0]=pa->fuv[1]=pa->fuv[2]=pa->fuv[3]= 0.0;
			pa->foffset= 0.0f;
			pa->num= -1;
		}
	}
}

/* threaded child particle distribution and path caching */
ParticleThread *psys_threads_create(struct Scene *scene, struct Object *ob, struct ParticleSystem *psys)
{
	ParticleThread *threads;
	ParticleThreadContext *ctx;
	int i, totthread;

	if(scene->r.mode & R_FIXED_THREADS)
		totthread= scene->r.threads;
	else
		totthread= BLI_system_thread_count();
	
	threads= MEM_callocN(sizeof(ParticleThread)*totthread, "ParticleThread");
	ctx= MEM_callocN(sizeof(ParticleThreadContext), "ParticleThreadContext");

	ctx->scene= scene;
	ctx->ob= ob;
	ctx->psys= psys;
	ctx->psmd= psys_get_modifier(ob, psys);
	ctx->dm= ctx->psmd->dm;
	ctx->ma= give_current_material(ob, psys->part->omat);

	memset(threads, 0, sizeof(ParticleThread)*totthread);

	for(i=0; i<totthread; i++) {
		threads[i].ctx= ctx;
		threads[i].num= i;
		threads[i].tot= totthread;
	}

	return threads;
}

void psys_threads_free(ParticleThread *threads)
{
	ParticleThreadContext *ctx= threads[0].ctx;
	int i, totthread= threads[0].tot;

	/* path caching */
	if(ctx->vg_length)
		MEM_freeN(ctx->vg_length);
	if(ctx->vg_clump)
		MEM_freeN(ctx->vg_clump);
	if(ctx->vg_kink)
		MEM_freeN(ctx->vg_kink);
	if(ctx->vg_rough1)
		MEM_freeN(ctx->vg_rough1);
	if(ctx->vg_rough2)
		MEM_freeN(ctx->vg_rough2);
	if(ctx->vg_roughe)
		MEM_freeN(ctx->vg_roughe);

	if(ctx->psys->lattice){
		end_latt_deform(ctx->psys->lattice);
		ctx->psys->lattice= NULL;
	}

	/* distribution */
	if(ctx->jit) MEM_freeN(ctx->jit);
	if(ctx->jitoff) MEM_freeN(ctx->jitoff);
	if(ctx->weight) MEM_freeN(ctx->weight);
	if(ctx->index) MEM_freeN(ctx->index);
	if(ctx->skip) MEM_freeN(ctx->skip);
	if(ctx->seams) MEM_freeN(ctx->seams);
	//if(ctx->vertpart) MEM_freeN(ctx->vertpart);
	BLI_kdtree_free(ctx->tree);

	/* threads */
	for(i=0; i<totthread; i++) {
		if(threads[i].rng)
			rng_free(threads[i].rng);
		if(threads[i].rng_path)
			rng_free(threads[i].rng_path);
	}

	MEM_freeN(ctx);
	MEM_freeN(threads);
}

/* set particle parameters that don't change during particle's life */
void initialize_particle(ParticleData *pa, int p, Object *ob, ParticleSystem *psys, ParticleSystemModifierData *psmd)
{
	ParticleSettings *part;
	ParticleTexture ptex;
	Material *ma=0;
	//IpoCurve *icu=0; // XXX old animation system
	int totpart;
	float rand;

	part=psys->part;

	totpart=psys->totpart;

	ptex.life=ptex.size=ptex.exist=ptex.length=1.0;
	ptex.time=(float)p/(float)totpart;

	BLI_srandom(psys->seed+p);

	if(part->from!=PART_FROM_PARTICLE && part->type!=PART_FLUID){
		ma=give_current_material(ob,part->omat);

		/* TODO: needs some work to make most blendtypes generally usefull */
		psys_get_texture(ob,ma,psmd,psys,pa,&ptex,MAP_PA_INIT);
	}
	
	pa->lifetime= part->lifetime*ptex.life;

	if(part->type==PART_HAIR)
		pa->time= 0.0f;
	else if(part->type==PART_REACTOR && (part->flag&PART_REACT_STA_END)==0)
		pa->time= 300000.0f;	/* max frame */
	else{
		//icu=find_ipocurve(psys->part->ipo,PART_EMIT_TIME);
		//if(icu){
		//	calc_icu(icu,100*ptex.time);
		//	ptex.time=icu->curval;
		//}

		pa->time= part->sta + (part->end - part->sta)*ptex.time;
	}


	if(part->type==PART_HAIR){
		pa->lifetime=100.0f;
	}
	else{
#if 0 // XXX old animation system
		icu=find_ipocurve(psys->part->ipo,PART_EMIT_LIFE);
		if(icu){
			calc_icu(icu,100*ptex.time);
			pa->lifetime*=icu->curval;
		}
#endif // XXX old animation system

	/* need to get every rand even if we don't use them so that randoms don't affect each other */
		rand= BLI_frand();
		if(part->randlife!=0.0)
			pa->lifetime*= 1.0f - part->randlife*rand;
	}

	pa->dietime= pa->time+pa->lifetime;

	if(part->type!=PART_HAIR && part->distr!=PART_DISTR_GRID && part->from != PART_FROM_VERT){
		if(ptex.exist < BLI_frand())
			pa->flag |= PARS_UNEXIST;
		else
			pa->flag &= ~PARS_UNEXIST;
	}

	pa->loop=0;
	/* we can't reset to -1 anymore since we've figured out correct index in distribute_particles */
	/* usage other than straight after distribute has to handle this index by itself - jahka*/
	//pa->num_dmcache = DMCACHE_NOTFOUND; /* assume we dont have a derived mesh face */
}
static void initialize_all_particles(Object *ob, ParticleSystem *psys, ParticleSystemModifierData *psmd)
{
	//IpoCurve *icu=0; // XXX old animation system
	PARTICLE_P;

	LOOP_PARTICLES
		initialize_particle(pa,p,ob,psys,psmd);
	
	if(psys->part->type != PART_FLUID) {
#if 0 // XXX old animation system
		icu=find_ipocurve(psys->part->ipo,PART_EMIT_FREQ);
		if(icu){
			float time=psys->part->sta, end=psys->part->end;
			float v1, v2, a=0.0f, t1,t2, d;

			p=0;
			pa=psys->particles;


			calc_icu(icu,time);
			v1=icu->curval;
			if(v1<0.0f) v1=0.0f;

			calc_icu(icu,time+1.0f);
			v2=icu->curval;
			if(v2<0.0f) v2=0.0f;

			for(p=0, pa=psys->particles; p<totpart && time<end; p++, pa++){
				while(a+0.5f*(v1+v2) < (float)(p+1) && time<end){
					a+=0.5f*(v1+v2);
					v1=v2;
					time++;
					calc_icu(icu,time+1.0f);
					v2=icu->curval;
				}
				if(time<end){
					if(v1==v2){
						pa->time=time+((float)(p+1)-a)/v1;
					}
					else{
						d=(float)sqrt(v1*v1-2.0f*(v2-v1)*(a-(float)(p+1)));
						t1=(-v1+d)/(v2-v1);
						t2=(-v1-d)/(v2-v1);

						/* the root between 0-1 is the correct one */
						if(t1>0.0f && t1<=1.0f)
							pa->time=time+t1;
						else
							pa->time=time+t2;
					}
				}

				pa->dietime = pa->time+pa->lifetime;
				pa->flag &= ~PARS_UNEXIST;
			}
			for(; p<totpart; p++, pa++){
				pa->flag |= PARS_UNEXIST;
			}
		}
#endif // XXX old animation system
	}
}
/* sets particle to the emitter surface with initial velocity & rotation */
void reset_particle(Scene *scene, ParticleData *pa, ParticleSystem *psys, ParticleSystemModifierData *psmd, Object *ob,
					float dtime, float cfra, float *vg_vel, float *vg_tan, float *vg_rot)
{
	ParticleSettings *part;
	ParticleTexture ptex;
	ParticleKey state;
	//IpoCurve *icu=0; // XXX old animation system
	float fac, phasefac, nor[3]={0,0,0},loc[3],tloc[3],vel[3]={0.0,0.0,0.0},rot[4],q2[4];
	float r_vel[3],r_ave[3],r_rot[4],p_vel[3]={0.0,0.0,0.0};
	float x_vec[3]={1.0,0.0,0.0}, utan[3]={0.0,1.0,0.0}, vtan[3]={0.0,0.0,1.0}, rot_vec[3]={0.0,0.0,0.0};
	float q_phase[4], length, r_phase;
	part=psys->part;

	ptex.ivel=1.0;

	BLI_srandom(psys->seed + (pa - psys->particles));

	/* we need to get every random even if they're not used so that they don't effect eachother */
	/* while loops are to have a spherical distribution (avoid cubic distribution) */
	length=2.0f;
	while(length>1.0){
		r_vel[0]=2.0f*(BLI_frand()-0.5f);
		r_vel[1]=2.0f*(BLI_frand()-0.5f);
		r_vel[2]=2.0f*(BLI_frand()-0.5f);
		length=VecLength(r_vel);
	}

	length=2.0f;
	while(length>1.0){
		r_ave[0]=2.0f*(BLI_frand()-0.5f);
		r_ave[1]=2.0f*(BLI_frand()-0.5f);
		r_ave[2]=2.0f*(BLI_frand()-0.5f);
		length=VecLength(r_ave);
	}

	r_rot[0]=2.0f*(BLI_frand()-0.5f);
	r_rot[1]=2.0f*(BLI_frand()-0.5f);
	r_rot[2]=2.0f*(BLI_frand()-0.5f);
	r_rot[3]=2.0f*(BLI_frand()-0.5f);

	NormalQuat(r_rot);

	r_phase = BLI_frand();
	
	if(part->from==PART_FROM_PARTICLE){
		Object *tob;
		ParticleSystem *tpsys=0;
		float speed;

		tob=psys->target_ob;
		if(tob==0)
			tob=ob;

		tpsys=BLI_findlink(&tob->particlesystem, psys->target_psys-1);

		state.time = pa->time;
		if(pa->num == -1)
			memset(&state, 0, sizeof(state));
		else
			psys_get_particle_state(scene, tob,tpsys,pa->num,&state,1);
		psys_get_from_key(&state, loc, nor, rot, 0);

		QuatMulVecf(rot, vtan);
		QuatMulVecf(rot, utan);

		VECCOPY(p_vel, state.vel);
		speed=Normalize(p_vel);
		VecMulf(p_vel, Inpf(r_vel, p_vel));
		VECSUB(p_vel, r_vel, p_vel);
		Normalize(p_vel);
		VecMulf(p_vel, speed);

		VECCOPY(pa->fuv, loc); /* abusing pa->fuv (not used for "from particle") for storing emit location */
	}
	else{
		/* get precise emitter matrix if particle is born */
		if(part->type!=PART_HAIR && pa->time < cfra && pa->time >= psys->cfra)
			where_is_object_time(scene, ob,pa->time);

		/* get birth location from object		*/
		if(part->tanfac!=0.0)
			psys_particle_on_emitter(psmd,part->from,pa->num, pa->num_dmcache, pa->fuv,pa->foffset,loc,nor,utan,vtan,0,0);
		else
			psys_particle_on_emitter(psmd,part->from,pa->num, pa->num_dmcache, pa->fuv,pa->foffset,loc,nor,0,0,0,0);
		
		/* save local coordinates for later		*/
		VECCOPY(tloc,loc);
		
		/* get possible textural influence */
		psys_get_texture(ob,give_current_material(ob,part->omat),psmd,psys,pa,&ptex,MAP_PA_IVEL);

		if(vg_vel && pa->num != -1)
			ptex.ivel*=psys_particle_value_from_verts(psmd->dm,part->from,pa,vg_vel);

		/* particles live in global space so	*/
		/* let's convert:						*/
		/* -location							*/
		Mat4MulVecfl(ob->obmat,loc);
		
		/* -normal								*/
		VECADD(nor,tloc,nor);
		Mat4MulVecfl(ob->obmat,nor);
		VECSUB(nor,nor,loc);
		Normalize(nor);

		/* -tangent								*/
		if(part->tanfac!=0.0){
			float phase=vg_rot?2.0f*(psys_particle_value_from_verts(psmd->dm,part->from,pa,vg_rot)-0.5f):0.0f;
			VecMulf(vtan,-(float)cos(M_PI*(part->tanphase+phase)));
			fac=-(float)sin(M_PI*(part->tanphase+phase));
			VECADDFAC(vtan,vtan,utan,fac);

			VECADD(vtan,tloc,vtan);
			Mat4MulVecfl(ob->obmat,vtan);
			VECSUB(vtan,vtan,loc);

			VECCOPY(utan,nor);
			VecMulf(utan,Inpf(vtan,nor));
			VECSUB(vtan,vtan,utan);
			
			Normalize(vtan);
		}
		

		/* -velocity							*/
		if(part->randfac!=0.0){
			Mat4Mul3Vecfl(ob->obmat,r_vel);
			Normalize(r_vel);
		}

		/* -angular velocity					*/
		if(part->avemode==PART_AVE_RAND){
			Mat4Mul3Vecfl(ob->obmat,r_ave);
			Normalize(r_ave);
		}
		
		/* -rotation							*/
		if(part->randrotfac != 0.0f){
			Mat4ToQuat(ob->obmat,rot);
			QuatMul(r_rot,r_rot,rot);
		}
	}

	if(part->phystype==PART_PHYS_BOIDS) {
		BoidParticle *bpa = pa->boid;
		float dvec[3], q[4], mat[3][3];

		VECCOPY(pa->state.co,loc);

		/* boids don't get any initial velocity  */
		pa->state.vel[0]=pa->state.vel[1]=pa->state.vel[2]=0.0f;

		/* boids store direction in ave */
		if(fabs(nor[2])==1.0f) {
			VecSubf(pa->state.ave, loc, ob->obmat[3]);
			Normalize(pa->state.ave);
		}
		else {
			VECCOPY(pa->state.ave, nor);
		}
		/* and gravity in r_ve */
		bpa->gravity[0] = bpa->gravity[1] = 0.0f;
		bpa->gravity[2] = -1.0f;
		if(part->acc[2]!=0.0f)
			bpa->gravity[2] = part->acc[2];

		//pa->r_ve[0] = pa->r_ve[1] = 0.0f;
		//pa->r_ve[2] = -1.0f;
		//if(part->acc[2]!=0.0f)
		//	pa->r_ve[2] = part->acc[2];

		/* calculate rotation matrix */
		Projf(dvec, r_vel, pa->state.ave);
		VecSubf(mat[0], pa->state.ave, dvec);
		Normalize(mat[0]);
		VECCOPY(mat[2], r_vel);
		VecMulf(mat[2], -1.0f);
		Normalize(mat[2]);
		Crossf(mat[1], mat[2], mat[0]);
		
		/* apply rotation */
		Mat3ToQuat_is_ok(mat, q);
		QuatCopy(pa->state.rot, q);

		bpa->data.health = part->boids->health;
		bpa->data.mode = eBoidMode_InAir;
		bpa->data.state_id = ((BoidState*)part->boids->states.first)->id;
		bpa->data.acc[0]=bpa->data.acc[1]=bpa->data.acc[2]=0.0f;
	}
	else {
		/* conversion done so now we apply new:	*/
		/* -velocity from:						*/

		/*		*reactions						*/
		if(dtime>0.0f){
			VECSUB(vel,pa->state.vel,pa->prev_state.vel);
		}

		/*		*emitter velocity				*/
		if(dtime!=0.0 && part->obfac!=0.0){
			VECSUB(vel,loc,pa->state.co);
			VecMulf(vel,part->obfac/dtime);
		}
		
		/*		*emitter normal					*/
		if(part->normfac!=0.0)
			VECADDFAC(vel,vel,nor,part->normfac);
		
		/*		*emitter tangent				*/
		if(psmd && part->tanfac!=0.0)
			VECADDFAC(vel,vel,vtan,part->tanfac*(vg_tan?psys_particle_value_from_verts(psmd->dm,part->from,pa,vg_tan):1.0f));

		/*		*texture						*/
		/* TODO	*/

		/*		*random							*/
		if(part->randfac!=0.0)
			VECADDFAC(vel,vel,r_vel,part->randfac);

		/*		*particle						*/
		if(part->partfac!=0.0)
			VECADDFAC(vel,vel,p_vel,part->partfac);

		//icu=find_ipocurve(psys->part->ipo,PART_EMIT_VEL);
		//if(icu){
		//	calc_icu(icu,100*((pa->time-part->sta)/(part->end-part->sta)));
		//	ptex.ivel*=icu->curval;
		//}

		VecMulf(vel,ptex.ivel);

		//if(ELEM(part->phystype, PART_PHYS_GRADU_EX, PART_PHYS_GRADU_SIM))
		//	VecAddf(vel,vel,part->acc);
		
		VECCOPY(pa->state.vel,vel);

		/* -location from emitter				*/
		VECCOPY(pa->state.co,loc);

		/* -rotation							*/
		pa->state.rot[0]=1.0;
		pa->state.rot[1]=pa->state.rot[2]=pa->state.rot[3]=0.0;

		if(part->rotmode){
			/* create vector into which rotation is aligned */
			switch(part->rotmode){
				case PART_ROT_NOR:
					VecCopyf(rot_vec, nor);
					break;
				case PART_ROT_VEL:
					VecCopyf(rot_vec, vel);
					break;
				case PART_ROT_GLOB_X:
				case PART_ROT_GLOB_Y:
				case PART_ROT_GLOB_Z:
					rot_vec[part->rotmode - PART_ROT_GLOB_X] = 1.0f;
					break;
				case PART_ROT_OB_X:
				case PART_ROT_OB_Y:
				case PART_ROT_OB_Z:
					VecCopyf(rot_vec, ob->obmat[part->rotmode - PART_ROT_OB_X]);
					break;
			}
			
			/* create rotation quat */
			VecNegf(rot_vec);
			vectoquat(rot_vec, OB_POSX, OB_POSZ, q2);

			/* randomize rotation quat */
			if(part->randrotfac!=0.0f)
				QuatInterpol(rot, q2, r_rot, part->randrotfac);
			else
				QuatCopy(rot,q2);

			/* rotation phase */
			phasefac = part->phasefac;
			if(part->randphasefac != 0.0f)
				phasefac += part->randphasefac * r_phase;
			VecRotToQuat(x_vec, phasefac*(float)M_PI, q_phase);

			/* combine base rotation & phase */
			QuatMul(pa->state.rot, rot, q_phase);
		}

		/* -angular velocity					*/

		pa->state.ave[0] = pa->state.ave[1] = pa->state.ave[2] = 0.0;

		if(part->avemode){
			switch(part->avemode){
				case PART_AVE_SPIN:
					VECCOPY(pa->state.ave,vel);
					break;
				case PART_AVE_RAND:
					VECCOPY(pa->state.ave,r_ave);
					break;
			}
			Normalize(pa->state.ave);
			VecMulf(pa->state.ave,part->avefac);

			//icu=find_ipocurve(psys->part->ipo,PART_EMIT_AVE);
			//if(icu){
			//	calc_icu(icu,100*((pa->time-part->sta)/(part->end-part->sta)));
			//	VecMulf(pa->state.ave,icu->curval);
			//}
		}
	}

	pa->dietime = pa->time + pa->lifetime;

	if(pa->time >= cfra)
		pa->alive = PARS_UNBORN;

	pa->state.time = cfra;

//	pa->flag &= ~PARS_STICKY;
}
static void reset_all_particles(Scene *scene, Object *ob, ParticleSystem *psys, ParticleSystemModifierData *psmd, float dtime, float cfra, int from)
{
	ParticleData *pa;
	int p, totpart=psys->totpart;
	float *vg_vel=psys_cache_vgroup(psmd->dm,psys,PSYS_VG_VEL);
	float *vg_tan=psys_cache_vgroup(psmd->dm,psys,PSYS_VG_TAN);
	float *vg_rot=psys_cache_vgroup(psmd->dm,psys,PSYS_VG_ROT);
	
	for(p=from, pa=psys->particles+from; p<totpart; p++, pa++)
		reset_particle(scene, pa, psys, psmd, ob, dtime, cfra, vg_vel, vg_tan, vg_rot);

	if(vg_vel)
		MEM_freeN(vg_vel);
}
/************************************************/
/*			Particle targets					*/
/************************************************/
ParticleSystem *psys_get_target_system(Object *ob, ParticleTarget *pt)
{
	ParticleSystem *psys = NULL;

	if(pt->ob == NULL || pt->ob == ob)
		psys = BLI_findlink(&ob->particlesystem, pt->psys-1);
	else
		psys = BLI_findlink(&pt->ob->particlesystem, pt->psys-1);

	if(psys)
		pt->flag |= PTARGET_VALID;
	else
		pt->flag &= ~PTARGET_VALID;

	return psys;
}
/************************************************/
/*			Keyed particles						*/
/************************************************/
/* Counts valid keyed targets */
void psys_count_keyed_targets(Object *ob, ParticleSystem *psys)
{
	ParticleSystem *kpsys;
	ParticleTarget *pt = psys->targets.first;
	int keys_valid = 1;
	psys->totkeyed = 0;

	for(; pt; pt=pt->next) {
		kpsys = psys_get_target_system(ob, pt);

		if(kpsys && kpsys->totpart) {
			psys->totkeyed += keys_valid;
			if(psys->flag & PSYS_KEYED_TIMING && pt->duration != 0.0f)
				psys->totkeyed += 1;
		}
		else {
			keys_valid = 0;
		}
	}

	psys->totkeyed *= psys->flag & PSYS_KEYED_TIMING ? 1 : psys->part->keyed_loops;
}

static void set_keyed_keys(Scene *scene, Object *ob, ParticleSystem *psys)
{
	ParticleSystem *kpsys = psys;
	ParticleTarget *pt;
	PARTICLE_P;
	ParticleKey *key;
	int totpart = psys->totpart, k, totkeys = psys->totkeyed;

	/* no proper targets so let's clear and bail out */
	if(psys->totkeyed==0) {
		free_keyed_keys(psys);
		psys->flag &= ~PSYS_KEYED;
		return;
	}

	if(totpart && psys->particles->totkey != totkeys) {
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
	for(k=0; k<totkeys; k++) {
		if(pt->ob)
			kpsys = BLI_findlink(&pt->ob->particlesystem, pt->psys - 1);
		else
			kpsys = BLI_findlink(&ob->particlesystem, pt->psys - 1);

		LOOP_PARTICLES {
			key = pa->keys + k;
			key->time = -1.0; /* use current time */

			psys_get_particle_state(scene, pt->ob, kpsys, p%kpsys->totpart, key, 1);

			if(psys->flag & PSYS_KEYED_TIMING){
				key->time = pa->time + pt->time;
				if(pt->duration != 0.0f && k+1 < totkeys) {
					copy_particle_key(key+1, key, 1);
					(key+1)->time = pa->time + pt->time + pt->duration;
				}
			}
			else if(totkeys > 1)
				key->time = pa->time + (float)k / (float)(totkeys - 1) * pa->lifetime;
			else
				key->time = pa->time;
		}

		if(psys->flag & PSYS_KEYED_TIMING && pt->duration!=0.0f)
			k++;

		pt = (pt->next && pt->next->flag & PTARGET_VALID)? pt->next : psys->targets.first;
	}

	psys->flag |= PSYS_KEYED;
}
/************************************************/
/*			Reactors							*/
/************************************************/
static void push_reaction(Object* ob, ParticleSystem *psys, int pa_num, int event, ParticleKey *state)
{
	Object *rob;
	ParticleSystem *rpsys;
	ParticleSettings *rpart;
	ParticleData *pa;
	ListBase *lb=&psys->effectors;
	ParticleEffectorCache *ec;
	ParticleReactEvent *re;

	if(lb->first) for(ec = lb->first; ec; ec= ec->next){
		if(ec->type & PSYS_EC_REACTOR){
			/* all validity checks already done in add_to_effectors */
			rob=ec->ob;
			rpsys=BLI_findlink(&rob->particlesystem,ec->psys_nbr);
			rpart=rpsys->part;
			if(rpsys->part->reactevent==event){
				pa=psys->particles+pa_num;
				re= MEM_callocN(sizeof(ParticleReactEvent), "react event");
				re->event=event;
				re->pa_num = pa_num;
				re->ob = ob;
				re->psys = psys;
				re->size = pa->size;
				copy_particle_key(&re->state,state,1);

				switch(event){
					case PART_EVENT_DEATH:
						re->time=pa->dietime;
						break;
					case PART_EVENT_COLLIDE:
						re->time=state->time;
						break;
					case PART_EVENT_NEAR:
						re->time=state->time;
						break;
				}

				BLI_addtail(&rpsys->reactevents, re);
			}
		}
	}
}
static void react_to_events(ParticleSystem *psys, int pa_num)
{
	ParticleSettings *part=psys->part;
	ParticleData *pa=psys->particles+pa_num;
	ParticleReactEvent *re=psys->reactevents.first;
	int birth=0;
	float dist=0.0f;

	for(re=psys->reactevents.first; re; re=re->next){
		birth=0;
		if(part->from==PART_FROM_PARTICLE){
			if(pa->num==re->pa_num && pa->alive==PARS_UNBORN){
				if(re->event==PART_EVENT_NEAR){
					ParticleData *tpa = re->psys->particles+re->pa_num;
					float pa_time=tpa->time + pa->foffset*tpa->lifetime;
					if(re->time >= pa_time){
						pa->time=pa_time;
						pa->dietime=pa->time+pa->lifetime;
					}
				}
				else{
					pa->time=re->time;
					pa->dietime=pa->time+pa->lifetime;
				}
			}
		}
		else{
			dist=VecLenf(pa->state.co, re->state.co);
			if(dist <= re->size){
				if(pa->alive==PARS_UNBORN){
					pa->time=re->time;
					pa->dietime=pa->time+pa->lifetime;
					birth=1;
				}
				if(birth || part->flag&PART_REACT_MULTIPLE){
					float vec[3];
					VECSUB(vec,pa->state.co, re->state.co);
					if(birth==0)
						VecMulf(vec,(float)pow(1.0f-dist/re->size,part->reactshape));
					VECADDFAC(pa->state.vel,pa->state.vel,vec,part->reactfac);
					VECADDFAC(pa->state.vel,pa->state.vel,re->state.vel,part->partfac);
				}
				if(birth)
					VecMulf(pa->state.vel,(float)pow(1.0f-dist/re->size,part->reactshape));
			}
		}
	}
}
void psys_get_reactor_target(Object *ob, ParticleSystem *psys, Object **target_ob, ParticleSystem **target_psys)
{
	Object *tob;

	tob=psys->target_ob;
	if(tob==0)
		tob=ob;
	
	*target_psys=BLI_findlink(&tob->particlesystem,psys->target_psys-1);
	if(*target_psys)
		*target_ob=tob;
	else
		*target_ob=0;
}
/************************************************/
/*			Point Cache							*/
/************************************************/
void psys_make_temp_pointcache(Object *ob, ParticleSystem *psys)
{
	PointCache *cache = psys->pointcache;
	PTCacheID pid;

	if((cache->flag & PTCACHE_DISK_CACHE)==0 || cache->mem_cache.first)
		return;

	BKE_ptcache_id_from_particles(&pid, ob, psys);

	BKE_ptcache_disk_to_mem(&pid);
}
void psys_clear_temp_pointcache(ParticleSystem *psys)
{
	if((psys->pointcache->flag & PTCACHE_DISK_CACHE)==0)
		return;

	BKE_ptcache_free_mem(&psys->pointcache->mem_cache);
}
void psys_get_pointcache_start_end(Scene *scene, ParticleSystem *psys, int *sfra, int *efra)
{
	ParticleSettings *part = psys->part;

	*sfra = MAX2(1, (int)part->sta);
	*efra = MIN2((int)(part->end + part->lifetime + 1.0), scene->r.efra);
}

/************************************************/
/*			Effectors							*/
/************************************************/
static void update_particle_tree(ParticleSystem *psys)
{
	if(psys) {
		PARTICLE_P;

		if(!psys->tree || psys->tree_frame != psys->cfra) {
			
			BLI_kdtree_free(psys->tree);

			psys->tree = BLI_kdtree_new(psys->totpart);
			
			LOOP_PARTICLES {
				if(pa->flag & (PARS_NO_DISP+PARS_UNEXIST) || pa->alive != PARS_ALIVE)
					continue;

				BLI_kdtree_insert(psys->tree, p, pa->state.co, NULL);
			}
			BLI_kdtree_balance(psys->tree);

			psys->tree_frame = psys->cfra;
		}
	}
}
static void do_texture_effector(Tex *tex, short mode, short is_2d, float nabla, short object, float *pa_co, float obmat[4][4], float force_val, float falloff, float *field)
{
	TexResult result[4];
	float tex_co[3], strength, mag_vec[3];
	int hasrgb;
	if(tex==NULL) return;

	result[0].nor = result[1].nor = result[2].nor = result[3].nor = 0;

	strength= force_val*falloff;///(float)pow((double)distance,(double)power);

	VECCOPY(tex_co,pa_co);

	if(is_2d){
		float fac=-Inpf(tex_co,obmat[2]);
		VECADDFAC(tex_co,tex_co,obmat[2],fac);
	}

	if(object){
		VecSubf(tex_co,tex_co,obmat[3]);
		Mat4Mul3Vecfl(obmat,tex_co);
	}

	hasrgb = multitex_ext(tex, tex_co, NULL,NULL, 1, result);

	if(hasrgb && mode==PFIELD_TEX_RGB){
		mag_vec[0]= (0.5f-result->tr)*strength;
		mag_vec[1]= (0.5f-result->tg)*strength;
		mag_vec[2]= (0.5f-result->tb)*strength;
	}
	else{
		strength/=nabla;

		tex_co[0]+= nabla;
		multitex_ext(tex, tex_co, NULL,NULL, 1, result+1);

		tex_co[0]-= nabla;
		tex_co[1]+= nabla;
		multitex_ext(tex, tex_co, NULL,NULL, 1, result+2);

		tex_co[1]-= nabla;
		tex_co[2]+= nabla;
		multitex_ext(tex, tex_co, NULL,NULL, 1, result+3);

		if(mode==PFIELD_TEX_GRAD || !hasrgb){ /* if we dont have rgb fall back to grad */
			mag_vec[0]= (result[0].tin-result[1].tin)*strength;
			mag_vec[1]= (result[0].tin-result[2].tin)*strength;
			mag_vec[2]= (result[0].tin-result[3].tin)*strength;
		}
		else{ /*PFIELD_TEX_CURL*/
			float dbdy,dgdz,drdz,dbdx,dgdx,drdy;

			dbdy= result[2].tb-result[0].tb;
			dgdz= result[3].tg-result[0].tg;
			drdz= result[3].tr-result[0].tr;
			dbdx= result[1].tb-result[0].tb;
			dgdx= result[1].tg-result[0].tg;
			drdy= result[2].tr-result[0].tr;

			mag_vec[0]=(dbdy-dgdz)*strength;
			mag_vec[1]=(drdz-dbdx)*strength;
			mag_vec[2]=(dgdx-drdy)*strength;
		}
	}

	if(is_2d){
		float fac=-Inpf(mag_vec,obmat[2]);
		VECADDFAC(mag_vec,mag_vec,obmat[2],fac);
	}

	VecAddf(field,field,mag_vec);
}
static void add_to_effectors(ListBase *lb, Scene *scene, Object *ob, Object *obsrc, ParticleSystem *psys)
{
	ParticleEffectorCache *ec;
	PartDeflect *pd= ob->pd;
	short type=0,i;

	if(pd && ob != obsrc){
		if(pd->forcefield == PFIELD_GUIDE) {
			if(ob->type==OB_CURVE) {
				Curve *cu= ob->data;
				if(cu->flag & CU_PATH) {
					if(cu->path==NULL || cu->path->data==NULL)
						makeDispListCurveTypes(scene, ob, 0);
					if(cu->path && cu->path->data) {
						type |= PSYS_EC_EFFECTOR;
					}
				}
			}
		}
		else if(pd->forcefield)
		{
			type |= PSYS_EC_EFFECTOR;
		}
	}
	
	if(pd && pd->deflect)
		type |= PSYS_EC_DEFLECT;

	if(type){
		ec= MEM_callocN(sizeof(ParticleEffectorCache), "effector cache");
		ec->ob= ob;
		ec->type=type;
		ec->distances=0;
		ec->locations=0;
		ec->rng = rng_new(1);
		rng_srandom(ec->rng, (unsigned int)(ceil(PIL_check_seconds_timer()))); // use better seed
		
		BLI_addtail(lb, ec);
	}

	type=0;

	/* add particles as different effectors */
	if(ob->particlesystem.first){
		ParticleSystem *epsys=ob->particlesystem.first;
		ParticleSettings *epart=0;
		Object *tob;

		for(i=0; epsys; epsys=epsys->next,i++){
			if(!psys_check_enabled(ob, epsys))
				continue;
			type=0;
			if(epsys!=psys || (psys->part->flag & PART_SELF_EFFECT)){
				epart=epsys->part;

				if((epsys->part->pd && epsys->part->pd->forcefield)
					|| (epsys->part->pd2 && epsys->part->pd2->forcefield))
				{
					type=PSYS_EC_PARTICLE;
				}

				if(epart->type==PART_REACTOR) {
					tob=epsys->target_ob;
					if(tob==0)
						tob=ob;
					if(BLI_findlink(&tob->particlesystem,epsys->target_psys-1)==psys)
						type|=PSYS_EC_REACTOR;
				}

				if(type){
					ec= MEM_callocN(sizeof(ParticleEffectorCache), "effector cache");
					ec->ob= ob;
					ec->type=type;
					ec->psys_nbr=i;
					ec->rng = rng_new(1);
					rng_srandom(ec->rng, (unsigned int)(ceil(PIL_check_seconds_timer())));
					
					BLI_addtail(lb, ec);
				}
			}
		}
				
	}
}

static void psys_init_effectors_recurs(Scene *scene, Object *ob, Object *obsrc, ParticleSystem *psys, ListBase *listb, int level)
{
	Group *group;
	GroupObject *go;
	unsigned int layer= obsrc->lay;

	if(level>MAX_DUPLI_RECUR) return;

	if(ob->lay & layer) {
		if(ob->pd || ob->particlesystem.first)
			add_to_effectors(listb, scene, ob, obsrc, psys);

		if(ob->dup_group) {
			group= ob->dup_group;
			for(go= group->gobject.first; go; go= go->next)
				psys_init_effectors_recurs(scene, go->ob, obsrc, psys, listb, level+1);
		}
	}
}

void psys_init_effectors(Scene *scene, Object *obsrc, Group *group, ParticleSystem *psys)
{
	ListBase *listb= &psys->effectors;
	Base *base;

	listb->first=listb->last=0;
	
	if(group) {
		GroupObject *go;
		
		for(go= group->gobject.first; go; go= go->next)
			psys_init_effectors_recurs(scene, go->ob, obsrc, psys, listb, 0);
	}
	else {
		for(base = scene->base.first; base; base= base->next)
			psys_init_effectors_recurs(scene, base->object, obsrc, psys, listb, 0);
	}
}

void psys_end_effectors(ParticleSystem *psys)
{
	/* NOTE:
	ec->ob is not valid in here anymore! - dg
	*/
	ParticleEffectorCache *ec = psys->effectors.first;

	for(; ec; ec= ec->next){
		if(ec->distances)
			MEM_freeN(ec->distances);

		if(ec->locations)
			MEM_freeN(ec->locations);

		if(ec->face_minmax)
			MEM_freeN(ec->face_minmax);

		if(ec->vert_cos)
			MEM_freeN(ec->vert_cos);

		if(ec->tree)
			BLI_kdtree_free(ec->tree);
		
		if(ec->rng)
			rng_free(ec->rng);
	}

	BLI_freelistN(&psys->effectors);
}

static void precalc_effectors(Scene *scene, Object *ob, ParticleSystem *psys, ParticleSystemModifierData *psmd, float cfra)
{
	ListBase *lb=&psys->effectors;
	ParticleEffectorCache *ec;
	ParticleSettings *part=psys->part;
	PARTICLE_P;
	int totpart;
	float vec2[3],loc[3],*co=0;
	
	for(ec= lb->first; ec; ec= ec->next) {
		PartDeflect *pd= ec->ob->pd;
		co = NULL;
		
		if(ec->type==PSYS_EC_EFFECTOR && pd->forcefield==PFIELD_GUIDE && ec->ob->type==OB_CURVE 
			&& part->phystype!=PART_PHYS_BOIDS) {
			float vec[4];

			where_on_path(ec->ob, 0.0, vec, vec2, NULL, NULL);

			Mat4MulVecfl(ec->ob->obmat,vec);
			Mat4Mul3Vecfl(ec->ob->obmat,vec2);

			QUATCOPY(ec->firstloc,vec);
			VECCOPY(ec->firstdir,vec2);

			totpart=psys->totpart;

			if(totpart){
				ec->distances=MEM_callocN(totpart*sizeof(float),"particle distances");
				ec->locations=MEM_callocN(totpart*3*sizeof(float),"particle locations");

				LOOP_PARTICLES {
					if(part->from == PART_FROM_PARTICLE) {
						VECCOPY(loc, pa->fuv);
					}
					else
						psys_particle_on_emitter(psmd,part->from,pa->num,pa->num_dmcache,pa->fuv,pa->foffset,loc,0,0,0,0,0);

					Mat4MulVecfl(ob->obmat,loc);
					ec->distances[p]=VecLenf(loc,vec);
					VECSUB(loc,loc,vec);
					VECCOPY(ec->locations+3*p,loc);
				}
			}
		}
		else if(ec->type==PSYS_EC_PARTICLE){
			Object *eob = ec->ob;
			ParticleSystem *epsys = BLI_findlink(&eob->particlesystem,ec->psys_nbr);
			ParticleSettings *epart = epsys->part;
			ParticleData *epa;
			int p, totepart = epsys->totpart;

			if(psys->part->phystype==PART_PHYS_BOIDS){
				ParticleKey state;
				PartDeflect *pd;
				
				pd= epart->pd;
				if(pd->forcefield==PFIELD_FORCE && totepart){
					KDTree *tree;

					tree=BLI_kdtree_new(totepart);
					ec->tree=tree;

					for(p=0, epa=epsys->particles; p<totepart; p++,epa++)
						if(epa->alive==PARS_ALIVE && psys_get_particle_state(scene, eob,epsys,p,&state,0))
							BLI_kdtree_insert(tree, p, state.co, NULL);

					BLI_kdtree_balance(tree);
				}
			}

		}
		else if(ec->type==PSYS_EC_DEFLECT) {
			CollisionModifierData *collmd = ( CollisionModifierData * ) ( modifiers_findByType ( ec->ob, eModifierType_Collision ) );
			if(collmd)
				collision_move_object(collmd, 1.0, 0.0);
		}
	}
}

int effector_find_co(Scene *scene, float *pco, SurfaceModifierData *sur, Object *ob, PartDeflect *pd, float *co, float *nor, float *vel, int *index)
{
	SurfaceModifierData *surmd = NULL;
	int ret = 0;

	if(sur)
		surmd = sur;
	else if(pd && pd->flag&PFIELD_SURFACE)
	{
		surmd = (SurfaceModifierData *)modifiers_findByType ( ob, eModifierType_Surface );
	}

	if(surmd) {
		/* closest point in the object surface is an effector */
		BVHTreeNearest nearest;

		nearest.index = -1;
		nearest.dist = FLT_MAX;

		BLI_bvhtree_find_nearest(surmd->bvhtree->tree, pco, &nearest, surmd->bvhtree->nearest_callback, surmd->bvhtree);

		if(nearest.index != -1) {
			VECCOPY(co, nearest.co);

			if(nor) {
				VECCOPY(nor, nearest.no);
			}

			if(vel) {
				MFace *mface = CDDM_get_face(surmd->dm, nearest.index);
				
				VECCOPY(vel, surmd->v[mface->v1].co);
				VecAddf(vel, vel, surmd->v[mface->v2].co);
				VecAddf(vel, vel, surmd->v[mface->v3].co);
				if(mface->v4)
					VecAddf(vel, vel, surmd->v[mface->v4].co);

				VecMulf(vel, mface->v4 ? 0.25f : 0.333f);
			}

			if(index)
				*index = nearest.index;

			ret = 1;
		}
		else {
			co[0] = co[1] = co[2] = 0.0f;

			if(nor)
				nor[0] = nor[1] = nor[2] = 0.0f;

			if(vel)
				vel[0] = vel[1] = vel[2] = 0.0f;
		}
	}
	else {
		/* use center of object for distance calculus */
		VECCOPY(co, ob->obmat[3]);

		if(nor) {
			VECCOPY(nor, ob->obmat[2]);
		}

		if(vel) {
			Object obcopy = *ob;
			
			VECCOPY(vel, ob->obmat[3]);

			where_is_object_time(scene, ob, scene->r.cfra - 1.0);

			VecSubf(vel, vel, ob->obmat[3]);

			*ob = obcopy;
		}
	}

	return ret;
}
/* calculate forces that all effectors apply to a particle*/
void do_effectors(int pa_no, ParticleData *pa, ParticleKey *state, Scene *scene, Object *ob, ParticleSystem *psys, float *rootco, float *force_field, float *vel,float framestep, float cfra)
{
	Object *eob;
	ParticleSystem *epsys;
	ParticleSettings *epart;
	ParticleData *epa;
	ParticleKey estate;
	PartDeflect *pd;
	ListBase *lb=&psys->effectors;
	ParticleEffectorCache *ec;
	float distance, vec_to_part[3], pco[3], co[3];
	float falloff, charge = 0.0f, strength;
	int p, face_index=-1;

	/* check all effector objects for interaction */
	if(lb->first){
		if(psys->part->pd && psys->part->pd->forcefield==PFIELD_CHARGE){
			/* Only the charge of the effected particle is used for 
			   interaction, not fall-offs. If the fall-offs aren't the	
			   same this will be unphysical, but for animation this		
			   could be the wanted behavior. If you want physical
			   correctness the fall-off should be spherical 2.0 anyways.
			 */
			charge = psys->part->pd->f_strength;
		}
		if(psys->part->pd2 && psys->part->pd2->forcefield==PFIELD_CHARGE){
			charge += psys->part->pd2->f_strength;
		}
		for(ec = lb->first; ec; ec= ec->next){
			eob= ec->ob;
			if(ec->type & PSYS_EC_EFFECTOR){
				pd=eob->pd;
				if(psys->part->type!=PART_HAIR && psys->part->integrator)
					where_is_object_time(scene, eob,cfra);

				if(pd && pd->flag&PFIELD_SURFACE) {
					float velocity[3];
					/* using velocity corrected location allows for easier sliding over effector surface */
					VecCopyf(velocity, state->vel);
					VecMulf(velocity, psys_get_timestep(psys->part));
					VecAddf(pco, state->co, velocity);
				}
				else 
					VECCOPY(pco, state->co);

				effector_find_co(scene, pco, NULL, eob, pd, co, NULL, NULL, &face_index);
				
				VecSubf(vec_to_part, state->co, co);

				distance = VecLength(vec_to_part);

				falloff=effector_falloff(pd,eob->obmat[2],vec_to_part);

				strength = pd->f_strength * psys->part->effector_weight[0] * psys->part->effector_weight[pd->forcefield];

				if(falloff<=0.0f)
					;	/* don't do anything */
				else if(pd->forcefield==PFIELD_TEXTURE) {
					do_texture_effector(pd->tex, pd->tex_mode, pd->flag&PFIELD_TEX_2D, pd->tex_nabla,
									pd->flag & PFIELD_TEX_OBJECT, (pd->flag & PFIELD_TEX_ROOTCO) ? rootco : state->co, eob->obmat,
									strength, falloff, force_field);
				} else {
					do_physical_effector(scene, eob, state->co, pd->forcefield,strength,distance,
										falloff,0.0,pd->f_damp,eob->obmat[2],vec_to_part,
										state->vel,force_field,pd->flag&PFIELD_PLANAR,ec->rng,pd->f_noise,charge,pa->size);
				}
			}
			if(ec->type & PSYS_EC_PARTICLE){
				int totepart, i;
				epsys= BLI_findlink(&eob->particlesystem,ec->psys_nbr);
				epart= epsys->part;
				pd=epart->pd;
				totepart= epsys->totpart;
				
				if(totepart <= 0)
					continue;
				
				if(pd && pd->forcefield==PFIELD_HARMONIC){
					/* every particle is mapped to only one harmonic effector particle */
					p= pa_no%epsys->totpart;
					totepart= p+1;
				}
				else{
					p=0;
				}

				epsys->lattice= psys_get_lattice(scene, ob, psys);

				for(; p<totepart; p++){
					/* particle skips itself as effector */
					if(epsys==psys && p == pa_no) continue;

					epa = epsys->particles + p;
					estate.time=cfra;
					if(psys_get_particle_state(scene, eob,epsys,p,&estate,0)){
						VECSUB(vec_to_part, state->co, estate.co);
						distance = VecLength(vec_to_part);
						
						for(i=0, pd = epart->pd; i<2; i++,pd = epart->pd2) {
							if(pd==NULL || pd->forcefield==0) continue;

							falloff=effector_falloff(pd,estate.vel,vec_to_part);

							strength = pd->f_strength * psys->part->effector_weight[0] * psys->part->effector_weight[pd->forcefield];

							if(falloff<=0.0f)
								;	/* don't do anything */
							else
								do_physical_effector(scene, eob, state->co, pd->forcefield,strength,distance,
								falloff,epart->size,pd->f_damp,estate.vel,vec_to_part,
								state->vel,force_field,0, ec->rng, pd->f_noise,charge,pa->size);
						}
					}
					else if(pd && pd->forcefield==PFIELD_HARMONIC && cfra-framestep <= epa->dietime && cfra>epa->dietime){
						/* first step after key release */
						psys_get_particle_state(scene, eob,epsys,p,&estate,1);
						VECADD(vel,vel,estate.vel);
						/* TODO: add rotation handling here too */
					}
				}

				if(epsys->lattice){
					end_latt_deform(epsys->lattice);
					epsys->lattice= NULL;
				}
			}
		}
	}
}

/************************************************/
/*			Newtonian physics					*/
/************************************************/
/* gathers all forces that effect particles and calculates a new state for the particle */
static void apply_particle_forces(Scene *scene, int pa_no, ParticleData *pa, Object *ob, ParticleSystem *psys, ParticleSettings *part, float timestep, float dfra, float cfra)
{
	ParticleKey states[5], tkey;
	float force[3],tvel[3],dx[4][3],dv[4][3];
	float dtime=dfra*timestep, time, pa_mass=part->mass, fac, fra=psys->cfra;
	int i, steps=1;
	
	/* maintain angular velocity */
	VECCOPY(pa->state.ave,pa->prev_state.ave);

	if(part->flag & PART_SIZEMASS)
		pa_mass*=pa->size;

	switch(part->integrator){
		case PART_INT_EULER:
			steps=1;
			break;
		case PART_INT_MIDPOINT:
			steps=2;
			break;
		case PART_INT_RK4:
			steps=4;
			break;
	}

	copy_particle_key(states,&pa->state,1);

	for(i=0; i<steps; i++){
		force[0]=force[1]=force[2]=0.0;
		tvel[0]=tvel[1]=tvel[2]=0.0;
		/* add effectors */
		if(part->type != PART_HAIR)
			do_effectors(pa_no,pa,states+i,scene, ob, psys,states->co,force,tvel,dfra,fra);

		/* calculate air-particle interaction */
		if(part->dragfac!=0.0f){
			fac=-part->dragfac*pa->size*pa->size*VecLength(states[i].vel);
			VECADDFAC(force,force,states[i].vel,fac);
		}

		/* brownian force */
		if(part->brownfac!=0.0){
			force[0]+=(BLI_frand()-0.5f)*part->brownfac;
			force[1]+=(BLI_frand()-0.5f)*part->brownfac;
			force[2]+=(BLI_frand()-0.5f)*part->brownfac;
		}

		/* force to acceleration*/
		VecMulf(force,1.0f/pa_mass);

		/* add global acceleration (gravitation) */
		VECADD(force,force,part->acc);
		
		/* calculate next state */
		VECADD(states[i].vel,states[i].vel,tvel);

		switch(part->integrator){
			case PART_INT_EULER:
				VECADDFAC(pa->state.co,states->co,states->vel,dtime);
				VECADDFAC(pa->state.vel,states->vel,force,dtime);
				break;
			case PART_INT_MIDPOINT:
				if(i==0){
					VECADDFAC(states[1].co,states->co,states->vel,dtime*0.5f);
					VECADDFAC(states[1].vel,states->vel,force,dtime*0.5f);
					fra=psys->cfra+0.5f*dfra;
				}
				else{
					VECADDFAC(pa->state.co,states->co,states[1].vel,dtime);
					VECADDFAC(pa->state.vel,states->vel,force,dtime);
				}
				break;
			case PART_INT_RK4:
				switch(i){
					case 0:
						VECCOPY(dx[0],states->vel);
						VecMulf(dx[0],dtime);
						VECCOPY(dv[0],force);
						VecMulf(dv[0],dtime);

						VECADDFAC(states[1].co,states->co,dx[0],0.5f);
						VECADDFAC(states[1].vel,states->vel,dv[0],0.5f);
						fra=psys->cfra+0.5f*dfra;
						break;
					case 1:
						VECADDFAC(dx[1],states->vel,dv[0],0.5f);
						VecMulf(dx[1],dtime);
						VECCOPY(dv[1],force);
						VecMulf(dv[1],dtime);

						VECADDFAC(states[2].co,states->co,dx[1],0.5f);
						VECADDFAC(states[2].vel,states->vel,dv[1],0.5f);
						break;
					case 2:
						VECADDFAC(dx[2],states->vel,dv[1],0.5f);
						VecMulf(dx[2],dtime);
						VECCOPY(dv[2],force);
						VecMulf(dv[2],dtime);

						VECADD(states[3].co,states->co,dx[2]);
						VECADD(states[3].vel,states->vel,dv[2]);
						fra=cfra;
						break;
					case 3:
						VECADD(dx[3],states->vel,dv[2]);
						VecMulf(dx[3],dtime);
						VECCOPY(dv[3],force);
						VecMulf(dv[3],dtime);

						VECADDFAC(pa->state.co,states->co,dx[0],1.0f/6.0f);
						VECADDFAC(pa->state.co,pa->state.co,dx[1],1.0f/3.0f);
						VECADDFAC(pa->state.co,pa->state.co,dx[2],1.0f/3.0f);
						VECADDFAC(pa->state.co,pa->state.co,dx[3],1.0f/6.0f);

						VECADDFAC(pa->state.vel,states->vel,dv[0],1.0f/6.0f);
						VECADDFAC(pa->state.vel,pa->state.vel,dv[1],1.0f/3.0f);
						VECADDFAC(pa->state.vel,pa->state.vel,dv[2],1.0f/3.0f);
						VECADDFAC(pa->state.vel,pa->state.vel,dv[3],1.0f/6.0f);
				}
				break;
		}
	}

	/* damp affects final velocity */
	if(part->dampfac!=0.0)
		VecMulf(pa->state.vel,1.0f-part->dampfac);

	/* finally we do guides */
	time=(cfra-pa->time)/pa->lifetime;
	CLAMP(time,0.0,1.0);

	VECCOPY(tkey.co,pa->state.co);
	VECCOPY(tkey.vel,pa->state.vel);
	tkey.time=pa->state.time;

	if(part->type != PART_HAIR) {
		if(do_guide(scene, &tkey, pa_no, time, &psys->effectors)) {
			VECCOPY(pa->state.co,tkey.co);
			/* guides don't produce valid velocity */
			VECSUB(pa->state.vel,tkey.co,pa->prev_state.co);
			VecMulf(pa->state.vel,1.0f/dtime);
			pa->state.time=tkey.time;
		}
	}
}
static void rotate_particle(ParticleSettings *part, ParticleData *pa, float dfra, float timestep)
{
	float rotfac, rot1[4], rot2[4]={1.0,0.0,0.0,0.0}, dtime=dfra*timestep;

	if((part->flag & PART_ROT_DYN)==0){
		if(part->avemode==PART_AVE_SPIN){
			float angle;
			float len1 = VecLength(pa->prev_state.vel);
			float len2 = VecLength(pa->state.vel);

			if(len1==0.0f || len2==0.0f)
				pa->state.ave[0]=pa->state.ave[1]=pa->state.ave[2]=0.0f;
			else{
				Crossf(pa->state.ave,pa->prev_state.vel,pa->state.vel);
				Normalize(pa->state.ave);
				angle=Inpf(pa->prev_state.vel,pa->state.vel)/(len1*len2);
				VecMulf(pa->state.ave,saacos(angle)/dtime);
			}

			VecRotToQuat(pa->state.vel,dtime*part->avefac,rot2);
		}
	}

	rotfac=VecLength(pa->state.ave);
	if(rotfac==0.0){ /* QuatOne (in VecRotToQuat) doesn't give unit quat [1,0,0,0]?? */
		rot1[0]=1.0;
		rot1[1]=rot1[2]=rot1[3]=0;
	}
	else{
		VecRotToQuat(pa->state.ave,rotfac*dtime,rot1);
	}
	QuatMul(pa->state.rot,rot1,pa->prev_state.rot);
	QuatMul(pa->state.rot,rot2,pa->state.rot);

	/* keep rotation quat in good health */
	NormalQuat(pa->state.rot);
}

/* convert from triangle barycentric weights to quad mean value weights */
static void intersect_dm_quad_weights(float *v1, float *v2, float *v3, float *v4, float *w)
{
	float co[3], vert[4][3];

	VECCOPY(vert[0], v1);
	VECCOPY(vert[1], v2);
	VECCOPY(vert[2], v3);
	VECCOPY(vert[3], v4);

	co[0]= v1[0]*w[0] + v2[0]*w[1] + v3[0]*w[2] + v4[0]*w[3];
	co[1]= v1[1]*w[0] + v2[1]*w[1] + v3[1]*w[2] + v4[1]*w[3];
	co[2]= v1[2]*w[0] + v2[2]*w[1] + v3[2]*w[2] + v4[2]*w[3];

	MeanValueWeights(vert, 4, co, w);
}

/* check intersection with a derivedmesh */
int psys_intersect_dm(Scene *scene, Object *ob, DerivedMesh *dm, float *vert_cos, float *co1, float* co2, float *min_d, int *min_face, float *min_w,
						  float *face_minmax, float *pa_minmax, float radius, float *ipoint)
{
	MFace *mface=0;
	MVert *mvert=0;
	int i, totface, intersect=0;
	float cur_d, cur_uv[2], v1[3], v2[3], v3[3], v4[3], min[3], max[3], p_min[3],p_max[3];
	float cur_ipoint[3];
	
	if(dm==0){
		psys_disable_all(ob);

		dm=mesh_get_derived_final(scene, ob, 0);
		if(dm==0)
			dm=mesh_get_derived_deform(scene, ob, 0);

		psys_enable_all(ob);

		if(dm==0)
			return 0;
	}

	

	if(pa_minmax==0){
		INIT_MINMAX(p_min,p_max);
		DO_MINMAX(co1,p_min,p_max);
		DO_MINMAX(co2,p_min,p_max);
	}
	else{
		VECCOPY(p_min,pa_minmax);
		VECCOPY(p_max,pa_minmax+3);
	}

	totface=dm->getNumFaces(dm);
	mface=dm->getFaceDataArray(dm,CD_MFACE);
	mvert=dm->getVertDataArray(dm,CD_MVERT);
	
	/* lets intersect the faces */
	for(i=0; i<totface; i++,mface++){
		if(vert_cos){
			VECCOPY(v1,vert_cos+3*mface->v1);
			VECCOPY(v2,vert_cos+3*mface->v2);
			VECCOPY(v3,vert_cos+3*mface->v3);
			if(mface->v4)
				VECCOPY(v4,vert_cos+3*mface->v4)
		}
		else{
			VECCOPY(v1,mvert[mface->v1].co);
			VECCOPY(v2,mvert[mface->v2].co);
			VECCOPY(v3,mvert[mface->v3].co);
			if(mface->v4)
				VECCOPY(v4,mvert[mface->v4].co)
		}

		if(face_minmax==0){
			INIT_MINMAX(min,max);
			DO_MINMAX(v1,min,max);
			DO_MINMAX(v2,min,max);
			DO_MINMAX(v3,min,max);
			if(mface->v4)
				DO_MINMAX(v4,min,max)
			if(AabbIntersectAabb(min,max,p_min,p_max)==0)
				continue;
		}
		else{
			VECCOPY(min, face_minmax+6*i);
			VECCOPY(max, face_minmax+6*i+3);
			if(AabbIntersectAabb(min,max,p_min,p_max)==0)
				continue;
		}

		if(radius>0.0f){
			if(SweepingSphereIntersectsTriangleUV(co1, co2, radius, v2, v3, v1, &cur_d, cur_ipoint)){
				if(cur_d<*min_d){
					*min_d=cur_d;
					VECCOPY(ipoint,cur_ipoint);
					*min_face=i;
					intersect=1;
				}
			}
			if(mface->v4){
				if(SweepingSphereIntersectsTriangleUV(co1, co2, radius, v4, v1, v3, &cur_d, cur_ipoint)){
					if(cur_d<*min_d){
						*min_d=cur_d;
						VECCOPY(ipoint,cur_ipoint);
						*min_face=i;
						intersect=1;
					}
				}
			}
		}
		else{
			if(LineIntersectsTriangle(co1, co2, v1, v2, v3, &cur_d, cur_uv)){
				if(cur_d<*min_d){
					*min_d=cur_d;
					min_w[0]= 1.0 - cur_uv[0] - cur_uv[1];
					min_w[1]= cur_uv[0];
					min_w[2]= cur_uv[1];
					min_w[3]= 0.0f;
					if(mface->v4)
						intersect_dm_quad_weights(v1, v2, v3, v4, min_w);
					*min_face=i;
					intersect=1;
				}
			}
			if(mface->v4){
				if(LineIntersectsTriangle(co1, co2, v1, v3, v4, &cur_d, cur_uv)){
					if(cur_d<*min_d){
						*min_d=cur_d;
						min_w[0]= 1.0 - cur_uv[0] - cur_uv[1];
						min_w[1]= 0.0f;
						min_w[2]= cur_uv[0];
						min_w[3]= cur_uv[1];
						intersect_dm_quad_weights(v1, v2, v3, v4, min_w);
						*min_face=i;
						intersect=1;
					}
				}
			}
		}
	}
	return intersect;
}

void particle_intersect_face(void *userdata, int index, const BVHTreeRay *ray, BVHTreeRayHit *hit)
{
	ParticleCollision *col = (ParticleCollision *) userdata;
	MFace *face = col->md->mfaces + index;
	MVert *x = col->md->x;
	MVert *v = col->md->current_v;
	float vel[3], co1[3], co2[3], uv[2], ipoint[3], temp[3], t;

	float *t0, *t1, *t2, *t3;
	t0 = x[ face->v1 ].co;
	t1 = x[ face->v2 ].co;
	t2 = x[ face->v3 ].co;
	t3 = face->v4 ? x[ face->v4].co : NULL;

	/* calculate average velocity of face */
	VECCOPY(vel, v[ face->v1 ].co);
	VECADD(vel, vel, v[ face->v2 ].co);
	VECADD(vel, vel, v[ face->v3 ].co);
	VecMulf(vel, 0.33334f);

	/* substract face velocity, in other words convert to 
	   a coordinate system where only the particle moves */
	VECADDFAC(co1, col->co1, vel, -col->t);
	VECSUB(co2, col->co2, vel);

	do
	{	
		if(ray->radius == 0.0f) {
			if(LineIntersectsTriangle(co1, co2, t0, t1, t2, &t, uv)) {
				if(t >= 0.0f && t < hit->dist/col->ray_len) {
					hit->dist = col->ray_len * t;
					hit->index = index;

					/* calculate normal that's facing the particle */
					CalcNormFloat(t0, t1, t2, col->nor);
					VECSUB(temp, co2, co1);
					if(Inpf(col->nor, temp) > 0.0f)
						VecNegf(col->nor);

					VECCOPY(col->vel,vel);

					col->ob = col->ob_t;
				}
			}
		}
		else {
			if(SweepingSphereIntersectsTriangleUV(co1, co2, ray->radius, t0, t1, t2, &t, ipoint)) {
				if(t >=0.0f && t < hit->dist/col->ray_len) {
					hit->dist = col->ray_len * t;
					hit->index = index;

					VecLerpf(temp, co1, co2, t);
					
					VECSUB(col->nor, temp, ipoint);
					Normalize(col->nor);

					VECCOPY(col->vel,vel);

					col->ob = col->ob_t;
				}
			}
		}

		t1 = t2;
		t2 = t3;
		t3 = NULL;

	} while(t2);
}
/* particle - mesh collision code */
/* in addition to basic point to surface collisions handles friction & damping,*/
/* angular momentum <-> linear momentum and swept sphere - mesh collisions */
/* 1. check for all possible deflectors for closest intersection on particle path */
/* 2. if deflection was found kill the particle or calculate new coordinates */
static void deflect_particle(Scene *scene, Object *pob, ParticleSystemModifierData *psmd, ParticleSystem *psys, ParticleSettings *part, ParticleData *pa, int p, float timestep, float dfra, float cfra){
	Object *ob = NULL, *skip_ob = NULL;
	ListBase *lb=&psys->effectors;
	ParticleEffectorCache *ec;
	ParticleKey reaction_state;
	ParticleCollision col;
	BVHTreeRayHit hit;
	float ray_dir[3], zerovec[3]={0.0,0.0,0.0};
	float radius = ((part->flag & PART_SIZE_DEFL)?pa->size:0.0f), boid_z = 0.0f;
	int deflections=0, max_deflections=10;

	VECCOPY(col.co1, pa->prev_state.co);
	VECCOPY(col.co2, pa->state.co);
	col.t = 0.0f;

	/* override for boids */
	if(part->phystype == PART_PHYS_BOIDS) {
		BoidParticle *bpa = pa->boid;
		radius = pa->size;
		boid_z = pa->state.co[2];
		skip_ob = bpa->ground;
	}

	/* 10 iterations to catch multiple deflections */
	if(lb->first) while(deflections < max_deflections){
		/* 1. */

		VECSUB(ray_dir, col.co2, col.co1);
		hit.index = -1;
		hit.dist = col.ray_len = VecLength(ray_dir);

		/* even if particle is stationary we want to check for moving colliders */
		/* if hit.dist is zero the bvhtree_ray_cast will just ignore everything */
		if(hit.dist == 0.0f)
			hit.dist = col.ray_len = 0.000001f;

		for(ec=lb->first; ec; ec=ec->next){
			if(ec->type & PSYS_EC_DEFLECT){
				ob= ec->ob;

				/* for boids: don't check with current ground object */
				if(ob==skip_ob)
					continue;

				/* particles should not collide with emitter at birth */
				if(ob==pob && pa->time < cfra && pa->time >= psys->cfra)
					continue;

				if(part->type!=PART_HAIR)
					where_is_object_time(scene,ob,cfra);

				col.md = ( CollisionModifierData * ) ( modifiers_findByType ( ec->ob, eModifierType_Collision ) );
				col.ob_t = ob;

				if(col.md && col.md->bvhtree)
					BLI_bvhtree_ray_cast(col.md->bvhtree, col.co1, ray_dir, radius, &hit, particle_intersect_face, &col);
			}
		}

		/* 2. */
		if(hit.index>=0) {
			PartDeflect *pd = col.ob->pd;
			int through = (BLI_frand() < pd->pdef_perm) ? 1 : 0;
			float co[3]; /* point of collision */
			float vec[3]; /* movement through collision */
			float t = hit.dist/col.ray_len; /* time of collision between this iteration */
			float dt = col.t + t * (1.0f - col.t); /* time of collision between frame change*/

			VecLerpf(co, col.co1, col.co2, t);
			VECSUB(vec, col.co2, col.co1);

			VecMulf(col.vel, 1.0f-col.t);

			/* particle dies in collision */
			if(through == 0 && (part->flag & PART_DIE_ON_COL || pd->flag & PDEFLE_KILL_PART)) {
				pa->alive = PARS_DYING;
				pa->dietime = pa->state.time + (cfra - pa->state.time) * dt;
				
				/* we have to add this for dying particles too so that reactors work correctly */
				VECADDFAC(co, co, col.nor, (through ? -0.0001f : 0.0001f));

				VECCOPY(pa->state.co, co);
				VecLerpf(pa->state.vel, pa->prev_state.vel, pa->state.vel, dt);
				QuatInterpol(pa->state.rot, pa->prev_state.rot, pa->state.rot, dt);
				VecLerpf(pa->state.ave, pa->prev_state.ave, pa->state.ave, dt);

				/* particle is dead so we don't need to calculate further */
				deflections=max_deflections;

				/* store for reactors */
				copy_particle_key(&reaction_state, &pa->state, 0);
			}
			else {
				float nor_vec[3], tan_vec[3], tan_vel[3], vel[3];
				float damp, frict;
				float inp, inp_v;
				
				/* get damping & friction factors */
				damp = pd->pdef_damp + pd->pdef_rdamp * 2 * (BLI_frand() - 0.5f);
				CLAMP(damp,0.0,1.0);

				frict = pd->pdef_frict + pd->pdef_rfrict * 2 * (BLI_frand() - 0.5f);
				CLAMP(frict,0.0,1.0);

				/* treat normal & tangent components separately */
				inp = Inpf(col.nor, vec);
				inp_v = Inpf(col.nor, col.vel);

				VECADDFAC(tan_vec, vec, col.nor, -inp);
				VECADDFAC(tan_vel, col.vel, col.nor, -inp_v);
				if((part->flag & PART_ROT_DYN)==0)
					VecLerpf(tan_vec, tan_vec, tan_vel, frict);

				VECCOPY(nor_vec, col.nor);
				inp *= 1.0f - damp;

				if(through)
					inp_v *= damp;

				/* special case for object hitting the particle from behind */
				if(through==0 && ((inp_v>0 && inp>0 && inp_v>inp) || (inp_v<0 && inp<0 && inp_v<inp)))
					VecMulf(nor_vec, inp_v);
				else
					VecMulf(nor_vec, inp_v + (through ? 1.0f : -1.0f) * inp);

				/* angular <-> linear velocity - slightly more physical and looks even nicer than before */
				if(part->flag & PART_ROT_DYN) {
					float surface_vel[3], rot_vel[3], friction[3], dave[3], dvel[3];

					/* apparent velocity along collision surface */
					VECSUB(surface_vel, tan_vec, tan_vel);

					/* direction of rolling friction */
					Crossf(rot_vel, pa->state.ave, col.nor);
					/* convert to current dt */
					VecMulf(rot_vel, (timestep*dfra) * (1.0f - col.t));
					VecMulf(rot_vel, pa->size);

					/* apply sliding friction */
					VECSUB(surface_vel, surface_vel, rot_vel);
					VECCOPY(friction, surface_vel);

					VecMulf(surface_vel, 1.0 - frict);
					VecMulf(friction, frict);

					/* sliding changes angular velocity */
					Crossf(dave, col.nor, friction);
					VecMulf(dave, 1.0f/MAX2(pa->size, 0.001));

					/* we assume rolling friction is around 0.01 of sliding friction */
					VecMulf(rot_vel, 1.0 - frict*0.01);

					/* change in angular velocity has to be added to the linear velocity too */
					Crossf(dvel, dave, col.nor);
					VecMulf(dvel, pa->size);
					VECADD(rot_vel, rot_vel, dvel);

					VECADD(surface_vel, surface_vel, rot_vel);
					VECADD(tan_vec, surface_vel, tan_vel);

					/* convert back to normal time */
					VecMulf(dave, 1.0f/MAX2((timestep*dfra) * (1.0f - col.t), 0.00001));

					VecMulf(pa->state.ave, 1.0 - frict*0.01);
					VECADD(pa->state.ave, pa->state.ave, dave);
				}

				/* combine components together again */
				VECADD(vec, nor_vec, tan_vec);

				/* calculate velocity from collision vector */
				VECCOPY(vel, vec);
				VecMulf(vel, 1.0f/MAX2((timestep*dfra) * (1.0f - col.t), 0.00001));

				/* make sure we don't hit the current face again */
				VECADDFAC(co, co, col.nor, (through ? -0.0001f : 0.0001f));

				if(part->phystype == PART_PHYS_BOIDS && part->boids->options & BOID_ALLOW_LAND) {
					BoidParticle *bpa = pa->boid;
					if(bpa->data.mode == eBoidMode_OnLand || co[2] <= boid_z) {
						co[2] = boid_z;
						vel[2] = 0.0f;
					}
				}

				/* store state for reactors */
				VECCOPY(reaction_state.co, co);
				VecLerpf(reaction_state.vel, pa->prev_state.vel, pa->state.vel, dt);
				QuatInterpol(reaction_state.rot, pa->prev_state.rot, pa->state.rot, dt);

				/* set coordinates for next iteration */
				VECCOPY(col.co1, co);
				VECADDFAC(col.co2, co, vec, 1.0f - t);
				col.t = dt;

				if(VecLength(vec) < 0.001 && VecLength(pa->state.vel) < 0.001) {
					/* kill speed to stop slipping */
					VECCOPY(pa->state.vel,zerovec);
					VECCOPY(pa->state.co, co);
					if(part->flag & PART_ROT_DYN) {
						VECCOPY(pa->state.ave,zerovec);
					}
				}
				else {
					VECCOPY(pa->state.co, col.co2);
					VECCOPY(pa->state.vel, vel);
				}
			}
			deflections++;

			reaction_state.time = cfra - (1.0f - dt) * dfra;
			push_reaction(col.ob, psys, p, PART_EVENT_COLLIDE, &reaction_state);
		}
		else
			return;
	}
}
/************************************************/
/*			Hair								*/
/************************************************/
/* check if path cache or children need updating and do it if needed */
static void psys_update_path_cache(Scene *scene, Object *ob, ParticleSystemModifierData *psmd, ParticleSystem *psys, float cfra)
{
	ParticleSettings *part=psys->part;
	ParticleEditSettings *pset=&scene->toolsettings->particle;
	int distr=0,alloc=0,skip=0;

	if((psys->part->childtype && psys->totchild != get_psys_tot_child(scene, psys)) || psys->recalc&PSYS_RECALC_RESET)
		alloc=1;

	if(alloc || psys->recalc&PSYS_RECALC_CHILD || (psys->vgroup[PSYS_VG_DENSITY] && (ob && ob->mode & OB_MODE_WEIGHT_PAINT)))
		distr=1;

	if(distr){
		if(alloc)
			realloc_particles(ob,psys,psys->totpart);

		if(get_psys_tot_child(scene, psys)) {
			/* don't generate children while computing the hair keys */
			if(!(psys->part->type == PART_HAIR) || (psys->flag & PSYS_HAIR_DONE)) {
				distribute_particles(scene, ob, psys, PART_FROM_CHILD);

				if(part->from!=PART_FROM_PARTICLE && part->childtype==PART_CHILD_FACES && part->parents!=0.0)
					psys_find_parents(ob,psmd,psys);
			}
		}
	}

	if((part->type==PART_HAIR || psys->flag&PSYS_KEYED || psys->pointcache->flag & PTCACHE_BAKED)==0)
		skip = 1; /* only hair, keyed and baked stuff can have paths */
	else if(part->ren_as != PART_DRAW_PATH)
		skip = 1; /* particle visualization must be set as path */
	else if(!psys->renderdata) {
		if(part->draw_as != PART_DRAW_REND)
			skip = 1; /* draw visualization */
		else if(psys->pointcache->flag & PTCACHE_BAKING)
			skip = 1; /* no need to cache paths while baking dynamics */
		else if(psys_in_edit_mode(scene, psys)) {
			if((pset->flag & PE_DRAW_PART)==0)
				skip = 1;
			else if(part->childtype==0 && (psys->flag & PSYS_HAIR_DYNAMICS && psys->pointcache->flag & PTCACHE_BAKED)==0)
				skip = 1; /* in edit mode paths are needed for child particles and dynamic hair */
		}
	}

	if(!skip) {
		psys_cache_paths(scene, ob, psys, cfra);

		/* for render, child particle paths are computed on the fly */
		if(part->childtype) {
			if(!psys->totchild)
				skip = 1;
			else if((psys->part->type == PART_HAIR && psys->flag & PSYS_HAIR_DONE)==0)
				skip = 1;

			if(!skip)
				psys_cache_child_paths(scene, ob, psys, cfra, 0);
		}
	}
	else if(psys->pathcache)
		psys_free_path_cache(psys, NULL);
}

static void do_hair_dynamics(Scene *scene, Object *ob, ParticleSystem *psys, ParticleSystemModifierData *psmd)
{
	DerivedMesh *dm = psys->hair_in_dm;
	MVert *mvert = NULL;
	MEdge *medge = NULL;
	MDeformVert *dvert = NULL;
	HairKey *key;
	PARTICLE_P;
	int totpoint = 0;
	int totedge;
	int k;
	float hairmat[4][4];

	if(!psys->clmd) {
		psys->clmd = (ClothModifierData*)modifier_new(eModifierType_Cloth);
		psys->clmd->sim_parms->goalspring = 0.0f;
		psys->clmd->sim_parms->flags |= CLOTH_SIMSETTINGS_FLAG_GOAL|CLOTH_SIMSETTINGS_FLAG_NO_SPRING_COMPRESS;
		psys->clmd->coll_parms->flags &= ~CLOTH_COLLSETTINGS_FLAG_SELF;
	}

	/* create a dm from hair vertices */
	LOOP_PARTICLES
		totpoint += pa->totkey;

	totedge = totpoint - psys->totpart;

	if(dm && (totpoint != dm->getNumVerts(dm) || totedge != dm->getNumEdges(dm))) {
		dm->release(dm);
		dm = psys->hair_in_dm = NULL;
	}

	if(!dm) {
		dm = psys->hair_in_dm = CDDM_new(totpoint, totedge, 0);
		DM_add_vert_layer(dm, CD_MDEFORMVERT, CD_CALLOC, NULL);
	}

	mvert = CDDM_get_verts(dm);
	medge = CDDM_get_edges(dm);
	dvert = DM_get_vert_data_layer(dm, CD_MDEFORMVERT);

	psys->clmd->sim_parms->vgroup_mass = 1;

	/* make vgroup for pin roots etc.. */
	psys->particles->hair_index = 0;
	LOOP_PARTICLES {
		if(p)
			pa->hair_index = (pa-1)->hair_index + (pa-1)->totkey;

		psys_mat_hair_to_object(ob, psmd->dm, psys->part->from, pa, hairmat);

		for(k=0, key=pa->hair; k<pa->totkey; k++,key++) {
			VECCOPY(mvert->co, key->co);
			Mat4MulVecfl(hairmat, mvert->co);
			mvert++;
			
			if(k) {
				medge->v1 = pa->hair_index + k - 1;
				medge->v2 = pa->hair_index + k;
				medge++;
			}

			if(dvert) {
				if(!dvert->totweight) {
					dvert->dw = MEM_callocN (sizeof(MDeformWeight), "deformWeight");
					dvert->totweight = 1;
				}

				/* no special reason for the 0.5 */
				/* just seems like a nice value from experiments */
				dvert->dw->weight = k ? 0.5f : 1.0f;
				dvert++;
			}
		}
	}

	if(psys->hair_out_dm)
		psys->hair_out_dm->release(psys->hair_out_dm);

	psys->clmd->point_cache = psys->pointcache;

	psys->hair_out_dm = clothModifier_do(psys->clmd, scene, ob, dm, 0, 0);
}
static void hair_step(Scene *scene, Object *ob, ParticleSystemModifierData *psmd, ParticleSystem *psys, float cfra)
{
	ParticleSettings *part = psys->part;
	PARTICLE_P;
	float disp = (float)get_current_display_percentage(psys)/100.0f;

	BLI_srandom(psys->seed);

	LOOP_PARTICLES {
		if(BLI_frand() > disp)
			pa->flag |= PARS_NO_DISP;
		else
			pa->flag &= ~PARS_NO_DISP;
	}

	if(psys->recalc & PSYS_RECALC_RESET) {
		/* need this for changing subsurf levels */
		psys_calc_dmcache(ob, psmd->dm, psys);

		if(psys->clmd)
			cloth_free_modifier(ob, psys->clmd);
	}

	if(psys->effectors.first)
		psys_end_effectors(psys);

	/* dynamics with cloth simulation */
	if(psys->part->type==PART_HAIR && psys->flag & PSYS_HAIR_DYNAMICS)
		do_hair_dynamics(scene, ob, psys, psmd);

	psys_init_effectors(scene, ob, part->eff_group, psys);
	if(psys->effectors.first)
		precalc_effectors(scene, ob,psys,psmd,cfra);

	psys_update_path_cache(scene, ob,psmd,psys,cfra);

	psys->flag |= PSYS_HAIR_UPDATED;
}

static void save_hair(Scene *scene, Object *ob, ParticleSystem *psys, ParticleSystemModifierData *psmd, float cfra){
	HairKey *key, *root;
	PARTICLE_P;
	int totpart;

	Mat4Invert(ob->imat,ob->obmat);
	
	psys->lattice= psys_get_lattice(scene, ob, psys);

	if(psys->totpart==0) return;

	totpart=psys->totpart;
	
	/* save new keys for elements if needed */
	LOOP_PARTICLES {
		/* first time alloc */
		if(pa->totkey==0 || pa->hair==NULL) {
			pa->hair = MEM_callocN((psys->part->hair_step + 1) * sizeof(HairKey), "HairKeys");
			pa->totkey = 0;
		}

		key = root = pa->hair;
		key += pa->totkey;

		/* convert from global to geometry space */
		VecCopyf(key->co, pa->state.co);
		Mat4MulVecfl(ob->imat, key->co);

		if(pa->totkey) {
			VECSUB(key->co, key->co, root->co);
			psys_vec_rot_to_face(psmd->dm, pa, key->co);
		}

		key->time = pa->state.time;

		key->weight = 1.0f - key->time / 100.0f;

		pa->totkey++;

		/* root is always in the origin of hair space so we set it to be so after the last key is saved*/
		if(pa->totkey == psys->part->hair_step + 1)
			root->co[0] = root->co[1] = root->co[2] = 0.0f;
	}
}
/************************************************/
/*			System Core							*/
/************************************************/
/* unbaked particles are calculated dynamically */
static void dynamics_step(Scene *scene, Object *ob, ParticleSystem *psys, ParticleSystemModifierData *psmd, float cfra,
						  float *vg_vel, float *vg_tan, float *vg_rot, float *vg_size)
{
	ParticleSettings *part=psys->part;
	KDTree *tree=0;
	IpoCurve *icu_esize= NULL; //=find_ipocurve(part->ipo,PART_EMIT_SIZE); // XXX old animation system
	Material *ma=give_current_material(ob,part->omat);
	BoidBrainData bbd;
	PARTICLE_P;
	float timestep;
	int totpart;
	/* current time */
	float ctime, ipotime; // XXX old animation system
	/* frame & time changes */
	float dfra, dtime, pa_dtime, pa_dfra=0.0;
	float birthtime, dietime;
	
	/* where have we gone in time since last time */
	dfra= cfra - psys->cfra;

	totpart=psys->totpart;

	timestep=psys_get_timestep(part);
	dtime= dfra*timestep;
	ctime= cfra*timestep;
	ipotime= cfra; // XXX old animation system

#if 0 // XXX old animation system
	if(part->flag&PART_ABS_TIME && part->ipo){
		calc_ipo(part->ipo, cfra);
		execute_ipo((ID *)part, part->ipo);
	}
#endif // XXX old animation system

	if(dfra<0.0){
		float *vg_size=0;
		if(part->type==PART_REACTOR)
			vg_size=psys_cache_vgroup(psmd->dm,psys,PSYS_VG_SIZE);

		LOOP_PARTICLES {
			if(pa->flag & PARS_UNEXIST) continue;

			/* set correct ipo timing */
#if 0 // XXX old animation system
			if((part->flag&PART_ABS_TIME)==0 && part->ipo){
				ipotime=100.0f*(cfra-pa->time)/pa->lifetime;
				calc_ipo(part->ipo, ipotime);
				execute_ipo((ID *)part, part->ipo);
			}
#endif // XXX old animation system
			pa->size=psys_get_size(ob,ma,psmd,icu_esize,psys,part,pa,vg_size);

			reset_particle(scene, pa,psys,psmd,ob,dtime,cfra,vg_vel,vg_tan,vg_rot);

			if(cfra>pa->time && part->flag & PART_LOOP && part->type!=PART_HAIR){
				pa->loop=(short)((cfra-pa->time)/pa->lifetime);
				pa->alive=PARS_UNBORN;
			}
			else{
				pa->loop = 0;
				if(cfra <= pa->time)
					pa->alive = PARS_UNBORN;
						/* without dynamics the state is allways known so no need to kill */
				else if(ELEM(part->phystype, PART_PHYS_NO, PART_PHYS_KEYED)){
					if(cfra < pa->dietime)
						pa->alive = PARS_ALIVE;
				}
				else
					pa->alive = PARS_KILLED;
			}
		}

		if(vg_size)
			MEM_freeN(vg_size);
	}
	else{
		BLI_srandom(31415926 + (int)cfra + psys->seed);
		
		/* update effectors */
		if(psys->effectors.first)
			psys_end_effectors(psys);

		psys_init_effectors(scene, ob, part->eff_group, psys);
		
		if(psys->effectors.first)
			precalc_effectors(scene, ob,psys,psmd,cfra);

		if(part->phystype==PART_PHYS_BOIDS){
			ParticleTarget *pt = psys->targets.first;
			bbd.scene = scene;
			bbd.ob = ob;
			bbd.psys = psys;
			bbd.part = part;
			bbd.cfra = cfra;
			bbd.dfra = dfra;
			bbd.timestep = timestep;

			update_particle_tree(psys);

			boids_precalc_rules(part, cfra);

			for(; pt; pt=pt->next) {
				if(pt->ob)
					update_particle_tree(BLI_findlink(&pt->ob->particlesystem, pt->psys-1));
			}
		}

		/* main loop: calculate physics for all particles */
		LOOP_PARTICLES {
			if(pa->flag & (PARS_UNEXIST+PARS_NO_DISP)) continue;

			copy_particle_key(&pa->prev_state,&pa->state,1);
			
			/* set correct ipo timing */
#if 0 // XXX old animation system
			if((part->flag&PART_ABS_TIME)==0 && part->ipo){
				ipotime=100.0f*(cfra-pa->time)/pa->lifetime;
				calc_ipo(part->ipo, ipotime);
				execute_ipo((ID *)part, part->ipo);
			}
#endif // XXX old animation system
			pa->size=psys_get_size(ob,ma,psmd,icu_esize,psys,part,pa,vg_size);

			/* reactions can change birth time so they need to be checked first */
			if(psys->reactevents.first && ELEM(pa->alive,PARS_DEAD,PARS_KILLED)==0)
				react_to_events(psys,p);

			birthtime = pa->time + pa->loop * pa->lifetime;
			dietime = birthtime + pa->lifetime;

			/* allways reset particles to emitter before birth */
			if(pa->alive==PARS_UNBORN
				|| pa->alive==PARS_KILLED
				|| ELEM(part->phystype,PART_PHYS_NO,PART_PHYS_KEYED)
				|| birthtime >= psys->cfra){
				reset_particle(scene, pa,psys,psmd,ob,dtime,cfra,vg_vel,vg_tan,vg_rot);
			}

			pa_dfra = dfra;
			pa_dtime = dtime;


			if(dietime <= cfra && psys->cfra < dietime){
				/* particle dies some time between this and last step */
				pa_dfra = dietime - ((birthtime > psys->cfra) ? birthtime : psys->cfra);
				pa_dtime = pa_dfra * timestep;
				pa->alive = PARS_DYING;
			}
			else if(birthtime <= cfra && birthtime >= psys->cfra){
				/* particle is born some time between this and last step*/
				pa->alive = PARS_ALIVE;
				pa_dfra = cfra - birthtime;
				pa_dtime = pa_dfra*timestep;
			}
			else if(dietime < cfra){
				/* nothing to be done when particle is dead */
			}


			if(dfra>0.0 && ELEM(pa->alive,PARS_ALIVE,PARS_DYING)){
				switch(part->phystype){
					case PART_PHYS_NEWTON:
						/* do global forces & effectors */
						apply_particle_forces(scene, p, pa, ob, psys, part, timestep,pa_dfra,cfra);
			
						/* deflection */
						deflect_particle(scene, ob,psmd,psys,part,pa,p,timestep,pa_dfra,cfra);

						/* rotations */
						rotate_particle(part,pa,pa_dfra,timestep);
						break;
					case PART_PHYS_BOIDS:
					{
						bbd.goal_ob = NULL;
						boid_brain(&bbd, p, pa);
						if(pa->alive != PARS_DYING) {
							boid_body(&bbd, pa);

							/* deflection */
							deflect_particle(scene,ob,psmd,psys,part,pa,p,timestep,pa_dfra,cfra);
						}
						break;
					}
				}

				if(pa->alive == PARS_DYING){
					push_reaction(ob,psys,p,PART_EVENT_DEATH,&pa->state);

					if(part->flag & PART_LOOP && part->type!=PART_HAIR){
						pa->loop++;
						reset_particle(scene, pa,psys,psmd,ob,0.0,cfra,vg_vel,vg_tan,vg_rot);
						pa->alive=PARS_ALIVE;
					}
					else{
						pa->alive=PARS_DEAD;
						pa->state.time=pa->dietime;
					}
				}
				else
					pa->state.time=cfra;

				push_reaction(ob,psys,p,PART_EVENT_NEAR,&pa->state);
			}
		}
	}
	if(psys->reactevents.first)
		BLI_freelistN(&psys->reactevents);

	if(tree)
		BLI_kdtree_free(tree);
}

/* updates cached particles' alive & other flags etc..*/
static void cached_step(Scene *scene, Object *ob, ParticleSystemModifierData *psmd, ParticleSystem *psys, float cfra)
{
	ParticleSettings *part=psys->part;
	ParticleKey state;
	IpoCurve *icu_esize= NULL; //=find_ipocurve(part->ipo,PART_EMIT_SIZE); // XXX old animation system
	Material *ma=give_current_material(ob,part->omat);
	PARTICLE_P;
	float disp, birthtime, dietime, *vg_size= NULL; // XXX ipotime=cfra

	BLI_srandom(psys->seed);

	if(part->from!=PART_FROM_PARTICLE)
		vg_size= psys_cache_vgroup(psmd->dm,psys,PSYS_VG_SIZE);

	if(psys->effectors.first)
		psys_end_effectors(psys);
	
	//if(part->flag & (PART_BAKED_GUIDES+PART_BAKED_DEATHS)){
		psys_init_effectors(scene, ob, part->eff_group, psys);
		if(psys->effectors.first)
			precalc_effectors(scene, ob,psys,psmd,cfra);
	//}
	
	disp= (float)get_current_display_percentage(psys)/100.0f;

	LOOP_PARTICLES {
#if 0 // XXX old animation system
		if((part->flag&PART_ABS_TIME)==0 && part->ipo){
			ipotime=100.0f*(cfra-pa->time)/pa->lifetime;
			calc_ipo(part->ipo, ipotime);
			execute_ipo((ID *)part, part->ipo);
		}
#endif // XXX old animation system
		pa->size= psys_get_size(ob,ma,psmd,icu_esize,psys,part,pa,vg_size);

		psys->lattice= psys_get_lattice(scene, ob, psys);

		if(part->flag & PART_LOOP && part->type!=PART_HAIR)
			pa->loop = (short)((cfra - pa->time) / pa->lifetime);
		else
			pa->loop = 0;

		birthtime = pa->time + pa->loop * pa->lifetime;
		dietime = birthtime + (1 + pa->loop) * (pa->dietime - pa->time);

		/* update alive status and push events */
		if(pa->time >= cfra) {
			pa->alive = pa->time==cfra ? PARS_ALIVE : PARS_UNBORN;
			if((psys->pointcache->flag & PTCACHE_EXTERNAL) == 0)
				reset_particle(scene, pa, psys, psmd, ob, 0.0f, cfra, NULL, NULL, NULL);
		}
		else if(dietime <= cfra){
			if(dietime > psys->cfra){
				state.time = dietime;
				psys_get_particle_state(scene, ob,psys,p,&state,1);
				push_reaction(ob,psys,p,PART_EVENT_DEATH,&state);
			}
			pa->alive = PARS_DEAD;
		}
		else{
			pa->alive = PARS_ALIVE;
			state.time = cfra;
			psys_get_particle_state(scene, ob,psys,p,&state,1);
			state.time = cfra;
			push_reaction(ob,psys,p,PART_EVENT_NEAR,&state);
		}

		if(psys->lattice){
			end_latt_deform(psys->lattice);
			psys->lattice= NULL;
		}

		if(BLI_frand() > disp)
			pa->flag |= PARS_NO_DISP;
		else
			pa->flag &= ~PARS_NO_DISP;
	}

	/* make sure that children are up to date */
	if(psys->part->childtype && psys->totchild != get_psys_tot_child(scene, psys)) {
		realloc_particles(ob, psys, psys->totpart);
		distribute_particles(scene, ob, psys, PART_FROM_CHILD);
	}

	psys_update_path_cache(scene, ob,psmd,psys,cfra);

	if(vg_size)
		MEM_freeN(vg_size);
}

static void psys_changed_type(Object *ob, ParticleSystem *psys)
{
	ParticleSettings *part;
	PTCacheID pid;

	part= psys->part;

	BKE_ptcache_id_from_particles(&pid, ob, psys);

	/* system type has changed so set sensible defaults and clear non applicable flags */
	if(part->from == PART_FROM_PARTICLE) {
		if(part->type != PART_REACTOR)
			part->from = PART_FROM_FACE;
		if(part->distr == PART_DISTR_GRID && part->from != PART_FROM_VERT)
			part->distr = PART_DISTR_JIT;
	}

	if(part->phystype != PART_PHYS_KEYED)
		psys->flag &= ~PSYS_KEYED;

	if(part->type == PART_HAIR) {
		if(ELEM4(part->ren_as, PART_DRAW_NOT, PART_DRAW_PATH, PART_DRAW_OB, PART_DRAW_GR)==0)
			part->ren_as = PART_DRAW_PATH;

		if(ELEM3(part->draw_as, PART_DRAW_NOT, PART_DRAW_REND, PART_DRAW_PATH)==0)
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

		if(!pa)
			return;

		if(psys->part && psys->part->phystype==PART_PHYS_BOIDS) {
			if(!pa->boid) {
				bpa = MEM_callocN(psys->totpart * sizeof(BoidParticle), "Boid Data");

				LOOP_PARTICLES
					pa->boid = bpa++;
			}
		}
		else if(pa->boid){
			MEM_freeN(pa->boid);
			LOOP_PARTICLES
				pa->boid = NULL;
		}
}
static void psys_changed_physics(Object *ob, ParticleSystem *psys)
{
	ParticleSettings *part = psys->part;

	if(ELEM(part->phystype, PART_PHYS_NO, PART_PHYS_KEYED)) {
		PTCacheID pid;
		BKE_ptcache_id_from_particles(&pid, ob, psys);
		BKE_ptcache_id_clear(&pid, PTCACHE_CLEAR_ALL, 0);
	}
	else {
		free_keyed_keys(psys);
		psys->flag &= ~PSYS_KEYED;
	}

	if(part->phystype == PART_PHYS_BOIDS && part->boids == NULL) {
		BoidState *state;

		psys_check_boid_data(psys);

		part->boids = MEM_callocN(sizeof(BoidSettings), "Boid Settings");
		boid_default_settings(part->boids);

		state = boid_new_state(part->boids);
		BLI_addtail(&state->rules, boid_new_rule(eBoidRuleType_Separate));
		BLI_addtail(&state->rules, boid_new_rule(eBoidRuleType_Flock));

		((BoidRule*)state->rules.first)->flag |= BOIDRULE_CURRENT;

		state->flag |= BOIDSTATE_CURRENT;
		BLI_addtail(&part->boids->states, state);
	}
}
static void particles_fluid_step(Scene *scene, Object *ob, ParticleSystem *psys, int cfra)
{	
	if(psys->particles){
		MEM_freeN(psys->particles);
		psys->particles = 0;
		psys->totpart = 0;
	}

	/* fluid sim particle import handling, actual loading of particles from file */
	#ifndef DISABLE_ELBEEM
	{
		FluidsimModifierData *fluidmd = (FluidsimModifierData *)modifiers_findByType(ob, eModifierType_Fluidsim);
		
		if( fluidmd && fluidmd->fss) { 
			FluidsimSettings *fss= fluidmd->fss;
			ParticleSettings *part = psys->part;
			ParticleData *pa=0;
			char *suffix  = "fluidsurface_particles_####";
			char *suffix2 = ".gz";
			char filename[256];
			char debugStrBuffer[256];
			int  curFrame = scene->r.cfra -1; // warning - sync with derived mesh fsmesh loading
			int  p, j, numFileParts, totpart;
			int readMask, activeParts = 0, fileParts = 0;
			gzFile gzf;
	
// XXX			if(ob==G.obedit) // off...
//				return;
	
			// ok, start loading
			strcpy(filename, fss->surfdataPath);
			strcat(filename, suffix);
			BLI_convertstringcode(filename, G.sce);
			BLI_convertstringframe(filename, curFrame); // fixed #frame-no 
			strcat(filename, suffix2);
	
			gzf = gzopen(filename, "rb");
			if (!gzf) {
				snprintf(debugStrBuffer,256,"readFsPartData::error - Unable to open file for reading '%s' \n", filename); 
				// XXX bad level call elbeemDebugOut(debugStrBuffer);
				return;
			}
	
			gzread(gzf, &totpart, sizeof(totpart));
			numFileParts = totpart;
			totpart = (G.rendering)?totpart:(part->disp*totpart)/100;
			
			part->totpart= totpart;
			part->sta=part->end = 1.0f;
			part->lifetime = scene->r.efra + 1;
	
			/* initialize particles */
			realloc_particles(ob, psys, part->totpart);
			initialize_all_particles(ob, psys, 0);
	
			// set up reading mask
			readMask = fss->typeFlags;
			
			for(p=0, pa=psys->particles; p<totpart; p++, pa++) {
				int ptype=0;
	
				gzread(gzf, &ptype, sizeof( ptype )); 
				if(ptype&readMask) {
					activeParts++;
	
					gzread(gzf, &(pa->size), sizeof( float )); 
	
					pa->size /= 10.0f;
	
					for(j=0; j<3; j++) {
						float wrf;
						gzread(gzf, &wrf, sizeof( wrf )); 
						pa->state.co[j] = wrf;
						//fprintf(stderr,"Rj%d ",j);
					}
					for(j=0; j<3; j++) {
						float wrf;
						gzread(gzf, &wrf, sizeof( wrf )); 
						pa->state.vel[j] = wrf;
					}
	
					pa->state.ave[0] = pa->state.ave[1] = pa->state.ave[2] = 0.0f;
					pa->state.rot[0] = 1.0;
					pa->state.rot[1] = pa->state.rot[2] = pa->state.rot[3] = 0.0;
	
					pa->alive = PARS_ALIVE;
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
	
			totpart = psys->totpart = activeParts;
			snprintf(debugStrBuffer,256,"readFsPartData::done - particles:%d, active:%d, file:%d, mask:%d  \n", psys->totpart,activeParts,fileParts,readMask);
			// bad level call
			// XXX elbeemDebugOut(debugStrBuffer);
			
		} // fluid sim particles done
	}
	#endif // DISABLE_ELBEEM
}

/* Calculates the next state for all particles of the system */
/* In particles code most fra-ending are frames, time-ending are fra*timestep (seconds)*/
static void system_step(Scene *scene, Object *ob, ParticleSystem *psys, ParticleSystemModifierData *psmd, float cfra)
{
	ParticleSettings *part;
	PointCache *cache;
	PTCacheID pid;
	PARTICLE_P;
	int totpart, oldtotpart, totchild, oldtotchild;
	float disp, *vg_vel= 0, *vg_tan= 0, *vg_rot= 0, *vg_size= 0;
	int init= 0, distr= 0, alloc= 0, usecache= 0, only_children_changed= 0;
	int framenr, framedelta, startframe, endframe;

	part= psys->part;
	cache= psys->pointcache;

	framenr= (int)scene->r.cfra;
	framedelta= framenr - cache->simframe;

	/* set suitable cache range automatically */
	if((cache->flag & (PTCACHE_BAKING|PTCACHE_BAKED))==0 && !(psys->flag & PSYS_HAIR_DYNAMICS))
		psys_get_pointcache_start_end(scene, psys, &cache->startframe, &cache->endframe);

	BKE_ptcache_id_from_particles(&pid, ob, psys);
	BKE_ptcache_id_time(&pid, scene, 0.0f, &startframe, &endframe, NULL);

	psys_clear_temp_pointcache(psys);

	/* update ipo's */
#if 0 // XXX old animation system
	if((part->flag & PART_ABS_TIME) && part->ipo) {
		calc_ipo(part->ipo, cfra);
		execute_ipo((ID *)part, part->ipo);
	}
#endif // XXX old animation system

	/* hair if it's already done is handled separate */
	if(part->type == PART_HAIR && (psys->flag & PSYS_HAIR_DONE)) {
		hair_step(scene, ob, psmd, psys, cfra);
		psys->cfra = cfra;
		psys->recalc = 0;
		return;
	}
	/* fluid is also handled separate */
	else if(part->type == PART_FLUID) {
		particles_fluid_step(scene, ob, psys, framenr);
		psys->cfra = cfra;
		psys->recalc = 0;
		return;
	}

	/* cache shouldn't be used for hair or "none" or "keyed" physics */
	if(part->type == PART_HAIR || ELEM(part->phystype, PART_PHYS_NO, PART_PHYS_KEYED))
		usecache= 0;
	else if(BKE_ptcache_get_continue_physics())
		usecache= 0;
	else
		usecache= 1;

	if(usecache) {
		/* frame clamping */
		if(framenr < startframe) {
			psys_reset(psys, PSYS_RESET_CACHE_MISS);
			psys->cfra = cfra;
			psys->recalc = 0;
			return;
		}
		else if(framenr > endframe) {
			framenr= endframe;
		}
	}

	/* verify if we need to reallocate */
	oldtotpart = psys->totpart;
	oldtotchild = psys->totchild;

	if(psys->pointcache->flag & PTCACHE_EXTERNAL)
		totpart = pid.cache->totpoint;
	else if(part->distr == PART_DISTR_GRID && part->from != PART_FROM_VERT)
		totpart = part->grid_res*part->grid_res*part->grid_res;
	else
		totpart = psys->part->totpart;
	totchild = get_psys_tot_child(scene, psys);

	if(oldtotpart != totpart || oldtotchild != totchild) {
		only_children_changed = (oldtotpart == totpart);
		alloc = 1;
		distr= 1;
		init= 1;
	}

	if(psys->recalc & PSYS_RECALC_RESET) {
		distr= 1;
		init= 1;
	}

	if(init) {
		if(distr) {
			if(alloc) {
				realloc_particles(ob, psys, totpart);

				if(usecache && !only_children_changed) {
					BKE_ptcache_id_clear(&pid, PTCACHE_CLEAR_ALL, 0);
					BKE_ptcache_id_from_particles(&pid, ob, psys);
				}
			}

			if(!only_children_changed)
				distribute_particles(scene, ob, psys, part->from);

			if((psys->part->type == PART_HAIR) && !(psys->flag & PSYS_HAIR_DONE))
			/* don't generate children while growing hair - waste of time */
				psys_free_children(psys);
			else if(get_psys_tot_child(scene, psys))
				distribute_particles(scene, ob, psys, PART_FROM_CHILD);
		}

		if(!only_children_changed) {
			free_keyed_keys(psys);

			initialize_all_particles(ob, psys, psmd);
			

			if(alloc) {
				reset_all_particles(scene, ob, psys, psmd, 0.0, cfra, oldtotpart);
			}
		}

		/* flag for possible explode modifiers after this system */
		psmd->flag |= eParticleSystemFlag_Pars;
	}

	/* try to read from the cache */
	if(usecache) {
		int result = BKE_ptcache_read_cache(&pid, cfra, scene->r.frs_sec);

		if(result == PTCACHE_READ_EXACT || result == PTCACHE_READ_INTERPOLATED) {
			cached_step(scene, ob, psmd, psys, cfra);
			psys->cfra=cfra;
			psys->recalc = 0;

			cache->simframe= framenr;
			cache->flag |= PTCACHE_SIMULATION_VALID;

			if(result == PTCACHE_READ_INTERPOLATED && cache->flag & PTCACHE_REDO_NEEDED)
				BKE_ptcache_write_cache(&pid, (int)cfra);

			return;
		}
		else if(result==PTCACHE_READ_OLD) {
			psys->cfra = (float)cache->simframe;
			LOOP_PARTICLES {
				/* update alive status */
				if(pa->time > psys->cfra)
					pa->alive = PARS_UNBORN;
				else if(pa->dietime <= psys->cfra)
					pa->alive = PARS_DEAD;
				else
					pa->alive = PARS_ALIVE;
			}
		}
		else if(ob->id.lib || (cache->flag & PTCACHE_BAKED)) {
			psys_reset(psys, PSYS_RESET_CACHE_MISS);
			psys->cfra=cfra;
			psys->recalc = 0;
			return;
		}
	}
	else {
		cache->flag &= ~PTCACHE_SIMULATION_VALID;
		cache->simframe= 0;
		cache->last_exact= 0;
	}

	/* if on second frame, write cache for first frame */
	if(usecache && psys->cfra == startframe && (cache->flag & PTCACHE_OUTDATED || cache->last_exact==0))
		BKE_ptcache_write_cache(&pid, startframe);

	if(part->phystype==PART_PHYS_KEYED)
		psys_count_keyed_targets(ob,psys);

	/* initialize vertex groups */
	if(part->from!=PART_FROM_PARTICLE) {
		vg_vel= psys_cache_vgroup(psmd->dm,psys,PSYS_VG_VEL);
		vg_tan= psys_cache_vgroup(psmd->dm,psys,PSYS_VG_TAN);
		vg_rot= psys_cache_vgroup(psmd->dm,psys,PSYS_VG_ROT);
		vg_size= psys_cache_vgroup(psmd->dm,psys,PSYS_VG_SIZE);
	}

	/* set particles to be not calculated TODO: can't work with pointcache */
	disp= (float)get_current_display_percentage(psys)/100.0f;

	BLI_srandom(psys->seed);
	LOOP_PARTICLES {
		if(BLI_frand() > disp)
			pa->flag |= PARS_NO_DISP;
		else
			pa->flag &= ~PARS_NO_DISP;
	}

	if(psys->totpart) {
		int dframe, totframesback = 0;

		/* handle negative frame start at the first frame by doing
		 * all the steps before the first frame */
		if(framenr == startframe && part->sta < startframe)
			totframesback = (startframe - (int)part->sta);

		for(dframe=-totframesback; dframe<=0; dframe++) {
			/* ok now we're all set so let's go */
			dynamics_step(scene, ob, psys, psmd, cfra+dframe, vg_vel, vg_tan, vg_rot, vg_size);
			psys->cfra = cfra+dframe;
		}
	}
	
	cache->simframe= framenr;
	cache->flag |= PTCACHE_SIMULATION_VALID;

	psys->recalc = 0;
	psys->cfra = cfra;

	/* only write cache starting from second frame */
	if(usecache && framenr != startframe)
		BKE_ptcache_write_cache(&pid, (int)cfra);

	/* for keyed particles the path is allways known so it can be drawn */
	if(part->phystype==PART_PHYS_KEYED) {
		set_keyed_keys(scene, ob, psys);
		psys_update_path_cache(scene, ob, psmd, psys,(int)cfra);
	}
	else if(psys->pathcache)
		psys_free_path_cache(psys, NULL);

	/* cleanup */
	if(vg_vel) MEM_freeN(vg_vel);
	if(vg_tan) MEM_freeN(vg_tan);
	if(vg_rot) MEM_freeN(vg_rot);
	if(vg_size) MEM_freeN(vg_size);

	if(psys->lattice){
		end_latt_deform(psys->lattice);
		psys->lattice= NULL;
	}
}

static int hair_needs_recalc(ParticleSystem *psys)
{
	if(!(psys->flag & PSYS_EDITED) && (!psys->edit || !psys->edit->edited) &&
		((psys->flag & PSYS_HAIR_DONE)==0 || psys->recalc & PSYS_RECALC_RESET)) {
		return 1;
	}

	return 0;
}

/* main particle update call, checks that things are ok on the large scale before actual particle calculations */
void particle_system_update(Scene *scene, Object *ob, ParticleSystem *psys)
{
	ParticleSystemModifierData *psmd;
	float cfra;

	if(!psys_check_enabled(ob, psys))
		return;

	cfra= bsystem_time(scene, ob, (float)scene->r.cfra, 0.0f);
	psmd= psys_get_modifier(ob, psys);

	/* system was already updated from modifier stack */
	if(psmd->flag & eParticleSystemFlag_psys_updated) {
		psmd->flag &= ~eParticleSystemFlag_psys_updated;
		/* make sure it really was updated to cfra */
		if(psys->cfra == cfra)
			return;
	}

	if(!psmd->dm)
		return;

	if(psys->recalc & PSYS_RECALC_TYPE)
		psys_changed_type(ob, psys);
	else if(psys->recalc & PSYS_RECALC_PHYS)
		psys_changed_physics(ob, psys);

	/* (re-)create hair */
	if(psys->part->type==PART_HAIR && hair_needs_recalc(psys)) {
		float hcfra=0.0f;
		int i;

		free_hair(ob, psys, 0);

		/* first step is negative so particles get killed and reset */
		psys->cfra= 1.0f;

		for(i=0; i<=psys->part->hair_step; i++){
			hcfra=100.0f*(float)i/(float)psys->part->hair_step;
			system_step(scene, ob, psys, psmd, hcfra);
			save_hair(scene, ob, psys, psmd, hcfra);
		}

		psys->flag |= PSYS_HAIR_DONE;
	}

	/* the main particle system step */
	system_step(scene, ob, psys, psmd, cfra);

	/* save matrix for duplicators */
	Mat4Invert(psys->imat, ob->obmat);
}

