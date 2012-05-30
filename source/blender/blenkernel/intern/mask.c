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
#include "DNA_object_types.h"
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
#include "BKE_movieclip.h"
#include "BKE_utildefines.h"

#include "raskter.h"

MaskSplinePoint *BKE_mask_spline_point_array(MaskSpline *spline)
{
	return spline->points_deform ? spline->points_deform : spline->points;
}

MaskSplinePoint *BKE_mask_spline_point_array_from_point(MaskSpline *spline, MaskSplinePoint *point_ref)
{
	if ((point_ref >= spline->points) && (point_ref < &spline->points[spline->tot_point])) {
		return spline->points;
	}

	if ((point_ref >= spline->points_deform) && (point_ref < &spline->points_deform[spline->tot_point])) {
		return spline->points_deform;
	}

	BLI_assert(!"wrong array");
	return NULL;
}

/* mask objects */

MaskObject *BKE_mask_object_new(Mask *mask, const char *name)
{
	MaskObject *maskobj = MEM_callocN(sizeof(MaskObject), "new mask object");

	if (name && name[0])
		BLI_strncpy(maskobj->name, name, sizeof(maskobj->name));
	else
		strcpy(maskobj->name, "MaskObject");

	BLI_addtail(&mask->maskobjs, maskobj);

	BKE_mask_object_unique_name(mask, maskobj);

	mask->tot_maskobj++;

	maskobj->alpha = 1.0f;

	return maskobj;
}

/* note: may still be hidden, caller needs to check */
MaskObject *BKE_mask_object_active(Mask *mask)
{
	return BLI_findlink(&mask->maskobjs, mask->act_maskobj);
}

void BKE_mask_object_active_set(Mask *mask, MaskObject *maskobj)
{
	mask->act_maskobj = BLI_findindex(&mask->maskobjs, maskobj);
}

void BKE_mask_object_remove(Mask *mask, MaskObject *maskobj)
{
	BLI_remlink(&mask->maskobjs, maskobj);
	BKE_mask_object_free(maskobj);

	mask->tot_maskobj--;

	if (mask->act_maskobj >= mask->tot_maskobj)
		mask->act_maskobj = mask->tot_maskobj - 1;
}

void BKE_mask_object_unique_name(Mask *mask, MaskObject *maskobj)
{
	BLI_uniquename(&mask->maskobjs, maskobj, "MaskObject", '.', offsetof(MaskObject, name), sizeof(maskobj->name));
}

/* splines */

MaskSpline *BKE_mask_spline_add(MaskObject *maskobj)
{
	MaskSpline *spline;

	spline = MEM_callocN(sizeof(MaskSpline), "new mask spline");
	BLI_addtail(&maskobj->splines, spline);

	/* spline shall have one point at least */
	spline->points = MEM_callocN(sizeof(MaskSplinePoint), "new mask spline point");
	spline->tot_point = 1;

	/* cyclic shapes are more usually used */
	// spline->flag |= MASK_SPLINE_CYCLIC; // disable because its not so nice for drawing. could be done differently

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
	MaskSplinePoint *points_array = BKE_mask_spline_point_array(spline);

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
	diff_points = fp = MEM_callocN((len + 1) * 2 * sizeof(float), "mask spline vets");

	a = spline->tot_point - 1;
	if (spline->flag & MASK_SPLINE_CYCLIC)
		a++;

	prev = points_array;
	point = prev + 1;

	while (a--) {
		BezTriple *prevbezt;
		BezTriple *bezt;
		int j;

		if (a == 0 && (spline->flag & MASK_SPLINE_CYCLIC))
			point = points_array;

		prevbezt = &prev->bezt;
		bezt = &point->bezt;

		for (j = 0; j < 2; j++) {
			BKE_curve_forward_diff_bezier(prevbezt->vec[1][j], prevbezt->vec[2][j],
			                              bezt->vec[0][j], bezt->vec[1][j],
			                              fp + j, resol, 2 * sizeof(float));
		}

		fp += 2 * resol;

		if (a == 0 && (spline->flag & MASK_SPLINE_CYCLIC) == 0) {
			copy_v2_v2(fp, bezt->vec[1]);
		}

		prev = point;
		point++;
	}

	return diff_points;
}

float *BKE_mask_spline_feather_differentiated_points(MaskSpline *spline, int *tot_feather_point)
{
	MaskSplinePoint *points_array = BKE_mask_spline_point_array(spline);

	float *feather, *fp;
	int i, j, tot, resol = BKE_mask_spline_feather_resolution(spline);

	tot = resol * spline->tot_point;
	feather = fp = MEM_callocN(2 * tot * sizeof(float), "mask spline feather diff points");

	for (i = 0; i < spline->tot_point; i++) {
		MaskSplinePoint *point = &points_array[i];

		for (j = 0; j < resol; j++, fp += 2) {
			float u = (float) j / resol, weight;
			float co[2], n[2];

			BKE_mask_point_segment_co(spline, point, u, co);
			BKE_mask_point_normal(spline, point, u, n);
			weight = BKE_mask_point_weight(spline, point, u);

			fp[0] = co[0] + n[0] * weight;
			fp[1] = co[1] + n[1] * weight;
		}
	}

	*tot_feather_point = tot;

	return feather;
}

float *BKE_mask_spline_feather_points(MaskSpline *spline, int *tot_feather_point)
{
	MaskSplinePoint *points_array = BKE_mask_spline_point_array(spline);

	int i, tot = 0;
	float *feather, *fp;

	/* count */
	for (i = 0; i < spline->tot_point; i++) {
		MaskSplinePoint *point = &points_array[i];

		tot += point->tot_uw + 1;
	}

	/* create data */
	feather = fp = MEM_callocN(2 * tot * sizeof(float), "mask spline feather points");

	for (i = 0; i < spline->tot_point; i++) {
		MaskSplinePoint *point = &points_array[i];
		BezTriple *bezt = &point->bezt;
		float weight, n[2];
		int j;

		BKE_mask_point_normal(spline, point, 0.0f, n);
		weight = BKE_mask_point_weight(spline, point, 0.0f);

		fp[0] = bezt->vec[1][0] + n[0] * weight;
		fp[1] = bezt->vec[1][1] + n[1] * weight;
		fp += 2;

		for (j = 0; j < point->tot_uw; j++) {
			float u = point->uw[j].u;
			float co[2];

			BKE_mask_point_segment_co(spline, point, u, co);
			BKE_mask_point_normal(spline, point, u, n);
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

void BKE_mask_point_handle(MaskSplinePoint *point, float handle[2])
{
	float vec[2];

	sub_v2_v2v2(vec, point->bezt.vec[0], point->bezt.vec[1]);

	handle[0] = (point->bezt.vec[1][0] + vec[1]);
	handle[1] = (point->bezt.vec[1][1] - vec[0]);
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
	}
	else {
		sub_v2_v2v2(v1, loc, bezt->vec[1]);

		v2[0] = -v1[1] * aspy / aspx;
		v2[1] =  v1[0] * aspx / aspy;

		add_v2_v2v2(bezt->vec[0], bezt->vec[1], v2);
		sub_v2_v2v2(bezt->vec[2], bezt->vec[1], v2);
	}
}

float *BKE_mask_point_segment_feather_diff(MaskSpline *spline, MaskSplinePoint *point, int *tot_feather_point)
{
	float *feather, *fp;
	int i, resol = BKE_mask_spline_feather_resolution(spline);

	feather = fp = MEM_callocN(2 * resol * sizeof(float), "mask point spline feather diff points");

	for (i = 0; i < resol; i++, fp += 2) {
		float u = (float)(i % resol) / resol, weight;
		float co[2], n[2];

		BKE_mask_point_segment_co(spline, point, u, co);
		BKE_mask_point_normal(spline, point, u, n);
		weight = BKE_mask_point_weight(spline, point, u);

		fp[0] = co[0] + n[0] * weight;
		fp[1] = co[1] + n[1] * weight;
	}

	*tot_feather_point = resol;

	return feather;
}

float *BKE_mask_point_segment_diff(MaskSpline *spline, MaskSplinePoint *point, int *tot_diff_point)
{
	MaskSplinePoint *points_array = BKE_mask_spline_point_array_from_point(spline, point);

	BezTriple *bezt, *next;
	float *diff_points, *fp;
	int j, resol = BKE_mask_spline_resolution(spline);

	bezt = &point->bezt;

	if (point == &points_array[spline->tot_point - 1]) {
		if (spline->flag & MASK_SPLINE_CYCLIC)
			next = &(points_array[0].bezt);
		else
			next = NULL;
	}
	else next = &((point + 1))->bezt;

	if (!next)
		return NULL;

	/* resol+1 because of 'forward_diff_bezier' function */
	*tot_diff_point = resol + 1;
	diff_points = fp = MEM_callocN((resol + 1) * 2 * sizeof(float), "mask segment vets");

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
	MaskSplinePoint *points_array = BKE_mask_spline_point_array_from_point(spline, point);

	BezTriple *bezt = &point->bezt, *next;
	float q0[2], q1[2], q2[2], r0[2], r1[2];

	if (point == &points_array[spline->tot_point - 1]) {
		if (spline->flag & MASK_SPLINE_CYCLIC)
			next = &(points_array[0].bezt);
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

void BKE_mask_point_normal(MaskSpline *spline, MaskSplinePoint *point, float u, float n[2])
{
	MaskSplinePoint *points_array = BKE_mask_spline_point_array_from_point(spline, point);

	BezTriple *bezt = &point->bezt, *next;
	float q0[2], q1[2], q2[2], r0[2], r1[2], vec[2];

	if (point == &points_array[spline->tot_point - 1]) {
		if (spline->flag & MASK_SPLINE_CYCLIC)
			next = &(points_array[0].bezt);
		else
			next = NULL;
	}
	else {
		next = &((point + 1))->bezt;
	}

	if (!next) {
		BKE_mask_point_handle(point, vec);

		sub_v2_v2v2(n, vec, bezt->vec[1]);
		normalize_v2(n);
		return;
	}

	interp_v2_v2v2(q0, bezt->vec[1], bezt->vec[2], u);
	interp_v2_v2v2(q1, bezt->vec[2], next->vec[0], u);
	interp_v2_v2v2(q2, next->vec[0], next->vec[1], u);

	interp_v2_v2v2(r0, q0, q1, u);
	interp_v2_v2v2(r1, q1, q2, u);

	sub_v2_v2v2(vec, r1, r0);

	n[0] = -vec[1];
	n[1] =  vec[0];

	normalize_v2(n);
}

float BKE_mask_point_weight(MaskSpline *spline, MaskSplinePoint *point, float u)
{
	MaskSplinePoint *points_array = BKE_mask_spline_point_array_from_point(spline, point);

	BezTriple *bezt = &point->bezt, *next;
	float cur_u, cur_w, next_u, next_w, fac;
	int i;

	if (point == &points_array[spline->tot_point - 1]) {
		if (spline->flag & MASK_SPLINE_CYCLIC)
			next = &(points_array[0].bezt);
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

void BKE_mask_point_select_set(MaskSplinePoint *point, int select)
{
	int i;

	if (select) {
		MASKPOINT_SEL(point);
	}
	else {
		MASKPOINT_DESEL(point);
	}

	for (i = 0; i < point->tot_uw; i++) {
		if (select) {
			point->uw[i].flag |= SELECT;
		}
		else {
			point->uw[i].flag &= ~SELECT;
		}
	}
}

void BKE_mask_point_select_set_handle(MaskSplinePoint *point, int select)
{
	if (select) {
		MASKPOINT_HANDLE_SEL(point);
	}
	else {
		MASKPOINT_HANDLE_DESEL(point);
	}
}

/* only mask block itself */
static Mask *mask_alloc(const char *name)
{
	Mask *mask;

	mask = BKE_libblock_alloc(&G.main->mask, ID_MSK, name);

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
		MaskSplinePoint *point;
		point = &spline->points[i];
		BKE_mask_point_free(point);

		if (spline->points_deform) {
			point = &spline->points_deform[i];
			BKE_mask_point_free(point);
		}
	}

	MEM_freeN(spline->points);

	if (spline->points_deform) {
		MEM_freeN(spline->points_deform);
	}

	MEM_freeN(spline);
}

void BKE_mask_object_shape_free(MaskObjectShape *maskobj_shape)
{
	MEM_freeN(maskobj_shape->data);

	MEM_freeN(maskobj_shape);
}

void BKE_mask_object_free(MaskObject *maskobj)
{
	MaskSpline *spline;
	MaskObjectShape *maskobj_shape;

	/* free splines */
	spline = maskobj->splines.first;
	while (spline) {
		MaskSpline *next_spline = spline->next;

		BLI_remlink(&maskobj->splines, spline);
		BKE_mask_spline_free(spline);

		spline = next_spline;
	}

	/* free animation data */
	maskobj_shape = maskobj->splines_shapes.first;
	while (maskobj_shape) {
		MaskObjectShape *next_maskobj_shape = maskobj_shape->next;

		BLI_remlink(&maskobj->splines_shapes, maskobj_shape);
		BKE_mask_object_shape_free(maskobj_shape);

		maskobj_shape = next_maskobj_shape;
	}

	MEM_freeN(maskobj);
}

void BKE_mask_free(Mask *mask)
{
	MaskObject *maskobj = mask->maskobjs.first;

	while (maskobj) {
		MaskObject *next_maskobj = maskobj->next;

		BLI_remlink(&mask->maskobjs, maskobj);
		BKE_mask_object_free(maskobj);

		maskobj = next_maskobj;
	}
}

void BKE_mask_unlink(Main *bmain, Mask *mask)
{
	bScreen *scr;
	ScrArea *area;
	SpaceLink *sl;

	for (scr = bmain->screen.first; scr; scr = scr->id.next) {
		for (area = scr->areabase.first; area; area = area->next) {
			for (sl = area->spacedata.first; sl; sl = sl->next) {
				if (sl->spacetype == SPACE_CLIP) {
					SpaceClip *sc = (SpaceClip *) sl;

					if (sc->mask == mask)
						sc->mask = NULL;
				}
			}
		}
	}

	mask->id.us = 0;
}

void BKE_mask_coord_from_movieclip(MovieClip *clip, MovieClipUser *user, float r_co[2], const float co[2])
{
	int width, height;

	/* scaling for the clip */
	BKE_movieclip_get_size(clip, user, &width, &height);

	if (width == height) {
		r_co[0] = co[0];
		r_co[1] = co[1];
	}
	else if (width < height) {
		r_co[0] = ((co[0] - 0.5f) * ((float)width / (float)height)) + 0.5f;
		r_co[1] = co[1];
	}
	else { /* (width > height) */
		r_co[0] = co[0];
		r_co[1] = ((co[1] - 0.5f) * ((float)height / (float)width)) + 0.5f;
	}
}

/* as above but divide */
void BKE_mask_coord_to_movieclip(MovieClip *clip, MovieClipUser *user, float r_co[2], const float co[2])
{
	int width, height;

	/* scaling for the clip */
	BKE_movieclip_get_size(clip, user, &width, &height);

	if (width == height) {
		r_co[0] = co[0];
		r_co[1] = co[1];
	}
	else if (width < height) {
		r_co[0] = ((co[0] - 0.5f) / ((float)width / (float)height)) + 0.5f;
		r_co[1] = co[1];
	}
	else { /* (width > height) */
		r_co[0] = co[0];
		r_co[1] = ((co[1] - 0.5f) / ((float)height / (float)width)) + 0.5f;
	}
}

static int BKE_mask_evaluate_parent(MaskParent *parent, float ctime, float r_co[2])
{
	if (!parent)
		return FALSE;

	if ((parent->flag & MASK_PARENT_ACTIVE) == 0)
		return FALSE;

	if (parent->id_type == ID_MC) {
		if (parent->id) {
			MovieClip *clip = (MovieClip *) parent->id;
			MovieTracking *tracking = (MovieTracking *) &clip->tracking;
			MovieTrackingObject *ob = BKE_tracking_named_object(tracking, parent->parent);

			if (ob) {
				MovieTrackingTrack *track = BKE_tracking_named_track(tracking, ob, parent->sub_parent);

				MovieClipUser user = {0};
				user.framenr = ctime;

				if (track) {
					MovieTrackingMarker *marker = BKE_tracking_get_marker(track, ctime);
					float marker_pos_ofs[2];
					add_v2_v2v2(marker_pos_ofs, marker->pos, track->offset);
					BKE_mask_coord_from_movieclip(clip, &user, r_co, marker_pos_ofs);

					return TRUE;
				}
			}
		}
	}

	return FALSE;
}

int BKE_mask_evaluate_parent_delta(MaskParent *parent, float ctime, float r_delta[2])
{
	float parent_co[2];

	if (BKE_mask_evaluate_parent(parent, ctime, parent_co)) {
		sub_v2_v2v2(r_delta, parent_co, parent->parent_orig);
		return TRUE;
	}
	else {
		return FALSE;
	}
}

static void mask_calc_point_handle(MaskSplinePoint *point, MaskSplinePoint *prev_point, MaskSplinePoint *next_point)
{
	BezTriple *bezt = &point->bezt;
	BezTriple *prev_bezt = NULL, *next_bezt = NULL;
	//int handle_type = bezt->h1;

	if (prev_point)
		prev_bezt = &prev_point->bezt;

	if (next_point)
		next_bezt = &next_point->bezt;

#if 1
	if (prev_bezt || next_bezt) {
		BKE_nurb_handle_calc(bezt, prev_bezt, next_bezt, 0);
	}
#else
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
#endif
}

void BKE_mask_get_handle_point_adjacent(Mask *UNUSED(mask), MaskSpline *spline, MaskSplinePoint *point,
                                        MaskSplinePoint **r_point_prev, MaskSplinePoint **r_point_next)
{
	MaskSplinePoint *prev_point, *next_point;
	int i = (int)(point - spline->points);

	BLI_assert(i >= i && i < spline->tot_point);

	if (i == 0) {
		if (spline->flag & MASK_SPLINE_CYCLIC) {
			prev_point = &spline->points[spline->tot_point - 1];
		}
		else {
			prev_point = NULL;
		}
	}
	else {
		prev_point = point - 1;
	}

	if (i == spline->tot_point - 1) {
		if (spline->flag & MASK_SPLINE_CYCLIC) {
			next_point = &spline->points[0];
		}
		else {
			next_point = NULL;
		}
	}
	else {
		next_point = point + 1;
	}

	*r_point_prev = prev_point;
	*r_point_next = next_point;
}

/* calculates the tanget of a point by its previous and next
 * (ignoring handles - as if its a poly line) */
void BKE_mask_calc_tangent_polyline(Mask *mask, MaskSpline *spline, MaskSplinePoint *point, float t[2])
{
	float tvec_a[2], tvec_b[2];

	MaskSplinePoint *prev_point, *next_point;

	BKE_mask_get_handle_point_adjacent(mask, spline, point,
	                                   &prev_point, &next_point);

	if (prev_point) {
		sub_v2_v2v2(tvec_a, point->bezt.vec[1], prev_point->bezt.vec[1]);
		normalize_v2(tvec_a);
	}
	else {
		zero_v2(tvec_a);
	}

	if (next_point) {
		sub_v2_v2v2(tvec_b, next_point->bezt.vec[1], point->bezt.vec[1]);
		normalize_v2(tvec_b);
	}
	else {
		zero_v2(tvec_b);
	}

	add_v2_v2v2(t, tvec_a, tvec_b);
	normalize_v2(t);
}

void BKE_mask_calc_handle_point(Mask *mask, MaskSpline *spline, MaskSplinePoint *point)
{
	MaskSplinePoint *prev_point, *next_point;

	BKE_mask_get_handle_point_adjacent(mask, spline, point,
	                                   &prev_point, &next_point);

	mask_calc_point_handle(point, prev_point, next_point);
}

static void enforce_dist_v2_v2fl(float v1[2], const float v2[2], const float dist)
{
	if (!equals_v2v2(v2, v1)) {
		float nor[2];

		sub_v2_v2v2(nor, v1, v2);
		normalize_v2(nor);
		madd_v2_v2v2fl(v1, v2, nor, dist);
	}
}

void BKE_mask_calc_handle_adjacent_length(Mask *mask, MaskSpline *spline, MaskSplinePoint *point)
{
	/* TODO! - make this interpolate between siblings - not always midpoint! */
	int length_tot = 0;
	float length_average = 0.0f;

	MaskSplinePoint *prev_point, *next_point;
	BKE_mask_get_handle_point_adjacent(mask, spline, point,
	                                   &prev_point, &next_point);

	if (prev_point) {
		length_average += len_v2v2(prev_point->bezt.vec[0], prev_point->bezt.vec[1]);
		length_tot++;
	}

	if (next_point) {
		length_average += len_v2v2(next_point->bezt.vec[2], next_point->bezt.vec[1]);
		length_tot++;
	}

	if (length_tot) {
		length_average /= (float)length_tot;

		enforce_dist_v2_v2fl(point->bezt.vec[0], point->bezt.vec[1], length_average);
		enforce_dist_v2_v2fl(point->bezt.vec[2], point->bezt.vec[1], length_average);
	}
}


/**
 * \brief Resets auto handles even for non-auto bezier points
 *
 * Useful for giving sane defaults.
 */
void BKE_mask_calc_handle_point_auto(Mask *mask, MaskSpline *spline, MaskSplinePoint *point,
                                     const short do_recalc_length)
{
	MaskSplinePoint *prev_point, *next_point;
	const char h_back[2] = {point->bezt.h1, point->bezt.h2};
	const float length_average = (do_recalc_length) ? 0.0f /* dummy value */ :
	                                                 (len_v3v3(point->bezt.vec[0], point->bezt.vec[1]) +
	                                                  len_v3v3(point->bezt.vec[1], point->bezt.vec[2])) / 2.0f;

	BKE_mask_get_handle_point_adjacent(mask, spline, point,
	                                   &prev_point, &next_point);

	point->bezt.h1 = HD_AUTO;
	point->bezt.h2 = HD_AUTO;
	mask_calc_point_handle(point, prev_point, next_point);

	point->bezt.h1 = h_back[0];
	point->bezt.h2 = h_back[1];

	/* preserve length by applying it back */
	if (do_recalc_length == FALSE) {
		enforce_dist_v2_v2fl(point->bezt.vec[0], point->bezt.vec[1], length_average);
		enforce_dist_v2_v2fl(point->bezt.vec[2], point->bezt.vec[1], length_average);
	}
}

void BKE_mask_calc_handles(Mask *mask)
{
	MaskObject *maskobj;

	for (maskobj = mask->maskobjs.first; maskobj; maskobj = maskobj->next) {
		MaskSpline *spline;

		for (spline = maskobj->splines.first; spline; spline = spline->next) {
			int i;

			for (i = 0; i < spline->tot_point; i++) {
				BKE_mask_calc_handle_point(mask, spline, &spline->points[i]);

				/* could be done in a different function... */
				if (spline->points_deform) {
					BKE_mask_calc_handle_point(mask, spline, &spline->points[i]);
				}
			}
		}
	}
}

void BKE_mask_update_deform(Mask *mask)
{
	MaskObject *maskobj;

	for (maskobj = mask->maskobjs.first; maskobj; maskobj = maskobj->next) {
		MaskSpline *spline;

		for (spline = maskobj->splines.first; spline; spline = spline->next) {
			int i;

			for (i = 0; i < spline->tot_point; i++) {
				const int i_prev = (i - 1) % spline->tot_point;
				const int i_next = (i + 1) % spline->tot_point;

				BezTriple *bezt_prev = &spline->points[i_prev].bezt;
				BezTriple *bezt      = &spline->points[i     ].bezt;
				BezTriple *bezt_next = &spline->points[i_next].bezt;

				BezTriple *bezt_def_prev = &spline->points_deform[i_prev].bezt;
				BezTriple *bezt_def      = &spline->points_deform[i     ].bezt;
				BezTriple *bezt_def_next = &spline->points_deform[i_next].bezt;

				float w_src[4];
				int j;

				for (j = 0; j <= 2; j += 2) { /* (0, 2) */
					printf("--- %d %d, %d, %d\n", i, j, i_prev, i_next);
					barycentric_weights_v2(bezt_prev->vec[1], bezt->vec[1], bezt_next->vec[1],
					                       bezt->vec[j], w_src);
					interp_v3_v3v3v3(bezt_def->vec[j],
					                 bezt_def_prev->vec[1], bezt_def->vec[1], bezt_def_next->vec[1], w_src);
				}
			}
		}
	}
}

void BKE_mask_spline_ensure_deform(MaskSpline *spline)
{
// printf("SPLINE ALLOC %p %d\n", spline->points_deform, (int)(MEM_allocN_len(spline->points_deform) / sizeof(*spline->points_deform)));

	if ((spline->points_deform == NULL) ||
	    (MEM_allocN_len(spline->points_deform) / sizeof(*spline->points_deform)) != spline->tot_point)
	{
		printf("alloc new spline\n");

		if (spline->points_deform) {
			MEM_freeN(spline->points_deform);
		}

		spline->points_deform = MEM_mallocN(sizeof(*spline->points_deform) * spline->tot_point, __func__);
	}
	else {
		// printf("alloc spline done\n");
	}
}

void BKE_mask_evaluate(Mask *mask, float ctime, const int do_newframe)
{
	MaskObject *maskobj;

	for (maskobj = mask->maskobjs.first; maskobj; maskobj = maskobj->next) {

		/* animation if available */
		if (do_newframe) {
			MaskObjectShape *maskobj_shape_a;
			MaskObjectShape *maskobj_shape_b;
			int found;

			if ((found = BKE_mask_object_shape_find_frame_range(maskobj, (int)ctime,
			                                                    &maskobj_shape_a, &maskobj_shape_b)))
			{
				if (found == 1) {
#if 0
					printf("%s: exact %d %d (%d)\n", __func__, (int)ctime, BLI_countlist(&maskobj->splines_shapes),
					       maskobj_shape_a->frame);
#endif

					BKE_mask_object_shape_to_mask(maskobj, maskobj_shape_a);
				}
				else if (found == 2) {
					float w = maskobj_shape_b->frame - maskobj_shape_a->frame;
#if 0
					printf("%s: tween %d %d (%d %d)\n", __func__, (int)ctime, BLI_countlist(&maskobj->splines_shapes),
					       maskobj_shape_a->frame, maskobj_shape_b->frame);
#endif
					BKE_mask_object_shape_to_mask_interp(maskobj, maskobj_shape_a, maskobj_shape_b,
					                                     (ctime - maskobj_shape_a->frame) / w);
				}
				else {
					/* always fail, should never happen */
					BLI_assert(found == 2);
				}
			}
		}
		/* animation done... */
	}

	BKE_mask_calc_handles(mask);


	for (maskobj = mask->maskobjs.first; maskobj; maskobj = maskobj->next) {
		MaskSpline *spline;
		int i;

		for (spline = maskobj->splines.first; spline; spline = spline->next) {

			BKE_mask_spline_ensure_deform(spline);

			for (i = 0; i < spline->tot_point; i++) {
				MaskSplinePoint *point = &spline->points[i];
				MaskSplinePoint *point_deform = &spline->points_deform[i];
				float delta[2];

				*point_deform = *point;
				point_deform->uw = point->uw ? MEM_dupallocN(point->uw) : NULL;

				if (BKE_mask_evaluate_parent_delta(&point->parent, ctime, delta)) {
					add_v2_v2(point_deform->bezt.vec[0], delta);
					add_v2_v2(point_deform->bezt.vec[1], delta);
					add_v2_v2(point_deform->bezt.vec[2], delta);
				}
			}
		}
	}
}

/* the purpose of this function is to ensure spline->points_deform is never out of date.
 * for now re-evaluate all. eventually this might work differently */
void BKE_mask_update_display(Mask *mask, float ctime)
{
#if 0
	MaskObject *maskobj;

	for (maskobj = mask->maskobjs.first; maskobj; maskobj = maskobj->next) {
		MaskSpline *spline;

		for (spline = maskobj->splines.first; spline; spline = spline->next) {
			if (spline->points_deform) {
				int i = 0;

				for (i = 0; i < spline->tot_point; i++) {
					MaskSplinePoint *point;

					if (spline->points_deform) {
						point = &spline->points_deform[i];
						BKE_mask_point_free(point);
					}
				}
				if (spline->points_deform) {
					MEM_freeN(spline->points_deform);
				}

				spline->points_deform = NULL;
			}
		}
	}
#endif

	BKE_mask_evaluate(mask, ctime, FALSE);
}

void BKE_mask_evaluate_all_masks(Main *bmain, float ctime, const int do_newframe)
{
	Mask *mask;

	for (mask = bmain->mask.first; mask; mask = mask->id.next) {
		BKE_mask_evaluate(mask, ctime, do_newframe);
	}
}

void BKE_mask_update_scene(Main *bmain, Scene *scene, const int do_newframe)
{
	Mask *mask;

	for (mask = bmain->mask.first; mask; mask = mask->id.next) {
		if (mask->id.flag & LIB_ID_RECALC) {
			BKE_mask_evaluate_all_masks(bmain, CFRA, do_newframe);
		}
	}
}

void BKE_mask_parent_init(MaskParent *parent)
{
	parent->id_type = ID_MC;
}


/* *** own animation/shapekey implimentation ***
 * BKE_mask_object_shape_XXX */

int BKE_mask_object_shape_totvert(MaskObject *maskobj)
{
	int tot = 0;
	MaskSpline *spline;

	for (spline = maskobj->splines.first; spline; spline = spline->next) {
		tot += spline->tot_point;
	}

	return tot;
}

static void mask_object_shape_from_mask_point(BezTriple *bezt, float fp[MASK_OBJECT_SHAPE_ELEM_SIZE])
{
	copy_v2_v2(&fp[0], bezt->vec[0]);
	copy_v2_v2(&fp[2], bezt->vec[1]);
	copy_v2_v2(&fp[4], bezt->vec[2]);
	fp[6] = bezt->weight;
	fp[7] = bezt->radius;
}

static void mask_object_shape_to_mask_point(BezTriple *bezt, float fp[MASK_OBJECT_SHAPE_ELEM_SIZE])
{
	copy_v2_v2(bezt->vec[0], &fp[0]);
	copy_v2_v2(bezt->vec[1], &fp[2]);
	copy_v2_v2(bezt->vec[2], &fp[4]);
	bezt->weight = fp[6];
	bezt->radius = fp[7];
}

/* these functions match. copy is swapped */
void BKE_mask_object_shape_from_mask(MaskObject *maskobj, MaskObjectShape *maskobj_shape)
{
	int tot = BKE_mask_object_shape_totvert(maskobj);

	if (maskobj_shape->tot_vert == tot) {
		float *fp = maskobj_shape->data;

		MaskSpline *spline;
		for (spline = maskobj->splines.first; spline; spline = spline->next) {
			int i;
			for (i = 0; i < spline->tot_point; i++) {
				mask_object_shape_from_mask_point(&spline->points[i].bezt, fp);
				fp += MASK_OBJECT_SHAPE_ELEM_SIZE;
			}
		}
	}
	else {
		printf("%s: vert mismatch %d != %d (frame %d)\n",
		       __func__, maskobj_shape->tot_vert, tot, maskobj_shape->frame);
	}
}

void BKE_mask_object_shape_to_mask(MaskObject *maskobj, MaskObjectShape *maskobj_shape)
{
	int tot = BKE_mask_object_shape_totvert(maskobj);

	if (maskobj_shape->tot_vert == tot) {
		float *fp = maskobj_shape->data;

		MaskSpline *spline;
		for (spline = maskobj->splines.first; spline; spline = spline->next) {
			int i;
			for (i = 0; i < spline->tot_point; i++) {
				mask_object_shape_to_mask_point(&spline->points[i].bezt, fp);
				fp += MASK_OBJECT_SHAPE_ELEM_SIZE;
			}
		}
	}
	else {
		printf("%s: vert mismatch %d != %d (frame %d)\n",
		       __func__, maskobj_shape->tot_vert, tot, maskobj_shape->frame);
	}
}

BLI_INLINE void interp_v2_v2v2_flfl(float target[2], const float a[2], const float b[2],
                                    const float t, const float s)
{
	target[0] = s * a[0] + t * b[0];
	target[1] = s * a[1] + t * b[1];
}

/* linear interpolation only */
void BKE_mask_object_shape_to_mask_interp(MaskObject *maskobj,
                                          MaskObjectShape *maskobj_shape_a,
                                          MaskObjectShape *maskobj_shape_b,
                                          const float fac)
{
	int tot = BKE_mask_object_shape_totvert(maskobj);
	if (maskobj_shape_a->tot_vert == tot && maskobj_shape_b->tot_vert == tot) {
		float *fp_a = maskobj_shape_a->data;
		float *fp_b = maskobj_shape_b->data;
		const float ifac = 1.0f - fac;

		MaskSpline *spline;
		for (spline = maskobj->splines.first; spline; spline = spline->next) {
			int i;
			for (i = 0; i < spline->tot_point; i++) {
				BezTriple *bezt = &spline->points[i].bezt;
				/* *** BKE_mask_object_shape_from_mask - swapped *** */
				interp_v2_v2v2_flfl(bezt->vec[0], fp_a, fp_b, fac, ifac); fp_a += 2; fp_b += 2;
				interp_v2_v2v2_flfl(bezt->vec[1], fp_a, fp_b, fac, ifac); fp_a += 2; fp_b += 2;
				interp_v2_v2v2_flfl(bezt->vec[2], fp_a, fp_b, fac, ifac); fp_a += 2; fp_b += 2;
				bezt->weight = (fp_a[0] * ifac) + (fp_b[0] * fac);
				bezt->radius = (fp_a[1] * ifac) + (fp_b[1] * fac); fp_a += 2; fp_b += 2;
			}
		}
	}
	else {
		printf("%s: vert mismatch %d != %d != %d (frame %d - %d)\n",
		       __func__, maskobj_shape_a->tot_vert, maskobj_shape_b->tot_vert, tot,
		       maskobj_shape_a->frame, maskobj_shape_b->frame);
	}
}

MaskObjectShape *BKE_mask_object_shape_find_frame(MaskObject *maskobj, int frame)
{
	MaskObjectShape *maskobj_shape;

	for (maskobj_shape = maskobj->splines_shapes.first;
	     maskobj_shape;
	     maskobj_shape = maskobj_shape->next)
	{
		if (frame == maskobj_shape->frame) {
			return maskobj_shape;
		}
		else if (frame < maskobj_shape->frame) {
			break;
		}
	}

	return NULL;
}

/* when returning 2 - the frame isnt found but before/after frames are */
int BKE_mask_object_shape_find_frame_range(MaskObject *maskobj, int frame,
                                           MaskObjectShape **r_maskobj_shape_a,
                                           MaskObjectShape **r_maskobj_shape_b)
{
	MaskObjectShape *maskobj_shape;

	for (maskobj_shape = maskobj->splines_shapes.first;
	     maskobj_shape;
	     maskobj_shape = maskobj_shape->next)
	{
		if (frame == maskobj_shape->frame) {
			*r_maskobj_shape_a = maskobj_shape;
			*r_maskobj_shape_b = NULL;
			return 1;
		}
		else if (frame < maskobj_shape->frame) {
			if (maskobj_shape->prev) {
				*r_maskobj_shape_a = maskobj_shape->prev;
				*r_maskobj_shape_b = maskobj_shape;
				return 2;
			}
			else {
				*r_maskobj_shape_a = maskobj_shape;
				*r_maskobj_shape_b = NULL;
				return 1;
			}
		}
	}

	*r_maskobj_shape_a = NULL;
	*r_maskobj_shape_b = NULL;

	return 0;
}

MaskObjectShape *BKE_mask_object_shape_varify_frame(MaskObject *maskobj, int frame)
{
	MaskObjectShape *maskobj_shape;

	maskobj_shape = BKE_mask_object_shape_find_frame(maskobj, frame);

	if (maskobj_shape == NULL) {
		int tot_vert = BKE_mask_object_shape_totvert(maskobj);

		maskobj_shape = MEM_mallocN(sizeof(MaskObjectShape), __func__);
		maskobj_shape->frame = frame;
		maskobj_shape->tot_vert = tot_vert;
		maskobj_shape->data = MEM_mallocN(tot_vert * sizeof(float) * MASK_OBJECT_SHAPE_ELEM_SIZE, __func__);

		BLI_addtail(&maskobj->splines_shapes, maskobj_shape);

		BKE_mask_object_shape_sort(maskobj);
	}

#if 0
		{
			MaskObjectShape *maskobj_shape;
			int i = 0;
			for (maskobj_shape = maskobj->splines_shapes.first;
			     maskobj_shape;
			     maskobj_shape = maskobj_shape->next)
			{
				printf("mask %d, %d\n", i++, maskobj_shape->frame);
			}
		}
#endif

	return maskobj_shape;
}

void BKE_mask_object_shape_unlink(MaskObject *maskobj, MaskObjectShape *maskobj_shape)
{
	BLI_remlink(&maskobj->splines_shapes, maskobj_shape);

	BKE_mask_object_shape_free(maskobj_shape);
}

static int mask_object_shape_sort_cb(void *maskobj_shape_a_ptr, void *maskobj_shape_b_ptr)
{
	MaskObjectShape *maskobj_shape_a = (MaskObjectShape *)maskobj_shape_a_ptr;
	MaskObjectShape *maskobj_shape_b = (MaskObjectShape *)maskobj_shape_b_ptr;

	if      (maskobj_shape_a->frame < maskobj_shape_b->frame)  return -1;
	else if (maskobj_shape_a->frame > maskobj_shape_b->frame)  return  1;
	else                                                       return  0;
}

void BKE_mask_object_shape_sort(MaskObject *maskobj)
{
	BLI_sortlist(&maskobj->splines_shapes, mask_object_shape_sort_cb);
}

int BKE_mask_object_shape_spline_from_index(MaskObject *maskobj, int index,
                                            MaskSpline **r_maskobj_shape, int *r_index)
{
	MaskSpline *spline;

	for (spline = maskobj->splines.first; spline; spline = spline->next) {
		if (index < spline->tot_point) {
			*r_maskobj_shape = spline;
			*r_index = index;
			return TRUE;
		}
		index -= spline->tot_point;
	}

	return FALSE;
}

int BKE_mask_object_shape_spline_to_index(MaskObject *maskobj, MaskSpline *spline)
{
	MaskSpline *spline_iter;
	int i_abs = 0;
	for (spline_iter = maskobj->splines.first;
	     spline_iter && spline_iter != spline;
	     i_abs += spline_iter->tot_point, spline_iter = spline_iter->next)
	{
		/* pass */
	}

	return i_abs;
}

/* basic 2D interpolation functions, could make more comprehensive later */
static void interp_weights_uv_v2_calc(float r_uv[2], const float pt[2], const float pt_a[2], const float pt_b[2])
{
	float pt_on_line[2];
	r_uv[0] = closest_to_line_v2(pt_on_line, pt, pt_a, pt_b);
	r_uv[1] = (len_v2v2(pt_on_line, pt) / len_v2v2(pt_a, pt_b)) *
	          ((line_point_side_v2(pt_a, pt_b, pt) < 0.0f) ? -1.0 : 1.0);  /* this line only sets the sign */
}


static void interp_weights_uv_v2_apply(const float uv[2], float r_pt[2], const float pt_a[2], const float pt_b[2])
{
	const float dvec[2] = {pt_b[0] - pt_a[0],
	                       pt_b[1] - pt_a[1]};

	/* u */
	madd_v2_v2v2fl(r_pt, pt_a, dvec, uv[0]);

	/* v */
	r_pt[0] += -dvec[1] * uv[1];
	r_pt[1] +=  dvec[0] * uv[1];
}

/* when a now points added - resize all shapekey array  */
void BKE_mask_object_shape_changed_add(MaskObject *maskobj, int index,
                                       int do_init, int do_init_interpolate)
{
	MaskObjectShape *maskobj_shape;

	/* spline index from maskobj */
	MaskSpline *spline;
	int         spline_point_index;

	if (BKE_mask_object_shape_spline_from_index(maskobj, index,
	                                            &spline, &spline_point_index))
	{
		/* sanity check */
		/* the point has already been removed in this array so subtract one when comparing with the shapes */
		int tot = BKE_mask_object_shape_totvert(maskobj) - 1;

		/* for interpolation */
		/* TODO - assumes closed curve for now */
		float uv[3][2]; /* 3x 2D handles */
		const int pi_curr =   spline_point_index;
		const int pi_prev = ((spline_point_index - 1) + spline->tot_point) % spline->tot_point;
		const int pi_next =  (spline_point_index + 1)                      % spline->tot_point;

		const int index_offset = index - spline_point_index;
		/* const int pi_curr_abs = index; */
		const int pi_prev_abs = pi_prev + index_offset;
		const int pi_next_abs = pi_next + index_offset;

		int i;
		if (do_init_interpolate) {
			for (i = 0; i < 3; i++) {
				interp_weights_uv_v2_calc(uv[i],
				                          spline->points[pi_curr].bezt.vec[i],
				                          spline->points[pi_prev].bezt.vec[i],
				                          spline->points[pi_next].bezt.vec[i]);
			}
		}

		for (maskobj_shape = maskobj->splines_shapes.first;
		     maskobj_shape;
		     maskobj_shape = maskobj_shape->next)
		{
			if (tot == maskobj_shape->tot_vert) {
				float *data_resized;

				maskobj_shape->tot_vert++;
				data_resized = MEM_mallocN(maskobj_shape->tot_vert * sizeof(float) * MASK_OBJECT_SHAPE_ELEM_SIZE, __func__);
				if (index > 0) {
					memcpy(data_resized,
					       maskobj_shape->data,
					       index * sizeof(float) * MASK_OBJECT_SHAPE_ELEM_SIZE);
				}

				if (index != maskobj_shape->tot_vert - 1) {
					memcpy(&data_resized[(index + 1) * MASK_OBJECT_SHAPE_ELEM_SIZE],
					       maskobj_shape->data + (index * MASK_OBJECT_SHAPE_ELEM_SIZE),
					       (maskobj_shape->tot_vert - (index + 1)) * sizeof(float) * MASK_OBJECT_SHAPE_ELEM_SIZE);
				}

				if (do_init) {
					float *fp = &data_resized[index * MASK_OBJECT_SHAPE_ELEM_SIZE];

					mask_object_shape_from_mask_point(&spline->points[spline_point_index].bezt, fp);

					if (do_init_interpolate && spline->tot_point > 2) {
						for (i = 0; i < 3; i++) {
							interp_weights_uv_v2_apply(uv[i],
							                           &fp[i * 2],
							                           &data_resized[(pi_prev_abs * MASK_OBJECT_SHAPE_ELEM_SIZE) + (i * 2)],
							                           &data_resized[(pi_next_abs * MASK_OBJECT_SHAPE_ELEM_SIZE) + (i * 2)]);
						}
					}
				}
				else {
					memset(&data_resized[index * MASK_OBJECT_SHAPE_ELEM_SIZE],
					       0,
					       sizeof(float) * MASK_OBJECT_SHAPE_ELEM_SIZE);
				}

				MEM_freeN(maskobj_shape->data);
				maskobj_shape->data = data_resized;
			}
			else {
				printf("%s: vert mismatch %d != %d (frame %d)\n",
				       __func__, maskobj_shape->tot_vert, tot, maskobj_shape->frame);
			}
		}
	}
}


/* move array to account for removed point */
void BKE_mask_object_shape_changed_remove(MaskObject *maskobj, int index, int count)
{
	MaskObjectShape *maskobj_shape;

	/* the point has already been removed in this array so add one when comparing with the shapes */
	int tot = BKE_mask_object_shape_totvert(maskobj);

	for (maskobj_shape = maskobj->splines_shapes.first;
	     maskobj_shape;
	     maskobj_shape = maskobj_shape->next)
	{
		if (tot == maskobj_shape->tot_vert - count) {
			float *data_resized;

			maskobj_shape->tot_vert -= count;
			data_resized = MEM_mallocN(maskobj_shape->tot_vert * sizeof(float) * MASK_OBJECT_SHAPE_ELEM_SIZE, __func__);
			if (index > 0) {
				memcpy(data_resized,
				       maskobj_shape->data,
				       index * sizeof(float) * MASK_OBJECT_SHAPE_ELEM_SIZE);
			}

			if (index != maskobj_shape->tot_vert) {
				memcpy(&data_resized[index * MASK_OBJECT_SHAPE_ELEM_SIZE],
				       maskobj_shape->data + ((index + count) * MASK_OBJECT_SHAPE_ELEM_SIZE),
				       (maskobj_shape->tot_vert - index) * sizeof(float) * MASK_OBJECT_SHAPE_ELEM_SIZE);
			}

			MEM_freeN(maskobj_shape->data);
			maskobj_shape->data = data_resized;
		}
		else {
			printf("%s: vert mismatch %d != %d (frame %d)\n",
			       __func__, maskobj_shape->tot_vert - count, tot, maskobj_shape->frame);
		}
	}
}

/* rasterization */
void BKE_mask_rasterize(Mask *mask, int width, int height, float *buffer)
{
	MaskObject *maskobj;

	for (maskobj = mask->maskobjs.first; maskobj; maskobj = maskobj->next) {
		MaskSpline *spline;

		for (spline = maskobj->splines.first; spline; spline = spline->next) {
			float *diff_points;
			int tot_diff_point;

			diff_points = BKE_mask_spline_differentiate(spline, &tot_diff_point);

			/* TODO, make this optional! */
			if (width != height) {
				float *fp;
				int i;
				float asp;

				if (width < height) {
					fp = &diff_points[0];
					asp = (float)width / (float)height;
				}
				else {
					fp = &diff_points[1];
					asp = (float)height / (float)width;
				}

				for (i = 0; i < tot_diff_point; i++, fp += 2) {
					(*fp) = (((*fp) - 0.5f) / asp) + 0.5f;
				}
			}

			if (tot_diff_point) {
				PLX_raskterize(diff_points, tot_diff_point, buffer, width, height);

				MEM_freeN(diff_points);
			}
		}
	}
}
