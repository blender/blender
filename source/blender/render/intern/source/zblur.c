/**
 * $Id$
 *
 * ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.
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
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

/* 
 * This file is largely based on the focal blur plugin by onk, 8.99
 *
 */

#include <math.h>
#include <string.h>
#include <limits.h>
#include <stdio.h>

#include "MEM_guardedalloc.h"

#include "DNA_camera_types.h"
#include "DNA_scene_types.h"

#include "BKE_global.h"
#include "BKE_utildefines.h"

#include "RE_callbacks.h"

#include "render.h"
#include "pixelblending.h"

#include "blendef.h"

//#include "BIF_gl.h"

/* -------------------------------------------------
 * defines, protos */

typedef enum { I_GRAY, I_FLOAT, I_FLOAT4 } IMGTYPE;

typedef struct {
	int x, y;
	int size, el_size;
	IMGTYPE type;
	char *data;
} Image;

typedef struct {			/* blur mask struct */
	int size;
	float fac;
	float *val;
} Mask;

typedef Mask* Maskarray;

/* don't change these */
#define NMASKS_SHIFT 2			
#define NMASKS 64


static Image *alloc_img(int x, int y, IMGTYPE type)
{
	Image *ret;
	int size, typesize;

	switch (type) {
	case I_GRAY:
		typesize = 1;
		break;
	case I_FLOAT:
		typesize = sizeof(float);
		break;
	case I_FLOAT4:
		typesize = 4 * sizeof(float);
		break;
	default:
		return 0;
	}

	size = x * y;
	
	ret = (Image *) MEM_mallocN(sizeof(Image) + size*typesize, "zblur_img");
	if (ret) {
		ret->x = x;
		ret->y = y;
		ret->size = size;
		ret->el_size = typesize;
		ret->type = type;
		ret->data = (char *) (ret + 1);
		size *= typesize;
		memset(ret->data, 0, size);
	}

	return ret;
}

static int free_img(Image *img)
{
	MEM_freeN(img);
	return 1;
}

/* 32 bits (int) rect to float buf */
static void recti2imgf(int *src, Image *dest, int x, int y)
{
	char *from;
	float *to;
	int i, ix, iy;
	
	if(dest->type != I_FLOAT4) return;
	
	from = (char *) src;
	to = (float *) dest->data;
	
	if (R.r.mode & R_FIELDS) {	/* double each scanline */
		for (iy=0; iy<y; iy++) {
			for (ix=0; ix<x; ix++) {
				*to++ = ((float)from[0])/255.0;
				*to++ = ((float)from[1])/255.0;
				*to++ = ((float)from[2])/255.0;
				*to++ = ((float)from[3])/255.0;
				from += 4;
			}
			
			memcpy(to, to-4*sizeof(float)*x, 4*sizeof(float)*x);
			to+= 4*x;
			
			iy++;
		}
	}
	else {
		i = x * y;
		while(i--) {
			*to++ = ((float)from[0])/255.0;
			*to++ = ((float)from[1])/255.0;
			*to++ = ((float)from[2])/255.0;
			*to++ = ((float)from[3])/255.0;
			from += 4;
		}
	}
}

/* float rect to float buf */
static void rectf2imgf(float *src, Image *dest, int x, int y)
{
	float *from;
	float *to;
	int i, iy;
	
	if(dest->type != I_FLOAT4) return;

	from = src;
	to = (float *) dest->data;
	
	if (R.r.mode & R_FIELDS) {	/* double each scanline */
		for (iy=0; iy<y; iy++) {
			
			memcpy(to, from, 4*sizeof(float)*x);
			to+= 4*x;
			memcpy(to, from, 4*sizeof(float)*x);
			to+= 4*x;
			
			iy++;
			from += 4*x;
		}
	}
	else {
		i = y;
		while(i--) {
			memcpy(to, from, 4*sizeof(float)*x);
			from += 4*x;
			to += 4*x;
		}
	}
}

/* floatbuf back to 32 bits rect */
static void imgf2recti(Image *src, int *dest)
{
	float *from;
	char *to;
	int i, ix, iy;
	
	if(src->type != I_FLOAT4) return;
	
	from = (float *) src->data;
	to = (char *) dest;
	
	if (R.r.mode & R_FIELDS) {
		for (iy=0; iy<src->y; iy++) {
			for (ix=0; ix<src->x; ix++) {
				*to++ = (char)(from[0]*255.0);
				*to++ = (char)(from[1]*255.0);
				*to++ = (char)(from[2]*255.0);
				*to++ = (char)(from[3]*255.0);
				from += 4;
			}
			iy++;
			from+= 4*src->x;
		}	
	}
	else {
		i = src->x * src->y;
		while(i--) {
			*to++ = (char)(from[0]*255.0);
			*to++ = (char)(from[1]*255.0);
			*to++ = (char)(from[2]*255.0);
			*to++ = (char)(from[3]*255.0);
			from += 4;
		}
	}
}

/* floatbuf back to float rect */
static void imgf2rectf(Image *src, float *dest)
{
	float *from;
	float *to;
	int i, iy;
	
	if(src->type != I_FLOAT4) return;
	
	from = (float *) src->data;
	to = dest;
	
	if (R.r.mode & R_FIELDS) {
		for (iy=0; iy<src->y; iy++) {
			
			memcpy(to, from, 4*sizeof(float)*src->x);
			
			iy++;
			to+= 4*src->x;
			from+= 8*src->x;
		}
	}
	else {
		i = src->x * src->y;
		memcpy(to, from, 4*sizeof(float)*i);
	}
}


static void imgf_gamma(Image *src, float gamma)
{
	float *to;
	int i;
	
	if(gamma==1.0) return;
	
	i = 4 * src->x * src->y;
	to= (float *) src->data;
	while(i--) {
		*to = (float)pow(*to, gamma); 
		to++;
	}
}

#if 0
/* create new image with alpha & color zero where mask is zero */
static Image *imgf_apply_mask(Image *src, Image *zmask)
{
	Image *dest;
	float *from, *to;
	int i;
	char *zptr;
	
	dest = alloc_img(src->x, src->y, I_FLOAT4);
	
	i= src->x * src->y;
	from= (float *) src->data;
	to= (float *) dest->data;
	zptr= (char *)zmask->data;

	while(i--) {
		if(*zptr) {
			to[0]= from[0];
			to[1]= from[1];
			to[2]= from[2];
			to[3]= from[3];
		}
		else {
			to[0]= to[1]= to[2]= to[3]= 0.0f;
		}
		zptr++;
		to+= 4;
		from+= 4;
	}
	
	return dest;
}

static void imgf_alpha_over(Image *dest, Image *src)
{
	float *from, *to;
	int i;
	
	i= src->x * src->y;
	from= (float *) src->data;
	to= (float *) dest->data;
	
	while(i--) {
		addAlphaOverFloat(to, from);
		to+= 4;
		from+= 4;
	}
}

#endif

/* --------------------------------------------------------------------- */
/* mask routines */

static Mask *alloc_mask(int size)
{
	Mask *m;
	int memsize;

	memsize = (sizeof(Mask) + (2 * size +1) * (2 * size +1) * sizeof(float));

	m = (Mask*) MEM_mallocN(memsize, "zblur_mask");
	m->size = size;
	m->val = (float *) (m + 1);

	return m;
}

static void free_mask(Mask *m)
{
	int memsize;

	memsize = 2 * m->size + 1;
	memsize *= memsize * sizeof(float);
	memsize += sizeof(Mask);
	
	MEM_freeN(m);
}

/* normalize mask to 1 */

static void norm_mask(Mask *m)
{
	float fac;
	int size;
	float *v;

	fac = m->fac;
	size = (2 * m->size +1)*(2 * m->size +1);

	v = m->val;
	while(size--) {
		*v++ *= fac;
	}
	m->fac = 1.0;
}

/* filters a grayvalue image with a gaussian IIR filter with blur radius "rad" 
 * For large blurs, it's more efficient to call the routine several times
 * instead of using big blur radii.
 * The original image is changed */


static void gauss_blur(Image *img, float rad)
{
	Image *new;
	register float sum, val;
	float gval;
	float *gausstab, *v;
	int r, n, m;
	int x, y;
	int i;
	int step, bigstep;
	char *src, *dest;

	r = (1.5 * rad + 1.5);
	n = 2 * r + 1;
	
	/* ugly : */
	if ((img->x <= n) || (img->y <= n)) {
		return;
	}
	
	gausstab = (float *) MEM_mallocN(n * sizeof(float), "zblur_gauss");
	if (!gausstab) {
		return;
	}
	
	sum = 0.0;
	v = gausstab;
	for (x = -r; x <= r; x++) {

		val = exp(-4*(float ) (x*x)/ (float) (r*r));
		sum += val;
		*v++ = val;
	}

	i = n;
	v = gausstab;
	while (i--) {
		*v++ /= sum;
	}

	new = alloc_img(img->x, img->y, I_GRAY);
	if (!new) {
		return;
	}

	/* horizontal */

	step = (n - 1);

	for (y = 0; y < img->y; y++) {
		src = (char *)img->data + (y * img->x);
		dest = (char *)new->data + (y * img->x);

		for (x = r; x > 0 ; x--) {
			m = n - x;
			gval = 0.0;
			sum = 0.0;
			v = gausstab + x;
			for (i = 0; i < m; i++) {
				val = *v++;
				sum += val;
				gval += val * (*src++);
			}
			*dest++ = gval / sum;
			src -= m;
		}

		for (x = 0; x <= (img->x - n); x++) {
			gval = 0.0;
			v = gausstab;

			for (i = 0; i < n; i++) {
				val = *v++;
				gval += val * (*src++);
			}
			*dest++ = gval;
			src -= step;
		}	

		for (x = 1; x <= r ; x++) {
			m = n - x;
			gval = 0.0;
			sum = 0.0;
			v = gausstab;
			for (i = 0; i < m; i++) {
				val = *v++;
				sum += val;
				gval += val * (*src++);
			}
			*dest++ = gval / sum;
			src -= (m - 1);
		}
	}

	/* vertical */

	step = img->x;
	bigstep = (n - 1) * step;
	for (x = 0; x < step  ; x++) {
		src = new->data + x;
		dest = img->data + x;

		for (y = r; y > 0; y--) {
			m = n - y;
			gval = 0.0;
			sum = 0.0;
			v = gausstab + y;
			for (i = 0; i < m; i++) {
				val = *v++;
				sum += val;
				gval += val * src[0];
				src += step;
			}
			dest[0] = gval / sum;
			src -= m * step;
			dest+= step;
		}
		for (y = 0; y <= (img->y - n); y++) {
			gval = 0.0;
			v = gausstab;
			for (i = 0; i < n; i++) {
				val = *v++;
				gval += val * src[0];
				src += step;
			}
			dest[0] = gval;
			dest += step;
			src -= bigstep;
		}
		for (y = 1; y <= r ; y++) {
			m = n - y;
			gval = 0.0;
			sum = 0.0;
			v = gausstab;
			for (i = 0; i < m; i++) {
				val = *v++;
				sum += val;
				gval += val * src[0];
				src += step;
			}
			dest[0] = gval / sum;
			dest += step;
			src -= (m - 1) * step;
		}
	}
	MEM_freeN(gausstab);
	free_img(new);
}

static float zigma(float x, float sigma, float sigma4)
{
	//return 1.0/(1.0+pow(x, sigma));
	
	if(x < sigma) {
		x*= sigma;
		return 1.0/exp(x*x) - sigma4;
	}
	return 0.0;
}


static Mask *gauss_mask(float rad, float sigma)
{
	Mask *m;
	float sum, val, *v, fac, radsq= rad*rad;
	float sigma4;
	int r;
	int ix, iy;
	
	r = (1.0 * rad + 1.0);
	m = alloc_mask(r);
	v = m->val;
	sum = 0.0;
	
	sigma4= 1.0/exp(sigma*sigma*sigma*sigma);
	
	for (iy = -r; iy <= r; iy++) {
		for (ix = -r; ix <= r; ix++) {
			
			fac= ((float)(ix*ix + iy*iy))/(radsq);
			val = zigma(fac, sigma, sigma4);
			
			// val = exp(-(float) (ix*ix + iy*iy)/(rad * rad));
			sum += val;
			*v++ = val;
		}
	}
	
	m->fac = 1.0 / sum;
	
	norm_mask(m);
	return m;
}

/* generates #num masks with the maximal blur radius 'rad' 
 * */
static Maskarray *init_masks(int num, float rad, float sigma)
{
	int i;
	float r, step;
	Maskarray *maskarray;

	maskarray = (Maskarray*) MEM_mallocN(num * sizeof (Maskarray), "zblur_masks");
	step = rad / num;
	r = 0.1;
	for (i = 0; i < num; i++) {
		maskarray[i] = gauss_mask(r, sigma);
		r += step;
	}
	return maskarray;
}


/* ********************* Do the blur ******************************** */

static Image *zblur(Image *src, Image *zbuf, float radius, float sigma)
{
	Image *dest;
	Maskarray *mar;
	Mask *m;
	float *sptr, *dptr;
	float *mval;				/* mask value pointer */
	float rval, gval, bval, aval;			
	float norm, fac;			
	int tmp;
	int zval;
	int size;
	int row;
	int mrow;
	int x, y;
	int i;
	int sx, sy, ex, ey;
	int mx, my;
	char *zptr;

	if(src->type != I_FLOAT4) return NULL;

	dest = alloc_img(src->x, src->y, I_FLOAT4);
	row = src->x * 4;

	mar = init_masks(NMASKS, radius, sigma);

	for (y = 0; y < src->y  ; y++) {
		for (x = 0; x < src->x; x++) {
			dptr = (float *) (dest->data + ((y * src->x + x) * src->el_size));
			zptr = zbuf->data + (y * src->x + x);
			zval = *zptr;
			sptr = (float *) (src->data + ((y *src->x + x )* src->el_size));

			m = mar[zval >> NMASKS_SHIFT];

			size = m->size;

			if(size==0 || zval==0) {
				dptr[0] = sptr[0];
				dptr[1] = sptr[1];
				dptr[2] = sptr[2];
				dptr[3] = sptr[3];
				continue;
			}

			ex = src->x - x;
			ey = src->y - y;

			sx = (x < size) ? x : size;
			sy = (y < size) ? y : size;
			ex = (ex <= size) ? ex - 1: size;
			ey = (ey <= size) ? ey - 1: size;

			sptr -= sy *src->x * 4;
			zptr -= sy * src->x;
			mrow = (size << 1) + 1;
			mval = m->val + (size - sy) * mrow + size;

			norm = rval = gval = bval = aval= 0.0;
	
			for (my = -sy; my <= ey; my++) {
				for (mx = -sx; mx <= ex; mx++) {
					if( zptr[mx] ) {
						tmp = 4 * mx;
						fac = mval[mx] * (float) zptr[mx] /255.0 ;
						
						norm += fac;
						rval += fac * sptr[tmp];
						gval += fac * sptr[tmp + 1];
						bval += fac * sptr[tmp + 2];
						aval += fac * sptr[tmp + 3];
					}
				}
				mval += mrow;
				sptr += row;
				zptr += src->x;
			}

			dptr[0] = rval / norm;
			dptr[1] = gval / norm;
			dptr[2] = bval / norm;
			dptr[3] = aval / norm;
		}
		if(!(y % 4) && RE_local_test_break()) break;		
	}
	
	for (i= 0; i < NMASKS; i++) {
		free_mask(mar[i]);
	}
	
	MEM_freeN(mar);
	
	return dest;
}


/* this splits the z-buffer into 2 gray-images (background, foreground)
* which are used for the weighted blur */

static void zsplit(int *zptr, Image *fg, Image *bg, int zfocus, int zmax, int zmin, int x, int y)
{
	char *p, *q;
	int i, ix, iy;
	float fdist;
	float fgnorm, bgnorm;

	p = fg->data;
	q = bg->data;
	bgnorm = 255.0 / ((float) zmax - (float) zfocus);
	fgnorm = 255.0 / ((float) zfocus - (float) zmin);

	if (R.r.mode & R_FIELDS) {
		for (iy=0; iy<y; iy++) {
			for (ix=0; ix<x; ix++) {
				fdist = (float) (*zptr++);
				if (fdist < zmin) fdist = zmin;
					
				fdist -= zfocus;
		
				if (fdist < 0) {
					*p = (char) (-fdist * fgnorm);
					*q = 0;
				}
				else {
					*q = (char) (fdist * bgnorm);
					*p = 0;
				}
				p++, q++;			
			}
			iy++;
			p+= x;
			q+= x;
		}
	}
	else {
		i = x * y;
		while(i--) {
			fdist = (float) (*zptr++);
			if (fdist < zmin) fdist = zmin;
				
			fdist -= zfocus;
	
			if (fdist < 0) {
				*p = (char) (-fdist * fgnorm);
				*q = 0;
			}
			else {
				*q = (char) (fdist * bgnorm);
				*p = 0;
			}
			p++, q++;
		}
	}
}

void add_zblur(void)
{
	Image *orig, *zfront, *work, *zback;
	float zblurr;
	int zfocus;
	int x, y, zmin;
	
	if (R.rectz == NULL) return;
	
	x= R.rectx;
	y= R.recty;

	zblurr= (R.r.zblur*R.r.size)/100;
	
	if (R.r.mode & R_FIELDS) {
		y *= 2;
		zblurr *= 2;
	} 

	zmin= INT_MAX*( 2.0*R.r.zmin - 1.0);	// R.r.zmin ranges 0 - 1
	zfocus = INT_MAX*( 2.0*R.r.focus - 1.0);
	
	if(zmin>zfocus) zmin= zfocus;
	
	zfront = alloc_img(x, y, I_GRAY);
	zback = alloc_img(x, y, I_GRAY);
	orig = alloc_img(x, y, I_FLOAT4);

	if(R.rectftot) rectf2imgf(R.rectftot, orig, x, y);
	else recti2imgf(R.rectot, orig, x, y);

	imgf_gamma(orig, R.r.zgamma);	// pregamma correct if required
	

	/* split up z buffer into 2 gray images	*/
	zsplit(R.rectz, zfront, zback, zfocus, INT_MAX, zmin, x, y);
	
//	glDrawBuffer(GL_FRONT);
//	glRasterPos2i(0, 0);
//	glDrawPixels(x, y, GL_RED, GL_UNSIGNED_BYTE, zback->data);
//	glFlush();
//	glDrawBuffer(GL_BACK);
	
	gauss_blur(zback, 1.0);
	gauss_blur(zfront, zblurr);
	
	/* blur back part */
	work = zblur(orig, zback, zblurr, R.r.zsigma);
	free_img(orig);
	
	/* blur front part */
	orig = zblur(work, zfront, zblurr, R.r.zsigma);

	imgf_gamma(orig, 1.0/R.r.zgamma);	// pregamma correct if required
	
	if(R.rectftot) imgf2rectf(orig, R.rectftot);
	else imgf2recti(orig, R.rectot);
	
	free_img(work);
	free_img(orig);
	free_img(zfront); 
	free_img(zback);

	/* make new display rect */
	if(R.rectftot) RE_floatbuffer_to_output();
}


