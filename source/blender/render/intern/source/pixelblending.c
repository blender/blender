/*
 * pixelblending.c
 *
 * Functions to blend pixels with or without alpha, in various formats
 * nzc - June 2000
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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * Contributor(s): Full recode, 2004-2006 Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <math.h>

/* global includes */
#include "BLI_arithb.h"
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
/* Debug/behaviour defines                                                   */
/* if defined: alpha blending with floats clips colour, as with shorts       */
/* #define RE_FLOAT_COLOUR_CLIPPING  */
/* if defined: alpha values are clipped                                      */
/* For now, we just keep alpha clipping. We run into thresholding and        */
/* blending difficulties otherwise. Be careful here.                         */
#define RE_ALPHA_CLIPPING



/* Threshold for a 'full' pixel: pixels with alpha above this level are      */
/* considered opaque This is the decimal value for 0xFFF0 / 0xFFFF           */
#define RE_FULL_COLOUR_FLOAT 0.9998
/* Threshold for an 'empty' pixel: pixels with alpha above this level are    */
/* considered completely transparent. This is the decimal value              */
/* for 0x000F / 0xFFFF                                                       */
#define RE_EMPTY_COLOUR_FLOAT 0.0002


/* ------------------------------------------------------------------------- */

void addAlphaOverFloat(float *dest, float *source)
{
    /* d = s + (1-alpha_s)d*/
    float c;
    float mul;
    
	mul= 1.0 - source[3];

	c= (mul*dest[0]) + source[0];
       dest[0]= c;
   
	c= (mul*dest[1]) + source[1];
       dest[1]= c;

	c= (mul*dest[2]) + source[2];
       dest[2]= c;

	c= (mul*dest[3]) + source[3];
       dest[3]= c;

}


/* ------------------------------------------------------------------------- */

void addAlphaUnderFloat(float *dest, float *source)
{
    float c;
    float mul;
    
    if( (-RE_EMPTY_COLOUR_FLOAT < dest[3])
        && (dest[3] <  RE_EMPTY_COLOUR_FLOAT) ) {	
        dest[0] = source[0];
        dest[1] = source[1];
        dest[2] = source[2];
        dest[3] = source[3];
        return;
    }

	mul= 1.0 - dest[3];

	c= (mul*source[0]) + dest[0];
       dest[0]= c;
   
	c= (mul*source[1]) + dest[1];
       dest[1]= c;

	c= (mul*source[2]) + dest[2];
       dest[2]= c;

	c= (mul*source[3]) + dest[3];
       dest[3]= c;

} 


/* ------------------------------------------------------------------------- */
void addalphaAddfacFloat(float *dest, float *source, char addfac)
{
    float m; /* weiging factor of destination */
    float c; /* intermediate colour           */

    /* Addfac is a number between 0 and 1: rescale */
    /* final target is to diminish the influence of dest when addfac rises */
    m = 1.0 - ( source[3] * ((255.0 - addfac) / 255.0));

    /* blend colours*/
    c= (m * dest[0]) + source[0];
#ifdef RE_FLOAT_COLOUR_CLIPPING
    if(c >= RE_FULL_COLOUR_FLOAT) dest[0] = RE_FULL_COLOUR_FLOAT; 
    else 
#endif
        dest[0]= c;
   
    c= (m * dest[1]) + source[1];
#ifdef RE_FLOAT_COLOUR_CLIPPING
    if(c >= RE_FULL_COLOUR_FLOAT) dest[1] = RE_FULL_COLOUR_FLOAT; 
    else 
#endif
        dest[1]= c;
    
    c= (m * dest[2]) + source[2];
#ifdef RE_FLOAT_COLOUR_CLIPPING
    if(c >= RE_FULL_COLOUR_FLOAT) dest[2] = RE_FULL_COLOUR_FLOAT; 
    else 
#endif
        dest[2]= c;

	c= (m * dest[3]) + source[3];
#ifdef RE_ALPHA_CLIPPING
	if(c >= RE_FULL_COLOUR_FLOAT) dest[3] = RE_FULL_COLOUR_FLOAT; 
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
	
	for(j=2; j>=0; j--) {
		
		a= j;
		
		val= *(fmask1[a] +maskand) + *(fmask2[a] +maskshift);
		if(val!=0.0) {
			rb1[0]+= val*r;
			rb1[1]+= val*g;
			rb1[2]+= val*b;
			rb1[3]+= val*al;
		}
		a+=3;
		
		val= *(fmask1[a] +maskand) + *(fmask2[a] +maskshift);
		if(val!=0.0) {
			rb2[0]+= val*r;
			rb2[1]+= val*g;
			rb2[2]+= val*b;
			rb2[3]+= val*al;
		}
		a+=3;
		
		val= *(fmask1[a] +maskand) + *(fmask2[a] +maskshift);
		if(val!=0.0) {
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

/* ------------------------------------------------------------------------- */
void addalphaAddFloat(float *dest, float *source)
{

	/* Makes me wonder whether this is required... */
	if( dest[3] < RE_EMPTY_COLOUR_FLOAT) {
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


/* ------------------------------------------------------------------------- */

/* ------------------------------------------------------------------------- */
/* Colour buffer related:                                                    */
/* This transforms the 4 inputvalues RE_COLBUFTYPE to a new value            */
/* It expects the values R.r.postigamma, R.r.postmul and R.r.postadd.         */
/* This is the standard transformation, more elaborate tools are for later.  */
/* ------------------------------------------------------------------------- */
void std_floatcol_to_charcol( float *buf, char *target)
{
	float col[3];
	
	float dither_value;
	
	dither_value = ((BLI_frand()-0.5)*R.r.dither_intensity)/256.0; 
	
	/* alpha */
	if((buf[3]+dither_value)<=0.0) target[3]= 0;
	else if((buf[3]+dither_value)>1.0) target[3]= 255;
	else target[3]= 255.0*(buf[3]+dither_value);
	
	if(R.r.postgamma==1.0) {
		/* r */
		col[0]= R.r.postmul*buf[0] + R.r.postadd + dither_value;
		/* g */
		col[1]= R.r.postmul*buf[1] + R.r.postadd + dither_value;
		/* b */
		col[2]= R.r.postmul*buf[2] + R.r.postadd + dither_value;
	}
	else {
		/* putting the postmul within the pow() gives an
		* easier control for the user, values from 1.0-2.0
		* are relevant then
		*/
		
		/* r */
		col[0]= pow(R.r.postmul*buf[0], R.r.postigamma) + R.r.postadd + dither_value;
		/* g */
		col[1]= pow( R.r.postmul*buf[1], R.r.postigamma) + R.r.postadd + dither_value;
		/* b */
		col[2]= pow(R.r.postmul*buf[2], R.r.postigamma) + R.r.postadd + dither_value;
	}
	
	if(R.r.posthue!=0.0 || R.r.postsat!=1.0) {
		float hsv[3];
		
		rgb_to_hsv(col[0], col[1], col[2], hsv, hsv+1, hsv+2);
		hsv[0]+= R.r.posthue;
		if(hsv[0]>1.0) hsv[0]-=1.0; else if(hsv[0]<0.0) hsv[0]+= 1.0;
		hsv[1]*= R.r.postsat;
		if(hsv[1]>1.0) hsv[1]= 1.0; else if(hsv[1]<0.0) hsv[1]= 0.0;
		hsv_to_rgb(hsv[0], hsv[1], hsv[2], col, col+1, col+2);
	}
	
	if(col[0]<=0.0) target[0]= 0;
	else if(col[0]>1.0) target[0]= 255;
	else target[0]= 255.0*col[0];
	
	if(col[1]<=0.0) target[1]= 0;
	else if(col[1]>1.0) target[1]= 255;
	else target[1]= 255.0*col[1];
	
	if(col[2]<=0.0) target[2]= 0;
	else if(col[2]>1.0) target[2]= 255;
	else target[2]= 255.0*col[2];
}

/* ----------------------------------------------------------------------------

Colour buffer related:

The colour buffer is a buffer of a single screen line. It contains        
four fields of type RE_COLBUFTYPE per pixel.

We can do several post-process steps. I would prefer to move them outside
the render module later on, but it's ok to leave it here for now. For the
time being, we have:
- post-process function
    Does some operations with the colours.
- Multiply with some factor
- Add constant offset
- Apply extra gamma correction (seems weird...)
- key-alpha correction
    Key alpha means 'un-applying' the alpha. For fully covered pixels, this
	operation has no effect.

- XXX WARNING! Added the inverse render gamma here, so this cannot be used external
	without setting Osa or Gamma flags off (ton)

---------------------------------------------------------------------------- */
/* used external! */
void transferColourBufferToOutput( float *buf, int y)
{
    /* Copy the contents of AColourBuffer3 to R.rectot + y * R.rectx */
    int x = 0;
//    char *target = (char*) (R.rectot + (y * R.rectx));
	
	/* Copy the first <R.rectx> pixels. We can do some more clipping on    */
	/* the z buffer, I think.                                                 */
	while (x < R.rectx) {
		
		
		/* invert gamma corrected additions */
		if(R.do_gamma) {
			buf[0] = invGammaCorrect(buf[0]);
			buf[1] = invGammaCorrect(buf[1]);
			buf[2] = invGammaCorrect(buf[2]);
		}			
		
//		std_floatcol_to_charcol(buf, target);
		
		/*
		 Key-alpha mode:
		 Need to un-apply alpha if alpha is non-full. For full alpha,
		 the operation doesn't have effect. Do this after the post-
		 processing, so we can still use the benefits of that.
		 
		 */
		
		if (R.r.alphamode == R_ALPHAKEY) {  
//			applyKeyAlphaCharCol(target);
		}				
		
//        target+=4;
        buf+=4;
        x++;
    }
}


/* eof pixelblending.c */


