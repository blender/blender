/**
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
 * Contributors: 2004/2005/2006 Blender Foundation, full recode
 *
 * ***** END GPL LICENSE BLOCK *****
 */


/* Global includes */

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "MEM_guardedalloc.h"

#include "PIL_time.h"

#include "BLI_math.h"
#include "BLI_blenlib.h"
#include "BLI_jitter.h"



#include "DNA_camera_types.h"
#include "DNA_group_types.h"
#include "DNA_image_types.h"
#include "DNA_lamp_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BKE_utildefines.h"
#include "BKE_global.h"
#include "BKE_material.h"
#include "BKE_object.h"
#include "BKE_image.h"
#include "BKE_ipo.h"
#include "BKE_key.h"
#include "BKE_action.h"
#include "BKE_writeavi.h"
#include "BKE_scene.h"

#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"

#ifdef WITH_QUICKTIME
#include "quicktime_export.h"
#endif

/* this module */
#include "renderpipeline.h"
#include "render_types.h"

#include "rendercore.h"
#include "pixelshading.h"
#include "zbuf.h"

/* Own includes */
#include "initrender.h"


/* ********************** */

static void init_render_jit(Render *re)
{
	static float jit[32][2];	/* simple caching */
	static int lastjit= 0;
	
	if(lastjit!=re->r.osa) {
		memset(jit, 0, sizeof(jit));
		BLI_initjit(jit[0], re->r.osa);
	}
	
	lastjit= re->r.osa;
	memcpy(re->jit, jit, sizeof(jit));
}


/* ****************** MASKS and LUTS **************** */

static float filt_quadratic(float x)
{
    if (x <  0.0f) x = -x;
    if (x < 0.5f) return 0.75f-(x*x);
    if (x < 1.5f) return 0.50f*(x-1.5f)*(x-1.5f);
    return 0.0f;
}


static float filt_cubic(float x)
{
	float x2= x*x;
	
    if (x <  0.0f) x = -x;
	
    if (x < 1.0f) return 0.5*x*x2 - x2 + 2.0f/3.0f;
    if (x < 2.0f) return (2.0-x)*(2.0-x)*(2.0-x)/6.0f;
    return 0.0f;
}


static float filt_catrom(float x)
{
	float x2= x*x;
	
    if (x <  0.0f) x = -x;
    if (x < 1.0f) return  1.5f*x2*x - 2.5f*x2  + 1.0f;
    if (x < 2.0f) return -0.5f*x2*x + 2.5*x2 - 4.0f*x + 2.0f;
    return 0.0f;
}

static float filt_mitchell(float x)	/* Mitchell & Netravali's two-param cubic */
{
	float b = 1.0f/3.0f, c = 1.0f/3.0f;
	float p0 = (  6.0 -  2.0*b         ) / 6.0;
	float p2 = (-18.0 + 12.0*b +  6.0*c) / 6.0;
	float p3 = ( 12.0 -  9.0*b -  6.0*c) / 6.0;
	float q0 = (	   8.0*b + 24.0*c) / 6.0;
	float q1 = (      - 12.0*b - 48.0*c) / 6.0;
	float q2 = (         6.0*b + 30.0*c) / 6.0;
	float q3 = (       -     b -  6.0*c) / 6.0;

	if (x<-2.0) return 0.0;
	if (x<-1.0) return (q0-x*(q1-x*(q2-x*q3)));
	if (x< 0.0) return (p0+x*x*(p2-x*p3));
	if (x< 1.0) return (p0+x*x*(p2+x*p3));
	if (x< 2.0) return (q0+x*(q1+x*(q2+x*q3)));
	return 0.0;
}

/* x ranges from -1 to 1 */
float RE_filter_value(int type, float x)
{
	float gaussfac= 1.6f;
	
	x= ABS(x);
	
	switch(type) {
		case R_FILTER_BOX:
			if(x>1.0) return 0.0f;
			return 1.0;
			
		case R_FILTER_TENT:
			if(x>1.0) return 0.0f;
			return 1.0f-x;
			
		case R_FILTER_GAUSS:
			x*= gaussfac;
			return (1.0/exp(x*x) - 1.0/exp(gaussfac*gaussfac*2.25));
			
		case R_FILTER_MITCH:
			return filt_mitchell(x*gaussfac);
			
		case R_FILTER_QUAD:
			return filt_quadratic(x*gaussfac);
			
		case R_FILTER_CUBIC:
			return filt_cubic(x*gaussfac);
			
		case R_FILTER_CATROM:
			return filt_catrom(x*gaussfac);
	}
	return 0.0f;
}

static float calc_weight(Render *re, float *weight, int i, int j)
{
	float x, y, dist, totw= 0.0;
	int a;

	for(a=0; a<re->osa; a++) {
		x= re->jit[a][0] + i;
		y= re->jit[a][1] + j;
		dist= sqrt(x*x+y*y);

		weight[a]= 0.0;

		/* Weighting choices */
		switch(re->r.filtertype) {
		case R_FILTER_BOX:
			if(i==0 && j==0) weight[a]= 1.0;
			break;
			
		case R_FILTER_TENT:
			if(dist < re->r.gauss)
				weight[a]= re->r.gauss - dist;
			break;
			
		case R_FILTER_GAUSS:
			x = dist*re->r.gauss;
			weight[a]= (1.0/exp(x*x) - 1.0/exp(re->r.gauss*re->r.gauss*2.25));
			break;
		
		case R_FILTER_MITCH:
			weight[a]= filt_mitchell(dist*re->r.gauss);
			break;
		
		case R_FILTER_QUAD:
			weight[a]= filt_quadratic(dist*re->r.gauss);
			break;
			
		case R_FILTER_CUBIC:
			weight[a]= filt_cubic(dist*re->r.gauss);
			break;
			
		case R_FILTER_CATROM:
			weight[a]= filt_catrom(dist*re->r.gauss);
			break;
			
		}
		
		totw+= weight[a];

	}
	return totw;
}

void free_sample_tables(Render *re)
{
	int a;
	
	if(re->samples) {
		for(a=0; a<9; a++) {
			MEM_freeN(re->samples->fmask1[a]);
			MEM_freeN(re->samples->fmask2[a]);
		}
		
		MEM_freeN(re->samples->centmask);
		MEM_freeN(re->samples);
		re->samples= NULL;
	}
}

/* based on settings in render, it makes the lookup tables */
void make_sample_tables(Render *re)
{
	static int firsttime= 1;
	SampleTables *st;
	float flweight[32];
	float weight[32], totw, val, *fpx1, *fpx2, *fpy1, *fpy2, *m3, *m4;
	int i, j, a;

	/* optimization tables, only once */
	if(firsttime) {
		firsttime= 0;
	}
	
	free_sample_tables(re);
	
	init_render_jit(re);	/* needed for mblur too */
	
	if(re->osa==0) {
		/* just prevents cpu cycles for larger render and copying */
		re->r.filtertype= 0;
		return;
	}
	
	st= re->samples= MEM_callocN(sizeof(SampleTables), "sample tables");
	
	for(a=0; a<9;a++) {
		st->fmask1[a]= MEM_callocN(256*sizeof(float), "initfilt");
		st->fmask2[a]= MEM_callocN(256*sizeof(float), "initfilt");
	}
	for(a=0; a<256; a++) {
		st->cmask[a]= 0;
		if(a &   1) st->cmask[a]++;
		if(a &   2) st->cmask[a]++;
		if(a &   4) st->cmask[a]++;
		if(a &   8) st->cmask[a]++;
		if(a &  16) st->cmask[a]++;
		if(a &  32) st->cmask[a]++;
		if(a &  64) st->cmask[a]++;
		if(a & 128) st->cmask[a]++;
	}
	
	st->centmask= MEM_mallocN((1<<re->osa), "Initfilt3");
	
	for(a=0; a<16; a++) {
		st->centLut[a]= -0.45+((float)a)/16.0;
	}

	/* calculate totw */
	totw= 0.0;
	for(j= -1; j<2; j++) {
		for(i= -1; i<2; i++) {
			totw+= calc_weight(re, weight, i, j);
		}
	}

	for(j= -1; j<2; j++) {
		for(i= -1; i<2; i++) {
			/* calculate using jit, with offset the weights */

			memset(weight, 0, sizeof(weight));
			calc_weight(re, weight, i, j);

			for(a=0; a<16; a++) flweight[a]= weight[a]*(1.0/totw);

			m3= st->fmask1[ 3*(j+1)+i+1 ];
			m4= st->fmask2[ 3*(j+1)+i+1 ];

			for(a=0; a<256; a++) {
				if(a &   1) {
					m3[a]+= flweight[0];
					m4[a]+= flweight[8];
				}
				if(a &   2) {
					m3[a]+= flweight[1];
					m4[a]+= flweight[9];
				}
				if(a &   4) {
					m3[a]+= flweight[2];
					m4[a]+= flweight[10];
				}
				if(a &   8) {
					m3[a]+= flweight[3];
					m4[a]+= flweight[11];
				}
				if(a &  16) {
					m3[a]+= flweight[4];
					m4[a]+= flweight[12];
				}
				if(a &  32) {
					m3[a]+= flweight[5];
					m4[a]+= flweight[13];
				}
				if(a &  64) {
					m3[a]+= flweight[6];
					m4[a]+= flweight[14];
				}
				if(a & 128) {
					m3[a]+= flweight[7];
					m4[a]+= flweight[15];
				}
			}
		}
	}

	/* centmask: the correct subpixel offset per mask */

	fpx1= MEM_mallocN(256*sizeof(float), "initgauss4");
	fpx2= MEM_mallocN(256*sizeof(float), "initgauss4");
	fpy1= MEM_mallocN(256*sizeof(float), "initgauss4");
	fpy2= MEM_mallocN(256*sizeof(float), "initgauss4");
	for(a=0; a<256; a++) {
		fpx1[a]= fpx2[a]= 0.0;
		fpy1[a]= fpy2[a]= 0.0;
		if(a & 1) {
			fpx1[a]+= re->jit[0][0];
			fpy1[a]+= re->jit[0][1];
			fpx2[a]+= re->jit[8][0];
			fpy2[a]+= re->jit[8][1];
		}
		if(a & 2) {
			fpx1[a]+= re->jit[1][0];
			fpy1[a]+= re->jit[1][1];
			fpx2[a]+= re->jit[9][0];
			fpy2[a]+= re->jit[9][1];
		}
		if(a & 4) {
			fpx1[a]+= re->jit[2][0];
			fpy1[a]+= re->jit[2][1];
			fpx2[a]+= re->jit[10][0];
			fpy2[a]+= re->jit[10][1];
		}
		if(a & 8) {
			fpx1[a]+= re->jit[3][0];
			fpy1[a]+= re->jit[3][1];
			fpx2[a]+= re->jit[11][0];
			fpy2[a]+= re->jit[11][1];
		}
		if(a & 16) {
			fpx1[a]+= re->jit[4][0];
			fpy1[a]+= re->jit[4][1];
			fpx2[a]+= re->jit[12][0];
			fpy2[a]+= re->jit[12][1];
		}
		if(a & 32) {
			fpx1[a]+= re->jit[5][0];
			fpy1[a]+= re->jit[5][1];
			fpx2[a]+= re->jit[13][0];
			fpy2[a]+= re->jit[13][1];
		}
		if(a & 64) {
			fpx1[a]+= re->jit[6][0];
			fpy1[a]+= re->jit[6][1];
			fpx2[a]+= re->jit[14][0];
			fpy2[a]+= re->jit[14][1];
		}
		if(a & 128) {
			fpx1[a]+= re->jit[7][0];
			fpy1[a]+= re->jit[7][1];
			fpx2[a]+= re->jit[15][0];
			fpy2[a]+= re->jit[15][1];
		}
	}

	for(a= (1<<re->osa)-1; a>0; a--) {
		val= st->cmask[a & 255] + st->cmask[a>>8];
		i= 8+(15.9*(fpy1[a & 255]+fpy2[a>>8])/val);
		CLAMP(i, 0, 15);
		j= 8+(15.9*(fpx1[a & 255]+fpx2[a>>8])/val);
		CLAMP(j, 0, 15);
		i= j + (i<<4);
		st->centmask[a]= i;
	}

	MEM_freeN(fpx1);
	MEM_freeN(fpx2);
	MEM_freeN(fpy1);
	MEM_freeN(fpy2);
}


/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/* call this after InitState() */
/* per render, there's one persistant viewplane. Parts will set their own viewplanes */
void RE_SetCamera(Render *re, Object *camera)
{
	Camera *cam=NULL;
	rctf viewplane;
	float pixsize, clipsta, clipend;
	float lens, shiftx=0.0, shifty=0.0, winside;
	
	/* question mark */
	re->ycor= ( (float)re->r.yasp)/( (float)re->r.xasp);
	if(re->r.mode & R_FIELDS)
		re->ycor *= 2.0f;
	
	if(camera->type==OB_CAMERA) {
		cam= camera->data;
		
		if(cam->type==CAM_ORTHO) re->r.mode |= R_ORTHO;
		if(cam->flag & CAM_PANORAMA) re->r.mode |= R_PANORAMA;
		
		/* solve this too... all time depending stuff is in convertblender.c?
		 * Need to update the camera early because it's used for projection matrices
		 * and other stuff BEFORE the animation update loop is done 
		 * */
#if 0 // XXX old animation system
		if(cam->ipo) {
			calc_ipo(cam->ipo, frame_to_float(re->scene, re->r.cfra));
			execute_ipo(&cam->id, cam->ipo);
		}
#endif // XXX old animation system
		lens= cam->lens;
		shiftx=cam->shiftx;
		shifty=cam->shifty;

		clipsta= cam->clipsta;
		clipend= cam->clipend;
	}
	else if(camera->type==OB_LAMP) {
		Lamp *la= camera->data;
		float fac= cos( M_PI*la->spotsize/360.0 );
		float phi= acos(fac);
		
		lens= 16.0*fac/sin(phi);
		if(lens==0.0f)
			lens= 35.0;
		clipsta= la->clipsta;
		clipend= la->clipend;
	}
	else {	/* envmap exception... */
		lens= re->lens;
		if(lens==0.0f)
			lens= 16.0;
		
		clipsta= re->clipsta;
		clipend= re->clipend;
		if(clipsta==0.0f || clipend==0.0f) {
			clipsta= 0.1f;
			clipend= 1000.0f;
		}
	}

	/* ortho only with camera available */
	if(cam && (re->r.mode & R_ORTHO)) {
		if( (re->r.xasp*re->winx) >= (re->r.yasp*re->winy) ) {
			re->viewfac= re->winx;
		}
		else {
			re->viewfac= re->ycor*re->winy;
		}
		/* ortho_scale == 1.0 means exact 1 to 1 mapping */
		pixsize= cam->ortho_scale/re->viewfac;
	}
	else {
		if( (re->r.xasp*re->winx) >= (re->r.yasp*re->winy) ) {
			re->viewfac= (re->winx*lens)/32.0;
		}
		else {
			re->viewfac= re->ycor*(re->winy*lens)/32.0;
		}
		
		pixsize= clipsta/re->viewfac;
	}
	
	/* viewplane fully centered, zbuffer fills in jittered between -.5 and +.5 */
	winside= MAX2(re->winx, re->winy);
	viewplane.xmin= -0.5f*(float)re->winx + shiftx*winside; 
	viewplane.ymin= -0.5f*re->ycor*(float)re->winy + shifty*winside;
	viewplane.xmax=  0.5f*(float)re->winx + shiftx*winside; 
	viewplane.ymax=  0.5f*re->ycor*(float)re->winy + shifty*winside; 

	if(re->flag & R_SEC_FIELD) {
		if(re->r.mode & R_ODDFIELD) {
			viewplane.ymin-= .5*re->ycor;
			viewplane.ymax-= .5*re->ycor;
		}
		else {
			viewplane.ymin+= .5*re->ycor;
			viewplane.ymax+= .5*re->ycor;
		}
	}
	/* the window matrix is used for clipping, and not changed during OSA steps */
	/* using an offset of +0.5 here would give clip errors on edges */
	viewplane.xmin= pixsize*(viewplane.xmin);
	viewplane.xmax= pixsize*(viewplane.xmax);
	viewplane.ymin= pixsize*(viewplane.ymin);
	viewplane.ymax= pixsize*(viewplane.ymax);
	
	re->viewdx= pixsize;
	re->viewdy= re->ycor*pixsize;

	if(re->r.mode & R_ORTHO)
		RE_SetOrtho(re, &viewplane, clipsta, clipend);
	else 
		RE_SetWindow(re, &viewplane, clipsta, clipend);

}

void RE_SetPixelSize(Render *re, float pixsize)
{
	re->viewdx= pixsize;
	re->viewdy= re->ycor*pixsize;
}

void RE_GetCameraWindow(struct Render *re, struct Object *camera, int frame, float mat[][4])
{
	re->r.cfra= frame;
	RE_SetCamera(re, camera);
	copy_m4_m4(mat, re->winmat);
}

/* ~~~~~~~~~~~~~~~~ part (tile) calculus ~~~~~~~~~~~~~~~~~~~~~~ */


void freeparts(Render *re)
{
	RenderPart *part= re->parts.first;
	
	while(part) {
		if(part->rectp) MEM_freeN(part->rectp);
		if(part->rectz) MEM_freeN(part->rectz);
		part= part->next;
	}
	BLI_freelistN(&re->parts);
}

void initparts(Render *re)
{
	int nr, xd, yd, partx, party, xparts, yparts;
	int xminb, xmaxb, yminb, ymaxb;
	
	freeparts(re);
	
	/* this is render info for caller, is not reset when parts are freed! */
	re->i.totpart= 0;
	re->i.curpart= 0;
	re->i.partsdone= 0;
	
	/* just for readable code.. */
	xminb= re->disprect.xmin;
	yminb= re->disprect.ymin;
	xmaxb= re->disprect.xmax;
	ymaxb= re->disprect.ymax;
	
	xparts= re->r.xparts;
	yparts= re->r.yparts;
	
	/* mininum part size, but for exr tile saving it was checked already */
	if(!(re->r.scemode & (R_EXR_TILE_FILE|R_FULL_SAMPLE))) {
		if(re->r.mode & R_PANORAMA) {
			if(ceil(re->rectx/(float)xparts) < 8) 
				xparts= 1 + re->rectx/8;
		}
		else
			if(ceil(re->rectx/(float)xparts) < 64) 
				xparts= 1 + re->rectx/64;
		
		if(ceil(re->recty/(float)yparts) < 64) 
			yparts= 1 + re->recty/64;
	}
	
	/* part size */
	partx= ceil(re->rectx/(float)xparts);
	party= ceil(re->recty/(float)yparts);
	
	re->xparts= xparts;
	re->yparts= yparts;
	re->partx= partx;
	re->party= party;
	
	/* calculate rotation factor of 1 pixel */
	if(re->r.mode & R_PANORAMA)
		re->panophi= panorama_pixel_rot(re);
	
	for(nr=0; nr<xparts*yparts; nr++) {
		rcti disprect;
		int rectx, recty;
		
		xd= (nr % xparts);
		yd= (nr-xd)/xparts;
		
		disprect.xmin= xminb+ xd*partx;
		disprect.ymin= yminb+ yd*party;
		
		/* ensure we cover the entire picture, so last parts go to end */
		if(xd<xparts-1) {
			disprect.xmax= disprect.xmin + partx;
			if(disprect.xmax > xmaxb)
				disprect.xmax = xmaxb;
		}
		else disprect.xmax= xmaxb;
		
		if(yd<yparts-1) {
			disprect.ymax= disprect.ymin + party;
			if(disprect.ymax > ymaxb)
				disprect.ymax = ymaxb;
		}
		else disprect.ymax= ymaxb;
		
		rectx= disprect.xmax - disprect.xmin;
		recty= disprect.ymax - disprect.ymin;
		
		/* so, now can we add this part? */
		if(rectx>0 && recty>0) {
			RenderPart *pa= MEM_callocN(sizeof(RenderPart), "new part");
			
			/* Non-box filters need 2 pixels extra to work */
			if((re->r.filtertype || (re->r.mode & R_EDGE))) {
				pa->crop= 2;
				disprect.xmin -= pa->crop;
				disprect.ymin -= pa->crop;
				disprect.xmax += pa->crop;
				disprect.ymax += pa->crop;
				rectx+= 2*pa->crop;
				recty+= 2*pa->crop;
			}
			pa->disprect= disprect;
			pa->rectx= rectx;
			pa->recty= recty;

			BLI_addtail(&re->parts, pa);
			re->i.totpart++;
		}
	}
}



