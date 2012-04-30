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

/** \file blender/editors/mask/mask_ops.c
 *  \ingroup edmask
 */

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"
#include "BLI_listbase.h"
#include "BLI_math.h"

#include "BKE_context.h"
#include "BKE_curve.h"
#include "BKE_depsgraph.h"
#include "BKE_mask.h"

#include "DNA_mask_types.h"
#include "DNA_object_types.h"	/* SELECT */

#include "WM_api.h"
#include "WM_types.h"

#include "ED_screen.h"
#include "ED_mask.h"
#include "ED_clip.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "mask_intern.h"	// own include

/******************** utility functions *********************/

static void spline_point_select(MaskSplinePoint *point, int action)
{
	int i;

	switch (action) {
		case SEL_SELECT:
			MASKPOINT_SEL(point);
			break;
		case SEL_DESELECT:
			MASKPOINT_DESEL(point);
			break;
		case SEL_INVERT:
			MASKPOINT_INVSEL(point);
			break;
	}

	for (i = 0; i < point->tot_uw; i++) {
		switch (action) {
			case SEL_SELECT:
				point->uw[i].flag |= SELECT;
				break;
			case SEL_DESELECT:
				point->uw[i].flag &= ~SELECT;
				break;
			case SEL_INVERT:
				point->uw[i].flag ^= SELECT;
				break;
		}
	}
}

static float projection_on_spline(MaskSpline *spline, MaskSplinePoint *point, float aspx, float aspy, float start_u, float co[2])
{
	const int N = 1000;
	float u = -1.0f, du = 1.0f / N, u1 = start_u, u2 = start_u;
	float ang = -1.0f;

	while (u1 > 0.0f || u2 < 1.0d) {
		float n1[2], n2[2], co1[2], co2[2];
		float v1[2], v2[2];
		float ang1, ang2;

		if (u1 >= 0.0f) {
			BKE_mask_point_segment_co(spline, point, u1, co1);
			BKE_mask_point_normal(spline, point, aspx, aspy, u1, n1);
			sub_v2_v2v2(v1, co, co1);

			if (len_v2(v1) > 1e-3) {
				ang1 = angle_v2v2(v1, n1);
				if (ang1 > M_PI / 2.0f)
					ang1 = M_PI  - ang1;

				if (ang < 0.0f || ang1 < ang) {
					ang = ang1;
					u = u1;
				}
			}
			else {
				u = u1;
				break;
			}
		}

		if (u2 <= 1.0f) {
			BKE_mask_point_segment_co(spline, point, u2, co2);
			BKE_mask_point_normal(spline, point, aspx, aspy, u2, n2);
			sub_v2_v2v2(v2, co, co2);

			if (len_v2(v2) > 1e-3) {
				ang2 = angle_v2v2(v2, n2);
				if (ang2 > M_PI / 2.0f)
					ang2 = M_PI  - ang2;

				if (ang2 < ang) {
					ang = ang2;
					u = u2;
				}
			}
			else {
				u = u2;
				break;
			}
		}

		u1 -= du;
		u2 += du;
	}

	return u;
}

static int points_has_selection(MaskSplinePoint *points, int tot_point)
{
	int i;

	for (i = 0; i < tot_point; i++) {
		MaskSplinePoint *point = &points[i];

		if (MASKPOINT_ISSEL(point))
			return TRUE;
	}

	return FALSE;
}

static int mask_has_selection(Mask *mask)
{
	MaskShape *shape = mask->shapes.first;

	while (shape) {
		MaskSpline *spline = shape->splines.first;

		while (spline) {
			if (points_has_selection(spline->points, spline->tot_point))
				return TRUE;

			spline = spline->next;
		}

		shape = shape->next;
	}

	return FALSE;
}

static void toggle_selection_all(Mask *mask, int action)
{
	MaskShape *shape = mask->shapes.first;

	if (action == SEL_TOGGLE) {
		if (mask_has_selection(mask))
			action = SEL_DESELECT;
		else
			action = SEL_SELECT;
	}

	while (shape) {
		MaskSpline *spline = shape->splines.first;

		while (spline) {
			int i;

			for (i = 0; i < spline->tot_point; i++) {
				MaskSplinePoint *point = &spline->points[i];

				spline_point_select(point, action);
			}

			spline = spline->next;
		}

		shape = shape->next;
	}
}

static MaskSplinePoint *find_nearest_point(bContext *C, Mask *mask, float normal_co[2], int threshold,
                                           MaskShape **shape_r, MaskSpline **spline_r, int *is_handle_r,
                                           float *score)
{
	MaskShape *shape;
	MaskShape *point_shape = NULL;
	MaskSpline *point_spline = NULL;
	MaskSplinePoint *point = NULL;
	float co[2], aspx, aspy;
	float len = FLT_MAX, scalex, scaley;
	int is_handle = FALSE, width, height;

	ED_mask_size(C, &width, &height);
	ED_mask_aspect(C, &aspx, &aspy);
	ED_mask_pixelspace_factor(C, &scalex, &scaley);

	co[0] = normal_co[0] * scalex;
	co[1] = normal_co[1] * scaley;

	shape = mask->shapes.first;
	while (shape) {
		MaskSpline *spline = shape->splines.first;

		while (spline) {
			int i;

			for (i = 0; i < spline->tot_point; i++) {
				MaskSplinePoint *cur_point = &spline->points[i];
				float cur_len, vec[2], handle[2];

				vec[0] = cur_point->bezt.vec[1][0] * scalex;
				vec[1] = cur_point->bezt.vec[1][1] * scaley;

				if (BKE_mask_point_has_handle(cur_point)) {
					BKE_mask_point_handle(cur_point, aspx, aspy, handle);
					handle[0] *= scalex;
					handle[1] *= scaley;

					cur_len = len_v2v2(co, handle);

					if (cur_len < len) {
						point_shape = shape;
						point_spline = spline;
						point = cur_point;
						len = cur_len;
						is_handle = TRUE;
					}
				}

				cur_len = len_v2v2(co, vec);

				if (cur_len < len) {
					point_spline = spline;
					point_shape = shape;
					point = cur_point;
					len = cur_len;
					is_handle = FALSE;
				}
			}

			spline = spline->next;
		}

		shape = shape->next;
	}

	if (len < threshold) {
		if (shape_r)
			*shape_r = point_shape;

		if (spline_r)
			*spline_r = point_spline;

		if (is_handle_r)
			*is_handle_r = is_handle;

		if (score)
			*score = len;

		return point;
	}

	if (shape_r)
		*shape_r = NULL;

	if (spline_r)
		*spline_r = NULL;

	if (is_handle_r)
		*is_handle_r = FALSE;

	return NULL;
}

static int find_nearest_feather(bContext *C, Mask *mask, float normal_co[2], int threshold,
                                MaskShape **shape_r, MaskSpline **spline_r, MaskSplinePoint **point_r,
                                MaskSplinePointUW **uw_r, float *score)
{
	MaskShape *shape, *point_shape = NULL;
	MaskSpline *point_spline = NULL;
	MaskSplinePoint *point = NULL;
	MaskSplinePointUW *uw = NULL;
	float len = FLT_MAX, co[2];
	float scalex, scaley, aspx, aspy;
	int width, height;

	ED_mask_size(C, &width, &height);
	ED_mask_aspect(C, &aspx, &aspy);
	ED_mask_pixelspace_factor(C, &scalex, &scaley);

	co[0] = normal_co[0] * scalex;
	co[1] = normal_co[1] * scaley;

	shape = mask->shapes.first;
	while (shape) {
		MaskSpline *spline = shape->splines.first;

		while (spline) {
			int i, tot_feather_point;
			float *feather_points, *fp;

			feather_points = fp = BKE_mask_spline_feather_points(spline, aspx, aspy, &tot_feather_point);

			for (i = 0; i < spline->tot_point; i++) {
				int j;
				MaskSplinePoint *cur_point = &spline->points[i];

				for (j = 0; j < cur_point->tot_uw + 1; j++) {
					float cur_len, vec[2];

					vec[0] = fp[0] * scalex;
					vec[1] = fp[1] * scaley;

					cur_len = len_v2v2(vec, co);

					if (point == NULL || cur_len < len) {
						if (j == 0)
							uw = NULL;
						else
							uw = &cur_point->uw[j - 1];

						point_shape = shape;
						point_spline = spline;
						point = cur_point;
						len = cur_len;
					}

					fp += 2;
				}
			}

			MEM_freeN(feather_points);

			spline = spline->next;
		}

		shape = shape->next;
	}

	if (len < threshold) {
		if (shape_r)
			*shape_r = point_shape;

		if (spline_r)
			*spline_r = point_spline;

		if (point_r)
			*point_r = point;

		if (uw_r)
			*uw_r = uw;

		if (score)
			*score = len;

		return TRUE;
	}

	if (shape_r)
		*shape_r = NULL;

	if (spline_r)
		*spline_r = NULL;

	if (point_r)
		*point_r = NULL;

	return FALSE;
}

static int find_nearest_diff_point(bContext *C, Mask *mask, float normal_co[2], int threshold, int feather,
                                   MaskShape **shape_r, MaskSpline **spline_r, MaskSplinePoint **point_r,
                                   float *u_r, float tangent[2])
{
	MaskShape *shape, *point_shape;
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

	shape = mask->shapes.first;
	while (shape) {
		MaskSpline *spline = shape->splines.first;

		while (spline) {
			int i;

			for (i = 0; i < spline->tot_point; i++) {
				MaskSplinePoint *cur_point = &spline->points[i];
				float *diff_points;
				int tot_diff_point;

				diff_points = BKE_mask_point_segment_diff(spline, cur_point, &tot_diff_point);

				if (diff_points) {
					int i, tot_feather_point, tot_point;
					float *feather_points = NULL, *points;

					if (feather) {
						feather_points = BKE_mask_point_segment_feather_diff(spline, cur_point,
								aspx, aspy, &tot_feather_point);

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

							point_shape = shape;
							point_spline = spline;
							point = cur_point;
							dist = cur_dist;
							u = (float)i / tot_point;

						}
					}

					if (feather_points)
						MEM_freeN(feather_points);
				}

				MEM_freeN(diff_points);
			}

			spline = spline->next;
		}

		shape = shape->next;
	}

	if (point && dist < threshold) {
		if (shape_r)
			*shape_r = point_shape;

		if (spline_r)
			*spline_r = point_spline;

		if (point_r)
			*point_r = point;

		if (u_r) {
			u = projection_on_spline(point_spline, point, aspx, aspy, u, normal_co);

			*u_r = u;
		}

		return TRUE;
	}

	if (shape_r)
		*shape_r = NULL;

	if (spline_r)
		*spline_r = NULL;

	if (point_r)
		*point_r = NULL;

	return FALSE;
}

static void mask_flush_selection(Mask *mask)
{
	MaskShape *shape;

	shape = mask->shapes.first;
	while (shape) {
		MaskSpline *spline = shape->splines.first;

		while (spline) {
			int i;

			spline->flag &= ~SELECT;

			for (i = 0; i < spline->tot_point; i++) {
				MaskSplinePoint *cur_point = &spline->points[i];

				if (MASKPOINT_ISSEL(cur_point)) {
					spline->flag |= SELECT;
				}
				else {
					int j;

					for (j = 0; j < cur_point->tot_uw; j++) {
						if (cur_point->uw[j].flag & SELECT) {
							spline->flag |= SELECT;
							break;
						}
					}
				}
			}

			spline = spline->next;
		}

		shape = shape->next;
	}
}

/******************** create new mask *********************/

static int mask_new_exec(bContext *C, wmOperator *op)
{
	SpaceClip *sc = CTX_wm_space_clip(C);
	Mask *mask;
	char name[MAX_ID_NAME-2];

	RNA_string_get(op->ptr, "name", name);

	mask = BKE_mask_new(name);

	if (sc)
		ED_space_clip_set_mask(C, sc, mask);

	return OPERATOR_FINISHED;
}

void MASK_OT_new(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "New Mask";
	ot->description = "Create new mask";
	ot->idname = "MASK_OT_new";

	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;

	/* api callbacks */
	ot->exec = mask_new_exec;
	ot->poll = ED_operator_mask;

	/* properties */
	RNA_def_string(ot->srna, "name", "", MAX_ID_NAME-2, "Name", "Name of new mask");
}

/******************** create new shape *********************/

static int shape_new_exec(bContext *C, wmOperator *op)
{
	Mask *mask = CTX_data_edit_mask(C);
	char name[MAX_ID_NAME-2];

	RNA_string_get(op->ptr, "name", name);

	BKE_mask_shape_new(mask, name);
	mask->shapenr = mask->tot_shape - 1;

	WM_event_add_notifier(C, NC_MASK|NA_EDITED, mask);

	return OPERATOR_FINISHED;
}

void MASK_OT_shape_new(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Add Shape";
	ot->description = "Add new shape for masking";
	ot->idname = "MASK_OT_shape_new";

	/* api callbacks */
	ot->exec = shape_new_exec;
	ot->poll = ED_maskediting_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;

	/* properties */
	RNA_def_string(ot->srna, "name", "", MAX_ID_NAME-2, "Name", "Name of new shape");
}

/******************** remove shape *********************/

static int shape_remove_exec(bContext *C, wmOperator *UNUSED(op))
{
	Mask *mask = CTX_data_edit_mask(C);
	MaskShape *shape = BKE_mask_shape_active(mask);

	if (shape) {
		BKE_mask_shape_remove(mask, shape);

		WM_event_add_notifier(C, NC_MASK|NA_EDITED, mask);
	}

	return OPERATOR_FINISHED;
}

void MASK_OT_shape_remove(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Remove Shape";
	ot->description = "Remove shape used for masking";
	ot->idname = "MASK_OT_shape_remove";

	/* api callbacks */
	ot->exec = shape_remove_exec;
	ot->poll = ED_maskediting_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;
}

/******************** slide *********************/

#define SLIDE_ACTION_NONE		0
#define SLIDE_ACTION_POINT		1
#define SLIDE_ACTION_HANDLE		2
#define SLIDE_ACTION_FEATHER	3

typedef struct SlidePointData {
	int action;

	float co[2];
	float vec[3][3];

	Mask *mask;
	MaskShape *shape;
	MaskSpline *spline;
	MaskSplinePoint *point;
	MaskSplinePointUW *uw;
	float handle[2], no[2], feather[2];
	float aspx, aspy;
	int width, height;
	float weight;

	short curvature_only, accurate;
} SlidePointData;

static void *slide_point_customdata(bContext *C, wmOperator *op, wmEvent *event)
{
	Mask *mask = CTX_data_edit_mask(C);
	SlidePointData *customdata = NULL;
	MaskShape *shape, *cv_shape, *feather_shape;
	MaskSpline *spline, *cv_spline, *feather_spline;
	MaskSplinePoint *point, *cv_point, *feather_point;
	MaskSplinePointUW *uw = NULL;
	int is_handle = FALSE, width, height, action = SLIDE_ACTION_NONE;
	int slide_feather = RNA_boolean_get(op->ptr, "slide_feather");
	float co[2], cv_score, feather_score;
	const float threshold = 19;

	ED_mask_mouse_pos(C, event, co);
	ED_mask_size(C, &width, &height);

	cv_point = find_nearest_point(C, mask, co, threshold, &cv_shape, &cv_spline, &is_handle, &cv_score);

	if (find_nearest_feather(C, mask, co, threshold, &feather_shape, &feather_spline, &feather_point, &uw, &feather_score)) {
		if (slide_feather || !cv_point || feather_score < cv_score) {
			action = SLIDE_ACTION_FEATHER;

			shape = feather_shape;
			spline = feather_spline;
			point = feather_point;
		}
	}

	if (cv_point && action == SLIDE_ACTION_NONE) {
		if (is_handle)
			action = SLIDE_ACTION_HANDLE;
		else
			action = SLIDE_ACTION_POINT;

		shape = cv_shape;
		spline = cv_spline;
		point = cv_point;
	}

	if (action != SLIDE_ACTION_NONE) {
		customdata = MEM_callocN(sizeof(SlidePointData), "mask slide point data");

		customdata->mask = mask;
		customdata->shape = shape;
		customdata->spline = spline;
		customdata->point = point;
		customdata->width = width;
		customdata->height = height;
		customdata->action = action;
		customdata->uw = uw;

		ED_mask_aspect(C, &customdata->aspx, &customdata->aspy);

		if (uw) {
			float co[2];

			customdata->weight = point->bezt.weight;

			customdata->weight = uw->w;
			BKE_mask_point_segment_co(spline, point, uw->u, co);
			BKE_mask_point_normal(spline, point, customdata->aspx, customdata->aspy, uw->u, customdata->no);

			customdata->feather[0] = co[0] + customdata->no[0] * uw->w;
			customdata->feather[1] = co[1] + customdata->no[1] * uw->w;
		}
		else {
			BezTriple *bezt = &point->bezt;
			BKE_mask_point_normal(spline, point, customdata->aspx, customdata->aspy, 0.0f, customdata->no);

			customdata->feather[0] = bezt->vec[1][0] + customdata->no[0] * bezt->weight;
			customdata->feather[1] = bezt->vec[1][1] + customdata->no[1] * bezt->weight;
		}

		copy_m3_m3(customdata->vec, point->bezt.vec);
		if (BKE_mask_point_has_handle(point))
			BKE_mask_point_handle(point, customdata->aspx, customdata->aspy, customdata->handle);
		ED_mask_mouse_pos(C, event, customdata->co);
	}

	return customdata;
}

static int slide_point_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	SlidePointData *slidedata = slide_point_customdata(C, op, event);

	if (slidedata) {
		Mask *mask = CTX_data_edit_mask(C);

		op->customdata = slidedata;

		WM_event_add_modal_handler(C, op);

		if (slidedata->uw) {
			if ((slidedata->uw->flag & SELECT) == 0) {
				toggle_selection_all(mask, SEL_DESELECT);

				slidedata->uw->flag |= SELECT;

				mask_flush_selection(mask);
			}
		}
		else if (!MASKPOINT_ISSEL(slidedata->point)) {
			toggle_selection_all(mask, SEL_DESELECT);

			spline_point_select(slidedata->point, SEL_SELECT);

			mask_flush_selection(mask);
		}

		slidedata->shape->act_spline = slidedata->spline;
		slidedata->shape->act_point = slidedata->point;

		WM_event_add_notifier(C, NC_MASK|ND_SELECT, mask);

		return OPERATOR_RUNNING_MODAL;
	}

	return OPERATOR_PASS_THROUGH;
}

static void cancel_slide_point(SlidePointData *data)
{
	/* cancel sliding */
	if (data->action == SLIDE_ACTION_FEATHER) {
		if (data->uw)
			data->uw->w = data->weight;
		else
			data->point->bezt.weight = data->weight;
	}
	else {
		copy_m3_m3(data->point->bezt.vec, data->vec);
	}
}

static void free_slide_point_data(SlidePointData *data)
{
	MEM_freeN(data);
}

static int slide_point_modal(bContext *C, wmOperator *op, wmEvent *event)
{
	SlidePointData *data = (SlidePointData *)op->customdata;
	BezTriple *bezt = &data->point->bezt;
	float co[2], dco[2];

	switch(event->type) {
		case LEFTCTRLKEY:
		case RIGHTCTRLKEY:
		case LEFTSHIFTKEY:
		case RIGHTSHIFTKEY:
			if (ELEM(event->type, LEFTCTRLKEY, RIGHTCTRLKEY))
				data->curvature_only = event->val==KM_PRESS;

			if (ELEM(event->type, LEFTSHIFTKEY, RIGHTSHIFTKEY))
				data->accurate = event->val==KM_PRESS;

			/* no break! update CV position */

		case MOUSEMOVE:
			ED_mask_mouse_pos(C, event, co);
			sub_v2_v2v2(dco, co, data->co);

			if (data->action == SLIDE_ACTION_HANDLE) {
				float delta[2], offco[2];

				sub_v2_v2v2(delta, data->handle, data->co);

				sub_v2_v2v2(offco, co, data->co);
				if (data->accurate)
					mul_v2_fl(offco, 0.2f);
				add_v2_v2(offco, data->co);
				add_v2_v2(offco, delta);

				BKE_mask_point_set_handle(data->point, offco, data->curvature_only, data->aspx, data->aspy, data->handle, data->vec);
			}
			else if (data->action == SLIDE_ACTION_POINT) {
				float delta[2];

				copy_v2_v2(delta, dco);
				if (data->accurate)
					mul_v2_fl(delta, 0.2f);

				add_v2_v2v2(bezt->vec[0], data->vec[0], delta);
				add_v2_v2v2(bezt->vec[1], data->vec[1], delta);
				add_v2_v2v2(bezt->vec[2], data->vec[2], delta);
			}
			else if (data->action == SLIDE_ACTION_FEATHER) {
				float vec[2], no[2], p[2], c[2], w, offco[2];
				float *weight;

				add_v2_v2v2(offco, data->feather, dco);

				if (data->uw) {
					float u = projection_on_spline(data->spline, data->point, data->aspx, data->aspy, data->uw->u, offco);

					if (u > 0.0f && u < 1.0f)
						data->uw->u = u;

					data->uw = BKE_mask_point_sort_uw(data->point, data->uw);
					weight = &data->uw->w;
					BKE_mask_point_normal(data->spline, data->point, data->aspx, data->aspy, data->uw->u, no);
					BKE_mask_point_segment_co(data->spline, data->point, data->uw->u, p);
				}
				else {
					weight = &bezt->weight;
					copy_v2_v2(no, data->no);
					copy_v2_v2(p, bezt->vec[1]);
				}

				sub_v2_v2v2(c, offco, p);
				project_v2_v2v2(vec, c, no);

				vec[0] *= data->aspx;
				vec[1] *= data->aspy;

				w = len_v2(vec);

				if (dot_v2v2(no, vec) > 0.0f)
					*weight = w;
				else
					*weight = 0;
			}

			WM_event_add_notifier(C, NC_MASK|NA_EDITED, data->mask);
			DAG_id_tag_update(&data->mask->id, 0);

			break;

		case LEFTMOUSE:
			if(event->val==KM_RELEASE) {
				free_slide_point_data(op->customdata);

				WM_event_add_notifier(C, NC_MASK|NA_EDITED, data->mask);
				DAG_id_tag_update(&data->mask->id, 0);

				return OPERATOR_FINISHED;
			}

			break;

		case ESCKEY:
			cancel_slide_point(op->customdata);

			free_slide_point_data(op->customdata);

			WM_event_add_notifier(C, NC_MASK|NA_EDITED, data->mask);
			DAG_id_tag_update(&data->mask->id, 0);

			return OPERATOR_CANCELLED;
	}

	return OPERATOR_RUNNING_MODAL;
}

void MASK_OT_slide_point(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Slide Point";
	ot->description = "Slide control points";
	ot->idname = "MASK_OT_slide_point";

	/* api callbacks */
	ot->invoke = slide_point_invoke;
	ot->modal = slide_point_modal;
	ot->poll = ED_maskediting_mask_poll;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;

	RNA_def_boolean(ot->srna, "slide_feather", 0, "Slide Feather", "First try to slide slide feather instead of vertex");
}

/******************** toggle selection *********************/

static int select_all_exec(bContext *C, wmOperator *op)
{
	Mask *mask = CTX_data_edit_mask(C);
	int action = RNA_enum_get(op->ptr, "action");

	toggle_selection_all(mask, action);
	mask_flush_selection(mask);

	WM_event_add_notifier(C, NC_MASK|ND_SELECT, mask);

	return OPERATOR_FINISHED;
}

void MASK_OT_select_all(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Select or Deselect All";
	ot->description = "Change selection of all curve points";
	ot->idname = "MASK_OT_select_all";

	/* api callbacks */
	ot->exec = select_all_exec;
	ot->poll = ED_maskediting_mask_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;

	/* properties */
	WM_operator_properties_select_all(ot);
}

/******************** select *********************/

static int select_exec(bContext *C, wmOperator *op)
{
	Mask *mask = CTX_data_edit_mask(C);
	MaskShape *shape;
	MaskSpline *spline;
	MaskSplinePoint *point = NULL;
	float co[2];
	int extend = RNA_boolean_get(op->ptr, "extend");
	int is_handle = 0;
	const float threshold = 19;

	RNA_float_get_array(op->ptr, "location", co);

	point = find_nearest_point(C, mask, co, threshold, &shape, &spline, &is_handle, NULL);

	if (point) {
		if (!extend)
			toggle_selection_all(mask, SEL_DESELECT);

		if (is_handle) {
			MASKPOINT_HANDLE_SEL(point);
		}
		else {
			spline_point_select(point, SEL_SELECT);
		}

		shape->act_spline = spline;
		shape->act_point = point;

		mask_flush_selection(mask);

		WM_event_add_notifier(C, NC_MASK|ND_SELECT, mask);
	}
	else {
		MaskSplinePointUW *uw;

		if (find_nearest_feather(C, mask, co, threshold, &shape, &spline, &point, &uw, NULL)) {
			if (!extend)
				toggle_selection_all(mask, SEL_DESELECT);

			uw->flag |= SELECT;

			shape->act_spline = spline;
			shape->act_point = point;

			mask_flush_selection(mask);

			WM_event_add_notifier(C, NC_MASK|ND_SELECT, mask);
		}
	}

	return OPERATOR_PASS_THROUGH;
}

static int select_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	float co[2];

	ED_mask_mouse_pos(C, event, co);

	RNA_float_set_array(op->ptr, "location", co);

	return select_exec(C, op);
}

void MASK_OT_select(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Select";
	ot->description = "Select spline points";
	ot->idname = "MASK_OT_select";

	/* api callbacks */
	ot->exec = select_exec;
	ot->invoke = select_invoke;
	ot->poll = ED_maskediting_mask_poll;

	/* flags */
	ot->flag = OPTYPE_UNDO;

	/* properties */
	RNA_def_boolean(ot->srna, "extend", 0,
		"Extend", "Extend selection rather than clearing the existing selection");
	RNA_def_float_vector(ot->srna, "location", 2, NULL, -FLT_MIN, FLT_MAX,
		"Location", "Location of vertex in normalized space", -1.0f, 1.0f);
}

/******************** add vertex *********************/

static void setup_vertex_point(bContext *C, Mask *mask, MaskSpline *spline, MaskSplinePoint *new_point,
                               float point_co[2], float tangent[2])
{
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
	} else {
		/* next points are aligning in the direction of previous/next point */
		MaskSplinePoint *point;
		float v1[2], v2[2], vec[2];
		float dir = 1.0f;

		if (new_point == spline->points) {
			point = new_point + 1;
			dir = -1.0f;
		} else
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
	}

	BKE_mask_parent_init(&new_point->parent);

	/* select new point */
	MASKPOINT_SEL(new_point);
	mask_flush_selection(mask);
}

/* **** add subdivide vertex **** */

static int add_vertex_subdivide(bContext *C, Mask *mask, float co[2])
{
	MaskShape *shape;
	MaskSpline *spline;
	MaskSplinePoint *point = NULL;
	const float threshold = 9;
	float tangent[2];

	if (find_nearest_diff_point(C, mask, co, threshold, FALSE, &shape, &spline, &point, NULL, tangent)) {
		MaskSplinePoint *new_point_array, *new_point;
		int point_index = point - spline->points;

		toggle_selection_all(mask, SEL_DESELECT);

		new_point_array = MEM_callocN(sizeof(MaskSplinePoint) * (spline->tot_point + 1), "add mask vert points");

		memcpy(new_point_array, spline->points, sizeof(MaskSplinePoint) * (point_index + 1));
		memcpy(new_point_array + point_index + 2, spline->points + point_index + 1,
		       sizeof(MaskSplinePoint) * (spline->tot_point - point_index - 1));

		MEM_freeN(spline->points);
		spline->points = new_point_array;
		spline->tot_point++;

		new_point = &new_point_array[point_index + 1];

		setup_vertex_point(C, mask, spline, new_point, co, tangent);

		shape->act_point = new_point;

		WM_event_add_notifier(C, NC_MASK|NA_EDITED, mask);

		return TRUE;
	}

	return FALSE;
}

/* **** add extrude vertex **** */

static void finSelectedSplinePoint(MaskShape *shape, MaskSpline **spline, MaskSplinePoint **point)
{
	MaskSpline *cur_spline = shape->splines.first;

	*spline = NULL;
	*point = NULL;

	while (cur_spline) {
		int i;

		for (i = 0; i < cur_spline->tot_point; i++) {
			MaskSplinePoint *cur_point = &cur_spline->points[i];

			if (MASKPOINT_ISSEL(cur_point)) {
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

static int add_vertex_extrude(bContext *C, Mask *mask, float co[2])
{
	MaskShape *shape;
	MaskSpline *spline;
	MaskSplinePoint *point, *new_point = NULL;

	shape = BKE_mask_shape_active(mask);

	if (!shape) {
		/* if there's no shape currently operationg on, create new one */
		shape = BKE_mask_shape_new(mask, "");
		mask->shapenr = mask->tot_shape - 1;
		spline = NULL;
		point = NULL;
	}
	else {
		finSelectedSplinePoint(shape, &spline, &point);
	}

	if (!spline) {
		/* no selected splines in actuve shape, create new spline */
		spline = BKE_mask_spline_add(shape);
		shape->act_spline = spline;
		new_point = spline->points;
	}

	if (!new_point) {
		MaskSplinePoint *new_point_array;

		if (point == &spline->points[spline->tot_point - 1]) {
			MASKPOINT_DESEL(point);
			new_point_array = MEM_callocN(sizeof(MaskSplinePoint) * (spline->tot_point + 1), "add mask vert points");
			memcpy(new_point_array, spline->points, sizeof(MaskSplinePoint) * spline->tot_point);
			MEM_freeN(spline->points);
			spline->points = new_point_array;
			spline->tot_point++;

			new_point = &spline->points[spline->tot_point - 1];
		}
		else if (point == &spline->points[0]) {
			MASKPOINT_DESEL(point);
			new_point_array = MEM_callocN(sizeof(MaskSplinePoint) * (spline->tot_point + 1), "add mask vert points");
			memcpy(new_point_array + 1, spline->points, sizeof(MaskSplinePoint) * spline->tot_point);
			MEM_freeN(spline->points);
			spline->points = new_point_array;
			spline->tot_point++;

			new_point = &spline->points[0];
		}
		else {
			spline = BKE_mask_spline_add(shape);
			shape->act_spline = spline;
			new_point = spline->points;
		}
	}

	shape->act_point = new_point;

	setup_vertex_point(C, mask, spline, new_point, co, NULL);
	WM_event_add_notifier(C, NC_MASK|NA_EDITED, mask);

	return TRUE;
}

static int add_vertex_exec(bContext *C, wmOperator *op)
{
	Mask *mask = CTX_data_edit_mask(C);
	float co[2];

	RNA_float_get_array(op->ptr, "location", co);

	if (!add_vertex_subdivide(C, mask, co)) {
		if (!add_vertex_extrude(C, mask, co))
			return OPERATOR_CANCELLED;
	}

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
	ot->poll = ED_maskediting_mask_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;

	/* properties */
	RNA_def_float_vector(ot->srna, "location", 2, NULL, -FLT_MIN, FLT_MAX,
		"Location", "Location of vertex in normalized space", -1.0f, 1.0f);
}

/******************** add feather vertex *********************/

static int add_feather_vertex_exec(bContext *C, wmOperator *op)
{
	Mask *mask = CTX_data_edit_mask(C);
	MaskShape *shape;
	MaskSpline *spline;
	MaskSplinePoint *point = NULL;
	const float threshold = 9;
	float co[2], u;

	RNA_float_get_array(op->ptr, "location", co);

	point = find_nearest_point(C, mask, co, threshold, NULL, NULL, NULL, NULL);
	if (point)
		return OPERATOR_FINISHED;

	if (find_nearest_diff_point(C, mask, co, threshold, TRUE, &shape, &spline, &point, &u, NULL)) {
		float w = BKE_mask_point_weight(spline, point, u);

		BKE_mask_point_add_uw(point, u, w);

		WM_event_add_notifier(C, NC_MASK|NA_EDITED, mask);

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
	ot->poll = ED_maskediting_mask_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;

	/* properties */
	RNA_def_float_vector(ot->srna, "location", 2, NULL, -FLT_MIN, FLT_MAX,
		"Location", "Location of vertex in normalized space", -1.0f, 1.0f);
}

/******************** toggle cyclic *********************/

static int cyclic_toggle_exec(bContext *C, wmOperator *UNUSED(op))
{
	Mask *mask = CTX_data_edit_mask(C);
	MaskShape *shape = mask->shapes.first;

	while (shape) {
		MaskSpline *spline = shape->splines.first;

		while (spline) {
			if (points_has_selection(spline->points, spline->tot_point))
				spline->flag ^= MASK_SPLINE_CYCLIC;

			spline = spline->next;
		}

		shape = shape->next;
	}

	WM_event_add_notifier(C, NC_MASK|NA_EDITED, mask);

	return OPERATOR_FINISHED;
}

void MASK_OT_cyclic_toggle(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Toggle Cyclic";
	ot->description = "Toggle cyclic for selected splines";
	ot->idname = "MASK_OT_cyclic_toggle";

	/* api callbacks */
	ot->exec = cyclic_toggle_exec;
	ot->poll = ED_maskediting_mask_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;
}

/******************** delete *********************/

static void delete_feather_points(MaskSplinePoint *point)
{
	int i, count = 0;

	if (!point->tot_uw)
		return;

	for (i = 0; i < point->tot_uw; i++) {
		if ((point->uw[i].flag & SELECT) == 0)
			count++;
	}

	if (count == 0) {
		MEM_freeN(point->uw);
		point->uw = NULL;
		point->tot_uw = 0;
	}
	else {
		MaskSplinePointUW *new_uw;
		int j = 0;

		new_uw = MEM_callocN(count * sizeof(MaskSplinePointUW), "new mask uw points");

		for (i = 0; i < point->tot_uw; i++) {
			if ((point->uw[i].flag & SELECT) == 0) {
				new_uw[j++] = point->uw[i];
			}
		}

		MEM_freeN(point->uw);

		point->uw = new_uw;
		point->tot_uw = count;
	}
}

static int delete_exec(bContext *C, wmOperator *UNUSED(op))
{
	Mask *mask = CTX_data_edit_mask(C);
	MaskShape *shape = mask->shapes.first;

	while (shape) {
		MaskSpline *spline = shape->splines.first;

		while (spline) {
			int i, count = 0;
			MaskSpline *next_spline = spline->next;

			/* count unselected points */
			for (i = 0; i < spline->tot_point; i++) {
				MaskSplinePoint *point = &spline->points[i];

				if (!MASKPOINT_ISSEL(point))
					count++;
			}

			if (count == 0) {
				/* delete the whole spline */
				BLI_remlink(&shape->splines, spline);
				BKE_mask_spline_free(spline);

				if (spline == shape->act_spline) {
					shape->act_spline = NULL;
					shape->act_point = NULL;
				}
			}
			else {
				MaskSplinePoint *new_points;
				int j;

				new_points = MEM_callocN(count*sizeof(MaskSplinePoint), "deleteMaskPoints");

				for (i = 0, j = 0; i < spline->tot_point; i++) {
					MaskSplinePoint *point = &spline->points[i];

					if (!MASKPOINT_ISSEL(point)) {
						if (point == shape->act_point)
							shape->act_point = &new_points[j];

						delete_feather_points(point);

						new_points[j] = *point;
						j++;
					}
					else {
						if (point == shape->act_point)
							shape->act_point = NULL;

						BKE_mask_point_free(point);
					}
				}

				MEM_freeN(spline->points);
				spline->points = new_points;
				spline->tot_point = j;

				mask_flush_selection(mask);
			}

			spline = next_spline;
		}

		shape = shape->next;
	}

	WM_event_add_notifier(C, NC_MASK|NA_EDITED, mask);

	return OPERATOR_FINISHED;
}

void MASK_OT_delete(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Delete";
	ot->description = "Delete selected control points or splines";
	ot->idname = "MASK_OT_delete";

	/* api callbacks */
	ot->invoke = WM_operator_confirm;
	ot->exec = delete_exec;
	ot->poll = ED_maskediting_mask_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;
}

/******************** set handle type *********************/

static int set_handle_type_exec(bContext *C, wmOperator *op)
{
	Mask *mask = CTX_data_edit_mask(C);
	MaskShape *shape = mask->shapes.first;
	int handle_type = RNA_enum_get(op->ptr, "type");

	while (shape) {
		MaskSpline *spline = shape->splines.first;
		int i;

		while (spline) {
			for (i = 0; i < spline->tot_point; i++) {
				MaskSplinePoint *point = &spline->points[i];

				if (MASKPOINT_ISSEL(point)) {
					BezTriple *bezt = &point->bezt;

					bezt->h1 = bezt->h2 = handle_type;
				}
			}

			spline = spline->next;
		}

		shape = shape->next;
	}

	WM_event_add_notifier(C, NC_MASK|ND_DATA, mask);
	DAG_id_tag_update(&mask->id, 0);

	return OPERATOR_FINISHED;
}

void MASK_OT_handle_type_set(wmOperatorType *ot)
{
	static EnumPropertyItem editcurve_handle_type_items[]= {
		{HD_AUTO, "AUTO", 0, "Auto", ""},
		{HD_VECT, "VECTOR", 0, "Vector", ""},
		{HD_ALIGN, "ALIGNED", 0, "Aligned", ""},
		{0, NULL, 0, NULL, NULL}};

	/* identifiers */
	ot->name = "Set Handle Type";
	ot->description = "Set type of handles for selected control points";
	ot->idname = "MASK_OT_handle_type_set";

	/* api callbacks */
	ot->invoke = WM_menu_invoke;
	ot->exec = set_handle_type_exec;
	ot->poll = ED_maskediting_mask_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;

	/* properties */
	ot->prop = RNA_def_enum(ot->srna, "type", editcurve_handle_type_items, 1, "Type", "Spline type");
}
