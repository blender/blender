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

/** \file blender/editors/space_clip/tracking_select.c
 *  \ingroup spclip
 */

#include "MEM_guardedalloc.h"

#include "DNA_camera_types.h"
#include "DNA_constraint_types.h"
#include "DNA_gpencil_types.h"
#include "DNA_movieclip_types.h"
#include "DNA_object_types.h"   /* SELECT */
#include "DNA_scene_types.h"

#include "BLI_utildefines.h"
#include "BLI_math.h"
#include "BLI_listbase.h"
#include "BLI_rect.h"
#include "BLI_lasso.h"
#include "BLI_blenlib.h"

#include "BKE_main.h"
#include "BKE_context.h"
#include "BKE_constraint.h"
#include "BKE_movieclip.h"
#include "BKE_tracking.h"
#include "BKE_global.h"
#include "BKE_depsgraph.h"
#include "BKE_object.h"
#include "BKE_report.h"
#include "BKE_scene.h"
#include "BKE_library.h"
#include "BKE_sound.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_screen.h"
#include "ED_clip.h"
#include "ED_keyframing.h"

#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"

#include "UI_interface.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "PIL_time.h"

#include "UI_view2d.h"

#include "clip_intern.h"    // own include

static float dist_to_crns(float co[2], float pos[2], float crns[4][2]);

/********************** mouse select operator *********************/

static int mouse_on_side(float co[2], float x1, float y1, float x2, float y2, float epsx, float epsy)
{
	if (x1 > x2)

		SWAP(float, x1, x2);

	if (y1 > y2)
		SWAP(float, y1, y2);

	return (co[0] >= x1 - epsx && co[0] <= x2 + epsx) && (co[1] >= y1 - epsy && co[1] <= y2 + epsy);
}

static int mouse_on_rect(float co[2], float pos[2], float min[2], float max[2], float epsx, float epsy)
{
	return mouse_on_side(co, pos[0] + min[0], pos[1] + min[1], pos[0] + max[0], pos[1] + min[1], epsx, epsy) ||
	       mouse_on_side(co, pos[0] + min[0], pos[1] + min[1], pos[0] + min[0], pos[1] + max[1], epsx, epsy) ||
	       mouse_on_side(co, pos[0] + min[0], pos[1] + max[1], pos[0] + max[0], pos[1] + max[1], epsx, epsy) ||
	       mouse_on_side(co, pos[0] + max[0], pos[1] + min[1], pos[0] + max[0], pos[1] + max[1], epsx, epsy);
}

static int mouse_on_crns(float co[2], float pos[2], float crns[4][2], float epsx, float epsy)
{
	float dist = dist_to_crns(co, pos, crns);

	return dist < max_ff(epsx, epsy);
}

static int track_mouse_area(const bContext *C, float co[2], MovieTrackingTrack *track)
{
	SpaceClip *sc = CTX_wm_space_clip(C);
	int framenr = ED_space_clip_get_clip_frame_number(sc);
	MovieTrackingMarker *marker = BKE_tracking_marker_get(track, framenr);
	float pat_min[2], pat_max[2];
	float epsx, epsy;
	int width, height;

	ED_space_clip_get_size(sc, &width, &height);

	BKE_tracking_marker_pattern_minmax(marker, pat_min, pat_max);

	epsx = min_ffff(pat_min[0] - marker->search_min[0], marker->search_max[0] - pat_max[0],
	                fabsf(pat_min[0]), fabsf(pat_max[0])) / 2;
	epsy = min_ffff(pat_min[1] - marker->search_min[1], marker->search_max[1] - pat_max[1],
	                fabsf(pat_min[1]), fabsf(pat_max[1])) / 2;

	epsx = max_ff(epsx, 2.0f / width);
	epsy = max_ff(epsy, 2.0f / height);

	if (sc->flag & SC_SHOW_MARKER_SEARCH) {
		if (mouse_on_rect(co, marker->pos, marker->search_min, marker->search_max, epsx, epsy))
			return TRACK_AREA_SEARCH;
	}

	if ((marker->flag & MARKER_DISABLED) == 0) {
		if (sc->flag & SC_SHOW_MARKER_PATTERN)
			if (mouse_on_crns(co, marker->pos, marker->pattern_corners, epsx, epsy))
				return TRACK_AREA_PAT;

		epsx = 12.0f / width;
		epsy = 12.0f / height;

		if (fabsf(co[0] - marker->pos[0] - track->offset[0]) < epsx &&
		    fabsf(co[1] - marker->pos[1] - track->offset[1]) <= epsy)
		{
			return TRACK_AREA_POINT;
		}
	}

	return TRACK_AREA_NONE;
}

static float dist_to_rect(float co[2], float pos[2], float min[2], float max[2])
{
	float d1, d2, d3, d4;
	float p[2] = {co[0] - pos[0], co[1] - pos[1]};
	float v1[2] = {min[0], min[1]}, v2[2] = {max[0], min[1]};
	float v3[2] = {max[0], max[1]}, v4[2] = {min[0], max[1]};

	d1 = dist_squared_to_line_segment_v2(p, v1, v2);
	d2 = dist_squared_to_line_segment_v2(p, v2, v3);
	d3 = dist_squared_to_line_segment_v2(p, v3, v4);
	d4 = dist_squared_to_line_segment_v2(p, v4, v1);

	return sqrtf(min_ffff(d1, d2, d3, d4));
}

/* Distance to quad defined by it's corners, corners are relative to pos */
static float dist_to_crns(float co[2], float pos[2], float crns[4][2])
{
	float d1, d2, d3, d4;
	float p[2] = {co[0] - pos[0], co[1] - pos[1]};
	float *v1 = crns[0], *v2 = crns[1];
	float *v3 = crns[2], *v4 = crns[3];

	d1 = dist_squared_to_line_segment_v2(p, v1, v2);
	d2 = dist_squared_to_line_segment_v2(p, v2, v3);
	d3 = dist_squared_to_line_segment_v2(p, v3, v4);
	d4 = dist_squared_to_line_segment_v2(p, v4, v1);

	return sqrtf(min_ffff(d1, d2, d3, d4));
}

/* Same as above, but all the coordinates are absolute */
static float dist_to_crns_abs(float co[2], float corners[4][2])
{
	float d1, d2, d3, d4;
	float *v1 = corners[0], *v2 = corners[1];
	float *v3 = corners[2], *v4 = corners[3];

	d1 = dist_squared_to_line_segment_v2(co, v1, v2);
	d2 = dist_squared_to_line_segment_v2(co, v2, v3);
	d3 = dist_squared_to_line_segment_v2(co, v3, v4);
	d4 = dist_squared_to_line_segment_v2(co, v4, v1);

	return sqrtf(min_ffff(d1, d2, d3, d4));
}

static MovieTrackingTrack *find_nearest_track(SpaceClip *sc, ListBase *tracksbase, float co[2], float *distance_r)
{
	MovieTrackingTrack *track = NULL, *cur;
	float mindist = 0.0f;
	int framenr = ED_space_clip_get_clip_frame_number(sc);

	cur = tracksbase->first;
	while (cur) {
		MovieTrackingMarker *marker = BKE_tracking_marker_get(cur, framenr);

		if (((cur->flag & TRACK_HIDDEN) == 0) && MARKER_VISIBLE(sc, cur, marker)) {
			float dist, d1, d2 = FLT_MAX, d3 = FLT_MAX;

			/* distance to marker point */
			d1 = sqrtf((co[0] - marker->pos[0] - cur->offset[0]) * (co[0] - marker->pos[0] - cur->offset[0]) +
			           (co[1] - marker->pos[1] - cur->offset[1]) * (co[1] - marker->pos[1] - cur->offset[1]));

			/* distance to pattern boundbox */
			if (sc->flag & SC_SHOW_MARKER_PATTERN)
				d2 = dist_to_crns(co, marker->pos, marker->pattern_corners);

			/* distance to search boundbox */
			if (sc->flag & SC_SHOW_MARKER_SEARCH && TRACK_VIEW_SELECTED(sc, cur))
				d3 = dist_to_rect(co, marker->pos, marker->search_min, marker->search_max);

			/* choose minimal distance. useful for cases of overlapped markers. */
			dist = min_fff(d1, d2, d3);

			if (track == NULL || dist < mindist) {
				track = cur;
				mindist = dist;
			}
		}

		cur = cur->next;
	}

	*distance_r = mindist;

	return track;
}

static MovieTrackingPlaneTrack *find_nearest_plane_track(SpaceClip *sc, ListBase *plane_tracks_base,
                                                         float co[2], float *distance_r)
{
	MovieTrackingPlaneTrack *plane_track = NULL, *current_plane_track;
	float min_distance = 0.0f;
	int framenr = ED_space_clip_get_clip_frame_number(sc);

	for (current_plane_track = plane_tracks_base->first;
	     current_plane_track;
	     current_plane_track = current_plane_track->next)
	{
		MovieTrackingPlaneMarker *plane_marker = BKE_tracking_plane_marker_get(current_plane_track, framenr);

		if ((current_plane_track->flag & TRACK_HIDDEN) == 0) {
			float distance = dist_to_crns_abs(co, plane_marker->corners);
			if (plane_track == NULL || distance < min_distance) {
				plane_track = current_plane_track;
				min_distance = distance;
			}
		}
	}

	*distance_r = min_distance;

	return plane_track;
}

static void delect_all_tracks(ListBase *tracks_base)
{
	MovieTrackingTrack *track;
	for (track = tracks_base->first;
	     track;
	     track = track->next)
	{
		BKE_tracking_track_flag_clear(track, TRACK_AREA_ALL, SELECT);
	}
}

static void delect_all_plane_tracks(ListBase *plane_tracks_base)
{
	MovieTrackingPlaneTrack *plane_track;
	for (plane_track = plane_tracks_base->first;
	     plane_track;
	     plane_track = plane_track->next)
	{
		plane_track->flag &= ~SELECT;
	}
}

static int mouse_select(bContext *C, float co[2], int extend)
{
	SpaceClip *sc = CTX_wm_space_clip(C);
	MovieClip *clip = ED_space_clip_get_clip(sc);
	MovieTracking *tracking = &clip->tracking;
	ListBase *tracksbase = BKE_tracking_get_active_tracks(tracking);
	ListBase *plane_tracks_base = BKE_tracking_get_active_plane_tracks(tracking);
	MovieTrackingTrack *act_track = BKE_tracking_track_get_active(tracking);
	MovieTrackingTrack *track;
	MovieTrackingPlaneTrack *plane_track;
	float distance_to_track, distance_to_plane_track;

	track = find_nearest_track(sc, tracksbase, co, &distance_to_track);
	plane_track = find_nearest_plane_track(sc, plane_tracks_base, co, &distance_to_plane_track);

	/* Between track and plane we choose closest to the mouse for selection here. */
	if (track && plane_track) {
		if (distance_to_track < distance_to_plane_track) {
			plane_track = NULL;
		}
		else {
			track = NULL;
		}
	}

	if (!extend) {
		delect_all_plane_tracks(plane_tracks_base);
	}

	if (track) {
		int area = track_mouse_area(C, co, track);

		if (!extend || !TRACK_VIEW_SELECTED(sc, track))
			area = TRACK_AREA_ALL;

		if (extend && TRACK_AREA_SELECTED(track, area)) {
			if (track == act_track) {
				BKE_tracking_track_deselect(track, area);
			}
			else {
				clip->tracking.act_track = track;
				clip->tracking.act_plane_track = NULL;
			}
		}
		else {
			if (area == TRACK_AREA_POINT)
				area = TRACK_AREA_ALL;

			BKE_tracking_track_select(tracksbase, track, area, extend);
			clip->tracking.act_track = track;
			clip->tracking.act_plane_track = NULL;
		}
	}
	else if (plane_track) {
		if (!extend) {
			delect_all_tracks(tracksbase);
		}

		if (plane_track->flag & SELECT) {
			if (extend) {
				plane_track->flag &= ~SELECT;
			}
		}
		else {
			plane_track->flag |= SELECT;
		}

		clip->tracking.act_track = NULL;
		clip->tracking.act_plane_track = plane_track;
	}

	if (!extend) {
		sc->xlockof = 0.0f;
		sc->ylockof = 0.0f;
	}

	BKE_tracking_dopesheet_tag_update(tracking);

	WM_event_add_notifier(C, NC_GEOM | ND_SELECT, NULL);

	return OPERATOR_FINISHED;
}

static int select_poll(bContext *C)
{
	SpaceClip *sc = CTX_wm_space_clip(C);

	if (sc) {
		return sc->clip && sc->view == SC_VIEW_CLIP;
	}

	return FALSE;
}

static int select_exec(bContext *C, wmOperator *op)
{
	float co[2];
	int extend;

	RNA_float_get_array(op->ptr, "location", co);
	extend = RNA_boolean_get(op->ptr, "extend");

	return mouse_select(C, co, extend);
}

static int select_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
	SpaceClip *sc = CTX_wm_space_clip(C);
	ARegion *ar = CTX_wm_region(C);

	float co[2];
	const bool extend = RNA_boolean_get(op->ptr, "extend");

	if (!extend) {
		MovieTrackingTrack *track = tracking_marker_check_slide(C, event, NULL, NULL, NULL);

		if (track) {
			MovieClip *clip = ED_space_clip_get_clip(sc);

			clip->tracking.act_track = track;

			WM_event_add_notifier(C, NC_GEOM | ND_SELECT, NULL);

			return OPERATOR_PASS_THROUGH;
		}
	}

	ED_clip_mouse_pos(sc, ar, event->mval, co);
	RNA_float_set_array(op->ptr, "location", co);

	return select_exec(C, op);
}

void CLIP_OT_select(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Select";
	ot->description = "Select tracking markers";
	ot->idname = "CLIP_OT_select";

	/* api callbacks */
	ot->exec = select_exec;
	ot->invoke = select_invoke;
	ot->poll = select_poll;

	/* flags */
	ot->flag = OPTYPE_UNDO;

	/* properties */
	RNA_def_boolean(ot->srna, "extend", 0,
	                "Extend", "Extend selection rather than clearing the existing selection");
	RNA_def_float_vector(ot->srna, "location", 2, NULL, -FLT_MAX, FLT_MAX,
	                     "Location", "Mouse location in normalized coordinates, 0.0 to 1.0 is within the image bounds", -100.0f, 100.0f);
}

/********************** border select operator *********************/

static int border_select_exec(bContext *C, wmOperator *op)
{
	SpaceClip *sc = CTX_wm_space_clip(C);
	ARegion *ar = CTX_wm_region(C);

	MovieClip *clip = ED_space_clip_get_clip(sc);
	MovieTracking *tracking = &clip->tracking;
	MovieTrackingTrack *track;
	MovieTrackingPlaneTrack *plane_track;
	ListBase *tracksbase = BKE_tracking_get_active_tracks(tracking);
	ListBase *plane_tracks_base = BKE_tracking_get_active_plane_tracks(tracking);
	rcti rect;
	rctf rectf;
	bool changed = false;
	int mode, extend;
	int framenr = ED_space_clip_get_clip_frame_number(sc);

	/* get rectangle from operator */
	WM_operator_properties_border_to_rcti(op, &rect);

	ED_clip_point_stable_pos(sc, ar, rect.xmin, rect.ymin, &rectf.xmin, &rectf.ymin);
	ED_clip_point_stable_pos(sc, ar, rect.xmax, rect.ymax, &rectf.xmax, &rectf.ymax);

	mode = RNA_int_get(op->ptr, "gesture_mode");
	extend = RNA_boolean_get(op->ptr, "extend");

	/* do actual selection */
	track = tracksbase->first;
	while (track) {
		if ((track->flag & TRACK_HIDDEN) == 0) {
			MovieTrackingMarker *marker = BKE_tracking_marker_get(track, framenr);

			if (MARKER_VISIBLE(sc, track, marker)) {
				if (BLI_rctf_isect_pt_v(&rectf, marker->pos)) {
					if (mode == GESTURE_MODAL_SELECT)
						BKE_tracking_track_flag_set(track, TRACK_AREA_ALL, SELECT);
					else
						BKE_tracking_track_flag_clear(track, TRACK_AREA_ALL, SELECT);
				}
				else if (!extend) {
					BKE_tracking_track_flag_clear(track, TRACK_AREA_ALL, SELECT);
				}

				changed = true;
			}
		}

		track = track->next;
	}

	for (plane_track = plane_tracks_base->first;
	     plane_track;
	     plane_track = plane_track->next)
	{
		if ((plane_track->flag & PLANE_TRACK_HIDDEN) == 0) {
			MovieTrackingPlaneMarker *plane_marker =
				BKE_tracking_plane_marker_get(plane_track, framenr);
			int i;

			for (i = 0; i < 4; i++) {
				if (BLI_rctf_isect_pt_v(&rectf, plane_marker->corners[i])) {
					if (mode == GESTURE_MODAL_SELECT) {
						plane_track->flag |= SELECT;
					}
					else {
						plane_track->flag &= ~SELECT;
					}
				}
				else if (!extend) {
					plane_track->flag &= ~SELECT;
				}
			}

			changed = true;
		}
	}

	if (changed) {
		BKE_tracking_dopesheet_tag_update(tracking);

		WM_event_add_notifier(C, NC_GEOM | ND_SELECT, NULL);

		return OPERATOR_FINISHED;
	}

	return OPERATOR_CANCELLED;
}

void CLIP_OT_select_border(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Border Select";
	ot->description = "Select markers using border selection";
	ot->idname = "CLIP_OT_select_border";

	/* api callbacks */
	ot->invoke = WM_border_select_invoke;
	ot->exec = border_select_exec;
	ot->modal = WM_border_select_modal;
	ot->poll = ED_space_clip_tracking_poll;

	/* flags */
	ot->flag = OPTYPE_UNDO;

	/* properties */
	WM_operator_properties_gesture_border(ot, TRUE);
}

/********************** lasso select operator *********************/

static int do_lasso_select_marker(bContext *C, const int mcords[][2], const short moves, short select)
{
	SpaceClip *sc = CTX_wm_space_clip(C);
	ARegion *ar = CTX_wm_region(C);

	MovieClip *clip = ED_space_clip_get_clip(sc);
	MovieTracking *tracking = &clip->tracking;
	MovieTrackingTrack *track;
	MovieTrackingPlaneTrack *plane_track;
	ListBase *tracksbase = BKE_tracking_get_active_tracks(tracking);
	ListBase *plane_tracks_base = BKE_tracking_get_active_plane_tracks(tracking);
	rcti rect;
	bool changed = false;
	int framenr = ED_space_clip_get_clip_frame_number(sc);

	/* get rectangle from operator */
	BLI_lasso_boundbox(&rect, mcords, moves);

	/* do actual selection */
	track = tracksbase->first;
	while (track) {
		if ((track->flag & TRACK_HIDDEN) == 0) {
			MovieTrackingMarker *marker = BKE_tracking_marker_get(track, framenr);

			if (MARKER_VISIBLE(sc, track, marker)) {
				float screen_co[2];

				/* marker in screen coords */
				ED_clip_point_stable_pos__reverse(sc, ar, marker->pos, screen_co);

				if (BLI_rcti_isect_pt(&rect, screen_co[0], screen_co[1]) &&
				    BLI_lasso_is_point_inside(mcords, moves, screen_co[0], screen_co[1], V2D_IS_CLIPPED))
				{
					if (select)
						BKE_tracking_track_flag_set(track, TRACK_AREA_ALL, SELECT);
					else
						BKE_tracking_track_flag_clear(track, TRACK_AREA_ALL, SELECT);
				}

				changed = true;
			}
		}

		track = track->next;
	}

	for (plane_track = plane_tracks_base->first;
	     plane_track;
	     plane_track = plane_track->next)
	{
		if ((plane_track->flag & PLANE_TRACK_HIDDEN) == 0) {
			MovieTrackingPlaneMarker *plane_marker =
				BKE_tracking_plane_marker_get(plane_track, framenr);
			int i;

			for (i = 0; i < 4; i++) {
				float screen_co[2];

				/* marker in screen coords */
				ED_clip_point_stable_pos__reverse(sc, ar, plane_marker->corners[i], screen_co);

				if (BLI_rcti_isect_pt(&rect, screen_co[0], screen_co[1]) &&
				    BLI_lasso_is_point_inside(mcords, moves, screen_co[0], screen_co[1], V2D_IS_CLIPPED))
				{
					if (select) {
						plane_track->flag |= SELECT;
					}
					else {
						plane_track->flag &= ~SELECT;
					}
				}
			}

			changed = true;
		}
	}

	if (changed) {
		BKE_tracking_dopesheet_tag_update(tracking);

		WM_event_add_notifier(C, NC_GEOM | ND_SELECT, NULL);
	}

	return changed;
}

static int clip_lasso_select_exec(bContext *C, wmOperator *op)
{
	int mcords_tot;
	const int (*mcords)[2] = WM_gesture_lasso_path_to_array(C, op, &mcords_tot);

	if (mcords) {
		short select;

		select = !RNA_boolean_get(op->ptr, "deselect");
		do_lasso_select_marker(C, mcords, mcords_tot, select);

		MEM_freeN((void *)mcords);

		return OPERATOR_FINISHED;
	}
	return OPERATOR_PASS_THROUGH;
}

void CLIP_OT_select_lasso(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Lasso Select";
	ot->description = "Select markers using lasso selection";
	ot->idname = "CLIP_OT_select_lasso";

	/* api callbacks */
	ot->invoke = WM_gesture_lasso_invoke;
	ot->modal = WM_gesture_lasso_modal;
	ot->exec = clip_lasso_select_exec;
	ot->poll = ED_space_clip_tracking_poll;
	ot->cancel = WM_gesture_lasso_cancel;

	/* flags */
	ot->flag = OPTYPE_UNDO;

	/* properties */
	RNA_def_collection_runtime(ot->srna, "path", &RNA_OperatorMousePath, "Path", "");
	RNA_def_boolean(ot->srna, "deselect", 0, "Deselect", "Deselect rather than select items");
	RNA_def_boolean(ot->srna, "extend", 1, "Extend", "Extend selection instead of deselecting everything first");
}

/********************** circle select operator *********************/

static int point_inside_ellipse(float point[2], float offset[2], float ellipse[2])
{
	/* normalized ellipse: ell[0] = scaleX, ell[1] = scaleY */
	float x, y;

	x = (point[0] - offset[0]) * ellipse[0];
	y = (point[1] - offset[1]) * ellipse[1];

	return x * x + y * y < 1.0f;
}

static int marker_inside_ellipse(MovieTrackingMarker *marker, float offset[2], float ellipse[2])
{
	return point_inside_ellipse(marker->pos, offset, ellipse);
}

static int circle_select_exec(bContext *C, wmOperator *op)
{
	SpaceClip *sc = CTX_wm_space_clip(C);
	ARegion *ar = CTX_wm_region(C);

	MovieClip *clip = ED_space_clip_get_clip(sc);
	MovieTracking *tracking = &clip->tracking;
	MovieTrackingTrack *track;
	MovieTrackingPlaneTrack *plane_track;
	ListBase *tracksbase = BKE_tracking_get_active_tracks(tracking);
	ListBase *plane_tracks_base = BKE_tracking_get_active_plane_tracks(tracking);
	int x, y, radius, width, height, mode;
	bool changed = false;
	float zoomx, zoomy, offset[2], ellipse[2];
	int framenr = ED_space_clip_get_clip_frame_number(sc);

	/* get operator properties */
	x = RNA_int_get(op->ptr, "x");
	y = RNA_int_get(op->ptr, "y");
	radius = RNA_int_get(op->ptr, "radius");

	mode = RNA_int_get(op->ptr, "gesture_mode");

	/* compute ellipse and position in unified coordinates */
	ED_space_clip_get_size(sc, &width, &height);
	ED_space_clip_get_zoom(sc, ar, &zoomx, &zoomy);

	ellipse[0] = width * zoomx / radius;
	ellipse[1] = height * zoomy / radius;

	ED_clip_point_stable_pos(sc, ar, x, y, &offset[0], &offset[1]);

	/* do selection */
	track = tracksbase->first;
	while (track) {
		if ((track->flag & TRACK_HIDDEN) == 0) {
			MovieTrackingMarker *marker = BKE_tracking_marker_get(track, framenr);

			if (MARKER_VISIBLE(sc, track, marker) && marker_inside_ellipse(marker, offset, ellipse)) {
				if (mode == GESTURE_MODAL_SELECT)
					BKE_tracking_track_flag_set(track, TRACK_AREA_ALL, SELECT);
				else
					BKE_tracking_track_flag_clear(track, TRACK_AREA_ALL, SELECT);

				changed = true;
			}
		}

		track = track->next;
	}

	for (plane_track = plane_tracks_base->first;
	     plane_track;
	     plane_track = plane_track->next)
	{
		if ((plane_track->flag & PLANE_TRACK_HIDDEN) == 0) {
			MovieTrackingPlaneMarker *plane_marker =
				BKE_tracking_plane_marker_get(plane_track, framenr);
			int i;

			for (i = 0; i < 4; i++) {
				if (point_inside_ellipse(plane_marker->corners[i], offset, ellipse)) {
					if (mode == GESTURE_MODAL_SELECT) {
						plane_track->flag |= SELECT;
					}
					else {
						plane_track->flag &= ~SELECT;
					}
				}
			}

			changed = true;
		}
	}

	if (changed) {
		BKE_tracking_dopesheet_tag_update(tracking);

		WM_event_add_notifier(C, NC_GEOM | ND_SELECT, NULL);

		return OPERATOR_FINISHED;
	}

	return OPERATOR_CANCELLED;
}

void CLIP_OT_select_circle(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Circle Select";
	ot->description = "Select markers using circle selection";
	ot->idname = "CLIP_OT_select_circle";

	/* api callbacks */
	ot->invoke = WM_gesture_circle_invoke;
	ot->modal = WM_gesture_circle_modal;
	ot->exec = circle_select_exec;
	ot->poll = ED_space_clip_tracking_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* properties */
	RNA_def_int(ot->srna, "x", 0, INT_MIN, INT_MAX, "X", "", INT_MIN, INT_MAX);
	RNA_def_int(ot->srna, "y", 0, INT_MIN, INT_MAX, "Y", "", INT_MIN, INT_MAX);
	RNA_def_int(ot->srna, "radius", 0, INT_MIN, INT_MAX, "Radius", "", INT_MIN, INT_MAX);
	RNA_def_int(ot->srna, "gesture_mode", 0, INT_MIN, INT_MAX, "Gesture Mode", "", INT_MIN, INT_MAX);
}

/********************** select all operator *********************/

static int select_all_exec(bContext *C, wmOperator *op)
{
	SpaceClip *sc = CTX_wm_space_clip(C);
	MovieClip *clip = ED_space_clip_get_clip(sc);
	MovieTracking *tracking = &clip->tracking;
	MovieTrackingTrack *track = NULL;   /* selected track */
	MovieTrackingPlaneTrack *plane_track = NULL;   /* selected plane track */
	MovieTrackingMarker *marker;
	ListBase *tracksbase = BKE_tracking_get_active_tracks(tracking);
	ListBase *plane_tracks_base = BKE_tracking_get_active_plane_tracks(tracking);
	int action = RNA_enum_get(op->ptr, "action");
	int framenr = ED_space_clip_get_clip_frame_number(sc);
	bool has_selection = false;

	if (action == SEL_TOGGLE) {
		action = SEL_SELECT;

		for (track = tracksbase->first; track; track = track->next) {
			if (TRACK_VIEW_SELECTED(sc, track)) {
				marker = BKE_tracking_marker_get(track, framenr);

				if (MARKER_VISIBLE(sc, track, marker)) {
					action = SEL_DESELECT;
					break;
				}
			}
		}

		for (plane_track = plane_tracks_base->first;
		     plane_track;
		     plane_track = plane_track->next)
		{
			if (plane_track->flag & SELECT) {
				action = SEL_DESELECT;
				break;
			}
		}
	}

	for (track = tracksbase->first; track; track = track->next) {
		if ((track->flag & TRACK_HIDDEN) == 0) {
			marker = BKE_tracking_marker_get(track, framenr);

			if (MARKER_VISIBLE(sc, track, marker)) {
				switch (action) {
					case SEL_SELECT:
						track->flag |= SELECT;
						track->pat_flag |= SELECT;
						track->search_flag |= SELECT;
						break;
					case SEL_DESELECT:
						track->flag &= ~SELECT;
						track->pat_flag &= ~SELECT;
						track->search_flag &= ~SELECT;
						break;
					case SEL_INVERT:
						track->flag ^= SELECT;
						track->pat_flag ^= SELECT;
						track->search_flag ^= SELECT;
						break;
				}
			}
		}

		if (TRACK_VIEW_SELECTED(sc, track))
			has_selection = true;
	}

	for (plane_track = plane_tracks_base->first;
	     plane_track;
	     plane_track = plane_track->next)
	{
		if ((plane_track->flag & PLANE_TRACK_HIDDEN) == 0) {
			switch (action) {
				case SEL_SELECT:
					plane_track->flag |= SELECT;
					break;
				case SEL_DESELECT:
					plane_track->flag &= ~SELECT;
					break;
				case SEL_INVERT:
					plane_track->flag ^= SELECT;
					break;
			}
		}

		if (plane_track->flag & SELECT) {
			has_selection = true;
		}
	}

	if (!has_selection)
		sc->flag &= ~SC_LOCK_SELECTION;

	BKE_tracking_dopesheet_tag_update(tracking);

	WM_event_add_notifier(C, NC_GEOM | ND_SELECT, NULL);

	return OPERATOR_FINISHED;
}

void CLIP_OT_select_all(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "(De)select All";
	ot->description = "Change selection of all tracking markers";
	ot->idname = "CLIP_OT_select_all";

	/* api callbacks */
	ot->exec = select_all_exec;
	ot->poll = ED_space_clip_tracking_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	WM_operator_properties_select_all(ot);
}

/********************** select grouped operator *********************/

static int select_groped_exec(bContext *C, wmOperator *op)
{
	SpaceClip *sc = CTX_wm_space_clip(C);
	MovieClip *clip = ED_space_clip_get_clip(sc);
	MovieTrackingTrack *track;
	MovieTrackingMarker *marker;
	MovieTracking *tracking = &clip->tracking;
	ListBase *tracksbase = BKE_tracking_get_active_tracks(tracking);
	int group = RNA_enum_get(op->ptr, "group");
	int framenr = ED_space_clip_get_clip_frame_number(sc);

	track = tracksbase->first;
	while (track) {
		bool ok = false;

		marker = BKE_tracking_marker_get(track, framenr);

		if (group == 0) { /* Keyframed */
			ok = marker->framenr == framenr && (marker->flag & MARKER_TRACKED) == 0;
		}
		else if (group == 1) { /* Estimated */
			ok = marker->framenr != framenr;
		}
		else if (group == 2) { /* tracked */
			ok = marker->framenr == framenr && (marker->flag & MARKER_TRACKED);
		}
		else if (group == 3) { /* locked */
			ok = track->flag & TRACK_LOCKED;
		}
		else if (group == 4) { /* disabled */
			ok = marker->flag & MARKER_DISABLED;
		}
		else if (group == 5) { /* color */
			MovieTrackingTrack *act_track = BKE_tracking_track_get_active(tracking);

			if (act_track) {
				ok = (track->flag & TRACK_CUSTOMCOLOR) == (act_track->flag & TRACK_CUSTOMCOLOR);

				if (ok && track->flag & TRACK_CUSTOMCOLOR)
					ok = equals_v3v3(track->color, act_track->color);
			}
		}
		else if (group == 6) { /* failed */
			ok = (track->flag & TRACK_HAS_BUNDLE) == 0;
		}

		if (ok) {
			track->flag |= SELECT;
			if (sc->flag & SC_SHOW_MARKER_PATTERN)
				track->pat_flag |= SELECT;
			if (sc->flag & SC_SHOW_MARKER_SEARCH)
				track->search_flag |= SELECT;
		}

		track = track->next;
	}

	BKE_tracking_dopesheet_tag_update(tracking);

	WM_event_add_notifier(C, NC_MOVIECLIP | ND_DISPLAY, clip);

	return OPERATOR_FINISHED;
}

void CLIP_OT_select_grouped(wmOperatorType *ot)
{
	static EnumPropertyItem select_group_items[] = {
		{0, "KEYFRAMED", 0, "Keyframed tracks", "Select all keyframed tracks"},
		{1, "ESTIMATED", 0, "Estimated tracks", "Select all estimated tracks"},
		{2, "TRACKED", 0, "Tracked tracks", "Select all tracked tracks"},
		{3, "LOCKED", 0, "Locked tracks", "Select all locked tracks"},
		{4, "DISABLED", 0, "Disabled tracks", "Select all disabled tracks"},
		{5, "COLOR", 0, "Tracks with same color", "Select all tracks with same color as active track"},
		{6, "FAILED", 0, "Failed Tracks", "Select all tracks which failed to be reconstructed"},
		{0, NULL, 0, NULL, NULL}
	};

	/* identifiers */
	ot->name = "Select Grouped";
	ot->description = "Select all tracks from specified group";
	ot->idname = "CLIP_OT_select_grouped";

	/* api callbacks */
	ot->exec = select_groped_exec;
	ot->poll = ED_space_clip_tracking_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* proeprties */
	RNA_def_enum(ot->srna, "group", select_group_items, TRACK_CLEAR_REMAINED, "Action", "Clear action to execute");
}
