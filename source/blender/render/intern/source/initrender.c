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

#include "BLI_math.h"
#include "BLI_blenlib.h"
#include "BLI_utildefines.h"

#include "DNA_camera_types.h"

#include "BKE_camera.h"

/* this module */
#include "renderpipeline.h"
#include "render_types.h"

/* Own includes */
#include "initrender.h"


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
		{
			const float two_gaussfac2 = 2.0f * gaussfac * gaussfac;
			x *= 3.0f * gaussfac;
			return 1.0f / sqrtf((float)M_PI * two_gaussfac2) * expf(-x*x / two_gaussfac2);
		}

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

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
struct Object *RE_GetCamera(Render *re)
{
	Object *camera = re->camera_override ? re->camera_override : re->scene->camera;
	return BKE_camera_multiview_render(re->scene, camera, re->viewname);
}

static void re_camera_params_get(Render *re, CameraParams *params, Object *cam_ob)
{
	copy_m4_m4(re->winmat, params->winmat);

	re->clipsta = params->clipsta;
	re->clipend = params->clipend;

	re->viewplane = params->viewplane;
}

void RE_SetOverrideCamera(Render *re, Object *camera)
{
	re->camera_override = camera;
}

static void re_camera_params_stereo3d(Render *re, CameraParams *params, Object *cam_ob)
{
	BKE_camera_multiview_params(&re->r, params, cam_ob, re->viewname);
}

/* call this after InitState() */
/* per render, there's one persistent viewplane. Parts will set their own viewplanes */
void RE_SetCamera(Render *re, Object *cam_ob)
{
	CameraParams params;

	/* setup parameters */
	BKE_camera_params_init(&params);
	BKE_camera_params_from_object(&params, cam_ob);
	re_camera_params_stereo3d(re, &params, cam_ob);

	/* compute matrix, viewplane, .. */
	BKE_camera_params_compute_viewplane(&params, re->winx, re->winy, re->r.xasp, re->r.yasp);
	BKE_camera_params_compute_matrix(&params);

	/* extract results */
	re_camera_params_get(re, &params, cam_ob);
}

void RE_GetCameraWindow(struct Render *re, struct Object *camera, int frame, float mat[4][4])
{
	re->r.cfra = frame;
	RE_SetCamera(re, camera);
	copy_m4_m4(mat, re->winmat);
}

void RE_GetCameraModelMatrix(Render *re, struct Object *camera, float r_mat[4][4])
{
	BKE_camera_multiview_model_matrix(&re->r, camera, re->viewname, r_mat);
}

/* ~~~~~~~~~~~~~~~~ part (tile) calculus ~~~~~~~~~~~~~~~~~~~~~~ */


void RE_parts_free(Render *re)
{
	BLI_freelistN(&re->parts);
}

void RE_parts_clamp(Render *re)
{
	/* part size */
	re->partx = max_ii(1, min_ii(re->r.tilex, re->rectx));
	re->party = max_ii(1, min_ii(re->r.tiley, re->recty));
}

void RE_parts_init(Render *re)
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

			pa->disprect = disprect;
			pa->rectx = rectx;
			pa->recty = recty;

			BLI_addtail(&re->parts, pa);
			re->i.totpart++;
		}
	}
}
