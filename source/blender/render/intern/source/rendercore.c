/*
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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * Contributors: Hos, Robert Wenzlaff.
 * Contributors: 2004/2005/2006 Blender Foundation, full recode
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/render/intern/source/rendercore.c
 *  \ingroup render
 */


/* system includes */
#include <stdio.h>
#include <math.h>
#include <float.h>
#include <string.h>
#include <assert.h>

/* External modules: */
#include "MEM_guardedalloc.h"

#include "BLI_math.h"
#include "BLI_blenlib.h"
#include "BLI_jitter.h"
#include "BLI_rand.h"
#include "BLI_threads.h"
#include "BLI_utildefines.h"



#include "DNA_image_types.h"
#include "DNA_lamp_types.h"
#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_group_types.h"

#include "BKE_customdata.h"
#include "BKE_depsgraph.h"
#include "BKE_global.h"
#include "BKE_image.h"
#include "BKE_main.h"
#include "BKE_node.h"
#include "BKE_texture.h"
#include "BKE_scene.h"

#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"
#include "IMB_colormanagement.h"

/* local include */
#include "rayintersection.h"
#include "rayobject.h"
#include "renderpipeline.h"
#include "render_result.h"
#include "render_types.h"
#include "renderdatabase.h"
#include "occlusion.h"
#include "pixelblending.h"
#include "pixelshading.h"
#include "shadbuf.h"
#include "shading.h"
#include "sss.h"
#include "zbuf.h"

#include "PIL_time.h"

/* own include */
#include "rendercore.h"


/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
/* defined in pipeline.c, is hardcopy of active dynamic allocated Render */
/* only to be used here in this file, it's for speed */
extern struct Render R;
/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/* x and y are current pixels in rect to be rendered */
/* do not normalize! */
void calc_view_vector(float view[3], float x, float y)
{

	view[2]= -ABS(R.clipsta);
	
	if (R.r.mode & R_ORTHO) {
		view[0]= view[1]= 0.0f;
	}
	else {
		
		if (R.r.mode & R_PANORAMA) {
			x-= R.panodxp;
		}
		
		/* move x and y to real viewplane coords */
		x = (x / (float)R.winx);
		view[0] = R.viewplane.xmin + x * BLI_rctf_size_x(&R.viewplane);
		
		y = (y / (float)R.winy);
		view[1] = R.viewplane.ymin + y * BLI_rctf_size_y(&R.viewplane);
		
//		if (R.flag & R_SEC_FIELD) {
//			if (R.r.mode & R_ODDFIELD) view[1]= (y+R.ystart)*R.ycor;
//			else view[1]= (y+R.ystart+1.0)*R.ycor;
//		}
//		else view[1]= (y+R.ystart+R.bluroffsy+0.5)*R.ycor;
	
		if (R.r.mode & R_PANORAMA) {
			float u= view[0] + R.panodxv; float v= view[2];
			view[0]= R.panoco*u + R.panosi*v;
			view[2]= -R.panosi*u + R.panoco*v;
		}
	}
}

void calc_renderco_ortho(float co[3], float x, float y, int z)
{
	/* x and y 3d coordinate can be derived from pixel coord and winmat */
	float fx= 2.0f/(R.winx*R.winmat[0][0]);
	float fy= 2.0f/(R.winy*R.winmat[1][1]);
	float zco;
	
	co[0]= (x - 0.5f*R.winx)*fx - R.winmat[3][0]/R.winmat[0][0];
	co[1]= (y - 0.5f*R.winy)*fy - R.winmat[3][1]/R.winmat[1][1];
	
	zco= ((float)z)/2147483647.0f;
	co[2]= R.winmat[3][2]/( R.winmat[2][3]*zco - R.winmat[2][2] );
}

void calc_renderco_zbuf(float co[3], const float view[3], int z)
{
	float fac, zco;
	
	/* inverse of zbuf calc: zbuf = MAXZ*hoco_z/hoco_w */
	zco= ((float)z)/2147483647.0f;
	co[2]= R.winmat[3][2]/( R.winmat[2][3]*zco - R.winmat[2][2] );

	fac= co[2]/view[2];
	co[0]= fac*view[0];
	co[1]= fac*view[1];
}

/* also used in zbuf.c and shadbuf.c */
int count_mask(unsigned short mask)
{
	if (R.samples)
		return (R.samples->cmask[mask & 255]+R.samples->cmask[mask>>8]);
	return 0;
}

static int calchalo_z(HaloRen *har, int zz)
{
	
	if (har->type & HA_ONLYSKY) {
		if (zz < 0x7FFFFFF0) zz= - 0x7FFFFF;	/* edge render messes zvalues */
	}
	else {
		zz= (zz>>8);
	}
	return zz;
}



static void halo_pixelstruct(HaloRen *har, RenderLayer **rlpp, int totsample, int od, float dist, float xn, float yn, PixStr *ps)
{
	float col[4], accol[4], fac;
	int amount, amountm, zz, flarec, sample, fullsample, mask=0;
	
	fullsample= (totsample > 1);
	amount= 0;
	accol[0] = accol[1] = accol[2] = accol[3]= 0.0f;
	col[0] = col[1] = col[2] = col[3]= 0.0f;
	flarec= har->flarec;
	
	while (ps) {
		amountm= count_mask(ps->mask);
		amount+= amountm;
		
		zz= calchalo_z(har, ps->z);
		if ((zz> har->zs) || (har->mat && (har->mat->mode & MA_HALO_SOFT))) {
			if (shadeHaloFloat(har, col, zz, dist, xn, yn, flarec)) {
				flarec= 0;

				if (fullsample) {
					for (sample=0; sample<totsample; sample++)
						if (ps->mask & (1 << sample))
							addalphaAddfacFloat(rlpp[sample]->rectf + od*4, col, har->add);
				}
				else {
					fac= ((float)amountm)/(float)R.osa;
					accol[0]+= fac*col[0];
					accol[1]+= fac*col[1];
					accol[2]+= fac*col[2];
					accol[3]+= fac*col[3];
				}
			}
		}
		
		mask |= ps->mask;
		ps= ps->next;
	}

	/* now do the sky sub-pixels */
	amount= R.osa-amount;
	if (amount) {
		if (shadeHaloFloat(har, col, 0x7FFFFF, dist, xn, yn, flarec)) {
			if (!fullsample) {
				fac= ((float)amount)/(float)R.osa;
				accol[0]+= fac*col[0];
				accol[1]+= fac*col[1];
				accol[2]+= fac*col[2];
				accol[3]+= fac*col[3];
			}
		}
	}

	if (fullsample) {
		for (sample=0; sample<totsample; sample++)
			if (!(mask & (1 << sample)))
				addalphaAddfacFloat(rlpp[sample]->rectf + od*4, col, har->add);
	}
	else {
		col[0]= accol[0];
		col[1]= accol[1];
		col[2]= accol[2];
		col[3]= accol[3];
		
		for (sample=0; sample<totsample; sample++)
			addalphaAddfacFloat(rlpp[sample]->rectf + od*4, col, har->add);
	}
}

static void halo_tile(RenderPart *pa, RenderLayer *rl)
{
	RenderLayer *rlpp[RE_MAX_OSA];
	HaloRen *har;
	rcti disprect= pa->disprect, testrect= pa->disprect;
	float dist, xsq, ysq, xn, yn;
	float col[4];
	intptr_t *rd= NULL;
	int a, *rz, zz, y, sample, totsample, od;
	short minx, maxx, miny, maxy, x;
	unsigned int lay= rl->lay;

	/* we don't render halos in the cropped area, gives errors in flare counter */
	if (pa->crop) {
		testrect.xmin+= pa->crop;
		testrect.xmax-= pa->crop;
		testrect.ymin+= pa->crop;
		testrect.ymax-= pa->crop;
	}
	
	totsample= get_sample_layers(pa, rl, rlpp);

	for (a=0; a<R.tothalo; a++) {
		har= R.sortedhalos[a];

		/* layer test, clip halo with y */
		if ((har->lay & lay) == 0) {
			/* pass */
		}
		else if (testrect.ymin > har->maxy) {
			/* pass */
		}
		else if (testrect.ymax < har->miny) {
			/* pass */
		}
		else {
			
			minx= floor(har->xs-har->rad);
			maxx= ceil(har->xs+har->rad);
			
			if (testrect.xmin > maxx) {
				/* pass */
			}
			else if (testrect.xmax < minx) {
				/* pass */
			}
			else {
				
				minx = max_ii(minx, testrect.xmin);
				maxx = min_ii(maxx, testrect.xmax);
			
				miny = max_ii(har->miny, testrect.ymin);
				maxy = min_ii(har->maxy, testrect.ymax);
			
				for (y=miny; y<maxy; y++) {
					int rectofs= (y-disprect.ymin)*pa->rectx + (minx - disprect.xmin);
					rz= pa->rectz + rectofs;
					od= rectofs;
					
					if (pa->rectdaps)
						rd= pa->rectdaps + rectofs;
					
					yn= (y-har->ys)*R.ycor;
					ysq= yn*yn;
					
					for (x=minx; x<maxx; x++, rz++, od++) {
						xn= x- har->xs;
						xsq= xn*xn;
						dist= xsq+ysq;
						if (dist<har->radsq) {
							if (rd && *rd) {
								halo_pixelstruct(har, rlpp, totsample, od, dist, xn, yn, (PixStr *)*rd);
							}
							else {
								zz= calchalo_z(har, *rz);
								if ((zz> har->zs) || (har->mat && (har->mat->mode & MA_HALO_SOFT))) {
									if (shadeHaloFloat(har, col, zz, dist, xn, yn, har->flarec)) {
										for (sample=0; sample<totsample; sample++)
											addalphaAddfacFloat(rlpp[sample]->rectf + od*4, col, har->add);
									}
								}
							}
						}
						if (rd) rd++;
					}
				}
			}
		}
		if (R.test_break(R.tbh) ) break; 
	}
}

static void lamphalo_tile(RenderPart *pa, RenderLayer *rl)
{
	RenderLayer *rlpp[RE_MAX_OSA];
	ShadeInput shi;
	float *pass;
	float fac, col[4];
	intptr_t *rd= pa->rectdaps;
	int *rz= pa->rectz;
	int x, y, sample, totsample, fullsample, od;
	
	totsample= get_sample_layers(pa, rl, rlpp);
	fullsample= (totsample > 1);

	shade_input_initialize(&shi, pa, rl, 0); /* this zero's ShadeInput for us */
	
	for (od=0, y=pa->disprect.ymin; y<pa->disprect.ymax; y++) {
		for (x=pa->disprect.xmin; x<pa->disprect.xmax; x++, rz++, od++) {
			
			calc_view_vector(shi.view, x, y);
			
			if (rd && *rd) {
				PixStr *ps= (PixStr *)*rd;
				int count, totsamp= 0, mask= 0;
				
				while (ps) {
					if (R.r.mode & R_ORTHO)
						calc_renderco_ortho(shi.co, (float)x, (float)y, ps->z);
					else
						calc_renderco_zbuf(shi.co, shi.view, ps->z);
					
					totsamp+= count= count_mask(ps->mask);
					mask |= ps->mask;

					col[0]= col[1]= col[2]= col[3]= 0.0f;
					renderspothalo(&shi, col, 1.0f);

					if (fullsample) {
						for (sample=0; sample<totsample; sample++) {
							if (ps->mask & (1 << sample)) {
								pass= rlpp[sample]->rectf + od*4;
								pass[0]+= col[0];
								pass[1]+= col[1];
								pass[2]+= col[2];
								pass[3]+= col[3];
								if (pass[3]>1.0f) pass[3]= 1.0f;
							}
						}
					}
					else {
						fac= ((float)count)/(float)R.osa;
						pass= rl->rectf + od*4;
						pass[0]+= fac*col[0];
						pass[1]+= fac*col[1];
						pass[2]+= fac*col[2];
						pass[3]+= fac*col[3];
						if (pass[3]>1.0f) pass[3]= 1.0f;
					}

					ps= ps->next;
				}

				if (totsamp<R.osa) {
					shi.co[2]= 0.0f;

					col[0]= col[1]= col[2]= col[3]= 0.0f;
					renderspothalo(&shi, col, 1.0f);

					if (fullsample) {
						for (sample=0; sample<totsample; sample++) {
							if (!(mask & (1 << sample))) {
								pass= rlpp[sample]->rectf + od*4;
								pass[0]+= col[0];
								pass[1]+= col[1];
								pass[2]+= col[2];
								pass[3]+= col[3];
								if (pass[3]>1.0f) pass[3]= 1.0f;
							}
						}
					}
					else {
						fac= ((float)R.osa-totsamp)/(float)R.osa;
						pass= rl->rectf + od*4;
						pass[0]+= fac*col[0];
						pass[1]+= fac*col[1];
						pass[2]+= fac*col[2];
						pass[3]+= fac*col[3];
						if (pass[3]>1.0f) pass[3]= 1.0f;
					}
				}
			}
			else {
				if (R.r.mode & R_ORTHO)
					calc_renderco_ortho(shi.co, (float)x, (float)y, *rz);
				else
					calc_renderco_zbuf(shi.co, shi.view, *rz);
				
				col[0]= col[1]= col[2]= col[3]= 0.0f;
				renderspothalo(&shi, col, 1.0f);

				for (sample=0; sample<totsample; sample++) {
					pass= rlpp[sample]->rectf + od*4;
					pass[0]+= col[0];
					pass[1]+= col[1];
					pass[2]+= col[2];
					pass[3]+= col[3];
					if (pass[3]>1.0f) pass[3]= 1.0f;
				}
			}
			
			if (rd) rd++;
		}
		if (y&1)
			if (R.test_break(R.tbh)) break; 
	}
}				


/* ********************* MAINLOOPS ******************** */

/* osa version */
static void add_filt_passes(RenderLayer *rl, int curmask, int rectx, int offset, ShadeInput *shi, ShadeResult *shr)
{
	RenderPass *rpass;

	/* combined rgb */
	add_filt_fmask(curmask, shr->combined, rl->rectf + 4*offset, rectx);
	
	for (rpass= rl->passes.first; rpass; rpass= rpass->next) {
		float *fp, *col= NULL;
		int pixsize= 3;
		
		switch (rpass->passtype) {
			case SCE_PASS_Z:
				fp= rpass->rect + offset;
				*fp= shr->z;
				break;
			case SCE_PASS_RGBA:
				col= shr->col;
				pixsize= 4;
				break;
			case SCE_PASS_EMIT:
				col= shr->emit;
				break;
			case SCE_PASS_DIFFUSE:
				col= shr->diff;
				break;
			case SCE_PASS_SPEC:
				col= shr->spec;
				break;
			case SCE_PASS_SHADOW:
				col= shr->shad;
				break;
			case SCE_PASS_AO:
				col= shr->ao;
				break;
			case SCE_PASS_ENVIRONMENT:
				col= shr->env;
				break;
			case SCE_PASS_INDIRECT:
				col= shr->indirect;
				break;
			case SCE_PASS_REFLECT:
				col= shr->refl;
				break;
			case SCE_PASS_REFRACT:
				col= shr->refr;
				break;
			case SCE_PASS_NORMAL:
				col= shr->nor;
				break;
			case SCE_PASS_UV:
				/* box filter only, gauss will screwup UV too much */
				if (shi->totuv) {
					float mult= (float)count_mask(curmask)/(float)R.osa;
					fp= rpass->rect + 3*offset;
					fp[0]+= mult*(0.5f + 0.5f*shi->uv[shi->actuv].uv[0]);
					fp[1]+= mult*(0.5f + 0.5f*shi->uv[shi->actuv].uv[1]);
					fp[2]+= mult;
				}
				break;
			case SCE_PASS_INDEXOB:
				/* no filter */
				if (shi->vlr) {
					fp= rpass->rect + offset;
					if (*fp==0.0f)
						*fp= (float)shi->obr->ob->index;
				}
				break;
			case SCE_PASS_INDEXMA:
					/* no filter */
					if (shi->vlr) {
							fp= rpass->rect + offset;
							if (*fp==0.0f)
									*fp= (float)shi->mat->index;
					}
					break;
			case SCE_PASS_MIST:
				/*  */
				col= &shr->mist;
				pixsize= 1;
				break;
			
			case SCE_PASS_VECTOR:
			{
				/* add minimum speed in pixel, no filter */
				fp= rpass->rect + 4*offset;
				if ( (ABS(shr->winspeed[0]) + ABS(shr->winspeed[1]))< (ABS(fp[0]) + ABS(fp[1])) ) {
					fp[0]= shr->winspeed[0];
					fp[1]= shr->winspeed[1];
				}
				if ( (ABS(shr->winspeed[2]) + ABS(shr->winspeed[3]))< (ABS(fp[2]) + ABS(fp[3])) ) {
					fp[2]= shr->winspeed[2];
					fp[3]= shr->winspeed[3];
				}
			}
				break;

			case SCE_PASS_RAYHITS:
				/*  */
				col= shr->rayhits;
				pixsize= 4;
				break;
		}
		if (col) {
			fp= rpass->rect + pixsize*offset;
			add_filt_fmask_pixsize(curmask, col, fp, rectx, pixsize);
		}
	}
}

/* non-osa version */
static void add_passes(RenderLayer *rl, int offset, ShadeInput *shi, ShadeResult *shr)
{
	RenderPass *rpass;
	float *fp;
	
	fp= rl->rectf + 4*offset;
	copy_v4_v4(fp, shr->combined);
	
	for (rpass= rl->passes.first; rpass; rpass= rpass->next) {
		float *col= NULL, uvcol[3];
		int a, pixsize= 3;
		
		switch (rpass->passtype) {
			case SCE_PASS_Z:
				fp= rpass->rect + offset;
				*fp= shr->z;
				break;
			case SCE_PASS_RGBA:
				col= shr->col;
				pixsize= 4;
				break;
			case SCE_PASS_EMIT:
				col= shr->emit;
				break;
			case SCE_PASS_DIFFUSE:
				col= shr->diff;
				break;
			case SCE_PASS_SPEC:
				col= shr->spec;
				break;
			case SCE_PASS_SHADOW:
				col= shr->shad;
				break;
			case SCE_PASS_AO:
				col= shr->ao;
				break;
			case SCE_PASS_ENVIRONMENT:
				col= shr->env;
				break;
			case SCE_PASS_INDIRECT:
				col= shr->indirect;
				break;
			case SCE_PASS_REFLECT:
				col= shr->refl;
				break;
			case SCE_PASS_REFRACT:
				col= shr->refr;
				break;
			case SCE_PASS_NORMAL:
				col= shr->nor;
				break;
			case SCE_PASS_UV:
				if (shi->totuv) {
					uvcol[0]= 0.5f + 0.5f*shi->uv[shi->actuv].uv[0];
					uvcol[1]= 0.5f + 0.5f*shi->uv[shi->actuv].uv[1];
					uvcol[2]= 1.0f;
					col= uvcol;
				}
				break;
			case SCE_PASS_VECTOR:
				col= shr->winspeed;
				pixsize= 4;
				break;
			case SCE_PASS_INDEXOB:
				if (shi->vlr) {
					fp= rpass->rect + offset;
					*fp= (float)shi->obr->ob->index;
				}
				break;
			case SCE_PASS_INDEXMA:
				if (shi->vlr) {
					fp= rpass->rect + offset;
					*fp= (float)shi->mat->index;
				}
				break;
			case SCE_PASS_MIST:
				fp= rpass->rect + offset;
				*fp= shr->mist;
				break;
			case SCE_PASS_RAYHITS:
				col= shr->rayhits;
				pixsize= 4;
				break;
		}
		if (col) {
			fp= rpass->rect + pixsize*offset;
			for (a=0; a<pixsize; a++)
				fp[a]= col[a];
		}
	}
}

int get_sample_layers(RenderPart *pa, RenderLayer *rl, RenderLayer **rlpp)
{
	
	if (pa->fullresult.first) {
		int sample, nr= BLI_findindex(&pa->result->layers, rl);
		
		for (sample=0; sample<R.osa; sample++) {
			RenderResult *rr= BLI_findlink(&pa->fullresult, sample);
		
			rlpp[sample]= BLI_findlink(&rr->layers, nr);
		}
		return R.osa;
	}
	else {
		rlpp[0]= rl;
		return 1;
	}
}


/* only do sky, is default in the solid layer (shade_tile) btw */
static void sky_tile(RenderPart *pa, RenderLayer *rl)
{
	RenderLayer *rlpp[RE_MAX_OSA];
	int x, y, od=0, totsample;
	
	if (R.r.alphamode!=R_ADDSKY)
		return;
	
	totsample= get_sample_layers(pa, rl, rlpp);
	
	for (y=pa->disprect.ymin; y<pa->disprect.ymax; y++) {
		for (x=pa->disprect.xmin; x<pa->disprect.xmax; x++, od+=4) {
			float col[4];
			int sample, done = FALSE;
			
			for (sample= 0; sample<totsample; sample++) {
				float *pass= rlpp[sample]->rectf + od;
				
				if (pass[3]<1.0f) {
					
					if (done==0) {
						shadeSkyPixel(col, x, y, pa->thread);
						done = TRUE;
					}
					
					if (pass[3]==0.0f) {
						copy_v4_v4(pass, col);
						pass[3] = 1.0f;
					}
					else {
						addAlphaUnderFloat(pass, col);
						pass[3] = 1.0f;
					}
				}
			}
		}
		
		if (y&1)
			if (R.test_break(R.tbh)) break; 
	}
}

static void atm_tile(RenderPart *pa, RenderLayer *rl)
{
	RenderPass *zpass;
	GroupObject *go;
	LampRen *lar;
	RenderLayer *rlpp[RE_MAX_OSA];
	int totsample;
	int x, y, od= 0;
	
	totsample= get_sample_layers(pa, rl, rlpp);

	/* check that z pass is enabled */
	if (pa->rectz==NULL) return;
	for (zpass= rl->passes.first; zpass; zpass= zpass->next)
		if (zpass->passtype==SCE_PASS_Z)
			break;
	
	if (zpass==NULL) return;

	/* check for at least one sun lamp that its atmosphere flag is enabled */
	for (go=R.lights.first; go; go= go->next) {
		lar= go->lampren;
		if (lar->type==LA_SUN && lar->sunsky && (lar->sunsky->effect_type & LA_SUN_EFFECT_AP))
			break;
	}
	/* do nothign and return if there is no sun lamp */
	if (go==NULL)
		return;
	
	/* for each x,y and each sample, and each sun lamp*/
	for (y=pa->disprect.ymin; y<pa->disprect.ymax; y++) {
		for (x=pa->disprect.xmin; x<pa->disprect.xmax; x++, od++) {
			int sample;
			
			for (sample=0; sample<totsample; sample++) {
				float *zrect= RE_RenderLayerGetPass(rlpp[sample], SCE_PASS_Z) + od;
				float *rgbrect = rlpp[sample]->rectf + 4*od;
				float rgb[3] = {0};
				int done = FALSE;
				
				for (go=R.lights.first; go; go= go->next) {
				
					
					lar= go->lampren;
					if (lar->type==LA_SUN &&	lar->sunsky) {
						
						/* if it's sky continue and don't apply atmosphere effect on it */
						if (*zrect >= 9.9e10f || rgbrect[3]==0.0f) {
							continue;
						}

						if ((lar->sunsky->effect_type & LA_SUN_EFFECT_AP)) {
							float tmp_rgb[3];
							
							/* skip if worldspace lamp vector is below horizon */
							if (go->ob->obmat[2][2] < 0.f) {
								continue;
							}
							
							copy_v3_v3(tmp_rgb, rgbrect);
							if (rgbrect[3]!=1.0f) {	/* de-premul */
								mul_v3_fl(tmp_rgb, 1.0f/rgbrect[3]);
							}
							shadeAtmPixel(lar->sunsky, tmp_rgb, x, y, *zrect);
							if (rgbrect[3]!=1.0f) {	/* premul */
								mul_v3_fl(tmp_rgb, rgbrect[3]);
							}
							
							if (done==0) {
								copy_v3_v3(rgb, tmp_rgb);
								done = TRUE;
							}
							else {
								rgb[0] = 0.5f*rgb[0] + 0.5f*tmp_rgb[0];
								rgb[1] = 0.5f*rgb[1] + 0.5f*tmp_rgb[1];
								rgb[2] = 0.5f*rgb[2] + 0.5f*tmp_rgb[2];
							}
						}
					}
				}

				/* if at least for one sun lamp aerial perspective was applied*/
				if (done) {
					copy_v3_v3(rgbrect, rgb);
				}
			}
		}
	}
}

static void shadeDA_tile(RenderPart *pa, RenderLayer *rl)
{
	RenderResult *rr= pa->result;
	ShadeSample ssamp;
	intptr_t *rd, *rectdaps= pa->rectdaps;
	int samp;
	int x, y, seed, crop=0, offs=0, od;
	
	if (R.test_break(R.tbh)) return; 
	
	/* irregular shadowb buffer creation */
	if (R.r.mode & R_SHADOW)
		ISB_create(pa, NULL);
	
	/* we set per pixel a fixed seed, for random AO and shadow samples */
	seed= pa->rectx*pa->disprect.ymin;
	
	/* general shader info, passes */
	shade_sample_initialize(&ssamp, pa, rl);

	/* occlusion caching */
	if (R.occlusiontree)
		cache_occ_samples(&R, pa, &ssamp);
		
	/* filtered render, for now we assume only 1 filter size */
	if (pa->crop) {
		crop= 1;
		rectdaps+= pa->rectx + 1;
		offs= pa->rectx + 1;
	}
	
	/* scanline updates have to be 2 lines behind */
	rr->renrect.ymin = 0;
	rr->renrect.ymax = -2*crop;
	rr->renlay= rl;
				
	for (y=pa->disprect.ymin+crop; y<pa->disprect.ymax-crop; y++, rr->renrect.ymax++) {
		rd= rectdaps;
		od= offs;
		
		for (x=pa->disprect.xmin+crop; x<pa->disprect.xmax-crop; x++, rd++, od++) {
			BLI_thread_srandom(pa->thread, seed++);
			
			if (*rd) {
				if (shade_samples(&ssamp, (PixStr *)(*rd), x, y)) {
					
					/* multisample buffers or filtered mask filling? */
					if (pa->fullresult.first) {
						int a;
						for (samp=0; samp<ssamp.tot; samp++) {
							int smask= ssamp.shi[samp].mask;
							for (a=0; a<R.osa; a++) {
								int mask= 1<<a;
								if (smask & mask)
									add_passes(ssamp.rlpp[a], od, &ssamp.shi[samp], &ssamp.shr[samp]);
							}
						}
					}
					else {
						for (samp=0; samp<ssamp.tot; samp++)
							add_filt_passes(rl, ssamp.shi[samp].mask, pa->rectx, od, &ssamp.shi[samp], &ssamp.shr[samp]);
					}
				}
			}
		}
		
		rectdaps+= pa->rectx;
		offs+= pa->rectx;
		
		if (y&1) if (R.test_break(R.tbh)) break; 
	}
	
	/* disable scanline updating */
	rr->renlay= NULL;
	
	if (R.r.mode & R_SHADOW)
		ISB_free(pa);

	if (R.occlusiontree)
		free_occ_samples(&R, pa);
}

/* ************* pixel struct ******** */


static PixStrMain *addpsmain(ListBase *lb)
{
	PixStrMain *psm;
	
	psm= (PixStrMain *)MEM_mallocN(sizeof(PixStrMain), "pixstrMain");
	BLI_addtail(lb, psm);
	
	psm->ps= (PixStr *)MEM_mallocN(4096*sizeof(PixStr), "pixstr");
	psm->counter= 0;
	
	return psm;
}

static void freeps(ListBase *lb)
{
	PixStrMain *psm, *psmnext;
	
	for (psm= lb->first; psm; psm= psmnext) {
		psmnext= psm->next;
		if (psm->ps)
			MEM_freeN(psm->ps);
		MEM_freeN(psm);
	}
	lb->first= lb->last= NULL;
}

static void addps(ListBase *lb, intptr_t *rd, int obi, int facenr, int z, int maskz, unsigned short mask)
{
	PixStrMain *psm;
	PixStr *ps, *last= NULL;
	
	if (*rd) {
		ps= (PixStr *)(*rd);
		
		while (ps) {
			if ( ps->obi == obi && ps->facenr == facenr ) {
				ps->mask |= mask;
				return;
			}
			last= ps;
			ps= ps->next;
		}
	}
	
	/* make new PS (pixel struct) */
	psm= lb->last;
	
	if (psm->counter==4095)
		psm= addpsmain(lb);
	
	ps= psm->ps + psm->counter++;
	
	if (last) last->next= ps;
	else *rd= (intptr_t)ps;
	
	ps->next= NULL;
	ps->obi= obi;
	ps->facenr= facenr;
	ps->z= z;
	ps->maskz= maskz;
	ps->mask = mask;
	ps->shadfac= 0;
}

static void edge_enhance_add(RenderPart *pa, float *rectf, float *arect)
{
	float addcol[4];
	int pix;
	
	if (arect==NULL)
		return;
	
	for (pix= pa->rectx*pa->recty; pix>0; pix--, arect++, rectf+=4) {
		if (*arect != 0.0f) {
			addcol[0]= *arect * R.r.edgeR;
			addcol[1]= *arect * R.r.edgeG;
			addcol[2]= *arect * R.r.edgeB;
			addcol[3]= *arect;
			addAlphaOverFloat(rectf, addcol);
		}
	}
}

/* clamp alpha and RGB to 0..1 and 0..inf, can go outside due to filter */
static void clamp_alpha_rgb_range(RenderPart *pa, RenderLayer *rl)
{
	RenderLayer *rlpp[RE_MAX_OSA];
	int y, sample, totsample;
	
	totsample= get_sample_layers(pa, rl, rlpp);

	/* not for full sample, there we clamp after compositing */
	if (totsample > 1)
		return;
	
	for (sample= 0; sample<totsample; sample++) {
		float *rectf= rlpp[sample]->rectf;
		
		for (y= pa->rectx*pa->recty; y>0; y--, rectf+=4) {
			rectf[0] = MAX2(rectf[0], 0.0f);
			rectf[1] = MAX2(rectf[1], 0.0f);
			rectf[2] = MAX2(rectf[2], 0.0f);
			CLAMP(rectf[3], 0.0f, 1.0f);
		}
	}
}

/* adds only alpha values */
static void edge_enhance_tile(RenderPart *pa, float *rectf, int *rectz)
{
	/* use zbuffer to define edges, add it to the image */
	int y, x, col, *rz, *rz1, *rz2, *rz3;
	int zval1, zval2, zval3;
	float *rf;
	
	/* shift values in zbuffer 4 to the right (anti overflows), for filter we need multiplying with 12 max */
	rz= rectz;
	if (rz==NULL) return;
	
	for (y=0; y<pa->recty; y++)
		for (x=0; x<pa->rectx; x++, rz++) (*rz)>>= 4;
	
	rz1= rectz;
	rz2= rz1+pa->rectx;
	rz3= rz2+pa->rectx;
	
	rf= rectf+pa->rectx+1;
	
	for (y=0; y<pa->recty-2; y++) {
		for (x=0; x<pa->rectx-2; x++, rz1++, rz2++, rz3++, rf++) {
			
			/* prevent overflow with sky z values */
			zval1=   rz1[0] + 2*rz1[1] +   rz1[2];
			zval2=  2*rz2[0]           + 2*rz2[2];
			zval3=   rz3[0] + 2*rz3[1] +   rz3[2];
			
			col= ( 4*rz2[1] - (zval1 + zval2 + zval3)/3 );
			if (col<0) col= -col;
			
			col >>= 5;
			if (col > (1<<16)) col= (1<<16);
			else col= (R.r.edgeint*col)>>8;
			
			if (col>0) {
				float fcol;
				
				if (col>255) fcol= 1.0f;
				else fcol= (float)col/255.0f;
				
				if (R.osa)
					*rf+= fcol/(float)R.osa;
				else
					*rf= fcol;
			}
		}
		rz1+= 2;
		rz2+= 2;
		rz3+= 2;
		rf+= 2;
	}
	
	/* shift back zbuf values, we might need it still */
	rz= rectz;
	for (y=0; y<pa->recty; y++)
		for (x=0; x<pa->rectx; x++, rz++) (*rz)<<= 4;
	
}

static void reset_sky_speed(RenderPart *pa, RenderLayer *rl)
{
	/* for all pixels with max speed, set to zero */
	RenderLayer *rlpp[RE_MAX_OSA];
	float *fp;
	int a, sample, totsample;
	
	totsample= get_sample_layers(pa, rl, rlpp);

	for (sample= 0; sample<totsample; sample++) {
		fp= RE_RenderLayerGetPass(rlpp[sample], SCE_PASS_VECTOR);
		if (fp==NULL) break;

		for (a= 4*pa->rectx*pa->recty - 1; a>=0; a--)
			if (fp[a] == PASS_VECTOR_MAX) fp[a]= 0.0f;
	}
}

static unsigned short *make_solid_mask(RenderPart *pa)
{ 
	intptr_t *rd= pa->rectdaps;
	unsigned short *solidmask, *sp;
	int x;

	if (rd==NULL) return NULL;

	sp=solidmask= MEM_mallocN(sizeof(short)*pa->rectx*pa->recty, "solidmask");

	for (x=pa->rectx*pa->recty; x>0; x--, rd++, sp++) {
		if (*rd) {
			PixStr *ps= (PixStr *)*rd;
			
			*sp= ps->mask;
			for (ps= ps->next; ps; ps= ps->next)
				*sp |= ps->mask;
		}
		else
			*sp= 0;
	}

	return solidmask;
}

static void addAlphaOverFloatMask(float *dest, float *source, unsigned short dmask, unsigned short smask)
{
	unsigned short shared= dmask & smask;
	float mul= 1.0f - source[3];
	
	if (shared) {	/* overlapping masks */
		
		/* masks differ, we make a mixture of 'add' and 'over' */
		if (shared!=dmask) {
			float shared_bits= (float)count_mask(shared);		/* alpha over */
			float tot_bits= (float)count_mask(smask|dmask);		/* alpha add */
			
			float add= (tot_bits - shared_bits)/tot_bits;		/* add level */
			mul= add + (1.0f-add)*mul;
		}
	}
	else if (dmask && smask) {
		/* works for premul only, of course */
		dest[0]+= source[0];
		dest[1]+= source[1];
		dest[2]+= source[2];
		dest[3]+= source[3];
		
		return;
	}

	dest[0]= (mul*dest[0]) + source[0];
	dest[1]= (mul*dest[1]) + source[1];
	dest[2]= (mul*dest[2]) + source[2];
	dest[3]= (mul*dest[3]) + source[3];
}

typedef struct ZbufSolidData {
	RenderLayer *rl;
	ListBase *psmlist;
	float *edgerect;
} ZbufSolidData;

static void make_pixelstructs(RenderPart *pa, ZSpan *zspan, int sample, void *data)
{
	ZbufSolidData *sdata = (ZbufSolidData *)data;
	ListBase *lb= sdata->psmlist;
	intptr_t *rd= pa->rectdaps;
	int *ro= zspan->recto;
	int *rp= zspan->rectp;
	int *rz= zspan->rectz;
	int *rm= zspan->rectmask;
	int x, y;
	int mask= 1<<sample;

	for (y=0; y<pa->recty; y++) {
		for (x=0; x<pa->rectx; x++, rd++, rp++, ro++, rz++, rm++) {
			if (*rp) {
				addps(lb, rd, *ro, *rp, *rz, (zspan->rectmask)? *rm: 0, mask);
			}
		}
	}

	if (sdata->rl->layflag & SCE_LAY_EDGE) 
		if (R.r.mode & R_EDGE) 
			edge_enhance_tile(pa, sdata->edgerect, zspan->rectz);
}

/* main call for shading Delta Accum, for OSA */
/* supposed to be fully threadable! */
void zbufshadeDA_tile(RenderPart *pa)
{
	RenderResult *rr= pa->result;
	RenderLayer *rl;
	ListBase psmlist= {NULL, NULL};
	float *edgerect= NULL;
	
	/* allocate the necessary buffers */
				/* zbuffer inits these rects */
	pa->recto= MEM_mallocN(sizeof(int)*pa->rectx*pa->recty, "recto");
	pa->rectp= MEM_mallocN(sizeof(int)*pa->rectx*pa->recty, "rectp");
	pa->rectz= MEM_mallocN(sizeof(int)*pa->rectx*pa->recty, "rectz");
	for (rl= rr->layers.first; rl; rl= rl->next) {
		if ((rl->layflag & SCE_LAY_ZMASK) && (rl->layflag & SCE_LAY_NEG_ZMASK))
			pa->rectmask= MEM_mallocN(sizeof(int)*pa->rectx*pa->recty, "rectmask");
	
		/* initialize pixelstructs and edge buffer */
		addpsmain(&psmlist);
		pa->rectdaps= MEM_callocN(sizeof(intptr_t)*pa->rectx*pa->recty+4, "zbufDArectd");
		
		if (rl->layflag & SCE_LAY_EDGE) 
			if (R.r.mode & R_EDGE) 
				edgerect= MEM_callocN(sizeof(float)*pa->rectx*pa->recty, "rectedge");
		
		/* always fill visibility */
		for (pa->sample=0; pa->sample<R.osa; pa->sample+=4) {
			ZbufSolidData sdata;

			sdata.rl= rl;
			sdata.psmlist= &psmlist;
			sdata.edgerect= edgerect;
			zbuffer_solid(pa, rl, make_pixelstructs, &sdata);
			if (R.test_break(R.tbh)) break; 
		}
		
		/* shades solid */
		if (rl->layflag & SCE_LAY_SOLID) 
			shadeDA_tile(pa, rl);
		
		/* lamphalo after solid, before ztra, looks nicest because ztra does own halo */
		if (R.flag & R_LAMPHALO)
			if (rl->layflag & SCE_LAY_HALO)
				lamphalo_tile(pa, rl);
		
		/* halo before ztra, because ztra fills in zbuffer now */
		if (R.flag & R_HALO)
			if (rl->layflag & SCE_LAY_HALO)
				halo_tile(pa, rl);

		/* transp layer */
		if (R.flag & R_ZTRA || R.totstrand) {
			if (rl->layflag & (SCE_LAY_ZTRA|SCE_LAY_STRAND)) {
				if (pa->fullresult.first) {
					zbuffer_transp_shade(pa, rl, rl->rectf, &psmlist);
				}
				else {
					unsigned short *ztramask, *solidmask= NULL; /* 16 bits, MAX_OSA */
					
					/* allocate, but not free here, for asynchronous display of this rect in main thread */
					rl->acolrect= MEM_callocN(4*sizeof(float)*pa->rectx*pa->recty, "alpha layer");
					
					/* swap for live updates, and it is used in zbuf.c!!! */
					SWAP(float *, rl->acolrect, rl->rectf);
					ztramask= zbuffer_transp_shade(pa, rl, rl->rectf, &psmlist);
					SWAP(float *, rl->acolrect, rl->rectf);
					
					/* zbuffer transp only returns ztramask if there's solid rendered */
					if (ztramask)
						solidmask= make_solid_mask(pa);

					if (ztramask && solidmask) {
						unsigned short *sps= solidmask, *spz= ztramask;
						unsigned short fullmask= (1<<R.osa)-1;
						float *fcol= rl->rectf; float *acol= rl->acolrect;
						int x;
						
						for (x=pa->rectx*pa->recty; x>0; x--, acol+=4, fcol+=4, sps++, spz++) {
							if (*sps == fullmask)
								addAlphaOverFloat(fcol, acol);
							else
								addAlphaOverFloatMask(fcol, acol, *sps, *spz);
						}
					}
					else {
						float *fcol= rl->rectf; float *acol= rl->acolrect;
						int x;
						for (x=pa->rectx*pa->recty; x>0; x--, acol+=4, fcol+=4) {
							addAlphaOverFloat(fcol, acol);
						}
					}
					if (solidmask) MEM_freeN(solidmask);
					if (ztramask) MEM_freeN(ztramask);
				}
			}
		}

		/* sun/sky */
		if (rl->layflag & SCE_LAY_SKY)
			atm_tile(pa, rl);
		
		/* sky before edge */
		if (rl->layflag & SCE_LAY_SKY)
			sky_tile(pa, rl);

		/* extra layers */
		if (rl->layflag & SCE_LAY_EDGE) 
			if (R.r.mode & R_EDGE) 
				edge_enhance_add(pa, rl->rectf, edgerect);
		
		if (rl->passflag & SCE_PASS_VECTOR)
			reset_sky_speed(pa, rl);

		/* clamp alpha to 0..1 range, can go outside due to filter */
		clamp_alpha_rgb_range(pa, rl);
		
		/* free stuff within loop! */
		MEM_freeN(pa->rectdaps); pa->rectdaps= NULL;
		freeps(&psmlist);
		
		if (edgerect) MEM_freeN(edgerect);
		edgerect= NULL;

		if (pa->rectmask) {
			MEM_freeN(pa->rectmask);
			pa->rectmask= NULL;
		}
	}
	
	/* free all */
	MEM_freeN(pa->recto); pa->recto= NULL;
	MEM_freeN(pa->rectp); pa->rectp= NULL;
	MEM_freeN(pa->rectz); pa->rectz= NULL;
	
	/* display active layer */
	rr->renrect.ymin=rr->renrect.ymax = 0;
	rr->renlay= render_get_active_layer(&R, rr);
}


/* ------------------------------------------------------------------------ */

/* non OSA case, full tile render */
/* supposed to be fully threadable! */
void zbufshade_tile(RenderPart *pa)
{
	ShadeSample ssamp;
	RenderResult *rr= pa->result;
	RenderLayer *rl;
	PixStr ps;
	float *edgerect= NULL;
	
	/* fake pixel struct, to comply to osa render */
	ps.next= NULL;
	ps.mask= 0xFFFF;
	
	/* zbuffer code clears/inits rects */
	pa->recto= MEM_mallocN(sizeof(int)*pa->rectx*pa->recty, "recto");
	pa->rectp= MEM_mallocN(sizeof(int)*pa->rectx*pa->recty, "rectp");
	pa->rectz= MEM_mallocN(sizeof(int)*pa->rectx*pa->recty, "rectz");

	for (rl= rr->layers.first; rl; rl= rl->next) {
		if ((rl->layflag & SCE_LAY_ZMASK) && (rl->layflag & SCE_LAY_NEG_ZMASK))
			pa->rectmask= MEM_mallocN(sizeof(int)*pa->rectx*pa->recty, "rectmask");

		/* general shader info, passes */
		shade_sample_initialize(&ssamp, pa, rl);
		
		zbuffer_solid(pa, rl, NULL, NULL);
		
		if (!R.test_break(R.tbh)) {	/* NOTE: this if () is not consistent */
			
			/* edges only for solid part, ztransp doesn't support it yet anti-aliased */
			if (rl->layflag & SCE_LAY_EDGE) {
				if (R.r.mode & R_EDGE) {
					edgerect= MEM_callocN(sizeof(float)*pa->rectx*pa->recty, "rectedge");
					edge_enhance_tile(pa, edgerect, pa->rectz);
				}
			}
			
			/* initialize scanline updates for main thread */
			rr->renrect.ymin = 0;
			rr->renlay= rl;
			
			if (rl->layflag & SCE_LAY_SOLID) {
				float *fcol= rl->rectf;
				int *ro= pa->recto, *rp= pa->rectp, *rz= pa->rectz;
				int x, y, offs=0, seed;
				
				/* we set per pixel a fixed seed, for random AO and shadow samples */
				seed= pa->rectx*pa->disprect.ymin;
				
				/* irregular shadowb buffer creation */
				if (R.r.mode & R_SHADOW)
					ISB_create(pa, NULL);

				if (R.occlusiontree)
					cache_occ_samples(&R, pa, &ssamp);
				
				for (y=pa->disprect.ymin; y<pa->disprect.ymax; y++, rr->renrect.ymax++) {
					for (x=pa->disprect.xmin; x<pa->disprect.xmax; x++, ro++, rz++, rp++, fcol+=4, offs++) {
						/* per pixel fixed seed */
						BLI_thread_srandom(pa->thread, seed++);
						
						if (*rp) {
							ps.obi= *ro;
							ps.facenr= *rp;
							ps.z= *rz;
							if (shade_samples(&ssamp, &ps, x, y)) {
								/* combined and passes */
								add_passes(rl, offs, ssamp.shi, ssamp.shr);
							}
						}
					}
					if (y&1)
						if (R.test_break(R.tbh)) break; 
				}
				
				if (R.occlusiontree)
					free_occ_samples(&R, pa);
				
				if (R.r.mode & R_SHADOW)
					ISB_free(pa);
			}
			
			/* disable scanline updating */
			rr->renlay= NULL;
		}
		
		/* lamphalo after solid, before ztra, looks nicest because ztra does own halo */
		if (R.flag & R_LAMPHALO)
			if (rl->layflag & SCE_LAY_HALO)
				lamphalo_tile(pa, rl);
		
		/* halo before ztra, because ztra fills in zbuffer now */
		if (R.flag & R_HALO)
			if (rl->layflag & SCE_LAY_HALO)
				halo_tile(pa, rl);
		
		if (R.flag & R_ZTRA || R.totstrand) {
			if (rl->layflag & (SCE_LAY_ZTRA|SCE_LAY_STRAND)) {
				float *fcol, *acol;
				int x;
				
				/* allocate, but not free here, for asynchronous display of this rect in main thread */
				rl->acolrect= MEM_callocN(4*sizeof(float)*pa->rectx*pa->recty, "alpha layer");
				
				/* swap for live updates */
				SWAP(float *, rl->acolrect, rl->rectf);
				zbuffer_transp_shade(pa, rl, rl->rectf, NULL);
				SWAP(float *, rl->acolrect, rl->rectf);
				
				fcol= rl->rectf; acol= rl->acolrect;
				for (x=pa->rectx*pa->recty; x>0; x--, acol+=4, fcol+=4) {
					addAlphaOverFloat(fcol, acol);
				}
			}
		}
		
		/* sun/sky */
		if (rl->layflag & SCE_LAY_SKY)
			atm_tile(pa, rl);
		
		/* sky before edge */
		if (rl->layflag & SCE_LAY_SKY)
			sky_tile(pa, rl);
		
		if (!R.test_break(R.tbh)) {
			if (rl->layflag & SCE_LAY_EDGE) 
				if (R.r.mode & R_EDGE)
					edge_enhance_add(pa, rl->rectf, edgerect);
		}
		
		if (rl->passflag & SCE_PASS_VECTOR)
			reset_sky_speed(pa, rl);
		
		if (edgerect) MEM_freeN(edgerect);
		edgerect= NULL;

		if (pa->rectmask) {
			MEM_freeN(pa->rectmask);
			pa->rectmask= NULL;
		}
	}

	/* display active layer */
	rr->renrect.ymin=rr->renrect.ymax = 0;
	rr->renlay= render_get_active_layer(&R, rr);
	
	MEM_freeN(pa->recto); pa->recto= NULL;
	MEM_freeN(pa->rectp); pa->rectp= NULL;
	MEM_freeN(pa->rectz); pa->rectz= NULL;
}

/* SSS preprocess tile render, fully threadable */
typedef struct ZBufSSSHandle {
	RenderPart *pa;
	ListBase psmlist;
	int totps;
} ZBufSSSHandle;

static void addps_sss(void *cb_handle, int obi, int facenr, int x, int y, int z)
{
	ZBufSSSHandle *handle = cb_handle;
	RenderPart *pa= handle->pa;

	/* extra border for filter gives double samples on part edges,
	 * don't use those */
	if (x<pa->crop || x>=pa->rectx-pa->crop)
		return;
	if (y<pa->crop || y>=pa->recty-pa->crop)
		return;
	
	if (pa->rectall) {
		intptr_t *rs= pa->rectall + pa->rectx*y + x;

		addps(&handle->psmlist, rs, obi, facenr, z, 0, 0);
		handle->totps++;
	}
	if (pa->rectz) {
		int *rz= pa->rectz + pa->rectx*y + x;
		int *rp= pa->rectp + pa->rectx*y + x;
		int *ro= pa->recto + pa->rectx*y + x;

		if (z < *rz) {
			if (*rp == 0)
				handle->totps++;
			*rz= z;
			*rp= facenr;
			*ro= obi;
		}
	}
	if (pa->rectbackz) {
		int *rz= pa->rectbackz + pa->rectx*y + x;
		int *rp= pa->rectbackp + pa->rectx*y + x;
		int *ro= pa->rectbacko + pa->rectx*y + x;

		if (z >= *rz) {
			if (*rp == 0)
				handle->totps++;
			*rz= z;
			*rp= facenr;
			*ro= obi;
		}
	}
}

static void shade_sample_sss(ShadeSample *ssamp, Material *mat, ObjectInstanceRen *obi, VlakRen *vlr, int quad, float x, float y, float z, float *co, float color[3], float *area)
{
	ShadeInput *shi= ssamp->shi;
	ShadeResult shr;
	float /* texfac,*/ /* UNUSED */ orthoarea, nor[3], alpha, sx, sy;

	/* cache for shadow */
	shi->samplenr= R.shadowsamplenr[shi->thread]++;
	
	if (quad) 
		shade_input_set_triangle_i(shi, obi, vlr, 0, 2, 3);
	else
		shade_input_set_triangle_i(shi, obi, vlr, 0, 1, 2);

	/* center pixel */
	sx = x + 0.5f;
	sy = y + 0.5f;

	/* we estimate the area here using shi->dxco and shi->dyco. we need to
	 * enabled shi->osatex these are filled. we compute two areas, one with
	 * the normal pointed at the camera and one with the original normal, and
	 * then clamp to avoid a too large contribution from a single pixel */
	shi->osatex= 1;

	copy_v3_v3(nor, shi->facenor);
	calc_view_vector(shi->facenor, sx, sy);
	normalize_v3(shi->facenor);
	shade_input_set_viewco(shi, x, y, sx, sy, z);
	orthoarea= len_v3(shi->dxco)*len_v3(shi->dyco);

	copy_v3_v3(shi->facenor, nor);
	shade_input_set_viewco(shi, x, y, sx, sy, z);
	*area = min_ff(len_v3(shi->dxco) * len_v3(shi->dyco), 2.0f * orthoarea);

	shade_input_set_uv(shi);
	shade_input_set_normals(shi);

	/* we don't want flipped normals, they screw up back scattering */
	if (shi->flippednor)
		shade_input_flip_normals(shi);

	/* not a pretty solution, but fixes common cases */
	if (shi->obr->ob && shi->obr->ob->transflag & OB_NEG_SCALE) {
		negate_v3(shi->vn);
		negate_v3(shi->vno);
		negate_v3(shi->nmapnorm);
	}

	/* if nodetree, use the material that we are currently preprocessing
	 * instead of the node material */
	if (shi->mat->nodetree && shi->mat->use_nodes)
		shi->mat= mat;

	/* init material vars */
	shade_input_init_material(shi);
	
	/* render */
	shade_input_set_shade_texco(shi);
	
	shade_samples_do_AO(ssamp);
	shade_material_loop(shi, &shr);
	
	copy_v3_v3(co, shi->co);
	copy_v3_v3(color, shr.combined);

	/* texture blending */
	/* texfac= shi->mat->sss_texfac; */ /* UNUSED */

	alpha= shr.combined[3];
	*area *= alpha;
}

static void zbufshade_sss_free(RenderPart *pa)
{
#if 0
	MEM_freeN(pa->rectall); pa->rectall= NULL;
	freeps(&handle.psmlist);
#else
	MEM_freeN(pa->rectz); pa->rectz= NULL;
	MEM_freeN(pa->rectp); pa->rectp= NULL;
	MEM_freeN(pa->recto); pa->recto= NULL;
	MEM_freeN(pa->rectbackz); pa->rectbackz= NULL;
	MEM_freeN(pa->rectbackp); pa->rectbackp= NULL;
	MEM_freeN(pa->rectbacko); pa->rectbacko= NULL;
#endif
}

void zbufshade_sss_tile(RenderPart *pa)
{
	Render *re= &R;
	ShadeSample ssamp;
	ZBufSSSHandle handle;
	RenderResult *rr= pa->result;
	RenderLayer *rl;
	VlakRen *vlr;
	Material *mat= re->sss_mat;
	float (*co)[3], (*color)[3], *area, *fcol;
	int x, y, seed, quad, totpoint, display = !(re->r.scemode & (R_BUTS_PREVIEW|R_VIEWPORT_PREVIEW));
	int *ro, *rz, *rp, *rbo, *rbz, *rbp, lay;
#if 0
	PixStr *ps;
	intptr_t *rs;
	int z;
#endif

	/* setup pixelstr list and buffer for zbuffering */
	handle.pa= pa;
	handle.totps= 0;

#if 0
	handle.psmlist.first= handle.psmlist.last= NULL;
	addpsmain(&handle.psmlist);

	pa->rectall= MEM_callocN(sizeof(intptr_t)*pa->rectx*pa->recty+4, "rectall");
#else
	pa->recto= MEM_mallocN(sizeof(int)*pa->rectx*pa->recty, "recto");
	pa->rectp= MEM_mallocN(sizeof(int)*pa->rectx*pa->recty, "rectp");
	pa->rectz= MEM_mallocN(sizeof(int)*pa->rectx*pa->recty, "rectz");
	pa->rectbacko= MEM_mallocN(sizeof(int)*pa->rectx*pa->recty, "rectbacko");
	pa->rectbackp= MEM_mallocN(sizeof(int)*pa->rectx*pa->recty, "rectbackp");
	pa->rectbackz= MEM_mallocN(sizeof(int)*pa->rectx*pa->recty, "rectbackz");
#endif

	/* setup shade sample with correct passes */
	memset(&ssamp, 0, sizeof(ssamp));
	shade_sample_initialize(&ssamp, pa, rr->layers.first);
	ssamp.tot= 1;
	
	for (rl=rr->layers.first; rl; rl=rl->next) {
		ssamp.shi[0].lay |= rl->lay;
		ssamp.shi[0].layflag |= rl->layflag;
		ssamp.shi[0].passflag |= rl->passflag;
		ssamp.shi[0].combinedflag |= ~rl->pass_xor;
	}

	rl= rr->layers.first;
	ssamp.shi[0].passflag |= SCE_PASS_RGBA|SCE_PASS_COMBINED;
	ssamp.shi[0].combinedflag &= ~(SCE_PASS_SPEC);
	ssamp.shi[0].mat_override= NULL;
	ssamp.shi[0].light_override= NULL;
	lay= ssamp.shi[0].lay;

	/* create the pixelstrs to be used later */
	zbuffer_sss(pa, lay, &handle, addps_sss);

	if (handle.totps==0) {
		zbufshade_sss_free(pa);
		return;
	}
	
	fcol= rl->rectf;

	co= MEM_mallocN(sizeof(float)*3*handle.totps, "SSSCo");
	color= MEM_mallocN(sizeof(float)*3*handle.totps, "SSSColor");
	area= MEM_mallocN(sizeof(float)*handle.totps, "SSSArea");

#if 0
	/* create ISB (does not work currently!) */
	if (re->r.mode & R_SHADOW)
		ISB_create(pa, NULL);
#endif

	if (display) {
		/* initialize scanline updates for main thread */
		rr->renrect.ymin = 0;
		rr->renlay= rl;
	}
	
	seed= pa->rectx*pa->disprect.ymin;
#if 0
	rs= pa->rectall;
#else
	rz= pa->rectz;
	rp= pa->rectp;
	ro= pa->recto;
	rbz= pa->rectbackz;
	rbp= pa->rectbackp;
	rbo= pa->rectbacko;
#endif
	totpoint= 0;

	for (y=pa->disprect.ymin; y<pa->disprect.ymax; y++, rr->renrect.ymax++) {
		for (x=pa->disprect.xmin; x<pa->disprect.xmax; x++, fcol+=4) {
			/* per pixel fixed seed */
			BLI_thread_srandom(pa->thread, seed++);
			
#if 0
			if (rs) {
				/* for each sample in this pixel, shade it */
				for (ps = (PixStr *)(*rs); ps; ps=ps->next) {
					ObjectInstanceRen *obi= &re->objectinstance[ps->obi];
					ObjectRen *obr= obi->obr;
					vlr= RE_findOrAddVlak(obr, (ps->facenr-1) & RE_QUAD_MASK);
					quad= (ps->facenr & RE_QUAD_OFFS);
					z= ps->z;

					shade_sample_sss(&ssamp, mat, obi, vlr, quad, x, y, z,
						co[totpoint], color[totpoint], &area[totpoint]);

					totpoint++;

					add_v3_v3(fcol, color);
					fcol[3]= 1.0f;
				}

				rs++;
			}
#else
			if (rp) {
				if (*rp != 0) {
					ObjectInstanceRen *obi= &re->objectinstance[*ro];
					ObjectRen *obr= obi->obr;

					/* shade front */
					vlr= RE_findOrAddVlak(obr, (*rp-1) & RE_QUAD_MASK);
					quad= ((*rp) & RE_QUAD_OFFS);

					shade_sample_sss(&ssamp, mat, obi, vlr, quad, x, y, *rz,
						co[totpoint], color[totpoint], &area[totpoint]);
					
					add_v3_v3(fcol, color[totpoint]);
					fcol[3]= 1.0f;
					totpoint++;
				}

				rp++; rz++; ro++;
			}

			if (rbp) {
				if (*rbp != 0 && !(*rbp == *(rp-1) && *rbo == *(ro-1))) {
					ObjectInstanceRen *obi= &re->objectinstance[*rbo];
					ObjectRen *obr= obi->obr;

					/* shade back */
					vlr= RE_findOrAddVlak(obr, (*rbp-1) & RE_QUAD_MASK);
					quad= ((*rbp) & RE_QUAD_OFFS);

					shade_sample_sss(&ssamp, mat, obi, vlr, quad, x, y, *rbz,
						co[totpoint], color[totpoint], &area[totpoint]);
					
					/* to indicate this is a back sample */
					area[totpoint]= -area[totpoint];

					add_v3_v3(fcol, color[totpoint]);
					fcol[3]= 1.0f;
					totpoint++;
				}

				rbz++; rbp++; rbo++;
			}
#endif
		}

		if (y&1)
			if (re->test_break(re->tbh)) break; 
	}

	/* note: after adding we do not free these arrays, sss keeps them */
	if (totpoint > 0) {
		sss_add_points(re, co, color, area, totpoint);
	}
	else {
		MEM_freeN(co);
		MEM_freeN(color);
		MEM_freeN(area);
	}
	
#if 0
	if (re->r.mode & R_SHADOW)
		ISB_free(pa);
#endif
		
	if (display) {
		/* display active layer */
		rr->renrect.ymin=rr->renrect.ymax = 0;
		rr->renlay= render_get_active_layer(&R, rr);
	}
	
	zbufshade_sss_free(pa);
}

/* ------------------------------------------------------------------------ */

static void renderhalo_post(RenderResult *rr, float *rectf, HaloRen *har)	/* postprocess version */
{
	float dist, xsq, ysq, xn, yn, colf[4], *rectft, *rtf;
	float haloxs, haloys;
	int minx, maxx, miny, maxy, x, y;

	/* calculate the disprect mapped coordinate for halo. note: rectx is disprect corrected */
	haloxs= har->xs - R.disprect.xmin;
	haloys= har->ys - R.disprect.ymin;
	
	har->miny= miny= haloys - har->rad/R.ycor;
	har->maxy= maxy= haloys + har->rad/R.ycor;
	
	if (maxy < 0) {
		/* pass */
	}
	else if (rr->recty < miny) {
		/* pass */
	}
	else {
		minx = floor(haloxs - har->rad);
		maxx = ceil(haloxs + har->rad);
			
		if (maxx < 0) {
			/* pass */
		}
		else if (rr->rectx < minx) {
			/* pass */
		}
		else {
			if (minx<0) minx= 0;
			if (maxx>=rr->rectx) maxx= rr->rectx-1;
			if (miny<0) miny= 0;
			if (maxy>rr->recty) maxy= rr->recty;
	
			rectft= rectf+ 4*rr->rectx*miny;

			for (y=miny; y<maxy; y++) {
	
				rtf= rectft+4*minx;
				
				yn= (y - haloys)*R.ycor;
				ysq= yn*yn;
				
				for (x=minx; x<=maxx; x++) {
					xn= x - haloxs;
					xsq= xn*xn;
					dist= xsq+ysq;
					if (dist<har->radsq) {
						
						if (shadeHaloFloat(har, colf, 0x7FFFFF, dist, xn, yn, har->flarec))
							addalphaAddfacFloat(rtf, colf, har->add);
					}
					rtf+=4;
				}
	
				rectft+= 4*rr->rectx;
				
				if (R.test_break(R.tbh)) break; 
			}
		}
	}
} 
/* ------------------------------------------------------------------------ */

static void renderflare(RenderResult *rr, float *rectf, HaloRen *har)
{
	extern const float hashvectf[];
	HaloRen fla;
	Material *ma;
	const float *rc;
	float rad, alfa, visifac, vec[3];
	int b, type;
	
	fla= *har;
	fla.linec= fla.ringc= fla.flarec= 0;
	
	rad= har->rad;
	alfa= har->alfa;
	
	visifac= R.ycor*(har->pixels);
	/* all radials added / r^3 == 1.0f! */
	visifac /= (har->rad*har->rad*har->rad);
	visifac*= visifac;

	ma= har->mat;
	
	/* first halo: just do */
	
	har->rad= rad*ma->flaresize*visifac;
	har->radsq= har->rad*har->rad;
	har->zs= fla.zs= 0;
	
	har->alfa= alfa*visifac;

	renderhalo_post(rr, rectf, har);
	
	/* next halo's: the flares */
	rc= hashvectf + ma->seed2;
	
	for (b=1; b<har->flarec; b++) {
		
		fla.r= fabs(rc[0]);
		fla.g= fabs(rc[1]);
		fla.b= fabs(rc[2]);
		fla.alfa= ma->flareboost*fabsf(alfa*visifac*rc[3]);
		fla.hard= 20.0f + fabsf(70.0f*rc[7]);
		fla.tex= 0;
		
		type= (int)(fabs(3.9f*rc[6]));

		fla.rad= ma->subsize*sqrtf(fabs(2.0f*har->rad*rc[4]));
		
		if (type==3) {
			fla.rad*= 3.0f;
			fla.rad+= R.rectx/10;
		}
		
		fla.radsq= fla.rad*fla.rad;
		
		vec[0]= 1.4f*rc[5]*(har->xs-R.winx/2);
		vec[1]= 1.4f*rc[5]*(har->ys-R.winy/2);
		vec[2]= 32.0f*sqrtf(vec[0]*vec[0] + vec[1]*vec[1] + 1.0f);
		
		fla.xs= R.winx/2 + vec[0] + (1.2f+rc[8])*R.rectx*vec[0]/vec[2];
		fla.ys= R.winy/2 + vec[1] + (1.2f+rc[8])*R.rectx*vec[1]/vec[2];

		if (R.flag & R_SEC_FIELD) {
			if (R.r.mode & R_ODDFIELD) fla.ys += 0.5f;
			else fla.ys -= 0.5f;
		}
		if (type & 1) fla.type= HA_FLARECIRC;
		else fla.type= 0;
		renderhalo_post(rr, rectf, &fla);

		fla.alfa*= 0.5f;
		if (type & 2) fla.type= HA_FLARECIRC;
		else fla.type= 0;
		renderhalo_post(rr, rectf, &fla);
		
		rc+= 7;
	}
}

/* needs recode... integrate this better! */
void add_halo_flare(Render *re)
{
	RenderResult *rr= re->result;
	RenderLayer *rl;
	HaloRen *har;
	int a, mode;
	
	/* for now, we get the first renderlayer in list with halos set */
	for (rl= rr->layers.first; rl; rl= rl->next) {
		int do_draw = FALSE;
		
		if ((rl->layflag & SCE_LAY_HALO) == 0)
			continue;
		if (rl->rectf==NULL)
			continue;
		
		mode= R.r.mode;
		R.r.mode &= ~R_PANORAMA;
		
		project_renderdata(&R, projectverto, 0, 0, 0);
		
		for (a=0; a<R.tothalo; a++) {
			har= R.sortedhalos[a];
			
			if (har->flarec && (har->lay & rl->lay)) {
				do_draw = TRUE;
				renderflare(rr, rl->rectf, har);
			}
		}
		
		if (do_draw) {
			/* weak... the display callback wants an active renderlayer pointer... */
			rr->renlay= rl;
			re->display_update(re->duh, rr, NULL);
		}

		R.r.mode= mode;
	}
}

