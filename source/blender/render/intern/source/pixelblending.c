/*
 * pixelblending.c
 *
 * Functions to blend pixels with or without alpha, in various formats
 * nzc - June 2000
 *
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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

#include <math.h>

/* global includes */
#include "render.h"
#include "render_intern.h"

/* local includes */
#include "vanillaRenderPipe_types.h"

/* own includes */
#include "pixelblending_types.h"
#include "pixelblending.h"
#include "gammaCorrectionTables.h"

/* externals */
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

/* ------------------------------------------------------------------------- */
/* Debug/behaviour defines                                                   */
/* if defined: alpha blending with floats clips colour, as with shorts       */
/* #define RE_FLOAT_COLOUR_CLIPPING  */
/* if defined: alpha values are clipped                                      */
/* For now, we just keep alpha clipping. We run into thresholding and        */
/* blending difficulties otherwise. Be careful here.                         */
#define RE_ALPHA_CLIPPING

/* functions --------------------------------------------------------------- */

/*
  One things about key-alpha is that simply dividing by the alpha will
  sometimes cause 'overflows' in that the pixel colours will be shot
  way over full colour. This should be caught, and subsequently, the
  operation will end up modifying the alpha as well.

  Actually, when the starting colour is premul, it shouldn't overflow
  ever. Strange thing is that colours keep overflowing...

*/
void applyKeyAlphaCharCol(char* target) {

	if ((!(target[3] == 0))
		|| (target[3] == 255)) {
		/* else: nothing to do */
		/* check whether div-ing is enough */
		float cf[4];
		cf[0] = target[0]/target[3];
		cf[1] = target[1]/target[3];
		cf[2] = target[2]/target[3];
		if ((cf[0] <= 1.0) && (cf[1] <= 1.0) && (cf[2] <= 1.0)) {
			/* all colours remain properly scaled? */
			/* scale to alpha */
			cf[0] = (float) target[0] * (255.0/ (float)target[3]);
			cf[1] = (float) target[1] * (255.0/ (float)target[3]);
			cf[2] = (float) target[2] * (255.0/ (float)target[3]);

			/* Clipping is important. */
			target[0] = (cf[0] > 255.0 ? 255 : (char) cf[0]);
			target[1] = (cf[1] > 255.0 ? 255 : (char) cf[1]);
			target[2] = (cf[2] > 255.0 ? 255 : (char) cf[2]);
			
		} else {
			/* shouldn't happen! we were premul, remember? */
/* should go to error handler: 			printf("Non-premul colour detected\n"); */
		}
	}

} /* end of void applyKeyAlphaCharCol(char* target) */

/* ------------------------------------------------------------------------- */

void addAddSampColF(float *sampvec, float *source, int mask, int osaNr, 
                    char addfac)
{
	int a;
	
	for(a=0; a < osaNr; a++) {
		if(mask & (1<<a)) addalphaAddfacFloat(sampvec, source, addfac);
		sampvec+= 4;
	}
} /* end of void addAddSampColF(float, float, int, int) */

/* ------------------------------------------------------------------------- */

void addOverSampColF(float *sampvec, float *source, int mask, int osaNr)
{
	int a;
	
	for(a=0; a < osaNr; a++) {
		if(mask & (1<<a)) addAlphaOverFloat(sampvec, source);
		sampvec+= 4;
	}
} /* end of void addOverSampColF(float, float, int, int) */

/* ------------------------------------------------------------------------- */

int addUnderSampColF(float *sampvec, float *source, int mask, int osaNr)
{
	int a, retval = osaNr;
	
	for(a=0; a < osaNr; a++) {
		if(mask & (1<<a)) addAlphaUnderFloat(sampvec, source);
		if(sampvec[3] > RE_FULL_COLOUR_FLOAT) retval--;
		sampvec+= 4;
	}
	return retval;
} /* end of int addToSampColF(float, float, int, int) */

/* ------------------------------------------------------------------------- */

int addToSampCol(unsigned short *sampcol, unsigned short *shortcol, int mask, int osaNr)
{
	int a, retval = osaNr;
	
	for(a=0; a < osaNr; a++) {
		if(mask & (1<<a)) addAlphaUnderShort(sampcol, shortcol);
		if(sampcol[3]>0xFFF0) retval--;
		sampcol+= 4;
	}
	return retval;
} /* end of int addToSampCol(unsigned short, uhost, int, int) */

/* ------------------------------------------------------------------------- */

int addtosampcol(unsigned short *sampcol, unsigned short *shortcol, int mask)
{
	int a, retval = R.osa;
	
	for(a=0; a < R.osa; a++) {
		if(mask & (1<<a)) addAlphaUnderShort(sampcol, shortcol);
		if(sampcol[3]>0xFFF0) retval--;
		sampcol+= 4;
	}
	return retval;
} /* end of int addtosampcol(unsigned short *sampcol, unsigned short *shortcol, int mask) */

/* ------------------------------------------------------------------------- */

void addAlphaOverShort(unsigned short *doel, unsigned short *bron)   /* vult bron over doel in met alpha van bron */
{
	unsigned int c;
	unsigned int mul;

	if( doel[3]==0 || bron[3]>=0xFFF0) {	/* is getest, scheelt veel */
		*((unsigned int *)doel)= *((unsigned int *)bron);
		*((unsigned int *)(doel+2))= *((unsigned int *)(bron+2));
		return;
	}

	mul= 0xFFFF-bron[3];

	c= ((mul*doel[0])>>16)+bron[0];
	if(c>=0xFFF0) doel[0]=0xFFF0; 
	else doel[0]= c;
	c= ((mul*doel[1])>>16)+bron[1];
	if(c>=0xFFF0) doel[1]=0xFFF0; 
	else doel[1]= c;
	c= ((mul*doel[2])>>16)+bron[2];
	if(c>=0xFFF0) doel[2]=0xFFF0; 
	else doel[2]= c;
	c= ((mul*doel[3])>>16)+bron[3];
	if(c>=0xFFF0) doel[3]=0xFFF0; 
	else doel[3]= c;

} /* end of void addAlphaOverShort(unsigned short *doel, unsigned short *bron) */

/* ------------------------------------------------------------------------- */

void addAlphaUnderShort(unsigned short *doel, unsigned short *bron)   /* vult bron onder doel in met alpha van doel */
{
	unsigned int c;
	unsigned int mul;

	if(doel[3]>=0xFFF0) return;
	if( doel[3]==0 ) {	/* is getest, scheelt veel */
		*((unsigned int *)doel)= *((unsigned int *)bron);
		*((unsigned int *)(doel+2))= *((unsigned int *)(bron+2));
		return;
	}

	mul= 0xFFFF-doel[3];

	c= ((mul*bron[0])>>16)+doel[0];
	if(c>=0xFFF0) doel[0]=0xFFF0; 
	else doel[0]= c;
	c= ((mul*bron[1])>>16)+doel[1];
	if(c>=0xFFF0) doel[1]=0xFFF0; 
	else doel[1]= c;
	c= ((mul*bron[2])>>16)+doel[2];
	if(c>=0xFFF0) doel[2]=0xFFF0;
	else doel[2]= c;
	c= ((mul*bron[3])>>16)+doel[3];
	if(c>=0xFFF0) doel[3]=0xFFF0;
	else doel[3]= c;

} /* end of void addAlphaUnderShort(unsigned short *doel, unsigned short *bron) */
  
/* ------------------------------------------------------------------------- */

void addAlphaOverFloat(float *dest, float *source)
{
    /* d = s + (1-alpha_s)d*/
    float c;
    float mul;
    
    /* I may want to disable this clipping */
#ifdef RE_FLOAT_COLOUR_CLIPPING
    if( /*  (-RE_FULL_COLOUR_FLOAT < source[3]) */
/*          && */ (source[3] >  RE_FULL_COLOUR_FLOAT) ) {	/* is getest, scheelt veel */
        dest[0] = source[0];
        dest[1] = source[1];
        dest[2] = source[2];
        dest[3] = source[3];
        return;
    }
#endif

	mul= 1.0 - source[3];

	c= (mul*dest[0]) + source[0];
#ifdef RE_FLOAT_COLOUR_CLIPPING
	if(c >= RE_FULL_COLOUR_FLOAT) dest[0] = RE_UNITY_COLOUR_FLOAT; 
	else 
#endif
       dest[0]= c;
   
	c= (mul*dest[1]) + source[1];
#ifdef RE_FLOAT_COLOUR_CLIPPING
	if(c >= RE_FULL_COLOUR_FLOAT) dest[1] = RE_UNITY_COLOUR_FLOAT; 
	else 
#endif
       dest[1]= c;

	c= (mul*dest[2]) + source[2];
#ifdef RE_FLOAT_COLOUR_CLIPPING
	if(c >= RE_FULL_COLOUR_FLOAT) dest[2] = RE_UNITY_COLOUR_FLOAT; 
	else 
#endif
       dest[2]= c;

	c= (mul*dest[3]) + source[3];
#ifdef RE_ALPHA_CLIPPING
	if(c >= RE_FULL_COLOUR_FLOAT) dest[3] = RE_UNITY_COLOUR_FLOAT; 
	else 
#endif
       dest[3]= c;

} /* end of void addAlphaOverFloat(float *doel, float *bron) */



/* ------------------------------------------------------------------------- */

void addAlphaUnderFloat(float *dest, float *source)
{
    float c;
    float mul;
    
    /* I may want to disable this clipping */
#ifdef RE_FLOAT_COLOUR_CLIPPING
    if( dest[3] >= RE_FULL_COLOUR_FLOAT) return;
#endif
    if( (-RE_EMPTY_COLOUR_FLOAT < dest[3])
        && (dest[3] <  RE_EMPTY_COLOUR_FLOAT) ) {	/* is getest, scheelt veel */
        dest[0] = source[0];
        dest[1] = source[1];
        dest[2] = source[2];
        dest[3] = source[3];
        return;
    }

	mul= 1.0 - dest[3];

	c= (mul*source[0]) + dest[0];
#ifdef RE_FLOAT_COLOUR_CLIPPING
	if(c >= RE_FULL_COLOUR_FLOAT) dest[0] = RE_UNITY_COLOUR_FLOAT; 
	else 
#endif
       dest[0]= c;
   
	c= (mul*source[1]) + dest[1];
#ifdef RE_FLOAT_COLOUR_CLIPPING
	if(c >= RE_FULL_COLOUR_FLOAT) dest[1] = RE_UNITY_COLOUR_FLOAT; 
	else 
#endif
       dest[1]= c;

	c= (mul*source[2]) + dest[2];
#ifdef RE_FLOAT_COLOUR_CLIPPING
	if(c >= RE_FULL_COLOUR_FLOAT) dest[2] = RE_UNITY_COLOUR_FLOAT; 
	else 
#endif
       dest[2]= c;

	c= (mul*source[3]) + dest[3];
#ifdef RE_ALPHA_CLIPPING
	if(c >= RE_FULL_COLOUR_FLOAT) dest[3] = RE_UNITY_COLOUR_FLOAT; 
	else 
#endif
       dest[3]= c;

} /* end of void addAlphaUnderFloat(float *doel, float *bron) */

/* ------------------------------------------------------------------------- */

void cpShortColV2CharColV(unsigned short *source, char *dest)
{
    dest[0] = source[0]>>8;
    dest[1] = source[1]>>8;
    dest[2] = source[2]>>8;
    dest[3] = source[3]>>8;
} /* end of void cpShortColV2CharColV(unsigned short *source, char *dest) */
/* ------------------------------------------------------------------------- */

void cpCharColV2ShortColV(char *source, unsigned short *dest)
{
    dest[0] = source[0]<<8;
    dest[1] = source[1]<<8;
    dest[2] = source[2]<<8;
    dest[3] = source[3]<<8;
} /* end of void cpShortColV2CharColV(char *source, unsigned short *dest) */

/* ------------------------------------------------------------------------- */

void cpIntColV2CharColV(unsigned int *source, char *dest)
{
    dest[0] = source[0]>>24;
    dest[1] = source[1]>>24;
    dest[2] = source[2]>>24;
    dest[3] = source[3]>>24;
} /* end of void cpIntColV2CharColV(unsigned int *source, char *dest) */

/* ------------------------------------------------------------------------- */

void cpCharColV2FloatColV(char *source, float *dest)
{
	/* What about endianness? Might be caught at this level :) */
    dest[0] = source[0]/255.0;  
    dest[1] = source[1]/255.0;  
    dest[2] = source[2]/255.0;
    dest[3] = source[3]/255.0;
} /* end of void cpCharColV2FloatColV(char *source, float *dest) */
/* ------------------------------------------------------------------------- */

void cpShortColV2FloatColV(unsigned short *source, float *dest)
{
    dest[0] = source[0]/65535.0;  
    dest[1] = source[1]/65535.0;  
    dest[2] = source[2]/65535.0;
    dest[3] = source[3]/65535.0;
} /* end of void cpShortColV2FloatColV(char *source, float *dest) */

/* ------------------------------------------------------------------------- */

void cpFloatColV2CharColV(float* source, char *dest)
{
  /* can't this be done more efficient? hope the conversions are correct... */
  if (source[0] < 0.0)      dest[0] = 0;
  else if (source[0] > 1.0) dest[0] = 255;
  else dest[0] = (char) (source[0] * 255.0);

  if (source[1] < 0.0)      dest[1] = 0;
  else if (source[1] > 1.0) dest[1] = 255;
  else dest[1] = (char) (source[1] * 255.0);

  if (source[2] < 0.0)      dest[2] = 0;
  else if (source[2] > 1.0) dest[2] = 255;
  else dest[2] = (char) (source[2] * 255.0);

  if (source[3] < 0.0)      dest[3] = 0;
  else if (source[3] > 1.0) dest[3] = 255;
  else dest[3] = (char) (source[3] * 255.0);

} /* end of void cpFloatColV2CharColV(float* source, char *dest) */

/* ------------------------------------------------------------------------- */

void cpShortColV(unsigned short *source, unsigned short *dest)
{
    dest[0] = source[0];
    dest[1] = source[1];
    dest[2] = source[2];
    dest[3] = source[3];
} /* end of void cpShortColV(unsigned short *source, unsigned short *dest) */

/* ------------------------------------------------------------------------- */
void cpFloatColV(float *source, float *dest)
{
    dest[0] = source[0];
    dest[1] = source[1];
    dest[2] = source[2];
    dest[3] = source[3];
} /* end of void cpFloatColV(float *source, float *dest) */

/* ------------------------------------------------------------------------- */

void cpCharColV(char *source, char *dest)
{
    dest[0] = source[0];
    dest[1] = source[1];
    dest[2] = source[2];
    dest[3] = source[3];
} /* end of void cpCharColV(char *source, char *dest) */

/* ------------------------------------------------------------------------- */
void addalphaAddfacFloat(float *dest, float *source, char addfac)
  /* doel= bron over doel  */
{
    float m; /* weiging factor of destination */
    float c; /* intermediate colour           */

    /* 1. copy source straight away if dest has zero alpha */
	/* 2. copy dest straight away if dest has full alpha   */
	/* I am not sure whether (2) is correct. It seems to   */
	/* me that this should not happen if float colours     */
	/* aren't clipped at 1.0 .                             */
	/* I'll keep the code, but disabled....                */
    if ( (dest[3] < RE_EMPTY_COLOUR_FLOAT) 
		/*   || source[3] > RE_FULL_COLOUR_FLOAT */ ) {
        dest[0] = source[0];
        dest[1] = source[1];
        dest[2] = source[2];
        dest[3] = source[3];
        return;
    }

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

	c= dest[3] + source[3];
#ifdef RE_ALPHA_CLIPPING
	if(c >= RE_FULL_COLOUR_FLOAT) dest[3] = RE_FULL_COLOUR_FLOAT; 
	else 
#endif
       dest[3]= c;

} /* end of void addalphaAddfacFloat(unsigned short *doel, unsigned short *bron, char addfac_help) */

/* ------------------------------------------------------------------------- */

void addalphaAddfacShort(unsigned short *doel, unsigned short *bron, char addfac)
  /* doel= bron over doel  */
{
    float m; /* weiging factor of destination */
    float c; /* intermediate colour           */

    /* 1. copy bron straight away if doel has zero alpha */
    if( doel[3] == 0) {
        *((unsigned int *)doel)     = *((unsigned int *)bron);
        *((unsigned int *)(doel+2)) = *((unsigned int *)(bron+2));
        return;
    }
    
    /* Addfac is a number between 0 and 1: rescale */
    /* final target is to diminish the influence of dest when addfac rises */
    m = 1.0 - ( bron[3] * ((255.0 - addfac) / 255.0));

    /* blend colours*/
    c = (m * doel[0]) + bron[0];
    if( c > 65535.0 ) doel[0]=65535; 
    else doel[0] = floor(c);
    c = (m * doel[1]) + bron[1];
    if( c > 65535.0 ) doel[1]=65535; 
    else doel[1] = floor(c);
    c = (m * doel[2]) + bron[2];
    if( c > 65535.0 ) doel[2]=65535; 
    else doel[2] = floor(c);

    c = doel[3] + bron[3];
    if(c > 65535.0) doel[3] = 65535; 
    else doel[3]=  floor(c);

} /* end of void addalphaAddfacShort(unsigned short *doel, unsigned short *bron, char addfac_help) */

/* ------------------------------------------------------------------------- */

void addHaloToHaloShort(unsigned short *d, unsigned short *s)
{
    /*  float m; */ /* weiging factor of destination */
    float c[4]; /* intermediate colour           */
    float rescale = 1.0;

    /* 1. copy <s> straight away if <d> has zero alpha */
    if( d[3] == 0) {
        *((unsigned int *) d)      = *((unsigned int *) s);
        *((unsigned int *)(d + 2)) = *((unsigned int *)(s + 2));
        return;
    }

    /* 2. halo blending  */
    /* no blending, just add */
    c[0] = s[0] + d[0];
    c[1] = s[1] + d[1];
    c[2] = s[2] + d[2];
    c[3] = s[3] + d[3];
    /* One thing that may happen is that this pixel is over-saturated with light - */
    /* i.e. too much light comes out, and the pixel is clipped. Currently, this    */
    /* leads to artifacts such as overproportional undersampling of background     */
    /* colours.                                                                    */
    /* Compensating for over-saturation:                                           */
    /* - increase alpha                                                            */
    /* - increase alpha and rescale colours                                        */

    /* let's try alpha increase and clipping */

    /* calculate how much rescaling we need */
    if( c[0] > 65535.0 ) { 
      rescale *= c[0] /65535.0;
      d[0] = 65535; 
    } else d[0] = floor(c[0]);
    if( c[1] > 65535.0 ) { 
      rescale *= c[1] /65535.0;
      d[1] = 65535; 
    } else d[1] = floor(c[1]);
    if( c[2] > 65535.0 ) { 
      rescale *= c[2] /65535.0;
      d[2] = 65535; 
    } else d[2] = floor(c[2]);

    /* a bit too hefty I think */
    c[3] *= rescale;

    if( c[3] > 65535.0 ) d[3] = 65535; else d[3]=  floor(c[3]);

} /* end of void addHaloToHaloShort(unsigned short *dest, unsigned short *source, char addfac) */

/* ------------------------------------------------------------------------- */

void sampleShortColV2ShortColV(unsigned short *sample, unsigned short *dest, int osaNr)
{
    unsigned int intcol[4] = {0};
    unsigned short *scol = sample; 
    int a = 0;
    
    for(a=0; a < osaNr; a++, scol+=4) {
        intcol[0]+= scol[0]; intcol[1]+= scol[1];
        intcol[2]+= scol[2]; intcol[3]+= scol[3];
    }
    
    /* Now normalise the integrated colour. It is guaranteed */
    /* to be correctly bounded.                              */
    dest[0]= intcol[0]/osaNr;
    dest[1]= intcol[1]/osaNr;
    dest[2]= intcol[2]/osaNr;
    dest[3]= intcol[3]/osaNr;
    
} /* end of void sampleShortColVToShortColV(unsigned short *sample, unsigned short *dest) */


/* ------------------------------------------------------------------------- */

void sampleFloatColV2FloatColV(float *sample, float *dest, int osaNr)
{
    float intcol[4] = {0};
    float *scol = sample; 
    int   a = 0;

	if (doGamma()) {
		/* use a LUT and interpolation to do the gamma correction */
		for(a=0; a < osaNr; a++, scol+=4) {
			intcol[0] += gammaCorrect(scol[0]); 
			intcol[1] += gammaCorrect(scol[1]); 
			intcol[2] += gammaCorrect(scol[2]); 
			intcol[3] += scol[3];
		}

		/* renormalise */
		intcol[0] /= osaNr;
		intcol[1] /= osaNr;
		intcol[2] /= osaNr;
		intcol[3] /= osaNr;

	/* back to pixel values */
		dest[0] = invGammaCorrect(intcol[0]);
		dest[1] = invGammaCorrect(intcol[1]);
		dest[2] = invGammaCorrect(intcol[2]);
		dest[3] = intcol[3];
	} else {
		/* no gamma */
		for(a=0; a < osaNr; a++, scol+=4) {
			intcol[0] += scol[0]; intcol[1] += scol[1];
			intcol[2] += scol[2]; intcol[3] += scol[3];
		}
    
		dest[0]= intcol[0]/osaNr;
		dest[1]= intcol[1]/osaNr;
		dest[2]= intcol[2]/osaNr;
		dest[3]= intcol[3]/osaNr;
	}
	
} /* end void sampleFloatColVToFloatColV(unsigned short *, unsigned short *) */

/* ------------------------------------------------------------------------- */
/* The following functions are 'old' blending functions:                     */

/* ------------------------------------------------------------------------- */
void keyalpha(char *doel)   /* maakt premul 255 */
{
	int c;
	short div;
	div= doel[3];
	if (!div)
	{
		doel[0] = (doel[0] ? 255 : 0);
		doel[1] = (doel[1] ? 255 : 0);
		doel[2] = (doel[2] ? 255 : 0);
	} else
	{
		c= (doel[0]<<8)/div;
		if(c>255) doel[0]=255; 
		else doel[0]= c;
		c= (doel[1]<<8)/div;
		if(c>255) doel[1]=255; 
		else doel[1]= c;
		c= (doel[2]<<8)/div;
		if(c>255) doel[2]=255; 
		else doel[2]= c;
	}
}

/* ------------------------------------------------------------------------- */
/* vult bron onder doel in met alpha van doel*/
void addalphaUnder(char *doel, char *bron)   
{
	int c;
	int mul;

	if(doel[3]==255) return;
	if( doel[3]==0) {	/* is getest, scheelt  */
		*((unsigned int *)doel)= *((unsigned int *)bron);
		return;
	}

	mul= 255-doel[3];

	c= doel[0]+ ((mul*bron[0])/255);
	if(c>255) doel[0]=255; 
	else doel[0]= c;
	c= doel[1]+ ((mul*bron[1])/255);
	if(c>255) doel[1]=255; 
	else doel[1]= c;
	c= doel[2]+ ((mul*bron[2])/255);
	if(c>255) doel[2]=255; 
	else doel[2]= c;
	
	c= doel[3]+ ((mul*bron[3])/255);
	if(c>255) doel[3]=255; 
	else doel[3]= c;
	
	/* doel[0]= MAX2(doel[0], bron[0]); */
}

/* ------------------------------------------------------------------------- */
/* gamma-gecorr: vult bron onder doel in met alpha van doel */
void addalphaUnderGamma(char *doel, char *bron)
{
	unsigned int tot;
	int c, doe, bro;
	int mul;

	/* hier doel[3]==0 of doel==255 afvangen gebeurt al in skylus */
	mul= 256-doel[3];
	
	doe= igamtab1[(int)doel[0]];
	bro= igamtab1[(int)bron[0]];
	tot= (doe+ ((mul*bro)>>8));
	if(tot>65535) tot=65535;
	doel[0]= *((gamtab+tot)) >>8;
	
	doe= igamtab1[(int)doel[1]];
	bro= igamtab1[(int)bron[1]];
	tot= (doe+ ((mul*bro)>>8));
	if(tot>65535) tot=65535;
	doel[1]= *((gamtab+tot)) >>8;

	doe= igamtab1[(int)doel[2]];
	bro= igamtab1[(int)bron[2]];
	tot= (doe+ ((mul*bro)>>8));
	if(tot>65535) tot=65535;
	doel[2]= *((gamtab+tot)) >>8;

	c= doel[3]+ ((mul*bron[3])/255);
	if(c>255) doel[3]=255; 
	else doel[3]= c;
	/* doel[0]= MAX2(doel[0], bron[0]); */
}

/* ------------------------------------------------------------------------- */
/* doel= bron over doel  */
void addalphaOver(char *doel, char *bron)   
{
	int c;
	int mul;

	if(bron[3]==0) return;
	if( bron[3]==255) {	/* is getest, scheelt  */
		*((unsigned int *)doel)= *((unsigned int *)bron);
		return;
	}

	mul= 255-bron[3];

	c= ((mul*doel[0])/255)+bron[0];
	if(c>255) doel[0]=255; 
	else doel[0]= c;
	c= ((mul*doel[1])/255)+bron[1];
	if(c>255) doel[1]=255; 
	else doel[1]= c;
	c= ((mul*doel[2])/255)+bron[2];
	if(c>255) doel[2]=255; 
	else doel[2]= c;
	c= ((mul*doel[3])/255)+bron[3];
	if(c>255) doel[3]=255; 
	else doel[3]= c;
}

/* ------------------------------------------------------------------------- */
void addalphaAdd(char *doel, char *bron)   /* telt bron bij doel */
{
	int c;

	if( doel[3]==0 || bron[3]==255) {	/* is getest, scheelt veel */
		*((unsigned int *)doel)= *((unsigned int *)bron);
		return;
	}
	c= doel[0]+bron[0];
	if(c>255) doel[0]=255; 
	else doel[0]= c;
	c= doel[1]+bron[1];
	if(c>255) doel[1]=255; 
	else doel[1]= c;
	c= doel[2]+bron[2];
	if(c>255) doel[2]=255; 
	else doel[2]= c;
	c= doel[3]+bron[3];
	if(c>255) doel[3]=255; 
	else doel[3]= c;
}
/* ------------------------------------------------------------------------- */
void addalphaAddshort(unsigned short *doel, unsigned short *bron)   /* telt bron bij doel */
{
	int c;

	if( doel[3]==0) {
		*((unsigned int *)doel)= *((unsigned int *)bron);
		*((unsigned int *)(doel+2))= *((unsigned int *)(bron+2));
		return;
	}
	c= doel[0]+bron[0];
	if(c>65535) doel[0]=65535; 
	else doel[0]= c;
	c= doel[1]+bron[1];
	if(c>65535) doel[1]=65535; 
	else doel[1]= c;
	c= doel[2]+bron[2];
	if(c>65535) doel[2]=65535; 
	else doel[2]= c;
	c= doel[3]+bron[3];
	if(c>65535) doel[3]=65535; 
	else doel[3]= c;
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

/* ALPHADDFAC: 
 * 
 *  Z= X alphaover Y:
 *  Zrgb= (1-Xa)*Yrgb + Xrgb
 * 
 *	Om ook de add te doen moet (1-Xa) moduleren met 1 via fac
 *  (1-fac)*(1-Xa) + fac <=>
 *  1-Xa-fac+fac*Xa+fac <=> 
 *  Xa*(fac-1)+1
 */


/* ------------------------------------------------------------------------- */
/* doel= bron over doel  */
void RE_addalphaAddfac(char *doel, char *bron, char addfac)
{
	
	int c, mul;

	if( doel[3]==0) {
		*((unsigned int *)doel)= *((unsigned int *)bron);
		return;
	}

	mul= 255 - (bron[3]*(255-addfac))/255;

	c= ((mul*doel[0])/255)+bron[0];
	if(c>255) doel[0]=255; 
	else doel[0]= c;
	c= ((mul*doel[1])/255)+bron[1];
	if(c>255) doel[1]=255; 
	else doel[1]= c;
	c= ((mul*doel[2])/255)+bron[2];
	if(c>255) doel[2]=255; 
	else doel[2]= c;
	
	/* c= ((mul*doel[3])/255)+bron[3]; */
	c= doel[3]+bron[3];
	if(c>255) doel[3]=255; 
	else doel[3]= c;
}

/* ------------------------------------------------------------------------- */
/* doel= bron over doel  */
void addalphaAddfacshort(unsigned short *doel,
						 unsigned short *bron,
						 short addfac)    
{
	int c, mul;

	if( doel[3]==0) {
		*((unsigned int *)doel)= *((unsigned int *)bron);
		*((unsigned int *)(doel+2))= *((unsigned int *)(bron+2));
		return;
	}

	mul= 0xFFFF - (bron[0]*(255-addfac))/255;
	
	c= ((mul*doel[0])>>16)+bron[0];
	if(c>=0xFFF0) doel[0]=0xFFF0; 
	else doel[0]= c;
	c= ((mul*doel[1])>>16)+bron[1];
	if(c>=0xFFF0) doel[1]=0xFFF0; 
	else doel[1]= c;
	c= ((mul*doel[2])>>16)+bron[2];
	if(c>=0xFFF0) doel[2]=0xFFF0; 
	else doel[2]= c;
	c= ((mul*doel[3])>>16)+bron[3];
	if(c>=0xFFF0) doel[3]=0xFFF0; 
	else doel[3]= c;

}

/* ------------------------------------------------------------------------- */

/* eof pixelblending.c */


