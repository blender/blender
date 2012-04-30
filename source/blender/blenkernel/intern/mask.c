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
 * The Original Code is Copyright (C) 2012 Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Blender Foundation,
 *                 Sergey Sharybin
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/blenkernel/intern/mask.c
 *  \ingroup bke
 */

#include <stddef.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"
#include "BLI_path_util.h"
#include "BLI_string.h"
#include "BLI_listbase.h"
#include "BLI_math.h"

#include "DNA_mask_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_movieclip_types.h"
#include "DNA_tracking_types.h"

#include "BKE_curve.h"
#include "BKE_global.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_mask.h"
#include "BKE_tracking.h"
#include "BKE_utildefines.h"

/* shapes */

MaskShape *BKE_mask_shape_new(Mask *mask, const char *name)
{
	MaskShape *shape = MEM_callocN(sizeof(MaskShape), "new mask shape");

	if (name && name[0])
		BLI_strncpy(shape->name, name, sizeof(shape->name));
	else
		strcpy(shape->name, "Shape");

	BLI_addtail(&mask->shapes, shape);

	BKE_mask_shape_unique_name(mask, shape);

	mask->tot_shape++;

	return shape;
}

MaskShape *BKE_mask_shape_active(Mask *mask)
{
	return BLI_findlink(&mask->shapes, mask->shapenr);
}

void BKE_mask_shape_active_set(Mask *mask, MaskShape *shape)
{
	int index = BLI_findindex(&mask->shapes, shape);

	if (index >= 0)
		mask->shapenr = index;
	else
		mask->shapenr = 0;
}

void BKE_mask_shape_remove(Mask *mask, MaskShape *shape)
{
	BLI_remlink(&mask->shapes, shape);
	BKE_mask_shape_free(shape);

	mask->tot_shape--;

	if (mask->shapenr >= mask->tot_shape)
		mask->shapenr = mask->tot_shape - 1;
}

void BKE_mask_shape_unique_name(Mask *mask, MaskShape *shape)
{
	BLI_uniquename(&mask->shapes, shape, "Shape", '.', offsetof(MaskShape, name), sizeof(shape->name));
}

/* splines */

MaskSpline *BKE_mask_spline_add(MaskShape *shape)
{
	MaskSpline *spline;

	spline = MEM_callocN(sizeof(MaskSpline), "new shape spline");
	BLI_addtail(&shape->splines, spline);

	/* spline shall have one point at least */
	spline->points = MEM_callocN(sizeof(MaskSplinePoint), "new shape spline point");
	spline->tot_point = 1;

	/* cyclic shapes are more usually used */
	spline->flag |= MASK_SPLINE_CYCLIC;

	spline->weight_interp = MASK_SPLINE_INTERP_LINEAR;

	BKE_mask_parent_init(&spline->parent);

	return spline;
}

int BKE_mask_spline_resolution(MaskSpline *spline)
{
	const float max_segment = 0.01;
	int i, resol = 1;

	for (i = 0; i < spline->tot_point; i++) {
		MaskSplinePoint *point = &spline->points[i];
		MaskSplinePoint *next_point;
		BezTriple *bezt, *next_bezt;
		float a, b, c, len;
		int cur_resol;

		if (i == spline->tot_point - 1) {
			if (spline->flag & MASK_SPLINE_CYCLIC)
				next_point = &spline->points[0];
			else
				break;
		}
		else
			next_point = &spline->points[i + 1];

		bezt = &point->bezt;
		next_bezt = &next_point->bezt;

		a = len_v3v3(bezt->vec[1], bezt->vec[2]);
		b = len_v3v3(bezt->vec[2], next_bezt->vec[0]);
		c = len_v3v3(next_bezt->vec[0], next_bezt->vec[1]);

		len = a + b + c;
		cur_resol = len / max_segment;

		resol = MAX2(resol, cur_resol);
	}

	return resol;
}

int BKE_mask_spline_feather_resolution(MaskSpline *spline)
{
	const float max_segment = 0.005;
	int resol = BKE_mask_spline_resolution(spline);
	float max_jump = 0.0f;
	int i;

	for (i = 0; i < spline->tot_point; i++) {
		MaskSplinePoint *point = &spline->points[i];
		float prev_u, prev_w;
		int j;

		prev_u = 0.0f;
		prev_w = point->bezt.weight;

		for (j = 0; j < point->tot_uw; j++) {
			float jump = fabsf((point->uw[j].w - prev_w) / (point->uw[j].u - prev_u));

			max_jump = MAX2(max_jump, jump);

			prev_u = point->uw[j].u;
			prev_w = point->uw[j].w;
		}
	}

	resol += max_jump / max_segment;

	return resol;
}

float *BKE_mask_spline_differentiate(MaskSpline *spline, int *tot_diff_point)
{
	MaskSplinePoint *point, *prev;
	float *diff_points, *fp;
	int a, len, resol = BKE_mask_spline_resolution(spline);

	if (spline->tot_point <= 1) {
		/* nothing to differentiate */
		*tot_diff_point = 0;
		return NULL;
	}

	/* count */
	len = (spline->tot_point - 1) * resol;

	if (spline->flag & MASK_SPLINE_CYCLIC)
		len += resol;
	else
		len++;

	/* len+1 because of 'forward_diff_bezier' function */
	*tot_diff_point = len;
	diff_points = fp = MEM_callocN((len + 1)*2*sizeof(float), "mask spline vets");

	a = spline->tot_point - 1;
	if (spline->flag & MASK_SPLINE_CYCLIC)
		a++;

	prev = spline->points;
	point = prev + 1;

	while (a--) {
		BezTriple *prevbezt;
		BezTriple *bezt;
		int j;

		if (a==0 && (spline->flag & MASK_SPLINE_CYCLIC))
			point = spline->points;

		prevbezt = &prev->bezt;
		bezt = &point->bezt;

		for (j = 0; j < 2; j++) {
			BKE_curve_forward_diff_bezier(prevbezt->vec[1][j], prevbezt->vec[2][j],
			                              bezt->vec[0][j], bezt->vec[1][j],
			                              fp + j, resol, 2 * sizeof(float));
		}

		fp += 2 * resol;

		if (a==0 && (spline->flag & MASK_SPLINE_CYCLIC)==0) {
			copy_v2_v2(fp, bezt->vec[1]);
		}

		prev = point;
		point++;
	}

	return diff_points;
}

float *BKE_mask_spline_feather_differentiated_points(MaskSpline *spline, float aspx, float aspy,
                                                    int *tot_feather_point)
{
	float *feather, *fp;
	int i, j, tot, resol = BKE_mask_spline_feather_resolution(spline);

	tot = resol * spline->tot_point;
	feather = fp = MEM_callocN(2 * tot * sizeof(float), "mask spline feather diff points");

	for (i = 0; i < spline->tot_point; i++) {
		MaskSplinePoint *point = &spline->points[i];

		for (j = 0; j < resol; j++, fp += 2) {
			float u = (float) j / resol, weight;
			float co[2], n[2];

			BKE_mask_point_segment_co(spline, point, u, co);
			BKE_mask_point_normal(spline, point, aspx, aspy, u, n);
			weight = BKE_mask_point_weight(spline, point, u);

			fp[0] = co[0] + n[0] * weight;
			fp[1] = co[1] + n[1] * weight;
		}
	}

	*tot_feather_point = tot;

	return feather;
}

float *BKE_mask_spline_feather_points(MaskSpline *spline, float aspx, float aspy, int *tot_feather_point)
{
	int i, tot = 0;
	float *feather, *fp;

	/* count */
	for (i = 0; i < spline->tot_point; i++) {
		MaskSplinePoint *point = &spline->points[i];

		tot += point->tot_uw + 1;
	}

	/* create data */
	feather = fp = MEM_callocN(2 * tot * sizeof(float), "mask spline feather points");

	for (i = 0; i < spline->tot_point; i++) {
		MaskSplinePoint *point = &spline->points[i];
		BezTriple *bezt = &point->bezt;
		float weight, n[2];
		int j;

		BKE_mask_point_normal(spline, point, aspx, aspy, 0.0f, n);
		weight = BKE_mask_point_weight(spline, point, 0.0f);

		fp[0] = bezt->vec[1][0] + n[0] * weight;
		fp[1] = bezt->vec[1][1] + n[1] * weight;
		fp += 2;

		for (j = 0; j < point->tot_uw; j++) {
			float u = point->uw[j].u;
			float co[2];

			BKE_mask_point_segment_co(spline, point, u, co);
			BKE_mask_point_normal(spline, point, aspx, aspy, u, n);
			weight = BKE_mask_point_weight(spline, point, u);

			fp[0] = co[0] + n[0] * weight;
			fp[1] = co[1] + n[1] * weight;

			fp += 2;
		}
	}

	*tot_feather_point = tot;

	return feather;
}

/* point */

int BKE_mask_point_has_handle(MaskSplinePoint *point)
{
	BezTriple *bezt = &point->bezt;

	return bezt->h1 == HD_ALIGN;
}

void BKE_mask_point_handle(MaskSplinePoint *point, float aspx, float aspy, float handle[2])
{
	float vec[2];

	sub_v2_v2v2(vec, point->bezt.vec[0], point->bezt.vec[1]);

	vec[0] *= aspx;
	vec[1] *= aspy;

	handle[0] = (point->bezt.vec[1][0]*aspx + vec[1]) / aspx;
	handle[1] = (point->bezt.vec[1][1]*aspy - vec[0]) / aspy;
}

void BKE_mask_point_set_handle(MaskSplinePoint *point, float loc[2], int keep_direction, float aspx, float aspy,
                               float orig_handle[2], float orig_vec[3][3])
{
	BezTriple *bezt = &point->bezt;
	float v1[2], v2[2], vec[2];

	if (keep_direction) {
		sub_v2_v2v2(v1, loc, orig_vec[1]);
		sub_v2_v2v2(v2, orig_handle, orig_vec[1]);

		v1[0] *= aspx;
		v1[1] *= aspy;
		v2[0] *= aspx;
		v2[1] *= aspx;

		project_v2_v2v2(vec, v1, v2);

		if (dot_v2v2(v2, vec) > 0) {
			float len = len_v2(vec);

			sub_v2_v2v2(v1, orig_vec[0], orig_vec[1]);

			v1[0] *= aspx;
			v1[1] *= aspy;

			mul_v2_fl(v1, len / len_v2(v1));

			v1[0] /= aspx;
			v1[1] /= aspy;

			add_v2_v2v2(bezt->vec[0], bezt->vec[1], v1);
			sub_v2_v2v2(bezt->vec[2], bezt->vec[1], v1);
		}
		else {
			copy_v3_v3(bezt->vec[0], bezt->vec[1]);
			copy_v3_v3(bezt->vec[2], bezt->vec[1]);
		}
	} else {
		sub_v2_v2v2(v1, loc, bezt->vec[1]);

		v2[0] = -v1[1] * aspy / aspx;
		v2[1] =  v1[0] * aspx / aspy;

		add_v2_v2v2(bezt->vec[0], bezt->vec[1], v2);
		sub_v2_v2v2(bezt->vec[2], bezt->vec[1], v2);
	}
}

float *BKE_mask_point_segment_feather_diff(MaskSpline *spline, MaskSplinePoint *point, float aspx, float aspy,
                                           int *tot_feather_point)
{
	float *feather, *fp;
	int i, resol = BKE_mask_spline_feather_resolution(spline);

	feather = fp = MEM_callocN(2 * resol * sizeof(float), "mask point spline feather diff points");

	for (i = 0; i < resol; i++, fp += 2) {
		float u = (float)(i % resol) / resol, weight;
		float co[2], n[2];

		BKE_mask_point_segment_co(spline, point, u, co);
		BKE_mask_point_normal(spline, point, aspx, aspy, u, n);
		weight = BKE_mask_point_weight(spline, point, u);

		fp[0] = co[0] + n[0] * weight;
		fp[1] = co[1] + n[1] * weight;
	}

	*tot_feather_point = resol;

	return feather;
}

float *BKE_mask_point_segment_diff(MaskSpline *spline, MaskSplinePoint *point, int *tot_diff_point)
{
	BezTriple *bezt, *next;
	float *diff_points, *fp;
	int j, resol = BKE_mask_spline_resolution(spline);

	bezt = &point->bezt;

	if (point == &spline->points[spline->tot_point - 1]) {
		if (spline->flag & MASK_SPLINE_CYCLIC)
			next = &(spline->points[0].bezt);
		else
			next = NULL;
	}
	else next = &((point + 1))->bezt;

	if (!next)
		return NULL;

	/* resol+1 because of 'forward_diff_bezier' function */
	*tot_diff_point = resol + 1;
	diff_points = fp = MEM_callocN((resol + 1)*2*sizeof(float), "mask segment vets");

	for (j = 0; j < 2; j++) {
		BKE_curve_forward_diff_bezier(bezt->vec[1][j], bezt->vec[2][j],
		                              next->vec[0][j], next->vec[1][j],
		                              fp + j, resol, 2 * sizeof(float));
	}

	copy_v2_v2(fp + 2 * resol, next->vec[1]);

	return diff_points;
}

void BKE_mask_point_segment_co(MaskSpline *spline, MaskSplinePoint *point, float u, float co[2])
{
	BezTriple *bezt = &point->bezt, *next;
	float q0[2], q1[2], q2[2], r0[2], r1[2];

	if (point == &spline->points[spline->tot_point - 1]) {
		if (spline->flag & MASK_SPLINE_CYCLIC)
			next = &(spline->points[0].bezt);
		else
			next = NULL;
	}
	else next = &((point + 1))->bezt;

	if (!next) {
		copy_v2_v2(co, bezt->vec[1]);
		return;
	}

	interp_v2_v2v2(q0, bezt->vec[1], bezt->vec[2], u);
	interp_v2_v2v2(q1, bezt->vec[2], next->vec[0], u);
	interp_v2_v2v2(q2, next->vec[0], next->vec[1], u);

	interp_v2_v2v2(r0, q0, q1, u);
	interp_v2_v2v2(r1, q1, q2, u);

	interp_v2_v2v2(co, r0, r1, u);
}

void BKE_mask_point_normal(MaskSpline *spline, MaskSplinePoint *point, float aspx, float aspy, float u, float n[2])
{
	BezTriple *bezt = &point->bezt, *next;
	float q0[2], q1[2], q2[2], r0[2], r1[2], vec[2];

	if (point == &spline->points[spline->tot_point - 1]) {
		if (spline->flag & MASK_SPLINE_CYCLIC)
			next = &(spline->points[0].bezt);
		else
			next = NULL;
	}
	else next = &((point + 1))->bezt;

	if (!next) {
		BKE_mask_point_handle(point, aspx, aspy, vec);

		sub_v2_v2v2(n, vec, bezt->vec[1]);

		n[0] *= aspx;
		n[1] *= aspy;

		normalize_v2(n);

		n[0] /= aspx;
		n[1] /= aspy;

		return;
	}

	interp_v2_v2v2(q0, bezt->vec[1], bezt->vec[2], u);
	interp_v2_v2v2(q1, bezt->vec[2], next->vec[0], u);
	interp_v2_v2v2(q2, next->vec[0], next->vec[1], u);

	interp_v2_v2v2(r0, q0, q1, u);
	interp_v2_v2v2(r1, q1, q2, u);

	sub_v2_v2v2(vec, r1, r0);

	n[0] = -vec[1] * aspy;
	n[1] =  vec[0] * aspx;

	normalize_v2(n);

	n[0] /= aspx;
	n[1] /= aspy;
}

float BKE_mask_point_weight(MaskSpline *spline, MaskSplinePoint *point, float u)
{
	BezTriple *bezt = &point->bezt, *next;
	float cur_u, cur_w, next_u, next_w, fac;
	int i;

	if (point == &spline->points[spline->tot_point - 1]) {
		if (spline->flag & MASK_SPLINE_CYCLIC)
			next = &(spline->points[0].bezt);
		else
			next = NULL;
	}
	else next = &((point + 1))->bezt;

	if (!next)
		return bezt->weight;

	for (i = 0; i < point->tot_uw + 1; i++) {

		if (i == 0) {
			cur_u = 0.0f;
			cur_w = bezt->weight;
		}
		else {
			cur_u = point->uw[i - 1].u;
			cur_w = point->uw[i - 1].w;
		}

		if (i == point->tot_uw) {
			next_u = 1.0f;
			next_w = next->weight;
		}
		else {
			next_u = point->uw[i].u;
			next_w = point->uw[i].w;
		}

		if (u >= cur_u && u <= next_u) {
			break;
		}
	}

	fac = (u - cur_u) / (next_u - cur_u);

	if (spline->weight_interp == MASK_SPLINE_INTERP_EASE)
		return cur_w + (next_w - cur_w) * (3.0f * fac * fac - 2.0f * fac * fac * fac);
	else
		return (1.0f - fac) * cur_w + fac * next_w;
}

MaskSplinePointUW *BKE_mask_point_sort_uw(MaskSplinePoint *point, MaskSplinePointUW *uw)
{
	if (point->tot_uw > 1) {
		int idx = uw - point->uw;

		if (idx > 0 && point->uw[idx - 1].u > uw->u) {
			while (idx > 0 && point->uw[idx - 1].u > point->uw[idx].u) {
				SWAP(MaskSplinePointUW, point->uw[idx - 1], point->uw[idx]);
				idx--;
			}
		}

		if (idx < point->tot_uw - 1 && point->uw[idx + 1].u < uw->u) {
			while (idx < point->tot_uw - 1 && point->uw[idx + 1].u < point->uw[idx].u) {
				SWAP(MaskSplinePointUW, point->uw[idx + 1], point->uw[idx]);
				idx++;
			}
		}

		return &point->uw[idx];
	}

	return uw;
}

void BKE_mask_point_add_uw(MaskSplinePoint *point, float u, float w)
{
	if (!point->uw)
		point->uw = MEM_callocN(sizeof(*point->uw), "mask point uw");
	else
		point->uw = MEM_reallocN(point->uw, (point->tot_uw + 1) * sizeof(*point->uw));

	point->uw[point->tot_uw].u = u;
	point->uw[point->tot_uw].w = w;

	point->tot_uw++;

	BKE_mask_point_sort_uw(point, &point->uw[point->tot_uw - 1]);
}

/* only mask block itself */
static Mask *mask_alloc(const char *name)
{
	Mask *mask;

	mask = alloc_libblock(&G.main->mask, ID_MSK, name);

	return mask;
}

Mask *BKE_mask_new(const char *name)
{
	Mask *mask;
	char mask_name[MAX_ID_NAME - 2];

	if (name && name[0])
		BLI_strncpy(mask_name, name, sizeof(mask_name));
	else
		strcpy(mask_name, "Mask");

	mask = mask_alloc(mask_name);

	return mask;
}

void BKE_mask_point_free(MaskSplinePoint *point)
{
	if (point->uw)
		MEM_freeN(point->uw);
}

void BKE_mask_spline_free(MaskSpline *spline)
{
	int i = 0;

	for (i = 0; i < spline->tot_point; i++) {
		MaskSplinePoint *point = &spline->points[i];

		BKE_mask_point_free(point);
	}

	MEM_freeN(spline->points);

	MEM_freeN(spline);
}

void BKE_mask_shape_free(MaskShape *shape)
{
	MaskSpline *spline = shape->splines.first;

	while (spline) {
		MaskSpline *next_spline = spline->next;

		BLI_remlink(&shape->splines, spline);
		BKE_mask_spline_free(spline);

		spline = next_spline;
	}

	MEM_freeN(shape);
}

void BKE_mask_free(Mask *mask)
{
	MaskShape *shape = mask->shapes.first;

	while (shape) {
		MaskShape *next_shape = shape->next;

		BLI_remlink(&mask->shapes, shape);
		BKE_mask_shape_free(shape);

		shape = next_shape;
	}
}

void BKE_mask_unlink(Main *bmain, Mask *mask)
{
	bScreen *scr;
	ScrArea *area;
	SpaceLink *sl;

	for (scr = bmain->screen.first; scr; scr = scr->id.next) {
		for (area = scr->areabase.first; area; area = area->next) {
			for(sl = area->spacedata.first; sl; sl = sl->next) {
				if(sl->spacetype == SPACE_CLIP) {
					SpaceClip *sc = (SpaceClip *) sl;

					if(sc->mask == mask)
						sc->mask = NULL;
				}
			}
		}
	}

	mask->id.us= 0;
}

static void evaluate_mask_parent(MaskParent *parent, float ctime, float co[2])
{
	if (!parent)
		return;

	if ((parent->flag & MASK_PARENT_ACTIVE) == 0)
		return;

	if (parent->id_type == ID_MC) {
		if (parent->id) {
			MovieClip *clip = (MovieClip *) parent->id;
			MovieTracking *tracking = (MovieTracking *) &clip->tracking;
			MovieTrackingObject *ob = BKE_tracking_named_object(tracking, parent->parent);

			if (ob) {
				MovieTrackingTrack *track = BKE_tracking_named_track(tracking, ob, parent->sub_parent);

				if (track) {
					MovieTrackingMarker *marker = BKE_tracking_get_marker(track, ctime);

					copy_v2_v2(co, marker->pos);
				}
			}
		}
	}
}

static void mask_calc_point_handle(MaskSplinePoint *point, MaskSplinePoint *prev_point, MaskSplinePoint *next_point)
{
	BezTriple *bezt = &point->bezt;
	BezTriple *prev_bezt = NULL, *next_bezt = NULL;
	int handle_type = bezt->h1;

	if (prev_point)
		prev_bezt = &prev_point->bezt;

	if (next_point)
		next_bezt = &next_point->bezt;

	if (handle_type == HD_VECT) {
		BKE_nurb_handle_calc(bezt, prev_bezt, next_bezt, 0);
	}
	else if (handle_type == HD_AUTO) {
		BKE_nurb_handle_calc(bezt, prev_bezt, next_bezt, 0);
	}
	else if (handle_type == HD_ALIGN) {
		float v1[3], v2[3];
		float vec[3], h[3];

		sub_v3_v3v3(v1, bezt->vec[0], bezt->vec[1]);
		sub_v3_v3v3(v2, bezt->vec[2], bezt->vec[1]);
		add_v3_v3v3(vec, v1, v2);

		if (len_v3(vec) > 1e-3) {
			h[0] = vec[1];
			h[1] = -vec[0];
			h[2] = 0.0f;
		}
		else {
			copy_v3_v3(h, v1);
		}

		add_v3_v3v3(bezt->vec[0], bezt->vec[1], h);
		sub_v3_v3v3(bezt->vec[2], bezt->vec[1], h);
	}
}

void BKE_mask_calc_handles(Mask *mask)
{
	MaskShape *shape = mask->shapes.first;

	while (shape) {
		MaskSpline *spline = shape->splines.first;
		int i;

		while (spline) {
			for (i = 0; i < spline->tot_point; i++) {
				MaskSplinePoint *point = &spline->points[i];
				MaskSplinePoint *prev_point, *next_point;

				if (i == 0) {
					if (spline->flag & MASK_SPLINE_CYCLIC)
						prev_point = &spline->points[spline->tot_point - 1];
					else
						prev_point = NULL;
				}
				else prev_point = point - 1;

				if (i == spline->tot_point - 1) {
					if (spline->flag & MASK_SPLINE_CYCLIC)
						next_point = &spline->points[0];
					else
						next_point = NULL;
				}
				else next_point = point + 1;

				mask_calc_point_handle(point, prev_point, next_point);
			}

			spline = spline->next;
		}

		shape = shape->next;
	}
}

void BKE_mask_evaluate(Mask *mask, float ctime)
{
	MaskShape *shape = mask->shapes.first;

	while (shape) {
		MaskSpline *spline = shape->splines.first;
		int i;

		while (spline) {
			for (i = 0; i < spline->tot_point; i++) {
				MaskSplinePoint *point = &spline->points[i];
				BezTriple *bezt = &point->bezt;
				float co[2], delta[2];

				copy_v2_v2(co, bezt->vec[1]);
				evaluate_mask_parent(&point->parent, ctime, co);
				sub_v2_v2v2(delta, co, bezt->vec[1]);

				add_v2_v2(bezt->vec[0], delta);
				add_v2_v2(bezt->vec[1], delta);
				add_v2_v2(bezt->vec[2], delta);
			}

			spline = spline->next;
		}

		shape = shape->next;
	}

	BKE_mask_calc_handles(mask);
}

void BKE_mask_evaluate_all_masks(Main *bmain, float ctime)
{
	Mask *mask;

	for (mask = bmain->mask.first; mask; mask = mask->id.next) {
		BKE_mask_evaluate(mask, ctime);
	}
}

void BKE_mask_update_scene(Main *bmain, Scene *scene)
{
	Mask *mask;

	for (mask = bmain->mask.first; mask; mask = mask->id.next) {
		if (mask->id.flag & LIB_ID_RECALC) {
			BKE_mask_evaluate_all_masks(bmain, CFRA);
		}
	}
}

void BKE_mask_parent_init(MaskParent *parent)
{
	parent->id_type = ID_MC;
}
