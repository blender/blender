/* particle.c
 *
 *
 * $Id: particle.c $
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

#include "DNA_scene_types.h"
#include "DNA_particle_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_force.h"
#include "DNA_texture_types.h"
#include "DNA_material_types.h"
#include "DNA_object_types.h"
#include "DNA_curve_types.h"
#include "DNA_key_types.h"

#include "BLI_arithb.h"
#include "BLI_blenlib.h"
#include "BLI_dynstr.h"
#include "BLI_kdtree.h"
#include "BLI_linklist.h"
#include "BLI_rand.h"
#include "BLI_threads.h"

#include "BKE_anim.h"

#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_lattice.h"
#include "BKE_utildefines.h"
#include "BKE_displist.h"
#include "BKE_particle.h"
#include "BKE_DerivedMesh.h"
#include "BKE_ipo.h"
#include "BKE_object.h"
#include "BKE_softbody.h"
#include "BKE_material.h"
#include "BKE_key.h"
#include "BKE_library.h"
#include "BKE_depsgraph.h"
#include "BKE_bad_level_calls.h"
#include "BKE_modifier.h"
#include "BKE_mesh.h"
#include "BKE_cdderivedmesh.h"
#include "BKE_pointcache.h"

#include "blendef.h"
#include "RE_render_ext.h"

static void key_from_object(Object *ob, ParticleKey *key);
static void get_cpa_texture(DerivedMesh *dm, Material *ma, int face_index,
				float *fuv, float *orco, ParticleTexture *ptex,	int event);

/* few helpers for countall etc. */
int count_particles(ParticleSystem *psys){
	ParticleSettings *part=psys->part;
	ParticleData *pa;
	int tot=0,p;

	for(p=0,pa=psys->particles; p<psys->totpart; p++,pa++){
		if(pa->alive == PARS_KILLED);
		else if(pa->alive == PARS_UNBORN && (part->flag & PART_UNBORN)==0);
		else if(pa->alive == PARS_DEAD && (part->flag & PART_DIED)==0);
		else if(pa->flag & (PARS_UNEXIST+PARS_NO_DISP));
		else tot++;
	}
	return tot;
}
int count_particles_mod(ParticleSystem *psys, int totgr, int cur){
	ParticleSettings *part=psys->part;
	ParticleData *pa;
	int tot=0,p;

	for(p=0,pa=psys->particles; p<psys->totpart; p++,pa++){
		if(pa->alive == PARS_KILLED);
		else if(pa->alive == PARS_UNBORN && (part->flag & PART_UNBORN)==0);
		else if(pa->alive == PARS_DEAD && (part->flag & PART_DIED)==0);
		else if(pa->flag & (PARS_UNEXIST+PARS_NO_DISP));
		else if(p%totgr==cur) tot++;
	}
	return tot;
}
int psys_count_keys(ParticleSystem *psys)
{
	ParticleData *pa;
	int i, totpart=psys->totpart, totkey=0;

	for(i=0, pa=psys->particles; i<totpart; i++, pa++)
		totkey += pa->totkey;

	return totkey;
}
/* remember to free the pointer returned from this! */
char *psys_menu_string(Object *ob, int for_sb)
{
	ParticleSystem *psys;
	DynStr *ds;
	char *str, num[6];
	int i;

	ds = BLI_dynstr_new();

	if(for_sb)
		BLI_dynstr_append(ds, "|Object%x-1");
	
	for(i=0,psys=ob->particlesystem.first; psys; i++,psys=psys->next){

		BLI_dynstr_append(ds, "|");
		sprintf(num,"%i. ",i+1);
		BLI_dynstr_append(ds, num);
		BLI_dynstr_append(ds, psys->part->id.name+2);
		sprintf(num,"%%x%i",i+1);
		BLI_dynstr_append(ds, num);
	}
	
	str = BLI_dynstr_get_cstring(ds);

	BLI_dynstr_free(ds);

	return str;
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
/* change object's active particle system */
void psys_change_act(void *ob_v, void *act_v)
{
	Object *ob = ob_v;
	ParticleSystem *npsys, *psys;
	short act = *((short*)act_v)-1;

	if(act>=0){
		npsys=BLI_findlink(&ob->particlesystem,act);
		psys=psys_get_current(ob);

		if(psys)
			psys->flag &= ~PSYS_CURRENT;
		if(npsys)
			npsys->flag |= PSYS_CURRENT;
	}
}
Object *psys_get_lattice(Object *ob, ParticleSystem *psys)
{
	Object *lattice=0;
	
	if(psys_in_edit_mode(psys)==0){

		ModifierData *md = (ModifierData*)psys_get_modifier(ob,psys);

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
int psys_ob_has_hair(Object *ob)
{
	ParticleSystem *psys = ob->particlesystem.first;

	for(; psys; psys=psys->next)
		if(psys->part->type == PART_HAIR)
			return 1;

	return 0;
}
int psys_in_edit_mode(ParticleSystem *psys)
{
	return ((G.f & G_PARTICLEEDIT) && psys==psys_get_current(OBACT) && psys->edit);
}
int psys_check_enabled(Object *ob, ParticleSystem *psys)
{
	ParticleSystemModifierData *psmd;
	Mesh *me;

	if(psys->flag & PSYS_DISABLED)
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
	
	return 1;
}

/************************************************/
/*			Freeing stuff						*/
/************************************************/
void psys_free_settings(ParticleSettings *part)
{
	if(part->pd)
		MEM_freeN(part->pd);
}

void free_hair(ParticleSystem *psys, int softbody)
{
	ParticleData *pa;
	int i, totpart=psys->totpart;

	for(i=0, pa=psys->particles; i<totpart; i++, pa++) {
		if(pa->hair)
			MEM_freeN(pa->hair);
		pa->hair = NULL;
	}

	psys->flag &= ~PSYS_HAIR_DONE;

	if(softbody && psys->soft) {
		sbFree(psys->soft);
		psys->soft = NULL;
	}
}
void free_keyed_keys(ParticleSystem *psys)
{
	if(psys->particles && psys->particles->keys)
		MEM_freeN(psys->particles->keys);
}
void free_child_path_cache(ParticleSystem *psys)
{
	psys_free_path_cache_buffers(psys->childcache, &psys->childcachebufs);
	psys->childcache = NULL;
	psys->totchildcache = 0;
}
void psys_free_path_cache(ParticleSystem *psys)
{
	psys_free_path_cache_buffers(psys->pathcache, &psys->pathcachebufs);
	psys->pathcache= NULL;
	psys->totcached= 0;

	free_child_path_cache(psys);
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
/* free everything */
void psys_free(Object *ob, ParticleSystem * psys)
{
	if(psys){
		if(ob->particlesystem.first == NULL && G.f & G_PARTICLEEDIT)
			G.f &= ~G_PARTICLEEDIT;

		psys_free_path_cache(psys);

		free_hair(psys, 1);

		free_keyed_keys(psys);

		PE_free_particle_edit(psys);

		if(psys->particles){
			MEM_freeN(psys->particles);
			psys->particles = 0;
			psys->totpart = 0;
		}

		if(psys->child){
			MEM_freeN(psys->child);
			psys->child = 0;
			psys->totchild = 0;
		}

		if(psys->effectors.first)
			psys_end_effectors(psys);

		if(psys->part){
			psys->part->id.us--;		
			psys->part=0;
		}

		if(psys->reactevents.first)
			BLI_freelistN(&psys->reactevents);

		if(psys->pointcache)
			BKE_ptcache_free(psys->pointcache);

		MEM_freeN(psys);
	}
}

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
	Mat4MulVec4fl(data->viewmat, co);
	
	/* compute two vectors orthogonal to view vector */
	VECCOPY(view, co);
	Normalize(view);
	VecOrthoBasisf(view, ortho1, ortho2);

	/* compute on screen minification */
	w= co[2]*data->winmat[2][3] + data->winmat[3][3];
	dx= data->winx*ortho2[0]*data->winmat[0][0];
	dy= data->winy*ortho2[1]*data->winmat[1][1];
	w= sqrt(dx*dx + dy*dy)/w;

	/* w squared because we are working with area */
	area= area*w*w;

	/* viewport of the screen test */

	/* project point on screen */
	Mat4MulVec4fl(data->winmat, co);
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
	data->totcached= psys->totcached;
	data->childcache= psys->childcache;
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

	Mat4CpyMat4(data->winmat, winmat);
	Mat4MulMat4(data->viewmat, ob->obmat, viewmat);
	Mat4MulMat4(data->mat, data->viewmat, winmat);
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

	psys_free_path_cache(psys);

	if(psys->child){
		MEM_freeN(psys->child);
		psys->child= 0;
		psys->totchild= 0;
	}

	psys->child= data->child;
	psys->totchild= data->totchild;
	psys->pathcache= data->pathcache;
	psys->totcached= data->totcached;
	psys->childcache= data->childcache;
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
	Mesh *me= (Mesh*)(ctx->ob->data);
	MFace *mf, *mface;
	MVert *mvert;
	ParticleRenderData *data;
	ParticleRenderElem *elems, *elem;
	ParticleSettings *part= ctx->psys->part;
	float *facearea, (*facecenter)[3], size[3], fac, powrate, scaleclamp;
	float co1[3], co2[3], co3[3], co4[3], lambda, arearatio, t, area, viewport;
	double vprate;
	int *origindex, *facetotvert;
	int a, b, totorigface, totface, newtot, skipped;

	if(part->draw_as!=PART_DRAW_PATH || !(part->draw & PART_DRAW_REN_STRAND))
		return tot;
	if(!ctx->psys->renderdata)
		return tot;

	data= ctx->psys->renderdata;
	if(data->timeoffset)
		return 0;
	if(!(part->simplify_flag & PART_SIMPLIFY_ENABLE))
		return tot;

	mvert= dm->getVertArray(dm);
	mface= dm->getFaceArray(dm);
	origindex= dm->getFaceDataArray(dm, CD_ORIGINDEX);
	totface= dm->getNumFaces(dm);
	totorigface= me->totface;

	if(totface == 0 || totorigface == 0 || origindex == NULL)
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
		b= origindex[ctx->index[a]];
		if(b != -1)
			elems[b].totchild++;
	}

	/* compute areas and centers of original faces */
	for(mf=mface, a=0; a<totface; a++, mf++) {
		b= origindex[a];

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
				facearea[b] += AreaQ3Dfl(co1, co2, co3, co4);
				facetotvert[b] += 4;
			}
			else {
				facearea[b] += AreaT3Dfl(co1, co2, co3);
				facetotvert[b] += 3;
			}
		}
	}

	for(a=0; a<totorigface; a++)
		if(facetotvert[a] > 0)
			VecMulf(facecenter[a], 1.0f/facetotvert[a]);

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
		area = psys_render_projected_area(ctx->psys, facecenter[a], facearea[a], vprate, &viewport);
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
		b= origindex[ctx->index[a]];
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
	
	b= data->origindex[cpa->num];
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
/*			Interpolated Particles				*/
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
static void interpolate_particle(short type, ParticleKey keys[4], float dt, ParticleKey *result, int velocity)
{
	float t[4];

	if(type<0) {
		VecfCubicInterpol(keys[1].co, keys[1].vel, keys[2].co, keys[2].vel, dt, result->co, result->vel);
	}
	else {
		set_four_ipo(dt, t, type);

		weighted_particle_vector(keys[0].co, keys[1].co, keys[2].co, keys[3].co, t, result->co);

		if(velocity){
			float temp[3];

			if(dt>0.999f){
				set_four_ipo(dt-0.001f, t, type);
				weighted_particle_vector(keys[0].co, keys[1].co, keys[2].co, keys[3].co, t, temp);
				VECSUB(result->vel, result->co, temp);
			}
			else{
				set_four_ipo(dt+0.001f, t, type);
				weighted_particle_vector(keys[0].co, keys[1].co, keys[2].co, keys[3].co, t, temp);
				VECSUB(result->vel, temp, result->co);
			}
		}
	}
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
	Normalize(n1);
	Normalize(n2);
	Normalize(n3);

	if(mface->v4) {
		v4= (mvert+mface->v4)->co;
		VECCOPY(n4,(mvert+mface->v4)->no);
		Normalize(n4);
		
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
				CalcNormFloat4(v1,v2,v3,v4,nor);
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
				CalcNormFloat(v1,v2,v3,nor);
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
			spheremap(v1[0], v1[1], v1[2], uv1, uv1+1);
			spheremap(v2[0], v2[1], v2[2], uv2, uv2+1);
			spheremap(v3[0], v3[1], v3[2], uv3, uv3+1);
			if(v4)
				spheremap(v4[0], v4[1], v4[2], uv4, uv4+1);
		}

		if(v4){
			s1= uv3[0] - uv1[0];
			s2= uv4[0] - uv1[0];

			t1= uv3[1] - uv1[1];
			t2= uv4[1] - uv1[1];

			VecSubf(e1, v3, v1);
			VecSubf(e2, v4, v1);
		}
		else{
			s1= uv2[0] - uv1[0];
			s2= uv3[0] - uv1[0];

			t1= uv2[1] - uv1[1];
			t2= uv3[1] - uv1[1];

			VecSubf(e1, v2, v1);
			VecSubf(e2, v3, v1);
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
					CalcNormFloat4(o1, o2, o3, o4, ornor);
			}
			else {
				orco[0]= w[0]*o1[0] + w[1]*o2[0] + w[2]*o3[0];
				orco[1]= w[0]*o1[1] + w[1]*o2[1] + w[2]*o3[1];
				orco[2]= w[0]*o1[2] + w[1]*o2[2] + w[2]*o3[2];

				if(ornor)
					CalcNormFloat(o1, o2, o3, ornor);
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

float psys_interpolate_value_from_verts(DerivedMesh *dm, short from, int index, float *fw, float *values)
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
		MeanValueWeights(v, 4, co, neww);
	}
	else {
		MeanValueWeights(v, 3, co, neww);
		neww[3]= 0.0f;
	}
}

/* find the derived mesh face for a particle, set the mf passed.
This is slow, can be optimized but only for many lookups, return the face lookup index*/
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
				if(IsectPQ2Df(uv, faceuv[0], faceuv[1], faceuv[2], faceuv[3]))
					return findex;
			}
			else if(IsectPT2Df(uv, faceuv[0], faceuv[1], faceuv[2]))
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
					if(IsectPQ2Df(uv, faceuv[0], faceuv[1], faceuv[2], faceuv[3]))
						return findex;
				}
				else if(IsectPT2Df(uv, faceuv[0], faceuv[1], faceuv[2]))
					return findex;
			}
		}
	}

	return DMCACHE_NOTFOUND;
}

/* interprets particle data to get a point on a mesh in object space */
#define PARTICLE_ON_DM_ERROR \
	{ if(vec) { vec[0]=vec[1]=vec[2]=0.0; } \
	  if(nor) { nor[0]=nor[1]=0.0; nor[2]=1.0; } \
	  if(orco) { orco[0]=orco[1]=orco[2]=0.0; } \
	  if(ornor) { ornor[0]=ornor[1]=0.0; ornor[2]=1.0; } \
	  if(utan) { utan[0]=utan[1]=utan[2]=0.0; } \
	  if(vtan) { vtan[0]=vtan[1]=vtan[2]=0.0; } }

void psys_particle_on_dm(Object *ob, DerivedMesh *dm, int from, int index, int index_dmcache, float *fw, float foffset, float *vec, float *nor, float *utan, float *vtan, float *orco, float *ornor)
{
	float temp1[3];
	float (*orcodata)[3];

	if(index < 0) { /* 'no dm' error has happened! */
		PARTICLE_ON_DM_ERROR;
		return;
	}
	orcodata= dm->getVertDataArray(dm, CD_ORCO);

	if (dm->deformedOnly || index_dmcache == DMCACHE_ISCHILD) {
		/* this works for meshes with deform verts only - constructive modifiers wont work properly*/
		if(from == PART_FROM_VERT) {
			if(index >= dm->getNumVerts(dm)) {
				PARTICLE_ON_DM_ERROR;
				return;
			}
	
			dm->getVertCo(dm,index,vec);
			if(nor){
				dm->getVertNo(dm,index,nor);
				Normalize(nor);
			}
			if(orco)
				VECCOPY(orco, orcodata[index])
			if(ornor) {
				dm->getVertNo(dm,index,nor);
				Normalize(nor);
			}
		}
		else { /* PART_FROM_FACE / PART_FROM_VOLUME */
			MFace *mface;
			MTFace *mtface=0;
			MVert *mvert;
			int uv_index;

			if(index >= dm->getNumFaces(dm)) {
				PARTICLE_ON_DM_ERROR;
				return;
			}
			
			mface=dm->getFaceData(dm,index,CD_MFACE);
			mvert=dm->getVertDataArray(dm,CD_MVERT);
			uv_index=CustomData_get_active_layer_index(&dm->faceData,CD_MTFACE);

			if(uv_index>=0){
				CustomDataLayer *layer=&dm->faceData.layers[uv_index];
				mtface= &((MTFace*)layer->data)[index];
			}

			if(from==PART_FROM_VOLUME){
				psys_interpolate_face(mvert,mface,mtface,orcodata,fw,vec,temp1,utan,vtan,orco,ornor);
				if(nor)
					VECCOPY(nor,temp1);
				Normalize(temp1);
				VecMulf(temp1,-foffset);
				VECADD(vec,vec,temp1);
			}
			else
				psys_interpolate_face(mvert,mface,mtface,orcodata,fw,vec,nor,utan,vtan,orco,ornor);
		}
	} else {
		/* Need to support constructive modifiers, this is a bit more tricky
			we need a customdata layer like UV's so we can position the particle */
		
		/* Only face supported at the moment */
		if(ELEM(from, PART_FROM_FACE, PART_FROM_VOLUME)) {
			/* find a face on the derived mesh that uses this face */
			Mesh *me= (Mesh*)ob->data;
			MVert *mvert;
			MFace *mface;
			MTFace *mtface;
			OrigSpaceFace *osface;
			int *origindex;
			float fw_mod[4];
			int i, totface;
			
			mvert= dm->getVertDataArray(dm,CD_MVERT);

			osface= dm->getFaceDataArray(dm, CD_ORIGSPACE);
			origindex= dm->getFaceDataArray(dm, CD_ORIGINDEX);

			/* For this to work we need origindex and OrigSpace coords */
			if(origindex==NULL || osface==NULL || index>=me->totface) {
				PARTICLE_ON_DM_ERROR;
				return;
			}
			
			if (index_dmcache == DMCACHE_NOTFOUND)
				i = psys_particle_dm_face_lookup(ob, dm, index, fw, (LinkNode*)NULL);
			else
				i = index_dmcache;

			totface = dm->getNumFaces(dm);

			/* Any time this happens, and the face has not been removed,
			* its a BUG watch out for this error! */
			if (i==-1) {
				printf("Cannot find original face %i\n", index);
				PARTICLE_ON_DM_ERROR;
				return;
			}
			else if(i >= totface)
				return;

			mface= dm->getFaceData(dm, i, CD_MFACE);
			mtface= dm->getFaceData(dm, i, CD_MTFACE); 
			osface += i;

			/* we need to modify the original weights to become weights for
			 * the derived mesh face */
			psys_origspace_to_w(osface, mface->v4, fw, fw_mod);

			if(from==PART_FROM_VOLUME){
				psys_interpolate_face(mvert,mface,mtface,orcodata,fw_mod,vec,temp1,utan,vtan,orco,ornor);
				if(nor)
					VECCOPY(nor,temp1);
				Normalize(temp1);
				VecMulf(temp1,-foffset);
				VECADD(vec,vec,temp1);
			}
			else
				psys_interpolate_face(mvert,mface,mtface,orcodata,fw_mod,vec,nor,utan,vtan,orco,ornor);
		}
		else if(from == PART_FROM_VERT) {
			if (index_dmcache == DMCACHE_NOTFOUND || index_dmcache > dm->getNumVerts(dm)) {
				PARTICLE_ON_DM_ERROR;
				return;
			}

			dm->getVertCo(dm,index_dmcache,vec);
			if(nor) {
				dm->getVertNo(dm,index_dmcache,nor);
				Normalize(nor);
			}
			if(orco)
				VECCOPY(orco, orcodata[index])
			if(ornor) {
				dm->getVertNo(dm,index_dmcache,nor);
				Normalize(nor);
			}
			if(utan && vtan) {
				utan[0]= utan[1]= utan[2]= 0.0f;
				vtan[0]= vtan[1]= vtan[2]= 0.0f;
			}
		}
		else {
			PARTICLE_ON_DM_ERROR;
		}
	}
}
#undef PARTICLE_ON_DM_ERROR

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
	return 0;
}
/************************************************/
/*			Particles on a shape				*/
/************************************************/
/* ready for future use */
void psys_particle_on_shape(int distr, int index, float *fuv, float *vec, float *nor, float *utan, float *vtan, float *orco, float *ornor)
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
void psys_particle_on_emitter(Object *ob, ParticleSystemModifierData *psmd, int from, int index, int index_dmcache, float *fuv, float foffset, float *vec, float *nor, float *utan, float *vtan, float *orco, float *ornor){
	if(psmd){
		if(psmd->psys->part->distr==PART_DISTR_GRID){
			if(vec){
				VECCOPY(vec,fuv);
			}
			return;
		}
		/* we cant use the num_dmcache */
		psys_particle_on_dm(ob, psmd->dm,from,index,index_dmcache,fuv,foffset,vec,nor,utan,vtan,orco,ornor);
	}
	else
		psys_particle_on_shape(from,index,fuv,vec,nor,utan,vtan,orco,ornor);

}
/************************************************/
/*			Path Cache							*/
/************************************************/
static void hair_to_particle(ParticleKey *key, HairKey *hkey)
{
	VECCOPY(key->co, hkey->co);
	key->time = hkey->time;
}
static void bp_to_particle(ParticleKey *key, BodyPoint *bp, HairKey *hkey)
{
	VECCOPY(key->co, bp->pos);
	key->time = hkey->time;
}
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
				vectoquat(par->vel,axis,(axis+1)%3, q2);
			QuatMulVecf(q2,vec);
			VecMulf(vec,amplitude);
			VECADD(state->co,state->co,vec);

			VECSUB(vec,state->co,par->co);

			if(t!=0.0)
				VecRotToQuat(par->vel,t,q1);
			
			QuatMulVecf(q1,vec);
			
			VECADD(state->co,par->co,vec);
			break;
		case PART_KINK_RADIAL:
			VECSUB(vec,state->co,par->co);

			Normalize(vec);
			VecMulf(vec,amplitude*(float)sin(t));

			VECADD(state->co,state->co,vec);
			break;
		case PART_KINK_WAVE:
			vec[axis]=1.0;
			if(obmat)
				Mat4MulVecfl(obmat,vec);

			if(par_rot)
				QuatMulVecf(par_rot,vec);

			Projf(q1,vec,par->vel);
			
			VECSUB(vec,vec,q1);
			Normalize(vec);

			VecMulf(vec,amplitude*(float)sin(t));

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
					vectoquat(par->vel,axis,(axis+1)%3,q2);
				QuatMulVecf(q2,y_vec);
				QuatMulVecf(q2,z_vec);
				
				VECSUB(vec_from_par,state->co,par->co);
				VECCOPY(vec_one,vec_from_par);
				radius=Normalize(vec_one);

				inp_y=Inpf(y_vec,vec_one);
				inp_z=Inpf(z_vec,vec_one);

				if(inp_y>0.5){
					VECCOPY(state_co,y_vec);

					VecMulf(y_vec,amplitude*(float)cos(t));
					VecMulf(z_vec,amplitude/2.0f*(float)sin(2.0f*t));
				}
				else if(inp_z>0.0){
					VECCOPY(state_co,z_vec);
					VecMulf(state_co,(float)sin(M_PI/3.0f));
					VECADDFAC(state_co,state_co,y_vec,-0.5f);

					VecMulf(y_vec,-amplitude*(float)cos(t + M_PI/3.0f));
					VecMulf(z_vec,amplitude/2.0f*(float)cos(2.0f*t + M_PI/6.0f));
				}
				else{
					VECCOPY(state_co,z_vec);
					VecMulf(state_co,-(float)sin(M_PI/3.0f));
					VECADDFAC(state_co,state_co,y_vec,-0.5f);

					VecMulf(y_vec,amplitude*(float)-sin(t+M_PI/6.0f));
					VecMulf(z_vec,amplitude/2.0f*(float)-sin(2.0f*t+M_PI/3.0f));
				}

				VecMulf(state_co,amplitude);
				VECADD(state_co,state_co,par->co);
				VECSUB(vec_from_par,state->co,state_co);

				length=Normalize(vec_from_par);
				VecMulf(vec_from_par,MIN2(length,amplitude/2.0f));

				VECADD(state_co,par->co,y_vec);
				VECADD(state_co,state_co,z_vec);
				VECADD(state_co,state_co,vec_from_par);

				shape=(2.0f*(float)M_PI)*(1.0f+shape);

				if(t<shape){
					shape=t/shape;
					shape=(float)sqrt((double)shape);
					VecLerpf(state->co,state->co,state_co,shape);
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
		VecLerpf(state->co,state->co,par->co,clump);
	}
}
int do_guide(ParticleKey *state, int pa_num, float time, ListBase *lb)
{
	PartDeflect *pd;
	ParticleEffectorCache *ec;
	Object *eob;
	Curve *cu;
	ParticleKey key, par;

	float effect[3]={0.0,0.0,0.0}, distance, f_force, mindist, totforce=0.0;
	float guidevec[4], guidedir[3], rot2[4], temp[3], angle, pa_loc[3], pa_zero[3]={0.0f,0.0f,0.0f};
	float veffect[3]={0.0,0.0,0.0}, guidetime;

	effect[0]=effect[1]=effect[2]=0.0;

	if(lb->first){
		for(ec = lb->first; ec; ec= ec->next){
			eob= ec->ob;
			if(ec->type & PSYS_EC_EFFECTOR){
				pd=eob->pd;
				if(pd->forcefield==PFIELD_GUIDE){
					cu = (Curve*)eob->data;

					distance=ec->distances[pa_num];
					mindist=pd->f_strength;

					VECCOPY(pa_loc, ec->locations+3*pa_num);
					VECCOPY(pa_zero,pa_loc);
					VECADD(pa_zero,pa_zero,ec->firstloc);

					guidetime=time/(1.0-pd->free_end);

					/* WARNING: bails out with continue here */
					if(((pd->flag & PFIELD_USEMAX) && distance>pd->maxdist) || guidetime>1.0f) continue;

					if(guidetime>1.0f) continue;

					/* calculate contribution factor for this guide */
					f_force=1.0f;
					if(distance<=mindist);
					else if(pd->flag & PFIELD_USEMAX) {
						if(mindist>=pd->maxdist) f_force= 0.0f;
						else if(pd->f_power!=0.0f){
							f_force= 1.0f - (distance-mindist)/(pd->maxdist - mindist);
							f_force = (float)pow(f_force, pd->f_power);
						}
					}
					else if(pd->f_power!=0.0f){
						f_force= 1.0f/(1.0f + distance-mindist);
						f_force = (float)pow(f_force, pd->f_power);
					}

					if(pd->flag & PFIELD_GUIDE_PATH_ADD)
						where_on_path(eob, f_force*guidetime, guidevec, guidedir);
					else
						where_on_path(eob, guidetime, guidevec, guidedir);

					Mat4MulVecfl(ec->ob->obmat,guidevec);
					Mat4Mul3Vecfl(ec->ob->obmat,guidedir);

					Normalize(guidedir);

					if(guidetime!=0.0){
						/* curve direction */
						Crossf(temp, ec->firstdir, guidedir);
						angle=Inpf(ec->firstdir,guidedir)/(VecLength(ec->firstdir));
						angle=saacos(angle);
						VecRotToQuat(temp,angle,rot2);
						QuatMulVecf(rot2,pa_loc);

						/* curve tilt */
						VecRotToQuat(guidedir,guidevec[3]-ec->firstloc[3],rot2);
						QuatMulVecf(rot2,pa_loc);

						//vectoquat(guidedir, pd->kink_axis, (pd->kink_axis+1)%3, q);
						//QuatMul(par.rot,rot2,q);
					}
					//else{
					//	par.rot[0]=1.0f;
					//	par.rot[1]=par.rot[2]=par.rot[3]=0.0f;
					//}

					/* curve taper */
					if(cu->taperobj)
						VecMulf(pa_loc,calc_taper(cu->taperobj,(int)(f_force*guidetime*100.0),100));
					/* TODO */
					//else{
					///* curve size*/
					//	calc_curve_subdiv_radius(cu,cu->nurb.first,((Nurb*)cu->nurb.first)->
					//}
					par.co[0]=par.co[1]=par.co[2]=0.0f;
					VECCOPY(key.co,pa_loc);
					do_prekink(&key, &par, 0, guidetime, pd->kink_freq, pd->kink_shape, pd->kink_amp, pd->kink, pd->kink_axis, 0);
					do_clump(&key, &par, guidetime, pd->clump_fac, pd->clump_pow, 1.0f);
					VECCOPY(pa_loc,key.co);

					VECADD(pa_loc,pa_loc,guidevec);
					VECSUB(pa_loc,pa_loc,pa_zero);
					VECADDFAC(effect,effect,pa_loc,f_force);
					VECADDFAC(veffect,veffect,guidedir,f_force);
					totforce+=f_force;
				}
			}
		}

		if(totforce!=0.0){
			if(totforce>1.0)
				VecMulf(effect,1.0f/totforce);
			CLAMP(totforce,0.0,1.0);
			VECADD(effect,effect,pa_zero);
			VecLerpf(state->co,state->co,effect,totforce);

			Normalize(veffect);
			VecMulf(veffect,VecLength(state->vel));
			VECCOPY(state->vel,veffect);
			return 1;
		}
	}
	return 0;
}
static void do_rough(float *loc, float t, float fac, float size, float thres, ParticleKey *state)
{
	float rough[3];
	float rco[3];

	if(thres!=0.0)
		if((float)fabs((float)(-1.5+loc[0]+loc[1]+loc[2]))<1.5f*thres) return;

	VECCOPY(rco,loc);
	VecMulf(rco,t);
	rough[0]=-1.0f+2.0f*BLI_gTurbulence(size, rco[0], rco[1], rco[2], 2,0,2);
	rough[1]=-1.0f+2.0f*BLI_gTurbulence(size, rco[1], rco[2], rco[0], 2,0,2);
	rough[2]=-1.0f+2.0f*BLI_gTurbulence(size, rco[2], rco[0], rco[1], 2,0,2);
	VECADDFAC(state->co,state->co,rough,fac);
}
static void do_rough_end(float *loc, float t, float fac, float shape, ParticleKey *state, ParticleKey *par)
{
	float rough[3], rnor[3];
	float roughfac;

	roughfac=fac*(float)pow((double)t,shape);
	VECCOPY(rough,loc);
	rough[0]=-1.0f+2.0f*rough[0];
	rough[1]=-1.0f+2.0f*rough[1];
	rough[2]=-1.0f+2.0f*rough[2];
	VecMulf(rough,roughfac);


	if(par){
		VECCOPY(rnor,par->vel);
	}
	else{
		VECCOPY(rnor,state->vel);
	}
	Normalize(rnor);
	Projf(rnor,rough,rnor);
	VECSUB(rough,rough,rnor);

	VECADD(state->co,state->co,rough);
}
static void do_path_effectors(Object *ob, ParticleSystem *psys, int i, ParticleCacheKey *ca, int k, int steps, float *rootco, float effector, float dfra, float cfra, float *length, float *vec)
{
	float force[3] = {0.0f,0.0f,0.0f}, vel[3] = {0.0f,0.0f,0.0f};
	ParticleKey eff_key;
	ParticleData *pa;

	VECCOPY(eff_key.co,(ca-1)->co);
	VECCOPY(eff_key.vel,(ca-1)->vel);
	QUATCOPY(eff_key.rot,(ca-1)->rot);

	pa= psys->particles+i;
	do_effectors(i, pa, &eff_key, ob, psys, rootco, force, vel, dfra, cfra);

	VecMulf(force, effector*pow((float)k / (float)steps, 100.0f * psys->part->eff_hair) / (float)steps);

	VecAddf(force, force, vec);

	Normalize(force);

	if(k < steps) {
		VecSubf(vec, (ca+1)->co, ca->co);
		*length = VecLength(vec);
	}

	VECADDFAC(ca->co, (ca-1)->co, force, *length);
}
static int check_path_length(int k, ParticleCacheKey *keys, ParticleCacheKey *state, float max_length, float *cur_length, float length, float *dvec)
{
	if(*cur_length + length > max_length){
		//if(p<totparent){
		//	if(k<=(int)cache[totpart+p]->time){
		//		/* parents need to be calculated fully first so that they don't mess up their children */
		//		/* we'll make a note of where we got to though so that they're easy to finish later */
		//		state->time=(max_length-*cur_length)/length;
		//		cache[totpart+p]->time=(float)k;
		//	}
		//}
		//else{
		VecMulf(dvec, (max_length - *cur_length) / length);
		VECADD(state->co, (state - 1)->co, dvec);
		keys->steps = k;
		/* something over the maximum step value */
		return k=100000;
		//}
	}
	else {
		*cur_length+=length;
		return k;
	}
}
static void finalize_path_length(ParticleCacheKey *keys)
{
	ParticleCacheKey *state = keys;
	float dvec[3];
	state += state->steps;

	VECSUB(dvec, state->co, (state - 1)->co);
	VecMulf(dvec, state->steps);
	VECADD(state->co, (state - 1)->co, dvec);
}
static void offset_child(ChildParticle *cpa, ParticleKey *par, ParticleKey *child, float flat, float radius)
{
	VECCOPY(child->co,cpa->fuv);
	VecMulf(child->co,radius);

	child->co[0]*=flat;

	VECCOPY(child->vel,par->vel);

	QuatMulVecf(par->rot,child->co);

	QUATCOPY(child->rot,par->rot);

	VECADD(child->co,child->co,par->co);
}
float *psys_cache_vgroup(DerivedMesh *dm, ParticleSystem *psys, int vgroup)
{
	float *vg=0;

	if(psys->vgroup[vgroup]){
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
void psys_find_parents(Object *ob, ParticleSystemModifierData *psmd, ParticleSystem *psys)
{
	ParticleSettings *part=psys->part;
	KDTree *tree;
	ChildParticle *cpa;
	int p, totparent,totchild=psys->totchild;
	float co[3], orco[3];
	int from=PART_FROM_FACE;
	totparent=(int)(totchild*part->parents*0.3);

	tree=BLI_kdtree_new(totparent);

	for(p=0,cpa=psys->child; p<totparent; p++,cpa++){
		psys_particle_on_emitter(ob,psmd,from,cpa->num,-1,cpa->fuv,cpa->foffset,co,0,0,0,orco,0);
		BLI_kdtree_insert(tree, p, orco, NULL);
	}

	BLI_kdtree_balance(tree);

	for(; p<totchild; p++,cpa++){
		psys_particle_on_emitter(ob,psmd,from,cpa->num,DMCACHE_ISCHILD,cpa->fuv,cpa->foffset,co,0,0,0,orco,0);
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
		Crossf(cross, surfnor, nor);
		Crossf(nstrand, nor, cross);

		blend= INPR(nstrand, surfnor);
		CLAMP(blend, 0.0f, 1.0f);

		VecLerpf(vnor, nstrand, surfnor, blend);
		Normalize(vnor);
	}
	else
		VECCOPY(vnor, nor)
	
	if(ma->strand_surfnor > 0.0f) {
		if(ma->strand_surfnor > surfdist) {
			blend= (ma->strand_surfnor - surfdist)/ma->strand_surfnor;
			VecLerpf(vnor, vnor, surfnor, blend);
			Normalize(vnor);
		}
	}

	VECCOPY(nor, vnor);
}

int psys_threads_init_path(ParticleThread *threads, float cfra, int editupdate)
{
	ParticleThreadContext *ctx= threads[0].ctx;
	Object *ob= ctx->ob;
	ParticleSystem *psys= ctx->psys;
	ParticleSettings *part = psys->part;
	ParticleEditSettings *pset = &G.scene->toolsettings->particle;
	int totparent=0, between=0;
	int steps = (int)pow(2.0,(double)part->draw_step);
	int totchild = psys->totchild;
	int i, seed, totthread= threads[0].tot;

	/*---start figuring out what is actually wanted---*/
	if(psys_in_edit_mode(psys))
		if(psys->renderdata==0 && (psys->edit==NULL || pset->flag & PE_SHOW_CHILD)==0)
			totchild=0;

	if(totchild && part->from!=PART_FROM_PARTICLE && part->childtype==PART_CHILD_FACES){
		totparent=(int)(totchild*part->parents*0.3);
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
	if(ctx->psys->part->flag & PART_ANIM_BRANCHING)
		seed= 31415926 + ctx->psys->seed + (int)cfra;
	else
		seed= 31415926 + ctx->psys->seed;
	
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
	ctx->cfra= cfra;

	psys->lattice = psys_get_lattice(ob, psys);

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
	if(part->flag&PART_ABS_TIME && part->ipo){
		calc_ipo(part->ipo, cfra);
		execute_ipo((ID *)part, part->ipo);
	}

	return 1;
}

/* note: this function must be thread safe, except for branching! */
void psys_thread_create_path(ParticleThread *thread, struct ChildParticle *cpa, ParticleCacheKey *keys, int i)
{
	ParticleThreadContext *ctx= thread->ctx;
	Object *ob= ctx->ob;
	ParticleSystem *psys = ctx->psys;
	ParticleSettings *part = psys->part;
	ParticleCacheKey **cache= psys->childcache;
	ParticleCacheKey **pcache= psys->pathcache;
	ParticleCacheKey *state, *par = NULL, *key[4];
	ParticleData *pa;
	ParticleTexture ptex;
	float *cpa_fuv=0;
	float co[3], orco[3], ornor[3], t, rough_t, cpa_1st[3], dvec[3];
	float branch_begin, branch_end, branch_prob, branchfac, rough_rand;
	float pa_rough1, pa_rough2, pa_roughe;
	float length, pa_length, pa_clump, pa_kink, pa_effector;
	float max_length = 1.0f, cur_length = 0.0f;
	float eff_length, eff_vec[3];
	int k, cpa_num, guided=0;
	short cpa_from;

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
				if(psys->particles[cpa->pa[w]].flag & PARS_EDIT_RECALC) {
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
		if(part->childtype == PART_CHILD_FACES)
			foffset = -(2.0f + part->childspread);
		cpa_fuv = cpa->fuv;
		cpa_from = PART_FROM_FACE;

		psys_particle_on_emitter(ob,ctx->psmd,cpa_from,cpa_num,DMCACHE_ISCHILD,cpa->fuv,foffset,co,ornor,0,0,orco,0);

		/* we need to save the actual root position of the child for positioning it accurately to the surface of the emitter */
		VECCOPY(cpa_1st,co);
		Mat4MulVecfl(ob->obmat,cpa_1st);

		pa=0;
	}
	else{
		if(ctx->editupdate && !(part->flag & PART_BRANCHING)) {
			if(!(psys->particles[cpa->parent].flag & PARS_EDIT_RECALC))
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

		psys_particle_on_emitter(ob,ctx->psmd,cpa_from,cpa_num,DMCACHE_ISCHILD,cpa_fuv,pa->foffset,co,ornor,0,0,orco,0);
	}

	keys->steps = ctx->steps;

	/* correct child ipo timing */
	if((part->flag&PART_ABS_TIME)==0 && part->ipo){
		float dsta=part->end-part->sta;
		calc_ipo(part->ipo, 100.0f*(ctx->cfra-(part->sta+dsta*cpa->rand[1]))/(part->lifetime*(1.0f - part->randlife*cpa->rand[0])));
		execute_ipo((ID *)part, part->ipo);
	}

	/* get different child parameters from textures & vgroups */
	ptex.length=part->length*(1.0f - part->randlength*cpa->rand[0]);
	ptex.clump=1.0;
	ptex.kink=1.0;
	ptex.rough= 1.0;

	get_cpa_texture(ctx->dm,ctx->ma,cpa_num,cpa_fuv,orco,&ptex,
		MAP_PA_LENGTH|MAP_PA_CLUMP|MAP_PA_KINK|MAP_PA_ROUGH);
	
	pa_length=ptex.length;
	pa_clump=ptex.clump;
	pa_kink=ptex.kink;
	pa_rough1=ptex.rough;
	pa_rough2=ptex.rough;
	pa_roughe=ptex.rough;
	pa_effector= 1.0f;

	if(ctx->vg_length)
		pa_length*=psys_interpolate_value_from_verts(ctx->dm,cpa_from,cpa_num,cpa_fuv,ctx->vg_length);
	if(ctx->vg_clump)
		pa_clump*=psys_interpolate_value_from_verts(ctx->dm,cpa_from,cpa_num,cpa_fuv,ctx->vg_clump);
	if(ctx->vg_kink)
		pa_kink*=psys_interpolate_value_from_verts(ctx->dm,cpa_from,cpa_num,cpa_fuv,ctx->vg_kink);
	if(ctx->vg_rough1)
		pa_rough1*=psys_interpolate_value_from_verts(ctx->dm,cpa_from,cpa_num,cpa_fuv,ctx->vg_rough1);
	if(ctx->vg_rough2)
		pa_rough2*=psys_interpolate_value_from_verts(ctx->dm,cpa_from,cpa_num,cpa_fuv,ctx->vg_rough2);
	if(ctx->vg_roughe)
		pa_roughe*=psys_interpolate_value_from_verts(ctx->dm,cpa_from,cpa_num,cpa_fuv,ctx->vg_roughe);
	if(ctx->vg_effector)
		pa_effector*=psys_interpolate_value_from_verts(ctx->dm,cpa_from,cpa_num,cpa_fuv,ctx->vg_effector);

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
			if(k==0){
				/* calculate the offset between actual child root position and first position interpolated from parents */
				VECSUB(cpa_1st,cpa_1st,state->co);
			}
			/* apply offset for correct positioning */
			VECADD(state->co,state->co,cpa_1st);
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
				do_path_effectors(ob, psys, cpa->pa[0], state, k, ctx->steps, keys->co, pa_effector, 0.0f, ctx->cfra, &eff_length, eff_vec);
			}
			else {
				VecSubf(eff_vec,(state+1)->co,state->co);
				eff_length= VecLength(eff_vec);
			}
		}
	}

	for(k=0,state=keys; k<=ctx->steps; k++,state++){
		t=(float)k/(float)ctx->steps;

		if(ctx->totparent){
			if(i>=ctx->totparent)
				/* this is not threadsafe, but should only happen for
				 * branching particles particles, which are not threaded */
				par = cache[cpa->parent] + k;
			else
				par=0;
		}
		else if(cpa->parent>=0){
			par=pcache[cpa->parent]+k;
		}

		/* apply different deformations to the child path */
		if(part->flag & PART_CHILD_EFFECT)
			/* state is safe to cast, since only co and vel are used */
			guided = do_guide((ParticleKey*)state, cpa->parent, t, &(psys->effectors));

		if(guided==0){
			if(part->kink)
				do_prekink((ParticleKey*)state, (ParticleKey*)par, par->rot, t,
				part->kink_freq * pa_kink, part->kink_shape, part->kink_amp, part->kink, part->kink_axis, ob->obmat);
					
			do_clump((ParticleKey*)state, (ParticleKey*)par, t, part->clumpfac, part->clumppow, pa_clump);
		}

		if(part->flag & PART_BRANCHING && ctx->between == 0 && part->flag & PART_ANIM_BRANCHING)
			rough_t = t * rough_rand;
		else
			rough_t = t;

		if(part->rough1 != 0.0 && pa_rough1 != 0.0)
			do_rough(orco, rough_t, pa_rough1*part->rough1, part->rough1_size, 0.0, (ParticleKey*)state);

		if(part->rough2 != 0.0 && pa_rough2 != 0.0)
			do_rough(cpa->rand, rough_t, pa_rough2*part->rough2, part->rough2_size, part->rough2_thres, (ParticleKey*)state);

		if(part->rough_end != 0.0 && pa_roughe != 0.0)
			do_rough_end(cpa->rand, rough_t, pa_roughe*part->rough_end, part->rough_end_shape, (ParticleKey*)state, (ParticleKey*)par);

		if(part->flag & PART_BRANCHING && ctx->between==0){
			if(branch_prob > part->branch_thres){
				branchfac=0.0f;
			}
			else{
				if(part->flag & PART_SYMM_BRANCHING){
					if(t < branch_begin || t > branch_end)
						branchfac=0.0f;
					else{
						if((t-branch_begin)/(branch_end-branch_begin)<0.5)
							branchfac=2.0f*(t-branch_begin)/(branch_end-branch_begin);
						else
							branchfac=2.0f*(branch_end-t)/(branch_end-branch_begin);

						CLAMP(branchfac,0.0f,1.0f);
					}
				}
				else{
					if(t < branch_begin){
						branchfac=0.0f;
					}
					else{
						branchfac=(t-branch_begin)/((1.0f-branch_begin)*0.5f);
						CLAMP(branchfac,0.0f,1.0f);
					}
				}
			}

			if(i<psys->totpart)
				VecLerpf(state->co, (pcache[i] + k)->co, state->co, branchfac);
			else
				/* this is not threadsafe, but should only happen for
				 * branching particles particles, which are not threaded */
				VecLerpf(state->co, (cache[i - psys->totpart] + k)->co, state->co, branchfac);
		}

		/* we have to correct velocity because of kink & clump */
		if(k>1){
			VECSUB((state-1)->vel,state->co,(state-2)->co);
			VecMulf((state-1)->vel,0.5);

			if(ctx->ma && (part->draw & PART_DRAW_MAT_COL))
				get_strand_normal(ctx->ma, ornor, cur_length, (state-1)->vel);
		}

		/* check if path needs to be cut before actual end of data points */
		if(k){
			VECSUB(dvec,state->co,(state-1)->co);
			if(part->flag&PART_ABS_LENGTH)
				length=VecLength(dvec);
			else
				length=1.0f/(float)ctx->steps;

			k=check_path_length(k,keys,state,max_length,&cur_length,length,dvec);
		}
		else{
			/* initialize length calculation */
			if(part->flag&PART_ABS_LENGTH)
				max_length= part->abslength*pa_length;
			else
				max_length= pa_length;

			cur_length= 0.0f;
		}

		if(ctx->ma && (part->draw & PART_DRAW_MAT_COL)) {
			VECCOPY(state->col, &ctx->ma->r)
			get_strand_normal(ctx->ma, ornor, cur_length, state->vel);
		}
	}

	/* now let's finalise the interpolated parents that we might have left half done before */
	if(i<ctx->totparent)
		finalize_path_length(keys);
}

void *exec_child_path_cache(void *data)
{
	ParticleThread *thread= (ParticleThread*)data;
	ParticleThreadContext *ctx= thread->ctx;
	ParticleSystem *psys= ctx->psys;
	ParticleCacheKey **cache= psys->childcache;
	ChildParticle *cpa;
	int i, totchild= ctx->totchild;
	
	cpa= psys->child + thread->num;
	for(i=thread->num; i<totchild; i+=thread->tot, cpa+=thread->tot)
		psys_thread_create_path(thread, cpa, cache[i], i);

	return 0;
}

void psys_cache_child_paths(Object *ob, ParticleSystem *psys, float cfra, int editupdate)
{
	ParticleSettings *part = psys->part;
	ParticleThread *pthreads;
	ParticleThreadContext *ctx;
	ParticleCacheKey **cache;
	ListBase threads;
	int i, totchild, totparent, totthread;

	pthreads= psys_threads_create(ob, psys);

	if(!psys_threads_init_path(pthreads, cfra, editupdate)) {
		psys_threads_free(pthreads);
		return;
	}

	ctx= pthreads[0].ctx;
	totchild= ctx->totchild;
	totparent= ctx->totparent;

	if(editupdate && psys->childcache && !(part->flag & PART_BRANCHING) && totchild == psys->totchildcache) {
		cache = psys->childcache;
	}
	else {
		/* clear out old and create new empty path cache */
		free_child_path_cache(psys);
		psys->childcache= psys_alloc_path_cache_buffers(&psys->childcachebufs, totchild, ctx->steps+1);
		psys->totchildcache = totchild;
	}

	totthread= pthreads[0].tot;

	if(totthread > 1) {
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
void psys_cache_paths(Object *ob, ParticleSystem *psys, float cfra, int editupdate)
{
	ParticleCacheKey *ca, **cache=psys->pathcache;
	ParticleSystemModifierData *psmd = psys_get_modifier(ob, psys);
	ParticleEditSettings *pset = &G.scene->toolsettings->particle;
	
	ParticleData *pa;
	ParticleKey keys[4], result, *kkey[2] = {NULL, NULL};
	HairKey *hkey[2] = {NULL, NULL};

	ParticleEdit *edit = 0;
	ParticleEditKey *ekey = 0;

	SoftBody *soft = 0;
	BodyPoint *bp[2] = {NULL, NULL};
	
	Material *ma;
	
	float birthtime = 0.0, dietime = 0.0;
	float t, time = 0.0, keytime = 0.0, dfra = 1.0, frs_sec = G.scene->r.frs_sec;
	float col[3] = {0.5f, 0.5f, 0.5f};
	float prev_tangent[3], hairmat[4][4];
	int k,i;
	int steps = (int)pow(2.0, (double)psys->part->draw_step);
	int totpart = psys->totpart;
	char nosel[4], sel[4];
	float sel_col[3];
	float nosel_col[3];
	float length, vec[3];
	float *vg_effector= NULL, effector=0.0f;

	/* we don't have anything valid to create paths from so let's quit here */
	if((psys->flag & PSYS_HAIR_DONE)==0 && (psys->flag & PSYS_KEYED)==0)
		return;

	if(psys->renderdata)
		steps = (int)pow(2.0, (double)psys->part->ren_step);
	else if(psys_in_edit_mode(psys)){
		edit=psys->edit;
		
		//timed = edit->draw_timed;

		PE_get_colors(sel,nosel);
		if(pset->brushtype == PE_BRUSH_WEIGHT){
			sel_col[0] = sel_col[1] = sel_col[2] = 1.0f;
			nosel_col[0] = nosel_col[1] = nosel_col[2] = 0.0f;
		}
		else{
			sel_col[0] = (float)sel[0] / 255.0f;
			sel_col[1] = (float)sel[1] / 255.0f;
			sel_col[2] = (float)sel[2] / 255.0f;
			nosel_col[0] = (float)nosel[0] / 255.0f;
			nosel_col[1] = (float)nosel[1] / 255.0f;
			nosel_col[2] = (float)nosel[2] / 255.0f;
		}
	}

	if(editupdate && psys->pathcache && totpart == psys->totcached) {
		cache = psys->pathcache;
	}
	else {
		/* clear out old and create new empty path cache */
		psys_free_path_cache(psys);
		cache= psys_alloc_path_cache_buffers(&psys->pathcachebufs, totpart, steps+1);
		psys->pathcache= cache;
	}

	if(edit==NULL && psys->soft && psys->softflag & OB_SB_ENABLE)
		soft = psys->soft;
	
	psys->lattice = psys_get_lattice(ob, psys);
	ma= give_current_material(ob, psys->part->omat);
	if(ma && (psys->part->draw & PART_DRAW_MAT_COL))
		VECCOPY(col, &ma->r)
	
	if(psys->part->from!=PART_FROM_PARTICLE) {
		if(!(psys->part->flag & PART_CHILD_EFFECT))
			vg_effector = psys_cache_vgroup(psmd->dm, psys, PSYS_VG_EFFECTOR);
	}

	/*---first main loop: create all actual particles' paths---*/
	for(i=0,pa=psys->particles; i<totpart; i++, pa++){
		if(psys && edit==NULL && (pa->flag & PARS_NO_DISP || pa->flag & PARS_UNEXIST)) {
			if(soft)
				bp[0] += pa->totkey; /* TODO use of initialized value? */
			continue;
		}

		if(editupdate && !(pa->flag & PARS_EDIT_RECALC)) continue;
		else memset(cache[i], 0, sizeof(*cache[i])*(steps+1));

		cache[i]->steps = steps;

		if(edit)
			ekey = edit->keys[i];

		/*--get the first data points--*/
		if(psys->flag & PSYS_KEYED) {
			kkey[0] = pa->keys;
			kkey[1] = kkey[0] + 1;

			birthtime = kkey[0]->time;
			dietime = kkey[0][pa->totkey-1].time;
		}
		else {
			hkey[0] = pa->hair;
			hkey[1] = hkey[0] + 1;

			birthtime = hkey[0]->time;
			dietime = hkey[0][pa->totkey-1].time;

			psys_mat_hair_to_global(ob, psmd->dm, psys->part->from, pa, hairmat);
		}

		if(soft){
			bp[0] = soft->bpoint + pa->bpi;
			bp[1] = bp[0] + 1;
		}

		/*--interpolate actual path from data points--*/
		for(k=0, ca=cache[i]; k<=steps; k++, ca++){
			time = (float)k / (float)steps;

			t = birthtime + time * (dietime - birthtime);

			if(psys->flag & PSYS_KEYED) {
				while(kkey[1]->time < t) {
					kkey[1]++;
				}

				kkey[0] = kkey[1] - 1;				
			}
			else {
				while(hkey[1]->time < t) {
					hkey[1]++;
					bp[1]++;
				}

				hkey[0] = hkey[1] - 1;
			}

			if(soft) {
				bp[0] = bp[1] - 1;
				bp_to_particle(keys + 1, bp[0], hkey[0]);
				bp_to_particle(keys + 2, bp[1], hkey[1]);
			}
			else if(psys->flag & PSYS_KEYED) {
				memcpy(keys + 1, kkey[0], sizeof(ParticleKey));
				memcpy(keys + 2, kkey[1], sizeof(ParticleKey));
			}
			else {
				hair_to_particle(keys + 1, hkey[0]);
				hair_to_particle(keys + 2, hkey[1]);
			}


			if((psys->flag & PSYS_KEYED)==0) {
				if(soft) {
					if(hkey[0] != pa->hair)
						bp_to_particle(keys, bp[0] - 1, hkey[0] - 1);
					else
						bp_to_particle(keys, bp[0], hkey[0]);
				}
				else {
					if(hkey[0] != pa->hair)
						hair_to_particle(keys, hkey[0] - 1);
					else
						hair_to_particle(keys, hkey[0]);
				}

				if(soft) {
					if(hkey[1] != pa->hair + pa->totkey - 1)
						bp_to_particle(keys + 3, bp[1] + 1, hkey[1] + 1);
					else
						bp_to_particle(keys + 3, bp[1], hkey[1]);
				}
				else {
					if(hkey[1] != pa->hair + pa->totkey - 1)
						hair_to_particle(keys + 3, hkey[1] + 1);
					else
						hair_to_particle(keys + 3, hkey[1]);
				}
			}

			dfra = keys[2].time - keys[1].time;

			keytime = (t - keys[1].time) / dfra;

			/* convert velocity to timestep size */
			if(psys->flag & PSYS_KEYED){
				VecMulf(keys[1].vel, dfra / frs_sec);
				VecMulf(keys[2].vel, dfra / frs_sec);
			}

			/* now we should have in chronologiacl order k1<=k2<=t<=k3<=k4 with keytime between [0,1]->[k2,k3] (k1 & k4 used for cardinal & bspline interpolation)*/
			interpolate_particle((psys->flag & PSYS_KEYED) ? -1 /* signal for cubic interpolation */
				: ((psys->part->flag & PART_HAIR_BSPLINE) ? KEY_BSPLINE : KEY_CARDINAL)
				,keys, keytime, &result, 0);

			/* the velocity needs to be converted back from cubic interpolation */
			if(psys->flag & PSYS_KEYED){
				VecMulf(result.vel, frs_sec / dfra);
			}
			else if(soft==NULL) { /* softbody and keyed are allready in global space */
				Mat4MulVecfl(hairmat, result.co);
			}

			VECCOPY(ca->co, result.co);

			/* selection coloring in edit mode */
			if(edit){
				if(pset->brushtype==PE_BRUSH_WEIGHT){
					if(k==steps)
						VecLerpf(ca->col, nosel_col, sel_col, hkey[0]->weight);
					else
						VecLerpf(ca->col, nosel_col, sel_col,
						(1.0f - keytime) * hkey[0]->weight + keytime * hkey[1]->weight);
				}
				else{
					if((ekey + (hkey[0] - pa->hair))->flag & PEK_SELECT){
						if((ekey + (hkey[1] - pa->hair))->flag & PEK_SELECT){
							VECCOPY(ca->col, sel_col);
						}
						else{
							VecLerpf(ca->col, sel_col, nosel_col, keytime);
						}
					}
					else{
						if((ekey + (hkey[1] - pa->hair))->flag & PEK_SELECT){
							VecLerpf(ca->col, nosel_col, sel_col, keytime);
						}
						else{
							VECCOPY(ca->col, nosel_col);
						}
					}
				}
			}
			else{
				VECCOPY(ca->col, col);
			}
		}
		
		/*--modify paths--*/

		VecSubf(vec,(cache[i]+1)->co,cache[i]->co);
		length = VecLength(vec);

		effector= 1.0f;
		if(vg_effector)
			effector*= psys_interpolate_value_from_verts(psmd->dm,psys->part->from,pa->num,pa->fuv,vg_effector);

		for(k=0, ca=cache[i]; k<=steps; k++, ca++) {
			/* apply effectors */
			if(!(psys->part->flag & PART_CHILD_EFFECT) && edit==0 && k)
				do_path_effectors(ob, psys, i, ca, k, steps, cache[i]->co, effector, dfra, cfra, &length, vec);

			/* apply guide curves to path data */
			if(edit==0 && psys->effectors.first && (psys->part->flag & PART_CHILD_EFFECT)==0)
				/* ca is safe to cast, since only co and vel are used */
				do_guide((ParticleKey*)ca, i, (float)k/(float)steps, &psys->effectors);

			/* apply lattice */
			if(psys->lattice && edit==0)
				calc_latt_deform(ca->co, 1.0f);

			/* figure out rotation */
			
			if(k) {
				float cosangle, angle, tangent[3], normal[3], q[4];

				if(k == 1) {
					VECSUB(tangent, ca->co, (ca - 1)->co);

					vectoquat(tangent, OB_POSX, OB_POSZ, (ca-1)->rot);

					VECCOPY(prev_tangent, tangent);
					Normalize(prev_tangent);
				}
				else {
					VECSUB(tangent, ca->co, (ca - 1)->co);
					Normalize(tangent);

					cosangle= Inpf(tangent, prev_tangent);

					/* note we do the comparison on cosangle instead of
					 * angle, since floating point accuracy makes it give
					 * different results across platforms */
					if(cosangle > 0.999999f) {
						QUATCOPY((ca - 1)->rot, (ca - 2)->rot);
					}
					else {
						angle= saacos(cosangle);
						Crossf(normal, prev_tangent, tangent);
						VecRotToQuat(normal, angle, q);
						QuatMul((ca - 1)->rot, q, (ca - 2)->rot);
					}

					VECCOPY(prev_tangent, tangent);
				}

				if(k == steps)
					QUATCOPY(ca->rot, (ca - 1)->rot);
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
		end_latt_deform();
		psys->lattice=0;
	}

	if(vg_effector)
		MEM_freeN(vg_effector);
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
	/*
	VECCOPY(to->co,from->co);
	VECCOPY(to->vel,from->vel);
	QUATCOPY(to->rot,from->rot);
	if(time)
		to->time=from->time;
	to->flag=from->flag;
	to->sbw=from->sbw;
	*/
}
void psys_get_from_key(ParticleKey *key, float *loc, float *vel, float *rot, float *time){
	if(loc) VECCOPY(loc,key->co);
	if(vel) VECCOPY(vel,key->vel);
	if(rot) QUATCOPY(rot,key->rot);
	if(time) *time=key->time;
}
/*-------changing particle keys from space to another-------*/
void psys_key_to_object(Object *ob, ParticleKey *key, float imat[][4]){
	float q[4], imat2[4][4];

	if(imat==0){
		Mat4Invert(imat2,ob->obmat);
		imat=imat2;
	}

	VECADD(key->vel,key->vel,key->co);

	Mat4MulVecfl(imat,key->co);
	Mat4MulVecfl(imat,key->vel);
	Mat4ToQuat(imat,q);

	VECSUB(key->vel,key->vel,key->co);
	QuatMul(key->rot,q,key->rot);
}
static void key_from_object(Object *ob, ParticleKey *key){
	float q[4];

	VECADD(key->vel,key->vel,key->co);

	Mat4MulVecfl(ob->obmat,key->co);
	Mat4MulVecfl(ob->obmat,key->vel);
	Mat4ToQuat(ob->obmat,q);

	VECSUB(key->vel,key->vel,key->co);
	QuatMul(key->rot,q,key->rot);
}

static void triatomat(float *v1, float *v2, float *v3, float (*uv)[2], float mat[][4])
{
	float det, w1, w2, d1[2], d2[2];

	memset(mat, 0, sizeof(float)*4*4);
	mat[3][3]= 1.0f;

	/* first axis is the normal */
	CalcNormFloat(v1, v2, v3, mat[2]);

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
			Normalize(mat[1]);
		}
		else
			mat[1][0]= mat[1][1]= mat[1][2]= 0.0f;
	}
	else {
		VecSubf(mat[1], v2, v1);
		Normalize(mat[1]);
	}
	
	/* third as a cross product */
	Crossf(mat[0], mat[1], mat[2]);
}

static void psys_face_mat(Object *ob, DerivedMesh *dm, ParticleData *pa, float mat[][4], int orco)
{
	float v[3][3];
	MFace *mface;
	OrigSpaceFace *osface;
	float (*orcodata)[3];

	int i = pa->num_dmcache==DMCACHE_NOTFOUND ? pa->num : pa->num_dmcache;
	
	if (i==-1 || i >= dm->getNumFaces(dm)) { Mat4One(mat); return; }

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
	psys_particle_on_dm(ob, dm, from, pa->num, pa->num_dmcache, pa->fuv, pa->foffset, vec, 0, 0, 0, 0, 0);
	VECCOPY(hairmat[3],vec);
}

void psys_mat_hair_to_orco(Object *ob, DerivedMesh *dm, short from, ParticleData *pa, float hairmat[][4])
{
	float vec[3], orco[3];

	psys_face_mat(ob, dm, pa, hairmat, 1);
	psys_particle_on_dm(ob, dm, from, pa->num, pa->num_dmcache, pa->fuv, pa->foffset, vec, 0, 0, 0, orco, 0);

	/* see psys_face_mat for why this function is called */
	transform_mesh_orco_verts(ob->data, &orco, 1, 1);
	VECCOPY(hairmat[3],orco);
}

/*
void psys_key_to_geometry(DerivedMesh *dm, ParticleData *pa, ParticleKey *key)
{
	float q[4], v1[3], v2[3], v3[3];

	dm->getVertCo(dm,pa->verts[0],v1);
	dm->getVertCo(dm,pa->verts[1],v2);
	dm->getVertCo(dm,pa->verts[2],v3);

	triatoquat(v1, v2, v3, q);

	QuatInv(q);

	VECSUB(key->co,key->co,v1);

	VECADD(key->vel,key->vel,key->co);

	QuatMulVecf(q, key->co);
	QuatMulVecf(q, key->vel);
	
	VECSUB(key->vel,key->vel,key->co);

	QuatMul(key->rot,q,key->rot);
}

void psys_key_from_geometry(DerivedMesh *dm, ParticleData *pa, ParticleKey *key)
{
	float q[4], v1[3], v2[3], v3[3];

	dm->getVertCo(dm,pa->verts[0],v1);
	dm->getVertCo(dm,pa->verts[1],v2);
	dm->getVertCo(dm,pa->verts[2],v3);

	triatoquat(v1, v2, v3, q);

	VECADD(key->vel,key->vel,key->co);

	QuatMulVecf(q, key->co);
	QuatMulVecf(q, key->vel);
	
	VECSUB(key->vel,key->vel,key->co);

	VECADD(key->co,key->co,v1);

	QuatMul(key->rot,q,key->rot);
}
*/

void psys_vec_rot_to_face(DerivedMesh *dm, ParticleData *pa, float *vec)//to_geometry(DerivedMesh *dm, ParticleData *pa, float *vec)
{
	float mat[4][4];

	psys_face_mat(0, dm, pa, mat, 0);
	Mat4Transp(mat); /* cheap inverse for rotation matrix */
	Mat4Mul3Vecfl(mat, vec);
}

/* unused */
#if 0
static void psys_vec_rot_from_face(DerivedMesh *dm, ParticleData *pa, float *vec)//from_geometry(DerivedMesh *dm, ParticleData *pa, float *vec)
{
	float q[4], v1[3], v2[3], v3[3];
	/*
	dm->getVertCo(dm,pa->verts[0],v1);
	dm->getVertCo(dm,pa->verts[1],v2);
	dm->getVertCo(dm,pa->verts[2],v3);
	*/
	/* replace with this */
	MFace *mface;
	int i; // = psys_particle_dm_face_lookup(dm, pa->num, pa->fuv, pa->foffset, (LinkNode*)NULL);
	i = pa->num_dmcache==DMCACHE_NOTFOUND ? pa->num : pa->num_dmcache;
	if (i==-1 || i >= dm->getNumFaces(dm)) { vec[0] = vec[1] = 0; vec[2] = 1; return; }
	mface=dm->getFaceData(dm,i,CD_MFACE);
	
	dm->getVertCo(dm,mface->v1,v1);
	dm->getVertCo(dm,mface->v2,v2);
	dm->getVertCo(dm,mface->v3,v3);
	/* done */
	
	triatoquat(v1, v2, v3, q);

	QuatMulVecf(q, vec);

	//VECADD(vec,vec,v1);
}
#endif

void psys_mat_hair_to_global(Object *ob, DerivedMesh *dm, short from, ParticleData *pa, float hairmat[][4])
{
	float facemat[4][4];

	psys_mat_hair_to_object(ob, dm, from, pa, facemat);

	Mat4MulMat4(hairmat, facemat, ob->obmat);
}

/************************************************/
/*			ParticleSettings handling			*/
/************************************************/
static void default_particle_settings(ParticleSettings *part)
{
	int i;

	part->type= PART_EMITTER;
	part->distr= PART_DISTR_JIT;
	part->draw_as=PART_DRAW_DOT;
	part->bb_uv_split=1;
	part->bb_align=PART_BB_VIEW;
	part->bb_split_offset=PART_BB_OFF_LINEAR;
	part->flag=PART_REACT_MULTIPLE|PART_HAIR_GEOMETRY;

	part->sta= 1.0;
	part->end= 100.0;
	part->lifetime= 50.0;
	part->jitfac= 1.0;
	part->totpart= 1000;
	part->grid_res= 10;
	part->timetweak= 1.0;
	part->keyed_time= 0.5;
	//part->userjit;
	
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
	part->length= 1.0;
	part->nbetween= 4;
	part->boidneighbours= 5;

	part->max_vel = 10.0f;
	part->average_vel = 0.3f;
	part->max_tan_acc = 0.2f;
	part->max_lat_acc = 1.0f;

	part->reactshape=1.0f;

	part->mass=1.0;
	part->size=1.0;
	part->childsize=1.0;

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

	part->draw_line[0]=0.5;

	part->banking=1.0;
	part->max_bank=1.0;

	for(i=0; i<BOID_TOT_RULES; i++){
		part->boidrule[i]=(char)i;
		part->boidfac[i]=0.5;
	}

	part->ipo = NULL;

	part->simplify_refsize= 1920;
	part->simplify_rate= 1.0f;
	part->simplify_transition= 0.1f;
	part->simplify_viewport= 0.8;
}


ParticleSettings *psys_new_settings(char *name, Main *main)
{
	ParticleSettings *part;

	part= alloc_libblock(&main->particle, ID_PA, name);
	
	default_particle_settings(part);

	return part;
}

ParticleSettings *psys_copy_settings(ParticleSettings *part)
{
	ParticleSettings *partn;
	
	partn= copy_libblock(part);
	if(partn->pd) partn->pd= MEM_dupallocN(part->pd);
	
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

/* should be integrated to depgraph signals */
void psys_flush_settings(ParticleSettings *part, int event, int hair_recalc)
{
	Base *base;
	Object *ob, *tob;
	ParticleSystem *psys;
	int flush;

	/* update all that have same particle settings */
	for(base = G.scene->base.first; base; base= base->next) {
		if(base->object->particlesystem.first) {
			ob=base->object;
			flush=0;
			for(psys=ob->particlesystem.first; psys; psys=psys->next){
				if(psys->part==part){
					psys->recalc |= event;
					if(hair_recalc)
						psys->recalc |= PSYS_RECALC_HAIR;
					flush++;
				}
				else if(psys->part->type==PART_REACTOR){
					ParticleSystem *tpsys;
					tob=psys->target_ob;
					if(tob==0)
						tob=ob;
					tpsys=BLI_findlink(&tob->particlesystem,psys->target_psys-1);

					if(tpsys && tpsys->part==part){
						psys->recalc |= event;
						flush++;
					}
				}
			}
			if(flush)
				DAG_object_flush_update(G.scene, ob, OB_RECALC_DATA);
		}
	}
}

LinkNode *psys_using_settings(ParticleSettings *part, int flush_update)
{
	Object *ob, *tob;
	ParticleSystem *psys, *tpsys;
	LinkNode *node= NULL;
	int found;

	/* update all that have same particle settings */
	for(ob=G.main->object.first; ob; ob=ob->id.next) {
		found= 0;

		for(psys=ob->particlesystem.first; psys; psys=psys->next) {
			if(psys->part == part) {
				BLI_linklist_append(&node, psys);
				found++;
			}
			else if(psys->part->type == PART_REACTOR){
				tob= (psys->target_ob)? psys->target_ob: ob;
				tpsys= BLI_findlink(&tob->particlesystem, psys->target_psys-1);

				if(tpsys && tpsys->part==part) {
					BLI_linklist_append(&node, tpsys);
					found++;
				}
			}
		}

		if(flush_update && found)
			DAG_object_flush_update(G.scene, ob, OB_RECALC_DATA);
	}

	return node;
}


/************************************************/
/*			Textures							*/
/************************************************/
static void get_cpa_texture(DerivedMesh *dm, Material *ma, int face_index, float *fw, float *orco, ParticleTexture *ptex, int event)
{
	MTex *mtex;
	int m,setvars=0;
	float value, rgba[4], texco[3];

	if(ma) for(m=0; m<MAX_MTEX; m++){
		mtex=ma->mtex[m];
		if(mtex && (ma->septex & (1<<m))==0){
			float def=mtex->def_var;
			float var=mtex->varfac;
			short blend=mtex->blendtype;
			short neg=mtex->pmaptoneg;

			if(mtex->texco & TEXCO_UV && fw){
				int uv_index=CustomData_get_named_layer_index(&dm->faceData,CD_MTFACE,mtex->uvname);
				if(uv_index<0){
					uv_index=CustomData_get_active_layer_index(&dm->faceData,CD_MTFACE);
				}
				if(uv_index>=0){
					CustomDataLayer *layer=&dm->faceData.layers[uv_index];
					MTFace *mtface= &((MTFace*)layer->data)[face_index];
					MFace *mf=dm->getFaceData(dm,face_index,CD_MFACE);
					psys_interpolate_uvs(mtface,mf->v4,fw,texco);
					texco[0]*=2.0;
					texco[1]*=2.0;
					texco[0]-=1.0;
					texco[1]-=1.0;
				}
				else
					VECCOPY(texco,orco);
			}
			else{
				VECCOPY(texco,orco);
			}
			externtex(mtex, texco, &value, rgba, rgba+1, rgba+2, rgba+3);
			if((event & mtex->pmapto) & MAP_PA_TIME){
				if((setvars&MAP_PA_TIME)==0){
					ptex->time=0.0;
					setvars|=MAP_PA_TIME;
				}
				ptex->time= texture_value_blend(mtex->def_var,ptex->time,value,var,blend,neg & MAP_PA_TIME);
			}
			if((event & mtex->pmapto) & MAP_PA_LENGTH)
				ptex->length= texture_value_blend(def,ptex->length,value,var,blend,neg & MAP_PA_LENGTH);
			if((event & mtex->pmapto) & MAP_PA_CLUMP)
				ptex->clump= texture_value_blend(def,ptex->clump,value,var,blend,neg & MAP_PA_CLUMP);
			if((event & mtex->pmapto) & MAP_PA_KINK)
				ptex->kink= texture_value_blend(def,ptex->kink,value,var,blend,neg & MAP_PA_KINK);
			if((event & mtex->pmapto) & MAP_PA_ROUGH)
				ptex->rough= texture_value_blend(def,ptex->rough,value,var,blend,neg & MAP_PA_ROUGH);
		}
	}
	if(event & MAP_PA_TIME) { CLAMP(ptex->time,0.0,1.0); }
	if(event & MAP_PA_LENGTH) { CLAMP(ptex->length,0.0,1.0); }
	if(event & MAP_PA_CLUMP) { CLAMP(ptex->clump,0.0,1.0); }
	if(event & MAP_PA_KINK) { CLAMP(ptex->kink,0.0,1.0); }
	if(event & MAP_PA_ROUGH) { CLAMP(ptex->rough,0.0,1.0); }
}
void psys_get_texture(Object *ob, Material *ma, ParticleSystemModifierData *psmd, ParticleSystem *psys, ParticleData *pa, ParticleTexture *ptex, int event)
{
	MTex *mtex;
	int m;
	float value, rgba[4], co[3], texco[3];
	int setvars=0;

	if(ma) for(m=0; m<MAX_MTEX; m++){
		mtex=ma->mtex[m];
		if(mtex && (ma->septex & (1<<m))==0){
			float var=mtex->varfac;
			float def=mtex->def_var;
			short blend=mtex->blendtype;
			short neg=mtex->pmaptoneg;

			if(mtex->texco & TEXCO_UV){
				int uv_index=CustomData_get_named_layer_index(&psmd->dm->faceData,CD_MTFACE,mtex->uvname);
				if(uv_index<0){
					uv_index=CustomData_get_active_layer_index(&psmd->dm->faceData,CD_MTFACE);
				}
				if(uv_index>=0){
					CustomDataLayer *layer=&psmd->dm->faceData.layers[uv_index];
					MTFace *mtface= &((MTFace*)layer->data)[pa->num];
					MFace *mf=psmd->dm->getFaceData(psmd->dm,pa->num,CD_MFACE);
					psys_interpolate_uvs(mtface,mf->v4,pa->fuv,texco);
					texco[0]*=2.0;
					texco[1]*=2.0;
					texco[0]-=1.0;
					texco[1]-=1.0;
				}
				else
					//psys_particle_on_emitter(ob,psmd,psys->part->from,pa->num,pa->fuv,pa->foffset,texco,0,0,0);
					/* <jahka> anyways I think it will be too small a difference to notice, so psys_get_texture should only know about the original mesh structure.. no dm needed anywhere */
					/* <brecht> the code only does dm based lookup now, so passing num_dmcache anyway to avoid^
					 * massive slowdown here */
					psys_particle_on_emitter(ob,psmd,psys->part->from,pa->num,pa->num_dmcache,pa->fuv,pa->foffset,co,0,0,0,texco, 0);
			}
			else{
				//psys_particle_on_emitter(ob,psmd,psys->part->from,pa->num,pa->fuv,pa->offset,texco,0,0,0);
				/* ditto above */
				psys_particle_on_emitter(ob,psmd,psys->part->from,pa->num,pa->num_dmcache,pa->fuv,pa->foffset,co,0,0,0,texco, 0);
			}
			externtex(mtex, texco, &value, rgba, rgba+1, rgba+2, rgba+3);

			if((event & mtex->pmapto) & MAP_PA_TIME){
				/* the first time has to set the base value for time regardless of blend mode */
				if((setvars&MAP_PA_TIME)==0){
					ptex->time *= 1.0f - var;
					ptex->time += var * ((neg & MAP_PA_TIME)? 1.0f - value : value);
					setvars |= MAP_PA_TIME;
				}
				else
					ptex->time= texture_value_blend(def,ptex->time,value,var,blend,neg & MAP_PA_TIME);
			}
			if((event & mtex->pmapto) & MAP_PA_LIFE)
				ptex->life= texture_value_blend(def,ptex->life,value,var,blend,neg & MAP_PA_LIFE);
			if((event & mtex->pmapto) & MAP_PA_DENS)
				ptex->exist= texture_value_blend(def,ptex->exist,value,var,blend,neg & MAP_PA_DENS);
			if((event & mtex->pmapto) & MAP_PA_SIZE)
				ptex->size= texture_value_blend(def,ptex->size,value,var,blend,neg & MAP_PA_SIZE);
			if((event & mtex->pmapto) & MAP_PA_IVEL)
				ptex->ivel= texture_value_blend(def,ptex->ivel,value,var,blend,neg & MAP_PA_IVEL);
			if((event & mtex->pmapto) & MAP_PA_PVEL)
				texture_rgb_blend(ptex->pvel,rgba,ptex->pvel,value,var,blend);
			if((event & mtex->pmapto) & MAP_PA_LENGTH)
				ptex->length= texture_value_blend(def,ptex->length,value,var,blend,neg & MAP_PA_LENGTH);
			if((event & mtex->pmapto) & MAP_PA_CLUMP)
				ptex->clump= texture_value_blend(def,ptex->clump,value,var,blend,neg & MAP_PA_CLUMP);
			if((event & mtex->pmapto) & MAP_PA_KINK)
				ptex->kink= texture_value_blend(def,ptex->kink,value,var,blend,neg & MAP_PA_CLUMP);
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
float psys_get_timestep(ParticleSettings *part)
{
	return 0.04f*part->timetweak;
}
/* part->size should be updated with possible ipo effection before this is called */
float psys_get_size(Object *ob, Material *ma, ParticleSystemModifierData *psmd, IpoCurve *icu_size, ParticleSystem *psys, ParticleSettings *part, ParticleData *pa, float *vg_size)
{
	ParticleTexture ptex;
	float size=1.0f;
	
	if(ma && part->from!=PART_FROM_PARTICLE){
		ptex.size=size;
		psys_get_texture(ob,ma,psmd,psys,pa,&ptex,MAP_PA_SIZE);
		size=ptex.size;
	}
	
	if(icu_size){
		calc_icu(icu_size,pa->time);
		size*=icu_size->curval;
	}

	if(vg_size)
		size*=psys_interpolate_value_from_verts(psmd->dm,part->from,pa->num,pa->fuv,vg_size);

	if(part->randsize!=0.0)
		size*= 1.0f - part->randsize*pa->sizemul;

	return size*part->size;
}
float psys_get_child_time(ParticleSystem *psys, ChildParticle *cpa, float cfra)
{
	ParticleSettings *part = psys->part;

	if(part->childtype==PART_CHILD_FACES){
		float time;
		int w=0;
		time=0.0;
		while(w<4 && cpa->pa[w]>=0){
			time+=cpa->w[w]*(psys->particles+cpa->pa[w])->time;
			w++;
		}

		return (cfra-time)/(part->lifetime*(1.0f-part->randlife*cpa->rand[1]));
	}
	else{
		ParticleData *pa = psys->particles + cpa->parent;
		return (cfra-pa->time)/pa->lifetime;
	}
}
float psys_get_child_size(ParticleSystem *psys, ChildParticle *cpa, float cfra, float *pa_time)
{
	ParticleSettings *part = psys->part;
	float size, time;
	
	if(part->childtype==PART_CHILD_FACES){
		if(pa_time)
			time=*pa_time;
		else
			time=psys_get_child_time(psys,cpa,cfra);

		if((part->flag&PART_ABS_TIME)==0 && part->ipo){
			calc_ipo(part->ipo, 100*time);
			execute_ipo((ID *)part, part->ipo);
		}
		size=part->size;
	}
	else
		size=psys->particles[cpa->parent].size;

	size*=part->childsize;

	if(part->childrandsize!=0.0)
		size *= 1.0f - part->childrandsize*cpa->rand[2];

	return size;
}
/* get's hair (or keyed) particles state at the "path time" specified in state->time */
void psys_get_particle_on_path(Object *ob, ParticleSystem *psys, int p, ParticleKey *state, int vel)
{
	ParticleSettings *part = psys->part;
	ParticleSystemModifierData *psmd = psys_get_modifier(ob, psys);
	Material *ma = give_current_material(ob, part->omat);
	ParticleData *pa;
	ChildParticle *cpa;
	ParticleTexture ptex;
	ParticleKey *kkey[2] = {NULL, NULL};
	HairKey *hkey[2] = {NULL, NULL};
	ParticleKey *par=0, keys[4];

	float t, real_t, dfra, keytime, frs_sec = G.scene->r.frs_sec;
	float co[3], orco[3];
	float hairmat[4][4];
	float pa_clump = 0.0, pa_kink = 0.0;
	int totparent = 0;
	int totpart = psys->totpart;
	int totchild = psys->totchild;
	short between = 0, edit = 0;

	float *cpa_fuv; int cpa_num; short cpa_from;

	//if(psys_in_edit_mode(psys)){
	//	if((psys->edit_path->flag & PSYS_EP_SHOW_CHILD)==0)
	//		totchild=0;
	//	edit=1;
	//}

	/* user want's cubic interpolation but only without sb it possible */
	//if(interpolation==PART_INTER_CUBIC && baked && psys->softflag==OB_SB_ENABLE)
	//	interpolation=PART_INTER_BSPLINE;
	//else if(baked==0) /* it doesn't make sense to use other types for keyed */
	//	interpolation=PART_INTER_CUBIC;

	t=state->time;
	CLAMP(t, 0.0, 1.0);

	if(p<totpart){
		pa = psys->particles + p;

		if(pa->alive==PARS_DEAD && part->flag & PART_STICKY && pa->flag & PARS_STICKY && pa->stick_ob){
			copy_particle_key(state,&pa->state,0);
			key_from_object(pa->stick_ob,state);
			return;
		}
		
		if(psys->flag & PSYS_KEYED) {
			kkey[0] = pa->keys;
			kkey[1] = kkey[0] + 1;

			real_t = kkey[0]->time + t * (kkey[0][pa->totkey-1].time - kkey[0]->time);
		}
		else {
			hkey[0] = pa->hair;
			hkey[1] = pa->hair + 1;

			real_t = hkey[0]->time + (hkey[0][pa->totkey-1].time - hkey[0]->time) * t;
		}

		if(psys->flag & PSYS_KEYED) {
			while(kkey[1]->time < real_t) {
				kkey[1]++;
			}
			kkey[0] = kkey[1] - 1;

			memcpy(keys + 1, kkey[0], sizeof(ParticleKey));
			memcpy(keys + 2, kkey[1], sizeof(ParticleKey));
		}
		else {
			while(hkey[1]->time < real_t)
				hkey[1]++;

			hkey[0] = hkey[1] - 1;

			hair_to_particle(keys + 1, hkey[0]);
			hair_to_particle(keys + 2, hkey[1]);
		}

		if((psys->flag & PSYS_KEYED)==0) {
		//if(soft){
		//	if(key[0] != sbel.keys)
		//		DB_copy_key(&k1,key[0]-1);
		//	else
		//		DB_copy_key(&k1,&k2);
		//}
		//else{
			if(hkey[0] != pa->hair)
				hair_to_particle(keys, hkey[0] - 1);
			else
				hair_to_particle(keys, hkey[0]);
		//}

		//if(soft){
		//	if(key[1] != sbel.keys + sbel.totkey-1)
		//		DB_copy_key(&k4,key[1]+1);
		//	else
		//		DB_copy_key(&k4,&k3);
		//}
		//else {
			if(hkey[1] != pa->hair + pa->totkey - 1)
				hair_to_particle(keys + 3, hkey[1] + 1);
			else
				hair_to_particle(keys + 3, hkey[1]);
		}
		//}

		//psys_get_particle_on_path(bsys,p,t,bkey,ckey[0]);

		//if(part->rotfrom==PART_ROT_KEYS)
		//	QuatInterpol(state->rot,k2.rot,k3.rot,keytime);
		//else{
		//	/* TODO: different rotations */
		//	float nvel[3];
		//	VECCOPY(nvel,state->vel);
		//	VecMulf(nvel,-1.0f);
		//	vectoquat(nvel, OB_POSX, OB_POSZ, state->rot);
		//}

		dfra = keys[2].time - keys[1].time;

		keytime = (real_t - keys[1].time) / dfra;

		/* convert velocity to timestep size */
		if(psys->flag & PSYS_KEYED){
			VecMulf(keys[1].vel, dfra / frs_sec);
			VecMulf(keys[2].vel, dfra / frs_sec);
			QuatInterpol(state->rot,keys[1].rot,keys[2].rot,keytime);
		}

		interpolate_particle((psys->flag & PSYS_KEYED) ? -1 /* signal for cubic interpolation */
			: ((psys->part->flag & PART_HAIR_BSPLINE) ? KEY_BSPLINE : KEY_CARDINAL)
			,keys, keytime, state, 1);

		/* the velocity needs to be converted back from cubic interpolation */
		if(psys->flag & PSYS_KEYED){
			VecMulf(state->vel, frs_sec / dfra);
		}
		else {
			if((pa->flag & PARS_REKEY)==0) {
				psys_mat_hair_to_global(ob, psmd->dm, part->from, pa, hairmat);
				Mat4MulVecfl(hairmat, state->co);
				Mat4Mul3Vecfl(hairmat, state->vel);

				if(psys->effectors.first && (part->flag & PART_CHILD_GUIDE)==0) {
					do_guide(state, p, state->time, &psys->effectors);
					/* TODO: proper velocity handling */
				}

				if(psys->lattice && edit==0)
					calc_latt_deform(state->co,1.0f);
			}
		}
	}
	else if(totchild){
		//Mat4Invert(imat,ob->obmat);
		
		cpa=psys->child+p-totpart;
		
		if(totchild && part->from!=PART_FROM_PARTICLE && part->childtype==PART_CHILD_FACES){
			totparent=(int)(totchild*part->parents*0.3);
			/* part->parents could still be 0 so we can't test with totparent */
			between=1;
		}
		if(between){
			int w = 0;
			float foffset;

			/* get parent states */
			while(w<4 && cpa->pa[w]>=0){
				keys[w].time = t;
				psys_get_particle_on_path(ob, psys, cpa->pa[w], keys+w, 1);
				w++;
			}

			/* get the original coordinates (orco) for texture usage */
			cpa_num=cpa->num;
			
			foffset= cpa->foffset;
			if(part->childtype == PART_CHILD_FACES)
				foffset = -(2.0f + part->childspread);
			cpa_fuv = cpa->fuv;
			cpa_from = PART_FROM_FACE;

			psys_particle_on_emitter(ob,psmd,cpa_from,cpa_num,DMCACHE_ISCHILD,cpa->fuv,foffset,co,0,0,0,orco,0);

			/* we need to save the actual root position of the child for positioning it accurately to the surface of the emitter */
			//VECCOPY(cpa_1st,co);

			//Mat4MulVecfl(ob->obmat,cpa_1st);

			pa=0;
		}
		else{
			/* get the parent state */

			keys->time = t;
			psys_get_particle_on_path(ob,psys,cpa->parent,keys,1);

			/* get the original coordinates (orco) for texture usage */
			pa=psys->particles+cpa->parent;

			cpa_from=part->from;
			cpa_num=pa->num;
			cpa_fuv=pa->fuv;

			psys_particle_on_emitter(ob,psmd,cpa_from,cpa_num,DMCACHE_ISCHILD,cpa_fuv,pa->foffset,co,0,0,0,orco,0);
		}

		/* correct child ipo timing */
		if((part->flag&PART_ABS_TIME)==0 && part->ipo){
			calc_ipo(part->ipo, 100.0f*t);
			execute_ipo((ID *)part, part->ipo);
		}

		/* get different child parameters from textures & vgroups */
		ptex.clump=1.0;
		ptex.kink=1.0;
		
		get_cpa_texture(psmd->dm,ma,cpa_num,cpa_fuv,orco,&ptex,MAP_PA_CLUMP|MAP_PA_KINK);
		
		pa_clump=ptex.clump;
		pa_kink=ptex.kink;

		/* TODO: vertex groups */

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
		//if(totparent){
		//	if(p-totpart>=totparent){
		//		key.time=t;
		//		psys_get_particle_on_path(ob,psys,totpart+cpa->parent,&key,1);
		//		bti->convert_dynamic_key(bsys,&key,par,cpar);
		//	}
		//	else
		//		par=0;
		//}
		//else
		//	DB_get_key_on_path(bsys,cpa->parent,t,par,cpar);

		/* apply different deformations to the child path */
		if(part->kink)
			do_prekink(state, par, par->rot, t, part->kink_freq * pa_kink, part->kink_shape,
			part->kink_amp, part->kink, part->kink_axis, ob->obmat);
		
		do_clump(state, par, t, part->clumpfac, part->clumppow, 1.0f);

		if(part->rough1 != 0.0)
			do_rough(orco, t, part->rough1, part->rough1_size, 0.0, state);

		if(part->rough2 != 0.0)
			do_rough(cpa->rand, t, part->rough2, part->rough2_size, part->rough2_thres, state);

		if(part->rough_end != 0.0)
			do_rough_end(cpa->rand, t, part->rough_end, part->rough_end_shape, state, par); 

		//if(vel){
		//	if(t>=0.001f){
		//		tstate.time=t-0.001f;
		//		psys_get_particle_on_path(ob,psys,p,&tstate,0);
		//		VECSUB(state->vel,state->co,tstate.co);
		//	}
		//	else{
		//		tstate.time=t+0.001f;
		//		psys_get_particle_on_path(ob,psys,p,&tstate,0);
		//		VECSUB(state->vel,tstate.co,state->co);
		//	}
		//}
	}
}
/* gets particle's state at a time, returns 1 if particle exists and can be seen and 0 if not */
int psys_get_particle_state(Object *ob, ParticleSystem *psys, int p, ParticleKey *state, int always){
	ParticleSettings *part=psys->part;
	ParticleData *pa=0;
	float cfra;
	int totpart=psys->totpart, between=0;

	if(state->time>0)
		cfra=state->time;
	else
		cfra=bsystem_time(0,(float)G.scene->r.cfra,0.0);

	if(psys->totchild && p>=totpart){
		if(part->from!=PART_FROM_PARTICLE && part->childtype==PART_CHILD_FACES){
			between=1;
		}
		else
			pa=psys->particles+(psys->child+p-totpart)->parent;
	}
	else
		pa=psys->particles+p;

	if(between){
		state->time = psys_get_child_time(psys,&psys->child[p-totpart],cfra);

		if(always==0)
			if((state->time<0.0 && (part->flag & PART_UNBORN)==0)
				|| (state->time>1.0 && (part->flag & PART_DIED)==0))
				return 0;
	}
	else{
		if(pa->alive==PARS_KILLED) return 0;
		if(always==0)
			if((pa->alive==PARS_UNBORN && (part->flag & PART_UNBORN)==0)
				|| (pa->alive==PARS_DEAD && (part->flag & PART_DIED)==0))
				return 0;
	}

	if(psys->flag & PSYS_KEYED){
		if(between){
			ChildParticle *cpa=psys->child+p-totpart;
			state->time= (cfra-(part->sta+(part->end-part->sta)*cpa->rand[0]))/(part->lifetime*cpa->rand[1]);
		}
		else
			state->time= (cfra-pa->time)/(pa->dietime-pa->time);

		psys_get_particle_on_path(ob,psys,p,state,1);
		return 1;
	}
	else{
		if(between)
			return 0; /* currently not supported */
		else if(psys->totchild && p>=psys->totpart){
			ChildParticle *cpa=psys->child+p-psys->totpart;
			ParticleKey *key1, skey;
			float t = (cfra - pa->time + pa->loop * pa->lifetime) / pa->lifetime;

			pa = psys->particles + cpa->parent;

			if(pa->alive==PARS_DEAD && part->flag&PART_STICKY && pa->flag&PARS_STICKY && pa->stick_ob) {
				key1 = &skey;
				copy_particle_key(key1,&pa->state,0);
				key_from_object(pa->stick_ob,key1);
			}
			else {
				key1=&pa->state;
			}
			
			offset_child(cpa, key1, state, part->childflat, part->childrad);
			
			CLAMP(t,0.0,1.0);
			if(part->kink)			/* TODO: part->kink_freq*pa_kink */
				do_prekink(state,key1,key1->rot,t,part->kink_freq,part->kink_shape,part->kink_amp,part->kink,part->kink_axis,ob->obmat);
			
			/* TODO: pa_clump vgroup */
			do_clump(state,key1,t,part->clumpfac,part->clumppow,1.0);
		}
		else{
			if (pa) { /* TODO PARTICLE - should this ever be NULL? - Campbell */
				copy_particle_key(state,&pa->state,0);

				if(pa->alive==PARS_DEAD && part->flag&PART_STICKY && pa->flag&PARS_STICKY && pa->stick_ob){
					key_from_object(pa->stick_ob,state);
				}

				if(psys->lattice)
					calc_latt_deform(state->co,1.0f);
			}
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

		psys_particle_on_emitter(ob, psmd,
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

		psys_particle_on_emitter(ob,psmd,part->from,pa->num,pa->num_dmcache,pa->fuv,pa->foffset,loc,0,0,0,orco,0);
	}
}

void psys_get_dupli_path_transform(Object *ob, ParticleSystem *psys, ParticleSystemModifierData *psmd, ParticleData *pa, ChildParticle *cpa, ParticleCacheKey *cache, float mat[][4], float *scale)
{
	float loc[3], nor[3], vec[3], side[3], len, obrotmat[4][4], qmat[4][4];
	float xvec[3] = {-1.0, 0.0, 0.0}, q[4];

	VecSubf(vec, (cache+cache->steps-1)->co, cache->co);
	len= Normalize(vec);

	if(pa)
		psys_particle_on_emitter(ob,psmd,psys->part->from,pa->num,pa->num_dmcache,pa->fuv,pa->foffset,loc,nor,0,0,0,0);
	else
		psys_particle_on_emitter(ob, psmd,
			(psys->part->childtype == PART_CHILD_FACES)? PART_FROM_FACE: PART_FROM_PARTICLE,
			cpa->num,DMCACHE_ISCHILD,cpa->fuv,cpa->foffset,loc,nor,0,0,0,0);
	
	if(psys->part->rotmode) {
		if(!pa)
			pa= psys->particles+cpa->pa[0];

		vectoquat(xvec, ob->trackflag, ob->upflag, q);
		QuatToMat4(q, obrotmat);
		obrotmat[3][3]= 1.0f;

		QuatToMat4(pa->state.rot, qmat);
		Mat4MulMat4(mat, obrotmat, qmat);
	}
	else {
		Crossf(side, nor, vec);
		Normalize(side);
		Crossf(nor, vec, side);

		Mat4One(mat);
		VECCOPY(mat[0], vec);
		VECCOPY(mat[1], side);
		VECCOPY(mat[2], nor);
	}

	*scale= len;
}

