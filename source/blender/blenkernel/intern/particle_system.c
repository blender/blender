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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2007 by Janne Karhu.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Raul Fernandez Hernandez (Farsthary), Stephen Swhitehorn.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include "BLI_storage.h" /* _LARGEFILE_SOURCE */

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
#include "BLI_math.h"
#include "BLI_blenlib.h"
#include "BLI_kdtree.h"
#include "BLI_kdopbvh.h"
#include "BLI_listbase.h"
#include "BLI_threads.h"

#include "BKE_anim.h"
#include "BKE_animsys.h"
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
	PARTICLE_P;

	if(ELEM(mode, PSYS_RESET_ALL, PSYS_RESET_DEPSGRAPH)) {
		if(mode == PSYS_RESET_ALL || !(psys->flag & PSYS_EDITED)) {
			psys_free_particles(psys);

			psys->totpart= 0;
			psys->totkeyed= 0;
			psys->flag &= ~(PSYS_HAIR_DONE|PSYS_KEYED);

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
	BKE_ptcache_invalidate(psys->pointcache);
}

static void realloc_particles(ParticleSimulationData *sim, int new_totpart)
{
	ParticleSystem *psys = sim->psys;
	ParticleSettings *part = psys->part;
	ParticleData *newpars = NULL;
	BoidParticle *newboids = NULL;
	PARTICLE_P;
	int totpart, totsaved = 0;

	if(new_totpart<0) {
		if(part->distr==PART_DISTR_GRID  && part->from != PART_FROM_VERT) {
			totpart= part->grid_res;
			totpart*=totpart*totpart;
		}
		else
			totpart=part->totpart;
	}
	else
		totpart=new_totpart;

	if(totpart && totpart != psys->totpart) {
		if(psys->edit && psys->free_edit) {
			psys->free_edit(psys->edit);
			psys->edit = NULL;
			psys->free_edit = NULL;
		}

		newpars= MEM_callocN(totpart*sizeof(ParticleData), "particles");
		if(psys->part->phystype == PART_PHYS_BOIDS)
			newboids= MEM_callocN(totpart*sizeof(BoidParticle), "boid particles");
	
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
			psys_free_pdd(psys);
		}
		
		psys->particles=newpars;
		psys->totpart=totpart;

		if(newboids) {
			LOOP_PARTICLES
				pa->boid = newboids++;
		}
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

	if(psys->renderdata)
		nbr= psys->part->ren_child_nbr;
	else
		nbr= psys->part->child_nbr;

	return get_render_child_particle_number(&scene->r, nbr);
}

static int get_psys_tot_child(struct Scene *scene, ParticleSystem *psys)
{
	return psys->totpart*get_psys_child_number(scene, psys);
}

static void alloc_child_particles(ParticleSystem *psys, int tot)
{
	if(psys->child){
		/* only re-allocate if we have to */
		if(psys->part->childtype && psys->totchild == tot) {
			memset(psys->child, 0, tot*sizeof(ChildParticle));
			return;
		}

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
			origindex= dm->getVertDataArray(dm, CD_ORIGINDEX);
		}
		else { /* FROM_FACE/FROM_VOLUME */
			totdmelem= dm->getNumFaces(dm);
			totelem= me->totface;
			origindex= dm->getFaceDataArray(dm, CD_ORIGINDEX);
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
				pa->hair_index=0; /* abused in volume calculation */
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
			sub_v3_v3v3(vec,mv->co,min);
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

						if(isect_axial_line_tri_v3(a,co1, co2, v2, v3, v1, &lambda)){
							if(from==PART_FROM_FACE)
								(pa+(int)(lambda*size[a])*a0mul)->flag &= ~PARS_UNEXIST;
							else /* store number of intersections */
								(pa+(int)(lambda*size[a])*a0mul)->hair_index++;
						}
						
						if(mface->v4){
							VECCOPY(v4,mvert[mface->v4].co);

							if(isect_axial_line_tri_v3(a,co1, co2, v4, v1, v3, &lambda)){
								if(from==PART_FROM_FACE)
									(pa+(int)(lambda*size[a])*a0mul)->flag &= ~PARS_UNEXIST;
								else
									(pa+(int)(lambda*size[a])*a0mul)->hair_index++;
							}
						}
					}

					if(from==PART_FROM_VOLUME){
						int in=pa->hair_index%2;
						if(in) pa->hair_index++;
						for(i=0; i<size[0]; i++){
							if(in || (pa+i*a0mul)->hair_index%2)
								(pa+i*a0mul)->flag &= ~PARS_UNEXIST;
							/* odd intersections == in->out / out->in */
							/* even intersections -> in stays same */
							in=(in + (pa+i*a0mul)->hair_index) % 2;
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
		interp_weights_poly_v3( w,vert, 4, co);
	}
	else {
		interp_weights_poly_v3( w,vert, 3, co);
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
static void psys_thread_distribute_particle(ParticleThread *thread, ParticleData *pa, ChildParticle *cpa, int p)
{
	ParticleThreadContext *ctx= thread->ctx;
	Object *ob= ctx->sim.ob;
	DerivedMesh *dm= ctx->dm;
	ParticleData *tpa;
/*	ParticleSettings *part= ctx->sim.psys->part; */
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
			break;
		case PART_DISTR_RAND:
			randu= rng_getFloat(thread->rng);
			randv= rng_getFloat(thread->rng);
			psys_uv_to_w(randu, randv, mface->v4, pa->fuv);
			break;
		}
		pa->foffset= 0.0f;
		
		/* experimental */
		if(from==PART_FROM_VOLUME){
			MVert *mvert=dm->getVertDataArray(dm,CD_MVERT);

			tot=dm->getNumFaces(dm);

			psys_interpolate_face(mvert,mface,0,0,pa->fuv,co1,nor,0,0,0,0);

			normalize_v3(nor);
			mul_v3_fl(nor,-100.0);

			VECADD(co2,co1,nor);

			min_d=2.0;
			intersect=0;

			for(i=0,mface=dm->getFaceDataArray(dm,CD_MFACE); i<tot; i++,mface++){
				if(i==pa->num) continue;

				v1=mvert[mface->v1].co;
				v2=mvert[mface->v2].co;
				v3=mvert[mface->v3].co;

				if(isect_line_tri_v3(co1, co2, v2, v3, v1, &cur_d, 0)){
					if(cur_d<min_d){
						min_d=cur_d;
						pa->foffset=cur_d*50.0f; /* to the middle of volume */
						intersect=1;
					}
				}
				if(mface->v4){
					v4=mvert[mface->v4].co;

					if(isect_line_tri_v3(co1, co2, v4, v1, v3, &cur_d, 0)){
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
			return;
		}

		mf= dm->getFaceData(dm, ctx->index[p], CD_MFACE);

		randu= rng_getFloat(thread->rng);
		randv= rng_getFloat(thread->rng);
		psys_uv_to_w(randu, randv, mf->v4, cpa->fuv);

		cpa->num = ctx->index[p];

		if(ctx->tree){
			KDTreeNearest ptn[10];
			int w,maxw;//, do_seams;
			float maxd,mind,dd,totw=0.0;
			int parent[10];
			float pweight[10];

			/*do_seams= (part->flag&PART_CHILD_SEAMS && ctx->seams);*/

			psys_particle_on_dm(dm,cfrom,cpa->num,DMCACHE_ISCHILD,cpa->fuv,cpa->foffset,co1,nor1,0,0,orco1,ornor1);
			transform_mesh_orco_verts((Mesh*)ob->data, &orco1, 1, 1);
			//maxw = BLI_kdtree_find_n_nearest(ctx->tree,(do_seams)?10:4,orco1,ornor1,ptn);
			maxw = BLI_kdtree_find_n_nearest(ctx->tree,4,orco1,ornor1,ptn);

			maxd=ptn[maxw-1].dist;
			mind=ptn[0].dist;
			dd=maxd-mind;
			
			/* the weights here could be done better */
			for(w=0; w<maxw; w++){
				parent[w]=ptn[w].index;
				pweight[w]=(float)pow(2.0,(double)(-6.0f*ptn[w].dist/maxd));
			}
			for(;w<10; w++){
				parent[w]=-1;
				pweight[w]=0.0f;
			}
			//if(do_seams){
			//	ParticleSeam *seam=ctx->seams;
			//	float temp[3],temp2[3],tan[3];
			//	float inp,cur_len,min_len=10000.0f;
			//	int min_seam=0, near_vert=0;
			//	/* find closest seam */
			//	for(i=0; i<ctx->totseam; i++, seam++){
			//		sub_v3_v3v3(temp,co1,seam->v0);
			//		inp=dot_v3v3(temp,seam->dir)/seam->length2;
			//		if(inp<0.0f){
			//			cur_len=len_v3v3(co1,seam->v0);
			//		}
			//		else if(inp>1.0f){
			//			cur_len=len_v3v3(co1,seam->v1);
			//		}
			//		else{
			//			copy_v3_v3(temp2,seam->dir);
			//			mul_v3_fl(temp2,inp);
			//			cur_len=len_v3v3(temp,temp2);
			//		}
			//		if(cur_len<min_len){
			//			min_len=cur_len;
			//			min_seam=i;
			//			if(inp<0.0f) near_vert=-1;
			//			else if(inp>1.0f) near_vert=1;
			//			else near_vert=0;
			//		}
			//	}
			//	seam=ctx->seams+min_seam;
			//	
			//	copy_v3_v3(temp,seam->v0);
			//	
			//	if(near_vert){
			//		if(near_vert==-1)
			//			sub_v3_v3v3(tan,co1,seam->v0);
			//		else{
			//			sub_v3_v3v3(tan,co1,seam->v1);
			//			copy_v3_v3(temp,seam->v1);
			//		}

			//		normalize_v3(tan);
			//	}
			//	else{
			//		copy_v3_v3(tan,seam->tan);
			//		sub_v3_v3v3(temp2,co1,temp);
			//		if(dot_v3v3(tan,temp2)<0.0f)
			//			negate_v3(tan);
			//	}
			//	for(w=0; w<maxw; w++){
			//		sub_v3_v3v3(temp2,ptn[w].co,temp);
			//		if(dot_v3v3(tan,temp2)<0.0f){
			//			parent[w]=-1;
			//			pweight[w]=0.0f;
			//		}
			//	}

			//}

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
	ParticleSystem *psys= thread->ctx->sim.psys;
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
static int psys_threads_init_distribution(ParticleThread *threads, Scene *scene, DerivedMesh *finaldm, int from)
{
	ParticleThreadContext *ctx= threads[0].ctx;
	Object *ob= ctx->sim.ob;
	ParticleSystem *psys= ctx->sim.psys;
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

	if (!finaldm->deformedOnly && !finaldm->getFaceDataArray(finaldm, CD_ORIGINDEX)) {
		printf("Can't create particles with the current modifier stack, disable destructive modifiers\n");
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

			//if(part->flag&PART_CHILD_SEAMS){
			//	MEdge *ed, *medge=dm->getEdgeDataArray(dm,CD_MEDGE);
			//	MVert *mvert=dm->getVertDataArray(dm,CD_MVERT);
			//	int totedge=dm->getNumEdges(dm);

			//	for(p=0, ed=medge; p<totedge; p++,ed++)
			//		if(ed->flag&ME_SEAM)
			//			totseam++;

			//	if(totseam){
			//		ParticleSeam *cur_seam=seams=MEM_callocN(totseam*sizeof(ParticleSeam),"Child Distribution Seams");
			//		float temp[3],temp2[3];

			//		for(p=0, ed=medge; p<totedge; p++,ed++){
			//			if(ed->flag&ME_SEAM){
			//				copy_v3_v3(cur_seam->v0,(mvert+ed->v1)->co);
			//				copy_v3_v3(cur_seam->v1,(mvert+ed->v2)->co);

			//				sub_v3_v3v3(cur_seam->dir,cur_seam->v1,cur_seam->v0);

			//				cur_seam->length2=len_v3(cur_seam->dir);
			//				cur_seam->length2*=cur_seam->length2;

			//				temp[0]=(float)((mvert+ed->v1)->no[0]);
			//				temp[1]=(float)((mvert+ed->v1)->no[1]);
			//				temp[2]=(float)((mvert+ed->v1)->no[2]);
			//				temp2[0]=(float)((mvert+ed->v2)->no[0]);
			//				temp2[1]=(float)((mvert+ed->v2)->no[1]);
			//				temp2[2]=(float)((mvert+ed->v2)->no[2]);

			//				add_v3_v3v3(cur_seam->nor,temp,temp2);
			//				normalize_v3(cur_seam->nor);

			//				cross_v3_v3v3(cur_seam->tan,cur_seam->dir,cur_seam->nor);

			//				normalize_v3(cur_seam->tan);

			//				cur_seam++;
			//			}
			//		}
			//	}
			//	
			//}
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
						length=len_v3(cpa->fuv);
					}

					cpa->num=-1;
				}
			}
			/* dmcache must be updated for parent particles if children from faces is used */
			psys_calc_dmcache(ob, finaldm, psys);

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
				cur= area_quad_v3(co1, co2, co3, co4);
			}
			else
				cur= area_tri_v3(co1, co2, co3);
			
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
	ctx->sim.psys= psys;
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
	
	seed= 31415926 + ctx->sim.psys->seed;
	for(i=0; i<totthread; i++) {
		threads[i].rng= rng_new(seed);
		threads[i].tot= totthread;
	}

	return 1;
}

static void distribute_particles_on_dm(ParticleSimulationData *sim, int from)
{
	DerivedMesh *finaldm = sim->psmd->dm;
	ListBase threads;
	ParticleThread *pthreads;
	ParticleThreadContext *ctx;
	int i, totthread;

	pthreads= psys_threads_create(sim);

	if(!psys_threads_init_distribution(pthreads, sim->scene, finaldm, from)) {
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

	psys_calc_dmcache(sim->ob, finaldm, sim->psys);

	ctx= pthreads[0].ctx;
	if(ctx->dm != finaldm)
		ctx->dm->release(ctx->dm);

	psys_threads_free(pthreads);
}

/* ready for future use, to emit particles without geometry */
static void distribute_particles_on_shape(ParticleSimulationData *sim, int from)
{
	ParticleSystem *psys = sim->psys;
	PARTICLE_P;

	fprintf(stderr,"Shape emission not yet possible!\n");

	LOOP_PARTICLES {
		pa->fuv[0]=pa->fuv[1]=pa->fuv[2]=pa->fuv[3]= 0.0;
		pa->foffset= 0.0f;
		pa->num= -1;
	}
}
static void distribute_particles(ParticleSimulationData *sim, int from)
{
	PARTICLE_PSMD;
	int distr_error=0;

	if(psmd){
		if(psmd->dm)
			distribute_particles_on_dm(sim, from);
		else
			distr_error=1;
	}
	else
		distribute_particles_on_shape(sim, from);

	if(distr_error){
		ParticleSystem *psys = sim->psys;
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
ParticleThread *psys_threads_create(ParticleSimulationData *sim)
{
	ParticleThread *threads;
	ParticleThreadContext *ctx;
	int i, totthread;

	if(sim->scene->r.mode & R_FIXED_THREADS)
		totthread= sim->scene->r.threads;
	else
		totthread= BLI_system_thread_count();
	
	threads= MEM_callocN(sizeof(ParticleThread)*totthread, "ParticleThread");
	ctx= MEM_callocN(sizeof(ParticleThreadContext), "ParticleThreadContext");

	ctx->sim = *sim;
	ctx->dm= ctx->sim.psmd->dm;
	ctx->ma= give_current_material(sim->ob, sim->psys->part->omat);

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

	if(ctx->sim.psys->lattice){
		end_latt_deform(ctx->sim.psys->lattice);
		ctx->sim.psys->lattice= NULL;
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
void initialize_particle(ParticleSimulationData *sim, ParticleData *pa, int p)
{
	ParticleSettings *part = sim->psys->part;
	ParticleTexture ptex;
	Material *ma=0;
	//IpoCurve *icu=0; // XXX old animation system
	int totpart;

	totpart=sim->psys->totpart;

	ptex.life=ptex.size=ptex.exist=ptex.length=1.0;
	ptex.time=(float)p/(float)totpart;

	BLI_srandom(sim->psys->seed + p + 125);

	if(part->from!=PART_FROM_PARTICLE && part->type!=PART_FLUID){
		ma=give_current_material(sim->ob,part->omat);

		/* TODO: needs some work to make most blendtypes generally usefull */
		psys_get_texture(sim,ma,pa,&ptex,MAP_PA_INIT);
	}
	
	pa->lifetime= part->lifetime*ptex.life;

	if(part->type==PART_HAIR)
		pa->time= 0.0f;
	//else if(part->type==PART_REACTOR && (part->flag&PART_REACT_STA_END)==0)
	//	pa->time= 300000.0f;	/* max frame */
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

		if(part->randlife!=0.0)
			pa->lifetime*= 1.0f - part->randlife * BLI_frand();
	}

	pa->dietime= pa->time+pa->lifetime;

	if(part->type!=PART_HAIR && part->distr!=PART_DISTR_GRID && part->from != PART_FROM_VERT){
		if(ptex.exist < BLI_frand())
			pa->flag |= PARS_UNEXIST;
		else
			pa->flag &= ~PARS_UNEXIST;
	}

	pa->hair_index=0;
	/* we can't reset to -1 anymore since we've figured out correct index in distribute_particles */
	/* usage other than straight after distribute has to handle this index by itself - jahka*/
	//pa->num_dmcache = DMCACHE_NOTFOUND; /* assume we dont have a derived mesh face */
}
static void initialize_all_particles(ParticleSimulationData *sim)
{
	//IpoCurve *icu=0; // XXX old animation system
	ParticleSystem *psys = sim->psys;
	PARTICLE_P;

	LOOP_PARTICLES
		initialize_particle(sim, pa, p);
	
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
void reset_particle(ParticleSimulationData *sim, ParticleData *pa, float dtime, float cfra)
{
	Object *ob = sim->ob;
	ParticleSystem *psys = sim->psys;
	ParticleSettings *part;
	ParticleTexture ptex;
	ParticleKey state;
	//IpoCurve *icu=0; // XXX old animation system
	float fac, phasefac, nor[3]={0,0,0},loc[3],vel[3]={0.0,0.0,0.0},rot[4],q2[4];
	float r_vel[3],r_ave[3],r_rot[4],vec[3],p_vel[3]={0.0,0.0,0.0};
	float x_vec[3]={1.0,0.0,0.0}, utan[3]={0.0,1.0,0.0}, vtan[3]={0.0,0.0,1.0}, rot_vec[3]={0.0,0.0,0.0};
	float q_phase[4], r_phase;
	int p = pa - psys->particles;
	part=psys->part;

	ptex.ivel=1.0;

	/* we need to get every random even if they're not used so that they don't effect eachother */
	r_vel[0] = 2.0f * (PSYS_FRAND(p + 10) - 0.5f);
	r_vel[1] = 2.0f * (PSYS_FRAND(p + 11) - 0.5f);
	r_vel[2] = 2.0f * (PSYS_FRAND(p + 12) - 0.5f);

	r_ave[0] = 2.0f * (PSYS_FRAND(p + 13) - 0.5f);
	r_ave[1] = 2.0f * (PSYS_FRAND(p + 14) - 0.5f);
	r_ave[2] = 2.0f * (PSYS_FRAND(p + 15) - 0.5f);

	r_rot[0] = 2.0f * (PSYS_FRAND(p + 16) - 0.5f);
	r_rot[1] = 2.0f * (PSYS_FRAND(p + 17) - 0.5f);
	r_rot[2] = 2.0f * (PSYS_FRAND(p + 18) - 0.5f);
	r_rot[3] = 2.0f * (PSYS_FRAND(p + 19) - 0.5f);
	normalize_qt(r_rot);

	r_phase = PSYS_FRAND(p + 20);
	
	if(part->from==PART_FROM_PARTICLE){
		ParticleSimulationData tsim = {sim->scene, psys->target_ob ? psys->target_ob : ob, NULL, NULL};
		float speed;

		tsim.psys = BLI_findlink(&tsim.ob->particlesystem, sim->psys->target_psys-1);

		state.time = pa->time;
		if(pa->num == -1)
			memset(&state, 0, sizeof(state));
		else
			psys_get_particle_state(&tsim, pa->num, &state, 1);
		psys_get_from_key(&state, loc, nor, rot, 0);

		mul_qt_v3(rot, vtan);
		mul_qt_v3(rot, utan);

		VECCOPY(p_vel, state.vel);
		speed=normalize_v3(p_vel);
		mul_v3_fl(p_vel, dot_v3v3(r_vel, p_vel));
		VECSUB(p_vel, r_vel, p_vel);
		normalize_v3(p_vel);
		mul_v3_fl(p_vel, speed);

		VECCOPY(pa->fuv, loc); /* abusing pa->fuv (not used for "from particle") for storing emit location */
	}
	else{
		/* get precise emitter matrix if particle is born */
		if(part->type!=PART_HAIR && pa->time < cfra && pa->time >= sim->psys->cfra) {
			/* we have to force RECALC_ANIM here since where_is_objec_time only does drivers */
			BKE_animsys_evaluate_animdata(&sim->ob->id, sim->ob->adt, pa->time, ADT_RECALC_ANIM);
			where_is_object_time(sim->scene, sim->ob, pa->time);
		}

		/* get birth location from object		*/
		if(part->tanfac!=0.0)
			psys_particle_on_emitter(sim->psmd, part->from,pa->num, pa->num_dmcache, pa->fuv,pa->foffset,loc,nor,utan,vtan,0,0);
		else
			psys_particle_on_emitter(sim->psmd, part->from,pa->num, pa->num_dmcache, pa->fuv,pa->foffset,loc,nor,0,0,0,0);
		
		/* get possible textural influence */
		psys_get_texture(sim, give_current_material(sim->ob,part->omat), pa, &ptex, MAP_PA_IVEL);

		//if(vg_vel && pa->num != -1)
		//	ptex.ivel*=psys_particle_value_from_verts(sim->psmd->dm,part->from,pa,vg_vel);

		/* particles live in global space so	*/
		/* let's convert:						*/
		/* -location							*/
		mul_m4_v3(ob->obmat,loc);
		
		/* -normal								*/
		mul_mat3_m4_v3(ob->obmat,nor);
		normalize_v3(nor);

		/* -tangent								*/
		if(part->tanfac!=0.0){
			//float phase=vg_rot?2.0f*(psys_particle_value_from_verts(sim->psmd->dm,part->from,pa,vg_rot)-0.5f):0.0f;
			float phase=0.0f;
			mul_v3_fl(vtan,-(float)cos(M_PI*(part->tanphase+phase)));
			fac=-(float)sin(M_PI*(part->tanphase+phase));
			VECADDFAC(vtan,vtan,utan,fac);

			mul_mat3_m4_v3(ob->obmat,vtan);

			VECCOPY(utan,nor);
			mul_v3_fl(utan,dot_v3v3(vtan,nor));
			VECSUB(vtan,vtan,utan);
			
			normalize_v3(vtan);
		}
		

		/* -velocity							*/
		if(part->randfac!=0.0){
			mul_mat3_m4_v3(ob->obmat,r_vel);
			normalize_v3(r_vel);
		}

		/* -angular velocity					*/
		if(part->avemode==PART_AVE_RAND){
			mul_mat3_m4_v3(ob->obmat,r_ave);
			normalize_v3(r_ave);
		}
		
		/* -rotation							*/
		if(part->randrotfac != 0.0f){
			mat4_to_quat(rot,ob->obmat);
			mul_qt_qtqt(r_rot,r_rot,rot);
		}
	}

	if(part->phystype==PART_PHYS_BOIDS && pa->boid) {
		BoidParticle *bpa = pa->boid;
		float dvec[3], q[4], mat[3][3];

		VECCOPY(pa->state.co,loc);

		/* boids don't get any initial velocity  */
		pa->state.vel[0]=pa->state.vel[1]=pa->state.vel[2]=0.0f;

		/* boids store direction in ave */
		if(fabs(nor[2])==1.0f) {
			sub_v3_v3v3(pa->state.ave, loc, ob->obmat[3]);
			normalize_v3(pa->state.ave);
		}
		else {
			VECCOPY(pa->state.ave, nor);
		}
		/* and gravity in r_ve */
		bpa->gravity[0] = bpa->gravity[1] = 0.0f;
		bpa->gravity[2] = -1.0f;
		if((sim->scene->physics_settings.flag & PHYS_GLOBAL_GRAVITY)
			&& sim->scene->physics_settings.gravity[2]!=0.0f)
			bpa->gravity[2] = sim->scene->physics_settings.gravity[2];

		/* calculate rotation matrix */
		project_v3_v3v3(dvec, r_vel, pa->state.ave);
		sub_v3_v3v3(mat[0], pa->state.ave, dvec);
		normalize_v3(mat[0]);
		negate_v3_v3(mat[2], r_vel);
		normalize_v3(mat[2]);
		cross_v3_v3v3(mat[1], mat[2], mat[0]);
		
		/* apply rotation */
		mat3_to_quat_is_ok( q,mat);
		copy_qt_qt(pa->state.rot, q);

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
			mul_v3_fl(vel,part->obfac/dtime);
		}
		
		/*		*emitter normal					*/
		if(part->normfac!=0.0)
			VECADDFAC(vel,vel,nor,part->normfac);
		
		/*		*emitter tangent				*/
		if(sim->psmd && part->tanfac!=0.0)
			VECADDFAC(vel,vel,vtan,part->tanfac);
			//VECADDFAC(vel,vel,vtan,part->tanfac*(vg_tan?psys_particle_value_from_verts(sim->psmd->dm,part->from,pa,vg_tan):1.0f));

		/*		*emitter object orientation		*/
		if(part->ob_vel[0]!=0.0) {
			VECCOPY(vec, ob->obmat[0]);
			normalize_v3(vec);
			VECADDFAC(vel, vel, vec, part->ob_vel[0]);
		}
		if(part->ob_vel[1]!=0.0) {
			VECCOPY(vec, ob->obmat[1]);
			normalize_v3(vec);
			VECADDFAC(vel, vel, vec, part->ob_vel[1]);
		}
		if(part->ob_vel[2]!=0.0) {
			VECCOPY(vec, ob->obmat[2]);
			normalize_v3(vec);
			VECADDFAC(vel, vel, vec, part->ob_vel[2]);
		}

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

		mul_v3_fl(vel,ptex.ivel);
		
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
					copy_v3_v3(rot_vec, nor);
					break;
				case PART_ROT_VEL:
					copy_v3_v3(rot_vec, vel);
					break;
				case PART_ROT_GLOB_X:
				case PART_ROT_GLOB_Y:
				case PART_ROT_GLOB_Z:
					rot_vec[part->rotmode - PART_ROT_GLOB_X] = 1.0f;
					break;
				case PART_ROT_OB_X:
				case PART_ROT_OB_Y:
				case PART_ROT_OB_Z:
					copy_v3_v3(rot_vec, ob->obmat[part->rotmode - PART_ROT_OB_X]);
					break;
			}
			
			/* create rotation quat */
			negate_v3(rot_vec);
			vec_to_quat( q2,rot_vec, OB_POSX, OB_POSZ);

			/* randomize rotation quat */
			if(part->randrotfac!=0.0f)
				interp_qt_qtqt(rot, q2, r_rot, part->randrotfac);
			else
				copy_qt_qt(rot,q2);

			/* rotation phase */
			phasefac = part->phasefac;
			if(part->randphasefac != 0.0f)
				phasefac += part->randphasefac * r_phase;
			axis_angle_to_quat( q_phase,x_vec, phasefac*(float)M_PI);

			/* combine base rotation & phase */
			mul_qt_qtqt(pa->state.rot, rot, q_phase);
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
			normalize_v3(pa->state.ave);
			mul_v3_fl(pa->state.ave,part->avefac);

			//icu=find_ipocurve(psys->part->ipo,PART_EMIT_AVE);
			//if(icu){
			//	calc_icu(icu,100*((pa->time-part->sta)/(part->end-part->sta)));
			//	mul_v3_fl(pa->state.ave,icu->curval);
			//}
		}
	}

	pa->dietime = pa->time + pa->lifetime;

	if(pa->time > cfra)
		pa->alive = PARS_UNBORN;
	else if(pa->dietime <= cfra)
		pa->alive = PARS_DEAD;
	else
		pa->alive = PARS_ALIVE;

	pa->state.time = cfra;
}
static void reset_all_particles(ParticleSimulationData *sim, float dtime, float cfra, int from)
{
	ParticleData *pa;
	int p, totpart=sim->psys->totpart;
	//float *vg_vel=psys_cache_vgroup(sim->psmd->dm,sim->psys,PSYS_VG_VEL);
	//float *vg_tan=psys_cache_vgroup(sim->psmd->dm,sim->psys,PSYS_VG_TAN);
	//float *vg_rot=psys_cache_vgroup(sim->psmd->dm,sim->psys,PSYS_VG_ROT);
	
	for(p=from, pa=sim->psys->particles+from; p<totpart; p++, pa++)
		reset_particle(sim, pa, dtime, cfra);

	//if(vg_vel)
	//	MEM_freeN(vg_vel);
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
void psys_count_keyed_targets(ParticleSimulationData *sim)
{
	ParticleSystem *psys = sim->psys, *kpsys;
	ParticleTarget *pt = psys->targets.first;
	int keys_valid = 1;
	psys->totkeyed = 0;

	for(; pt; pt=pt->next) {
		kpsys = psys_get_target_system(sim->ob, pt);

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

static void set_keyed_keys(ParticleSimulationData *sim)
{
	ParticleSystem *psys = sim->psys;
	ParticleSimulationData ksim = {sim->scene, NULL, NULL, NULL};
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
		ksim.ob = pt->ob ? pt->ob : sim->ob;
		ksim.psys = BLI_findlink(&ksim.ob->particlesystem, pt->psys - 1);

		LOOP_PARTICLES {
			key = pa->keys + k;
			key->time = -1.0; /* use current time */

			psys_get_particle_state(&ksim, p%ksim.psys->totpart, key, 1);

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
//static void push_reaction(ParticleSimulationData *sim, int pa_num, int event, ParticleKey *state)
//{
//	Object *rob;
//	ParticleSystem *rpsys;
//	ParticleSettings *rpart;
//	ParticleData *pa;
//	ListBase *lb=&sim->psys->effectors;
//	ParticleEffectorCache *ec;
//	ParticleReactEvent *re;
//
//	if(lb->first) for(ec = lb->first; ec; ec= ec->next){
//		if(ec->type & PSYS_EC_REACTOR){
//			/* all validity checks already done in add_to_effectors */
//			rob=ec->ob;
//			rpsys=BLI_findlink(&rob->particlesystem,ec->psys_nbr);
//			rpart=rpsys->part;
//			if(rpsys->part->reactevent==event){
//				pa=sim->psys->particles+pa_num;
//				re= MEM_callocN(sizeof(ParticleReactEvent), "react event");
//				re->event=event;
//				re->pa_num = pa_num;
//				re->ob = sim->ob;
//				re->psys = sim->psys;
//				re->size = pa->size;
//				copy_particle_key(&re->state,state,1);
//
//				switch(event){
//					case PART_EVENT_DEATH:
//						re->time=pa->dietime;
//						break;
//					case PART_EVENT_COLLIDE:
//						re->time=state->time;
//						break;
//					case PART_EVENT_NEAR:
//						re->time=state->time;
//						break;
//				}
//
//				BLI_addtail(&rpsys->reactevents, re);
//			}
//		}
//	}
//}
//static void react_to_events(ParticleSystem *psys, int pa_num)
//{
//	ParticleSettings *part=psys->part;
//	ParticleData *pa=psys->particles+pa_num;
//	ParticleReactEvent *re=psys->reactevents.first;
//	int birth=0;
//	float dist=0.0f;
//
//	for(re=psys->reactevents.first; re; re=re->next){
//		birth=0;
//		if(part->from==PART_FROM_PARTICLE){
//			if(pa->num==re->pa_num && pa->alive==PARS_UNBORN){
//				if(re->event==PART_EVENT_NEAR){
//					ParticleData *tpa = re->psys->particles+re->pa_num;
//					float pa_time=tpa->time + pa->foffset*tpa->lifetime;
//					if(re->time >= pa_time){
//						pa->time=pa_time;
//						pa->dietime=pa->time+pa->lifetime;
//					}
//				}
//				else{
//					pa->time=re->time;
//					pa->dietime=pa->time+pa->lifetime;
//				}
//			}
//		}
//		else{
//			dist=len_v3v3(pa->state.co, re->state.co);
//			if(dist <= re->size){
//				if(pa->alive==PARS_UNBORN){
//					pa->time=re->time;
//					pa->dietime=pa->time+pa->lifetime;
//					birth=1;
//				}
//				if(birth || part->flag&PART_REACT_MULTIPLE){
//					float vec[3];
//					VECSUB(vec,pa->state.co, re->state.co);
//					if(birth==0)
//						mul_v3_fl(vec,(float)pow(1.0f-dist/re->size,part->reactshape));
//					VECADDFAC(pa->state.vel,pa->state.vel,vec,part->reactfac);
//					VECADDFAC(pa->state.vel,pa->state.vel,re->state.vel,part->partfac);
//				}
//				if(birth)
//					mul_v3_fl(pa->state.vel,(float)pow(1.0f-dist/re->size,part->reactshape));
//			}
//		}
//	}
//}
//void psys_get_reactor_target(ParticleSimulationData *sim, Object **target_ob, ParticleSystem **target_psys)
//{
//	Object *tob;
//
//	tob = sim->psys->target_ob ? sim->psys->target_ob : sim->ob;
//	
//	*target_psys = BLI_findlink(&tob->particlesystem, sim->psys->target_psys-1);
//	if(*target_psys)
//		*target_ob=tob;
//	else
//		*target_ob=0;
//}
/************************************************/
/*			Point Cache							*/
/************************************************/
void psys_make_temp_pointcache(Object *ob, ParticleSystem *psys)
{
	PointCache *cache = psys->pointcache;

	if(cache->flag & PTCACHE_DISK_CACHE && cache->mem_cache.first == NULL) {
		PTCacheID pid;
		BKE_ptcache_id_from_particles(&pid, ob, psys);
		BKE_ptcache_disk_to_mem(&pid);
	}
}
static void psys_clear_temp_pointcache(ParticleSystem *psys)
{
	if(psys->pointcache->flag & PTCACHE_DISK_CACHE)
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
void psys_update_particle_tree(ParticleSystem *psys, float cfra)
{
	if(psys) {
		PARTICLE_P;

		if(!psys->tree || psys->tree_frame != cfra) {
			
			BLI_kdtree_free(psys->tree);

			psys->tree = BLI_kdtree_new(psys->totpart);
			
			LOOP_SHOWN_PARTICLES {
				if(pa->alive == PARS_ALIVE) {
					if(pa->state.time == cfra)
						BLI_kdtree_insert(psys->tree, p, pa->prev_state.co, NULL);
					else
						BLI_kdtree_insert(psys->tree, p, pa->state.co, NULL);
				}
			}
			BLI_kdtree_balance(psys->tree);

			psys->tree_frame = psys->cfra;
		}
	}
}

static void psys_update_effectors(ParticleSimulationData *sim)
{
	pdEndEffectors(&sim->psys->effectors);
	sim->psys->effectors = pdInitEffectors(sim->scene, sim->ob, sim->psys, sim->psys->part->effector_weights);
	precalc_guides(sim, sim->psys->effectors);
}

/*************************************************
                    SPH fluid physics 

 In theory, there could be unlimited implementation
                    of SPH simulators
**************************************************/
void particle_fluidsim(ParticleSystem *psys, ParticleData *pa, ParticleSettings *part, ParticleSimulationData *sim, float dfra, float cfra, float mass){
/****************************************************************************************************************
* 	This code uses in some parts adapted algorithms from the pseduo code as outlined in the Research paper
*	Titled: Particle-based Viscoelastic Fluid Simulation.
* 	Authors: Simon Clavet, Philippe Beaudoin and Pierre Poulin
*
*	Website: http://www.iro.umontreal.ca/labs/infographie/papers/Clavet-2005-PVFS/
*	Presented at Siggraph, (2005)
*
*****************************************************************************************************************/
	KDTree *tree = psys->tree;
	KDTreeNearest *ptn = NULL;
	
	SPHFluidSettings *fluid = part->fluid;
	ParticleData *second_particle;

	float start[3], end[3], v[3];
	float temp[3];
	float q, radius, D;
	float p, pnear, pressure_near, pressure;
	float dtime = dfra * psys_get_timestep(sim);
	float omega = fluid->viscosity_omega;
	float beta = fluid->viscosity_omega;
	float massfactor = 1.0f/mass;
	int n, neighbours;

		
	radius 	= fluid->radius;

	VECCOPY(start, pa->prev_state.co);
	VECCOPY(end, pa->state.co);

	sub_v3_v3v3(v, end, start);
	mul_v3_fl(v, 1.f/dtime);

	neighbours = BLI_kdtree_range_search(tree, radius, start, NULL, &ptn);

	/* use ptn[n].co to store relative direction */
	for(n=1; n<neighbours; n++) {
		sub_v3_v3(ptn[n].co, start);
		normalize_v3(ptn[n].co);
	}
        
	/* Viscosity - Algorithm 5  */
	if (omega > 0.f || beta > 0.f) {
		float u, I;

		for(n=1; n<neighbours; n++) {
			second_particle = psys->particles + ptn[n].index;
			q = ptn[n].dist/radius;
			
			sub_v3_v3v3(temp, v, second_particle->prev_state.vel);
			
			u = dot_v3v3(ptn[n].co, temp);

			if (u > 0){
				I = dtime * ((1-q) * (omega * u + beta * u*u)) * 0.5f;
				madd_v3_v3fl(v, ptn[n].co, -I * massfactor);
			} 
		}	
	}

	/* Hooke's spring force  */
	if (fluid->spring_k > 0.f) {
		float D, L = fluid->rest_length;
		for(n=1; n<neighbours; n++) {
			/* L is a factor of radius */
			D = dtime * 10.f * fluid->spring_k * (1.f - L) * (L - ptn[n].dist/radius);
			madd_v3_v3fl(v, ptn[n].co, -D * massfactor);
		}
	}
	/* Update particle position */	
	VECADDFAC(end, start, v, dtime);

	/* Double Density Relaxation - Algorithm 2 */
	p = 0;
	pnear = 0;
	for(n=1; n<neighbours; n++) {
		q = ptn[n].dist/radius;
		p += ((1-q)*(1-q));
		pnear += ((1-q)*(1-q)*(1-q));
	}
	p *= part->mass;
	pnear *= part->mass;
	pressure =  fluid->stiffness_k * (p - fluid->rest_density);
	pressure_near = fluid->stiffness_knear * pnear;

	for(n=1; n<neighbours; n++) {
		q = ptn[n].dist/radius;

		D =  dtime * dtime * (pressure*(1-q) + pressure_near*(1-q)*(1-q))* 0.5f;
		madd_v3_v3fl(end, ptn[n].co, -D * massfactor);
	} 	

	/* Artificial buoyancy force in negative gravity direction  */
	if (fluid->buoyancy >= 0.f && psys_uses_gravity(sim)) {
		float B = -dtime * dtime * fluid->buoyancy * (p - fluid->rest_density) * 0.5f;
		madd_v3_v3fl(end, sim->scene->physics_settings.gravity, -B * massfactor);
	}

	/* apply final result and recalculate velocity */
	VECCOPY(pa->state.co, end);
	sub_v3_v3v3(pa->state.vel, end, start);
	mul_v3_fl(pa->state.vel, 1.f/dtime);

	if(ptn){ MEM_freeN(ptn); ptn=NULL;}
}

static void apply_particle_fluidsim(ParticleSystem *psys, ParticleData *pa, ParticleSettings *part, ParticleSimulationData *sim, float dfra, float cfra){
	ParticleTarget *pt;
	float dtime = dfra*psys_get_timestep(sim);
	float particle_mass = part->mass;

	particle_fluidsim(psys, pa, part, sim, dfra, cfra, particle_mass);
	
	/*----check other SPH systems (Multifluids) , each fluid has its own parameters---*/
	for(pt=sim->psys->targets.first; pt; pt=pt->next) {
		ParticleSystem *epsys = psys_get_target_system(sim->ob, pt);

		if(epsys)
			particle_fluidsim(epsys, pa, epsys->part, sim, dfra, cfra, particle_mass);
	}
	/*----------------------------------------------------------------*/	 	 
}

/************************************************/
/*			Newtonian physics					*/
/************************************************/
/* gathers all forces that effect particles and calculates a new state for the particle */
static void apply_particle_forces(ParticleSimulationData *sim, int p, float dfra, float cfra)
{
	ParticleSettings *part = sim->psys->part;
	ParticleData *pa = sim->psys->particles + p;
	EffectedPoint epoint;
	ParticleKey states[5], tkey;
	float timestep = psys_get_timestep(sim);
	float force[3],impulse[3],dx[4][3],dv[4][3],oldpos[3];
	float dtime=dfra*timestep, time, pa_mass=part->mass, fac, fra=sim->psys->cfra;
	int i, steps=1;
	
	/* maintain angular velocity */
	VECCOPY(pa->state.ave,pa->prev_state.ave);
	VECCOPY(oldpos,pa->state.co);

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
		case PART_INT_VERLET:
			steps=1;
			break;
	}

	copy_particle_key(states,&pa->state,1);

	for(i=0; i<steps; i++){
		force[0]=force[1]=force[2]=0.0;
		impulse[0]=impulse[1]=impulse[2]=0.0;
		/* add effectors */
		pd_point_from_particle(sim, pa, states+i, &epoint);
		if(part->type != PART_HAIR || part->effector_weights->flag & EFF_WEIGHT_DO_HAIR)
			pdDoEffectors(sim->psys->effectors, sim->colliders, part->effector_weights, &epoint, force, impulse);

		/* calculate air-particle interaction */
		if(part->dragfac!=0.0f){
			fac=-part->dragfac*pa->size*pa->size*len_v3(states[i].vel);
			VECADDFAC(force,force,states[i].vel,fac);
		}

		/* brownian force */
		if(part->brownfac!=0.0){
			force[0]+=(BLI_frand()-0.5f)*part->brownfac;
			force[1]+=(BLI_frand()-0.5f)*part->brownfac;
			force[2]+=(BLI_frand()-0.5f)*part->brownfac;
		}

		/* force to acceleration*/
		mul_v3_fl(force,1.0f/pa_mass);

		/* add global acceleration (gravitation) */
		if(psys_uses_gravity(sim)
			/* normal gravity is too strong for hair so it's disabled by default */
			&& (part->type != PART_HAIR || part->effector_weights->flag & EFF_WEIGHT_DO_HAIR)) {
			float gravity[3];
			VECCOPY(gravity, sim->scene->physics_settings.gravity);
			mul_v3_fl(gravity, part->effector_weights->global_gravity);
			VECADD(force,force,gravity);
		}
		
		/* calculate next state */
		VECADD(states[i].vel,states[i].vel,impulse);

		switch(part->integrator){
			case PART_INT_EULER:
				VECADDFAC(pa->state.co,states->co,states->vel,dtime);
				VECADDFAC(pa->state.vel,states->vel,force,dtime);
				break;
			case PART_INT_MIDPOINT:
				if(i==0){
					VECADDFAC(states[1].co,states->co,states->vel,dtime*0.5f);
					VECADDFAC(states[1].vel,states->vel,force,dtime*0.5f);
					fra=sim->psys->cfra+0.5f*dfra;
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
						mul_v3_fl(dx[0],dtime);
						VECCOPY(dv[0],force);
						mul_v3_fl(dv[0],dtime);

						VECADDFAC(states[1].co,states->co,dx[0],0.5f);
						VECADDFAC(states[1].vel,states->vel,dv[0],0.5f);
						fra=sim->psys->cfra+0.5f*dfra;
						break;
					case 1:
						VECADDFAC(dx[1],states->vel,dv[0],0.5f);
						mul_v3_fl(dx[1],dtime);
						VECCOPY(dv[1],force);
						mul_v3_fl(dv[1],dtime);

						VECADDFAC(states[2].co,states->co,dx[1],0.5f);
						VECADDFAC(states[2].vel,states->vel,dv[1],0.5f);
						break;
					case 2:
						VECADDFAC(dx[2],states->vel,dv[1],0.5f);
						mul_v3_fl(dx[2],dtime);
						VECCOPY(dv[2],force);
						mul_v3_fl(dv[2],dtime);

						VECADD(states[3].co,states->co,dx[2]);
						VECADD(states[3].vel,states->vel,dv[2]);
						fra=cfra;
						break;
					case 3:
						VECADD(dx[3],states->vel,dv[2]);
						mul_v3_fl(dx[3],dtime);
						VECCOPY(dv[3],force);
						mul_v3_fl(dv[3],dtime);

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
			case PART_INT_VERLET:   /* Verlet integration */
				VECADDFAC(pa->state.vel,pa->state.vel,force,dtime);
				VECADDFAC(pa->state.co,pa->state.co,pa->state.vel,dtime);

				VECSUB(pa->state.vel,pa->state.co,oldpos);
				mul_v3_fl(pa->state.vel,1.0f/dtime);
				break;
		}
	}

	/* damp affects final velocity */
	if(part->dampfac!=0.0)
		mul_v3_fl(pa->state.vel,1.0f-part->dampfac);

	VECCOPY(pa->state.ave, states->ave);

	/* finally we do guides */
	time=(cfra-pa->time)/pa->lifetime;
	CLAMP(time,0.0,1.0);

	VECCOPY(tkey.co,pa->state.co);
	VECCOPY(tkey.vel,pa->state.vel);
	tkey.time=pa->state.time;

	if(part->type != PART_HAIR) {
		if(do_guides(sim->psys->effectors, &tkey, p, time)) {
			VECCOPY(pa->state.co,tkey.co);
			/* guides don't produce valid velocity */
			VECSUB(pa->state.vel,tkey.co,pa->prev_state.co);
			mul_v3_fl(pa->state.vel,1.0f/dtime);
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
			float len1 = len_v3(pa->prev_state.vel);
			float len2 = len_v3(pa->state.vel);

			if(len1==0.0f || len2==0.0f)
				pa->state.ave[0]=pa->state.ave[1]=pa->state.ave[2]=0.0f;
			else{
				cross_v3_v3v3(pa->state.ave,pa->prev_state.vel,pa->state.vel);
				normalize_v3(pa->state.ave);
				angle=dot_v3v3(pa->prev_state.vel,pa->state.vel)/(len1*len2);
				mul_v3_fl(pa->state.ave,saacos(angle)/dtime);
			}

			axis_angle_to_quat(rot2,pa->state.vel,dtime*part->avefac);
		}
	}

	rotfac=len_v3(pa->state.ave);
	if(rotfac==0.0){ /* unit_qt(in VecRotToQuat) doesn't give unit quat [1,0,0,0]?? */
		rot1[0]=1.0;
		rot1[1]=rot1[2]=rot1[3]=0;
	}
	else{
		axis_angle_to_quat(rot1,pa->state.ave,rotfac*dtime);
	}
	mul_qt_qtqt(pa->state.rot,rot1,pa->prev_state.rot);
	mul_qt_qtqt(pa->state.rot,rot2,pa->state.rot);

	/* keep rotation quat in good health */
	normalize_qt(pa->state.rot);
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

	interp_weights_poly_v3( w,vert, 4, co);
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
			if(isect_aabb_aabb_v3(min,max,p_min,p_max)==0)
				continue;
		}
		else{
			VECCOPY(min, face_minmax+6*i);
			VECCOPY(max, face_minmax+6*i+3);
			if(isect_aabb_aabb_v3(min,max,p_min,p_max)==0)
				continue;
		}

		if(radius>0.0f){
			if(isect_sweeping_sphere_tri_v3(co1, co2, radius, v2, v3, v1, &cur_d, cur_ipoint)){
				if(cur_d<*min_d){
					*min_d=cur_d;
					VECCOPY(ipoint,cur_ipoint);
					*min_face=i;
					intersect=1;
				}
			}
			if(mface->v4){
				if(isect_sweeping_sphere_tri_v3(co1, co2, radius, v4, v1, v3, &cur_d, cur_ipoint)){
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
			if(isect_line_tri_v3(co1, co2, v1, v2, v3, &cur_d, cur_uv)){
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
				if(isect_line_tri_v3(co1, co2, v1, v3, v4, &cur_d, cur_uv)){
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
	mul_v3_fl(vel, 0.33334f);

	/* substract face velocity, in other words convert to 
	   a coordinate system where only the particle moves */
	VECADDFAC(co1, col->co1, vel, -col->t);
	VECSUB(co2, col->co2, vel);

	do
	{	
		if(ray->radius == 0.0f) {
			if(isect_line_tri_v3(co1, co2, t0, t1, t2, &t, uv)) {
				if(t >= 0.0f && t < hit->dist/col->ray_len) {
					hit->dist = col->ray_len * t;
					hit->index = index;

					/* calculate normal that's facing the particle */
					normal_tri_v3( col->nor,t0, t1, t2);
					VECSUB(temp, co2, co1);
					if(dot_v3v3(col->nor, temp) > 0.0f)
						negate_v3(col->nor);

					VECCOPY(col->vel,vel);

					col->hit_ob = col->ob;
					col->hit_md = col->md;
				}
			}
		}
		else {
			if(isect_sweeping_sphere_tri_v3(co1, co2, ray->radius, t0, t1, t2, &t, ipoint)) {
				if(t >=0.0f && t < hit->dist/col->ray_len) {
					hit->dist = col->ray_len * t;
					hit->index = index;

					interp_v3_v3v3(temp, co1, co2, t);
					
					VECSUB(col->nor, temp, ipoint);
					normalize_v3(col->nor);

					VECCOPY(col->vel,vel);

					col->hit_ob = col->ob;
					col->hit_md = col->md;
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
static void deflect_particle(ParticleSimulationData *sim, int p, float dfra, float cfra){
	Object *ground_ob = NULL;
	ParticleSettings *part = sim->psys->part;
	ParticleData *pa = sim->psys->particles + p;
	ParticleCollision col;
	ColliderCache *coll;
	BVHTreeRayHit hit;
	float ray_dir[3], zerovec[3]={0.0,0.0,0.0};
	float radius = ((part->flag & PART_SIZE_DEFL)?pa->size:0.0f), boid_z = 0.0f;
	float timestep = psys_get_timestep(sim);
	int deflections=0, max_deflections=10;

	VECCOPY(col.co1, pa->prev_state.co);
	VECCOPY(col.co2, pa->state.co);
	
	VECCOPY(col.ve1, pa->prev_state.vel);
	VECCOPY(col.ve2, pa->state.vel);
	mul_v3_fl(col.ve1, timestep * dfra);
	mul_v3_fl(col.ve2, timestep * dfra);
	
	col.t = 0.0f;

	/* override for boids */
	if(part->phystype == PART_PHYS_BOIDS) {
		BoidParticle *bpa = pa->boid;
		radius = pa->size;
		boid_z = pa->state.co[2];
		ground_ob = bpa->ground;
	}

	/* 10 iterations to catch multiple deflections */
	if(sim->colliders) while(deflections < max_deflections){
		/* 1. */

		VECSUB(ray_dir, col.co2, col.co1);
		hit.index = -1;
		hit.dist = col.ray_len = len_v3(ray_dir);

		/* even if particle is stationary we want to check for moving colliders */
		/* if hit.dist is zero the bvhtree_ray_cast will just ignore everything */
		if(hit.dist == 0.0f)
			hit.dist = col.ray_len = 0.000001f;

		for(coll = sim->colliders->first; coll; coll=coll->next){
			/* for boids: don't check with current ground object */
			if(coll->ob == ground_ob)
				continue;

			/* particles should not collide with emitter at birth */
			if(coll->ob == sim->ob && pa->time < cfra && pa->time >= sim->psys->cfra)
				continue;

			col.ob = coll->ob;
			col.md = coll->collmd;

			if(col.md && col.md->bvhtree)
				BLI_bvhtree_ray_cast(col.md->bvhtree, col.co1, ray_dir, radius, &hit, particle_intersect_face, &col);
		}

		/* 2. */
		if(hit.index>=0) {
			PartDeflect *pd = col.hit_ob->pd;
			int through = (BLI_frand() < pd->pdef_perm) ? 1 : 0;
			float co[3]; /* point of collision */
			float vec[3]; /* movement through collision */
			float acc[3]; /* acceleration */

			float x = hit.dist/col.ray_len; /* location of collision between this iteration */
			float le = len_v3(col.ve1)/col.ray_len;
			float ac = len_v3(col.ve2)/col.ray_len - le; /* (taking acceleration into account) */
			float t = (-le + sqrt(le*le + 2*ac*x))/ac; /* time of collision between this iteration */
			float dt = col.t + x * (1.0f - col.t); /* time of collision between frame change*/
			float it = 1.0 - t;
			
			interp_v3_v3v3(co, col.co1, col.co2, x);
			VECSUB(vec, col.co2, col.co1);

			VECSUB(acc, col.ve2, col.ve1);
			
			mul_v3_fl(col.vel, 1.0f-col.t);

			/* particle dies in collision */
			if(through == 0 && (part->flag & PART_DIE_ON_COL || pd->flag & PDEFLE_KILL_PART)) {
				pa->alive = PARS_DYING;
				pa->dietime = pa->state.time + (cfra - pa->state.time) * dt;
				
				/* we have to add this for dying particles too so that reactors work correctly */
				VECADDFAC(co, co, col.nor, (through ? -0.0001f : 0.0001f));

				VECCOPY(pa->state.co, co);
				interp_v3_v3v3(pa->state.vel, pa->prev_state.vel, pa->state.vel, dt);
				interp_qt_qtqt(pa->state.rot, pa->prev_state.rot, pa->state.rot, dt);
				interp_v3_v3v3(pa->state.ave, pa->prev_state.ave, pa->state.ave, dt);

				/* particle is dead so we don't need to calculate further */
				deflections=max_deflections;
			}
			else {
				float nor_vec[3], tan_vec[3], tan_vel[3];
				float damp, frict;
				float inp, inp_v;
				
				/* get damping & friction factors */
				damp = pd->pdef_damp + pd->pdef_rdamp * 2 * (BLI_frand() - 0.5f);
				CLAMP(damp,0.0,1.0);

				frict = pd->pdef_frict + pd->pdef_rfrict * 2 * (BLI_frand() - 0.5f);
				CLAMP(frict,0.0,1.0);

				/* treat normal & tangent components separately */
				inp = dot_v3v3(col.nor, vec);
				inp_v = dot_v3v3(col.nor, col.vel);

				VECADDFAC(tan_vec, vec, col.nor, -inp);
				VECADDFAC(tan_vel, col.vel, col.nor, -inp_v);
				if((part->flag & PART_ROT_DYN)==0)
					interp_v3_v3v3(tan_vec, tan_vec, tan_vel, frict);

				VECCOPY(nor_vec, col.nor);
				inp *= 1.0f - damp;

				if(through)
					inp_v *= damp;

				/* special case for object hitting the particle from behind */
				if(through==0 && ((inp_v>0 && inp>0 && inp_v>inp) || (inp_v<0 && inp<0 && inp_v<inp)))
					mul_v3_fl(nor_vec, inp_v);
				else
					mul_v3_fl(nor_vec, inp_v + (through ? 1.0f : -1.0f) * inp);

				/* angular <-> linear velocity - slightly more physical and looks even nicer than before */
				if(part->flag & PART_ROT_DYN) {
					float surface_vel[3], rot_vel[3], friction[3], dave[3], dvel[3];

					/* apparent velocity along collision surface */
					VECSUB(surface_vel, tan_vec, tan_vel);

					/* direction of rolling friction */
					cross_v3_v3v3(rot_vel, pa->state.ave, col.nor);
					/* convert to current dt */
					mul_v3_fl(rot_vel, (timestep*dfra) * (1.0f - col.t));
					mul_v3_fl(rot_vel, pa->size);

					/* apply sliding friction */
					VECSUB(surface_vel, surface_vel, rot_vel);
					VECCOPY(friction, surface_vel);

					mul_v3_fl(surface_vel, 1.0 - frict);
					mul_v3_fl(friction, frict);

					/* sliding changes angular velocity */
					cross_v3_v3v3(dave, col.nor, friction);
					mul_v3_fl(dave, 1.0f/MAX2(pa->size, 0.001));

					/* we assume rolling friction is around 0.01 of sliding friction */
					mul_v3_fl(rot_vel, 1.0 - frict*0.01);

					/* change in angular velocity has to be added to the linear velocity too */
					cross_v3_v3v3(dvel, dave, col.nor);
					mul_v3_fl(dvel, pa->size);
					VECADD(rot_vel, rot_vel, dvel);

					VECADD(surface_vel, surface_vel, rot_vel);
					VECADD(tan_vec, surface_vel, tan_vel);

					/* convert back to normal time */
					mul_v3_fl(dave, 1.0f/MAX2((timestep*dfra) * (1.0f - col.t), 0.00001));

					mul_v3_fl(pa->state.ave, 1.0 - frict*0.01);
					VECADD(pa->state.ave, pa->state.ave, dave);
				}

				/* combine components together again */
				VECADD(vec, nor_vec, tan_vec);

				/* make sure we don't hit the current face again */
				VECADDFAC(co, co, col.nor, (through ? -0.0001f : 0.0001f));

				if(part->phystype == PART_PHYS_BOIDS && part->boids->options & BOID_ALLOW_LAND) {
					BoidParticle *bpa = pa->boid;
					if(bpa->data.mode == eBoidMode_OnLand || co[2] <= boid_z) {
						co[2] = boid_z;
						vec[2] = 0.0f;
					}
				}

				/* set coordinates for next iteration */
				
				/* apply acceleration to final position, but make sure particle stays above surface */
				madd_v3_v3v3fl(acc, vec, acc, it);
				ac = dot_v3v3(acc, col.nor);
				if((!through && ac < 0.0f) || (through && ac > 0.0f))
					madd_v3_v3fl(acc, col.nor, -ac);

				VECCOPY(col.co1, co);
				VECADDFAC(col.co2, co, acc, it);

				VECCOPY(col.ve1, vec);
				VECCOPY(col.ve2, acc);

				if(len_v3(vec) < 0.001 && len_v3v3(pa->state.co, pa->prev_state.co) < 0.001) {
					/* kill speed to stop slipping */
					VECCOPY(pa->state.vel,zerovec);
					VECCOPY(pa->state.co, co);
					if(part->flag & PART_ROT_DYN) {
						VECCOPY(pa->state.ave,zerovec);
					}
				}
				else {
					VECCOPY(pa->state.co, col.co2);
					mul_v3_v3fl(pa->state.vel, acc, 1.0f/MAX2((timestep*dfra) * (1.0f - col.t), 0.00001));
					
					/* Stickness to surface */
					normalize_v3(nor_vec);
					madd_v3_v3fl(pa->state.vel, nor_vec, -pd->pdef_stickness);
				}

				col.t = dt;
			}
			deflections++;

			//reaction_state.time = cfra - (1.0f - dt) * dfra;
			//push_reaction(col.ob, psys, p, PART_EVENT_COLLIDE, &reaction_state);
		}
		else
			return;
	}
}
/************************************************/
/*			Hair								*/
/************************************************/
/* check if path cache or children need updating and do it if needed */
static void psys_update_path_cache(ParticleSimulationData *sim, float cfra)
{
	ParticleSystem *psys = sim->psys;
	ParticleSettings *part = psys->part;
	ParticleEditSettings *pset = &sim->scene->toolsettings->particle;
	int distr=0, alloc=0, skip=0;

	if((psys->part->childtype && psys->totchild != get_psys_tot_child(sim->scene, psys)) || psys->recalc&PSYS_RECALC_RESET)
		alloc=1;

	if(alloc || psys->recalc&PSYS_RECALC_CHILD || (psys->vgroup[PSYS_VG_DENSITY] && (sim->ob && sim->ob->mode & OB_MODE_WEIGHT_PAINT)))
		distr=1;

	if(distr){
		if(alloc)
			realloc_particles(sim, sim->psys->totpart);

		if(get_psys_tot_child(sim->scene, psys)) {
			/* don't generate children while computing the hair keys */
			if(!(psys->part->type == PART_HAIR) || (psys->flag & PSYS_HAIR_DONE)) {
				distribute_particles(sim, PART_FROM_CHILD);

				if(part->from!=PART_FROM_PARTICLE && part->childtype==PART_CHILD_FACES && part->parents!=0.0)
					psys_find_parents(sim);
			}
		}
		else
			psys_free_children(psys);
	}

	if((part->type==PART_HAIR || psys->flag&PSYS_KEYED || psys->pointcache->flag & PTCACHE_BAKED)==0)
		skip = 1; /* only hair, keyed and baked stuff can have paths */
	else if(part->ren_as != PART_DRAW_PATH && !(part->type==PART_HAIR && ELEM(part->ren_as, PART_DRAW_OB, PART_DRAW_GR)))
		skip = 1; /* particle visualization must be set as path */
	else if(!psys->renderdata) {
		if(part->draw_as != PART_DRAW_REND)
			skip = 1; /* draw visualization */
		else if(psys->pointcache->flag & PTCACHE_BAKING)
			skip = 1; /* no need to cache paths while baking dynamics */
		else if(psys_in_edit_mode(sim->scene, psys)) {
			if((pset->flag & PE_DRAW_PART)==0)
				skip = 1;
			else if(part->childtype==0 && (psys->flag & PSYS_HAIR_DYNAMICS && psys->pointcache->flag & PTCACHE_BAKED)==0)
				skip = 1; /* in edit mode paths are needed for child particles and dynamic hair */
		}
	}

	if(!skip) {
		psys_cache_paths(sim, cfra);

		/* for render, child particle paths are computed on the fly */
		if(part->childtype) {
			if(!psys->totchild)
				skip = 1;
			else if((psys->part->type == PART_HAIR && psys->flag & PSYS_HAIR_DONE)==0)
				skip = 1;

			if(!skip)
				psys_cache_child_paths(sim, cfra, 0);
		}
	}
	else if(psys->pathcache)
		psys_free_path_cache(psys, NULL);
}

static void do_hair_dynamics(ParticleSimulationData *sim)
{
	ParticleSystem *psys = sim->psys;
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

	totedge = totpoint;
	totpoint += psys->totpart;

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
	psys->particles->hair_index = 1;
	LOOP_PARTICLES {
		if(p)
			pa->hair_index = (pa-1)->hair_index + (pa-1)->totkey + 1;

		psys_mat_hair_to_object(sim->ob, sim->psmd->dm, psys->part->from, pa, hairmat);

		for(k=0, key=pa->hair; k<pa->totkey; k++,key++) {
			
			/* create fake root before actual root to resist bending */
			if(k==0) {
				float temp[3];
				VECSUB(temp, key->co, (key+1)->co);
				VECCOPY(mvert->co, key->co);
				VECADD(mvert->co, mvert->co, temp);
				mul_m4_v3(hairmat, mvert->co);
				mvert++;

				medge->v1 = pa->hair_index - 1;
				medge->v2 = pa->hair_index;
				medge++;

				if(dvert) {
					if(!dvert->totweight) {
						dvert->dw = MEM_callocN (sizeof(MDeformWeight), "deformWeight");
						dvert->totweight = 1;
					}

					dvert->dw->weight = 1.0f;
					dvert++;
				}
			}

			VECCOPY(mvert->co, key->co);
			mul_m4_v3(hairmat, mvert->co);
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
				/* roots should be 1.0, the rest can be anything from 0.0 to 1.0 */
				dvert->dw->weight = key->weight;
				dvert++;
			}
		}
	}

	if(psys->hair_out_dm)
		psys->hair_out_dm->release(psys->hair_out_dm);

	psys->clmd->point_cache = psys->pointcache;
	psys->clmd->sim_parms->effector_weights = psys->part->effector_weights;

	psys->hair_out_dm = clothModifier_do(psys->clmd, sim->scene, sim->ob, dm, 0, 0);

	psys->clmd->sim_parms->effector_weights = NULL;
}
static void hair_step(ParticleSimulationData *sim, float cfra)
{
	ParticleSystem *psys = sim->psys;
/*	ParticleSettings *part = psys->part; */
	PARTICLE_P;
	float disp = (float)get_current_display_percentage(psys)/100.0f;

	BLI_srandom(psys->seed);

	LOOP_PARTICLES {
		if(PSYS_FRAND(p) > disp)
			pa->flag |= PARS_NO_DISP;
		else
			pa->flag &= ~PARS_NO_DISP;
	}

	if(psys->recalc & PSYS_RECALC_RESET) {
		/* need this for changing subsurf levels */
		psys_calc_dmcache(sim->ob, sim->psmd->dm, psys);

		if(psys->clmd)
			cloth_free_modifier(sim->ob, psys->clmd);
	}

	/* dynamics with cloth simulation */
	if(psys->part->type==PART_HAIR && psys->flag & PSYS_HAIR_DYNAMICS)
		do_hair_dynamics(sim);

	psys_update_effectors(sim);

	psys_update_path_cache(sim, cfra);

	psys->flag |= PSYS_HAIR_UPDATED;
}

static void save_hair(ParticleSimulationData *sim, float cfra){
	Object *ob = sim->ob;
	ParticleSystem *psys = sim->psys;
	HairKey *key, *root;
	PARTICLE_P;
	int totpart;

	invert_m4_m4(ob->imat, ob->obmat);
	
	psys->lattice= psys_get_lattice(sim);

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
		copy_v3_v3(key->co, pa->state.co);
		mul_m4_v3(ob->imat, key->co);

		if(pa->totkey) {
			VECSUB(key->co, key->co, root->co);
			psys_vec_rot_to_face(sim->psmd->dm, pa, key->co);
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
static void dynamics_step(ParticleSimulationData *sim, float cfra)
{
	ParticleSystem *psys = sim->psys;
	ParticleSettings *part=psys->part;
	BoidBrainData bbd;
	PARTICLE_P;
	float timestep;
	/* current time */
	float ctime;
	/* frame & time changes */
	float dfra, dtime, pa_dtime, pa_dfra=0.0;
	float birthtime, dietime;
	
	/* where have we gone in time since last time */
	dfra= cfra - psys->cfra;

	timestep = psys_get_timestep(sim);
	dtime= dfra*timestep;
	ctime= cfra*timestep;

	if(dfra<0.0){
		LOOP_EXISTING_PARTICLES {
			pa->size = part->size;
			if(part->randsize > 0.0)
				pa->size *= 1.0f - part->randsize * PSYS_FRAND(p + 1);

			reset_particle(sim, pa, dtime, cfra);
		}
		return;
	}

	BLI_srandom(31415926 + (int)cfra + psys->seed);

	psys_update_effectors(sim);

	if(part->type != PART_HAIR)
		sim->colliders = get_collider_cache(sim->scene, NULL, NULL);

	if(part->phystype==PART_PHYS_BOIDS){
		ParticleTarget *pt = psys->targets.first;
		bbd.sim = sim;
		bbd.part = part;
		bbd.cfra = cfra;
		bbd.dfra = dfra;
		bbd.timestep = timestep;

		psys_update_particle_tree(psys, cfra);

		boids_precalc_rules(part, cfra);

		for(; pt; pt=pt->next) {
			if(pt->ob)
				psys_update_particle_tree(BLI_findlink(&pt->ob->particlesystem, pt->psys-1), cfra);
		}
	}
	else if(part->phystype==PART_PHYS_FLUID){
		ParticleTarget *pt = psys->targets.first;
		psys_update_particle_tree(psys, cfra);
		
		for(; pt; pt=pt->next) {  /* Updating others systems particle tree for fluid-fluid interaction */
			if(pt->ob) psys_update_particle_tree(BLI_findlink(&pt->ob->particlesystem, pt->psys-1), cfra);
		}
	}

	/* main loop: calculate physics for all particles */
	LOOP_SHOWN_PARTICLES {
		copy_particle_key(&pa->prev_state,&pa->state,1);

		pa->size = part->size;
		if(part->randsize > 0.0)
			pa->size *= 1.0f - part->randsize * PSYS_FRAND(p + 1);

		///* reactions can change birth time so they need to be checked first */
		//if(psys->reactevents.first && ELEM(pa->alive,PARS_DEAD,PARS_KILLED)==0)
		//	react_to_events(psys,p);

		birthtime = pa->time;
		dietime = birthtime + pa->lifetime;

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
			reset_particle(sim, pa, dtime, cfra);
			pa->alive = PARS_ALIVE;
			pa_dfra = cfra - birthtime;
			pa_dtime = pa_dfra*timestep;
		}
		else if(dietime < cfra){
			/* nothing to be done when particle is dead */
		}

		/* only reset unborn particles if they're shown or if the particle is born soon*/
		if(pa->alive==PARS_UNBORN
			&& (part->flag & PART_UNBORN || cfra + psys->pointcache->step > pa->time))
			reset_particle(sim, pa, dtime, cfra);
		else if(part->phystype == PART_PHYS_NO)
			reset_particle(sim, pa, dtime, cfra);

		if(dfra>0.0 && ELEM(pa->alive,PARS_ALIVE,PARS_DYING)){
			switch(part->phystype){
				case PART_PHYS_NEWTON:
					/* do global forces & effectors */
					apply_particle_forces(sim, p, pa_dfra, cfra);
		
					/* deflection */
					if(sim->colliders)
						deflect_particle(sim, p, pa_dfra, cfra);

					/* rotations */
					rotate_particle(part, pa, pa_dfra, timestep);
					break;
				case PART_PHYS_BOIDS:
				{
					bbd.goal_ob = NULL;
					boid_brain(&bbd, p, pa);
					if(pa->alive != PARS_DYING) {
						boid_body(&bbd, pa);

						/* deflection */
						if(sim->colliders)
							deflect_particle(sim, p, pa_dfra, cfra);
					}
					break;
				}
				case PART_PHYS_FLUID:
				{	
					/* do global forces & effectors */
					apply_particle_forces(sim, p, pa_dfra, cfra);

					/* do fluid sim */
					apply_particle_fluidsim(psys, pa, part, sim, pa_dfra, cfra);

					/* deflection */
 					if(sim->colliders)
						deflect_particle(sim, p, pa_dfra, cfra);
 					
					/* rotations, SPH particles are not physical particles, just interpolation particles,  thus rotation has not a direct sense for them */	
					rotate_particle(part, pa, pa_dfra, timestep);  
 					break;
				} 
			}

			if(pa->alive == PARS_DYING){
				//push_reaction(ob,psys,p,PART_EVENT_DEATH,&pa->state);

				pa->alive=PARS_DEAD;
				pa->state.time=pa->dietime;
			}
			else
				pa->state.time=cfra;

			//push_reaction(ob,psys,p,PART_EVENT_NEAR,&pa->state);
		}
	}

	free_collider_cache(&sim->colliders);
}
static void update_children(ParticleSimulationData *sim)
{
	if((sim->psys->part->type == PART_HAIR) && (sim->psys->flag & PSYS_HAIR_DONE)==0)
	/* don't generate children while growing hair - waste of time */
		psys_free_children(sim->psys);
	else if(sim->psys->part->childtype && sim->psys->totchild != get_psys_tot_child(sim->scene, sim->psys))
		distribute_particles(sim, PART_FROM_CHILD);
	else
		psys_free_children(sim->psys);
}
/* updates cached particles' alive & other flags etc..*/
static void cached_step(ParticleSimulationData *sim, float cfra)
{
	ParticleSystem *psys = sim->psys;
	ParticleSettings *part = psys->part;
	PARTICLE_P;
	float disp, birthtime, dietime;

	BLI_srandom(psys->seed);

	psys_update_effectors(sim);
	
	disp= (float)get_current_display_percentage(psys)/100.0f;

	LOOP_PARTICLES {
		pa->size = part->size;
		if(part->randsize > 0.0)
			pa->size *= 1.0f - part->randsize * PSYS_FRAND(p + 1);

		psys->lattice= psys_get_lattice(sim);

		birthtime = pa->time;
		dietime = pa->dietime;

		/* update alive status and push events */
		if(pa->time > cfra) {
			pa->alive = PARS_UNBORN;
			if(part->flag & PART_UNBORN && (psys->pointcache->flag & PTCACHE_EXTERNAL) == 0)
				reset_particle(sim, pa, 0.0f, cfra);
		}
		else if(dietime <= cfra)
			pa->alive = PARS_DEAD;
		else
			pa->alive = PARS_ALIVE;

		if(psys->lattice){
			end_latt_deform(psys->lattice);
			psys->lattice= NULL;
		}

		if(PSYS_FRAND(p) > disp)
			pa->flag |= PARS_NO_DISP;
		else
			pa->flag &= ~PARS_NO_DISP;
	}
}

static void particles_fluid_step(ParticleSimulationData *sim, int cfra)
{	
	ParticleSystem *psys = sim->psys;
	if(psys->particles){
		MEM_freeN(psys->particles);
		psys->particles = 0;
		psys->totpart = 0;
	}

	/* fluid sim particle import handling, actual loading of particles from file */
	#ifndef DISABLE_ELBEEM
	{
		FluidsimModifierData *fluidmd = (FluidsimModifierData *)modifiers_findByType(sim->ob, eModifierType_Fluidsim);
		
		if( fluidmd && fluidmd->fss) { 
			FluidsimSettings *fss= fluidmd->fss;
			ParticleSettings *part = psys->part;
			ParticleData *pa=0;
			char *suffix  = "fluidsurface_particles_####";
			char *suffix2 = ".gz";
			char filename[256];
			char debugStrBuffer[256];
			int  curFrame = sim->scene->r.cfra -1; // warning - sync with derived mesh fsmesh loading
			int  p, j, numFileParts, totpart;
			int readMask, activeParts = 0, fileParts = 0;
			gzFile gzf;
	
// XXX			if(ob==G.obedit) // off...
//				return;
	
			// ok, start loading
			strcpy(filename, fss->surfdataPath);
			strcat(filename, suffix);
			BLI_path_abs(filename, G.sce);
			BLI_path_frame(filename, curFrame, 0); // fixed #frame-no 
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
			part->lifetime = sim->scene->r.efra + 1;
	
			/* initialize particles */
			realloc_particles(sim, part->totpart);
			initialize_all_particles(sim);
	
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

static int emit_particles(ParticleSimulationData *sim, PTCacheID *pid, float cfra)
{
	ParticleSystem *psys = sim->psys;
	ParticleSettings *part = psys->part;
	int oldtotpart = psys->totpart;
	int totpart = oldtotpart;

	if(pid && psys->pointcache->flag & PTCACHE_EXTERNAL)
		totpart = pid->cache->totpoint;
	else if(part->distr == PART_DISTR_GRID && part->from != PART_FROM_VERT)
		totpart = part->grid_res*part->grid_res*part->grid_res;
	else
		totpart = psys->part->totpart;

	if(totpart != oldtotpart)
		realloc_particles(sim, totpart);

	return totpart - oldtotpart;
}
/* Calculates the next state for all particles of the system
 * In particles code most fra-ending are frames, time-ending are fra*timestep (seconds)
 * 1. Emit particles
 * 2. Check cache (if used) and return if frame is cached
 * 3. Do dynamics
 * 4. Save to cache */
static void system_step(ParticleSimulationData *sim, float cfra)
{
	ParticleSystem *psys = sim->psys;
	ParticleSettings *part = psys->part;
	PointCache *cache = psys->pointcache;
	PTCacheID pid, *use_cache = NULL;
	PARTICLE_P;
	int oldtotpart;
	float disp, *vg_vel= 0, *vg_tan= 0, *vg_rot= 0, *vg_size= 0;
	int init= 0, emit= 0, only_children_changed= 0;
	int framenr, framedelta, startframe = 0, endframe = 100;

	framenr= (int)sim->scene->r.cfra;
	framedelta= framenr - cache->simframe;

	/* cache shouldn't be used for hair or "continue physics" */
	if(part->type != PART_HAIR && BKE_ptcache_get_continue_physics() == 0) {
		BKE_ptcache_id_from_particles(&pid, sim->ob, psys);
		use_cache = &pid;
	}

	if(use_cache) {
		psys_clear_temp_pointcache(sim->psys);

		/* set suitable cache range automatically */
		if((cache->flag & (PTCACHE_BAKING|PTCACHE_BAKED))==0)
			psys_get_pointcache_start_end(sim->scene, sim->psys, &cache->startframe, &cache->endframe);
		
		BKE_ptcache_id_time(&pid, sim->scene, 0.0f, &startframe, &endframe, NULL);

		/* simulation is only active during a specific period */
		if(framenr < startframe) {
			psys_reset(psys, PSYS_RESET_CACHE_MISS);
			return;
		}
		else if(framenr > endframe) {
			framenr= endframe;
		}
		
		if(framenr == startframe) {
			BKE_ptcache_id_reset(sim->scene, use_cache, PTCACHE_RESET_OUTDATED);
			BKE_ptcache_validate(cache, framenr);
			cache->flag &= ~PTCACHE_REDO_NEEDED;
		}
	}

/* 1. emit particles */

	/* verify if we need to reallocate */
	oldtotpart = psys->totpart;

	emit = emit_particles(sim, use_cache, cfra);
	if(emit > 0)
		BKE_ptcache_id_clear(&pid, PTCACHE_CLEAR_ALL, cfra);
	init = emit*emit + (psys->recalc & PSYS_RECALC_RESET);

	if(init) {
		distribute_particles(sim, part->from);
		initialize_all_particles(sim);
		reset_all_particles(sim, 0.0, cfra, oldtotpart);

		/* flag for possible explode modifiers after this system */
		sim->psmd->flag |= eParticleSystemFlag_Pars;
	}

/* 2. try to read from the cache */
	if(use_cache) {
		int cache_result = BKE_ptcache_read_cache(use_cache, cfra, sim->scene->r.frs_sec);

		if(ELEM(cache_result, PTCACHE_READ_EXACT, PTCACHE_READ_INTERPOLATED)) {
			cached_step(sim, cfra);
			update_children(sim);
			psys_update_path_cache(sim, cfra);

			BKE_ptcache_validate(cache, framenr);

			if(cache_result == PTCACHE_READ_INTERPOLATED && cache->flag & PTCACHE_REDO_NEEDED)
				BKE_ptcache_write_cache(use_cache, framenr);

			return;
		}
		else if(cache_result == PTCACHE_READ_OLD) {
			psys->cfra = (float)cache->simframe;
			cached_step(sim, psys->cfra);
		}
		else if(cfra != startframe && ( /*sim->ob->id.lib ||*/ (cache->flag & PTCACHE_BAKED))) { /* 2.4x disabled lib, but this can be used in some cases, testing further - campbell */
			psys_reset(psys, PSYS_RESET_CACHE_MISS);
			return;
		}

		/* if on second frame, write cache for first frame */
		if(psys->cfra == startframe && (cache->flag & PTCACHE_OUTDATED || cache->last_exact==0))
			BKE_ptcache_write_cache(use_cache, startframe);
	}
	else
		BKE_ptcache_invalidate(cache);

/* 3. do dynamics */
	/* set particles to be not calculated TODO: can't work with pointcache */
	disp= (float)get_current_display_percentage(psys)/100.0f;

	BLI_srandom(psys->seed);
	LOOP_PARTICLES {
		if(PSYS_FRAND(p) > disp)
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
			dynamics_step(sim, cfra+dframe);
			psys->cfra = cfra+dframe;
		}
	}
	
/* 4. only write cache starting from second frame */
	if(use_cache) {
		BKE_ptcache_validate(cache, framenr);
		if(framenr != startframe)
			BKE_ptcache_write_cache(use_cache, framenr);
	}

	if(init)
		update_children(sim);

/* cleanup */
	if(psys->lattice){
		end_latt_deform(psys->lattice);
		psys->lattice= NULL;
	}
}

/* system type has changed so set sensible defaults and clear non applicable flags */
static void psys_changed_type(ParticleSimulationData *sim)
{
	ParticleSettings *part = sim->psys->part;
	PTCacheID pid;

	BKE_ptcache_id_from_particles(&pid, sim->ob, sim->psys);

	if(part->from == PART_FROM_PARTICLE) {
		//if(part->type != PART_REACTOR)
		part->from = PART_FROM_FACE;
		if(part->distr == PART_DISTR_GRID && part->from != PART_FROM_VERT)
			part->distr = PART_DISTR_JIT;
	}

	if(part->phystype != PART_PHYS_KEYED)
		sim->psys->flag &= ~PSYS_KEYED;

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
		free_hair(sim->ob, sim->psys, 1);

		CLAMP(part->path_start, 0.0f, MAX2(100.0f, part->end + part->lifetime));
		CLAMP(part->path_end, 0.0f, MAX2(100.0f, part->end + part->lifetime));
	}

	psys_reset(sim->psys, PSYS_RESET_ALL);
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

static void fluid_default_settings(ParticleSettings *part){
	SPHFluidSettings *fluid = part->fluid;

	fluid->radius = 0.5f;
	fluid->spring_k = 0.f;
	fluid->rest_length = 0.5f;
	fluid->viscosity_omega = 2.f;
	fluid->viscosity_beta = 0.f;
	fluid->stiffness_k = 0.1f;
	fluid->stiffness_knear = 0.05f;
	fluid->rest_density = 10.f;
	fluid->buoyancy = 0.f;
}

static void psys_changed_physics(ParticleSimulationData *sim)
{
	ParticleSettings *part = sim->psys->part;

	if(ELEM(part->phystype, PART_PHYS_NO, PART_PHYS_KEYED)) {
		PTCacheID pid;
		BKE_ptcache_id_from_particles(&pid, sim->ob, sim->psys);
		BKE_ptcache_id_clear(&pid, PTCACHE_CLEAR_ALL, 0);
	}
	else {
		free_keyed_keys(sim->psys);
		sim->psys->flag &= ~PSYS_KEYED;
	}

	if(part->phystype == PART_PHYS_BOIDS && part->boids == NULL) {
		BoidState *state;

		part->boids = MEM_callocN(sizeof(BoidSettings), "Boid Settings");
		boid_default_settings(part->boids);

		state = boid_new_state(part->boids);
		BLI_addtail(&state->rules, boid_new_rule(eBoidRuleType_Separate));
		BLI_addtail(&state->rules, boid_new_rule(eBoidRuleType_Flock));

		((BoidRule*)state->rules.first)->flag |= BOIDRULE_CURRENT;

		state->flag |= BOIDSTATE_CURRENT;
		BLI_addtail(&part->boids->states, state);
	}
	else if(part->phystype == PART_PHYS_FLUID && part->fluid == NULL) {
		part->fluid = MEM_callocN(sizeof(SPHFluidSettings), "SPH Fluid Settings");
		fluid_default_settings(part);
	}

	psys_check_boid_data(sim->psys);
}
static int hair_needs_recalc(ParticleSystem *psys)
{
	if(!(psys->flag & PSYS_EDITED) && (!psys->edit || !psys->edit->edited) &&
		((psys->flag & PSYS_HAIR_DONE)==0 || psys->recalc & PSYS_RECALC_RESET)) {
		return 1;
	}

	return 0;
}

/* main particle update call, checks that things are ok on the large scale and
 * then advances in to actual particle calculations depending on particle type */
void particle_system_update(Scene *scene, Object *ob, ParticleSystem *psys)
{
	ParticleSimulationData sim = {scene, ob, psys, NULL, NULL};
	ParticleSettings *part = psys->part;
	float cfra;

	/* drawdata is outdated after ANY change */
	if(psys->pdd) psys->pdd->flag &= ~PARTICLE_DRAW_DATA_UPDATED;

	if(!psys_check_enabled(ob, psys))
		return;

	cfra= bsystem_time(scene, ob, (float)scene->r.cfra, 0.0f);
	sim.psmd= psys_get_modifier(ob, psys);

	/* system was already updated from modifier stack */
	if(sim.psmd->flag & eParticleSystemFlag_psys_updated) {
		sim.psmd->flag &= ~eParticleSystemFlag_psys_updated;
		/* make sure it really was updated to cfra */
		if(psys->cfra == cfra)
			return;
	}

	if(!sim.psmd->dm)
		return;

	/* execute drivers only, as animation has already been done */
	BKE_animsys_evaluate_animdata(&part->id, part->adt, cfra, ADT_RECALC_DRIVERS);

	if(psys->recalc & PSYS_RECALC_TYPE)
		psys_changed_type(&sim);
	else if(psys->recalc & PSYS_RECALC_PHYS)
		psys_changed_physics(&sim);

	switch(part->type) {
		case PART_HAIR:
		{
			/* (re-)create hair */
			if(hair_needs_recalc(psys)) {
				float hcfra=0.0f;
				int i, recalc = psys->recalc;

				free_hair(ob, psys, 0);

				/* first step is negative so particles get killed and reset */
				psys->cfra= 1.0f;

				for(i=0; i<=part->hair_step; i++){
					hcfra=100.0f*(float)i/(float)psys->part->hair_step;
					BKE_animsys_evaluate_animdata(&part->id, part->adt, hcfra, ADT_RECALC_ANIM);
					system_step(&sim, hcfra);
					psys->cfra = hcfra;
					psys->recalc = 0;
					save_hair(&sim, hcfra);
				}

				psys->flag |= PSYS_HAIR_DONE;
				psys->recalc = recalc;
			}

			if(psys->flag & PSYS_HAIR_DONE)
				hair_step(&sim, cfra);
			break;
		}
		case PART_FLUID:
		{
			particles_fluid_step(&sim, (int)cfra);
			break;
		}
		default:
		{
			switch(part->phystype) {
				case PART_PHYS_NO:
				case PART_PHYS_KEYED:
				{
					if(emit_particles(&sim, NULL, cfra)) {
						free_keyed_keys(psys);
						distribute_particles(&sim, part->from);
						initialize_all_particles(&sim);
					}
					reset_all_particles(&sim, 0.0, cfra, 0);

					if(part->phystype == PART_PHYS_KEYED) {
						psys_count_keyed_targets(&sim);
						set_keyed_keys(&sim);
						psys_update_path_cache(&sim,(int)cfra);
					}
					break;
				}
				default:
				{
					/* the main dynamic particle system step */
					system_step(&sim, cfra);
					break;
				}
			}
			break;
		}
	}

	psys->cfra = cfra;
	psys->recalc = 0;

	/* save matrix for duplicators */
	invert_m4_m4(psys->imat, ob->obmat);
}

