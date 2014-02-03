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
#include "DNA_screen_types.h"
#include "DNA_mask_types.h"
#include "DNA_object_types.h"  /* SELECT */

#include "WM_api.h"
#include "WM_types.h"

#include "ED_mask.h"  /* own include */
#include "ED_screen.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "mask_intern.h"  /* own include */


static int find_nearest_diff_point(const bContext *C, Mask *mask, const float normal_co[2], int threshold, int feather,
                                   MaskLayer **masklay_r, MaskSpline **spline_r, MaskSplinePoint **point_r,
                                   float *u_r, float tangent[2],
                                   const short use_deform)
{
	ScrArea *sa = CTX_wm_area(C);
	ARegion *ar = CTX_wm_region(C);

	MaskLayer *masklay, *point_masklay;
	MaskSpline *point_spline;
	MaskSplinePoint *point = NULL;
	float dist = FLT_MAX, co[2];
	int width, height;
	float u;
	float scalex, scaley;

	ED_mask_get_size(sa, &width, &height);
	ED_mask_pixelspace_factor(sa, ar, &scalex, &scaley);

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
				unsigned int tot_diff_point;

				diff_points = BKE_mask_point_segment_diff(spline, cur_point, width, height,
				                                          &tot_diff_point);

				if (diff_points) {
					int j, tot_point;
					unsigned int tot_feather_point;
					float *feather_points = NULL, *points;

					if (feather) {
						feather_points = BKE_mask_point_segment_feather_diff(spline, cur_point,
						                                                     width, height,
						                                                     &tot_feather_point);

						points = feather_points;
						tot_point = tot_feather_point;
					}
					else {
						points = diff_points;
						tot_point = tot_diff_point;
					}

					for (j = 0; j < tot_point - 1; j++) {
						float cur_dist, a[2], b[2];

						a[0] = points[2 * j] * scalex;
						a[1] = points[2 * j + 1] * scaley;

						b[0] = points[2 * j + 2] * scalex;
						b[1] = points[2 * j + 3] * scaley;

						cur_dist = dist_to_line_segment_v2(co, a, b);

						if (cur_dist < dist) {
							if (tangent)
								sub_v2_v2v2(tangent, &diff_points[2 * j + 2], &diff_points[2 * j]);

							point_masklay = masklay;
							point_spline = spline;
							point = use_deform ? &spline->points[(cur_point - spline->points_deform)] : cur_point;
							dist = cur_dist;
							u = (float)j / tot_point;

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

static void setup_vertex_point(Mask *mask, MaskSpline *spline, MaskSplinePoint *new_point,
                               const float point_co[2], const float tangent[2], const float u,
                               MaskSplinePoint *reference_point, const short reference_adjacent,
                               const float view_zoom)
{
	MaskSplinePoint *prev_point = NULL;
	MaskSplinePoint *next_point = NULL;
	BezTriple *bezt;
	float co[3];
	const float len = 10.0; /* default length of handle in pixel space */

	copy_v2_v2(co, point_co);
	co[2] = 0.0f;

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
		bezt->vec[0][0] -= len * view_zoom;
		bezt->vec[2][0] += len * view_zoom;
	}
	else if (tangent) {
		float vec[2];

		copy_v2_v2(vec, tangent);

		mul_v2_fl(vec, len);

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
		/* TODO, having an active point but no active spline is possible, why? */
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

static int add_vertex_subdivide(const bContext *C, Mask *mask, const float co[2])
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

		setup_vertex_point(mask, spline, new_point, co, tangent, u, NULL, TRUE, 1.0f);

		/* TODO - we could pass the spline! */
		BKE_mask_layer_shape_changed_add(masklay, BKE_mask_layer_shape_spline_to_index(masklay, spline) + point_index + 1, true, true);

		masklay->act_spline = spline;
		masklay->act_point = new_point;

		WM_event_add_notifier(C, NC_MASK | NA_EDITED, mask);

		return TRUE;
	}

	return FALSE;
}

static int add_vertex_extrude(const bContext *C, Mask *mask, MaskLayer *masklay, const float co[2])
{
	MaskSpline *spline;
	MaskSplinePoint *point;
	MaskSplinePoint *new_point = NULL, *ref_point = NULL;

	/* check on which side we want to add the point */
	int point_index;
	float tangent_point[2];
	float tangent_co[2];
	bool do_cyclic_correct = false;
	bool do_recalc_src = false;  /* when extruding from endpoints only */
	bool do_prev;                /* use prev point rather then next?? */

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
		do_prev = FALSE;  /* quiet warning */
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

	setup_vertex_point(mask, spline, new_point, co, NULL, 0.5f, ref_point, FALSE, 1.0f);

	if (masklay->splines_shapes.first) {
		point_index = (((int)(new_point - spline->points) + 0) % spline->tot_point);
		BKE_mask_layer_shape_changed_add(masklay, BKE_mask_layer_shape_spline_to_index(masklay, spline) + point_index, true, true);
	}

	if (do_recalc_src) {
		/* TODO, update keyframes in time */
		BKE_mask_calc_handle_point_auto(spline, ref_point, FALSE);
	}

	WM_event_add_notifier(C, NC_MASK | NA_EDITED, mask);

	return TRUE;
}

static int add_vertex_new(const bContext *C, Mask *mask, MaskLayer *masklay, const float co[2])
{
	MaskSpline *spline;
	MaskSplinePoint *point;
	MaskSplinePoint *new_point = NULL, *ref_point = NULL;
	float view_zoom;

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

	{
		ScrArea *sa = CTX_wm_area(C);
		ARegion *ar = CTX_wm_region(C);

		float zoom_x, zoom_y;
		/* calc view zoom in a simplistic way */
		ED_mask_zoom(sa, ar, &zoom_x, &zoom_y);

		view_zoom = zoom_x + zoom_y / 2.0f;
		view_zoom = 1.0f / view_zoom;

		/* arbitrary but gives good results */
		view_zoom /= 500.0f;
	}

	setup_vertex_point(mask, spline, new_point, co, NULL, 0.5f, ref_point, FALSE, view_zoom);

	{
		int point_index = (((int)(new_point - spline->points) + 0) % spline->tot_point);
		BKE_mask_layer_shape_changed_add(masklay, BKE_mask_layer_shape_spline_to_index(masklay, spline) + point_index, true, true);
	}

	WM_event_add_notifier(C, NC_MASK | NA_EDITED, mask);

	return TRUE;
}

static int add_vertex_exec(bContext *C, wmOperator *op)
{
	Scene *scene = CTX_data_scene(C);
	Mask *mask = CTX_data_edit_mask(C);
	MaskLayer *masklay;

	float co[2];

	if (mask == NULL) {
		/* if there's no active mask, create one */
		mask = ED_mask_new(C, NULL);
	}

	masklay = BKE_mask_layer_active(mask);

	if (masklay && masklay->restrictflag & (MASK_RESTRICT_VIEW | MASK_RESTRICT_SELECT)) {
		masklay = NULL;
	}

	RNA_float_get_array(op->ptr, "location", co);

	/* TODO, having an active point but no active spline is possible, why? */
	if (masklay && masklay->act_spline && masklay->act_point && MASKPOINT_ISSEL_ANY(masklay->act_point)) {

		/* cheap trick - double click for cyclic */
		MaskSpline *spline = masklay->act_spline;
		MaskSplinePoint *point = masklay->act_point;

		const bool is_sta = (point == spline->points);
		const bool is_end = (point == &spline->points[spline->tot_point - 1]);

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
				BKE_mask_update_display(mask, CFRA);

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
	BKE_mask_update_display(mask, CFRA);

	return OPERATOR_FINISHED;
}

static int add_vertex_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
	ScrArea *sa = CTX_wm_area(C);
	ARegion *ar = CTX_wm_region(C);

	float co[2];

	ED_mask_mouse_pos(sa, ar, event->mval, co);

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
	ot->poll = ED_operator_mask;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* properties */
	RNA_def_float_vector(ot->srna, "location", 2, NULL, -FLT_MAX, FLT_MAX,
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

static int add_feather_vertex_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
	ScrArea *sa = CTX_wm_area(C);
	ARegion *ar = CTX_wm_region(C);

	float co[2];

	ED_mask_mouse_pos(sa, ar, event->mval, co);

	RNA_float_set_array(op->ptr, "location", co);

	return add_feather_vertex_exec(C, op);
}

void MASK_OT_add_feather_vertex(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Add Feather Vertex";
	ot->description = "Add vertex to feather";
	ot->idname = "MASK_OT_add_feather_vertex";

	/* api callbacks */
	ot->exec = add_feather_vertex_exec;
	ot->invoke = add_feather_vertex_invoke;
	ot->poll = ED_maskedit_mask_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* properties */
	RNA_def_float_vector(ot->srna, "location", 2, NULL, -FLT_MAX, FLT_MAX,
	                     "Location", "Location of vertex in normalized space", -1.0f, 1.0f);
}

/******************** common primitive functions *********************/

static int create_primitive_from_points(bContext *C, wmOperator *op, const float (*points)[2],
                                        int num_points, char handle_type)
{
	ScrArea *sa = CTX_wm_area(C);
	Scene *scene = CTX_data_scene(C);
	Mask *mask;
	MaskLayer *mask_layer;
	MaskSpline *new_spline;
	float scale, location[2], frame_size[2];
	int i, width, height;
	int size = RNA_float_get(op->ptr, "size");

	ED_mask_get_size(sa, &width, &height);
	scale = (float)size / max_ii(width, height);

	/* Get location in mask space. */
	frame_size[0] = width;
	frame_size[1] = height;
	RNA_float_get_array(op->ptr, "location", location);
	location[0] /= width;
	location[1] /= height;
	BKE_mask_coord_from_frame(location, location, frame_size);

	/* Make it so new primitive is centered to mouse location. */
	location[0] -= 0.5f * scale;
	location[1] -= 0.5f * scale;

	mask_layer = ED_mask_layer_ensure(C);
	mask = CTX_data_edit_mask(C);

	ED_mask_select_toggle_all(mask, SEL_DESELECT);

	new_spline = BKE_mask_spline_add(mask_layer);
	new_spline->flag = MASK_SPLINE_CYCLIC | SELECT;
	new_spline->tot_point = num_points;
	new_spline->points = MEM_recallocN(new_spline->points,
	                                   sizeof(MaskSplinePoint) * new_spline->tot_point);

	mask_layer->act_spline = new_spline;
	mask_layer->act_point = NULL;

	for (i = 0; i < num_points; i++) {
		MaskSplinePoint *new_point = &new_spline->points[i];

		copy_v2_v2(new_point->bezt.vec[1], points[i]);
		mul_v2_fl(new_point->bezt.vec[1], scale);
		add_v2_v2(new_point->bezt.vec[1], location);

		new_point->bezt.h1 = handle_type;
		new_point->bezt.h2 = handle_type;
		BKE_mask_point_select_set(new_point, true);
	}

	WM_event_add_notifier(C, NC_MASK | NA_EDITED, mask);

	/* TODO: only update this spline */
	BKE_mask_update_display(mask, CFRA);

	return OPERATOR_FINISHED;
}

static int primitive_add_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
	ScrArea *sa = CTX_wm_area(C);
	float cursor[2];
	int width, height;

	ED_mask_get_size(sa, &width, &height);
	ED_mask_cursor_location_get(sa, cursor);

	cursor[0] *= width;
	cursor[1] *= height;

	RNA_float_set_array(op->ptr, "location", cursor);

	return op->type->exec(C, op);
}

static void define_prinitive_add_properties(wmOperatorType *ot)
{
	RNA_def_float(ot->srna, "size", 100, -FLT_MAX, FLT_MAX,
	                     "Size", "Size of new circle", -FLT_MAX, FLT_MAX);
	RNA_def_float_vector(ot->srna, "location", 2, NULL, -FLT_MAX, FLT_MAX,
	                     "Location", "Location of new circle", -FLT_MAX, FLT_MAX);
}

/******************** primitive add circle *********************/

static int primitive_circle_add_exec(bContext *C, wmOperator *op)
{
	const float points[4][2] = {{0.0f, 0.5f},
	                            {0.5f, 1.0f},
	                            {1.0f, 0.5f},
	                            {0.5f, 0.0f}};
	int num_points = sizeof(points) / (2 * sizeof(float));

	create_primitive_from_points(C, op, points, num_points, HD_AUTO);

	return OPERATOR_FINISHED;
}

void MASK_OT_primitive_circle_add(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Add Circle";
	ot->description = "Add new circle-shaped spline";
	ot->idname = "MASK_OT_primitive_circle_add";

	/* api callbacks */
	ot->exec = primitive_circle_add_exec;
	ot->invoke = primitive_add_invoke;
	ot->poll = ED_operator_mask;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* properties */
	define_prinitive_add_properties(ot);
}

/******************** primitive add suqare *********************/

static int primitive_square_add_exec(bContext *C, wmOperator *op)
{
	const float points[4][2] = {{0.0f, 0.0f},
	                            {0.0f, 1.0f},
	                            {1.0f, 1.0f},
	                            {1.0f, 0.0f}};
	int num_points = sizeof(points) / (2 * sizeof(float));

	create_primitive_from_points(C, op, points, num_points, HD_VECT);

	return OPERATOR_FINISHED;
}

void MASK_OT_primitive_square_add(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Add Square";
	ot->description = "Add new square-shaped spline";
	ot->idname = "MASK_OT_primitive_square_add";

	/* api callbacks */
	ot->exec = primitive_square_add_exec;
	ot->invoke = primitive_add_invoke;
	ot->poll = ED_operator_mask;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* properties */
	define_prinitive_add_properties(ot);
}
