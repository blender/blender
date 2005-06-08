/**
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
 * vanillaRenderPipe.c
 *
 * 28-06-2000 nzc
 *
 * $Id$
 *
 */

/*
  The render pipe
  ---------------

  The overall results of the render pass should end up in R.rectot. This 
  buffer already exists, and although its contents may change, its location
  may not. A lot of other routines depend on it!

*/

/* global includes */
#include <math.h>
#include <limits.h>       /* INT_MIN,MAX are used here                       */
#include <stdlib.h>
#include "MTC_vectorops.h"
#include "MTC_matrixops.h"
#include "MEM_guardedalloc.h"

#include "DNA_camera_types.h"
#include "DNA_object_types.h"
#include "BKE_global.h"
#include "BKE_utildefines.h"

#include "BLI_arithb.h"
#include "BLI_rand.h"

/* local includes (from the render module) */
#include "RE_callbacks.h"
#include "render.h"       /* all kinds of stuff                              */
#include "zbuf.h"         /* for vergzvlak, zbufclip, zbufclipwire           */
#include "edgeRender.h"   /* all edge rendering stuff                        */
#include "pixelshading.h" /* painting the pixels                             */
#include "rendercore.h"

/* general calculus and data manipulation, also local                        */
#include "gammaCorrectionTables.h"
#include "jitter.h"
#include "pixelblending.h"
#include "zbufferdatastruct.h"

/* own includes */
#include "vanillaRenderPipe.h"

#include "SDL_thread.h"

/* threshold for alpha                                                       */
#define RE_FULL_ALPHA_FLOAT 0.9998

/* ------------------------------------------------------------------------- */
/* Debug defines: disable all for production level code.                     */
/* These variables control faking of rendered colours, extra tracing,        */
/* extra error checking and such.                                            */
/* ------------------------------------------------------------------------- */

/* if defined: _very_ explicit tracing and checking enabled                  */
/*  #define RE_FULL_SAFETY */
/* if defined: use 'simple' alpha thresholding on oversampling               */
/* #define RE_SIMPLE_ALPHA_THRESHOLD */

/* ------------------------------------------------------------------------- */

/* ------------------------------------------------------------------------- */

/* External : -------------------------------------------------------------- */

extern float centLut[16];    /* Lookup for jitter offsets.                   */
extern unsigned int  Zsample;        /* Nr. of the currently active oversample. This */
                             /* counter must be set explicitly by the        */
                             /* function that builds the z-buffer.           */
                             /* The buffer-filling functions use it.         */
extern float Zjitx,Zjity;    /* The x,y values for jitter offset             */

extern float Zmulx, Zmuly;   /* Some kind of scale?                          */

extern char cmask[256];      /* When a pixel is supersampled, we must        */
extern char *centmask;       /* compute its colour on a point _on_ the face. */
                             /* These two are used to compute an offset to   */
                             /* guarantee we use valid coordinates.          */

extern RE_APixstrExt *APixbufExt;/*Zbuffer: linked list of face, halo indices*/

/* Globals : --------------------------------------------------------------- */
							   /* we use 2 x three lines, for gaussian sample     */
RE_COLBUFTYPE *AColourBuffer0; /* Buffer for colours of 1 line of pixels      */
RE_COLBUFTYPE *AColourBuffer1; /* Buffer for colours of 1 line of pixels      */
RE_COLBUFTYPE *AColourBuffer2; /* Buffer for colours of 1 line of pixels      */
RE_COLBUFTYPE *AColourBuffer1a; /* Buffer for colours of 1 line of pixels      */
RE_COLBUFTYPE *AColourBuffer2a; /* Buffer for colours of 1 line of pixels      */
RE_COLBUFTYPE *AColourBuffer3; /* Buffer for colours of 1 line of pixels      */

static int     Aminy;         /* y value of first line in the accu buffer    */
static int     Amaxy;         /* y value of last line in the accu buffer     */
                              /* -also used to clip when zbuffering          */

/* Buffer width refers to the size of the buffers we build. Image size is    */
/* the same as R.rectx, R.recty.                                             */
static int     zBufferWidth;  /* special width because zbuffer needs to be   */
                              /* wider */

static int     Azvoordeel;    /* A small offset for transparent rendering.   */
int            alphaLUT[32];  /* alpha lookuptable, for oversampling         */
                              /* Its function has been superceded because    */
                              /* pixels are always integrated. This          */
                              /* performs the same normalization.            */
int            osaNr;         /* The oversample number. I keep it            */
                              /* separately here, because I treat no OSA     */
                              /* as if it were osa=1.                        */

/* ------------------------------------------------------------------------- */

/**
* Z buffer initializer, for new pipeline.
 * <LI>
 * <IT> AColourBuffer : colour buffer for one line
 * <IT> APixbufExt    : pixel data buffer for one line, depth RE_ZBUFLEN 
 * </LI>
 */
static void initRenderBuffers(int bwidth) 
{
	/* bwidth+4, as in rendercore.c. I think it's too much, but yah (ton) */
    AColourBuffer0 = MEM_callocN(4 * sizeof(RE_COLBUFTYPE) * (bwidth+4), "Acolrow");
    AColourBuffer1 = MEM_callocN(4 * sizeof(RE_COLBUFTYPE) * (bwidth+4), "Acolrow");
    AColourBuffer2 = MEM_callocN(4 * sizeof(RE_COLBUFTYPE) * (bwidth+4), "Acolrow");
    AColourBuffer1a = MEM_callocN(4 * sizeof(RE_COLBUFTYPE) * (bwidth+4), "Acolrow");
    AColourBuffer2a = MEM_callocN(4 * sizeof(RE_COLBUFTYPE) * (bwidth+4), "Acolrow");
    AColourBuffer3 = MEM_callocN(4 * sizeof(RE_COLBUFTYPE) * (bwidth+4), "Acolrow");
	
    /* The +1 is needed because the fill-functions use a +1 offset when      */
    /* filling in pixels. Mind that also the buffer-clearing function needs  */
    /* this offset (done in calcZBufLine).                                   */
	/* The offset is wrong: it shouldn't be there. I need to fix this still. */
	zBufferWidth = bwidth + 1;
	initZbuffer(bwidth + 1);

    Aminy= -1000; /* indices of lines in the z buffer: no lines buffered     */
    Amaxy= -1000;


} 

/* ------------------------------------------------------------------------- */
/**
 * Z buffer destructor, frees stuff from initZBuffers().
 */

static void freeRenderBuffers(void) {	
    if (AColourBuffer0) MEM_freeN(AColourBuffer0);
    if (AColourBuffer1) MEM_freeN(AColourBuffer1);
    if (AColourBuffer2) MEM_freeN(AColourBuffer2);
    if (AColourBuffer1a) MEM_freeN(AColourBuffer1a);
    if (AColourBuffer2a) MEM_freeN(AColourBuffer2a);
    if (AColourBuffer3) MEM_freeN(AColourBuffer3);
	freeZbuffer();
} 
/* ------------------------------------------------------------------------- */

/**
 * New fill function for z buffer, for edge-only rendering.
 */
static void zBufferFillFace(unsigned int zvlnr, float *v1, float *v2, float *v3)  
{
	/* Coordinates of the vertices are specified in ZCS */
	VlakRen *vlr;
	int apteller, apoffsetteller;
	double z0; /* used as temp var*/
	double xx1;
	double zxd,zyd,zy0, tmp;
	float *minv,*maxv,*midv;
	register int zverg,zvlak,x;
	int my0,my2,sn1,sn2,rectx,zd;
	int y,omsl,xs0,xs1,xs2,xs3, dx0,dx1,dx2, mask;
	int obtype;
	/* These used to be doubles.  We may want to change them back if the     */
	/* loss of accuracy proves to be a problem? There does not seem to be    */
	/* any performance issues here, so I'll just keep the doubles.           */
	/*  	float vec0[3], vec1[3], vec2[3]; */
	double vec0[3], vec1[3], vec2[3];
	
	vlr= RE_findOrAddVlak( (zvlnr-1) & 0x7FFFFF);
	if(vlr->mat->mode & MA_ZTRA) obtype= RE_POLY;
	else obtype= RE_POLY|RE_SOLID;
	
	/* MIN MAX */
	/* sort vertices for min mid max y value */
	if(v1[1]<v2[1]) {
		if(v2[1]<v3[1])      { minv=v1; midv=v2; maxv=v3;}
		else if(v1[1]<v3[1]) { minv=v1; midv=v3; maxv=v2;}
		else	             { minv=v3; midv=v1; maxv=v2;}
	}
	else {
		if(v1[1]<v3[1]) 	 { minv=v2; midv=v1; maxv=v3;}
		else if(v2[1]<v3[1]) { minv=v2; midv=v3; maxv=v1;}
		else	             { minv=v3; midv=v2; maxv=v1;}
	}

	if(minv[1] == maxv[1]) return;	/* security to remove 'zero' size faces */

	my0  = ceil(minv[1]);
	my2  = floor(maxv[1]);
	omsl = floor(midv[1]);

	/* outside the current z buffer slice: clip whole face */
	if( (my2 < Aminy) || (my0 > Amaxy)) return;

	if(my0<Aminy) my0= Aminy;

	/* EDGES : THE LONGEST */
	xx1= maxv[1]-minv[1];
	if(xx1>2.0/65536.0) {
		z0= (maxv[0]-minv[0])/xx1;
		
		tmp= (-65536.0*z0);
		dx0= CLAMPIS(tmp, INT_MIN, INT_MAX);
		
		tmp= 65536.0*(z0*(my2-minv[1])+minv[0]);
		xs0= CLAMPIS(tmp, INT_MIN, INT_MAX);
	}
	else {
		dx0= 0;
		xs0= 65536.0*(MIN2(minv[0],maxv[0]));
	}
	/* EDGES : THE TOP ONE */
	xx1= maxv[1]-midv[1];
	if(xx1>2.0/65536.0) {
		z0= (maxv[0]-midv[0])/xx1;
		
		tmp= (-65536.0*z0);
		dx1= CLAMPIS(tmp, INT_MIN, INT_MAX);
		
		tmp= 65536.0*(z0*(my2-midv[1])+midv[0]);
		xs1= CLAMPIS(tmp, INT_MIN, INT_MAX);
	}
	else {
		dx1= 0;
		xs1= 65536.0*(MIN2(midv[0],maxv[0]));
	}
	/* EDGES : THE BOTTOM ONE */
	xx1= midv[1]-minv[1];
	if(xx1>2.0/65536.0) {
		z0= (midv[0]-minv[0])/xx1;
		
		tmp= (-65536.0*z0);
		dx2= CLAMPIS(tmp, INT_MIN, INT_MAX);
		
		tmp= 65536.0*(z0*(omsl-minv[1])+minv[0]);
		xs2= CLAMPIS(tmp, INT_MIN, INT_MAX);
	}
	else {
		dx2= 0;
		xs2= 65536.0*(MIN2(minv[0],midv[0]));
	}

	/* ZBUF DX DY */
	/* xyz_1 = v_1 - v_2 */
	MTC_diff3DFF(vec1, v1, v2);
	/* xyz_2 = v_2 - v_3 */
	MTC_diff3DFF(vec2, v2, v3);
	/* xyz_0 = xyz_1 cross xyz_2 */
	MTC_cross3Double(vec0, vec1, vec2);

	/* cross product of two of the sides is 0 => this face is too small */
	if(vec0[2]==0.0) return;

	if(midv[1] == maxv[1]) omsl= my2;
	if(omsl < Aminy) omsl= Aminy-1;  /* make sure it takes the first loop entirely */

	while (my2 > Amaxy) {  /* my2 can be larger */
		xs0+=dx0;
		if (my2<=omsl) {
			xs2+= dx2;
		}
		else{
			xs1+= dx1;
		}
		my2--;
	}

	xx1= (vec0[0]*v1[0]+vec0[1]*v1[1])/vec0[2]+v1[2];

	zxd= -vec0[0]/vec0[2];
	zyd= -vec0[1]/vec0[2];
	zy0= my2*zyd+xx1;
	zd= (int)CLAMPIS(zxd, INT_MIN, INT_MAX);

	/* start-ofset in rect */
	/*    	rectx= R.rectx;  */
	/* I suspect this var needs very careful setting... When edge rendering  */
	/* is on, this is strange */
  	rectx = zBufferWidth;
	apoffsetteller = rectx*(my2-Aminy);

	mask= 1<<Zsample;
	zvlak= zvlnr;

	xs3= 0;		/* flag */
	if(dx0>dx1) {
		MTC_swapInt(&xs0, &xs1);
		MTC_swapInt(&dx0, &dx1);
		xs3= 1;	/* flag */

	}

	for(y=my2;y>omsl;y--) {

		sn1= xs0>>16;
		xs0+= dx0;

		sn2= xs1>>16;
		xs1+= dx1;

		sn1++;

		if(sn2>=rectx) sn2= rectx-1;
		if(sn1<0) sn1= 0;
		zverg= (int) CLAMPIS((sn1*zxd+zy0), INT_MIN, INT_MAX);
		apteller = apoffsetteller + sn1;
		x= sn2-sn1;
		
		zverg-= Azvoordeel;
		
		while(x>=0) {
			insertObject(apteller, zvlnr, obtype, zverg, mask);
			zverg+= zd;
			apteller++;
			x--;
		}
		zy0-= zyd;
		apoffsetteller -= rectx;
	}

	if(xs3) {
		xs0= xs1;
		dx0= dx1;
	}
	if(xs0>xs2) {
		xs3= xs0;
		xs0= xs2;
		xs2= xs3;
		xs3= dx0;
		dx0= dx2;
		dx2= xs3;
	}

	for(; y>=my0; y--) {

		sn1= xs0>>16;
		xs0+= dx0;

		sn2= xs2>>16;
		xs2+= dx2;

		sn1++;

		if(sn2>=rectx) sn2= rectx-1;
		if(sn1<0) sn1= 0;
		zverg= (int) CLAMPIS((sn1*zxd+zy0), INT_MIN, INT_MAX);
		apteller = apoffsetteller + sn1;
		x= sn2-sn1;
      
		zverg-= Azvoordeel;
      
		while(x>=0) {
			insertObject(apteller, zvlnr, obtype, zverg, mask);
			zverg+= zd;
			apteller++;
			x--;
		}
		
		zy0-=zyd;
		apoffsetteller -= rectx;
	}
} 
/* ------------------------------------------------------------------------- */

static void zBufferFillEdge(unsigned int zvlnr, float *vec1, float *vec2)
{
	int apteller;
	int start, end, x, y, oldx, oldy, ofs;
	int dz, vergz, mask, maxtest=0;
	float dx, dy;
	float v1[3], v2[3];
	
	dx= vec2[0]-vec1[0];
	dy= vec2[1]-vec1[1];
	
	if(fabs(dx) > fabs(dy)) {
		
		/* all lines from left to right */
		if(vec1[0]<vec2[0]) {
			VECCOPY(v1, vec1);
			VECCOPY(v2, vec2);
		}
		else {
			VECCOPY(v2, vec1);
			VECCOPY(v1, vec2);
			dx= -dx; dy= -dy;
		}
		
		start= floor(v1[0]);
		end= start+floor(dx);
		if(end >= zBufferWidth) end = zBufferWidth - 1;
		
		oldy= floor(v1[1]);
		dy/= dx;
		
		vergz= v1[2];
		vergz-= Azvoordeel;
		dz= (v2[2]-v1[2])/dx;
		if(vergz>0x70000000 && dz>0) maxtest= 1;		// prevent overflow
		
		apteller = zBufferWidth*(oldy-Aminy) +start;
		mask  = 1<<Zsample;	
		
		if(dy<0) ofs= -zBufferWidth;
		else ofs= zBufferWidth;
		
		for(x= start; x<=end; x++, /*  ap++, */ apteller++) {
			
			y= floor(v1[1]);
			if(y!=oldy) {
				oldy= y;
				apteller += ofs;
			}
			
			if(x>=0 && y>=Aminy && y<=Amaxy) {
				insertObject(apteller, zvlnr, RE_POLY, vergz, mask);
			}
			
			v1[1]+= dy;
			vergz+= dz;
			if(maxtest && vergz<0) vergz= 0x7FFFFFF0;
		}
	}
	else {
		
		/* all lines from top to bottom */
		if(vec1[1]<vec2[1]) {
			VECCOPY(v1, vec1);
			VECCOPY(v2, vec2);
		}
		else {
			VECCOPY(v2, vec1);
			VECCOPY(v1, vec2);
			dx= -dx; dy= -dy;
		}
		
		start= floor(v1[1]);
		end= start+floor(dy);
		
		if(start>Amaxy || end<Aminy) return;
		
		if(end>Amaxy) end= Amaxy;
		
		oldx= floor(v1[0]);
		dx/= dy;
		
		vergz= v1[2];
		vergz-= Azvoordeel;
		dz= (v2[2]-v1[2])/dy;
		if(vergz>0x70000000 && dz>0) maxtest= 1;		// prevent overflow
		
		apteller = zBufferWidth*(start-Aminy) +oldx;
		
		mask= 1<<Zsample;
		
		if(dx<0) ofs= -1;
		else ofs= 1;
		
		for(y= start; y<=end; y++, apteller += zBufferWidth) {
			
			x= floor(v1[0]);
			if(x!=oldx) {
				oldx= x;
				apteller += ofs;
			}
			
			if(x>=0 && y>=Aminy && (x < zBufferWidth)) {
				insertObject(apteller, zvlnr, RE_POLY, vergz, mask);
			}
			
			v1[0]+= dx;
			vergz+= dz;
			if(maxtest && vergz<0) vergz= 0x7FFFFFF0;
		}
	}
}
/* ------------------------------------------------------------------------- */

/**
 * Count and sort the list behind ap into buf. Sorts on min. distance.
 * Low index <=> high z
 */
static int countAndSortPixelFaces(int zrow[][RE_PIXELFIELDSIZE], 
                           RE_APixstrExt *ap)
{
    int totvlak;          /* face counter                          */
    int i;                /* generic counter                       */

    totvlak= 0;
    while(ap) {
        for(i=0; i<4; i++) {
            if(ap->t[i]) {
                zrow[totvlak][0] = ap->zmin[i];
                zrow[totvlak][1] = ap->p[i];
                zrow[totvlak][2] = ap->mask[i];
                zrow[totvlak][3] = ap->t[i]; 
                zrow[totvlak][4] = ap->zmax[i];
                totvlak++;
                if(totvlak > (RE_MAX_FACES_PER_PIXEL - 1)) 
				{
                    totvlak = (RE_MAX_FACES_PER_PIXEL - 1);
				}
            } else break;
        };
        ap= ap->next;
    }
    
    if(totvlak==2) { /* Sort faces ----------------------------- */
        if(zrow[0][0] < zrow[1][0]) {
            i= zrow[0][0]; zrow[0][0]= zrow[1][0]; zrow[1][0]= i;
            i= zrow[0][1]; zrow[0][1]= zrow[1][1]; zrow[1][1]= i;
            i= zrow[0][2]; zrow[0][2]= zrow[1][2]; zrow[1][2]= i;
            i= zrow[0][3]; zrow[0][3]= zrow[1][3]; zrow[1][3]= i;
            i= zrow[0][4]; zrow[0][4]= zrow[1][4]; zrow[1][4]= i;
        } /* else: two faces, and ordering is ok */                    
    } else if (totvlak != 1) qsort(zrow, totvlak, 
                                   sizeof(int)*RE_PIXELFIELDSIZE, vergzvlak);	
    return totvlak;
} 

/* ------------------------------------------------------------------------- */
/* Oversampler v3 - check CVS for older versions                             */
/*                                                                           */
/* In this version, I have split up the rendering into several parts, so I   */
/* can generate better profiles.                                             */
/*                                                                           */
/* - multiple blend functions ?                                              */
/* - x-rays?                                                                 */
/* - volumetric stuff ?                                                      */
/* - maybe the oversampling should move to the shading part                  */
/*                                                                           */
/* ------------------------------------------------------------------------- */

/* These variables describe the buffers needed for the oversampling.         */
/* 1. A bit vector with flags to indicate which pixels have received colour. */
static int            VR_covered = 0;
/* 2. The local vector collector, for resolving conflicts only. */
static int            VR_cbuf[RE_MAX_FACES_PER_PIXEL][2];

/** 
 * Analyze the z-buffer, and pre-sample the colours.
 */
static int composeStack(int zrow[][RE_PIXELFIELDSIZE], RE_COLBUFTYPE *collector, 
				 struct RE_faceField* stack, int ptr,
				 int totvlak, float x, float y, int osaNr) 
{
	VlakRen *vlr= NULL;
    float  xs = 0.0;
    float  ys = 0.0;  /* coordinates for the render-spot              */

    float  alphathreshold[RE_MAX_OSA_COUNT];
	float colbuf[4];
    int    inconflict          = 0;
    int    saturationthreshold = 0;
    int    saturated           = 0;
	int    i                   = 0;
	int    Ccount              = 0;
    int    Cthresh             = 0;
	int    save_totvlak        = totvlak;
	int    fullsubpixelflags   = 0;
	int    full_osa;

	VR_covered = 0;
    for(i = 0; i < osaNr; i++) alphathreshold[i] = 0.0;
    saturationthreshold = ( (1<<osaNr) - 1);

    while ( (!saturated || (saturated && inconflict) ) && (totvlak > 0) ) {
        totvlak--;
		
		full_osa= 0;
		if(R.osa && (zrow[totvlak][RE_TYPE] & RE_POLY)) {
			vlr= RE_findOrAddVlak((zrow[totvlak][RE_INDEX]-1) & 0x7FFFFF);
			if(vlr->flag & R_FULL_OSA) full_osa= 1;
		}
		
		if(full_osa) {
			float div=0.0, accol[4]={0.0, 0.0, 0.0, 0.0};
			int a, mask= zrow[totvlak][RE_MASK];
			
			for(a=0; a<R.osa; a++) {
				if(mask & (1<<a)) {
					xs= (float)x + jit[a][0];
					ys= (float)y + jit[a][1];
					renderPixel(collector, xs, ys, zrow[totvlak], 1<<a);
					accol[0] += collector[0]; accol[1] += collector[1]; accol[2] += collector[2]; accol[3] += collector[3];
					div+= 1.0;
				}
			}
			if(div!=0.0) {
				div= 1.0/div;
				collector[0]= accol[0]*div; collector[1]= accol[1]*div; collector[2]= accol[2]*div; collector[3]= accol[3]*div;
			}
			stack[ptr].mask= mask;
			stack[ptr].data= vlr;
		}
		else {
			if(R.osa) {
				i= centmask[ zrow[totvlak][RE_MASK] ]; /* recenter sample position - */
				xs= (float)x+centLut[i &  15];
				ys= (float)y+centLut[i >> 4];
			}
			else {
				xs= (float)x;
				ys= (float)y;
			}
			
			/* stack face ----------- */
			stack[ptr].mask     = zrow[totvlak][RE_MASK];
			stack[ptr].data     = renderPixel(collector, xs, ys, zrow[totvlak], stack[ptr].mask);
		}
		stack[ptr].faceType = zrow[totvlak][RE_TYPE];
        cpFloatColV(collector, stack[ptr].colour);

		/* This is done so that spothalos are properly overlayed on halos    */
		/* maybe we need to check the colour here...                         */
  		if(zrow[totvlak][RE_TYPE] & RE_POLY) VR_covered |= zrow[totvlak][RE_MASK]; 
		
        /* calculate conflict parameters: ---------------------------------- */
        if( zrow[totvlak][RE_ZMIN] < Cthresh ) {
            inconflict = 1;
			/* Prevent from switching on bad data. This may be done more     */
			/* efficiently later on. It is _quite_ important.                */
			if (totvlak == save_totvlak - 1) Ccount = 0;
			else if(Ccount == 0)             Ccount = 2;
			else                             Ccount++;
			stack[ptr].conflictCount = Ccount;
            if (zrow[totvlak][RE_ZMAX] > Cthresh) 
				Cthresh = zrow[totvlak][RE_ZMAX]; 
        } else { 
            Cthresh         = zrow[totvlak][RE_ZMAX];
            Ccount          = 0;
			stack[ptr].conflictCount = 0;
            if (totvlak > 0 )
				inconflict = (zrow[totvlak-1][RE_ZMIN] < Cthresh);
			else inconflict = 0;
        }

		ptr++;
		
		/* alpha threshold ------------------------------------------------- */
		/* There are currently two ways of blending: alpha-over, and add.    */
		/* Add-blending does strange things, in the sense that alpha is      */
		/* simply added, and colour is sort of alpha-over blended. Using the */
		/* same thresholding relation seems to work ok. For less than unity  */
		/* add factor, the alpha threshold may rise faster, but currently we */
		/* do not check for this factor.                                     */
		for(i = 0; i < osaNr; i++) {
			if ( zrow[totvlak][RE_MASK] & (1<<i)) {
				alphathreshold[i] += 
					((1.0 - alphathreshold[i]) * collector[3]);
				if (alphathreshold[i] > RE_FULL_ALPHA_FLOAT) 
					fullsubpixelflags |= (1<<i);
			}
		}
		saturated = (fullsubpixelflags >= saturationthreshold);

    } /* done stacking ----------------------------------------------------- */

	/*
	  STACK_SKY Sometimes, a sky pixel is needed. Since there are
	  some issues with mist/ ztra/ env, I always put the sky here.
	*/
/*  	if (!saturated) { */
	totvlak--;

	xs= (float)x;
	ys= (float)y;

	/* code identical for rendering empty sky pixel */
	renderSkyPixelFloat(collector, xs, ys);
	cpFloatColV(collector, colbuf);

	if(R.flag & R_LAMPHALO) {
		renderSpotHaloPixel(x, y, collector);
		addAlphaOverFloat(colbuf, collector);
	}

	stack[ptr].faceType      = RE_SKY;
	cpFloatColV(colbuf, stack[ptr].colour);
	stack[ptr].data          = NULL;
	stack[ptr].mask          = 0xFFFF;
	stack[ptr].conflictCount = 0;
	ptr++;
/*  	} */

	/* Index of the top of the stack */
	return ptr;
}

/* ------------------------------------------------------------------------- */

/**
 * Calculate the view depth to this object on this location, with 
 * the current view parameters in R.
 */
static int calcDepth(float x, float y, void *data, int type)
{
    float view[3];
	
    if (type & RE_POLY) {
        VlakRen* vlr = (VlakRen*) data;
        VertRen* v1;
        float dface, div, zco, hoco_z, hoco_w;
        int zbuf_co;
        
        v1 = vlr->v1;
        
        /* vertex dot face normal: WCS */
        dface= v1->co[0]*vlr->n[0]+v1->co[1]*vlr->n[1]+v1->co[2]*vlr->n[2]; 
        
        /* jitter has been added to x, y ! */
        /* view vector view: screen coords */
		view[0]= (x+(R.xstart)+0.5);
        
        if(R.flag & R_SEC_FIELD) {
            if(R.r.mode & R_ODDFIELD) view[1]= (y + R.ystart)*R.ycor;
            else view[1]= (y + R.ystart + 1.0)*R.ycor;
        } 
		else view[1]= (y + R.ystart + 0.5)*R.ycor;
        
		
		/* for pano, another rotation in the xz plane is needed.... */
		
        /* this is ok, in WCS */
        view[2]= -R.viewfac;  /* distance to viewplane */
        
		/* calculate zcoord */
		if(R.r.mode & R_ORTHO) {
			/* x and y 3d coordinate can be derived from pixel coord and winmat */
			float fx= 2.0/(R.rectx*R.winmat[0][0]);
			float fy= 2.0/(R.recty*R.winmat[1][1]);
			
			fx= (0.5 + x - 0.5*R.rectx)*fx - R.winmat[3][0]/R.winmat[0][0];
			fy= (0.5 + y - 0.5*R.recty)*fy - R.winmat[3][1]/R.winmat[1][1];
			
			/* using a*x + b*y + c*z = d equation, (a b c) is normal */
			zco= (dface - vlr->n[0]*fx - vlr->n[1]*fy)/vlr->n[2];
			
		}
		else {
			/* face normal dot view vector: but how can this work? (nzc) */
			div = MTC_dot3Float(vlr->n, view);
			if (div!=0.0) zco = (view[2]*dface)/div;
			else zco = 0.0;
		}
        
        /* same as in zbuf.c */
        hoco_z =  zco*R.winmat[2][2] + R.winmat[3][2]; 
        hoco_w =  zco*R.winmat[2][3] + R.winmat[3][3]; 
        
		if(hoco_w!=0.0) zbuf_co = 0x7FFFFFFF*(hoco_z/hoco_w);
		else zbuf_co= 0x7FFFFFFF;
        
        return  zbuf_co; /* z component of R.co */
    } else if (type & RE_HALO) {
        HaloRen* har = (HaloRen*) data;
        return har->zBufDist;
    }
    return 0;
} 

/**
 * Blend source over dest, and leave result in dest. 1 pixel.
 */
static void blendOverFloat(int type, float* dest, float* source, void* data)
{
	
    if (type & RE_POLY) {
        VlakRen *ver = (VlakRen*) data;
        if ((ver->mat != NULL) && (ver->mat->add > RE_FACE_ADD_THRESHOLD)) {
            char addf = (char) (ver->mat->add * 255.0);
            addalphaAddfacFloat(dest, source, addf);
        }
        else
            addAlphaOverFloat(dest, source);
    } else if (type & RE_HALO) {
        HaloRen *har= (HaloRen*) data;
        addalphaAddfacFloat(dest, source, har->add);
    } else if (type & RE_SKY) {
		addAlphaOverFloat(dest, source);
	}
	
}


/**
 * New approach: sample substacks. Each substack is first copied into
 * a stack buffer, and then blended down.
 * */
static void integratePerSubStack(float *sampcol, struct RE_faceField* stack,
						  int ptr,  float x,  float y, int osaNr) 
{
	int i = 0;
	int j = 0;
	int k = 0;
	int l = 0;
	int filterMask = 0;
	/* next step would be to improve on the substack, I guess */
	int subStack[RE_MAX_FACES_PER_PIXEL + 1];
	float colSubStack[4 * (RE_MAX_FACES_PER_PIXEL + 1)];
	int subStackPtr = 0;
	int subStackSize = 0;
	float xs, ys;

	
	while (i < osaNr) {
		xs = x + jit[i][0];
		ys = y + jit[i][1];
		
		/*
		 *  1. Copy all relevant faces. Mind that stack is built from
		 *  low index = low z to high index =high z. The sub-stack is
		 *  exactly the other way around! (low index = high z)
		 */
		filterMask = (1 << i);
		subStackPtr = 0;
		j = ptr - 1; /* the topmost valid face */
		while (j >= 0) {
			if (stack[j].conflictCount) {
				/* Conflict: we sort the faces for distance right
				 * away. We could adapt conflict count, and adjust the
				 * stack later on, but that's really doing too much,
				 * too complicated. This is just fine.
				 * */
				k = 0;
				l = 0;
				/* check whether the face intersects, and if so,
                 * stores depth */
				while (k < stack[j].conflictCount) {
					if (stack[j - k].mask & filterMask) {
						VR_cbuf[l][0] = calcDepth(xs, ys,
												  stack[j - k].data,
												  stack[j - k].faceType);
						VR_cbuf[l][1] = j - k;
						l++;
					}
					k++;
				}
				/* VR_cbuf now contains l pairs (distance, stackindex) */
				qsort(VR_cbuf, l, sizeof(int)*2, vergzvlak); 
				/*
				 * Now we put the sorted indices on the
				 * substack. qsort delivers low index = low z, which
				 * is the right wrong order for the substack */
				k = 0;
				while (k < l) {
					subStack[subStackPtr] = VR_cbuf[k][1];
					cpFloatColV(stack[VR_cbuf[k][1]].colour, &colSubStack[4*subStackPtr]);
					subStackPtr++;
					k++;
				}
				
				j -= stack[j].conflictCount;
			} else {
				/* no conflict */
				if (stack[j].mask & filterMask) {
					subStack[subStackPtr] = j;
					cpFloatColV(stack[j].colour, &colSubStack[4*subStackPtr]);
					subStackPtr++;
				}
				j--;
			}
		}
		subStackSize = subStackPtr;
		
		/* 2. Operations on the faces can go here for now. I might
		 * want to mix this code with the blending. Currently, I only
		 * handle env/ztra faces. It's a dirty patch now...*/
		subStackPtr = subStackSize - 1;
		while (subStackPtr >= 0) {
			/* we can make a general meachanism here for operations */
			if (stack[subStack[subStackPtr]].faceType == RE_POLY){ 
				VlakRen* vlr = (VlakRen*) stack[subStack[subStackPtr]].data;
				if (vlr->mat) {
					/* ENV faces */
					if (vlr->mat->mode & MA_ENV) {
						int m;
						colSubStack[4*subStackPtr]       = 0.0;
						colSubStack[(4*subStackPtr) + 1] = 0.0;
						colSubStack[(4*subStackPtr) + 2] = 0.0;
						colSubStack[(4*subStackPtr) + 3] = 0.0;
						m = subStackPtr - 1;
						while (m >= 0) {
							if (stack[subStack[m]].faceType != RE_SKY) {
								colSubStack[4*m]       = 0.0;
								colSubStack[(4*m) + 1] = 0.0;
								colSubStack[(4*m) + 2] = 0.0;
								colSubStack[(4*m) + 3] = 0.0;
							}
							m--;
						}
					}
					/* ZTRA faces */
					else if (!(vlr->mat->mode & MA_ZTRA)) { 
						int m;
						m = subStackPtr - 1;
						while (m >= 0) {
							if (stack[subStack[m]].faceType != RE_SKY) {
								colSubStack[4*m]       = 0.0;
								colSubStack[(4*m) + 1] = 0.0;
								colSubStack[(4*m) + 2] = 0.0;
								colSubStack[(4*m) + 3] = 0.0;
							}
							m--;
						}
					}
				}
			}
			subStackPtr--;
		}
		
		/* 3. blend down */
		subStackPtr = 0;
        while( subStackPtr < subStackSize ) {
			blendOverFloat(stack[subStack[subStackPtr]].faceType, /* type */
						   sampcol + (4 * i),                 /* dest */
						   &colSubStack[4 * subStackPtr],
						   stack[subStack[subStackPtr]].data);    /* data */
			subStackPtr++;
		}
		
		i++;
	}
}



/* ------------------------------------------------------------------------- */
/* Rendering: per line                                                       */
/*                                                                           */
/* For each pixel in this line, we render as follows:                        */
/* a. Count the number of objects buffered for this pixel, and sort on z     */
/* ------- Result is left in zrow                                            */
/* b. Shade the pixel:                                                       */
/*  1. From front to back: calculate the colour for this object              */
/*  2. Blend this colour in with the already calculated colour               */
/*    Repeat 1. and 2. until no faces remain.                                */
/*  For each pixel, a face is only rendered once, even if it is              */
/*  jittered. All subpixels get the colour of the weighted centre            */
/*  of the jitter-positions this face covers.                                */
/* ------- Result is left in sampcol[]                                       */
/* c. Copy the result to the colour buffer                                   */
/* d. Do gamma-corrected blending                                            */
/*                                                                           */
/* zrow may need some clarification:                                         */
/* 0 - min. distance                                                         */
/* 1 - face/halo index                                                       */
/* 2 - masks                                                                 */
/* 3 - type RE_POLY or RE_HALO                                               */
/* 4 - max. distance                                                         */
/* It is used to store copies of RE_APixstrExt records. These are sorted for */
/* distance, and then used for rendering pixels. zrow might be replaced by   */
/* an RE_APixstrExt* array                                                   */
/* - redo the numbering to something more logical                            */


/* threadsafe global arrays, too large for stack */
typedef struct zbufline {
	int  zrow[RE_MAX_FACES_PER_PIXEL][RE_PIXELFIELDSIZE];
	struct RE_faceField osastack[RE_MAX_FACES_PER_PIXEL + 1]; 
} zbufline;

static zbufline zb1, zb2;

static void renderZBufLine(int y, RE_COLBUFTYPE *colbuf1, RE_COLBUFTYPE *colbuf2, RE_COLBUFTYPE *colbuf3) 
{
     RE_APixstrExt *ap;       /* iterator for the face-lists                  */
	RE_COLBUFTYPE  collector[4];
	RE_COLBUFTYPE  sampcol[RE_MAX_OSA_COUNT * 4];
    RE_COLBUFTYPE *j = NULL; /* generic pixel pointer                        */
	int apteller;
    int x;                   /* pixel counter                                */
    int i;                   /* yet another counter                          */
    int stackDepth;          /* faces-behind-this-pixel counter              */
	int osastack_ptr;	/* Points to the lowest empty field. The indexed */
	zbufline *zbl;

	/* thread safe row buffers */
	if(y & 1) zbl= &zb1;
	else zbl= &zb2;
	
    /* Prepare iterators */
    ap        = APixbufExt + (zBufferWidth * (y - Aminy));
	apteller  = (zBufferWidth * (y - Aminy));
	
    /* Rendering: give the right colour to this pixel (shade it) */
	for( x = 0; x < R.rectx; x++, ap++, colbuf1+=4, colbuf2+=4, colbuf3+=4) {
		if(ap->t[0]) {
            /* reset sample collector */
            j = sampcol;
            for(i = 0; i < osaNr; i++, j+=4) { 
                j[0] = 0.0f; j[1] = 0.0f; 
                j[2] = 0.0f; j[3] = 0.0f;
            };

            /* a. count and sort number of faces */
            stackDepth = countAndSortPixelFaces( zbl->zrow, ap);
			
            /* b,c. oversample all subpixels, then integrate                 */
			osastack_ptr = 0;
			osastack_ptr = composeStack(zbl->zrow, collector, zbl->osastack, osastack_ptr, 
										stackDepth, x, y, osaNr);
  			integratePerSubStack(sampcol, zbl->osastack, osastack_ptr,  x, y, osaNr); 
			
			/* d. Gamma corrected blending and Gaussian                      */
			sampleFloatColV2FloatColVFilter(sampcol, colbuf1, colbuf2, colbuf3, osaNr);
			
		} else {
			/* Remember to do things back-to-front!                          */
			
			/* This is a bit dirty. Depending on sky-mode, the pixel is      */
			/* blended in differently.                                       */
			renderSkyPixelFloat(collector, x, y);
			
			j = sampcol;
			for(i = 0; i < osaNr; i++, j+=4) { 
				j[0]= collector[0]; j[1]= collector[1];
				j[2]= collector[2]; j[3]= collector[3];
			}
			
			sampleFloatColV2FloatColVFilter(sampcol, colbuf1, colbuf2, colbuf3, osaNr);
			
			
			/* Spothalos are part of the normal pixelshader, so for covered  */
			/* pixels they are handled ok. They are 'normally' alpha blended */
			/* onto the existing colour in the collector.                    */
			if(R.flag & R_LAMPHALO) {
				renderSpotHaloPixel(x, y, collector);
				if(do_gamma) {
					collector[0]= gammaCorrect(collector[0]);
					collector[1]= gammaCorrect(collector[1]);
					collector[2]= gammaCorrect(collector[2]);
				}
				addAlphaOverFloat(colbuf2+4, collector);
			}
		}
    } 
} 


/**
 * Fills in distances of faces in the z buffer.
 *
 * Halo z buffering ---------------------------------------------- 
 *
 * A halo is treated here as a billboard: no z-extension, always   
 * oriented perpendicular to the viewer. The rest of the z-buffer  
 * stores face-numbers first, then calculates colours as the       
 * final image is rendered. We'll use the same approach here,      
 * which differs from the original method (which was add halos per 
										   * scan line). This means that the z-buffer now also needs to      
 * store info about what sort of 'thing' the index refers to.      
 *                                                                 
 * Halo extension:                                                 
 * h.maxy  ---------                                               
 *         |          h.xs + h.rad                                 
 *             |      h.xs                                         
 *                 |  h.xs - h.rad                                 
 * h.miny  ---------                                               
 *                                                                 
 * These coordinates must be clipped to picture size.              
 * I'm not quite certain about halo numbering.                     
 *                                                                 
 * Halos and jittering -------------------------------------------  
 *                                                                 
 * Halos were not jittered previously. Now they are. I wonder      
 * whether this may have some adverse effects here.                
 
 * @return 1 for succes, 0 if the operation was interrupted.
 */

/* ------------------------------------------------------------------------- */
/* Transparent faces and the 'Azvoordeel'                                    */
/* A transparent face can get a z-offset, which is a                         */
/* way of pretending the face is a bit closer than it                        */
/* actually is. This is used in animations, when faces                       */
/* that are used to glue on animated characters, items,                      */
/* et. need their shadows to be drawn on top of the                          */
/* objects they stand on. The Azvoordeel is added to                         */
/* the calculated z-coordinate in the buffer-fill                            */
/* procedures.                                                               */

/*  static int RE_treat_face_as_opaque; */

static int zBufferAllFaces(void) 
{
	VlakRen *vlr=NULL;
	unsigned int zvlnr;
	int faceCounter;
    int keepLooping = 1; 
    float vec[3], hoco[4], mul, zval, fval; 
    Material *ma=0;
    
    faceCounter = 0;

/*  	printf("Going to buffer faces:\n"); */
/*  	printf("\tfirst pass:\n"); */

/*  	RE_treat_face_as_opaque = 1; */
	
	while ( (faceCounter < R.totvlak) && keepLooping) {
		if((faceCounter & 255)==0) { vlr= R.blovl[faceCounter>>8]; }
		else vlr++;
		
		ma= vlr->mat;
		
		/* VERY dangerous construction... zoffs is set by a slide in the ui */
		/* so it should be safe...                                          */
		if((ma->mode & (MA_ZTRA)) && (ma->zoffs != 0.0)) {
			mul= 0x7FFFFFFF;
			zval= mul*(1.0+vlr->v1->ho[2]/vlr->v1->ho[3]);
			
			VECCOPY(vec, vlr->v1->co);
			/* z is negatief, wordt anders geclipt */ 
			vec[2]-= ma->zoffs;
			RE_projectverto(vec, hoco); /* vec onto hoco */
			fval= mul*(1.0+hoco[2]/hoco[3]);
			
			Azvoordeel= (int) fabs(zval - fval );
		} else {
			Azvoordeel= 0;
		} 
		/* face number is used in the fill functions */
		zvlnr = faceCounter + 1;
		
		if(vlr->flag & R_VISIBLE) { /* might test for this sooner...  */
			
			if(ma->mode & (MA_WIRE)) zbufclipwire(zvlnr, vlr);
			else {
				zbufclip(zvlnr, vlr->v1->ho,   vlr->v2->ho,   vlr->v3->ho, 
						 vlr->v1->clip, vlr->v2->clip, vlr->v3->clip);
				if(vlr->v4) {
					zvlnr+= 0x800000; /* in a sense, the 'adjoint' face */
					zbufclip(zvlnr, vlr->v1->ho,   vlr->v3->ho,   vlr->v4->ho, 
							 vlr->v1->clip, vlr->v3->clip, vlr->v4->clip);
				}
			}
		}
		if(RE_local_test_break()) keepLooping = 0; 
		faceCounter++;
	}
	
    return keepLooping;
}

/**
 * Fills in distances of halos in the z buffer.
 * @return 1 for succes, 0 if the operation was interrupted.
 */

/* ------------------------------------------------------------------------- */
/* We cheat a little here: we only fill the halo on the first pass, and we   */
/* set a full complement of mask flags. This can be done because we consider */
/* halos to be flat (billboards), so we do not have to correct the z range   */
/* every time we insert a halo. Also, halos fall off to zero at the edges,   */
/* so we can safely render them in pixels where they do not exist.           */
static int zBufferAllHalos(void)
{
    HaloRen *har = NULL;
    int haloCounter = 0;
    int dist = 0;
    int keepLooping = 1;
    short miny = 0, maxy = 0, minx = 0, maxx = 0;
    short ycount = 0, xcount = 0;
    RE_APixstrExt *ap, *apoffset;
    int mask; /* jitter mask */

	if (!Zsample) 
	{
		mask = (1 << osaNr) - 1 ; /* Fill all samples together */
    
		while ( (haloCounter < R.tothalo) && keepLooping) {
			if((haloCounter & 255)==0) har= R.bloha[haloCounter>>8];
			else har++;

			/* Halos are sometimes wrongly kicked out of the box they belong   */
			/* in...                                                           */
			
			/* Only buffer the current alpha buffer contents!!! The line       */
			/* indices have already been clipped to picture size.              */ 
			minx = floor(har->xs - har->rad) - 1; /* assume min =< max is true*/
			if (minx < 0 ) minx = 0;
			maxx = ceil(har->xs + har->rad ) + 1;
			/* Do the extra -1 because of the +1 later on. I guess halos might */
			/* have to start one pixel sooner? Or maybe the lower clip should  */
			/* be adjusted                                                     */
			if (maxx >= zBufferWidth - 1) maxx = zBufferWidth - 2;

			miny = har->miny;
			if (miny < Aminy) miny = Aminy;
			maxy = har->maxy;
			if (maxy > Amaxy) maxy = Amaxy;
			
			if ( (minx <= maxx) && (miny <= maxy)) {            
				/* distance to this halo? */
				dist = har->zBufDist /*   * R.ycor  */;
				/* strange that the ycor influences the z coordinate ..*/
				ycount = miny;
				while (ycount <= maxy) {
					apoffset = APixbufExt + (zBufferWidth * (ycount - Aminy));
					ap = apoffset + minx;
					xcount = minx;
					while (xcount <= maxx) {    
  						insertFlatObjectNoOsa(ap, haloCounter, RE_HALO, dist, mask);
						xcount++;
						ap++;
					}
					ycount++;
				}
			}
			if(RE_local_test_break()) keepLooping = 0;
			haloCounter++;
		}
	} 

    return keepLooping;
}
/* ------------------------------------------------------------------------- */

/**
* Fills in distances of all faces in a z buffer, for given jitter settings.
 */
static int fillZBufDistances() 
{
    int keepLooping = 1;
	
    keepLooping = zBufferAllFaces(); /* Solid and transparent faces*/
    keepLooping = zBufferAllHalos() && keepLooping; /* ...and halos*/
    return keepLooping;
	
}



#if 0
/* ------------------------------------------------------------------------- */
/**
 * One more filler: fill in halo data in z buffer.
 * Empty so far, but may receive content of halo loop.
 */
void zBufferFillHalo(void)
{
    /* so far, intentionally empty */
}
#endif

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
	without setting Osa or Gamme flags off (ton)

---------------------------------------------------------------------------- */
/* used external! */
void transferColourBufferToOutput( float *buf, int y)
{
    /* Copy the contents of AColourBuffer3 to R.rectot + y * R.rectx */
    int x = 0;
    char *target = (char*) (R.rectot + (y * R.rectx));

	/* Copy the first <R.rectx> pixels. We can do some more clipping on    */
	/* the z buffer, I think.                                                 */
	while (x < R.rectx) {

		
		/* invert gamma corrected additions */
		if(do_gamma) {
			buf[0] = invGammaCorrect(buf[0]);
			buf[1] = invGammaCorrect(buf[1]);
			buf[2] = invGammaCorrect(buf[2]);
		}			
			
		std_floatcol_to_charcol(buf, target);
		
		/*
		  Key-alpha mode:
		  Need to un-apply alpha if alpha is non-full. For full alpha,
		  the operation doesn't have effect. Do this after the post-
		  processing, so we can still use the benefits of that.

		*/

		if (getSkyBlendingMode() == RE_ALPHA_KEY) {  
			applyKeyAlphaCharCol(target);
		}				
		
        target+=4;
        buf+=4;
        x++;
    }
}

/* used for redisplay after render. assumes size globals to be set OK! */
void RE_floatbuffer_to_output(void)
{
	float *buf= R.rectftot;
	int pix= R.rectx*R.recty;
	char *target = (char *)R.rectot;
	
	if(R.rectftot==NULL) return;

	while(pix--) {
		std_floatcol_to_charcol(buf, target);
		buf+= 4;
		target+= 4;
	}
}

/* ------------------------------------------------------------------------- */

static void eraseColBuf(RE_COLBUFTYPE *buf) 
{
    /* By definition, the buffer's length is 4 * R.rectx items */
    int i = 0;

    while (i < 4 * (R.rectx+3)) {
        *buf = 0.0f;
        buf++; i++;
    }
} 

/* ------------------------------------------------------------------------- */


/** 
 * Fill the accumulation buffer APixbufExt with face and halo indices. 
 * Note: Uses globals.
 * @param y the line number to set
 */
static void calcZBufLine(int y)
{
	/* These function pointers are used for z buffer filling.    */
	extern void (*zbuffunc)(unsigned int, float *, float *, float *);
	extern void (*zbuflinefunc)(unsigned int, float *, float *);
	int part;
    int keepLooping = 1;
	
    if(y<0) return;
	
	/* zbuffer fix: here? */
	Zmulx= ((float) R.rectx)/2.0;
  	Zmuly= ((float) R.recty)/2.0;
	
	
	/* use these buffer fill functions */    
	zbuffunc     = zBufferFillFace;
	zbuflinefunc = zBufferFillEdge;
	
    /* (FORALL y: Aminy =< y =< Amaxy: y is buffered)                        */
    if( (y < Aminy) || (y > Amaxy)) {

        /* prepare buffer */
        part  = (y/RE_ZBUFLEN);     /* These two lines are mystifying me...  */
        Aminy = part * RE_ZBUFLEN;  /* Possibly for rounding things?         */
        Amaxy = Aminy + RE_ZBUFLEN - 1;
		/*          if(Amaxy >= R.recty) Amaxy = R.recty-1; */
        if(Amaxy >= R.recty) Amaxy = R.recty - 1;
		resetZbuffer();
        
        Zsample = 0; /* Zsample is used internally !                         */
        while ( (Zsample < osaNr) && keepLooping ) {
            /* Apply jitter to this pixel. The jitter offsets are globals.   */
            /* They are added in zbufclip()                                  */
            /* Negative: these offsets are added to the vertex coordinates   */
            /* so it equals translating viewpoint over the positive vector.  */
            Zjitx= -jit[Zsample][0]-0.5;
            Zjity= -jit[Zsample][1]-0.5;
			
            keepLooping = fillZBufDistances();
            
            if(RE_local_test_break()) keepLooping = 0;
            Zsample++;
        }
    }
    
} 


/* ------------------------------------------------------------------------- */

struct renderline {
	RE_COLBUFTYPE *buf1, *buf2, *buf3;
	int y;
};

static int do_renderline(void *poin)
{
	struct renderline *rl= poin;
	
	renderZBufLine(rl->y, rl->buf1, rl->buf2, rl->buf3);
	return 1;
}

void zBufShadeAdvanced()
{
	RE_COLBUFTYPE *cycle;
	struct renderline rl1, rl2;
    int      y, keepLooping = 1;
    float xjit = 0.0, yjit = 0.0;
	
    Zjitx=Zjity= -0.5; /* jitter preset: -0.5 pixel */
	
    /* Set osaNr. Treat 'no osa' as 'osa = 1' */
    if(R.r.mode & R_OSA) {
		osaNr = R.osa;
		if(osaNr > 16) { /* check was moved from calcZBufLine */
			printf("zBufShadeAdvanced> osa too large (internal error)\n");
			G.afbreek= 1;
			return;
		}
    } else {
        /* little hack */
        osaNr = 1;
        xjit = jit[0][0];
        yjit = jit[0][1];
        jit[0][0] = 0.0;
        jit[0][1] = 0.0;        
    }

	RE_setwindowclip(0, -1); /* just to be sure, reset the view matrix       */

	initRenderBuffers(R.rectx);

	y = 0;
	while ( (y < R.recty) && keepLooping) {
		
		calcZBufLine(y);
		
		rl1.buf1= AColourBuffer1;
		rl1.buf2= AColourBuffer2;
		rl1.buf3= AColourBuffer3;
		rl1.y= y;
		
		if(R.r.mode & R_THREADS) {
			if((y & 1)==0) {
				SDL_Thread *thread;
				
				thread = SDL_CreateThread(do_renderline, &rl1);
				if ( thread == NULL ) {
					fprintf(stderr, "Unable to create thread");
					G.afbreek= 1;
					break;
				}
				
				rl2.buf1= AColourBuffer0;
				rl2.buf2= AColourBuffer1a;
				rl2.buf3= AColourBuffer2a;
				rl2.y= y+1;
				
				do_renderline(&rl2);
				
				SDL_WaitThread(thread, NULL);
				
				if(R.r.mode & R_GAUSS) {
					float *rb1= AColourBuffer1, *rb2= AColourBuffer2, *rb1a= AColourBuffer1a, *rb2a= AColourBuffer2a;
					int a= 4*(R.rectx + 4);
					while(a--) {
						*rb1 += *rb1a;
						*rb2 += *rb2a;
						*(rb1a++)= 0.0; rb1++;
						*(rb2a++)= 0.0; rb2++;
					}
				}
				else {
					cycle= AColourBuffer1a; AColourBuffer1a= AColourBuffer1; AColourBuffer1= cycle;
				}
			}
		}
		else do_renderline(&rl1);
		
		if(y) {
			transferColourBufferToOutput(AColourBuffer3+4, y-1);
			
			if((y & 1)==0) RE_local_render_display(y-2, y-1, R.rectx, R.recty, R.rectot);		
		}
		
		/* buffer cycling */
		eraseColBuf(AColourBuffer3);
		cycle= AColourBuffer3;
		AColourBuffer3= AColourBuffer2;
		AColourBuffer2= AColourBuffer1;
		AColourBuffer1= AColourBuffer0;
		AColourBuffer0= cycle;
		
		if(RE_local_test_break()) keepLooping = 0;
		y++; 
	}
	if(keepLooping) transferColourBufferToOutput(AColourBuffer3+4, y-1);

	freeRenderBuffers();

	/* Edge rendering is done purely as a post-effect                        */
	if(R.r.mode & R_EDGE) {
		addEdges((char*)R.rectot, R.rectx, R.recty,
				 osaNr,
				 R.r.edgeint, R.r.same_mat_redux,
				 G.compat, G.notonlysolid,
				 R.r.edgeR, R.r.edgeG, R.r.edgeB);
	}

	if (!(R.r.mode & R_OSA)) {
		jit[0][0] = xjit;
		jit[0][1] = yjit; 
	}

} 

/* ------------------------------------------------------------------------- */




/* eof vanillaRenderPipe.c */
