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

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"
#include "BLI_math.h"
#include "BLI_listbase.h"

#include "BKE_context.h"
#include "BKE_movieclip.h"
#include "BKE_tracking.h"

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

typedef struct {
	int action;
} SelectUserData;

static void toggle_selection_cb(void *userdata, MovieTrackingMarker *marker)
{
	SelectUserData *data= (SelectUserData *)userdata;

	switch(data->action) {
		case SEL_SELECT:
			marker->flag|= MARKER_GRAPH_SEL;
			break;
		case SEL_DESELECT:
			marker->flag&= ~MARKER_GRAPH_SEL;
			break;
		case SEL_INVERT:
			marker->flag^= MARKER_GRAPH_SEL;
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
			MovieTrackingMarker *marker, int coord, float val)
{
	MouseSelectUserData *data= userdata;
	float co[2]= {marker->framenr, val};

	if(data->has_prev) {
		float d= dist_to_line_segment_v2(data->mouse_co, data->prev_co, co);

		if(data->track==NULL || d<data->min_dist) {
			data->track= track;
			data->min_dist= d;
			data->coord= coord;
			copy_v2_v2(data->min_co, co);
		}
	}

	data->has_prev= 1;
	copy_v2_v2(data->prev_co, co);
}

void find_nearest_tracking_segment_end_cb(void *userdata)
{
	MouseSelectUserData *data= userdata;

	data->has_prev= 0;
}

static void find_nearest_tracking_knot_cb(void *userdata, MovieTrackingTrack *track,
			MovieTrackingMarker *marker, int coord, float val)
{
	MouseSelectUserData *data= userdata;
	float dx= marker->framenr-data->mouse_co[0], dy= val-data->mouse_co[1];
	float d= dx*dx+dy*dy;

	if(data->marker==NULL || d<data->min_dist) {
		float co[2]= {marker->framenr, val};

		data->track= track;
		data->marker= marker;
		data->min_dist= d;
		data->coord= coord;
		copy_v2_v2(data->min_co, co);
	}

}

static void mouse_select_init_data(MouseSelectUserData *userdata, float *co)
{
	memset(userdata, 0, sizeof(MouseSelectUserData));
	userdata->min_dist= FLT_MAX;
	copy_v2_v2(userdata->mouse_co, co);
}

static int mouse_select_knot(bContext *C, float co[2], int extend)
{
	SpaceClip *sc= CTX_wm_space_clip(C);
	MovieClip *clip= ED_space_clip(sc);
	ARegion *ar= CTX_wm_region(C);
	View2D *v2d= &ar->v2d;
	MovieTracking *tracking= &clip->tracking;
	MovieTrackingTrack *act_track= BKE_tracking_active_track(tracking);
	static const int delta= 6;

	if(act_track) {
		MouseSelectUserData userdata;

		mouse_select_init_data(&userdata, co);
		clip_graph_tracking_values_iterate_track(sc, act_track,
					&userdata, find_nearest_tracking_knot_cb, NULL, NULL);

		if(userdata.marker) {
			int x1, y1, x2, y2;

			UI_view2d_view_to_region(v2d, co[0], co[1], &x1, &y1);
			UI_view2d_view_to_region(v2d, userdata.min_co[0], userdata.min_co[1], &x2, &y2);

			if(abs(x2-x1)<=delta && abs(y2-y1)<=delta) {
				if(!extend) {
					SelectUserData selectdata = {SEL_DESELECT};
					clip_graph_tracking_iterate(sc, &selectdata, toggle_selection_cb);
				}

				userdata.marker->flag|= MARKER_GRAPH_SEL;

				return 1;
			}
		}
	}

	return 0;
}

static int mouse_select_curve(bContext *C, float co[2], int extend)
{
	SpaceClip *sc= CTX_wm_space_clip(C);
	MovieClip *clip= ED_space_clip(sc);
	MovieTracking *tracking= &clip->tracking;
	MovieTrackingTrack *act_track= BKE_tracking_active_track(tracking);
	MouseSelectUserData userdata;

	mouse_select_init_data(&userdata, co);
	clip_graph_tracking_values_iterate(sc, &userdata, find_nearest_tracking_segment_cb, NULL, find_nearest_tracking_segment_end_cb);

	if(userdata.track) {
		if(extend) {
			if(act_track==userdata.track) {
				/* currently only single curve can be selected (selected curve represents active track) */
				act_track= NULL;
			}
		}
		else if(act_track!=userdata.track) {
			MovieTrackingMarker *marker;
			SelectUserData selectdata = {SEL_DESELECT};

			tracking->act_track= userdata.track;

			/* make active track be centered to screen */
			marker= BKE_tracking_get_marker(userdata.track, sc->user.framenr);

			clip_view_center_to_point(sc, marker->pos[0], marker->pos[1]);

			/* deselect all knots on newly selected curve */
			clip_graph_tracking_iterate(sc, &selectdata, toggle_selection_cb);
		}

		return 1;
	}

	return 0;
}

static int mouse_select(bContext *C, float co[2], int extend)
{
	int sel= 0;

	/* first try to select knot on selected curves */
	sel= mouse_select_knot(C, co, extend);

	if(!sel) {
		/* if there's no close enough knot to mouse osition, select nearest curve */
		sel= mouse_select_curve(C, co, extend);
	}

	if(sel)
		WM_event_add_notifier(C, NC_GEOM|ND_SELECT, NULL);

	return OPERATOR_FINISHED;
}

static int select_exec(bContext *C, wmOperator *op)
{
	float co[2];
	int  extend= RNA_boolean_get(op->ptr, "extend");

	RNA_float_get_array(op->ptr, "location", co);

	return mouse_select(C, co, extend);
}

static int select_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	ARegion *ar= CTX_wm_region(C);
	float co[2];

	UI_view2d_region_to_view(&ar->v2d, event->mval[0], event->mval[1], &co[0], &co[1]);
	RNA_float_set_array(op->ptr, "location", co);

	return select_exec(C, op);
}

void CLIP_OT_graph_select(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Select";
	ot->description= "Select graph curves";
	ot->idname= "CLIP_OT_graph_select";

	/* api callbacks */
	ot->exec= select_exec;
	ot->invoke= select_invoke;
	ot->poll= ED_space_clip_poll;

	/* flags */
	ot->flag= OPTYPE_UNDO;

	/* properties */
	RNA_def_float_vector(ot->srna, "location", 2, NULL, -FLT_MAX, FLT_MAX,
		"Location", "Mouse location to select nearest entity", -100.0f, 100.0f);
	RNA_def_boolean(ot->srna, "extend", 0,
		"Extend", "Extend selection rather than clearing the existing selection");
}

/******************** delete curve operator ********************/

static int delete_curve_exec(bContext *C, wmOperator *UNUSED(op))
{
	SpaceClip *sc= CTX_wm_space_clip(C);
	MovieClip *clip= ED_space_clip(sc);
	MovieTracking *tracking= &clip->tracking;
	ListBase *tracksbase= BKE_tracking_get_tracks(tracking);
	MovieTrackingTrack *act_track= BKE_tracking_active_track(tracking);

	if(act_track)
		clip_delete_track(C, clip, tracksbase, act_track);

	return OPERATOR_FINISHED;
}

void CLIP_OT_graph_delete_curve(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Delete Curve";
	ot->description= "Delete selected curves";
	ot->idname= "CLIP_OT_graph_delete_curve";

	/* api callbacks */
	ot->invoke= WM_operator_confirm;
	ot->exec= delete_curve_exec;
	ot->poll= ED_space_clip_poll;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

/******************** delete knot operator ********************/

static int delete_knot_exec(bContext *C, wmOperator *UNUSED(op))
{
	SpaceClip *sc= CTX_wm_space_clip(C);
	MovieClip *clip= ED_space_clip(sc);
	MovieTracking *tracking= &clip->tracking;
	ListBase *tracksbase= BKE_tracking_get_tracks(tracking);
	MovieTrackingTrack *act_track= BKE_tracking_active_track(tracking);

	if(act_track) {
		int a= 0;

		while(a<act_track->markersnr) {
			MovieTrackingMarker *marker= &act_track->markers[a];

			if(marker->flag&MARKER_GRAPH_SEL)
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
	ot->name= "Delete Knot";
	ot->description= "Delete curve knots";
	ot->idname= "CLIP_OT_graph_delete_knot";

	/* api callbacks */
	ot->exec= delete_knot_exec;
	ot->poll= ED_space_clip_poll;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}
