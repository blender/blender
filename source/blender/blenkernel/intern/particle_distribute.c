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
 * Contributor(s): Raul Fernandez Hernandez (Farsthary),
 *                 Stephen Swhitehorn,
 *                 Lukas Toenne
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/blenkernel/intern/particle_distribute.c
 *  \ingroup bke
 */

#include <string.h>

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"
#include "BLI_jitter.h"
#include "BLI_kdtree.h"
#include "BLI_math.h"
#include "BLI_rand.h"
#include "BLI_sort.h"
#include "BLI_task.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_particle_types.h"
#include "DNA_scene_types.h"

#include "BKE_cdderivedmesh.h"
#include "BKE_DerivedMesh.h"
#include "BKE_global.h"
#include "BKE_mesh.h"
#include "BKE_object.h"
#include "BKE_particle.h"

static int psys_render_simplify_distribution(ParticleThreadContext *ctx, int tot);

static void alloc_child_particles(ParticleSystem *psys, int tot)
{
	if (psys->child) {
		/* only re-allocate if we have to */
		if (psys->part->childtype && psys->totchild == tot) {
			memset(psys->child, 0, tot*sizeof(ChildParticle));
			return;
		}

		MEM_freeN(psys->child);
		psys->child=NULL;
		psys->totchild=0;
	}

	if (psys->part->childtype) {
		psys->totchild= tot;
		if (psys->totchild)
			psys->child= MEM_callocN(psys->totchild*sizeof(ChildParticle), "child_particles");
	}
}

static void distribute_simple_children(Scene *scene, Object *ob, DerivedMesh *finaldm, ParticleSystem *psys)
{
	ChildParticle *cpa = NULL;
	int i, p;
	int child_nbr= psys_get_child_number(scene, psys);
	int totpart= psys_get_tot_child(scene, psys);

	alloc_child_particles(psys, totpart);

	cpa = psys->child;
	for (i=0; i<child_nbr; i++) {
		for (p=0; p<psys->totpart; p++,cpa++) {
			float length=2.0;
			cpa->parent=p;
					
			/* create even spherical distribution inside unit sphere */
			while (length>=1.0f) {
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
}
static void distribute_grid(DerivedMesh *dm, ParticleSystem *psys)
{
	ParticleData *pa=NULL;
	float min[3], max[3], delta[3], d;
	MVert *mv, *mvert = dm->getVertDataArray(dm,0);
	int totvert=dm->getNumVerts(dm), from=psys->part->from;
	int i, j, k, p, res=psys->part->grid_res, size[3], axis;

	/* find bounding box of dm */
	if (totvert > 0) {
		mv=mvert;
		copy_v3_v3(min, mv->co);
		copy_v3_v3(max, mv->co);
		mv++;
		for (i = 1; i < totvert; i++, mv++) {
			minmax_v3v3_v3(min, max, mv->co);
		}
	}
	else {
		zero_v3(min);
		zero_v3(max);
	}

	sub_v3_v3v3(delta, max, min);

	/* determine major axis */
	axis = axis_dominant_v3_single(delta);
	 
	d = delta[axis]/(float)res;

	size[axis] = res;
	size[(axis+1)%3] = (int)ceil(delta[(axis+1)%3]/d);
	size[(axis+2)%3] = (int)ceil(delta[(axis+2)%3]/d);

	/* float errors grrr.. */
	size[(axis+1)%3] = MIN2(size[(axis+1)%3],res);
	size[(axis+2)%3] = MIN2(size[(axis+2)%3],res);

	size[0] = MAX2(size[0], 1);
	size[1] = MAX2(size[1], 1);
	size[2] = MAX2(size[2], 1);

	/* no full offset for flat/thin objects */
	min[0]+= d < delta[0] ? d/2.f : delta[0]/2.f;
	min[1]+= d < delta[1] ? d/2.f : delta[1]/2.f;
	min[2]+= d < delta[2] ? d/2.f : delta[2]/2.f;

	for (i=0,p=0,pa=psys->particles; i<res; i++) {
		for (j=0; j<res; j++) {
			for (k=0; k<res; k++,p++,pa++) {
				pa->fuv[0] = min[0] + (float)i*d;
				pa->fuv[1] = min[1] + (float)j*d;
				pa->fuv[2] = min[2] + (float)k*d;
				pa->flag |= PARS_UNEXIST;
				pa->hair_index = 0; /* abused in volume calculation */
			}
		}
	}

	/* enable particles near verts/edges/faces/inside surface */
	if (from==PART_FROM_VERT) {
		float vec[3];

		pa=psys->particles;

		min[0] -= d/2.0f;
		min[1] -= d/2.0f;
		min[2] -= d/2.0f;

		for (i=0,mv=mvert; i<totvert; i++,mv++) {
			sub_v3_v3v3(vec,mv->co,min);
			vec[0]/=delta[0];
			vec[1]/=delta[1];
			vec[2]/=delta[2];
			pa[((int)(vec[0] * (size[0] - 1))  * res +
			    (int)(vec[1] * (size[1] - 1))) * res +
			    (int)(vec[2] * (size[2] - 1))].flag &= ~PARS_UNEXIST;
		}
	}
	else if (ELEM(from,PART_FROM_FACE,PART_FROM_VOLUME)) {
		float co1[3], co2[3];

		MFace *mface= NULL, *mface_array;
		float v1[3], v2[3], v3[3], v4[4], lambda;
		int a, a1, a2, a0mul, a1mul, a2mul, totface;
		int amax= from==PART_FROM_FACE ? 3 : 1;

		totface=dm->getNumTessFaces(dm);
		mface=mface_array=dm->getTessFaceDataArray(dm,CD_MFACE);
		
		for (a=0; a<amax; a++) {
			if (a==0) { a0mul=res*res; a1mul=res; a2mul=1; }
			else if (a==1) { a0mul=res; a1mul=1; a2mul=res*res; }
			else { a0mul=1; a1mul=res*res; a2mul=res; }

			for (a1=0; a1<size[(a+1)%3]; a1++) {
				for (a2=0; a2<size[(a+2)%3]; a2++) {
					mface= mface_array;

					pa = psys->particles + a1*a1mul + a2*a2mul;
					copy_v3_v3(co1, pa->fuv);
					co1[a] -= d < delta[a] ? d/2.f : delta[a]/2.f;
					copy_v3_v3(co2, co1);
					co2[a] += delta[a] + 0.001f*d;
					co1[a] -= 0.001f*d;
					
					/* lets intersect the faces */
					for (i=0; i<totface; i++,mface++) {
						copy_v3_v3(v1, mvert[mface->v1].co);
						copy_v3_v3(v2, mvert[mface->v2].co);
						copy_v3_v3(v3, mvert[mface->v3].co);

						if (isect_axial_line_tri_v3(a, co1, co2, v2, v3, v1, &lambda)) {
							if (from==PART_FROM_FACE)
								(pa+(int)(lambda*size[a])*a0mul)->flag &= ~PARS_UNEXIST;
							else /* store number of intersections */
								(pa+(int)(lambda*size[a])*a0mul)->hair_index++;
						}
						else if (mface->v4) {
							copy_v3_v3(v4, mvert[mface->v4].co);

							if (isect_axial_line_tri_v3(a, co1, co2, v4, v1, v3, &lambda)) {
								if (from==PART_FROM_FACE)
									(pa+(int)(lambda*size[a])*a0mul)->flag &= ~PARS_UNEXIST;
								else
									(pa+(int)(lambda*size[a])*a0mul)->hair_index++;
							}
						}
					}

					if (from==PART_FROM_VOLUME) {
						int in=pa->hair_index%2;
						if (in) pa->hair_index++;
						for (i=0; i<size[0]; i++) {
							if (in || (pa+i*a0mul)->hair_index%2)
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

	if (psys->part->flag & PART_GRID_HEXAGONAL) {
		for (i=0,p=0,pa=psys->particles; i<res; i++) {
			for (j=0; j<res; j++) {
				for (k=0; k<res; k++,p++,pa++) {
					if (j%2)
						pa->fuv[0] += d/2.f;

					if (k%2) {
						pa->fuv[0] += d/2.f;
						pa->fuv[1] += d/2.f;
					}
				}
			}
		}
	}

	if (psys->part->flag & PART_GRID_INVERT) {
		for (i=0; i<size[0]; i++) {
			for (j=0; j<size[1]; j++) {
				pa=psys->particles + res*(i*res + j);
				for (k=0; k<size[2]; k++, pa++) {
					pa->flag ^= PARS_UNEXIST;
				}
			}
		}
	}

	if (psys->part->grid_rand > 0.f) {
		float rfac = d * psys->part->grid_rand;
		for (p=0,pa=psys->particles; p<psys->totpart; p++,pa++) {
			if (pa->flag & PARS_UNEXIST)
				continue;

			pa->fuv[0] += rfac * (psys_frand(psys, p + 31) - 0.5f);
			pa->fuv[1] += rfac * (psys_frand(psys, p + 32) - 0.5f);
			pa->fuv[2] += rfac * (psys_frand(psys, p + 33) - 0.5f);
		}
	}
}

/* modified copy from rayshade.c */
static void hammersley_create(float *out, int n, int seed, float amount)
{
	RNG *rng;
	double p, t, offs[2];
	int k, kk;

	rng = BLI_rng_new(31415926 + n + seed);
	offs[0] = BLI_rng_get_double(rng) + (double)amount;
	offs[1] = BLI_rng_get_double(rng) + (double)amount;
	BLI_rng_free(rng);

	for (k = 0; k < n; k++) {
		t = 0;
		for (p = 0.5, kk = k; kk; p *= 0.5, kk >>= 1)
			if (kk & 1) /* kk mod 2 = 1 */
				t += p;

		out[2*k + 0] = fmod((double)k/(double)n + offs[0], 1.0);
		out[2*k + 1] = fmod(t + offs[1], 1.0);
	}
}

/* almost exact copy of BLI_jitter_init */
static void init_mv_jit(float *jit, int num, int seed2, float amount)
{
	RNG *rng;
	float *jit2, x, rad1, rad2, rad3;
	int i, num2;

	if (num==0) return;

	rad1= (float)(1.0f/sqrtf((float)num));
	rad2= (float)(1.0f/((float)num));
	rad3= (float)sqrtf((float)num)/((float)num);

	rng = BLI_rng_new(31415926 + num + seed2);
	x= 0;
		num2 = 2 * num;
	for (i=0; i<num2; i+=2) {
	
		jit[i] = x + amount*rad1*(0.5f - BLI_rng_get_float(rng));
		jit[i+1] = i/(2.0f*num) + amount*rad1*(0.5f - BLI_rng_get_float(rng));
		
		jit[i]-= (float)floor(jit[i]);
		jit[i+1]-= (float)floor(jit[i+1]);
		
		x+= rad3;
		x -= (float)floor(x);
	}

	jit2= MEM_mallocN(12 + 2*sizeof(float)*num, "initjit");

	for (i=0 ; i<4 ; i++) {
		BLI_jitterate1((float (*)[2])jit, (float (*)[2])jit2, num, rad1);
		BLI_jitterate1((float (*)[2])jit, (float (*)[2])jit2, num, rad1);
		BLI_jitterate2((float (*)[2])jit, (float (*)[2])jit2, num, rad2);
	}
	MEM_freeN(jit2);
	BLI_rng_free(rng);
}

static void psys_uv_to_w(float u, float v, int quad, float *w)
{
	float vert[4][3], co[3];

	if (!quad) {
		if (u+v > 1.0f)
			v= 1.0f-v;
		else
			u= 1.0f-u;
	}

	vert[0][0] = 0.0f; vert[0][1] = 0.0f; vert[0][2] = 0.0f;
	vert[1][0] = 1.0f; vert[1][1] = 0.0f; vert[1][2] = 0.0f;
	vert[2][0] = 1.0f; vert[2][1] = 1.0f; vert[2][2] = 0.0f;

	co[0] = u;
	co[1] = v;
	co[2] = 0.0f;

	if (quad) {
		vert[3][0] = 0.0f; vert[3][1] = 1.0f; vert[3][2] = 0.0f;
		interp_weights_poly_v3( w,vert, 4, co);
	}
	else {
		interp_weights_poly_v3( w,vert, 3, co);
		w[3] = 0.0f;
	}
}

/* Find the index in "sum" array before "value" is crossed. */
static int distribute_binary_search(float *sum, int n, float value)
{
	int mid, low=0, high=n;

	if (value == 0.f)
		return 0;

	while (low <= high) {
		mid= (low + high)/2;
		
		if (sum[mid] < value && value <= sum[mid+1])
			return mid;
		
		if (sum[mid] >= value)
			high= mid - 1;
		else if (sum[mid] < value)
			low= mid + 1;
		else
			return mid;
	}

	return low;
}

/* the max number if calls to rng_* funcs within psys_thread_distribute_particle
 * be sure to keep up to date if this changes */
#define PSYS_RND_DIST_SKIP 2

/* note: this function must be thread safe, for from == PART_FROM_CHILD */
#define ONLY_WORKING_WITH_PA_VERTS 0
static void distribute_from_verts_exec(ParticleTask *thread, ParticleData *pa, int p)
{
	ParticleThreadContext *ctx= thread->ctx;
	int rng_skip_tot= PSYS_RND_DIST_SKIP; /* count how many rng_* calls wont need skipping */

	/* TODO_PARTICLE - use original index */
	pa->num= ctx->index[p];
	pa->fuv[0] = 1.0f;
	pa->fuv[1] = pa->fuv[2] = pa->fuv[3] = 0.0;
	
#if ONLY_WORKING_WITH_PA_VERTS
	if (ctx->tree) {
		KDTreeNearest ptn[3];
		int w, maxw;
		
		psys_particle_on_dm(ctx->dm,from,pa->num,pa->num_dmcache,pa->fuv,pa->foffset,co1,0,0,0,orco1,0);
		BKE_mesh_orco_verts_transform((Mesh*)ob->data, &orco1, 1, 1);
		maxw = BLI_kdtree_find_nearest_n(ctx->tree,orco1,ptn,3);
		
		for (w=0; w<maxw; w++) {
			pa->verts[w]=ptn->num;
		}
	}
#endif
	
	if (rng_skip_tot > 0) /* should never be below zero */
		BLI_rng_skip(thread->rng, rng_skip_tot);
}

static void distribute_from_faces_exec(ParticleTask *thread, ParticleData *pa, int p) {
	ParticleThreadContext *ctx= thread->ctx;
	DerivedMesh *dm= ctx->dm;
	float randu, randv;
	int distr= ctx->distr;
	int i;
	int rng_skip_tot= PSYS_RND_DIST_SKIP; /* count how many rng_* calls wont need skipping */

	MFace *mface;
	
	pa->num = i = ctx->index[p];
	mface = dm->getTessFaceData(dm,i,CD_MFACE);
	
	switch (distr) {
		case PART_DISTR_JIT:
			if (ctx->jitlevel == 1) {
				if (mface->v4)
					psys_uv_to_w(0.5f, 0.5f, mface->v4, pa->fuv);
				else
					psys_uv_to_w(1.0f / 3.0f, 1.0f / 3.0f, mface->v4, pa->fuv);
			}
			else {
				ctx->jitoff[i] = fmod(ctx->jitoff[i],(float)ctx->jitlevel);
				if (!isnan(ctx->jitoff[i])) {
					psys_uv_to_w(ctx->jit[2*(int)ctx->jitoff[i]], ctx->jit[2*(int)ctx->jitoff[i]+1], mface->v4, pa->fuv);
					ctx->jitoff[i]++;
				}
			}
			break;
		case PART_DISTR_RAND:
			randu= BLI_rng_get_float(thread->rng);
			randv= BLI_rng_get_float(thread->rng);
			rng_skip_tot -= 2;
			
			psys_uv_to_w(randu, randv, mface->v4, pa->fuv);
			break;
	}
	pa->foffset= 0.0f;
	
	if (rng_skip_tot > 0) /* should never be below zero */
		BLI_rng_skip(thread->rng, rng_skip_tot);
}

static void distribute_from_volume_exec(ParticleTask *thread, ParticleData *pa, int p) {
	ParticleThreadContext *ctx= thread->ctx;
	DerivedMesh *dm= ctx->dm;
	float *v1, *v2, *v3, *v4, nor[3], co1[3], co2[3];
	float cur_d, min_d, randu, randv;
	int distr= ctx->distr;
	int i, intersect, tot;
	int rng_skip_tot= PSYS_RND_DIST_SKIP; /* count how many rng_* calls wont need skipping */
	
	MFace *mface;
	MVert *mvert=dm->getVertDataArray(dm,CD_MVERT);
	
	pa->num = i = ctx->index[p];
	mface = dm->getTessFaceData(dm,i,CD_MFACE);
	
	switch (distr) {
		case PART_DISTR_JIT:
			if (ctx->jitlevel == 1) {
				if (mface->v4)
					psys_uv_to_w(0.5f, 0.5f, mface->v4, pa->fuv);
				else
					psys_uv_to_w(1.0f / 3.0f, 1.0f / 3.0f, mface->v4, pa->fuv);
			}
			else {
				ctx->jitoff[i] = fmod(ctx->jitoff[i],(float)ctx->jitlevel);
				if (!isnan(ctx->jitoff[i])) {
					psys_uv_to_w(ctx->jit[2*(int)ctx->jitoff[i]], ctx->jit[2*(int)ctx->jitoff[i]+1], mface->v4, pa->fuv);
					ctx->jitoff[i]++;
				}
			}
			break;
		case PART_DISTR_RAND:
			randu= BLI_rng_get_float(thread->rng);
			randv= BLI_rng_get_float(thread->rng);
			rng_skip_tot -= 2;
			
			psys_uv_to_w(randu, randv, mface->v4, pa->fuv);
			break;
	}
	pa->foffset= 0.0f;
	
	/* experimental */
	tot=dm->getNumTessFaces(dm);
	
	psys_interpolate_face(mvert,mface,0,0,pa->fuv,co1,nor,0,0,0,0);
	
	normalize_v3(nor);
	mul_v3_fl(nor,-100.0);
	
	add_v3_v3v3(co2,co1,nor);
	
	min_d=2.0;
	intersect=0;
	
	for (i=0,mface=dm->getTessFaceDataArray(dm,CD_MFACE); i<tot; i++,mface++) {
		if (i==pa->num) continue;
		
		v1=mvert[mface->v1].co;
		v2=mvert[mface->v2].co;
		v3=mvert[mface->v3].co;
		
		if (isect_line_tri_v3(co1, co2, v2, v3, v1, &cur_d, 0)) {
			if (cur_d<min_d) {
				min_d=cur_d;
				pa->foffset=cur_d*50.0f; /* to the middle of volume */
				intersect=1;
			}
		}
		if (mface->v4) {
			v4=mvert[mface->v4].co;
			
			if (isect_line_tri_v3(co1, co2, v4, v1, v3, &cur_d, 0)) {
				if (cur_d<min_d) {
					min_d=cur_d;
					pa->foffset=cur_d*50.0f; /* to the middle of volume */
					intersect=1;
				}
			}
		}
	}
	if (intersect==0)
		pa->foffset=0.0;
	else {
		switch (distr) {
			case PART_DISTR_JIT:
				pa->foffset *= ctx->jit[p % (2 * ctx->jitlevel)];
				break;
			case PART_DISTR_RAND:
				pa->foffset *= BLI_frand();
				break;
		}
	}
	
	if (rng_skip_tot > 0) /* should never be below zero */
		BLI_rng_skip(thread->rng, rng_skip_tot);
}

static void distribute_children_exec(ParticleTask *thread, ChildParticle *cpa, int p) {
	ParticleThreadContext *ctx= thread->ctx;
	Object *ob= ctx->sim.ob;
	DerivedMesh *dm= ctx->dm;
	float orco1[3], co1[3], nor1[3];
	float randu, randv;
	int cfrom= ctx->cfrom;
	int i;
	int rng_skip_tot= PSYS_RND_DIST_SKIP; /* count how many rng_* calls wont need skipping */
	
	MFace *mf;
	
	if (ctx->index[p] < 0) {
		cpa->num=0;
		cpa->fuv[0]=cpa->fuv[1]=cpa->fuv[2]=cpa->fuv[3]=0.0f;
		cpa->pa[0]=cpa->pa[1]=cpa->pa[2]=cpa->pa[3]=0;
		return;
	}
	
	mf= dm->getTessFaceData(dm, ctx->index[p], CD_MFACE);
	
	randu= BLI_rng_get_float(thread->rng);
	randv= BLI_rng_get_float(thread->rng);
	rng_skip_tot -= 2;
	
	psys_uv_to_w(randu, randv, mf->v4, cpa->fuv);
	
	cpa->num = ctx->index[p];
	
	if (ctx->tree) {
		KDTreeNearest ptn[10];
		int w,maxw;//, do_seams;
		float maxd /*, mind,dd */, totw= 0.0f;
		int parent[10];
		float pweight[10];
		
		psys_particle_on_dm(dm,cfrom,cpa->num,DMCACHE_ISCHILD,cpa->fuv,cpa->foffset,co1,nor1,NULL,NULL,orco1,NULL);
		BKE_mesh_orco_verts_transform((Mesh*)ob->data, &orco1, 1, 1);
		maxw = BLI_kdtree_find_nearest_n(ctx->tree,orco1,ptn,3);
		
		maxd=ptn[maxw-1].dist;
		/* mind=ptn[0].dist; */ /* UNUSED */
		
		/* the weights here could be done better */
		for (w=0; w<maxw; w++) {
			parent[w]=ptn[w].index;
			pweight[w]=(float)pow(2.0,(double)(-6.0f*ptn[w].dist/maxd));
		}
		for (;w<10; w++) {
			parent[w]=-1;
			pweight[w]=0.0f;
		}
		
		for (w=0,i=0; w<maxw && i<4; w++) {
			if (parent[w]>=0) {
				cpa->pa[i]=parent[w];
				cpa->w[i]=pweight[w];
				totw+=pweight[w];
				i++;
			}
		}
		for (;i<4; i++) {
			cpa->pa[i]=-1;
			cpa->w[i]=0.0f;
		}
		
		if (totw>0.0f) for (w=0; w<4; w++)
			cpa->w[w]/=totw;
		
		cpa->parent=cpa->pa[0];
	}

	if (rng_skip_tot > 0) /* should never be below zero */
		BLI_rng_skip(thread->rng, rng_skip_tot);
}

static void exec_distribute_parent(TaskPool *UNUSED(pool), void *taskdata, int UNUSED(threadid))
{
	ParticleTask *task = taskdata;
	ParticleSystem *psys= task->ctx->sim.psys;
	ParticleData *pa;
	int p;
	
	pa= psys->particles + task->begin;
	switch (psys->part->from) {
		case PART_FROM_FACE:
			for (p = task->begin; p < task->end; ++p, ++pa)
				distribute_from_faces_exec(task, pa, p);
			break;
		case PART_FROM_VOLUME:
			for (p = task->begin; p < task->end; ++p, ++pa)
				distribute_from_volume_exec(task, pa, p);
			break;
		case PART_FROM_VERT:
			for (p = task->begin; p < task->end; ++p, ++pa)
				distribute_from_verts_exec(task, pa, p);
			break;
	}
}

static void exec_distribute_child(TaskPool *UNUSED(pool), void *taskdata, int UNUSED(threadid))
{
	ParticleTask *task = taskdata;
	ParticleSystem *psys = task->ctx->sim.psys;
	ChildParticle *cpa;
	int p;
	
	/* RNG skipping at the beginning */
	cpa = psys->child;
	for (p = 0; p < task->begin; ++p, ++cpa) {
		if (task->ctx->skip) /* simplification skip */
			BLI_rng_skip(task->rng, PSYS_RND_DIST_SKIP * task->ctx->skip[p]);
		
		BLI_rng_skip(task->rng, PSYS_RND_DIST_SKIP);
	}
		
	for (; p < task->end; ++p, ++cpa) {
		if (task->ctx->skip) /* simplification skip */
			BLI_rng_skip(task->rng, PSYS_RND_DIST_SKIP * task->ctx->skip[p]);
		
		distribute_children_exec(task, cpa, p);
	}
}

static int distribute_compare_orig_index(const void *p1, const void *p2, void *user_data)
{
	int *orig_index = (int *) user_data;
	int index1 = orig_index[*(const int *)p1];
	int index2 = orig_index[*(const int *)p2];

	if (index1 < index2)
		return -1;
	else if (index1 == index2) {
		/* this pointer comparison appears to make qsort stable for glibc,
		 * and apparently on solaris too, makes the renders reproducible */
		if (p1 < p2)
			return -1;
		else if (p1 == p2)
			return 0;
		else
			return 1;
	}
	else
		return 1;
}

static void distribute_invalid(Scene *scene, ParticleSystem *psys, int from)
{
	if (from == PART_FROM_CHILD) {
		ChildParticle *cpa;
		int p, totchild = psys_get_tot_child(scene, psys);

		if (psys->child && totchild) {
			for (p=0,cpa=psys->child; p<totchild; p++,cpa++) {
				cpa->fuv[0]=cpa->fuv[1]=cpa->fuv[2]=cpa->fuv[3] = 0.0;
				cpa->foffset= 0.0f;
				cpa->parent=0;
				cpa->pa[0]=cpa->pa[1]=cpa->pa[2]=cpa->pa[3]=0;
				cpa->num= -1;
			}
		}
	}
	else {
		PARTICLE_P;
		LOOP_PARTICLES {
			pa->fuv[0] = pa->fuv[1] = pa->fuv[2] = pa->fuv[3] = 0.0;
			pa->foffset= 0.0f;
			pa->num= -1;
		}
	}
}

/* Creates a distribution of coordinates on a DerivedMesh	*/
/* This is to denote functionality that does not yet work with mesh - only derived mesh */
static int psys_thread_context_init_distribute(ParticleThreadContext *ctx, ParticleSimulationData *sim, int from)
{
	Scene *scene = sim->scene;
	DerivedMesh *finaldm = sim->psmd->dm;
	Object *ob = sim->ob;
	ParticleSystem *psys= sim->psys;
	ParticleData *pa=0, *tpars= 0;
	ParticleSettings *part;
	ParticleSeam *seams= 0;
	KDTree *tree=0;
	DerivedMesh *dm= NULL;
	float *jit= NULL;
	int i, p=0;
	int cfrom=0;
	int totelem=0, totpart, *particle_element=0, children=0, totseam=0;
	int jitlevel= 1, distr;
	float *element_weight=NULL,*element_sum=NULL,*jitter_offset=NULL, *vweight=NULL;
	float cur, maxweight=0.0, tweight, totweight, inv_totweight, co[3], nor[3], orco[3];
	
	if (ELEM(NULL, ob, psys, psys->part))
		return 0;
	
	part=psys->part;
	totpart=psys->totpart;
	if (totpart==0)
		return 0;
	
	if (!finaldm->deformedOnly && !finaldm->getTessFaceDataArray(finaldm, CD_ORIGINDEX)) {
		printf("Can't create particles with the current modifier stack, disable destructive modifiers\n");
// XXX		error("Can't paint with the current modifier stack, disable destructive modifiers");
		return 0;
	}
	
	psys_thread_context_init(ctx, sim);
	
	/* First handle special cases */
	if (from == PART_FROM_CHILD) {
		/* Simple children */
		if (part->childtype != PART_CHILD_FACES) {
			BLI_srandom(31415926 + psys->seed + psys->child_seed);
			distribute_simple_children(scene, ob, finaldm, psys);
			return 0;
		}
	}
	else {
		/* Grid distribution */
		if (part->distr==PART_DISTR_GRID && from != PART_FROM_VERT) {
			BLI_srandom(31415926 + psys->seed);
			dm= CDDM_from_mesh((Mesh*)ob->data);
			DM_ensure_tessface(dm);
			distribute_grid(dm,psys);
			dm->release(dm);
			return 0;
		}
	}
	
	/* Create trees and original coordinates if needed */
	if (from == PART_FROM_CHILD) {
		distr=PART_DISTR_RAND;
		BLI_srandom(31415926 + psys->seed + psys->child_seed);
		dm= finaldm;

		/* BMESH ONLY */
		DM_ensure_tessface(dm);

		children=1;

		tree=BLI_kdtree_new(totpart);

		for (p=0,pa=psys->particles; p<totpart; p++,pa++) {
			psys_particle_on_dm(dm,part->from,pa->num,pa->num_dmcache,pa->fuv,pa->foffset,co,nor,0,0,orco,NULL);
			BKE_mesh_orco_verts_transform((Mesh*)ob->data, &orco, 1, 1);
			BLI_kdtree_insert(tree, p, orco);
		}

		BLI_kdtree_balance(tree);

		totpart = psys_get_tot_child(scene, psys);
		cfrom = from = PART_FROM_FACE;
	}
	else {
		distr = part->distr;
		BLI_srandom(31415926 + psys->seed);
		
		if (psys->part->use_modifier_stack)
			dm = finaldm;
		else
			dm= CDDM_from_mesh((Mesh*)ob->data);

		/* BMESH ONLY, for verts we don't care about tessfaces */
		if (from != PART_FROM_VERT) {
			DM_ensure_tessface(dm);
		}

		/* we need orco for consistent distributions */
		if (!CustomData_has_layer(&dm->vertData, CD_ORCO))
			DM_add_vert_layer(dm, CD_ORCO, CD_ASSIGN, BKE_mesh_orco_verts_get(ob));

		if (from == PART_FROM_VERT) {
			MVert *mv= dm->getVertDataArray(dm, CD_MVERT);
			float (*orcodata)[3] = dm->getVertDataArray(dm, CD_ORCO);
			int totvert = dm->getNumVerts(dm);

			tree=BLI_kdtree_new(totvert);

			for (p=0; p<totvert; p++) {
				if (orcodata) {
					copy_v3_v3(co,orcodata[p]);
					BKE_mesh_orco_verts_transform((Mesh*)ob->data, &co, 1, 1);
				}
				else
					copy_v3_v3(co,mv[p].co);
				BLI_kdtree_insert(tree, p, co);
			}

			BLI_kdtree_balance(tree);
		}
	}

	/* Get total number of emission elements and allocate needed arrays */
	totelem = (from == PART_FROM_VERT) ? dm->getNumVerts(dm) : dm->getNumTessFaces(dm);

	if (totelem == 0) {
		distribute_invalid(scene, psys, children ? PART_FROM_CHILD : 0);

		if (G.debug & G_DEBUG)
			fprintf(stderr,"Particle distribution error: Nothing to emit from!\n");

		if (dm != finaldm) dm->release(dm);

		BLI_kdtree_free(tree);

		return 0;
	}

	element_weight	= MEM_callocN(sizeof(float)*totelem, "particle_distribution_weights");
	particle_element= MEM_callocN(sizeof(int)*totpart, "particle_distribution_indexes");
	element_sum		= MEM_callocN(sizeof(float)*(totelem+1), "particle_distribution_sum");
	jitter_offset	= MEM_callocN(sizeof(float)*totelem, "particle_distribution_jitoff");

	/* Calculate weights from face areas */
	if ((part->flag&PART_EDISTR || children) && from != PART_FROM_VERT) {
		MVert *v1, *v2, *v3, *v4;
		float totarea=0.f, co1[3], co2[3], co3[3], co4[3];
		float (*orcodata)[3];
		
		orcodata= dm->getVertDataArray(dm, CD_ORCO);

		for (i=0; i<totelem; i++) {
			MFace *mf=dm->getTessFaceData(dm,i,CD_MFACE);

			if (orcodata) {
				copy_v3_v3(co1, orcodata[mf->v1]);
				copy_v3_v3(co2, orcodata[mf->v2]);
				copy_v3_v3(co3, orcodata[mf->v3]);
				BKE_mesh_orco_verts_transform((Mesh*)ob->data, &co1, 1, 1);
				BKE_mesh_orco_verts_transform((Mesh*)ob->data, &co2, 1, 1);
				BKE_mesh_orco_verts_transform((Mesh*)ob->data, &co3, 1, 1);
				if (mf->v4) {
					copy_v3_v3(co4, orcodata[mf->v4]);
					BKE_mesh_orco_verts_transform((Mesh*)ob->data, &co4, 1, 1);
				}
			}
			else {
				v1= (MVert*)dm->getVertData(dm,mf->v1,CD_MVERT);
				v2= (MVert*)dm->getVertData(dm,mf->v2,CD_MVERT);
				v3= (MVert*)dm->getVertData(dm,mf->v3,CD_MVERT);
				copy_v3_v3(co1, v1->co);
				copy_v3_v3(co2, v2->co);
				copy_v3_v3(co3, v3->co);
				if (mf->v4) {
					v4= (MVert*)dm->getVertData(dm,mf->v4,CD_MVERT);
					copy_v3_v3(co4, v4->co);
				}
			}

			cur = mf->v4 ? area_quad_v3(co1, co2, co3, co4) : area_tri_v3(co1, co2, co3);
			
			if (cur > maxweight)
				maxweight = cur;

			element_weight[i] = cur;
			totarea += cur;
		}

		for (i=0; i<totelem; i++)
			element_weight[i] /= totarea;

		maxweight /= totarea;
	}
	else {
		float min=1.0f/(float)(MIN2(totelem,totpart));
		for (i=0; i<totelem; i++)
			element_weight[i]=min;
		maxweight=min;
	}

	/* Calculate weights from vgroup */
	vweight = psys_cache_vgroup(dm,psys,PSYS_VG_DENSITY);

	if (vweight) {
		if (from==PART_FROM_VERT) {
			for (i=0;i<totelem; i++)
				element_weight[i]*=vweight[i];
		}
		else { /* PART_FROM_FACE / PART_FROM_VOLUME */
			for (i=0;i<totelem; i++) {
				MFace *mf=dm->getTessFaceData(dm,i,CD_MFACE);
				tweight = vweight[mf->v1] + vweight[mf->v2] + vweight[mf->v3];
				
				if (mf->v4) {
					tweight += vweight[mf->v4];
					tweight /= 4.0f;
				}
				else {
					tweight /= 3.0f;
				}

				element_weight[i]*=tweight;
			}
		}
		MEM_freeN(vweight);
	}

	/* Calculate total weight of all elements */
	totweight= 0.0f;
	for (i=0;i<totelem; i++)
		totweight += element_weight[i];

	inv_totweight = (totweight > 0.f ? 1.f/totweight : 0.f);

	/* Calculate cumulative weights */
	element_sum[0] = 0.0f;
	for (i=0; i<totelem; i++)
		element_sum[i+1] = element_sum[i] + element_weight[i] * inv_totweight;
	
	/* Finally assign elements to particles */
	if ((part->flag&PART_TRAND) || (part->simplify_flag&PART_SIMPLIFY_ENABLE)) {
		float pos;

		for (p=0; p<totpart; p++) {
			/* In theory element_sum[totelem] should be 1.0, but due to float errors this is not necessarily always true, so scale pos accordingly. */
			pos= BLI_frand() * element_sum[totelem];
			particle_element[p] = distribute_binary_search(element_sum, totelem, pos);
			particle_element[p] = MIN2(totelem-1, particle_element[p]);
			jitter_offset[particle_element[p]] = pos;
		}
	}
	else {
		double step, pos;
		
		step= (totpart < 2) ? 0.5 : 1.0/(double)totpart;
		pos= 1e-6; /* tiny offset to avoid zero weight face */
		i= 0;

		for (p=0; p<totpart; p++, pos+=step) {
			while ((i < totelem) && (pos > (double)element_sum[i + 1]))
				i++;

			particle_element[p] = MIN2(totelem-1, i);

			/* avoid zero weight face */
			if (p == totpart-1 && element_weight[particle_element[p]] == 0.0f)
				particle_element[p] = particle_element[p-1];

			jitter_offset[particle_element[p]] = pos;
		}
	}

	MEM_freeN(element_sum);

	/* For hair, sort by origindex (allows optimization's in rendering), */
	/* however with virtual parents the children need to be in random order. */
	if (part->type == PART_HAIR && !(part->childtype==PART_CHILD_FACES && part->parents!=0.0f)) {
		int *orig_index = NULL;

		if (from == PART_FROM_VERT) {
			if (dm->numVertData)
				orig_index = dm->getVertDataArray(dm, CD_ORIGINDEX);
		}
		else {
			if (dm->numTessFaceData)
				orig_index = dm->getTessFaceDataArray(dm, CD_ORIGINDEX);
		}

		if (orig_index) {
			BLI_qsort_r(particle_element, totpart, sizeof(int), distribute_compare_orig_index, orig_index);
		}
	}

	/* Create jittering if needed */
	if (distr==PART_DISTR_JIT && ELEM(from,PART_FROM_FACE,PART_FROM_VOLUME)) {
		jitlevel= part->userjit;
		
		if (jitlevel == 0) {
			jitlevel= totpart/totelem;
			if (part->flag & PART_EDISTR) jitlevel*= 2;	/* looks better in general, not very scietific */
			if (jitlevel<3) jitlevel= 3;
		}
		
		jit= MEM_callocN((2+ jitlevel*2)*sizeof(float), "jit");

		/* for small amounts of particles we use regular jitter since it looks
		 * a bit better, for larger amounts we switch to hammersley sequence 
		 * because it is much faster */
		if (jitlevel < 25)
			init_mv_jit(jit, jitlevel, psys->seed, part->jitfac);
		else
			hammersley_create(jit, jitlevel+1, psys->seed, part->jitfac);
		BLI_array_randomize(jit, 2*sizeof(float), jitlevel, psys->seed); /* for custom jit or even distribution */
	}

	/* Setup things for threaded distribution */
	ctx->tree= tree;
	ctx->seams= seams;
	ctx->totseam= totseam;
	ctx->sim.psys= psys;
	ctx->index= particle_element;
	ctx->jit= jit;
	ctx->jitlevel= jitlevel;
	ctx->jitoff= jitter_offset;
	ctx->weight= element_weight;
	ctx->maxweight= maxweight;
	ctx->cfrom= cfrom;
	ctx->distr= distr;
	ctx->dm= dm;
	ctx->tpars= tpars;

	if (children) {
		totpart= psys_render_simplify_distribution(ctx, totpart);
		alloc_child_particles(psys, totpart);
	}

	return 1;
}

static void psys_task_init_distribute(ParticleTask *task, ParticleSimulationData *sim)
{
	/* init random number generator */
	int seed = 31415926 + sim->psys->seed;
	
	task->rng = BLI_rng_new(seed);
}

static void distribute_particles_on_dm(ParticleSimulationData *sim, int from)
{
	TaskScheduler *task_scheduler;
	TaskPool *task_pool;
	ParticleThreadContext ctx;
	ParticleTask *tasks;
	DerivedMesh *finaldm = sim->psmd->dm;
	int i, totpart, numtasks;
	
	/* create a task pool for distribution tasks */
	if (!psys_thread_context_init_distribute(&ctx, sim, from))
		return;
	
	task_scheduler = BLI_task_scheduler_get();
	task_pool = BLI_task_pool_create(task_scheduler, &ctx);
	
	totpart = (from == PART_FROM_CHILD ? sim->psys->totchild : sim->psys->totpart);
	psys_tasks_create(&ctx, totpart, &tasks, &numtasks);
	for (i = 0; i < numtasks; ++i) {
		ParticleTask *task = &tasks[i];
		
		psys_task_init_distribute(task, sim);
		if (from == PART_FROM_CHILD)
			BLI_task_pool_push(task_pool, exec_distribute_child, task, false, TASK_PRIORITY_LOW);
		else
			BLI_task_pool_push(task_pool, exec_distribute_parent, task, false, TASK_PRIORITY_LOW);
	}
	BLI_task_pool_work_and_wait(task_pool);
	
	BLI_task_pool_free(task_pool);
	
	psys_calc_dmcache(sim->ob, finaldm, sim->psys);
	
	if (ctx.dm != finaldm)
		ctx.dm->release(ctx.dm);
	
	psys_tasks_free(tasks, numtasks);
	
	psys_thread_context_free(&ctx);
}

/* ready for future use, to emit particles without geometry */
static void distribute_particles_on_shape(ParticleSimulationData *sim, int UNUSED(from))
{
	distribute_invalid(sim->scene, sim->psys, 0);

	fprintf(stderr,"Shape emission not yet possible!\n");
}

void distribute_particles(ParticleSimulationData *sim, int from)
{
	PARTICLE_PSMD;
	int distr_error=0;

	if (psmd) {
		if (psmd->dm)
			distribute_particles_on_dm(sim, from);
		else
			distr_error=1;
	}
	else
		distribute_particles_on_shape(sim, from);

	if (distr_error) {
		distribute_invalid(sim->scene, sim->psys, from);

		fprintf(stderr,"Particle distribution error!\n");
	}
}

/* ======== Simplify ======== */

static float psys_render_viewport_falloff(double rate, float dist, float width)
{
	return pow(rate, dist / width);
}

static float psys_render_projected_area(ParticleSystem *psys, const float center[3], float area, double vprate, float *viewport)
{
	ParticleRenderData *data = psys->renderdata;
	float co[4], view[3], ortho1[3], ortho2[3], w, dx, dy, radius;
	
	/* transform to view space */
	copy_v3_v3(co, center);
	co[3] = 1.0f;
	mul_m4_v4(data->viewmat, co);
	
	/* compute two vectors orthogonal to view vector */
	normalize_v3_v3(view, co);
	ortho_basis_v3v3_v3(ortho1, ortho2, view);

	/* compute on screen minification */
	w = co[2] * data->winmat[2][3] + data->winmat[3][3];
	dx = data->winx * ortho2[0] * data->winmat[0][0];
	dy = data->winy * ortho2[1] * data->winmat[1][1];
	w = sqrtf(dx * dx + dy * dy) / w;

	/* w squared because we are working with area */
	area = area * w * w;

	/* viewport of the screen test */

	/* project point on screen */
	mul_m4_v4(data->winmat, co);
	if (co[3] != 0.0f) {
		co[0] = 0.5f * data->winx * (1.0f + co[0] / co[3]);
		co[1] = 0.5f * data->winy * (1.0f + co[1] / co[3]);
	}

	/* screen space radius */
	radius = sqrtf(area / (float)M_PI);

	/* make smaller using fallof once over screen edge */
	*viewport = 1.0f;

	if (co[0] + radius < 0.0f)
		*viewport *= psys_render_viewport_falloff(vprate, -(co[0] + radius), data->winx);
	else if (co[0] - radius > data->winx)
		*viewport *= psys_render_viewport_falloff(vprate, (co[0] - radius) - data->winx, data->winx);

	if (co[1] + radius < 0.0f)
		*viewport *= psys_render_viewport_falloff(vprate, -(co[1] + radius), data->winy);
	else if (co[1] - radius > data->winy)
		*viewport *= psys_render_viewport_falloff(vprate, (co[1] - radius) - data->winy, data->winy);
	
	return area;
}

/* BMESH_TODO, for orig face data, we need to use MPoly */
static int psys_render_simplify_distribution(ParticleThreadContext *ctx, int tot)
{
	DerivedMesh *dm = ctx->dm;
	Mesh *me = (Mesh *)(ctx->sim.ob->data);
	MFace *mf, *mface;
	MVert *mvert;
	ParticleRenderData *data;
	ParticleRenderElem *elems, *elem;
	ParticleSettings *part = ctx->sim.psys->part;
	float *facearea, (*facecenter)[3], size[3], fac, powrate, scaleclamp;
	float co1[3], co2[3], co3[3], co4[3], lambda, arearatio, t, area, viewport;
	double vprate;
	int *facetotvert;
	int a, b, totorigface, totface, newtot, skipped;

	/* double lookup */
	const int *index_mf_to_mpoly;
	const int *index_mp_to_orig;

	if (part->ren_as != PART_DRAW_PATH || !(part->draw & PART_DRAW_REN_STRAND))
		return tot;
	if (!ctx->sim.psys->renderdata)
		return tot;

	data = ctx->sim.psys->renderdata;
	if (data->timeoffset)
		return 0;
	if (!(part->simplify_flag & PART_SIMPLIFY_ENABLE))
		return tot;

	mvert = dm->getVertArray(dm);
	mface = dm->getTessFaceArray(dm);
	totface = dm->getNumTessFaces(dm);
	totorigface = me->totpoly;

	if (totface == 0 || totorigface == 0)
		return tot;

	index_mf_to_mpoly = dm->getTessFaceDataArray(dm, CD_ORIGINDEX);
	index_mp_to_orig  = dm->getPolyDataArray(dm, CD_ORIGINDEX);
	if (index_mf_to_mpoly == NULL) {
		index_mp_to_orig = NULL;
	}

	facearea = MEM_callocN(sizeof(float) * totorigface, "SimplifyFaceArea");
	facecenter = MEM_callocN(sizeof(float[3]) * totorigface, "SimplifyFaceCenter");
	facetotvert = MEM_callocN(sizeof(int) * totorigface, "SimplifyFaceArea");
	elems = MEM_callocN(sizeof(ParticleRenderElem) * totorigface, "SimplifyFaceElem");

	if (data->elems)
		MEM_freeN(data->elems);

	data->do_simplify = true;
	data->elems = elems;
	data->index_mf_to_mpoly = index_mf_to_mpoly;
	data->index_mp_to_orig  = index_mp_to_orig;

	/* compute number of children per original face */
	for (a = 0; a < tot; a++) {
		b = (index_mf_to_mpoly) ? DM_origindex_mface_mpoly(index_mf_to_mpoly, index_mp_to_orig, ctx->index[a]) : ctx->index[a];
		if (b != ORIGINDEX_NONE) {
			elems[b].totchild++;
		}
	}

	/* compute areas and centers of original faces */
	for (mf = mface, a = 0; a < totface; a++, mf++) {
		b = (index_mf_to_mpoly) ? DM_origindex_mface_mpoly(index_mf_to_mpoly, index_mp_to_orig, a) : a;

		if (b != ORIGINDEX_NONE) {
			copy_v3_v3(co1, mvert[mf->v1].co);
			copy_v3_v3(co2, mvert[mf->v2].co);
			copy_v3_v3(co3, mvert[mf->v3].co);

			add_v3_v3(facecenter[b], co1);
			add_v3_v3(facecenter[b], co2);
			add_v3_v3(facecenter[b], co3);

			if (mf->v4) {
				copy_v3_v3(co4, mvert[mf->v4].co);
				add_v3_v3(facecenter[b], co4);
				facearea[b] += area_quad_v3(co1, co2, co3, co4);
				facetotvert[b] += 4;
			}
			else {
				facearea[b] += area_tri_v3(co1, co2, co3);
				facetotvert[b] += 3;
			}
		}
	}

	for (a = 0; a < totorigface; a++)
		if (facetotvert[a] > 0)
			mul_v3_fl(facecenter[a], 1.0f / facetotvert[a]);

	/* for conversion from BU area / pixel area to reference screen size */
	BKE_mesh_texspace_get(me, 0, 0, size);
	fac = ((size[0] + size[1] + size[2]) / 3.0f) / part->simplify_refsize;
	fac = fac * fac;

	powrate = log(0.5f) / log(part->simplify_rate * 0.5f);
	if (part->simplify_flag & PART_SIMPLIFY_VIEWPORT)
		vprate = pow(1.0f - part->simplify_viewport, 5.0);
	else
		vprate = 1.0;

	/* set simplification parameters per original face */
	for (a = 0, elem = elems; a < totorigface; a++, elem++) {
		area = psys_render_projected_area(ctx->sim.psys, facecenter[a], facearea[a], vprate, &viewport);
		arearatio = fac * area / facearea[a];

		if ((arearatio < 1.0f || viewport < 1.0f) && elem->totchild) {
			/* lambda is percentage of elements to keep */
			lambda = (arearatio < 1.0f) ? powf(arearatio, powrate) : 1.0f;
			lambda *= viewport;

			lambda = MAX2(lambda, 1.0f / elem->totchild);

			/* compute transition region */
			t = part->simplify_transition;
			elem->t = (lambda - t < 0.0f) ? lambda : (lambda + t > 1.0f) ? 1.0f - lambda : t;
			elem->reduce = 1;

			/* scale at end and beginning of the transition region */
			elem->scalemax = (lambda + t < 1.0f) ? 1.0f / lambda : 1.0f / (1.0f - elem->t * elem->t / t);
			elem->scalemin = (lambda + t < 1.0f) ? 0.0f : elem->scalemax * (1.0f - elem->t / t);

			elem->scalemin = sqrtf(elem->scalemin);
			elem->scalemax = sqrtf(elem->scalemax);

			/* clamp scaling */
			scaleclamp = (float)min_ii(elem->totchild, 10);
			elem->scalemin = MIN2(scaleclamp, elem->scalemin);
			elem->scalemax = MIN2(scaleclamp, elem->scalemax);

			/* extend lambda to include transition */
			lambda = lambda + elem->t;
			if (lambda > 1.0f)
				lambda = 1.0f;
		}
		else {
			lambda = arearatio;

			elem->scalemax = 1.0f; //sqrt(lambda);
			elem->scalemin = 1.0f; //sqrt(lambda);
			elem->reduce = 0;
		}

		elem->lambda = lambda;
		elem->scalemin = sqrtf(elem->scalemin);
		elem->scalemax = sqrtf(elem->scalemax);
		elem->curchild = 0;
	}

	MEM_freeN(facearea);
	MEM_freeN(facecenter);
	MEM_freeN(facetotvert);

	/* move indices and set random number skipping */
	ctx->skip = MEM_callocN(sizeof(int) * tot, "SimplificationSkip");

	skipped = 0;
	for (a = 0, newtot = 0; a < tot; a++) {
		b = (index_mf_to_mpoly) ? DM_origindex_mface_mpoly(index_mf_to_mpoly, index_mp_to_orig, ctx->index[a]) : ctx->index[a];

		if (b != ORIGINDEX_NONE) {
			if (elems[b].curchild++ < ceil(elems[b].lambda * elems[b].totchild)) {
				ctx->index[newtot] = ctx->index[a];
				ctx->skip[newtot] = skipped;
				skipped = 0;
				newtot++;
			}
			else skipped++;
		}
		else skipped++;
	}

	for (a = 0, elem = elems; a < totorigface; a++, elem++)
		elem->curchild = 0;

	return newtot;
}
