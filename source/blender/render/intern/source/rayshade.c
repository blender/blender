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
 * The Original Code is Copyright (C) 1990-1998 NeoGeo BV.
 * All rights reserved.
 *
 * Contributors: 2004/2005 Blender Foundation, full recode
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <float.h>

#include "MEM_guardedalloc.h"

#include "DNA_material_types.h"
#include "DNA_lamp_types.h"

#include "BKE_global.h"
#include "BKE_node.h"
#include "BKE_utildefines.h"

#include "BLI_arithb.h"
#include "BLI_rand.h"
#include "BLI_jitter.h"

#include "PIL_time.h"

#include "render_types.h"
#include "renderpipeline.h"
#include "rendercore.h"
#include "renderdatabase.h"
#include "pixelblending.h"
#include "pixelshading.h"
#include "shading.h"
#include "texture.h"

#include "RE_raytrace.h"

#define RAY_TRA		1
#define RAY_TRAFLIP	2

#define DEPTH_SHADOW_TRA  10

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
/* defined in pipeline.c, is hardcopy of active dynamic allocated Render */
/* only to be used here in this file, it's for speed */
extern struct Render R;
/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

static void vlr_face_coords(RayFace *face, float **v1, float **v2, float **v3, float **v4)
{
	VlakRen *vlr= (VlakRen*)face;

	*v1 = (vlr->v1)? vlr->v1->co: NULL;
	*v2 = (vlr->v2)? vlr->v2->co: NULL;
	*v3 = (vlr->v3)? vlr->v3->co: NULL;
	*v4 = (vlr->v4)? vlr->v4->co: NULL;
}

static int vlr_check_intersect(Isect *is, RayFace *face)
{
	VlakRen *vlr = (VlakRen*)face;

	/* I know... cpu cycle waste, might do smarter once */
	if(is->mode==RE_RAY_MIRROR)
		return !(vlr->mat->mode & MA_ONLYCAST);
	else
		return (is->lay & vlr->lay);
}

void freeraytree(Render *re)
{
	if(re->raytree) {
		RE_ray_tree_free(re->raytree);
		re->raytree= NULL;
	}
}

void makeraytree(Render *re)
{
	VlakRen *vlr= NULL;
	float min[3], max[3];
	double lasttime= PIL_check_seconds_timer();
	int v, totface = 0;

	INIT_MINMAX(min, max);

	/* first min max raytree space */
	for(v=0;v<re->totvlak;v++) {
		if((v & 255)==0) vlr= re->vlaknodes[v>>8].vlak;
		else vlr++;
		if(vlr->mat->mode & MA_TRACEBLE) {	
			if((vlr->mat->mode & MA_WIRE)==0) {	
				
				DO_MINMAX(vlr->v1->co, min, max);
				DO_MINMAX(vlr->v2->co, min, max);
				DO_MINMAX(vlr->v3->co, min, max);
				if(vlr->v4) {
					DO_MINMAX(vlr->v4->co, min, max);
				}

				totface++;
			}
		}
	}

	re->raytree= RE_ray_tree_create(re->r.ocres, totface, min, max, vlr_face_coords, vlr_check_intersect);

	if(min[0] > max[0]) { /* empty raytree */
		RE_ray_tree_done(re->raytree);
		return;	
	}

	for(v=0; v<re->totvlak; v++) {
		if((v & 255)==0) {
			double time= PIL_check_seconds_timer();

			vlr= re->vlaknodes[v>>8].vlak;
			if(re->test_break())
				break;
			if(time-lasttime>1.0f) {
				char str[32];
				sprintf(str, "Filling Octree: %d", v);
				re->i.infostr= str;
				re->stats_draw(&re->i);
				re->i.infostr= NULL;
				lasttime= time;
			}
		}
		else vlr++;
		
		if(vlr->mat->mode & MA_TRACEBLE) {
			if((vlr->mat->mode & MA_WIRE)==0) {	
				RE_ray_tree_add_face(re->raytree, (RayFace*)vlr);
			}
		}
	}

	RE_ray_tree_done(re->raytree);
	
	re->i.infostr= NULL;
	re->stats_draw(&re->i);
}

static void shade_ray(Isect *is, ShadeInput *shi, ShadeResult *shr)
{
	VlakRen *vlr= (VlakRen*)is->face;
	int osatex= 0, norflip;
	
	/* set up view vector */
	VECCOPY(shi->view, is->vec);

	/* render co */
	shi->co[0]= is->start[0]+is->labda*(shi->view[0]);
	shi->co[1]= is->start[1]+is->labda*(shi->view[1]);
	shi->co[2]= is->start[2]+is->labda*(shi->view[2]);
	
	Normalize(shi->view);

	shi->vlr= vlr;
	shi->mat= vlr->mat;
	memcpy(&shi->r, &shi->mat->r, 23*sizeof(float));	// note, keep this synced with render_types.h
	shi->har= shi->mat->har;
	
	// Osa structs we leave unchanged now
	SWAP(int, osatex, shi->osatex);
	
	shi->dxco[0]= shi->dxco[1]= shi->dxco[2]= 0.0f;
	shi->dyco[0]= shi->dyco[1]= shi->dyco[2]= 0.0f;
	
	// but, set Osa stuff to zero where it can confuse texture code
	if(shi->mat->texco & (TEXCO_NORM|TEXCO_REFL) ) {
		shi->dxno[0]= shi->dxno[1]= shi->dxno[2]= 0.0f;
		shi->dyno[0]= shi->dyno[1]= shi->dyno[2]= 0.0f;
	}

	/* face normal, check for flip, need to set puno here */
	norflip= (vlr->n[0]*shi->view[0]+vlr->n[1]*shi->view[1]+vlr->n[2]*shi->view[2]) < 0.0f;
	
	if(norflip)
		shi->puno= vlr->puno ^ 15;// only flip lower 4 bits
	else
		shi->puno= vlr->puno;
	
	if(vlr->v4) {
		if(is->isect==2) 
			shade_input_set_triangle_i(shi, vlr, 2, 1, 3);
		else
			shade_input_set_triangle_i(shi, vlr, 0, 1, 3);
	}
	else {
		shade_input_set_triangle_i(shi, vlr, 0, 1, 2);
	}
	
	/* shade_input_set_triangle_i() sets facenor, now we flip */
	if(norflip) {
		shi->facenor[0]= -vlr->n[0];
		shi->facenor[1]= -vlr->n[1];
		shi->facenor[2]= -vlr->n[2];
	}	
	
	shi->u= is->u;
	shi->v= is->v;
	shi->dx_u= shi->dx_v= shi->dy_u= shi->dy_v=  0.0f;
	shade_input_set_normals(shi);
	shade_input_set_shade_texco(shi);
	
	if(is->mode==RE_RAY_SHADOW_TRA) 
		shade_color(shi, shr);
	else {
		if(shi->mat->nodetree && shi->mat->use_nodes) {
			ntreeShaderExecTree(shi->mat->nodetree, shi, shr);
			shi->mat= vlr->mat;		/* shi->mat is being set in nodetree */
		}
		else
			shade_material_loop(shi, shr);
		
		/* raytrace likes to separate the spec color */
		VECSUB(shr->diff, shr->combined, shr->spec);
	}	
	
	SWAP(int, osatex, shi->osatex);  // XXXXX!!!!

}

static int refraction(float *refract, float *n, float *view, float index)
{
	float dot, fac;

	VECCOPY(refract, view);
	
	dot= view[0]*n[0] + view[1]*n[1] + view[2]*n[2];

	if(dot>0.0f) {
		index = 1.0f/index;
		fac= 1.0f - (1.0f - dot*dot)*index*index;
		if(fac<= 0.0f) return 0;
		fac= -dot*index + sqrt(fac);
	}
	else {
		fac= 1.0f - (1.0f - dot*dot)*index*index;
		if(fac<= 0.0f) return 0;
		fac= -dot*index - sqrt(fac);
	}

	refract[0]= index*view[0] + fac*n[0];
	refract[1]= index*view[1] + fac*n[1];
	refract[2]= index*view[2] + fac*n[2];

	return 1;
}

/* orn = original face normal */
static void reflection(float *ref, float *n, float *view, float *orn)
{
	float f1;
	
	f1= -2.0f*(n[0]*view[0]+ n[1]*view[1]+ n[2]*view[2]);
	
	ref[0]= (view[0]+f1*n[0]);
	ref[1]= (view[1]+f1*n[1]);
	ref[2]= (view[2]+f1*n[2]);

	if(orn) {
		/* test phong normals, then we should prevent vector going to the back */
		f1= ref[0]*orn[0]+ ref[1]*orn[1]+ ref[2]*orn[2];
		if(f1>0.0f) {
			f1+= .01f;
			ref[0]-= f1*orn[0];
			ref[1]-= f1*orn[1];
			ref[2]-= f1*orn[2];
		}
	}
}

#if 0
static void color_combine(float *result, float fac1, float fac2, float *col1, float *col2)
{
	float col1t[3], col2t[3];
	
	col1t[0]= sqrt(col1[0]);
	col1t[1]= sqrt(col1[1]);
	col1t[2]= sqrt(col1[2]);
	col2t[0]= sqrt(col2[0]);
	col2t[1]= sqrt(col2[1]);
	col2t[2]= sqrt(col2[2]);

	result[0]= (fac1*col1t[0] + fac2*col2t[0]);
	result[0]*= result[0];
	result[1]= (fac1*col1t[1] + fac2*col2t[1]);
	result[1]*= result[1];
	result[2]= (fac1*col1t[2] + fac2*col2t[2]);
	result[2]*= result[2];
}
#endif

static float shade_by_transmission(Isect *is, ShadeInput *shi, ShadeResult *shr)
{
	float dx, dy, dz, d, p;

	if (0 == (shi->mat->mode & (MA_RAYTRANSP|MA_ZTRA)))
		return -1;
	   
	if (shi->mat->tx_limit <= 0.0f) {
		d= 1.0f;
	} 
	else {
		/* shi.co[] calculated by shade_ray() */
		dx= shi->co[0] - is->start[0];
		dy= shi->co[1] - is->start[1];
		dz= shi->co[2] - is->start[2];
		d= sqrt(dx*dx+dy*dy+dz*dz);
		if (d > shi->mat->tx_limit)
			d= shi->mat->tx_limit;

		p = shi->mat->tx_falloff;
		if(p < 0.0f) p= 0.0f;
		else if (p > 10.0f) p= 10.0f;

		shr->alpha *= pow(d, p);
		if (shr->alpha > 1.0f)
			shr->alpha= 1.0f;
	}

	return d;
}

/* the main recursive tracer itself */
static void traceray(ShadeInput *origshi, short depth, float *start, float *vec, float *col, VlakRen *vlr, int traflag)
{
	ShadeInput shi;
	ShadeResult shr;
	Isect isec;
	float f, f1, fr, fg, fb;
	float ref[3], maxsize= RE_ray_tree_max_size(R.raytree);

	VECCOPY(isec.start, start);
	isec.end[0]= start[0]+maxsize*vec[0];
	isec.end[1]= start[1]+maxsize*vec[1];
	isec.end[2]= start[2]+maxsize*vec[2];
	isec.mode= RE_RAY_MIRROR;
	isec.faceorig= (RayFace*)vlr;

	if(RE_ray_tree_intersect(R.raytree, &isec)) {
		float d= 1.0f;
		
		shi.mask= origshi->mask;
		shi.osatex= origshi->osatex;
		shi.depth= 1;					/* only used to indicate tracing */
		shi.thread= origshi->thread;
		shi.sample= 0;
		shi.xs= origshi->xs;
		shi.ys= origshi->ys;
		shi.lay= origshi->lay;
		shi.passflag= SCE_PASS_COMBINED; /* result of tracing needs no pass info */
		shi.combinedflag= 0xFFFFFF;		 /* ray trace does all options */
		shi.do_preview= 0;
		shi.light_override= origshi->light_override;
		shi.mat_override= origshi->mat_override;
		
		memset(&shr, 0, sizeof(ShadeResult));
		
		shade_ray(&isec, &shi, &shr);
		if (traflag & RAY_TRA)
			d= shade_by_transmission(&isec, &shi, &shr);
		
		if(depth>0) {

			if(shi.mat->mode_l & (MA_RAYTRANSP|MA_ZTRA) && shr.alpha < 1.0f) {
				float nf, f, f1, refract[3], tracol[4];
				
				tracol[0]= shi.r;
				tracol[1]= shi.g;
				tracol[2]= shi.b;
				tracol[3]= col[3];	// we pass on and accumulate alpha
				
				if(shi.mat->mode & MA_RAYTRANSP) {
					/* odd depths: use normal facing viewer, otherwise flip */
					if(traflag & RAY_TRAFLIP) {
						float norm[3];
						norm[0]= - shi.vn[0];
						norm[1]= - shi.vn[1];
						norm[2]= - shi.vn[2];
						if (!refraction(refract, norm, shi.view, shi.ang))
							reflection(refract, norm, shi.view, shi.vn);
					}
					else {
						if (!refraction(refract, shi.vn, shi.view, shi.ang))
							reflection(refract, shi.vn, shi.view, shi.vn);
					}
					traflag |= RAY_TRA;
					traceray(origshi, depth-1, shi.co, refract, tracol, shi.vlr, traflag ^ RAY_TRAFLIP);
				}
				else
					traceray(origshi, depth-1, shi.co, shi.view, tracol, shi.vlr, 0);
				
				f= shr.alpha; f1= 1.0f-f;
				nf= d * shi.mat->filter;
				fr= 1.0f+ nf*(shi.r-1.0f);
				fg= 1.0f+ nf*(shi.g-1.0f);
				fb= 1.0f+ nf*(shi.b-1.0f);
				shr.diff[0]= f*shr.diff[0] + f1*fr*tracol[0];
				shr.diff[1]= f*shr.diff[1] + f1*fg*tracol[1];
				shr.diff[2]= f*shr.diff[2] + f1*fb*tracol[2];
				
				shr.spec[0] *=f;
				shr.spec[1] *=f;
				shr.spec[2] *=f;

				col[3]= f1*tracol[3] + f;
			}
			else 
				col[3]= 1.0f;

			if(shi.mat->mode_l & MA_RAYMIRROR) {
				f= shi.ray_mirror;
				if(f!=0.0f) f*= fresnel_fac(shi.view, shi.vn, shi.mat->fresnel_mir_i, shi.mat->fresnel_mir);
			}
			else f= 0.0f;
			
			if(f!=0.0f) {
				float mircol[4];
				
				reflection(ref, shi.vn, shi.view, NULL);			
				traceray(origshi, depth-1, shi.co, ref, mircol, shi.vlr, 0);
			
				f1= 1.0f-f;

				/* combine */
				//color_combine(col, f*fr*(1.0f-shr.spec[0]), f1, col, shr.diff);
				//col[0]+= shr.spec[0];
				//col[1]+= shr.spec[1];
				//col[2]+= shr.spec[2];
				
				fr= shi.mirr;
				fg= shi.mirg;
				fb= shi.mirb;
		
				col[0]= f*fr*(1.0f-shr.spec[0])*mircol[0] + f1*shr.diff[0] + shr.spec[0];
				col[1]= f*fg*(1.0f-shr.spec[1])*mircol[1] + f1*shr.diff[1] + shr.spec[1];
				col[2]= f*fb*(1.0f-shr.spec[2])*mircol[2] + f1*shr.diff[2] + shr.spec[2];
			}
			else {
				col[0]= shr.diff[0] + shr.spec[0];
				col[1]= shr.diff[1] + shr.spec[1];
				col[2]= shr.diff[2] + shr.spec[2];
			}
		}
		else {
			col[0]= shr.diff[0] + shr.spec[0];
			col[1]= shr.diff[1] + shr.spec[1];
			col[2]= shr.diff[2] + shr.spec[2];
		}
		
	}
	else {	/* sky */
		VECCOPY(shi.view, vec);
		Normalize(shi.view);
		
		shadeSkyView(col, isec.start, shi.view, NULL);
	}
}

/* **************** jitter blocks ********** */

/* calc distributed planar energy */

static void DP_energy(float *table, float *vec, int tot, float xsize, float ysize)
{
	int x, y, a;
	float *fp, force[3], result[3];
	float dx, dy, dist, min;
	
	min= MIN2(xsize, ysize);
	min*= min;
	result[0]= result[1]= 0.0f;
	
	for(y= -1; y<2; y++) {
		dy= ysize*y;
		for(x= -1; x<2; x++) {
			dx= xsize*x;
			fp= table;
			for(a=0; a<tot; a++, fp+= 2) {
				force[0]= vec[0] - fp[0]-dx;
				force[1]= vec[1] - fp[1]-dy;
				dist= force[0]*force[0] + force[1]*force[1];
				if(dist < min && dist>0.0f) {
					result[0]+= force[0]/dist;
					result[1]+= force[1]/dist;
				}
			}
		}
	}
	vec[0] += 0.1*min*result[0]/(float)tot;
	vec[1] += 0.1*min*result[1]/(float)tot;
	// cyclic clamping
	vec[0]= vec[0] - xsize*floor(vec[0]/xsize + 0.5);
	vec[1]= vec[1] - ysize*floor(vec[1]/ysize + 0.5);
}

// random offset of 1 in 2
static void jitter_plane_offset(float *jitter1, float *jitter2, int tot, float sizex, float sizey, float ofsx, float ofsy)
{
	float dsizex= sizex*ofsx;
	float dsizey= sizey*ofsy;
	float hsizex= 0.5*sizex, hsizey= 0.5*sizey;
	int x;
	
	for(x=tot; x>0; x--, jitter1+=2, jitter2+=2) {
		jitter2[0]= jitter1[0] + dsizex;
		jitter2[1]= jitter1[1] + dsizey;
		if(jitter2[0] > hsizex) jitter2[0]-= sizex;
		if(jitter2[1] > hsizey) jitter2[1]-= sizey;
	}
}

/* called from convertBlenderScene.c */
/* we do this in advance to get consistant random, not alter the render seed, and be threadsafe */
void init_jitter_plane(LampRen *lar)
{
	float *fp;
	int x, iter=12, tot= lar->ray_totsamp;
	
	/* at least 4, or max threads+1 tables */
	if(BLENDER_MAX_THREADS < 4) x= 4;
	else x= BLENDER_MAX_THREADS+1;
	fp= lar->jitter= MEM_mallocN(x*tot*2*sizeof(float), "lamp jitter tab");
	
	/* set per-lamp fixed seed */
	BLI_srandom(tot);
	
	/* fill table with random locations, area_size large */
	for(x=0; x<tot; x++, fp+=2) {
		fp[0]= (BLI_frand()-0.5)*lar->area_size;
		fp[1]= (BLI_frand()-0.5)*lar->area_sizey;
	}
	
	while(iter--) {
		fp= lar->jitter;
		for(x=tot; x>0; x--, fp+=2) {
			DP_energy(lar->jitter, fp, tot, lar->area_size, lar->area_sizey);
		}
	}
	
	/* create the dithered tables (could just check lamp type!) */
	jitter_plane_offset(lar->jitter, lar->jitter+2*tot, tot, lar->area_size, lar->area_sizey, 0.5f, 0.0f);
	jitter_plane_offset(lar->jitter, lar->jitter+4*tot, tot, lar->area_size, lar->area_sizey, 0.5f, 0.5f);
	jitter_plane_offset(lar->jitter, lar->jitter+6*tot, tot, lar->area_size, lar->area_sizey, 0.0f, 0.5f);
}

/* table around origin, -0.5*size to 0.5*size */
static float *give_jitter_plane(LampRen *lar, int thread, int xs, int ys)
{
	int tot;
	
	tot= lar->ray_totsamp;
			
	if(lar->ray_samp_type & LA_SAMP_JITTER) {
		/* made it threadsafe */
		
		if(lar->xold[thread]!=xs || lar->yold[thread]!=ys) {
			jitter_plane_offset(lar->jitter, lar->jitter+2*(thread+1)*tot, tot, lar->area_size, lar->area_sizey, BLI_thread_frand(thread), BLI_thread_frand(thread));
			lar->xold[thread]= xs; 
			lar->yold[thread]= ys;
		}
		return lar->jitter+2*(thread+1)*tot;
	}
	if(lar->ray_samp_type & LA_SAMP_DITHER) {
		return lar->jitter + 2*tot*((xs & 1)+2*(ys & 1));
	}
	
	return lar->jitter;
}


/* ***************** main calls ************** */


/* extern call from render loop */
void ray_trace(ShadeInput *shi, ShadeResult *shr)
{
	VlakRen *vlr;
	float i, f, f1, fr, fg, fb, vec[3], mircol[4], tracol[4];
	float diff[3];
	int do_tra, do_mir;
	
	do_tra= ((shi->mat->mode & (MA_RAYTRANSP)) && shr->alpha!=1.0f);
	do_mir= ((shi->mat->mode & MA_RAYMIRROR) && shi->ray_mirror!=0.0f);
	vlr= shi->vlr;
	
	/* raytrace mirror amd refract like to separate the spec color */
	if(shi->combinedflag & SCE_PASS_SPEC)
		VECSUB(diff, shr->combined, shr->spec) /* no ; */
	else
		VECCOPY(diff, shr->combined);
	
	if(do_tra) {
		float refract[3];
		float olddiff[3];
		
		tracol[3]= shr->alpha;
		
		refraction(refract, shi->vn, shi->view, shi->ang);
		traceray(shi, shi->mat->ray_depth_tra, shi->co, refract, tracol, shi->vlr, RAY_TRA|RAY_TRAFLIP);
		
		f= shr->alpha; f1= 1.0f-f;
		fr= 1.0f+ shi->mat->filter*(shi->r-1.0f);
		fg= 1.0f+ shi->mat->filter*(shi->g-1.0f);
		fb= 1.0f+ shi->mat->filter*(shi->b-1.0f);
		
		/* for refract pass */
		VECCOPY(olddiff, diff);
		
		diff[0]= f*diff[0] + f1*fr*tracol[0];
		diff[1]= f*diff[1] + f1*fg*tracol[1];
		diff[2]= f*diff[2] + f1*fb*tracol[2];
		
		if(shi->passflag & SCE_PASS_REFRACT)
			VECSUB(shr->refr, diff, olddiff);
		
		if(shi->combinedflag & SCE_PASS_REFRACT)
			VECCOPY(olddiff, diff);
		
		shr->alpha= tracol[3];
	}
	
	if(do_mir) {
	
		i= shi->ray_mirror*fresnel_fac(shi->view, shi->vn, shi->mat->fresnel_mir_i, shi->mat->fresnel_mir);
		if(i!=0.0f) {
			
			fr= i*shi->mirr;
			fg= i*shi->mirg;
			fb= i*shi->mirb;

			if(vlr->flag & R_SMOOTH) 
				reflection(vec, shi->vn, shi->view, shi->facenor);
			else
				reflection(vec, shi->vn, shi->view, NULL);
	
			traceray(shi, shi->mat->ray_depth, shi->co, vec, mircol, shi->vlr, 0);
			
			if(shi->passflag & SCE_PASS_REFLECT) {
				/* mirror pass is not blocked out with spec */
				shr->refl[0]= fr*mircol[0] - fr*diff[0];
				shr->refl[1]= fg*mircol[1] - fg*diff[1];
				shr->refl[2]= fb*mircol[2] - fb*diff[2];
			}
			
			if(shi->combinedflag & SCE_PASS_REFLECT) {
				
				f= fr*(1.0f-shr->spec[0]);	f1= 1.0f-i;
				diff[0]= f*mircol[0] + f1*diff[0];
				
				f= fg*(1.0f-shr->spec[1]);	f1= 1.0f-i;
				diff[1]= f*mircol[1] + f1*diff[1];
				
				f= fb*(1.0f-shr->spec[2]);	f1= 1.0f-i;
				diff[2]= f*mircol[2] + f1*diff[2];
			}
		}
	}
	/* put back together */
	if(shi->combinedflag & SCE_PASS_SPEC)
		VECADD(shr->combined, diff, shr->spec) /* no ; */
	else
		VECCOPY(shr->combined, diff);
}

/* color 'shadfac' passes through 'col' with alpha and filter */
/* filter is only applied on alpha defined transparent part */
static void addAlphaLight(float *shadfac, float *col, float alpha, float filter)
{
	float fr, fg, fb;
	
	fr= 1.0f+ filter*(col[0]-1.0f);
	fg= 1.0f+ filter*(col[1]-1.0f);
	fb= 1.0f+ filter*(col[2]-1.0f);
	
	shadfac[0]= alpha*col[0] + fr*(1.0f-alpha)*shadfac[0];
	shadfac[1]= alpha*col[1] + fg*(1.0f-alpha)*shadfac[1];
	shadfac[2]= alpha*col[2] + fb*(1.0f-alpha)*shadfac[2];
	
	shadfac[3]= (1.0f-alpha)*shadfac[3];
}

static void ray_trace_shadow_tra(Isect *is, int depth, int traflag)
{
	/* ray to lamp, find first face that intersects, check alpha properties,
	   if it has col[3]>0.0f  continue. so exit when alpha is full */
	ShadeInput shi;
	ShadeResult shr;

	if(RE_ray_tree_intersect(R.raytree, is)) {
		float d= 1.0f;
		/* we got a face */
		
		shi.depth= 1;					/* only used to indicate tracing */
		shi.mask= 1;
		shi.osatex= 0;
		shi.thread= shi.sample= 0;
		shi.lay= 0;
		shi.passflag= 0;
		shi.combinedflag= 0;
		shi.do_preview= 0;
		shi.light_override= NULL;
		shi.mat_override= NULL;
		
		shade_ray(is, &shi, &shr);
		if (traflag & RAY_TRA)
			d= shade_by_transmission(is, &shi, &shr);
		
		/* mix colors based on shadfac (rgb + amount of light factor) */
		addAlphaLight(is->col, shr.diff, shr.alpha, d*shi.mat->filter);
		
		if(depth>0 && is->col[3]>0.0f) {
			
			/* adapt isect struct */
			VECCOPY(is->start, shi.co);
			is->faceorig= (RayFace*)shi.vlr;

			ray_trace_shadow_tra(is, depth-1, traflag | RAY_TRA);
		}
	}
}

/* not used, test function for ambient occlusion (yaf: pathlight) */
/* main problem; has to be called within shading loop, giving unwanted recursion */
int ray_trace_shadow_rad(ShadeInput *ship, ShadeResult *shr)
{
	static int counter=0, only_one= 0;
	extern float hashvectf[];
	Isect isec;
	ShadeInput shi;
	ShadeResult shr_t;
	float vec[3], accum[3], div= 0.0f, maxsize= RE_ray_tree_max_size(R.raytree);
	int a;
	
	if(only_one) {
		return 0;
	}
	only_one= 1;
	
	accum[0]= accum[1]= accum[2]= 0.0f;
	isec.mode= RE_RAY_MIRROR;
	isec.faceorig= (RayFace*)ship->vlr;
	
	for(a=0; a<8*8; a++) {
		
		counter+=3;
		counter %= 768;
		VECCOPY(vec, hashvectf+counter);
		if(ship->vn[0]*vec[0]+ship->vn[1]*vec[1]+ship->vn[2]*vec[2]>0.0f) {
			vec[0]-= vec[0];
			vec[1]-= vec[1];
			vec[2]-= vec[2];
		}
		VECCOPY(isec.start, ship->co);
		isec.end[0]= isec.start[0] + maxsize*vec[0];
		isec.end[1]= isec.start[1] + maxsize*vec[1];
		isec.end[2]= isec.start[2] + maxsize*vec[2];
		
		if(RE_ray_tree_intersect(R.raytree, &isec)) {
			float fac;
			shade_ray(&isec, &shi, &shr_t);
			fac= isec.labda*isec.labda;
			fac= 1.0f;
			accum[0]+= fac*(shr_t.diff[0]+shr_t.spec[0]);
			accum[1]+= fac*(shr_t.diff[1]+shr_t.spec[1]);
			accum[2]+= fac*(shr_t.diff[2]+shr_t.spec[2]);
			div+= fac;
		}
		else div+= 1.0f;
	}
	
	if(div!=0.0f) {
		shr->diff[0]+= accum[0]/div;
		shr->diff[1]+= accum[1]/div;
		shr->diff[2]+= accum[2]/div;
	}
	shr->alpha= 1.0f;
	
	only_one= 0;
	return 1;
}

/* aolight: function to create random unit sphere vectors for total random sampling */
static void RandomSpherical(float *v)
{
	float r;
	v[2] = 2.f*BLI_frand()-1.f;
	if ((r = 1.f - v[2]*v[2])>0.f) {
		float a = 6.283185307f*BLI_frand();
		r = sqrt(r);
		v[0] = r * cos(a);
		v[1] = r * sin(a);
	}
	else v[2] = 1.f;
}

/* calc distributed spherical energy */
static void DS_energy(float *sphere, int tot, float *vec)
{
	float *fp, fac, force[3], res[3];
	int a;
	
	res[0]= res[1]= res[2]= 0.0f;
	
	for(a=0, fp=sphere; a<tot; a++, fp+=3) {
		VecSubf(force, vec, fp);
		fac= force[0]*force[0] + force[1]*force[1] + force[2]*force[2];
		if(fac!=0.0f) {
			fac= 1.0f/fac;
			res[0]+= fac*force[0];
			res[1]+= fac*force[1];
			res[2]+= fac*force[2];
		}
	}

	VecMulf(res, 0.5);
	VecAddf(vec, vec, res);
	Normalize(vec);
	
}

/* called from convertBlenderScene.c */
/* creates an equally distributed spherical sample pattern */
/* and allocates threadsafe memory */
void init_ao_sphere(World *wrld)
{
	float *fp;
	int a, tot, iter= 16;

	/* we make twice the amount of samples, because only a hemisphere is used */
	tot= 2*wrld->aosamp*wrld->aosamp;
	
	wrld->aosphere= MEM_mallocN(3*tot*sizeof(float), "AO sphere");
	
	/* fixed random */
	BLI_srandom(tot);
	
	/* init */
	fp= wrld->aosphere;
	for(a=0; a<tot; a++, fp+= 3) {
		RandomSpherical(fp);
	}
	
	while(iter--) {
		for(a=0, fp= wrld->aosphere; a<tot; a++, fp+= 3) {
			DS_energy(wrld->aosphere, tot, fp);
		}
	}
	
	/* tables */
	wrld->aotables= MEM_mallocN(BLENDER_MAX_THREADS*3*tot*sizeof(float), "AO tables");
}

/* give per thread a table, we have to compare xs ys because of way OSA works... */
static float *threadsafe_table_sphere(int test, int thread, int xs, int ys, int tot)
{
	static int xso[BLENDER_MAX_THREADS], yso[BLENDER_MAX_THREADS];
	static int firsttime= 1;
	
	if(firsttime) {
		memset(xso, 255, sizeof(xso));
		memset(yso, 255, sizeof(yso));
		firsttime= 0;
	}
	
	if(xs==xso[thread] && ys==yso[thread]) return R.wrld.aotables+ thread*tot*3;
	if(test) return NULL;
	xso[thread]= xs; yso[thread]= ys;
	return R.wrld.aotables+ thread*tot*3;
}

static float *sphere_sampler(int type, int resol, int thread, int xs, int ys)
{
	int tot;
	float *vec;
	
	if(resol>16) resol= 16;
	
	tot= 2*resol*resol;

	if (type & WO_AORNDSMP) {
		static float sphere[2*3*256];
		int a;
		
		/* total random sampling. NOT THREADSAFE! (should be removed, is not useful) */
		vec= sphere;
		for (a=0; a<tot; a++, vec+=3) {
			RandomSpherical(vec);
		}
		
		return sphere;
	} 
	else {
		float *sphere;
		float cosfi, sinfi, cost, sint;
		float ang, *vec1;
		int a;
		
		sphere= threadsafe_table_sphere(1, thread, xs, ys, tot);	// returns table if xs and ys were equal to last call
		if(sphere==NULL) {
			sphere= threadsafe_table_sphere(0, thread, xs, ys, tot);
			
			// random rotation
			ang= BLI_thread_frand(thread);
			sinfi= sin(ang); cosfi= cos(ang);
			ang= BLI_thread_frand(thread);
			sint= sin(ang); cost= cos(ang);
			
			vec= R.wrld.aosphere;
			vec1= sphere;
			for (a=0; a<tot; a++, vec+=3, vec1+=3) {
				vec1[0]= cost*cosfi*vec[0] - sinfi*vec[1] + sint*cosfi*vec[2];
				vec1[1]= cost*sinfi*vec[0] + cosfi*vec[1] + sint*sinfi*vec[2];
				vec1[2]= -sint*vec[0] + cost*vec[2];			
			}
		}
		return sphere;
	}
}


/* extern call from shade_lamp_loop, ambient occlusion calculus */
void ray_ao(ShadeInput *shi, float *shadfac)
{
	Isect isec;
	float *vec, *nrm, div, bias, sh=0.0f;
	float maxdist = R.wrld.aodist;
	float dxyview[3];
	int j= -1, tot, actual=0, skyadded=0, aocolor;

	isec.faceorig= (RayFace*)shi->vlr;
	isec.face_last= NULL;
	isec.mode= (R.wrld.aomode & WO_AODIST)?RE_RAY_SHADOW_TRA:RE_RAY_SHADOW;
	isec.lay= -1;

	shadfac[0]= shadfac[1]= shadfac[2]= 0.0f;

	/* bias prevents smoothed faces to appear flat */
	if(shi->vlr->flag & R_SMOOTH) {
		bias= G.scene->world->aobias;
		nrm= shi->vn;
	}
	else {
		bias= 0.0f;
		nrm= shi->facenor;
	}

	/* prevent sky colors to be added for only shadow (shadow becomes alpha) */
	aocolor= R.wrld.aocolor;
	if(shi->mat->mode & MA_ONLYSHADOW)
		aocolor= WO_AOPLAIN;
	
	vec= sphere_sampler(R.wrld.aomode, R.wrld.aosamp, shi->thread, shi->xs, shi->ys);
	
	// warning: since we use full sphere now, and dotproduct is below, we do twice as much
	tot= 2*R.wrld.aosamp*R.wrld.aosamp;

	if(aocolor == WO_AOSKYTEX) {
		dxyview[0]= 1.0f/(float)R.wrld.aosamp;
		dxyview[1]= 1.0f/(float)R.wrld.aosamp;
		dxyview[2]= 0.0f;
	}
	
	while(tot--) {
		
		if ((vec[0]*nrm[0] + vec[1]*nrm[1] + vec[2]*nrm[2]) > bias) {
			/* only ao samples for mask */
			if(R.r.mode & R_OSA) {
				j++;
				if(j==R.osa) j= 0;
				if(!(shi->mask & (1<<j))) {
					vec+=3;
					continue;
				}
			}
			
			actual++;
			
			/* always set start/end, 3dda clips it */
			VECCOPY(isec.start, shi->co);
			isec.end[0] = shi->co[0] - maxdist*vec[0];
			isec.end[1] = shi->co[1] - maxdist*vec[1];
			isec.end[2] = shi->co[2] - maxdist*vec[2];
			
			/* do the trace */
			if(RE_ray_tree_intersect(R.raytree, &isec)) {
				if (R.wrld.aomode & WO_AODIST) sh+= exp(-isec.labda*R.wrld.aodistfac); 
				else sh+= 1.0f;
			}
			else if(aocolor!=WO_AOPLAIN) {
				float skycol[4];
				float fac, view[3];
				
				view[0]= -vec[0];
				view[1]= -vec[1];
				view[2]= -vec[2];
				Normalize(view);
				
				if(aocolor==WO_AOSKYCOL) {
					fac= 0.5*(1.0f+view[0]*R.grvec[0]+ view[1]*R.grvec[1]+ view[2]*R.grvec[2]);
					shadfac[0]+= (1.0f-fac)*R.wrld.horr + fac*R.wrld.zenr;
					shadfac[1]+= (1.0f-fac)*R.wrld.horg + fac*R.wrld.zeng;
					shadfac[2]+= (1.0f-fac)*R.wrld.horb + fac*R.wrld.zenb;
				}
				else {	/* WO_AOSKYTEX */
					shadeSkyView(skycol, isec.start, view, dxyview);
					shadfac[0]+= skycol[0];
					shadfac[1]+= skycol[1];
					shadfac[2]+= skycol[2];
				}
				skyadded++;
			}
		}
		// samples
		vec+= 3;
	}
	
	if(actual==0) sh= 1.0f;
	else sh = 1.0f - sh/((float)actual);
	
	if(aocolor!=WO_AOPLAIN && skyadded) {
		div= sh/((float)skyadded);
		
		shadfac[0]*= div;	// average color times distances/hits formula
		shadfac[1]*= div;	// average color times distances/hits formula
		shadfac[2]*= div;	// average color times distances/hits formula
	}
	else {
		shadfac[0]= shadfac[1]= shadfac[2]= sh;
	}
}



/* extern call from shade_lamp_loop */
void ray_shadow(ShadeInput *shi, LampRen *lar, float *shadfac)
{
	Isect isec;
	float lampco[3], maxsize;

	/* setup isec */
	if(shi->mat->mode & MA_SHADOW_TRA) isec.mode= RE_RAY_SHADOW_TRA;
	else isec.mode= RE_RAY_SHADOW;
	
	if(lar->mode & LA_LAYER) isec.lay= lar->lay; else isec.lay= -1;

	/* only when not mir tracing, first hit optimm */
	if(shi->depth==0) 
		isec.face_last= (RayFace*)lar->vlr_last[shi->thread];
	else 
		isec.face_last= NULL;
	
	
	if(lar->type==LA_SUN || lar->type==LA_HEMI) {
		maxsize= RE_ray_tree_max_size(R.raytree);
		lampco[0]= shi->co[0] - maxsize*lar->vec[0];
		lampco[1]= shi->co[1] - maxsize*lar->vec[1];
		lampco[2]= shi->co[2] - maxsize*lar->vec[2];
	}
	else {
		VECCOPY(lampco, lar->co);
	}
	
	if(lar->ray_totsamp<2) {
		
		isec.faceorig= (RayFace*)shi->vlr;
		shadfac[3]= 1.0f; // 1.0=full light
		
		/* set up isec vec */
		VECCOPY(isec.start, shi->co);
		VECCOPY(isec.end, lampco);

		if(isec.mode==RE_RAY_SHADOW_TRA) {
			/* isec.col is like shadfac, so defines amount of light (0.0 is full shadow) */
			isec.col[0]= isec.col[1]= isec.col[2]=  1.0f;
			isec.col[3]= 1.0f;

			ray_trace_shadow_tra(&isec, DEPTH_SHADOW_TRA, 0);
			QUATCOPY(shadfac, isec.col);
			//printf("shadfac %f %f %f %f\n", shadfac[0], shadfac[1], shadfac[2], shadfac[3]);
		}
		else if( RE_ray_tree_intersect(R.raytree, &isec)) shadfac[3]= 0.0f;
	}
	else {
		/* area soft shadow */
		float *jitlamp;
		float fac=0.0f, div=0.0f, vec[3];
		int a, j= -1, mask;
		
		if(isec.mode==RE_RAY_SHADOW_TRA) {
			shadfac[0]= shadfac[1]= shadfac[2]= shadfac[3]= 0.0f;
		}
		else shadfac[3]= 1.0f;							// 1.0=full light
		
		fac= 0.0f;
		jitlamp= give_jitter_plane(lar, shi->thread, shi->xs, shi->ys);

		a= lar->ray_totsamp;
		
		/* this correction to make sure we always take at least 1 sample */
		mask= shi->mask;
		if(a==4) mask |= (mask>>4)|(mask>>8);
		else if(a==9) mask |= (mask>>9);
		
		while(a--) {
			
			if(R.r.mode & R_OSA) {
				j++;
				if(j>=R.osa) j= 0;
				if(!(mask & (1<<j))) {
					jitlamp+= 2;
					continue;
				}
			}
			
			isec.faceorig= (RayFace*)shi->vlr; // ray_trace_shadow_tra changes it
			
			vec[0]= jitlamp[0];
			vec[1]= jitlamp[1];
			vec[2]= 0.0f;
			Mat3MulVecfl(lar->mat, vec);
			
			/* set start and end, RE_ray_tree_intersect clips it */
			VECCOPY(isec.start, shi->co);
			isec.end[0]= lampco[0]+vec[0];
			isec.end[1]= lampco[1]+vec[1];
			isec.end[2]= lampco[2]+vec[2];
			
			if(isec.mode==RE_RAY_SHADOW_TRA) {
				/* isec.col is like shadfac, so defines amount of light (0.0 is full shadow) */
				isec.col[0]= isec.col[1]= isec.col[2]=  1.0f;
				isec.col[3]= 1.0f;
				
				ray_trace_shadow_tra(&isec, DEPTH_SHADOW_TRA, 0);
				shadfac[0] += isec.col[0];
				shadfac[1] += isec.col[1];
				shadfac[2] += isec.col[2];
				shadfac[3] += isec.col[3];
			}
			else if( RE_ray_tree_intersect(R.raytree, &isec) ) fac+= 1.0f;
			
			div+= 1.0f;
			jitlamp+= 2;
		}
		
		if(isec.mode==RE_RAY_SHADOW_TRA) {
			shadfac[0] /= div;
			shadfac[1] /= div;
			shadfac[2] /= div;
			shadfac[3] /= div;
		}
		else {
			// sqrt makes nice umbra effect
			if(lar->ray_samp_type & LA_SAMP_UMBRA)
				shadfac[3]= sqrt(1.0f-fac/div);
			else
				shadfac[3]= 1.0f-fac/div;
		}
	}

	/* for first hit optim, set last interesected shadow face */
	if(shi->depth==0) 
		lar->vlr_last[shi->thread]= (VlakRen*)isec.face_last;

}

/* only when face points away from lamp, in direction of lamp, trace ray and find first exit point */
void ray_translucent(ShadeInput *shi, LampRen *lar, float *distfac, float *co)
{
	Isect isec;
	float lampco[3], maxsize;
	
	/* setup isec */
	isec.mode= RE_RAY_SHADOW_TRA;
	
	if(lar->mode & LA_LAYER) isec.lay= lar->lay; else isec.lay= -1;
	
	if(lar->type==LA_SUN || lar->type==LA_HEMI) {
		maxsize= RE_ray_tree_max_size(R.raytree);
		lampco[0]= shi->co[0] - maxsize*lar->vec[0];
		lampco[1]= shi->co[1] - maxsize*lar->vec[1];
		lampco[2]= shi->co[2] - maxsize*lar->vec[2];
	}
	else {
		VECCOPY(lampco, lar->co);
	}
	
	isec.faceorig= (RayFace*)shi->vlr;
	
	/* set up isec vec */
	VECCOPY(isec.start, shi->co);
	VECCOPY(isec.end, lampco);
	
	if(RE_ray_tree_intersect(R.raytree, &isec)) {
		/* we got a face */
		
		/* render co */
		co[0]= isec.start[0]+isec.labda*(isec.vec[0]);
		co[1]= isec.start[1]+isec.labda*(isec.vec[1]);
		co[2]= isec.start[2]+isec.labda*(isec.vec[2]);
		
		*distfac= VecLength(isec.vec);
	}
	else
		*distfac= 0.0f;
}


