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
 * The Original Code is Copyright (C) 2011 Blender Foundation.
 * All rights reserved.
 *
 *
 * Contributor(s): Blender Foundation,
 *                 Sergey Sharybin
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/space_clip/clip_graph_ops.c
 *  \ingroup spclip
 */

#include "DNA_object_types.h"	/* SELECT */
#include "DNA_scene_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"
#include "BLI_math.h"
#include "BLI_listbase.h"
#include "BLI_rect.h"

#include "BKE_context.h"
#include "BKE_movieclip.h"
#include "BKE_tracking.h"
#include "BKE_depsgraph.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_screen.h"
#include "ED_clip.h"

#include "UI_interface.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "UI_view2d.h"

#include "clip_intern.h"	// own include

/******************** common graph-editing utilities ********************/

static int ED_space_clip_graph_poll(bContext *C)
{
	if (ED_space_clip_tracking_poll(C)) {
		SpaceClip *sc = CTX_wm_space_clip(C);

		return sc->view == SC_VIEW_GRAPH;
	}

	return FALSE;
}

typedef struct {
	int action;
} SelectUserData;

static void toggle_selection_cb(void *userdata, MovieTrackingMarker *marker)
{
	SelectUserData *data = (SelectUserData *)userdata;

	switch (data->action) {
		case SEL_SELECT:
			marker->flag |= MARKER_GRAPH_SEL;
			break;
		case SEL_DESELECT:
			marker->flag &= ~MARKER_GRAPH_SEL;
			break;
		case SEL_INVERT:
			marker->flag ^= MARKER_GRAPH_SEL;
			break;
	}
}

/******************** mouse select operator ********************/

typedef struct {
	int coord, 		/* coordinate index of found entuty (0 = X-axis, 1 = Y-axis) */
	    has_prev;		/* if there's valid coordinate of previous point of curve segment */

	float min_dist,		/* minimal distance between mouse and currently found entuty */
	      mouse_co[2],	/* mouse coordinate */
	      prev_co[2],	/* coordinate of previeous point of segment */
	      min_co[2];	/* coordinate of entity with minimal distance */

	MovieTrackingTrack *track;	/* nearest found track */
	MovieTrackingMarker *marker;	/* nearest found marker */
} MouseSelectUserData;

static void find_nearest_tracking_segment_cb(void *userdata, MovieTrackingTrack *track,
                                             MovieTrackingMarker *UNUSED(marker),
                                             int coord, int scene_framenr, float val)
{
	MouseSelectUserData *data = userdata;
	float co[2] = {scene_framenr, val};

	if (data->has_prev) {
		float d = dist_to_line_segment_v2(data->mouse_co, data->prev_co, co);

		if (data->track == NULL || d < data->min_dist) {
			data->track = track;
			data->min_dist = d;
			data->coord = coord;
			copy_v2_v2(data->min_co, co);
		}
	}

	data->has_prev = TRUE;
	copy_v2_v2(data->prev_co, co);
}

void find_nearest_tracking_segment_end_cb(void *userdata)
{
	MouseSelectUserData *data = userdata;

	data->has_prev = FALSE;
}

static void find_nearest_tracking_knot_cb(void *userdata, MovieTrackingTrack *track,
                                          MovieTrackingMarker *marker, int coord, int scene_framenr, float val)
{
	MouseSelectUserData *data = userdata;
	float dx = scene_framenr - data->mouse_co[0], dy = val - data->mouse_co[1];
	float d = dx * dx + dy * dy;

	if (data->marker == NULL || d < data->min_dist) {
		float co[2] = {scene_framenr, val};

		data->track = track;
		data->marker = marker;
		data->min_dist = d;
		data->coord = coord;
		copy_v2_v2(data->min_co, co);
	}

}

static void mouse_select_init_data(MouseSelectUserData *userdata, float *co)
{
	memset(userdata, 0, sizeof(MouseSelectUserData));
	userdata->min_dist = FLT_MAX;
	copy_v2_v2(userdata->mouse_co, co);
}

static int mouse_select_knot(bContext *C, float co[2], int extend)
{
	SpaceClip *sc = CTX_wm_space_clip(C);
	MovieClip *clip = ED_space_clip(sc);
	ARegion *ar = CTX_wm_region(C);
	View2D *v2d = &ar->v2d;
	MovieTracking *tracking = &clip->tracking;
	MovieTrackingTrack *act_track = BKE_tracking_active_track(tracking);
	static const int delta = 6;

	if (act_track) {
		MouseSelectUserData userdata;

		mouse_select_init_data(&userdata, co);
		clip_graph_tracking_values_iterate_track(sc, act_track, &userdata,
		                                         find_nearest_tracking_knot_cb, NULL, NULL);

		if (userdata.marker) {
			int x1, y1, x2, y2;

			UI_view2d_view_to_region(v2d, co[0], co[1], &x1, &y1);
			UI_view2d_view_to_region(v2d, userdata.min_co[0], userdata.min_co[1], &x2, &y2);

			if (abs(x2 - x1) <= delta && abs(y2 - y1) <= delta) {
				if (!extend) {
					SelectUserData selectdata = {SEL_DESELECT};

					clip_graph_tracking_iterate(sc, &selectdata, toggle_selection_cb);
				}

				if (userdata.coord == 0)
					userdata.marker->flag |= MARKER_GRAPH_SEL_X;
				else
					userdata.marker->flag |= MARKER_GRAPH_SEL_Y;

				return TRUE;
			}
		}
	}

	return FALSE;
}

static int mouse_select_curve(bContext *C, float co[2], int extend)
{
	SpaceClip *sc = CTX_wm_space_clip(C);
	MovieClip *clip = ED_space_clip(sc);
	MovieTracking *tracking = &clip->tracking;
	MovieTrackingTrack *act_track = BKE_tracking_active_track(tracking);
	MouseSelectUserData userdata;

	mouse_select_init_data(&userdata, co);
	clip_graph_tracking_values_iterate(sc, &userdata, find_nearest_tracking_segment_cb,
	                                   NULL, find_nearest_tracking_segment_end_cb);

	if (userdata.track) {
		if (extend) {
			if (act_track == userdata.track) {
				/* currently only single curve can be selected (selected curve represents active track) */
				act_track = NULL;
			}
		}
		else if (act_track != userdata.track) {
			SelectUserData selectdata = {SEL_DESELECT};

			tracking->act_track = userdata.track;

			/* deselect all knots on newly selected curve */
			clip_graph_tracking_iterate(sc, &selectdata, toggle_selection_cb);
		}

		return TRUE;
	}

	return FALSE;
}

static int mouse_select(bContext *C, float co[2], int extend)
{
	int sel = FALSE;

	/* first try to select knot on selected curves */
	sel = mouse_select_knot(C, co, extend);

	if (!sel) {
		/* if there's no close enough knot to mouse osition, select nearest curve */
		sel = mouse_select_curve(C, co, extend);
	}

	if (sel)
		WM_event_add_notifier(C, NC_GEOM | ND_SELECT, NULL);

	return OPERATOR_FINISHED;
}

static int select_exec(bContext *C, wmOperator *op)
{
	float co[2];
	int  extend = RNA_boolean_get(op->ptr, "extend");

	RNA_float_get_array(op->ptr, "location", co);

	return mouse_select(C, co, extend);
}

static int select_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	ARegion *ar = CTX_wm_region(C);
	float co[2];

	UI_view2d_region_to_view(&ar->v2d, event->mval[0], event->mval[1], &co[0], &co[1]);
	RNA_float_set_array(op->ptr, "location", co);

	return select_exec(C, op);
}

void CLIP_OT_graph_select(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Select";
	ot->description = "Select graph curves";
	ot->idname = "CLIP_OT_graph_select";

	/* api callbacks */
	ot->exec = select_exec;
	ot->invoke = select_invoke;
	ot->poll = ED_space_clip_graph_poll;

	/* flags */
	ot->flag = OPTYPE_UNDO;

	/* properties */
	RNA_def_float_vector(ot->srna, "location", 2, NULL, -FLT_MAX, FLT_MAX,
		"Location", "Mouse location to select nearest entity", -100.0f, 100.0f);
	RNA_def_boolean(ot->srna, "extend", 0,
		"Extend", "Extend selection rather than clearing the existing selection");
}

/********************** border select operator *********************/

typedef struct BorderSelectuserData {
	rctf rect;
	int change, mode, extend;
} BorderSelectuserData;

static void border_select_cb(void *userdata, MovieTrackingTrack *UNUSED(track),
                             MovieTrackingMarker *marker, int coord, int scene_framenr, float val)
{
	BorderSelectuserData *data = (BorderSelectuserData *) userdata;

	if (BLI_in_rctf(&data->rect, scene_framenr, val)) {
		int flag = 0;

		if (coord == 0)
			flag = MARKER_GRAPH_SEL_X;
		else
			flag = MARKER_GRAPH_SEL_Y;

		if (data->mode == GESTURE_MODAL_SELECT)
			marker->flag |= flag;
		else
			marker->flag &= ~flag;

		data->change = TRUE;
	}
	else if (!data->extend) {
		marker->flag &= ~MARKER_GRAPH_SEL;
	}
}

static int border_select_graph_exec(bContext *C, wmOperator *op)
{
	SpaceClip *sc = CTX_wm_space_clip(C);
	ARegion *ar = CTX_wm_region(C);
	MovieClip *clip = ED_space_clip(sc);
	MovieTracking *tracking = &clip->tracking;
	MovieTrackingTrack *act_track = BKE_tracking_active_track(tracking);
	BorderSelectuserData userdata;
	rcti rect;

	/* get rectangle from operator */
	rect.xmin = RNA_int_get(op->ptr, "xmin");
	rect.ymin = RNA_int_get(op->ptr, "ymin");
	rect.xmax = RNA_int_get(op->ptr, "xmax");
	rect.ymax = RNA_int_get(op->ptr, "ymax");

	UI_view2d_region_to_view(&ar->v2d, rect.xmin, rect.ymin, &userdata.rect.xmin, &userdata.rect.ymin);
	UI_view2d_region_to_view(&ar->v2d, rect.xmax, rect.ymax, &userdata.rect.xmax, &userdata.rect.ymax);

	userdata.change = FALSE;
	userdata.mode = RNA_int_get(op->ptr, "gesture_mode");
	userdata.extend = RNA_boolean_get(op->ptr, "extend");

	clip_graph_tracking_values_iterate_track(sc, act_track, &userdata, border_select_cb, NULL, NULL);

	if (userdata.change) {
		WM_event_add_notifier(C, NC_GEOM | ND_SELECT, NULL);

		return OPERATOR_FINISHED;
	}

	return OPERATOR_CANCELLED;
}

void CLIP_OT_graph_select_border(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Border Select";
	ot->description = "Select curve points using border selection";
	ot->idname = "CLIP_OT_graph_select_border";

	/* api callbacks */
	ot->invoke = WM_border_select_invoke;
	ot->exec = border_select_graph_exec;
	ot->modal = WM_border_select_modal;
	ot->poll = ED_space_clip_graph_poll;

	/* flags */
	ot->flag = OPTYPE_UNDO;

	/* properties */
	WM_operator_properties_gesture_border(ot, TRUE);
}

/********************** select all operator *********************/

static int graph_select_all_markers_exec(bContext *C, wmOperator *op)
{
	SpaceClip *sc = CTX_wm_space_clip(C);
	MovieClip *clip = ED_space_clip(sc);
	MovieTracking *tracking = &clip->tracking;
	MovieTrackingTrack *act_track = BKE_tracking_active_track(tracking);
	MovieTrackingMarker *marker;
	int action = RNA_enum_get(op->ptr, "action");
	int a;

	if (!act_track)
		return OPERATOR_CANCELLED;

	if (action == SEL_TOGGLE) {
		action = SEL_SELECT;

		for (a = 0; a < act_track->markersnr; a++) {
			marker = &act_track->markers[a];

			if (marker->flag & MARKER_GRAPH_SEL) {
				action = SEL_DESELECT;
				break;
			}
		}
	}

	for (a = 0; a < act_track->markersnr; a++) {
		marker = &act_track->markers[a];

		switch (action) {
			case SEL_SELECT:
				marker->flag |= MARKER_GRAPH_SEL;
				break;
			case SEL_DESELECT:
				marker->flag &= ~MARKER_GRAPH_SEL;
				break;
			case SEL_INVERT:
				marker->flag ^= MARKER_GRAPH_SEL;
				break;
		}
	}

	WM_event_add_notifier(C, NC_GEOM | ND_SELECT, NULL);

	return OPERATOR_FINISHED;
}

void CLIP_OT_graph_select_all_markers(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "(De)select All Markers";
	ot->description = "Change selection of all markers of active track";
	ot->idname = "CLIP_OT_graph_select_all_markers";

	/* api callbacks */
	ot->exec = graph_select_all_markers_exec;
	ot->poll = ED_space_clip_graph_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	WM_operator_properties_select_all(ot);
}

/******************** delete curve operator ********************/

static int delete_curve_exec(bContext *C, wmOperator *UNUSED(op))
{
	SpaceClip *sc = CTX_wm_space_clip(C);
	MovieClip *clip = ED_space_clip(sc);
	MovieTracking *tracking = &clip->tracking;
	ListBase *tracksbase = BKE_tracking_get_tracks(tracking);
	MovieTrackingTrack *act_track = BKE_tracking_active_track(tracking);

	if (act_track)
		clip_delete_track(C, clip, tracksbase, act_track);

	return OPERATOR_FINISHED;
}

void CLIP_OT_graph_delete_curve(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Delete Curve";
	ot->description = "Delete selected curves";
	ot->idname = "CLIP_OT_graph_delete_curve";

	/* api callbacks */
	ot->invoke = WM_operator_confirm;
	ot->exec = delete_curve_exec;
	ot->poll = ED_space_clip_tracking_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/******************** delete knot operator ********************/

static int delete_knot_exec(bContext *C, wmOperator *UNUSED(op))
{
	SpaceClip *sc = CTX_wm_space_clip(C);
	MovieClip *clip = ED_space_clip(sc);
	MovieTracking *tracking = &clip->tracking;
	ListBase *tracksbase = BKE_tracking_get_tracks(tracking);
	MovieTrackingTrack *act_track = BKE_tracking_active_track(tracking);

	if (act_track) {
		int a = 0;

		while (a < act_track->markersnr) {
			MovieTrackingMarker *marker = &act_track->markers[a];

			if (marker->flag & MARKER_GRAPH_SEL)
				clip_delete_marker(C, clip, tracksbase, act_track, marker);
			else
				a++;
		}
	}

	return OPERATOR_FINISHED;
}

void CLIP_OT_graph_delete_knot(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Delete Knot";
	ot->description = "Delete curve knots";
	ot->idname = "CLIP_OT_graph_delete_knot";

	/* api callbacks */
	ot->exec = delete_knot_exec;
	ot->poll = ED_space_clip_graph_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/******************** view all operator ********************/

typedef struct {
	float min, max;
} ViewAllUserData;

static void view_all_cb(void *userdata, MovieTrackingTrack *UNUSED(track), MovieTrackingMarker *UNUSED(marker),
                        int UNUSED(coord), int UNUSED(scene_framenr), float val)
{
	ViewAllUserData *data = (ViewAllUserData *) userdata;

	if (val < data->min)
		data->min = val;

	if (val > data->max)
		data->max = val;
}

static int view_all_exec(bContext *C, wmOperator *UNUSED(op))
{
	Scene *scene = CTX_data_scene(C);
	ARegion *ar = CTX_wm_region(C);
	SpaceClip *sc = CTX_wm_space_clip(C);
	View2D *v2d = &ar->v2d;
	ViewAllUserData userdata;
	float extra;

	userdata.max = -FLT_MAX;
	userdata.min = FLT_MAX;

	clip_graph_tracking_values_iterate(sc, &userdata, view_all_cb, NULL, NULL);

	/* set extents of view to start/end frames */
	v2d->cur.xmin = (float) SFRA;
	v2d->cur.xmax = (float) EFRA;

	if (userdata.min < userdata.max) {
		v2d->cur.ymin = userdata.min;
		v2d->cur.ymax = userdata.max;
	}
	else {
		v2d->cur.ymin = -10;
		v2d->cur.ymax = 10;
	}

	/* we need an extra "buffer" factor on either side so that the endpoints are visible */
	extra = 0.01f * (v2d->cur.xmax - v2d->cur.xmin);
	v2d->cur.xmin -= extra;
	v2d->cur.xmax += extra;

	extra = 0.01f * (v2d->cur.ymax - v2d->cur.ymin);
	v2d->cur.ymin -= extra;
	v2d->cur.ymax += extra;

	ED_region_tag_redraw(ar);

	return OPERATOR_FINISHED;
}

void CLIP_OT_graph_view_all(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "View All";
	ot->description = "View all curves in editor";
	ot->idname = "CLIP_OT_graph_view_all";

	/* api callbacks */
	ot->exec = view_all_exec;
	ot->poll = ED_space_clip_graph_poll;
}

/******************** jump to current frame operator ********************/

void ED_clip_graph_center_current_frame(Scene *scene, ARegion *ar)
{
	View2D *v2d = &ar->v2d;
	float extra = (v2d->cur.xmax - v2d->cur.xmin) / 2.0f;

	/* set extents of view to start/end frames */
	v2d->cur.xmin = (float)CFRA - extra;
	v2d->cur.xmax = (float)CFRA + extra;
}

static int center_current_frame_exec(bContext *C, wmOperator *UNUSED(op))
{
	Scene *scene = CTX_data_scene(C);
	ARegion *ar = CTX_wm_region(C);

	ED_clip_graph_center_current_frame(scene, ar);

	ED_region_tag_redraw(ar);

	return OPERATOR_FINISHED;
}

void CLIP_OT_graph_center_current_frame(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Center Current Frame";
	ot->description = "Scroll view so current frame would be centered";
	ot->idname = "CLIP_OT_graph_center_current_frame";

	/* api callbacks */
	ot->exec = center_current_frame_exec;
	ot->poll = ED_space_clip_graph_poll;
}

/********************** disable markers operator *********************/

static int graph_disable_markers_exec(bContext *C, wmOperator *op)
{
	SpaceClip *sc = CTX_wm_space_clip(C);
	MovieClip *clip = ED_space_clip(sc);
	MovieTracking *tracking = &clip->tracking;
	MovieTrackingTrack *act_track = BKE_tracking_active_track(tracking);
	MovieTrackingMarker *marker;
	int action = RNA_enum_get(op->ptr, "action");
	int a;

	if (!act_track || (act_track->flag & TRACK_LOCKED))
		return OPERATOR_CANCELLED;

	for (a = 0; a < act_track->markersnr; a++) {
		marker = &act_track->markers[a];

		if (marker->flag & MARKER_GRAPH_SEL) {
			if (action == 0)
				marker->flag |= MARKER_DISABLED;
			else if (action == 1)
				marker->flag &= ~MARKER_DISABLED;
			else
				marker->flag ^= MARKER_DISABLED;
		}
	}

	DAG_id_tag_update(&clip->id, 0);

	WM_event_add_notifier(C, NC_MOVIECLIP | NA_EVALUATED, clip);

	return OPERATOR_FINISHED;
}

void CLIP_OT_graph_disable_markers(wmOperatorType *ot)
{
	static EnumPropertyItem actions_items[] = {
			{0, "DISABLE", 0, "Disable", "Disable selected markers"},
			{1, "ENABLE", 0, "Enable", "Enable selected markers"},
			{2, "TOGGLE", 0, "Toggle", "Toggle disabled flag for selected markers"},
			{0, NULL, 0, NULL, NULL}
	};

	/* identifiers */
	ot->name = "Disable Markers";
	ot->description = "Disable/enable selected markers";
	ot->idname = "CLIP_OT_graph_disable_markers";

	/* api callbacks */
	ot->exec = graph_disable_markers_exec;
	ot->poll = ED_space_clip_graph_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* properties */
	RNA_def_enum(ot->srna, "action", actions_items, 0, "Action", "Disable action to execute");
}
