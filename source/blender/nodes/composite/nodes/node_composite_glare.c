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
 * The Original Code is Copyright (C) 2006 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Alfredo de Greef  (eeshlo)
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/nodes/composite/nodes/node_composite_glare.c
 *  \ingroup cmpnodes
 */


#include "node_composite_util.h"

static bNodeSocketTemplate cmp_node_glare_in[]= {
	{	SOCK_RGBA, 1, N_("Image"),			1.0f, 1.0f, 1.0f, 1.0f},
	{	-1, 0, ""	}
};
static bNodeSocketTemplate cmp_node_glare_out[]= {
	{	SOCK_RGBA, 0, N_("Image")},
	{	-1, 0, ""	}
};


// mix two images, src buffer does not have to be same size,
static void mixImages(CompBuf *dst, CompBuf *src, float mix)
{
	int x, y;
	fRGB c1, c2, *dcolp, *scolp;
	const float mf = 2.f - 2.f*fabsf(mix - 0.5f);
	if ((dst->x == src->x) && (dst->y == src->y)) {
		for (y=0; y<dst->y; y++) {
			dcolp = (fRGB*)&dst->rect[y*dst->x*dst->type];
			scolp = (fRGB*)&src->rect[y*dst->x*dst->type];
			for (x=0; x<dst->x; x++) {
				fRGB_copy(c1, dcolp[x]);
				fRGB_copy(c2, scolp[x]);
				c1[0] += mix*(c2[0] - c1[0]);
				c1[1] += mix*(c2[1] - c1[1]);
				c1[2] += mix*(c2[2] - c1[2]);
				if (c1[0] < 0.f) c1[0] = 0.f;
				if (c1[1] < 0.f) c1[1] = 0.f;
				if (c1[2] < 0.f) c1[2] = 0.f;
				fRGB_mult(c1, mf);
				fRGB_copy(dcolp[x], c1);
			}
		}
	}
	else {
		float xr = src->x / (float)dst->x;
		float yr = src->y / (float)dst->y;
		for (y=0; y<dst->y; y++) {
			dcolp = (fRGB*)&dst->rect[y*dst->x*dst->type];
			for (x=0; x<dst->x; x++) {
				fRGB_copy(c1, dcolp[x]);
				qd_getPixelLerp(src, (x + 0.5f)*xr - 0.5f, (y + 0.5f)*yr - 0.5f, c2);
				c1[0] += mix*(c2[0] - c1[0]);
				c1[1] += mix*(c2[1] - c1[1]);
				c1[2] += mix*(c2[2] - c1[2]);
				if (c1[0] < 0.f) c1[0] = 0.f;
				if (c1[1] < 0.f) c1[1] = 0.f;
				if (c1[2] < 0.f) c1[2] = 0.f;
				fRGB_mult(c1, mf);
				fRGB_copy(dcolp[x], c1);
			}
		}
	}
}


// adds src to dst image, must be of same size
static void addImage(CompBuf* dst, CompBuf* src, float scale)
{
	if ((dst->x == src->x) && (dst->y == src->y)) {
		int p = dst->x*dst->y*dst->type;
		float *dcol = dst->rect, *scol = src->rect;
		while (p--) *dcol++ += *scol++ * scale;
	}
}


// returns possibly downscaled copy of all pixels above threshold
static CompBuf* BTP(CompBuf* src, float threshold, int scaledown)
{
	int x, y;
	CompBuf* bsrc = qd_downScaledCopy(src, scaledown);
	float* cr = bsrc->rect;
	for (y=0; y<bsrc->y; ++y)
		for (x=0; x<bsrc->x; ++x, cr+=4) {
			if (rgb_to_luma_y(cr) >= threshold) {
				cr[0] -= threshold, cr[1] -= threshold, cr[2] -= threshold;
				cr[0] = MAX2(cr[0], 0.f);
				cr[1] = MAX2(cr[1], 0.f);
				cr[2] = MAX2(cr[2], 0.f);
			}
			else cr[0] = cr[1] = cr[2] = 0.f;
		}
	return bsrc;
}

//--------------------------------------------------------------------------------------------
// simple 4-point star filter

static void star4(NodeGlare* ndg, CompBuf* dst, CompBuf* src)
{
	int x, y, i, xm, xp, ym, yp;
	float c[4] = {0, 0, 0, 0}, tc[4] = {0, 0, 0, 0};
	CompBuf *tbuf1, *tbuf2, *tsrc;
	const float f1 = 1.f - ndg->fade, f2 = (1.f - f1)*0.5f;
	//const float t3 = ndg->threshold*3.f;
	const float sc = (float)(1 << ndg->quality);
	const float isc = 1.f/sc;

	tsrc = BTP(src, ndg->threshold, (int)sc);

	tbuf1 = dupalloc_compbuf(tsrc);
	tbuf2 = dupalloc_compbuf(tsrc);

	for (i=0; i<ndg->iter; i++) {
		// (x || x-1, y-1) to (x || x+1, y+1)
		// F
		for (y=0; y<tbuf1->y; y++) {
			ym = y - i;
			yp = y + i;
			for (x=0; x<tbuf1->x; x++) {
				xm = x - i;
				xp = x + i;
				qd_getPixel(tbuf1, x, y, c);
				fRGB_mult(c, f1);
				qd_getPixel(tbuf1, (ndg->angle ? xm : x), ym, tc);
				fRGB_madd(c, tc, f2);
				qd_getPixel(tbuf1, (ndg->angle ? xp : x), yp, tc);
				fRGB_madd(c, tc, f2);
				qd_setPixel(tbuf1, x, y, c);
			}
		}
		// B
		for (y=tbuf1->y-1; y>=0; y--) {
			ym = y - i;
			yp = y + i;
			for (x=tbuf1->x-1; x>=0; x--) {
				xm = x - i;
				xp = x + i;
				qd_getPixel(tbuf1, x, y, c);
				fRGB_mult(c, f1);
				qd_getPixel(tbuf1, (ndg->angle ? xm : x), ym, tc);
				fRGB_madd(c, tc, f2);
				qd_getPixel(tbuf1, (ndg->angle ? xp : x), yp, tc);
				fRGB_madd(c, tc, f2);
				qd_setPixel(tbuf1, x, y, c);
			}
		}
		// (x-1, y || y+1) to (x+1, y || y-1)
		// F
		for (y=0; y<tbuf2->y; y++) {
			ym = y - i;
			yp = y + i;
			for (x=0; x<tbuf2->x; x++) {
				xm = x - i;
				xp = x + i;
				qd_getPixel(tbuf2, x, y, c);
				fRGB_mult(c, f1);
				qd_getPixel(tbuf2, xm, (ndg->angle ? yp : y), tc);
				fRGB_madd(c, tc, f2);
				qd_getPixel(tbuf2, xp, (ndg->angle ? ym : y), tc);
				fRGB_madd(c, tc, f2);
				qd_setPixel(tbuf2, x, y, c);
			}
		}
		// B
		for (y=tbuf2->y-1; y>=0; y--) {
			ym = y - i;
			yp = y + i;
			for (x=tbuf2->x-1; x>=0; x--) {
				xm = x - i;
				xp = x + i;
				qd_getPixel(tbuf2, x, y, c);
				fRGB_mult(c, f1);
				qd_getPixel(tbuf2, xm, (ndg->angle ? yp : y), tc);
				fRGB_madd(c, tc, f2);
				qd_getPixel(tbuf2, xp, (ndg->angle ? ym : y), tc);
				fRGB_madd(c, tc, f2);
				qd_setPixel(tbuf2, x, y, c);
			}
		}
	}

	for (y=0; y<tbuf1->y; ++y)
		for (x=0; x<tbuf1->x; ++x) {
			unsigned int p = (x + y*tbuf1->x)*tbuf1->type;
			tbuf1->rect[p] += tbuf2->rect[p];
			tbuf1->rect[p+1] += tbuf2->rect[p+1];
			tbuf1->rect[p+2] += tbuf2->rect[p+2];
		}

	for (y=0; y<dst->y; ++y) {
		const float m = 0.5f + 0.5f*ndg->mix;
		for (x=0; x<dst->x; ++x) {
			unsigned int p = (x + y*dst->x)*dst->type;
			qd_getPixelLerp(tbuf1, x*isc, y*isc, tc);
			dst->rect[p] = src->rect[p] + m*(tc[0] - src->rect[p]);
			dst->rect[p+1] = src->rect[p+1] + m*(tc[1] - src->rect[p+1]);
			dst->rect[p+2] = src->rect[p+2] + m*(tc[2] - src->rect[p+2]);
		}
	}

	free_compbuf(tbuf1);
	free_compbuf(tbuf2);
	free_compbuf(tsrc);
}

//--------------------------------------------------------------------------------------------
// streak filter

static void streaks(NodeGlare* ndg, CompBuf* dst, CompBuf* src)
{
	CompBuf *bsrc, *tsrc, *tdst, *sbuf;
	int x, y, n;
	unsigned int nump=0;
	fRGB c1, c2, c3, c4;
	float a, ang = DEG2RADF(360.0f)/(float)ndg->angle;

	bsrc = BTP(src, ndg->threshold, 1 << ndg->quality);
	tsrc = dupalloc_compbuf(bsrc); // sample from buffer
	tdst = alloc_compbuf(tsrc->x, tsrc->y, tsrc->type, 1); // sample to buffer
	sbuf = alloc_compbuf(tsrc->x, tsrc->y, tsrc->type, 1);  // streak sum buffer

	
	for (a=0.f; a<DEG2RADF(360.0f); a+=ang) {
		const float an = a + ndg->angle_ofs;
		const float vx = cos((double)an), vy = sin((double)an);
		for (n=0; n<ndg->iter; ++n) {
			const float p4 = pow(4.0, (double)n);
			const float vxp = vx*p4, vyp = vy*p4;
			const float wt = pow((double)ndg->fade, (double)p4);
			const float cmo = 1.f - (float)pow((double)ndg->colmod, (double)n+1);	// colormodulation amount relative to current pass
			float* tdstcol = tdst->rect;
			for (y=0; y<tsrc->y; ++y) {
				for (x=0; x<tsrc->x; ++x, tdstcol+=4) {
					// first pass no offset, always same for every pass, exact copy,
					// otherwise results in uneven brightness, only need once
					if (n==0) qd_getPixel(tsrc, x, y, c1); else c1[0]=c1[1]=c1[2]=0;
					qd_getPixelLerp(tsrc, x + vxp,     y + vyp,     c2);
					qd_getPixelLerp(tsrc, x + vxp*2.f, y + vyp*2.f, c3);
					qd_getPixelLerp(tsrc, x + vxp*3.f, y + vyp*3.f, c4);
					// modulate color to look vaguely similar to a color spectrum
					fRGB_rgbmult(c2, 1.f, cmo, cmo);
					fRGB_rgbmult(c3, cmo, cmo, 1.f);
					fRGB_rgbmult(c4, cmo, 1.f, cmo);
					tdstcol[0] = 0.5f*(tdstcol[0] + c1[0] + wt*(c2[0] + wt*(c3[0] + wt*c4[0])));
					tdstcol[1] = 0.5f*(tdstcol[1] + c1[1] + wt*(c2[1] + wt*(c3[1] + wt*c4[1])));
					tdstcol[2] = 0.5f*(tdstcol[2] + c1[2] + wt*(c2[2] + wt*(c3[2] + wt*c4[2])));
				}
			}
			memcpy(tsrc->rect, tdst->rect, sizeof(float)*tdst->x*tdst->y*tdst->type);
		}

		addImage(sbuf, tsrc, 1.f/(float)(6 - ndg->iter));
		memset(tdst->rect, 0, tdst->x*tdst->y*tdst->type*sizeof(float));
		memcpy(tsrc->rect, bsrc->rect, bsrc->x*bsrc->y*bsrc->type*sizeof(float));
		nump++;
	}

	mixImages(dst, sbuf, 0.5f + 0.5f*ndg->mix);

	free_compbuf(tsrc);
	free_compbuf(tdst);
	free_compbuf(sbuf);
	free_compbuf(bsrc);
}


//--------------------------------------------------------------------------------------------
// Ghosts (lensflare)

static float smoothMask(float x, float y)
{
	float t;
	x = 2.f*x - 1.f, y = 2.f*y - 1.f;
	if ((t = 1.f - sqrtf(x*x + y*y)) <= 0.f) return 0.f;
	return t;
}

static void ghosts(NodeGlare* ndg, CompBuf* dst, CompBuf* src)
{
	// colormodulation and scale factors (cm & scalef) for 16 passes max: 64
	int x, y, n, p, np;
	fRGB c, tc, cm[64];
	float sc, isc, u, v, sm, s, t, ofs, scalef[64];
	CompBuf *tbuf1, *tbuf2, *gbuf;
	const float cmo = 1.f - ndg->colmod;
	const int qt = 1 << ndg->quality;
	const float s1 = 4.f/(float)qt, s2 = 2.f*s1;

	gbuf = BTP(src, ndg->threshold, qt);
	tbuf1 = dupalloc_compbuf(gbuf);
	IIR_gauss(tbuf1, s1, 0, 3);
	IIR_gauss(tbuf1, s1, 1, 3);
	IIR_gauss(tbuf1, s1, 2, 3);
	tbuf2 = dupalloc_compbuf(tbuf1);
	IIR_gauss(tbuf2, s2, 0, 3);
	IIR_gauss(tbuf2, s2, 1, 3);
	IIR_gauss(tbuf2, s2, 2, 3);

	if (ndg->iter & 1) ofs = 0.5f; else ofs = 0.f;
	for (x=0; x<(ndg->iter*4); x++) {
		y = x & 3;
		cm[x][0] = cm[x][1] = cm[x][2] = 1;
		if (y==1) fRGB_rgbmult(cm[x], 1.f, cmo, cmo);
		if (y==2) fRGB_rgbmult(cm[x], cmo, cmo, 1.f);
		if (y==3) fRGB_rgbmult(cm[x], cmo, 1.f, cmo);
		scalef[x] = 2.1f*(1.f-(x+ofs)/(float)(ndg->iter*4));
		if (x & 1) scalef[x] = -0.99f/scalef[x];
	}

	sc = 2.13;
	isc = -0.97;
	for (y=0; y<gbuf->y; y++) {
		v = (float)(y+0.5f) / (float)gbuf->y;
		for (x=0; x<gbuf->x; x++) {
			u = (float)(x+0.5f) / (float)gbuf->x;
			s = (u-0.5f)*sc + 0.5f, t = (v-0.5f)*sc + 0.5f;
			qd_getPixelLerp(tbuf1, s*gbuf->x, t*gbuf->y, c);
			sm = smoothMask(s, t);
			fRGB_mult(c, sm);
			s = (u-0.5f)*isc + 0.5f, t = (v-0.5f)*isc + 0.5f;
			qd_getPixelLerp(tbuf2, s*gbuf->x - 0.5f, t*gbuf->y - 0.5f, tc);
			sm = smoothMask(s, t);
			fRGB_madd(c, tc, sm);
			qd_setPixel(gbuf, x, y, c);
		}
	}

	memset(tbuf1->rect, 0, tbuf1->x*tbuf1->y*tbuf1->type*sizeof(float));
	for (n=1; n<ndg->iter; n++) {
		for (y=0; y<gbuf->y; y++) {
			v = (float)(y+0.5f) / (float)gbuf->y;
			for (x=0; x<gbuf->x; x++) {
				u = (float)(x+0.5f) / (float)gbuf->x;
				tc[0] = tc[1] = tc[2] = 0.f;
				for (p=0;p<4;p++) {
					np = (n<<2) + p;
					s = (u-0.5f)*scalef[np] + 0.5f;
					t = (v-0.5f)*scalef[np] + 0.5f;
					qd_getPixelLerp(gbuf, s*gbuf->x - 0.5f, t*gbuf->y - 0.5f, c);
					fRGB_colormult(c, cm[np]);
					sm = smoothMask(s, t)*0.25f;
					fRGB_madd(tc, c, sm);
				}
				p = (x + y*tbuf1->x)*tbuf1->type;
				tbuf1->rect[p] += tc[0];
				tbuf1->rect[p+1] += tc[1];
				tbuf1->rect[p+2] += tc[2];
			}
		}
		memcpy(gbuf->rect, tbuf1->rect, tbuf1->x*tbuf1->y*tbuf1->type*sizeof(float));
	}

	free_compbuf(tbuf1);
	free_compbuf(tbuf2);

	mixImages(dst, gbuf, 0.5f + 0.5f*ndg->mix);
	free_compbuf(gbuf);
}

//--------------------------------------------------------------------------------------------
// Fog glow (convolution with kernel of exponential falloff)

static void fglow(NodeGlare* ndg, CompBuf* dst, CompBuf* src)
{
	int x, y;
	float scale, u, v, r, w, d;
	fRGB fcol;
	CompBuf *tsrc, *ckrn;
	unsigned int sz = 1 << ndg->size;
	const float cs_r = 1.f, cs_g = 1.f, cs_b = 1.f;

	// temp. src image
	tsrc = BTP(src, ndg->threshold, 1 << ndg->quality);
	// make the convolution kernel
	ckrn = alloc_compbuf(sz, sz, CB_RGBA, 1);

	scale = 0.25f*sqrtf(sz*sz);

	for (y=0; y<sz; ++y) {
		v = 2.f*(y / (float)sz) - 1.f;
		for (x=0; x<sz; ++x) {
			u = 2.f*(x / (float)sz) - 1.f;
			r = (u*u + v*v)*scale;
			d = -sqrtf(sqrtf(sqrtf(r)))*9.f;
			fcol[0] = expf(d*cs_r), fcol[1] = expf(d*cs_g), fcol[2] = expf(d*cs_b);
			// linear window good enough here, visual result counts, not scientific analysis
			//w = (1.f-fabs(u))*(1.f-fabs(v));
			// actually, Hanning window is ok, cos^2 for some reason is slower
			w = (0.5f + 0.5f*cos((double)u*M_PI))*(0.5f + 0.5f*cos((double)v*M_PI));
			fRGB_mult(fcol, w);
			qd_setPixel(ckrn, x, y, fcol);
		}
	}

	convolve(tsrc, tsrc, ckrn);
	free_compbuf(ckrn);
	mixImages(dst, tsrc, 0.5f + 0.5f*ndg->mix);
	free_compbuf(tsrc);
}

//--------------------------------------------------------------------------------------------

static void node_composit_exec_glare(void *UNUSED(data), bNode *node, bNodeStack **in, bNodeStack **out)
{
	CompBuf *new, *src, *img = in[0]->data;
	NodeGlare* ndg = node->storage;

	if ((img == NULL) || (out[0]->hasoutput == 0)) return;

	if (img->type != CB_RGBA) {
		new = typecheck_compbuf(img, CB_RGBA);
		src = typecheck_compbuf(img, CB_RGBA);
	}
	else {
		new = dupalloc_compbuf(img);
		src = dupalloc_compbuf(img);
	}

	{
		int x, y;
		for (y=0; y<new->y; ++y) {
			fRGB* col = (fRGB*)&new->rect[y*new->x*new->type];
			for (x=0; x<new->x; ++x) {
				col[x][0] = MAX2(col[x][0], 0.f);
				col[x][1] = MAX2(col[x][1], 0.f);
				col[x][2] = MAX2(col[x][2], 0.f);
			}
		}
	}

	switch (ndg->type) {
		case 0:
			star4(ndg, new, src);
			break;
		case 1:
			fglow(ndg, new, src);
			break;
		case 3:
			ghosts(ndg, new, src);
			break;
		case 2:
		default:
			streaks(ndg, new, src);
			break;
	}

	free_compbuf(src);
	out[0]->data = new;
}

static void node_composit_init_glare(bNodeTree *UNUSED(ntree), bNode* node, bNodeTemplate *UNUSED(ntemp))
{
	NodeGlare *ndg = MEM_callocN(sizeof(NodeGlare), "node glare data");
	ndg->quality = 1;
	ndg->type = 2;
	ndg->iter = 3;
	ndg->colmod = 0.25;
	ndg->mix = 0;
	ndg->threshold = 1;
	ndg->angle = 4;
	ndg->angle_ofs = 0.0f;
	ndg->fade = 0.9;
	ndg->size = 8;
	node->storage = ndg;
}

void register_node_type_cmp_glare(bNodeTreeType *ttype)
{
	static bNodeType ntype;

	node_type_base(ttype, &ntype, CMP_NODE_GLARE, "Glare", NODE_CLASS_OP_FILTER, NODE_OPTIONS);
	node_type_socket_templates(&ntype, cmp_node_glare_in, cmp_node_glare_out);
	node_type_size(&ntype, 150, 120, 200);
	node_type_init(&ntype, node_composit_init_glare);
	node_type_storage(&ntype, "NodeGlare", node_free_standard_storage, node_copy_standard_storage);
	node_type_exec(&ntype, node_composit_exec_glare);

	nodeRegisterType(ttype, &ntype);
}
