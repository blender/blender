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

/* local includes (from the render module) */
#include "RE_callbacks.h"
#include "render.h"       /* all kinds of stuff                              */
#include "render_intern.h"
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
#include "vanillaRenderPipe_int.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

/* crud */
#define MIN2(x,y)               ( (x)<(y) ? (x) : (y) )

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

extern unsigned int Zvlnr;           /* Face rendering pointer and counter: these    */
extern VlakRen *Zvlr;        /* are used for 'caching' render results.       */

 /* These function pointers are used for z buffer filling.    */
extern void (*zbuffunc)(float *, float *, float *);
extern void (*zbuflinefunc)(float *, float *);

extern char cmask[256];      /* When a pixel is supersampled, we must        */
extern char *centmask;       /* compute its colour on a point _on_ the face. */
                             /* These two are used to compute an offset to   */
                             /* guarantee we use valid coordinates.          */

/* unsorted */
extern float fmask[256];
extern unsigned short usegamtab, shortcol[4], 
	*mask1[9], *mask2[9],/*   *igamtab1, */ *igamtab2/*,   *gamtab */;

extern RE_APixstrExt *APixbufExt;/*Zbuffer: linked list of face, halo indices*/

/* Globals : --------------------------------------------------------------- */

RE_COLBUFTYPE *AColourBuffer; /* Buffer for colours of 1 line of pixels      */
static int     Aminy;         /* y value of first line in the accu buffer    */
static int     Amaxy;         /* y value of last line in the accu buffer     */
                              /* -also used to clip when zbuffering          */

/* Buffer width refers to the size of the buffers we build. Image size is    */
/* the same as R.rectx, R.recty.                                             */
static int     imageHeight;   /* image size in pixels in y direction         */
static int     imageWidth;    /* image size in pixels in x direction         */
static int     bufferHeight;  /* image size in pixels in y direction         */
static int     bufferWidth;   /* image size in pixels in x direction         */
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
RE_COLBUFTYPE  collector[4];  /* used throughout as pixel colour accu        */
RE_COLBUFTYPE  sampcol[RE_MAX_OSA_COUNT * 4]; /* subpixel accu buffer        */

/* ------------------------------------------------------------------------- */
/* Local (for now) */
/*  void integrateStack(struct RE_faceField* stack, */
/*  					int ptr, */
/*  					float x,  */
/*  					float y,  */
/*  					int osaNr); */
void integratePerSubStack(struct RE_faceField* stack,
						  int ptr,
						  float x, 
						  float y, 
						  int osaNr);

/* ------------------------------------------------------------------------- */

void zBufShadeAdvanced()
{
    int      y, keepLooping = 1;
    float xjit = 0.0, yjit = 0.0;

    Zjitx=Zjity= -0.5; /* jitter preset: 0.5 pixel */

	/* EDGE: for edge rendering we should compute a larger buffer, but this  */
	/* may require modifications at a deeper level. For now, we just         */
	/* 'ignore' edge pixels.                                                 */
	imageHeight  = R.recty;
	imageWidth   = R.rectx;
	bufferHeight = R.recty;
	bufferWidth  = R.rectx;
   
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
        jit[0][0] = 0.45;
        jit[0][1] = 0.45;        
    }

    RE_setwindowclip(0, -1); /* just to be sure, reset the view matrix       */

	initRenderBuffers(bufferWidth);

	/* ugh! should be converted sooner!! */
	switch (R.r.alphamode) {
	case R_ALPHAKEY:	
		setSkyBlendingMode(RE_ALPHA_KEY);
		break;
	case R_ALPHAPREMUL:	
		setSkyBlendingMode(RE_ALPHA_PREMUL);
		break;
/* not there... this is the default case */
/*  	case R_ALPHASKY:	 */
/*  		setSkyBlendingMode(RE_ALPHA_SKY); */
/*  		break; */
	default:
		setSkyBlendingMode(RE_ALPHA_SKY);		
	}
	
    y = 0;
    while ( (y < bufferHeight) && keepLooping) {
		calcZBufLine(y);

		renderZBufLine(y);
		transferColourBufferToOutput(y);
		
		if(y & 1) RE_local_render_display(y-1, y, imageWidth, imageHeight, R.rectot);		

		if(RE_local_test_break()) keepLooping = 0;
		y++; 
    }
	freeRenderBuffers();

	/* Edge rendering is done purely as a post-effect                        */
	if(R.r.mode & R_EDGE) {
		addEdges((char*)R.rectot, imageWidth, imageHeight,
			 osaNr,
			 R.r.edgeint, R.r.same_mat_redux,
			 G.compat, G.notonlysolid,
			 R.r.edgeR, R.r.edgeG, R.r.edgeB);
	}

	add_halo_flare(); /* from rendercore */
	
	if (!(R.r.mode & R_OSA)) {
		jit[0][0] = xjit;
		jit[0][1] = yjit; 
	}
    
} /* end of void zbufshadeAdvanced() */

/* ------------------------------------------------------------------------- */

void initRenderBuffers(int bwidth) 
{

    /* The +1 is needed because the fill-functions use a +1 offset when      */
    /* filling in pixels. Mind that also the buffer-clearing function needs  */
    /* this offset (done in calcZBufLine).                                   */
	/* The offset is wrong: it shouldn't be there. I need to fix this still. */
    AColourBuffer = MEM_callocN(4 * sizeof(RE_COLBUFTYPE) * bwidth, 
                            "Acolrow");
	zBufferWidth = bwidth + 1;
	initZbuffer(bwidth + 1);

    Aminy= -1000; /* indices of lines in the z buffer: no lines buffered     */
    Amaxy= -1000;

	/* Use slider when the gamma button is pressed. */
	if (R.r.mode & R_GAMMA) {		
		makeGammaTables(R.r.gamma);
		setDoGamma(1);
	} else {
		/*
		  Needed for spotlights! Maybe a separate gammatable would be
		  required here
		*/
		makeGammaTables(1.0);
		setDoGamma(0);
	}

} /* End of void initZBuffers(void) */

/* ------------------------------------------------------------------------- */

void freeRenderBuffers(void) {	
    if (AColourBuffer) MEM_freeN(AColourBuffer);
	freeZbuffer();
} /* End of void freeZBuffers(void) */

/* ------------------------------------------------------------------------- */

void calcZBufLine(int y)
{

    int part;
    int keepLooping = 1;

    if(y<0) return;

	/* zbuffer fix: here? */
	Zmulx= ((float) bufferWidth)/2.0;
  	Zmuly= ((float) bufferHeight)/2.0;

	
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
        if(Amaxy >= bufferHeight) Amaxy = bufferHeight - 1;
		resetZbuffer();
        
        Zsample = 0; /* Zsample is used internally !                         */
        while ( (Zsample < osaNr) && keepLooping ) {
            /* Apply jitter to this pixel. The jitter offsets are globals.   */
            /* They are added in zbufclip()                                  */
            /* Negative: these offsets are added to the vertex coordinates   */
            /* so it equals translating viewpoint over the positive vector.  */
            Zjitx= -jit[Zsample][0];
            Zjity= -jit[Zsample][1];

            keepLooping = fillZBufDistances();
            
            if(RE_local_test_break()) keepLooping = 0;
            Zsample++;
        }
    };
    
} /*End of void calcZBufLine(int y) */

/* ------------------------------------------------------------------------- */

int countAndSortPixelFaces(int zrow[RE_MAX_FACES_PER_PIXEL][RE_PIXELFIELDSIZE], 
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
} /* end of int countAndSortPixelFaces(int* zrow,RE_APixstrExt *ap ) */

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
int composeStack(int zrow[RE_MAX_FACES_PER_PIXEL][RE_PIXELFIELDSIZE],
				 struct RE_faceField* stack, int ptr,
				 int totvlak, float x, float y, int osaNr) {

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

	VR_covered = 0;
    for(i = 0; i < osaNr; i++) alphathreshold[i] = 0.0;
    saturationthreshold = ( (1<<osaNr) - 1);

    while ( (!saturated || (saturated && inconflict) ) && (totvlak > 0) ) {
        totvlak--;
			
        i= centmask[ zrow[totvlak][RE_MASK] ]; /* recenter sample position - */
        xs= (float)x+centLut[i &  15];
        ys= (float)y+centLut[i >> 4];
        
        /* stack face ----------- */
		stack[ptr].mask     = zrow[totvlak][RE_MASK];
		stack[ptr].data     = renderPixel(xs, ys, zrow[totvlak], stack[ptr].mask);
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
	renderSkyPixelFloat(xs, ys);
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

/*
  Start resolving the conflict: the stack is primed to the top-most valid
  layer on the stack. Call this layer n. Layer n has a conflict count of c.
  This means layers [ n - c, ..., n ]
 */
int resolveConflict(struct RE_faceField* stack, int ptr, float x, float y) {
	int face;
    int layer;
    float dx, dy;
	float xs = 0.0;
	float ys = 0.0;  /* coordinates for the render-spot              */
	int i;

    for(i = 0; i< osaNr; i++) { /* per bin, buffer all faces         */
		dx = jit[i][0];
        dy = jit[i][1];
        xs = (float)x + dx;
		ys = (float)y + dy;   

		face = 0;  /* only counts covering faces ------------------- */
        layer = 0; /* counts all faces ----------------------------- */

        while (layer < stack[ptr].conflictCount) {
			if ( (1<<i) & stack[ptr - layer].mask)  {
				VR_cbuf[face][0] = 
					calcDepth(xs, ys, 
							  stack[ptr - layer].data,
							  stack[ptr - layer].faceType);
                VR_cbuf[face][1] = ptr - layer;
                face++;
			}
			layer++;
        }
        qsort(VR_cbuf, face, sizeof(int)*2, vergzvlak); 
        for(layer = 0; layer < face; layer++) {
			blendOverFloat(stack[VR_cbuf[layer][1]].faceType, /* type */
						   sampcol + (4 * i),                 /* dest */
						   stack[VR_cbuf[layer][1]].colour,   /* src */
						   stack[VR_cbuf[layer][1]].data);    /* data */
		}
	}

	/* The number of layers that were handled. This is how many layers the   */
	/* top-level algorithm needs to skip.                                    */
	return stack[ptr].conflictCount;
}

/* The colour stack is blended down in a pretty straight-forward manner, or  */
/* a part of the stack is re-evaluated to resolve the conflict.              */
/* About 25-30% of rendering time is eaten here!                             */
void integrateStack(struct RE_faceField* stack, int ptr,
					float x, 
					float y, 
					int osaNr) {
	/* sample the colour stack: back to front ----------------------------   */
	/*    is there a possible way to ignore alpha? this would save 25% work  */
	ptr--;
	/* Little different now: let ptr point to the topmost valid face.*/
	while (ptr >= 0) {
		if (stack[ptr].conflictCount == 0) {
			/*
			  No conflict: sample one colour into multiple bins
			*/
			blendOverFloatRow(stack[ptr].faceType, 
							  sampcol, 
							  stack[ptr].colour,
							  stack[ptr].data, 
							  stack[ptr].mask, 
							  osaNr);
			ptr--;
		} else {
			/*
			  Recalc all z-values, and integrate per sub-pixel. 
			*/
  		 	ptr -= resolveConflict(stack, ptr, x, y);
		}
	}

	/* Done sampling. Now we still need to fill in pixels that were not      */
	/* covered at all It seems strange that we have to check for empty alpha */
	/* but somehow this is necessary. Check out the cover condition :)....   */

	/* It is important that we find a more efficient algorithm here, because */
	/* this little loop eats _lots_ of cycles.                               */

	/* Should be integrated in the rest of the rendering... */

	if(R.flag & R_LAMPHALO) {
		float halocol[4];
		int i;
		
		renderSpotHaloPixel(x, y, halocol);
		/* test seems to be wrong? */
		if (halocol[3] > RE_EMPTY_COLOUR_FLOAT) {
			for (i = 0; i < osaNr; i++) { 
				/* here's a pinch: if the pixel was only covered by a halo,  */
				/* we still need to fill spothalo. How do we detect this?    */
  				if (!(VR_covered & (1 << i)))
					/* maybe a copy is enough here... */
					addAlphaOverFloat(sampcol + (4 * i), halocol);
			}
		}
	} 
}

/**
 * New approach: sample substacks. Each substack is first copied into
 * a stack buffer, and then blended down.
 * */
void integratePerSubStack(struct RE_faceField* stack,
						  int ptr,
						  float x, 
						  float y, 
						  int osaNr) {
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
void renderZBufLine(int y) {
    int  zrow[RE_MAX_FACES_PER_PIXEL][RE_PIXELFIELDSIZE];
    RE_APixstrExt *ap;       /* iterator for the face-lists                  */
	int apteller;
    int x;                   /* pixel counter                                */
    RE_COLBUFTYPE *colbuf;   /* pointer into the line buffer                 */
    RE_COLBUFTYPE *j = NULL; /* generic pixel pointer                        */
    int i;                   /* yet another counter                          */
    int stackDepth;          /* faces-behind-this-pixel counter              */
	struct RE_faceField RE_OSAstack[RE_MAX_FACES_PER_PIXEL + 1]; 
	int RE_OSAstack_ptr;    /* Points to the lowest empty field. The indexed */
                            /* field is NOT readable.                        */
	
    /* Prepare buffers and iterators */
    colbuf    = AColourBuffer;
    eraseColBuf(AColourBuffer);
    ap        = APixbufExt + (zBufferWidth * (y - Aminy));
	apteller  = (zBufferWidth * (y - Aminy));
	
    /* Rendering: give the right colour to this pixel (shade it) */
	for( x = 0; x < bufferWidth; x++, ap++, colbuf+=4) {
		if(ap->t[0]) {
            /* reset sample collector */
            j = sampcol;
            for(i = 0; i < osaNr; i++, j+=4) { 
                j[0] = RE_ZERO_COLOUR_FLOAT; j[1] = RE_ZERO_COLOUR_FLOAT; 
                j[2] = RE_ZERO_COLOUR_FLOAT; j[3] = RE_ZERO_COLOUR_FLOAT;
            };

            /* a. count and sort number of faces */
            stackDepth = countAndSortPixelFaces(zrow, ap);
			
            /* b,c. oversample all subpixels, then integrate                 */
			RE_OSAstack_ptr = 0;
			RE_OSAstack_ptr = composeStack(zrow,
										   RE_OSAstack, RE_OSAstack_ptr,
										   stackDepth, x, y, osaNr);
  			integratePerSubStack(RE_OSAstack, RE_OSAstack_ptr, 
  								 x, y, osaNr); 
			
			/* d. Gamma corrected blending                                   */
			sampleFloatColV2FloatColV(sampcol, colbuf, osaNr);
		} else {
			/* Remember to do things back-to-front!                          */
			
			/* This is a bit dirty. Depending on sky-mode, the pixel is      */
			/* blended in differently.                                       */
			renderSkyPixelFloat(x, y);
			cpFloatColV(collector, colbuf);

			/* Spothalos are part of the normal pixelshader, so for covered  */
			/* pixels they are handled ok. They are 'normally' alpha blended */
			/* onto the existing colour in the collector.                    */
			if(R.flag & R_LAMPHALO) {
				renderSpotHaloPixel(x, y, collector);
				addAlphaOverFloat(colbuf, collector);
			}
			
		}
    } /* End of pixel loop */
    
} /* End of void renderZBufLine(int y) */


/* ------------------------------------------------------------------------- */

int fillZBufDistances() 
{
    int keepLooping = 1;

    keepLooping = zBufferAllFaces(); /* Solid and transparent faces*/
    keepLooping = zBufferAllHalos() && keepLooping; /* ...and halos*/
    return keepLooping;

} /* End of void fillZBufDistances() */

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

int zBufferAllFaces(void) 
{
    int keepLooping = 1; 
    int faceCounter; /* counter for face number */
    float vec[3], hoco[4], mul, zval, fval; 
    Material *ma=0;
    
    faceCounter = 0;

/*  	printf("Going to buffer faces:\n"); */
/*  	printf("\tfirst pass:\n"); */

/*  	RE_treat_face_as_opaque = 1; */
	
	while ( (faceCounter < R.totvlak) && keepLooping) {
		if((faceCounter & 255)==0) { Zvlr= R.blovl[faceCounter>>8]; }
		else Zvlr++;
		
		ma= Zvlr->mat;
		
		/* VERY dangerous construction... zoffs is set by a slide in the ui */
		/* so it should be safe...                                          */
		if((ma->mode & (MA_ZTRA)) && (ma->zoffs != 0.0)) {
			mul= 0x7FFFFFFF;
			zval= mul*(1.0+Zvlr->v1->ho[2]/Zvlr->v1->ho[3]);
			
			VECCOPY(vec, Zvlr->v1->co);
			/* z is negatief, wordt anders geclipt */ 
			vec[2]-= ma->zoffs;
			RE_projectverto(vec, hoco); /* vec onto hoco */
			fval= mul*(1.0+hoco[2]/hoco[3]);
			
			Azvoordeel= (int) fabs(zval - fval );
		} else {
			Azvoordeel= 0;
		} 
		/* face number is used in the fill functions */
		Zvlnr = faceCounter + 1;
		
		if(Zvlr->flag & R_VISIBLE) { /* might test for this sooner...  */
			
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
		if(RE_local_test_break()) keepLooping = 0; 
		faceCounter++;
	}
	
    return keepLooping;
} /* End of int zBufferAllFaces(void) */

/* ------------------------------------------------------------------------- */
/* We cheat a little here: we only fill the halo on the first pass, and we   */
/* set a full complement of mask flags. This can be done because we consider */
/* halos to be flat (billboards), so we do not have to correct the z range   */
/* every time we insert a halo. Also, halos fall off to zero at the edges,   */
/* so we can safely render them in pixels where they do not exist.           */
int zBufferAllHalos(void)
{
    HaloRen *har = NULL;
    unsigned int haloCounter = 0;
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
} /* end of int zbufferAllHalos(void) */

/* ------------------------------------------------------------------------- */
void zBufferFillHalo(void)
{
    /* so far, intentionally empty */
} /* end of void zBufferFillHalo(void) */

/* ------------------------------------------------------------------------- */
void zBufferFillFace(float *v1, float *v2, float *v3)  
{
	/* Coordinates of the vertices are specified in ZCS */
	int apteller, apoffsetteller;
	double z0; /* used as temp var*/
	double xx1;
	double zxd,zyd,zy0, tmp;
	float *minv,*maxv,*midv;
	register int zverg,zvlak,x;
	int my0,my2,sn1,sn2,rectx,zd;
	int y,omsl,xs0,xs1,xs2,xs3, dx0,dx1,dx2, mask;

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
	zvlak= Zvlnr;

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
			insertObject(apteller, /*  RE_treat_face_as_opaque, */ Zvlnr, RE_POLY, zverg, mask);
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
			insertObject(apteller, /*  RE_treat_face_as_opaque, */ Zvlnr, RE_POLY, zverg, mask);
			zverg+= zd;
			apteller++;
			x--;
		}
		
		zy0-=zyd;
		apoffsetteller -= rectx;
	}
} /* end of void zBufferFillFace(float *v1, float *v2, float *v3) */

/* ------------------------------------------------------------------------- */

void zBufferFillEdge(float *vec1, float *vec2)
{
	int apteller;
	int start, end, x, y, oldx, oldy, ofs;
	int dz, vergz, mask;
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
				insertObject(apteller, /*  RE_treat_face_as_opaque, */ Zvlnr, RE_POLY, vergz, mask);
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
		vergz-= Azvoordeel;
		dz= (v2[2]-v1[2])/dy;

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
				insertObject(apteller, /*  RE_treat_face_as_opaque, */ Zvlnr, RE_POLY, vergz, mask);
			}
			
			v1[0]+= dx;
			vergz+= dz;
		}
	}
} /* End of void zBufferFillEdge(float *vec1, float *vec2) */


/* ------------------------------------------------------------------------- */
/* Colour buffer related:                                                    */
/* This transforms the 4 inputvalues RE_COLBUFTYPE to a new value            */
/* It expects the values R.r.postigamma, R.r.postmul and R.r.postadd.         */
/* This is the standard transformation, more elaborate tools are for later.  */
/* ------------------------------------------------------------------------- */
void std_transFloatColV2CharColV( RE_COLBUFTYPE *buf, char *target)
{
	float fval;
	
	/* alpha */
	if(buf[3]<=0.0) target[3]= 0;
	else if(buf[3]>1.0) target[3]= 255;
	else target[3]= 255.0*buf[3];
	
	if(R.r.postgamma==1.0) {
		/* r */
		fval= R.r.postmul*buf[0] + R.r.postadd;
		if(fval<=0.0) target[0]= 0;
		else if(fval>1.0) target[0]= 255;
		else target[0]= 255.0*fval;

		/* g */
		fval= R.r.postmul*buf[1] + R.r.postadd;
		if(fval<=0.0) target[1]= 0;
		else if(fval>1.0) target[1]= 255;
		else target[1]= 255.0*fval;

		/* b */
		fval= R.r.postmul*buf[2] + R.r.postadd;
		if(fval<=0.0) target[2]= 0;
		else if(fval>1.0) target[2]= 255;
		else target[2]= 255.0*fval;

	}
	else {
		/* putting the postmul within the pow() gives an
		 * easier control for the user, values from 1.0-2.0
		 * are relevant then
		 */
	
	
		/* r */
		fval= pow(R.r.postmul*buf[0], R.r.postigamma) + R.r.postadd;
		if(fval<=0.0) target[0]= 0;
		else if(fval>1.0) target[0]= 255;
		else target[0]= 255.0*fval;

		/* g */
		fval=pow( R.r.postmul*buf[1], R.r.postigamma) + R.r.postadd;
		if(fval<=0.0) target[1]= 0;
		else if(fval>1.0) target[1]= 255;
		else target[1]= 255.0*fval;

		/* b */
		fval= pow(R.r.postmul*buf[2], R.r.postigamma) + R.r.postadd;
		if(fval<=0.0) target[2]= 0;
		else if(fval>1.0) target[2]= 255;
		else target[2]= 255.0*fval;
	}

} /* end of void std_transFloatColV2CharColV( RE_COLBUFTYPE *buf, uchar *target) */


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

---------------------------------------------------------------------------- */
void transferColourBufferToOutput(int y)
{
    /* Copy the contents of AColourBuffer to R.rectot + y * R.rectx */
    int x = 0;
    RE_COLBUFTYPE *buf = AColourBuffer;
    char *target = (char*) (R.rectot + (y * imageWidth));

	/* Copy the first <imageWidth> pixels. We can do some more clipping on    */
	/* the z buffer, I think.                                                 */
	while (x < imageWidth) {

		std_transFloatColV2CharColV(buf, target);
		
		/* old function was: leave it for test */
/*  		cpFloatColV2CharColV(buf, target); */

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
} /* end of void transferColourBufferToOutput(int y) */

/* ------------------------------------------------------------------------- */

void eraseColBuf(RE_COLBUFTYPE *buf) {
    /* By definition, the buffer's length is 4 * R.rectx items */
    int i = 0;
/*      while (i < 4 * R.rectx) { */
    while (i < 4 * bufferWidth) {
        *buf = RE_ZERO_COLOUR_FLOAT;
        buf++; i++;
    }
} /* End of void eraseColBuf(RE_COLBUFTYPE *buf) */

/* ------------------------------------------------------------------------- */

int calcDepth(float x, float y, void *data, int type)
{
    float view[3];
	
    if (type & RE_POLY) {
        VlakRen* vlr = (VlakRen*) data;
        VertRen* v1;
        float dvlak, deler, fac, hoco_z, hoco_w;
        int zbuf_co;
        
        v1 = vlr->v1;
        
        /* vertex dot face normal: WCS */
        dvlak= v1->co[0]*vlr->n[0]+v1->co[1]*vlr->n[1]+v1->co[2]*vlr->n[2]; 
        
        /* jitter has been added to x, y ! */
        /* view vector view: screen coords */
		view[0]= (x+(R.xstart) + 0.5  );
        
        if(R.flag & R_SEC_FIELD) {
            if(R.r.mode & R_ODDFIELD) view[1]= (y + R.ystart)*R.ycor;
            else view[1]= (y+R.ystart + 1.0)*R.ycor;
        } else view[1]= (y+R.ystart  + 0.5 )*R.ycor;
        

		/* for pano, another rotation in the xz plane is needed.... */

        /* this is ok, in WCS */
        view[2]= -R.viewfac;  /* distance to viewplane */
        
        /* face normal dot view vector: but how can this work? */
		deler = MTC_dot3Float(vlr->n, view);
        if (deler!=0.0) fac = dvlak/deler;
        else fac = 0.0;
        
        /* indices are wrong.... but gives almost the right value? */
        hoco_z =  (fac*view[2]) * R.winmat[2][2] + R.winmat[3][2]; 
        hoco_w =  (fac*view[2]) * R.winmat[2][3] + R.winmat[3][3]; 
        
        zbuf_co = 0x7FFFFFFF*(hoco_z/hoco_w);            
        
        return  zbuf_co; /* z component of R.co */
    } else if (type & RE_HALO) {
        HaloRen* har = (HaloRen*) data;
        return har->zBufDist;
    }
    return 0;
} /* end of int calcDepth(float x, float y, void* data, int type) */

/* Maybe these two should be in pixelblendeing.c---------------------------- */

void blendOverFloat(int type, float* dest, float* source, void* data)
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

} /* end of void blendOverFloat(int , float*, float*, void*) */

/* ------------------------------------------------------------------------- */
void blendOverFloatRow(int type, float* dest, float* source, 
                       void* data, int mask, int osaNr) 
{
    
    if (type & RE_POLY) {
        VlakRen *ver = (VlakRen*) data;
        if ((ver->mat != NULL) 
            && (ver->mat->add > RE_FACE_ADD_THRESHOLD)) {
            char addf = (ver->mat->add * 255.0);
            addAddSampColF(dest, source, mask, osaNr, addf);
        } else {
            addOverSampColF(dest, source, mask, osaNr);
        }
    } else if (type & RE_HALO) {
        HaloRen *har = (HaloRen*) data;
        addAddSampColF(dest, source, mask, osaNr, har->add);
    } else if (type & RE_SKY) {
            addOverSampColF(dest, source, mask, osaNr);		
	}
} /* end of void blendOverFloatRow(int, float*, float*, void*) */

/* ------------------------------------------------------------------------- */

/* eof vanillaRenderPipe.c */
