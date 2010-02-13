/**
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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 * This file was moved here from the src/ directory. It is meant to
 * deal with endianness. It resided in a general blending lib. The
 * other functions were only used during rendering. This single
 * function remained. It should probably move to imbuf/intern/util.c,
 * but we'll keep it here for the time being. (nzc)*/

/*  imageprocess.c        MIXED MODEL
 * 
 *  april 95
 * 
 * $Id$
 */

#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"
#include "math.h"

/* This define should be relocated to a global header some where  Kent Mein 
I stole it from util.h in the plugins api */
#define MAX2(x,y)                ( (x)>(y) ? (x) : (y) )

/* Only this one is used liberally here, and in imbuf */
void IMB_convert_rgba_to_abgr(struct ImBuf *ibuf)
{
	int size;
	unsigned char rt, *cp = (unsigned char *)ibuf->rect;
	float rtf, *cpf = ibuf->rect_float;

	if (ibuf->rect) {
		size = ibuf->x * ibuf->y;

		while(size-- > 0) {
			rt= cp[0];
			cp[0]= cp[3];
			cp[3]= rt;
			rt= cp[1];
			cp[1]= cp[2];
			cp[2]= rt;
			cp+= 4;
		}
	}

	if (ibuf->rect_float) {
		size = ibuf->x * ibuf->y;

		while(size-- > 0) {
			rtf= cpf[0];
			cpf[0]= cpf[3];
			cpf[3]= rtf;
			rtf= cpf[1];
			cpf[1]= cpf[2];
			cpf[2]= rtf;
			cpf+= 4;
		}
	}
}
static void pixel_from_buffer(struct ImBuf *ibuf, unsigned char **outI, float **outF, int x, int y)

{
	int offset = ibuf->x * y * 4 + 4*x;
	
	if (ibuf->rect)
		*outI= (unsigned char *)ibuf->rect + offset;
	
	if (ibuf->rect_float)
		*outF= (float *)ibuf->rect_float + offset;
}

/**************************************************************************
*                            INTERPOLATIONS 
*
* Reference and docs:
* http://wiki.blender.org/index.php/User:Damiles#Interpolations_Algorithms
***************************************************************************/

/* BICUBIC Interpolation functions */
/*  More info: http://wiki.blender.org/index.php/User:Damiles#Bicubic_pixel_interpolation
*/
/* function assumes out to be zero'ed, only does RGBA */

static float P(float k){
	float p1, p2, p3, p4;
	p1 = MAX2(k+2.0f,0);
	p2 = MAX2(k+1.0f,0);
	p3 = MAX2(k,0);
	p4 = MAX2(k-1.0f,0);
	return (float)(1.0f/6.0f)*( p1*p1*p1 - 4.0f * p2*p2*p2 + 6.0f * p3*p3*p3 - 4.0f * p4*p4*p4);
}


#if 0
/* older, slower function, works the same as above */
static float P(float k){
	return (float)(1.0f/6.0f)*( pow( MAX2(k+2.0f,0) , 3.0f ) - 4.0f * pow( MAX2(k+1.0f,0) , 3.0f ) + 6.0f * pow( MAX2(k,0) , 3.0f ) - 4.0f * pow( MAX2(k-1.0f,0) , 3.0f));
}
#endif

void bicubic_interpolation_color(struct ImBuf *in, unsigned char *outI, float *outF, float u, float v)
{
	int i,j,n,m,x1,y1;
	unsigned char *dataI;
	float a,b,w,wx,wy[4], outR,outG,outB,outA,*dataF;

	/* ImBuf in must have a valid rect or rect_float, assume this is alredy checked */

	i= (int)floor(u);
	j= (int)floor(v);
	a= u - i;
	b= v - j;

	outR = outG = outB = outA = 0.0f;
	
/* Optimized and not so easy to read */
	
	/* avoid calling multiple times */
	wy[0] = P(b-(-1));
	wy[1] = P(b-  0);
	wy[2] = P(b-  1);
	wy[3] = P(b-  2);
	
	for(n= -1; n<= 2; n++){
		x1= i+n;
		if (x1>0 && x1 < in->x) {
			wx = P(n-a);
			for(m= -1; m<= 2; m++){
				y1= j+m;
				if (y1>0 && y1<in->y) {
					/* normally we could do this */
					/* w = P(n-a) * P(b-m); */
					/* except that would call P() 16 times per pixel therefor pow() 64 times, better precalc these */
					w = wx * wy[m+1];
					
					if (outF) {
						dataF= in->rect_float + in->x * y1 * 4 + 4*x1;
						outR+= dataF[0] * w;
						outG+= dataF[1] * w;
						outB+= dataF[2] * w;
						outA+= dataF[3] * w;
					}
					if (outI) {
						dataI= (unsigned char*)in->rect + in->x * y1 * 4 + 4*x1;
						outR+= dataI[0] * w;
						outG+= dataI[1] * w;
						outB+= dataI[2] * w;
						outA+= dataI[3] * w;
					}
				}
			}
		}
	}

/* Done with optimized part */
	
#if 0 
	/* older, slower function, works the same as above */
	for(n= -1; n<= 2; n++){
		for(m= -1; m<= 2; m++){
			x1= i+n;
			y1= j+m;
			if (x1>0 && x1 < in->x && y1>0 && y1<in->y) {
				if (do_float) {
					dataF= in->rect_float + in->x * y1 * 4 + 4*x1;
					outR+= dataF[0] * P(n-a) * P(b-m);
					outG+= dataF[1] * P(n-a) * P(b-m);
					outB+= dataF[2] * P(n-a) * P(b-m);
					outA+= dataF[3] * P(n-a) * P(b-m);
				}
				if (do_rect) {
					dataI= (unsigned char*)in->rect + in->x * y1 * 4 + 4*x1;
					outR+= dataI[0] * P(n-a) * P(b-m);
					outG+= dataI[1] * P(n-a) * P(b-m);
					outB+= dataI[2] * P(n-a) * P(b-m);
					outA+= dataI[3] * P(n-a) * P(b-m);
				}
			}
		}
	}
#endif
	
	if (outI) {
		outI[0]= (int)outR;
		outI[1]= (int)outG;
		outI[2]= (int)outB;
		outI[3]= (int)outA;
	}
	if (outF) {
		outF[0]= outR;
		outF[1]= outG;
		outF[2]= outB;
		outF[3]= outA;
	}
}


void bicubic_interpolation(ImBuf *in, ImBuf *out, float u, float v, int xout, int yout)
{
	
	unsigned char *outI = NULL;
	float *outF = NULL;
	
	if (in == NULL || (in->rect == NULL && in->rect_float == NULL)) return;
	
	pixel_from_buffer(out, &outI, &outF, xout, yout); /* gcc warns these could be uninitialized, but its ok */
	
	bicubic_interpolation_color(in, outI, outF, u, v);
}

/* function assumes out to be zero'ed, only does RGBA */
/* BILINEAR INTERPOLATION */
void bilinear_interpolation_color(struct ImBuf *in, unsigned char *outI, float *outF, float u, float v)
{
	float *row1, *row2, *row3, *row4, a, b;
	unsigned char *row1I, *row2I, *row3I, *row4I;
	float a_b, ma_b, a_mb, ma_mb;
	float empty[4]= {0.0f, 0.0f, 0.0f, 0.0f};
	unsigned char emptyI[4]= {0, 0, 0, 0};
	int y1, y2, x1, x2;
	
	
	/* ImBuf in must have a valid rect or rect_float, assume this is alredy checked */

	x1= (int)floor(u);
	x2= (int)ceil(u);
	y1= (int)floor(v);
	y2= (int)ceil(v);

	// sample area entirely outside image? 
	if (x2<0 || x1>in->x-1 || y2<0 || y1>in->y-1) return;

	if (outF) {
		// sample including outside of edges of image 
		if (x1<0 || y1<0) row1= empty;
		else row1= (float *)in->rect_float + in->x * y1 * 4 + 4*x1;
		
		if (x1<0 || y2>in->y-1) row2= empty;
		else row2= (float *)in->rect_float + in->x * y2 * 4 + 4*x1;
		
		if (x2>in->x-1 || y1<0) row3= empty;
		else row3= (float *)in->rect_float + in->x * y1 * 4 + 4*x2;
		
		if (x2>in->x-1 || y2>in->y-1) row4= empty;
		else row4= (float *)in->rect_float + in->x * y2 * 4 + 4*x2;
		
		a= u-floor(u);
		b= v-floor(v);
		a_b= a*b; ma_b= (1.0f-a)*b; a_mb= a*(1.0f-b); ma_mb= (1.0f-a)*(1.0f-b);
		
		outF[0]= ma_mb*row1[0] + a_mb*row3[0] + ma_b*row2[0]+ a_b*row4[0];
		outF[1]= ma_mb*row1[1] + a_mb*row3[1] + ma_b*row2[1]+ a_b*row4[1];
		outF[2]= ma_mb*row1[2] + a_mb*row3[2] + ma_b*row2[2]+ a_b*row4[2];
		outF[3]= ma_mb*row1[3] + a_mb*row3[3] + ma_b*row2[3]+ a_b*row4[3];
	}
	if (outI) {
		// sample including outside of edges of image 
		if (x1<0 || y1<0) row1I= emptyI;
		else row1I= (unsigned char *)in->rect + in->x * y1 * 4 + 4*x1;
		
		if (x1<0 || y2>in->y-1) row2I= emptyI;
		else row2I= (unsigned char *)in->rect + in->x * y2 * 4 + 4*x1;
		
		if (x2>in->x-1 || y1<0) row3I= emptyI;
		else row3I= (unsigned char *)in->rect + in->x * y1 * 4 + 4*x2;
		
		if (x2>in->x-1 || y2>in->y-1) row4I= emptyI;
		else row4I= (unsigned char *)in->rect + in->x * y2 * 4 + 4*x2;
		
		a= u-floor(u);
		b= v-floor(v);
		a_b= a*b; ma_b= (1.0f-a)*b; a_mb= a*(1.0f-b); ma_mb= (1.0f-a)*(1.0f-b);
		
		/* need to add 0.5 to avoid rounding down (causes darken with the smear brush)
		 * tested with white images and this should not wrap back to zero */
		outI[0]= (ma_mb*row1I[0] + a_mb*row3I[0] + ma_b*row2I[0]+ a_b*row4I[0]) + 0.5f;
		outI[1]= (ma_mb*row1I[1] + a_mb*row3I[1] + ma_b*row2I[1]+ a_b*row4I[1]) + 0.5f;
		outI[2]= (ma_mb*row1I[2] + a_mb*row3I[2] + ma_b*row2I[2]+ a_b*row4I[2]) + 0.5f;
		outI[3]= (ma_mb*row1I[3] + a_mb*row3I[3] + ma_b*row2I[3]+ a_b*row4I[3]) + 0.5f;
	}
}

/* function assumes out to be zero'ed, only does RGBA */
/* BILINEAR INTERPOLATION */

/* Note about wrapping, the u/v still needs to be within the image bounds,
 * just the interpolation is wrapped.
 * This the same as bilinear_interpolation_color except it wraps rather then using empty and emptyI */
void bilinear_interpolation_color_wrap(struct ImBuf *in, unsigned char *outI, float *outF, float u, float v)
{
	float *row1, *row2, *row3, *row4, a, b;
	unsigned char *row1I, *row2I, *row3I, *row4I;
	float a_b, ma_b, a_mb, ma_mb;
	int y1, y2, x1, x2;
	
	
	/* ImBuf in must have a valid rect or rect_float, assume this is alredy checked */

	x1= (int)floor(u);
	x2= (int)ceil(u);
	y1= (int)floor(v);
	y2= (int)ceil(v);

	// sample area entirely outside image? 
	if (x2<0 || x1>in->x-1 || y2<0 || y1>in->y-1) return;
	
	/* wrap interpolation pixels - main difference from bilinear_interpolation_color  */
	if(x1<0)x1= in->x+x1;
	if(y1<0)y1= in->y+y1;
	
	if(x2>=in->x)x2= x2-in->x;
	if(y2>=in->y)y2= y2-in->y;

	if (outF) {
		// sample including outside of edges of image 
		row1= (float *)in->rect_float + in->x * y1 * 4 + 4*x1;
		row2= (float *)in->rect_float + in->x * y2 * 4 + 4*x1;
		row3= (float *)in->rect_float + in->x * y1 * 4 + 4*x2;
		row4= (float *)in->rect_float + in->x * y2 * 4 + 4*x2;
		
		a= u-floor(u);
		b= v-floor(v);
		a_b= a*b; ma_b= (1.0f-a)*b; a_mb= a*(1.0f-b); ma_mb= (1.0f-a)*(1.0f-b);
		
		outF[0]= ma_mb*row1[0] + a_mb*row3[0] + ma_b*row2[0]+ a_b*row4[0];
		outF[1]= ma_mb*row1[1] + a_mb*row3[1] + ma_b*row2[1]+ a_b*row4[1];
		outF[2]= ma_mb*row1[2] + a_mb*row3[2] + ma_b*row2[2]+ a_b*row4[2];
		outF[3]= ma_mb*row1[3] + a_mb*row3[3] + ma_b*row2[3]+ a_b*row4[3];
	}
	if (outI) {
		// sample including outside of edges of image 
		row1I= (unsigned char *)in->rect + in->x * y1 * 4 + 4*x1;
		row2I= (unsigned char *)in->rect + in->x * y2 * 4 + 4*x1;
		row3I= (unsigned char *)in->rect + in->x * y1 * 4 + 4*x2;
		row4I= (unsigned char *)in->rect + in->x * y2 * 4 + 4*x2;
		
		a= u-floor(u);
		b= v-floor(v);
		a_b= a*b; ma_b= (1.0f-a)*b; a_mb= a*(1.0f-b); ma_mb= (1.0f-a)*(1.0f-b);
		
		/* need to add 0.5 to avoid rounding down (causes darken with the smear brush)
		 * tested with white images and this should not wrap back to zero */
		outI[0]= (ma_mb*row1I[0] + a_mb*row3I[0] + ma_b*row2I[0]+ a_b*row4I[0]) + 0.5f;
		outI[1]= (ma_mb*row1I[1] + a_mb*row3I[1] + ma_b*row2I[1]+ a_b*row4I[1]) + 0.5f;
		outI[2]= (ma_mb*row1I[2] + a_mb*row3I[2] + ma_b*row2I[2]+ a_b*row4I[2]) + 0.5f;
		outI[3]= (ma_mb*row1I[3] + a_mb*row3I[3] + ma_b*row2I[3]+ a_b*row4I[3]) + 0.5f;
	}
}

void bilinear_interpolation(ImBuf *in, ImBuf *out, float u, float v, int xout, int yout)
{
	
	unsigned char *outI = NULL;
	float *outF = NULL;
	
	if (in == NULL || (in->rect == NULL && in->rect_float == NULL)) return;
	
	pixel_from_buffer(out, &outI, &outF, xout, yout); /* gcc warns these could be uninitialized, but its ok */
	
	bilinear_interpolation_color(in, outI, outF, u, v);
}

/* function assumes out to be zero'ed, only does RGBA */
/* NEAREST INTERPOLATION */
void neareast_interpolation_color(struct ImBuf *in, unsigned char *outI, float *outF, float u, float v)
{
	float *dataF;
	unsigned char *dataI;
	int y1, x1;

	/* ImBuf in must have a valid rect or rect_float, assume this is alredy checked */
	
	x1= (int)(u);
	y1= (int)(v);

	// sample area entirely outside image? 
	if (x1<0 || x1>in->x-1 || y1<0 || y1>in->y-1) return;
	
	// sample including outside of edges of image 
	if (x1<0 || y1<0) {
		if (outI) {
			outI[0]= 0;
			outI[1]= 0;
			outI[2]= 0;
			outI[3]= 0;
		}
		if (outF) {
			outF[0]= 0.0f;
			outF[1]= 0.0f;
			outF[2]= 0.0f;
			outF[3]= 0.0f;
		}
	} else {
		dataI= (unsigned char *)in->rect + in->x * y1 * 4 + 4*x1;
		if (outI) {
			outI[0]= dataI[0];
			outI[1]= dataI[1];
			outI[2]= dataI[2];
			outI[3]= dataI[3];
		}
		dataF= in->rect_float + in->x * y1 * 4 + 4*x1;
		if (outF) {
			outF[0]= dataF[0];
			outF[1]= dataF[1];
			outF[2]= dataF[2];
			outF[3]= dataF[3];
		}
	}	
}

void neareast_interpolation(ImBuf *in, ImBuf *out, float x, float y, int xout, int yout)
{
	
	unsigned char *outI = NULL;
	float *outF = NULL;

	if (in == NULL || (in->rect == NULL && in->rect_float == NULL)) return;
	
	pixel_from_buffer(out, &outI, &outF, xout, yout); /* gcc warns these could be uninitialized, but its ok */
	
	neareast_interpolation_color(in, outI, outF, x, y);
}
