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
#include "DNA_material_types.h"
#include "DNA_object_types.h"
#include "DNA_image_types.h"
#include "DNA_texture_types.h"
#include "DNA_lamp_types.h"

#include "BKE_image.h"
#include "BKE_global.h"
#include "BKE_texture.h"
#include "BKE_utildefines.h"

#include "render.h"
#include "texture.h"

#include "vanillaRenderPipe_types.h"
#include "pixelblending.h"
#include "rendercore.h" /* for some shading functions... */
#include "shadbuf.h"
#include "zbufferdatastruct.h"

#include "renderHelp.h"

#include "gammaCorrectionTables.h"
#include "errorHandler.h"
#include "pixelshading.h"


/* ton: 
  - unified render now uses face render routines from rendercore.c
  - todo still: shalo render and sky routines */


/* ------------------------------------------------------------------------- */
static int calcHaloZ(HaloRen *har, int zz)
{
	
	if(har->type & HA_ONLYSKY) {
		if(zz!=0x7FFFFFFF) zz= - 0x7FFFFF;
	}
	else {
		zz= (zz>>8);
	}
	return zz;
}

static void *renderHaloPixel(RE_COLBUFTYPE *collector, float x, float y, int haloNr) 
{
    HaloRen *har = NULL;
    float dist = 0.0;
    int zz = 0;
	
    /* Find har to go with haloNr */
    har = RE_findOrAddHalo(haloNr);
	
    /* zz is a strange number... This call should effect that halo's are  */
    /* never cut? Seems a bit strange to me now...   (nzc)                */
	/* it checks for sky... which is info not available in unified (ton) */
    zz = calcHaloZ(har, 0x7FFFFFFF);
	if(zz> har->zs) {
	
		/* distance of this point wrt. the halo center. Maybe xcor is also needed? */
		dist = ((x - har->xs) * (x - har->xs)) 
			+  ((y - har->ys) * (y - har->ys) * R.ycor * R.ycor) ;
		
		collector[0] = 0.0f; collector[1] = 0.0f; 
		collector[2] = 0.0f; collector[3] = 0.0f;
		
		if (dist < har->radsq) {
			shadeHaloFloat(har, collector, zz, dist, 
						   (x - har->xs), (y - har->ys) * R.ycor, har->flarec);
		}; /* else: this pixel is not rendered for this halo: no colour */
	}
    return (void*) har;

} /* end of void* renderHaloPixel(float x, float y, int haloNr) */



/* ------------------------------------------------------------------------- */

void *renderPixel(RE_COLBUFTYPE *collector, float x, float y, int *obdata, int mask)
{
    void* data = NULL;
    
    if (obdata[3] & RE_POLY) {
		data = shadepixel(x, y, obdata[0], obdata[1], mask, collector);
    }
    else if (obdata[3] & RE_HALO) {
        data = renderHaloPixel(collector, x, y, obdata[1]);
    }
	else if( obdata[1] == 0 ) {	
		/* for lamphalo, but doesn't seem to be called? Actually it is, and  */
		/* it returns NULL pointers. */
        data = shadepixel(x, y, obdata[0], obdata[1], mask, collector);
 	}
    return data;
   
} /* end of void renderPixel(float x, float y, int *obdata) */

/* ------------------------------------------------------------------------- */

void renderSpotHaloPixel(float x, float y, float* fcol)
{
	shadepixel(x, y, 0, 0, 0, fcol);	
}


/* ------------------------------------------------------------------------- */

extern float hashvectf[];

static void render_lighting_halo(HaloRen *har, float *colf)
{
	LampRen *lar;
	float i, inp, inpr, rco[3], dco[3], lv[3], lampdist, ld, t, *vn;
	float ir, ig, ib, shadfac, soft, lacol[3];
	int a;
	
	ir= ig= ib= 0.0;
	
	VECCOPY(rco, har->co);	
	dco[0]=dco[1]=dco[2]= 1.0/har->rad;
	
	vn= har->no;
	
	for(a=0; a<R.totlamp; a++) {
		lar= R.la[a];
		
		/* test for lamplayer */
		if(lar->mode & LA_LAYER) if((lar->lay & har->lay)==0) continue;
		
		/* lampdist cacluation */
		if(lar->type==LA_SUN || lar->type==LA_HEMI) {
			VECCOPY(lv, lar->vec);
			lampdist= 1.0;
		}
		else {
			lv[0]= rco[0]-lar->co[0];
			lv[1]= rco[1]-lar->co[1];
			lv[2]= rco[2]-lar->co[2];
			ld= sqrt(lv[0]*lv[0]+lv[1]*lv[1]+lv[2]*lv[2]);
			lv[0]/= ld;
			lv[1]/= ld;
			lv[2]/= ld;
			
			/* ld is re-used further on (texco's) */
			
			if(lar->mode & LA_QUAD) {
				t= 1.0;
				if(lar->ld1>0.0)
					t= lar->dist/(lar->dist+lar->ld1*ld);
				if(lar->ld2>0.0)
					t*= lar->distkw/(lar->distkw+lar->ld2*ld*ld);
				
				lampdist= t;
			}
			else {
				lampdist= (lar->dist/(lar->dist+ld));
			}
			
			if(lar->mode & LA_SPHERE) {
				t= lar->dist - ld;
				if(t<0.0) continue;
				
				t/= lar->dist;
				lampdist*= (t);
			}
			
		}
		
		lacol[0]= lar->r;
		lacol[1]= lar->g;
		lacol[2]= lar->b;
		
		if(lar->mode & LA_TEXTURE) {
			ShadeInput shi;
			VECCOPY(shi.co, rco);
			shi.osatex= 0;
			do_lamp_tex(lar, lv, &shi, lacol);
		}
		
		if(lar->type==LA_SPOT) {
			
			if(lar->mode & LA_SQUARE) {
				if(lv[0]*lar->vec[0]+lv[1]*lar->vec[1]+lv[2]*lar->vec[2]>0.0) {
					float x, lvrot[3];
					
					/* rotate view to lampspace */
					VECCOPY(lvrot, lv);
					MTC_Mat3MulVecfl(lar->imat, lvrot);
					
					x= MAX2(fabs(lvrot[0]/lvrot[2]) , fabs(lvrot[1]/lvrot[2]));
					/* 1.0/(sqrt(1+x*x)) is equivalent to cos(atan(x)) */
					
					inpr= 1.0/(sqrt(1.0+x*x));
				}
				else inpr= 0.0;
			}
			else {
				inpr= lv[0]*lar->vec[0]+lv[1]*lar->vec[1]+lv[2]*lar->vec[2];
			}
			
			t= lar->spotsi;
			if(inpr<t) continue;
			else {
				t= inpr-t;
				i= 1.0;
				soft= 1.0;
				if(t<lar->spotbl && lar->spotbl!=0.0) {
					/* soft area */
					i= t/lar->spotbl;
					t= i*i;
					soft= (3.0*t-2.0*t*i);
					inpr*= soft;
				}
				if(lar->mode & LA_ONLYSHADOW) {
					/* if(ma->mode & MA_SHADOW) { */
					/* dot product positive: front side face! */
					inp= vn[0]*lv[0] + vn[1]*lv[1] + vn[2]*lv[2];
					if(inp>0.0) {
						/* testshadowbuf==0.0 : 100% shadow */
						shadfac = testshadowbuf(lar->shb, rco, dco, dco, inp);
						if( shadfac>0.0 ) {
							shadfac*= inp*soft*lar->energy;
							ir -= shadfac;
							ig -= shadfac;
							ib -= shadfac;
							
							continue;
						}
					}
					/* } */
				}
				lampdist*=inpr;
			}
			if(lar->mode & LA_ONLYSHADOW) continue;
			
		}
		
		/* dot product and  reflectivity*/
		
		inp= 1.0-fabs(vn[0]*lv[0] + vn[1]*lv[1] + vn[2]*lv[2]);
		
		/* inp= cos(0.5*M_PI-acos(inp)); */
		
		i= inp;
		
		if(lar->type==LA_HEMI) {
			i= 0.5*i+0.5;
		}
		if(i>0.0) {
			i*= lampdist;
		}
		
		/* shadow  */
		if(i> -0.41) {			/* heuristic valua! */
			shadfac= 1.0;
			if(lar->shb) {
				shadfac = testshadowbuf(lar->shb, rco, dco, dco, inp);
				if(shadfac==0.0) continue;
				i*= shadfac;
			}
		}
		
		if(i>0.0) {
			ir+= i*lacol[0];
			ig+= i*lacol[1];
			ib+= i*lacol[2];
		}
	}
	
	if(ir<0.0) ir= 0.0;
	if(ig<0.0) ig= 0.0;
	if(ib<0.0) ib= 0.0;

	colf[0]*= ir;
	colf[1]*= ig;
	colf[2]*= ib;
	
}



void shadeHaloFloat(HaloRen *har,  float *col, int zz, 
					float dist, float xn,  float yn, short flarec)
{
	/* fill in col */
	float t, zn, radist, ringf=0.0, linef=0.0, alpha, si, co;
	int a;
   
	if(R.wrld.mode & WO_MIST) {
       if(har->type & HA_ONLYSKY) {
           /* stars but no mist */
           alpha= har->alfa;
       }
       else {
           /* a bit patchy... */
           alpha= mistfactor(-har->co[2], har->co)*har->alfa;
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
		col[0]= har->r; 
		col[1]= har->g; 
		col[2]= har->b;
		col[3]= dist;
		
		do_halo_tex(har, xn, yn, col);
		
		col[0]*= col[3];
		col[1]*= col[3];
		col[2]*= col[3];
		
	}
	else {
		col[0]= dist*har->r;
		col[1]= dist*har->g;
		col[2]= dist*har->b;
		if(har->type & HA_XALPHA) col[3]= dist*dist;
		else col[3]= dist;
	}

	if(har->mat) {
		if(har->mat->mode & MA_HALO_SHADE) {
			/* we test for lights because of preview... */
			if(R.totlamp) render_lighting_halo(har, col);
		}

		/* Next, we do the line and ring factor modifications. */
		if(linef!=0.0) {
			Material *ma= har->mat;
			
			col[0]+= linef * ma->specr;
			col[1]+= linef * ma->specg;
			col[2]+= linef * ma->specb;
			
			if(har->type & HA_XALPHA) col[3]+= linef*linef;
			else col[3]+= linef;
		}
		if(ringf!=0.0) {
			Material *ma= har->mat;

			col[0]+= ringf * ma->mirr;
			col[1]+= ringf * ma->mirg;
			col[2]+= ringf * ma->mirb;
			
			if(har->type & HA_XALPHA) col[3]+= ringf*ringf;
			else col[3]+= ringf;
		}
	}
}

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
void renderSkyPixelFloat(RE_COLBUFTYPE *collector, float x, float y)
{

	switch (keyingType) {
	case RE_ALPHA_PREMUL:
	case RE_ALPHA_KEY:
		/* Premul or key: don't fill, and don't change the values! */
		/* key alpha used to fill in color in 'empty' pixels, doesn't work anymore this way */
		collector[0] = 0.0; 
		collector[1] = 0.0; 
		collector[2] = 0.0; 
		collector[3] = 0.0; 
		break;
	case RE_ALPHA_SKY:
		/* Fill in the sky as if it were a normal face. */
		shadeSkyPixel(collector, x, y);
		collector[3]= 0.0;
		break;
	default:
		; /* Error: illegal alpha blending state */
	}
}



/*
  Stuff the sky colour into the collector.
 */
void shadeSkyPixel(RE_COLBUFTYPE *collector, float fx, float fy) 
{
	float view[3], dxyview[2];
	
	/*
	  The rules for sky:
	  1. Draw an image, if a background image was provided. Stop
	  2. get texture and colour blend, and combine these.
	*/

	float fac;

	/* 1. Do a backbuffer image: */ 
	if(R.r.bufflag & 1) {
		fillBackgroundImage(collector, fx, fy);
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
		collector[3] = 1.0f;
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
			
			dxyview[0]= 1.0/(float)R.afmx;
			dxyview[1]= 1.0/(float)R.afmy;
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
				dxyview[0]= 1.0/fac;
				dxyview[1]= R.ycor/fac;
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
		shadeSkyPixelFloat(collector, view, dxyview);
		collector[3] = 1.0f;
	}
}

/* Only view vector is important here. Result goes to colf[3] */
void shadeSkyPixelFloat(float *colf, float *view, float *dxyview)
{
	float lo[3], zen[3], hor[3], blend, blendm;
	
	/* Why is this setting forced? Seems silly to me. It is tested in the texture unit. */
	R.wrld.skytype |= WO_ZENUP;
	
	/* Some view vector stuff. */
	if(R.wrld.skytype & WO_SKYREAL) {
	
		blend= view[0]*R.grvec[0]+ view[1]*R.grvec[1]+ view[2]*R.grvec[2];

		if(blend<0.0) R.wrld.skytype-= WO_ZENUP;
		blend= fabs(blend);
	}
	else if(R.wrld.skytype & WO_SKYPAPER) {
		blend= 0.5+ 0.5*view[1];
	}
	else {
		/* the fraction of how far we are above the bottom of the screen */
		blend= fabs(0.5+ view[1]);
	}

	hor[0]= R.wrld.horr; hor[1]= R.wrld.horg; hor[2]= R.wrld.horb;
	zen[0]= R.wrld.zenr; zen[1]= R.wrld.zeng; zen[2]= R.wrld.zenb;
	
	/* Careful: SKYTEX and SKYBLEND are NOT mutually exclusive! If           */
	/* SKYBLEND is active, the texture and colour blend are added.           */
	if(R.wrld.skytype & WO_SKYTEX) {
		VECCOPY(lo, view);
		if(R.wrld.skytype & WO_SKYREAL) {
			
			MTC_Mat3MulVecfl(R.imat, lo);

			SWAP(float, lo[1],  lo[2]);
			
		}
		do_sky_tex(lo, dxyview, hor, zen, &blend);
	}

	if(blend>1.0) blend= 1.0;
	blendm= 1.0-blend;

	/* No clipping, no conversion! */
	if(R.wrld.skytype & WO_SKYBLEND) {
		colf[0] = (blendm*hor[0] + blend*zen[0]);
		colf[1] = (blendm*hor[1] + blend*zen[1]);
		colf[2] = (blendm*hor[2] + blend*zen[2]);
	} else {
		/* Done when a texture was grabbed. */
		colf[0]= hor[0];
		colf[1]= hor[1];
		colf[2]= hor[2];
	}
}


/*
  Render pixel (x,y) from the backbuffer into the collector
	  
  backbuf is type Image, backbuf->ibuf is an ImBuf.  ibuf->rect is the
  rgba data (32 bit total), in ibuf->x by ibuf->y pixels. Copying
  should be really easy. I hope I understand the way ImBuf works
  correctly. (nzc)
*/
void fillBackgroundImageChar(char *col, float x, float y)
{

	int iy, ix;
	unsigned int* imBufPtr;
	
	/* check to be sure... */
	if (R.backbuf==NULL || R.backbuf->ok==0) {
		/* bail out */
		col[0] = 0;
		col[1] = 0;
		col[2] = 0;
		col[3] = 255;
		return;
	}
	/* load image if not already done?*/
	if(R.backbuf->ibuf==0) {
		R.backbuf->ok= 0;
		return;
	}

	tag_image_time(R.backbuf);

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

	*( (int *)col) = *imBufPtr;
	
}

void fillBackgroundImage(RE_COLBUFTYPE *collector, float x, float y)
{
	char col[4];
	
	fillBackgroundImageChar(col, x, y);
	cpCharColV2FloatColV(col, collector);
	
}

/* eof */
