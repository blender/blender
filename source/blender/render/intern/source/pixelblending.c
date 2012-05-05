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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * Contributor(s): Full recode, 2004-2006 Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/render/intern/source/pixelblending.c
 *  \ingroup render
 *
 * Functions to blend pixels with or without alpha, in various formats
 * nzc - June 2000
 */


#include <math.h>
#include <string.h>

/* global includes */
#include "BLI_math.h"
#include "BLI_rand.h"

/* own includes */
#include "render_types.h"
#include "renderpipeline.h"
#include "pixelblending.h"
#include "gammaCorrectionTables.h"

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
/* defined in pipeline.c, is hardcopy of active dynamic allocated Render */
/* only to be used here in this file, it's for speed */
extern struct Render R;
/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */


/* ------------------------------------------------------------------------- */
/* Debug/behavior defines                                                   */
/* if defined: alpha blending with floats clips color, as with shorts       */
/* #define RE_FLOAT_COLOR_CLIPPING  */
/* if defined: alpha values are clipped                                      */
/* For now, we just keep alpha clipping. We run into thresholding and        */
/* blending difficulties otherwise. Be careful here.                         */
#define RE_ALPHA_CLIPPING



/* Threshold for a 'full' pixel: pixels with alpha above this level are      */
/* considered opaque This is the decimal value for 0xFFF0 / 0xFFFF           */
#define RE_FULL_COLOR_FLOAT 0.9998f
/* Threshold for an 'empty' pixel: pixels with alpha above this level are    */
/* considered completely transparent. This is the decimal value              */
/* for 0x000F / 0xFFFF                                                       */
#define RE_EMPTY_COLOR_FLOAT 0.0002f


/* ------------------------------------------------------------------------- */

void addAlphaOverFloat(float *dest, float *source)
{
	/* d = s + (1-alpha_s)d*/
	float mul;

	mul= 1.0f - source[3];

	dest[0]= (mul*dest[0]) + source[0];
	dest[1]= (mul*dest[1]) + source[1];
	dest[2]= (mul*dest[2]) + source[2];
	dest[3]= (mul*dest[3]) + source[3];

}


/* ------------------------------------------------------------------------- */

void addAlphaUnderFloat(float *dest, float *source)
{
	float mul;

	mul= 1.0f - dest[3];

	dest[0]+= (mul*source[0]);
	dest[1]+= (mul*source[1]);
	dest[2]+= (mul*source[2]);
	dest[3]+= (mul*source[3]);
} 


/* ------------------------------------------------------------------------- */
void addalphaAddfacFloat(float *dest, float *source, char addfac)
{
	float m; /* weiging factor of destination */
	float c; /* intermediate color           */

	/* Addfac is a number between 0 and 1: rescale */
	/* final target is to diminish the influence of dest when addfac rises */
	m = 1.0f - ( source[3] * ((255 - addfac) / 255.0f));

	/* blend colors*/
	c= (m * dest[0]) + source[0];
#ifdef RE_FLOAT_COLOR_CLIPPING
	if (c >= RE_FULL_COLOR_FLOAT) dest[0] = RE_FULL_COLOR_FLOAT; 
	else 
#endif
		dest[0]= c;
   
	c= (m * dest[1]) + source[1];
#ifdef RE_FLOAT_COLOR_CLIPPING
	if (c >= RE_FULL_COLOR_FLOAT) dest[1] = RE_FULL_COLOR_FLOAT; 
	else 
#endif
		dest[1]= c;

	c= (m * dest[2]) + source[2];
#ifdef RE_FLOAT_COLOR_CLIPPING
	if (c >= RE_FULL_COLOR_FLOAT) dest[2] = RE_FULL_COLOR_FLOAT; 
	else 
#endif
		dest[2]= c;

	c= (m * dest[3]) + source[3];
#ifdef RE_ALPHA_CLIPPING
	if (c >= RE_FULL_COLOR_FLOAT) dest[3] = RE_FULL_COLOR_FLOAT; 
	else 
#endif
	dest[3]= c;

}


/* ------------------------------------------------------------------------- */

/* filtered adding to scanlines */
void add_filt_fmask(unsigned int mask, float *col, float *rowbuf, int row_w)
{
	/* calc the value of mask */
	float **fmask1= R.samples->fmask1, **fmask2=R.samples->fmask2;
	float *rb1, *rb2, *rb3;
	float val, r, g, b, al;
	unsigned int a, maskand, maskshift;
	int j;
	
	r= col[0];
	g= col[1];
	b= col[2];
	al= col[3];
	
	rb2= rowbuf-4;
	rb3= rb2-4*row_w;
	rb1= rb2+4*row_w;
	
	maskand= (mask & 255);
	maskshift= (mask >>8);
	
	for (j=2; j>=0; j--) {
		
		a= j;
		
		val= *(fmask1[a] +maskand) + *(fmask2[a] +maskshift);
		if (val!=0.0f) {
			rb1[0]+= val*r;
			rb1[1]+= val*g;
			rb1[2]+= val*b;
			rb1[3]+= val*al;
		}
		a+=3;
		
		val= *(fmask1[a] +maskand) + *(fmask2[a] +maskshift);
		if (val!=0.0f) {
			rb2[0]+= val*r;
			rb2[1]+= val*g;
			rb2[2]+= val*b;
			rb2[3]+= val*al;
		}
		a+=3;
		
		val= *(fmask1[a] +maskand) + *(fmask2[a] +maskshift);
		if (val!=0.0f) {
			rb3[0]+= val*r;
			rb3[1]+= val*g;
			rb3[2]+= val*b;
			rb3[3]+= val*al;
		}
		
		rb1+= 4;
		rb2+= 4;
		rb3+= 4;
	}
}


void mask_array(unsigned int mask, float filt[][3])
{
	float **fmask1= R.samples->fmask1, **fmask2=R.samples->fmask2;
	unsigned int maskand= (mask & 255);
	unsigned int maskshift= (mask >>8);
	int a, j;
	
	for (j=2; j>=0; j--) {
		
		a= j;
		
		filt[2][2-j]= *(fmask1[a] +maskand) + *(fmask2[a] +maskshift);

		a+=3;
		
		filt[1][2-j]= *(fmask1[a] +maskand) + *(fmask2[a] +maskshift);
		
		a+=3;
		
		filt[0][2-j]= *(fmask1[a] +maskand) + *(fmask2[a] +maskshift);
	}
}


/**
 * Index ordering, scanline based:
 *
 *      ---    ---   ---
 *     | 2,0 | 2,1 | 2,2 |
 *      ---    ---   ---
 *     | 1,0 | 1,1 | 1,2 |
 *      ---    ---   ---
 *     | 0,0 | 0,1 | 0,2 |
 *      ---    ---   ---
 */

void add_filt_fmask_coord(float filt[][3], float *col, float *rowbuf, int row_w, int col_h, int x, int y)
{
	float *fpoin[3][3];
	float val, r, g, b, al, lfilt[3][3];
	
	r= col[0];
	g= col[1];
	b= col[2];
	al= col[3];
	
	memcpy(lfilt, filt, sizeof(lfilt));
	
	fpoin[0][1]= rowbuf-4*row_w;
	fpoin[1][1]= rowbuf;
	fpoin[2][1]= rowbuf+4*row_w;
	
	fpoin[0][0]= fpoin[0][1] - 4;
	fpoin[1][0]= fpoin[1][1] - 4;
	fpoin[2][0]= fpoin[2][1] - 4;
	
	fpoin[0][2]= fpoin[0][1] + 4;
	fpoin[1][2]= fpoin[1][1] + 4;
	fpoin[2][2]= fpoin[2][1] + 4;
	
	if (y==0) {
		fpoin[0][0]= fpoin[1][0];
		fpoin[0][1]= fpoin[1][1];
		fpoin[0][2]= fpoin[1][2];
		/* filter needs the opposite value yes! */
		lfilt[0][0]= filt[2][0];
		lfilt[0][1]= filt[2][1];
		lfilt[0][2]= filt[2][2];
	}
	else if (y==col_h-1) {
		fpoin[2][0]= fpoin[1][0];
		fpoin[2][1]= fpoin[1][1];
		fpoin[2][2]= fpoin[1][2];
		
		lfilt[2][0]= filt[0][0];
		lfilt[2][1]= filt[0][1];
		lfilt[2][2]= filt[0][2];
	}
	
	if (x==0) {
		fpoin[2][0]= fpoin[2][1];
		fpoin[1][0]= fpoin[1][1];
		fpoin[0][0]= fpoin[0][1];
		
		lfilt[2][0]= filt[2][2];
		lfilt[1][0]= filt[1][2];
		lfilt[0][0]= filt[0][2];
	}
	else if (x==row_w-1) {
		fpoin[2][2]= fpoin[2][1];
		fpoin[1][2]= fpoin[1][1];
		fpoin[0][2]= fpoin[0][1];
		
		lfilt[2][2]= filt[2][0];
		lfilt[1][2]= filt[1][0];
		lfilt[0][2]= filt[0][0];
	}
	
	
	/* loop unroll */
#define MASKFILT(i, j) 	val= lfilt[i][j]; if (val!=0.0f) {float *fp= fpoin[i][j]; fp[0]+= val*r; fp[1]+= val*g; fp[2]+= val*b; fp[3]+= val*al; }
	
	MASKFILT(0, 0)
	MASKFILT(0, 1)
	MASKFILT(0, 2)
	MASKFILT(1, 0)
	MASKFILT(1, 1)
	MASKFILT(1, 2)
	MASKFILT(2, 0)
	MASKFILT(2, 1)
	MASKFILT(2, 2)
}

void add_filt_fmask_pixsize(unsigned int mask, float *in, float *rowbuf, int row_w, int pixsize)
{
	/* calc the value of mask */
	float **fmask1= R.samples->fmask1, **fmask2=R.samples->fmask2;
	float *rb1, *rb2, *rb3;
	float val;
	unsigned int a, maskand, maskshift;
	int i, j;
	
	rb2= rowbuf-pixsize;
	rb3= rb2-pixsize*row_w;
	rb1= rb2+pixsize*row_w;
	
	maskand= (mask & 255);
	maskshift= (mask >>8);
	
	for (j=2; j>=0; j--) {
		
		a= j;
		
		val= *(fmask1[a] +maskand) + *(fmask2[a] +maskshift);
		if (val!=0.0f) {
			for (i= 0; i<pixsize; i++)
				rb1[i]+= val*in[i];
		}
		a+=3;
		
		val= *(fmask1[a] +maskand) + *(fmask2[a] +maskshift);
		if (val!=0.0f) {
			for (i= 0; i<pixsize; i++)
				rb2[i]+= val*in[i];
		}
		a+=3;
		
		val= *(fmask1[a] +maskand) + *(fmask2[a] +maskshift);
		if (val!=0.0f) {
			for (i= 0; i<pixsize; i++)
				rb3[i]+= val*in[i];
		}
		
		rb1+= pixsize;
		rb2+= pixsize;
		rb3+= pixsize;
	}
}

/* ------------------------------------------------------------------------- */
void addalphaAddFloat(float *dest, float *source)
{

	/* Makes me wonder whether this is required... */
	if ( dest[3] < RE_EMPTY_COLOR_FLOAT) {
		dest[0] = source[0];
		dest[1] = source[1];
		dest[2] = source[2];
		dest[3] = source[3];
		return;
	}

	/* no clipping! */
	dest[0] = dest[0]+source[0];
	dest[1] = dest[1]+source[1];
	dest[2] = dest[2]+source[2];
	dest[3] = dest[3]+source[3];

}


/* ---------------------------------------------------------------------------- */


/* eof pixelblending.c */
