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
 * Contributors: Brecht Van Lommel.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/render/intern/source/strand.c
 *  \ingroup render
 */


#include <math.h>
#include <string.h>
#include <stdlib.h>

#include "MEM_guardedalloc.h"

#include "DNA_key_types.h"
#include "DNA_material_types.h"
#include "DNA_meshdata_types.h"

#include "BLI_math.h"
#include "BLI_blenlib.h"
#include "BLI_utildefines.h"
#include "BLI_ghash.h"
#include "BLI_memarena.h"
#include "BLI_rand.h"

#include "BKE_DerivedMesh.h"
#include "BKE_key.h"


#include "render_types.h"
#include "rendercore.h"
#include "renderdatabase.h"
#include "shading.h"
#include "strand.h"
#include "zbuf.h"

/* *************** */

static float strand_eval_width(Material *ma, float strandco)
{
	float fac;

	strandco= 0.5f*(strandco + 1.0f);

	if (ma->strand_ease!=0.0f) {
		if (ma->strand_ease<0.0f)
			fac= pow(strandco, 1.0f+ma->strand_ease);
		else
			fac= pow(strandco, 1.0f/(1.0f-ma->strand_ease));
	}
	else fac= strandco;
	
	return ((1.0f-fac)*ma->strand_sta + (fac)*ma->strand_end);
}

void strand_eval_point(StrandSegment *sseg, StrandPoint *spoint)
{
	Material *ma;
	StrandBuffer *strandbuf;
	const float *simplify;
	float p[4][3], data[4], cross[3], w, dx, dy, t;
	int type;

	strandbuf= sseg->buffer;
	ma= sseg->buffer->ma;
	t= spoint->t;
	type= (strandbuf->flag & R_STRAND_BSPLINE)? KEY_BSPLINE: KEY_CARDINAL;

	copy_v3_v3(p[0], sseg->v[0]->co);
	copy_v3_v3(p[1], sseg->v[1]->co);
	copy_v3_v3(p[2], sseg->v[2]->co);
	copy_v3_v3(p[3], sseg->v[3]->co);

	if (sseg->obi->flag & R_TRANSFORMED) {
		mul_m4_v3(sseg->obi->mat, p[0]);
		mul_m4_v3(sseg->obi->mat, p[1]);
		mul_m4_v3(sseg->obi->mat, p[2]);
		mul_m4_v3(sseg->obi->mat, p[3]);
	}

	if (t == 0.0f) {
		copy_v3_v3(spoint->co, p[1]);
		spoint->strandco= sseg->v[1]->strandco;

		spoint->dtstrandco= (sseg->v[2]->strandco - sseg->v[0]->strandco);
		if (sseg->v[0] != sseg->v[1])
			spoint->dtstrandco *= 0.5f;
	}
	else if (t == 1.0f) {
		copy_v3_v3(spoint->co, p[2]);
		spoint->strandco= sseg->v[2]->strandco;

		spoint->dtstrandco= (sseg->v[3]->strandco - sseg->v[1]->strandco);
		if (sseg->v[3] != sseg->v[2])
			spoint->dtstrandco *= 0.5f;
	}
	else {
		key_curve_position_weights(t, data, type);
		spoint->co[0]= data[0]*p[0][0] + data[1]*p[1][0] + data[2]*p[2][0] + data[3]*p[3][0];
		spoint->co[1]= data[0]*p[0][1] + data[1]*p[1][1] + data[2]*p[2][1] + data[3]*p[3][1];
		spoint->co[2]= data[0]*p[0][2] + data[1]*p[1][2] + data[2]*p[2][2] + data[3]*p[3][2];
		spoint->strandco= (1.0f-t)*sseg->v[1]->strandco + t*sseg->v[2]->strandco;
	}

	key_curve_tangent_weights(t, data, type);
	spoint->dtco[0]= data[0]*p[0][0] + data[1]*p[1][0] + data[2]*p[2][0] + data[3]*p[3][0];
	spoint->dtco[1]= data[0]*p[0][1] + data[1]*p[1][1] + data[2]*p[2][1] + data[3]*p[3][1];
	spoint->dtco[2]= data[0]*p[0][2] + data[1]*p[1][2] + data[2]*p[2][2] + data[3]*p[3][2];

	normalize_v3_v3(spoint->tan, spoint->dtco);
	normalize_v3_v3(spoint->nor, spoint->co);
	negate_v3(spoint->nor);

	spoint->width= strand_eval_width(ma, spoint->strandco);
	
	/* simplification */
	simplify= RE_strandren_get_simplify(strandbuf->obr, sseg->strand, 0);
	spoint->alpha= (simplify)? simplify[1]: 1.0f;

	/* outer points */
	cross_v3_v3v3(cross, spoint->co, spoint->tan);

	w= spoint->co[2]*strandbuf->winmat[2][3] + strandbuf->winmat[3][3];
	dx= strandbuf->winx*cross[0]*strandbuf->winmat[0][0]/w;
	dy= strandbuf->winy*cross[1]*strandbuf->winmat[1][1]/w;
	w = sqrtf(dx * dx + dy * dy);

	if (w > 0.0f) {
		if (strandbuf->flag & R_STRAND_B_UNITS) {
			const float crosslen= len_v3(cross);
			w= 2.0f*crosslen*strandbuf->minwidth/w;

			if (spoint->width < w) {
				spoint->alpha= spoint->width/w;
				spoint->width= w;
			}

			if (simplify)
				/* squared because we only change width, not length */
				spoint->width *= simplify[0]*simplify[0];

			mul_v3_fl(cross, spoint->width*0.5f/crosslen);
		}
		else
			mul_v3_fl(cross, spoint->width/w);
	}

	sub_v3_v3v3(spoint->co1, spoint->co, cross);
	add_v3_v3v3(spoint->co2, spoint->co, cross);

	copy_v3_v3(spoint->dsco, cross);
}

/* *************** */

static void interpolate_vec1(float *v1, float *v2, float t, float negt, float *v)
{
	v[0]= negt*v1[0] + t*v2[0];
}

static void interpolate_vec3(float *v1, float *v2, float t, float negt, float *v)
{
	v[0]= negt*v1[0] + t*v2[0];
	v[1]= negt*v1[1] + t*v2[1];
	v[2]= negt*v1[2] + t*v2[2];
}

static void interpolate_vec4(float *v1, float *v2, float t, float negt, float *v)
{
	v[0]= negt*v1[0] + t*v2[0];
	v[1]= negt*v1[1] + t*v2[1];
	v[2]= negt*v1[2] + t*v2[2];
	v[3]= negt*v1[3] + t*v2[3];
}

static void interpolate_shade_result(ShadeResult *shr1, ShadeResult *shr2, float t, ShadeResult *shr, int addpassflag)
{
	float negt= 1.0f - t;

	interpolate_vec4(shr1->combined, shr2->combined, t, negt, shr->combined);

	if (addpassflag & SCE_PASS_VECTOR) {
		interpolate_vec4(shr1->winspeed, shr2->winspeed, t, negt, shr->winspeed);
	}
	/* optim... */
	if (addpassflag & ~(SCE_PASS_VECTOR)) {
		if (addpassflag & SCE_PASS_Z)
			interpolate_vec1(&shr1->z, &shr2->z, t, negt, &shr->z);
		if (addpassflag & SCE_PASS_RGBA)
			interpolate_vec4(shr1->col, shr2->col, t, negt, shr->col);
		if (addpassflag & SCE_PASS_NORMAL) {
			interpolate_vec3(shr1->nor, shr2->nor, t, negt, shr->nor);
			normalize_v3(shr->nor);
		}
		if (addpassflag & SCE_PASS_EMIT)
			interpolate_vec3(shr1->emit, shr2->emit, t, negt, shr->emit);
		if (addpassflag & SCE_PASS_DIFFUSE) {
			interpolate_vec3(shr1->diff, shr2->diff, t, negt, shr->diff);
			interpolate_vec3(shr1->diffshad, shr2->diffshad, t, negt, shr->diffshad);
		}
		if (addpassflag & SCE_PASS_SPEC)
			interpolate_vec3(shr1->spec, shr2->spec, t, negt, shr->spec);
		if (addpassflag & SCE_PASS_SHADOW)
			interpolate_vec3(shr1->shad, shr2->shad, t, negt, shr->shad);
		if (addpassflag & SCE_PASS_AO)
			interpolate_vec3(shr1->ao, shr2->ao, t, negt, shr->ao);
		if (addpassflag & SCE_PASS_ENVIRONMENT)
			interpolate_vec3(shr1->env, shr2->env, t, negt, shr->env);
		if (addpassflag & SCE_PASS_INDIRECT)
			interpolate_vec3(shr1->indirect, shr2->indirect, t, negt, shr->indirect);
		if (addpassflag & SCE_PASS_REFLECT)
			interpolate_vec3(shr1->refl, shr2->refl, t, negt, shr->refl);
		if (addpassflag & SCE_PASS_REFRACT)
			interpolate_vec3(shr1->refr, shr2->refr, t, negt, shr->refr);
		if (addpassflag & SCE_PASS_MIST)
			interpolate_vec1(&shr1->mist, &shr2->mist, t, negt, &shr->mist);
	}
}

static void strand_apply_shaderesult_alpha(ShadeResult *shr, float alpha)
{
	if (alpha < 1.0f) {
		shr->combined[0] *= alpha;
		shr->combined[1] *= alpha;
		shr->combined[2] *= alpha;
		shr->combined[3] *= alpha;

		shr->col[0] *= alpha;
		shr->col[1] *= alpha;
		shr->col[2] *= alpha;
		shr->col[3] *= alpha;

		shr->alpha *= alpha;
	}
}

static void strand_shade_point(Render *re, ShadeSample *ssamp, StrandSegment *sseg, StrandVert *svert, StrandPoint *spoint)
{
	ShadeInput *shi= ssamp->shi;
	ShadeResult *shr= ssamp->shr;
	VlakRen vlr;
	int seed;

	memset(&vlr, 0, sizeof(vlr));
	vlr.flag= R_SMOOTH;
	if (sseg->buffer->ma->mode & MA_TANGENT_STR)
		vlr.flag |= R_TANGENT;

	shi->vlr= &vlr;
	shi->v1= NULL;
	shi->v2= NULL;
	shi->v3= NULL;
	shi->strand= sseg->strand;
	shi->obi= sseg->obi;
	shi->obr= sseg->obi->obr;

	/* cache for shadow */
	shi->samplenr= re->shadowsamplenr[shi->thread]++;

	/* all samples */
	shi->mask= 0xFFFF;

	/* seed RNG for consistent results across tiles */
	seed = shi->strand->index + (svert - shi->strand->vert);
	BLI_thread_srandom(shi->thread, seed);

	shade_input_set_strand(shi, sseg->strand, spoint);
	shade_input_set_strand_texco(shi, sseg->strand, sseg->v[1], spoint);
	
	/* init material vars */
	shade_input_init_material(shi);
	
	/* shade */
	shade_samples_do_AO(ssamp);
	shade_input_do_shade(shi, shr);

	/* apply simplification */
	strand_apply_shaderesult_alpha(shr, spoint->alpha);

	/* include lamphalos for strand, since halo layer was added already */
	if (re->flag & R_LAMPHALO)
		if (shi->layflag & SCE_LAY_HALO)
			renderspothalo(shi, shr->combined, shr->combined[3]);
	
	shi->strand= NULL;
}

/* *************** */

struct StrandShadeCache {
	GHash *resulthash;
	GHash *refcounthash;
	MemArena *memarena;
};

typedef struct StrandCacheEntry {
	GHashPair pair;
	ShadeResult shr;
} StrandCacheEntry;

StrandShadeCache *strand_shade_cache_create(void)
{
	StrandShadeCache *cache;

	cache= MEM_callocN(sizeof(StrandShadeCache), "StrandShadeCache");
	cache->resulthash= BLI_ghash_pair_new("strand_shade_cache_create1 gh");
	cache->refcounthash= BLI_ghash_pair_new("strand_shade_cache_create2 gh");
	cache->memarena= BLI_memarena_new(BLI_MEMARENA_STD_BUFSIZE, "strand shade cache arena");
	
	return cache;
}

void strand_shade_cache_free(StrandShadeCache *cache)
{
	BLI_ghash_free(cache->refcounthash, NULL, NULL);
	BLI_ghash_free(cache->resulthash, MEM_freeN, NULL);
	BLI_memarena_free(cache->memarena);
	MEM_freeN(cache);
}

static GHashPair strand_shade_hash_pair(ObjectInstanceRen *obi, StrandVert *svert)
{
	GHashPair pair = {obi, svert};
	return pair;
}

static void strand_shade_get(Render *re, StrandShadeCache *cache, ShadeSample *ssamp, StrandSegment *sseg, StrandVert *svert)
{
	StrandCacheEntry *entry;
	StrandPoint p;
	int *refcount;
	GHashPair pair = strand_shade_hash_pair(sseg->obi, svert);

	entry= BLI_ghash_lookup(cache->resulthash, &pair);
	refcount= BLI_ghash_lookup(cache->refcounthash, &pair);

	if (!entry) {
		/* not shaded yet, shade and insert into hash */
		p.t= (sseg->v[1] == svert)? 0.0f: 1.0f;
		strand_eval_point(sseg, &p);
		strand_shade_point(re, ssamp, sseg, svert, &p);

		entry= MEM_callocN(sizeof(StrandCacheEntry), "StrandCacheEntry");
		entry->pair = pair;
		entry->shr = ssamp->shr[0];
		BLI_ghash_insert(cache->resulthash, entry, entry);
	}
	else
		/* already shaded, just copy previous result from hash */
		ssamp->shr[0]= entry->shr;
	
	/* lower reference count and remove if not needed anymore by any samples */
	(*refcount)--;
	if (*refcount == 0) {
		BLI_ghash_remove(cache->resulthash, &pair, MEM_freeN, NULL);
		BLI_ghash_remove(cache->refcounthash, &pair, NULL, NULL);
	}
}

void strand_shade_segment(Render *re, StrandShadeCache *cache, StrandSegment *sseg, ShadeSample *ssamp, float t, float s, int addpassflag)
{
	ShadeResult shr1, shr2;

	/* get shading for two endpoints and interpolate */
	strand_shade_get(re, cache, ssamp, sseg, sseg->v[1]);
	shr1= ssamp->shr[0];
	strand_shade_get(re, cache, ssamp, sseg, sseg->v[2]);
	shr2= ssamp->shr[0];

	interpolate_shade_result(&shr1, &shr2, t, ssamp->shr, addpassflag);

	/* apply alpha along width */
	if (sseg->buffer->widthfade != -1.0f) {
		s = 1.0f - powf(fabsf(s), sseg->buffer->widthfade);

		strand_apply_shaderesult_alpha(ssamp->shr, s);
	}
}

void strand_shade_unref(StrandShadeCache *cache, ObjectInstanceRen *obi, StrandVert *svert)
{
	GHashPair pair = strand_shade_hash_pair(obi, svert);
	int *refcount;

	/* lower reference count and remove if not needed anymore by any samples */
	refcount= BLI_ghash_lookup(cache->refcounthash, &pair);

	(*refcount)--;
	if (*refcount == 0) {
		BLI_ghash_remove(cache->resulthash, &pair, MEM_freeN, NULL);
		BLI_ghash_remove(cache->refcounthash, &pair, NULL, NULL);
	}
}

static void strand_shade_refcount(StrandShadeCache *cache, StrandSegment *sseg, StrandVert *svert)
{
	GHashPair pair = strand_shade_hash_pair(sseg->obi, svert);
	GHashPair *key;
	int *refcount= BLI_ghash_lookup(cache->refcounthash, &pair);

	if (!refcount) {
		key= BLI_memarena_alloc(cache->memarena, sizeof(GHashPair));
		*key = pair;
		refcount= BLI_memarena_alloc(cache->memarena, sizeof(int));
		*refcount= 1;
		BLI_ghash_insert(cache->refcounthash, key, refcount);
	}
	else
		(*refcount)++;
}

/* *************** */

typedef struct StrandPart {
	Render *re;
	ZSpan *zspan;

	APixstrand *apixbuf;
	int *totapixbuf;
	int *rectz;
	int *rectmask;
	intptr_t *rectdaps;
	int rectx, recty;
	int sample;
	int shadow;
	float (*jit)[2];
	int samples;

	StrandSegment *segment;
	float t[3], s[3];

	StrandShadeCache *cache;
} StrandPart;

typedef struct StrandSortSegment {
	struct StrandSortSegment *next;
	int obi, strand, segment;
	float z;
} StrandSortSegment;

static int compare_strand_segment(const void *poin1, const void *poin2)
{
	const StrandSortSegment *seg1= (const StrandSortSegment*)poin1;
	const StrandSortSegment *seg2= (const StrandSortSegment*)poin2;

	if (seg1->z < seg2->z)
		return -1;
	else if (seg1->z == seg2->z)
		return 0;
	else
		return 1;
}

static void do_strand_point_project(float winmat[4][4], ZSpan *zspan, float *co, float *hoco, float *zco)
{
	projectvert(co, winmat, hoco);
	hoco_to_zco(zspan, zco, hoco);
}

static void strand_project_point(float winmat[4][4], float winx, float winy, StrandPoint *spoint)
{
	float div;

	projectvert(spoint->co, winmat, spoint->hoco);

	div= 1.0f/spoint->hoco[3];
	spoint->x= spoint->hoco[0]*div*winx*0.5f;
	spoint->y= spoint->hoco[1]*div*winy*0.5f;
}

static APixstrand *addpsmainAstrand(ListBase *lb)
{
	APixstrMain *psm;

	psm= MEM_mallocN(sizeof(APixstrMain), "addpsmainA");
	BLI_addtail(lb, psm);
	psm->ps = MEM_callocN(4096 * sizeof(APixstrand), "pixstr");

	return psm->ps;
}

static APixstrand *addpsAstrand(ZSpan *zspan)
{
	/* make new PS */
	if (zspan->apstrandmcounter==0) {
		zspan->curpstrand= addpsmainAstrand(zspan->apsmbase);
		zspan->apstrandmcounter= 4095;
	}
	else {
		zspan->curpstrand++;
		zspan->apstrandmcounter--;
	}
	return zspan->curpstrand;
}

#define MAX_ZROW	2000

static void do_strand_fillac(void *handle, int x, int y, float u, float v, float z)
{
	StrandPart *spart= (StrandPart *)handle;
	StrandShadeCache *cache= spart->cache;
	StrandSegment *sseg= spart->segment;
	APixstrand *apn, *apnew;
	float t, s;
	int offset, mask, obi, strnr, seg, zverg, bufferz, maskz=0;

	offset = y*spart->rectx + x;
	obi= sseg->obi - spart->re->objectinstance;
	strnr= sseg->strand->index + 1;
	seg= sseg->v[1] - sseg->strand->vert;
	mask= (1<<spart->sample);

	/* check against solid z-buffer */
	zverg= (int)z;

	if (spart->rectdaps) {
		/* find the z of the sample */
		PixStr *ps;
		intptr_t *rd= spart->rectdaps + offset;
		
		bufferz= 0x7FFFFFFF;
		if (spart->rectmask) maskz= 0x7FFFFFFF;
		
		if (*rd) {
			for (ps= (PixStr *)(*rd); ps; ps= ps->next) {
				if (mask & ps->mask) {
					bufferz= ps->z;
					if (spart->rectmask)
						maskz= ps->maskz;
					break;
				}
			}
		}
	}
	else {
		bufferz= (spart->rectz)? spart->rectz[offset]: 0x7FFFFFFF;
		if (spart->rectmask)
			maskz= spart->rectmask[offset];
	}

#define CHECK_ADD(n) \
	if (apn->p[n]==strnr && apn->obi[n]==obi && apn->seg[n]==seg) \
	{ if (!(apn->mask[n] & mask)) { apn->mask[n] |= mask; apn->v[n] += t; apn->u[n] += s; } break; } (void)0
#define CHECK_ASSIGN(n) \
	if (apn->p[n]==0) \
	{apn->obi[n]= obi; apn->p[n]= strnr; apn->z[n]= zverg; apn->mask[n]= mask; apn->v[n]= t; apn->u[n]= s; apn->seg[n]= seg; break; } (void)0

	/* add to pixel list */
	if (zverg < bufferz && (spart->totapixbuf[offset] < MAX_ZROW)) {
		if (!spart->rectmask || zverg > maskz) {
			t = u * spart->t[0] + v * spart->t[1] + (1.0f - u - v) * spart->t[2];
			s = fabsf(u * spart->s[0] + v * spart->s[1] + (1.0f - u - v) * spart->s[2]);

			apn= spart->apixbuf + offset;
			while (apn) {
				CHECK_ADD(0);
				CHECK_ADD(1);
				CHECK_ADD(2);
				CHECK_ADD(3);
				CHECK_ASSIGN(0);
				CHECK_ASSIGN(1);
				CHECK_ASSIGN(2);
				CHECK_ASSIGN(3);

				apnew= addpsAstrand(spart->zspan);
				SWAP(APixstrand, *apnew, *apn);
				apn->next= apnew;
				CHECK_ASSIGN(0);
			}

			if (cache) {
				strand_shade_refcount(cache, sseg, sseg->v[1]);
				strand_shade_refcount(cache, sseg, sseg->v[2]);
			}
			spart->totapixbuf[offset]++;
		}
	}
}

/* width is calculated in hoco space, to ensure strands are visible */
static int strand_test_clip(float winmat[4][4], ZSpan *UNUSED(zspan), float *bounds, float *co, float *zcomp, float widthx, float widthy)
{
	float hoco[4];
	int clipflag= 0;

	projectvert(co, winmat, hoco);

	/* we compare z without perspective division for segment sorting */
	*zcomp= hoco[2];

	if (hoco[0]+widthx < bounds[0]*hoco[3]) clipflag |= 1;
	else if (hoco[0]-widthx > bounds[1]*hoco[3]) clipflag |= 2;
	
	if (hoco[1]-widthy > bounds[3]*hoco[3]) clipflag |= 4;
	else if (hoco[1]+widthy < bounds[2]*hoco[3]) clipflag |= 8;

	clipflag |= testclip(hoco);

	return clipflag;
}

static void do_scanconvert_strand(Render *UNUSED(re), StrandPart *spart, ZSpan *zspan, float t, float dt, float *co1, float *co2, float *co3, float *co4, int sample)
{
	float jco1[3], jco2[3], jco3[3], jco4[3], jx, jy;

	copy_v3_v3(jco1, co1);
	copy_v3_v3(jco2, co2);
	copy_v3_v3(jco3, co3);
	copy_v3_v3(jco4, co4);

	if (spart->jit) {
		jx= -spart->jit[sample][0];
		jy= -spart->jit[sample][1];

		jco1[0] += jx; jco1[1] += jy;
		jco2[0] += jx; jco2[1] += jy;
		jco3[0] += jx; jco3[1] += jy;
		jco4[0] += jx; jco4[1] += jy;

		/* XXX mblur? */
	}

	spart->sample= sample;

	spart->t[0]= t-dt;
	spart->s[0]= -1.0f;
	spart->t[1]= t-dt;
	spart->s[1]= 1.0f;
	spart->t[2]= t;
	spart->s[2]= 1.0f;
	zspan_scanconvert_strand(zspan, spart, jco1, jco2, jco3, do_strand_fillac);
	spart->t[0]= t-dt;
	spart->s[0]= -1.0f;
	spart->t[1]= t;
	spart->s[1]= 1.0f;
	spart->t[2]= t;
	spart->s[2]= -1.0f;
	zspan_scanconvert_strand(zspan, spart, jco1, jco3, jco4, do_strand_fillac);
}

static void strand_render(Render *re, StrandSegment *sseg, float winmat[4][4], StrandPart *spart, ZSpan *zspan, int totzspan, StrandPoint *p1, StrandPoint *p2)
{
	if (spart) {
		float t= p2->t;
		float dt= p2->t - p1->t;
		int a;

		for (a=0; a<spart->samples; a++)
			do_scanconvert_strand(re, spart, zspan, t, dt, p1->zco2, p1->zco1, p2->zco1, p2->zco2, a);
	}
	else {
		float hoco1[4], hoco2[4];
		int a, obi, index;

		obi= sseg->obi - re->objectinstance;
		index= sseg->strand->index;

		projectvert(p1->co, winmat, hoco1);
		projectvert(p2->co, winmat, hoco2);


		for (a=0; a<totzspan; a++) {
#if 0
			/* render both strand and single pixel wire to counter aliasing */
			zbufclip4(re, &zspan[a], obi, index, p1->hoco2, p1->hoco1, p2->hoco1, p2->hoco2, p1->clip2, p1->clip1, p2->clip1, p2->clip2);
#endif
			/* only render a line for now, which makes the shadow map more
			 * similar across frames, and so reduces flicker */
			zbufsinglewire(&zspan[a], obi, index, hoco1, hoco2);
		}
	}
}

static int strand_segment_recursive(Render *re, float winmat[4][4], StrandPart *spart, ZSpan *zspan, int totzspan, StrandSegment *sseg, StrandPoint *p1, StrandPoint *p2, int depth)
{
	StrandPoint p;
	StrandBuffer *buffer= sseg->buffer;
	float dot, d1[2], d2[2], len1, len2;

	if (depth == buffer->maxdepth)
		return 0;

	p.t= (p1->t + p2->t)*0.5f;
	strand_eval_point(sseg, &p);
	strand_project_point(buffer->winmat, buffer->winx, buffer->winy, &p);

	d1[0]= (p.x - p1->x);
	d1[1]= (p.y - p1->y);
	len1= d1[0]*d1[0] + d1[1]*d1[1];

	d2[0]= (p2->x - p.x);
	d2[1]= (p2->y - p.y);
	len2= d2[0]*d2[0] + d2[1]*d2[1];

	if (len1 == 0.0f || len2 == 0.0f)
		return 0;
	
	dot= d1[0]*d2[0] + d1[1]*d2[1];
	if (dot*dot > sseg->sqadaptcos*len1*len2)
		return 0;

	if (spart) {
		do_strand_point_project(winmat, zspan, p.co1, p.hoco1, p.zco1);
		do_strand_point_project(winmat, zspan, p.co2, p.hoco2, p.zco2);
	}
	else {
#if 0
		projectvert(p.co1, winmat, p.hoco1);
		projectvert(p.co2, winmat, p.hoco2);
		p.clip1= testclip(p.hoco1);
		p.clip2= testclip(p.hoco2);
#endif
	}

	if (!strand_segment_recursive(re, winmat, spart, zspan, totzspan, sseg, p1, &p, depth+1))
		strand_render(re, sseg, winmat, spart, zspan, totzspan, p1, &p);
	if (!strand_segment_recursive(re, winmat, spart, zspan, totzspan, sseg, &p, p2, depth+1))
		strand_render(re, sseg, winmat, spart, zspan, totzspan, &p, p2);
	
	return 1;
}

void render_strand_segment(Render *re, float winmat[4][4], StrandPart *spart, ZSpan *zspan, int totzspan, StrandSegment *sseg)
{
	StrandBuffer *buffer= sseg->buffer;
	StrandPoint *p1= &sseg->point1;
	StrandPoint *p2= &sseg->point2;

	p1->t= 0.0f;
	p2->t= 1.0f;

	strand_eval_point(sseg, p1);
	strand_project_point(buffer->winmat, buffer->winx, buffer->winy, p1);
	strand_eval_point(sseg, p2);
	strand_project_point(buffer->winmat, buffer->winx, buffer->winy, p2);

	if (spart) {
		do_strand_point_project(winmat, zspan, p1->co1, p1->hoco1, p1->zco1);
		do_strand_point_project(winmat, zspan, p1->co2, p1->hoco2, p1->zco2);
		do_strand_point_project(winmat, zspan, p2->co1, p2->hoco1, p2->zco1);
		do_strand_point_project(winmat, zspan, p2->co2, p2->hoco2, p2->zco2);
	}
	else {
#if 0
		projectvert(p1->co1, winmat, p1->hoco1);
		projectvert(p1->co2, winmat, p1->hoco2);
		projectvert(p2->co1, winmat, p2->hoco1);
		projectvert(p2->co2, winmat, p2->hoco2);
		p1->clip1= testclip(p1->hoco1);
		p1->clip2= testclip(p1->hoco2);
		p2->clip1= testclip(p2->hoco1);
		p2->clip2= testclip(p2->hoco2);
#endif
	}

	if (!strand_segment_recursive(re, winmat, spart, zspan, totzspan, sseg, p1, p2, 0))
		strand_render(re, sseg, winmat, spart, zspan, totzspan, p1, p2);
}

/* render call to fill in strands */
int zbuffer_strands_abuf(Render *re, RenderPart *pa, APixstrand *apixbuf, ListBase *apsmbase, unsigned int lay, int UNUSED(negzmask), float winmat[4][4], int winx, int winy, int samples, float (*jit)[2], float clipcrop, int shadow, StrandShadeCache *cache)
{
	ObjectRen *obr;
	ObjectInstanceRen *obi;
	ZSpan zspan;
	StrandRen *strand = NULL;
	StrandVert *svert;
	StrandBound *sbound;
	StrandPart spart;
	StrandSegment sseg;
	StrandSortSegment *sortsegments = NULL, *sortseg, *firstseg;
	MemArena *memarena;
	float z[4], bounds[4], obwinmat[4][4];
	int a, b, c, i, totsegment, clip[4];

	if (re->test_break(re->tbh))
		return 0;
	if (re->totstrand == 0)
		return 0;

	/* setup StrandPart */
	memset(&spart, 0, sizeof(spart));

	spart.re= re;
	spart.rectx= pa->rectx;
	spart.recty= pa->recty;
	spart.apixbuf= apixbuf;
	spart.zspan= &zspan;
	spart.rectdaps= pa->rectdaps;
	spart.rectz= pa->rectz;
	spart.rectmask= pa->rectmask;
	spart.cache= cache;
	spart.shadow= shadow;
	spart.jit= jit;
	spart.samples= samples;

	zbuf_alloc_span(&zspan, pa->rectx, pa->recty, clipcrop);

	/* needed for transform from hoco to zbuffer co */
	zspan.zmulx= ((float)winx)/2.0f;
	zspan.zmuly= ((float)winy)/2.0f;
	
	zspan.zofsx= -pa->disprect.xmin;
	zspan.zofsy= -pa->disprect.ymin;

	/* to center the sample position */
	if (!shadow) {
		zspan.zofsx -= 0.5f;
		zspan.zofsy -= 0.5f;
	}

	zspan.apsmbase= apsmbase;

	/* clipping setup */
	bounds[0]= (2*pa->disprect.xmin - winx-1)/(float)winx;
	bounds[1]= (2*pa->disprect.xmax - winx+1)/(float)winx;
	bounds[2]= (2*pa->disprect.ymin - winy-1)/(float)winy;
	bounds[3]= (2*pa->disprect.ymax - winy+1)/(float)winy;

	memarena= BLI_memarena_new(BLI_MEMARENA_STD_BUFSIZE, "strand sort arena");
	firstseg= NULL;
	totsegment= 0;

	/* for all object instances */
	for (obi=re->instancetable.first, i=0; obi; obi=obi->next, i++) {
		Material *ma;
		float widthx, widthy;

		obr= obi->obr;

		if (!obr->strandbuf || !(obr->strandbuf->lay & lay))
			continue;

		/* compute matrix and try clipping whole object */
		if (obi->flag & R_TRANSFORMED)
			mul_m4_m4m4(obwinmat, winmat, obi->mat);
		else
			copy_m4_m4(obwinmat, winmat);

		/* test if we should skip it */
		ma = obr->strandbuf->ma;

		if (shadow && (!(ma->mode2 & MA_CASTSHADOW) || !(ma->mode & MA_SHADBUF)))
			continue;
		else if (!shadow && (ma->mode & MA_ONLYCAST))
			continue;

		if (clip_render_object(obi->obr->boundbox, bounds, obwinmat))
			continue;
		
		widthx= obr->strandbuf->maxwidth*obwinmat[0][0];
		widthy= obr->strandbuf->maxwidth*obwinmat[1][1];

		/* for each bounding box containing a number of strands */
		sbound= obr->strandbuf->bound;
		for (c=0; c<obr->strandbuf->totbound; c++, sbound++) {
			if (clip_render_object(sbound->boundbox, bounds, obwinmat))
				continue;

			/* for each strand in this bounding box */
			for (a=sbound->start; a<sbound->end; a++) {
				strand= RE_findOrAddStrand(obr, a);
				svert= strand->vert;

				/* keep clipping and z depth for 4 control points */
				clip[1]= strand_test_clip(obwinmat, &zspan, bounds, svert->co, &z[1], widthx, widthy);
				clip[2]= strand_test_clip(obwinmat, &zspan, bounds, (svert+1)->co, &z[2], widthx, widthy);
				clip[0]= clip[1]; z[0]= z[1];

				for (b=0; b<strand->totvert-1; b++, svert++) {
					/* compute 4th point clipping and z depth */
					if (b < strand->totvert-2) {
						clip[3]= strand_test_clip(obwinmat, &zspan, bounds, (svert+2)->co, &z[3], widthx, widthy);
					}
					else {
						clip[3]= clip[2]; z[3]= z[2];
					}

					/* check clipping and add to sortsegments buffer */
					if (!(clip[0] & clip[1] & clip[2] & clip[3])) {
						sortseg= BLI_memarena_alloc(memarena, sizeof(StrandSortSegment));
						sortseg->obi= i;
						sortseg->strand= strand->index;
						sortseg->segment= b;

						sortseg->z= 0.5f*(z[1] + z[2]);

						sortseg->next= firstseg;
						firstseg= sortseg;
						totsegment++;
					}

					/* shift clipping and z depth */
					clip[0]= clip[1]; z[0]= z[1];
					clip[1]= clip[2]; z[1]= z[2];
					clip[2]= clip[3]; z[2]= z[3];
				}
			}
		}
	}

	if (!re->test_break(re->tbh)) {
		/* convert list to array and sort */
		sortsegments= MEM_mallocN(sizeof(StrandSortSegment)*totsegment, "StrandSortSegment");
		for (a=0, sortseg=firstseg; a<totsegment; a++, sortseg=sortseg->next)
			sortsegments[a]= *sortseg;
		qsort(sortsegments, totsegment, sizeof(StrandSortSegment), compare_strand_segment);
	}

	BLI_memarena_free(memarena);

	spart.totapixbuf= MEM_callocN(sizeof(int)*pa->rectx*pa->recty, "totapixbuf");

	if (!re->test_break(re->tbh)) {
		/* render segments in sorted order */
		sortseg= sortsegments;
		for (a=0; a<totsegment; a++, sortseg++) {
			if (re->test_break(re->tbh))
				break;

			obi= &re->objectinstance[sortseg->obi];
			obr= obi->obr;

			sseg.obi= obi;
			sseg.strand= RE_findOrAddStrand(obr, sortseg->strand);
			sseg.buffer= sseg.strand->buffer;
			sseg.sqadaptcos= sseg.buffer->adaptcos;
			sseg.sqadaptcos *= sseg.sqadaptcos;

			svert= sseg.strand->vert + sortseg->segment;
			sseg.v[0]= (sortseg->segment > 0)? (svert-1): svert;
			sseg.v[1]= svert;
			sseg.v[2]= svert+1;
			sseg.v[3]= (sortseg->segment < sseg.strand->totvert-2)? svert+2: svert+1;
			sseg.shaded= 0;

			spart.segment= &sseg;

			render_strand_segment(re, winmat, &spart, &zspan, 1, &sseg);
		}
	}

	if (sortsegments)
		MEM_freeN(sortsegments);
	MEM_freeN(spart.totapixbuf);
	
	zbuf_free_span(&zspan);

	return totsegment;
}

/* *************** */

StrandSurface *cache_strand_surface(Render *re, ObjectRen *obr, DerivedMesh *dm, float mat[4][4], int timeoffset)
{
	StrandSurface *mesh;
	MFace *mface;
	MVert *mvert;
	float (*co)[3];
	int a, totvert, totface;

	totvert= dm->getNumVerts(dm);
	totface= dm->getNumTessFaces(dm);

	for (mesh = re->strandsurface.first; mesh; mesh = mesh->next) {
		if ((mesh->obr.ob    == obr->ob) &&
		    (mesh->obr.par   == obr->par) &&
		    (mesh->obr.index == obr->index) &&
		    (mesh->totvert   == totvert) &&
		    (mesh->totface   == totface))
		{
			break;
		}
	}

	if (!mesh) {
		mesh= MEM_callocN(sizeof(StrandSurface), "StrandSurface");
		mesh->obr= *obr;
		mesh->totvert= totvert;
		mesh->totface= totface;
		mesh->face= MEM_callocN(sizeof(int)*4*mesh->totface, "StrandSurfFaces");
		mesh->ao= MEM_callocN(sizeof(float)*3*mesh->totvert, "StrandSurfAO");
		mesh->env= MEM_callocN(sizeof(float)*3*mesh->totvert, "StrandSurfEnv");
		mesh->indirect= MEM_callocN(sizeof(float)*3*mesh->totvert, "StrandSurfIndirect");
		BLI_addtail(&re->strandsurface, mesh);
	}

	if (timeoffset == -1 && !mesh->prevco)
		mesh->prevco= co= MEM_callocN(sizeof(float)*3*mesh->totvert, "StrandSurfCo");
	else if (timeoffset == 0 && !mesh->co)
		mesh->co= co= MEM_callocN(sizeof(float)*3*mesh->totvert, "StrandSurfCo");
	else if (timeoffset == 1 && !mesh->nextco)
		mesh->nextco= co= MEM_callocN(sizeof(float)*3*mesh->totvert, "StrandSurfCo");
	else
		return mesh;

	mvert= dm->getVertArray(dm);
	for (a=0; a<mesh->totvert; a++, mvert++) {
		copy_v3_v3(co[a], mvert->co);
		mul_m4_v3(mat, co[a]);
	}

	mface= dm->getTessFaceArray(dm);
	for (a=0; a<mesh->totface; a++, mface++) {
		mesh->face[a][0]= mface->v1;
		mesh->face[a][1]= mface->v2;
		mesh->face[a][2]= mface->v3;
		mesh->face[a][3]= mface->v4;
	}

	return mesh;
}

void free_strand_surface(Render *re)
{
	StrandSurface *mesh;

	for (mesh=re->strandsurface.first; mesh; mesh=mesh->next) {
		if (mesh->co) MEM_freeN(mesh->co);
		if (mesh->prevco) MEM_freeN(mesh->prevco);
		if (mesh->nextco) MEM_freeN(mesh->nextco);
		if (mesh->ao) MEM_freeN(mesh->ao);
		if (mesh->env) MEM_freeN(mesh->env);
		if (mesh->indirect) MEM_freeN(mesh->indirect);
		if (mesh->face) MEM_freeN(mesh->face);
	}

	BLI_freelistN(&re->strandsurface);
}

void strand_minmax(StrandRen *strand, float min[3], float max[3], const float width)
{
	StrandVert *svert;
	const float width2 = width * 2.0f;
	float vec[3];
	int a;

	for (a=0, svert=strand->vert; a<strand->totvert; a++, svert++) {
		copy_v3_v3(vec, svert->co);
		minmax_v3v3_v3(min, max, vec);
		
		if (width!=0.0f) {
			add_v3_fl(vec, width);
			minmax_v3v3_v3(min, max, vec);
			add_v3_fl(vec, -width2);
			minmax_v3v3_v3(min, max, vec);
		}
	}
}

