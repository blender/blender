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
 * Shading hierarchy:
 *
 * (externally visible)
 *
 * renderPixel----------
 *                     | renderHaloPixel-- shadeHaloFloat
 *                     |                
 *                     | renderFacePixel-- shadeLampLusFloat
 *                                       | shadeSpotHaloPixelFloat-- spotHaloFloat
 *
 *
 * renderSpotHaloPixel--(should call shadeSpotHaloPixelFloat, but there's numerical)
 *                      ( issues there... need to iron that out still              )
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
#include "render_intern.h"

#include "vanillaRenderPipe_types.h"
#include "pixelblending.h"
#include "zbuf.h"
#include "rendercore.h" /* for some shading functions... */
#include "zbufferdatastruct.h"

#include "shadbuf.h"
#include "shadowBuffer.h"

#include "renderHelp.h"

#include "gammaCorrectionTables.h"
#include "errorHandler.h"
#include "pixelshading.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

/* ------------------------------------------------------------------------- */
/* maybe declare local functions here?                                       */
/* ------------------------------------------------------------------------- */

/* The collector is the communication channel with the render pipe.          */
extern RE_COLBUFTYPE  collector[4];  /* used throughout as pixel colour accu */
/* shortcol was the old collector */
extern float holoofs, fmask[256];
extern float Zmulx, Zmuly;   /* Some kind of scale?                          */

unsigned int calcHaloZ(HaloRen *har, unsigned int zz);

/* ------------------------------------------------------------------------- */
/* if defined: do full error tracing and reporting here                      */
/*  #define RE_PIXSHADE_FULL_SAFETY */
/* if defined: use fake (dummy) colours for filling pixels (all is purple)   */
/*  #define RE_FAKE_PIXELS  */
/* if defined: use fake (dummy) colours for filling faces (all blue)         */
/* #define RE_FAKE_FACE_PIXELS */
/* if defined: use fake (dummy) colours for filling halos (all red)          */
/*  #define RE_FAKE_HALO_PIXELS  */
/*  #define RE_FAKE_HALO_PIXELS_2 */
/* if defined: use fake (dummy) colours for filling spothalos (green)        */
/*  #define RE_FAKE_SPOTHALO_PIXELS */
/* if defined: use fake (dummy) colours for shading lighting                 */
/*  #define RE_FAKE_LAMP_SHADE */
/* if defined: fake colours for sky pixels                                   */
/*  #define RE_FAKE_SKY_PIXELS */

/* ------------------------------------------------------------------------- */

unsigned int calcHaloZ(HaloRen *har, unsigned int zz)
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

/* ------------------------------------------------------------------------- */

void *renderPixel(float x, float y, int *obdata)
{
    void* data = NULL;
#ifdef RE_PIXSHADE_FULL_SAFETY
    char fname[] = "renderPixel";
#endif
#ifdef RE_FAKE_PIXELS
    collector[0] = RE_UNITY_COLOUR_FLOAT;
    collector[1] = 0;
    collector[2] = RE_UNITY_COLOUR_FLOAT;
    collector[3] = RE_UNITY_COLOUR_FLOAT;
    return NULL;  
#endif
    
    if (obdata[3] & RE_POLY) {
        /* face pixels aren't rendered in floats yet, so we wrap it here */
		data = renderFacePixel(x, y, obdata[1]);
    }
    else if (obdata[3] & RE_HALO) {
        data = renderHaloPixel(x, y, obdata[1]);
    }
	else if( obdata[1] == 0 ) {	
		/* for lamphalo, but doesn't seem to be called? Actually it is, and  */
		/* it returns NULL pointers. */
        data = renderFacePixel(x, y, obdata[1]);
 	}		
#ifdef RE_PIXSHADE_FULL_SAFETY
    else RE_error(RE_BAD_FACE_TYPE, fname);
#endif /* RE_PIXSHADE_FULL_SAFETY */    
    return data;
   
} /* end of void renderPixel(float x, float y, int *obdata) */

/* ------------------------------------------------------------------------- */

void *renderFacePixel(float x, float y, int vlaknr) 
/* Result goes into <collector>                                              */
{
#ifdef RE_PIXSHADE_FULL_SAFETY
	char fname[]= "renderFacePixelFloat";
#endif
    static VlakRen *vlr; /* static, because we don't want to recalculate vlr */
                         /* when we already know it                          */
    static VertRen *v1, *v2, *v3;
    static float t00, t01, t10, t11, dvlak, n1[3], n2[3], n3[3];
    static float s00, s01, s10, s11;
    float *o1, *o2, *o3;
    float u, v, l, dl, hox, hoy, detsh, fac, deler, alpha;
    char *cp1, *cp2, *cp3;

/*  	RE_error(RE_TRACE_COUNTER, fname); */
	
#ifdef RE_FAKE_FACE_PIXELS
    collector[0] = 0;
    collector[1] = 0;
    collector[2] = RE_UNITY_COLOUR_FLOAT;
    collector[3] = RE_UNITY_COLOUR_FLOAT;
    /* try to keep the rest as clean as possible... */
    if( ((vlaknr & 0x7FFFFF) <= R.totvlak) 
        && ! ((R.vlaknr== -1)||(vlaknr<=0)) ) {
        /* a bit superfluous to do this always, but    */
        /* when this is switched on, it doesn't matter */
        vlr= RE_findOrAddVlak( (vlaknr-1) & 0x7FFFFF);
    } else vlr = NULL; 
    return vlr;
#endif
    
    if(R.vlaknr== -1) {	/* set by initrender */
        /* also set in the pixelrender loop */
        vlr= R.vlr= 0;
    }
    
    if(vlaknr<=0) {	/* sky */
        R.vlaknr= 0;
		collector[3] = 0.0;
    }
    else if( (vlaknr & 0x7FFFFF) <= R.totvlak) {

		/* What follows now is a large bunch of texture coordinate mappings. */
	    /* When this face is the same as the previous one, that means all    */
        /* the coordinate remapping does not need to be recomputed.          */
        if(vlaknr!=R.vlaknr) {
            
            vlr= RE_findOrAddVlak( (vlaknr-1) & 0x7FFFFF);
            
            R.mat= vlr->mat;
            R.matren= R.mat->ren;
            
            if(R.matren==0) {	/* purple color, for debug */
				collector[3] = RE_UNITY_COLOUR_FLOAT;
				collector[2] = 0.0;
				collector[1] = RE_UNITY_COLOUR_FLOAT;
				collector[0] = RE_UNITY_COLOUR_FLOAT;
                return NULL;
            }
            
            R.vlr= vlr;
            
            R.vno= vlr->n;
            R.osatex= (R.matren->texco & TEXCO_OSA);
            R.vlaknr= vlaknr;
            
            v1= vlr->v1;
            dvlak= MTC_dot3Float(v1->co, vlr->n);
            
            if( (vlr->flag & R_SMOOTH) || (R.matren->texco & NEED_UV)) {	/* uv needed */
                if(vlaknr & 0x800000) {
                    v2= vlr->v3;
                    v3= vlr->v4;
                }
                else {
                    v2= vlr->v2;
                    v3= vlr->v3;
                }
                
                if(vlr->snproj==0) {
                    t00= v3->co[0]-v1->co[0]; t01= v3->co[1]-v1->co[1];
                    t10= v3->co[0]-v2->co[0]; t11= v3->co[1]-v2->co[1];
                }
                else if(vlr->snproj==1) {
                    t00= v3->co[0]-v1->co[0]; t01= v3->co[2]-v1->co[2];
                    t10= v3->co[0]-v2->co[0]; t11= v3->co[2]-v2->co[2];
                }
                else {
                    t00= v3->co[1]-v1->co[1]; t01= v3->co[2]-v1->co[2];
                    t10= v3->co[1]-v2->co[1]; t11= v3->co[2]-v2->co[2];
                }
                
                detsh= t00*t11-t10*t01;
                t00/= detsh; t01/=detsh; 
                t10/=detsh; t11/=detsh;
                
                if(vlr->flag & R_SMOOTH) { /* set vertex normals ("punos") */
                    if(vlr->puno & ME_FLIPV1) MTC_cp3FloatInv(v1->n, n1);
                    else                      MTC_cp3Float(v1->n, n1);
					
					if(vlaknr & 0x800000) {
                        if(vlr->puno & ME_FLIPV3) MTC_cp3FloatInv(v2->n, n2);
                        else                      MTC_cp3Float(v2->n, n2);
                        
                        if(vlr->puno & ME_FLIPV4) MTC_cp3FloatInv(v3->n, n3);
                        else                      MTC_cp3Float(v3->n, n3);
                    }
                    else {
                        if(vlr->puno & ME_FLIPV2) MTC_cp3FloatInv(v2->n, n2);
                        else                      MTC_cp3Float(v2->n, n2);
                        
                        if(vlr->puno & ME_FLIPV3) MTC_cp3FloatInv(v3->n, n3);
                        else                      MTC_cp3Float(v3->n, n3);
                    }
                }
                if(R.matren->texco & TEXCO_STICKY) {
                    s00= v3->ho[0]/v3->ho[3] - v1->ho[0]/v1->ho[3];
                    s01= v3->ho[1]/v3->ho[3] - v1->ho[1]/v1->ho[3];
                    s10= v3->ho[0]/v3->ho[3] - v2->ho[0]/v2->ho[3];
                    s11= v3->ho[1]/v3->ho[3] - v2->ho[1]/v2->ho[3];
                    
                    detsh= s00*s11-s10*s01;
                    s00/= detsh; s01/=detsh; 
                    s10/=detsh; s11/=detsh;
                }
            }
        } /* end of if vlaknr*/
        
		/* This trafo might be migrated to a separate function. It is used   */
		/* quite often.                                                      */
        if( (G.special1 & G_HOLO)
			&& (((Camera *)G.scene->camera->data)->flag & CAM_HOLO2) ) {
            R.view[0]= (x+(R.xstart)+1.0+holoofs);
        }
        else {
            R.view[0]= (x+(R.xstart)+1.0);
        }
        
        if(R.flag & R_SEC_FIELD) {
            if(R.r.mode & R_ODDFIELD) R.view[1]= (y+R.ystart+0.5)*R.ycor;
            else R.view[1]= (y+R.ystart+1.5)*R.ycor;
        }
        else R.view[1]= (y+R.ystart+1.0)*R.ycor;
        
        R.view[2]= -R.viewfac;
        
        if(R.r.mode & R_PANORAMA) {
			float panoco, panosi;
			panoco = getPanovCo();
			panosi = getPanovSi();
			u= R.view[0]; v= R.view[2];
			
			R.view[0]= panoco*u + panosi*v;
			R.view[2]= -panosi*u + panoco*v;
        }
        
        deler= vlr->n[0]*R.view[0] + vlr->n[1]*R.view[1] + vlr->n[2]*R.view[2];
        if (deler!=0.0) fac= R.zcor= dvlak/deler;
        else fac= R.zcor= 0.0;
        
        R.co[0]= fac*R.view[0];
        R.co[1]= fac*R.view[1];
        R.co[2]= fac*R.view[2];
        
        if(R.osatex || (R.r.mode & R_SHADOW) ) {
            u= dvlak/(deler-vlr->n[0]);
            v= dvlak/(deler- R.ycor*vlr->n[1]);
            
            O.dxco[0]= R.co[0]- (R.view[0]-1.0)*u;
            O.dxco[1]= R.co[1]- (R.view[1])*u;
            O.dxco[2]= R.co[2]- (R.view[2])*u;
            
            O.dyco[0]= R.co[0]- (R.view[0])*v;
            O.dyco[1]= R.co[1]- (R.view[1]-1.0*R.ycor)*v;
            O.dyco[2]= R.co[2]- (R.view[2])*v;
            
        }
        
        fac= Normalise(R.view);
        R.zcor*= fac;	/* for mist */
        
        if(R.osatex) {
            if( (R.matren->texco & TEXCO_REFL) ) {
                O.dxview= 1.0/fac;
                O.dyview= R.ycor/fac;
            }
        }
        
        /* UV en TEX*/
        if( (vlr->flag & R_SMOOTH) || (R.matren->texco & NEED_UV)) {
            if(vlr->snproj==0) {
                u= (R.co[0]-v3->co[0])*t11-(R.co[1]-v3->co[1])*t10;
                v= (R.co[1]-v3->co[1])*t00-(R.co[0]-v3->co[0])*t01;
                if(R.osatex) {
                    O.dxuv[0]=  O.dxco[0]*t11- O.dxco[1]*t10;
                    O.dxuv[1]=  O.dxco[1]*t00- O.dxco[0]*t01;
                    O.dyuv[0]=  O.dyco[0]*t11- O.dyco[1]*t10;
                    O.dyuv[1]=  O.dyco[1]*t00- O.dyco[0]*t01;
                }
            }
            else if(vlr->snproj==1) {
                u= (R.co[0]-v3->co[0])*t11-(R.co[2]-v3->co[2])*t10;
                v= (R.co[2]-v3->co[2])*t00-(R.co[0]-v3->co[0])*t01;
                if(R.osatex) {
                    O.dxuv[0]=  O.dxco[0]*t11- O.dxco[2]*t10;
                    O.dxuv[1]=  O.dxco[2]*t00- O.dxco[0]*t01;
                    O.dyuv[0]=  O.dyco[0]*t11- O.dyco[2]*t10;
                    O.dyuv[1]=  O.dyco[2]*t00- O.dyco[0]*t01;
                }
            }
            else {
                u= (R.co[1]-v3->co[1])*t11-(R.co[2]-v3->co[2])*t10;
                v= (R.co[2]-v3->co[2])*t00-(R.co[1]-v3->co[1])*t01;
                if(R.osatex) {
                    O.dxuv[0]=  O.dxco[1]*t11- O.dxco[2]*t10;
                    O.dxuv[1]=  O.dxco[2]*t00- O.dxco[1]*t01;
                    O.dyuv[0]=  O.dyco[1]*t11- O.dyco[2]*t10;
                    O.dyuv[1]=  O.dyco[2]*t00- O.dyco[1]*t01;
                }
            }
            l= 1.0+u+v;
            
            if(vlr->flag & R_SMOOTH) {
                R.vn[0]= l*n3[0]-u*n1[0]-v*n2[0];
                R.vn[1]= l*n3[1]-u*n1[1]-v*n2[1];
                R.vn[2]= l*n3[2]-u*n1[2]-v*n2[2];
                
                Normalise(R.vn);
                if(R.osatex && (R.matren->texco & (TEXCO_NORM+TEXCO_REFL)) ) {
                    dl= O.dxuv[0]+O.dxuv[1];
                    O.dxno[0]= dl*n3[0]-O.dxuv[0]*n1[0]-O.dxuv[1]*n2[0];
                    O.dxno[1]= dl*n3[1]-O.dxuv[0]*n1[1]-O.dxuv[1]*n2[1];
                    O.dxno[2]= dl*n3[2]-O.dxuv[0]*n1[2]-O.dxuv[1]*n2[2];
                    dl= O.dyuv[0]+O.dyuv[1];
                    O.dyno[0]= dl*n3[0]-O.dyuv[0]*n1[0]-O.dyuv[1]*n2[0];
                    O.dyno[1]= dl*n3[1]-O.dyuv[0]*n1[1]-O.dyuv[1]*n2[1];
                    O.dyno[2]= dl*n3[2]-O.dyuv[0]*n1[2]-O.dyuv[1]*n2[2];
                    
                }
            }
            else {
                VECCOPY(R.vn, vlr->n);
            }
            
            if(R.matren->mode & MA_ZINV) {	/* z invert */
                /* R.vn[0]= -R.vn[0]; */
                /* R.vn[1]= -R.vn[1]; */
            }
            
            if(R.matren->texco & TEXCO_ORCO) {
                if(v2->orco) {
                    o1= v1->orco;
                    o2= v2->orco;
                    o3= v3->orco;
                    
                    R.lo[0]= l*o3[0]-u*o1[0]-v*o2[0];
                    R.lo[1]= l*o3[1]-u*o1[1]-v*o2[1];
                    R.lo[2]= l*o3[2]-u*o1[2]-v*o2[2];
                    
                    if(R.osatex) {
                        dl= O.dxuv[0]+O.dxuv[1];
                        O.dxlo[0]= dl*o3[0]-O.dxuv[0]*o1[0]-O.dxuv[1]*o2[0];
                        O.dxlo[1]= dl*o3[1]-O.dxuv[0]*o1[1]-O.dxuv[1]*o2[1];
                        O.dxlo[2]= dl*o3[2]-O.dxuv[0]*o1[2]-O.dxuv[1]*o2[2];
                        dl= O.dyuv[0]+O.dyuv[1];
                        O.dylo[0]= dl*o3[0]-O.dyuv[0]*o1[0]-O.dyuv[1]*o2[0];
                        O.dylo[1]= dl*o3[1]-O.dyuv[0]*o1[1]-O.dyuv[1]*o2[1];
                        O.dylo[2]= dl*o3[2]-O.dyuv[0]*o1[2]-O.dyuv[1]*o2[2];
                    }
                }
            }
            
            if(R.matren->texco & TEXCO_GLOB) {
                VECCOPY(R.gl, R.co);
                MTC_Mat4MulVecfl(R.viewinv, R.gl);
                if(R.osatex) {
                    VECCOPY(O.dxgl, O.dxco);
                    MTC_Mat3MulVecfl(R.imat, O.dxco);
                    VECCOPY(O.dygl, O.dyco);
                    MTC_Mat3MulVecfl(R.imat, O.dyco);
                }
            }
            if((R.matren->texco & TEXCO_UV) || (R.matren->mode & (MA_VERTEXCOL|MA_FACETEXTURE)))  {
                if(R.vlr->tface) {
                    float *uv1, *uv2, *uv3;
                    
                    uv1= R.vlr->tface->uv[0];
                    if( (vlaknr & 0x800000) || (R.vlr->flag & R_FACE_SPLIT) ) {
                        uv2= R.vlr->tface->uv[2];
                        uv3= R.vlr->tface->uv[3];
                    }
                    else {
                        uv2= R.vlr->tface->uv[1];
                        uv3= R.vlr->tface->uv[2];
                    }
                    
                    R.uv[0]= -1.0 + 2.0*(l*uv3[0]-u*uv1[0]-v*uv2[0]);
                    R.uv[1]= -1.0 + 2.0*(l*uv3[1]-u*uv1[1]-v*uv2[1]);
                    
                    if(R.osatex) {
                        float duv[2];
                        
                        dl= O.dxuv[0]+O.dxuv[1];
                        duv[0]= O.dxuv[0]; 
                        duv[1]= O.dxuv[1];
                        
                        O.dxuv[0]= 2.0*(dl*uv3[0]-duv[0]*uv1[0]-duv[1]*uv2[0]);
                        O.dxuv[1]= 2.0*(dl*uv3[1]-duv[0]*uv1[1]-duv[1]*uv2[1]);
                        
                        dl= O.dyuv[0]+O.dyuv[1];
                        duv[0]= O.dyuv[0]; 
                        duv[1]= O.dyuv[1];
                        
                        O.dyuv[0]= 2.0*(dl*uv3[0]-duv[0]*uv1[0]-duv[1]*uv2[0]);
                        O.dyuv[1]= 2.0*(dl*uv3[1]-duv[0]*uv1[1]-duv[1]*uv2[1]);
                    }
                }
                else {
                    R.uv[0]= 2.0*(u+.5);
                    R.uv[1]= 2.0*(v+.5);
                }
            }
            if(R.matren->texco & TEXCO_NORM) {
                R.orn[0]= R.vn[0];
                R.orn[1]= -R.vn[1];
                R.orn[2]= R.vn[2];
            }
            if(R.matren->mode & MA_VERTEXCOL) {

				/* some colour calculations here */
				cp1= (char *)vlr->vcol;
                if(cp1) {
                    if( (vlaknr & 0x800000) || (R.vlr->flag & R_FACE_SPLIT) ) {
                        cp2= (char *)(vlr->vcol+2);
                        cp3= (char *)(vlr->vcol+3);
                    }
                    else {
                        cp2= (char *)(vlr->vcol+1);
                        cp3= (char *)(vlr->vcol+2);
                    }
                    R.vcol[0]= (l*cp3[3]-u*cp1[3]-v*cp2[3])/255.0;
                    R.vcol[1]= (l*cp3[2]-u*cp1[2]-v*cp2[2])/255.0;
                    R.vcol[2]= (l*cp3[1]-u*cp1[1]-v*cp2[1])/255.0;
                    
                }
                else {
                    R.vcol[0]= 0.0;
                    R.vcol[1]= 0.0;
                    R.vcol[2]= 0.0;
                }
            }
            if(R.matren->mode & MA_FACETEXTURE) {
                if((R.matren->mode & MA_VERTEXCOL)==0) {
                    R.vcol[0]= 1.0;
                    R.vcol[1]= 1.0;
                    R.vcol[2]= 1.0;
                }
				/* shading here */
                if(vlr->tface) render_realtime_texture();
            }
            
            /* after this, the u en v AND O.dxuv and O.dyuv are incorrect */
            if(R.matren->texco & TEXCO_STICKY) {
                if(v2->sticky) {
                    
                    /* recalc u en v  */
                    hox= x/Zmulx -1.0;
                    hoy= y/Zmuly -1.0;
                    u= (hox - v3->ho[0]/v3->ho[3])*s11
						- (hoy - v3->ho[1]/v3->ho[3])*s10;
                    v= (hoy - v3->ho[1]/v3->ho[3])*s00
						- (hox - v3->ho[0]/v3->ho[3])*s01;
                    l= 1.0+u+v;
                    
                    o1= v1->sticky;
                    o2= v2->sticky;
                    o3= v3->sticky;
                    
                    R.sticky[0]= l*o3[0]-u*o1[0]-v*o2[0];
                    R.sticky[1]= l*o3[1]-u*o1[1]-v*o2[1];
                    
                    if(R.osatex) {
                        O.dxuv[0]=  s11/Zmulx;
                        O.dxuv[1]=  - s01/Zmulx;
                        O.dyuv[0]=  - s10/Zmuly;
                        O.dyuv[1]=  s00/Zmuly;
                        
                        dl= O.dxuv[0]+O.dxuv[1];
                        O.dxsticky[0]= dl*o3[0]-O.dxuv[0]*o1[0]-O.dxuv[1]*o2[0];
                        O.dxsticky[1]= dl*o3[1]-O.dxuv[0]*o1[1]-O.dxuv[1]*o2[1];
                        dl= O.dyuv[0]+O.dyuv[1];
                        O.dysticky[0]= dl*o3[0]-O.dyuv[0]*o1[0]-O.dyuv[1]*o2[0];
                        O.dysticky[1]= dl*o3[1]-O.dyuv[0]*o1[1]-O.dyuv[1]*o2[1];
                    }
                }
            }
        }
        else {
            VECCOPY(R.vn, vlr->n);
        }
        if(R.matren->texco & TEXCO_WINDOW) {
            R.winco[0]= (x+(R.xstart))/(float)R.afmx;
            R.winco[1]= (y+(R.ystart))/(float)R.afmy;
        }
        
		/* After all texture coordinates are set and converted and           */
		/* transformed, we need to put some colour on it:                    */
		shadeLampLusFloat(); 

        /* MIST */
        if( (R.wrld.mode & WO_MIST) && (R.matren->mode & MA_NOMIST)==0 ){
			/* alpha returned in float? */
            alpha= mistfactor(R.co);
        }
        else alpha= 1.0;
        
        /* RAYTRACE WAS HERE! */
        
        if(R.matren->alpha!=1.0 || alpha!=1.0) {
            fac= alpha*(R.matren->alpha);
            			
			collector[0] *= fac; /* This applies to transparent faces! Even  */
			collector[1] *= fac; /* though it may seem to be a premul op, it */
			collector[2] *= fac; /* isn't.                                   */
			collector[3] = fac;  /* doesn't need scaling? */
        }
        else {
			collector[3] = 1.0;
        }
    }
    else {
		collector[0] = 1.0;
		collector[1] = 1.0;
		collector[2] = 0.0;
		collector[3] = 1.0;
    }

	/* Spothalos: do this here for covered pixels. It seems messy to place   */
	/* it here, structure-wise, but it's more efficient. Also, not having it */
	/* here makes it difficult to do proper overlaying later on.             */
	/* It starts off with a coordinate transform again.                      */
	if(R.flag & R_LAMPHALO) {
		if(vlaknr<=0) {	/* calculate view vector and set R.co at far */

			/* this view vector stuff should get its own function */
			if( (G.special1 & G_HOLO) && 
				((Camera *)G.scene->camera->data)->flag & CAM_HOLO2) {
				R.view[0]= (x+(R.xstart)+1.0+holoofs);
			} else {
				R.view[0]= (x+(R.xstart)+1.0);
			}

			if(R.flag & R_SEC_FIELD) {
				if(R.r.mode & R_ODDFIELD) R.view[1]= (y+R.ystart+0.5)*R.ycor;
				else R.view[1]= (y+R.ystart+1.5)*R.ycor;
			} else {
				R.view[1]= (y+R.ystart+1.0)*R.ycor;
			}
			
			R.view[2]= -R.viewfac;
			
			if(R.r.mode & R_PANORAMA) {
				float panoco, panosi;
				panoco = getPanovCo();
				panosi = getPanovSi();
				u= R.view[0]; v= R.view[2];
				
				R.view[0]= panoco*u + panosi*v;
				R.view[2]= -panosi*u + panoco*v;
			}

			R.co[2]= 0.0;
			
		}

		shadeSpotHaloPixelFloat(collector);
/*  		renderspothaloFix(collector); */
				
	}
	
#ifdef RE_PIXSHADE_FULL_SAFETY
	if (!vlr) RE_error(RE_BAD_DATA_POINTER, fname);
#endif
	
	return vlr;
    
} /* end of void renderFacePixelFloat(float x, float y, int vlaknr) */

/* ------------------------------------------------------------------------- */
/*
  - uses R.view to determine which pixel, I think?                          
  - the spothalo is dumped quite unceremoniously on top of the col vector   
  This function is also (sort of) implemented in shadespothalofix, but without
  all the clipping stuff. Somehow, the clipping here is _quite_ critical.
 */
void shadeSpotHaloPixelFloat(float *col)
{
	LampRen *lar;
	float factor = 0.0;
	int a;
	float rescol[4];
	
	
	for(a=0; a<R.totlamp; a++) {
		lar= R.la[a];
		if(lar->type==LA_SPOT && (lar->mode & LA_HALO) && lar->haint>0) {
	
			if(lar->org) {
				lar->r= lar->org->r;
				lar->g= lar->org->g;
				lar->b= lar->org->b;
			}

			/* determines how much spothalo we see */
			spotHaloFloat(lar, R.view, &factor);
			
			if(factor>0.0) {

				/* Why is alpha clipped? */
				if(factor > RE_FULL_COLOUR_FLOAT) rescol[3]= 1.0;
				else rescol[3]= factor;

				/* nasty issue: gamma corrected rendering AND 'addalphaADD'  */
				/* do not work well togethe                                  */
				/* actually, we should invent a new 'add' gamma type... (ton) */
				/*
				  There is a strange thing here: the spothalo seems to be
				  calculated in the space you would get when you go from
				  value space through inverse gamma! So we gamma-transform
				  to value-space, then integrate, blend, and gamma correct
				  _again_. -nzc-
				*/
				
				rescol[0] = factor * lar->r; /* Lampren rgb's are floats     */
				rescol[1] = factor * lar->g;
				rescol[2] = factor * lar->b;
				
				/* ---->add values, disregard alpha                          */
				/* - check for dest. alpha = 0. If so , just copy            */
				/* this is a slightly different approach: I do the gamma     */
				/* correction BEFORE the addition. What does the other       */
				/* approach do?                                              */
				if (col[3]< RE_EMPTY_COLOUR_FLOAT) {
					col[0] = gammaCorrect(rescol[0]);
					col[1] = gammaCorrect(rescol[1]);
					col[2] = gammaCorrect(rescol[2]);
					col[3] = rescol[3];
				} else {
					col[0] += gammaCorrect(rescol[0]);
					col[1] += gammaCorrect(rescol[1]);
					col[2] += gammaCorrect(rescol[2]);
					col[3] += rescol[3];
				}

				/* this clipping may have to go? Actually, if it's */
				/* done sooner, it may be more efficient */
				if(col[0] > RE_FULL_COLOUR_FLOAT) col[0] = 1.0;
				if(col[1] > RE_FULL_COLOUR_FLOAT) col[1] = 1.0;
				if(col[2] > RE_FULL_COLOUR_FLOAT) col[2] = 1.0;
				if(col[3] > RE_FULL_COLOUR_FLOAT) col[3] = 1.0;
				if(col[0] < RE_EMPTY_COLOUR_FLOAT) col[0] = 0.0;
				if(col[1] < RE_EMPTY_COLOUR_FLOAT) col[1] = 0.0;
				if(col[2] < RE_EMPTY_COLOUR_FLOAT) col[2] = 0.0;
				if(col[3] < RE_EMPTY_COLOUR_FLOAT) col[3] = 0.0;
			}
		}
	}
	
	if(col[0] < RE_EMPTY_COLOUR_FLOAT) col[0] = 0.0;
	if(col[1] < RE_EMPTY_COLOUR_FLOAT) col[1] = 0.0;
	if(col[2] < RE_EMPTY_COLOUR_FLOAT) col[2] = 0.0;
	if(col[3] < RE_EMPTY_COLOUR_FLOAT) col[3] = 0.0;

}

/* ------------------------------------------------------------------------- */

void spotHaloFloat(struct LampRen *lar, float *view, float *intens)
{
	double a, b, c, disc, nray[3], npos[3];
	float t0, t1 = 0.0, t2= 0.0, t3, haint;
	float p1[3], p2[3], ladist, maxz = 0.0, maxy = 0.0;
	int snijp, doclip=1, use_yco=0;
	int ok1=0, ok2=0;
	
	*intens= 0.0;
	haint= lar->haint;
	
	VECCOPY(npos, lar->sh_invcampos);	/* calculated in initlamp */
	
	/* rotate view */
	VECCOPY(nray, view);
	MTC_Mat3MulVecd(lar->imat, nray);
	
	if(R.wrld.mode & WO_MIST) {
		/* a bit patchy... */
		R.zcor= -lar->co[2];
		haint *= mistfactor(lar->co);
		if(haint==0.0) {
			return;
		}
	}


	/* rotate maxz */
	if(R.co[2]==0) doclip= 0;	/* for when halo over a sky */
	else {
		p1[0]= R.co[0]-lar->co[0];
		p1[1]= R.co[1]-lar->co[1];
		p1[2]= R.co[2]-lar->co[2];
	
		maxz= lar->imat[0][2]*p1[0]+lar->imat[1][2]*p1[1]+lar->imat[2][2]*p1[2];
		maxz*= lar->sh_zfac;
		maxy= lar->imat[0][1]*p1[0]+lar->imat[1][1]*p1[1]+lar->imat[2][1]*p1[2];

		if( fabs(nray[2]) <0.000001 ) use_yco= 1;
	}
	
	/* scale z to make sure we've got a normalized volume */	
	nray[2]*= lar->sh_zfac;
	/* nray does not need normalize */
	
	ladist= lar->sh_zfac*lar->dist;
	
	/* solve */
	a = nray[0] * nray[0] + nray[1] * nray[1] - nray[2]*nray[2];
	b = nray[0] * npos[0] + nray[1] * npos[1] - nray[2]*npos[2];
	c = npos[0] * npos[0] + npos[1] * npos[1] - npos[2]*npos[2];

	snijp= 0;
	if (fabs(a) < 0.00000001) {
		/*
		 * Only one intersection point...
		 */
		return;
	}
	else {
		disc = b*b - a*c;
		
		if(disc==0.0) {
			t1=t2= (-b)/ a;
			snijp= 2;
		}
		else if (disc > 0.0) {
			disc = sqrt(disc);
			t1 = (-b + disc) / a;
			t2 = (-b - disc) / a;
			snijp= 2;
		}
	}
	if(snijp==2) {
		/* sort */
		if(t1>t2) {
			a= t1; t1= t2; t2= a;
		}

		/* the z of intersection points with 'diabolo' shape */
		p1[2]= npos[2] + t1*nray[2];
		p2[2]= npos[2] + t2*nray[2];

		/* evaluate both points */
		if(p1[2]<=0.0) ok1= 1;
		if(p2[2]<=0.0 && t1!=t2) ok2= 1;
		
		/* at least 1 point with negative z */
		if(ok1==0 && ok2==0) return;
		
		/* intersction point with -ladist, the bottom of the cone */
		if(use_yco==0) {
			t3= (-ladist-npos[2])/nray[2];
				
			/* does one intersection point has to be replaced? */
			if(ok1) {
				if(p1[2]<-ladist) t1= t3;
			}
			else {
				ok1= 1;
				t1= t3;
			}
			if(ok2) {
				if(p2[2]<-ladist) t2= t3;
			}
			else {
				ok2= 1;
				t2= t3;
			}
		}
		else if(ok1==0 || ok2==0) return;
		
		/* at least 1 visible intersection point */
		if(t1<0.0 && t2<0.0) return;
		
		if(t1<0.0) t1= 0.0;
		if(t2<0.0) t2= 0.0;
		
		if(t1==t2) return;
		
		/* to be sure, sort again */
		if(t1>t2) {
			a= t1; t1= t2; t2= a;
		}
		
		/* calculate t0: is de maximal visible z (for when halo was intersected with face */
		if(doclip) {
			if(use_yco==0) t0= (maxz-npos[2])/nray[2];
			else t0= (maxy-npos[1])/nray[1];

			if(t0<t1) return;
			if(t0<t2) t2= t0;
		}

		/* calculate points */
		p1[0]= npos[0] + t1*nray[0];
		p1[1]= npos[1] + t1*nray[1];
		p1[2]= npos[2] + t1*nray[2];
		p2[0]= npos[0] + t2*nray[0];
		p2[1]= npos[1] + t2*nray[1];
		p2[2]= npos[2] + t2*nray[2];
		
			
		/* now we've got two points, we make three lengths with that */
		
		a= sqrt(p1[0]*p1[0]+p1[1]*p1[1]+p1[2]*p1[2]);
		b= sqrt(p2[0]*p2[0]+p2[1]*p2[1]+p2[2]*p2[2]);
		c= VecLenf(p1, p2);
		
		a/= ladist;
		a= sqrt(a);
		b/= ladist; 
		b= sqrt(b);
		c/= ladist;
		
		*intens= c*( (1.0-a)+(1.0-b) );
		
		/* WATCH IT: do not clip a,b en c at 1.0, this gives nasty little overflows
		   at the edges (especially with narrow halos) */
		if(*intens<=0.0) return;
		
		/* soft area */
		/* not needed because t0 has been used for p1/p2 as well */
		/* if(doclip && t0<t2) { */
		/* 	*intens *= (t0-t1)/(t2-t1); */
		/* } */
		
		*intens *= haint;
		
		if(lar->shb && lar->shb->shadhalostep) {
			/* from shadbuf.c, returns float */
			*intens *= shadow_halo(lar, p1, p2);
		}
		/* this was a test, for textured halos! unfortunately i could not get transformations right... (ton) */
		/* if(lar->mode & LA_TEXTURE)  do_lamphalo_tex(lar, p1, p2, intens); */
		
	}
} /* end of void spotHaloFloat(struct LampRen *, float *view, float *intens) */

/* ------------------------------------------------------------------------- */

void shadeLampLusFloat()
{
	LampRen *lar;
	register Material *ma;
	float i, inp, inpr, t, lv[3], lampdist, ld = 0;
	float ir, ig, ib;
	float isr=0,isg=0,isb=0;
	float lvrot[3], *vn, *view, shadfac, soft;
	int a;
	float shadfacvec[3] = {1.0, 1.0, 1.0};

	vn= R.vn;
	view= R.view;
	ma= R.matren;
	
	/* separate loop */
	if(ma->mode & MA_ONLYSHADOW) {
		shadfac= ir= 0.0;
		for(a=0; a<R.totlamp; a++) {
			lar= R.la[a];
			
			if(lar->mode & LA_LAYER) if((lar->lay & R.vlr->lay)==0) continue;
			
			if(lar->shb) {
				/* only test within spotbundle */
				lv[0]= R.co[0]-lar->co[0];
				lv[1]= R.co[1]-lar->co[1];
				lv[2]= R.co[2]-lar->co[2];
				Normalise(lv);
				inpr= lv[0]*lar->vec[0]+lv[1]*lar->vec[1]+lv[2]*lar->vec[2];
				if(inpr>lar->spotsi) {
					
					inp= vn[0]*lv[0] + vn[1]*lv[1] + vn[2]*lv[2];

					RE_testshadowbuf(lar->shadowBufOb, lar->shb, inp, shadfacvec);
/*  					testshadowbuf(lar->shb, inp, shadfacvec); */
					
					i= shadfacvec[0];

					t= inpr - lar->spotsi;
					if(t<lar->spotbl && lar->spotbl!=0.0) {
						t/= lar->spotbl;
						t*= t;
						i= t*i+(1.0-t);
					}
					
					shadfac+= i;
					ir+= 1.0;
				}
				else {
					shadfac+= 1.0;
					ir+= 1.0;
				}
			}
		}
		if(ir>0.0) shadfac/= ir;
		ma->alpha= (R.mat->alpha)*(1.0-shadfac);

		collector[0] = 0.0;
		collector[1] = 0.0;
		collector[2] = 0.0;
		/* alpha is not set.... why?*/
		return;
	}
		
	if(ma->mode & (MA_VERTEXCOLP|MA_FACETEXTURE)) {
		ma->r= R.vcol[0];
		ma->g= R.vcol[1];
		ma->b= R.vcol[2];
	}

	/* mirror reflection colour */
	R.refcol[0]= R.refcol[1]= R.refcol[2]= R.refcol[3]= 0.0;

	if(ma->texco) {

		if(ma->texco & TEXCO_REFL) {
			RE_calc_R_ref();
		}
		
		if(ma->mode & (MA_VERTEXCOLP|MA_FACETEXTURE)) {
			R.mat->r= R.vcol[0];
			R.mat->g= R.vcol[1];
			R.mat->b= R.vcol[2];
		}

		do_material_tex();
	}
	
	if(ma->mode & MA_SHLESS) {
		if( (ma->mode & (MA_VERTEXCOL+MA_VERTEXCOLP+MA_FACETEXTURE) )) {
			ir= R.vcol[0]*ma->r;
			ig= R.vcol[1]*ma->g;
			ib= R.vcol[2]*ma->b;
		}
		else {
			ir= ma->r; /* apparently stored as [0,1]? */
			ig= ma->g;
			ib= ma->b;
		}

		collector[0] = ir; /* no clipping, no alpha */
		collector[1] = ig;
		collector[2] = ib;
		return;
	}

	if( (ma->mode & (MA_VERTEXCOL+MA_VERTEXCOLP))== MA_VERTEXCOL ) {
		ir= ma->emit+R.vcol[0];
		ig= ma->emit+R.vcol[1];
		ib= ma->emit+R.vcol[2];
	}
	else ir= ig= ib= ma->emit;

	for(a=0; a<R.totlamp; a++) {
		lar= R.la[a];

		/* test for lamplayer */
		if(lar->mode & LA_LAYER) if((lar->lay & R.vlr->lay)==0) continue;

		/* lampdist calculation */
		if(lar->type==LA_SUN || lar->type==LA_HEMI) {
			VECCOPY(lv, lar->vec);
			lampdist= 1.0;
		}
		else {
			lv[0]= R.co[0]-lar->co[0];
			lv[1]= R.co[1]-lar->co[1];
			lv[2]= R.co[2]-lar->co[2];
			ld= sqrt(lv[0]*lv[0]+lv[1]*lv[1]+lv[2]*lv[2]);
			lv[0]/= ld;
			lv[1]/= ld;
			lv[2]/= ld;
			
			/* ld is used further on too (texco's) */
			
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
		
		if(lar->mode & LA_TEXTURE)  do_lamp_tex(lar, lv);

		if(lar->type==LA_SPOT) {

			/* using here a function call Inp() slows down! */
			
			if(lar->mode & LA_SQUARE) {
				if(lv[0]*lar->vec[0]+lv[1]*lar->vec[1]+lv[2]*lar->vec[2]>0.0) {
					float x;
					
					/* rotate view to lampspace */
					VECCOPY(lvrot, lv);
					MTC_Mat3MulVecfl(lar->imat, lvrot);
					
					x= MAX2(fabs(lvrot[0]/lvrot[2]) , fabs(lvrot[1]/lvrot[2]));
					/* 1.0/(sqrt(1+x*x)) is equivalent to cos(atan(x)) */

					inpr= 1.0/(sqrt(1+x*x));
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
				if(lar->mode & LA_ONLYSHADOW && lar->shb) {
					if(ma->mode & MA_SHADOW) {
						/* dot product positive: front side face! */
						inp= vn[0]*lv[0] + vn[1]*lv[1] + vn[2]*lv[2];
						if(inp>0.0) {
					
							/* testshadowbuf==0.0 : 100% shadow */
							RE_testshadowbuf(lar->shadowBufOb, lar->shb, inp, shadfacvec);
							shadfac = 1.0 - shadfacvec[0];
							
							if(shadfac>0.0) {
								shadfac*= inp*soft*lar->energy;
								ir -= shadfac;
								ig -= shadfac;
								ib -= shadfac;
								
								continue;
							}
						}
					}
				}
				lampdist*=inpr;
			}
			if(lar->mode & LA_ONLYSHADOW) continue;

			if(lar->mode & LA_OSATEX) {
				R.osatex= 1;	/* signal for multitex() */
				
				O.dxlv[0]= lv[0] - (R.co[0]-lar->co[0]+O.dxco[0])/ld;
				O.dxlv[1]= lv[1] - (R.co[1]-lar->co[1]+O.dxco[1])/ld;
				O.dxlv[2]= lv[2] - (R.co[2]-lar->co[2]+O.dxco[2])/ld;

				O.dylv[0]= lv[0] - (R.co[0]-lar->co[0]+O.dyco[0])/ld;
				O.dylv[1]= lv[1] - (R.co[1]-lar->co[1]+O.dyco[1])/ld;
				O.dylv[2]= lv[2] - (R.co[2]-lar->co[2]+O.dyco[2])/ld;
			}
			
		}

		/* dot product and reflectivity*/
		inp=i= vn[0]*lv[0] + vn[1]*lv[1] + vn[2]*lv[2];
		
		if(lar->mode & LA_NO_DIFF) {
			i= 0.0;	// skip shaders
		}
		else if(lar->type==LA_HEMI) {
			i= 0.5*i+0.5;
		}
		else {
			/* diffuse shaders */
			if(ma->diff_shader==MA_DIFF_ORENNAYAR) i= OrenNayar_Diff(vn, lv, view, ma->roughness);
			else if(ma->diff_shader==MA_DIFF_TOON) i= Toon_Diff(vn, lv, view, ma->param[0], ma->param[1]);
			else i= inp;	// Lambert
		}
		if(i>0.0) {
			i*= lampdist*ma->ref;
		}

		/* shadow and spec */
		if(i> -0.41) {			/* heuristic value... :) */
			shadfac= 1.0;
			if(lar->shb) {
				if(ma->mode & MA_SHADOW) {
					float shadfacvec[3] = {1.0, 1.0, 1.0};
					RE_testshadowbuf(lar->shadowBufOb, lar->shb, inp, shadfacvec);
					shadfac = shadfacvec[0];

/*  					shadfac = 1.0; : no shadow */ 
					if(shadfac==0.0) continue;
					i*= shadfac;
				}
			}
			/* specularity */
			
			if(ma->spec!=0.0 && !(lar->mode & LA_NO_SPEC)) {
				
				if(lar->type==LA_HEMI) {
					/* hemi uses no spec shaders (yet) */
					
					lv[0]+= view[0];
					lv[1]+= view[1];
					lv[2]+= view[2];
					
					Normalise(lv);
					
					t= vn[0]*lv[0]+vn[1]*lv[1]+vn[2]*lv[2];
					
					if(lar->type==LA_HEMI) {
						t= 0.5*t+0.5;
					}
					
					t= ma->spec*spec(t, ma->har);
					isr+= t*(lar->r * ma->specr);
					isg+= t*(lar->g * ma->specg);
					isb+= t*(lar->b * ma->specb);
				}
				else {
					/* specular shaders */
					float specfac;

					/* we force a different lamp vector for sun light */
					if(lar->type==LA_SUN) lv[2]-= 1.0;
										
					if(ma->spec_shader==MA_SPEC_PHONG) 
						specfac= Phong_Spec(vn, lv, view, ma->har);
					else if(ma->spec_shader==MA_SPEC_COOKTORR) 
						specfac= CookTorr_Spec(vn, lv, view, ma->har);
					else if(ma->spec_shader==MA_SPEC_BLINN) 
						specfac= Blinn_Spec(vn, lv, view, ma->refrac, (float)ma->har);
					else 
						specfac= Toon_Spec(vn, lv, view, ma->param[2], ma->param[3]);
					
					t= shadfac*ma->spec*lampdist*specfac;
					
					isr+= t*(lar->r * ma->specr);
					isg+= t*(lar->g * ma->specg);
					isb+= t*(lar->b * ma->specb);
				}
			}
		}
		
		if(i>0.0 && !(lar->mode & LA_NO_DIFF)) {
			ir+= i*lar->r;
			ig+= i*lar->g;
			ib+= i*lar->b;
		}
	}

	/* clipping: maybe don't clip? (nzc) */
	/* yes, it shouldn't be done... unfortunately the current 
	 * gammaCorrect implementation doesn't handle negative values
	 * correctly ( (-1)^2 = 1!!)  (ton)
	 */
	/* Well, it does now. -(1^2) = -1 :) (nzc) */

	if(ma->mode & MA_ZTRA) {	/* ztra shade */
		if(ma->spectra!=0.0) {

			t = MAX3(isr, isb, isg);
			t *= ma->spectra;
			if(t>1.0) t= 1.0;
			if(ma->mapto & MAP_ALPHA) ma->alpha= (1.0-t)*ma->alpha+t;
			else ma->alpha= (1.0-t)*R.mat->alpha+t;
		}
	}
		
	if(R.refcol[0]==0.0) {
		collector[0] = (ma->r * ir) + ma->ambr + isr;
		collector[1] = (ma->g * ig) + ma->ambg + isg;
		collector[2] = (ma->b * ib) + ma->ambb + isb;
		/* clip for >0 ? */
	}
	else {
		collector[0] = (ma->mirr * R.refcol[1])
			+ ((1.0 - (ma->mirr * R.refcol[0])) * ((ma->r * ir) + ma->ambr))
			+ isr;
		collector[1] = (ma->mirg*R.refcol[2])
			+ ((1.0 - (ma->mirg * R.refcol[0])) * ((ma->g * ig) +ma->ambg))
			+isg;
		collector[2] = (ma->mirb*R.refcol[3])
			+ ((1.0 - (ma->mirb * R.refcol[0])) * ((ma->b * ib) +ma->ambb))
			+isb;
	}

#ifdef RE_FAKE_LAMP_SHADE
	collector[0] = 0.5;
	collector[1] = 0.5;
	collector[2] = 1.0;
#endif
	
}

/* ------------------------------------------------------------------------- */

void* renderHaloPixel(float x, float y, int haloNr) {
    HaloRen *har = NULL;
    float dist = 0.0;
    unsigned int zz = 0;
    
#ifdef RE_FAKE_HALO_PIXELS
    collector[0] = RE_UNITY_COLOUR_FLOAT;
    collector[1] = 0;
    collector[2] = 0;
    collector[3] = RE_UNITY_COLOUR_FLOAT;
    har = RE_findOrAddHalo(haloNr); /* crash prevention */
    return (void*) har;
#endif

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
		/* make a second fake pixel? */
#ifdef RE_FAKE_HALO_PIXELS_2
		collector[0] = RE_UNITY_COLOUR_FLOAT;
		collector[1] = 0;
		collector[2] = 0;
		collector[3] = RE_UNITY_COLOUR_FLOAT;
#endif
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
		linef *= 255.0;
		
		colf[0]+= linef * ma->specr;
		colf[1]+= linef * ma->specg;
		colf[2]+= linef * ma->specb;
		
		if(har->type & HA_XALPHA) colf[3]+= linef*linef;
		else colf[3]+= linef;
	}
	if(ringf!=0.0) {
		Material *ma= har->mat;
		ringf *= 255.0;

		colf[0]+= ringf * ma->mirr;
		colf[1]+= ringf * ma->mirg;
		colf[2]+= ringf * ma->mirb;
		
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

void renderSpotHaloPixel(float x, float y, float* target)
{
	float u = 0.0, v = 0.0;
	
#ifdef RE_FAKE_SPOTHALO_PIXELS
	target[0] = 0.0;
	target[1] = 1.0;
	target[2] = 0.0;
	target[3] = 1.0;
	return;
#endif
	
	/* Strange fix? otherwise done inside shadepixel. It's sort  */
	/* of like telling this is a 'sky' pixel.                    */
	R.vlaknr = 0;
	target[3] = 0.0;
	
	/*
	  Here's the viewvector setting again.
	*/
	if( (G.special1 & G_HOLO) && ((Camera *)G.scene->camera->data)->flag & CAM_HOLO2) {
		R.view[0]= (x+(R.xstart)+1.0+holoofs);
	}
	else {
		R.view[0]= (x+(R.xstart)+1.0);
	}
	
	if(R.flag & R_SEC_FIELD) {
		if(R.r.mode & R_ODDFIELD) R.view[1]= (y+R.ystart+0.5)*R.ycor;
		else R.view[1]= (y+R.ystart+1.5)*R.ycor;
	}
	else R.view[1]= (y+R.ystart+1.0)*R.ycor;
	
	R.view[2]= -R.viewfac;
	
	if(R.r.mode & R_PANORAMA) {
		float panoco, panosi;
		panoco = getPanovCo();
		panosi = getPanovSi();
		u= R.view[0]; v= R.view[2];
		
		R.view[0]= panoco*u + panosi*v;
		R.view[2]= -panosi*u + panoco*v;
	}
	
	R.co[2]= 0.0;

	/* This little function is a patch for spothalos on non-covered pixels. */
  	renderspothaloFix(target);
	
} /* end of void renderSpotHaloPixel(float x, float y, float colbuf[4]) */

/* ------------------------------------------------------------------------- */
/*
  This routine is only for sky-pixels. Therefore, no gamma needs to be done.
  One strange side-effect is when you have a negative halo lamp. This suddenly
  gives loads of colour. That particular case has been explicitly guarded: no
  halo for negative halo spots!

  This routine uses the viewvector in R... to determine what to shade. Just
  deposit the colour to be blended in col.

  I would like to add colours 'normally', so this routine would be the same
  for spothalo on covered pixels, but that doesn't work. Some strange clipping
  occurs...
 */
void renderspothaloFix(float *col)
{
	LampRen *lar;
	float i;
	int a;
		
	for(a=0; a<R.totlamp; a++) {
		lar= R.la[a];
		if((lar->type==LA_SPOT)
		   && (lar->mode & LA_HALO)
		   && !(lar->mode & LA_NEG)
		   && (lar->haint>0)) {
	
			if(lar->org) {
				lar->r= lar->org->r;
				lar->g= lar->org->g;
				lar->b= lar->org->b;
			}

			/* returns the intensity in i */
			spotHaloFloat(lar, R.view, &i);
			
			if(i>0.0) {
				/* Premul colours here! */
				col[0] = i * lar->r;
				col[1] = i * lar->g;
				col[2] = i * lar->b;
				col[3] = i;
			} 
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
void renderSkyPixelFloat(float x, float y)
{
#ifdef RE_FAKE_SKY_PIXELS
	collector[0] = 1.0;
	collector[1] = 1.0;
	collector[2] = 0.0;
	collector[3] = 1.0;
	return;
#endif

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
void shadeSkyPixel(float fx, float fy) {

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
			R.view[0]= (fx+(R.xstart))/(float)R.afmx;
			R.view[1]= (fy+(R.ystart))/(float)R.afmy;
			R.view[2]= 0.0;
		}
		else {
			/* Wasn't this some pano stuff? */
			R.view[0]= (fx+(R.xstart)+1.0);
			
			if(R.flag & R_SEC_FIELD) {
				if(R.r.mode & R_ODDFIELD) R.view[1]= (fy+R.ystart+0.5)*R.ycor;
				else R.view[1]= (fy+R.ystart+1.5)*R.ycor;
			}
			else R.view[1]= (fy+R.ystart+1.0)*R.ycor;
			
			R.view[2]= -R.viewfac;
			
			fac= Normalise(R.view);
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
			u= R.view[0]; v= R.view[2];
			
			R.view[0]= panoco*u + panosi*v;
			R.view[2]= -panosi*u + panoco*v;
		}
	
		/* get sky colour in the collector */
		shadeSkyPixelFloat(fy);
	}

	
}

/* Only line number is important here. Result goes to collector[4] */
void shadeSkyPixelFloat(float y)
{

	/* Why is this setting forced? Seems silly to me. It is tested in the texture unit. */
	R.wrld.skytype |= WO_ZENUP;
	
	/* Some view vector stuff. */
	if(R.wrld.skytype & WO_SKYREAL) {
	
		R.inprz= R.view[0]*R.grvec[0]+ R.view[1]*R.grvec[1]+ R.view[2]*R.grvec[2];

		if(R.inprz<0.0) R.wrld.skytype-= WO_ZENUP;
		R.inprz= fabs(R.inprz);
	}
	else if(R.wrld.skytype & WO_SKYPAPER) {
		R.inprz= 0.5+ 0.5*R.view[1];
	}
	else {
		/* the fraction of how far we are above the bottom of the screen */
		R.inprz= fabs(0.5+ R.view[1]);
	}

	/* Careful: SKYTEX and SKYBLEND are NOT mutually exclusive! If           */
	/* SKYBLEND is active, the texture and colour blend are added.           */
	if(R.wrld.skytype & WO_SKYTEX) {
		VECCOPY(R.lo, R.view);
		if(R.wrld.skytype & WO_SKYREAL) {
			
			MTC_Mat3MulVecfl(R.imat, R.lo);

			SWAP(float, R.lo[1],  R.lo[2]);
			
		}

		R.osatex= 0;

		/* sky texture? I wonder how this manages to work... */
		/* Does this communicate with R.wrld.hor{rgb}? Yes.  */
		do_sky_tex();
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
