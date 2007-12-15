/**
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
 * The Original Code is: none of this file.
 *
 * Contributors: Brecht Van Lommel.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <math.h>
#include <string.h>
#include <stdlib.h>

#include "MEM_guardedalloc.h"

#include "DNA_key_types.h"
#include "DNA_material_types.h"

#include "BLI_arithb.h"
#include "BLI_blenlib.h"
#include "BLI_ghash.h"
#include "BLI_memarena.h"

#include "BKE_key.h"
#include "BKE_utildefines.h"

#include "render_types.h"
#include "initrender.h"
#include "rendercore.h"
#include "renderdatabase.h"
#include "renderpipeline.h"
#include "pixelblending.h"
#include "shading.h"
#include "strand.h"
#include "zbuf.h"

/* to be removed */
void merge_transp_passes(RenderLayer *rl, ShadeResult *shr);
void add_transp_passes(RenderLayer *rl, int offset, ShadeResult *shr, float alpha);
void hoco_to_zco(ZSpan *zspan, float *zco, float *hoco);
void zspan_scanconvert_strand(ZSpan *zspan, void *handle, float *v1, float *v2, float *v3, void (*func)(void *, int, int, float, float, float) );
void zbufsinglewire(ZSpan *zspan, ObjectRen *obr, int zvlnr, float *ho1, float *ho2);
int addtosamp_shr(ShadeResult *samp_shr, ShadeSample *ssamp, int addpassflag);
void add_transp_speed(RenderLayer *rl, int offset, float *speed, float alpha, long *rdrect);
void reset_sky_speedvectors(RenderPart *pa, RenderLayer *rl, float *rectf);

/* *************** */

#define BUCKETPRIMS_SIZE 256

typedef struct BucketPrims {
	struct BucketPrims *next, *prev;
	void *prim[BUCKETPRIMS_SIZE];
	int totprim;
} BucketPrims;

typedef struct RenderBuckets {
	ListBase all;
	ListBase *inside;
	ListBase *overlap;
	int x, y;
	float insize[2];
	float zmulx, zmuly, zofsx, zofsy;
} RenderBuckets;

static void add_bucket_prim(ListBase *lb, void *prim)
{
	BucketPrims *bpr= lb->last;

	if(!bpr || bpr->totprim == BUCKETPRIMS_SIZE) {
		bpr= MEM_callocN(sizeof(BucketPrims), "BucketPrims");
		BLI_addtail(lb, bpr);
	}

	bpr->prim[bpr->totprim++]= prim;
}

RenderBuckets *init_buckets(Render *re)
{
	RenderBuckets *buckets;
	RenderPart *pa;
	float scalex, scaley, cropx, cropy;
	int x, y, tempparts= 0;

	buckets= MEM_callocN(sizeof(RenderBuckets), "RenderBuckets");

	if(!re->parts.first) {
		initparts(re);
		tempparts= 1;
	}

	pa= re->parts.first;
	if(!pa)
		return buckets;
	
	x= re->xparts+1;
	y= re->yparts+1;
	buckets->x= x;
	buckets->y= y;

	scalex= (2.0f - re->xparts*re->partx/(float)re->winx);
	scaley= (2.0f - re->yparts*re->party/(float)re->winy);

	cropx= pa->crop/(float)re->partx;
	cropy= pa->crop/(float)re->party;

	buckets->insize[0]= 1.0f - 2.0f*cropx;
	buckets->insize[1]= 1.0f - 2.0f*cropy;

	buckets->zmulx= re->xparts*scalex;
	buckets->zmuly= re->yparts*scaley;
	buckets->zofsx= scalex*(1.0f - cropx);
	buckets->zofsy= scaley*(1.0f - cropy);

	buckets->inside= MEM_callocN(sizeof(ListBase)*x*y, "BucketPrimsInside");
	buckets->overlap= MEM_callocN(sizeof(ListBase)*x*y, "BucketPrimsOverlap");

	if(tempparts)
		freeparts(re);

	return buckets;
}

void add_buckets_primitive(RenderBuckets *buckets, float *min, float *max, void *prim)
{
	float end[3];
	int x, y, a;

	x= (int)min[0];
	y= (int)min[1];

	if(x >= 0 && x < buckets->x && y >= 0 && y < buckets->y) {
		a= y*buckets->x + x;

		end[0]= x + buckets->insize[0];
		end[1]= y + buckets->insize[1];

		if(max[0] <= end[0] && max[1] <= end[1]) {
			add_bucket_prim(&buckets->inside[a], prim);
			return;
		}
		else {
			end[0]= x + 2;
			end[1]= y + 2;

			if(max[0] <= end[0] && max[1] <= end[1]) {
				add_bucket_prim(&buckets->overlap[a], prim);
				return;
			}
		}
	}

	add_bucket_prim(&buckets->all, prim);
}

void free_buckets(RenderBuckets *buckets)
{
	int a, size;

	BLI_freelistN(&buckets->all);

	size= buckets->x*buckets->y;
	for(a=0; a<size; a++) {
		BLI_freelistN(&buckets->inside[a]);
		BLI_freelistN(&buckets->overlap[a]);
	}

	if(buckets->inside)
		MEM_freeN(buckets->inside);
	if(buckets->overlap)
		MEM_freeN(buckets->overlap);
	
	MEM_freeN(buckets);
}

void project_hoco_to_bucket(RenderBuckets *buckets, float *hoco, float *bucketco)
{
	float div;

	div= 1.0f/hoco[3];
	bucketco[0]= buckets->zmulx*(0.5 + 0.5f*hoco[0]*div) + buckets->zofsx;
	bucketco[1]= buckets->zmuly*(0.5 + 0.5f*hoco[1]*div) + buckets->zofsy;
}

typedef struct RenderPrimitiveIterator {
	Render *re;
	RenderBuckets *buckets;
	ListBase *list[6];
	int listindex, totlist;
	BucketPrims *bpr;
	int bprindex;

	ObjectInstanceRen *obi;
	StrandRen *strand;
	int index, tot;
} RenderPrimitiveIterator;

RenderPrimitiveIterator *init_primitive_iterator(Render *re, RenderBuckets *buckets, RenderPart *pa)
{
	RenderPrimitiveIterator *iter;
	int nr, x, y, width;

	iter= MEM_callocN(sizeof(RenderPrimitiveIterator), "RenderPrimitiveIterator");
	iter->re= re;

	if(buckets) {
		iter->buckets= buckets;

		nr= BLI_findindex(&re->parts, pa);
		width= buckets->x - 1;
		x= (nr % width) + 1;
		y= (nr / width) + 1;

		iter->list[iter->totlist++]= &buckets->all;
		iter->list[iter->totlist++]= &buckets->inside[y*buckets->x + x];
		iter->list[iter->totlist++]= &buckets->overlap[y*buckets->x + x];
		iter->list[iter->totlist++]= &buckets->overlap[y*buckets->x + (x-1)];
		iter->list[iter->totlist++]= &buckets->overlap[(y-1)*buckets->x + (x-1)];
		iter->list[iter->totlist++]= &buckets->overlap[(y-1)*buckets->x + x];
	}
	else {
		iter->index= 0;
		iter->obi= re->instancetable.first;
		if(iter->obi)
			iter->tot= iter->obi->obr->totstrand;
	}

	return iter;
}

void *next_primitive_iterator(RenderPrimitiveIterator *iter)
{
	if(iter->buckets) {
		if(iter->bpr && iter->bprindex >= iter->bpr->totprim) {
			iter->bpr= iter->bpr->next;
			iter->bprindex= 0;
		}

		while(iter->bpr == NULL) {
			if(iter->listindex == iter->totlist)
				return NULL;

			iter->bpr= iter->list[iter->listindex++]->first;
			iter->bprindex= 0;
		}

		return iter->bpr->prim[iter->bprindex++];
	}
	else {
		if(!iter->obi)
			return NULL;

		if(iter->index >= iter->tot) {
			while((iter->obi=iter->obi->next) && !iter->obi->obr->totstrand)
				iter->obi= iter->obi->next;

			if(iter->obi)
				iter->tot= iter->obi->obr->totstrand;
			else
				return NULL;
		}

		if(iter->index < iter->tot) {
			if((iter->index & 255)==0)
				iter->strand= iter->obi->obr->strandnodes[iter->index>>8].strand;
			else
				iter->strand++;

			return iter->strand;
		}
		else
			return NULL;
	}
}

void free_primitive_iterator(RenderPrimitiveIterator *iter)
{
	MEM_freeN(iter);
}

/* *************** */

static float strand_eval_width(Material *ma, float strandco)
{
	float fac;

	strandco= 0.5f*(strandco + 1.0f);

	if(ma->strand_ease!=0.0f) {
		if(ma->strand_ease<0.0f)
			fac= pow(strandco, 1.0+ma->strand_ease);
		else
			fac= pow(strandco, 1.0/(1.0f-ma->strand_ease));
	}
	else fac= strandco;
	
	return ((1.0f-fac)*ma->strand_sta + (fac)*ma->strand_end);
}

void strand_eval_point(StrandSegment *sseg, StrandPoint *spoint)
{
	Material *ma;
	StrandBuffer *strandbuf;
	float p[4][3], data[4], cross[3], w, dx, dy, t;
	int type;

	strandbuf= sseg->buffer;
	ma= sseg->buffer->ma;
	t= spoint->t;
	type= (strandbuf->flag & R_STRAND_BSPLINE)? KEY_BSPLINE: KEY_CARDINAL;

	VECCOPY(p[0], sseg->v[0]->co);
	VECCOPY(p[1], sseg->v[1]->co);
	VECCOPY(p[2], sseg->v[2]->co);
	VECCOPY(p[3], sseg->v[3]->co);

	if(sseg->obi->flag & R_TRANSFORMED) {
		Mat4MulVecfl(sseg->obi->mat, p[0]);
		Mat4MulVecfl(sseg->obi->mat, p[1]);
		Mat4MulVecfl(sseg->obi->mat, p[2]);
		Mat4MulVecfl(sseg->obi->mat, p[3]);
	}

	if(t == 0.0f) {
		VECCOPY(spoint->co, p[1]);
		spoint->strandco= sseg->v[1]->strandco;

		spoint->dtstrandco= (sseg->v[2]->strandco - sseg->v[0]->strandco);
		if(sseg->v[0] != sseg->v[1])
			spoint->dtstrandco *= 0.5f;
	}
	else if(t == 1.0f) {
		VECCOPY(spoint->co, p[2]);
		spoint->strandco= sseg->v[2]->strandco;

		spoint->dtstrandco= (sseg->v[3]->strandco - sseg->v[1]->strandco);
		if(sseg->v[3] != sseg->v[2])
			spoint->dtstrandco *= 0.5f;
	}
	else {
		set_four_ipo(t, data, type);
		spoint->co[0]= data[0]*p[0][0] + data[1]*p[1][0] + data[2]*p[2][0] + data[3]*p[3][0];
		spoint->co[1]= data[0]*p[0][1] + data[1]*p[1][1] + data[2]*p[2][1] + data[3]*p[3][1];
		spoint->co[2]= data[0]*p[0][2] + data[1]*p[1][2] + data[2]*p[2][2] + data[3]*p[3][2];
		spoint->strandco= (1.0f-t)*sseg->v[1]->strandco + t*sseg->v[2]->strandco;
	}

	set_afgeleide_four_ipo(t, data, type);
	spoint->dtco[0]= data[0]*p[0][0] + data[1]*p[1][0] + data[2]*p[2][0] + data[3]*p[3][0];
	spoint->dtco[1]= data[0]*p[0][1] + data[1]*p[1][1] + data[2]*p[2][1] + data[3]*p[3][1];
	spoint->dtco[2]= data[0]*p[0][2] + data[1]*p[1][2] + data[2]*p[2][2] + data[3]*p[3][2];

	VECCOPY(spoint->tan, spoint->dtco);
	Normalize(spoint->tan);

	VECCOPY(spoint->nor, spoint->co);
	VECMUL(spoint->nor, -1.0f);
	Normalize(spoint->nor);

	spoint->width= strand_eval_width(ma, spoint->strandco);

	/* outer points */
	Crossf(cross, spoint->co, spoint->tan);

	if(strandbuf->flag & R_STRAND_B_UNITS)
		Normalize(cross);

	w= spoint->co[2]*strandbuf->winmat[2][3] + strandbuf->winmat[3][3];
	dx= strandbuf->winx*cross[0]*strandbuf->winmat[0][0]/w;
	dy= strandbuf->winy*cross[1]*strandbuf->winmat[1][1]/w;
	w= sqrt(dx*dx + dy*dy);

	if(w > 0.0f) {
		if(strandbuf->flag & R_STRAND_B_UNITS) {
			w= 1.0f/w;

			if(spoint->width < w)
				spoint->width= w;
			VecMulf(cross, spoint->width*0.5f);
		}
		else
			VecMulf(cross, spoint->width/w);
	}

	VecSubf(spoint->co1, spoint->co, cross);
	VecAddf(spoint->co2, spoint->co, cross);

	VECCOPY(spoint->dsco, cross);
}

/* *************** */

typedef struct StrandPart {
	Render *re;
	ZSpan *zspan;

	RenderLayer *rl;
	ShadeResult *result;
	float *pass;
	int *rectz, *outrectz;
	unsigned short *mask;
	int rectx, recty;
	int addpassflag, addzbuf, sample;

	StrandSegment *segment;
	GHash *hash;
	StrandPoint point1, point2;
	ShadeSample ssamp1, ssamp2, ssamp;
	float t[3];
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

	if(seg1->z < seg2->z)
		return -1;
	else if(seg1->z == seg2->z)
		return 0;
	else
		return 1;
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

	if(addpassflag & SCE_PASS_VECTOR) {
		interpolate_vec4(shr1->winspeed, shr2->winspeed, t, negt, shr->winspeed);
	}
	/* optim... */
	if(addpassflag & ~(SCE_PASS_VECTOR)) {
		if(addpassflag & SCE_PASS_RGBA)
			interpolate_vec4(shr1->col, shr2->col, t, negt, shr->col);
		if(addpassflag & SCE_PASS_NORMAL) {
			interpolate_vec3(shr1->nor, shr2->nor, t, negt, shr->nor);
			Normalize(shr->nor);
		}
		if(addpassflag & SCE_PASS_DIFFUSE)
			interpolate_vec3(shr1->diff, shr2->diff, t, negt, shr->diff);
		if(addpassflag & SCE_PASS_SPEC)
			interpolate_vec3(shr1->spec, shr2->spec, t, negt, shr->spec);
		if(addpassflag & SCE_PASS_SHADOW)
			interpolate_vec3(shr1->shad, shr2->shad, t, negt, shr->shad);
		if(addpassflag & SCE_PASS_AO)
			interpolate_vec3(shr1->ao, shr2->ao, t, negt, shr->ao);
		if(addpassflag & SCE_PASS_REFLECT)
			interpolate_vec3(shr1->refl, shr2->refl, t, negt, shr->refl);
		if(addpassflag & SCE_PASS_REFRACT)
			interpolate_vec3(shr1->refr, shr2->refr, t, negt, shr->refr);
		if(addpassflag & SCE_PASS_RADIO)
			interpolate_vec3(shr1->rad, shr2->rad, t, negt, shr->rad);
	}
}

static void add_strand_obindex(RenderLayer *rl, int offset, ObjectRen *obr)
{
	RenderPass *rpass;
	
	for(rpass= rl->passes.first; rpass; rpass= rpass->next) {
		if(rpass->passtype == SCE_PASS_INDEXOB) {
			float *fp= rpass->rect + offset;
			*fp= (float)obr->ob->index;
			break;
		}
	}
}

static void do_strand_point_project(float winmat[][4], ZSpan *zspan, float *co, float *hoco, float *zco)
{
	projectvert(co, winmat, hoco);
	hoco_to_zco(zspan, zco, hoco);
}

static void strand_project_point(float winmat[][4], float winx, float winy, StrandPoint *spoint)
{
	float div;

	projectvert(spoint->co, winmat, spoint->hoco);

	div= 1.0f/spoint->hoco[3];
	spoint->x= spoint->hoco[0]*div*winx*0.5f;
	spoint->y= spoint->hoco[1]*div*winy*0.5f;
}

#include "BLI_rand.h"
void strand_shade_point(Render *re, ShadeSample *ssamp, StrandSegment *sseg, StrandPoint *spoint);

static void strand_shade_get(StrandPart *spart, int lookup, ShadeSample *ssamp, StrandPoint *spoint, StrandVert *svert, StrandSegment *sseg)
{
	ShadeResult *hashshr;

	if(lookup) {
		hashshr= BLI_ghash_lookup(spart->hash, svert);

		if(!hashshr) {
			strand_shade_point(spart->re, ssamp, sseg, spoint);

			hashshr= MEM_callocN(sizeof(ShadeResult), "HashShadeResult");
			*hashshr= ssamp->shr[0];
			BLI_ghash_insert(spart->hash, svert, hashshr);
		}
		else {
			ssamp->shr[0]= *hashshr;
			BLI_ghash_remove(spart->hash, svert, NULL, (GHashValFreeFP)MEM_freeN);
		}
	}
	else
		strand_shade_point(spart->re, ssamp, sseg, spoint);
}

static void strand_shade_segment(StrandPart *spart)
{
	StrandSegment *sseg= spart->segment;
	int first, last;

	if(!sseg->shaded) {
		first= (sseg->v[1] == &sseg->strand->vert[0]);
		last= (sseg->v[2] == &sseg->strand->vert[sseg->strand->totvert-1]);

		strand_shade_get(spart, !first, &spart->ssamp1, &sseg->point1, sseg->v[1], sseg);
		strand_shade_get(spart, !last, &spart->ssamp2, &sseg->point2, sseg->v[2], sseg);
		sseg->shaded= 1;
	}

#if 0
	float c[3];
	
	c[0]= BLI_frand();
	c[1]= BLI_frand();
	c[2]= BLI_frand();

	spart->ssamp1.shr[0].combined[0] *= c[0];
	spart->ssamp1.shr[0].combined[1] *= c[1];
	spart->ssamp1.shr[0].combined[2] *= c[2];

	spart->ssamp2.shr[0].combined[0] *= c[0];
	spart->ssamp2.shr[0].combined[1] *= c[1];
	spart->ssamp2.shr[0].combined[2] *= c[2];
#endif
}

static void do_strand_blend(void *handle, int x, int y, float u, float v, float z)
{
	StrandPart *spart= (StrandPart*)handle;
	StrandBuffer *buffer= spart->segment->buffer;
	ShadeResult *shr;
	float /**pass,*/ t;
	int offset, zverg;

	/* check again solid z-buffer */
	offset = y*spart->rectx + x;
	zverg= (int)z;

	if(zverg < spart->rectz[offset]) {
		/* fill in output z-buffer if needed */
		if(spart->addzbuf)
			if(zverg < spart->outrectz[offset])
				spart->outrectz[offset]= zverg;

		/* check alpha limit */
		shr= spart->result + offset*(spart->re->osa? spart->re->osa: 1);
		if(shr[spart->sample].combined[3]>0.999f)
			return;

		/* shade points if not shaded yet */
		strand_shade_segment(spart);

		/* interpolate shading from two control points */
		t = u*spart->t[0] + v*spart->t[1] + (1.0f-u-v)*spart->t[2];
		interpolate_shade_result(spart->ssamp1.shr, spart->ssamp2.shr, t,
			spart->ssamp.shr, spart->addpassflag);

		/* add in shaderesult array for part */
		spart->ssamp.shi[0].mask= (1<<spart->sample);
		addtosamp_shr(shr, &spart->ssamp, spart->addpassflag);
		spart->mask[offset] |= (1<<spart->sample);

#if 0
		/* fill in pass for preview */
		if(spart->sample == 0) {
			pass= spart->pass + offset*4;
			QUATCOPY(pass, shr->combined);
		}
#endif

		if(spart->addpassflag & SCE_PASS_INDEXOB)
			add_strand_obindex(spart->rl, offset, buffer->obr);
	}
}

static int strand_test_clip(float winmat[][4], ZSpan *zspan, float *bounds, float *co, float *zcomp)
{
	float hoco[4];
	int clipflag= 0;

	projectvert(co, winmat, hoco);

	/* we compare z without perspective division for segment sorting */
	*zcomp= hoco[2];

	if(hoco[0] > bounds[1]*hoco[3]) clipflag |= 1;
	else if(hoco[0]< bounds[0]*hoco[3]) clipflag |= 2;
	else if(hoco[1] > bounds[3]*hoco[3]) clipflag |= 4;
	else if(hoco[1]< bounds[2]*hoco[3]) clipflag |= 8;

	return clipflag;
}

void strand_shade_point(Render *re, ShadeSample *ssamp, StrandSegment *sseg, StrandPoint *spoint)
{
	ShadeInput *shi= ssamp->shi;
	ShadeResult *shr= ssamp->shr;
	VlakRen vlr;

	memset(&vlr, 0, sizeof(vlr));
	vlr.flag= R_SMOOTH;
	vlr.lay= sseg->strand->buffer->lay;
	vlr.obr= sseg->strand->buffer->obr;
	if(sseg->buffer->ma->mode & MA_TANGENT_STR)
		vlr.flag |= R_TANGENT;

	shi->vlr= &vlr;
	shi->obi= sseg->obi;
	shi->obr= sseg->obi->obr;

	/* cache for shadow */
	shi->samplenr++;

	shade_input_set_strand(shi, sseg->strand, spoint);
	shade_input_set_strand_texco(shi, sseg->strand, sseg->v[1], spoint);
	
	/* init material vars */
	// note, keep this synced with render_types.h
	memcpy(&shi->r, &shi->mat->r, 23*sizeof(float));
	shi->har= shi->mat->har;
	
	/* shade */
	shade_samples_do_AO(ssamp);
	shade_input_do_shade(shi, shr);

	/* include lamphalos for strand, since halo layer was added already */
	if(re->flag & R_LAMPHALO)
		if(shi->layflag & SCE_LAY_HALO)
			renderspothalo(shi, shr->combined, shr->combined[3]);
}

static void do_scanconvert_strand(Render *re, StrandPart *spart, ZSpan *zspan, float t, float dt, float *co1, float *co2, float *co3, float *co4, int sample)
{
	float jco1[3], jco2[3], jco3[3], jco4[3], jx, jy;

	VECCOPY(jco1, co1);
	VECCOPY(jco2, co2);
	VECCOPY(jco3, co3);
	VECCOPY(jco4, co4);

	if(re->osa) {
		jx= -re->jit[sample][0];
		jy= -re->jit[sample][1];

		jco1[0] += jx; jco1[1] += jy;
		jco2[0] += jx; jco2[1] += jy;
		jco3[0] += jx; jco3[1] += jy;
		jco4[0] += jx; jco4[1] += jy;
	}
	else if(re->i.curblur) {
		jx= -re->jit[re->i.curblur-1][0];
		jy= -re->jit[re->i.curblur-1][1];

		jco1[0] += jx; jco1[1] += jy;
		jco2[0] += jx; jco2[1] += jy;
		jco3[0] += jx; jco3[1] += jy;
		jco4[0] += jx; jco4[1] += jy;
	}

	spart->sample= sample;

	spart->t[0]= t-dt;
	spart->t[1]= t-dt;
	spart->t[2]= t;
	zspan_scanconvert_strand(zspan, spart, jco1, jco2, jco3, do_strand_blend);
	spart->t[0]= t-dt;
	spart->t[1]= t;
	spart->t[2]= t;
	zspan_scanconvert_strand(zspan, spart, jco1, jco3, jco4, do_strand_blend);
}

static void strand_render(Render *re, float winmat[][4], StrandPart *spart, ZSpan *zspan, StrandPoint *p1, StrandPoint *p2)
{
	if(spart) {
		float t= p2->t;
		float dt= p2->t - p1->t;
		int a;

		if(re->osa) {
			for(a=0; a<re->osa; a++)
				do_scanconvert_strand(re, spart, zspan, t, dt, p1->zco2, p1->zco1, p2->zco1, p2->zco2, a);
		}
		else
			do_scanconvert_strand(re, spart, zspan, t, dt, p1->zco2, p1->zco1, p2->zco1, p2->zco2, 0);
	}
	else {
		float hoco1[4], hoco2[3];

		projectvert(p1->co, winmat, hoco1);
		projectvert(p2->co, winmat, hoco2);

		/* render both strand and single pixel wire to counter aliasing */
		zbufclip4(zspan, 0, 0, p1->hoco2, p1->hoco1, p2->hoco1, p2->hoco2, 0, 0, 0, 0);
		zbufsinglewire(zspan, 0, 0, hoco1, hoco2);
	}
}

static int strand_segment_recursive(Render *re, float winmat[][4], StrandPart *spart, ZSpan *zspan, StrandSegment *sseg, StrandPoint *p1, StrandPoint *p2, int depth)
{
	StrandPoint p;
	StrandBuffer *buffer= sseg->buffer;
	float dot, d1[2], d2[2], len1, len2;

	if(depth == buffer->maxdepth)
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

	if(len1 == 0.0f || len2 == 0.0f)
		return 0;
	
	dot= d1[0]*d2[0] + d1[1]*d2[1];
	if(dot*dot > sseg->sqadaptcos*len1*len2)
		return 0;

	if(spart) {
		do_strand_point_project(winmat, zspan, p.co1, p.hoco1, p.zco1);
		do_strand_point_project(winmat, zspan, p.co2, p.hoco2, p.zco2);
	}
	else {
		projectvert(p.co1, winmat, p.hoco1);
		projectvert(p.co2, winmat, p.hoco2);
	}

	if(!strand_segment_recursive(re, winmat, spart, zspan, sseg, p1, &p, depth+1))
		strand_render(re, winmat, spart, zspan, p1, &p);
	if(!strand_segment_recursive(re, winmat, spart, zspan, sseg, &p, p2, depth+1))
		strand_render(re, winmat, spart, zspan, &p, p2);
	
	return 1;
}

void render_strand_segment(Render *re, float winmat[][4], StrandPart *spart, ZSpan *zspan, StrandSegment *sseg)
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

	if(spart) {
		do_strand_point_project(winmat, zspan, p1->co1, p1->hoco1, p1->zco1);
		do_strand_point_project(winmat, zspan, p1->co2, p1->hoco2, p1->zco2);
		do_strand_point_project(winmat, zspan, p2->co1, p2->hoco1, p2->zco1);
		do_strand_point_project(winmat, zspan, p2->co2, p2->hoco2, p2->zco2);
	}
	else {
		projectvert(p1->co1, winmat, p1->hoco1);
		projectvert(p1->co2, winmat, p1->hoco2);
		projectvert(p2->co1, winmat, p2->hoco1);
		projectvert(p2->co2, winmat, p2->hoco2);
	}

	if(!strand_segment_recursive(re, winmat, spart, zspan, sseg, p1, p2, 0))
		strand_render(re, winmat, spart, zspan, p1, p2);
}

static void zbuffer_strands_filter(Render *re, RenderPart *pa, RenderLayer *rl, StrandPart *spart, float *pass)
{
	RenderResult *rr= pa->result;
	ShadeResult *shr, *shrrect= spart->result;
	float *passrect= pass;
	long *rdrect;
	int osa, x, y, a, crop= 0, offs=0, od;

	osa= (re->osa? re->osa: 1);

	/* filtered render, for now we assume only 1 filter size */
	if(pa->crop) {
		crop= 1;
		offs= pa->rectx + 1;
		passrect+= 4*offs;
		shrrect+= offs*osa;
	}

	rdrect= pa->rectdaps;

	/* zero alpha pixels get speed vector max again */
	if(spart->addpassflag & SCE_PASS_VECTOR)
		if(rl->layflag & SCE_LAY_SOLID)
			reset_sky_speedvectors(pa, rl, rl->scolrect);

	/* init scanline updates */
	rr->renrect.ymin= 0;
	rr->renrect.ymax= -pa->crop;
	rr->renlay= rl;
	
	/* filter the shade results */
	for(y=pa->disprect.ymin+crop; y<pa->disprect.ymax-crop; y++, rr->renrect.ymax++) {
		pass= passrect;
		shr= shrrect;
		od= offs;
		
		for(x=pa->disprect.xmin+crop; x<pa->disprect.xmax-crop; x++, shr+=osa, pass+=4, od++) {
			if(spart->mask[od] == 0) {
				if(spart->addpassflag & SCE_PASS_VECTOR) 
					add_transp_speed(rl, od, NULL, 0.0f, rdrect);
			}
			else {
				if(re->osa == 0) {
					addAlphaUnderFloat(pass, shr->combined);
				}
				else {
					for(a=0; a<re->osa; a++)
						add_filt_fmask(1<<a, shr[a].combined, pass, rr->rectx);
				}

				if(spart->addpassflag) {
					/* merge all in one, and then add */
					merge_transp_passes(rl, shr);
					add_transp_passes(rl, od, shr, pass[3]);

					if(spart->addpassflag & SCE_PASS_VECTOR)
						add_transp_speed(rl, od, shr->winspeed, pass[3], rdrect);
				}
			}
		}

		shrrect+= pa->rectx*osa;
		passrect+= 4*pa->rectx;
		offs+= pa->rectx;
	}

	/* disable scanline updating */
	rr->renlay= NULL;
}

/* render call to fill in strands */
unsigned short *zbuffer_strands_shade(Render *re, RenderPart *pa, RenderLayer *rl, float *pass)
{
	//struct RenderPrimitiveIterator *iter;
	ObjectRen *obr;
	ObjectInstanceRen *obi;
	ZSpan zspan;
	StrandRen *strand=0;
	StrandVert *svert;
	StrandPart spart;
	StrandSegment sseg;
	StrandSortSegment *sortsegments = NULL, *sortseg, *firstseg;
	MemArena *memarena;
	float z[4], bounds[4], winmat[4][4];
	int a, b, i, resultsize, totsegment, clip[4];

	if(re->test_break())
		return NULL;
	if(re->totstrand == 0)
		return NULL;

	/* setup StrandPart */
	memset(&spart, 0, sizeof(spart));

	spart.re= re;
	spart.rl= rl;
	spart.pass= pass;
	spart.rectx= pa->rectx;
	spart.recty= pa->recty;
	spart.rectz= pa->rectz;
	spart.addpassflag= rl->passflag & ~(SCE_PASS_Z|SCE_PASS_COMBINED);
	spart.addzbuf= rl->passflag & SCE_PASS_Z;

	if(re->osa) resultsize= pa->rectx*pa->recty*re->osa;
	else resultsize= pa->rectx*pa->recty;
	spart.result= MEM_callocN(sizeof(ShadeResult)*resultsize, "StrandPartResult");
	spart.mask= MEM_callocN(pa->rectx*pa->recty*sizeof(short), "StrandPartMask");

	if(spart.addpassflag & SCE_PASS_VECTOR) {
		/* initialize speed vectors */
		for(a=0; a<resultsize; a++) {
			spart.result[a].winspeed[0]= PASS_VECTOR_MAX;
			spart.result[a].winspeed[1]= PASS_VECTOR_MAX;
			spart.result[a].winspeed[2]= PASS_VECTOR_MAX;
			spart.result[a].winspeed[3]= PASS_VECTOR_MAX;
		}
	}

	if(spart.addzbuf) {
		/* duplicate rectz so we can read from the old buffer, while
		 * writing new z values */
		spart.rectz= MEM_dupallocN(pa->rectz);
		spart.outrectz= pa->rectz;
	}

	shade_sample_initialize(&spart.ssamp1, pa, rl);
	shade_sample_initialize(&spart.ssamp2, pa, rl);
	shade_sample_initialize(&spart.ssamp, pa, rl);
	spart.ssamp1.shi[0].sample= 0;
	spart.ssamp2.shi[0].sample= 1;
	spart.ssamp1.tot= 1;
	spart.ssamp2.tot= 1;
	spart.ssamp.tot= 1;

	zbuf_alloc_span(&zspan, pa->rectx, pa->recty);

	/* needed for transform from hoco to zbuffer co */
	zspan.zmulx= ((float)re->winx)/2.0;
	zspan.zmuly= ((float)re->winy)/2.0;
	
	zspan.zofsx= -pa->disprect.xmin;
	zspan.zofsy= -pa->disprect.ymin;

	/* to center the sample position */
	zspan.zofsx -= 0.5f;
	zspan.zofsy -= 0.5f;

	/* clipping setup */
	bounds[0]= (2*pa->disprect.xmin - re->winx-1)/(float)re->winx;
	bounds[1]= (2*pa->disprect.xmax - re->winx+1)/(float)re->winx;
	bounds[2]= (2*pa->disprect.ymin - re->winy-1)/(float)re->winy;
	bounds[3]= (2*pa->disprect.ymax - re->winy+1)/(float)re->winy;

	/* sort segments */
	//iter= init_primitive_iterator(re, re->strandbuckets, pa);

	memarena= BLI_memarena_new(BLI_MEMARENA_STD_BUFSIZE);
	firstseg= NULL;
	sortseg= sortsegments;
	totsegment= 0;

	//while((strand = next_primitive_iterator(iter))) {
	for(obi=re->instancetable.first, i=0; obi; obi=obi->next, i++) {
		obr= obi->obr;

		if(obi->flag & R_TRANSFORMED)
			zbuf_make_winmat(re, obi->mat, winmat);
		else
			zbuf_make_winmat(re, NULL, winmat);

		for(a=0; a<obr->totstrand; a++) {
			if((a & 255)==0) strand= obr->strandnodes[a>>8].strand;
			else strand++;

			if(re->test_break())
				break;

#if 0
			if(strand->clip)
				continue;
#endif

			svert= strand->vert;

			/* keep clipping and z depth for 4 control points */
			clip[1]= strand_test_clip(winmat, &zspan, bounds, svert->co, &z[1]);
			clip[2]= strand_test_clip(winmat, &zspan, bounds, (svert+1)->co, &z[2]);
			clip[0]= clip[1]; z[0]= z[1];

			for(b=0; b<strand->totvert-1; b++, svert++) {
				/* compute 4th point clipping and z depth */
				if(b < strand->totvert-2) {
					clip[3]= strand_test_clip(winmat, &zspan, bounds, (svert+2)->co, &z[3]);
				}
				else {
					clip[3]= clip[2]; z[3]= z[2];
				}

				/* check clipping and add to sortsegments buffer */
				if(!(clip[0] & clip[1] & clip[2] & clip[3])) {
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

#if 0
	free_primitive_iterator(iter);
#endif

	if(!re->test_break()) {
		/* convert list to array and sort */
		sortsegments= MEM_mallocN(sizeof(StrandSortSegment)*totsegment, "StrandSortSegment");
		for(a=0, sortseg=firstseg; a<totsegment; a++, sortseg=sortseg->next)
			sortsegments[a]= *sortseg;
		qsort(sortsegments, totsegment, sizeof(StrandSortSegment), compare_strand_segment);
	}

	BLI_memarena_free(memarena);

	spart.hash= BLI_ghash_new(BLI_ghashutil_ptrhash, BLI_ghashutil_ptrcmp);

	if(!re->test_break()) {
		/* render segments in sorted order */
		sortseg= sortsegments;
		for(a=0; a<totsegment; a++, sortseg++) {
			if(re->test_break())
				break;

			obi= &re->objectinstance[sortseg->obi];
			obr= obi->obr;
			zbuf_make_winmat(re, NULL, winmat);

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

			render_strand_segment(re, winmat, &spart, &zspan, &sseg);
		}
	}

	// TODO printf(">>> %d\n", BLI_ghash_size(spart.hash));
	BLI_ghash_free(spart.hash, NULL, (GHashValFreeFP)MEM_freeN);

	zbuffer_strands_filter(re, pa, rl, &spart, pass);

	/* free */
	MEM_freeN(spart.result);

	if(spart.addzbuf)
		MEM_freeN(spart.rectz);

	if(sortsegments)
		MEM_freeN(sortsegments);
	
	zbuf_free_span(&zspan);

	if(!(re->osa && (rl->layflag & SCE_LAY_SOLID))) {
		MEM_freeN(spart.mask);
		spart.mask= NULL;
	}

	return spart.mask;
}

void project_strands(Render *re, void (*projectfunc)(float *, float mat[][4], float *),  int do_pano, int do_buckets)
{
#if 0
	ObjectRen *obr;
	StrandRen *strand = NULL;
	StrandVert *svert;
	float hoco[4], min[2], max[2], bucketco[2], vec[3];
	int a, b;
	/* float bmin[3], bmax[3], bpad[3], padding[2]; */
	
	if(re->strandbuckets) {
		free_buckets(re->strandbuckets);
		re->strandbuckets= NULL;
	}

	if(re->totstrand == 0)
		return;
	
	if(do_buckets)
		re->strandbuckets= init_buckets(re);

	/* calculate view coordinates (and zbuffer value) */
	for(obr=re->objecttable.first; obr; obr=obr->next) {
		for(a=0; a<obr->totstrand; a++) {
			if((a & 255)==0) strand= obr->strandnodes[a>>8].strand;
			else strand++;

			strand->clip= ~0;

#if 0
			if(!(strand->buffer->flag & R_STRAND_BSPLINE)) {
				INIT_MINMAX(bmin, bmax);
				svert= strand->vert;
				for(b=0; b<strand->totvert; b++, svert++)
					DO_MINMAX(svert->co, bmin, bmax)

				bpad[0]= (bmax[0]-bmin[0])*0.2f;
				bpad[1]= (bmax[1]-bmin[1])*0.2f;
				bpad[2]= (bmax[2]-bmin[2])*0.2f;
			}
			else
				bpad[0]= bpad[1]= bpad[2]= 0.0f;

			ma= strand->buffer->ma;
			width= MAX2(ma->strand_sta, ma->strand_end);
			if(strand->buffer->flag & R_STRAND_B_UNITS) {
				bpad[0] += 0.5f*width;
				bpad[1] += 0.5f*width;
				bpad[2] += 0.5f*width;
			}
#endif

			INIT_MINMAX2(min, max);
			svert= strand->vert;
			for(b=0; b<strand->totvert; b++, svert++) {
				//VECADD(vec, svert->co, bpad);

				/* same as VertRen */
				if(do_pano) {
					vec[0]= re->panoco*svert->co[0] + re->panosi*svert->co[2];
					vec[1]= svert->co[1];
					vec[2]= -re->panosi*svert->co[0] + re->panoco*svert->co[2];
				}
				else
					VECCOPY(vec, svert->co)

				/* Go from wcs to hcs ... */
				projectfunc(vec, re->winmat, hoco);
				/* ... and clip in that system. */
				strand->clip &= testclip(hoco);

#if 0
				if(do_buckets) {
					project_hoco_to_bucket(re->strandbuckets, hoco, bucketco);
					DO_MINMAX2(bucketco, min, max);
				}
#endif
			}

#if 0
			if(do_buckets) {
				if(strand->buffer->flag & R_STRAND_BSPLINE) {
					min[0] -= width;
					min[1] -= width;
					max[0] += width;
					max[1] += width;
				}
				else {
					/* catmull-rom stays within 1.2f bounds in object space,
					 * is this still true after projection? */
					min[0] -= width + (max[0]-min[0])*0.2f;
					min[1] -= width + (max[1]-min[1])*0.2f;
					max[0] += width + (max[0]-min[0])*0.2f;
					max[1] += width + (max[1]-min[1])*0.2f;
				}

				add_buckets_primitive(re->strandbuckets, min, max, strand);
			}
#endif
		}
	}
#endif
}

