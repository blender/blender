/* particle.c
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

#include <stdlib.h>
#include <math.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "DNA_boid_types.h"
#include "DNA_curve_types.h"
#include "DNA_group_types.h"
#include "DNA_ipo_types.h" 	// XXX old animation system stuff to remove!
#include "DNA_key_types.h"
#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_force.h"
#include "DNA_object_types.h"
#include "DNA_particle_types.h"
#include "DNA_scene_types.h"
#include "DNA_smoke_types.h"
#include "DNA_texture_types.h"

#include "BLI_math.h"
#include "BLI_blenlib.h"
#include "BLI_dynstr.h"
#include "BLI_kdtree.h"
#include "BLI_listbase.h"
#include "BLI_rand.h"
#include "BLI_threads.h"

#include "BKE_anim.h"

#include "BKE_boids.h"
#include "BKE_cloth.h"
#include "BKE_effect.h"
#include "BKE_global.h"
#include "BKE_group.h"
#include "BKE_main.h"
#include "BKE_lattice.h"
#include "BKE_utildefines.h"
#include "BKE_displist.h"
#include "BKE_particle.h"
#include "BKE_DerivedMesh.h"
#include "BKE_object.h"
#include "BKE_cloth.h"
#include "BKE_material.h"
#include "BKE_key.h"
#include "BKE_library.h"
#include "BKE_depsgraph.h"
#include "BKE_modifier.h"
#include "BKE_mesh.h"
#include "BKE_cdderivedmesh.h"
#include "BKE_pointcache.h"

#include "RE_render_ext.h"

static void get_cpa_texture(DerivedMesh *dm, Material *ma, int face_index,
				float *fuv, float *orco, ParticleTexture *ptex,	int event);
static void get_child_modifier_parameters(ParticleSettings *part, ParticleThreadContext *ctx,
				ChildParticle *cpa, short cpa_from, int cpa_num, float *cpa_fuv, float *orco, ParticleTexture *ptex);
static void do_child_modifiers(ParticleSimulationData *sim,
				ParticleTexture *ptex, ParticleKey *par, float *par_rot, ChildParticle *cpa,
				float *orco, float mat[4][4], ParticleKey *state, float t);

/* few helpers for countall etc. */
int count_particles(ParticleSystem *psys){
	ParticleSettings *part=psys->part;
	PARTICLE_P;
	int tot=0;

	LOOP_SHOWN_PARTICLES {
		if(pa->alive == PARS_UNBORN && (part->flag & PART_UNBORN)==0);
		else if(pa->alive == PARS_DEAD && (part->flag & PART_DIED)==0);
		else tot++;
	}
	return tot;
}
int count_particles_mod(ParticleSystem *psys, int totgr, int cur){
	ParticleSettings *part=psys->part;
	PARTICLE_P;
	int tot=0;

	LOOP_SHOWN_PARTICLES {
		if(pa->alive == PARS_UNBORN && (part->flag & PART_UNBORN)==0);
		else if(pa->alive == PARS_DEAD && (part->flag & PART_DIED)==0);
		else if(p%totgr==cur) tot++;
	}
	return tot;
}
/* we allocate path cache memory in chunks instead of a big continguous
 * chunk, windows' memory allocater fails to find big blocks of memory often */

#define PATH_CACHE_BUF_SIZE 1024

static ParticleCacheKey **psys_alloc_path_cache_buffers(ListBase *bufs, int tot, int steps)
{
	LinkData *buf;
	ParticleCacheKey **cache;
	int i, totkey, totbufkey;

	tot= MAX2(tot, 1);
	totkey = 0;
	cache = MEM_callocN(tot*sizeof(void*), "PathCacheArray");

	while(totkey < tot) {
		totbufkey= MIN2(tot-totkey, PATH_CACHE_BUF_SIZE);
		buf= MEM_callocN(sizeof(LinkData), "PathCacheLinkData");
		buf->data= MEM_callocN(sizeof(ParticleCacheKey)*totbufkey*steps, "ParticleCacheKey");

		for(i=0; i<totbufkey; i++)
			cache[totkey+i] = ((ParticleCacheKey*)buf->data) + i*steps;

		totkey += totbufkey;
		BLI_addtail(bufs, buf);
	}

	return cache;
}

static void psys_free_path_cache_buffers(ParticleCacheKey **cache, ListBase *bufs)
{
	LinkData *buf;

	if(cache)
		MEM_freeN(cache);

	for(buf= bufs->first; buf; buf=buf->next)
		MEM_freeN(buf->data);
	BLI_freelistN(bufs);
}

/************************************************/
/*			Getting stuff						*/
/************************************************/
/* get object's active particle system safely */
ParticleSystem *psys_get_current(Object *ob)
{
	ParticleSystem *psys;
	if(ob==0) return 0;

	for(psys=ob->particlesystem.first; psys; psys=psys->next){
		if(psys->flag & PSYS_CURRENT)
			return psys;
	}
	
	return 0;
}
short psys_get_current_num(Object *ob)
{
	ParticleSystem *psys;
	short i;

	if(ob==0) return 0;

	for(psys=ob->particlesystem.first, i=0; psys; psys=psys->next, i++)
		if(psys->flag & PSYS_CURRENT)
			return i;
	
	return i;
}
void psys_set_current_num(Object *ob, int index)
{
	ParticleSystem *psys;
	short i;

	if(ob==0) return;

	for(psys=ob->particlesystem.first, i=0; psys; psys=psys->next, i++) {
		if(i == index)
			psys->flag |= PSYS_CURRENT;
		else
			psys->flag &= ~PSYS_CURRENT;
	}
}
Object *psys_find_object(Scene *scene, ParticleSystem *psys)
{
	Base *base = scene->base.first;
	ParticleSystem *tpsys;

	for(base = scene->base.first; base; base = base->next) {
		for(tpsys = base->object->particlesystem.first; psys; psys=psys->next) {
			if(tpsys == psys)
				return base->object;
		}
	}

	return NULL;
}
Object *psys_get_lattice(ParticleSimulationData *sim)
{
	Object *lattice=0;
	
	if(psys_in_edit_mode(sim->scene, sim->psys)==0){

		ModifierData *md = (ModifierData*)psys_get_modifier(sim->ob, sim->psys);

		for(; md; md=md->next){
			if(md->type==eModifierType_Lattice){
				LatticeModifierData *lmd = (LatticeModifierData *)md;
				lattice=lmd->object;
				break;
			}
		}
		if(lattice)
			init_latt_deform(lattice,0);
	}

	return lattice;
}
void psys_disable_all(Object *ob)
{
	ParticleSystem *psys=ob->particlesystem.first;

	for(; psys; psys=psys->next)
		psys->flag |= PSYS_DISABLED;
}
void psys_enable_all(Object *ob)
{
	ParticleSystem *psys=ob->particlesystem.first;

	for(; psys; psys=psys->next)
		psys->flag &= ~PSYS_DISABLED;
}
int psys_in_edit_mode(Scene *scene, ParticleSystem *psys)
{
	return (scene->basact && (scene->basact->object->mode & OB_MODE_PARTICLE_EDIT) && psys==psys_get_current((scene->basact)->object) && (psys->edit || psys->pointcache->edit));
}
static void psys_create_frand(ParticleSystem *psys)
{
	int i;
	float *rand = psys->frand = MEM_callocN(PSYS_FRAND_COUNT * sizeof(float), "particle randoms");

	BLI_srandom(psys->seed);

	for(i=0; i<1024; i++, rand++)
		*rand = BLI_frand();
}
int psys_check_enabled(Object *ob, ParticleSystem *psys)
{
	ParticleSystemModifierData *psmd;
	Mesh *me;

	if(psys->flag & PSYS_DISABLED || psys->flag & PSYS_DELETE || !psys->part)
		return 0;

	if(ob->type == OB_MESH) {
		me= (Mesh*)ob->data;
		if(me->mr && me->mr->current != 1)
			return 0;
	}

	psmd= psys_get_modifier(ob, psys);
	if(psys->renderdata) {
		if(!(psmd->modifier.mode & eModifierMode_Render))
			return 0;
	}
	else if(!(psmd->modifier.mode & eModifierMode_Realtime))
		return 0;

	/* perhaps not the perfect place, but we have to be sure the rands are there before usage */
	if(!psys->frand)
		psys_create_frand(psys);
	else if(psys->recalc & PSYS_RECALC_RESET) {
		MEM_freeN(psys->frand);
		psys_create_frand(psys);
	}
	
	return 1;
}

int psys_check_edited(ParticleSystem *psys)
{
	if(psys->part && psys->part->type==PART_HAIR)
		return (psys->flag & PSYS_EDITED || (psys->edit && psys->edit->edited));
	else
		return (psys->pointcache->edit && psys->pointcache->edit->edited);
}

void psys_check_group_weights(ParticleSettings *part)
{
	ParticleDupliWeight *dw, *tdw;
	GroupObject *go;
	int current = 0;

	if(part->ren_as == PART_DRAW_GR && part->dup_group && part->dup_group->gobject.first) {
		/* first remove all weights that don't have an object in the group */
		dw = part->dupliweights.first;
		while(dw) {
			if(!object_in_group(dw->ob, part->dup_group)) {
				tdw = dw->next;
				BLI_freelinkN(&part->dupliweights, dw);
				dw = tdw;
			}
			else
				dw = dw->next;
		}

		/* then add objects in the group to new list */
		go = part->dup_group->gobject.first;
		while(go) {
			dw = part->dupliweights.first;
			while(dw && dw->ob != go->ob)
				dw = dw->next;
			
			if(!dw) {
				dw = MEM_callocN(sizeof(ParticleDupliWeight), "ParticleDupliWeight");
				dw->ob = go->ob;
				dw->count = 1;
				BLI_addtail(&part->dupliweights, dw);
			}

			go = go->next;	
		}

		dw = part->dupliweights.first;
		for(; dw; dw=dw->next) {
			if(dw->flag & PART_DUPLIW_CURRENT) {
				current = 1;
				break;
			}
		}

		if(!current) {
			dw = part->dupliweights.first;
			if(dw)
				dw->flag |= PART_DUPLIW_CURRENT;
		}
	}
	else {
		BLI_freelistN(&part->dupliweights);
	}
}
/************************************************/
/*			Freeing stuff						*/
/************************************************/
void psys_free_settings(ParticleSettings *part)
{
	free_partdeflect(part->pd);
	free_partdeflect(part->pd2);

	if(part->effector_weights)
		MEM_freeN(part->effector_weights);

	BLI_freelistN(&part->dupliweights);

	boid_free_settings(part->boids);
}

void free_hair(Object *ob, ParticleSystem *psys, int dynamics)
{
	PARTICLE_P;

	if(psys->part->type != PART_HAIR)
		return;

	LOOP_PARTICLES {
		if(pa->hair)
			MEM_freeN(pa->hair);
		pa->hair = NULL;
		pa->totkey = 0;
	}

	psys->flag &= ~PSYS_HAIR_DONE;

	if(psys->clmd) {
		if(dynamics) {
			BKE_ptcache_free_list(&psys->ptcaches);
			psys->clmd->point_cache = psys->pointcache = NULL;
			psys->clmd->ptcaches.first = psys->clmd->ptcaches.first = NULL;

			modifier_free((ModifierData*)psys->clmd);
			
			psys->clmd = NULL;
		}
		else {
			cloth_free_modifier(ob, psys->clmd);
		}
	}

	if(psys->hair_in_dm)
		psys->hair_in_dm->release(psys->hair_in_dm);
	psys->hair_in_dm = NULL;

	if(psys->hair_out_dm)
		psys->hair_out_dm->release(psys->hair_out_dm);
	psys->hair_out_dm = NULL;
}
void free_keyed_keys(ParticleSystem *psys)
{
	PARTICLE_P;

	if(psys->part->type == PART_HAIR)
		return;

	if(psys->particles && psys->particles->keys) {
		MEM_freeN(psys->particles->keys);

		LOOP_PARTICLES {
			if(pa->keys) {
				pa->keys= NULL;
				pa->totkey= 0;
			}
		}
	}
}
static void free_child_path_cache(ParticleSystem *psys)
{
	psys_free_path_cache_buffers(psys->childcache, &psys->childcachebufs);
	psys->childcache = NULL;
	psys->totchildcache = 0;
}
void psys_free_path_cache(ParticleSystem *psys, PTCacheEdit *edit)
{
	if(edit) {
		psys_free_path_cache_buffers(edit->pathcache, &edit->pathcachebufs);
		edit->pathcache= NULL;
		edit->totcached= 0;
	}
	if(psys) {
		psys_free_path_cache_buffers(psys->pathcache, &psys->pathcachebufs);
		psys->pathcache= NULL;
		psys->totcached= 0;

		free_child_path_cache(psys);
	}
}
void psys_free_children(ParticleSystem *psys)
{
	if(psys->child) {
		MEM_freeN(psys->child);
		psys->child=0;
		psys->totchild=0;
	}

	free_child_path_cache(psys);
}
void psys_free_particles(ParticleSystem *psys)
{
	PARTICLE_P;

	if(psys->particles) {
		if(psys->part->type==PART_HAIR) {
			LOOP_PARTICLES {
				if(pa->hair)
					MEM_freeN(pa->hair);
			}
		}
		
		if(psys->particles->keys)
			MEM_freeN(psys->particles->keys);
		
		if(psys->particles->boid)
			MEM_freeN(psys->particles->boid);

		MEM_freeN(psys->particles);
		psys->particles= NULL;
		psys->totpart= 0;
	}
}
void psys_free_pdd(ParticleSystem *psys)
{
	if(psys->pdd) {
		if(psys->pdd->cdata)
			MEM_freeN(psys->pdd->cdata);
		psys->pdd->cdata = NULL;

		if(psys->pdd->vdata)
			MEM_freeN(psys->pdd->vdata);
		psys->pdd->vdata = NULL;

		if(psys->pdd->ndata)
			MEM_freeN(psys->pdd->ndata);
		psys->pdd->ndata = NULL;

		if(psys->pdd->vedata)
			MEM_freeN(psys->pdd->vedata);
		psys->pdd->vedata = NULL;

		psys->pdd->totpoint = 0;
		psys->pdd->tot_vec_size = 0;
	}
}
/* free everything */
void psys_free(Object *ob, ParticleSystem * psys)
{	
	if(psys){
		int nr = 0;
		ParticleSystem * tpsys;
		
		psys_free_path_cache(psys, NULL);

		free_hair(ob, psys, 1);

		psys_free_particles(psys);

		if(psys->edit && psys->free_edit)
			psys->free_edit(psys->edit);

		if(psys->child){
			MEM_freeN(psys->child);
			psys->child = 0;
			psys->totchild = 0;
		}
		
		// check if we are last non-visible particle system
		for(tpsys=ob->particlesystem.first; tpsys; tpsys=tpsys->next){
			if(tpsys->part)
			{
				if(ELEM(tpsys->part->ren_as,PART_DRAW_OB,PART_DRAW_GR))
				{
					nr++;
					break;
				}
			}
		}
		// clear do-not-draw-flag
		if(!nr)
			ob->transflag &= ~OB_DUPLIPARTS;

		if(psys->part){
			psys->part->id.us--;		
			psys->part=0;
		}

		BKE_ptcache_free_list(&psys->ptcaches);
		psys->pointcache = NULL;
		
		BLI_freelistN(&psys->targets);

		BLI_kdtree_free(psys->tree);

		pdEndEffectors(&psys->effectors);

		if(psys->frand)
			MEM_freeN(psys->frand);

		if(psys->pdd) {
			psys_free_pdd(psys);
			MEM_freeN(psys->pdd);
		}

		MEM_freeN(psys);
	}
}

/************************************************/
/*			Rendering							*/
/************************************************/
/* these functions move away particle data and bring it back after
 * rendering, to make different render settings possible without
 * removing the previous data. this should be solved properly once */

typedef struct ParticleRenderElem {
	int curchild, totchild, reduce;
	float lambda, t, scalemin, scalemax;
} ParticleRenderElem;

typedef struct ParticleRenderData {
	ChildParticle *child;
	ParticleCacheKey **pathcache;
	ParticleCacheKey **childcache;
	ListBase pathcachebufs, childcachebufs;
	int totchild, totcached, totchildcache;
	DerivedMesh *dm;
	int totdmvert, totdmedge, totdmface;

	float mat[4][4];
	float viewmat[4][4], winmat[4][4];
	int winx, winy;

	int dosimplify;
	int timeoffset;
	ParticleRenderElem *elems;
	int *origindex;
} ParticleRenderData;

static float psys_render_viewport_falloff(double rate, float dist, float width)
{
	return pow(rate, dist/width);
}

static float psys_render_projected_area(ParticleSystem *psys, float *center, float area, double vprate, float *viewport)
{
	ParticleRenderData *data= psys->renderdata;
	float co[4], view[3], ortho1[3], ortho2[3], w, dx, dy, radius;
	
	/* transform to view space */
	VECCOPY(co, center);
	co[3]= 1.0f;
	mul_m4_v4(data->viewmat, co);
	
	/* compute two vectors orthogonal to view vector */
	VECCOPY(view, co);
	normalize_v3(view);
	ortho_basis_v3v3_v3( ortho1, ortho2,view);

	/* compute on screen minification */
	w= co[2]*data->winmat[2][3] + data->winmat[3][3];
	dx= data->winx*ortho2[0]*data->winmat[0][0];
	dy= data->winy*ortho2[1]*data->winmat[1][1];
	w= sqrt(dx*dx + dy*dy)/w;

	/* w squared because we are working with area */
	area= area*w*w;

	/* viewport of the screen test */

	/* project point on screen */
	mul_m4_v4(data->winmat, co);
	if(co[3] != 0.0f) {
		co[0]= 0.5f*data->winx*(1.0f + co[0]/co[3]);
		co[1]= 0.5f*data->winy*(1.0f + co[1]/co[3]);
	}

	/* screen space radius */
	radius= sqrt(area/M_PI);

	/* make smaller using fallof once over screen edge */
	*viewport= 1.0f;

	if(co[0]+radius < 0.0f)
		*viewport *= psys_render_viewport_falloff(vprate, -(co[0]+radius), data->winx);
	else if(co[0]-radius > data->winx)
		*viewport *= psys_render_viewport_falloff(vprate, (co[0]-radius) - data->winx, data->winx);

	if(co[1]+radius < 0.0f)
		*viewport *= psys_render_viewport_falloff(vprate, -(co[1]+radius), data->winy);
	else if(co[1]-radius > data->winy)
		*viewport *= psys_render_viewport_falloff(vprate, (co[1]-radius) - data->winy, data->winy);
	
	return area;
}

void psys_render_set(Object *ob, ParticleSystem *psys, float viewmat[][4], float winmat[][4], int winx, int winy, int timeoffset)
{
	ParticleRenderData*data;
	ParticleSystemModifierData *psmd= psys_get_modifier(ob, psys);

	if(!G.rendering)
		return;
	if(psys->renderdata)
		return;

	data= MEM_callocN(sizeof(ParticleRenderData), "ParticleRenderData");

	data->child= psys->child;
	data->totchild= psys->totchild;
	data->pathcache= psys->pathcache;
	data->pathcachebufs.first = psys->pathcachebufs.first;
	data->pathcachebufs.last = psys->pathcachebufs.last;
	data->totcached= psys->totcached;
	data->childcache= psys->childcache;
	data->childcachebufs.first = psys->childcachebufs.first;
	data->childcachebufs.last = psys->childcachebufs.last;
	data->totchildcache= psys->totchildcache;

	if(psmd->dm)
		data->dm= CDDM_copy(psmd->dm);
	data->totdmvert= psmd->totdmvert;
	data->totdmedge= psmd->totdmedge;
	data->totdmface= psmd->totdmface;

	psys->child= NULL;
	psys->pathcache= NULL;
	psys->childcache= NULL;
	psys->totchild= psys->totcached= psys->totchildcache= 0;
	psys->pathcachebufs.first = psys->pathcachebufs.last = NULL;
	psys->childcachebufs.first = psys->childcachebufs.last = NULL;

	copy_m4_m4(data->winmat, winmat);
	mul_m4_m4m4(data->viewmat, ob->obmat, viewmat);
	mul_m4_m4m4(data->mat, data->viewmat, winmat);
	data->winx= winx;
	data->winy= winy;

	data->timeoffset= timeoffset;

	psys->renderdata= data;
}

void psys_render_restore(Object *ob, ParticleSystem *psys)
{
	ParticleRenderData*data;
	ParticleSystemModifierData *psmd= psys_get_modifier(ob, psys);

	data= psys->renderdata;
	if(!data)
		return;
	
	if(data->elems)
		MEM_freeN(data->elems);

	if(psmd->dm) {
		psmd->dm->needsFree= 1;
		psmd->dm->release(psmd->dm);
	}

	psys_free_path_cache(psys, NULL);

	if(psys->child){
		MEM_freeN(psys->child);
		psys->child= 0;
		psys->totchild= 0;
	}

	psys->child= data->child;
	psys->totchild= data->totchild;
	psys->pathcache= data->pathcache;
	psys->pathcachebufs.first = data->pathcachebufs.first;
	psys->pathcachebufs.last = data->pathcachebufs.last;
	psys->totcached= data->totcached;
	psys->childcache= data->childcache;
	psys->childcachebufs.first = data->childcachebufs.first;
	psys->childcachebufs.last = data->childcachebufs.last;
	psys->totchildcache= data->totchildcache;

	psmd->dm= data->dm;
	psmd->totdmvert= data->totdmvert;
	psmd->totdmedge= data->totdmedge;
	psmd->totdmface= data->totdmface;
	psmd->flag &= ~eParticleSystemFlag_psys_updated;

	if(psmd->dm)
		psys_calc_dmcache(ob, psmd->dm, psys);

	MEM_freeN(data);
	psys->renderdata= NULL;
}

int psys_render_simplify_distribution(ParticleThreadContext *ctx, int tot)
{
	DerivedMesh *dm= ctx->dm;
	Mesh *me= (Mesh*)(ctx->sim.ob->data);
	MFace *mf, *mface;
	MVert *mvert;
	ParticleRenderData *data;
	ParticleRenderElem *elems, *elem;
	ParticleSettings *part= ctx->sim.psys->part;
	float *facearea, (*facecenter)[3], size[3], fac, powrate, scaleclamp;
	float co1[3], co2[3], co3[3], co4[3], lambda, arearatio, t, area, viewport;
	double vprate;
	int *origindex, *facetotvert;
	int a, b, totorigface, totface, newtot, skipped;

	if(part->ren_as!=PART_DRAW_PATH || !(part->draw & PART_DRAW_REN_STRAND))
		return tot;
	if(!ctx->sim.psys->renderdata)
		return tot;

	data= ctx->sim.psys->renderdata;
	if(data->timeoffset)
		return 0;
	if(!(part->simplify_flag & PART_SIMPLIFY_ENABLE))
		return tot;

	mvert= dm->getVertArray(dm);
	mface= dm->getFaceArray(dm);
	origindex= dm->getFaceDataArray(dm, CD_ORIGINDEX);
	totface= dm->getNumFaces(dm);
	totorigface= me->totface;

	if(totface == 0 || totorigface == 0)
		return tot;

	facearea= MEM_callocN(sizeof(float)*totorigface, "SimplifyFaceArea");
	facecenter= MEM_callocN(sizeof(float[3])*totorigface, "SimplifyFaceCenter");
	facetotvert= MEM_callocN(sizeof(int)*totorigface, "SimplifyFaceArea");
	elems= MEM_callocN(sizeof(ParticleRenderElem)*totorigface, "SimplifyFaceElem");

	if(data->elems)
		MEM_freeN(data->elems);

	data->dosimplify= 1;
	data->elems= elems;
	data->origindex= origindex;

	/* compute number of children per original face */
	for(a=0; a<tot; a++) {
		b= (origindex)? origindex[ctx->index[a]]: ctx->index[a];
		if(b != -1)
			elems[b].totchild++;
	}

	/* compute areas and centers of original faces */
	for(mf=mface, a=0; a<totface; a++, mf++) {
		b= (origindex)? origindex[a]: a;

		if(b != -1) {
			VECCOPY(co1, mvert[mf->v1].co);
			VECCOPY(co2, mvert[mf->v2].co);
			VECCOPY(co3, mvert[mf->v3].co);

			VECADD(facecenter[b], facecenter[b], co1);
			VECADD(facecenter[b], facecenter[b], co2);
			VECADD(facecenter[b], facecenter[b], co3);

			if(mf->v4) {
				VECCOPY(co4, mvert[mf->v4].co);
				VECADD(facecenter[b], facecenter[b], co4);
				facearea[b] += area_quad_v3(co1, co2, co3, co4);
				facetotvert[b] += 4;
			}
			else {
				facearea[b] += area_tri_v3(co1, co2, co3);
				facetotvert[b] += 3;
			}
		}
	}

	for(a=0; a<totorigface; a++)
		if(facetotvert[a] > 0)
			mul_v3_fl(facecenter[a], 1.0f/facetotvert[a]);

	/* for conversion from BU area / pixel area to reference screen size */
	mesh_get_texspace(me, 0, 0, size);
	fac= ((size[0] + size[1] + size[2])/3.0f)/part->simplify_refsize;
	fac= fac*fac;

	powrate= log(0.5f)/log(part->simplify_rate*0.5f);
	if(part->simplify_flag & PART_SIMPLIFY_VIEWPORT)
		vprate= pow(1.0 - part->simplify_viewport, 5.0);
	else
		vprate= 1.0;

	/* set simplification parameters per original face */
	for(a=0, elem=elems; a<totorigface; a++, elem++) {
		area = psys_render_projected_area(ctx->sim.psys, facecenter[a], facearea[a], vprate, &viewport);
		arearatio= fac*area/facearea[a];

		if((arearatio < 1.0f || viewport < 1.0f) && elem->totchild) {
			/* lambda is percentage of elements to keep */
			lambda= (arearatio < 1.0f)? pow(arearatio, powrate): 1.0f;
			lambda *= viewport;

			lambda= MAX2(lambda, 1.0f/elem->totchild);

			/* compute transition region */
			t= part->simplify_transition;
			elem->t= (lambda-t < 0.0f)? lambda: (lambda+t > 1.0f)? 1.0f-lambda: t;
			elem->reduce= 1;

			/* scale at end and beginning of the transition region */
			elem->scalemax= (lambda+t < 1.0f)? 1.0f/lambda: 1.0f/(1.0f - elem->t*elem->t/t);
			elem->scalemin= (lambda+t < 1.0f)? 0.0f: elem->scalemax*(1.0f-elem->t/t);

			elem->scalemin= sqrt(elem->scalemin);
			elem->scalemax= sqrt(elem->scalemax);

			/* clamp scaling */
			scaleclamp= MIN2(elem->totchild, 10.0f);
			elem->scalemin= MIN2(scaleclamp, elem->scalemin);
			elem->scalemax= MIN2(scaleclamp, elem->scalemax);

			/* extend lambda to include transition */
			lambda= lambda + elem->t;
			if(lambda > 1.0f)
				lambda= 1.0f;
		}
		else {
			lambda= arearatio;

			elem->scalemax= 1.0f; //sqrt(lambda);
			elem->scalemin= 1.0f; //sqrt(lambda);
			elem->reduce= 0;
		}

		elem->lambda= lambda;
		elem->scalemin= sqrt(elem->scalemin);
		elem->scalemax= sqrt(elem->scalemax);
		elem->curchild= 0;
	}

	MEM_freeN(facearea);
	MEM_freeN(facecenter);
	MEM_freeN(facetotvert);

	/* move indices and set random number skipping */
	ctx->skip= MEM_callocN(sizeof(int)*tot, "SimplificationSkip");

	skipped= 0;
	for(a=0, newtot=0; a<tot; a++) {
		b= (origindex)? origindex[ctx->index[a]]: ctx->index[a];
		if(b != -1) {
			if(elems[b].curchild++ < ceil(elems[b].lambda*elems[b].totchild)) {
				ctx->index[newtot]= ctx->index[a];
				ctx->skip[newtot]= skipped;
				skipped= 0;
				newtot++;
			}
			else skipped++;
		}
		else skipped++;
	}

	for(a=0, elem=elems; a<totorigface; a++, elem++)
		elem->curchild= 0;

	return newtot;
}

int psys_render_simplify_params(ParticleSystem *psys, ChildParticle *cpa, float *params)
{
	ParticleRenderData *data;
	ParticleRenderElem *elem;
	float x, w, scale, alpha, lambda, t, scalemin, scalemax;
	int b;

	if(!(psys->renderdata && (psys->part->simplify_flag & PART_SIMPLIFY_ENABLE)))
		return 0;
	
	data= psys->renderdata;
	if(!data->dosimplify)
		return 0;
	
	b= (data->origindex)? data->origindex[cpa->num]: cpa->num;
	if(b == -1)
		return 0;

	elem= &data->elems[b];

	lambda= elem->lambda;
	t= elem->t;
	scalemin= elem->scalemin;
	scalemax= elem->scalemax;

	if(!elem->reduce) {
		scale= scalemin;
		alpha= 1.0f;
	}
	else {
		x= (elem->curchild+0.5f)/elem->totchild;
		if(x < lambda-t) {
			scale= scalemax;
			alpha= 1.0f;
		}
		else if(x >= lambda+t) {
			scale= scalemin;
			alpha= 0.0f;
		}
		else {
			w= (lambda+t - x)/(2.0f*t);
			scale= scalemin + (scalemax - scalemin)*w;
			alpha= w;
		}
	}

	params[0]= scale;
	params[1]= alpha;

	elem->curchild++;

	return 1;
}

/************************************************/
/*			Interpolation						*/
/************************************************/
static float interpolate_particle_value(float v1, float v2, float v3, float v4, float *w, int four)
{
	float value;

	value= w[0]*v1 + w[1]*v2 + w[2]*v3;
	if(four)
		value += w[3]*v4;
	
	return value;
}
static void weighted_particle_vector(float *v1, float *v2, float *v3, float *v4, float *weights, float *vec)
{
	vec[0]= weights[0]*v1[0] + weights[1]*v2[0] + weights[2]*v3[0] + weights[3]*v4[0];
	vec[1]= weights[0]*v1[1] + weights[1]*v2[1] + weights[2]*v3[1] + weights[3]*v4[1];
	vec[2]= weights[0]*v1[2] + weights[1]*v2[2] + weights[2]*v3[2] + weights[3]*v4[2];
}
void psys_interpolate_particle(short type, ParticleKey keys[4], float dt, ParticleKey *result, int velocity)
{
	float t[4];

	if(type<0) {
		interp_cubic_v3( result->co, result->vel,keys[1].co, keys[1].vel, keys[2].co, keys[2].vel, dt);
	}
	else {
		key_curve_position_weights(dt, t, type);

		weighted_particle_vector(keys[0].co, keys[1].co, keys[2].co, keys[3].co, t, result->co);

		if(velocity){
			float temp[3];

			if(dt>0.999f){
				key_curve_position_weights(dt-0.001f, t, type);
				weighted_particle_vector(keys[0].co, keys[1].co, keys[2].co, keys[3].co, t, temp);
				VECSUB(result->vel, result->co, temp);
			}
			else{
				key_curve_position_weights(dt+0.001f, t, type);
				weighted_particle_vector(keys[0].co, keys[1].co, keys[2].co, keys[3].co, t, temp);
				VECSUB(result->vel, temp, result->co);
			}
		}
	}
}



typedef struct ParticleInterpolationData {
	HairKey *hkey[2];

	DerivedMesh *dm;
	MVert *mvert[2];

	int keyed;
	ParticleKey *kkey[2];

	PointCache *cache;

	PTCacheEditPoint *epoint;
	PTCacheEditKey *ekey[2];

	float birthtime, dietime;
	int bspline;
} ParticleInterpolationData;
/* Assumes pointcache->mem_cache exists, so for disk cached particles call psys_make_temp_pointcache() before use */
static void get_pointcache_keys_for_time(Object *ob, PointCache *cache, int index, float t, ParticleKey *key1, ParticleKey *key2)
{
	static PTCacheMem *pm = NULL; /* not thread safe */

	if(index < 0) { /* initialize */
		pm = cache->mem_cache.first;

		if(pm)
			pm = pm->next;
	}
	else {
		if(pm) {
			while(pm && pm->next && (float)pm->frame < t)
				pm = pm->next;

			BKE_ptcache_make_particle_key(key2, pm->index_array ? pm->index_array[index] : index, pm->data, (float)pm->frame);
			BKE_ptcache_make_particle_key(key1, pm->prev->index_array ? pm->prev->index_array[index] : index, pm->prev->data, (float)pm->prev->frame);
		}
		else if(cache->mem_cache.first) {
			PTCacheMem *pm2 = cache->mem_cache.first;
			BKE_ptcache_make_particle_key(key2, pm2->index_array ? pm2->index_array[index] : index, pm2->data, (float)pm2->frame);
			copy_particle_key(key1, key2, 1);
		}
	}
}
static void init_particle_interpolation(Object *ob, ParticleSystem *psys, ParticleData *pa, ParticleInterpolationData *pind)
{

	if(pind->epoint) {
		PTCacheEditPoint *point = pind->epoint;

		pind->ekey[0] = point->keys;
		pind->ekey[1] = point->totkey > 1 ? point->keys + 1 : NULL;

		pind->birthtime = *(point->keys->time);
		pind->dietime = *((point->keys + point->totkey - 1)->time);
	}
	else if(pind->keyed) {
		ParticleKey *key = pa->keys;
		pind->kkey[0] = key;
		pind->kkey[1] = pa->totkey > 1 ? key + 1 : NULL;

		pind->birthtime = key->time;
		pind->dietime = (key + pa->totkey - 1)->time;
	}
	else if(pind->cache) {
		get_pointcache_keys_for_time(ob, pind->cache, -1, 0.0f, NULL, NULL);

		pind->birthtime = pa ? pa->time : pind->cache->startframe;
		pind->dietime = pa ? pa->dietime : pind->cache->endframe;
	}
	else {
		HairKey *key = pa->hair;
		pind->hkey[0] = key;
		pind->hkey[1] = key + 1;

		pind->birthtime = key->time;
		pind->dietime = (key + pa->totkey - 1)->time;

		if(pind->dm) {
			pind->mvert[0] = CDDM_get_vert(pind->dm, pa->hair_index);
			pind->mvert[1] = pind->mvert[0] + 1;
		}
	}
}
static void edit_to_particle(ParticleKey *key, PTCacheEditKey *ekey)
{
	VECCOPY(key->co, ekey->co);
	if(ekey->vel) {
		VECCOPY(key->vel, ekey->vel);
	}
	key->time = *(ekey->time);
}
static void hair_to_particle(ParticleKey *key, HairKey *hkey)
{
	VECCOPY(key->co, hkey->co);
	key->time = hkey->time;
}

static void mvert_to_particle(ParticleKey *key, MVert *mvert, HairKey *hkey)
{
	VECCOPY(key->co, mvert->co);
	key->time = hkey->time;
}

static void do_particle_interpolation(ParticleSystem *psys, int p, ParticleData *pa, float t, float frs_sec, ParticleInterpolationData *pind, ParticleKey *result)
{
	PTCacheEditPoint *point = pind->epoint;
	ParticleKey keys[4];
	int point_vel = (point && point->keys->vel);
	float real_t, dfra, keytime;

	/* interpret timing and find keys */
	if(point) {
		if(result->time < 0.0f)
			real_t = -result->time;
		else
			real_t = *(pind->ekey[0]->time) + t * (*(pind->ekey[0][point->totkey-1].time) - *(pind->ekey[0]->time));

		while(*(pind->ekey[1]->time) < real_t)
			pind->ekey[1]++;

		pind->ekey[0] = pind->ekey[1] - 1;
	}
	else if(pind->keyed) {
		/* we have only one key, so let's use that */
		if(pind->kkey[1]==NULL) {
			copy_particle_key(result, pind->kkey[0], 1);
			return;
		}

		if(result->time < 0.0f)
			real_t = -result->time;
		else
			real_t = pind->kkey[0]->time + t * (pind->kkey[0][pa->totkey-1].time - pind->kkey[0]->time);

		if(psys->part->phystype==PART_PHYS_KEYED && psys->flag & PSYS_KEYED_TIMING) {
			ParticleTarget *pt = psys->targets.first;

			pt=pt->next;

			while(pt && pa->time + pt->time < real_t)
				pt= pt->next;

			if(pt) {
				pt=pt->prev;

				if(pa->time + pt->time + pt->duration > real_t)
					real_t = pa->time + pt->time;
			}
			else
				real_t = pa->time + ((ParticleTarget*)psys->targets.last)->time;
		}

		CLAMP(real_t, pa->time, pa->dietime);

		while(pind->kkey[1]->time < real_t)
			pind->kkey[1]++;
		
		pind->kkey[0] = pind->kkey[1] - 1;
	}
	else if(pind->cache) {
		if(result->time < 0.0f) /* flag for time in frames */
			real_t = -result->time;
		else
			real_t = pa->time + t * (pa->dietime - pa->time);
	}
	else {
		if(result->time < 0.0f)
			real_t = -result->time;
		else
			real_t = pind->hkey[0]->time + t * (pind->hkey[0][pa->totkey-1].time - pind->hkey[0]->time);

		while(pind->hkey[1]->time < real_t) {
			pind->hkey[1]++;
			pind->mvert[1]++;
		}

		pind->hkey[0] = pind->hkey[1] - 1;
	}

	/* set actual interpolation keys */
	if(point) {
		edit_to_particle(keys + 1, pind->ekey[0]);
		edit_to_particle(keys + 2, pind->ekey[1]);
	}
	else if(pind->dm) {
		pind->mvert[0] = pind->mvert[1] - 1;
		mvert_to_particle(keys + 1, pind->mvert[0], pind->hkey[0]);
		mvert_to_particle(keys + 2, pind->mvert[1], pind->hkey[1]);
	}
	else if(pind->keyed) {
		memcpy(keys + 1, pind->kkey[0], sizeof(ParticleKey));
		memcpy(keys + 2, pind->kkey[1], sizeof(ParticleKey));
	}
	else if(pind->cache) {
		get_pointcache_keys_for_time(NULL, pind->cache, p, real_t, keys+1, keys+2);
	}
	else {
		hair_to_particle(keys + 1, pind->hkey[0]);
		hair_to_particle(keys + 2, pind->hkey[1]);
	}

	/* set secondary interpolation keys for hair */
	if(!pind->keyed && !pind->cache && !point_vel) {
		if(point) {
			if(pind->ekey[0] != point->keys)
				edit_to_particle(keys, pind->ekey[0] - 1);
			else
				edit_to_particle(keys, pind->ekey[0]);
		}
		else if(pind->dm) {
			if(pind->hkey[0] != pa->hair)
				mvert_to_particle(keys, pind->mvert[0] - 1, pind->hkey[0] - 1);
			else
				mvert_to_particle(keys, pind->mvert[0], pind->hkey[0]);
		}
		else {
			if(pind->hkey[0] != pa->hair)
				hair_to_particle(keys, pind->hkey[0] - 1);
			else
				hair_to_particle(keys, pind->hkey[0]);
		}

		if(point) {
			if(pind->ekey[1] != point->keys + point->totkey - 1)
				edit_to_particle(keys + 3, pind->ekey[1] + 1);
			else
				edit_to_particle(keys + 3, pind->ekey[1]);
		}
		else if(pind->dm) {
			if(pind->hkey[1] != pa->hair + pa->totkey - 1)
				mvert_to_particle(keys + 3, pind->mvert[1] + 1, pind->hkey[1] + 1);
			else
				mvert_to_particle(keys + 3, pind->mvert[1], pind->hkey[1]);
		}
		else {
			if(pind->hkey[1] != pa->hair + pa->totkey - 1)
				hair_to_particle(keys + 3, pind->hkey[1] + 1);
			else
				hair_to_particle(keys + 3, pind->hkey[1]);
		}
	}

	dfra = keys[2].time - keys[1].time;
	keytime = (real_t - keys[1].time) / dfra;

	/* convert velocity to timestep size */
	if(pind->keyed || pind->cache || point_vel){
		mul_v3_fl(keys[1].vel, dfra / frs_sec);
		mul_v3_fl(keys[2].vel, dfra / frs_sec);
		interp_qt_qtqt(result->rot,keys[1].rot,keys[2].rot,keytime);
	}

	/* now we should have in chronologiacl order k1<=k2<=t<=k3<=k4 with keytime between [0,1]->[k2,k3] (k1 & k4 used for cardinal & bspline interpolation)*/
	psys_interpolate_particle((pind->keyed || pind->cache || point_vel) ? -1 /* signal for cubic interpolation */
		: (pind->bspline ? KEY_BSPLINE : KEY_CARDINAL)
		,keys, keytime, result, 1);

	/* the velocity needs to be converted back from cubic interpolation */
	if(pind->keyed || pind->cache || point_vel)
		mul_v3_fl(result->vel, frs_sec / dfra);
}
/************************************************/
/*			Particles on a dm					*/
/************************************************/
/* interpolate a location on a face based on face coordinates */
void psys_interpolate_face(MVert *mvert, MFace *mface, MTFace *tface, float (*orcodata)[3], float *w, float *vec, float *nor, float *utan, float *vtan, float *orco,float *ornor){
	float *v1=0, *v2=0, *v3=0, *v4=0;
	float e1[3],e2[3],s1,s2,t1,t2;
	float *uv1, *uv2, *uv3, *uv4;
	float n1[3], n2[3], n3[3], n4[3];
	float tuv[4][2];
	float *o1, *o2, *o3, *o4;

	v1= (mvert+mface->v1)->co;
	v2= (mvert+mface->v2)->co;
	v3= (mvert+mface->v3)->co;
	VECCOPY(n1,(mvert+mface->v1)->no);
	VECCOPY(n2,(mvert+mface->v2)->no);
	VECCOPY(n3,(mvert+mface->v3)->no);
	normalize_v3(n1);
	normalize_v3(n2);
	normalize_v3(n3);

	if(mface->v4) {
		v4= (mvert+mface->v4)->co;
		VECCOPY(n4,(mvert+mface->v4)->no);
		normalize_v3(n4);
		
		vec[0]= w[0]*v1[0] + w[1]*v2[0] + w[2]*v3[0] + w[3]*v4[0];
		vec[1]= w[0]*v1[1] + w[1]*v2[1] + w[2]*v3[1] + w[3]*v4[1];
		vec[2]= w[0]*v1[2] + w[1]*v2[2] + w[2]*v3[2] + w[3]*v4[2];

		if(nor){
			if(mface->flag & ME_SMOOTH){
				nor[0]= w[0]*n1[0] + w[1]*n2[0] + w[2]*n3[0] + w[3]*n4[0];
				nor[1]= w[0]*n1[1] + w[1]*n2[1] + w[2]*n3[1] + w[3]*n4[1];
				nor[2]= w[0]*n1[2] + w[1]*n2[2] + w[2]*n3[2] + w[3]*n4[2];
			}
			else
				normal_quad_v3(nor,v1,v2,v3,v4);
		}
	}
	else {
		vec[0]= w[0]*v1[0] + w[1]*v2[0] + w[2]*v3[0];
		vec[1]= w[0]*v1[1] + w[1]*v2[1] + w[2]*v3[1];
		vec[2]= w[0]*v1[2] + w[1]*v2[2] + w[2]*v3[2];
		
		if(nor){
			if(mface->flag & ME_SMOOTH){
				nor[0]= w[0]*n1[0] + w[1]*n2[0] + w[2]*n3[0];
				nor[1]= w[0]*n1[1] + w[1]*n2[1] + w[2]*n3[1];
				nor[2]= w[0]*n1[2] + w[1]*n2[2] + w[2]*n3[2];
			}
			else
				normal_tri_v3(nor,v1,v2,v3);
		}
	}
	
	/* calculate tangent vectors */
	if(utan && vtan){
		if(tface){
			uv1= tface->uv[0];
			uv2= tface->uv[1];
			uv3= tface->uv[2];
			uv4= tface->uv[3];
		}
		else{
			uv1= tuv[0]; uv2= tuv[1]; uv3= tuv[2]; uv4= tuv[3];
			map_to_sphere( uv1, uv1+1,v1[0], v1[1], v1[2]);
			map_to_sphere( uv2, uv2+1,v2[0], v2[1], v2[2]);
			map_to_sphere( uv3, uv3+1,v3[0], v3[1], v3[2]);
			if(v4)
				map_to_sphere( uv4, uv4+1,v4[0], v4[1], v4[2]);
		}

		if(v4){
			s1= uv3[0] - uv1[0];
			s2= uv4[0] - uv1[0];

			t1= uv3[1] - uv1[1];
			t2= uv4[1] - uv1[1];

			sub_v3_v3v3(e1, v3, v1);
			sub_v3_v3v3(e2, v4, v1);
		}
		else{
			s1= uv2[0] - uv1[0];
			s2= uv3[0] - uv1[0];

			t1= uv2[1] - uv1[1];
			t2= uv3[1] - uv1[1];

			sub_v3_v3v3(e1, v2, v1);
			sub_v3_v3v3(e2, v3, v1);
		}

		vtan[0] = (s1*e2[0] - s2*e1[0]);
		vtan[1] = (s1*e2[1] - s2*e1[1]);
		vtan[2] = (s1*e2[2] - s2*e1[2]);

		utan[0] = (t1*e2[0] - t2*e1[0]);
		utan[1] = (t1*e2[1] - t2*e1[1]);
		utan[2] = (t1*e2[2] - t2*e1[2]);
	}

	if(orco) {
		if(orcodata) {
			o1= orcodata[mface->v1];
			o2= orcodata[mface->v2];
			o3= orcodata[mface->v3];

			if(mface->v4) {
				o4= orcodata[mface->v4];
				orco[0]= w[0]*o1[0] + w[1]*o2[0] + w[2]*o3[0] + w[3]*o4[0];
				orco[1]= w[0]*o1[1] + w[1]*o2[1] + w[2]*o3[1] + w[3]*o4[1];
				orco[2]= w[0]*o1[2] + w[1]*o2[2] + w[2]*o3[2] + w[3]*o4[2];

				if(ornor)
					normal_quad_v3( ornor,o1, o2, o3, o4);
			}
			else {
				orco[0]= w[0]*o1[0] + w[1]*o2[0] + w[2]*o3[0];
				orco[1]= w[0]*o1[1] + w[1]*o2[1] + w[2]*o3[1];
				orco[2]= w[0]*o1[2] + w[1]*o2[2] + w[2]*o3[2];

				if(ornor)
					normal_tri_v3( ornor,o1, o2, o3);
			}
		}
		else {
			VECCOPY(orco, vec);
			if(ornor)
				VECCOPY(ornor, nor);
		}
	}
}
void psys_interpolate_uvs(MTFace *tface, int quad, float *w, float *uvco)
{
	float v10= tface->uv[0][0];
	float v11= tface->uv[0][1];
	float v20= tface->uv[1][0];
	float v21= tface->uv[1][1];
	float v30= tface->uv[2][0];
	float v31= tface->uv[2][1];
	float v40,v41;

	if(quad) {
		v40= tface->uv[3][0];
		v41= tface->uv[3][1];

		uvco[0]= w[0]*v10 + w[1]*v20 + w[2]*v30 + w[3]*v40;
		uvco[1]= w[0]*v11 + w[1]*v21 + w[2]*v31 + w[3]*v41;
	}
	else {
		uvco[0]= w[0]*v10 + w[1]*v20 + w[2]*v30;
		uvco[1]= w[0]*v11 + w[1]*v21 + w[2]*v31;
	}
}

void psys_interpolate_mcol(MCol *mcol, int quad, float *w, MCol *mc)
{
	char *cp, *cp1, *cp2, *cp3, *cp4;

	cp= (char *)mc;
	cp1= (char *)&mcol[0];
	cp2= (char *)&mcol[1];
	cp3= (char *)&mcol[2];
	
	if(quad) {
		cp4= (char *)&mcol[3];

		cp[0]= (int)(w[0]*cp1[0] + w[1]*cp2[0] + w[2]*cp3[0] + w[3]*cp4[0]);
		cp[1]= (int)(w[0]*cp1[1] + w[1]*cp2[1] + w[2]*cp3[1] + w[3]*cp4[1]);
		cp[2]= (int)(w[0]*cp1[2] + w[1]*cp2[2] + w[2]*cp3[2] + w[3]*cp4[2]);
		cp[3]= (int)(w[0]*cp1[3] + w[1]*cp2[3] + w[2]*cp3[3] + w[3]*cp4[3]);
	}
	else {
		cp[0]= (int)(w[0]*cp1[0] + w[1]*cp2[0] + w[2]*cp3[0]);
		cp[1]= (int)(w[0]*cp1[1] + w[1]*cp2[1] + w[2]*cp3[1]);
		cp[2]= (int)(w[0]*cp1[2] + w[1]*cp2[2] + w[2]*cp3[2]);
		cp[3]= (int)(w[0]*cp1[3] + w[1]*cp2[3] + w[2]*cp3[3]);
	}
}

static float psys_interpolate_value_from_verts(DerivedMesh *dm, short from, int index, float *fw, float *values)
{
	if(values==0 || index==-1)
		return 0.0;

	switch(from){
		case PART_FROM_VERT:
			return values[index];
		case PART_FROM_FACE:
		case PART_FROM_VOLUME:
		{
			MFace *mf=dm->getFaceData(dm,index,CD_MFACE);
			return interpolate_particle_value(values[mf->v1],values[mf->v2],values[mf->v3],values[mf->v4],fw,mf->v4);
		}
			
	}
	return 0.0;
}

/* conversion of pa->fw to origspace layer coordinates */
static void psys_w_to_origspace(float *w, float *uv)
{
	uv[0]= w[1] + w[2];
	uv[1]= w[2] + w[3];
}

/* conversion of pa->fw to weights in face from origspace */
static void psys_origspace_to_w(OrigSpaceFace *osface, int quad, float *w, float *neww)
{
	float v[4][3], co[3];

	v[0][0]= osface->uv[0][0]; v[0][1]= osface->uv[0][1]; v[0][2]= 0.0f;
	v[1][0]= osface->uv[1][0]; v[1][1]= osface->uv[1][1]; v[1][2]= 0.0f;
	v[2][0]= osface->uv[2][0]; v[2][1]= osface->uv[2][1]; v[2][2]= 0.0f;

	psys_w_to_origspace(w, co);
	co[2]= 0.0f;
	
	if(quad) {
		v[3][0]= osface->uv[3][0]; v[3][1]= osface->uv[3][1]; v[3][2]= 0.0f;
		interp_weights_poly_v3( neww,v, 4, co);
	}
	else {
		interp_weights_poly_v3( neww,v, 3, co);
		neww[3]= 0.0f;
	}
}

/* find the derived mesh face for a particle, set the mf passed. this is slow
 * and can be optimized but only for many lookups. returns the face index. */
int psys_particle_dm_face_lookup(Object *ob, DerivedMesh *dm, int index, float *fw, struct LinkNode *node)
{
	Mesh *me= (Mesh*)ob->data;
	MFace *mface;
	OrigSpaceFace *osface;
	int *origindex;
	int quad, findex, totface;
	float uv[2], (*faceuv)[2];

	mface = dm->getFaceDataArray(dm, CD_MFACE);
	origindex = dm->getFaceDataArray(dm, CD_ORIGINDEX);
	osface = dm->getFaceDataArray(dm, CD_ORIGSPACE);

	totface = dm->getNumFaces(dm);
	
	if(osface==NULL || origindex==NULL) {
		/* Assume we dont need osface data */
		if (index <totface) {
			//printf("\tNO CD_ORIGSPACE, assuming not needed\n");
			return index;
		} else {
			printf("\tNO CD_ORIGSPACE, error out of range\n");
			return DMCACHE_NOTFOUND;
		}
	}
	else if(index >= me->totface)
		return DMCACHE_NOTFOUND; /* index not in the original mesh */

	psys_w_to_origspace(fw, uv);
	
	if(node) { /* we have a linked list of faces that we use, faster! */
		for(;node; node=node->next) {
			findex= GET_INT_FROM_POINTER(node->link);
			faceuv= osface[findex].uv;
			quad= mface[findex].v4;

			/* check that this intersects - Its possible this misses :/ -
			 * could also check its not between */
			if(quad) {
				if(isect_point_quad_v2(uv, faceuv[0], faceuv[1], faceuv[2], faceuv[3]))
					return findex;
			}
			else if(isect_point_tri_v2(uv, faceuv[0], faceuv[1], faceuv[2]))
				return findex;
		}
	}
	else { /* if we have no node, try every face */
		for(findex=0; findex<totface; findex++) {
			if(origindex[findex] == index) {
				faceuv= osface[findex].uv;
				quad= mface[findex].v4;

				/* check that this intersects - Its possible this misses :/ -
				 * could also check its not between */
				if(quad) {
					if(isect_point_quad_v2(uv, faceuv[0], faceuv[1], faceuv[2], faceuv[3]))
						return findex;
				}
				else if(isect_point_tri_v2(uv, faceuv[0], faceuv[1], faceuv[2]))
					return findex;
			}
		}
	}

	return DMCACHE_NOTFOUND;
}

static int psys_map_index_on_dm(DerivedMesh *dm, int from, int index, int index_dmcache, float *fw, float foffset, int *mapindex, float *mapfw)
{
	if(index < 0)
		return 0;

	if (dm->deformedOnly || index_dmcache == DMCACHE_ISCHILD) {
		/* for meshes that are either only defined or for child particles, the
		 * index and fw do not require any mapping, so we can directly use it */
		if(from == PART_FROM_VERT) {
			if(index >= dm->getNumVerts(dm))
				return 0;

			*mapindex = index;
		}
		else  { /* FROM_FACE/FROM_VOLUME */
			if(index >= dm->getNumFaces(dm))
				return 0;

			*mapindex = index;
			QUATCOPY(mapfw, fw);
		}
	} else {
		/* for other meshes that have been modified, we try to map the particle
		 * to their new location, which means a different index, and for faces
		 * also a new face interpolation weights */
		if(from == PART_FROM_VERT) {
			if (index_dmcache == DMCACHE_NOTFOUND || index_dmcache > dm->getNumVerts(dm))
				return 0;

			*mapindex = index_dmcache;
		}
		else  { /* FROM_FACE/FROM_VOLUME */
			/* find a face on the derived mesh that uses this face */
			MFace *mface;
			OrigSpaceFace *osface;
			int i;

			i = index_dmcache;

			if(i== DMCACHE_NOTFOUND || i >= dm->getNumFaces(dm))
				return 0;

			*mapindex = i;

			/* modify the original weights to become
			 * weights for the derived mesh face */
			osface= dm->getFaceDataArray(dm, CD_ORIGSPACE);
			mface= dm->getFaceData(dm, i, CD_MFACE);

			if(osface == NULL)
				mapfw[0]= mapfw[1]= mapfw[2]= mapfw[3]= 0.0f;
			else
				psys_origspace_to_w(&osface[i], mface->v4, fw, mapfw);
		}
	}

	return 1;
}

/* interprets particle data to get a point on a mesh in object space */
void psys_particle_on_dm(DerivedMesh *dm, int from, int index, int index_dmcache, float *fw, float foffset, float *vec, float *nor, float *utan, float *vtan, float *orco, float *ornor)
{
	float tmpnor[3], mapfw[4];
	float (*orcodata)[3];
	int mapindex;

	if(!psys_map_index_on_dm(dm, from, index, index_dmcache, fw, foffset, &mapindex, mapfw)) {
		if(vec) { vec[0]=vec[1]=vec[2]=0.0; }
		if(nor) { nor[0]=nor[1]=0.0; nor[2]=1.0; }
		if(orco) { orco[0]=orco[1]=orco[2]=0.0; }
		if(ornor) { ornor[0]=ornor[1]=0.0; ornor[2]=1.0; }
		if(utan) { utan[0]=utan[1]=utan[2]=0.0; }
		if(vtan) { vtan[0]=vtan[1]=vtan[2]=0.0; }

		return;
	}

	orcodata= dm->getVertDataArray(dm, CD_ORCO);

	if(from == PART_FROM_VERT) {
		dm->getVertCo(dm,mapindex,vec);

		if(nor) {
			dm->getVertNo(dm,mapindex,nor);
			normalize_v3(nor);
		}

		if(orco)
			VECCOPY(orco, orcodata[mapindex])

		if(ornor) {
			dm->getVertNo(dm,mapindex,nor);
			normalize_v3(nor);
		}

		if(utan && vtan) {
			utan[0]= utan[1]= utan[2]= 0.0f;
			vtan[0]= vtan[1]= vtan[2]= 0.0f;
		}
	}
	else { /* PART_FROM_FACE / PART_FROM_VOLUME */
		MFace *mface;
		MTFace *mtface;
		MVert *mvert;

		mface=dm->getFaceData(dm,mapindex,CD_MFACE);
		mvert=dm->getVertDataArray(dm,CD_MVERT);
		mtface=CustomData_get_layer(&dm->faceData,CD_MTFACE);

		if(mtface)
			mtface += mapindex;

		if(from==PART_FROM_VOLUME) {
			psys_interpolate_face(mvert,mface,mtface,orcodata,mapfw,vec,tmpnor,utan,vtan,orco,ornor);
			if(nor)
				VECCOPY(nor,tmpnor);

			normalize_v3(tmpnor);
			mul_v3_fl(tmpnor,-foffset);
			VECADD(vec,vec,tmpnor);
		}
		else
			psys_interpolate_face(mvert,mface,mtface,orcodata,mapfw,vec,nor,utan,vtan,orco,ornor);
	}
}

float psys_particle_value_from_verts(DerivedMesh *dm, short from, ParticleData *pa, float *values)
{
	float mapfw[4];
	int mapindex;

	if(!psys_map_index_on_dm(dm, from, pa->num, pa->num_dmcache, pa->fuv, pa->foffset, &mapindex, mapfw))
		return 0.0f;
	
	return psys_interpolate_value_from_verts(dm, from, mapindex, mapfw, values);
}

ParticleSystemModifierData *psys_get_modifier(Object *ob, ParticleSystem *psys)
{
	ModifierData *md;
	ParticleSystemModifierData *psmd;

	for(md=ob->modifiers.first; md; md=md->next){
		if(md->type==eModifierType_ParticleSystem){
			psmd= (ParticleSystemModifierData*) md;
			if(psmd->psys==psys){
				return psmd;
			}
		}
	}
	return NULL;
}
/************************************************/
/*			Particles on a shape				*/
/************************************************/
/* ready for future use */
static void psys_particle_on_shape(int distr, int index, float *fuv, float *vec, float *nor, float *utan, float *vtan, float *orco, float *ornor)
{
	/* TODO */
	float zerovec[3]={0.0f,0.0f,0.0f};
	if(vec){
		VECCOPY(vec,zerovec);
	}
	if(nor){
		VECCOPY(nor,zerovec);
	}
	if(utan){
		VECCOPY(utan,zerovec);
	}
	if(vtan){
		VECCOPY(vtan,zerovec);
	}
	if(orco){
		VECCOPY(orco,zerovec);
	}
	if(ornor){
		VECCOPY(ornor,zerovec);
	}
}
/************************************************/
/*			Particles on emitter				*/
/************************************************/
void psys_particle_on_emitter(ParticleSystemModifierData *psmd, int from, int index, int index_dmcache, float *fuv, float foffset, float *vec, float *nor, float *utan, float *vtan, float *orco, float *ornor){
	if(psmd){
		if(psmd->psys->part->distr==PART_DISTR_GRID && psmd->psys->part->from != PART_FROM_VERT){
			if(vec){
				VECCOPY(vec,fuv);
			}
			return;
		}
		/* we cant use the num_dmcache */
		psys_particle_on_dm(psmd->dm,from,index,index_dmcache,fuv,foffset,vec,nor,utan,vtan,orco,ornor);
	}
	else
		psys_particle_on_shape(from,index,fuv,vec,nor,utan,vtan,orco,ornor);

}
/************************************************/
/*			Path Cache							*/
/************************************************/
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

static void do_prekink(ParticleKey *state, ParticleKey *par, float *par_rot, float time, float freq, float shape, float amplitude, short type, short axis, float obmat[][4])
{
	float vec[3]={0.0,0.0,0.0}, q1[4]={1,0,0,0},q2[4];
	float t;

	CLAMP(time,0.0,1.0);

	if(shape!=0.0f && type!=PART_KINK_BRAID) {
		if(shape<0.0f)
			time= (float)pow(time, 1.0+shape);
		else
			time= (float)pow(time, 1.0/(1.0-shape));
	}

	t=time;

	t*=(float)M_PI*freq;

	if(par==0) return;

	switch(type){
		case PART_KINK_CURL:
			vec[axis]=1.0;
			if(par_rot)
				QUATCOPY(q2,par_rot)
			else
				vec_to_quat( q2,par->vel,axis,(axis+1)%3);
			mul_qt_v3(q2,vec);
			mul_v3_fl(vec,amplitude);
			VECADD(state->co,state->co,vec);

			VECSUB(vec,state->co,par->co);

			if(t!=0.0)
				axis_angle_to_quat(q1,par->vel,t);
			
			mul_qt_v3(q1,vec);
			
			VECADD(state->co,par->co,vec);
			break;
		case PART_KINK_RADIAL:
			VECSUB(vec,state->co,par->co);

			normalize_v3(vec);
			mul_v3_fl(vec,amplitude*(float)sin(t));

			VECADD(state->co,state->co,vec);
			break;
		case PART_KINK_WAVE:
			vec[axis]=1.0;
			if(obmat)
				mul_mat3_m4_v3(obmat,vec);

			if(par_rot)
				mul_qt_v3(par_rot,vec);

			project_v3_v3v3(q1,vec,par->vel);
			
			VECSUB(vec,vec,q1);
			normalize_v3(vec);

			mul_v3_fl(vec,amplitude*(float)sin(t));

			VECADD(state->co,state->co,vec);
			break;
		case PART_KINK_BRAID:
			if(par){
				float y_vec[3]={0.0,1.0,0.0};
				float z_vec[3]={0.0,0.0,1.0};
				float vec_from_par[3], vec_one[3], radius, state_co[3];
				float inp_y,inp_z,length;
				
				if(par_rot)
					QUATCOPY(q2,par_rot)
				else
					vec_to_quat(q2,par->vel,axis,(axis+1)%3);
				mul_qt_v3(q2,y_vec);
				mul_qt_v3(q2,z_vec);
				
				VECSUB(vec_from_par,state->co,par->co);
				VECCOPY(vec_one,vec_from_par);
				radius=normalize_v3(vec_one);

				inp_y=dot_v3v3(y_vec,vec_one);
				inp_z=dot_v3v3(z_vec,vec_one);

				if(inp_y>0.5){
					VECCOPY(state_co,y_vec);

					mul_v3_fl(y_vec,amplitude*(float)cos(t));
					mul_v3_fl(z_vec,amplitude/2.0f*(float)sin(2.0f*t));
				}
				else if(inp_z>0.0){
					VECCOPY(state_co,z_vec);
					mul_v3_fl(state_co,(float)sin(M_PI/3.0f));
					VECADDFAC(state_co,state_co,y_vec,-0.5f);

					mul_v3_fl(y_vec,-amplitude*(float)cos(t + M_PI/3.0f));
					mul_v3_fl(z_vec,amplitude/2.0f*(float)cos(2.0f*t + M_PI/6.0f));
				}
				else{
					VECCOPY(state_co,z_vec);
					mul_v3_fl(state_co,-(float)sin(M_PI/3.0f));
					VECADDFAC(state_co,state_co,y_vec,-0.5f);

					mul_v3_fl(y_vec,amplitude*(float)-sin(t+M_PI/6.0f));
					mul_v3_fl(z_vec,amplitude/2.0f*(float)-sin(2.0f*t+M_PI/3.0f));
				}

				mul_v3_fl(state_co,amplitude);
				VECADD(state_co,state_co,par->co);
				VECSUB(vec_from_par,state->co,state_co);

				length=normalize_v3(vec_from_par);
				mul_v3_fl(vec_from_par,MIN2(length,amplitude/2.0f));

				VECADD(state_co,par->co,y_vec);
				VECADD(state_co,state_co,z_vec);
				VECADD(state_co,state_co,vec_from_par);

				shape=(2.0f*(float)M_PI)*(1.0f+shape);

				if(t<shape){
					shape=t/shape;
					shape=(float)sqrt((double)shape);
					interp_v3_v3v3(state->co,state->co,state_co,shape);
				}
				else{
					VECCOPY(state->co,state_co);
				}
			}
			break;
	}
}

static void do_clump(ParticleKey *state, ParticleKey *par, float time, float clumpfac, float clumppow, float pa_clump)
{
	if(par && clumpfac!=0.0){
		float clump, cpow;

		if(clumppow<0.0)
			cpow=1.0f+clumppow;
		else
			cpow=1.0f+9.0f*clumppow;

		if(clumpfac<0.0) /* clump roots instead of tips */
			clump = -clumpfac*pa_clump*(float)pow(1.0-(double)time,(double)cpow);
		else
			clump = clumpfac*pa_clump*(float)pow((double)time,(double)cpow);
		interp_v3_v3v3(state->co,state->co,par->co,clump);
	}
}
void precalc_guides(ParticleSimulationData *sim, ListBase *effectors)
{
	EffectedPoint point;
	ParticleKey state;
	EffectorData efd;
	EffectorCache *eff;
	ParticleSystem *psys = sim->psys;
	EffectorWeights *weights = sim->psys->part->effector_weights;
	GuideEffectorData *data;
	PARTICLE_P;

	if(!effectors)
		return;

	LOOP_PARTICLES {
		psys_particle_on_emitter(sim->psmd,sim->psys->part->from,pa->num,pa->num_dmcache,pa->fuv,pa->foffset,state.co,0,0,0,0,0);
		pd_point_from_particle(sim, pa, &state, &point);

		for(eff = effectors->first; eff; eff=eff->next) {
			if(eff->pd->forcefield != PFIELD_GUIDE)
				continue;

			if(!eff->guide_data)
				eff->guide_data = MEM_callocN(sizeof(GuideEffectorData)*psys->totpart, "GuideEffectorData");

			data = eff->guide_data + p;

			VECSUB(efd.vec_to_point, state.co, eff->guide_loc);
			VECCOPY(efd.nor, eff->guide_dir);
			efd.distance = len_v3(efd.vec_to_point);

			VECCOPY(data->vec_to_point, efd.vec_to_point);
			data->strength = effector_falloff(eff, &efd, &point, weights);
		}
	}
}
int do_guides(ListBase *effectors, ParticleKey *state, int index, float time)
{
	EffectorCache *eff;
	PartDeflect *pd;
	Curve *cu;
	ParticleKey key, par;
	GuideEffectorData *data;

	float effect[3] = {0.0f, 0.0f, 0.0f}, veffect[3] = {0.0f, 0.0f, 0.0f};
	float guidevec[4], guidedir[3], rot2[4], temp[3];
	float guidetime, radius, angle, totstrength = 0.0f;
	float vec_to_point[3];

	if(effectors) for(eff = effectors->first; eff; eff=eff->next) {
		pd = eff->pd;

		if(pd->forcefield != PFIELD_GUIDE)
			continue;

		data = eff->guide_data + index;

		if(data->strength <= 0.0f)
			continue;

		guidetime = time / (1.0 - pd->free_end);

		if(guidetime>1.0f)
			continue;

		cu = (Curve*)eff->ob->data;

		if(pd->flag & PFIELD_GUIDE_PATH_ADD) {
			if(where_on_path(eff->ob, data->strength * guidetime, guidevec, guidedir, NULL, &radius)==0)
				return 0;
		}
		else {
			if(where_on_path(eff->ob, guidetime, guidevec, guidedir, NULL, &radius)==0)
				return 0;
		}

		mul_m4_v3(eff->ob->obmat, guidevec);
		mul_mat3_m4_v3(eff->ob->obmat, guidedir);

		normalize_v3(guidedir);

		VECCOPY(vec_to_point, data->vec_to_point);

		if(guidetime != 0.0){
			/* curve direction */
			cross_v3_v3v3(temp, eff->guide_dir, guidedir);
			angle = dot_v3v3(eff->guide_dir, guidedir)/(len_v3(eff->guide_dir));
			angle = saacos(angle);
			axis_angle_to_quat( rot2,temp, angle);
			mul_qt_v3(rot2, vec_to_point);

			/* curve tilt */
			axis_angle_to_quat( rot2,guidedir, guidevec[3] - eff->guide_loc[3]);
			mul_qt_v3(rot2, vec_to_point);
		}

		/* curve taper */
		if(cu->taperobj)
			mul_v3_fl(vec_to_point, calc_taper(eff->scene, cu->taperobj, (int)(data->strength*guidetime*100.0), 100));

		else{ /* curve size*/
			if(cu->flag & CU_PATH_RADIUS) {
				mul_v3_fl(vec_to_point, radius);
			}
		}
		par.co[0] = par.co[1] = par.co[2] = 0.0f;
		VECCOPY(key.co, vec_to_point);
		do_prekink(&key, &par, 0, guidetime, pd->kink_freq, pd->kink_shape, pd->kink_amp, pd->kink, pd->kink_axis, 0);
		do_clump(&key, &par, guidetime, pd->clump_fac, pd->clump_pow, 1.0f);
		VECCOPY(vec_to_point, key.co);

		VECADD(vec_to_point, vec_to_point, guidevec);
		//VECSUB(pa_loc,pa_loc,pa_zero);
		VECADDFAC(effect, effect, vec_to_point, data->strength);
		VECADDFAC(veffect, veffect, guidedir, data->strength);
		totstrength += data->strength;
	}

	if(totstrength != 0.0){
		if(totstrength > 1.0)
			mul_v3_fl(effect, 1.0f / totstrength);
		CLAMP(totstrength, 0.0, 1.0);
		//VECADD(effect,effect,pa_zero);
		interp_v3_v3v3(state->co, state->co, effect, totstrength);

		normalize_v3(veffect);
		mul_v3_fl(veffect, len_v3(state->vel));
		VECCOPY(state->vel, veffect);
		return 1;
	}
	return 0;
}
static void do_rough(float *loc, float mat[4][4], float t, float fac, float size, float thres, ParticleKey *state)
{
	float rough[3];
	float rco[3];

	if(thres!=0.0)
		if((float)fabs((float)(-1.5+loc[0]+loc[1]+loc[2]))<1.5f*thres) return;

	VECCOPY(rco,loc);
	mul_v3_fl(rco,t);
	rough[0]=-1.0f+2.0f*BLI_gTurbulence(size, rco[0], rco[1], rco[2], 2,0,2);
	rough[1]=-1.0f+2.0f*BLI_gTurbulence(size, rco[1], rco[2], rco[0], 2,0,2);
	rough[2]=-1.0f+2.0f*BLI_gTurbulence(size, rco[2], rco[0], rco[1], 2,0,2);

	VECADDFAC(state->co,state->co,mat[0],fac*rough[0]);
	VECADDFAC(state->co,state->co,mat[1],fac*rough[1]);
	VECADDFAC(state->co,state->co,mat[2],fac*rough[2]);
}
static void do_rough_end(float *loc, float mat[4][4], float t, float fac, float shape, ParticleKey *state)
{
	float rough[2];
	float roughfac;

	roughfac=fac*(float)pow((double)t,shape);
	copy_v2_v2(rough,loc);
	rough[0]=-1.0f+2.0f*rough[0];
	rough[1]=-1.0f+2.0f*rough[1];
	mul_v2_fl(rough,roughfac);

	VECADDFAC(state->co,state->co,mat[0],rough[0]);
	VECADDFAC(state->co,state->co,mat[1],rough[1]);
}
static void do_path_effectors(ParticleSimulationData *sim, int i, ParticleCacheKey *ca, int k, int steps, float *rootco, float effector, float dfra, float cfra, float *length, float *vec)
{
	float force[3] = {0.0f,0.0f,0.0f};
	ParticleKey eff_key;
	EffectedPoint epoint;

	/* Don't apply effectors for dynamic hair, otherwise the effectors don't get applied twice. */
	if(sim->psys->flag & PSYS_HAIR_DYNAMICS)
		return;

	VECCOPY(eff_key.co,(ca-1)->co);
	VECCOPY(eff_key.vel,(ca-1)->vel);
	QUATCOPY(eff_key.rot,(ca-1)->rot);

	pd_point_from_particle(sim, sim->psys->particles+i, &eff_key, &epoint);
	pdDoEffectors(sim->psys->effectors, sim->colliders, sim->psys->part->effector_weights, &epoint, force, NULL);

	mul_v3_fl(force, effector*pow((float)k / (float)steps, 100.0f * sim->psys->part->eff_hair) / (float)steps);

	add_v3_v3v3(force, force, vec);

	normalize_v3(force);

	VECADDFAC(ca->co, (ca-1)->co, force, *length);

	if(k < steps) {
		sub_v3_v3v3(vec, (ca+1)->co, ca->co);
		*length = len_v3(vec);
	}
}
static int check_path_length(int k, ParticleCacheKey *keys, ParticleCacheKey *state, float max_length, float *cur_length, float length, float *dvec)
{
	if(*cur_length + length > max_length){
		mul_v3_fl(dvec, (max_length - *cur_length) / length);
		VECADD(state->co, (state - 1)->co, dvec);
		keys->steps = k;
		/* something over the maximum step value */
		return k=100000;
	}
	else {
		*cur_length+=length;
		return k;
	}
}
static void offset_child(ChildParticle *cpa, ParticleKey *par, ParticleKey *child, float flat, float radius)
{
	VECCOPY(child->co,cpa->fuv);
	mul_v3_fl(child->co,radius);

	child->co[0]*=flat;

	VECCOPY(child->vel,par->vel);

	mul_qt_v3(par->rot,child->co);

	QUATCOPY(child->rot,par->rot);

	VECADD(child->co,child->co,par->co);
}
float *psys_cache_vgroup(DerivedMesh *dm, ParticleSystem *psys, int vgroup)
{
	float *vg=0;

	if(vgroup < 0) {
		/* hair dynamics pinning vgroup */

	}
	else if(psys->vgroup[vgroup]){
		MDeformVert *dvert = dm->getVertDataArray(dm, CD_MDEFORMVERT);
		if(dvert){
			int totvert=dm->getNumVerts(dm), i;
			vg=MEM_callocN(sizeof(float)*totvert, "vg_cache");
			if(psys->vg_neg&(1<<vgroup)){
				for(i=0; i<totvert; i++)
					vg[i]=1.0f-vert_weight(dvert+i,psys->vgroup[vgroup]-1);
			}
			else{
				for(i=0; i<totvert; i++)
					vg[i]=vert_weight(dvert+i,psys->vgroup[vgroup]-1);
			}
		}
	}
	return vg;
}
void psys_find_parents(ParticleSimulationData *sim)
{
	ParticleSettings *part=sim->psys->part;
	KDTree *tree;
	ChildParticle *cpa;
	int p, totparent,totchild=sim->psys->totchild;
	float co[3], orco[3];
	int from=PART_FROM_FACE;
	totparent=(int)(totchild*part->parents*0.3);

	if(G.rendering && part->child_nbr && part->ren_child_nbr)
		totparent*=(float)part->child_nbr/(float)part->ren_child_nbr;

	tree=BLI_kdtree_new(totparent);

	for(p=0,cpa=sim->psys->child; p<totparent; p++,cpa++){
		psys_particle_on_emitter(sim->psmd,from,cpa->num,DMCACHE_ISCHILD,cpa->fuv,cpa->foffset,co,0,0,0,orco,0);
		BLI_kdtree_insert(tree, p, orco, NULL);
	}

	BLI_kdtree_balance(tree);

	for(; p<totchild; p++,cpa++){
		psys_particle_on_emitter(sim->psmd,from,cpa->num,DMCACHE_ISCHILD,cpa->fuv,cpa->foffset,co,0,0,0,orco,0);
		cpa->parent=BLI_kdtree_find_nearest(tree, orco, NULL, NULL);
	}

	BLI_kdtree_free(tree);
}

static void get_strand_normal(Material *ma, float *surfnor, float surfdist, float *nor)
{
	float cross[3], nstrand[3], vnor[3], blend;

	if(!((ma->mode & MA_STR_SURFDIFF) || (ma->strand_surfnor > 0.0f)))
		return;

	if(ma->mode & MA_STR_SURFDIFF) {
		cross_v3_v3v3(cross, surfnor, nor);
		cross_v3_v3v3(nstrand, nor, cross);

		blend= INPR(nstrand, surfnor);
		CLAMP(blend, 0.0f, 1.0f);

		interp_v3_v3v3(vnor, nstrand, surfnor, blend);
		normalize_v3(vnor);
	}
	else
		VECCOPY(vnor, nor)
	
	if(ma->strand_surfnor > 0.0f) {
		if(ma->strand_surfnor > surfdist) {
			blend= (ma->strand_surfnor - surfdist)/ma->strand_surfnor;
			interp_v3_v3v3(vnor, vnor, surfnor, blend);
			normalize_v3(vnor);
		}
	}

	VECCOPY(nor, vnor);
}

static int psys_threads_init_path(ParticleThread *threads, Scene *scene, float cfra, int editupdate)
{
	ParticleThreadContext *ctx= threads[0].ctx;
/*	Object *ob= ctx->sim.ob; */
	ParticleSystem *psys= ctx->sim.psys;
	ParticleSettings *part = psys->part;
/*	ParticleEditSettings *pset = &scene->toolsettings->particle; */
	int totparent=0, between=0;
	int steps = (int)pow(2.0, (double)part->draw_step);
	int totchild = psys->totchild;
	int i, seed, totthread= threads[0].tot;

	/*---start figuring out what is actually wanted---*/
	if(psys_in_edit_mode(scene, psys)) {
		ParticleEditSettings *pset = &scene->toolsettings->particle;

		if(psys->renderdata==0 && (psys->edit==NULL || pset->flag & PE_DRAW_PART)==0)
			totchild=0;

		steps = (int)pow(2.0, (double)pset->draw_step);
	}

	if(totchild && part->from!=PART_FROM_PARTICLE && part->childtype==PART_CHILD_FACES){
		totparent=(int)(totchild*part->parents*0.3);
		
		if(G.rendering && part->child_nbr && part->ren_child_nbr)
			totparent*=(float)part->child_nbr/(float)part->ren_child_nbr;

		/* part->parents could still be 0 so we can't test with totparent */
		between=1;
	}

	if(psys->renderdata)
		steps=(int)pow(2.0,(double)part->ren_step);
	else{
		totchild=(int)((float)totchild*(float)part->disp/100.0f);
		totparent=MIN2(totparent,totchild);
	}

	if(totchild==0) return 0;

	/* init random number generator */
	if(ctx->sim.psys->part->flag & PART_ANIM_BRANCHING)
		seed= 31415926 + ctx->sim.psys->seed + (int)cfra;
	else
		seed= 31415926 + ctx->sim.psys->seed;
	
	if(part->flag & PART_BRANCHING || ctx->editupdate || totchild < 10000)
		totthread= 1;
	
	for(i=0; i<totthread; i++) {
		threads[i].rng_path= rng_new(seed);
		threads[i].tot= totthread;
	}

	/* fill context values */
	ctx->between= between;
	ctx->steps= steps;
	ctx->totchild= totchild;
	ctx->totparent= totparent;
	ctx->parent_pass= 0;
	ctx->cfra= cfra;
	ctx->editupdate= editupdate;

	psys->lattice = psys_get_lattice(&ctx->sim);

	/* cache all relevant vertex groups if they exist */
	if(part->from!=PART_FROM_PARTICLE){
		ctx->vg_length = psys_cache_vgroup(ctx->dm,psys,PSYS_VG_LENGTH);
		ctx->vg_clump = psys_cache_vgroup(ctx->dm,psys,PSYS_VG_CLUMP);
		ctx->vg_kink = psys_cache_vgroup(ctx->dm,psys,PSYS_VG_KINK);
		ctx->vg_rough1 = psys_cache_vgroup(ctx->dm,psys,PSYS_VG_ROUGH1);
		ctx->vg_rough2 = psys_cache_vgroup(ctx->dm,psys,PSYS_VG_ROUGH2);
		ctx->vg_roughe = psys_cache_vgroup(ctx->dm,psys,PSYS_VG_ROUGHE);
		if(psys->part->flag & PART_CHILD_EFFECT)
			ctx->vg_effector = psys_cache_vgroup(ctx->dm,psys,PSYS_VG_EFFECTOR);
	}

	/* set correct ipo timing */
#if 0 // XXX old animation system
	if(part->flag&PART_ABS_TIME && part->ipo){
		calc_ipo(part->ipo, cfra);
		execute_ipo((ID *)part, part->ipo);
	}
#endif // XXX old animation system

	return 1;
}

/* note: this function must be thread safe, except for branching! */
static void psys_thread_create_path(ParticleThread *thread, struct ChildParticle *cpa, ParticleCacheKey *keys, int i)
{
	ParticleThreadContext *ctx= thread->ctx;
	Object *ob= ctx->sim.ob;
	ParticleSystem *psys = ctx->sim.psys;
	ParticleSettings *part = psys->part;
	ParticleCacheKey **cache= psys->childcache;
	ParticleCacheKey **pcache= psys_in_edit_mode(ctx->sim.scene, psys) ? psys->edit->pathcache : psys->pathcache;
	ParticleCacheKey *state, *par = NULL, *key[4];
	ParticleData *pa=NULL;
	ParticleTexture ptex;
	float *cpa_fuv=0, *par_rot=0;
	float co[3], orco[3], ornor[3], hairmat[4][4], t, cpa_1st[3], dvec[3];
	float branch_begin, branch_end, branch_prob, rough_rand;
	float length, max_length = 1.0f, cur_length = 0.0f;
	float eff_length, eff_vec[3];
	int k, cpa_num;
	short cpa_from;

	if(!pcache)
		return;

	if(part->flag & PART_BRANCHING) {
		branch_begin=rng_getFloat(thread->rng_path);
		branch_end=branch_begin+(1.0f-branch_begin)*rng_getFloat(thread->rng_path);
		branch_prob=rng_getFloat(thread->rng_path);
		rough_rand=rng_getFloat(thread->rng_path);
	}
	else {
		branch_begin= 0.0f;
		branch_end= 0.0f;
		branch_prob= 0.0f;
		rough_rand= 0.0f;
	}

	if(i<psys->totpart){
		branch_begin=0.0f;
		branch_end=1.0f;
		branch_prob=0.0f;
	}

	if(ctx->between){
		int w, needupdate;
		float foffset;

		if(ctx->editupdate && !(part->flag & PART_BRANCHING)) {
			needupdate= 0;
			w= 0;
			while(w<4 && cpa->pa[w]>=0) {
				if(psys->edit->points[cpa->pa[w]].flag & PEP_EDIT_RECALC) {
					needupdate= 1;
					break;
				}
				w++;
			}

			if(!needupdate)
				return;
			else
				memset(keys, 0, sizeof(*keys)*(ctx->steps+1));
		}

		/* get parent paths */
		w= 0;
		while(w<4 && cpa->pa[w]>=0){
			key[w] = pcache[cpa->pa[w]];
			w++;
		}

		/* get the original coordinates (orco) for texture usage */
		cpa_num = cpa->num;
		
		foffset= cpa->foffset;
		cpa_fuv = cpa->fuv;
		cpa_from = PART_FROM_FACE;

		psys_particle_on_emitter(ctx->sim.psmd,cpa_from,cpa_num,DMCACHE_ISCHILD,cpa->fuv,foffset,co,ornor,0,0,orco,0);

		if(part->path_start==0.0f) {
			/* we need to save the actual root position of the child for positioning it accurately to the surface of the emitter */
			VECCOPY(cpa_1st,co);
			mul_m4_v3(ob->obmat,cpa_1st);
		}

		pa = psys->particles + cpa->parent;

		psys_mat_hair_to_global(ob, ctx->sim.psmd->dm, psys->part->from, pa, hairmat);

		pa=0;
	}
	else{
		if(ctx->editupdate && !(part->flag & PART_BRANCHING)) {
			if(!(psys->edit->points[cpa->parent].flag & PEP_EDIT_RECALC))
				return;

			memset(keys, 0, sizeof(*keys)*(ctx->steps+1));
		}

		/* get the parent path */
		key[0]=pcache[cpa->parent];

		/* get the original coordinates (orco) for texture usage */
		pa=psys->particles+cpa->parent;

		cpa_from=part->from;
		cpa_num=pa->num;
		cpa_fuv=pa->fuv;

		psys_particle_on_emitter(ctx->sim.psmd,cpa_from,cpa_num,DMCACHE_ISCHILD,cpa_fuv,pa->foffset,co,ornor,0,0,orco,0);

		psys_mat_hair_to_global(ob, ctx->sim.psmd->dm, psys->part->from, pa, hairmat);
	}

	keys->steps = ctx->steps;

	/* correct child ipo timing */
#if 0 // XXX old animation system
	if((part->flag&PART_ABS_TIME)==0 && part->ipo){
		float dsta=part->end-part->sta;
		calc_ipo(part->ipo, 100.0f*(ctx->cfra-(part->sta+dsta*cpa->rand[1]))/(part->lifetime*(1.0f - part->randlife*cpa->rand[0])));
		execute_ipo((ID *)part, part->ipo);
	}
#endif // XXX old animation system

	/* get different child parameters from textures & vgroups */
	get_child_modifier_parameters(part, ctx, cpa, cpa_from, cpa_num, cpa_fuv, orco, &ptex);

	if(ptex.exist < PSYS_FRAND(i + 24)) {
		keys->steps = -1;
		return;
	}

	/* create the child path */
	for(k=0,state=keys; k<=ctx->steps; k++,state++){
		if(ctx->between){
			int w=0;

			state->co[0] = state->co[1] = state->co[2] = 0.0f;
			state->vel[0] = state->vel[1] = state->vel[2] = 0.0f;
			state->rot[0] = state->rot[1] = state->rot[2] = state->rot[3] = 0.0f;

			//QUATCOPY(state->rot,key[0]->rot);

			/* child position is the weighted sum of parent positions */
			while(w<4 && cpa->pa[w]>=0){
				state->co[0] += cpa->w[w] * key[w]->co[0];
				state->co[1] += cpa->w[w] * key[w]->co[1];
				state->co[2] += cpa->w[w] * key[w]->co[2];

				state->vel[0] += cpa->w[w] * key[w]->vel[0];
				state->vel[1] += cpa->w[w] * key[w]->vel[1];
				state->vel[2] += cpa->w[w] * key[w]->vel[2];
				key[w]++;
				w++;
			}
			if(part->path_start==0.0f) {
				if(k==0){
					/* calculate the offset between actual child root position and first position interpolated from parents */
					VECSUB(cpa_1st,cpa_1st,state->co);
				}
				/* apply offset for correct positioning */
				VECADD(state->co,state->co,cpa_1st);
			}
		}
		else{
			/* offset the child from the parent position */
			offset_child(cpa, (ParticleKey*)key[0], (ParticleKey*)state, part->childflat, part->childrad);

			key[0]++;
		}
	}

	/* apply effectors */
	if(part->flag & PART_CHILD_EFFECT) {
		for(k=0,state=keys; k<=ctx->steps; k++,state++) {
			if(k) {
				do_path_effectors(&ctx->sim, cpa->pa[0], state, k, ctx->steps, keys->co, ptex.effector, 0.0f, ctx->cfra, &eff_length, eff_vec);
			}
			else {
				sub_v3_v3v3(eff_vec,(state+1)->co,state->co);
				eff_length= len_v3(eff_vec);
			}
		}
	}

	for(k=0,state=keys; k<=ctx->steps; k++,state++){
		t=(float)k/(float)ctx->steps;

		if(ctx->totparent){
			if(i>=ctx->totparent) {
				/* this is now threadsafe, virtual parents are calculated before rest of children */
				par = cache[cpa->parent] + k;
			}
			else
				par=0;
		}
		else if(cpa->parent>=0){
			par=pcache[cpa->parent]+k;
			par_rot = par->rot;
		}

		/* apply different deformations to the child path */
		do_child_modifiers(&ctx->sim, &ptex, (ParticleKey *)par, par_rot, cpa, orco, hairmat, (ParticleKey *)state, t);

		/* TODO: better branching */
		//if(part->flag & PART_BRANCHING && ctx->between == 0 && part->flag & PART_ANIM_BRANCHING)
		//	rough_t = t * rough_rand;
		//else
		//	rough_t = t;

		/* TODO: better branching */
		//if(part->flag & PART_BRANCHING && ctx->between==0){
		//	if(branch_prob > part->branch_thres){
		//		branchfac=0.0f;
		//	}
		//	else{
		//		if(part->flag & PART_SYMM_BRANCHING){
		//			if(t < branch_begin || t > branch_end)
		//				branchfac=0.0f;
		//			else{
		//				if((t-branch_begin)/(branch_end-branch_begin)<0.5)
		//					branchfac=2.0f*(t-branch_begin)/(branch_end-branch_begin);
		//				else
		//					branchfac=2.0f*(branch_end-t)/(branch_end-branch_begin);

		//				CLAMP(branchfac,0.0f,1.0f);
		//			}
		//		}
		//		else{
		//			if(t < branch_begin){
		//				branchfac=0.0f;
		//			}
		//			else{
		//				branchfac=(t-branch_begin)/((1.0f-branch_begin)*0.5f);
		//				CLAMP(branchfac,0.0f,1.0f);
		//			}
		//		}
		//	}

		//	if(i<psys->totpart)
		//		interp_v3_v3v3(state->co, (pcache[i] + k)->co, state->co, branchfac);
		//	else
		//		/* this is not threadsafe, but should only happen for
		//		 * branching particles particles, which are not threaded */
		//		interp_v3_v3v3(state->co, (cache[i - psys->totpart] + k)->co, state->co, branchfac);
		//}

		/* we have to correct velocity because of kink & clump */
		if(k>1){
			VECSUB((state-1)->vel,state->co,(state-2)->co);
			mul_v3_fl((state-1)->vel,0.5);

			if(ctx->ma && (part->draw & PART_DRAW_MAT_COL))
				get_strand_normal(ctx->ma, ornor, cur_length, (state-1)->vel);
		}

		if(k == ctx->steps)
			VECSUB(state->vel,state->co,(state-1)->co);

		/* check if path needs to be cut before actual end of data points */
		if(k){
			VECSUB(dvec,state->co,(state-1)->co);
			length=1.0f/(float)ctx->steps;
			k=check_path_length(k,keys,state,max_length,&cur_length,length,dvec);
		}
		else{
			/* initialize length calculation */
			max_length= ptex.length;
			cur_length= 0.0f;
		}

		if(ctx->ma && (part->draw & PART_DRAW_MAT_COL)) {
			VECCOPY(state->col, &ctx->ma->r)
			get_strand_normal(ctx->ma, ornor, cur_length, state->vel);
		}
	}
}

static void *exec_child_path_cache(void *data)
{
	ParticleThread *thread= (ParticleThread*)data;
	ParticleThreadContext *ctx= thread->ctx;
	ParticleSystem *psys= ctx->sim.psys;
	ParticleCacheKey **cache= psys->childcache;
	ChildParticle *cpa;
	int i, totchild= ctx->totchild, first= 0;

	if(thread->tot > 1){
		first= ctx->parent_pass? 0 : ctx->totparent;
		totchild= ctx->parent_pass? ctx->totparent : ctx->totchild;
	}
	
	cpa= psys->child + first + thread->num;
	for(i=first+thread->num; i<totchild; i+=thread->tot, cpa+=thread->tot)
		psys_thread_create_path(thread, cpa, cache[i], i);

	return 0;
}

void psys_cache_child_paths(ParticleSimulationData *sim, float cfra, int editupdate)
{
	ParticleSettings *part = sim->psys->part;
	ParticleThread *pthreads;
	ParticleThreadContext *ctx;
	ParticleCacheKey **cache;
	ListBase threads;
	int i, totchild, totparent, totthread;

	if(sim->psys->flag & PSYS_GLOBAL_HAIR)
		return;

	pthreads= psys_threads_create(sim);

	if(!psys_threads_init_path(pthreads, sim->scene, cfra, editupdate)) {
		psys_threads_free(pthreads);
		return;
	}

	ctx= pthreads[0].ctx;
	totchild= ctx->totchild;
	totparent= ctx->totparent;

	if(editupdate && sim->psys->childcache && !(part->flag & PART_BRANCHING) && totchild == sim->psys->totchildcache) {
		cache = sim->psys->childcache;
	}
	else {
		/* clear out old and create new empty path cache */
		free_child_path_cache(sim->psys);
		sim->psys->childcache= psys_alloc_path_cache_buffers(&sim->psys->childcachebufs, totchild, ctx->steps+1);
		sim->psys->totchildcache = totchild;
	}

	totthread= pthreads[0].tot;

	if(totthread > 1) {

		/* make virtual child parents thread safe by calculating them first */
		if(totparent) {
			BLI_init_threads(&threads, exec_child_path_cache, totthread);
			
			for(i=0; i<totthread; i++) {
				pthreads[i].ctx->parent_pass = 1;
				BLI_insert_thread(&threads, &pthreads[i]);
			}

			BLI_end_threads(&threads);

			for(i=0; i<totthread; i++)
				pthreads[i].ctx->parent_pass = 0;
		}

		BLI_init_threads(&threads, exec_child_path_cache, totthread);

		for(i=0; i<totthread; i++)
			BLI_insert_thread(&threads, &pthreads[i]);

		BLI_end_threads(&threads);
	}
	else
		exec_child_path_cache(&pthreads[0]);

	psys_threads_free(pthreads);
}
/* Calculates paths ready for drawing/rendering.									*/
/* -Usefull for making use of opengl vertex arrays for super fast strand drawing.	*/
/* -Makes child strands possible and creates them too into the cache.				*/
/* -Cached path data is also used to determine cut position for the editmode tool.	*/
void psys_cache_paths(ParticleSimulationData *sim, float cfra)
{
	PARTICLE_PSMD;
	ParticleEditSettings *pset = &sim->scene->toolsettings->particle;
	ParticleSystem *psys = sim->psys;
	ParticleSettings *part = psys->part;
	ParticleCacheKey *ca, **cache= psys->pathcache;

	DerivedMesh *hair_dm = psys->hair_out_dm;
	
	ParticleKey result;
	
	Material *ma;
	ParticleInterpolationData pind;

	PARTICLE_P;
	
	float birthtime = 0.0, dietime = 0.0;
	float t, time = 0.0, dfra = 1.0, frs_sec = sim->scene->r.frs_sec;
	float col[4] = {0.5f, 0.5f, 0.5f, 1.0f};
	float prev_tangent[3], hairmat[4][4];
	float rotmat[3][3];
	int k;
	int steps = (int)pow(2.0, (double)(psys->renderdata ? part->ren_step : part->draw_step));
	int totpart = psys->totpart;
	float length, vec[3];
	float *vg_effector= NULL, effector=0.0f;
	float *vg_length= NULL, pa_length=1.0f;
	int keyed, baked;

	/* we don't have anything valid to create paths from so let's quit here */
	if((psys->flag & PSYS_HAIR_DONE || psys->flag & PSYS_KEYED || psys->pointcache->flag & PTCACHE_BAKED)==0)
		return;

	if(psys_in_edit_mode(sim->scene, psys))
		if(psys->renderdata==0 && (psys->edit==NULL || pset->flag & PE_DRAW_PART)==0)
			return;
	
	BLI_srandom(psys->seed);

	keyed = psys->flag & PSYS_KEYED;
	baked = !hair_dm && psys->pointcache->flag & PTCACHE_BAKED;

	/* clear out old and create new empty path cache */
	psys_free_path_cache(psys, psys->edit);
	cache= psys->pathcache= psys_alloc_path_cache_buffers(&psys->pathcachebufs, totpart, steps+1);

	psys->lattice = psys_get_lattice(sim);
	ma= give_current_material(sim->ob, psys->part->omat);
	if(ma && (psys->part->draw & PART_DRAW_MAT_COL))
		VECCOPY(col, &ma->r)

	if(psys->part->from!=PART_FROM_PARTICLE && !(psys->flag & PSYS_GLOBAL_HAIR)) {
		if(!(psys->part->flag & PART_CHILD_EFFECT))
			vg_effector = psys_cache_vgroup(psmd->dm, psys, PSYS_VG_EFFECTOR);
		
		if(!psys->totchild)
			vg_length = psys_cache_vgroup(psmd->dm, psys, PSYS_VG_LENGTH);
	}

	/*---first main loop: create all actual particles' paths---*/
	LOOP_SHOWN_PARTICLES {
		if(!psys->totchild) {
			BLI_srandom(psys->seed + p);
			pa_length = 1.0f - part->randlength * BLI_frand();
			if(vg_length)
				pa_length *= psys_particle_value_from_verts(psmd->dm,part->from,pa,vg_length);
		}

		pind.keyed = keyed;
		pind.cache = baked ? psys->pointcache : NULL;
		pind.epoint = NULL;
		pind.bspline = (psys->part->flag & PART_HAIR_BSPLINE);
		pind.dm = hair_dm;

		memset(cache[p], 0, sizeof(*cache[p])*(steps+1));

		cache[p]->steps = steps;

		/*--get the first data points--*/
		init_particle_interpolation(sim->ob, sim->psys, pa, &pind);

		/* hairmat is needed for for non-hair particle too so we get proper rotations */
		psys_mat_hair_to_global(sim->ob, psmd->dm, psys->part->from, pa, hairmat);
		VECCOPY(rotmat[0], hairmat[2]);
		VECCOPY(rotmat[1], hairmat[1]);
		VECCOPY(rotmat[2], hairmat[0]);

		if(part->draw & PART_ABS_PATH_TIME) {
			birthtime = MAX2(pind.birthtime, part->path_start);
			dietime = MIN2(pind.dietime, part->path_end);
		}
		else {
			float tb = pind.birthtime;
			birthtime = tb + part->path_start * (pind.dietime - tb);
			dietime = tb + part->path_end * (pind.dietime - tb);
		}

		if(birthtime >= dietime) {
			cache[p]->steps = -1;
			continue;
		}

		dietime = birthtime + pa_length * (dietime - birthtime);

		/*--interpolate actual path from data points--*/
		for(k=0, ca=cache[p]; k<=steps; k++, ca++){
			time = (float)k / (float)steps;

			t = birthtime + time * (dietime - birthtime);

			result.time = -t;

			do_particle_interpolation(psys, p, pa, t, frs_sec, &pind, &result);

			/* dynamic hair is in object space */
			/* keyed and baked are allready in global space */
			if(hair_dm)
				mul_m4_v3(sim->ob->obmat, result.co);
			else if(!keyed && !baked && !(psys->flag & PSYS_GLOBAL_HAIR))
				mul_m4_v3(hairmat, result.co);

			VECCOPY(ca->co, result.co);
			VECCOPY(ca->col, col);
		}
		
		/*--modify paths and calculate rotation & velocity--*/

		sub_v3_v3v3(vec,(cache[p]+1)->co,cache[p]->co);
		length = len_v3(vec);

		effector= 1.0f;
		if(vg_effector)
			effector*= psys_particle_value_from_verts(psmd->dm,psys->part->from,pa,vg_effector);

		for(k=0, ca=cache[p]; k<=steps; k++, ca++) {
			if(!(psys->flag & PSYS_GLOBAL_HAIR)) {
			/* apply effectors */
				if(!(psys->part->flag & PART_CHILD_EFFECT) && k)
					do_path_effectors(sim, p, ca, k, steps, cache[p]->co, effector, dfra, cfra, &length, vec);

				/* apply guide curves to path data */
				if(sim->psys->effectors && (psys->part->flag & PART_CHILD_EFFECT)==0)
					/* ca is safe to cast, since only co and vel are used */
					do_guides(sim->psys->effectors, (ParticleKey*)ca, p, (float)k/(float)steps);

				/* apply lattice */
				if(psys->lattice)
					calc_latt_deform(psys->lattice, ca->co, 1.0f);

				/* figure out rotation */
				
				if(k) {
					float cosangle, angle, tangent[3], normal[3], q[4];

					if(k == 1) {
						/* calculate initial tangent for incremental rotations */
						VECSUB(tangent, ca->co, (ca - 1)->co);
						VECCOPY(prev_tangent, tangent);
						normalize_v3(prev_tangent);

						/* First rotation is based on emitting face orientation.		*/
						/* This is way better than having flipping rotations resulting	*/
						/* from using a global axis as a rotation pole (vec_to_quat()). */
						/* It's not an ideal solution though since it disregards the	*/
						/* initial tangent, but taking that in to account will allow	*/
						/* the possibility of flipping again. -jahka					*/
						mat3_to_quat_is_ok( (ca-1)->rot,rotmat);
					}
					else {
						VECSUB(tangent, ca->co, (ca - 1)->co);
						normalize_v3(tangent);

						cosangle= dot_v3v3(tangent, prev_tangent);

						/* note we do the comparison on cosangle instead of
						* angle, since floating point accuracy makes it give
						* different results across platforms */
						if(cosangle > 0.999999f) {
							QUATCOPY((ca - 1)->rot, (ca - 2)->rot);
						}
						else {
							angle= saacos(cosangle);
							cross_v3_v3v3(normal, prev_tangent, tangent);
							axis_angle_to_quat( q,normal, angle);
							mul_qt_qtqt((ca - 1)->rot, q, (ca - 2)->rot);
						}

						VECCOPY(prev_tangent, tangent);
					}

					if(k == steps)
						QUATCOPY(ca->rot, (ca - 1)->rot);
				}

			}
			
			/* set velocity */

			if(k){
				VECSUB(ca->vel, ca->co, (ca-1)->co);

				if(k==1) {
					VECCOPY((ca-1)->vel, ca->vel);
				}

			}
		}
	}

	psys->totcached = totpart;

	if(psys && psys->lattice){
		end_latt_deform(psys->lattice);
		psys->lattice= NULL;
	}

	if(vg_effector)
		MEM_freeN(vg_effector);

	if(vg_length)
		MEM_freeN(vg_length);
}
void psys_cache_edit_paths(Scene *scene, Object *ob, PTCacheEdit *edit, float cfra)
{
	ParticleCacheKey *ca, **cache= edit->pathcache;
	ParticleEditSettings *pset = &scene->toolsettings->particle;
	
	PTCacheEditPoint *point = edit->points;
	PTCacheEditKey *ekey = NULL;

	ParticleSystem *psys = edit->psys;
	ParticleSystemModifierData *psmd = psys_get_modifier(ob, psys);
	ParticleData *pa = psys ? psys->particles : NULL;

	ParticleInterpolationData pind;
	ParticleKey result;
	
	float birthtime = 0.0, dietime = 0.0;
	float t, time = 0.0, keytime = 0.0, frs_sec;
	float hairmat[4][4], rotmat[3][3], prev_tangent[3];
	int k,i;
	int steps = (int)pow(2.0, (double)pset->draw_step);
	int totpart = edit->totpoint;
	float sel_col[3];
	float nosel_col[3];

	steps = MAX2(steps, 4);

	if(!cache || edit->totpoint != edit->totcached) {
		/* clear out old and create new empty path cache */
		psys_free_path_cache(edit->psys, edit);
		cache= edit->pathcache= psys_alloc_path_cache_buffers(&edit->pathcachebufs, totpart, steps+1);
	}

	frs_sec = (psys || edit->pid.flag & PTCACHE_VEL_PER_SEC) ? 25.0f : 1.0f;

	sel_col[0] = (float)edit->sel_col[0] / 255.0f;
	sel_col[1] = (float)edit->sel_col[1] / 255.0f;
	sel_col[2] = (float)edit->sel_col[2] / 255.0f;
	nosel_col[0] = (float)edit->nosel_col[0] / 255.0f;
	nosel_col[1] = (float)edit->nosel_col[1] / 255.0f;
	nosel_col[2] = (float)edit->nosel_col[2] / 255.0f;

	/*---first main loop: create all actual particles' paths---*/
	for(i=0; i<totpart; i++, pa+=pa?1:0, point++){
		if(edit->totcached && !(point->flag & PEP_EDIT_RECALC))
			continue;

		ekey = point->keys;

		pind.keyed = 0;
		pind.cache = NULL;
		pind.epoint = point;
		pind.bspline = psys ? (psys->part->flag & PART_HAIR_BSPLINE) : 0;
		pind.dm = NULL;

		memset(cache[i], 0, sizeof(*cache[i])*(steps+1));

		cache[i]->steps = steps;

		/*--get the first data points--*/
		init_particle_interpolation(ob, psys, pa, &pind);

		if(psys) {
			psys_mat_hair_to_global(ob, psmd->dm, psys->part->from, pa, hairmat);
			VECCOPY(rotmat[0], hairmat[2]);
			VECCOPY(rotmat[1], hairmat[1]);
			VECCOPY(rotmat[2], hairmat[0]);
		}

		birthtime = pind.birthtime;
		dietime = pind.dietime;

		if(birthtime >= dietime) {
			cache[i]->steps = -1;
			continue;
		}

		/*--interpolate actual path from data points--*/
		for(k=0, ca=cache[i]; k<=steps; k++, ca++){
			time = (float)k / (float)steps;

			t = birthtime + time * (dietime - birthtime);

			result.time = -t;

			do_particle_interpolation(psys, i, pa, t, frs_sec, &pind, &result);

			 /* non-hair points are allready in global space */
			if(psys && !(psys->flag & PSYS_GLOBAL_HAIR)) {
				mul_m4_v3(hairmat, result.co);

				/* create rotations for proper creation of children */
				if(k) {
					float cosangle, angle, tangent[3], normal[3], q[4];

					if(k == 1) {
						/* calculate initial tangent for incremental rotations */
						VECSUB(tangent, ca->co, (ca - 1)->co);
						VECCOPY(prev_tangent, tangent);
						normalize_v3(prev_tangent);

						/* First rotation is based on emitting face orientation.		*/
						/* This is way better than having flipping rotations resulting	*/
						/* from using a global axis as a rotation pole (vec_to_quat()). */
						/* It's not an ideal solution though since it disregards the	*/
						/* initial tangent, but taking that in to account will allow	*/
						/* the possibility of flipping again. -jahka					*/
						mat3_to_quat_is_ok( (ca-1)->rot,rotmat);
					}
					else {
						VECSUB(tangent, ca->co, (ca - 1)->co);
						normalize_v3(tangent);

						cosangle= dot_v3v3(tangent, prev_tangent);

						/* note we do the comparison on cosangle instead of
						* angle, since floating point accuracy makes it give
						* different results across platforms */
						if(cosangle > 0.999999f) {
							QUATCOPY((ca - 1)->rot, (ca - 2)->rot);
						}
						else {
							angle= saacos(cosangle);
							cross_v3_v3v3(normal, prev_tangent, tangent);
							axis_angle_to_quat( q,normal, angle);
							mul_qt_qtqt((ca - 1)->rot, q, (ca - 2)->rot);
						}

						VECCOPY(prev_tangent, tangent);
					}

					if(k == steps)
						QUATCOPY(ca->rot, (ca - 1)->rot);
				}

			}

			VECCOPY(ca->co, result.co);

			ca->vel[0] = ca->vel[1] = 0.0f;
			ca->vel[1] = 1.0f;

			/* selection coloring in edit mode */
			if((ekey + (pind.ekey[0] - point->keys))->flag & PEK_SELECT){
				if((ekey + (pind.ekey[1] - point->keys))->flag & PEK_SELECT){
					VECCOPY(ca->col, sel_col);
				}
				else{
					keytime = (t - (*pind.ekey[0]->time))/((*pind.ekey[1]->time) - (*pind.ekey[0]->time));
					interp_v3_v3v3(ca->col, sel_col, nosel_col, keytime);
				}
			}
			else{
				if((ekey + (pind.ekey[1] - point->keys))->flag & PEK_SELECT){
					keytime = (t - (*pind.ekey[0]->time))/((*pind.ekey[1]->time) - (*pind.ekey[0]->time));
					interp_v3_v3v3(ca->col, nosel_col, sel_col, keytime);
				}
				else{
					VECCOPY(ca->col, nosel_col);
				}
			}

			ca->time = t;
		}
	}

	edit->totcached = totpart;

	if(psys && psys->part->type == PART_HAIR) {
		ParticleSimulationData sim = {scene, ob, psys, psys_get_modifier(ob, psys), NULL};
		psys_cache_child_paths(&sim, cfra, 1);
	}
}
/************************************************/
/*			Particle Key handling				*/
/************************************************/
void copy_particle_key(ParticleKey *to, ParticleKey *from, int time){
	if(time){
		memcpy(to,from,sizeof(ParticleKey));
	}
	else{
		float to_time=to->time;
		memcpy(to,from,sizeof(ParticleKey));
		to->time=to_time;
	}
}
void psys_get_from_key(ParticleKey *key, float *loc, float *vel, float *rot, float *time){
	if(loc) VECCOPY(loc,key->co);
	if(vel) VECCOPY(vel,key->vel);
	if(rot) QUATCOPY(rot,key->rot);
	if(time) *time=key->time;
}
/*-------changing particle keys from space to another-------*/
#if 0
static void key_from_object(Object *ob, ParticleKey *key){
	float q[4];

	VECADD(key->vel,key->vel,key->co);

	mul_m4_v3(ob->obmat,key->co);
	mul_m4_v3(ob->obmat,key->vel);
	mat4_to_quat(q,ob->obmat);

	VECSUB(key->vel,key->vel,key->co);
	mul_qt_qtqt(key->rot,q,key->rot);
}
#endif

static void triatomat(float *v1, float *v2, float *v3, float (*uv)[2], float mat[][4])
{
	float det, w1, w2, d1[2], d2[2];

	memset(mat, 0, sizeof(float)*4*4);
	mat[3][3]= 1.0f;

	/* first axis is the normal */
	normal_tri_v3( mat[2],v1, v2, v3);

	/* second axis along (1, 0) in uv space */
	if(uv) {
		d1[0]= uv[1][0] - uv[0][0];
		d1[1]= uv[1][1] - uv[0][1];
		d2[0]= uv[2][0] - uv[0][0];
		d2[1]= uv[2][1] - uv[0][1];

		det = d2[0]*d1[1] - d2[1]*d1[0];

		if(det != 0.0f) {
			det= 1.0f/det;
			w1= -d2[1]*det;
			w2= d1[1]*det;

			mat[1][0]= w1*(v2[0] - v1[0]) + w2*(v3[0] - v1[0]);
			mat[1][1]= w1*(v2[1] - v1[1]) + w2*(v3[1] - v1[1]);
			mat[1][2]= w1*(v2[2] - v1[2]) + w2*(v3[2] - v1[2]);
			normalize_v3(mat[1]);
		}
		else
			mat[1][0]= mat[1][1]= mat[1][2]= 0.0f;
	}
	else {
		sub_v3_v3v3(mat[1], v2, v1);
		normalize_v3(mat[1]);
	}
	
	/* third as a cross product */
	cross_v3_v3v3(mat[0], mat[1], mat[2]);
}

static void psys_face_mat(Object *ob, DerivedMesh *dm, ParticleData *pa, float mat[][4], int orco)
{
	float v[3][3];
	MFace *mface;
	OrigSpaceFace *osface;
	float (*orcodata)[3];

	int i = pa->num_dmcache==DMCACHE_NOTFOUND ? pa->num : pa->num_dmcache;
	
	if (i==-1 || i >= dm->getNumFaces(dm)) { unit_m4(mat); return; }

	mface=dm->getFaceData(dm,i,CD_MFACE);
	osface=dm->getFaceData(dm,i,CD_ORIGSPACE);
	
	if(orco && (orcodata=dm->getVertDataArray(dm, CD_ORCO))) {
		VECCOPY(v[0], orcodata[mface->v1]);
		VECCOPY(v[1], orcodata[mface->v2]);
		VECCOPY(v[2], orcodata[mface->v3]);

		/* ugly hack to use non-transformed orcos, since only those
		 * give symmetric results for mirroring in particle mode */
		transform_mesh_orco_verts(ob->data, v, 3, 1);
	}
	else {
		dm->getVertCo(dm,mface->v1,v[0]);
		dm->getVertCo(dm,mface->v2,v[1]);
		dm->getVertCo(dm,mface->v3,v[2]);
	}

	triatomat(v[0], v[1], v[2], (osface)? osface->uv: NULL, mat);
}

void psys_mat_hair_to_object(Object *ob, DerivedMesh *dm, short from, ParticleData *pa, float hairmat[][4])
{
	float vec[3];

	psys_face_mat(0, dm, pa, hairmat, 0);
	psys_particle_on_dm(dm, from, pa->num, pa->num_dmcache, pa->fuv, pa->foffset, vec, 0, 0, 0, 0, 0);
	VECCOPY(hairmat[3],vec);
}

void psys_mat_hair_to_orco(Object *ob, DerivedMesh *dm, short from, ParticleData *pa, float hairmat[][4])
{
	float vec[3], orco[3];

	psys_face_mat(ob, dm, pa, hairmat, 1);
	psys_particle_on_dm(dm, from, pa->num, pa->num_dmcache, pa->fuv, pa->foffset, vec, 0, 0, 0, orco, 0);

	/* see psys_face_mat for why this function is called */
	transform_mesh_orco_verts(ob->data, &orco, 1, 1);
	VECCOPY(hairmat[3],orco);
}

void psys_vec_rot_to_face(DerivedMesh *dm, ParticleData *pa, float *vec)
{
	float mat[4][4];

	psys_face_mat(0, dm, pa, mat, 0);
	transpose_m4(mat); /* cheap inverse for rotation matrix */
	mul_mat3_m4_v3(mat, vec);
}

void psys_mat_hair_to_global(Object *ob, DerivedMesh *dm, short from, ParticleData *pa, float hairmat[][4])
{
	float facemat[4][4];

	psys_mat_hair_to_object(ob, dm, from, pa, facemat);

	mul_m4_m4m4(hairmat, facemat, ob->obmat);
}

/************************************************/
/*			ParticleSettings handling			*/
/************************************************/
ModifierData *object_add_particle_system(Scene *scene, Object *ob, char *name)
{
	ParticleSystem *psys;
	ModifierData *md;
	ParticleSystemModifierData *psmd;

	if(!ob || ob->type != OB_MESH)
		return NULL;

	psys = ob->particlesystem.first;
	for(; psys; psys=psys->next)
		psys->flag &= ~PSYS_CURRENT;

	psys = MEM_callocN(sizeof(ParticleSystem), "particle_system");
	psys->pointcache = BKE_ptcache_add(&psys->ptcaches);
	BLI_addtail(&ob->particlesystem, psys);

	psys->part = psys_new_settings("ParticleSettings", NULL);

	if(BLI_countlist(&ob->particlesystem)>1)
		sprintf(psys->name, "ParticleSystem %i", BLI_countlist(&ob->particlesystem));
	else
		strcpy(psys->name, "ParticleSystem");

	md= modifier_new(eModifierType_ParticleSystem);

	if(name)	BLI_strncpy(md->name, name, sizeof(md->name));
	else		sprintf(md->name, "ParticleSystem %i", BLI_countlist(&ob->particlesystem));
	modifier_unique_name(&ob->modifiers, md);

	psmd= (ParticleSystemModifierData*) md;
	psmd->psys=psys;
	BLI_addtail(&ob->modifiers, md);

	psys->totpart=0;
	psys->flag = PSYS_ENABLED|PSYS_CURRENT;
	psys->cfra=bsystem_time(scene,ob,scene->r.cfra+1,0.0);

	DAG_scene_sort(scene);
	DAG_id_flush_update(&ob->id, OB_RECALC_DATA);

	return md;
}
void object_remove_particle_system(Scene *scene, Object *ob)
{
	ParticleSystem *psys = psys_get_current(ob);
	ParticleSystemModifierData *psmd;
	ModifierData *md;

	if(!psys)
		return;

	/* clear all other appearances of this pointer (like on smoke flow modifier) */
	if((md = modifiers_findByType(ob, eModifierType_Smoke)))
	{
		SmokeModifierData *smd = (SmokeModifierData *)md;
		if((smd->type == MOD_SMOKE_TYPE_FLOW) && smd->flow && smd->flow->psys)
			if(smd->flow->psys == psys)
				smd->flow->psys = NULL;
	}

	/* clear modifier */
	psmd= psys_get_modifier(ob, psys);
	BLI_remlink(&ob->modifiers, psmd);
	modifier_free((ModifierData *)psmd);

	/* clear particle system */
	BLI_remlink(&ob->particlesystem, psys);
	psys_free(ob,psys);

	if(ob->particlesystem.first)
		((ParticleSystem *) ob->particlesystem.first)->flag |= PSYS_CURRENT;
	else
		ob->mode &= ~OB_MODE_PARTICLE_EDIT;

	DAG_scene_sort(scene);
	DAG_id_flush_update(&ob->id, OB_RECALC_DATA);
}
static void default_particle_settings(ParticleSettings *part)
{
	part->type= PART_EMITTER;
	part->distr= PART_DISTR_JIT;
	part->draw_as = PART_DRAW_REND;
	part->ren_as = PART_DRAW_HALO;
	part->bb_uv_split=1;
	part->bb_align=PART_BB_VIEW;
	part->bb_split_offset=PART_BB_OFF_LINEAR;
	part->flag=PART_REACT_MULTIPLE|PART_HAIR_GEOMETRY|PART_EDISTR|PART_TRAND;

	part->sta= 1.0;
	part->end= 200.0;
	part->lifetime= 50.0;
	part->jitfac= 1.0;
	part->totpart= 1000;
	part->grid_res= 10;
	part->timetweak= 1.0;
	
	part->integrator= PART_INT_MIDPOINT;
	part->phystype= PART_PHYS_NEWTON;
	part->hair_step= 5;
	part->keys_step= 5;
	part->draw_step= 2;
	part->ren_step= 3;
	part->adapt_angle= 5;
	part->adapt_pix= 3;
	part->kink_axis= 2;
	part->reactevent= PART_EVENT_DEATH;
	part->disp=100;
	part->from= PART_FROM_FACE;

	part->normfac= 1.0f;

	part->reactshape=1.0f;

	part->mass=1.0;
	part->size=0.05;
	part->childsize=1.0;

	part->rotmode = PART_ROT_VEL;
	part->avemode = PART_AVE_SPIN;

	part->child_nbr=10;
	part->ren_child_nbr=100;
	part->childrad=0.2f;
	part->childflat=0.0f;
	part->clumppow=0.0f;
	part->kink_amp=0.2f;
	part->kink_freq=2.0;

	part->rough1_size=1.0;
	part->rough2_size=1.0;
	part->rough_end_shape=1.0;

	part->clength=1.0f;
	part->clength_thres=0.0f;

	part->draw= PART_DRAW_EMITTER|PART_DRAW_MAT_COL;
	part->draw_line[0]=0.5;
	part->path_start = 0.0f;
	part->path_end = 1.0f;

	part->keyed_loops = 1;

#if 0 // XXX old animation system
	part->ipo = NULL;
#endif // XXX old animation system

	part->simplify_refsize= 1920;
	part->simplify_rate= 1.0f;
	part->simplify_transition= 0.1f;
	part->simplify_viewport= 0.8;

	if(!part->effector_weights)
		part->effector_weights = BKE_add_effector_weights(NULL);
}


ParticleSettings *psys_new_settings(char *name, Main *main)
{
	ParticleSettings *part;

	if(main==NULL)
		main = G.main;

	part= alloc_libblock(&main->particle, ID_PA, name);
	
	default_particle_settings(part);

	return part;
}

ParticleSettings *psys_copy_settings(ParticleSettings *part)
{
	ParticleSettings *partn;
	
	partn= copy_libblock(part);
	if(partn->pd) partn->pd= MEM_dupallocN(part->pd);
	if(partn->pd2) partn->pd2= MEM_dupallocN(part->pd2);

	partn->boids = boid_copy_settings(part->boids);
	
	return partn;
}

void make_local_particlesettings(ParticleSettings *part)
{
	Object *ob;
	ParticleSettings *par;
	int local=0, lib=0;

	/* - only lib users: do nothing
	    * - only local users: set flag
	    * - mixed: make copy
	    */
	
	if(part->id.lib==0) return;
	if(part->id.us==1) {
		part->id.lib= 0;
		part->id.flag= LIB_LOCAL;
		new_id(0, (ID *)part, 0);
		return;
	}
	
	/* test objects */
	ob= G.main->object.first;
	while(ob) {
		ParticleSystem *psys=ob->particlesystem.first;
		for(; psys; psys=psys->next){
			if(psys->part==part) {
				if(ob->id.lib) lib= 1;
				else local= 1;
			}
		}
		ob= ob->id.next;
	}
	
	if(local && lib==0) {
		part->id.lib= 0;
		part->id.flag= LIB_LOCAL;
		new_id(0, (ID *)part, 0);
	}
	else if(local && lib) {
		
		par= psys_copy_settings(part);
		par->id.us= 0;
		
		/* do objects */
		ob= G.main->object.first;
		while(ob) {
			ParticleSystem *psys=ob->particlesystem.first;
			for(; psys; psys=psys->next){
				if(psys->part==part && ob->id.lib==0) {
					psys->part= par;
					par->id.us++;
					part->id.us--;
				}
			}
			ob= ob->id.next;
		}
	}
}

/************************************************/
/*			Textures							*/
/************************************************/

static int get_particle_uv(DerivedMesh *dm, ParticleData *pa, int face_index, float *fuv, char *name, float *texco)
{
	MFace *mf;
	MTFace *tf;
	int i;
	
	tf= CustomData_get_layer_named(&dm->faceData, CD_MTFACE, name);

	if(tf == NULL)
		tf= CustomData_get_layer(&dm->faceData, CD_MTFACE);

	if(tf == NULL)
		return 0;

	if(pa) {
		i= (pa->num_dmcache==DMCACHE_NOTFOUND)? pa->num: pa->num_dmcache;
		if(i >= dm->getNumFaces(dm))
			i = -1;
	}
	else
		i= face_index;

	if (i==-1) {
		texco[0]= 0.0f;
		texco[1]= 0.0f;
		texco[2]= 0.0f;
	}
	else {
		mf= dm->getFaceData(dm, i, CD_MFACE);

		psys_interpolate_uvs(&tf[i], mf->v4, fuv, texco);

		texco[0]= texco[0]*2.0f - 1.0f;
		texco[1]= texco[1]*2.0f - 1.0f;
		texco[2]= 0.0f;
	}

	return 1;
}

static void get_cpa_texture(DerivedMesh *dm, Material *ma, int face_index, float *fw, float *orco, ParticleTexture *ptex, int event)
{
	MTex *mtex;
	int m,setvars=0;
	float value, rgba[4], texco[3];

	if(ma) for(m=0; m<MAX_MTEX; m++){
		mtex=ma->mtex[m];
		if(mtex && (ma->septex & (1<<m))==0 && mtex->pmapto){
			float def=mtex->def_var;
			short blend=mtex->blendtype;

			if((mtex->texco & TEXCO_UV) && fw) {
				if(!get_particle_uv(dm, NULL, face_index, fw, mtex->uvname, texco))
					VECCOPY(texco,orco);
			}
			else
				VECCOPY(texco,orco);

			externtex(mtex, texco, &value, rgba, rgba+1, rgba+2, rgba+3);
			if((event & mtex->pmapto) & MAP_PA_TIME){
				if((setvars&MAP_PA_TIME)==0){
					ptex->time=0.0;
					setvars|=MAP_PA_TIME;
				}
				ptex->time= texture_value_blend(mtex->def_var,ptex->time,value,mtex->timefac,blend);
			}
			if((event & mtex->pmapto) & MAP_PA_LENGTH)
				ptex->length= texture_value_blend(def,ptex->length,value,mtex->lengthfac,blend);
			if((event & mtex->pmapto) & MAP_PA_CLUMP)
				ptex->clump= texture_value_blend(def,ptex->clump,value,mtex->clumpfac,blend);
			if((event & mtex->pmapto) & MAP_PA_KINK)
				ptex->kink= texture_value_blend(def,ptex->kink,value,mtex->kinkfac,blend);
			if((event & mtex->pmapto) & MAP_PA_ROUGH)
				ptex->rough1= ptex->rough2= ptex->roughe= texture_value_blend(def,ptex->rough1,value,mtex->roughfac,blend);
			if((event & mtex->pmapto) & MAP_PA_DENS)
				ptex->exist= texture_value_blend(def,ptex->exist,value,mtex->padensfac,blend);
		}
	}
	if(event & MAP_PA_TIME) { CLAMP(ptex->time,0.0,1.0); }
	if(event & MAP_PA_LENGTH) { CLAMP(ptex->length,0.0,1.0); }
	if(event & MAP_PA_CLUMP) { CLAMP(ptex->clump,0.0,1.0); }
	if(event & MAP_PA_KINK) { CLAMP(ptex->kink,0.0,1.0); }
	if(event & MAP_PA_ROUGH) {
		CLAMP(ptex->rough1,0.0,1.0);
		CLAMP(ptex->rough2,0.0,1.0);
		CLAMP(ptex->roughe,0.0,1.0);
	}
	if(event & MAP_PA_DENS) { CLAMP(ptex->exist,0.0,1.0); }
}
void psys_get_texture(ParticleSimulationData *sim, Material *ma, ParticleData *pa, ParticleTexture *ptex, int event)
{
	MTex *mtex;
	int m;
	float value, rgba[4], co[3], texco[3];
	int setvars=0;

	if(ma) for(m=0; m<MAX_MTEX; m++){
		mtex=ma->mtex[m];
		if(mtex && (ma->septex & (1<<m))==0 && mtex->pmapto){
			float def=mtex->def_var;
			short blend=mtex->blendtype;

			if((mtex->texco & TEXCO_UV) && ELEM(sim->psys->part->from, PART_FROM_FACE, PART_FROM_VOLUME)) {
				if(!get_particle_uv(sim->psmd->dm, pa, 0, pa->fuv, mtex->uvname, texco)) {
					/* failed to get uv's, let's try orco's */
					psys_particle_on_emitter(sim->psmd,sim->psys->part->from,pa->num,pa->num_dmcache,pa->fuv,pa->foffset,co,0,0,0,texco, 0);
				}
			}
			else {
				psys_particle_on_emitter(sim->psmd,sim->psys->part->from,pa->num,pa->num_dmcache,pa->fuv,pa->foffset,co,0,0,0,texco, 0);
			}

			externtex(mtex, texco, &value, rgba, rgba+1, rgba+2, rgba+3);

			if((event & mtex->pmapto) & MAP_PA_TIME){
				/* the first time has to set the base value for time regardless of blend mode */
				if((setvars&MAP_PA_TIME)==0){
					int flip= (mtex->timefac < 0.0f);
					float timefac= fabsf(mtex->timefac);
					ptex->time *= 1.0f - timefac;
					ptex->time += timefac * ((flip)? 1.0f - value : value);
					setvars |= MAP_PA_TIME;
				}
				else
					ptex->time= texture_value_blend(def,ptex->time,value,mtex->timefac,blend);
			}
			if((event & mtex->pmapto) & MAP_PA_LIFE)
				ptex->life= texture_value_blend(def,ptex->life,value,mtex->lifefac,blend);
			if((event & mtex->pmapto) & MAP_PA_DENS)
				ptex->exist= texture_value_blend(def,ptex->exist,value,mtex->padensfac,blend);
			if((event & mtex->pmapto) & MAP_PA_SIZE)
				ptex->size= texture_value_blend(def,ptex->size,value,mtex->sizefac,blend);
			if((event & mtex->pmapto) & MAP_PA_IVEL)
				ptex->ivel= texture_value_blend(def,ptex->ivel,value,mtex->ivelfac,blend);
			if((event & mtex->pmapto) & MAP_PA_PVEL)
				texture_rgb_blend(ptex->pvel,rgba,ptex->pvel,value,mtex->pvelfac,blend);
			if((event & mtex->pmapto) & MAP_PA_LENGTH)
				ptex->length= texture_value_blend(def,ptex->length,value,mtex->lengthfac,blend);
			if((event & mtex->pmapto) & MAP_PA_CLUMP)
				ptex->clump= texture_value_blend(def,ptex->clump,value,mtex->clumpfac,blend);
			if((event & mtex->pmapto) & MAP_PA_KINK)
				ptex->kink= texture_value_blend(def,ptex->kink,value,mtex->kinkfac,blend);
		}
	}
	if(event & MAP_PA_TIME) { CLAMP(ptex->time,0.0,1.0); }
	if(event & MAP_PA_LIFE) { CLAMP(ptex->life,0.0,1.0); }
	if(event & MAP_PA_DENS) { CLAMP(ptex->exist,0.0,1.0); }
	if(event & MAP_PA_SIZE) { CLAMP(ptex->size,0.0,1.0); }
	if(event & MAP_PA_IVEL) { CLAMP(ptex->ivel,0.0,1.0); }
	if(event & MAP_PA_LENGTH) { CLAMP(ptex->length,0.0,1.0); }
	if(event & MAP_PA_CLUMP) { CLAMP(ptex->clump,0.0,1.0); }
	if(event & MAP_PA_KINK) { CLAMP(ptex->kink,0.0,1.0); }
}
/************************************************/
/*			Particle State						*/
/************************************************/
float psys_get_timestep(ParticleSimulationData *sim)
{
	return 0.04f * sim->psys->part->timetweak;
}
float psys_get_child_time(ParticleSystem *psys, ChildParticle *cpa, float cfra, float *birthtime, float *dietime)
{
	ParticleSettings *part = psys->part;
	float time, life;

	if(part->childtype==PART_CHILD_FACES){
		int w=0;
		time=0.0;
		while(w<4 && cpa->pa[w]>=0){
			time+=cpa->w[w]*(psys->particles+cpa->pa[w])->time;
			w++;
		}

		life = part->lifetime * (1.0f - part->randlife * PSYS_FRAND(cpa - psys->child + 25));
	}
	else{
		ParticleData *pa = psys->particles + cpa->parent;

		time = pa->time;
		life = pa->lifetime;
	}

	if(birthtime)
		*birthtime = time;
	if(dietime)
		*dietime = time+life;

	return (cfra-time)/life;
}
float psys_get_child_size(ParticleSystem *psys, ChildParticle *cpa, float cfra, float *pa_time)
{
	ParticleSettings *part = psys->part;
	float size; // time XXX
	
	if(part->childtype==PART_CHILD_FACES){
		size=part->size;

#if 0 // XXX old animation system
		if((part->flag&PART_ABS_TIME)==0 && part->ipo){
			IpoCurve *icu;

			if(pa_time)
				time=*pa_time;
			else
				time=psys_get_child_time(psys,cpa,cfra,NULL,NULL);

			/* correction for lifetime */
			calc_ipo(part->ipo, 100*time);

			for(icu = part->ipo->curve.first; icu; icu=icu->next) {
				if(icu->adrcode == PART_SIZE)
					size = icu->curval;
			}
		}
#endif // XXX old animation system
	}
	else
		size=psys->particles[cpa->parent].size;

	size*=part->childsize;

	if(part->childrandsize!=0.0)
		size *= 1.0f - part->childrandsize * PSYS_FRAND(cpa - psys->child + 26);

	return size;
}
static void get_child_modifier_parameters(ParticleSettings *part, ParticleThreadContext *ctx, ChildParticle *cpa, short cpa_from, int cpa_num, float *cpa_fuv, float *orco, ParticleTexture *ptex)
{
	ParticleSystem *psys = ctx->sim.psys;
	int i = cpa - psys->child;

	ptex->length= 1.0f - part->randlength * PSYS_FRAND(i + 26);
	ptex->clump=1.0;
	ptex->kink=1.0;
	ptex->rough1= 1.0;
	ptex->rough2= 1.0;
	ptex->roughe= 1.0;
	ptex->exist= 1.0;
	ptex->effector= 1.0;

	ptex->length*= part->clength_thres < PSYS_FRAND(i + 27) ? part->clength : 1.0f;

	get_cpa_texture(ctx->dm,ctx->ma,cpa_num,cpa_fuv,orco,ptex,
		MAP_PA_DENS|MAP_PA_LENGTH|MAP_PA_CLUMP|MAP_PA_KINK|MAP_PA_ROUGH);


	if(ptex->exist < PSYS_FRAND(i + 24))
		return;

	if(ctx->vg_length)
		ptex->length*=psys_interpolate_value_from_verts(ctx->dm,cpa_from,cpa_num,cpa_fuv,ctx->vg_length);
	if(ctx->vg_clump)
		ptex->clump*=psys_interpolate_value_from_verts(ctx->dm,cpa_from,cpa_num,cpa_fuv,ctx->vg_clump);
	if(ctx->vg_kink)
		ptex->kink*=psys_interpolate_value_from_verts(ctx->dm,cpa_from,cpa_num,cpa_fuv,ctx->vg_kink);
	if(ctx->vg_rough1)
		ptex->rough1*=psys_interpolate_value_from_verts(ctx->dm,cpa_from,cpa_num,cpa_fuv,ctx->vg_rough1);
	if(ctx->vg_rough2)
		ptex->rough2*=psys_interpolate_value_from_verts(ctx->dm,cpa_from,cpa_num,cpa_fuv,ctx->vg_rough2);
	if(ctx->vg_roughe)
		ptex->roughe*=psys_interpolate_value_from_verts(ctx->dm,cpa_from,cpa_num,cpa_fuv,ctx->vg_roughe);
	if(ctx->vg_effector)
		ptex->effector*=psys_interpolate_value_from_verts(ctx->dm,cpa_from,cpa_num,cpa_fuv,ctx->vg_effector);
}
static void do_child_modifiers(ParticleSimulationData *sim, ParticleTexture *ptex, ParticleKey *par, float *par_rot, ChildParticle *cpa, float *orco, float mat[4][4], ParticleKey *state, float t)
{
	ParticleSettings *part = sim->psys->part;
	int i = cpa - sim->psys->child;
	int guided = 0;

	if(part->flag & PART_CHILD_EFFECT)
		/* state is safe to cast, since only co and vel are used */
		guided = do_guides(sim->psys->effectors, (ParticleKey*)state, cpa->parent, t);

	if(guided==0){
		if(part->kink)
			do_prekink(state, par, par_rot, t, part->kink_freq * ptex->kink, part->kink_shape,
			part->kink_amp, part->kink, part->kink_axis, sim->ob->obmat);
				
		do_clump(state, par, t, part->clumpfac, part->clumppow, ptex->clump);
	}

	if(part->rough1 != 0.0 && ptex->rough1 != 0.0)
		do_rough(orco, mat, t, ptex->rough1*part->rough1, part->rough1_size, 0.0, state);

	if(part->rough2 != 0.0 && ptex->rough2 != 0.0)
		do_rough(sim->psys->frand + ((i + 27) % (PSYS_FRAND_COUNT - 3)), mat, t, ptex->rough2*part->rough2, part->rough2_size, part->rough2_thres, state);

	if(part->rough_end != 0.0 && ptex->roughe != 0.0)
		do_rough_end(sim->psys->frand + ((i + 27) % (PSYS_FRAND_COUNT - 3)), mat, t, ptex->roughe*part->rough_end, part->rough_end_shape, state);
}
/* get's hair (or keyed) particles state at the "path time" specified in state->time */
void psys_get_particle_on_path(ParticleSimulationData *sim, int p, ParticleKey *state, int vel)
{
	PARTICLE_PSMD;
	ParticleSystem *psys = sim->psys;
	ParticleSettings *part = sim->psys->part;
	Material *ma = give_current_material(sim->ob, part->omat);
	ParticleData *pa;
	ChildParticle *cpa;
	ParticleTexture ptex;
	ParticleKey *par=0, keys[4], tstate;
	ParticleThreadContext ctx; /* fake thread context for child modifiers */
	ParticleInterpolationData pind;

	float t, frs_sec = sim->scene->r.frs_sec;
	float co[3], orco[3];
	float hairmat[4][4];
	int totparent = 0;
	int totpart = psys->totpart;
	int totchild = psys->totchild;
	short between = 0, edit = 0;

	int keyed = part->phystype & PART_PHYS_KEYED && psys->flag & PSYS_KEYED;
	int cached = !keyed && part->type != PART_HAIR;

	float *cpa_fuv; int cpa_num; short cpa_from;

	//if(psys_in_edit_mode(scene, psys)){
	//	if((psys->edit_path->flag & PSYS_EP_SHOW_CHILD)==0)
	//		totchild=0;
	//	edit=1;
	//}

	t=state->time;
	CLAMP(t, 0.0, 1.0);

	if(p<totpart){
		pa = psys->particles + p;
		pind.keyed = keyed;
		pind.cache = cached ? psys->pointcache : NULL;
		pind.epoint = NULL;
		pind.bspline = (psys->part->flag & PART_HAIR_BSPLINE);
		pind.dm = psys->hair_out_dm;
		init_particle_interpolation(sim->ob, psys, pa, &pind);
		do_particle_interpolation(psys, p, pa, t, frs_sec, &pind, state);

		if(!keyed && !cached) {
			if((pa->flag & PARS_REKEY)==0) {
				psys_mat_hair_to_global(sim->ob, sim->psmd->dm, part->from, pa, hairmat);
				mul_m4_v3(hairmat, state->co);
				mul_mat3_m4_v3(hairmat, state->vel);

				if(sim->psys->effectors && (part->flag & PART_CHILD_GUIDE)==0) {
					do_guides(sim->psys->effectors, state, p, state->time);
					/* TODO: proper velocity handling */
				}

				if(psys->lattice && edit==0)
					calc_latt_deform(psys->lattice, state->co,1.0f);
			}
		}
	}
	else if(totchild){
		//invert_m4_m4(imat,ob->obmat);

		cpa=psys->child+p-totpart;

		if(state->time < 0.0f)
			t = psys_get_child_time(psys, cpa, -state->time, NULL, NULL);
		
		if(totchild && part->from!=PART_FROM_PARTICLE && part->childtype==PART_CHILD_FACES){
			totparent=(int)(totchild*part->parents*0.3);
			
			if(G.rendering && part->child_nbr && part->ren_child_nbr)
				totparent*=(float)part->child_nbr/(float)part->ren_child_nbr;
			
			/* part->parents could still be 0 so we can't test with totparent */
			between=1;
		}
		if(between){
			int w = 0;
			float foffset;

			/* get parent states */
			while(w<4 && cpa->pa[w]>=0){
				keys[w].time = state->time;
				psys_get_particle_on_path(sim, cpa->pa[w], keys+w, 1);
				w++;
			}

			/* get the original coordinates (orco) for texture usage */
			cpa_num=cpa->num;
			
			foffset= cpa->foffset;
			cpa_fuv = cpa->fuv;
			cpa_from = PART_FROM_FACE;

			psys_particle_on_emitter(psmd,cpa_from,cpa_num,DMCACHE_ISCHILD,cpa->fuv,foffset,co,0,0,0,orco,0);

			/* we need to save the actual root position of the child for positioning it accurately to the surface of the emitter */
			//VECCOPY(cpa_1st,co);

			//mul_m4_v3(ob->obmat,cpa_1st);

			pa = psys->particles + cpa->parent;

			psys_mat_hair_to_global(sim->ob, sim->psmd->dm, psys->part->from, pa, hairmat);

			pa=0;
		}
		else{
			/* get the parent state */
			keys->time = state->time;
			psys_get_particle_on_path(sim, cpa->parent, keys,1);

			/* get the original coordinates (orco) for texture usage */
			pa=psys->particles+cpa->parent;

			cpa_from=part->from;
			cpa_num=pa->num;
			cpa_fuv=pa->fuv;

			psys_particle_on_emitter(psmd,cpa_from,cpa_num,DMCACHE_ISCHILD,cpa_fuv,pa->foffset,co,0,0,0,orco,0);

			psys_mat_hair_to_global(sim->ob, sim->psmd->dm, psys->part->from, pa, hairmat);
		}

		/* correct child ipo timing */
#if 0 // XXX old animation system
		if((part->flag&PART_ABS_TIME)==0 && part->ipo){
			calc_ipo(part->ipo, 100.0f*t);
			execute_ipo((ID *)part, part->ipo);
		}
#endif // XXX old animation system
		
		/* get different child parameters from textures & vgroups */
		memset(&ctx, 0, sizeof(ParticleThreadContext));
		ctx.sim = *sim;
		ctx.dm = psmd->dm;
		ctx.ma = ma;
		/* TODO: assign vertex groups */
		get_child_modifier_parameters(part, &ctx, cpa, cpa_from, cpa_num, cpa_fuv, orco, &ptex);

		if(between){
			int w=0;

			state->co[0] = state->co[1] = state->co[2] = 0.0f;
			state->vel[0] = state->vel[1] = state->vel[2] = 0.0f;

			/* child position is the weighted sum of parent positions */
			while(w<4 && cpa->pa[w]>=0){
				state->co[0] += cpa->w[w] * keys[w].co[0];
				state->co[1] += cpa->w[w] * keys[w].co[1];
				state->co[2] += cpa->w[w] * keys[w].co[2];

				state->vel[0] += cpa->w[w] * keys[w].vel[0];
				state->vel[1] += cpa->w[w] * keys[w].vel[1];
				state->vel[2] += cpa->w[w] * keys[w].vel[2];
				w++;
			}
			/* apply offset for correct positioning */
			//VECADD(state->co,state->co,cpa_1st);
		}
		else{
			/* offset the child from the parent position */
			offset_child(cpa, keys, state, part->childflat, part->childrad);
		}

		par = keys;

		if(vel)
			copy_particle_key(&tstate, state, 1);

		/* apply different deformations to the child path */
		do_child_modifiers(sim, &ptex, par, par->rot, cpa, orco, hairmat, state, t);

		/* try to estimate correct velocity */
		if(vel){
			ParticleKey tstate;
			float length = len_v3(state->vel);

			if(t>=0.001f){
				tstate.time=t-0.001f;
				psys_get_particle_on_path(sim,p,&tstate,0);
				VECSUB(state->vel,state->co,tstate.co);
				normalize_v3(state->vel);
			}
			else{
				tstate.time=t+0.001f;
				psys_get_particle_on_path(sim,p,&tstate,0);
				VECSUB(state->vel,tstate.co,state->co);
				normalize_v3(state->vel);
			}

			mul_v3_fl(state->vel, length);
		}
	}
}
/* gets particle's state at a time, returns 1 if particle exists and can be seen and 0 if not */
int psys_get_particle_state(ParticleSimulationData *sim, int p, ParticleKey *state, int always){
	ParticleSystem *psys = sim->psys;
	ParticleSettings *part = psys->part;
	ParticleData *pa = NULL;
	ChildParticle *cpa = NULL;
	float cfra;
	int totpart = psys->totpart;
	float timestep = psys_get_timestep(sim);

	/* negative time means "use current time" */
	cfra = state->time > 0 ? state->time : bsystem_time(sim->scene, 0, (float)sim->scene->r.cfra, 0.0);

	if(p>=totpart){
		if(!psys->totchild)
			return 0;

		if(part->from != PART_FROM_PARTICLE && part->childtype == PART_CHILD_FACES){
			if(!(psys->flag & PSYS_KEYED))
				return 0;

			cpa = psys->child + p - totpart;

			state->time = psys_get_child_time(psys, cpa, cfra, NULL, NULL);

			if(!always)
				if((state->time < 0.0 && !(part->flag & PART_UNBORN))
					|| (state->time > 1.0 && !(part->flag & PART_DIED)))
					return 0;

			state->time= (cfra - (part->sta + (part->end - part->sta) * PSYS_FRAND(p + 23))) / (part->lifetime * PSYS_FRAND(p + 24));

			psys_get_particle_on_path(sim, p, state,1);
			return 1;
		}
		else {
			cpa = sim->psys->child + p - totpart;
			pa = sim->psys->particles + cpa->parent;
		}
	}
	else {
		pa = sim->psys->particles + p;
	}

	if(pa) {
		if(!always)
			if((pa->alive==PARS_UNBORN && (part->flag & PART_UNBORN)==0)
				|| (pa->alive==PARS_DEAD && (part->flag & PART_DIED)==0))
				return 0;

		state->time = MIN2(state->time, pa->dietime);
	}

	if(sim->psys->flag & PSYS_KEYED){
		state->time= -cfra;
		psys_get_particle_on_path(sim, p, state,1);
		return 1;
	}
	else{
		if(cpa){
			ParticleKey *key1;
			float t = (cfra - pa->time + pa->loop * pa->lifetime) / pa->lifetime;

			key1=&pa->state;
			offset_child(cpa, key1, state, part->childflat, part->childrad);
			
			CLAMP(t,0.0,1.0);
			if(part->kink)			/* TODO: part->kink_freq*pa_kink */
				do_prekink(state,key1,key1->rot,t,part->kink_freq,part->kink_shape,part->kink_amp,part->kink,part->kink_axis,sim->ob->obmat);
			
			/* TODO: pa_clump vgroup */
			do_clump(state,key1,t,part->clumpfac,part->clumppow,1.0);

			if(psys->lattice)
				calc_latt_deform(sim->psys->lattice, state->co,1.0f);
		}
		else{
			if(pa->state.time==state->time || ELEM(part->phystype,PART_PHYS_NO,PART_PHYS_KEYED)
				|| pa->prev_state.time <= 0.0f)
				copy_particle_key(state, &pa->state, 1);
			else if(pa->prev_state.time==state->time)
				copy_particle_key(state, &pa->prev_state, 1);
			else {
				/* let's interpolate to try to be as accurate as possible */
				if(pa->state.time + 2.0f > state->time && pa->prev_state.time - 2.0f < state->time) {
					ParticleKey keys[4];
					float dfra, keytime, frs_sec = sim->scene->r.frs_sec;

					if(pa->prev_state.time >= pa->state.time) {
						/* prev_state is wrong so let's not use it, this can happen at frame 1 or particle birth */
						copy_particle_key(state, &pa->state, 1);

						VECADDFAC(state->co, state->co, state->vel, (state->time-pa->state.time)/frs_sec);
					}
					else {
						copy_particle_key(keys+1, &pa->prev_state, 1);
						copy_particle_key(keys+2, &pa->state, 1);

						dfra = keys[2].time - keys[1].time;

						keytime = (state->time - keys[1].time) / dfra;

						/* convert velocity to timestep size */
						mul_v3_fl(keys[1].vel, dfra * timestep);
						mul_v3_fl(keys[2].vel, dfra * timestep);
						
						psys_interpolate_particle(-1, keys, keytime, state, 1);
						
						/* convert back to real velocity */
						mul_v3_fl(state->vel, 1.0f / (dfra * timestep));

						interp_v3_v3v3(state->ave, keys[1].ave, keys[2].ave, keytime);
						interp_qt_qtqt(state->rot, keys[1].rot, keys[2].rot, keytime);
					}
				}
				else {
					/* extrapolating over big ranges is not accurate so let's just give something close to reasonable back */
					copy_particle_key(state, &pa->state, 0);
				}
			}

			if(sim->psys->lattice)
				calc_latt_deform(sim->psys->lattice, state->co,1.0f);
		}
		
		return 1;
	}
}

void psys_get_dupli_texture(Object *ob, ParticleSettings *part, ParticleSystemModifierData *psmd, ParticleData *pa, ChildParticle *cpa, float *uv, float *orco)
{
	MFace *mface;
	MTFace *mtface;
	float loc[3];
	int num;

	if(cpa) {
		if(part->childtype == PART_CHILD_FACES) {
			mtface= CustomData_get_layer(&psmd->dm->faceData, CD_MTFACE);
			if(mtface) {
				mface= psmd->dm->getFaceData(psmd->dm, cpa->num, CD_MFACE);
				mtface += cpa->num;
				psys_interpolate_uvs(mtface, mface->v4, cpa->fuv, uv);
			}
			else
				uv[0]= uv[1]= 0.0f;
		}
		else
			uv[0]= uv[1]= 0.0f;

		psys_particle_on_emitter(psmd,
			(part->childtype == PART_CHILD_FACES)? PART_FROM_FACE: PART_FROM_PARTICLE,
			cpa->num,DMCACHE_ISCHILD,cpa->fuv,cpa->foffset,loc,0,0,0,orco,0);
	}
	else {
		if(part->from == PART_FROM_FACE) {
			mtface= CustomData_get_layer(&psmd->dm->faceData, CD_MTFACE);
			num= pa->num_dmcache;

			if(num == DMCACHE_NOTFOUND)
				if(pa->num < psmd->dm->getNumFaces(psmd->dm))
					num= pa->num;

			if(mtface && num != DMCACHE_NOTFOUND) {
				mface= psmd->dm->getFaceData(psmd->dm, num, CD_MFACE);
				mtface += num;
				psys_interpolate_uvs(mtface, mface->v4, pa->fuv, uv);
			}
			else
				uv[0]= uv[1]= 0.0f;
		}
		else
			uv[0]= uv[1]= 0.0f;

		psys_particle_on_emitter(psmd,part->from,pa->num,pa->num_dmcache,pa->fuv,pa->foffset,loc,0,0,0,orco,0);
	}
}

void psys_get_dupli_path_transform(ParticleSimulationData *sim, ParticleData *pa, ChildParticle *cpa, ParticleCacheKey *cache, float mat[][4], float *scale)
{
	Object *ob = sim->ob;
	ParticleSystem *psys = sim->psys;
	ParticleSystemModifierData *psmd = sim->psmd;
	float loc[3], nor[3], vec[3], side[3], len, obrotmat[4][4], qmat[4][4];
	float xvec[3] = {-1.0, 0.0, 0.0}, q[4];

	sub_v3_v3v3(vec, (cache+cache->steps-1)->co, cache->co);
	len= normalize_v3(vec);

	if(pa)
		psys_particle_on_emitter(psmd,sim->psys->part->from,pa->num,pa->num_dmcache,pa->fuv,pa->foffset,loc,nor,0,0,0,0);
	else
		psys_particle_on_emitter(psmd,
			(psys->part->childtype == PART_CHILD_FACES)? PART_FROM_FACE: PART_FROM_PARTICLE,
			cpa->num,DMCACHE_ISCHILD,cpa->fuv,cpa->foffset,loc,nor,0,0,0,0);
	
	if(psys->part->rotmode) {
		if(!pa)
			pa= psys->particles+cpa->pa[0];

		vec_to_quat( q,xvec, ob->trackflag, ob->upflag);
		quat_to_mat4( obrotmat,q);
		obrotmat[3][3]= 1.0f;

		quat_to_mat4( qmat,pa->state.rot);
		mul_m4_m4m4(mat, obrotmat, qmat);
	}
	else {
		/* make sure that we get a proper side vector */
		if(fabs(dot_v3v3(nor,vec))>0.999999) {
			if(fabs(dot_v3v3(nor,xvec))>0.999999) {
				nor[0] = 0.0f;
				nor[1] = 1.0f;
				nor[2] = 0.0f;
			}
			else {
				nor[0] = 1.0f;
				nor[1] = 0.0f;
				nor[2] = 0.0f;
			}
		}
		cross_v3_v3v3(side, nor, vec);
		normalize_v3(side);
		cross_v3_v3v3(nor, vec, side);

 		unit_m4(mat);
		VECCOPY(mat[0], vec);
		VECCOPY(mat[1], side);
		VECCOPY(mat[2], nor);
	}

	*scale= len;
}

void psys_make_billboard(ParticleBillboardData *bb, float xvec[3], float yvec[3], float zvec[3], float center[3])
{
	float onevec[3] = {0.0f,0.0f,0.0f}, tvec[3], tvec2[3];

	xvec[0] = 1.0f; xvec[1] = 0.0f; xvec[2] = 0.0f;
	yvec[0] = 0.0f; yvec[1] = 1.0f; yvec[2] = 0.0f;

	if(bb->align < PART_BB_VIEW)
		onevec[bb->align]=1.0f;

	if(bb->lock && (bb->align == PART_BB_VIEW)) {
		VECCOPY(xvec, bb->ob->obmat[0]);
		normalize_v3(xvec);

		VECCOPY(yvec, bb->ob->obmat[1]);
		normalize_v3(yvec);

		VECCOPY(zvec, bb->ob->obmat[2]);
		normalize_v3(zvec);
	}
	else if(bb->align == PART_BB_VEL) {
		float temp[3];

		VECCOPY(temp, bb->vel);
		normalize_v3(temp);

		VECSUB(zvec, bb->ob->obmat[3], bb->vec);

		if(bb->lock) {
			float fac = -dot_v3v3(zvec, temp);

			VECADDFAC(zvec, zvec, temp, fac);
		}
		normalize_v3(zvec);

		cross_v3_v3v3(xvec,temp,zvec);
		normalize_v3(xvec);

		cross_v3_v3v3(yvec,zvec,xvec);
	}
	else {
		VECSUB(zvec, bb->ob->obmat[3], bb->vec);
		if(bb->lock)
			zvec[bb->align] = 0.0f;
		normalize_v3(zvec);

		if(bb->align < PART_BB_VIEW)
			cross_v3_v3v3(xvec, onevec, zvec);
		else
			cross_v3_v3v3(xvec, bb->ob->obmat[1], zvec);
		normalize_v3(xvec);

		cross_v3_v3v3(yvec,zvec,xvec);
	}

	VECCOPY(tvec, xvec);
	VECCOPY(tvec2, yvec);

	mul_v3_fl(xvec, cos(bb->tilt * (float)M_PI));
	mul_v3_fl(tvec2, sin(bb->tilt * (float)M_PI));
	VECADD(xvec, xvec, tvec2);

	mul_v3_fl(yvec, cos(bb->tilt * (float)M_PI));
	mul_v3_fl(tvec, -sin(bb->tilt * (float)M_PI));
	VECADD(yvec, yvec, tvec);

	mul_v3_fl(xvec, bb->size);
	mul_v3_fl(yvec, bb->size);

	VECADDFAC(center, bb->vec, xvec, bb->offset[0]);
	VECADDFAC(center, center, yvec, bb->offset[1]);
}

