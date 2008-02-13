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
 * Contributor(s): 2004-2006, Blender Foundation, full recode
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <math.h>
#include <string.h>
#include "BLI_arithb.h"

/* External modules: */
#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"
#include "MTC_matrixops.h"
#include "MTC_vectorops.h"

#include "DNA_camera_types.h"
#include "DNA_group_types.h"
#include "DNA_material_types.h"
#include "DNA_object_types.h"
#include "DNA_image_types.h"
#include "DNA_texture_types.h"
#include "DNA_lamp_types.h"

#include "BKE_image.h"
#include "BKE_global.h"
#include "BKE_texture.h"
#include "BKE_utildefines.h"

/* own module */
#include "render_types.h"
#include "renderpipeline.h"
#include "renderdatabase.h"
#include "texture.h"
#include "pixelblending.h"
#include "rendercore.h"
#include "shadbuf.h"
#include "pixelshading.h"

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
/* defined in pipeline.c, is hardcopy of active dynamic allocated Render */
/* only to be used here in this file, it's for speed */
extern struct Render R;
/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */


extern float hashvectf[];

static void render_lighting_halo(HaloRen *har, float *colf)
{
	GroupObject *go;
	LampRen *lar;
	float i, inp, inpr, rco[3], dco[3], lv[3], lampdist, ld, t, *vn;
	float ir, ig, ib, shadfac, soft, lacol[3];
	
	ir= ig= ib= 0.0;
	
	VECCOPY(rco, har->co);	
	dco[0]=dco[1]=dco[2]= 1.0/har->rad;
	
	vn= har->no;
	
	for(go=R.lights.first; go; go= go->next) {
		lar= go->lampren;
		
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
			
			/* Warning, This is not that nice, and possibly a bit slow,
			however some variables were not initialized properly in, unless using shade_input_initialize(...), we need to do a memset */
			memset(&shi, 0, sizeof(ShadeInput)); 
			/* end warning! - Campbell */
			
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
						shadfac = testshadowbuf(&R, lar->shb, rco, dco, dco, inp, 0.0f);
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
				shadfac = testshadowbuf(&R, lar->shb, rco, dco, dco, inp, 0.0f);
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
	float t, zn, radist, ringf=0.0f, linef=0.0f, alpha, si, co;
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

	if(dist < 1.0f)
		dist= (1.0f-dist);
	else
		dist= 0.0f;
	
	if(har->linec) {
		float *rc, fac;
		int ofs;
		
		/* per starpoint an antialiased line */
		ofs= har->seed;
		
		for(a= har->linec; a>0; a--, ofs+=3) {
			
			rc= hashvectf + (ofs % 768);
			
			fac= fabs( (xn)*rc[0]+(yn)*rc[1]);
			
			if(fac< 1.0f )
				linef+= (1.0f-fac);
		}
		
		linef*= dist;
	}

	if(har->starpoints) {
		float ster, angle;
		/* rotation */
		angle= atan2(yn, xn);
		angle*= (1.0+0.25*har->starpoints);
		
		co= cos(angle);
		si= sin(angle);
		
		angle= (co*xn+si*yn)*(co*yn-si*xn);
		
		ster= fabs(angle);
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

	/* disputable optimize... (ton) */
	if(dist<=0.00001) {
		col[0] = 0.0;
		col[1] = 0.0;
		col[2] = 0.0;
		col[3] = 0.0;
		return;
	}
	
	dist*= alpha;
	ringf*= dist;
	linef*= alpha;
	
	/* The color is either the rgb spec-ed by the user, or extracted from   */
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
			if(R.lights.first) render_lighting_halo(har, col);
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
	
	/* alpha requires clip, gives black dots */
	if(col[3] > 1.0f)
		col[3]= 1.0f;
}

/* ------------------------------------------------------------------------- */

static void fillBackgroundImage(float *collector, float fx, float fy)
{
	collector[0] = 0.0; 
	collector[1] = 0.0; 
	collector[2] = 0.0; 
	collector[3] = 0.0; 
	
	if(R.backbuf) {
		float dx= 1.0f/(float)R.winx;
		float dy= 1.0f/(float)R.winy;
		
		image_sample(R.backbuf, fx*dx, fy*dy, dx, dy, collector);
	}
}

/* Only view vector is important here. Result goes to colf[3] */
void shadeSkyView(float *colf, float *rco, float *view, float *dxyview)
{
	float lo[3], zen[3], hor[3], blend, blendm;
	int skyflag;
	
	/* flag indicating if we render the top hemisphere */
	skyflag = WO_ZENUP;
	
	/* Some view vector stuff. */
	if(R.wrld.skytype & WO_SKYREAL) {
		
		blend= view[0]*R.grvec[0]+ view[1]*R.grvec[1]+ view[2]*R.grvec[2];
		
		if(blend<0.0) skyflag= 0;
		
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
	/* SKYBLEND is active, the texture and color blend are added.           */
	if(R.wrld.skytype & WO_SKYTEX) {
		VECCOPY(lo, view);
		if(R.wrld.skytype & WO_SKYREAL) {
			
			MTC_Mat3MulVecfl(R.imat, lo);
			
			SWAP(float, lo[1],  lo[2]);
			
		}
		do_sky_tex(rco, lo, dxyview, hor, zen, &blend, skyflag);
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
  Stuff the sky color into the collector.
 */
void shadeSkyPixel(float *collector, float fx, float fy) 
{
	float view[3], dxyview[2];
	
	/*
	  The rules for sky:
	  1. Draw an image, if a background image was provided. Stop
	  2. get texture and color blend, and combine these.
	*/

	float fac;

	/* 1. Do a backbuffer image: */ 
	if(R.r.bufflag & 1) {
		fillBackgroundImage(collector, fx, fy);
		return;
	} 
	else if((R.wrld.skytype & (WO_SKYBLEND+WO_SKYTEX))==0) {
		/* 2. solid color */
		collector[0] = R.wrld.horr;
		collector[1] = R.wrld.horg;
		collector[2] = R.wrld.horb;
		collector[3] = 0.0f;
	} 
	else {
		/* 3. */

		/* This one true because of the context of this routine  */
		if(R.wrld.skytype & WO_SKYPAPER) {
			view[0]= -1.0f + 2.0f*(fx/(float)R.winx);
			view[1]= -1.0f + 2.0f*(fy/(float)R.winy);
			view[2]= 0.0;
			
			dxyview[0]= 1.0f/(float)R.winx;
			dxyview[1]= 1.0f/(float)R.winy;
		}
		else {
			calc_view_vector(view, fx, fy);
			fac= Normalize(view);
			
			if(R.wrld.skytype & WO_SKYTEX) {
				dxyview[0]= -R.viewdx/fac;
				dxyview[1]= -R.viewdy/fac;
			}
		}
		
		/* get sky color in the collector */
		shadeSkyView(collector, NULL, view, dxyview);
		collector[3] = 0.0f;
	}
}


/* eof */
