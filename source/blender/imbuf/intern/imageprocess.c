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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
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
	int size, do_float=0;
	unsigned char rt, *cp = (unsigned char *)ibuf->rect;
	float rtf, *cpf = ibuf->rect_float;
	
	if (ibuf->rect_float)  do_float = 1;
	size = ibuf->x * ibuf->y;

	while(size-- > 0) {
		rt= cp[0];
		cp[0]= cp[3];
		cp[3]= rt;
		rt= cp[1];
		cp[1]= cp[2];
		cp[2]= rt;
		cp+= 4;
		if (do_float) {
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
	float aux;
	aux=(float)(1.0f/6.0f)*( pow( MAX2(k+2.0f,0) , 3.0f ) - 4.0f * pow( MAX2(k+1.0f,0) , 3.0f ) + 6.0f * pow( MAX2(k,0) , 3.0f ) - 4.0f * pow( MAX2(k-1.0f,0) , 3.0f));
	return aux ;
}

void bicubic_interpolation(ImBuf *in, ImBuf *out, float x, float y, int xout, int yout)
{
	int i,j,n,m,x1,y1;
	unsigned char *dataI,*outI;
	float a,b, outR,outG,outB,outA,*dataF,*outF;
	int do_rect, do_float;

	if (in == NULL) return;
	if (in->rect == NULL && in->rect_float == NULL) return;

	do_rect= (out->rect != NULL);
	do_float= (out->rect_float != NULL);

	i= (int)floor(x);
	j= (int)floor(y);
	a= x - i;
	b= y - j;

	outR= 0.0f;
	outG= 0.0f;
	outB= 0.0f;
	outA= 0.0f;
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
	if (do_rect) {
		outI= (unsigned char *)out->rect + out->x * yout * 4 + 4*xout;
		outI[0]= (int)outR;
		outI[1]= (int)outG;
		outI[2]= (int)outB;
		outI[3]= (int)outA;
	}
	if (do_float) {
		outF= (float *)out->rect_float + out->x * yout * 4 + 4*xout;
		outF[0]= outR;
		outF[1]= outG;
		outF[2]= outB;
		outF[3]= outA;
	}
}

/* function assumes out to be zero'ed, only does RGBA */
/* BILINEAR INTERPOLATION */
void bilinear_interpolation(ImBuf *in, ImBuf *out, float u, float v, int xout, int yout)
{
	float *row1, *row2, *row3, *row4, a, b, *outF;
	unsigned char *row1I, *row2I, *row3I, *row4I, *outI;
	float a_b, ma_b, a_mb, ma_mb;
	float empty[4]= {0.0f, 0.0f, 0.0f, 0.0f};
	unsigned char emptyI[4]= {0, 0, 0, 0};
	int y1, y2, x1, x2;
	int do_rect, do_float;

	if (in==NULL) return;
	if (in->rect==NULL && in->rect_float==NULL) return;

	do_rect= (out->rect != NULL);
	do_float= (out->rect_float != NULL);

	x1= (int)floor(u);
	x2= (int)ceil(u);
	y1= (int)floor(v);
	y2= (int)ceil(v);

	// sample area entirely outside image? 
	if (x2<0 || x1>in->x-1 || y2<0 || y1>in->y-1) return;

	if (do_rect)
		outI=(unsigned char *)out->rect + out->x * yout * 4 + 4*xout;
	else
		outI= NULL;
	if (do_float)
		outF=(float *)out->rect_float + out->x * yout * 4 + 4*xout;
	else	
		outF= NULL;

	if (do_float) {
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
	if (do_rect) {
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
		
		outI[0]= ma_mb*row1I[0] + a_mb*row3I[0] + ma_b*row2I[0]+ a_b*row4I[0];
		outI[1]= ma_mb*row1I[1] + a_mb*row3I[1] + ma_b*row2I[1]+ a_b*row4I[1];
		outI[2]= ma_mb*row1I[2] + a_mb*row3I[2] + ma_b*row2I[2]+ a_b*row4I[2];
		outI[3]= ma_mb*row1I[3] + a_mb*row3I[3] + ma_b*row2I[3]+ a_b*row4I[3];
	}
}

/* function assumes out to be zero'ed, only does RGBA */
/* NEAREST INTERPOLATION */
void neareast_interpolation(ImBuf *in, ImBuf *out, float u, float v,int xout, int yout)
{
	float *outF,*dataF;
	unsigned char *dataI,*outI;
	int y1, x1;
	int do_rect, do_float;

	if (in==NULL) return;
	if (in->rect==NULL && in->rect_float==NULL) return;

	do_rect= (out->rect != NULL);
	do_float= (out->rect_float != NULL);

	x1= (int)(u);
	y1= (int)(v);

	if (do_rect)
		outI=(unsigned char *)out->rect + out->x * yout * 4 + 4*xout;
	else
		outI= NULL;
	if (do_float)
		outF=(float *)out->rect_float + out->x * yout * 4 + 4*xout;
	else
		outF= NULL;

	// sample area entirely outside image? 
	if (x1<0 || x1>in->x-1 || y1<0 || y1>in->y-1) return;
	
	// sample including outside of edges of image 
	if (x1<0 || y1<0) {
		if (do_rect) {
			outI[0]= 0;
			outI[1]= 0;
			outI[2]= 0;
			outI[3]= 0;
		}
		if (do_float) {
			outF[0]= 0.0f;
			outF[1]= 0.0f;
			outF[2]= 0.0f;
			outF[3]= 0.0f;
		}
	} else {
		dataI= (unsigned char *)in->rect + in->x * y1 * 4 + 4*x1;
		if (do_rect) {
			outI[0]= dataI[0];
			outI[1]= dataI[1];
			outI[2]= dataI[2];
			outI[3]= dataI[3];
		}
		dataF= in->rect_float + in->x * y1 * 4 + 4*x1;
		if (do_float) {
			outF[0]= dataF[0];
			outF[1]= dataF[1];
			outF[2]= dataF[2];
			outF[3]= dataF[3];
		}
	}	
}
