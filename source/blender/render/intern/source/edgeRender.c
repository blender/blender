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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 * Add enhanced edges on a rendered image (toon shading, edge shading).
 */

/*
 * Edge rendering: use a mask to weigh the depth of neighbouring
 * pixels, and do a colour correction.
 *
 * We need:
 * - a buffer to store the depths (ints)
 * - a function that alters the colours in R.rectot (copy edge_enhance?)
 *   The max. z buffer depth is 0x7FFF.FFFF (7 F's)
 *
 * - We 'ignore' the pixels falling outside the regular buffer (we fill)
 *   these with the max depth. This causes artefacts when rendering in
 *   parts.
 */

/* ------------------------------------------------------------------------- */

/* enable extra bounds checking and tracing                                  */
/* #define RE_EDGERENDERSAFE */
/* disable the actual edge correction                                        */
/*  #define RE_EDGERENDER_NO_CORRECTION */

#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <limits.h>       /* INT_MIN,MAX are used here                       */ 
#include <stdio.h>

#include "MEM_guardedalloc.h"
#include "MTC_vectorops.h"

#include "RE_callbacks.h"
#include "edgeRender.h"
#include "render.h"
#include "render_intern.h"
#include "zbuf.h" /* for zbufclipwire and zbufclip */
#include "jitter.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef RE_EDGERENDERSAFE
char edgeRender_h[] = EDGERENDER_H;
char edgeRender_c[] = "$Id$";
#include "errorHandler.h"
#endif

/* ------------------------------------------------------------------------- */
/* the lazy way: */
#define MIN2(x,y)               ( (x)<(y) ? (x) : (y) )

/* ------------------------------------------------------------------------- */

 /* These function pointers are used for z buffer filling.    */
extern void (*zbuffunc)(float *, float *, float *);
extern void (*zbuflinefunc)(float *v1, float *v2); 
extern float Zmulx, Zmuly;   /* Some kind of scale?                          */
extern float Zjitx,Zjity;    /* The x,y values for jitter offset             */
extern unsigned int Zvlnr;           /* Face rendering pointer and counter: these    */
extern VlakRen *Zvlr;        /* are used for 'caching' render results.       */

/* ------------------------------------------------------------------------- */

/* exp: */
static Material** matBuffer;  /* buffer with material indices                */
static Material* mat_cache;  /* material of the face being buffered          */

static char* colBuffer;      /* buffer with colour correction                */
static int *edgeBuffer;      /* buffer with distances                        */
static int  bufWidth;        /* x-dimension of the buffer                    */
static int  bufHeight;       /* y-dimension of the buffer                    */
static int  imWidth;         /* x-dimension of the image                     */
static int  imHeight;        /* y-dimension of the image                     */
static int  osaCount;        /* oversample count                             */
static int  maskBorder;      /* size of the mask border                      */
static short int intensity;  /* edge intensity                               */
static short int same_mat_redux; /* intensity reduction for boundaries with the same material */
static int  compatible_mode; /* edge positioning compatible with old rederer */
static int  selectmode;      /* 0: only solid faces, 1: also transparent f's */

static int  Aminy;           /* y value of first line in the accu buffer     */
static int  Amaxy;           /* y value of last line in the accu buffer      */
                             /* -also used to clip when zbuffering           */
static char edgeR;           /* Colour for the edges. The edges will receive */ 
static char edgeG;           /* this tint. The colour is fully used!         */
static char edgeB;
/*  static float edgeBlend;  */     /* Indicate opaqueness of the edge colour.      */

/* Local functions --------------------------------------------------------- */
/**
 * Initialise the edge render buffer memory.
 */
void initEdgeRenderBuffer(void);
/**
 * Release buffer memory.
 */
void freeEdgeRenderBuffer(void);

/**
 * Set all distances in the distance buffer to the maximum distance.
 */
void resetDistanceBuffer(void);

/**
 * Insert this distance at these pixel coordinates.
 */
void insertInEdgeBuffer(int x, int y, int dist);

/**
 * Renders enhanced edges. Distances from distRect are used to
 * determine a correction on colourRect
 */
void renderEdges(char * colourRect);

/**
 * Buffer an edge between these two vertices in the e.r. distance buffer.
 */
void fillEdgeRenderEdge(float *vec1, float *vec2);

/**
 * Buffer a face between these two vertices in the e.r. distance buffer.
 */
void fillEdgeRenderFace(float *v1, float *v2, float *v3);

/**
 * Compose the edge render colour buffer.
 */
void calcEdgeRenderColBuf(char * tarbuf);

/**
 * Loop over all objects that need to be edge rendered. This loop determines
 * which objects get to be elected for edge rendering.
 */
int  zBufferEdgeRenderObjects(void);

/**
 * Add edge pixels to the original image. It blends <bron> over <doel>.
 */
void addEdgeOver(unsigned char *dst, unsigned char *src);

/* ------------------------------------------------------------------------- */

void addEdges(
	char * targetbuf,
	int iw, int ih,
	int osanr,
	short int intens, short int intens_redux,
	int compat, int mode,
	float r, float g, float b
	)
{
	float rf, gf ,bf;
	/* render parameters */
	selectmode = mode;
	imWidth    = iw;
	imHeight   = ih;
	compatible_mode = compat;
	osaCount   = osanr;
	intensity  = intens;
	/* Reduction doesn't exceed intensity. */
	same_mat_redux = ((intens_redux < intensity)? intens_redux : intensity);
	
	rf      = r * 255.0;
	if (rf > 255) edgeR = 255; else edgeR = rf;
	gf      = g * 255.0;
	if (gf > 255) edgeG = 255; else edgeG = gf;
	bf      = b * 255.0;
	if (bf > 255) edgeB = 255; else edgeB = bf;

	/* Go! */
	initEdgeRenderBuffer();
	calcEdgeRenderColBuf(targetbuf);
	freeEdgeRenderBuffer();
	
} /* end of void addEdges(char *, int, int, int, short int , int) */

/* ------------------------------------------------------------------------- */

void initEdgeRenderBuffer()
{
	char *ptr;
	int i;
	
	maskBorder = 1; /* for 3 by 3 mask*/
		
	bufWidth   = imWidth + (2 * maskBorder);
	bufHeight  = imHeight + (2 * maskBorder);
	
	/* Experimental: store the material indices. */
	if (same_mat_redux) {
		matBuffer  = MEM_callocN(sizeof(Material*)
					 * bufWidth * bufHeight, "matBuffer");
	}
	
	edgeBuffer = MEM_callocN(sizeof(int) * bufWidth * bufHeight, "edgeBuffer");
	colBuffer  = MEM_callocN(sizeof(char) * 4 * imWidth * imHeight, "colBuffer");

	
	if ((edgeR != 0) || (edgeG != 0) || (edgeB != 0)) {
		/* Set all colbuf pixels to the edge colour. Leave alpha channel     */
		/* cleared. Actually, we could blend in any image here...            */
		ptr = colBuffer;
		for (i = 0; i < imWidth * imHeight; i++, ptr+=4)
		{
			ptr[0] = edgeR;
			ptr[1] = edgeG;
			ptr[2] = edgeB;
			ptr[3] = 0;
		}
	}
	
#ifdef RE_EDGERENDERSAFE
	if (!edgeBuffer || !colBuffer) {
		char *fname = "initEdgeRenderBuffer";
		RE_error(RE_CANNOT_ALLOCATE_MEMORY, fname);
	}
#endif
} /* end of void initEdgeRenderBuffer(void) */

/* ------------------------------------------------------------------------- */
void freeEdgeRenderBuffer(void)
{
	if(edgeBuffer) MEM_freeN(edgeBuffer);
	if(colBuffer)  MEM_freeN(colBuffer);
	if(matBuffer)  MEM_freeN(matBuffer);
} /* end of void freeEdgeRenderBuffer(void) */

/* ------------------------------------------------------------------------- */

void resetDistanceBuffer(void)
{
	int i;
	for(i = 0; i < bufWidth * bufHeight; i++) edgeBuffer[i] = 0x7FFFFFFF;
} /* end of void resetDistanceBuffer(void) */

/* ------------------------------------------------------------------------- */

void insertInEdgeBuffer(int x, int y, int dist)
{
	int index;
#ifdef RE_EDGERENDERSAFE
	char *fname = "insertInEdgeBuffer";
	if ((x < 0) || (x > imWidth ) ||
		(y < 0) || (y > (imHeight-1) ) ) {
		RE_error(RE_EDGERENDER_WRITE_OUTSIDE_BUFFER, fname);
		return;
	}
#endif

	/* +1? */
	index = (y * bufWidth) + x + maskBorder;

	/*exp: just dump a single index here. Maybe we can do more
	 * sophisticated things later on. */
	if (same_mat_redux) {
		matBuffer[index] = mat_cache;
	}
	
	if (edgeBuffer[index] >dist ) edgeBuffer[index] = dist;

} /* end of void insertInEdgeBuffer(int x, int y, int dist) */

/* ------------------------------------------------------------------------- */
/* Modelled after rendercore.c/edge_enhance()                                */
void renderEdges(char *colourRect)
{
	/* use zbuffer to define edges, add it to the image */
	int val, y, x, col, *rz, *rz1, *rz2, *rz3;
	char *cp;
	int targetoffset, heightoffset;
	int i;
	int matdif; /* For now: just a bogus int, 0 when all materials
		     * under the mask are the same, non-0 otherwise*/
	int *matptr_low = 0, *matptr_cent = 0, *matptr_high = 0;
	int matdiffac = 0;

#ifdef RE_EDGERENDER_NO_CORRECTION
	return; /* no edge correction */
#endif
	
#ifdef RE_EDGERENDERSAFE
    fprintf(stderr, "\n*** Activated full error trace on "
            "edge rendering  using:\n\t%s\n\t%s"
			"\n*** Rendering edges at %d intensity", 
            edgeRender_c, edgeRender_h, intensity);
#endif

	
	/* Old renderer uses wrong positions! With the compat switch on, the po- */
	/* sitions will be corrected to be offset in the same way.               */
	if (compatible_mode) {
		targetoffset = 4 * (imWidth - 1);
		heightoffset = -1;
	} else {
		targetoffset = 0;
		heightoffset = 0;
	}
	
	/* Fill edges with some default values. We just copy what is in the edge */
	/* This looks messy, but it appears to be ok.                            */
	edgeBuffer[0]                          = edgeBuffer[bufWidth + 1];
	edgeBuffer[bufWidth - 1]               = edgeBuffer[(2 * bufWidth) - 2];
	edgeBuffer[bufWidth * (bufHeight - 1)] =
		edgeBuffer[bufWidth * (bufHeight - 2) + 1];
	edgeBuffer[(bufWidth * bufHeight) - 1] =
		edgeBuffer[(bufWidth * (bufHeight - 1)) - 2];
	for (i = 1; i < bufWidth - 1; i++) { /* lieing edges */
		edgeBuffer[i] = edgeBuffer[bufWidth + i]; /* bottom*/
		edgeBuffer[((bufHeight - 1)*bufWidth) + i]
			= edgeBuffer[((bufHeight - 2)*bufWidth) + i]; /* top */
	}
	for (i = 1; i < bufHeight - 2; i++) { /* standing edges */
		edgeBuffer[i * bufWidth] = edgeBuffer[(i * bufWidth) + 1]; /* left */
		edgeBuffer[((i + 1) * bufWidth) - 1] =
			edgeBuffer[((i + 1) * bufWidth) - 2]; /* right */
	}

	/* same hack for the materials: */
	if (same_mat_redux) {
		matBuffer[0]                          = matBuffer[bufWidth + 1];
		matBuffer[bufWidth - 1]               = matBuffer[(2 * bufWidth) - 2];
		matBuffer[bufWidth * (bufHeight - 1)] =
			matBuffer[bufWidth * (bufHeight - 2) + 1];
		matBuffer[(bufWidth * bufHeight) - 1] =
			matBuffer[(bufWidth * (bufHeight - 1)) - 2];
		for (i = 1; i < bufWidth - 1; i++) { /* lieing mats */
			matBuffer[i] = matBuffer[bufWidth + i]; /* bottom*/
			matBuffer[((bufHeight - 1)*bufWidth) + i]
				= matBuffer[((bufHeight - 2)*bufWidth) + i]; /* top */
		}
		for (i = 1; i < bufHeight - 2; i++) { /* standing mats */
			matBuffer[i * bufWidth] = matBuffer[(i * bufWidth) + 1]; /* left */
			matBuffer[((i + 1) * bufWidth) - 1] =
				matBuffer[((i + 1) * bufWidth) - 2]; /* right */
		}
	}
		
	/* shift values in zbuffer 3 to the right */
  	rz = edgeBuffer;
	if(rz==0) return;
	
	for(y=0; y < bufHeight * bufWidth; y++, rz++) {
		(*rz)>>= 3;
	}
	
	/* Distance pointers */
	rz1= edgeBuffer;
	rz2= rz1 + bufWidth;
	rz3= rz2 + bufWidth;

	if (same_mat_redux) {
		matptr_low = (int *) matBuffer;
		matptr_cent = matptr_low + bufWidth;
		matptr_high = matptr_cent + bufWidth;
	}
	
	if (osaCount == 1) {
		cp = colourRect + targetoffset;
	} else {
		cp = colBuffer + targetoffset;
	}

	i = 0;
	
	for(y = 0; y < (imHeight + heightoffset) ; y++) {


		/* All these indices are a bit silly. I need to
		 * rewrite this, so all buffers use the same
		 * indexing. */
		for(x = 0;
		    x < imWidth;
		    x++, rz1++, rz2++, rz3++, cp+=4,
			    matptr_low++,
			    matptr_cent++,
			    matptr_high++) {
			
			col= abs( -   rz1[0] -  2*rz1[1] -   rz1[2]
				  - 2*rz2[0] + 12*rz2[1] - 2*rz2[2]
				  -   rz3[0] -  2*rz3[1] -   rz3[2]) / 3;

			/* Several options for matdif:
			 *
			 * - suppress all boundaries with 0 dif
			 *
			 * - weaken col dif? Or decrease intensity by
			 * a factor when non 0 dif??
			 */

			/* exp: matdif is non-0 if the mask-center
			 * material differs from any of the
			 * corners. */

			if (same_mat_redux) {
				matdif = abs (matptr_cent[1] - matptr_low[0])
					+ abs (matptr_cent[1] - matptr_low[1])
					+ abs (matptr_cent[1] - matptr_low[2])
					+ abs (matptr_cent[1] - matptr_cent[0])
					+ abs (matptr_cent[1] - matptr_low[2])
					+ abs (matptr_cent[1] - matptr_high[0])
					+ abs (matptr_cent[1] - matptr_high[1])
					+ abs (matptr_cent[1] - matptr_high[2]);
				
				matdiffac = (matdif ? 0 : same_mat_redux); 
			}
			
			col= ((intensity - matdiffac) * col)>>14;
			if(col>255) col= 255;
			
			/* Colour edge if
			 *
			 * 1. there is an appreciable, non-uniform
			 * gradient,
			 *
			 * 2. there are different materials bordering
			 * on the center pixel
			 */
			if( (col>0)
			    /* && (matdif != 0) */) {
				
				if(osaCount > 1) {
					/* Currently done by tweaking alpha. The colBuffer is     */
					/* filled with pixels of the colour appropriate for the   */
					/* edges. This colour is alpha-blended over the image.    */
					/* This calculation determines how much colour each pixel */
					/* gets.                                                  */
					col/= osaCount;
					val= cp[3]+col;
					if(val>255) cp[3]= 255; else cp[3]= val;
				}
				else {
					/* the pixel is blackened when col is too big */
					val = cp[0] - col;
					if(val<=0) {
						cp[0]= edgeR;
					} else {
						cp[0]= val;
					}
					val = cp[1] - col;
					if(val<=0) {
						cp[1]= edgeG;
					}else {
						cp[1]= val;
					}
					val = cp[2] - col;
					if(val<=0) {
						cp[2]= edgeB;
					} else {
						cp[2]= val;
					}
				}
			}
		}
		rz1+= 2;
		rz2+= 2;
		rz3+= 2;
		if (same_mat_redux) {
			matptr_low += 2;
			matptr_cent += 2;
			matptr_high += 2;
		}
		
	}

} /* end of void renderEdges() */

/* ------------------------------------------------------------------------- */

/* adds src to dst */
void addEdgeOver(unsigned char *dst, unsigned char *src)   
{
   unsigned char inverse;
   unsigned char alpha;
   unsigned int  c;

   alpha = src[3];
  
   if( alpha == 0) return;
   if( alpha == 255) { 
      /* when full opacity, just copy the pixel */
      /* this code assumes an int is 32 bit, fix 

      *(( unsigned int *)dst)= *((unsigned int *)src);
	replaced with memcpy
		*/
       memcpy(dst,src,4);
       return;
   }
  
   /* This must be a special blend-mode, because we get a 'weird' data      */
   /* input format now. With edge = (c_e, a_e), picture = (c_p, a_p), we    */
   /* get: result = ( c_e*a_e + c_p(1 - a_e), a_p ).                        */
  
   inverse = 255 - alpha;
  
   c = ((unsigned int)inverse * (unsigned int) dst[0] + (unsigned int)src[0] *
	(unsigned int)alpha) >> 8;
   dst[0] = c;

   c = ((unsigned int)inverse * (unsigned int) dst[1] + (unsigned int)src[1] *
	(unsigned int)alpha) >> 8;
   dst[1] = c;
   c = ((unsigned int)inverse * (unsigned int) dst[2] + (unsigned int)src[2] *
	(unsigned int)alpha) >> 8;
   dst[2] = c;
}

void calcEdgeRenderColBuf(char* colTargetBuffer)
{

    int keepLooping = 1;
	int sample;
	
	/* zbuffer fix: here? */
	Zmulx= ((float) imWidth)/2.0;
  	Zmuly= ((float) imHeight)/2.0;
	
	/* use these buffer fill functions */    
	zbuffunc     = fillEdgeRenderFace;
	zbuflinefunc = fillEdgeRenderEdge;

	/* always buffer the max. extent */
	Aminy = 0;
	Amaxy = imHeight;
	        
	sample = 0; /* Zsample is used internally !                         */
	while ( (sample < osaCount) && keepLooping ) {
		/* jitter */
		Zjitx= -jit[sample][0];
		Zjity= -jit[sample][1];

		/* should reset dis buffer here */
		resetDistanceBuffer();
		
		/* kick all into a z buffer */
		keepLooping = zBufferEdgeRenderObjects();

		/* do filtering */
		renderEdges(colTargetBuffer);

  		if(RE_local_test_break()) keepLooping = 0; 
  		sample++; 
	}

	/* correction for osa-sampling...*/
	if( osaCount != 1) {
		char *rp, *rt;
		int a;
		
		rt= colTargetBuffer;
		rp= colBuffer;
		for(a = imWidth * imHeight; a>0; a--, rt+=4, rp+=4) {
			/* there seem to be rounding errors here... */
			addEdgeOver(rt, rp);
		}
	}
	
} /*End of void calcEdgeRenderZBuf(void) */

/* ------------------------------------------------------------------------- */
/* Clip flags etc. should still be set. When called in the span of 'normal'  */
/* rendering, this should be ok.                                             */
int zBufferEdgeRenderObjects(void)
{
    int keepLooping; 
    int faceCounter; /* counter for face number */
    Material *ma;
	
    keepLooping = 1;
    ma          = NULL;
    faceCounter = 0;
			
    while ( (faceCounter < R.totvlak) && keepLooping) {
	    if((faceCounter & 255)==0) { Zvlr= R.blovl[faceCounter>>8]; }
	    else Zvlr++;
        
	    ma= Zvlr->mat;

	    /*exp*/
	    mat_cache = ma;

	    /* face number is used in the fill functions */
	    Zvlnr = faceCounter + 1; 
        
	    if(Zvlr->flag & R_VISIBLE) {
			
		    /* here we cull all transparent faces if mode == 0 */
		    if (selectmode || !(ma->mode & MA_ZTRA)) {
			    /* here we can add all kinds of extra selection criteria */
			    if(ma->mode & (MA_WIRE)) zbufclipwire(Zvlr);
			    else {
				    zbufclip(Zvlr->v1->ho,   Zvlr->v2->ho,   Zvlr->v3->ho, 
					     Zvlr->v1->clip, Zvlr->v2->clip, Zvlr->v3->clip);
				    if(Zvlr->v4) {
					    Zvlnr+= 0x800000; /* in a sense, the 'adjoint' face */
					    zbufclip(Zvlr->v1->ho,   Zvlr->v3->ho,   Zvlr->v4->ho, 
						     Zvlr->v1->clip, Zvlr->v3->clip, Zvlr->v4->clip);
				    }
			    }
		    }
	    };
	    if(RE_local_test_break()) keepLooping = 0; 
	    faceCounter++;
    }
    return keepLooping;
} /* End of int zBufferEdgeRenderObjects(void) */

/* ------------------------------------------------------------------------- */

void fillEdgeRenderFace(float *v1, float *v2, float *v3)  
{
	/* Coordinates of the vertices are specified in ZCS */
	double z0; /* used as temp var*/
	double xx1;
	double zxd,zyd,zy0, tmp;
	float *minv,*maxv,*midv;
	int zverg,x;
	int my0,my2,sn1,sn2,rectx,zd;
	int y,omsl,xs0,xs1,xs2,xs3, dx0,dx1,dx2/*  , mask */;
	int linex, liney, xoffset, yoffset; /* pointers to the pixel number */

	/* These used to be doubles.  We may want to change them back if the     */
	/* loss of accuracy proves to be a problem? There does not seem to be    */
	/* any performance issues here, so I'll just keep the doubles.           */
	/*  	float vec0[3], vec1[3], vec2[3]; */
	double vec0[3], vec1[3], vec2[3];

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

	if(minv[1] == maxv[1]) return;	/* security, for 'zero' size faces */

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
	MTC_diff3DFF(vec1, v1, v2);
	MTC_diff3DFF(vec2, v2, v3);
	MTC_cross3Double(vec0, vec1, vec2);

	/* cross product of two of the sides is 0 => this face is too small */
	if(vec0[2]==0.0) return;

	if(midv[1] == maxv[1]) omsl= my2;
	if(omsl < Aminy) omsl= Aminy-1;  /* that way it does the first loop entirely */

	while (my2 > Amaxy) {  /* my2 can really be larger */
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
  	rectx   = imWidth;
	yoffset = my2;
	xoffset = 0;
	
	xs3= 0;		/* flag */
	if(dx0>dx1) {
		MTC_swapInt(&xs0, &xs1);
		MTC_swapInt(&dx0, &dx1);
		xs3= 1;	/* flag */

	}

	liney = yoffset;
	for(y=my2;y>omsl;y--) {

		sn1= xs0>>16;
		xs0+= dx0;

		sn2= xs1>>16;
		xs1+= dx1;

		sn1++;

		if(sn2>=rectx) sn2= rectx-1;
		if(sn1<0) sn1= 0;
		zverg= (int) CLAMPIS((sn1*zxd+zy0), INT_MIN, INT_MAX);

		if ((sn1 < 0) || (sn1>imWidth) ) printf("\n sn1 exceeds line");
		linex = xoffset + sn1;
		liney = yoffset;
		
		x= sn2-sn1;
		
		while(x>=0) {
			insertInEdgeBuffer(linex , liney, zverg); /* line y not needed here */
			zverg+= zd;
			linex++;
			x--;
		}
		zy0-= zyd;
		yoffset--;
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
		
		linex = sn1;
		liney = yoffset;
				
		x= sn2-sn1;
      
		while(x>=0) {
			insertInEdgeBuffer(linex, liney, zverg); /* line y not needed here */
			zverg+= zd;
			linex++;
			x--;
		}
		zy0-=zyd;
		yoffset--;
	}
} /* end of void fillEdgeRenderFace(float *v1, float *v2, float *v3) */

/* ------------------------------------------------------------------------- */

void fillEdgeRenderEdge(float *vec1, float *vec2)
{
	int start, end, x, y, oldx, oldy, ofs;
	int dz, vergz/*  , mask */;
	float dx, dy;
	float v1[3], v2[3];
	int linex, liney;
	int xoffset, yoffset;

	
	dx= vec2[0]-vec1[0];
	dy= vec2[1]-vec1[1];
	
	if(fabs(dx) > fabs(dy)) {

		/* alle lines from left to right */
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
		if(end >= imWidth) end = imWidth - 1;
		
		oldy= floor(v1[1]);
		dy/= dx;
		
		vergz= v1[2];
		dz= (v2[2]-v1[2])/dx;
		
		yoffset = oldy;
		xoffset = start;
		
		if(dy<0) ofs= -imWidth;
		else ofs= imWidth;

		liney = yoffset;
		linex = xoffset;
		
		for(x= start; x<=end; x++, xoffset++) {
			
			y= floor(v1[1]);
			if(y!=oldy) {
				oldy= y;
				liney++;
			}
			
			if(x>=0 && y>=Aminy && y<=Amaxy) {
				insertInEdgeBuffer(linex , liney, vergz);
			}
			
			v1[1]+= dy;
			vergz+= dz;
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
		dz= (v2[2]-v1[2])/dy;

		yoffset = start;
		xoffset = oldx;
				
		if(dx<0) ofs= -1;
		else ofs= 1;

		linex = xoffset;
		liney = yoffset;
		
		for(y= start; y<=end; y++, liney++) {
			
			x= floor(v1[0]);
			if(x!=oldx) {
				oldx= x;
				linex += ofs;
			}
			
			if(x>=0 && y>=Aminy && (x < imWidth)) {
				insertInEdgeBuffer(linex, liney, vergz);
			}
			
			v1[0]+= dx;
			vergz+= dz;
		}
	}
} /* End of void fillEdgeRenderEdge(float *vec1, float *vec2) */

/* ------------------------------------------------------------------------- */

/* eof edgeRender.c */
