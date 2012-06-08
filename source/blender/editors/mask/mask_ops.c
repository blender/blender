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

#include "BLI_listbase.h"
#include "BLI_math.h"

#include "BKE_context.h"
#include "BKE_depsgraph.h"
#include "BKE_mask.h"

#include "DNA_scene_types.h"
#include "DNA_mask_types.h"
#include "DNA_object_types.h"  /* SELECT */

#include "WM_api.h"
#include "WM_types.h"

#include "ED_screen.h"
#include "ED_mask.h"
#include "ED_clip.h"
#include "ED_keyframing.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "mask_intern.h"  /* own include */

/******************** utility functions *********************/

MaskSplinePoint *ED_mask_point_find_nearest(bContext *C, Mask *mask, float normal_co[2], int threshold,
                                            MaskLayer **masklay_r, MaskSpline **spline_r, int *is_handle_r,
                                            float *score)
{
	MaskLayer *masklay;
	MaskLayer *point_masklay = NULL;
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

	for (masklay = mask->masklayers.first; masklay; masklay = masklay->next) {
		MaskSpline *spline;

		if (masklay->restrictflag & (MASK_RESTRICT_VIEW | MASK_RESTRICT_SELECT)) {
			continue;
		}

		for (spline = masklay->splines.first; spline; spline = spline->next) {
			MaskSplinePoint *points_array = BKE_mask_spline_point_array(spline);

			int i;

			for (i = 0; i < spline->tot_point; i++) {
				MaskSplinePoint *cur_point = &spline->points[i];
				MaskSplinePoint *cur_point_deform = &points_array[i];
				float cur_len, vec[2], handle[2];

				vec[0] = cur_point_deform->bezt.vec[1][0] * scalex;
				vec[1] = cur_point_deform->bezt.vec[1][1] * scaley;

				if (BKE_mask_point_has_handle(cur_point)) {
					BKE_mask_point_handle(cur_point_deform, handle);
					handle[0] *= scalex;
					handle[1] *= scaley;

					cur_len = len_v2v2(co, handle);

					if (cur_len < len) {
						point_masklay = masklay;
						point_spline = spline;
						point = cur_point;
						len = cur_len;
						is_handle = TRUE;
					}
				}

				cur_len = len_v2v2(co, vec);

				if (cur_len < len) {
					point_spline = spline;
					point_masklay = masklay;
					point = cur_point;
					len = cur_len;
					is_handle = FALSE;
				}
			}
		}
	}

	if (len < threshold) {
		if (masklay_r)
			*masklay_r = point_masklay;

		if (spline_r)
			*spline_r = point_spline;

		if (is_handle_r)
			*is_handle_r = is_handle;

		if (score)
			*score = len;

		return point;
	}

	if (masklay_r)
		*masklay_r = NULL;

	if (spline_r)
		*spline_r = NULL;

	if (is_handle_r)
		*is_handle_r = FALSE;

	return NULL;
}

int ED_mask_feather_find_nearest(bContext *C, Mask *mask, float normal_co[2], int threshold,
                                 MaskLayer **masklay_r, MaskSpline **spline_r, MaskSplinePoint **point_r,
                                 MaskSplinePointUW **uw_r, float *score)
{
	MaskLayer *masklay, *point_masklay = NULL;
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

	for (masklay = mask->masklayers.first; masklay; masklay = masklay->next) {
		MaskSpline *spline;

		for (spline = masklay->splines.first; spline; spline = spline->next) {
			//MaskSplinePoint *points_array = BKE_mask_spline_point_array(spline);

			int i, tot_feather_point;
			float (*feather_points)[2], (*fp)[2];

			if (masklay->restrictflag & (MASK_RESTRICT_VIEW | MASK_RESTRICT_SELECT)) {
				continue;
			}

			feather_points = fp = BKE_mask_spline_feather_points(spline, &tot_feather_point);

			for (i = 0; i < spline->tot_point; i++) {
				int j;
				MaskSplinePoint *cur_point = &spline->points[i];

				for (j = 0; j < cur_point->tot_uw + 1; j++) {
					float cur_len, vec[2];

					vec[0] = (*fp)[0] * scalex;
					vec[1] = (*fp)[1] * scaley;

					cur_len = len_v2v2(vec, co);

					if (point == NULL || cur_len < len) {
						if (j == 0)
							uw = NULL;
						else
							uw = &cur_point->uw[j - 1];

						point_masklay = masklay;
						point_spline = spline;
						point = cur_point;
						len = cur_len;
					}

					fp++;
				}
			}

			MEM_freeN(feather_points);
		}
	}

	if (len < threshold) {
		if (masklay_r)
			*masklay_r = point_masklay;

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

	if (masklay_r)
		*masklay_r = NULL;

	if (spline_r)
		*spline_r = NULL;

	if (point_r)
		*point_r = NULL;

	return FALSE;
}


/******************** create new mask *********************/

static int mask_new_exec(bContext *C, wmOperator *op)
{
	SpaceClip *sc = CTX_wm_space_clip(C);
	Mask *mask;
	char name[MAX_ID_NAME - 2];

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
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* api callbacks */
	ot->exec = mask_new_exec;
	ot->poll = ED_operator_mask;

	/* properties */
	RNA_def_string(ot->srna, "name", "", MAX_ID_NAME - 2, "Name", "Name of new mask");
}

/******************** create new masklay *********************/

static int masklay_new_exec(bContext *C, wmOperator *op)
{
	Mask *mask = CTX_data_edit_mask(C);
	char name[MAX_ID_NAME - 2];

	RNA_string_get(op->ptr, "name", name);

	BKE_mask_layer_new(mask, name);
	mask->masklay_act = mask->masklay_tot - 1;

	WM_event_add_notifier(C, NC_MASK | NA_EDITED, mask);

	return OPERATOR_FINISHED;
}

void MASK_OT_layer_new(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Add Mask Layer";
	ot->description = "Add new mask layer for masking";
	ot->idname = "MASK_OT_layer_new";

	/* api callbacks */
	ot->exec = masklay_new_exec;
	ot->poll = ED_maskedit_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* properties */
	RNA_def_string(ot->srna, "name", "", MAX_ID_NAME - 2, "Name", "Name of new mask layer");
}

/******************** remove mask layer *********************/

static int masklay_remove_exec(bContext *C, wmOperator *UNUSED(op))
{
	Mask *mask = CTX_data_edit_mask(C);
	MaskLayer *masklay = BKE_mask_layer_active(mask);

	if (masklay) {
		BKE_mask_layer_remove(mask, masklay);

		WM_event_add_notifier(C, NC_MASK | NA_EDITED, mask);
	}

	return OPERATOR_FINISHED;
}

void MASK_OT_layer_remove(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Remove Mask Layer";
	ot->description = "Remove mask layer";
	ot->idname = "MASK_OT_layer_remove";

	/* api callbacks */
	ot->exec = masklay_remove_exec;
	ot->poll = ED_maskedit_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/******************** slide *********************/

enum {
	SLIDE_ACTION_NONE    = 0,
	SLIDE_ACTION_POINT   = 1,
	SLIDE_ACTION_HANDLE  = 2,
	SLIDE_ACTION_FEATHER = 3
};

typedef struct SlidePointData {
	int action;

	float co[2];
	float vec[3][3];

	Mask *mask;
	MaskLayer *masklay;
	MaskSpline *spline, *orig_spline;
	MaskSplinePoint *point;
	MaskSplinePointUW *uw;
	float handle[2], no[2], feather[2];
	int width, height;
	float weight;

	short curvature_only, accurate;
	short initial_feather, overall_feather;
} SlidePointData;

static int slide_point_check_initial_feather(MaskSpline *spline)
{
	int i;

	for (i = 0; i < spline->tot_point; i++) {
		MaskSplinePoint *point = &spline->points[i];

		if (point->bezt.weight != 0.0f)
			return FALSE;

		/* comment for now. if all bezt weights are zero - this is as good-as initial */
#if 0
		int j;
		for (j = 0; j < point->tot_uw; j++) {
			if (point->uw[j].w != 0.0f)
				return FALSE;
		}
#endif
	}

	return TRUE;
}

static void *slide_point_customdata(bContext *C, wmOperator *op, wmEvent *event)
{
	Mask *mask = CTX_data_edit_mask(C);
	SlidePointData *customdata = NULL;
	MaskLayer *masklay, *cv_masklay, *feather_masklay;
	MaskSpline *spline, *cv_spline, *feather_spline;
	MaskSplinePoint *point, *cv_point, *feather_point;
	MaskSplinePointUW *uw = NULL;
	int is_handle = FALSE, width, height, action = SLIDE_ACTION_NONE;
	int slide_feather = RNA_boolean_get(op->ptr, "slide_feather");
	float co[2], cv_score, feather_score;
	const float threshold = 19;

	ED_mask_mouse_pos(C, event, co);
	ED_mask_size(C, &width, &height);

	cv_point = ED_mask_point_find_nearest(C, mask, co, threshold, &cv_masklay, &cv_spline, &is_handle, &cv_score);

	if (ED_mask_feather_find_nearest(C, mask, co, threshold, &feather_masklay, &feather_spline, &feather_point, &uw, &feather_score)) {
		if (slide_feather || !cv_point || feather_score < cv_score) {
			action = SLIDE_ACTION_FEATHER;

			masklay = feather_masklay;
			spline = feather_spline;
			point = feather_point;
		}
	}

	if (cv_point && action == SLIDE_ACTION_NONE) {
		if (is_handle)
			action = SLIDE_ACTION_HANDLE;
		else
			action = SLIDE_ACTION_POINT;

		masklay = cv_masklay;
		spline = cv_spline;
		point = cv_point;
	}

	if (action != SLIDE_ACTION_NONE) {
		customdata = MEM_callocN(sizeof(SlidePointData), "mask slide point data");

		customdata->mask = mask;
		customdata->masklay = masklay;
		customdata->spline = spline;
		customdata->point = point;
		customdata->width = width;
		customdata->height = height;
		customdata->action = action;
		customdata->uw = uw;

		if (uw) {
			float co[2];
			float weight_scalar = BKE_mask_point_weight_scalar(spline, point, uw->u);

			customdata->weight = uw->w;
			BKE_mask_point_segment_co(spline, point, uw->u, co);
			BKE_mask_point_normal(spline, point, uw->u, customdata->no);

			madd_v2_v2v2fl(customdata->feather, co, customdata->no, uw->w * weight_scalar);
		}
		else {
			BezTriple *bezt = &point->bezt;

			customdata->weight = bezt->weight;
			BKE_mask_point_normal(spline, point, 0.0f, customdata->no);

			madd_v2_v2v2fl(customdata->feather, bezt->vec[1], customdata->no, bezt->weight);
		}

		if (customdata->action == SLIDE_ACTION_FEATHER)
			customdata->initial_feather = slide_point_check_initial_feather(spline);

		copy_m3_m3(customdata->vec, point->bezt.vec);
		if (BKE_mask_point_has_handle(point))
			BKE_mask_point_handle(point, customdata->handle);
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
				ED_mask_select_toggle_all(mask, SEL_DESELECT);

				slidedata->uw->flag |= SELECT;

				ED_mask_select_flush_all(mask);
			}
		}
		else if (!MASKPOINT_ISSEL_ANY(slidedata->point)) {
			ED_mask_select_toggle_all(mask, SEL_DESELECT);

			BKE_mask_point_select_set(slidedata->point, TRUE);

			ED_mask_select_flush_all(mask);
		}

		slidedata->masklay->act_spline = slidedata->spline;
		slidedata->masklay->act_point = slidedata->point;

		WM_event_add_notifier(C, NC_MASK | ND_SELECT, mask);

		return OPERATOR_RUNNING_MODAL;
	}

	return OPERATOR_PASS_THROUGH;
}

static void slide_point_delta_all_feather(SlidePointData *data, float delta)
{
	int i;

	for (i = 0; i < data->spline->tot_point; i++) {
		MaskSplinePoint *point = &data->spline->points[i];
		MaskSplinePoint *orig_point = &data->orig_spline->points[i];

		point->bezt.weight = orig_point->bezt.weight + delta;
		if (point->bezt.weight < 0.0f)
			point->bezt.weight = 0.0f;

		/* not needed anymore */
#if 0
		int j;
		for (j = 0; j < point->tot_uw; j++) {
			point->uw[j].w = orig_point->uw[j].w + delta;
			if (point->uw[j].w < 0.0f)
				point->uw[j].w = 0.0f;
		}
#endif
	}
}

static void slide_point_restore_spline(SlidePointData *data)
{
	int i;

	for (i = 0; i < data->spline->tot_point; i++) {
		MaskSplinePoint *point = &data->spline->points[i];
		MaskSplinePoint *orig_point = &data->orig_spline->points[i];
		int j;

		point->bezt = orig_point->bezt;

		for (j = 0; j < point->tot_uw; j++)
			point->uw[j] = orig_point->uw[j];
	}
}

static void cancel_slide_point(SlidePointData *data)
{
	/* cancel sliding */

	if (data->orig_spline) {
		slide_point_restore_spline(data);
	}
	else {
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
}

static void free_slide_point_data(SlidePointData *data)
{
	if (data->orig_spline)
		BKE_mask_spline_free(data->orig_spline);

	MEM_freeN(data);
}

static int slide_point_modal(bContext *C, wmOperator *op, wmEvent *event)
{
	SlidePointData *data = (SlidePointData *)op->customdata;
	BezTriple *bezt = &data->point->bezt;
	float co[2], dco[2];

	switch (event->type) {
		case LEFTCTRLKEY:
		case RIGHTCTRLKEY:
		case LEFTSHIFTKEY:
		case RIGHTSHIFTKEY:
			if (ELEM(event->type, LEFTCTRLKEY, RIGHTCTRLKEY)) {
				if (data->action == SLIDE_ACTION_FEATHER)
					data->overall_feather = event->val == KM_PRESS;
				else
					data->curvature_only = event->val == KM_PRESS;
			}

			if (ELEM(event->type, LEFTSHIFTKEY, RIGHTSHIFTKEY))
				data->accurate = event->val == KM_PRESS;

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

				BKE_mask_point_set_handle(data->point, offco, data->curvature_only, data->handle, data->vec);
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
				float *weight = NULL;
				float weight_scalar = 1.0f;
				int overall_feather = data->overall_feather || data->initial_feather;

				add_v2_v2v2(offco, data->feather, dco);

				if (data->uw) {
					/* project on both sides and find the closest one,
					 * prevents flickering when projecting onto both sides can happen */
					const float u_pos = BKE_mask_spline_project_co(data->spline, data->point,
					                                               data->uw->u, offco, MASK_PROJ_NEG);
					const float u_neg = BKE_mask_spline_project_co(data->spline, data->point,
					                                               data->uw->u, offco, MASK_PROJ_POS);
					float dist_pos = FLT_MAX;
					float dist_neg = FLT_MAX;
					float co_pos[2];
					float co_neg[2];
					float u;

					if (u_pos > 0.0f && u_pos < 1.0f) {
						BKE_mask_point_segment_co(data->spline, data->point, u_pos, co_pos);
						dist_pos = len_squared_v2v2(offco, co_pos);
					}

					if (u_neg > 0.0f && u_neg < 1.0f) {
						BKE_mask_point_segment_co(data->spline, data->point, u_neg, co_neg);
						dist_neg = len_squared_v2v2(offco, co_neg);
					}

					u = dist_pos < dist_neg ? u_pos : u_neg;

					if (u > 0.0f && u < 1.0f) {
						data->uw->u = u;

						data->uw = BKE_mask_point_sort_uw(data->point, data->uw);
						weight = &data->uw->w;
						weight_scalar = BKE_mask_point_weight_scalar(data->spline, data->point, u);
						if (weight_scalar != 0.0f) {
							weight_scalar = 1.0f / weight_scalar;
						}

						BKE_mask_point_normal(data->spline, data->point, data->uw->u, no);
						BKE_mask_point_segment_co(data->spline, data->point, data->uw->u, p);
					}
				}
				else {
					weight = &bezt->weight;
					/* weight_scalar = 1.0f; keep as is */
					copy_v2_v2(no, data->no);
					copy_v2_v2(p, bezt->vec[1]);
				}

				if (weight) {
					sub_v2_v2v2(c, offco, p);
					project_v2_v2v2(vec, c, no);

					w = len_v2(vec);

					if (overall_feather) {
						float delta;

						if (dot_v2v2(no, vec) <= 0.0f)
							w = -w;

						delta = w - data->weight;

						if (data->orig_spline == NULL) {
							/* restore weight for currently sliding point, so orig_spline would be created
							 * with original weights used
							 */
							*weight = data->weight * weight_scalar;

							data->orig_spline = BKE_mask_spline_copy(data->spline);
						}

						slide_point_delta_all_feather(data, delta);
					}
					else {
						if (dot_v2v2(no, vec) <= 0.0f)
							w = 0.0f;

						if (data->orig_spline) {
							/* restore possible overall feather changes */
							slide_point_restore_spline(data);

							BKE_mask_spline_free(data->orig_spline);
							data->orig_spline = NULL;
						}

						if (weight_scalar != 0.0f) {
							*weight = w * weight_scalar;
						}
					}
				}
			}

			WM_event_add_notifier(C, NC_MASK | NA_EDITED, data->mask);
			DAG_id_tag_update(&data->mask->id, 0);

			break;

		case LEFTMOUSE:
			if (event->val == KM_RELEASE) {
				Scene *scene = CTX_data_scene(C);

				/* dont key sliding feather uw's */
				if ((data->action == SLIDE_ACTION_FEATHER && data->uw) == FALSE) {
					if (IS_AUTOKEY_ON(scene)) {
						ED_mask_layer_shape_auto_key(data->masklay, CFRA);
					}
				}

				free_slide_point_data(op->customdata);

				WM_event_add_notifier(C, NC_MASK | NA_EDITED, data->mask);
				DAG_id_tag_update(&data->mask->id, 0);

				return OPERATOR_FINISHED;
			}

			break;

		case ESCKEY:
			cancel_slide_point(op->customdata);

			free_slide_point_data(op->customdata);

			WM_event_add_notifier(C, NC_MASK | NA_EDITED, data->mask);
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
	ot->poll = ED_maskedit_mask_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	RNA_def_boolean(ot->srna, "slide_feather", 0, "Slide Feather", "First try to slide feather instead of vertex");
}

/******************** toggle cyclic *********************/

static int cyclic_toggle_exec(bContext *C, wmOperator *UNUSED(op))
{
	Mask *mask = CTX_data_edit_mask(C);
	MaskLayer *masklay;

	for (masklay = mask->masklayers.first; masklay; masklay = masklay->next) {
		MaskSpline *spline;

		if (masklay->restrictflag & (MASK_RESTRICT_VIEW | MASK_RESTRICT_SELECT)) {
			continue;
		}

		for (spline = masklay->splines.first; spline; spline = spline->next) {
			if (ED_mask_spline_select_check(spline)) {
				spline->flag ^= MASK_SPLINE_CYCLIC;
			}
		}
	}

	WM_event_add_notifier(C, NC_MASK | NA_EDITED, mask);

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
	ot->poll = ED_maskedit_mask_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
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
	MaskLayer *masklay;

	for (masklay = mask->masklayers.first; masklay; masklay = masklay->next) {
		MaskSpline *spline;
		int mask_layer_shape_ofs = 0;

		if (masklay->restrictflag & (MASK_RESTRICT_VIEW | MASK_RESTRICT_SELECT)) {
			continue;
		}

		spline = masklay->splines.first;

		while (spline) {
			const int tot_point_orig = spline->tot_point;
			int i, count = 0;
			MaskSpline *next_spline = spline->next;

			/* count unselected points */
			for (i = 0; i < spline->tot_point; i++) {
				MaskSplinePoint *point = &spline->points[i];

				if (!MASKPOINT_ISSEL_ANY(point))
					count++;
			}

			if (count == 0) {

				/* delete the whole spline */
				BLI_remlink(&masklay->splines, spline);
				BKE_mask_spline_free(spline);

				if (spline == masklay->act_spline) {
					masklay->act_spline = NULL;
					masklay->act_point = NULL;
				}

				BKE_mask_layer_shape_changed_remove(masklay, mask_layer_shape_ofs, tot_point_orig);
			}
			else {
				MaskSplinePoint *new_points;
				int j;

				new_points = MEM_callocN(count * sizeof(MaskSplinePoint), "deleteMaskPoints");

				for (i = 0, j = 0; i < tot_point_orig; i++) {
					MaskSplinePoint *point = &spline->points[i];

					if (!MASKPOINT_ISSEL_ANY(point)) {
						if (point == masklay->act_point)
							masklay->act_point = &new_points[j];

						delete_feather_points(point);

						new_points[j] = *point;
						j++;
					}
					else {
						if (point == masklay->act_point)
							masklay->act_point = NULL;

						BKE_mask_point_free(point);
						spline->tot_point--;

						BKE_mask_layer_shape_changed_remove(masklay, mask_layer_shape_ofs + j, 1);
					}
				}

				mask_layer_shape_ofs += spline->tot_point;

				MEM_freeN(spline->points);
				spline->points = new_points;

				ED_mask_select_flush_all(mask);
			}

			spline = next_spline;
		}
	}

	/* TODO: only update edited splines */
	BKE_mask_update_display(mask, CTX_data_scene(C)->r.cfra);

	WM_event_add_notifier(C, NC_MASK | NA_EDITED, mask);

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
	ot->poll = ED_maskedit_mask_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* *** switch direction *** */
static int mask_switch_direction_exec(bContext *C, wmOperator *UNUSED(op))
{
	Mask *mask = CTX_data_edit_mask(C);
	MaskLayer *masklay;

	int change = FALSE;

	/* do actual selection */
	for (masklay = mask->masklayers.first; masklay; masklay = masklay->next) {
		MaskSpline *spline;

		if (masklay->restrictflag & (MASK_RESTRICT_VIEW | MASK_RESTRICT_SELECT)) {
			continue;
		}

		for (spline = masklay->splines.first; spline; spline = spline->next) {
			if (ED_mask_spline_select_check(spline)) {
				BKE_mask_spline_direction_switch(masklay, spline);
				change = TRUE;
			}
		}
	}

	if (change) {
		/* TODO: only update this spline */
		BKE_mask_update_display(mask, CTX_data_scene(C)->r.cfra);

		WM_event_add_notifier(C, NC_MASK | ND_SELECT, mask);

		return OPERATOR_FINISHED;
	}

	return OPERATOR_CANCELLED;
}

void MASK_OT_switch_direction(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Switch Direction";
	ot->description = "Switch direction of selected splines";
	ot->idname = "MASK_OT_switch_direction";

	/* api callbacks */
	ot->exec = mask_switch_direction_exec;
	ot->poll = ED_maskedit_mask_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}


/******************** set handle type *********************/

static int set_handle_type_exec(bContext *C, wmOperator *op)
{
	Mask *mask = CTX_data_edit_mask(C);
	MaskLayer *masklay;
	int handle_type = RNA_enum_get(op->ptr, "type");

	for (masklay = mask->masklayers.first; masklay; masklay = masklay->next) {
		MaskSpline *spline;
		int i;

		if (masklay->restrictflag & (MASK_RESTRICT_VIEW | MASK_RESTRICT_SELECT)) {
			continue;
		}

		for (spline = masklay->splines.first; spline; spline = spline->next) {
			for (i = 0; i < spline->tot_point; i++) {
				MaskSplinePoint *point = &spline->points[i];

				if (MASKPOINT_ISSEL_ANY(point)) {
					BezTriple *bezt = &point->bezt;

					bezt->h1 = bezt->h2 = handle_type;
				}
			}
		}
	}

	WM_event_add_notifier(C, NC_MASK | ND_DATA, mask);
	DAG_id_tag_update(&mask->id, 0);

	return OPERATOR_FINISHED;
}

void MASK_OT_handle_type_set(wmOperatorType *ot)
{
	static EnumPropertyItem editcurve_handle_type_items[] = {
		{HD_AUTO, "AUTO", 0, "Auto", ""},
		{HD_VECT, "VECTOR", 0, "Vector", ""},
		{HD_ALIGN, "ALIGNED", 0, "Aligned", ""},
		{0, NULL, 0, NULL, NULL}
	};

	/* identifiers */
	ot->name = "Set Handle Type";
	ot->description = "Set type of handles for selected control points";
	ot->idname = "MASK_OT_handle_type_set";

	/* api callbacks */
	ot->invoke = WM_menu_invoke;
	ot->exec = set_handle_type_exec;
	ot->poll = ED_maskedit_mask_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* properties */
	ot->prop = RNA_def_enum(ot->srna, "type", editcurve_handle_type_items, 1, "Type", "Spline type");
}


/* ********* clear/set restrict view *********/
static int mask_hide_view_clear_exec(bContext *C, wmOperator *UNUSED(op))
{
	Mask *mask = CTX_data_edit_mask(C);
	MaskLayer *masklay;
	int changed = FALSE;

	for (masklay = mask->masklayers.first; masklay; masklay = masklay->next) {

		if (masklay->restrictflag & OB_RESTRICT_VIEW) {
			ED_mask_layer_select_set(masklay, TRUE);
			masklay->restrictflag &= ~OB_RESTRICT_VIEW;
			changed = 1;
		}
	}

	if (changed) {
		WM_event_add_notifier(C, NC_MASK | ND_DRAW, mask);
		DAG_id_tag_update(&mask->id, 0);

		return OPERATOR_FINISHED;
	}
	else {
		return OPERATOR_CANCELLED;
	}
}

void MASK_OT_hide_view_clear(wmOperatorType *ot)
{

	/* identifiers */
	ot->name = "Clear Restrict View";
	ot->description = "Reveal the layer by setting the hide flag";
	ot->idname = "MASK_OT_hide_view_clear";

	/* api callbacks */
	ot->exec = mask_hide_view_clear_exec;
	ot->poll = ED_maskedit_mask_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static int mask_hide_view_set_exec(bContext *C, wmOperator *op)
{
	Mask *mask = CTX_data_edit_mask(C);
	MaskLayer *masklay;
	const int unselected = RNA_boolean_get(op->ptr, "unselected");
	int changed = FALSE;

	for (masklay = mask->masklayers.first; masklay; masklay = masklay->next) {

		if (masklay->restrictflag & MASK_RESTRICT_SELECT) {
			continue;
		}

		if (!unselected) {
			if (ED_mask_layer_select_check(masklay)) {
				ED_mask_layer_select_set(masklay, FALSE);

				masklay->restrictflag |= OB_RESTRICT_VIEW;
				changed = 1;
				if (masklay == BKE_mask_layer_active(mask)) {
					BKE_mask_layer_active_set(mask, NULL);
				}
			}
		}
		else {
			if (!ED_mask_layer_select_check(masklay)) {
				masklay->restrictflag |= OB_RESTRICT_VIEW;
				changed = 1;
				if (masklay == BKE_mask_layer_active(mask)) {
					BKE_mask_layer_active_set(mask, NULL);
				}
			}
		}
	}

	if (changed) {
		WM_event_add_notifier(C, NC_MASK | ND_DRAW, mask);
		DAG_id_tag_update(&mask->id, 0);

		return OPERATOR_FINISHED;
	}
	else {
		return OPERATOR_CANCELLED;
	}
}

void MASK_OT_hide_view_set(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Set Restrict View";
	ot->description = "Hide the layer by setting the hide flag";
	ot->idname = "MASK_OT_hide_view_set";

	/* api callbacks */
	ot->exec = mask_hide_view_set_exec;
	ot->poll = ED_maskedit_mask_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	RNA_def_boolean(ot->srna, "unselected", 0, "Unselected", "Hide unselected rather than selected layers");
}


static int mask_feather_weight_clear_exec(bContext *C, wmOperator *UNUSED(op))
{
	Mask *mask = CTX_data_edit_mask(C);
	MaskLayer *masklay;
	int changed = FALSE;
	int i;

	for (masklay = mask->masklayers.first; masklay; masklay = masklay->next) {
		MaskSpline *spline;

		if (masklay->restrictflag & (MASK_RESTRICT_SELECT | MASK_RESTRICT_VIEW)) {
			continue;
		}

		for (spline = masklay->splines.first; spline; spline = spline->next) {
			for (i = 0; i < spline->tot_point; i++) {
				MaskSplinePoint *point = &spline->points[i];

				if (MASKPOINT_ISSEL_ANY(point)) {
					BezTriple *bezt = &point->bezt;
					bezt->weight = 0.0f;
					changed = TRUE;
				}
			}
		}
	}

	if (changed) {
		/* TODO: only update edited splines */
		BKE_mask_update_display(mask, CTX_data_scene(C)->r.cfra);

		WM_event_add_notifier(C, NC_MASK | ND_DRAW, mask);
		DAG_id_tag_update(&mask->id, 0);

		return OPERATOR_FINISHED;
	}
	else {
		return OPERATOR_CANCELLED;
	}
}

void MASK_OT_feather_weight_clear(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Clear Feather Weight";
	ot->description = "Reset the feather weight to zero";
	ot->idname = "MASK_OT_feather_weight_clear";

	/* api callbacks */
	ot->exec = mask_feather_weight_clear_exec;
	ot->poll = ED_maskedit_mask_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}
