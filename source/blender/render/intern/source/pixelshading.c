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
 * Shading of pixels
 *
 * 11-09-2000 nzc
 *
 * $Id$
 *
 */

#include <math.h>
#include "BLI_arithb.h"

/* External modules: */
#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"
#include "MTC_matrixops.h"
#include "MTC_vectorops.h"

#include "DNA_camera_types.h"
#include "DNA_object_types.h"
#include "DNA_mesh_types.h"
#include "DNA_image_types.h"
#include "DNA_texture_types.h"
#include "DNA_lamp_types.h"

#include "BKE_global.h"
#include "BKE_texture.h"
#include "BKE_utildefines.h"

#include "render.h"
#include "texture.h"
#include "render_intern.h"

#include "vanillaRenderPipe_types.h"
#include "pixelblending.h"
#include "rendercore.h" /* for some shading functions... */
#include "zbufferdatastruct.h"

#include "renderHelp.h"

#include "gammaCorrectionTables.h"
#include "errorHandler.h"
#include "pixelshading.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif


/* The collector is the communication channel with the render pipe.          */
extern RE_COLBUFTYPE  collector[4];  /* used throughout as pixel colour accu */

/* ton: 
  - unified render now uses face render routines from rendercore.c
  - todo still: shalo render and sky routines */

/* ------------------------------------------------------------------------- */

void *renderPixel(float x, float y, int *obdata, int mask)
{
    void* data = NULL;
    
    if (obdata[3] & RE_POLY) {
        /* face pixels aren't rendered in floats yet, so we wrap it here */
		data = shadepixel(x, y, obdata[1], mask, collector);
    }
    else if (obdata[3] & RE_HALO) {
        data = renderHaloPixel(x, y, obdata[1]);
    }
	else if( obdata[1] == 0 ) {	
		/* for lamphalo, but doesn't seem to be called? Actually it is, and  */
		/* it returns NULL pointers. */
        data = shadepixel(x, y, obdata[1], mask, collector);
 	}
    return data;
   
} /* end of void renderPixel(float x, float y, int *obdata) */

/* ------------------------------------------------------------------------- */

void renderSpotHaloPixel(float x, float y, float* target)
{
	shadepixel(x, y, 0, 0, target);
}


/* ------------------------------------------------------------------------- */
static unsigned int calcHaloZ(HaloRen *har, unsigned int zz)
{

	if(har->type & HA_ONLYSKY) {
		if(zz!=0x7FFFFFFF) zz= 0;
	}
	else {
		zz= (zz>>8);
		if(zz<0x800000) zz= (zz+0x7FFFFF);
		else zz= (zz-0x800000);
	}
	return zz;
}

void *renderHaloPixel(float x, float y, int haloNr) 
{
    HaloRen *har = NULL;
    float dist = 0.0;
    unsigned int zz = 0;

    /* Find har to go with haloNr */
    har = RE_findOrAddHalo(haloNr);
                    
    /* zz is a strange number... This call should effect that halo's are  */
    /* never cut? Seems a bit strange to me now...                        */
    /* This might be the zbuffer depth                                    */
    zz = calcHaloZ(har, 0x7FFFFFFF);

    /* distance of this point wrt. the halo center. Maybe xcor is also needed? */
    dist = ((x - har->xs) * (x - har->xs)) 
        +  ((y - har->ys) * (y - har->ys) * R.ycor * R.ycor) ;

    collector[0] = RE_ZERO_COLOUR_FLOAT; collector[1] = RE_ZERO_COLOUR_FLOAT; 
    collector[2] = RE_ZERO_COLOUR_FLOAT; collector[3] = RE_ZERO_COLOUR_FLOAT;

    if (dist < har->radsq) {
        shadeHaloFloat(har, collector, zz, dist, 
					  (x - har->xs), (y - har->ys) * R.ycor, har->flarec);
    }; /* else: this pixel is not rendered for this halo: no colour */

    return (void*) har;

} /* end of void* renderHaloPixel(float x, float y, int haloNr) */

/* ------------------------------------------------------------------------- */

extern float hashvectf[];
void shadeHaloFloat(HaloRen *har, 
					float *col, unsigned int zz, 
					float dist, float xn, 
					float yn, short flarec)
{
	/* fill in col */
	/* migrate: fill collector */
	float t, zn, radist, ringf=0.0, linef=0.0, alpha, si, co, colf[4];
	int a;
   
	if(R.wrld.mode & WO_MIST) {
       if(har->type & HA_ONLYSKY) {
           /* stars but no mist */
           alpha= har->alfa;
       }
       else {
           /* a but patchy... */
           R.zcor= -har->co[2];
           alpha= mistfactor(har->co)*har->alfa;
       }
	}
	else alpha= har->alfa;
	
	if(alpha==0.0) {
		col[0] = 0.0;
		col[1] = 0.0;
		col[2] = 0.0;
		col[3] = 0.0;
		return;
	}

	radist= sqrt(dist);

	/* watch it: not used nicely: flarec is set at zero in pixstruct */
	if(flarec) har->pixels+= (int)(har->rad-radist);

	if(har->ringc) {
		float *rc, fac;
		int ofs;
		
		/* per ring an antialised circle */
		ofs= har->seed;
		
		for(a= har->ringc; a>0; a--, ofs+=2) {
			
			rc= hashvectf + (ofs % 768);
			
			fac= fabs( rc[1]*(har->rad*fabs(rc[0]) - radist) );
			
			if(fac< 1.0) {
				ringf+= (1.0-fac);
			}
		}
	}

	if(har->type & HA_VECT) {
		dist= fabs( har->cos*(yn) - har->sin*(xn) )/har->rad;
		if(dist>1.0) dist= 1.0;
		if(har->tex) {
			zn= har->sin*xn - har->cos*yn;
			yn= har->cos*xn + har->sin*yn;
			xn= zn;
		}
	}
	else dist= dist/har->radsq;

	if(har->type & HA_FLARECIRC) {
		
		dist= 0.5+fabs(dist-0.5);
		
	}

	if(har->hard>=30) {
		dist= sqrt(dist);
		if(har->hard>=40) {
			dist= sin(dist*M_PI_2);
			if(har->hard>=50) {
				dist= sqrt(dist);
			}
		}
	}
	else if(har->hard<20) dist*=dist;

	dist=(1.0-dist);
	
	if(har->linec) {
		float *rc, fac;
		int ofs;
		
		/* per starpoint an antialiased line */
		ofs= har->seed;
		
		for(a= har->linec; a>0; a--, ofs+=3) {
			
			rc= hashvectf + (ofs % 768);
			
			fac= fabs( (xn)*rc[0]+(yn)*rc[1]);
			
			if(fac< 1.0 ) {
				linef+= (1.0-fac);
			}
		}
		
		linef*= dist;
		
	}

	if(har->starpoints) {
		float ster, hoek;
		/* rotation */
		hoek= atan2(yn, xn);
		hoek*= (1.0+0.25*har->starpoints);
		
		co= cos(hoek);
		si= sin(hoek);
		
		hoek= (co*xn+si*yn)*(co*yn-si*xn);
		
		ster= fabs(hoek);
		if(ster>1.0) {
			ster= (har->rad)/(ster);
			
			if(ster<1.0) dist*= sqrt(ster);
		}
	}
	
	/* halo being intersected? */
	if(har->zs> zz-har->zd) {
		t= ((float)(zz-har->zs))/(float)har->zd;
		alpha*= sqrt(sqrt(t));
	}

	dist*= alpha;
	ringf*= dist;
	linef*= alpha;
	
	if(dist<0.003) {
		col[0] = 0.0;
		col[1] = 0.0;
		col[2] = 0.0;
		col[3] = 0.0;
		return;
	}

	/* The colour is either the rgb spec-ed by the user, or extracted from   */
	/* the texture                                                           */
	if(har->tex) {
		colf[3]= dist;
		do_halo_tex(har, xn, yn, colf);
		colf[0]*= colf[3];
		colf[1]*= colf[3];
		colf[2]*= colf[3];
		
	}
	else {
		colf[0]= dist*har->r;
		colf[1]= dist*har->g;
		colf[2]= dist*har->b;
		if(har->type & HA_XALPHA) colf[3]= dist*dist;
		else colf[3]= dist;
	}

	if(har->mat && har->mat->mode & MA_HALO_SHADE) {
		/* we test for lights because of preview... */
		if(R.totlamp) render_lighting_halo(har, colf);
	}

	/* Next, we do the line and ring factor modifications. It seems we do    */
	/* uchar calculations, but it's basically doing float arith with a 255   */
	/* scale factor.                                                         */
	if(linef!=0.0) {
		Material *ma= har->mat;
		
		colf[0]+= 255.0*linef * ma->specr;
		colf[1]+= 255.0*linef * ma->specg;
		colf[2]+= 255.0*linef * ma->specb;
		
		if(har->type & HA_XALPHA) colf[3]+= linef*linef;
		else colf[3]+= linef;
	}
	if(ringf!=0.0) {
		Material *ma= har->mat;

		colf[0]+= 255.0*ringf * ma->mirr;
		colf[1]+= 255.0*ringf * ma->mirg;
		colf[2]+= 255.0*ringf * ma->mirb;
		
		if(har->type & HA_XALPHA) colf[3]+= ringf*ringf;
		else colf[3]+= ringf;
	}

	/* convert to [0.0; 1.0] range */
	col[0] = colf[0] / 255.0;
	col[1] = colf[1] / 255.0;
	col[2] = colf[2] / 255.0;
	col[3] = colf[3];

} /* end of shadeHaloFloat() */


/* ------------------------------------------------------------------------- */
/*
  
  There are three different modes for blending sky behind a picture:       
  1. sky    = blend in sky directly                                        
  2. premul = don't do sky, but apply alpha (so pretend the picture ends   
     exactly at it's boundaries)                                  
  3. key    = don't do anything                                            
  Now the stupid thing is that premul means do nothing for us, and key     
  we have to adjust a bit...

*/

/* Sky vars. */
enum RE_SkyAlphaBlendingType keyingType = RE_ALPHA_SKY; /* The blending type    */

void setSkyBlendingMode(enum RE_SkyAlphaBlendingType mode) {
	if ((RE_ALPHA_NODEF < mode) && (mode < RE_ALPHA_MAX) ) {
		keyingType = mode;
	} else {
		/* error: false mode received */
		keyingType = RE_ALPHA_SKY;
	}
}

enum RE_SkyAlphaBlendingType getSkyBlendingMode() {
	return keyingType;
}

/* This one renders into collector, as always.                               */
void renderSkyPixelFloat(float x, float y)
{

	switch (keyingType) {
	case RE_ALPHA_PREMUL:
		/* Premul: don't fill, and don't change the values! */
	case RE_ALPHA_KEY:
		/*
		  Key: Leave pixels fully coloured, but retain alpha data, so you   
		  can composit the picture later on.                                
		  - Should operate on the stack outcome!
		*/		
/*  		collector[0] = 0.0; */
/*  		collector[1] = 0.0; */
/*  		collector[2] = 0.0; */
/*  		collector[3] = 0.0; */
		collector[3]= 0.0;
		collector[0]= R.wrld.horr;
		collector[1]= R.wrld.horg;
		collector[2]= R.wrld.horb;
		break;
	case RE_ALPHA_SKY:
		/* Fill in the sky as if it were a normal face. */
		shadeSkyPixel(x, y);
		break;
	default:
		; /* Error: illegal alpha blending state */
	}
}



/*
  Stuff the sky colour into the collector.
 */
void shadeSkyPixel(float fx, float fy) 
{
	float view[3];
	
	/*
	  The rules for sky:
	  1. Draw an image, if a background image was provided. Stop
	  2. get texture and colour blend, and combine these.
	*/

	float fac;

	/* 1. Do a backbuffer image: */ 
	if(R.r.bufflag & 1) {
		fillBackgroundImage(fx, fy);
		return;
	} else if((R.wrld.skytype & (WO_SKYBLEND+WO_SKYTEX))==0) {
		/*
		  2. Test for these types of sky. The old renderer always had to check for
		  coverage, but we don't need that anymore                                 
		  - SKYBLEND or SKYTEX disabled: fill in a flat colour                     
		  - otherwise, do the appropriate mapping (tex or colour blend)            
		  There used to be cached chars here, but they are not useful anymore
		*/
		collector[0] = R.wrld.horr;
		collector[1] = R.wrld.horg;
		collector[2] = R.wrld.horb;
		collector[3] = RE_UNITY_COLOUR_FLOAT;
	} else {
		/*
		  3. Which type(s) is(are) this (these)? This has to be done when no simple
		  way of determining the colour exists.
		*/

		/* This one true because of the context of this routine  */
/*  		if(rect[3] < 254) {  */
		if(R.wrld.skytype & WO_SKYPAPER) {
			view[0]= (fx+(R.xstart))/(float)R.afmx;
			view[1]= (fy+(R.ystart))/(float)R.afmy;
			view[2]= 0.0;
		}
		else {
			/* Wasn't this some pano stuff? */
			view[0]= (fx+(R.xstart)+1.0);
			
			if(R.flag & R_SEC_FIELD) {
				if(R.r.mode & R_ODDFIELD) view[1]= (fy+R.ystart+0.5)*R.ycor;
				else view[1]= (fy+R.ystart+1.5)*R.ycor;
			}
			else view[1]= (fy+R.ystart+1.0)*R.ycor;
			
			view[2]= -R.viewfac;
			
			fac= Normalise(view);
			if(R.wrld.skytype & WO_SKYTEX) {
				O.dxview= 1.0/fac;
				O.dyview= R.ycor/fac;
			}
		}
		
		if(R.r.mode & R_PANORAMA) {
			float panoco, panosi;
			float u, v;
			
			panoco = getPanovCo();
			panosi = getPanovSi();
			u= view[0]; v= view[2];
			
			view[0]= panoco*u + panosi*v;
			view[2]= -panosi*u + panoco*v;
		}
	
		/* get sky colour in the collector */
		shadeSkyPixelFloat(fy, view);
	}

	
}

/* Only line number is important here. Result goes to collector[4] */
void shadeSkyPixelFloat(float y, float *view)
{
	float lo[3];
	
	/* Why is this setting forced? Seems silly to me. It is tested in the texture unit. */
	R.wrld.skytype |= WO_ZENUP;
	
	/* Some view vector stuff. */
	if(R.wrld.skytype & WO_SKYREAL) {
	
		R.inprz= view[0]*R.grvec[0]+ view[1]*R.grvec[1]+ view[2]*R.grvec[2];

		if(R.inprz<0.0) R.wrld.skytype-= WO_ZENUP;
		R.inprz= fabs(R.inprz);
	}
	else if(R.wrld.skytype & WO_SKYPAPER) {
		R.inprz= 0.5+ 0.5*view[1];
	}
	else {
		/* the fraction of how far we are above the bottom of the screen */
		R.inprz= fabs(0.5+ view[1]);
	}

	/* Careful: SKYTEX and SKYBLEND are NOT mutually exclusive! If           */
	/* SKYBLEND is active, the texture and colour blend are added.           */
	if(R.wrld.skytype & WO_SKYTEX) {
		VECCOPY(lo, view);
		if(R.wrld.skytype & WO_SKYREAL) {
			
			MTC_Mat3MulVecfl(R.imat, lo);

			SWAP(float, lo[1],  lo[2]);
			
		}

		/* sky texture? I wonder how this manages to work... */
		/* Does this communicate with R.wrld.hor{rgb}? Yes.  */
		do_sky_tex(lo);
		/* internally, T{rgb} are used for communicating colours in the      */
		/* texture pipe, externally, this particular routine uses the        */
		/* R.wrld.hor{rgb} thingies.                                         */
		
	}

	/* Why are this R. members? because textures need it (ton) */
	if(R.inprz>1.0) R.inprz= 1.0;
	R.inprh= 1.0-R.inprz;

	/* No clipping, no conversion! */
	if(R.wrld.skytype & WO_SKYBLEND) {
		collector[0] = (R.inprh*R.wrld.horr + R.inprz*R.wrld.zenr);
		collector[1] = (R.inprh*R.wrld.horg + R.inprz*R.wrld.zeng);
		collector[2] = (R.inprh*R.wrld.horb + R.inprz*R.wrld.zenb);
	} else {
		/* Done when a texture was grabbed. */
		collector[0]= R.wrld.horr;
		collector[1]= R.wrld.horg;
		collector[2]= R.wrld.horb;
	}

	collector[3]= RE_UNITY_COLOUR_FLOAT;
}


/*
  Render pixel (x,y) from the backbuffer into the collector
	  
  backbuf is type Image, backbuf->ibuf is an ImBuf.  ibuf->rect is the
  rgba data (32 bit total), in ibuf->x by ibuf->y pixels. Copying
  should be really easy. I hope I understand the way ImBuf works
  correctly. (nzc)
*/
void fillBackgroundImage(float x, float y)
{

	int iy, ix;
	unsigned int* imBufPtr;
	char *colSource;
	
	/* This double check is bad... */
	if (!(R.backbuf->ok)) {
		/* Something went sour here... bail... */
		collector[0] = 0.0;
		collector[1] = 0.0;
		collector[2] = 0.0;
		collector[3] = 1.0;
		return;
	}
	/* load image if not already done?*/
	if(R.backbuf->ibuf==0) {
		R.backbuf->ibuf= IMB_loadiffname(R.backbuf->name, IB_rect);
		if(R.backbuf->ibuf==0) {
			/* load failed .... keep skipping */
			R.backbuf->ok= 0;
			return;
		}
	}

	/* Now for the real extraction: */
	/* Get the y-coordinate of the scanline? */
	iy= (int) ((y+R.afmy+R.ystart)*R.backbuf->ibuf->y)/(2*R.afmy);
	ix= (int) ((x+R.afmx+R.xstart)*R.backbuf->ibuf->x)/(2*R.afmx);
	
	/* correct in case of fields rendering: */
	if(R.flag & R_SEC_FIELD) {
		if((R.r.mode & R_ODDFIELD)==0) {
			if( iy<R.backbuf->ibuf->y) iy++;
		}
		else {
			if( iy>0) iy--;
		}
	}

	/* Offset into the buffer: start of scanline y: */
  	imBufPtr = R.backbuf->ibuf->rect
		+ (iy * R.backbuf->ibuf->x)
		+ ix;

	colSource = (char*) imBufPtr;

	cpCharColV2FloatColV(colSource, collector);

}

/* eof */
