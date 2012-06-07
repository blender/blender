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
 *
 * Contributor(s): Blender Foundation,
 *                 Sergey Sharybin
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/mask/mask_add.c
 *  \ingroup edmask
 */

#include "MEM_guardedalloc.h"

#include "BLI_math.h"

#include "BKE_context.h"
#include "BKE_depsgraph.h"
#include "BKE_mask.h"

#include "DNA_scene_types.h"
#include "DNA_mask_types.h"
#include "DNA_object_types.h"  /* SELECT */

#include "WM_api.h"
#include "WM_types.h"

#include "ED_mask.h"  /* own include */

#include "RNA_access.h"
#include "RNA_define.h"

#include "mask_intern.h"  /* own include */


static int find_nearest_diff_point(bContext *C, Mask *mask, const float normal_co[2], int threshold, int feather,
                                   MaskLayer **masklay_r, MaskSpline **spline_r, MaskSplinePoint **point_r,
                                   float *u_r, float tangent[2],
                                   const short use_deform)
{
	MaskLayer *masklay, *point_masklay;
	MaskSpline *point_spline;
	MaskSplinePoint *point = NULL;
	float dist, co[2];
	int width, height;
	float u;
	float scalex, scaley, aspx, aspy;

	ED_mask_size(C, &width, &height);
	ED_mask_aspect(C, &aspx, &aspy);
	ED_mask_pixelspace_factor(C, &scalex, &scaley);

	co[0] = normal_co[0] * scalex;
	co[1] = normal_co[1] * scaley;

	for (masklay = mask->masklayers.first; masklay; masklay = masklay->next) {
		MaskSpline *spline;

		if (masklay->restrictflag & (MASK_RESTRICT_VIEW | MASK_RESTRICT_SELECT)) {
			continue;
		}

		for (spline = masklay->splines.first; spline; spline = spline->next) {
			int i;
			MaskSplinePoint *cur_point;

			for (i = 0, cur_point = use_deform ? spline->points_deform : spline->points;
			     i < spline->tot_point;
			     i++, cur_point++)
			{
				float *diff_points;
				int tot_diff_point;

				diff_points = BKE_mask_point_segment_diff_with_resolution(spline, cur_point, width, height,
				                                                          &tot_diff_point);

				if (diff_points) {
					int i, tot_feather_point, tot_point;
					float *feather_points = NULL, *points;

					if (feather) {
						feather_points = BKE_mask_point_segment_feather_diff_with_resolution(spline, cur_point,
						                                                                     width, height,
						                                                                     &tot_feather_point);

						points = feather_points;
						tot_point = tot_feather_point;
					}
					else {
						points = diff_points;
						tot_point = tot_diff_point;
					}

					for (i = 0; i < tot_point - 1; i++) {
						float cur_dist, a[2], b[2];

						a[0] = points[2 * i] * scalex;
						a[1] = points[2 * i + 1] * scaley;

						b[0] = points[2 * i + 2] * scalex;
						b[1] = points[2 * i + 3] * scaley;

						cur_dist = dist_to_line_segment_v2(co, a, b);

						if (point == NULL || cur_dist < dist) {
							if (tangent)
								sub_v2_v2v2(tangent, &diff_points[2 * i + 2], &diff_points[2 * i]);

							point_masklay = masklay;
							point_spline = spline;
							point = use_deform ? &spline->points[(cur_point - spline->points_deform)] : cur_point;
							dist = cur_dist;
							u = (float)i / tot_point;

						}
					}

					if (feather_points)
						MEM_freeN(feather_points);

					MEM_freeN(diff_points);
				}
			}
		}
	}

	if (point && dist < threshold) {
		if (masklay_r)
			*masklay_r = point_masklay;

		if (spline_r)
			*spline_r = point_spline;

		if (point_r)
			*point_r = point;

		if (u_r) {
			u = BKE_mask_spline_project_co(point_spline, point, u, normal_co, MASK_PROJ_ANY);

			*u_r = u;
		}

		return TRUE;
	}

	if (masklay_r)
		*masklay_r = NULL;

	if (spline_r)
		*spline_r = NULL;

	if (point_r)
		*point_r = NULL;

	return FALSE;
}

/******************** add vertex *********************/

static void setup_vertex_point(bContext *C, Mask *mask, MaskSpline *spline, MaskSplinePoint *new_point,
                               const float point_co[2], const float tangent[2], const float u,
                               MaskSplinePoint *reference_point, const short reference_adjacent)
{
	MaskSplinePoint *prev_point = NULL;
	MaskSplinePoint *next_point = NULL;
	BezTriple *bezt;
	int width, height;
	float co[3];
	const float len = 20.0; /* default length of handle in pixel space */

	copy_v2_v2(co, point_co);
	co[2] = 0.0f;

	ED_mask_size(C, &width, &height);

	/* point coordinate */
	bezt = &new_point->bezt;

	bezt->h1 = bezt->h2 = HD_ALIGN;

	if (reference_point) {
		bezt->h1 = bezt->h2 = MAX2(reference_point->bezt.h2, reference_point->bezt.h1);
	}
	else if (reference_adjacent) {
		if (spline->tot_point != 1) {
			int index = (int)(new_point - spline->points);
			prev_point = &spline->points[(index - 1) % spline->tot_point];
			next_point = &spline->points[(index + 1) % spline->tot_point];

			bezt->h1 = bezt->h2 = MAX2(prev_point->bezt.h2, next_point->bezt.h1);

			/* note, we may want to copy other attributes later, radius? pressure? color? */
		}
	}

	copy_v3_v3(bezt->vec[0], co);
	copy_v3_v3(bezt->vec[1], co);
	copy_v3_v3(bezt->vec[2], co);

	/* initial offset for handles */
	if (spline->tot_point == 1) {
		/* first point of splien is aligned horizontally */
		bezt->vec[0][0] -= len / width;
		bezt->vec[2][0] += len / width;
	}
	else if (tangent) {
		float vec[2];

		copy_v2_v2(vec, tangent);

		vec[0] *= width;
		vec[1] *= height;

		mul_v2_fl(vec, len / len_v2(vec));

		vec[0] /= width;
		vec[1] /= height;

		sub_v2_v2(bezt->vec[0], vec);
		add_v2_v2(bezt->vec[2], vec);

		if (reference_adjacent) {
			BKE_mask_calc_handle_adjacent_interp(spline, new_point, u);
		}
	}
	else {

		/* calculating auto handles works much nicer */
#if 0
		/* next points are aligning in the direction of previous/next point */
		MaskSplinePoint *point;
		float v1[2], v2[2], vec[2];
		float dir = 1.0f;

		if (new_point == spline->points) {
			point = new_point + 1;
			dir = -1.0f;
		}
		else
			point = new_point - 1;

		if (spline->tot_point < 3) {
			v1[0] = point->bezt.vec[1][0] * width;
			v1[1] = point->bezt.vec[1][1] * height;

			v2[0] = new_point->bezt.vec[1][0] * width;
			v2[1] = new_point->bezt.vec[1][1] * height;
		}
		else {
			if (new_point == spline->points) {
				v1[0] = spline->points[1].bezt.vec[1][0] * width;
				v1[1] = spline->points[1].bezt.vec[1][1] * height;

				v2[0] = spline->points[spline->tot_point - 1].bezt.vec[1][0] * width;
				v2[1] = spline->points[spline->tot_point - 1].bezt.vec[1][1] * height;
			}
			else {
				v1[0] = spline->points[0].bezt.vec[1][0] * width;
				v1[1] = spline->points[0].bezt.vec[1][1] * height;

				v2[0] = spline->points[spline->tot_point - 2].bezt.vec[1][0] * width;
				v2[1] = spline->points[spline->tot_point - 2].bezt.vec[1][1] * height;
			}
		}

		sub_v2_v2v2(vec, v1, v2);
		mul_v2_fl(vec, len * dir / len_v2(vec));

		vec[0] /= width;
		vec[1] /= height;

		add_v2_v2(bezt->vec[0], vec);
		sub_v2_v2(bezt->vec[2], vec);
#else
		BKE_mask_calc_handle_point_auto(spline, new_point, TRUE);
		BKE_mask_calc_handle_adjacent_interp(spline, new_point, u);

#endif
	}

	BKE_mask_parent_init(&new_point->parent);

	/* select new point */
	MASKPOINT_SEL_ALL(new_point);
	ED_mask_select_flush_all(mask);
}


/* **** add extrude vertex **** */

static void finSelectedSplinePoint(MaskLayer *masklay, MaskSpline **spline, MaskSplinePoint **point, short check_active)
{
	MaskSpline *cur_spline = masklay->splines.first;

	*spline = NULL;
	*point = NULL;

	if (check_active) {
		if (masklay->act_spline && masklay->act_point && MASKPOINT_ISSEL_ANY(masklay->act_point)) {
			*spline = masklay->act_spline;
			*point = masklay->act_point;
			return;
		}
	}

	while (cur_spline) {
		int i;

		for (i = 0; i < cur_spline->tot_point; i++) {
			MaskSplinePoint *cur_point = &cur_spline->points[i];

			if (MASKPOINT_ISSEL_ANY(cur_point)) {
				if (*spline != NULL && *spline != cur_spline) {
					*spline = NULL;
					*point = NULL;
					return;
				}
				else if (*point) {
					*point = NULL;
				}
				else {
					*spline = cur_spline;
					*point = cur_point;
				}
			}
		}

		cur_spline = cur_spline->next;
	}
}

/* **** add subdivide vertex **** */

static void mask_spline_add_point_at_index(MaskSpline *spline, int point_index)
{
	MaskSplinePoint *new_point_array;

	new_point_array = MEM_callocN(sizeof(MaskSplinePoint) * (spline->tot_point + 1), "add mask vert points");

	memcpy(new_point_array, spline->points, sizeof(MaskSplinePoint) * (point_index + 1));
	memcpy(new_point_array + point_index + 2, spline->points + point_index + 1,
	       sizeof(MaskSplinePoint) * (spline->tot_point - point_index - 1));

	MEM_freeN(spline->points);
	spline->points = new_point_array;
	spline->tot_point++;
}

static int add_vertex_subdivide(bContext *C, Mask *mask, const float co[2])
{
	MaskLayer *masklay;
	MaskSpline *spline;
	MaskSplinePoint *point = NULL;
	const float threshold = 9;
	float tangent[2];
	float u;

	if (find_nearest_diff_point(C, mask, co, threshold, FALSE, &masklay, &spline, &point, &u, tangent, TRUE)) {
		MaskSplinePoint *new_point;
		int point_index = point - spline->points;

		ED_mask_select_toggle_all(mask, SEL_DESELECT);

		mask_spline_add_point_at_index(spline, point_index);

		new_point = &spline->points[point_index + 1];

		setup_vertex_point(C, mask, spline, new_point, co, tangent, u, NULL, TRUE);

		/* TODO - we could pass the spline! */
		BKE_mask_layer_shape_changed_add(masklay, BKE_mask_layer_shape_spline_to_index(masklay, spline) + point_index + 1, TRUE, TRUE);

		masklay->act_point = new_point;

		WM_event_add_notifier(C, NC_MASK | NA_EDITED, mask);

		return TRUE;
	}

	return FALSE;
}

static int add_vertex_extrude(bContext *C, Mask *mask, MaskLayer *masklay, const float co[2])
{
	MaskSpline *spline;
	MaskSplinePoint *point;
	MaskSplinePoint *new_point = NULL, *ref_point = NULL;

	/* check on which side we want to add the point */
	int point_index;
	float tangent_point[2];
	float tangent_co[2];
	int do_cyclic_correct = FALSE;
	int do_recalc_src = FALSE;  /* when extruding from endpoints only */
	int do_prev;                /* use prev point rather then next?? */

	if (!masklay) {
		return FALSE;
	}
	else {
		finSelectedSplinePoint(masklay, &spline, &point, TRUE);
	}

	ED_mask_select_toggle_all(mask, SEL_DESELECT);

	point_index = (point - spline->points);

	MASKPOINT_DESEL_ALL(point);

	if ((spline->flag & MASK_SPLINE_CYCLIC) ||
	    (point_index > 0 && point_index != spline->tot_point - 1))
	{
		BKE_mask_calc_tangent_polyline(spline, point, tangent_point);
		sub_v2_v2v2(tangent_co, co, point->bezt.vec[1]);

		if (dot_v2v2(tangent_point, tangent_co) < 0.0f) {
			do_prev = TRUE;
		}
		else {
			do_prev = FALSE;
		}
	}
	else if (((spline->flag & MASK_SPLINE_CYCLIC) == 0) && (point_index == 0)) {
		do_prev = TRUE;
		do_recalc_src = TRUE;
	}
	else if (((spline->flag & MASK_SPLINE_CYCLIC) == 0) && (point_index == spline->tot_point - 1)) {
		do_prev = FALSE;
		do_recalc_src = TRUE;
	}
	else {
		/* should never get here */
		BLI_assert(0);
	}

	/* use the point before the active one */
	if (do_prev) {
		point_index--;
		if (point_index < 0) {
			point_index += spline->tot_point; /* wrap index */
			if ((spline->flag & MASK_SPLINE_CYCLIC) == 0) {
				do_cyclic_correct = TRUE;
				point_index = 0;
			}
		}
	}

//		print_v2("", tangent_point);
//		printf("%d\n", point_index);

	mask_spline_add_point_at_index(spline, point_index);

	if (do_cyclic_correct) {
		ref_point = &spline->points[point_index + 1];
		new_point = &spline->points[point_index];
		*ref_point = *new_point;
		memset(new_point, 0, sizeof(*new_point));
	}
	else {
		ref_point = &spline->points[point_index];
		new_point = &spline->points[point_index + 1];
	}

	masklay->act_point = new_point;

	setup_vertex_point(C, mask, spline, new_point, co, NULL, 0.5f, ref_point, FALSE);

	if (masklay->splines_shapes.first) {
		point_index = (((int)(new_point - spline->points) + 0) % spline->tot_point);
		BKE_mask_layer_shape_changed_add(masklay, BKE_mask_layer_shape_spline_to_index(masklay, spline) + point_index, TRUE, TRUE);
	}

	if (do_recalc_src) {
		/* TODO, update keyframes in time */
		BKE_mask_calc_handle_point_auto(spline, ref_point, FALSE);
	}

	WM_event_add_notifier(C, NC_MASK | NA_EDITED, mask);

	return TRUE;
}

static int add_vertex_new(bContext *C, Mask *mask, MaskLayer *masklay, const float co[2])
{
	MaskSpline *spline;
	MaskSplinePoint *point;
	MaskSplinePoint *new_point = NULL, *ref_point = NULL;

	if (!masklay) {
		/* if there's no masklay currently operationg on, create new one */
		masklay = BKE_mask_layer_new(mask, "");
		mask->masklay_act = mask->masklay_tot - 1;
		spline = NULL;
		point = NULL;
	}
	else {
		finSelectedSplinePoint(masklay, &spline, &point, TRUE);
	}

	ED_mask_select_toggle_all(mask, SEL_DESELECT);

	if (!spline) {
		/* no selected splines in active masklay, create new spline */
		spline = BKE_mask_spline_add(masklay);
	}

	masklay->act_spline = spline;
	new_point = spline->points;

	masklay->act_point = new_point;

	setup_vertex_point(C, mask, spline, new_point, co, NULL, 0.5f, ref_point, FALSE);

	{
		int point_index = (((int)(new_point - spline->points) + 0) % spline->tot_point);
		BKE_mask_layer_shape_changed_add(masklay, BKE_mask_layer_shape_spline_to_index(masklay, spline) + point_index, TRUE, TRUE);
	}

	WM_event_add_notifier(C, NC_MASK | NA_EDITED, mask);

	return TRUE;
}

static int add_vertex_exec(bContext *C, wmOperator *op)
{
	Mask *mask = CTX_data_edit_mask(C);
	MaskLayer *masklay;

	float co[2];

	masklay = BKE_mask_layer_active(mask);

	if (masklay && masklay->restrictflag & (MASK_RESTRICT_VIEW | MASK_RESTRICT_SELECT)) {
		masklay = NULL;
	}

	RNA_float_get_array(op->ptr, "location", co);

	if (masklay && masklay->act_point && MASKPOINT_ISSEL_ANY(masklay->act_point)) {

		/* cheap trick - double click for cyclic */
		MaskSpline *spline = masklay->act_spline;
		MaskSplinePoint *point = masklay->act_point;

		int is_sta = (point == spline->points);
		int is_end = (point == &spline->points[spline->tot_point - 1]);

		/* then check are we overlapping the mouse */
		if ((is_sta || is_end) && equals_v2v2(co, point->bezt.vec[1])) {
			if (spline->flag & MASK_SPLINE_CYCLIC) {
				/* nothing to do */
				return OPERATOR_CANCELLED;
			}
			else {
				/* recalc the connecting point as well to make a nice even curve */
				MaskSplinePoint *point_other = is_end ? spline->points : &spline->points[spline->tot_point - 1];
				spline->flag |= MASK_SPLINE_CYCLIC;

				/* TODO, update keyframes in time */
				BKE_mask_calc_handle_point_auto(spline, point, FALSE);
				BKE_mask_calc_handle_point_auto(spline, point_other, FALSE);

				/* TODO: only update this spline */
				BKE_mask_update_display(mask, CTX_data_scene(C)->r.cfra);

				WM_event_add_notifier(C, NC_MASK | NA_EDITED, mask);
				return OPERATOR_FINISHED;
			}
		}

		if (!add_vertex_subdivide(C, mask, co)) {
			if (!add_vertex_extrude(C, mask, masklay, co)) {
				return OPERATOR_CANCELLED;
			}
		}
	}
	else {
		if (!add_vertex_subdivide(C, mask, co)) {
			if (!add_vertex_new(C, mask, masklay, co)) {
				return OPERATOR_CANCELLED;
			}
		}
	}

	/* TODO: only update this spline */
	BKE_mask_update_display(mask, CTX_data_scene(C)->r.cfra);

	return OPERATOR_FINISHED;
}

static int add_vertex_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	float co[2];

	ED_mask_mouse_pos(C, event, co);

	RNA_float_set_array(op->ptr, "location", co);

	return add_vertex_exec(C, op);
}

void MASK_OT_add_vertex(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Add Vertex";
	ot->description = "Add vertex to active spline";
	ot->idname = "MASK_OT_add_vertex";

	/* api callbacks */
	ot->exec = add_vertex_exec;
	ot->invoke = add_vertex_invoke;
	ot->poll = ED_maskedit_mask_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* properties */
	RNA_def_float_vector(ot->srna, "location", 2, NULL, -FLT_MIN, FLT_MAX,
	                     "Location", "Location of vertex in normalized space", -1.0f, 1.0f);
}

/******************** add feather vertex *********************/

static int add_feather_vertex_exec(bContext *C, wmOperator *op)
{
	Mask *mask = CTX_data_edit_mask(C);
	MaskLayer *masklay;
	MaskSpline *spline;
	MaskSplinePoint *point = NULL;
	const float threshold = 9;
	float co[2], u;

	RNA_float_get_array(op->ptr, "location", co);

	point = ED_mask_point_find_nearest(C, mask, co, threshold, NULL, NULL, NULL, NULL);
	if (point)
		return OPERATOR_FINISHED;

	if (find_nearest_diff_point(C, mask, co, threshold, TRUE, &masklay, &spline, &point, &u, NULL, TRUE)) {
		Scene *scene = CTX_data_scene(C);
		float w = BKE_mask_point_weight(spline, point, u);
		float weight_scalar = BKE_mask_point_weight_scalar(spline, point, u);

		if (weight_scalar != 0.0f) {
			w = w / weight_scalar;
		}

		BKE_mask_point_add_uw(point, u, w);

		BKE_mask_update_display(mask, scene->r.cfra);

		WM_event_add_notifier(C, NC_MASK | NA_EDITED, mask);

		DAG_id_tag_update(&mask->id, 0);

		return OPERATOR_FINISHED;
	}

	return OPERATOR_CANCELLED;
}

static int add_feather_vertex_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	float co[2];

	ED_mask_mouse_pos(C, event, co);

	RNA_float_set_array(op->ptr, "location", co);

	return add_feather_vertex_exec(C, op);
}

void MASK_OT_add_feather_vertex(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Add feather Vertex";
	ot->description = "Add vertex to feather";
	ot->idname = "MASK_OT_add_feather_vertex";

	/* api callbacks */
	ot->exec = add_feather_vertex_exec;
	ot->invoke = add_feather_vertex_invoke;
	ot->poll = ED_maskedit_mask_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* properties */
	RNA_def_float_vector(ot->srna, "location", 2, NULL, -FLT_MIN, FLT_MAX,
	                     "Location", "Location of vertex in normalized space", -1.0f, 1.0f);
}
