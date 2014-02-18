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

/** \file blender/editors/space_clip/tracking_ops.c
 *  \ingroup spclip
 */

#include "MEM_guardedalloc.h"

#include "DNA_camera_types.h"
#include "DNA_constraint_types.h"
#include "DNA_gpencil_types.h"
#include "DNA_mask_types.h"
#include "DNA_movieclip_types.h"
#include "DNA_object_types.h"   /* SELECT */
#include "DNA_scene_types.h"

#include "BLI_utildefines.h"
#include "BLI_math.h"
#include "BLI_listbase.h"
#include "BLI_rect.h"
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
#include "BKE_mask.h"
#include "BKE_node.h"
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

#include "BLF_translation.h"

#include "PIL_time.h"

#include "UI_view2d.h"

#include "clip_intern.h"    // own include

/********************** add marker operator *********************/

static bool add_marker(const bContext *C, float x, float y)
{
	SpaceClip *sc = CTX_wm_space_clip(C);
	MovieClip *clip = ED_space_clip_get_clip(sc);
	MovieTracking *tracking = &clip->tracking;
	ListBase *tracksbase = BKE_tracking_get_active_tracks(tracking);
	ListBase *plane_tracks_base = BKE_tracking_get_active_plane_tracks(tracking);
	MovieTrackingTrack *track;
	int width, height;
	int framenr = ED_space_clip_get_clip_frame_number(sc);

	ED_space_clip_get_size(sc, &width, &height);

	if (width == 0 || height == 0) {
		return false;
	}

	track = BKE_tracking_track_add(tracking, tracksbase, x, y, framenr, width, height);

	BKE_tracking_track_select(tracksbase, track, TRACK_AREA_ALL, 0);
	BKE_tracking_plane_tracks_deselect_all(plane_tracks_base);

	clip->tracking.act_track = track;
	clip->tracking.act_plane_track = NULL;

	return true;
}

static int add_marker_exec(bContext *C, wmOperator *op)
{
	SpaceClip *sc = CTX_wm_space_clip(C);
	MovieClip *clip = ED_space_clip_get_clip(sc);
	float pos[2];

	RNA_float_get_array(op->ptr, "location", pos);

	if (!add_marker(C, pos[0], pos[1])) {
		return OPERATOR_CANCELLED;
	}

	/* reset offset from locked position, so frame jumping wouldn't be so confusing */
	sc->xlockof = 0;
	sc->ylockof = 0;

	WM_event_add_notifier(C, NC_MOVIECLIP | NA_EDITED, clip);

	return OPERATOR_FINISHED;
}

static int add_marker_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
	SpaceClip *sc = CTX_wm_space_clip(C);
	ARegion *ar = CTX_wm_region(C);

	if (!RNA_struct_property_is_set(op->ptr, "location")) {
		/* If location is not set, use mouse positio nas default. */
		float co[2];
		ED_clip_mouse_pos(sc, ar, event->mval, co);
		RNA_float_set_array(op->ptr, "location", co);
	}

	return add_marker_exec(C, op);
}

void CLIP_OT_add_marker(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Add Marker";
	ot->idname = "CLIP_OT_add_marker";
	ot->description = "Place new marker at specified location";

	/* api callbacks */
	ot->invoke = add_marker_invoke;
	ot->exec = add_marker_exec;
	ot->poll = ED_space_clip_tracking_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* properties */
	RNA_def_float_vector(ot->srna, "location", 2, NULL, -FLT_MAX, FLT_MAX,
	                     "Location", "Location of marker on frame", -1.0f, 1.0f);
}

/********************** add marker operator *********************/

static int add_marker_at_click_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
	ED_area_headerprint(CTX_wm_area(C), IFACE_("Use LMB click to define location where place the marker"));

	/* add modal handler for ESC */
	WM_event_add_modal_handler(C, op);

	return OPERATOR_RUNNING_MODAL;
}

static int add_marker_at_click_modal(bContext *C, wmOperator *UNUSED(op), const wmEvent *event)
{
	SpaceClip *sc = CTX_wm_space_clip(C);
	MovieClip *clip = ED_space_clip_get_clip(sc);
	ARegion *ar = CTX_wm_region(C);
	float pos[2];

	switch (event->type) {
		case MOUSEMOVE:
			return OPERATOR_RUNNING_MODAL;
			break;

		case LEFTMOUSE:
			ED_area_headerprint(CTX_wm_area(C), NULL);

			ED_clip_point_stable_pos(sc, ar,
			                         event->x - ar->winrct.xmin,
			                         event->y - ar->winrct.ymin,
			                         &pos[0], &pos[1]);

			if (!add_marker(C, pos[0], pos[1]))
				return OPERATOR_CANCELLED;

			WM_event_add_notifier(C, NC_MOVIECLIP | NA_EDITED, clip);
			return OPERATOR_FINISHED;
			break;

		case ESCKEY:
			ED_area_headerprint(CTX_wm_area(C), NULL);
			return OPERATOR_CANCELLED;
	}

	return OPERATOR_PASS_THROUGH;
}

void CLIP_OT_add_marker_at_click(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Add Marker at Click";
	ot->idname = "CLIP_OT_add_marker_at_click";
	ot->description = "Place new marker at the desired (clicked) position";

	/* api callbacks */
	ot->invoke = add_marker_at_click_invoke;
	ot->poll = ED_space_clip_tracking_poll;
	ot->modal = add_marker_at_click_modal;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_BLOCKING;
}

/********************** delete track operator *********************/

static int delete_track_exec(bContext *C, wmOperator *UNUSED(op))
{
	SpaceClip *sc = CTX_wm_space_clip(C);
	MovieClip *clip = ED_space_clip_get_clip(sc);
	MovieTracking *tracking = &clip->tracking;
	ListBase *tracksbase = BKE_tracking_get_active_tracks(tracking);
	ListBase *plane_tracks_base = BKE_tracking_get_active_plane_tracks(tracking);
	MovieTrackingTrack *track = tracksbase->first, *next;
	MovieTrackingPlaneTrack *plane_track, *next_plane_track;
	bool changed = false;

	/* Delete selected plane tracks. */
	for (plane_track = plane_tracks_base->first;
	     plane_track;
	     plane_track = next_plane_track)
	{
		next_plane_track = plane_track->next;

		if (plane_track->flag & SELECT) {
			BKE_tracking_plane_track_free(plane_track);
			BLI_freelinkN(plane_tracks_base, plane_track);
			changed = true;
		}
	}

	/* Remove selected point tracks (they'll also be removed from planes which uses them). */
	while (track) {
		next = track->next;

		if (TRACK_VIEW_SELECTED(sc, track)) {
			clip_delete_track(C, clip, track);
			changed = true;
		}

		track = next;
	}

	/* nothing selected now, unlock view so it can be scrolled nice again */
	sc->flag &= ~SC_LOCK_SELECTION;

	if (changed)
		WM_event_add_notifier(C, NC_MOVIECLIP | NA_EDITED, clip);

	return OPERATOR_FINISHED;
}

void CLIP_OT_delete_track(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Delete Track";
	ot->idname = "CLIP_OT_delete_track";
	ot->description = "Delete selected tracks";

	/* api callbacks */
	ot->invoke = WM_operator_confirm;
	ot->exec = delete_track_exec;
	ot->poll = ED_space_clip_tracking_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/********************** delete marker operator *********************/

static int delete_marker_exec(bContext *C, wmOperator *UNUSED(op))
{
	SpaceClip *sc = CTX_wm_space_clip(C);
	MovieClip *clip = ED_space_clip_get_clip(sc);
	ListBase *tracksbase = BKE_tracking_get_active_tracks(&clip->tracking);
	ListBase *plane_tracks_base = BKE_tracking_get_active_plane_tracks(&clip->tracking);
	MovieTrackingTrack *track = tracksbase->first, *next;
	MovieTrackingPlaneTrack *plane_track, *plane_track_next;
	int framenr = ED_space_clip_get_clip_frame_number(sc);
	bool has_selection = false;
	bool changed = false;

	while (track) {
		next = track->next;

		if (TRACK_VIEW_SELECTED(sc, track)) {
			MovieTrackingMarker *marker = BKE_tracking_marker_get_exact(track, framenr);

			if (marker) {
				has_selection |= track->markersnr > 1;

				clip_delete_marker(C, clip, track, marker);
				changed = true;
			}
		}

		track = next;
	}

	for (plane_track = plane_tracks_base->first;
	     plane_track;
	     plane_track = plane_track_next)
	{
		plane_track_next = plane_track->next;

		if (plane_track->flag & SELECT) {
			MovieTrackingPlaneMarker *plane_marker = BKE_tracking_plane_marker_get_exact(plane_track, framenr);

			if (plane_marker) {
				if (plane_track->markersnr == 1) {
					BKE_tracking_plane_track_free(plane_track);
					BLI_freelinkN(plane_tracks_base, plane_track);
				}
				else {
					BKE_tracking_plane_marker_delete(plane_track, framenr);
				}

				changed = true;
			}
		}
	}

	if (!has_selection) {
		/* nothing selected now, unlock view so it can be scrolled nice again */
		sc->flag &= ~SC_LOCK_SELECTION;
	}

	if (!changed)
		return OPERATOR_CANCELLED;

	return OPERATOR_FINISHED;
}

void CLIP_OT_delete_marker(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Delete Marker";
	ot->idname = "CLIP_OT_delete_marker";
	ot->description = "Delete marker for current frame from selected tracks";

	/* api callbacks */
	ot->invoke = WM_operator_confirm;
	ot->exec = delete_marker_exec;
	ot->poll = ED_space_clip_tracking_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/********************** slide marker operator *********************/

#define SLIDE_ACTION_POS       0
#define SLIDE_ACTION_SIZE      1
#define SLIDE_ACTION_OFFSET    2
#define SLIDE_ACTION_TILT_SIZE 3

typedef struct {
	short area, action;
	MovieTrackingTrack *track;
	MovieTrackingMarker *marker;

	int mval[2];
	int width, height;
	float *min, *max, *pos, *offset, (*corners)[2];
	float spos[2];

	short lock, accurate;

	/* data to restore on cancel */
	float old_search_min[2], old_search_max[2], old_pos[2], old_offset[2];
	float old_corners[4][2];
	float (*old_markers)[2];
} SlideMarkerData;

static void slide_marker_tilt_slider(MovieTrackingMarker *marker, float slider[2])
{
	add_v2_v2v2(slider, marker->pattern_corners[1], marker->pattern_corners[2]);
	add_v2_v2(slider, marker->pos);
}

static SlideMarkerData *create_slide_marker_data(SpaceClip *sc, MovieTrackingTrack *track,
                                                 MovieTrackingMarker *marker, const wmEvent *event,
                                                 int area, int corner, int action, int width, int height)
{
	SlideMarkerData *data = MEM_callocN(sizeof(SlideMarkerData), "slide marker data");
	int framenr = ED_space_clip_get_clip_frame_number(sc);

	marker = BKE_tracking_marker_ensure(track, framenr);

	data->area = area;
	data->action = action;
	data->track = track;
	data->marker = marker;

	if (area == TRACK_AREA_POINT) {
		data->pos = marker->pos;
		data->offset = track->offset;
	}
	else if (area == TRACK_AREA_PAT) {
		if (action == SLIDE_ACTION_SIZE) {
			data->corners = marker->pattern_corners;
		}
		else if (action == SLIDE_ACTION_OFFSET) {
			int a;

			data->pos = marker->pos;
			data->offset = track->offset;

			data->old_markers = MEM_callocN(sizeof(*data->old_markers) * track->markersnr, "slide marekrs");
			for (a = 0; a < track->markersnr; a++)
				copy_v2_v2(data->old_markers[a], track->markers[a].pos);
		}
		else if (action == SLIDE_ACTION_POS) {
			data->corners = marker->pattern_corners;
			data->pos = marker->pattern_corners[corner];
			copy_v2_v2(data->spos, data->pos);
		}
		else if (action == SLIDE_ACTION_TILT_SIZE) {
			data->corners = marker->pattern_corners;
			slide_marker_tilt_slider(marker, data->spos);
		}
	}
	else if (area == TRACK_AREA_SEARCH) {
		data->min = marker->search_min;
		data->max = marker->search_max;
	}

	data->mval[0] = event->mval[0];
	data->mval[1] = event->mval[1];

	data->width = width;
	data->height = height;

	if (action == SLIDE_ACTION_SIZE)
		data->lock = 1;

	/* backup marker's settings */
	memcpy(data->old_corners, marker->pattern_corners, sizeof(data->old_corners));
	copy_v2_v2(data->old_search_min, marker->search_min);
	copy_v2_v2(data->old_search_max, marker->search_max);
	copy_v2_v2(data->old_pos, marker->pos);
	copy_v2_v2(data->old_offset, track->offset);

	return data;
}

static int mouse_on_slide_zone(SpaceClip *sc, MovieTrackingMarker *marker,
                               int area, float co[2], float slide_zone[2],
                               float padding, int width, int height)
{
	const float size = 12.0f;
	float min[2], max[2];
	float dx, dy;

	if (area == TRACK_AREA_SEARCH) {
		copy_v2_v2(min, marker->search_min);
		copy_v2_v2(max, marker->search_max);
	}
	else {
		BKE_tracking_marker_pattern_minmax(marker, min, max);
	}

	min[0] -= padding / width;
	min[1] -= padding / height;
	max[0] += padding / width;
	max[1] += padding / height;

	dx = size / width / sc->zoom;
	dy = size / height / sc->zoom;

	dx = min_ff(dx, (max[0] - min[0]) / 6.0f);
	dy = min_ff(dy, (max[1] - min[1]) / 6.0f);

	return IN_RANGE_INCL(co[0], slide_zone[0] - dx, slide_zone[0] + dx) &&
	       IN_RANGE_INCL(co[1], slide_zone[1] - dy, slide_zone[1] + dy);
}

static int mouse_on_corner(SpaceClip *sc, MovieTrackingMarker *marker,
                           int area, float co[2], int corner, float padding,
                           int width, int height)
{
	float min[2], max[2], crn[2];

	if (area == TRACK_AREA_SEARCH) {
		copy_v2_v2(min, marker->search_min);
		copy_v2_v2(max, marker->search_max);
	}
	else {
		BKE_tracking_marker_pattern_minmax(marker, min, max);
	}

	min[0] -= padding / width;
	min[1] -= padding / height;
	max[0] += padding / width;
	max[1] += padding / height;

	if (corner == 0) {
		crn[0] = marker->pos[0] + max[0];
		crn[1] = marker->pos[1] + min[1];
	}
	else {
		crn[0] = marker->pos[0] + min[0];
		crn[1] = marker->pos[1] + max[1];
	}

	return mouse_on_slide_zone(sc, marker, area, co, crn, padding, width, height);
}

static int get_mouse_pattern_corner(SpaceClip *sc, MovieTrackingMarker *marker, float co[2], int width, int height)
{
	int i, next;
	float len = FLT_MAX, dx, dy;

	for (i = 0; i < 4; i++) {
		float cur_len_sq;

		next = (i + 1) % 4;

		cur_len_sq = len_squared_v2v2(marker->pattern_corners[i], marker->pattern_corners[next]);

		len = min_ff(cur_len_sq, len);
	}
	len = sqrtf(len);

	dx = 12.0f / width / sc->zoom;
	dy = 12.0f / height / sc->zoom;

	dx = min_ff(dx, len * 2.0f / 3.0f);
	dy = min_ff(dy, len * width / height * 2.0f / 3.0f);

	for (i = 0; i < 4; i++) {
		float crn[2];
		int inside;

		add_v2_v2v2(crn, marker->pattern_corners[i], marker->pos);

		inside = IN_RANGE_INCL(co[0], crn[0] - dx, crn[0] + dx) &&
		         IN_RANGE_INCL(co[1], crn[1] - dy, crn[1] + dy);

		if (inside)
			return i;
	}

	return -1;
}

static int mouse_on_offset(SpaceClip *sc, MovieTrackingTrack *track, MovieTrackingMarker *marker,
                           float co[2], int width, int height)
{
	float pos[2], dx, dy;
	float pat_min[2], pat_max[2];

	BKE_tracking_marker_pattern_minmax(marker, pat_min, pat_max);

	add_v2_v2v2(pos, marker->pos, track->offset);

	dx = 12.0f / width / sc->zoom;
	dy = 12.0f / height / sc->zoom;

	dx = min_ff(dx, (pat_max[0] - pat_min[0]) / 2.0f);
	dy = min_ff(dy, (pat_max[1] - pat_min[1]) / 2.0f);

	return co[0] >= pos[0] - dx && co[0] <= pos[0] + dx && co[1] >= pos[1] - dy && co[1] <= pos[1] + dy;
}

static int mouse_on_tilt(SpaceClip *sc, MovieTrackingMarker *marker, float co[2], int width, int height)
{
	float slider[2];

	slide_marker_tilt_slider(marker, slider);

	return mouse_on_slide_zone(sc, marker, TRACK_AREA_PAT, co, slider, 0.0f, width, height);
}

static bool slide_check_corners(float (*corners)[2])
{
	int i, next, prev;
	float cross = 0.0f;
	float p[2] = {0.0f, 0.0f};

	if (!isect_point_quad_v2(p, corners[0], corners[1], corners[2], corners[3]))
		return false;

	for (i = 0; i < 4; i++) {
		float v1[2], v2[2], cur_cross;

		next = (i + 1) % 4;
		prev = (4 + i - 1) % 4;

		sub_v2_v2v2(v1, corners[i], corners[prev]);
		sub_v2_v2v2(v2, corners[next], corners[i]);

		cur_cross = cross_v2v2(v1, v2);

		if (fabsf(cur_cross) > FLT_EPSILON) {
			if (cross == 0.0f) {
				cross = cur_cross;
			}
			else if (cross * cur_cross < 0.0f) {
				return false;
			}
		}
	}

	return true;
}

static void hide_cursor(bContext *C)
{
	wmWindow *win = CTX_wm_window(C);

	WM_cursor_set(win, CURSOR_NONE);
}

static void show_cursor(bContext *C)
{
	wmWindow *win = CTX_wm_window(C);

	WM_cursor_set(win, CURSOR_STD);
}

MovieTrackingTrack *tracking_marker_check_slide(bContext *C, const wmEvent *event, int *area_r, int *action_r, int *corner_r)
{
	SpaceClip *sc = CTX_wm_space_clip(C);
	ARegion *ar = CTX_wm_region(C);

	MovieClip *clip = ED_space_clip_get_clip(sc);
	MovieTrackingTrack *track;
	int width, height;
	float co[2];
	ListBase *tracksbase = BKE_tracking_get_active_tracks(&clip->tracking);
	int framenr = ED_space_clip_get_clip_frame_number(sc);
	int action = -1, area = 0, corner = -1;

	ED_space_clip_get_size(sc, &width, &height);

	if (width == 0 || height == 0)
		return NULL;

	ED_clip_mouse_pos(sc, ar, event->mval, co);

	track = tracksbase->first;
	while (track) {
		if (TRACK_VIEW_SELECTED(sc, track) && (track->flag & TRACK_LOCKED) == 0) {
			MovieTrackingMarker *marker = BKE_tracking_marker_get(track, framenr);
			bool ok = false;

			if ((marker->flag & MARKER_DISABLED) == 0) {
				if (mouse_on_offset(sc, track, marker, co, width, height)) {
					area = TRACK_AREA_POINT;
					action = SLIDE_ACTION_POS;
					ok = true;
				}

				if (!ok && (sc->flag & SC_SHOW_MARKER_SEARCH)) {
					if (mouse_on_corner(sc, marker, TRACK_AREA_SEARCH, co, 1, 0.0f, width, height)) {
						area = TRACK_AREA_SEARCH;
						action = SLIDE_ACTION_OFFSET;
						ok = true;
					}
					else if (mouse_on_corner(sc, marker, TRACK_AREA_SEARCH, co, 0, 0.0f, width, height)) {
						area = TRACK_AREA_SEARCH;
						action = SLIDE_ACTION_SIZE;
						ok = true;
					}
				}

				if (!ok && (sc->flag & SC_SHOW_MARKER_PATTERN)) {
					int current_corner = get_mouse_pattern_corner(sc, marker, co, width, height);

					if (current_corner != -1) {
						area = TRACK_AREA_PAT;
						action = SLIDE_ACTION_POS;
						corner = current_corner;
						ok = true;
					}
					else {
#if 0
						/* TODO: disable for now, needs better approaches for visualization */

						if (mouse_on_corner(sc, marker, TRACK_AREA_PAT, co, 1, 12.0f, width, height)) {
							area = TRACK_AREA_PAT;
							action = SLIDE_ACTION_OFFSET;
							ok = true;
						}
						if (!ok && mouse_on_corner(sc, marker, TRACK_AREA_PAT, co, 0, 12.0f, width, height)) {
							area = TRACK_AREA_PAT;
							action = SLIDE_ACTION_SIZE;
							ok = true;
						}
#endif
						if (!ok && mouse_on_tilt(sc, marker, co, width, height)) {
							area = TRACK_AREA_PAT;
							action = SLIDE_ACTION_TILT_SIZE;
							ok = true;
						}
					}
				}

				if (ok) {
					if (area_r)
						*area_r = area;

					if (action_r)
						*action_r = action;

					if (corner_r)
						*corner_r = corner;

					return track;
				}
			}
		}

		track = track->next;
	}

	return NULL;
}

static void *slide_marker_customdata(bContext *C, const wmEvent *event)
{
	SpaceClip *sc = CTX_wm_space_clip(C);
	ARegion *ar = CTX_wm_region(C);

	MovieTrackingTrack *track;
	int width, height;
	float co[2];
	void *customdata = NULL;
	int framenr = ED_space_clip_get_clip_frame_number(sc);
	int area, action, corner;

	ED_space_clip_get_size(sc, &width, &height);

	if (width == 0 || height == 0)
		return NULL;

	ED_clip_mouse_pos(sc, ar, event->mval, co);

	track = tracking_marker_check_slide(C, event, &area, &action, &corner);
	if (track) {
		MovieTrackingMarker *marker = BKE_tracking_marker_get(track, framenr);

		customdata = create_slide_marker_data(sc, track, marker, event, area, corner, action, width, height);
	}

	return customdata;
}

static int slide_marker_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
	SlideMarkerData *slidedata = slide_marker_customdata(C, event);

	if (slidedata) {
		SpaceClip *sc = CTX_wm_space_clip(C);
		MovieClip *clip = ED_space_clip_get_clip(sc);
		MovieTracking *tracking = &clip->tracking;

		tracking->act_track = slidedata->track;
		tracking->act_plane_track = NULL;

		op->customdata = slidedata;

		hide_cursor(C);
		WM_event_add_modal_handler(C, op);

		WM_event_add_notifier(C, NC_GEOM | ND_SELECT, NULL);

		return OPERATOR_RUNNING_MODAL;
	}

	return OPERATOR_PASS_THROUGH;
}

static void cancel_mouse_slide(SlideMarkerData *data)
{
	MovieTrackingTrack *track = data->track;
	MovieTrackingMarker *marker = data->marker;

	memcpy(marker->pattern_corners, data->old_corners, sizeof(marker->pattern_corners));
	copy_v2_v2(marker->search_min, data->old_search_min);
	copy_v2_v2(marker->search_max, data->old_search_max);
	copy_v2_v2(marker->pos, data->old_pos);
	copy_v2_v2(track->offset, data->old_offset);

	if (data->old_markers) {
		int a;

		for (a = 0; a < data->track->markersnr; a++)
			copy_v2_v2(data->track->markers[a].pos, data->old_markers[a]);
	}
}

static void apply_mouse_slide(bContext *C, SlideMarkerData *data)
{
	if (data->area == TRACK_AREA_POINT) {
		SpaceClip *sc = CTX_wm_space_clip(C);
		MovieClip *clip = ED_space_clip_get_clip(sc);
		MovieTrackingPlaneTrack *plane_track;
		ListBase *plane_tracks_base = BKE_tracking_get_active_plane_tracks(&clip->tracking);
		int framenr = ED_space_clip_get_clip_frame_number(sc);

		for (plane_track = plane_tracks_base->first;
		     plane_track;
		     plane_track = plane_track->next)
		{
			if ((plane_track->flag & PLANE_TRACK_AUTOKEY) == 0) {
				int i;
				for (i = 0; i < plane_track->point_tracksnr; i++) {
					if (plane_track->point_tracks[i] == data->track) {
						BKE_tracking_track_plane_from_existing_motion(plane_track, framenr);
						break;
					}
				}
			}
		}
	}
}

static void free_slide_data(SlideMarkerData *data)
{
	if (data->old_markers)
		MEM_freeN(data->old_markers);

	MEM_freeN(data);
}

static int slide_marker_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
	SpaceClip *sc = CTX_wm_space_clip(C);
	ARegion *ar = CTX_wm_region(C);

	SlideMarkerData *data = (SlideMarkerData *)op->customdata;
	float dx, dy, mdelta[2];

	switch (event->type) {
		case LEFTCTRLKEY:
		case RIGHTCTRLKEY:
		case LEFTSHIFTKEY:
		case RIGHTSHIFTKEY:
			if (data->action == SLIDE_ACTION_SIZE)
				if (ELEM(event->type, LEFTCTRLKEY, RIGHTCTRLKEY))
					data->lock = event->val == KM_RELEASE;

			if (ELEM(event->type, LEFTSHIFTKEY, RIGHTSHIFTKEY))
				data->accurate = event->val == KM_PRESS;

			/* fall-through */
		case MOUSEMOVE:
			mdelta[0] = event->mval[0] - data->mval[0];
			mdelta[1] = event->mval[1] - data->mval[1];

			dx = mdelta[0] / data->width / sc->zoom;

			if (data->lock)
				dy = -dx / data->height * data->width;
			else
				dy = mdelta[1] / data->height / sc->zoom;

			if (data->accurate) {
				dx /= 5;
				dy /= 5;
			}

			if (data->area == TRACK_AREA_POINT) {
				if (data->action == SLIDE_ACTION_OFFSET) {
					data->offset[0] = data->old_offset[0] + dx;
					data->offset[1] = data->old_offset[1] + dy;
				}
				else {
					data->pos[0] = data->old_pos[0] + dx;
					data->pos[1] = data->old_pos[1] + dy;
				}

				WM_event_add_notifier(C, NC_OBJECT | ND_TRANSFORM, NULL);
				DAG_id_tag_update(&sc->clip->id, 0);
			}
			else if (data->area == TRACK_AREA_PAT) {
				if (data->action == SLIDE_ACTION_SIZE) {
					float start[2], end[2];
					float scale;

					ED_clip_point_stable_pos(sc, ar, data->mval[0], data->mval[1], &start[0], &start[1]);

					sub_v2_v2(start, data->old_pos);

					if (len_squared_v2(start) != 0.0f) {
						float mval[2];

						if (data->accurate) {
							mval[0] = data->mval[0] + (event->mval[0] - data->mval[0]) / 5.0f;
							mval[1] = data->mval[1] + (event->mval[1] - data->mval[1]) / 5.0f;
						}
						else {
							mval[0] = event->mval[0];
							mval[1] = event->mval[1];
						}

						ED_clip_point_stable_pos(sc, ar, mval[0], mval[1], &end[0], &end[1]);

						sub_v2_v2(end, data->old_pos);

						scale = len_v2(end) / len_v2(start);

						if (scale > 0.0f) {
							int a;

							for (a = 0; a < 4; a++) {
								mul_v2_v2fl(data->corners[a], data->old_corners[a], scale);
							}
						}
					}

					BKE_tracking_marker_clamp(data->marker, CLAMP_PAT_DIM);
				}
				else if (data->action == SLIDE_ACTION_OFFSET) {
					float d[2] = {dx, dy};
					int a;

					for (a = 0; a < data->track->markersnr; a++)
						add_v2_v2v2(data->track->markers[a].pos, data->old_markers[a], d);

					sub_v2_v2v2(data->offset, data->old_offset, d);
				}
				else if (data->action == SLIDE_ACTION_POS) {
					float spos[2];

					copy_v2_v2(spos, data->pos);

					data->pos[0] = data->spos[0] + dx;
					data->pos[1] = data->spos[1] + dy;

					if (!slide_check_corners(data->corners)) {
						copy_v2_v2(data->pos, spos);
					}

					/* currently only patterns are allowed to have such combination of event and data */
					BKE_tracking_marker_clamp(data->marker, CLAMP_PAT_DIM);
				}
				else if (data->action == SLIDE_ACTION_TILT_SIZE) {
					float start[2], end[2];
					float scale = 1.0f, angle = 0.0f;
					int a;
					float mval[2];

					if (data->accurate) {
						mval[0] = data->mval[0] + (event->mval[0] - data->mval[0]) / 5.0f;
						mval[1] = data->mval[1] + (event->mval[1] - data->mval[1]) / 5.0f;
					}
					else {
						mval[0] = event->mval[0];
						mval[1] = event->mval[1];
					}

					sub_v2_v2v2(start, data->spos, data->old_pos);

					ED_clip_point_stable_pos(sc, ar, mval[0], mval[1], &end[0], &end[1]);
					sub_v2_v2(end, data->old_pos);

					if (len_squared_v2(start) != 0.0f) {
						scale = len_v2(end) / len_v2(start);

						if (scale < 0.0f) {
							scale = 0.0;
						}
					}

					angle = -angle_signed_v2v2(start, end);

					for (a = 0; a < 4; a++) {
						float vec[2];

						mul_v2_v2fl(data->corners[a], data->old_corners[a], scale);

						copy_v2_v2(vec, data->corners[a]);
						vec[0] *= data->width;
						vec[1] *= data->height;

						data->corners[a][0] = (vec[0] * cosf(angle) - vec[1] * sinf(angle)) / data->width;
						data->corners[a][1] = (vec[1] * cosf(angle) + vec[0] * sinf(angle)) / data->height;
					}

					BKE_tracking_marker_clamp(data->marker, CLAMP_PAT_DIM);

				}
			}
			else if (data->area == TRACK_AREA_SEARCH) {
				if (data->action == SLIDE_ACTION_SIZE) {
					data->min[0] = data->old_search_min[0] - dx;
					data->max[0] = data->old_search_max[0] + dx;

					data->min[1] = data->old_search_min[1] + dy;
					data->max[1] = data->old_search_max[1] - dy;

					BKE_tracking_marker_clamp(data->marker, CLAMP_SEARCH_DIM);
				}
				else if (data->area == TRACK_AREA_SEARCH) {
					float d[2] = {dx, dy};

					add_v2_v2v2(data->min, data->old_search_min, d);
					add_v2_v2v2(data->max, data->old_search_max, d);
				}

				BKE_tracking_marker_clamp(data->marker, CLAMP_SEARCH_POS);
			}

			data->marker->flag &= ~MARKER_TRACKED;

			WM_event_add_notifier(C, NC_MOVIECLIP | NA_EDITED, NULL);

			break;

		case LEFTMOUSE:
			if (event->val == KM_RELEASE) {
				apply_mouse_slide(C, op->customdata);
				free_slide_data(op->customdata);

				show_cursor(C);

				return OPERATOR_FINISHED;
			}

			break;

		case ESCKEY:
			cancel_mouse_slide(op->customdata);

			free_slide_data(op->customdata);

			show_cursor(C);

			WM_event_add_notifier(C, NC_MOVIECLIP | NA_EDITED, NULL);

			return OPERATOR_CANCELLED;
	}

	return OPERATOR_RUNNING_MODAL;
}

void CLIP_OT_slide_marker(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Slide Marker";
	ot->description = "Slide marker areas";
	ot->idname = "CLIP_OT_slide_marker";

	/* api callbacks */
	ot->poll = ED_space_clip_tracking_poll;
	ot->invoke = slide_marker_invoke;
	ot->modal = slide_marker_modal;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_GRAB_POINTER | OPTYPE_BLOCKING;

	/* properties */
	RNA_def_float_vector(ot->srna, "offset", 2, NULL, -FLT_MAX, FLT_MAX,
	                     "Offset", "Offset in floating point units, 1.0 is the width and height of the image", -FLT_MAX, FLT_MAX);
}

/********************** track operator *********************/

typedef struct TrackMarkersJob {
	struct MovieTrackingContext *context;   /* tracking context */
	int sfra, efra, lastfra;    /* Start, end and recently tracked frames */
	int backwards;              /* Backwards tracking flag */
	MovieClip *clip;            /* Clip which is tracking */
	float delay;                /* Delay in milliseconds to allow tracking at fixed FPS */

	struct Main *main;
	struct Scene *scene;
	struct bScreen *screen;
} TrackMarkersJob;

static bool track_markers_testbreak(void)
{
	return G.is_break;
}

static int track_count_markers(SpaceClip *sc, MovieClip *clip, int framenr)
{
	int tot = 0;
	ListBase *tracksbase = BKE_tracking_get_active_tracks(&clip->tracking);
	MovieTrackingTrack *track;

	track = tracksbase->first;
	while (track) {
		bool selected = sc ? TRACK_VIEW_SELECTED(sc, track) : TRACK_SELECTED(track);
		if (selected && (track->flag & TRACK_LOCKED) == 0) {
			MovieTrackingMarker *marker = BKE_tracking_marker_get(track, framenr);

			if (!marker || (marker->flag & MARKER_DISABLED) == 0)
				tot++;
		}

		track = track->next;
	}

	return tot;
}

static void clear_invisible_track_selection(SpaceClip *sc, MovieClip *clip)
{
	ListBase *tracksbase = BKE_tracking_get_active_tracks(&clip->tracking);
	int hidden = 0;

	if ((sc->flag & SC_SHOW_MARKER_PATTERN) == 0)
		hidden |= TRACK_AREA_PAT;

	if ((sc->flag & SC_SHOW_MARKER_SEARCH) == 0)
		hidden |= TRACK_AREA_SEARCH;

	if (hidden) {
		MovieTrackingTrack *track = tracksbase->first;

		while (track) {
			if ((track->flag & TRACK_HIDDEN) == 0)
				BKE_tracking_track_flag_clear(track, hidden, SELECT);

			track = track->next;
		}
	}
}

static void track_init_markers(SpaceClip *sc, MovieClip *clip, int framenr, int *frames_limit_r)
{
	ListBase *tracksbase = BKE_tracking_get_active_tracks(&clip->tracking);
	MovieTrackingTrack *track;
	int frames_limit = 0;

	if (sc != NULL) {
		clear_invisible_track_selection(sc, clip);
	}

	track = tracksbase->first;
	while (track) {
		bool selected = sc ? TRACK_VIEW_SELECTED(sc, track) : TRACK_SELECTED(track);
		if (selected) {
			if ((track->flag & TRACK_HIDDEN) == 0 && (track->flag & TRACK_LOCKED) == 0) {
				BKE_tracking_marker_ensure(track, framenr);

				if (track->frames_limit) {
					if (frames_limit == 0)
						frames_limit = track->frames_limit;
					else
						frames_limit = min_ii(frames_limit, (int)track->frames_limit);
				}
			}
		}

		track = track->next;
	}

	*frames_limit_r = frames_limit;
}

static bool track_markers_check_direction(int backwards, int curfra, int efra)
{
	if (backwards) {
		if (curfra < efra)
			return false;
	}
	else {
		if (curfra > efra)
			return false;
	}

	return true;
}

static int track_markers_initjob(bContext *C, TrackMarkersJob *tmj, int backwards)
{
	SpaceClip *sc = CTX_wm_space_clip(C);
	MovieClip *clip = ED_space_clip_get_clip(sc);
	Scene *scene = CTX_data_scene(C);
	MovieTrackingSettings *settings = &clip->tracking.settings;
	int frames_limit;
	int framenr = ED_space_clip_get_clip_frame_number(sc);

	track_init_markers(sc, clip, framenr, &frames_limit);

	tmj->sfra = ED_space_clip_get_clip_frame_number(sc);
	tmj->clip = clip;
	tmj->backwards = backwards;

	if (backwards)
		tmj->efra = SFRA;
	else
		tmj->efra = EFRA;

	/* limit frames to be tracked by user setting */
	if (frames_limit) {
		if (backwards)
			tmj->efra = MAX2(tmj->efra, tmj->sfra - frames_limit);
		else
			tmj->efra = MIN2(tmj->efra, tmj->sfra + frames_limit);
	}

	tmj->efra = BKE_movieclip_remap_scene_to_clip_frame(clip, tmj->efra);

	if (settings->speed != TRACKING_SPEED_FASTEST) {
		tmj->delay = 1.0f / scene->r.frs_sec * 1000.0f;

		if (settings->speed == TRACKING_SPEED_HALF)
			tmj->delay *= 2;
		else if (settings->speed == TRACKING_SPEED_QUARTER)
			tmj->delay *= 4;
		else if (settings->speed == TRACKING_SPEED_DOUBLE)
			tmj->delay /= 2;
	}

	tmj->context = BKE_tracking_context_new(clip, &sc->user, backwards, 1);

	clip->tracking_context = tmj->context;

	tmj->lastfra = tmj->sfra;

	/* XXX: silly to store this, but this data is needed to update scene and movie-clip
	 *      frame numbers when tracking is finished. This introduces better feedback for artists.
	 *      Maybe there's another way to solve this problem, but can't think better way atm.
	 *      Anyway, this way isn't more unstable as animation rendering animation
	 *      which uses the same approach (except storing screen). */
	tmj->scene = scene;
	tmj->main = CTX_data_main(C);
	tmj->screen = CTX_wm_screen(C);

	return track_markers_check_direction(backwards, tmj->sfra, tmj->efra);
}

static void track_markers_startjob(void *tmv, short *stop, short *do_update, float *progress)
{
	TrackMarkersJob *tmj = (TrackMarkersJob *)tmv;
	int framenr = tmj->sfra;
	// double t = PIL_check_seconds_timer();

	while (framenr != tmj->efra) {
		if (tmj->delay > 0) {
			/* tracking should happen with fixed fps. Calculate time
			 * using current timer value before tracking frame and after.
			 *
			 * Small (and maybe unneeded optimization): do not calculate exec_time
			 * for "Fastest" tracking */

			double start_time = PIL_check_seconds_timer(), exec_time;

			if (!BKE_tracking_context_step(tmj->context))
				break;

			exec_time = PIL_check_seconds_timer() - start_time;
			if (tmj->delay > (float)exec_time)
				PIL_sleep_ms(tmj->delay - (float)exec_time);
		}
		else if (!BKE_tracking_context_step(tmj->context))
			break;

		*do_update = TRUE;
		*progress = (float)(framenr - tmj->sfra) / (tmj->efra - tmj->sfra);

		if (tmj->backwards)
			framenr--;
		else
			framenr++;

		tmj->lastfra = framenr;

		if (*stop || track_markers_testbreak())
			break;
	}

	// printf("Tracking time: %lf\n", PIL_check_seconds_timer()-t);
}

static void track_markers_updatejob(void *tmv)
{
	TrackMarkersJob *tmj = (TrackMarkersJob *)tmv;

	BKE_tracking_context_sync(tmj->context);
}

static void track_markers_endjob(void *tmv)
{
	TrackMarkersJob *tmj = (TrackMarkersJob *)tmv;

	tmj->clip->tracking_context = NULL;
	tmj->scene->r.cfra = BKE_movieclip_remap_clip_to_scene_frame(tmj->clip, tmj->lastfra);
	ED_update_for_newframe(tmj->main, tmj->scene, 0);

	BKE_tracking_context_sync(tmj->context);
	BKE_tracking_context_finish(tmj->context);

	WM_main_add_notifier(NC_SCENE | ND_FRAME, tmj->scene);
}

static void track_markers_freejob(void *tmv)
{
	TrackMarkersJob *tmj = (TrackMarkersJob *)tmv;
	BKE_tracking_context_free(tmj->context);
	MEM_freeN(tmj);
}

static int track_markers_exec(bContext *C, wmOperator *op)
{
	SpaceClip *sc;
	MovieClip *clip;
	Scene *scene = CTX_data_scene(C);
	struct MovieTrackingContext *context;
	MovieClipUser *user, fake_user = {0};
	int framenr, sfra, efra;
	const bool backwards = RNA_boolean_get(op->ptr, "backwards");
	const bool sequence = RNA_boolean_get(op->ptr, "sequence");
	int frames_limit;

	if (RNA_struct_property_is_set(op->ptr, "clip")) {
		Main *bmain = CTX_data_main(C);
		char clip_name[MAX_ID_NAME - 2];

		RNA_string_get(op->ptr, "clip", clip_name);
		clip = (MovieClip *)BLI_findstring(&bmain->movieclip, clip_name, offsetof(ID, name) + 2);
		sc = NULL;

		if (clip == NULL) {
			return OPERATOR_CANCELLED;
		}
		framenr = BKE_movieclip_remap_scene_to_clip_frame(clip, CFRA);
		fake_user.framenr = framenr;
		user = &fake_user;
	}
	else {
		sc = CTX_wm_space_clip(C);

		if (sc == NULL) {
			return OPERATOR_CANCELLED;
		}

		clip = ED_space_clip_get_clip(sc);
		framenr = ED_space_clip_get_clip_frame_number(sc);
		user = &sc->user;
	}

	sfra = framenr;

	if (track_count_markers(sc, clip, framenr) == 0)
		return OPERATOR_CANCELLED;

	track_init_markers(sc, clip, framenr, &frames_limit);

	if (backwards)
		efra = SFRA;
	else
		efra = EFRA;

	/* limit frames to be tracked by user setting */
	if (frames_limit) {
		if (backwards)
			efra = MAX2(efra, sfra - frames_limit);
		else
			efra = MIN2(efra, sfra + frames_limit);
	}

	efra = BKE_movieclip_remap_scene_to_clip_frame(clip, efra);

	if (!track_markers_check_direction(backwards, framenr, efra))
		return OPERATOR_CANCELLED;

	/* do not disable tracks due to threshold when tracking frame-by-frame */
	context = BKE_tracking_context_new(clip, user, backwards, sequence);

	while (framenr != efra) {
		if (!BKE_tracking_context_step(context))
			break;

		if (backwards) framenr--;
		else framenr++;

		if (!sequence)
			break;
	}

	BKE_tracking_context_sync(context);
	BKE_tracking_context_finish(context);
	BKE_tracking_context_free(context);

	/* update scene current frame to the lastes tracked frame */
	scene->r.cfra = BKE_movieclip_remap_clip_to_scene_frame(clip, framenr);

	WM_event_add_notifier(C, NC_MOVIECLIP | NA_EVALUATED, clip);
	WM_event_add_notifier(C, NC_SCENE | ND_FRAME, scene);

	return OPERATOR_FINISHED;
}

static int track_markers_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
	TrackMarkersJob *tmj;
	ScrArea *sa = CTX_wm_area(C);
	SpaceClip *sc = CTX_wm_space_clip(C);
	MovieClip *clip;
	wmJob *wm_job;
	bool backwards = RNA_boolean_get(op->ptr, "backwards");
	bool sequence = RNA_boolean_get(op->ptr, "sequence");
	int framenr;

	if (sc == NULL) {
		/* TODO(sergey): Support clip for invoke as well. */
		BKE_report(op->reports, RPT_ERROR,
		           "Invoking this operator only supported from Clip Editor space");
		return OPERATOR_CANCELLED;
	}

	clip = ED_space_clip_get_clip(sc);
	framenr = ED_space_clip_get_clip_frame_number(sc);

	if (WM_jobs_test(CTX_wm_manager(C), CTX_wm_area(C), WM_JOB_TYPE_ANY)) {
		/* only one tracking is allowed at a time */
		return OPERATOR_CANCELLED;
	}

	if (clip->tracking_context)
		return OPERATOR_CANCELLED;

	if (track_count_markers(sc, clip, framenr) == 0)
		return OPERATOR_CANCELLED;

	if (!sequence)
		return track_markers_exec(C, op);

	tmj = MEM_callocN(sizeof(TrackMarkersJob), "TrackMarkersJob data");
	if (!track_markers_initjob(C, tmj, backwards)) {
		track_markers_freejob(tmj);

		return OPERATOR_CANCELLED;
	}

	/* setup job */
	wm_job = WM_jobs_get(CTX_wm_manager(C), CTX_wm_window(C), sa, "Track Markers",
	                     WM_JOB_PROGRESS, WM_JOB_TYPE_CLIP_TRACK_MARKERS);
	WM_jobs_customdata_set(wm_job, tmj, track_markers_freejob);

	/* if there's delay set in tracking job, tracking should happen
	 * with fixed FPS. To deal with editor refresh we have to synchronize
	 * tracks from job and tracks in clip. Do this in timer callback
	 * to prevent threading conflicts. */
	if (tmj->delay > 0)
		WM_jobs_timer(wm_job, tmj->delay / 1000.0f, NC_MOVIECLIP | NA_EVALUATED, 0);
	else
		WM_jobs_timer(wm_job, 0.2, NC_MOVIECLIP | NA_EVALUATED, 0);

	WM_jobs_callbacks(wm_job, track_markers_startjob, NULL, track_markers_updatejob, track_markers_endjob);

	G.is_break = FALSE;

	WM_jobs_start(CTX_wm_manager(C), wm_job);
	WM_cursor_wait(0);

	/* add modal handler for ESC */
	WM_event_add_modal_handler(C, op);

	return OPERATOR_RUNNING_MODAL;
}

static int track_markers_modal(bContext *C, wmOperator *UNUSED(op), const wmEvent *event)
{
	/* no running tracking, remove handler and pass through */
	if (0 == WM_jobs_test(CTX_wm_manager(C), CTX_wm_area(C), WM_JOB_TYPE_ANY))
		return OPERATOR_FINISHED | OPERATOR_PASS_THROUGH;

	/* running tracking */
	switch (event->type) {
		case ESCKEY:
			return OPERATOR_RUNNING_MODAL;
			break;
	}

	return OPERATOR_PASS_THROUGH;
}

void CLIP_OT_track_markers(wmOperatorType *ot)
{
	PropertyRNA *prop;

	/* identifiers */
	ot->name = "Track Markers";
	ot->description = "Track selected markers";
	ot->idname = "CLIP_OT_track_markers";

	/* api callbacks */
	ot->exec = track_markers_exec;
	ot->invoke = track_markers_invoke;
	ot->modal = track_markers_modal;

	/* flags */
	ot->flag = OPTYPE_UNDO;

	/* properties */
	RNA_def_boolean(ot->srna, "backwards", 0, "Backwards", "Do backwards tracking");
	RNA_def_boolean(ot->srna, "sequence", 0, "Track Sequence", "Track marker during image sequence rather than single image");
	prop = RNA_def_string(ot->srna, "clip", NULL, MAX_NAME, "Movie Clip", "Movie Clip to be tracked");
	RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

/********************** refine track position operator *********************/

static int refine_marker_exec(bContext *C, wmOperator *op)
{
	SpaceClip *sc = CTX_wm_space_clip(C);
	MovieClip *clip = ED_space_clip_get_clip(sc);
	MovieTracking *tracking = &clip->tracking;
	ListBase *tracksbase = BKE_tracking_get_active_tracks(tracking);
	MovieTrackingTrack *track;
	bool backwards = RNA_boolean_get(op->ptr, "backwards");
	int framenr = ED_space_clip_get_clip_frame_number(sc);

	for (track = tracksbase->first; track; track = track->next) {
		if (TRACK_VIEW_SELECTED(sc, track)) {
			MovieTrackingMarker *marker = BKE_tracking_marker_get(track, framenr);

			BKE_tracking_refine_marker(clip, track, marker, backwards);
		}
	}

	WM_event_add_notifier(C, NC_MOVIECLIP | NA_EVALUATED, clip);

	return OPERATOR_FINISHED;
}

void CLIP_OT_refine_markers(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Refine Markers";
	ot->description = "Refine selected markers positions "
	                  "by running the tracker from track's reference to current frame";
	ot->idname = "CLIP_OT_refine_markers";

	/* api callbacks */
	ot->exec = refine_marker_exec;
	ot->poll = ED_space_clip_tracking_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* properties */
	RNA_def_boolean(ot->srna, "backwards", 0, "Backwards", "Do backwards tracking");
}

/********************** solve camera operator *********************/

typedef struct {
	Scene *scene;
	MovieClip *clip;
	MovieClipUser user;

	ReportList *reports;

	char stats_message[256];

	struct MovieReconstructContext *context;
} SolveCameraJob;

static int solve_camera_initjob(bContext *C, SolveCameraJob *scj, wmOperator *op, char *error_msg, int max_error)
{
	SpaceClip *sc = CTX_wm_space_clip(C);
	MovieClip *clip = ED_space_clip_get_clip(sc);
	Scene *scene = CTX_data_scene(C);
	MovieTracking *tracking = &clip->tracking;
	MovieTrackingObject *object = BKE_tracking_object_get_active(tracking);
	int width, height;

	if (!BKE_tracking_reconstruction_check(tracking, object, error_msg, max_error))
		return 0;

	/* could fail if footage uses images with different sizes */
	BKE_movieclip_get_size(clip, &sc->user, &width, &height);

	scj->clip = clip;
	scj->scene = scene;
	scj->reports = op->reports;
	scj->user = sc->user;

	scj->context = BKE_tracking_reconstruction_context_new(clip, object,
	                                                       object->keyframe1, object->keyframe2, width, height);

	tracking->stats = MEM_callocN(sizeof(MovieTrackingStats), "solve camera stats");

	return 1;
}

static void solve_camera_updatejob(void *scv)
{
	SolveCameraJob *scj = (SolveCameraJob *)scv;
	MovieTracking *tracking = &scj->clip->tracking;

	BLI_strncpy(tracking->stats->message, scj->stats_message, sizeof(tracking->stats->message));
}

static void solve_camera_startjob(void *scv, short *stop, short *do_update, float *progress)
{
	SolveCameraJob *scj = (SolveCameraJob *)scv;

	BKE_tracking_reconstruction_solve(scj->context, stop, do_update, progress,
	                                  scj->stats_message, sizeof(scj->stats_message));
}

static void solve_camera_freejob(void *scv)
{
	SolveCameraJob *scj = (SolveCameraJob *)scv;
	MovieTracking *tracking = &scj->clip->tracking;
	Scene *scene = scj->scene;
	MovieClip *clip = scj->clip;
	int solved;

	if (!scj->context) {
		/* job weren't fully initialized due to some error */
		MEM_freeN(scj);
		return;
	}

	solved = BKE_tracking_reconstruction_finish(scj->context, tracking);
	if (!solved)
		BKE_report(scj->reports, RPT_WARNING, "Some data failed to reconstruct (see console for details)");
	else
		BKE_reportf(scj->reports, RPT_INFO, "Average re-projection error: %.3f", tracking->reconstruction.error);

	/* set currently solved clip as active for scene */
	if (scene->clip)
		id_us_min(&clip->id);

	scene->clip = clip;
	id_us_plus(&clip->id);

	/* set blender camera focal length so result would look fine there */
	if (scene->camera && scene->camera->data && GS(((ID *) scene->camera->data)->name) == ID_CA) {
		Camera *camera = (Camera *)scene->camera->data;
		int width, height;

		BKE_movieclip_get_size(clip, &scj->user, &width, &height);

		BKE_tracking_camera_to_blender(tracking, scene, camera, width, height);

		WM_main_add_notifier(NC_OBJECT, camera);
	}

	MEM_freeN(tracking->stats);
	tracking->stats = NULL;

	DAG_id_tag_update(&clip->id, 0);

	WM_main_add_notifier(NC_MOVIECLIP | NA_EVALUATED, clip);
	WM_main_add_notifier(NC_OBJECT | ND_TRANSFORM, NULL);

	/* update active clip displayed in scene buttons */
	WM_main_add_notifier(NC_SCENE, scene);

	BKE_tracking_reconstruction_context_free(scj->context);
	MEM_freeN(scj);
}

static int solve_camera_exec(bContext *C, wmOperator *op)
{
	SolveCameraJob *scj;
	char error_msg[256] = "\0";

	scj = MEM_callocN(sizeof(SolveCameraJob), "SolveCameraJob data");
	if (!solve_camera_initjob(C, scj, op, error_msg, sizeof(error_msg))) {
		if (error_msg[0])
			BKE_report(op->reports, RPT_ERROR, error_msg);

		solve_camera_freejob(scj);

		return OPERATOR_CANCELLED;
	}

	solve_camera_startjob(scj, NULL, NULL, NULL);

	solve_camera_freejob(scj);

	return OPERATOR_FINISHED;
}

static int solve_camera_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
	SolveCameraJob *scj;
	ScrArea *sa = CTX_wm_area(C);
	SpaceClip *sc = CTX_wm_space_clip(C);
	MovieClip *clip = ED_space_clip_get_clip(sc);
	MovieTracking *tracking = &clip->tracking;
	MovieTrackingReconstruction *reconstruction = BKE_tracking_get_active_reconstruction(tracking);
	wmJob *wm_job;
	char error_msg[256] = "\0";

	if (WM_jobs_test(CTX_wm_manager(C), CTX_wm_area(C), WM_JOB_TYPE_ANY)) {
		/* only one solve is allowed at a time */
		return OPERATOR_CANCELLED;
	}

	scj = MEM_callocN(sizeof(SolveCameraJob), "SolveCameraJob data");
	if (!solve_camera_initjob(C, scj, op, error_msg, sizeof(error_msg))) {
		if (error_msg[0])
			BKE_report(op->reports, RPT_ERROR, error_msg);

		solve_camera_freejob(scj);

		return OPERATOR_CANCELLED;
	}

	BLI_strncpy(tracking->stats->message, "Solving camera | Preparing solve", sizeof(tracking->stats->message));

	/* hide reconstruction statistics from previous solve */
	reconstruction->flag &= ~TRACKING_RECONSTRUCTED;
	WM_event_add_notifier(C, NC_MOVIECLIP | NA_EVALUATED, clip);

	/* setup job */
	wm_job = WM_jobs_get(CTX_wm_manager(C), CTX_wm_window(C), sa, "Solve Camera",
	                     WM_JOB_PROGRESS, WM_JOB_TYPE_CLIP_SOLVE_CAMERA);
	WM_jobs_customdata_set(wm_job, scj, solve_camera_freejob);
	WM_jobs_timer(wm_job, 0.1, NC_MOVIECLIP | NA_EVALUATED, 0);
	WM_jobs_callbacks(wm_job, solve_camera_startjob, NULL, solve_camera_updatejob, NULL);

	G.is_break = FALSE;

	WM_jobs_start(CTX_wm_manager(C), wm_job);
	WM_cursor_wait(0);

	/* add modal handler for ESC */
	WM_event_add_modal_handler(C, op);

	return OPERATOR_RUNNING_MODAL;
}

static int solve_camera_modal(bContext *C, wmOperator *UNUSED(op), const wmEvent *event)
{
	/* no running solver, remove handler and pass through */
	if (0 == WM_jobs_test(CTX_wm_manager(C), CTX_wm_area(C), WM_JOB_TYPE_ANY))
		return OPERATOR_FINISHED | OPERATOR_PASS_THROUGH;

	/* running tracking */
	switch (event->type) {
		case ESCKEY:
			return OPERATOR_RUNNING_MODAL;
			break;
	}

	return OPERATOR_PASS_THROUGH;
}

void CLIP_OT_solve_camera(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Solve Camera";
	ot->description = "Solve camera motion from tracks";
	ot->idname = "CLIP_OT_solve_camera";

	/* api callbacks */
	ot->exec = solve_camera_exec;
	ot->invoke = solve_camera_invoke;
	ot->modal = solve_camera_modal;
	ot->poll = ED_space_clip_tracking_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/********************** clear solution operator *********************/

static int clear_solution_exec(bContext *C, wmOperator *UNUSED(op))
{
	SpaceClip *sc = CTX_wm_space_clip(C);
	MovieClip *clip = ED_space_clip_get_clip(sc);
	MovieTracking *tracking = &clip->tracking;
	ListBase *tracksbase = BKE_tracking_get_active_tracks(&clip->tracking);
	MovieTrackingReconstruction *reconstruction = BKE_tracking_get_active_reconstruction(tracking);
	MovieTrackingTrack *track = tracksbase->first;

	while (track) {
		track->flag &= ~TRACK_HAS_BUNDLE;

		track = track->next;
	}

	if (reconstruction->cameras)
		MEM_freeN(reconstruction->cameras);

	reconstruction->cameras = NULL;
	reconstruction->camnr = 0;

	reconstruction->flag &= ~TRACKING_RECONSTRUCTED;

	DAG_id_tag_update(&clip->id, 0);

	WM_event_add_notifier(C, NC_MOVIECLIP | NA_EVALUATED, clip);
	WM_event_add_notifier(C, NC_SPACE | ND_SPACE_VIEW3D, NULL);

	return OPERATOR_FINISHED;
}

void CLIP_OT_clear_solution(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Clear Solution";
	ot->description = "Clear all calculated data";
	ot->idname = "CLIP_OT_clear_solution";

	/* api callbacks */
	ot->exec = clear_solution_exec;
	ot->poll = ED_space_clip_tracking_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/********************** clear track operator *********************/

static int clear_track_path_exec(bContext *C, wmOperator *op)
{
	SpaceClip *sc = CTX_wm_space_clip(C);
	MovieClip *clip = ED_space_clip_get_clip(sc);
	MovieTracking *tracking = &clip->tracking;
	MovieTrackingTrack *track;
	ListBase *tracksbase = BKE_tracking_get_active_tracks(tracking);
	int action = RNA_enum_get(op->ptr, "action");
	const bool clear_active = RNA_boolean_get(op->ptr, "clear_active");
	int framenr = ED_space_clip_get_clip_frame_number(sc);

	if (clear_active) {
		track = BKE_tracking_track_get_active(tracking);
		if (track) {
			BKE_tracking_track_path_clear(track, framenr, action);
		}
	}
	else {
		track = tracksbase->first;
		while (track) {
			if (TRACK_VIEW_SELECTED(sc, track))
				BKE_tracking_track_path_clear(track, framenr, action);

			track = track->next;
		}
	}

	BKE_tracking_dopesheet_tag_update(tracking);
	WM_event_add_notifier(C, NC_MOVIECLIP | NA_EVALUATED, clip);

	return OPERATOR_FINISHED;
}

void CLIP_OT_clear_track_path(wmOperatorType *ot)
{
	static EnumPropertyItem clear_path_actions[] = {
		{TRACK_CLEAR_UPTO, "UPTO", 0, "Clear up-to", "Clear path up to current frame"},
		{TRACK_CLEAR_REMAINED, "REMAINED", 0, "Clear remained", "Clear path at remaining frames (after current)"},
		{TRACK_CLEAR_ALL, "ALL", 0, "Clear all", "Clear the whole path"},
		{0, NULL, 0, NULL, NULL}
	};

	/* identifiers */
	ot->name = "Clear Track Path";
	ot->description = "Clear tracks after/before current position or clear the whole track";
	ot->idname = "CLIP_OT_clear_track_path";

	/* api callbacks */
	ot->exec = clear_track_path_exec;
	ot->poll = ED_space_clip_tracking_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* proeprties */
	RNA_def_enum(ot->srna, "action", clear_path_actions, TRACK_CLEAR_REMAINED, "Action", "Clear action to execute");
	RNA_def_boolean(ot->srna, "clear_active", 0, "Clear Active", "Clear active track only instead of all selected tracks");
}

/********************** disable markers operator *********************/

static int disable_markers_exec(bContext *C, wmOperator *op)
{
	SpaceClip *sc = CTX_wm_space_clip(C);
	MovieClip *clip = ED_space_clip_get_clip(sc);
	MovieTracking *tracking = &clip->tracking;
	ListBase *tracksbase = BKE_tracking_get_active_tracks(tracking);
	MovieTrackingTrack *track = tracksbase->first;
	int action = RNA_enum_get(op->ptr, "action");
	int framenr = ED_space_clip_get_clip_frame_number(sc);

	while (track) {
		if (TRACK_VIEW_SELECTED(sc, track) && (track->flag & TRACK_LOCKED) == 0) {
			MovieTrackingMarker *marker = BKE_tracking_marker_ensure(track, framenr);

			if (action == 0)
				marker->flag |= MARKER_DISABLED;
			else if (action == 1)
				marker->flag &= ~MARKER_DISABLED;
			else marker->flag ^= MARKER_DISABLED;
		}

		track = track->next;
	}

	DAG_id_tag_update(&clip->id, 0);

	WM_event_add_notifier(C, NC_MOVIECLIP | NA_EVALUATED, clip);

	return OPERATOR_FINISHED;
}

void CLIP_OT_disable_markers(wmOperatorType *ot)
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
	ot->idname = "CLIP_OT_disable_markers";

	/* api callbacks */
	ot->exec = disable_markers_exec;
	ot->poll = ED_space_clip_tracking_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* properties */
	RNA_def_enum(ot->srna, "action", actions_items, 0, "Action", "Disable action to execute");
}

/********************** set origin operator *********************/

static Object *get_camera_with_movieclip(Scene *scene, MovieClip *clip)
{
	Object *camera = scene->camera;
	Base *base;

	if (camera && BKE_object_movieclip_get(scene, camera, false) == clip)
		return camera;

	base = scene->base.first;
	while (base) {
		if (base->object->type == OB_CAMERA) {
			if (BKE_object_movieclip_get(scene, base->object, false) == clip) {
				camera = base->object;
				break;
			}
		}

		base = base->next;
	}

	return camera;
}

static Object *get_orientation_object(bContext *C)
{
	Scene *scene = CTX_data_scene(C);
	SpaceClip *sc = CTX_wm_space_clip(C);
	MovieClip *clip = ED_space_clip_get_clip(sc);
	MovieTracking *tracking = &clip->tracking;
	MovieTrackingObject *tracking_object = BKE_tracking_object_get_active(tracking);
	Object *object = NULL;

	if (tracking_object->flag & TRACKING_OBJECT_CAMERA) {
		object = get_camera_with_movieclip(scene, clip);
	}
	else {
		object = OBACT;
	}

	if (object && object->parent)
		object = object->parent;

	return object;
}

static int set_orientation_poll(bContext *C)
{
	SpaceClip *sc = CTX_wm_space_clip(C);

	if (sc) {
		Scene *scene = CTX_data_scene(C);
		MovieClip *clip = ED_space_clip_get_clip(sc);

		if (clip) {
			MovieTracking *tracking = &clip->tracking;
			MovieTrackingObject *tracking_object = BKE_tracking_object_get_active(tracking);

			if (tracking_object->flag & TRACKING_OBJECT_CAMERA) {
				return TRUE;
			}
			else {
				return OBACT != NULL;
			}
		}
	}

	return FALSE;
}

static int count_selected_bundles(bContext *C)
{
	SpaceClip *sc = CTX_wm_space_clip(C);
	MovieClip *clip = ED_space_clip_get_clip(sc);
	ListBase *tracksbase = BKE_tracking_get_active_tracks(&clip->tracking);
	MovieTrackingTrack *track;
	int tot = 0;

	track = tracksbase->first;
	while (track) {
		if (TRACK_VIEW_SELECTED(sc, track) && (track->flag & TRACK_HAS_BUNDLE))
			tot++;

		track = track->next;
	}

	return tot;
}

static void object_solver_inverted_matrix(Scene *scene, Object *ob, float invmat[4][4])
{
	bConstraint *con;
	bool found = false;

	for (con = ob->constraints.first; con; con = con->next) {
		bConstraintTypeInfo *cti = BKE_constraint_get_typeinfo(con);

		if (!cti)
			continue;

		if (cti->type == CONSTRAINT_TYPE_OBJECTSOLVER) {
			bObjectSolverConstraint *data = (bObjectSolverConstraint *)con->data;

			if (!found) {
				Object *cam = data->camera ? data->camera : scene->camera;

				BKE_object_where_is_calc_mat4(scene, cam, invmat);
			}

			mul_m4_m4m4(invmat, invmat, data->invmat);

			found = true;
		}
	}

	if (found)
		invert_m4(invmat);
	else
		unit_m4(invmat);
}

static Object *object_solver_camera(Scene *scene, Object *ob)
{
	bConstraint *con;

	for (con = ob->constraints.first; con; con = con->next) {
		bConstraintTypeInfo *cti = BKE_constraint_get_typeinfo(con);

		if (!cti)
			continue;

		if (cti->type == CONSTRAINT_TYPE_OBJECTSOLVER) {
			bObjectSolverConstraint *data = (bObjectSolverConstraint *)con->data;

			return data->camera ? data->camera : scene->camera;
		}
	}

	return NULL;
}

static int set_origin_exec(bContext *C, wmOperator *op)
{
	SpaceClip *sc = CTX_wm_space_clip(C);
	MovieClip *clip = ED_space_clip_get_clip(sc);
	MovieTracking *tracking = &clip->tracking;
	MovieTrackingTrack *track;
	MovieTrackingObject *tracking_object;
	Scene *scene = CTX_data_scene(C);
	Object *object;
	Object *camera = get_camera_with_movieclip(scene, clip);
	ListBase *tracksbase;
	float mat[4][4], vec[3], median[3];
	int selected_count = count_selected_bundles(C);

	if (selected_count == 0) {
		BKE_report(op->reports, RPT_ERROR,
		           "At least one track with bundle should be selected to define origin position");

		return OPERATOR_CANCELLED;
	}

	object = get_orientation_object(C);
	if (!object) {
		BKE_report(op->reports, RPT_ERROR, "No object to apply orientation on");

		return OPERATOR_CANCELLED;
	}

	tracking_object = BKE_tracking_object_get_active(tracking);

	tracksbase = BKE_tracking_object_get_tracks(tracking, tracking_object);

	track = tracksbase->first;
	zero_v3(median);
	while (track) {
		if (TRACK_VIEW_SELECTED(sc, track) && (track->flag & TRACK_HAS_BUNDLE)) {
			add_v3_v3(median, track->bundle_pos);
		}

		track = track->next;
	}
	mul_v3_fl(median, 1.0f / selected_count);

	BKE_tracking_get_camera_object_matrix(scene, camera, mat);

	mul_v3_m4v3(vec, mat, median);

	if (tracking_object->flag & TRACKING_OBJECT_CAMERA) {
		sub_v3_v3(object->loc, vec);
	}
	else {
		object_solver_inverted_matrix(scene, object, mat);
		mul_v3_m4v3(vec, mat, vec);
		copy_v3_v3(object->loc, vec);
	}

	DAG_id_tag_update(&clip->id, 0);
	DAG_id_tag_update(&object->id, OB_RECALC_OB);

	WM_event_add_notifier(C, NC_MOVIECLIP | NA_EVALUATED, clip);
	WM_event_add_notifier(C, NC_OBJECT | ND_TRANSFORM, NULL);

	return OPERATOR_FINISHED;
}

void CLIP_OT_set_origin(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Set Origin";
	ot->description = "Set active marker as origin by moving camera (or its parent if present) in 3D space";
	ot->idname = "CLIP_OT_set_origin";

	/* api callbacks */
	ot->exec = set_origin_exec;
	ot->poll = set_orientation_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* properties */
	RNA_def_boolean(ot->srna, "use_median", 0, "Use Median", "Set origin to median point of selected bundles");
}

/********************** set floor operator *********************/

static void set_axis(Scene *scene,  Object *ob, MovieClip *clip, MovieTrackingObject *tracking_object,
                     MovieTrackingTrack *track, char axis)
{
	Object *camera = get_camera_with_movieclip(scene, clip);
	const bool is_camera = (tracking_object->flag & TRACKING_OBJECT_CAMERA) != 0;
	bool flip = false;
	float mat[4][4], vec[3], obmat[4][4], dvec[3];

	BKE_object_to_mat4(ob, obmat);

	BKE_tracking_get_camera_object_matrix(scene, camera, mat);
	mul_v3_m4v3(vec, mat, track->bundle_pos);
	copy_v3_v3(dvec, vec);

	if (!is_camera) {
		float imat[4][4];

		object_solver_inverted_matrix(scene, ob, imat);
		mul_v3_m4v3(vec, imat, vec);

		invert_m4_m4(imat, obmat);
		mul_v3_m4v3(dvec, imat, vec);

		sub_v3_v3(vec, obmat[3]);
	}

	if (len_squared_v2(vec) < (1e-3f * 1e-3f))
		return;

	unit_m4(mat);

	if (axis == 'X') {
		if (fabsf(dvec[1]) < 1e-3f) {
			flip = true;

			mat[0][0] = -1.0f; mat[0][1] = 0.0f; mat[0][2] = 0.0f;
			mat[1][0] = 0.0f; mat[1][1] = -1.0f; mat[1][2] = 0.0f;
			mat[2][0] = 0.0f; mat[2][1] = 0.0f; mat[2][2] = 1.0f;
		}
		else {
			copy_v3_v3(mat[0], vec);

			if (is_camera || fabsf(vec[2]) < 1e-3f) {
				mat[0][2] = 0.0f;
				mat[2][0] = 0.0f; mat[2][1] = 0.0f; mat[2][2] = 1.0f;
				cross_v3_v3v3(mat[1], mat[2], mat[0]);
			}
			else {
				vec[2] = 0.0f;

				cross_v3_v3v3(mat[1], mat[0], vec);
				cross_v3_v3v3(mat[2], mat[0], mat[1]);
			}
		}
	}
	else {
		if (fabsf(dvec[0]) < 1e-3f) {
			flip = true;

			mat[0][0] = -1.0f; mat[0][1] = 0.0f; mat[0][2] = 0.0f;
			mat[1][0] = 0.0f; mat[1][1] = -1.0f; mat[1][2] = 0.0f;
			mat[2][0] = 0.0f; mat[2][1] = 0.0f; mat[2][2] = 1.0f;
		}
		else {
			copy_v3_v3(mat[1], vec);

			if (is_camera || fabsf(vec[2]) < 1e-3f) {
				mat[1][2] = 0.0f;
				mat[2][0] = 0.0f; mat[2][1] = 0.0f; mat[2][2] = 1.0f;
				cross_v3_v3v3(mat[0], mat[1], mat[2]);
			}
			else {
				vec[2] = 0.0f;

				cross_v3_v3v3(mat[0], vec, mat[1]);
				cross_v3_v3v3(mat[2], mat[0], mat[1]);
			}
		}
	}

	normalize_v3(mat[0]);
	normalize_v3(mat[1]);
	normalize_v3(mat[2]);

	if (is_camera) {
		invert_m4(mat);

		mul_m4_m4m4(mat, mat, obmat);
	}
	else {
		if (!flip) {
			float lmat[4][4], ilmat[4][4], rmat[3][3];

			BKE_object_rot_to_mat3(ob, rmat, TRUE);
			invert_m3(rmat);
			mul_m4_m4m3(mat, mat, rmat);

			unit_m4(lmat);
			copy_v3_v3(lmat[3], obmat[3]);
			invert_m4_m4(ilmat, lmat);

			mul_serie_m4(mat, lmat, mat, ilmat, obmat, NULL, NULL, NULL, NULL);
		}
		else {
			mul_m4_m4m4(mat, obmat, mat);
		}
	}

	BKE_object_apply_mat4(ob, mat, 0, 0);
}

static int set_plane_exec(bContext *C, wmOperator *op)
{
	SpaceClip *sc = CTX_wm_space_clip(C);
	MovieClip *clip = ED_space_clip_get_clip(sc);
	Scene *scene = CTX_data_scene(C);
	MovieTracking *tracking = &clip->tracking;
	MovieTrackingObject *tracking_object;
	MovieTrackingTrack *track, *axis_track = NULL, *act_track;
	ListBase *tracksbase;
	Object *object;
	Object *camera = get_camera_with_movieclip(scene, clip);
	int tot = 0;
	float vec[3][3], mat[4][4], obmat[4][4], newmat[4][4], orig[3] = {0.0f, 0.0f, 0.0f};
	int plane = RNA_enum_get(op->ptr, "plane");
	float rot[4][4] = {{0.0f, 0.0f, -1.0f, 0.0f},
	                   {0.0f, 1.0f, 0.0f, 0.0f},
	                   {1.0f, 0.0f, 0.0f, 0.0f},
	                   {0.0f, 0.0f, 0.0f, 1.0f}};  /* 90 degrees Y-axis rotation matrix */

	if (count_selected_bundles(C) != 3) {
		BKE_report(op->reports, RPT_ERROR, "Three tracks with bundles are needed to orient the floor");

		return OPERATOR_CANCELLED;
	}

	tracking_object = BKE_tracking_object_get_active(tracking);
	tracksbase = BKE_tracking_object_get_tracks(tracking, tracking_object);
	act_track = BKE_tracking_track_get_active(tracking);

	object = get_orientation_object(C);
	if (!object) {
		BKE_report(op->reports, RPT_ERROR, "No object to apply orientation on");

		return OPERATOR_CANCELLED;
	}

	BKE_tracking_get_camera_object_matrix(scene, camera, mat);

	/* get 3 bundles to use as reference */
	track = tracksbase->first;
	while (track && tot < 3) {
		if (track->flag & TRACK_HAS_BUNDLE && TRACK_VIEW_SELECTED(sc, track)) {
			mul_v3_m4v3(vec[tot], mat, track->bundle_pos);

			if (tot == 0 || track == act_track)
				copy_v3_v3(orig, vec[tot]);
			else
				axis_track = track;

			tot++;
		}

		track = track->next;
	}

	sub_v3_v3(vec[1], vec[0]);
	sub_v3_v3(vec[2], vec[0]);

	/* construct ortho-normal basis */
	unit_m4(mat);

	if (plane == 0) { /* floor */
		cross_v3_v3v3(mat[0], vec[1], vec[2]);
		copy_v3_v3(mat[1], vec[1]);
		cross_v3_v3v3(mat[2], mat[0], mat[1]);
	}
	else if (plane == 1) { /* wall */
		cross_v3_v3v3(mat[2], vec[1], vec[2]);
		copy_v3_v3(mat[1], vec[1]);
		cross_v3_v3v3(mat[0], mat[1], mat[2]);
	}

	normalize_v3(mat[0]);
	normalize_v3(mat[1]);
	normalize_v3(mat[2]);

	/* move to origin point */
	mat[3][0] = orig[0];
	mat[3][1] = orig[1];
	mat[3][2] = orig[2];

	if (tracking_object->flag & TRACKING_OBJECT_CAMERA) {
		invert_m4(mat);

		BKE_object_to_mat4(object, obmat);
		mul_m4_m4m4(mat, mat, obmat);
		mul_m4_m4m4(newmat, rot, mat);
		BKE_object_apply_mat4(object, newmat, 0, 0);

		/* make camera have positive z-coordinate */
		if (object->loc[2] < 0) {
			invert_m4(rot);
			mul_m4_m4m4(newmat, rot, mat);
			BKE_object_apply_mat4(object, newmat, 0, 0);
		}
	}
	else {
		BKE_object_apply_mat4(object, mat, 0, 0);
	}

	BKE_object_where_is_calc(scene, object);
	set_axis(scene, object, clip, tracking_object, axis_track, 'X');

	DAG_id_tag_update(&clip->id, 0);
	DAG_id_tag_update(&object->id, OB_RECALC_OB);

	WM_event_add_notifier(C, NC_MOVIECLIP | NA_EVALUATED, clip);
	WM_event_add_notifier(C, NC_OBJECT | ND_TRANSFORM, NULL);

	return OPERATOR_FINISHED;
}

void CLIP_OT_set_plane(wmOperatorType *ot)
{
	static EnumPropertyItem plane_items[] = {
		{0, "FLOOR", 0, "Floor", "Set floor plane"},
		{1, "WALL", 0, "Wall", "Set wall plane"},
		{0, NULL, 0, NULL, NULL}
	};

	/* identifiers */
	ot->name = "Set Plane";
	ot->description = "Set plane based on 3 selected bundles by moving camera (or its parent if present) in 3D space";
	ot->idname = "CLIP_OT_set_plane";

	/* api callbacks */
	ot->exec = set_plane_exec;
	ot->poll = set_orientation_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* properties */
	RNA_def_enum(ot->srna, "plane", plane_items, 0, "Plane", "Plane to be used for orientation");
}

/********************** set axis operator *********************/

static int set_axis_exec(bContext *C, wmOperator *op)
{
	SpaceClip *sc = CTX_wm_space_clip(C);
	MovieClip *clip = ED_space_clip_get_clip(sc);
	MovieTracking *tracking = &clip->tracking;
	MovieTrackingObject *tracking_object = BKE_tracking_object_get_active(tracking);
	MovieTrackingTrack *track;
	Scene *scene = CTX_data_scene(C);
	Object *object;
	ListBase *tracksbase;
	int axis = RNA_enum_get(op->ptr, "axis");

	if (count_selected_bundles(C) != 1) {
		BKE_report(op->reports, RPT_ERROR, "Single track with bundle should be selected to define axis");

		return OPERATOR_CANCELLED;
	}

	object = get_orientation_object(C);
	if (!object) {
		BKE_report(op->reports, RPT_ERROR, "No object to apply orientation on");

		return OPERATOR_CANCELLED;
	}

	tracksbase = BKE_tracking_object_get_tracks(tracking, tracking_object);

	track = tracksbase->first;
	while (track) {
		if (TRACK_VIEW_SELECTED(sc, track) && (track->flag & TRACK_HAS_BUNDLE))
			break;

		track = track->next;
	}

	set_axis(scene, object, clip, tracking_object, track, axis == 0 ? 'X' : 'Y');

	DAG_id_tag_update(&clip->id, 0);
	DAG_id_tag_update(&object->id, OB_RECALC_OB);

	WM_event_add_notifier(C, NC_MOVIECLIP | NA_EVALUATED, clip);
	WM_event_add_notifier(C, NC_OBJECT | ND_TRANSFORM, NULL);

	return OPERATOR_FINISHED;
}

void CLIP_OT_set_axis(wmOperatorType *ot)
{
	static EnumPropertyItem axis_actions[] = {
		{0, "X", 0, "X", "Align bundle align X axis"},
		{1, "Y", 0, "Y", "Align bundle align Y axis"},
		{0, NULL, 0, NULL, NULL}
	};

	/* identifiers */
	ot->name = "Set Axis";
	ot->description = "Set direction of scene axis rotating camera (or its parent if present) and assume selected track lies on real axis, joining it with the origin";
	ot->idname = "CLIP_OT_set_axis";

	/* api callbacks */
	ot->exec = set_axis_exec;
	ot->poll = set_orientation_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* properties */
	RNA_def_enum(ot->srna, "axis", axis_actions, 0, "Axis", "Axis to use to align bundle along");
}

/********************** set scale operator *********************/

static int do_set_scale(bContext *C, wmOperator *op, bool scale_solution, bool apply_scale)
{
	SpaceClip *sc = CTX_wm_space_clip(C);
	MovieClip *clip = ED_space_clip_get_clip(sc);
	MovieTracking *tracking = &clip->tracking;
	MovieTrackingObject *tracking_object = BKE_tracking_object_get_active(tracking);
	MovieTrackingTrack *track;
	Scene *scene = CTX_data_scene(C);
	Object *object = NULL;
	Object *camera = get_camera_with_movieclip(scene, clip);
	ListBase *tracksbase = BKE_tracking_get_active_tracks(tracking);
	int tot = 0;
	float vec[2][3], mat[4][4], scale;
	float dist = RNA_float_get(op->ptr, "distance");

	if (count_selected_bundles(C) != 2) {
		BKE_report(op->reports, RPT_ERROR, "Two tracks with bundles should be selected to set scale");

		return OPERATOR_CANCELLED;
	}

	if (!scale_solution && !apply_scale) {
		object = get_orientation_object(C);
		if (!object) {
			BKE_report(op->reports, RPT_ERROR, "No object to apply orientation on");

			return OPERATOR_CANCELLED;
		}
	}

	BKE_tracking_get_camera_object_matrix(scene, camera, mat);

	track = tracksbase->first;
	while (track) {
		if (TRACK_VIEW_SELECTED(sc, track)) {
			mul_v3_m4v3(vec[tot], mat, track->bundle_pos);
			tot++;
		}

		track = track->next;
	}

	sub_v3_v3(vec[0], vec[1]);

	if (len_v3(vec[0]) > 1e-5f) {
		scale = dist / len_v3(vec[0]);

		if (apply_scale) {
			/* Apply scale on reconstructed scene itself */
			MovieTrackingReconstruction *reconstruction = BKE_tracking_get_active_reconstruction(tracking);
			MovieReconstructedCamera *reconstructed_cameras;
			int i;

			for (track = tracksbase->first; track; track = track->next) {
				mul_v3_fl(track->bundle_pos, scale);
			}

			reconstructed_cameras = reconstruction->cameras;
			for (i = 0; i < reconstruction->camnr; i++) {
				mul_v3_fl(reconstructed_cameras[i].mat[3], scale);
			}

			WM_event_add_notifier(C, NC_MOVIECLIP | NA_EVALUATED, clip);
			WM_event_add_notifier(C, NC_OBJECT | ND_TRANSFORM, NULL);
		}
		else {
			if (tracking_object->flag & TRACKING_OBJECT_CAMERA) {
				mul_v3_fl(object->size, scale);
				mul_v3_fl(object->loc, scale);
			}
			else if (!scale_solution) {
				Object *solver_camera = object_solver_camera(scene, object);

				object->size[0] = object->size[1] = object->size[2] = 1.0f / scale;

				if (solver_camera) {
					object->size[0] /= solver_camera->size[0];
					object->size[1] /= solver_camera->size[1];
					object->size[2] /= solver_camera->size[2];
				}
			}
			else {
				tracking_object->scale = scale;
			}

			DAG_id_tag_update(&clip->id, 0);

			if (object)
				DAG_id_tag_update(&object->id, OB_RECALC_OB);

			WM_event_add_notifier(C, NC_MOVIECLIP | NA_EVALUATED, clip);
			WM_event_add_notifier(C, NC_OBJECT | ND_TRANSFORM, NULL);
		}
	}

	return OPERATOR_FINISHED;
}

static int set_scale_exec(bContext *C, wmOperator *op)
{
	return do_set_scale(C, op, false, false);
}

static int set_scale_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
	SpaceClip *sc = CTX_wm_space_clip(C);
	MovieClip *clip = ED_space_clip_get_clip(sc);

	if (!RNA_struct_property_is_set(op->ptr, "distance"))
		RNA_float_set(op->ptr, "distance", clip->tracking.settings.dist);

	return set_scale_exec(C, op);
}

void CLIP_OT_set_scale(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Set Scale";
	ot->description = "Set scale of scene by scaling camera (or its parent if present)";
	ot->idname = "CLIP_OT_set_scale";

	/* api callbacks */
	ot->exec = set_scale_exec;
	ot->invoke = set_scale_invoke;
	ot->poll = set_orientation_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* properties */
	RNA_def_float(ot->srna, "distance", 0.0f, -FLT_MAX, FLT_MAX,
	              "Distance", "Distance between selected tracks", -100.0f, 100.0f);
}

/********************** set solution scale operator *********************/

static int set_solution_scale_poll(bContext *C)
{
	SpaceClip *sc = CTX_wm_space_clip(C);

	if (sc) {
		MovieClip *clip = ED_space_clip_get_clip(sc);

		if (clip) {
			MovieTracking *tracking = &clip->tracking;
			MovieTrackingObject *tracking_object = BKE_tracking_object_get_active(tracking);

			return (tracking_object->flag & TRACKING_OBJECT_CAMERA) == 0;
		}
	}

	return FALSE;
}

static int set_solution_scale_exec(bContext *C, wmOperator *op)
{
	return do_set_scale(C, op, true, false);
}

static int set_solution_scale_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
	SpaceClip *sc = CTX_wm_space_clip(C);
	MovieClip *clip = ED_space_clip_get_clip(sc);

	if (!RNA_struct_property_is_set(op->ptr, "distance"))
		RNA_float_set(op->ptr, "distance", clip->tracking.settings.object_distance);

	return set_solution_scale_exec(C, op);
}

void CLIP_OT_set_solution_scale(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Set Solution Scale";
	ot->description = "Set object solution scale using distance between two selected tracks";
	ot->idname = "CLIP_OT_set_solution_scale";

	/* api callbacks */
	ot->exec = set_solution_scale_exec;
	ot->invoke = set_solution_scale_invoke;
	ot->poll = set_solution_scale_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* properties */
	RNA_def_float(ot->srna, "distance", 0.0f, -FLT_MAX, FLT_MAX,
	              "Distance", "Distance between selected tracks", -100.0f, 100.0f);
}

/********************** apply solution scale operator *********************/

static int apply_solution_scale_poll(bContext *C)
{
	SpaceClip *sc = CTX_wm_space_clip(C);

	if (sc) {
		MovieClip *clip = ED_space_clip_get_clip(sc);

		if (clip) {
			MovieTracking *tracking = &clip->tracking;
			MovieTrackingObject *tracking_object = BKE_tracking_object_get_active(tracking);

			return tracking_object->flag & TRACKING_OBJECT_CAMERA;
		}
	}

	return FALSE;
}

static int apply_solution_scale_exec(bContext *C, wmOperator *op)
{
	return do_set_scale(C, op, false, true);
}

static int apply_solution_scale_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
	SpaceClip *sc = CTX_wm_space_clip(C);
	MovieClip *clip = ED_space_clip_get_clip(sc);

	if (!RNA_struct_property_is_set(op->ptr, "distance"))
		RNA_float_set(op->ptr, "distance", clip->tracking.settings.dist);

	return apply_solution_scale_exec(C, op);
}

void CLIP_OT_apply_solution_scale(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Apply Solution Scale";
	ot->description = "Apply scale on solution itself to make distance between selected tracks equals to desired";
	ot->idname = "CLIP_OT_apply_solution_scale";

	/* api callbacks */
	ot->exec = apply_solution_scale_exec;
	ot->invoke = apply_solution_scale_invoke;
	ot->poll = apply_solution_scale_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* properties */
	RNA_def_float(ot->srna, "distance", 0.0f, -FLT_MAX, FLT_MAX,
	              "Distance", "Distance between selected tracks", -100.0f, 100.0f);
}

/********************** set principal center operator *********************/

static int set_center_principal_exec(bContext *C, wmOperator *UNUSED(op))
{
	SpaceClip *sc = CTX_wm_space_clip(C);
	MovieClip *clip = ED_space_clip_get_clip(sc);
	int width, height;

	BKE_movieclip_get_size(clip, &sc->user, &width, &height);

	if (width == 0 || height == 0)
		return OPERATOR_CANCELLED;

	clip->tracking.camera.principal[0] = ((float)width) / 2.0f;
	clip->tracking.camera.principal[1] = ((float)height) / 2.0f;

	WM_event_add_notifier(C, NC_MOVIECLIP | NA_EDITED, clip);

	return OPERATOR_FINISHED;
}

void CLIP_OT_set_center_principal(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Set Principal to Center";
	ot->description = "Set optical center to center of footage";
	ot->idname = "CLIP_OT_set_center_principal";

	/* api callbacks */
	ot->exec = set_center_principal_exec;
	ot->poll = ED_space_clip_tracking_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/********************** hide tracks operator *********************/

static int hide_tracks_exec(bContext *C, wmOperator *op)
{
	SpaceClip *sc = CTX_wm_space_clip(C);
	MovieClip *clip = ED_space_clip_get_clip(sc);
	MovieTrackingTrack *track;
	MovieTracking *tracking = &clip->tracking;
	ListBase *tracksbase = BKE_tracking_get_active_tracks(tracking);
	MovieTrackingTrack *act_track = BKE_tracking_track_get_active(tracking);
	int unselected;

	unselected = RNA_boolean_get(op->ptr, "unselected");

	track = tracksbase->first;
	while (track) {
		if (unselected == 0 && TRACK_VIEW_SELECTED(sc, track)) {
			track->flag |= TRACK_HIDDEN;
		}
		else if (unselected == 1 && !TRACK_VIEW_SELECTED(sc, track)) {
			track->flag |= TRACK_HIDDEN;
		}

		track = track->next;
	}

	if (act_track && act_track->flag & TRACK_HIDDEN)
		clip->tracking.act_track = NULL;

	if (unselected == 0) {
		/* no selection on screen now, unlock view so it can be scrolled nice again */
		sc->flag &= ~SC_LOCK_SELECTION;
	}

	BKE_tracking_dopesheet_tag_update(tracking);

	WM_event_add_notifier(C, NC_MOVIECLIP | ND_DISPLAY, NULL);

	return OPERATOR_FINISHED;
}

void CLIP_OT_hide_tracks(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Hide Tracks";
	ot->description = "Hide selected tracks";
	ot->idname = "CLIP_OT_hide_tracks";

	/* api callbacks */
	ot->exec = hide_tracks_exec;
	ot->poll = ED_space_clip_tracking_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* properties */
	RNA_def_boolean(ot->srna, "unselected", 0, "Unselected", "Hide unselected tracks");
}

/********************** hide tracks clear operator *********************/

static int hide_tracks_clear_exec(bContext *C, wmOperator *UNUSED(op))
{
	SpaceClip *sc = CTX_wm_space_clip(C);
	MovieClip *clip = ED_space_clip_get_clip(sc);
	MovieTracking *tracking = &clip->tracking;
	ListBase *tracksbase = BKE_tracking_get_active_tracks(tracking);
	MovieTrackingTrack *track;

	track = tracksbase->first;
	while (track) {
		track->flag &= ~TRACK_HIDDEN;

		track = track->next;
	}

	BKE_tracking_dopesheet_tag_update(tracking);

	WM_event_add_notifier(C, NC_MOVIECLIP | ND_DISPLAY, NULL);

	return OPERATOR_FINISHED;
}

void CLIP_OT_hide_tracks_clear(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Hide Tracks Clear";
	ot->description = "Clear hide selected tracks";
	ot->idname = "CLIP_OT_hide_tracks_clear";

	/* api callbacks */
	ot->exec = hide_tracks_clear_exec;
	ot->poll = ED_space_clip_tracking_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/********************** detect features operator *********************/

static bGPDlayer *detect_get_layer(MovieClip *clip)
{
	bGPDlayer *layer;

	if (!clip->gpd)
		return NULL;

	layer = clip->gpd->layers.first;
	while (layer) {
		if (layer->flag & GP_LAYER_ACTIVE)
			return layer;

		layer = layer->next;
	}

	return NULL;
}

static int detect_features_exec(bContext *C, wmOperator *op)
{
	SpaceClip *sc = CTX_wm_space_clip(C);
	MovieClip *clip = ED_space_clip_get_clip(sc);
	int clip_flag = clip->flag & MCLIP_TIMECODE_FLAGS;
	ImBuf *ibuf = BKE_movieclip_get_ibuf_flag(clip, &sc->user, clip_flag, MOVIECLIP_CACHE_SKIP);
	MovieTracking *tracking = &clip->tracking;
	ListBase *tracksbase = BKE_tracking_get_active_tracks(tracking);
	MovieTrackingTrack *track = tracksbase->first;
	int placement = RNA_enum_get(op->ptr, "placement");
	int margin = RNA_int_get(op->ptr, "margin");
	int min_distance = RNA_int_get(op->ptr, "min_distance");
	float threshold = RNA_float_get(op->ptr, "threshold");
	int place_outside_layer = 0;
	int framenr = ED_space_clip_get_clip_frame_number(sc);
	bGPDlayer *layer = NULL;

	if (!ibuf) {
		BKE_report(op->reports, RPT_ERROR, "Feature detection requires valid clip frame");
		return OPERATOR_CANCELLED;
	}

	if (placement != 0) {
		layer = detect_get_layer(clip);
		place_outside_layer = placement == 2;
	}

	/* deselect existing tracks */
	while (track) {
		track->flag &= ~SELECT;
		track->pat_flag &= ~SELECT;
		track->search_flag &= ~SELECT;

		track = track->next;
	}

	BKE_tracking_detect_harris(tracking, tracksbase, ibuf, framenr, margin,
	                           threshold / 100000.0f, min_distance, layer, place_outside_layer);

	IMB_freeImBuf(ibuf);

	BKE_tracking_dopesheet_tag_update(tracking);
	WM_event_add_notifier(C, NC_MOVIECLIP | NA_EDITED, NULL);

	return OPERATOR_FINISHED;
}

void CLIP_OT_detect_features(wmOperatorType *ot)
{
	static EnumPropertyItem placement_items[] = {
		{0, "FRAME",            0, "Whole Frame",           "Place markers across the whole frame"},
		{1, "INSIDE_GPENCIL",   0, "Inside grease pencil",  "Place markers only inside areas outlined with grease pencil"},
		{2, "OUTSIDE_GPENCIL",  0, "Outside grease pencil", "Place markers only outside areas outlined with grease pencil"},
		{0, NULL, 0, NULL, NULL}
	};

	/* identifiers */
	ot->name = "Detect Features";
	ot->description = "Automatically detect features and place markers to track";
	ot->idname = "CLIP_OT_detect_features";

	/* api callbacks */
	ot->exec = detect_features_exec;
	ot->poll = ED_space_clip_tracking_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* properties */
	RNA_def_enum(ot->srna, "placement", placement_items, 0, "Placement", "Placement for detected features");
	RNA_def_int(ot->srna, "margin", 16, 0, INT_MAX, "Margin", "Only features further than margin pixels from the image edges are considered", 0, 300);
	RNA_def_float(ot->srna, "threshold", 1.0f, 0.0001f, FLT_MAX, "Threshold", "Threshold level to consider feature good enough for tracking", 0.0001f, FLT_MAX);
	RNA_def_int(ot->srna, "min_distance", 120, 0, INT_MAX, "Distance", "Minimal distance accepted between two features", 0, 300);
}

/********************** frame jump operator *********************/

static int frame_jump_exec(bContext *C, wmOperator *op)
{
	Scene *scene = CTX_data_scene(C);
	SpaceClip *sc = CTX_wm_space_clip(C);
	MovieClip *clip = ED_space_clip_get_clip(sc);
	MovieTrackingTrack *track;
	int pos = RNA_enum_get(op->ptr, "position");
	int delta;

	if (pos <= 1) { /* jump to path */
		track = BKE_tracking_track_get_active(&clip->tracking);

		if (!track)
			return OPERATOR_CANCELLED;

		delta = pos == 1 ? 1 : -1;

		while (sc->user.framenr + delta >= SFRA && sc->user.framenr + delta <= EFRA) {
			int framenr = BKE_movieclip_remap_scene_to_clip_frame(clip, sc->user.framenr + delta);
			MovieTrackingMarker *marker = BKE_tracking_marker_get_exact(track, framenr);

			if (!marker || marker->flag & MARKER_DISABLED)
				break;

			sc->user.framenr += delta;
		}
	}
	else {  /* to to failed frame */
		if (clip->tracking.reconstruction.flag & TRACKING_RECONSTRUCTED) {
			int a = ED_space_clip_get_clip_frame_number(sc);
			MovieTracking *tracking = &clip->tracking;
			MovieTrackingObject *object = BKE_tracking_object_get_active(tracking);

			delta = pos == 3 ? 1 : -1;

			a += delta;

			while (a + delta >= SFRA && a + delta <= EFRA) {
				MovieReconstructedCamera *cam;

				cam = BKE_tracking_camera_get_reconstructed(tracking, object, a);

				if (!cam) {
					sc->user.framenr = BKE_movieclip_remap_clip_to_scene_frame(clip, a);

					break;
				}

				a += delta;
			}
		}
	}

	if (CFRA != sc->user.framenr) {
		CFRA = sc->user.framenr;
		sound_seek_scene(CTX_data_main(C), CTX_data_scene(C));

		WM_event_add_notifier(C, NC_SCENE | ND_FRAME, scene);
	}

	WM_event_add_notifier(C, NC_MOVIECLIP | ND_DISPLAY, NULL);

	return OPERATOR_FINISHED;
}

void CLIP_OT_frame_jump(wmOperatorType *ot)
{
	static EnumPropertyItem position_items[] = {
		{0, "PATHSTART",    0, "Path Start",        "Jump to start of current path"},
		{1, "PATHEND",      0, "Path End",          "Jump to end of current path"},
		{2, "FAILEDPREV",   0, "Previous Failed",   "Jump to previous failed frame"},
		{2, "FAILNEXT",     0, "Next Failed",       "Jump to next failed frame"},
		{0, NULL, 0, NULL, NULL}
	};

	/* identifiers */
	ot->name = "Jump to Frame";
	ot->description = "Jump to special frame";
	ot->idname = "CLIP_OT_frame_jump";

	/* api callbacks */
	ot->exec = frame_jump_exec;
	ot->poll = ED_space_clip_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* properties */
	RNA_def_enum(ot->srna, "position", position_items, 0, "Position", "Position to jump to");
}

/********************** join tracks operator *********************/

static int join_tracks_exec(bContext *C, wmOperator *op)
{
	SpaceClip *sc = CTX_wm_space_clip(C);
	MovieClip *clip = ED_space_clip_get_clip(sc);
	MovieTracking *tracking = &clip->tracking;
	ListBase *tracksbase = BKE_tracking_get_active_tracks(tracking);
	MovieTrackingTrack *act_track, *track, *next;

	act_track = BKE_tracking_track_get_active(tracking);

	if (!act_track) {
		BKE_report(op->reports, RPT_ERROR, "No active track to join to");
		return OPERATOR_CANCELLED;
	}

	track = tracksbase->first;
	while (track) {
		next = track->next;

		if (TRACK_VIEW_SELECTED(sc, track) && track != act_track) {
			BKE_tracking_tracks_join(tracking, act_track, track);

			if (tracking->stabilization.rot_track == track)
				tracking->stabilization.rot_track = act_track;

			BKE_tracking_track_free(track);
			BLI_freelinkN(tracksbase, track);
		}

		track = next;
	}

	WM_event_add_notifier(C, NC_MOVIECLIP | NA_EDITED, clip);

	return OPERATOR_FINISHED;
}

void CLIP_OT_join_tracks(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Join Tracks";
	ot->description = "Join selected tracks";
	ot->idname = "CLIP_OT_join_tracks";

	/* api callbacks */
	ot->exec = join_tracks_exec;
	ot->poll = ED_space_clip_tracking_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/********************** lock tracks operator *********************/

static int lock_tracks_exec(bContext *C, wmOperator *op)
{
	SpaceClip *sc = CTX_wm_space_clip(C);
	MovieClip *clip = ED_space_clip_get_clip(sc);
	MovieTracking *tracking = &clip->tracking;
	ListBase *tracksbase = BKE_tracking_get_active_tracks(tracking);
	MovieTrackingTrack *track = tracksbase->first;
	int action = RNA_enum_get(op->ptr, "action");

	while (track) {
		if (TRACK_VIEW_SELECTED(sc, track)) {
			if (action == 0)
				track->flag |= TRACK_LOCKED;
			else if (action == 1)
				track->flag &= ~TRACK_LOCKED;
			else track->flag ^= TRACK_LOCKED;
		}

		track = track->next;
	}

	WM_event_add_notifier(C, NC_MOVIECLIP | NA_EVALUATED, clip);

	return OPERATOR_FINISHED;
}

void CLIP_OT_lock_tracks(wmOperatorType *ot)
{
	static EnumPropertyItem actions_items[] = {
		{0, "LOCK", 0, "Lock", "Lock selected tracks"},
		{1, "UNLOCK", 0, "Unlock", "Unlock selected tracks"},
		{2, "TOGGLE", 0, "Toggle", "Toggle locked flag for selected tracks"},
		{0, NULL, 0, NULL, NULL}
	};

	/* identifiers */
	ot->name = "Lock Tracks";
	ot->description = "Lock/unlock selected tracks";
	ot->idname = "CLIP_OT_lock_tracks";

	/* api callbacks */
	ot->exec = lock_tracks_exec;
	ot->poll = ED_space_clip_tracking_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* properties */
	RNA_def_enum(ot->srna, "action", actions_items, 0, "Action", "Lock action to execute");
}

/********************** set keyframe operator *********************/

static int set_solver_keyframe_exec(bContext *C, wmOperator *op)
{
	SpaceClip *sc = CTX_wm_space_clip(C);
	MovieClip *clip = ED_space_clip_get_clip(sc);
	MovieTracking *tracking = &clip->tracking;
	MovieTrackingObject *object = BKE_tracking_object_get_active(tracking);
	int keyframe = RNA_enum_get(op->ptr, "keyframe");
	int framenr = BKE_movieclip_remap_scene_to_clip_frame(clip, sc->user.framenr);

	if (keyframe == 0)
		object->keyframe1 = framenr;
	else
		object->keyframe2 = framenr;

	WM_event_add_notifier(C, NC_MOVIECLIP | ND_DISPLAY, clip);

	return OPERATOR_FINISHED;
}

void CLIP_OT_set_solver_keyframe(wmOperatorType *ot)
{
	static EnumPropertyItem keyframe_items[] = {
		{0, "KEYFRAME_A", 0, "Keyframe A", ""},
		{1, "KEYFRAME_B", 0, "Keyframe B", ""},
		{0, NULL, 0, NULL, NULL}
	};

	/* identifiers */
	ot->name = "Set Solver Keyframe";
	ot->description = "Set keyframe used by solver";
	ot->idname = "CLIP_OT_set_solver_keyframe";

	/* api callbacks */
	ot->exec = set_solver_keyframe_exec;
	ot->poll = ED_space_clip_tracking_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* properties */
	RNA_def_enum(ot->srna, "keyframe", keyframe_items, 0, "Keyframe", "Keyframe to set");
}

/********************** track copy color operator *********************/

static int track_copy_color_exec(bContext *C, wmOperator *UNUSED(op))
{
	SpaceClip *sc = CTX_wm_space_clip(C);
	MovieClip *clip = ED_space_clip_get_clip(sc);
	MovieTracking *tracking = &clip->tracking;
	ListBase *tracksbase = BKE_tracking_get_active_tracks(tracking);
	MovieTrackingTrack *track, *act_track = BKE_tracking_track_get_active(tracking);

	if (!act_track)
		return OPERATOR_CANCELLED;

	track = tracksbase->first;
	while (track) {
		if (TRACK_VIEW_SELECTED(sc, track) && track != act_track) {
			track->flag &= ~TRACK_CUSTOMCOLOR;

			if (act_track->flag & TRACK_CUSTOMCOLOR) {
				copy_v3_v3(track->color, act_track->color);
				track->flag |= TRACK_CUSTOMCOLOR;
			}
		}

		track = track->next;
	}

	WM_event_add_notifier(C, NC_MOVIECLIP | ND_DISPLAY, clip);

	return OPERATOR_FINISHED;
}

void CLIP_OT_track_copy_color(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Copy Color";
	ot->description = "Copy color to all selected tracks";
	ot->idname = "CLIP_OT_track_copy_color";

	/* api callbacks */
	ot->exec = track_copy_color_exec;
	ot->poll = ED_space_clip_tracking_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/********************** add 2d stabilization tracks operator *********************/

static int stabilize_2d_poll(bContext *C)
{
	if (ED_space_clip_tracking_poll(C)) {
		SpaceClip *sc = CTX_wm_space_clip(C);
		MovieClip *clip = ED_space_clip_get_clip(sc);
		MovieTrackingObject *tracking_object = BKE_tracking_object_get_active(&clip->tracking);

		return tracking_object->flag & TRACKING_OBJECT_CAMERA;
	}

	return FALSE;
}

static int stabilize_2d_add_exec(bContext *C, wmOperator *UNUSED(op))
{
	SpaceClip *sc = CTX_wm_space_clip(C);
	MovieClip *clip = ED_space_clip_get_clip(sc);
	MovieTracking *tracking = &clip->tracking;
	ListBase *tracksbase = BKE_tracking_get_active_tracks(tracking);
	MovieTrackingTrack *track;
	MovieTrackingStabilization *stab = &tracking->stabilization;
	int update = 0;

	track = tracksbase->first;
	while (track) {
		if (TRACK_VIEW_SELECTED(sc, track) && (track->flag & TRACK_USE_2D_STAB) == 0) {
			track->flag |= TRACK_USE_2D_STAB;
			stab->tot_track++;

			update = 1;
		}

		track = track->next;
	}

	if (update) {
		stab->ok = 0;

		DAG_id_tag_update(&clip->id, 0);
		WM_event_add_notifier(C, NC_MOVIECLIP | ND_DISPLAY, clip);
	}

	return OPERATOR_FINISHED;
}

void CLIP_OT_stabilize_2d_add(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Add Stabilization Tracks";
	ot->description = "Add selected tracks to 2D stabilization tool";
	ot->idname = "CLIP_OT_stabilize_2d_add";

	/* api callbacks */
	ot->exec = stabilize_2d_add_exec;
	ot->poll = stabilize_2d_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/********************** remove 2d stabilization tracks operator *********************/

static int stabilize_2d_remove_exec(bContext *C, wmOperator *UNUSED(op))
{
	SpaceClip *sc = CTX_wm_space_clip(C);
	MovieClip *clip = ED_space_clip_get_clip(sc);
	MovieTracking *tracking = &clip->tracking;
	MovieTrackingStabilization *stab = &tracking->stabilization;
	ListBase *tracksbase = BKE_tracking_get_active_tracks(tracking);
	MovieTrackingTrack *track;
	int a = 0, update = 0;

	track = tracksbase->first;
	while (track) {
		if (track->flag & TRACK_USE_2D_STAB) {
			if (a == stab->act_track) {
				track->flag &= ~TRACK_USE_2D_STAB;

				stab->act_track--;
				stab->tot_track--;

				if (stab->act_track < 0)
					stab->act_track = 0;

				update = 1;

				break;
			}

			a++;
		}

		track = track->next;
	}

	if (update) {
		stab->ok = 0;

		DAG_id_tag_update(&clip->id, 0);
		WM_event_add_notifier(C, NC_MOVIECLIP | ND_DISPLAY, clip);
	}

	return OPERATOR_FINISHED;
}

void CLIP_OT_stabilize_2d_remove(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Remove Stabilization Track";
	ot->description = "Remove selected track from stabilization";
	ot->idname = "CLIP_OT_stabilize_2d_remove";

	/* api callbacks */
	ot->exec = stabilize_2d_remove_exec;
	ot->poll = stabilize_2d_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/********************** select 2d stabilization tracks operator *********************/

static int stabilize_2d_select_exec(bContext *C, wmOperator *UNUSED(op))
{
	SpaceClip *sc = CTX_wm_space_clip(C);
	MovieClip *clip = ED_space_clip_get_clip(sc);
	MovieTracking *tracking = &clip->tracking;
	ListBase *tracksbase = BKE_tracking_get_active_tracks(tracking);
	MovieTrackingTrack *track;
	int update = 0;

	track = tracksbase->first;
	while (track) {
		if (track->flag & TRACK_USE_2D_STAB) {
			BKE_tracking_track_flag_set(track, TRACK_AREA_ALL, SELECT);

			update = 1;
		}

		track = track->next;
	}

	if (update)
		WM_event_add_notifier(C, NC_MOVIECLIP | ND_SELECT, clip);

	return OPERATOR_FINISHED;
}

void CLIP_OT_stabilize_2d_select(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Select Stabilization Tracks";
	ot->description = "Select tracks which are used for stabilization";
	ot->idname = "CLIP_OT_stabilize_2d_select";

	/* api callbacks */
	ot->exec = stabilize_2d_select_exec;
	ot->poll = stabilize_2d_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/********************** set 2d stabilization rotation track operator *********************/

static int stabilize_2d_set_rotation_exec(bContext *C, wmOperator *UNUSED(op))
{
	SpaceClip *sc = CTX_wm_space_clip(C);
	MovieClip *clip = ED_space_clip_get_clip(sc);
	MovieTracking *tracking = &clip->tracking;
	MovieTrackingTrack *act_track = BKE_tracking_track_get_active(tracking);

	if (act_track) {
		MovieTrackingStabilization *stab = &tracking->stabilization;

		stab->rot_track = act_track;
		stab->ok = 0;

		DAG_id_tag_update(&clip->id, 0);
		WM_event_add_notifier(C, NC_MOVIECLIP | ND_DISPLAY, clip);
	}

	return OPERATOR_FINISHED;
}

void CLIP_OT_stabilize_2d_set_rotation(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Set Rotation Track";
	ot->description = "Use active track to compensate rotation when doing 2D stabilization";
	ot->idname = "CLIP_OT_stabilize_2d_set_rotation";

	/* api callbacks */
	ot->exec = stabilize_2d_set_rotation_exec;
	ot->poll = stabilize_2d_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/********************** clean tracks operator *********************/

static int is_track_clean(MovieTrackingTrack *track, int frames, int del)
{
	int ok = 1, a, prev = -1, count = 0;
	MovieTrackingMarker *markers = track->markers, *new_markers = NULL;
	int start_disabled = 0;
	int markersnr = track->markersnr;

	if (del)
		new_markers = MEM_callocN(markersnr * sizeof(MovieTrackingMarker), "track cleaned markers");

	for (a = 0; a < markersnr; a++) {
		int end = 0;

		if (prev == -1) {
			if ((markers[a].flag & MARKER_DISABLED) == 0)
				prev = a;
			else
				start_disabled = 1;
		}

		if (prev >= 0) {
			end =  a == markersnr - 1;
			end |= (a < markersnr - 1) && (markers[a].framenr != markers[a + 1].framenr - 1 ||
			                               markers[a].flag & MARKER_DISABLED);
		}

		if (end) {
			int segok = 1, len = 0;

			if (a != prev && markers[a].framenr != markers[a - 1].framenr + 1)
				len = a - prev;
			else if (markers[a].flag & MARKER_DISABLED)
				len = a - prev;
			else len = a - prev + 1;

			if (frames) {
				if (len < frames) {
					segok = 0;
					ok = 0;

					if (!del)
						break;
				}
			}

			if (del) {
				if (segok) {
					int t = len;

					if (markers[a].flag & MARKER_DISABLED)
						t++;

					/* place disabled marker in front of current segment */
					if (start_disabled) {
						memcpy(new_markers + count, markers + prev, sizeof(MovieTrackingMarker));
						new_markers[count].framenr--;
						new_markers[count].flag |= MARKER_DISABLED;

						count++;
						start_disabled = 0;
					}

					memcpy(new_markers + count, markers + prev, t * sizeof(MovieTrackingMarker));
					count += t;
				}
				else if (markers[a].flag & MARKER_DISABLED) {
					/* current segment which would be deleted was finished by disabled marker,
					 * so next segment should be started from disabled marker */
					start_disabled = 1;
				}
			}

			prev = -1;
		}
	}

	if (del) {
		MEM_freeN(track->markers);

		if (count) {
			track->markers = new_markers;
		}
		else {
			track->markers = NULL;
			MEM_freeN(new_markers);
		}

		track->markersnr = count;
	}

	return ok;
}

static int clean_tracks_exec(bContext *C, wmOperator *op)
{
	SpaceClip *sc = CTX_wm_space_clip(C);
	MovieClip *clip = ED_space_clip_get_clip(sc);
	MovieTracking *tracking = &clip->tracking;
	ListBase *tracksbase = BKE_tracking_get_active_tracks(tracking);
	MovieTrackingTrack *track, *next, *act_track = BKE_tracking_track_get_active(tracking);
	int frames = RNA_int_get(op->ptr, "frames");
	int action = RNA_enum_get(op->ptr, "action");
	float error = RNA_float_get(op->ptr, "error");

	if (error && action == TRACKING_CLEAN_DELETE_SEGMENT)
		action = TRACKING_CLEAN_DELETE_TRACK;

	track = tracksbase->first;
	while (track) {
		next = track->next;

		if ((track->flag & TRACK_HIDDEN) == 0 && (track->flag & TRACK_LOCKED) == 0) {
			bool ok;

			ok = (is_track_clean(track, frames, action == TRACKING_CLEAN_DELETE_SEGMENT)) &&
			     (error == 0.0f || (track->flag & TRACK_HAS_BUNDLE) == 0  || track->error < error);

			if (!ok) {
				if (action == TRACKING_CLEAN_SELECT) {
					BKE_tracking_track_flag_set(track, TRACK_AREA_ALL, SELECT);
				}
				else if (action == TRACKING_CLEAN_DELETE_TRACK) {
					if (track == act_track)
						clip->tracking.act_track = NULL;

					BKE_tracking_track_free(track);
					BLI_freelinkN(tracksbase, track);
					track = NULL;
				}

				/* happens when all tracking segments are not long enough */
				if (track && track->markersnr == 0) {
					if (track == act_track)
						clip->tracking.act_track = NULL;

					BKE_tracking_track_free(track);
					BLI_freelinkN(tracksbase, track);
				}
			}
		}

		track = next;
	}

	BKE_tracking_dopesheet_tag_update(tracking);

	WM_event_add_notifier(C, NC_MOVIECLIP | ND_SELECT, clip);

	return OPERATOR_FINISHED;
}

static int clean_tracks_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
	SpaceClip *sc = CTX_wm_space_clip(C);
	MovieClip *clip = ED_space_clip_get_clip(sc);

	if (!RNA_struct_property_is_set(op->ptr, "frames"))
		RNA_int_set(op->ptr, "frames", clip->tracking.settings.clean_frames);

	if (!RNA_struct_property_is_set(op->ptr, "error"))
		RNA_float_set(op->ptr, "error", clip->tracking.settings.clean_error);

	if (!RNA_struct_property_is_set(op->ptr, "action"))
		RNA_enum_set(op->ptr, "action", clip->tracking.settings.clean_action);

	return clean_tracks_exec(C, op);
}

void CLIP_OT_clean_tracks(wmOperatorType *ot)
{
	static EnumPropertyItem actions_items[] = {
		{TRACKING_CLEAN_SELECT, "SELECT", 0, "Select", "Select unclean tracks"},
		{TRACKING_CLEAN_DELETE_TRACK, "DELETE_TRACK", 0, "Delete Track", "Delete unclean tracks"},
		{TRACKING_CLEAN_DELETE_SEGMENT, "DELETE_SEGMENTS", 0, "Delete Segments", "Delete unclean segments of tracks"},
		{0, NULL, 0, NULL, NULL}
	};

	/* identifiers */
	ot->name = "Clean Tracks";
	ot->description = "Clean tracks with high error values or few frames";
	ot->idname = "CLIP_OT_clean_tracks";

	/* api callbacks */
	ot->exec = clean_tracks_exec;
	ot->invoke = clean_tracks_invoke;
	ot->poll = ED_space_clip_tracking_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* properties */
	RNA_def_int(ot->srna, "frames", 0, 0, INT_MAX, "Tracked Frames",
	            "Effect on tracks which are tracked less than specified amount of frames", 0, INT_MAX);
	RNA_def_float(ot->srna, "error", 0.0f, 0.0f, FLT_MAX, "Reprojection Error",
	              "Effect on tracks which have got larger re-projection error", 0.0f, 100.0f);
	RNA_def_enum(ot->srna, "action", actions_items, 0, "Action", "Cleanup action to execute");
}

/********************** add tracking object *********************/

static int tracking_object_new_exec(bContext *C, wmOperator *UNUSED(op))
{
	SpaceClip *sc = CTX_wm_space_clip(C);
	MovieClip *clip = ED_space_clip_get_clip(sc);
	MovieTracking *tracking = &clip->tracking;

	BKE_tracking_object_add(tracking, "Object");

	WM_event_add_notifier(C, NC_MOVIECLIP | NA_EDITED, clip);

	return OPERATOR_FINISHED;
}

void CLIP_OT_tracking_object_new(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Add Tracking Object";
	ot->description = "Add new object for tracking";
	ot->idname = "CLIP_OT_tracking_object_new";

	/* api callbacks */
	ot->exec = tracking_object_new_exec;
	ot->poll = ED_space_clip_tracking_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/********************** remove tracking object *********************/

static int tracking_object_remove_exec(bContext *C, wmOperator *op)
{
	SpaceClip *sc = CTX_wm_space_clip(C);
	MovieClip *clip = ED_space_clip_get_clip(sc);
	MovieTracking *tracking = &clip->tracking;
	MovieTrackingObject *object;

	object = BKE_tracking_object_get_active(tracking);

	if (object->flag & TRACKING_OBJECT_CAMERA) {
		BKE_report(op->reports, RPT_WARNING, "Object used for camera tracking cannot be deleted");
		return OPERATOR_CANCELLED;
	}

	BKE_tracking_object_delete(tracking, object);

	WM_event_add_notifier(C, NC_MOVIECLIP | NA_EDITED, clip);

	return OPERATOR_FINISHED;
}

void CLIP_OT_tracking_object_remove(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Remove Tracking Object";
	ot->description = "Remove object for tracking";
	ot->idname = "CLIP_OT_tracking_object_remove";

	/* api callbacks */
	ot->exec = tracking_object_remove_exec;
	ot->poll = ED_space_clip_tracking_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/********************** copy tracks to clipboard operator *********************/

static int copy_tracks_exec(bContext *C, wmOperator *UNUSED(op))
{
	SpaceClip *sc = CTX_wm_space_clip(C);
	MovieClip *clip = ED_space_clip_get_clip(sc);
	MovieTracking *tracking = &clip->tracking;
	MovieTrackingObject *object = BKE_tracking_object_get_active(tracking);

	clear_invisible_track_selection(sc, clip);

	BKE_tracking_clipboard_copy_tracks(tracking, object);

	return OPERATOR_FINISHED;
}

void CLIP_OT_copy_tracks(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Copy Tracks";
	ot->description = "Copy selected tracks to clipboard";
	ot->idname = "CLIP_OT_copy_tracks";

	/* api callbacks */
	ot->exec = copy_tracks_exec;
	ot->poll = ED_space_clip_tracking_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER;
}

/********************** paste tracks from clipboard operator *********************/

static int paste_tracks_poll(bContext *C)
{
	if (ED_space_clip_tracking_poll(C)) {
		return BKE_tracking_clipboard_has_tracks();
	}

	return 0;
}

static int paste_tracks_exec(bContext *C, wmOperator *UNUSED(op))
{
	SpaceClip *sc = CTX_wm_space_clip(C);
	MovieClip *clip = ED_space_clip_get_clip(sc);
	MovieTracking *tracking = &clip->tracking;
	MovieTrackingObject *object = BKE_tracking_object_get_active(tracking);

	BKE_tracking_clipboard_paste_tracks(tracking, object);

	WM_event_add_notifier(C, NC_MOVIECLIP | NA_EDITED, clip);

	return OPERATOR_FINISHED;
}

void CLIP_OT_paste_tracks(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Paste Tracks";
	ot->description = "Paste tracks from clipboard";
	ot->idname = "CLIP_OT_paste_tracks";

	/* api callbacks */
	ot->exec = paste_tracks_exec;
	ot->poll = paste_tracks_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/********************** Create plane track operator *********************/

static int create_plane_track_tracks_exec(bContext *C, wmOperator *op)
{
	SpaceClip *sc = CTX_wm_space_clip(C);
	MovieClip *clip = ED_space_clip_get_clip(sc);
	MovieTracking *tracking = &clip->tracking;
	MovieTrackingPlaneTrack *plane_track;
	ListBase *tracks_base = BKE_tracking_get_active_tracks(tracking);
	ListBase *plane_tracks_base = BKE_tracking_get_active_plane_tracks(tracking);
	int framenr = ED_space_clip_get_clip_frame_number(sc);

	plane_track = BKE_tracking_plane_track_add(tracking, plane_tracks_base, tracks_base, framenr);

	if (plane_track == NULL) {
		BKE_report(op->reports, RPT_ERROR, "Need at least 4 selected point tracks to create a plane");
		return OPERATOR_CANCELLED;
	}
	else {
		BKE_tracking_tracks_deselect_all(tracks_base);

		plane_track->flag |= SELECT;
		clip->tracking.act_track = NULL;
		clip->tracking.act_plane_track = plane_track;

		/* Copute homoraphies and apply them on marker's corner, so we've got
		 * quite nice motion from the very beginning.
		 */
		BKE_tracking_track_plane_from_existing_motion(plane_track, framenr);
	}

	WM_event_add_notifier(C, NC_MOVIECLIP | NA_EDITED, clip);

	return OPERATOR_FINISHED;
}

void CLIP_OT_create_plane_track(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Create Plane Track";
	ot->description = "Create new plane track out of selected point tracks";
	ot->idname = "CLIP_OT_create_plane_track";

	/* api callbacks */
	ot->exec = create_plane_track_tracks_exec;
	ot->poll = ED_space_clip_tracking_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/********************** Slide plane marker corner operator *********************/

typedef struct SlidePlaneMarkerData {
	int event_type;
	MovieTrackingPlaneTrack *plane_track;
	MovieTrackingPlaneMarker *plane_marker;
	int width, height;
	int corner_index;
	float *corner;
	int previous_mval[2];
	float previous_corner[2];
	float old_corner[2];
	bool accurate;
} SlidePlaneMarkerData;

static bool mouse_on_plane_slide_zone(SpaceClip *sc, float co[2], float slide_zone[2], int width, int height)
{
	const float size = 12.0f;
	float dx, dy;

	dx = size / width / sc->zoom;
	dy = size / height / sc->zoom;

	return IN_RANGE_INCL(co[0], slide_zone[0] - dx, slide_zone[0] + dx) &&
	       IN_RANGE_INCL(co[1], slide_zone[1] - dy, slide_zone[1] + dy);
}

static MovieTrackingPlaneTrack *tracking_plane_marker_check_slide(bContext *C, const wmEvent *event, int *corner_r)
{
	SpaceClip *sc = CTX_wm_space_clip(C);
	ARegion *ar = CTX_wm_region(C);
	MovieClip *clip = ED_space_clip_get_clip(sc);
	MovieTrackingPlaneTrack *plane_track;
	int width, height;
	float co[2];
	ListBase *plane_tracks_base = BKE_tracking_get_active_plane_tracks(&clip->tracking);
	int framenr = ED_space_clip_get_clip_frame_number(sc);

	ED_space_clip_get_size(sc, &width, &height);

	if (width == 0 || height == 0) {
		return NULL;
	}

	ED_clip_mouse_pos(sc, ar, event->mval, co);

	for (plane_track = plane_tracks_base->first;
	     plane_track;
	     plane_track = plane_track->next)
	{
		if (plane_track->flag & SELECT) {
			MovieTrackingPlaneMarker *plane_marker = BKE_tracking_plane_marker_get(plane_track, framenr);
			bool ok = false;
			int i;

			for (i = 0; i < 4; i++) {
				if (mouse_on_plane_slide_zone(sc, co, plane_marker->corners[i], width, height)) {
					if (corner_r) {
						*corner_r = i;
					}
					ok = true;
					break;
				}
			}

			if (ok) {
				return plane_track;
			}
		}
	}

	return NULL;
}

static void *slide_plane_marker_customdata(bContext *C, const wmEvent *event)
{
	SpaceClip *sc = CTX_wm_space_clip(C);
	ARegion *ar = CTX_wm_region(C);
	MovieTrackingPlaneTrack *plane_track;
	int width, height;
	float co[2];
	SlidePlaneMarkerData *customdata = NULL;
	int framenr = ED_space_clip_get_clip_frame_number(sc);
	int corner;

	ED_space_clip_get_size(sc, &width, &height);

	if (width == 0 || height == 0) {
		return NULL;
	}

	ED_clip_mouse_pos(sc, ar, event->mval, co);

	plane_track = tracking_plane_marker_check_slide(C, event, &corner);
	if (plane_track) {
		MovieTrackingPlaneMarker *plane_marker;

		customdata = MEM_callocN(sizeof(SlidePlaneMarkerData), "slide plane marker data");

		customdata->event_type = event->type;

		plane_marker = BKE_tracking_plane_marker_ensure(plane_track, framenr);

		customdata->plane_track = plane_track;
		customdata->plane_marker = plane_marker;
		customdata->width = width;
		customdata->height = height;

		customdata->previous_mval[0] = event->mval[0];
		customdata->previous_mval[1] = event->mval[1];

		customdata->corner_index = corner;
		customdata->corner = plane_marker->corners[corner];

		copy_v2_v2(customdata->previous_corner, customdata->corner);
		copy_v2_v2(customdata->old_corner, customdata->corner);
	}

	return customdata;
}

static int slide_plane_marker_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
	SlidePlaneMarkerData *slidedata = slide_plane_marker_customdata(C, event);

	if (slidedata) {
		SpaceClip *sc = CTX_wm_space_clip(C);
		MovieClip *clip = ED_space_clip_get_clip(sc);
		MovieTracking *tracking = &clip->tracking;

		tracking->act_plane_track = slidedata->plane_track;
		tracking->act_track = NULL;

		op->customdata = slidedata;

		hide_cursor(C);
		WM_event_add_modal_handler(C, op);

		WM_event_add_notifier(C, NC_GEOM | ND_SELECT, NULL);

		return OPERATOR_RUNNING_MODAL;
	}

	return OPERATOR_PASS_THROUGH;
}

static void cancel_mouse_slide_plane_marker(SlidePlaneMarkerData *data)
{
	copy_v2_v2(data->corner, data->old_corner);
}

static void free_slide_plane_marker_data(SlidePlaneMarkerData *data)
{
	MEM_freeN(data);
}

static void slide_plane_marker_update_homographies(SpaceClip *sc, SlidePlaneMarkerData *data)
{
	int framenr = ED_space_clip_get_clip_frame_number(sc);

	BKE_tracking_track_plane_from_existing_motion(data->plane_track, framenr);
}

static int slide_plane_marker_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
	SpaceClip *sc = CTX_wm_space_clip(C);
	MovieClip *clip = ED_space_clip_get_clip(sc);
	SlidePlaneMarkerData *data = (SlidePlaneMarkerData *) op->customdata;
	float dx, dy, mdelta[2];
	int next_corner_index, prev_corner_index, diag_corner_index;
	float *next_corner, *prev_corner, *diag_corner;
	float next_edge[2], prev_edge[2], next_diag_edge[2], prev_diag_edge[2];

	switch (event->type) {
		case LEFTCTRLKEY:
		case RIGHTCTRLKEY:
		case LEFTSHIFTKEY:
		case RIGHTSHIFTKEY:
			if (ELEM(event->type, LEFTSHIFTKEY, RIGHTSHIFTKEY))
				data->accurate = event->val == KM_PRESS;

			/* fall-through */
		case MOUSEMOVE:
			mdelta[0] = event->mval[0] - data->previous_mval[0];
			mdelta[1] = event->mval[1] - data->previous_mval[1];

			dx = mdelta[0] / data->width / sc->zoom;
			dy = mdelta[1] / data->height / sc->zoom;

			if (data->accurate) {
				dx /= 5.0f;
				dy /= 5.0f;
			}

			data->corner[0] = data->previous_corner[0] + dx;
			data->corner[1] = data->previous_corner[1] + dy;


			/*
			                               prev_edge
			    (Corner 3, current) <-----------------------   (Corner 2, previous)
			            |                                              ^
			            |                                              |
			            |                                              |
			            |                                              |
			  next_edge |                                              | next_diag_edge
			            |                                              |
			            |                                              |
			            |                                              |
			            v                                              |
			     (Corner 0, next)   ----------------------->   (Corner 1, diagonal)
			                              prev_diag_edge
			 */

			next_corner_index = (data->corner_index + 1) % 4;
			prev_corner_index = (data->corner_index + 3) % 4;
			diag_corner_index = (data->corner_index + 2) % 4;

			next_corner = data->plane_marker->corners[next_corner_index];
			prev_corner = data->plane_marker->corners[prev_corner_index];
			diag_corner = data->plane_marker->corners[diag_corner_index];

			sub_v2_v2v2(next_edge, next_corner, data->corner);
			sub_v2_v2v2(prev_edge, data->corner, prev_corner);
			sub_v2_v2v2(next_diag_edge, prev_corner, diag_corner);
			sub_v2_v2v2(prev_diag_edge, diag_corner, next_corner);

			if (cross_v2v2(prev_edge, next_edge) < 0.0f) {
				closest_to_line_v2(data->corner, data->corner, prev_corner, next_corner);
			}

			if (cross_v2v2(next_diag_edge, prev_edge) < 0.0f) {
				closest_to_line_v2(data->corner, data->corner, prev_corner, diag_corner);
			}

			if (cross_v2v2(next_edge, prev_diag_edge) < 0.0f) {
				closest_to_line_v2(data->corner, data->corner, next_corner, diag_corner);
			}

			data->previous_mval[0] = event->mval[0];
			data->previous_mval[1] = event->mval[1];
			copy_v2_v2(data->previous_corner, data->corner);

			DAG_id_tag_update(&sc->clip->id, 0);

			WM_event_add_notifier(C, NC_MOVIECLIP | NA_EDITED, NULL);

			break;

		case LEFTMOUSE:
		case RIGHTMOUSE:
			if (event->type == data->event_type && event->val == KM_RELEASE) {
				/* Marker is now keyframed. */
				data->plane_marker->flag &= ~PLANE_MARKER_TRACKED;

				slide_plane_marker_update_homographies(sc, data);

				free_slide_plane_marker_data(op->customdata);

				show_cursor(C);

				DAG_id_tag_update(&sc->clip->id, 0);
				WM_event_add_notifier(C, NC_MOVIECLIP | NA_EDITED, clip);

				return OPERATOR_FINISHED;
			}

			break;

		case ESCKEY:
			cancel_mouse_slide_plane_marker(op->customdata);

			free_slide_plane_marker_data(op->customdata);

			show_cursor(C);

			WM_event_add_notifier(C, NC_MOVIECLIP | NA_EDITED, clip);

			return OPERATOR_CANCELLED;
	}

	return OPERATOR_RUNNING_MODAL;
}

void CLIP_OT_slide_plane_marker(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Slide Plane Marker";
	ot->description = "Slide plane marker areas";
	ot->idname = "CLIP_OT_slide_plane_marker";

	/* api callbacks */
	ot->poll = ED_space_clip_tracking_poll;
	ot->invoke = slide_plane_marker_invoke;
	ot->modal = slide_plane_marker_modal;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_GRAB_POINTER | OPTYPE_BLOCKING;
}

/********************** Insert track keyframe operator *********************/

static void keyframe_set_flag(bContext *C, bool set)
{
	SpaceClip *sc = CTX_wm_space_clip(C);
	MovieClip *clip = ED_space_clip_get_clip(sc);
	MovieTracking *tracking = &clip->tracking;
	ListBase *tracks_base = BKE_tracking_get_active_tracks(tracking);
	ListBase *plane_tracks_base = BKE_tracking_get_active_plane_tracks(tracking);
	MovieTrackingTrack *track;
	MovieTrackingPlaneTrack *plane_track;
	int framenr = ED_space_clip_get_clip_frame_number(sc);

	for (track = tracks_base->first; track; track = track->next) {
		if (TRACK_VIEW_SELECTED(sc, track)) {
			if (set) {
				MovieTrackingMarker *marker = BKE_tracking_marker_ensure(track, framenr);
				marker->flag &= ~MARKER_TRACKED;
			}
			else {
				MovieTrackingMarker *marker = BKE_tracking_marker_get_exact(track, framenr);
				if (marker) {
					marker->flag |= MARKER_TRACKED;
				}
			}
		}
	}

	for (plane_track = plane_tracks_base->first; plane_track; plane_track = plane_track->next) {
		if (plane_track->flag & SELECT) {
			if (set) {
				MovieTrackingPlaneMarker *plane_marker = BKE_tracking_plane_marker_ensure(plane_track, framenr);
				if (plane_marker->flag & PLANE_MARKER_TRACKED) {
					plane_marker->flag &= ~PLANE_MARKER_TRACKED;
					BKE_tracking_track_plane_from_existing_motion(plane_track, plane_marker->framenr);
				}
			}
			else {
				MovieTrackingPlaneMarker *plane_marker = BKE_tracking_plane_marker_get_exact(plane_track, framenr);
				if (plane_marker) {
					if ((plane_marker->flag & PLANE_MARKER_TRACKED) == 0) {
						plane_marker->flag |= PLANE_MARKER_TRACKED;
						BKE_tracking_retrack_plane_from_existing_motion_at_segment(plane_track, plane_marker->framenr);
					}
				}
			}
		}
	}

	WM_event_add_notifier(C, NC_MOVIECLIP | NA_EDITED, clip);
}

static int keyframe_insert_exec(bContext *C, wmOperator *UNUSED(op))
{
	keyframe_set_flag(C, true);
	return OPERATOR_FINISHED;
}

void CLIP_OT_keyframe_insert(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Insert keyframe";
	ot->description = "Insert a keyframe to selected tracks at current frame";
	ot->idname = "CLIP_OT_keyframe_insert";

	/* api callbacks */
	ot->poll = ED_space_clip_tracking_poll;
	ot->exec = keyframe_insert_exec;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/********************** Delete track keyframe operator *********************/

static int keyframe_delete_exec(bContext *C, wmOperator *UNUSED(op))
{
	keyframe_set_flag(C, false);
	return OPERATOR_FINISHED;
}

void CLIP_OT_keyframe_delete(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Delete keyframe";
	ot->description = "Delete a keyframe from selected tracks at current frame";
	ot->idname = "CLIP_OT_keyframe_delete";

	/* api callbacks */
	ot->poll = ED_space_clip_tracking_poll;
	ot->exec = keyframe_delete_exec;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}
