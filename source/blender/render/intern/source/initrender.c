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
 * Contributors: 2004/2005/2006 Blender Foundation, full recode
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/render/intern/source/initrender.c
 *  \ingroup render
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
#include "BLI_utildefines.h"

#include "DNA_camera_types.h"
#include "DNA_group_types.h"
#include "DNA_image_types.h"
#include "DNA_lamp_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"


#include "BKE_camera.h"
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
	static float jit[32][2];        /* simple caching */
	static float mblur_jit[32][2];  /* simple caching */
	static int lastjit = 0;
	static int last_mblur_jit = 0;
	
	if (lastjit != re->r.osa || last_mblur_jit != re->r.mblur_samples) {
		memset(jit, 0, sizeof(jit));
		BLI_jitter_init(jit[0], re->r.osa);
		
		memset(mblur_jit, 0, sizeof(mblur_jit));
		BLI_jitter_init(mblur_jit[0], re->r.mblur_samples);
	}
	
	lastjit = re->r.osa;
	memcpy(re->jit, jit, sizeof(jit));
	
	last_mblur_jit = re->r.mblur_samples;
	memcpy(re->mblur_jit, mblur_jit, sizeof(mblur_jit));
}


/* ****************** MASKS and LUTS **************** */

static float filt_quadratic(float x)
{
	if (x <  0.0f) x = -x;
	if (x < 0.5f) return 0.75f - (x * x);
	if (x < 1.5f) return 0.50f * (x - 1.5f) * (x - 1.5f);
	return 0.0f;
}


static float filt_cubic(float x)
{
	float x2 = x * x;
	
	if (x <  0.0f) x = -x;
	
	if (x < 1.0f) return 0.5f * x * x2 - x2 + 2.0f / 3.0f;
	if (x < 2.0f) return (2.0f - x) * (2.0f - x) * (2.0f - x) / 6.0f;
	return 0.0f;
}


static float filt_catrom(float x)
{
	float x2 = x * x;
	
	if (x <  0.0f) x = -x;
	if (x < 1.0f) return  1.5f * x2 * x - 2.5f * x2 + 1.0f;
	if (x < 2.0f) return -0.5f * x2 * x + 2.5f * x2 - 4.0f * x + 2.0f;
	return 0.0f;
}

static float filt_mitchell(float x) /* Mitchell & Netravali's two-param cubic */
{
	float b = 1.0f / 3.0f, c = 1.0f / 3.0f;
	float p0 = (  6.0f -  2.0f * b)             / 6.0f;
	float p2 = (-18.0f + 12.0f * b +  6.0f * c) / 6.0f;
	float p3 = ( 12.0f -  9.0f * b -  6.0f * c) / 6.0f;
	float q0 = (          8.0f * b + 24.0f * c) / 6.0f;
	float q1 = (        -12.0f * b - 48.0f * c) / 6.0f;
	float q2 = (          6.0f * b + 30.0f * c) / 6.0f;
	float q3 = (                -b -  6.0f * c) / 6.0f;

	if (x < -2.0f) return 0.0f;
	if (x < -1.0f) return (q0 - x * (q1 - x * (q2 - x * q3)));
	if (x < 0.0f) return (p0 + x * x * (p2 - x * p3));
	if (x < 1.0f) return (p0 + x * x * (p2 + x * p3));
	if (x < 2.0f) return (q0 + x * (q1 + x * (q2 + x * q3)));
	return 0.0f;
}

/* x ranges from -1 to 1 */
float RE_filter_value(int type, float x)
{
	float gaussfac = 1.6f;
	
	x = ABS(x);
	
	switch (type) {
		case R_FILTER_BOX:
			if (x > 1.0f) return 0.0f;
			return 1.0f;
			
		case R_FILTER_TENT:
			if (x > 1.0f) return 0.0f;
			return 1.0f - x;
			
		case R_FILTER_GAUSS:
			x *= gaussfac;
			return (1.0f / expf(x * x) - 1.0f / expf(gaussfac * gaussfac * 2.25f));
			
		case R_FILTER_MITCH:
			return filt_mitchell(x * gaussfac);
			
		case R_FILTER_QUAD:
			return filt_quadratic(x * gaussfac);
			
		case R_FILTER_CUBIC:
			return filt_cubic(x * gaussfac);
			
		case R_FILTER_CATROM:
			return filt_catrom(x * gaussfac);
	}
	return 0.0f;
}

static float calc_weight(Render *re, float *weight, int i, int j)
{
	float x, y, dist, totw = 0.0;
	int a;

	for (a = 0; a < re->osa; a++) {
		x = re->jit[a][0] + i;
		y = re->jit[a][1] + j;
		dist = sqrt(x * x + y * y);

		weight[a] = 0.0;

		/* Weighting choices */
		switch (re->r.filtertype) {
			case R_FILTER_BOX:
				if (i == 0 && j == 0) weight[a] = 1.0;
				break;
			
			case R_FILTER_TENT:
				if (dist < re->r.gauss)
					weight[a] = re->r.gauss - dist;
				break;
			
			case R_FILTER_GAUSS:
				x = dist * re->r.gauss;
				weight[a] = (1.0f / expf(x * x) - 1.0f / expf(re->r.gauss * re->r.gauss * 2.25f));
				break;
		
			case R_FILTER_MITCH:
				weight[a] = filt_mitchell(dist * re->r.gauss);
				break;
		
			case R_FILTER_QUAD:
				weight[a] = filt_quadratic(dist * re->r.gauss);
				break;
			
			case R_FILTER_CUBIC:
				weight[a] = filt_cubic(dist * re->r.gauss);
				break;
			
			case R_FILTER_CATROM:
				weight[a] = filt_catrom(dist * re->r.gauss);
				break;
			
		}
		
		totw += weight[a];

	}
	return totw;
}

void free_sample_tables(Render *re)
{
	int a;
	
	if (re->samples) {
		for (a = 0; a < 9; a++) {
			MEM_freeN(re->samples->fmask1[a]);
			MEM_freeN(re->samples->fmask2[a]);
		}
		
		MEM_freeN(re->samples->centmask);
		MEM_freeN(re->samples);
		re->samples = NULL;
	}
}

/* based on settings in render, it makes the lookup tables */
void make_sample_tables(Render *re)
{
	static int firsttime = 1;
	SampleTables *st;
	float flweight[32];
	float weight[32], totw, val, *fpx1, *fpx2, *fpy1, *fpy2, *m3, *m4;
	int i, j, a, centmasksize;

	/* optimization tables, only once */
	if (firsttime) {
		firsttime = 0;
	}
	
	free_sample_tables(re);
	
	init_render_jit(re);    /* needed for mblur too */
	
	if (re->osa == 0) {
		/* just prevents cpu cycles for larger render and copying */
		re->r.filtertype = 0;
		return;
	}
	
	st = re->samples = MEM_callocN(sizeof(SampleTables), "sample tables");
	
	for (a = 0; a < 9; a++) {
		st->fmask1[a] = MEM_callocN(256 * sizeof(float), "initfilt");
		st->fmask2[a] = MEM_callocN(256 * sizeof(float), "initfilt");
	}
	for (a = 0; a < 256; a++) {
		st->cmask[a] = 0;
		if (a &   1) st->cmask[a]++;
		if (a &   2) st->cmask[a]++;
		if (a &   4) st->cmask[a]++;
		if (a &   8) st->cmask[a]++;
		if (a &  16) st->cmask[a]++;
		if (a &  32) st->cmask[a]++;
		if (a &  64) st->cmask[a]++;
		if (a & 128) st->cmask[a]++;
	}
	
	centmasksize = (1 << re->osa);
	st->centmask = MEM_mallocN(centmasksize, "Initfilt3");
	
	for (a = 0; a < 16; a++) {
		st->centLut[a] = -0.45f + ((float)a) / 16.0f;
	}

	/* calculate totw */
	totw = 0.0;
	for (j = -1; j < 2; j++) {
		for (i = -1; i < 2; i++) {
			totw += calc_weight(re, weight, i, j);
		}
	}

	for (j = -1; j < 2; j++) {
		for (i = -1; i < 2; i++) {
			/* calculate using jit, with offset the weights */

			memset(weight, 0, sizeof(weight));
			calc_weight(re, weight, i, j);

			for (a = 0; a < 16; a++) flweight[a] = weight[a] * (1.0f / totw);

			m3 = st->fmask1[3 * (j + 1) + i + 1];
			m4 = st->fmask2[3 * (j + 1) + i + 1];

			for (a = 0; a < 256; a++) {
				if (a &   1) {
					m3[a] += flweight[0];
					m4[a] += flweight[8];
				}
				if (a &   2) {
					m3[a] += flweight[1];
					m4[a] += flweight[9];
				}
				if (a &   4) {
					m3[a] += flweight[2];
					m4[a] += flweight[10];
				}
				if (a &   8) {
					m3[a] += flweight[3];
					m4[a] += flweight[11];
				}
				if (a &  16) {
					m3[a] += flweight[4];
					m4[a] += flweight[12];
				}
				if (a &  32) {
					m3[a] += flweight[5];
					m4[a] += flweight[13];
				}
				if (a &  64) {
					m3[a] += flweight[6];
					m4[a] += flweight[14];
				}
				if (a & 128) {
					m3[a] += flweight[7];
					m4[a] += flweight[15];
				}
			}
		}
	}

	/* centmask: the correct subpixel offset per mask */

	fpx1 = MEM_mallocN(256 * sizeof(float), "initgauss4");
	fpx2 = MEM_mallocN(256 * sizeof(float), "initgauss4");
	fpy1 = MEM_mallocN(256 * sizeof(float), "initgauss4");
	fpy2 = MEM_mallocN(256 * sizeof(float), "initgauss4");
	for (a = 0; a < 256; a++) {
		fpx1[a] = fpx2[a] = 0.0;
		fpy1[a] = fpy2[a] = 0.0;
		if (a & 1) {
			fpx1[a] += re->jit[0][0];
			fpy1[a] += re->jit[0][1];
			fpx2[a] += re->jit[8][0];
			fpy2[a] += re->jit[8][1];
		}
		if (a & 2) {
			fpx1[a] += re->jit[1][0];
			fpy1[a] += re->jit[1][1];
			fpx2[a] += re->jit[9][0];
			fpy2[a] += re->jit[9][1];
		}
		if (a & 4) {
			fpx1[a] += re->jit[2][0];
			fpy1[a] += re->jit[2][1];
			fpx2[a] += re->jit[10][0];
			fpy2[a] += re->jit[10][1];
		}
		if (a & 8) {
			fpx1[a] += re->jit[3][0];
			fpy1[a] += re->jit[3][1];
			fpx2[a] += re->jit[11][0];
			fpy2[a] += re->jit[11][1];
		}
		if (a & 16) {
			fpx1[a] += re->jit[4][0];
			fpy1[a] += re->jit[4][1];
			fpx2[a] += re->jit[12][0];
			fpy2[a] += re->jit[12][1];
		}
		if (a & 32) {
			fpx1[a] += re->jit[5][0];
			fpy1[a] += re->jit[5][1];
			fpx2[a] += re->jit[13][0];
			fpy2[a] += re->jit[13][1];
		}
		if (a & 64) {
			fpx1[a] += re->jit[6][0];
			fpy1[a] += re->jit[6][1];
			fpx2[a] += re->jit[14][0];
			fpy2[a] += re->jit[14][1];
		}
		if (a & 128) {
			fpx1[a] += re->jit[7][0];
			fpy1[a] += re->jit[7][1];
			fpx2[a] += re->jit[15][0];
			fpy2[a] += re->jit[15][1];
		}
	}

	for (a = centmasksize - 1; a > 0; a--) {
		val = st->cmask[a & 255] + st->cmask[a >> 8];
		i = 8 + (15.9f * (fpy1[a & 255] + fpy2[a >> 8]) / val);
		CLAMP(i, 0, 15);
		j = 8 + (15.9f * (fpx1[a & 255] + fpx2[a >> 8]) / val);
		CLAMP(j, 0, 15);
		i = j + (i << 4);
		st->centmask[a] = i;
	}

	MEM_freeN(fpx1);
	MEM_freeN(fpx2);
	MEM_freeN(fpy1);
	MEM_freeN(fpy2);
}


/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

struct Object *RE_GetCamera(Render *re)
{
	return re->camera_override ? re->camera_override : re->scene->camera;
}

static void re_camera_params_get(Render *re, CameraParams *params, Object *cam_ob)
{
	copy_m4_m4(re->winmat, params->winmat);

	re->clipsta = params->clipsta;
	re->clipend = params->clipend;

	re->ycor = params->ycor;
	re->viewdx = params->viewdx;
	re->viewdy = params->viewdy;
	re->viewplane = params->viewplane;

	BKE_camera_object_mode(&re->r, cam_ob);
}

void RE_SetEnvmapCamera(Render *re, Object *cam_ob, float viewscale, float clipsta, float clipend)
{
	CameraParams params;

	/* setup parameters */
	BKE_camera_params_init(&params);
	BKE_camera_params_from_object(&params, cam_ob);

	params.lens = 16.0f * viewscale;
	params.sensor_x = 32.0f;
	params.sensor_y = 32.0f;
	params.sensor_fit = CAMERA_SENSOR_FIT_AUTO;
	params.clipsta = clipsta;
	params.clipend = clipend;
	
	/* compute matrix, viewplane, .. */
	BKE_camera_params_compute_viewplane(&params, re->winx, re->winy, 1.0f, 1.0f);
	BKE_camera_params_compute_matrix(&params);

	/* extract results */
	re_camera_params_get(re, &params, cam_ob);
}

/* call this after InitState() */
/* per render, there's one persistent viewplane. Parts will set their own viewplanes */
void RE_SetCamera(Render *re, Object *cam_ob)
{
	CameraParams params;

	/* setup parameters */
	BKE_camera_params_init(&params);
	BKE_camera_params_from_object(&params, cam_ob);

	params.use_fields = (re->r.mode & R_FIELDS);
	params.field_second = (re->flag & R_SEC_FIELD);
	params.field_odd = (re->r.mode & R_ODDFIELD);

	/* compute matrix, viewplane, .. */
	BKE_camera_params_compute_viewplane(&params, re->winx, re->winy, re->r.xasp, re->r.yasp);
	BKE_camera_params_compute_matrix(&params);

	/* extract results */
	re_camera_params_get(re, &params, cam_ob);
}

void RE_SetPixelSize(Render *re, float pixsize)
{
	re->viewdx = pixsize;
	re->viewdy = re->ycor * pixsize;
}

void RE_GetCameraWindow(struct Render *re, struct Object *camera, int frame, float mat[4][4])
{
	re->r.cfra = frame;
	RE_SetCamera(re, camera);
	copy_m4_m4(mat, re->winmat);
}

/* ~~~~~~~~~~~~~~~~ part (tile) calculus ~~~~~~~~~~~~~~~~~~~~~~ */


void RE_parts_free(Render *re)
{
	RenderPart *part = re->parts.first;
	
	while (part) {
		if (part->rectp) MEM_freeN(part->rectp);
		if (part->rectz) MEM_freeN(part->rectz);
		part = part->next;
	}
	BLI_freelistN(&re->parts);
}

void RE_parts_clamp(Render *re)
{
	/* part size */
	re->partx = max_ii(1, min_ii(re->r.tilex, re->rectx));
	re->party = max_ii(1, min_ii(re->r.tiley, re->recty));
}

void RE_parts_init(Render *re, int do_crop)
{
	int nr, xd, yd, partx, party, xparts, yparts;
	int xminb, xmaxb, yminb, ymaxb;
	
	RE_parts_free(re);
	
	/* this is render info for caller, is not reset when parts are freed! */
	re->i.totpart = 0;
	re->i.curpart = 0;
	re->i.partsdone = 0;
	
	/* just for readable code.. */
	xminb = re->disprect.xmin;
	yminb = re->disprect.ymin;
	xmaxb = re->disprect.xmax;
	ymaxb = re->disprect.ymax;
	
	RE_parts_clamp(re);

	partx = re->partx;
	party = re->party;
	/* part count */
	xparts = (re->rectx + partx - 1) / partx;
	yparts = (re->recty + party - 1) / party;
	
	/* calculate rotation factor of 1 pixel */
	if (re->r.mode & R_PANORAMA)
		re->panophi = panorama_pixel_rot(re);
	
	for (nr = 0; nr < xparts * yparts; nr++) {
		rcti disprect;
		int rectx, recty;
		
		xd = (nr % xparts);
		yd = (nr - xd) / xparts;
		
		disprect.xmin = xminb + xd * partx;
		disprect.ymin = yminb + yd * party;
		
		/* ensure we cover the entire picture, so last parts go to end */
		if (xd < xparts - 1) {
			disprect.xmax = disprect.xmin + partx;
			if (disprect.xmax > xmaxb)
				disprect.xmax = xmaxb;
		}
		else disprect.xmax = xmaxb;
		
		if (yd < yparts - 1) {
			disprect.ymax = disprect.ymin + party;
			if (disprect.ymax > ymaxb)
				disprect.ymax = ymaxb;
		}
		else disprect.ymax = ymaxb;
		
		rectx = BLI_rcti_size_x(&disprect);
		recty = BLI_rcti_size_y(&disprect);
		
		/* so, now can we add this part? */
		if (rectx > 0 && recty > 0) {
			RenderPart *pa = MEM_callocN(sizeof(RenderPart), "new part");
			
			/* Non-box filters need 2 pixels extra to work */
			if (do_crop && (re->r.filtertype || (re->r.mode & R_EDGE))) {
				pa->crop = 2;
				disprect.xmin -= pa->crop;
				disprect.ymin -= pa->crop;
				disprect.xmax += pa->crop;
				disprect.ymax += pa->crop;
				rectx += 2 * pa->crop;
				recty += 2 * pa->crop;
			}
			pa->disprect = disprect;
			pa->rectx = rectx;
			pa->recty = recty;

			BLI_addtail(&re->parts, pa);
			re->i.totpart++;
		}
	}
}



