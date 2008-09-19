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
#include "BLI_blenlib.h"
#include "BLI_jitter.h"
#include "BLI_rand.h"

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

static int vlr_check_intersect(Isect *is, int ob, RayFace *face)
{
	ObjectInstanceRen *obi= RAY_OBJECT_GET((Render*)is->userdata, ob);
	VlakRen *vlr = (VlakRen*)face;

	/* for baking selected to active non-traceable materials might still
	 * be in the raytree */
	if(!(vlr->mat->mode & MA_TRACEBLE))
		return 0;

	/* I know... cpu cycle waste, might do smarter once */
	if(is->mode==RE_RAY_MIRROR)
		return !(vlr->mat->mode & MA_ONLYCAST);
	else
		return (is->lay & obi->lay);
}

static float *vlr_get_transform(void *userdata, int i)
{
	ObjectInstanceRen *obi= RAY_OBJECT_GET((Render*)userdata, i);

	return (obi->flag & R_TRANSFORMED)? (float*)obi->mat: NULL;
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
	ObjectInstanceRen *obi;
	ObjectRen *obr;
	VlakRen *vlr= NULL;
	float min[3], max[3], co1[3], co2[3], co3[3], co4[3];
	double lasttime= PIL_check_seconds_timer();
	int v, totv = 0, totface = 0;

	INIT_MINMAX(min, max);

	/* first min max raytree space */
	for(obi=re->instancetable.first; obi; obi=obi->next) {
		obr= obi->obr;

		if(re->excludeob && obr->ob == re->excludeob)
			continue;

		for(v=0;v<obr->totvlak;v++) {
			if((v & 255)==0) vlr= obr->vlaknodes[v>>8].vlak;
			else vlr++;
			/* baking selected to active needs non-traceable too */
			if((re->flag & R_BAKE_TRACE) || (vlr->mat->mode & MA_TRACEBLE)) {	
				if((vlr->mat->mode & MA_WIRE)==0) {	
					VECCOPY(co1, vlr->v1->co);
					VECCOPY(co2, vlr->v2->co);
					VECCOPY(co3, vlr->v3->co);

					if(obi->flag & R_TRANSFORMED) {
						Mat4MulVecfl(obi->mat, co1);
						Mat4MulVecfl(obi->mat, co2);
						Mat4MulVecfl(obi->mat, co3);
					}

					DO_MINMAX(co1, min, max);
					DO_MINMAX(co2, min, max);
					DO_MINMAX(co3, min, max);

					if(vlr->v4) {
						VECCOPY(co4, vlr->v4->co);
						if(obi->flag & R_TRANSFORMED)
							Mat4MulVecfl(obi->mat, co4);
						DO_MINMAX(co4, min, max);
					}

					totface++;
				}
			}
		}
	}

	re->raytree= RE_ray_tree_create(re->r.ocres, totface, min, max,
		vlr_face_coords, vlr_check_intersect, vlr_get_transform, re);

	if(min[0] > max[0]) { /* empty raytree */
		RE_ray_tree_done(re->raytree);
		return;	
	}

	for(obi=re->instancetable.first; obi; obi=obi->next) {
		obr= obi->obr;

		if(re->excludeob && obr->ob == re->excludeob)
			continue;

		for(v=0; v<obr->totvlak; v++, totv++) {
			if((v & 255)==0) {
				double time= PIL_check_seconds_timer();

				vlr= obr->vlaknodes[v>>8].vlak;
				if(re->test_break())
					break;
				if(time-lasttime>1.0f) {
					char str[32];
					sprintf(str, "Filling Octree: %d", totv);
					re->i.infostr= str;
					re->stats_draw(&re->i);
					re->i.infostr= NULL;
					lasttime= time;
				}
			}
			else vlr++;
			
			if((re->flag & R_BAKE_TRACE) || (vlr->mat->mode & MA_TRACEBLE))
				if((vlr->mat->mode & MA_WIRE)==0)
					RE_ray_tree_add_face(re->raytree, RAY_OBJECT_SET(re, obi), vlr);
		}
	}

	RE_ray_tree_done(re->raytree);
	
	re->i.infostr= NULL;
	re->stats_draw(&re->i);
}

static void shade_ray(Isect *is, ShadeInput *shi, ShadeResult *shr)
{
	VlakRen *vlr= (VlakRen*)is->face;
	ObjectInstanceRen *obi= RAY_OBJECT_GET(&R, is->ob);
	int osatex= 0;
	
	/* set up view vector */
	VECCOPY(shi->view, is->vec);

	/* render co */
	shi->co[0]= is->start[0]+is->labda*(shi->view[0]);
	shi->co[1]= is->start[1]+is->labda*(shi->view[1]);
	shi->co[2]= is->start[2]+is->labda*(shi->view[2]);
	
	Normalize(shi->view);

	shi->obi= obi;
	shi->obr= obi->obr;
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

	if(vlr->v4) {
		if(is->isect==2) 
			shade_input_set_triangle_i(shi, obi, vlr, 2, 1, 3);
		else
			shade_input_set_triangle_i(shi, obi, vlr, 0, 1, 3);
	}
	else {
		shade_input_set_triangle_i(shi, obi, vlr, 0, 1, 2);
	}

	shi->u= is->u;
	shi->v= is->v;
	shi->dx_u= shi->dx_v= shi->dy_u= shi->dy_v=  0.0f;

	shade_input_set_normals(shi);

	/* point normals to viewing direction */
	if(INPR(shi->facenor, shi->view) < 0.0f)
		shade_input_flip_normals(shi);

	shade_input_set_shade_texco(shi);
	
	if(is->mode==RE_RAY_SHADOW_TRA) 
		if(shi->mat->nodetree && shi->mat->use_nodes) {
			ntreeShaderExecTree(shi->mat->nodetree, shi, shr);
			shi->mat= vlr->mat;		/* shi->mat is being set in nodetree */
		}
		else
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

static void ray_fadeout_endcolor(float *col, ShadeInput *origshi, ShadeInput *shi, ShadeResult *shr, Isect *isec, float *vec)
{
	/* un-intersected rays get either rendered material color or sky color */
	if (origshi->mat->fadeto_mir == MA_RAYMIR_FADETOMAT) {
		VECCOPY(col, shr->combined);
	} else if (origshi->mat->fadeto_mir == MA_RAYMIR_FADETOSKY) {
		VECCOPY(shi->view, vec);
		Normalize(shi->view);
		
		shadeSkyView(col, isec->start, shi->view, NULL);
	}
}

static void ray_fadeout(Isect *is, ShadeInput *shi, float *col, float *blendcol, float dist_mir)
{
	/* if fading out, linear blend against fade color */
	float blendfac;

	blendfac = 1.0 - VecLenf(shi->co, is->start)/dist_mir;
	
	col[0] = col[0]*blendfac + (1.0 - blendfac)*blendcol[0];
	col[1] = col[1]*blendfac + (1.0 - blendfac)*blendcol[1];
	col[2] = col[2]*blendfac + (1.0 - blendfac)*blendcol[2];
}

/* the main recursive tracer itself */
static void traceray(ShadeInput *origshi, ShadeResult *origshr, short depth, float *start, float *vec, float *col, ObjectInstanceRen *obi, VlakRen *vlr, int traflag)
{
	ShadeInput shi;
	ShadeResult shr;
	Isect isec;
	float f, f1, fr, fg, fb;
	float ref[3], maxsize=RE_ray_tree_max_size(R.raytree);
	float dist_mir = origshi->mat->dist_mir;

	/* Warning, This is not that nice, and possibly a bit slow for every ray,
	however some variables were not initialized properly in, unless using shade_input_initialize(...), we need to do a memset */
	memset(&shi, 0, sizeof(ShadeInput)); 
	/* end warning! - Campbell */
	
	VECCOPY(isec.start, start);
	if (dist_mir > 0.0) {
		isec.end[0]= start[0]+dist_mir*vec[0];
		isec.end[1]= start[1]+dist_mir*vec[1];
		isec.end[2]= start[2]+dist_mir*vec[2];
	} else {
		isec.end[0]= start[0]+maxsize*vec[0];
		isec.end[1]= start[1]+maxsize*vec[1];
		isec.end[2]= start[2]+maxsize*vec[2];
	}
	isec.mode= RE_RAY_MIRROR;
	isec.faceorig= (RayFace*)vlr;
	isec.oborig= RAY_OBJECT_SET(&R, obi);

	if(RE_ray_tree_intersect(R.raytree, &isec)) {
		float d= 1.0f;
		
		shi.mask= origshi->mask;
		shi.osatex= origshi->osatex;
		shi.depth= 1;					/* only used to indicate tracing */
		shi.thread= origshi->thread;
		//shi.sample= 0; // memset above, so dont need this
		shi.xs= origshi->xs;
		shi.ys= origshi->ys;
		shi.lay= origshi->lay;
		shi.passflag= SCE_PASS_COMBINED; /* result of tracing needs no pass info */
		shi.combinedflag= 0xFFFFFF;		 /* ray trace does all options */
		//shi.do_preview= 0; // memset above, so dont need this
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
					traceray(origshi, origshr, depth-1, shi.co, refract, tracol, shi.obi, shi.vlr, traflag ^ RAY_TRAFLIP);
				}
				else
					traceray(origshi, origshr, depth-1, shi.co, shi.view, tracol, shi.obi, shi.vlr, 0);
				
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
				traceray(origshi, origshr, depth-1, shi.co, ref, mircol, shi.obi, shi.vlr, 0);
			
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
			
			if (dist_mir > 0.0) {
				float blendcol[3];
				
				/* max ray distance set, but found an intersection, so fade this color
				 * out towards the sky/material color for a smooth transition */
				ray_fadeout_endcolor(blendcol, origshi, &shi, origshr, &isec, vec);
				ray_fadeout(&isec, &shi, col, blendcol, dist_mir);
			}
		}
		else {
			col[0]= shr.diff[0] + shr.spec[0];
			col[1]= shr.diff[1] + shr.spec[1];
			col[2]= shr.diff[2] + shr.spec[2];
		}
		
	}
	else {
		ray_fadeout_endcolor(col, origshi, &shi, origshr, &isec, vec);
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
	
	/* test if already initialized */
	if(lar->jitter) return;
	
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


/* **************** QMC sampling *************** */

static void halton_sample(double *ht_invprimes, double *ht_nums, double *v)
{
	// incremental halton sequence generator, from:
	// "Instant Radiosity", Keller A.
	unsigned int i;
	
	for (i = 0; i < 2; i++)
	{
		double r = fabs((1.0 - ht_nums[i]) - 1e-10);
		
		if (ht_invprimes[i] >= r)
		{
			double lasth;
			double h = ht_invprimes[i];
			
			do {
				lasth = h;
				h *= ht_invprimes[i];
			} while (h >= r);
			
			ht_nums[i] += ((lasth + h) - 1.0);
		}
		else
			ht_nums[i] += ht_invprimes[i];
		
		v[i] = (float)ht_nums[i];
	}
}

/* Generate Hammersley points in [0,1)^2
 * From Lucille renderer */
static void hammersley_create(double *out, int n)
{
	double p, t;
	int k, kk;

	for (k = 0; k < n; k++) {
		t = 0;
		for (p = 0.5, kk = k; kk; p *= 0.5, kk >>= 1) {
			if (kk & 1) {		/* kk mod 2 = 1		*/
				t += p;
			}
		}
	
		out[2 * k + 0] = (double)k / (double)n;
		out[2 * k + 1] = t;
	}
}

struct QMCSampler *QMC_initSampler(int type, int tot)
{	
	QMCSampler *qsa = MEM_callocN(sizeof(QMCSampler), "qmc sampler");
	qsa->samp2d = MEM_callocN(2*sizeof(double)*tot, "qmc sample table");

	qsa->tot = tot;
	qsa->type = type;
	
	if (qsa->type==SAMP_TYPE_HAMMERSLEY) 
		hammersley_create(qsa->samp2d, qsa->tot);
		
	return qsa;
}

static void QMC_initPixel(QMCSampler *qsa, int thread)
{
	if (qsa->type==SAMP_TYPE_HAMMERSLEY)
	{
		/* hammersley sequence is fixed, already created in QMCSampler init.
		 * per pixel, gets a random offset. We create separate offsets per thread, for write-safety */
		qsa->offs[thread][0] = 0.5 * BLI_thread_frand(thread);
		qsa->offs[thread][1] = 0.5 * BLI_thread_frand(thread);
	}
	else { 	/* SAMP_TYPE_HALTON */
		
		/* generate a new randomised halton sequence per pixel
		 * to alleviate qmc artifacts and make it reproducable 
		 * between threads/frames */
		double ht_invprimes[2], ht_nums[2];
		double r[2];
		int i;
	
		ht_nums[0] = BLI_thread_frand(thread);
		ht_nums[1] = BLI_thread_frand(thread);
		ht_invprimes[0] = 0.5;
		ht_invprimes[1] = 1.0/3.0;
		
		for (i=0; i< qsa->tot; i++) {
			halton_sample(ht_invprimes, ht_nums, r);
			qsa->samp2d[2*i+0] = r[0];
			qsa->samp2d[2*i+1] = r[1];
		}
	}
}

static void QMC_freeSampler(QMCSampler *qsa)
{
	MEM_freeN(qsa->samp2d);
	MEM_freeN(qsa);
}

static void QMC_getSample(double *s, QMCSampler *qsa, int thread, int num)
{
	if (qsa->type == SAMP_TYPE_HAMMERSLEY) {
		s[0] = fmod(qsa->samp2d[2*num+0] + qsa->offs[thread][0], 1.0f);
		s[1] = fmod(qsa->samp2d[2*num+1] + qsa->offs[thread][1], 1.0f);
	}
	else { /* SAMP_TYPE_HALTON */
		s[0] = qsa->samp2d[2*num+0];
		s[1] = qsa->samp2d[2*num+1];
	}
}

/* phong weighted disc using 'blur' for exponent, centred on 0,0 */
static void QMC_samplePhong(float *vec, QMCSampler *qsa, int thread, int num, float blur)
{
	double s[2];
	float phi, pz, sqr;
	
	QMC_getSample(s, qsa, thread, num);

	phi = s[0]*2*M_PI;
	pz = pow(s[1], blur);
	sqr = sqrt(1.0f-pz*pz);

	vec[0] = cos(phi)*sqr;
	vec[1] = sin(phi)*sqr;
	vec[2] = 0.0f;
}

/* rect of edge lengths sizex, sizey, centred on 0.0,0.0 i.e. ranging from -sizex/2 to +sizey/2 */
static void QMC_sampleRect(float *vec, QMCSampler *qsa, int thread, int num, float sizex, float sizey)
{
	double s[2];

	QMC_getSample(s, qsa, thread, num);
		
	vec[0] = (s[0] - 0.5) * sizex;
	vec[1] = (s[1] - 0.5) * sizey;
	vec[2] = 0.0f;
}

/* disc of radius 'radius', centred on 0,0 */
static void QMC_sampleDisc(float *vec, QMCSampler *qsa, int thread, int num, float radius)
{
	double s[2];
	float phi, sqr;
	
	QMC_getSample(s, qsa, thread, num);
	
	phi = s[0]*2*M_PI;
	sqr = sqrt(s[1]);

	vec[0] = cos(phi)*sqr* radius/2.0;
	vec[1] = sin(phi)*sqr* radius/2.0;
	vec[2] = 0.0f;
}

/* uniform hemisphere sampling */
static void QMC_sampleHemi(float *vec, QMCSampler *qsa, int thread, int num)
{
	double s[2];
	float phi, sqr;
	
	QMC_getSample(s, qsa, thread, num);
	
	phi = s[0]*2.f*M_PI;	
	sqr = sqrt(s[1]);

	vec[0] = cos(phi)*sqr;
	vec[1] = sin(phi)*sqr;
	vec[2] = 1.f - s[1]*s[1];
}

#if 0 /* currently not used */
/* cosine weighted hemisphere sampling */
static void QMC_sampleHemiCosine(float *vec, QMCSampler *qsa, int thread, int num)
{
	double s[2];
	float phi, sqr;
	
	QMC_getSample(s, qsa, thread, num);
	
	phi = s[0]*2.f*M_PI;	
	sqr = s[1]*sqrt(2-s[1]*s[1]);

	vec[0] = cos(phi)*sqr;
	vec[1] = sin(phi)*sqr;
	vec[2] = 1.f - s[1]*s[1];

}
#endif

/* called from convertBlenderScene.c */
void init_render_qmcsampler(Render *re)
{
	re->qmcsamplers= MEM_callocN(sizeof(ListBase)*BLENDER_MAX_THREADS, "QMCListBase");
}

QMCSampler *get_thread_qmcsampler(Render *re, int thread, int type, int tot)
{
	QMCSampler *qsa;

	/* create qmc samplers as needed, since recursion makes it hard to
	 * predict how many are needed */

	for(qsa=re->qmcsamplers[thread].first; qsa; qsa=qsa->next) {
		if(qsa->type == type && qsa->tot == tot && !qsa->used) {
			qsa->used= 1;
			return qsa;
		}
	}

	qsa= QMC_initSampler(type, tot);
	qsa->used= 1;
	BLI_addtail(&re->qmcsamplers[thread], qsa);

	return qsa;
}

void release_thread_qmcsampler(Render *re, int thread, QMCSampler *qsa)
{
	qsa->used= 0;
}

void free_render_qmcsampler(Render *re)
{
	QMCSampler *qsa, *next;
	int a;

	if(re->qmcsamplers) {
		for(a=0; a<BLENDER_MAX_THREADS; a++) {
			for(qsa=re->qmcsamplers[a].first; qsa; qsa=next) {
				next= qsa->next;
				QMC_freeSampler(qsa);
			}

			re->qmcsamplers[a].first= re->qmcsamplers[a].last= NULL;
		}

		MEM_freeN(re->qmcsamplers);
		re->qmcsamplers= NULL;
	}
}

static int adaptive_sample_variance(int samples, float *col, float *colsq, float thresh)
{
	float var[3], mean[3];

	/* scale threshold just to give a bit more precision in input rather than dealing with 
	 * tiny tiny numbers in the UI */
	thresh /= 2;
	
	mean[0] = col[0] / (float)samples;
	mean[1] = col[1] / (float)samples;
	mean[2] = col[2] / (float)samples;

	var[0] = (colsq[0] / (float)samples) - (mean[0]*mean[0]);
	var[1] = (colsq[1] / (float)samples) - (mean[1]*mean[1]);
	var[2] = (colsq[2] / (float)samples) - (mean[2]*mean[2]);
	
	if ((var[0] * 0.4 < thresh) && (var[1] * 0.3 < thresh) && (var[2] * 0.6 < thresh))
		return 1;
	else
		return 0;
}

static int adaptive_sample_contrast_val(int samples, float prev, float val, float thresh)
{
	/* if the last sample's contribution to the total value was below a small threshold
	 * (i.e. the samples taken are very similar), then taking more samples that are probably 
	 * going to be the same is wasting effort */
	if (fabs( prev/(float)(samples-1) - val/(float)samples ) < thresh) {
		return 1;
	} else
		return 0;
}

static float get_avg_speed(ShadeInput *shi)
{
	float pre_x, pre_y, post_x, post_y, speedavg;
	
	pre_x = (shi->winspeed[0] == PASS_VECTOR_MAX)?0.0:shi->winspeed[0];
	pre_y = (shi->winspeed[1] == PASS_VECTOR_MAX)?0.0:shi->winspeed[1];
	post_x = (shi->winspeed[2] == PASS_VECTOR_MAX)?0.0:shi->winspeed[2];
	post_y = (shi->winspeed[3] == PASS_VECTOR_MAX)?0.0:shi->winspeed[3];
	
	speedavg = (sqrt(pre_x*pre_x + pre_y*pre_y) + sqrt(post_x*post_x + post_y*post_y)) / 2.0;
	
	return speedavg;
}

/* ***************** main calls ************** */


static void trace_refract(float *col, ShadeInput *shi, ShadeResult *shr)
{
	QMCSampler *qsa=NULL;
	int samp_type;
	
	float samp3d[3], orthx[3], orthy[3];
	float v_refract[3], v_refract_new[3];
	float sampcol[4], colsq[4];
	
	float blur = pow(1.0 - shi->mat->gloss_tra, 3);
	short max_samples = shi->mat->samp_gloss_tra;
	float adapt_thresh = shi->mat->adapt_thresh_tra;
	
	int samples=0;
	
	colsq[0] = colsq[1] = colsq[2] = 0.0;
	col[0] = col[1] = col[2] = 0.0;
	col[3]= shr->alpha;
	
	if (blur > 0.0) {
		if (adapt_thresh != 0.0) samp_type = SAMP_TYPE_HALTON;
		else samp_type = SAMP_TYPE_HAMMERSLEY;
			
		/* all samples are generated per pixel */
		qsa = get_thread_qmcsampler(&R, shi->thread, samp_type, max_samples);
		QMC_initPixel(qsa, shi->thread);
	} else 
		max_samples = 1;
	

	while (samples < max_samples) {		
		refraction(v_refract, shi->vn, shi->view, shi->ang);
		
		if (max_samples > 1) {
			/* get a quasi-random vector from a phong-weighted disc */
			QMC_samplePhong(samp3d, qsa, shi->thread, samples, blur);
						
			VecOrthoBasisf(v_refract, orthx, orthy);
			VecMulf(orthx, samp3d[0]);
			VecMulf(orthy, samp3d[1]);
				
			/* and perturb the refraction vector in it */
			VecAddf(v_refract_new, v_refract, orthx);
			VecAddf(v_refract_new, v_refract_new, orthy);
			
			Normalize(v_refract_new);
		} else {
			/* no blurriness, use the original normal */
			VECCOPY(v_refract_new, v_refract);
		}
	
		traceray(shi, shr, shi->mat->ray_depth_tra, shi->co, v_refract_new, sampcol, shi->obi, shi->vlr, RAY_TRA|RAY_TRAFLIP);
	
		col[0] += sampcol[0];
		col[1] += sampcol[1];
		col[2] += sampcol[2];
		col[3] += sampcol[3];
		
		/* for variance calc */
		colsq[0] += sampcol[0]*sampcol[0];
		colsq[1] += sampcol[1]*sampcol[1];
		colsq[2] += sampcol[2]*sampcol[2];
		
		samples++;
		
		/* adaptive sampling */
		if (adapt_thresh < 1.0 && samples > max_samples/2) 
		{
			if (adaptive_sample_variance(samples, col, colsq, adapt_thresh))
				break;
			
			/* if the pixel so far is very dark, we can get away with less samples */
			if ( (col[0] + col[1] + col[2])/3.0/(float)samples < 0.01 )
				max_samples--;
		}
	}
	
	col[0] /= (float)samples;
	col[1] /= (float)samples;
	col[2] /= (float)samples;
	col[3] /= (float)samples;
	
	if (qsa)
		release_thread_qmcsampler(&R, shi->thread, qsa);
}

static void trace_reflect(float *col, ShadeInput *shi, ShadeResult *shr, float fresnelfac)
{
	QMCSampler *qsa=NULL;
	int samp_type;
	
	float samp3d[3], orthx[3], orthy[3];
	float v_nor_new[3], v_reflect[3];
	float sampcol[4], colsq[4];
		
	float blur = pow(1.0 - shi->mat->gloss_mir, 3);
	short max_samples = shi->mat->samp_gloss_mir;
	float adapt_thresh = shi->mat->adapt_thresh_mir;
	float aniso = 1.0 - shi->mat->aniso_gloss_mir;
	
	int samples=0;
	
	col[0] = col[1] = col[2] = 0.0;
	colsq[0] = colsq[1] = colsq[2] = 0.0;
	
	if (blur > 0.0) {
		if (adapt_thresh != 0.0) samp_type = SAMP_TYPE_HALTON;
		else samp_type = SAMP_TYPE_HAMMERSLEY;
			
		/* all samples are generated per pixel */
		qsa = get_thread_qmcsampler(&R, shi->thread, samp_type, max_samples);
		QMC_initPixel(qsa, shi->thread);
	} else 
		max_samples = 1;
	
	while (samples < max_samples) {
				
		if (max_samples > 1) {
			/* get a quasi-random vector from a phong-weighted disc */
			QMC_samplePhong(samp3d, qsa, shi->thread, samples, blur);

			/* find the normal's perpendicular plane, blurring along tangents
			 * if tangent shading enabled */
			if (shi->mat->mode & (MA_TANGENT_V)) {
				Crossf(orthx, shi->vn, shi->tang);      // bitangent
				VECCOPY(orthy, shi->tang);
				VecMulf(orthx, samp3d[0]);
				VecMulf(orthy, samp3d[1]*aniso);
			} else {
				VecOrthoBasisf(shi->vn, orthx, orthy);
				VecMulf(orthx, samp3d[0]);
				VecMulf(orthy, samp3d[1]);
			}

			/* and perturb the normal in it */
			VecAddf(v_nor_new, shi->vn, orthx);
			VecAddf(v_nor_new, v_nor_new, orthy);
			Normalize(v_nor_new);
		} else {
			/* no blurriness, use the original normal */
			VECCOPY(v_nor_new, shi->vn);
		}
		
		if((shi->vlr->flag & R_SMOOTH)) 
			reflection(v_reflect, v_nor_new, shi->view, shi->facenor);
		else
			reflection(v_reflect, v_nor_new, shi->view, NULL);
		
		traceray(shi, shr, shi->mat->ray_depth, shi->co, v_reflect, sampcol, shi->obi, shi->vlr, 0);

		
		col[0] += sampcol[0];
		col[1] += sampcol[1];
		col[2] += sampcol[2];
	
		/* for variance calc */
		colsq[0] += sampcol[0]*sampcol[0];
		colsq[1] += sampcol[1]*sampcol[1];
		colsq[2] += sampcol[2]*sampcol[2];
		
		samples++;

		/* adaptive sampling */
		if (adapt_thresh > 0.0 && samples > max_samples/3) 
		{
			if (adaptive_sample_variance(samples, col, colsq, adapt_thresh))
				break;
			
			/* if the pixel so far is very dark, we can get away with less samples */
			if ( (col[0] + col[1] + col[2])/3.0/(float)samples < 0.01 )
				max_samples--;
		
			/* reduce samples when reflection is dim due to low ray mirror blend value or fresnel factor
			 * and when reflection is blurry */
			if (fresnelfac < 0.1 * (blur+1)) {
				max_samples--;
				
				/* even more for very dim */
				if (fresnelfac < 0.05 * (blur+1)) 
					max_samples--;
			}
		}
	}
	
	col[0] /= (float)samples;
	col[1] /= (float)samples;
	col[2] /= (float)samples;
	
	if (qsa)
		release_thread_qmcsampler(&R, shi->thread, qsa);
}

/* extern call from render loop */
void ray_trace(ShadeInput *shi, ShadeResult *shr)
{
	VlakRen *vlr;
	float i, f, f1, fr, fg, fb;
	float mircol[4], tracol[4];
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
		float olddiff[3];
		
		trace_refract(tracol, shi, shr);
		
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
		
		if(!(shi->combinedflag & SCE_PASS_REFRACT))
			VECSUB(diff, diff, shr->refr);
		
		shr->alpha= tracol[3];
	}
	
	if(do_mir) {
	
		i= shi->ray_mirror*fresnel_fac(shi->view, shi->vn, shi->mat->fresnel_mir_i, shi->mat->fresnel_mir);
		if(i!=0.0f) {
		
			trace_reflect(mircol, shi, shr, i);
			
			fr= i*shi->mirr;
			fg= i*shi->mirg;
			fb= i*shi->mirb;

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
		
		/* Warning, This is not that nice, and possibly a bit slow for every ray,
		however some variables were not initialized properly in, unless using shade_input_initialize(...), we need to do a memset */
		memset(&shi, 0, sizeof(ShadeInput)); 
		/* end warning! - Campbell */
		
		shi.depth= 1;					/* only used to indicate tracing */
		shi.mask= 1;
		
		/*shi.osatex= 0;
		shi.thread= shi.sample= 0;
		shi.lay= 0;
		shi.passflag= 0;
		shi.combinedflag= 0;
		shi.do_preview= 0;
		shi.light_override= NULL;
		shi.mat_override= NULL;*/
		
		shade_ray(is, &shi, &shr);
		if (traflag & RAY_TRA)
			d= shade_by_transmission(is, &shi, &shr);
		
		/* mix colors based on shadfac (rgb + amount of light factor) */
		addAlphaLight(is->col, shr.diff, shr.alpha, d*shi.mat->filter);
		
		if(depth>0 && is->col[3]>0.0f) {
			
			/* adapt isect struct */
			VECCOPY(is->start, shi.co);
			is->oborig= RAY_OBJECT_SET(&R, shi.obi);
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
	isec.oborig= RAY_OBJECT_SET(&R, ship->obi);
	
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
			
			/* Warning, This is not that nice, and possibly a bit slow for every ray,
			however some variables were not initialized properly in, unless using shade_input_initialize(...), we need to do a memset */
			memset(&shi, 0, sizeof(ShadeInput)); 
			/* end warning! - Campbell */
			
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
	
	tot= 2*resol*resol;

	if (type & WO_AORNDSMP) {
		float *sphere;
		int a;
		
		// always returns table
		sphere= threadsafe_table_sphere(0, thread, xs, ys, tot);

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
		
		// returns table if xs and ys were equal to last call
		sphere= threadsafe_table_sphere(1, thread, xs, ys, tot);
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

void ray_ao_qmc(ShadeInput *shi, float *shadfac)
{
	Isect isec;
	QMCSampler *qsa=NULL;
	float samp3d[3];
	float up[3], side[3], dir[3], nrm[3];
	
	float maxdist = R.wrld.aodist;
	float fac=0.0f, prev=0.0f;
	float adapt_thresh = G.scene->world->ao_adapt_thresh;
	float adapt_speed_fac = G.scene->world->ao_adapt_speed_fac;
	float bias = G.scene->world->aobias;
	
	int samples=0;
	int max_samples = R.wrld.aosamp*R.wrld.aosamp;
	
	float dxyview[3], skyadded=0, div;
	int aocolor;
	
	isec.faceorig= (RayFace*)shi->vlr;
	isec.oborig= RAY_OBJECT_SET(&R, shi->obi);
	isec.face_last= NULL;
	isec.ob_last= 0;
	isec.mode= (R.wrld.aomode & WO_AODIST)?RE_RAY_SHADOW_TRA:RE_RAY_SHADOW;
	isec.lay= -1;
	
	shadfac[0]= shadfac[1]= shadfac[2]= 0.0f;
	
	/* prevent sky colors to be added for only shadow (shadow becomes alpha) */
	aocolor= R.wrld.aocolor;
	if(shi->mat->mode & MA_ONLYSHADOW)
		aocolor= WO_AOPLAIN;
	
	if(aocolor == WO_AOSKYTEX) {
		dxyview[0]= 1.0f/(float)R.wrld.aosamp;
		dxyview[1]= 1.0f/(float)R.wrld.aosamp;
		dxyview[2]= 0.0f;
	}
	
	/* bias prevents smoothed faces to appear flat */
	if(shi->vlr->flag & R_SMOOTH) {
		bias= G.scene->world->aobias;
		VECCOPY(nrm, shi->vn);
	}
	else {
		bias= 0.0f;
		VECCOPY(nrm, shi->facenor);
	}
	
	VecOrthoBasisf(nrm, up, side);
	
	/* sampling init */
	if (R.wrld.ao_samp_method==WO_AOSAMP_HALTON) {
		float speedfac;
		
		speedfac = get_avg_speed(shi) * adapt_speed_fac;
		CLAMP(speedfac, 1.0, 1000.0);
		max_samples /= speedfac;
		if (max_samples < 5) max_samples = 5;
		
		qsa = get_thread_qmcsampler(&R, shi->thread, SAMP_TYPE_HALTON, max_samples);
	} else if (R.wrld.ao_samp_method==WO_AOSAMP_HAMMERSLEY)
		qsa = get_thread_qmcsampler(&R, shi->thread, SAMP_TYPE_HAMMERSLEY, max_samples);

	QMC_initPixel(qsa, shi->thread);
	
	while (samples < max_samples) {

		/* sampling, returns quasi-random vector in unit hemisphere */
		QMC_sampleHemi(samp3d, qsa, shi->thread, samples);

		dir[0] = (samp3d[0]*up[0] + samp3d[1]*side[0] + samp3d[2]*nrm[0]);
		dir[1] = (samp3d[0]*up[1] + samp3d[1]*side[1] + samp3d[2]*nrm[1]);
		dir[2] = (samp3d[0]*up[2] + samp3d[1]*side[2] + samp3d[2]*nrm[2]);
		
		Normalize(dir);
			
		VECCOPY(isec.start, shi->co);
		isec.end[0] = shi->co[0] - maxdist*dir[0];
		isec.end[1] = shi->co[1] - maxdist*dir[1];
		isec.end[2] = shi->co[2] - maxdist*dir[2];
		
		prev = fac;
		
		if(RE_ray_tree_intersect(R.raytree, &isec)) {
			if (R.wrld.aomode & WO_AODIST) fac+= exp(-isec.labda*R.wrld.aodistfac); 
			else fac+= 1.0f;
		}
		else if(aocolor!=WO_AOPLAIN) {
			float skycol[4];
			float skyfac, view[3];
			
			view[0]= -dir[0];
			view[1]= -dir[1];
			view[2]= -dir[2];
			Normalize(view);
			
			if(aocolor==WO_AOSKYCOL) {
				skyfac= 0.5*(1.0f+view[0]*R.grvec[0]+ view[1]*R.grvec[1]+ view[2]*R.grvec[2]);
				shadfac[0]+= (1.0f-skyfac)*R.wrld.horr + skyfac*R.wrld.zenr;
				shadfac[1]+= (1.0f-skyfac)*R.wrld.horg + skyfac*R.wrld.zeng;
				shadfac[2]+= (1.0f-skyfac)*R.wrld.horb + skyfac*R.wrld.zenb;
			}
			else {	/* WO_AOSKYTEX */
				shadeSkyView(skycol, isec.start, view, dxyview);
				shadfac[0]+= skycol[0];
				shadfac[1]+= skycol[1];
				shadfac[2]+= skycol[2];
			}
			skyadded++;
		}
		
		samples++;
		
		if (qsa->type == SAMP_TYPE_HALTON) {
			/* adaptive sampling - consider samples below threshold as in shadow (or vice versa) and exit early */		
			if (adapt_thresh > 0.0 && (samples > max_samples/2) ) {
				
				if (adaptive_sample_contrast_val(samples, prev, fac, adapt_thresh)) {
					break;
				}
			}
		}
	}
	
	if(aocolor!=WO_AOPLAIN && skyadded) {
		div= (1.0f - fac/(float)samples)/((float)skyadded);
		
		shadfac[0]*= div;	// average color times distances/hits formula
		shadfac[1]*= div;	// average color times distances/hits formula
		shadfac[2]*= div;	// average color times distances/hits formula
	} else {
		shadfac[0]= shadfac[1]= shadfac[2]= 1.0f - fac/(float)samples;
	}
	
	if (qsa)
		release_thread_qmcsampler(&R, shi->thread, qsa);
}

/* extern call from shade_lamp_loop, ambient occlusion calculus */
void ray_ao_spheresamp(ShadeInput *shi, float *shadfac)
{
	Isect isec;
	float *vec, *nrm, div, bias, sh=0.0f;
	float maxdist = R.wrld.aodist;
	float dxyview[3];
	int j= -1, tot, actual=0, skyadded=0, aocolor, resol= R.wrld.aosamp;
	
	isec.faceorig= (RayFace*)shi->vlr;
	isec.oborig= RAY_OBJECT_SET(&R, shi->obi);
	isec.face_last= NULL;
	isec.ob_last= 0;
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
	
	if(resol>32) resol= 32;
	
	vec= sphere_sampler(R.wrld.aomode, resol, shi->thread, shi->xs, shi->ys);
	
	// warning: since we use full sphere now, and dotproduct is below, we do twice as much
	tot= 2*resol*resol;

	if(aocolor == WO_AOSKYTEX) {
		dxyview[0]= 1.0f/(float)resol;
		dxyview[1]= 1.0f/(float)resol;
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
			
			/* always set start/end, RE_ray_tree_intersect clips it */
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

void ray_ao(ShadeInput *shi, float *shadfac)
{
	/* Unfortunately, the unusual way that the sphere sampler calculates roughly twice as many
	 * samples as are actually traced, and skips them based on bias and OSA settings makes it very difficult
	 * to reuse code between these two functions. This is the easiest way I can think of to do it
	 * --broken */
	if (ELEM(R.wrld.ao_samp_method, WO_AOSAMP_HAMMERSLEY, WO_AOSAMP_HALTON))
		ray_ao_qmc(shi, shadfac);
	else if (R.wrld.ao_samp_method == WO_AOSAMP_CONSTANT)
		ray_ao_spheresamp(shi, shadfac);
}


static void ray_shadow_qmc(ShadeInput *shi, LampRen *lar, float *lampco, float *shadfac, Isect *isec)
{
	QMCSampler *qsa=NULL;
	QMCSampler *qsa_jit=NULL;
	int samples=0;
	float samp3d[3], jit[3], jitbias= 0.0f;

	float fac=0.0f, vec[3];
	float colsq[4];
	float adapt_thresh = lar->adapt_thresh;
	int max_samples = lar->ray_totsamp;
	float pos[3];
	int do_soft=1, full_osa=0;

	colsq[0] = colsq[1] = colsq[2] = 0.0;
	if(isec->mode==RE_RAY_SHADOW_TRA) {
		shadfac[0]= shadfac[1]= shadfac[2]= shadfac[3]= 0.0f;
	} else
		shadfac[3]= 1.0f;
	
	if (lar->ray_totsamp < 2) do_soft = 0;
	if ((R.r.mode & R_OSA) && (R.osa > 0) && (shi->vlr->flag & R_FULL_OSA)) full_osa = 1;
	
	if (full_osa) {
		if (do_soft) max_samples  = max_samples/R.osa + 1;
		else max_samples = 1;
	} else {
		if (do_soft) max_samples = lar->ray_totsamp;
		else max_samples = (R.osa > 4)?R.osa:5;
	}

	if(shi->vlr && ((shi->vlr->flag & R_FULL_OSA) == 0))
		jitbias= 0.5f*(VecLength(shi->dxco) + VecLength(shi->dyco));

	/* sampling init */
	if (lar->ray_samp_method==LA_SAMP_HALTON) {
		qsa = get_thread_qmcsampler(&R, shi->thread, SAMP_TYPE_HALTON, max_samples);
		qsa_jit = get_thread_qmcsampler(&R, shi->thread, SAMP_TYPE_HALTON, max_samples);
	} else if (lar->ray_samp_method==LA_SAMP_HAMMERSLEY) {
		qsa = get_thread_qmcsampler(&R, shi->thread, SAMP_TYPE_HAMMERSLEY, max_samples);
		qsa_jit = get_thread_qmcsampler(&R, shi->thread, SAMP_TYPE_HAMMERSLEY, max_samples);
	}
	
	QMC_initPixel(qsa, shi->thread);
	QMC_initPixel(qsa_jit, shi->thread);
	
	VECCOPY(vec, lampco);
	
	
	while (samples < max_samples) {
		isec->faceorig= (RayFace*)shi->vlr;
		isec->oborig= RAY_OBJECT_SET(&R, shi->obi);
		
		/* manually jitter the start shading co-ord per sample
		 * based on the pre-generated OSA texture sampling offsets, 
		 * for anti-aliasing sharp shadow edges. */
		VECCOPY(pos, shi->co);
		if (shi->vlr && !full_osa) {
			QMC_sampleRect(jit, qsa_jit, shi->thread, samples, 1.0, 1.0);
			
			pos[0] += shi->dxco[0]*jit[0] + shi->dyco[0]*jit[1];
			pos[1] += shi->dxco[1]*jit[0] + shi->dyco[1]*jit[1];
			pos[2] += shi->dxco[2]*jit[0] + shi->dyco[2]*jit[1];
		}

		if (do_soft) {
			/* sphere shadow source */
			if (lar->type == LA_LOCAL) {
				float ru[3], rv[3], v[3], s[3];
				
				/* calc tangent plane vectors */
				v[0] = pos[0] - lampco[0];
				v[1] = pos[1] - lampco[1];
				v[2] = pos[2] - lampco[2];
				Normalize(v);
				VecOrthoBasisf(v, ru, rv);
				
				/* sampling, returns quasi-random vector in area_size disc */
				QMC_sampleDisc(samp3d, qsa, shi->thread, samples,lar->area_size);

				/* distribute disc samples across the tangent plane */
				s[0] = samp3d[0]*ru[0] + samp3d[1]*rv[0];
				s[1] = samp3d[0]*ru[1] + samp3d[1]*rv[1];
				s[2] = samp3d[0]*ru[2] + samp3d[1]*rv[2];
				
				VECCOPY(samp3d, s);

				if(jitbias != 0.0f) {
					/* bias away somewhat to avoid self intersection */
					pos[0] -= jitbias*v[0];
					pos[1] -= jitbias*v[1];
					pos[2] -= jitbias*v[2];
				}
			}
			else {
				/* sampling, returns quasi-random vector in [sizex,sizey]^2 plane */
				QMC_sampleRect(samp3d, qsa, shi->thread, samples, lar->area_size, lar->area_sizey);
								
				/* align samples to lamp vector */
				Mat3MulVecfl(lar->mat, samp3d);
			}
			isec->end[0]= vec[0]+samp3d[0];
			isec->end[1]= vec[1]+samp3d[1];
			isec->end[2]= vec[2]+samp3d[2];
		} else {
			VECCOPY(isec->end, vec);
		}

		if(jitbias != 0.0f && !(do_soft && lar->type==LA_LOCAL)) {
			/* bias away somewhat to avoid self intersection */
			float v[3];

			VECSUB(v, pos, isec->end);
			Normalize(v);

			pos[0] -= jitbias*v[0];
			pos[1] -= jitbias*v[1];
			pos[2] -= jitbias*v[2];
		}

		VECCOPY(isec->start, pos);
		
		
		/* trace the ray */
		if(isec->mode==RE_RAY_SHADOW_TRA) {
			isec->col[0]= isec->col[1]= isec->col[2]=  1.0f;
			isec->col[3]= 1.0f;
			
			ray_trace_shadow_tra(isec, DEPTH_SHADOW_TRA, 0);
			shadfac[0] += isec->col[0];
			shadfac[1] += isec->col[1];
			shadfac[2] += isec->col[2];
			shadfac[3] += isec->col[3];
			
			/* for variance calc */
			colsq[0] += isec->col[0]*isec->col[0];
			colsq[1] += isec->col[1]*isec->col[1];
			colsq[2] += isec->col[2]*isec->col[2];
		}
		else {
			if( RE_ray_tree_intersect(R.raytree, isec) ) fac+= 1.0f;
		}
		
		samples++;
		
		if ((lar->ray_samp_method == LA_SAMP_HALTON)) {
		
			/* adaptive sampling - consider samples below threshold as in shadow (or vice versa) and exit early */
			if ((max_samples > 4) && (adapt_thresh > 0.0) && (samples > max_samples / 3)) {
				if (isec->mode==RE_RAY_SHADOW_TRA) {
					if ((shadfac[3] / samples > (1.0-adapt_thresh)) || (shadfac[3] / samples < adapt_thresh))
						break;
					else if (adaptive_sample_variance(samples, shadfac, colsq, adapt_thresh))
						break;
				} else {
					if ((fac / samples > (1.0-adapt_thresh)) || (fac / samples < adapt_thresh))
						break;
				}
			}
		}
	}
	
	if(isec->mode==RE_RAY_SHADOW_TRA) {
		shadfac[0] /= samples;
		shadfac[1] /= samples;
		shadfac[2] /= samples;
		shadfac[3] /= samples;
	} else
		shadfac[3]= 1.0f-fac/samples;

	if (qsa_jit)
		release_thread_qmcsampler(&R, shi->thread, qsa_jit);
	if (qsa)
		release_thread_qmcsampler(&R, shi->thread, qsa);
}

static void ray_shadow_jitter(ShadeInput *shi, LampRen *lar, float *lampco, float *shadfac, Isect *isec)
{
	/* area soft shadow */
	float *jitlamp;
	float fac=0.0f, div=0.0f, vec[3];
	int a, j= -1, mask;
	
	if(isec->mode==RE_RAY_SHADOW_TRA) {
		shadfac[0]= shadfac[1]= shadfac[2]= shadfac[3]= 0.0f;
	}
	else shadfac[3]= 1.0f;
	
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
		
		isec->faceorig= (RayFace*)shi->vlr;
		isec->oborig= RAY_OBJECT_SET(&R, shi->obi);
		
		vec[0]= jitlamp[0];
		vec[1]= jitlamp[1];
		vec[2]= 0.0f;
		Mat3MulVecfl(lar->mat, vec);
		
		/* set start and end, RE_ray_tree_intersect clips it */
		VECCOPY(isec->start, shi->co);
		isec->end[0]= lampco[0]+vec[0];
		isec->end[1]= lampco[1]+vec[1];
		isec->end[2]= lampco[2]+vec[2];
		
		if(isec->mode==RE_RAY_SHADOW_TRA) {
			/* isec.col is like shadfac, so defines amount of light (0.0 is full shadow) */
			isec->col[0]= isec->col[1]= isec->col[2]=  1.0f;
			isec->col[3]= 1.0f;
			
			ray_trace_shadow_tra(isec, DEPTH_SHADOW_TRA, 0);
			shadfac[0] += isec->col[0];
			shadfac[1] += isec->col[1];
			shadfac[2] += isec->col[2];
			shadfac[3] += isec->col[3];
		}
		else if( RE_ray_tree_intersect(R.raytree, isec) ) fac+= 1.0f;
		
		div+= 1.0f;
		jitlamp+= 2;
	}
	
	if(isec->mode==RE_RAY_SHADOW_TRA) {
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
	if(shi->depth==0) {
		isec.face_last= (RayFace*)lar->vlr_last[shi->thread];
		isec.ob_last= RAY_OBJECT_SET(&R, lar->obi_last[shi->thread]);
	}
	else {
		isec.face_last= NULL;
		isec.ob_last= 0;
	}
	
	if(lar->type==LA_SUN || lar->type==LA_HEMI) {
		maxsize= RE_ray_tree_max_size(R.raytree);
		lampco[0]= shi->co[0] - maxsize*lar->vec[0];
		lampco[1]= shi->co[1] - maxsize*lar->vec[1];
		lampco[2]= shi->co[2] - maxsize*lar->vec[2];
	}
	else {
		VECCOPY(lampco, lar->co);
	}
	
	if (ELEM(lar->ray_samp_method, LA_SAMP_HALTON, LA_SAMP_HAMMERSLEY)) {
		
		ray_shadow_qmc(shi, lar, lampco, shadfac, &isec);
		
	} else {
		if(lar->ray_totsamp<2) {
			
			isec.faceorig= (RayFace*)shi->vlr;
			isec.oborig= RAY_OBJECT_SET(&R, shi->obi);
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
			}
			else if(RE_ray_tree_intersect(R.raytree, &isec)) shadfac[3]= 0.0f;
		}
		else {
			ray_shadow_jitter(shi, lar, lampco, shadfac, &isec);
		}
	}
		
	/* for first hit optim, set last interesected shadow face */
	if(shi->depth==0) {
		lar->vlr_last[shi->thread]= (VlakRen*)isec.face_last;
		lar->obi_last[shi->thread]= RAY_OBJECT_GET(&R, isec.ob_last);
	}

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
	isec.oborig= RAY_OBJECT_SET(&R, shi->obi);
	
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


