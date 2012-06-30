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
 * Contributor(s): Campbell Barton, Alfredo de Greef, David Millan Escriva,
 * Juho Vepsäläinen
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/nodes/composite/nodes/node_composite_blur.c
 *  \ingroup cmpnodes
 */


#include "node_composite_util.h"

/* **************** BLUR ******************** */
static bNodeSocketTemplate cmp_node_blur_in[] = {
	{   SOCK_RGBA, 1, N_("Image"),          1.0f, 1.0f, 1.0f, 1.0f},
	{   SOCK_FLOAT, 1, N_("Size"),          1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, PROP_FACTOR},
	{   -1, 0, ""   }
};
static bNodeSocketTemplate cmp_node_blur_out[] = {
	{   SOCK_RGBA, 0, N_("Image")},
	{   -1, 0, ""   }
};

static float *make_gausstab(int filtertype, int rad)
{
	float *gausstab, sum, val;
	int i, n;
	
	n = 2 * rad + 1;
	
	gausstab = (float *) MEM_mallocN(n * sizeof(float), "gauss");
	
	sum = 0.0f;
	for (i = -rad; i <= rad; i++) {
		val = RE_filter_value(filtertype, (float)i / (float)rad);
		sum += val;
		gausstab[i + rad] = val;
	}
	
	sum = 1.0f / sum;
	for (i = 0; i < n; i++)
		gausstab[i] *= sum;
	
	return gausstab;
}

static float *make_bloomtab(int rad)
{
	float *bloomtab, val;
	int i, n;
	
	n = 2 * rad + 1;
	
	bloomtab = (float *) MEM_mallocN(n * sizeof(float), "bloom");
	
	for (i = -rad; i <= rad; i++) {
		val = powf(1.0f - fabsf((float)i) / ((float)rad), 4.0f);
		bloomtab[i + rad] = val;
	}
	
	return bloomtab;
}

/* both input images of same type, either 4 or 1 channel */
static void blur_single_image(bNode *node, CompBuf *new, CompBuf *img, float scale)
{
	NodeBlurData *nbd = node->storage;
	CompBuf *work;
	register float sum, val;
	float rval, gval, bval, aval;
	float *gausstab, *gausstabcent;
	int rad, imgx = img->x, imgy = img->y;
	int x, y, pix = img->type;
	int i, bigstep;
	float *src, *dest;
	
	/* helper image */
	work = alloc_compbuf(imgx, imgy, img->type, 1); /* allocs */

	/* horizontal */
	if (nbd->sizex == 0) {
		memcpy(work->rect, img->rect, sizeof(float) * img->type * imgx * imgy);
	}
	else {
		rad = scale * (float)nbd->sizex;
		if (rad > imgx / 2)
			rad = imgx / 2;
		else if (rad < 1)
			rad = 1;
		
		gausstab = make_gausstab(nbd->filtertype, rad);
		gausstabcent = gausstab + rad;
		
		for (y = 0; y < imgy; y++) {
			float *srcd = img->rect + pix * (y * img->x);
			
			dest = work->rect + pix * (y * img->x);
			
			for (x = 0; x < imgx; x++) {
				int minr = x - rad < 0 ? -x : -rad;
				int maxr = x + rad > imgx ? imgx - x : rad;
				
				src = srcd + pix * (x + minr);
				
				sum = gval = rval = bval = aval = 0.0f;
				for (i = minr; i < maxr; i++) {
					val = gausstabcent[i];
					sum += val;
					rval += val * (*src++);
					if (pix == 4) {
						gval += val * (*src++);
						bval += val * (*src++);
						aval += val * (*src++);
					}
				}
				sum = 1.0f / sum;
				*dest++ = rval * sum;
				if (pix == 4) {
					*dest++ = gval * sum;
					*dest++ = bval * sum;
					*dest++ = aval * sum;
				}
			}
			if (node->exec & NODE_BREAK)
				break;
		}
		
		/* vertical */
		MEM_freeN(gausstab);
	}
	
	if (nbd->sizey == 0) {
		memcpy(new->rect, work->rect, sizeof(float) * img->type * imgx * imgy);
	}
	else {
		rad = scale * (float)nbd->sizey;
		if (rad > imgy / 2)
			rad = imgy / 2;
		else if (rad < 1)
			rad = 1;
	
		gausstab = make_gausstab(nbd->filtertype, rad);
		gausstabcent = gausstab + rad;
		
		bigstep = pix * imgx;
		for (x = 0; x < imgx; x++) {
			float *srcd = work->rect + pix * x;
			
			dest = new->rect + pix * x;
			
			for (y = 0; y < imgy; y++) {
				int minr = y - rad < 0 ? -y : -rad;
				int maxr = y + rad > imgy ? imgy - y : rad;
				
				src = srcd + bigstep * (y + minr);
				
				sum = gval = rval = bval = aval = 0.0f;
				for (i = minr; i < maxr; i++) {
					val = gausstabcent[i];
					sum += val;
					rval += val * src[0];
					if (pix == 4) {
						gval += val * src[1];
						bval += val * src[2];
						aval += val * src[3];
					}
					src += bigstep;
				}
				sum = 1.0f / sum;
				dest[0] = rval * sum;
				if (pix == 4) {
					dest[1] = gval * sum;
					dest[2] = bval * sum;
					dest[3] = aval * sum;
				}
				dest += bigstep;
			}
			if (node->exec & NODE_BREAK)
				break;
		}
		MEM_freeN(gausstab);
	}

	free_compbuf(work);
}

/* reference has to be mapped 0-1, and equal in size */
static void bloom_with_reference(CompBuf *new, CompBuf *img, CompBuf *UNUSED(ref), float UNUSED(fac), NodeBlurData *nbd)
{
	CompBuf *wbuf;
	register float val;
	float radxf, radyf;
	float **maintabs;
	float *gausstabx, *gausstabcenty;
	float *gausstaby, *gausstabcentx;
	int radx, rady, imgx = img->x, imgy = img->y;
	int x, y;
	int i, j;
	float *src, *dest, *wb;
	
	wbuf = alloc_compbuf(imgx, imgy, CB_VAL, 1);
	
	/* horizontal */
	radx = (float)nbd->sizex;
	if (radx > imgx / 2)
		radx = imgx / 2;
	else if (radx < 1)
		radx = 1;
	
	/* vertical */
	rady = (float)nbd->sizey;
	if (rady > imgy / 2)
		rady = imgy / 2;
	else if (rady < 1)
		rady = 1;

	x = MAX2(radx, rady);
	maintabs = MEM_mallocN(x * sizeof(void *), "gauss array");
	for (i = 0; i < x; i++)
		maintabs[i] = make_bloomtab(i + 1);
		
	/* vars to store before we go */
//	refd= ref->rect;
	src = img->rect;
	
	radxf = (float)radx;
	radyf = (float)rady;
	
	for (y = 0; y < imgy; y++) {
		for (x = 0; x < imgx; x++, src += 4) { //, refd++) {
			
//			int refradx= (int)(refd[0]*radxf);
//			int refrady= (int)(refd[0]*radyf);
			
			int refradx = (int)(radxf * 0.3f * src[3] * (src[0] + src[1] + src[2]));
			int refrady = (int)(radyf * 0.3f * src[3] * (src[0] + src[1] + src[2]));
			
			if (refradx > radx) refradx = radx;
			else if (refradx < 1) refradx = 1;
			if (refrady > rady) refrady = rady;
			else if (refrady < 1) refrady = 1;

			if (refradx == 1 && refrady == 1) {
				wb = wbuf->rect + (y * imgx + x);
				dest = new->rect + 4 * (y * imgx + x);
				wb[0] += 1.0f;
				dest[0] += src[0];
				dest[1] += src[1];
				dest[2] += src[2];
				dest[3] += src[3];
			}
			else {
				int minxr = x - refradx < 0 ? -x : -refradx;
				int maxxr = x + refradx > imgx ? imgx - x : refradx;
				int minyr = y - refrady < 0 ? -y : -refrady;
				int maxyr = y + refrady > imgy ? imgy - y : refrady;

				float *destd = new->rect + 4 * ( (y + minyr) * imgx + x + minxr);
				float *wbufd = wbuf->rect + ( (y + minyr) * imgx + x + minxr);

				gausstabx = maintabs[refradx - 1];
				gausstabcentx = gausstabx + refradx;
				gausstaby = maintabs[refrady - 1];
				gausstabcenty = gausstaby + refrady;

				for (i = minyr; i < maxyr; i++, destd += 4 * imgx, wbufd += imgx) {
					dest = destd;
					wb = wbufd;
					for (j = minxr; j < maxxr; j++, dest += 4, wb++) {
						
						val = gausstabcenty[i] * gausstabcentx[j];
						wb[0] += val;
						dest[0] += val * src[0];
						dest[1] += val * src[1];
						dest[2] += val * src[2];
						dest[3] += val * src[3];
					}
				}
			}
		}
	}
	
	x = imgx * imgy;
	dest = new->rect;
	wb = wbuf->rect;
	while (x--) {
		val = 1.0f / wb[0];
		dest[0] *= val;
		dest[1] *= val;
		dest[2] *= val;
		dest[3] *= val;
		wb++;
		dest += 4;
	}
	
	free_compbuf(wbuf);
	
	x = MAX2(radx, rady);
	for (i = 0; i < x; i++)
		MEM_freeN(maintabs[i]);
	MEM_freeN(maintabs);
	
}

#if 0
static float hexagon_filter(float fi, float fj)
{
	fi = fabs(fi);
	fj = fabs(fj);
	
	if (fj > 0.33f) {
		fj = (fj - 0.33f) / 0.66f;
		if (fi + fj > 1.0f)
			return 0.0f;
		else
			return 1.0f;
	}
	else return 1.0f;
}
#endif

/* uses full filter, no horizontal/vertical optimize possible */
/* both images same type, either 1 or 4 channels */
static void bokeh_single_image(bNode *node, CompBuf *new, CompBuf *img, float fac)
{
	NodeBlurData *nbd = node->storage;
	register float val;
	float radxf, radyf;
	float *gausstab, *dgauss;
	int radx, rady, imgx = img->x, imgy = img->y;
	int x, y, pix = img->type;
	int i, j, n;
	float *src = NULL, *dest, *srcd = NULL;
	
	/* horizontal */
	radxf = fac * (float)nbd->sizex;
	if (radxf > imgx / 2.0f)
		radxf = imgx / 2.0f;
	else if (radxf < 1.0f)
		radxf = 1.0f;
	
	/* vertical */
	radyf = fac * (float)nbd->sizey;
	if (radyf > imgy / 2.0f)
		radyf = imgy / 2.0f;
	else if (radyf < 1.0f)
		radyf = 1.0f;
	
	radx = ceil(radxf);
	rady = ceil(radyf);
	
	n = (2 * radx + 1) * (2 * rady + 1);
	
	/* create a full filter image */
	gausstab = MEM_mallocN(sizeof(float) * n, "filter tab");
	dgauss = gausstab;
	val = 0.0f;
	for (j = -rady; j <= rady; j++) {
		for (i = -radx; i <= radx; i++, dgauss++) {
			float fj = (float)j / radyf;
			float fi = (float)i / radxf;
			float dist = sqrt(fj * fj + fi * fi);

			// *dgauss= hexagon_filter(fi, fj);
			*dgauss = RE_filter_value(nbd->filtertype, dist);

			val += *dgauss;
		}
	}

	if (val != 0.0f) {
		val = 1.0f / val;
		for (j = n - 1; j >= 0; j--)
			gausstab[j] *= val;
	}
	else gausstab[4] = 1.0f;

	for (y = -rady + 1; y < imgy + rady - 1; y++) {

		if (y <= 0) srcd = img->rect;
		else if (y < imgy) srcd += pix * imgx;
		else srcd = img->rect + pix * (imgy - 1) * imgx;

		for (x = -radx + 1; x < imgx + radx - 1; x++) {
			int minxr = x - radx < 0 ? -x : -radx;
			int maxxr = x + radx >= imgx ? imgx - x - 1 : radx;
			int minyr = y - rady < 0 ? -y : -rady;
			int maxyr = y + rady > imgy - 1 ? imgy - y - 1 : rady;

			float *destd = new->rect + pix * ( (y + minyr) * imgx + x + minxr);
			float *dgausd = gausstab + (minyr + rady) * (2 * radx + 1) + minxr + radx;

			if (x <= 0) src = srcd;
			else if (x < imgx) src += pix;
			else src = srcd + pix * (imgx - 1);

			for (i = minyr; i <= maxyr; i++, destd += pix * imgx, dgausd += 2 * radx + 1) {
				dest = destd;
				dgauss = dgausd;
				for (j = minxr; j <= maxxr; j++, dest += pix, dgauss++) {
					val = *dgauss;
					if (val != 0.0f) {
						dest[0] += val * src[0];
						if (pix > 1) {
							dest[1] += val * src[1];
							dest[2] += val * src[2];
							dest[3] += val * src[3];
						}
					}
				}
			}
		}
		if (node->exec & NODE_BREAK)
			break;
	}
	
	MEM_freeN(gausstab);
}


/* reference has to be mapped 0-1, and equal in size */
static void blur_with_reference(bNode *node, CompBuf *new, CompBuf *img, CompBuf *ref)
{
	NodeBlurData *nbd = node->storage;
	CompBuf *blurbuf, *ref_use;
	register float sum, val;
	float rval, gval, bval, aval, radxf, radyf;
	float **maintabs;
	float *gausstabx, *gausstabcenty;
	float *gausstaby, *gausstabcentx;
	int radx, rady, imgx = img->x, imgy = img->y;
	int x, y, pix = img->type;
	int i, j;
	float *src, *dest, *refd, *blurd;
	float defcol[4] = {1.0f, 1.0f, 1.0f, 1.0f}; /* default color for compbuf_get_pixel */
	float proccol[4];   /* local color if compbuf is procedural */
	int refradx, refrady;

	if (ref->x != img->x || ref->y != img->y)
		return;
	
	ref_use = typecheck_compbuf(ref, CB_VAL);
	
	/* trick is; we blur the reference image... but only works with clipped values*/
	blurbuf = alloc_compbuf(imgx, imgy, CB_VAL, 1);
	blurbuf->xof = ref_use->xof;
	blurbuf->yof = ref_use->yof;
	blurd = blurbuf->rect;
	refd = ref_use->rect;
	for (x = imgx * imgy; x > 0; x--, refd++, blurd++) {
		if (refd[0] < 0.0f) blurd[0] = 0.0f;
		else if (refd[0] > 1.0f) blurd[0] = 1.0f;
		else blurd[0] = refd[0];
	}
	
	blur_single_image(node, blurbuf, blurbuf, 1.0f);
	
	/* horizontal */
	radx = (float)nbd->sizex;
	if (radx > imgx / 2)
		radx = imgx / 2;
	else if (radx < 1)
		radx = 1;
	
	/* vertical */
	rady = (float)nbd->sizey;
	if (rady > imgy / 2)
		rady = imgy / 2;
	else if (rady < 1)
		rady = 1;

	x = MAX2(radx, rady);
	maintabs = MEM_mallocN(x * sizeof(void *), "gauss array");
	for (i = 0; i < x; i++)
		maintabs[i] = make_gausstab(nbd->filtertype, i + 1);

	dest = new->rect;
	radxf = (float)radx;
	radyf = (float)rady;
	
	for (y = 0; y < imgy; y++) {
		for (x = 0; x < imgx; x++, dest += pix) {
			refd = compbuf_get_pixel(blurbuf, defcol, proccol, x - blurbuf->xrad, y - blurbuf->yrad, blurbuf->xrad, blurbuf->yrad);
			refradx = (int)(refd[0] * radxf);
			refrady = (int)(refd[0] * radyf);

			if (refradx > radx) refradx = radx;
			else if (refradx < 1) refradx = 1;
			if (refrady > rady) refrady = rady;
			else if (refrady < 1) refrady = 1;

			if (refradx == 1 && refrady == 1) {
				src = img->rect + pix * (y * imgx + x);
				if (pix == 1)
					dest[0] = src[0];
				else
					copy_v4_v4(dest, src);
			}
			else {
				int minxr = x - refradx < 0 ? -x : -refradx;
				int maxxr = x + refradx > imgx ? imgx - x : refradx;
				int minyr = y - refrady < 0 ? -y : -refrady;
				int maxyr = y + refrady > imgy ? imgy - y : refrady;

				float *srcd = img->rect + pix * ( (y + minyr) * imgx + x + minxr);

				gausstabx = maintabs[refradx - 1];
				gausstabcentx = gausstabx + refradx;
				gausstaby = maintabs[refrady - 1];
				gausstabcenty = gausstaby + refrady;

				sum = gval = rval = bval = aval = 0.0f;

				for (i = minyr; i < maxyr; i++, srcd += pix * imgx) {
					src = srcd;
					for (j = minxr; j < maxxr; j++, src += pix) {
					
						val = gausstabcenty[i] * gausstabcentx[j];
						sum += val;
						rval += val * src[0];
						if (pix > 1) {
							gval += val * src[1];
							bval += val * src[2];
							aval += val * src[3];
						}
					}
				}
				sum = 1.0f / sum;
				dest[0] = rval * sum;
				if (pix > 1) {
					dest[1] = gval * sum;
					dest[2] = bval * sum;
					dest[3] = aval * sum;
				}
			}
		}
		if (node->exec & NODE_BREAK)
			break;
	}
	
	free_compbuf(blurbuf);
	
	x = MAX2(radx, rady);
	for (i = 0; i < x; i++)
		MEM_freeN(maintabs[i]);
	MEM_freeN(maintabs);
	
	if (ref_use != ref)
		free_compbuf(ref_use);
}

static void node_composit_exec_blur(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
	CompBuf *new, *img = in[0]->data;
	NodeBlurData *nbd = node->storage;
	
	if (img == NULL) return;
	
	/* store image in size that is needed for absolute/relative conversions on ui level */
	nbd->image_in_width = img->x;
	nbd->image_in_height = img->y;
	
	if (out[0]->hasoutput == 0) return;
	
	if (nbd->relative) {
		if (nbd->aspect == CMP_NODE_BLUR_ASPECT_NONE) {
			nbd->sizex = (int)(nbd->percentx * 0.01f * nbd->image_in_width);
			nbd->sizey = (int)(nbd->percenty * 0.01f * nbd->image_in_height);
		}
		else if (nbd->aspect == CMP_NODE_BLUR_ASPECT_Y) {
			nbd->sizex = (int)(nbd->percentx * 0.01f * nbd->image_in_width);
			nbd->sizey = (int)(nbd->percenty * 0.01f * nbd->image_in_width);
		}
		else if (nbd->aspect == CMP_NODE_BLUR_ASPECT_X) {
			nbd->sizex = (int)(nbd->percentx * 0.01f * nbd->image_in_height);
			nbd->sizey = (int)(nbd->percenty * 0.01f * nbd->image_in_height);
		}
	}

	if (nbd->sizex == 0 && nbd->sizey == 0) {
		new = pass_on_compbuf(img);
		out[0]->data = new;
	}
	else if (nbd->filtertype == R_FILTER_FAST_GAUSS) {
		if (in[1]->vec[0] < 0.001f) { /* time node inputs can be a tiny value */
			new = pass_on_compbuf(img);
		}
		else {
			// TODO: can this be mapped with reference, too?
			const float sx = ((float)nbd->sizex * in[1]->vec[0]) / 2.0f, sy = ((float)nbd->sizey * in[1]->vec[0]) / 2.0f;
			int c;

			if ((img == NULL) || (out[0]->hasoutput == 0)) return;

			if (img->type == CB_VEC2)
				new = typecheck_compbuf(img, CB_VAL);
			else if (img->type == CB_VEC3)
				new = typecheck_compbuf(img, CB_RGBA);
			else
				new = dupalloc_compbuf(img);

			if ((sx == sy) && (sx > 0.f)) {
				for (c = 0; c < new->type; ++c)
					IIR_gauss(new, sx, c, 3);
			}
			else {
				if (sx > 0.f) {
					for (c = 0; c < new->type; ++c)
						IIR_gauss(new, sx, c, 1);
				}
				if (sy > 0.f) {
					for (c = 0; c < new->type; ++c)
						IIR_gauss(new, sy, c, 2);
				}
			}
		}
		out[0]->data = new;
	}
	else {
		/* All non fast gauss blur methods */
		if (img->type == CB_VEC2 || img->type == CB_VEC3) {
			img = typecheck_compbuf(in[0]->data, CB_RGBA);
		}
		
		/* if fac input, we do it different */
		if (in[1]->data) {
			CompBuf *gammabuf;
			
			/* make output size of input image */
			new = alloc_compbuf(img->x, img->y, img->type, 1); /* allocs */
			
			/* accept image offsets from other nodes */
			new->xof = img->xof;
			new->yof = img->yof;
			
			if (nbd->gamma) {
				gammabuf = dupalloc_compbuf(img);
				gamma_correct_compbuf(gammabuf, 0);
			}
			else gammabuf = img;
			
			blur_with_reference(node, new, gammabuf, in[1]->data);
			
			if (nbd->gamma) {
				gamma_correct_compbuf(new, 1);
				free_compbuf(gammabuf);
			}
			if (node->exec & NODE_BREAK) {
				free_compbuf(new);
				new = NULL;
			}
			out[0]->data = new;
		}
		else {
			
			if (in[1]->vec[0] <= 0.001f) {    /* time node inputs can be a tiny value */
				new = pass_on_compbuf(img);
			}
			else {
				CompBuf *gammabuf;
				
				/* make output size of input image */
				new = alloc_compbuf(img->x, img->y, img->type, 1); /* allocs */
				
				/* accept image offsets from other nodes */
				new->xof = img->xof;
				new->yof = img->yof;
					
				if (nbd->gamma) {
					gammabuf = dupalloc_compbuf(img);
					gamma_correct_compbuf(gammabuf, 0);
				}
				else gammabuf = img;
				
				if (nbd->bokeh)
					bokeh_single_image(node, new, gammabuf, in[1]->vec[0]);
				else if (1)
					blur_single_image(node, new, gammabuf, in[1]->vec[0]);
				else  /* bloom experimental... */
					bloom_with_reference(new, gammabuf, NULL, in[1]->vec[0], nbd);
				
				if (nbd->gamma) {
					gamma_correct_compbuf(new, 1);
					free_compbuf(gammabuf);
				}
				if (node->exec & NODE_BREAK) {
					free_compbuf(new);
					new = NULL;
				}
			}
			out[0]->data = new;
		}
		if (img != in[0]->data)
			free_compbuf(img);
	}

	generate_preview(data, node, out[0]->data);
}

static void node_composit_init_blur(bNodeTree *UNUSED(ntree), bNode *node, bNodeTemplate *UNUSED(ntemp))
{
	node->storage = MEM_callocN(sizeof(NodeBlurData), "node blur data");
}

void register_node_type_cmp_blur(bNodeTreeType *ttype)
{
	static bNodeType ntype;

	node_type_base(ttype, &ntype, CMP_NODE_BLUR, "Blur", NODE_CLASS_OP_FILTER, NODE_PREVIEW | NODE_OPTIONS);
	node_type_socket_templates(&ntype, cmp_node_blur_in, cmp_node_blur_out);
	node_type_size(&ntype, 120, 80, 200);
	node_type_init(&ntype, node_composit_init_blur);
	node_type_storage(&ntype, "NodeBlurData", node_free_standard_storage, node_copy_standard_storage);
	node_type_exec(&ntype, node_composit_exec_blur);

	nodeRegisterType(ttype, &ntype);
}
