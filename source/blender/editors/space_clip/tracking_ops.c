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
#include "DNA_movieclip_types.h"
#include "DNA_object_types.h"	/* SELECT */
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

#include "clip_intern.h"	// own include

/********************** add marker operator *********************/

static void add_marker(SpaceClip *sc, float x, float y)
{
	MovieClip *clip = ED_space_clip(sc);
	MovieTracking *tracking = &clip->tracking;
	ListBase *tracksbase = BKE_tracking_get_tracks(tracking);
	MovieTrackingTrack *track;
	int width, height;
	int framenr = ED_space_clip_clip_framenr(sc);

	ED_space_clip_size(sc, &width, &height);

	track = BKE_tracking_add_track(tracking, tracksbase, x, y, framenr, width, height);

	BKE_tracking_select_track(tracksbase, track, TRACK_AREA_ALL, 0);

	clip->tracking.act_track = track;
}

static int add_marker_exec(bContext *C, wmOperator *op)
{
	SpaceClip *sc = CTX_wm_space_clip(C);
	MovieClip *clip = ED_space_clip(sc);
	float pos[2];
	int width, height;

	ED_space_clip_size(sc, &width, &height);

	if (!width || !height)
		return OPERATOR_CANCELLED;

	RNA_float_get_array(op->ptr, "location", pos);

	add_marker(sc, pos[0], pos[1]);

	/* reset offset from locked position, so frame jumping wouldn't be so confusing */
	sc->xlockof = 0;
	sc->ylockof = 0;

	WM_event_add_notifier(C, NC_MOVIECLIP | NA_EDITED, clip);

	return OPERATOR_FINISHED;
}

static int add_marker_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	float co[2];

	ED_clip_mouse_pos(C, event, co);

	RNA_float_set_array(op->ptr, "location", co);

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
	RNA_def_float_vector(ot->srna, "location", 2, NULL, -FLT_MIN, FLT_MAX,
		"Location", "Location of marker on frame", -1.0f, 1.0f);
}

/********************** delete track operator *********************/

static int delete_track_exec(bContext *C, wmOperator *UNUSED(op))
{
	SpaceClip *sc = CTX_wm_space_clip(C);
	MovieClip *clip = ED_space_clip(sc);
	MovieTracking *tracking = &clip->tracking;
	ListBase *tracksbase = BKE_tracking_get_tracks(tracking);
	MovieTrackingTrack *track = tracksbase->first, *next;

	while (track) {
		next = track->next;

		if (TRACK_VIEW_SELECTED(sc, track))
			clip_delete_track(C, clip, tracksbase, track);

		track = next;
	}

	/* nothing selected now, unlock view so it can be scrolled nice again */
	sc->flag &= ~SC_LOCK_SELECTION;

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
	MovieClip *clip = ED_space_clip(sc);
	ListBase *tracksbase = BKE_tracking_get_tracks(&clip->tracking);
	MovieTrackingTrack *track = tracksbase->first, *next;
	int framenr = ED_space_clip_clip_framenr(sc);
	int has_selection = 0;

	while (track) {
		next = track->next;

		if (TRACK_VIEW_SELECTED(sc, track)) {
			MovieTrackingMarker *marker = BKE_tracking_exact_marker(track, framenr);

			if (marker) {
				has_selection |= track->markersnr > 1;

				clip_delete_marker(C, clip, tracksbase, track, marker);
			}
		}

		track = next;
	}

	if (!has_selection) {
		/* nothing selected now, unlock view so it can be scrolled nice again */
		sc->flag &= ~SC_LOCK_SELECTION;
	}

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

#define SLIDE_ACTION_POS	0
#define SLIDE_ACTION_SIZE	1
#define SLIDE_ACTION_OFFSET	2

typedef struct {
	int area, action;
	MovieTrackingTrack *track;
	MovieTrackingMarker *marker;

	int mval[2];
	int width, height;
	float *min, *max, *pos, *offset;
	float smin[2], smax[2], spos[2], soff[2];
	float (*smarkers)[2];

	int lock, accurate;
} SlideMarkerData;

static SlideMarkerData *create_slide_marker_data(SpaceClip *sc, MovieTrackingTrack *track,
			MovieTrackingMarker *marker, wmEvent *event, int area, int action, int width, int height)
{
	SlideMarkerData *data = MEM_callocN(sizeof(SlideMarkerData), "slide marker data");
	int framenr = ED_space_clip_clip_framenr(sc);

	marker = BKE_tracking_ensure_marker(track, framenr);

	data->area = area;
	data->action = action;
	data->track = track;
	data->marker = marker;

	if (area == TRACK_AREA_POINT) {
		data->pos = marker->pos;
		data->offset = track->offset;
		copy_v2_v2(data->spos, marker->pos);
		copy_v2_v2(data->soff, track->offset);
	}
	else if (area == TRACK_AREA_PAT) {
		if (action == SLIDE_ACTION_SIZE) {
			data->min = track->pat_min;
			data->max = track->pat_max;
		}
		else {
			int a;

			data->pos = marker->pos;
			data->offset = track->offset;

			copy_v2_v2(data->soff, track->offset);

			data->smarkers = MEM_callocN(sizeof(*data->smarkers)*track->markersnr, "slide marekrs");
			for (a = 0; a < track->markersnr; a++)
				copy_v2_v2(data->smarkers[a], track->markers[a].pos);
		}
	}
	else if (area == TRACK_AREA_SEARCH) {
		data->min = track->search_min;
		data->max = track->search_max;
	}

	if (area == TRACK_AREA_SEARCH || (area == TRACK_AREA_PAT && action != SLIDE_ACTION_OFFSET)) {
		copy_v2_v2(data->smin, data->min);
		copy_v2_v2(data->smax, data->max);
	}

	data->mval[0] = event->mval[0];
	data->mval[1] = event->mval[1];

	data->width = width;
	data->height = height;

	if (action == SLIDE_ACTION_SIZE)
		data->lock = 1;

	return data;
}

/* corner = 0: right-bottom corner,
 * corner = 1: left-top corner */
static int mouse_on_corner(SpaceClip *sc, MovieTrackingTrack *track, MovieTrackingMarker *marker,
                           int area, float co[2], int corner, int width, int height)
{
	int inside = 0;
	float size = 12.0f;
	float min[2], max[2];
	float crn[2], dx, dy, tdx, tdy;

	if (area == TRACK_AREA_SEARCH) {
		copy_v2_v2(min, track->search_min);
		copy_v2_v2(max, track->search_max);
	}
	else {
		copy_v2_v2(min, track->pat_min);
		copy_v2_v2(max, track->pat_max);
	}

	dx = size / width / sc->zoom;
	dy = size / height / sc->zoom;

	tdx = 5.0f / width / sc->zoom;
	tdy = 5.0f / height / sc->zoom;

	dx = MIN2(dx, (max[0] - min[0]) / 6.0f) + tdx;
	dy = MIN2(dy, (max[1] - min[1]) / 6.0f) + tdy;

	if (corner == 0) {
		crn[0] = marker->pos[0] + max[0];
		crn[1] = marker->pos[1] + min[1];

		inside = co[0] >= crn[0] - dx && co[0] <= crn[0] + tdx && co[1] >= crn[1] - tdy && co[1] <= crn[1] + dy;
	}
	else {
		crn[0] = marker->pos[0] + min[0];
		crn[1] = marker->pos[1] + max[1];

		inside = co[0] >= crn[0] - dx && co[0] <= crn[0] + dx && co[1] >= crn[1] - dy && co[1] <= crn[1] + dy;
	}

	return inside;
}

static int mouse_on_offset(SpaceClip *sc, MovieTrackingTrack *track, MovieTrackingMarker *marker,
			float co[2], int width, int height)
{
	float pos[2], dx, dy;

	add_v2_v2v2(pos, marker->pos, track->offset);

	dx = 12.0f / width / sc->zoom;
	dy = 12.0f / height / sc->zoom;

	dx = MIN2(dx, (track->pat_max[0] - track->pat_min[0]) / 2.0f);
	dy = MIN2(dy, (track->pat_max[1] - track->pat_min[1]) / 2.0f);

	return co[0] >= pos[0] - dx && co[0] <= pos[0] + dx && co[1] >= pos[1] - dy && co[1] <= pos[1] + dy;
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

static void *slide_marker_customdata(bContext *C, wmEvent *event)
{
	SpaceClip *sc = CTX_wm_space_clip(C);
	MovieClip *clip = ED_space_clip(sc);
	MovieTrackingTrack *track;
	int width, height;
	float co[2];
	void *customdata = NULL;
	ListBase *tracksbase = BKE_tracking_get_tracks(&clip->tracking);
	int framenr = ED_space_clip_clip_framenr(sc);

	ED_space_clip_size(sc, &width, &height);

	if (width == 0 || height == 0)
		return NULL;

	ED_clip_mouse_pos(C, event, co);

	track = tracksbase->first;
	while (track) {
		if (TRACK_VIEW_SELECTED(sc, track) && (track->flag & TRACK_LOCKED) == 0) {
			MovieTrackingMarker *marker = BKE_tracking_get_marker(track, framenr);

			if ((marker->flag & MARKER_DISABLED) == 0) {
				if (!customdata)
					if (mouse_on_offset(sc, track, marker, co, width, height))
						customdata = create_slide_marker_data(sc, track, marker, event, TRACK_AREA_POINT,
						                                      SLIDE_ACTION_POS, width, height);

				if (sc->flag & SC_SHOW_MARKER_SEARCH) {
					if (mouse_on_corner(sc, track, marker, TRACK_AREA_SEARCH, co, 1, width, height))
						customdata = create_slide_marker_data(sc, track, marker, event, TRACK_AREA_SEARCH,
						                                      SLIDE_ACTION_OFFSET, width, height);
					else if (mouse_on_corner(sc, track, marker, TRACK_AREA_SEARCH, co, 0, width, height))
						customdata = create_slide_marker_data(sc, track, marker, event, TRACK_AREA_SEARCH,
						                                      SLIDE_ACTION_SIZE, width, height);
				}

				if (!customdata && (sc->flag & SC_SHOW_MARKER_PATTERN)) {
					if (mouse_on_corner(sc, track, marker, TRACK_AREA_PAT, co, 1,  width, height))
						customdata = create_slide_marker_data(sc, track, marker, event, TRACK_AREA_PAT,
                                                              SLIDE_ACTION_OFFSET, width, height);

					if (!customdata && mouse_on_corner(sc, track, marker, TRACK_AREA_PAT, co, 0, width, height))
						customdata = create_slide_marker_data(sc, track, marker, event, TRACK_AREA_PAT,
						                                      SLIDE_ACTION_SIZE, width, height);
				}

				if (customdata)
					break;
			}
		}

		track = track->next;
	}

	return customdata;
}

static int slide_marker_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	SlideMarkerData *slidedata = slide_marker_customdata(C, event);

	if (slidedata) {
		SpaceClip *sc = CTX_wm_space_clip(C);
		MovieClip *clip = ED_space_clip(sc);
		MovieTracking *tracking = &clip->tracking;

		tracking->act_track = slidedata->track;

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
	/* cancel sliding */
	if (data->area == TRACK_AREA_POINT) {
		if (data->action == SLIDE_ACTION_OFFSET)
			copy_v2_v2(data->offset, data->soff);
		else
			copy_v2_v2(data->pos, data->spos);
	}
	else {
		if (data->action == SLIDE_ACTION_SIZE) {
			copy_v2_v2(data->min, data->smin);
			copy_v2_v2(data->max, data->smax);
		}
		else {
			int a;

			for (a = 0; a < data->track->markersnr; a++)
				copy_v2_v2(data->track->markers[a].pos, data->smarkers[a]);

			copy_v2_v2(data->offset, data->soff);
		}
	}
}

static void free_slide_data(SlideMarkerData *data)
{
	if (data->smarkers)
		MEM_freeN(data->smarkers);

	MEM_freeN(data);
}

static int slide_marker_modal(bContext *C, wmOperator *op, wmEvent *event)
{
	SpaceClip *sc = CTX_wm_space_clip(C);
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

			/* no break! update area size */

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
					data->offset[0] = data->soff[0] + dx;
					data->offset[1] = data->soff[1] + dy;
				}
				else {
					data->pos[0] = data->spos[0] + dx;
					data->pos[1] = data->spos[1] + dy;

					data->marker->flag &= ~MARKER_TRACKED;
				}

				WM_event_add_notifier(C, NC_OBJECT | ND_TRANSFORM, NULL);
				DAG_id_tag_update(&sc->clip->id, 0);
			}
			else {
				if (data->action == SLIDE_ACTION_SIZE) {
					data->min[0] = data->smin[0] - dx;
					data->max[0] = data->smax[0] + dx;

					data->min[1] = data->smin[1] + dy;
					data->max[1] = data->smax[1] - dy;

					if (data->area == TRACK_AREA_SEARCH)
						BKE_tracking_clamp_track(data->track, CLAMP_SEARCH_DIM);
					else
						BKE_tracking_clamp_track(data->track, CLAMP_PAT_DIM);
				}
				else {
					float d[2] = {dx, dy};

					if (data->area == TRACK_AREA_SEARCH) {
						add_v2_v2v2(data->min, data->smin, d);
						add_v2_v2v2(data->max, data->smax, d);
					}
					else {
						int a;

						for (a = 0; a < data->track->markersnr; a++)
							add_v2_v2v2(data->track->markers[a].pos, data->smarkers[a], d);

						sub_v2_v2v2(data->offset, data->soff, d);
					}

					if (data->area == TRACK_AREA_SEARCH)
						BKE_tracking_clamp_track(data->track, CLAMP_SEARCH_POS);
				}
			}

			WM_event_add_notifier(C, NC_MOVIECLIP | NA_EDITED, NULL);

			break;

		case LEFTMOUSE:
			if (event->val == KM_RELEASE) {
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

static int track_mouse_area(SpaceClip *sc, float co[2], MovieTrackingTrack *track)
{
	int framenr = ED_space_clip_clip_framenr(sc);
	MovieTrackingMarker *marker = BKE_tracking_get_marker(track, framenr);
	float epsx, epsy;
	int width, height;

	ED_space_clip_size(sc, &width, &height);

	epsx = MIN4(track->pat_min[0] - track->search_min[0], track->search_max[0] - track->pat_max[0],
	           fabsf(track->pat_min[0]), fabsf(track->pat_max[0])) / 2;
	epsy = MIN4(track->pat_min[1] - track->search_min[1], track->search_max[1] - track->pat_max[1],
	           fabsf(track->pat_min[1]), fabsf(track->pat_max[1])) / 2;

	epsx = MAX2(epsx, 2.0f / width);
	epsy = MAX2(epsy, 2.0f / height);

	if (sc->flag & SC_SHOW_MARKER_SEARCH) {
		if (mouse_on_rect(co, marker->pos, track->search_min, track->search_max, epsx, epsy))
			return TRACK_AREA_SEARCH;
	}

	if ((marker->flag & MARKER_DISABLED) == 0) {
		if (sc->flag & SC_SHOW_MARKER_PATTERN)
			if (mouse_on_rect(co, marker->pos, track->pat_min, track->pat_max, epsx, epsy))
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

	d1 = dist_to_line_segment_v2(p, v1, v2);
	d2 = dist_to_line_segment_v2(p, v2, v3);
	d3 = dist_to_line_segment_v2(p, v3, v4);
	d4 = dist_to_line_segment_v2(p, v4, v1);

	return MIN4(d1, d2, d3, d4);
}

static MovieTrackingTrack *find_nearest_track(SpaceClip *sc, ListBase *tracksbase, float co[2])
{
	MovieTrackingTrack *track = NULL, *cur;
	float mindist = 0.0f;
	int framenr = ED_space_clip_clip_framenr(sc);

	cur = tracksbase->first;
	while (cur) {
		MovieTrackingMarker *marker = BKE_tracking_get_marker(cur, framenr);

		if (((cur->flag & TRACK_HIDDEN) == 0) && MARKER_VISIBLE(sc, cur, marker)) {
			float dist, d1, d2 = FLT_MAX, d3 = FLT_MAX;

			/* distance to marker point */
			d1 = sqrtf((co[0] - marker->pos[0] - cur->offset[0]) * (co[0] - marker->pos[0] - cur->offset[0]) +
					  (co[1] - marker->pos[1] - cur->offset[1]) * (co[1] - marker->pos[1] - cur->offset[1]));

			/* distance to pattern boundbox */
			if (sc->flag & SC_SHOW_MARKER_PATTERN)
				d2 = dist_to_rect(co, marker->pos, cur->pat_min, cur->pat_max);

			/* distance to search boundbox */
			if (sc->flag & SC_SHOW_MARKER_SEARCH && TRACK_VIEW_SELECTED(sc, cur))
				d3 = dist_to_rect(co, marker->pos, cur->search_min, cur->search_max);

			/* choose minimal distance. useful for cases of overlapped markers. */
			dist = MIN3(d1, d2, d3);

			if (track == NULL || dist < mindist) {
				track = cur;
				mindist = dist;
			}
		}

		cur = cur->next;
	}

	return track;
}

static int mouse_select(bContext *C, float co[2], int extend)
{
	SpaceClip *sc = CTX_wm_space_clip(C);
	MovieClip *clip = ED_space_clip(sc);
	MovieTracking *tracking = &clip->tracking;
	ListBase *tracksbase = BKE_tracking_get_tracks(tracking);
	MovieTrackingTrack *act_track = BKE_tracking_active_track(tracking);
	MovieTrackingTrack *track = NULL;	/* selected marker */

	track = find_nearest_track(sc, tracksbase, co);

	if (track) {
		int area = track_mouse_area(sc, co, track);

		if (!extend || !TRACK_VIEW_SELECTED(sc, track))
			area = TRACK_AREA_ALL;

		if (extend && TRACK_AREA_SELECTED(track, area)) {
			if (track == act_track)
				BKE_tracking_deselect_track(track, area);
			else
				clip->tracking.act_track = track;
		}
		else {
			if (area == TRACK_AREA_POINT)
				area = TRACK_AREA_ALL;

			BKE_tracking_select_track(tracksbase, track, area, extend);
			clip->tracking.act_track = track;
		}
	}

	if (!extend) {
		sc->xlockof = 0.0f;
		sc->ylockof = 0.0f;
	}

	BKE_tracking_dopesheet_tag_update(tracking);

	WM_event_add_notifier(C, NC_GEOM | ND_SELECT, NULL);

	return OPERATOR_FINISHED;
}

static int select_exec(bContext *C, wmOperator *op)
{
	float co[2];
	int extend;

	RNA_float_get_array(op->ptr, "location", co);
	extend = RNA_boolean_get(op->ptr, "extend");

	return mouse_select(C, co, extend);
}

static int select_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	float co[2];
	int extend = RNA_boolean_get(op->ptr, "extend");

	if (!extend) {
		SlideMarkerData *slidedata = slide_marker_customdata(C, event);

		if (slidedata) {
			SpaceClip *sc = CTX_wm_space_clip(C);
			MovieClip *clip = ED_space_clip(sc);

			clip->tracking.act_track = slidedata->track;

			WM_event_add_notifier(C, NC_GEOM | ND_SELECT, NULL);

			MEM_freeN(slidedata);

			return OPERATOR_PASS_THROUGH;
		}
	}

	ED_clip_mouse_pos(C, event, co);
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
	ot->poll = ED_space_clip_tracking_poll;

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
	MovieClip *clip = ED_space_clip(sc);
	MovieTracking *tracking = &clip->tracking;
	MovieTrackingTrack *track;
	ListBase *tracksbase = BKE_tracking_get_tracks(tracking);
	rcti rect;
	rctf rectf;
	int change = FALSE, mode, extend;
	int framenr = ED_space_clip_clip_framenr(sc);

	/* get rectangle from operator */
	rect.xmin = RNA_int_get(op->ptr, "xmin");
	rect.ymin = RNA_int_get(op->ptr, "ymin");
	rect.xmax = RNA_int_get(op->ptr, "xmax");
	rect.ymax = RNA_int_get(op->ptr, "ymax");

	ED_clip_point_stable_pos(C, rect.xmin, rect.ymin, &rectf.xmin, &rectf.ymin);
	ED_clip_point_stable_pos(C, rect.xmax, rect.ymax, &rectf.xmax, &rectf.ymax);

	mode = RNA_int_get(op->ptr, "gesture_mode");
	extend = RNA_boolean_get(op->ptr, "extend");

	/* do actual selection */
	track = tracksbase->first;
	while (track) {
		if ((track->flag & TRACK_HIDDEN) == 0) {
			MovieTrackingMarker *marker = BKE_tracking_get_marker(track, framenr);

			if (MARKER_VISIBLE(sc, track, marker)) {
				if (BLI_in_rctf(&rectf, marker->pos[0], marker->pos[1])) {
					BKE_tracking_track_flag(track, TRACK_AREA_ALL, SELECT, mode != GESTURE_MODAL_SELECT);
				}
				else if (!extend) {
					BKE_tracking_track_flag(track, TRACK_AREA_ALL, SELECT, 1);
				}

				change = TRUE;
			}
		}

		track = track->next;
	}

	if (change) {
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


static int do_lasso_select_marker(bContext *C, int mcords[][2], short moves, short select)
{
	ARegion *ar = CTX_wm_region(C);
	SpaceClip *sc = CTX_wm_space_clip(C);
	MovieClip *clip = ED_space_clip(sc);
	MovieTracking *tracking = &clip->tracking;
	MovieTrackingTrack *track;
	ListBase *tracksbase = BKE_tracking_get_tracks(tracking);
	rcti rect;
	int change = FALSE;
	int framenr = ED_space_clip_clip_framenr(sc);

	/* get rectangle from operator */
	BLI_lasso_boundbox(&rect, mcords, moves);

	/* do actual selection */
	track = tracksbase->first;
	while (track) {
		if ((track->flag & TRACK_HIDDEN) == 0) {
			MovieTrackingMarker *marker = BKE_tracking_get_marker(track, framenr);

			if (MARKER_VISIBLE(sc, track, marker)) {
				float screen_co[2];

				/* marker in screen coords */
				ED_clip_point_stable_pos__reverse(sc, ar, marker->pos, screen_co);

				if (BLI_in_rcti(&rect, screen_co[0], screen_co[1]) &&
				    BLI_lasso_is_point_inside(mcords, moves, screen_co[0], screen_co[1], V2D_IS_CLIPPED))
				{
					BKE_tracking_track_flag(track, TRACK_AREA_ALL, SELECT, !select);
				}

				change = TRUE;
			}
		}

		track = track->next;
	}

	if (change) {
		BKE_tracking_dopesheet_tag_update(tracking);

		WM_event_add_notifier(C, NC_GEOM | ND_SELECT, NULL);
	}

	return change;
}

static int clip_lasso_select_exec(bContext *C, wmOperator *op)
{
	int mcords_tot;
	int (*mcords)[2] = WM_gesture_lasso_path_to_array(C, op, &mcords_tot);

	if (mcords) {
		short select;

		select = !RNA_boolean_get(op->ptr, "deselect");
		do_lasso_select_marker(C, mcords, mcords_tot, select);

		MEM_freeN(mcords);

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

static int marker_inside_ellipse(MovieTrackingMarker *marker, float offset[2], float ellipse[2])
{
	/* normalized ellipse: ell[0] = scaleX, ell[1] = scaleY */
	float x, y;

	x = (marker->pos[0] - offset[0])*ellipse[0];
	y = (marker->pos[1] - offset[1])*ellipse[1];

	return x*x + y*y < 1.0f;
}

static int circle_select_exec(bContext *C, wmOperator *op)
{
	SpaceClip *sc = CTX_wm_space_clip(C);
	MovieClip *clip = ED_space_clip(sc);
	ARegion *ar = CTX_wm_region(C);
	MovieTracking *tracking = &clip->tracking;
	MovieTrackingTrack *track;
	ListBase *tracksbase = BKE_tracking_get_tracks(tracking);
	int x, y, radius, width, height, mode, change = FALSE;
	float zoomx, zoomy, offset[2], ellipse[2];
	int framenr = ED_space_clip_clip_framenr(sc);

	/* get operator properties */
	x = RNA_int_get(op->ptr, "x");
	y = RNA_int_get(op->ptr, "y");
	radius = RNA_int_get(op->ptr, "radius");

	mode = RNA_int_get(op->ptr, "gesture_mode");

	/* compute ellipse and position in unified coordinates */
	ED_space_clip_size(sc, &width, &height);
	ED_space_clip_zoom(sc, ar, &zoomx, &zoomy);

	ellipse[0] = width * zoomx / radius;
	ellipse[1] = height * zoomy / radius;

	ED_clip_point_stable_pos(C, x, y, &offset[0], &offset[1]);

	/* do selection */
	track = tracksbase->first;
	while (track) {
		if ((track->flag & TRACK_HIDDEN) == 0) {
			MovieTrackingMarker *marker = BKE_tracking_get_marker(track, framenr);

			if (MARKER_VISIBLE(sc, track, marker) && marker_inside_ellipse(marker, offset, ellipse)) {
				BKE_tracking_track_flag(track, TRACK_AREA_ALL, SELECT, mode != GESTURE_MODAL_SELECT);

				change = TRUE;
			}
		}

		track = track->next;
	}

	if (change) {
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
	MovieClip *clip = ED_space_clip(sc);
	MovieTracking *tracking = &clip->tracking;
	MovieTrackingTrack *track = NULL;	/* selected track */
	MovieTrackingMarker *marker;
	ListBase *tracksbase = BKE_tracking_get_tracks(tracking);
	int action = RNA_enum_get(op->ptr, "action");
	int framenr = ED_space_clip_clip_framenr(sc);
	int has_selection = FALSE;

	if (action == SEL_TOGGLE) {
		action = SEL_SELECT;
		track = tracksbase->first;
		while (track) {
			if (TRACK_VIEW_SELECTED(sc, track)) {
				marker = BKE_tracking_get_marker(track, framenr);

				if (MARKER_VISIBLE(sc, track, marker)) {
					action = SEL_DESELECT;
					break;
				}
			}

			track = track->next;
		}
	}

	track = tracksbase->first;
	while (track) {
		if ((track->flag & TRACK_HIDDEN) == 0) {
			marker = BKE_tracking_get_marker(track, framenr);

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
			has_selection = TRUE;

		track = track->next;
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
	MovieClip *clip = ED_space_clip(sc);
	MovieTrackingTrack *track;
	MovieTrackingMarker *marker;
	MovieTracking *tracking = &clip->tracking;
	ListBase *tracksbase = BKE_tracking_get_tracks(tracking);
	int group = RNA_enum_get(op->ptr, "group");
	int framenr = ED_space_clip_clip_framenr(sc);

	track = tracksbase->first;
	while (track) {
		int ok = FALSE;

		marker = BKE_tracking_get_marker(track, framenr);

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
			MovieTrackingTrack *act_track = BKE_tracking_active_track(tracking);

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

/********************** track operator *********************/

typedef struct TrackMarkersJob {
	struct MovieTrackingContext *context;	/* tracking context */
	int sfra, efra, lastfra;	/* Start, end and recently tracked frames */
	int backwards;				/* Backwards tracking flag */
	MovieClip *clip;			/* Clip which is tracking */
	float delay;				/* Delay in milliseconds to allow tracking at fixed FPS */

	struct Main *main;
	struct Scene *scene;
	struct bScreen *screen;
} TrackMarkersJob;

static int track_markers_testbreak(void)
{
	return G.afbreek;
}

static int track_count_markers(SpaceClip *sc, MovieClip *clip)
{
	int tot = 0;
	ListBase *tracksbase = BKE_tracking_get_tracks(&clip->tracking);
	MovieTrackingTrack *track;
	int framenr = ED_space_clip_clip_framenr(sc);

	track = tracksbase->first;
	while (track) {
		if (TRACK_VIEW_SELECTED(sc, track) && (track->flag & TRACK_LOCKED) == 0) {
			MovieTrackingMarker *marker = BKE_tracking_get_marker(track, framenr);

			if (!marker || (marker->flag & MARKER_DISABLED) == 0)
				tot++;
		}

		track = track->next;
	}

	return tot;
}

static void clear_invisible_track_selection(SpaceClip *sc, MovieClip *clip)
{
	ListBase *tracksbase = BKE_tracking_get_tracks(&clip->tracking);
	int hidden = 0;

	if ((sc->flag & SC_SHOW_MARKER_PATTERN) == 0)
		hidden |= TRACK_AREA_PAT;

	if ((sc->flag & SC_SHOW_MARKER_SEARCH) == 0)
		hidden |= TRACK_AREA_SEARCH;

	if (hidden) {
		MovieTrackingTrack *track = tracksbase->first;

		while (track) {
			if ((track->flag & TRACK_HIDDEN) == 0)
				BKE_tracking_track_flag(track, hidden, SELECT, 1);

			track = track->next;
		}
	}
}

static void track_init_markers(SpaceClip *sc, MovieClip *clip, int *frames_limit_r)
{
	ListBase *tracksbase = BKE_tracking_get_tracks(&clip->tracking);
	MovieTrackingTrack *track;
	int framenr = ED_space_clip_clip_framenr(sc);
	int frames_limit = 0;

	clear_invisible_track_selection(sc, clip);

	track = tracksbase->first;
	while (track) {
		if (TRACK_VIEW_SELECTED(sc, track)) {
			if ((track->flag & TRACK_HIDDEN) == 0 && (track->flag & TRACK_LOCKED) == 0) {
				BKE_tracking_ensure_marker(track, framenr);

				if (track->frames_limit) {
					if (frames_limit == 0)
						frames_limit = track->frames_limit;
					else
						frames_limit = MIN2(frames_limit, track->frames_limit);
				}
			}
		}

		track = track->next;
	}

	*frames_limit_r = frames_limit;
}

static int track_markers_check_direction(int backwards, int curfra, int efra)
{
	if (backwards) {
		if (curfra < efra)
			return FALSE;
	}
	else {
		if (curfra > efra)
			return FALSE;
	}

	return TRUE;
}

static int track_markers_initjob(bContext *C, TrackMarkersJob *tmj, int backwards)
{
	SpaceClip *sc = CTX_wm_space_clip(C);
	MovieClip *clip = ED_space_clip(sc);
	Scene *scene = CTX_data_scene(C);
	MovieTrackingSettings *settings = &clip->tracking.settings;
	int frames_limit;

	track_init_markers(sc, clip, &frames_limit);

	tmj->sfra = ED_space_clip_clip_framenr(sc);
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
	//double t = PIL_check_seconds_timer();

	while (framenr != tmj->efra) {
		if (tmj->delay > 0) {
			/* tracking should happen with fixed fps. Calculate time
			 * using current timer value before tracking frame and after.
			 *
			 * Small (and maybe unneeded optimization): do not calculate exec_time
			 * for "Fastest" tracking */

			double start_time = PIL_check_seconds_timer(), exec_time;

			if (!BKE_tracking_next(tmj->context))
				break;

			exec_time = PIL_check_seconds_timer() - start_time;
			if (tmj->delay > (float)exec_time)
				PIL_sleep_ms(tmj->delay - (float)exec_time);
		}
		else if (!BKE_tracking_next(tmj->context))
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

	//printf("Tracking time: %lf\n", PIL_check_seconds_timer()-t);
}

static void track_markers_updatejob(void *tmv)
{
	TrackMarkersJob *tmj = (TrackMarkersJob *)tmv;

	BKE_tracking_sync(tmj->context);
}

static void track_markers_freejob(void *tmv)
{
	TrackMarkersJob *tmj = (TrackMarkersJob *)tmv;

	tmj->clip->tracking_context = NULL;
	tmj->scene->r.cfra = BKE_movieclip_remap_clip_to_scene_frame(tmj->clip, tmj->lastfra);
	ED_update_for_newframe(tmj->main, tmj->scene, 0);

	BKE_tracking_sync(tmj->context);
	BKE_tracking_context_free(tmj->context);

	MEM_freeN(tmj);

	WM_main_add_notifier(NC_SCENE | ND_FRAME, tmj->scene);
}

static int track_markers_exec(bContext *C, wmOperator *op)
{
	SpaceClip *sc = CTX_wm_space_clip(C);
	MovieClip *clip = ED_space_clip(sc);
	Scene *scene = CTX_data_scene(C);
	struct MovieTrackingContext *context;
	int framenr = ED_space_clip_clip_framenr(sc);
	int sfra = framenr, efra;
	int backwards = RNA_boolean_get(op->ptr, "backwards");
	int sequence = RNA_boolean_get(op->ptr, "sequence");
	int frames_limit;

	if (track_count_markers(sc, clip) == 0)
		return OPERATOR_CANCELLED;

	track_init_markers(sc, clip, &frames_limit);

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
	context = BKE_tracking_context_new(clip, &sc->user, backwards, sequence);

	while (framenr != efra) {
		if (!BKE_tracking_next(context))
			break;

		if (backwards) framenr--;
		else framenr++;

		if (!sequence)
			break;
	}

	BKE_tracking_sync(context);
	BKE_tracking_context_free(context);

	/* update scene current frame to the lastes tracked frame */
	scene->r.cfra = BKE_movieclip_remap_clip_to_scene_frame(clip, framenr);

	WM_event_add_notifier(C, NC_MOVIECLIP | NA_EVALUATED, clip);
	WM_event_add_notifier(C, NC_SCENE | ND_FRAME, scene);

	return OPERATOR_FINISHED;
}

static int track_markers_invoke(bContext *C, wmOperator *op, wmEvent *UNUSED(event))
{
	TrackMarkersJob *tmj;
	ScrArea *sa = CTX_wm_area(C);
	SpaceClip *sc = CTX_wm_space_clip(C);
	MovieClip *clip = ED_space_clip(sc);
	wmJob *steve;
	int backwards = RNA_boolean_get(op->ptr, "backwards");
	int sequence = RNA_boolean_get(op->ptr, "sequence");

	if (WM_jobs_test(CTX_wm_manager(C), CTX_wm_area(C))) {
		/* only one tracking is allowed at a time */
		return OPERATOR_CANCELLED;
	}

	if (clip->tracking_context)
		return OPERATOR_CANCELLED;

	if (track_count_markers(sc, clip) == 0)
		return OPERATOR_CANCELLED;

	if (!sequence)
		return track_markers_exec(C, op);

	tmj = MEM_callocN(sizeof(TrackMarkersJob), "TrackMarkersJob data");
	if (!track_markers_initjob(C, tmj, backwards)) {
		track_markers_freejob(tmj);

		return OPERATOR_CANCELLED;
	}

	/* setup job */
	steve = WM_jobs_get(CTX_wm_manager(C), CTX_wm_window(C), sa, "Track Markers", WM_JOB_PROGRESS);
	WM_jobs_customdata(steve, tmj, track_markers_freejob);

	/* if there's delay set in tracking job, tracking should happen
	 * with fixed FPS. To deal with editor refresh we have to synchronize
	 * tracks from job and tracks in clip. Do this in timer callback
	 * to prevent threading conflicts. */
	if (tmj->delay > 0)
		WM_jobs_timer(steve, tmj->delay / 1000.0f, NC_MOVIECLIP | NA_EVALUATED, 0);
	else
		WM_jobs_timer(steve, 0.2, NC_MOVIECLIP | NA_EVALUATED, 0);

	WM_jobs_callbacks(steve, track_markers_startjob, NULL, track_markers_updatejob, NULL);

	G.afbreek = 0;

	WM_jobs_start(CTX_wm_manager(C), steve);
	WM_cursor_wait(0);

	/* add modal handler for ESC */
	WM_event_add_modal_handler(C, op);

	return OPERATOR_RUNNING_MODAL;
}

static int track_markers_modal(bContext *C, wmOperator *UNUSED(op), wmEvent *event)
{
	/* no running tracking, remove handler and pass through */
	if (0 == WM_jobs_test(CTX_wm_manager(C), CTX_wm_area(C)))
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
	/* identifiers */
	ot->name = "Track Markers";
	ot->description = "Track selected markers";
	ot->idname = "CLIP_OT_track_markers";

	/* api callbacks */
	ot->exec = track_markers_exec;
	ot->invoke = track_markers_invoke;
	ot->poll = ED_space_clip_tracking_poll;
	ot->modal = track_markers_modal;

	/* flags */
	ot->flag = OPTYPE_UNDO;

	/* properties */
	RNA_def_boolean(ot->srna, "backwards", 0, "Backwards", "Do backwards tracking");
	RNA_def_boolean(ot->srna, "sequence", 0, "Track Sequence", "Track marker during image sequence rather than single image");
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
	MovieClip *clip = ED_space_clip(sc);
	Scene *scene = CTX_data_scene(C);
	MovieTracking *tracking = &clip->tracking;
	MovieTrackingSettings *settings = &clip->tracking.settings;
	MovieTrackingObject *object = BKE_tracking_active_object(tracking);
	int width, height;

	if (!BKE_tracking_can_reconstruct(tracking, object, error_msg, max_error))
		return 0;

	/* could fail if footage uses images with different sizes */
	BKE_movieclip_get_size(clip, &sc->user, &width, &height);

	scj->clip = clip;
	scj->scene = scene;
	scj->reports = op->reports;
	scj->user = sc->user;

	scj->context = BKE_tracking_reconstruction_context_new(tracking, object,
			settings->keyframe1, settings->keyframe2, width, height);

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

	BKE_tracking_solve_reconstruction(scj->context, stop, do_update, progress,
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

	solved = BKE_tracking_finish_reconstruction(scj->context, tracking);

	if (!solved)
		BKE_report(scj->reports, RPT_WARNING, "Some data failed to reconstruct, see console for details");
	else
		BKE_reportf(scj->reports, RPT_INFO, "Average re-projection error %.3f", tracking->reconstruction.error);

	/* set currently solved clip as active for scene */
	if (scene->clip)
		id_us_min(&clip->id);

	scene->clip = clip;
	id_us_plus(&clip->id);

	/* set blender camera focal length so result would look fine there */
	if (scene->camera) {
		Camera *camera = (Camera*)scene->camera->data;
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

static int solve_camera_invoke(bContext *C, wmOperator *op, wmEvent *UNUSED(event))
{
	SolveCameraJob *scj;
	ScrArea *sa = CTX_wm_area(C);
	SpaceClip *sc = CTX_wm_space_clip(C);
	MovieClip *clip = ED_space_clip(sc);
	MovieTracking *tracking = &clip->tracking;
	MovieTrackingReconstruction *reconstruction = BKE_tracking_get_reconstruction(tracking);
	wmJob *steve;
	char error_msg[256] = "\0";

	if (WM_jobs_test(CTX_wm_manager(C), CTX_wm_area(C))) {
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
	steve = WM_jobs_get(CTX_wm_manager(C), CTX_wm_window(C), sa, "Solve Camera", WM_JOB_PROGRESS);
	WM_jobs_customdata(steve, scj, solve_camera_freejob);
	WM_jobs_timer(steve, 0.1, NC_MOVIECLIP | NA_EVALUATED, 0);
	WM_jobs_callbacks(steve, solve_camera_startjob, NULL, solve_camera_updatejob, NULL);

	G.afbreek = 0;

	WM_jobs_start(CTX_wm_manager(C), steve);
	WM_cursor_wait(0);

	/* add modal handler for ESC */
	WM_event_add_modal_handler(C, op);

	return OPERATOR_RUNNING_MODAL;
}

static int solve_camera_modal(bContext *C, wmOperator *UNUSED(op), wmEvent *event)
{
	/* no running solver, remove handler and pass through */
	if (0 == WM_jobs_test(CTX_wm_manager(C), CTX_wm_area(C)))
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
	MovieClip *clip = ED_space_clip(sc);
	MovieTracking *tracking = &clip->tracking;
	ListBase *tracksbase = BKE_tracking_get_tracks(&clip->tracking);
	MovieTrackingReconstruction *reconstruction = BKE_tracking_get_reconstruction(tracking);
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
	MovieClip *clip = ED_space_clip(sc);
	MovieTrackingTrack *track;
	ListBase *tracksbase = BKE_tracking_get_tracks(&clip->tracking);
	int action = RNA_enum_get(op->ptr, "action");
	int clear_active = RNA_boolean_get(op->ptr, "clear_active");
	int framenr = ED_space_clip_clip_framenr(sc);

	if (clear_active) {
		track = BKE_tracking_active_track(&clip->tracking);
		BKE_tracking_clear_path(track, framenr, action);
	}
	else {
		track = tracksbase->first;
		while (track) {
			if (TRACK_VIEW_SELECTED(sc, track))
				BKE_tracking_clear_path(track, framenr, action);

			track = track->next;
		}
	}

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
	MovieClip *clip = ED_space_clip(sc);
	MovieTracking *tracking = &clip->tracking;
	ListBase *tracksbase = BKE_tracking_get_tracks(tracking);
	MovieTrackingTrack *track = tracksbase->first;
	int action = RNA_enum_get(op->ptr, "action");
	int framenr = ED_space_clip_clip_framenr(sc);

	while (track) {
		if (TRACK_VIEW_SELECTED(sc, track) && (track->flag & TRACK_LOCKED) == 0) {
			MovieTrackingMarker *marker = BKE_tracking_ensure_marker(track, framenr);

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

	if (camera && BKE_object_movieclip_get(scene, camera, 0) == clip)
		return camera;

	base = scene->base.first;
	while (base) {
		if (base->object->type == OB_CAMERA) {
			if (BKE_object_movieclip_get(scene, base->object, 0) == clip) {
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
	MovieClip *clip = ED_space_clip(sc);
	MovieTracking *tracking = &clip->tracking;
	MovieTrackingObject *tracking_object = BKE_tracking_active_object(tracking);
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
		MovieClip *clip = ED_space_clip(sc);
		MovieTracking *tracking = &clip->tracking;
		MovieTrackingObject *tracking_object = BKE_tracking_active_object(tracking);

		if (tracking_object->flag & TRACKING_OBJECT_CAMERA) {
			return TRUE;
		}
		else {
			return OBACT != NULL;
		}
	}

	return FALSE;
}

static int count_selected_bundles(bContext *C)
{
	SpaceClip *sc = CTX_wm_space_clip(C);
	MovieClip *clip = ED_space_clip(sc);
	ListBase *tracksbase = BKE_tracking_get_tracks(&clip->tracking);
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
	int found = FALSE;

	for (con = ob->constraints.first; con; con = con->next) {
		bConstraintTypeInfo *cti = constraint_get_typeinfo(con);

		if (!cti)
			continue;

		if (cti->type == CONSTRAINT_TYPE_OBJECTSOLVER) {
			bObjectSolverConstraint *data = (bObjectSolverConstraint *)con->data;

			if (!found) {
				Object *cam = data->camera ? data->camera : scene->camera;

				BKE_object_where_is_calc_mat4(scene, cam, invmat);
			}

			mult_m4_m4m4(invmat, invmat, data->invmat);

			found = TRUE;
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
		bConstraintTypeInfo *cti = constraint_get_typeinfo(con);

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
	MovieClip *clip = ED_space_clip(sc);
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
		BKE_report(op->reports, RPT_ERROR, "At least one track with bundle should be selected to define origin position");

		return OPERATOR_CANCELLED;
	}

	object = get_orientation_object(C);
	if (!object) {
		BKE_report(op->reports, RPT_ERROR, "No object to apply orientation on");

		return OPERATOR_CANCELLED;
	}

	tracking_object = BKE_tracking_active_object(tracking);

	tracksbase = BKE_tracking_object_tracks(tracking, tracking_object);

	track = tracksbase->first;
	zero_v3(median);
	while (track) {
		if (TRACK_VIEW_SELECTED(sc, track) && (track->flag & TRACK_HAS_BUNDLE)) {
			add_v3_v3(median, track->bundle_pos);
		}

		track = track->next;
	}
	mul_v3_fl(median, 1.0f / selected_count);

	BKE_get_tracking_mat(scene, camera, mat);

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
	ot->description = "Set active marker as origin by moving camera (or it's parent if present) in 3D space";
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
	int is_camera = tracking_object->flag & TRACKING_OBJECT_CAMERA;
	int  flip = FALSE;
	float mat[4][4], vec[3], obmat[4][4], dvec[3];

	BKE_object_to_mat4(ob, obmat);

	BKE_get_tracking_mat(scene, camera, mat);
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

	if (len_v2(vec) < 1e-3f)
		return;

	unit_m4(mat);

	if (axis == 'X') {
		if (fabsf(dvec[1]) < 1e-3f) {
			flip = TRUE;

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
			flip = TRUE;

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

		mult_m4_m4m4(mat, mat, obmat);
	}
	else {
		if (!flip) {
			float lmat[4][4], ilmat[4][4], rmat[3][3];

			BKE_object_rot_to_mat3(ob, rmat);
			invert_m3(rmat);
			mul_m4_m4m3(mat, mat, rmat);

			unit_m4(lmat);
			copy_v3_v3(lmat[3], obmat[3]);
			invert_m4_m4(ilmat, lmat);

			mul_serie_m4(mat, lmat, mat, ilmat, obmat, NULL, NULL, NULL, NULL);
		}
		else {
			mult_m4_m4m4(mat, obmat, mat);
		}
	}

	BKE_object_apply_mat4(ob, mat, 0, 0);
}

static int set_plane_exec(bContext *C, wmOperator *op)
{
	SpaceClip *sc = CTX_wm_space_clip(C);
	MovieClip *clip = ED_space_clip(sc);
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
	                   {0.0f, 0.0f, 0.0f, 1.0f}};	/* 90 degrees Y-axis rotation matrix */

	if (count_selected_bundles(C) != 3) {
		BKE_report(op->reports, RPT_ERROR, "Three tracks with bundles are needed to orient the floor");

		return OPERATOR_CANCELLED;
	}

	tracking_object = BKE_tracking_active_object(tracking);
	tracksbase = BKE_tracking_object_tracks(tracking, tracking_object);
	act_track = BKE_tracking_active_track(tracking);

	object = get_orientation_object(C);
	if (!object) {
		BKE_report(op->reports, RPT_ERROR, "No object to apply orientation on");

		return OPERATOR_CANCELLED;
	}

	BKE_get_tracking_mat(scene, camera, mat);

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
		mult_m4_m4m4(mat, mat, obmat);
		mult_m4_m4m4(newmat, rot, mat);
		BKE_object_apply_mat4(object, newmat, 0, 0);

		/* make camera have positive z-coordinate */
		if (object->loc[2] < 0) {
			invert_m4(rot);
			mult_m4_m4m4(newmat, rot, mat);
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
	ot->description = "Set plane based on 3 selected bundles by moving camera (or it's parent if present) in 3D space";
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
	MovieClip *clip = ED_space_clip(sc);
	MovieTracking *tracking = &clip->tracking;
	MovieTrackingObject *tracking_object = BKE_tracking_active_object(tracking);
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

	tracksbase = BKE_tracking_object_tracks(tracking, tracking_object);

	track = tracksbase->first;
	while (track) {
		if (TRACK_VIEW_SELECTED(sc, track))
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
	ot->description = "Set direction of scene axis rotating camera (or it's parent if present) and assuming selected track lies on real axis joining it with the origin";
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

static int do_set_scale(bContext *C, wmOperator *op, int scale_solution)
{
	SpaceClip *sc = CTX_wm_space_clip(C);
	MovieClip *clip = ED_space_clip(sc);
	MovieTracking *tracking = &clip->tracking;
	MovieTrackingObject *tracking_object = BKE_tracking_active_object(tracking);
	MovieTrackingTrack *track;
	Scene *scene = CTX_data_scene(C);
	Object *object = NULL;
	Object *camera = get_camera_with_movieclip(scene, clip);
	ListBase *tracksbase = BKE_tracking_get_tracks(tracking);
	int tot = 0;
	float vec[2][3], mat[4][4], scale;
	float dist = RNA_float_get(op->ptr, "distance");

	if (count_selected_bundles(C) != 2) {
		BKE_report(op->reports, RPT_ERROR, "Two tracks with bundles should be selected to set scale");

		return OPERATOR_CANCELLED;
	}

	object = get_orientation_object(C);
	if (!object) {
		BKE_report(op->reports, RPT_ERROR, "No object to apply orientation on");

		return OPERATOR_CANCELLED;
	}

	BKE_get_tracking_mat(scene, camera, mat);

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

	return OPERATOR_FINISHED;
}

static int set_scale_exec(bContext *C, wmOperator *op)
{
	return do_set_scale(C, op, 0);
}

static int set_scale_invoke(bContext *C, wmOperator *op, wmEvent *UNUSED(event))
{
	SpaceClip *sc = CTX_wm_space_clip(C);
	MovieClip *clip = ED_space_clip(sc);

	if (!RNA_struct_property_is_set(op->ptr, "distance"))
		RNA_float_set(op->ptr, "distance", clip->tracking.settings.dist);

	return set_scale_exec(C, op);
}

void CLIP_OT_set_scale(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Set Scale";
	ot->description = "Set scale of scene by scaling camera (or it's parent if present)";
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
		MovieClip *clip = ED_space_clip(sc);
		MovieTracking *tracking = &clip->tracking;
		MovieTrackingObject *tracking_object = BKE_tracking_active_object(tracking);

		return (tracking_object->flag & TRACKING_OBJECT_CAMERA) == 0;
	}

	return FALSE;
}

static int set_solution_scale_exec(bContext *C, wmOperator *op)
{
	return do_set_scale(C, op, 1);
}

static int set_solution_scale_invoke(bContext *C, wmOperator *op, wmEvent *UNUSED(event))
{
	SpaceClip *sc = CTX_wm_space_clip(C);
	MovieClip *clip = ED_space_clip(sc);

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

/********************** set principal center operator *********************/

static int set_center_principal_exec(bContext *C, wmOperator *UNUSED(op))
{
	SpaceClip *sc = CTX_wm_space_clip(C);
	MovieClip *clip = ED_space_clip(sc);
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
	MovieClip *clip = ED_space_clip(sc);
	MovieTrackingTrack *track;
	MovieTracking *tracking = &clip->tracking;
	ListBase *tracksbase = BKE_tracking_get_tracks(tracking);
	MovieTrackingTrack *act_track = BKE_tracking_active_track(tracking);
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
	MovieClip *clip = ED_space_clip(sc);
	MovieTracking *tracking = &clip->tracking;
	ListBase *tracksbase = BKE_tracking_get_tracks(tracking);
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
	MovieClip *clip = ED_space_clip(sc);
	int clip_flag = clip->flag & MCLIP_TIMECODE_FLAGS;
	ImBuf *ibuf = BKE_movieclip_get_ibuf_flag(clip, &sc->user, clip_flag, MOVIECLIP_CACHE_SKIP);
	MovieTracking *tracking = &clip->tracking;
	ListBase *tracksbase = BKE_tracking_get_tracks(tracking);
	MovieTrackingTrack *track = tracksbase->first;
	int placement = RNA_enum_get(op->ptr, "placement");
	int margin = RNA_int_get(op->ptr, "margin");
	int min_trackability = RNA_int_get(op->ptr, "min_trackability");
	int min_distance = RNA_int_get(op->ptr, "min_distance");
	int place_outside_layer = 0;
	int framenr = ED_space_clip_clip_framenr(sc);
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

	BKE_tracking_detect_fast(tracking, tracksbase, ibuf, framenr, margin,
				min_trackability, min_distance, layer, place_outside_layer);

	IMB_freeImBuf(ibuf);

	WM_event_add_notifier(C, NC_MOVIECLIP | NA_EDITED, NULL);

	return OPERATOR_FINISHED;
}

void CLIP_OT_detect_features(wmOperatorType *ot)
{
	static EnumPropertyItem placement_items[] = {
			{0, "FRAME",			0, "Whole Frame",			"Place markers across the whole frame"},
			{1, "INSIDE_GPENCIL",	0, "Inside grease pencil",	"Place markers only inside areas outlined with grease pencil"},
			{2, "OUTSIDE_GPENCIL",	0, "Outside grease pencil",	"Place markers only outside areas outlined with grease pencil"},
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
	RNA_def_int(ot->srna, "margin", 16, 0, INT_MAX, "Margin", "Only corners further than margin pixels from the image edges are considered", 0, 300);
	RNA_def_int(ot->srna, "min_trackability", 16, 0, INT_MAX, "Trackability", "Minimum trackability score to add a corner", 0, 300);
	RNA_def_int(ot->srna, "min_distance", 120, 0, INT_MAX, "Distance", "Minimal distance accepted between two corners", 0, 300);
}

/********************** frame jump operator *********************/

static int frame_jump_exec(bContext *C, wmOperator *op)
{
	Scene *scene = CTX_data_scene(C);
	SpaceClip *sc = CTX_wm_space_clip(C);
	MovieClip *clip = ED_space_clip(sc);
	MovieTrackingTrack *track;
	int pos = RNA_enum_get(op->ptr, "position");
	int delta;

	if (pos <= 1) {	/* jump to path */
		track = BKE_tracking_active_track(&clip->tracking);

		if (!track)
			return OPERATOR_CANCELLED;

		delta = pos == 1 ? 1 : -1;

		while (sc->user.framenr + delta >= SFRA && sc->user.framenr + delta <= EFRA) {
			int framenr = BKE_movieclip_remap_scene_to_clip_frame(clip, sc->user.framenr + delta);
			MovieTrackingMarker *marker = BKE_tracking_exact_marker(track, framenr);

			if (!marker || marker->flag & MARKER_DISABLED)
				break;

			sc->user.framenr += delta;
		}
	}
	else {	/* to to failed frame */
		if (clip->tracking.reconstruction.flag & TRACKING_RECONSTRUCTED) {
			int a = ED_space_clip_clip_framenr(sc);
			MovieTracking *tracking = &clip->tracking;
			MovieTrackingObject *object = BKE_tracking_active_object(tracking);

			delta = pos == 3 ? 1 : -1;

			a += delta;

			while (a + delta >= SFRA && a + delta <= EFRA) {
				MovieReconstructedCamera *cam;

				cam = BKE_tracking_get_reconstructed_camera(tracking, object, a);

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
			{0, "PATHSTART",	0, "Path Start",		"Jump to start of current path"},
			{1, "PATHEND",		0, "Path End",			"Jump to end of current path"},
			{2, "FAILEDPREV",	0, "Previous Failed",	"Jump to previous failed frame"},
			{2, "FAILNEXT",		0, "Next Failed",		"Jump to next failed frame"},
			{0, NULL, 0, NULL, NULL}
	};

	/* identifiers */
	ot->name = "Jump to Frame";
	ot->description = "Jump to special frame";
	ot->idname = "CLIP_OT_frame_jump";

	/* api callbacks */
	ot->exec = frame_jump_exec;
	ot->poll = ED_space_clip_tracking_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* properties */
	RNA_def_enum(ot->srna, "position", position_items, 0, "Position", "Position to jump to");
}

/********************** join tracks operator *********************/

static int join_tracks_exec(bContext *C, wmOperator *op)
{
	SpaceClip *sc = CTX_wm_space_clip(C);
	MovieClip *clip = ED_space_clip(sc);
	MovieTracking *tracking = &clip->tracking;
	ListBase *tracksbase = BKE_tracking_get_tracks(tracking);
	MovieTrackingTrack *act_track, *track, *next;

	act_track = BKE_tracking_active_track(tracking);

	if (!act_track) {
		BKE_report(op->reports, RPT_ERROR, "No active track to join to");
		return OPERATOR_CANCELLED;
	}

	track = tracksbase->first;
	while (track) {
		next = track->next;

		if (TRACK_VIEW_SELECTED(sc, track) && track != act_track) {
			BKE_tracking_join_tracks(act_track, track);

			if (tracking->stabilization.rot_track == track)
				tracking->stabilization.rot_track = act_track;

			BKE_tracking_free_track(track);
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
	MovieClip *clip = ED_space_clip(sc);
	MovieTracking *tracking = &clip->tracking;
	ListBase *tracksbase = BKE_tracking_get_tracks(tracking);
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

/********************** track copy color operator *********************/

static int track_copy_color_exec(bContext *C, wmOperator *UNUSED(op))
{
	SpaceClip *sc = CTX_wm_space_clip(C);
	MovieClip *clip = ED_space_clip(sc);
	MovieTracking *tracking = &clip->tracking;
	ListBase *tracksbase = BKE_tracking_get_tracks(tracking);
	MovieTrackingTrack *track, *act_track = BKE_tracking_active_track(tracking);

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

static int stabilize_2d_add_exec(bContext *C, wmOperator *UNUSED(op))
{
	SpaceClip *sc = CTX_wm_space_clip(C);
	MovieClip *clip = ED_space_clip(sc);
	MovieTracking *tracking = &clip->tracking;
	ListBase *tracksbase = BKE_tracking_get_tracks(tracking);
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
	ot->poll = ED_space_clip_tracking_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/********************** remove 2d stabilization tracks operator *********************/

static int stabilize_2d_remove_exec(bContext *C, wmOperator *UNUSED(op))
{
	SpaceClip *sc = CTX_wm_space_clip(C);
	MovieClip *clip = ED_space_clip(sc);
	MovieTracking *tracking = &clip->tracking;
	MovieTrackingStabilization *stab = &tracking->stabilization;
	ListBase *tracksbase = BKE_tracking_get_tracks(tracking);
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
	ot->poll = ED_space_clip_tracking_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/********************** select 2d stabilization tracks operator *********************/

static int stabilize_2d_select_exec(bContext *C, wmOperator *UNUSED(op))
{
	SpaceClip *sc = CTX_wm_space_clip(C);
	MovieClip *clip = ED_space_clip(sc);
	MovieTracking *tracking = &clip->tracking;
	ListBase *tracksbase = BKE_tracking_get_tracks(tracking);
	MovieTrackingTrack *track;
	int update = 0;

	track = tracksbase->first;
	while (track) {
		if (track->flag & TRACK_USE_2D_STAB) {
			BKE_tracking_track_flag(track, TRACK_AREA_ALL, SELECT, 0);

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
	ot->description = "Select track which are used for stabilization";
	ot->idname = "CLIP_OT_stabilize_2d_select";

	/* api callbacks */
	ot->exec = stabilize_2d_select_exec;
	ot->poll = ED_space_clip_tracking_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/********************** set 2d stabilization rotation track operator *********************/

static int stabilize_2d_set_rotation_exec(bContext *C, wmOperator *UNUSED(op))
{
	SpaceClip *sc = CTX_wm_space_clip(C);
	MovieClip *clip = ED_space_clip(sc);
	MovieTracking *tracking = &clip->tracking;
	MovieTrackingTrack *act_track = BKE_tracking_active_track(tracking);

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
	ot->poll = ED_space_clip_tracking_poll;

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
		new_markers = MEM_callocN(markersnr*sizeof(MovieTrackingMarker), "track cleaned markers");

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
	MovieClip *clip = ED_space_clip(sc);
	MovieTracking *tracking = &clip->tracking;
	ListBase *tracksbase = BKE_tracking_get_tracks(tracking);
	MovieTrackingTrack *track, *next, *act_track = BKE_tracking_active_track(tracking);
	int frames = RNA_int_get(op->ptr, "frames");
	int action = RNA_enum_get(op->ptr, "action");
	float error = RNA_float_get(op->ptr, "error");

	if (error && action == TRACKING_CLEAN_DELETE_SEGMENT)
		action = TRACKING_CLEAN_DELETE_TRACK;

	track = tracksbase->first;
	while (track) {
		next = track->next;

		if ((track->flag & TRACK_HIDDEN) == 0 && (track->flag & TRACK_LOCKED) == 0) {
			int ok = 1;

			ok = (is_track_clean(track, frames, action == TRACKING_CLEAN_DELETE_SEGMENT)) &&
			    (error == 0.0f || (track->flag & TRACK_HAS_BUNDLE) == 0  || track->error < error);

			if (!ok) {
				if (action == TRACKING_CLEAN_SELECT) {
					BKE_tracking_track_flag(track, TRACK_AREA_ALL, SELECT, 0);
				}
				else if (action == TRACKING_CLEAN_DELETE_TRACK) {
					if (track == act_track)
						clip->tracking.act_track = NULL;

					BKE_tracking_free_track(track);
					BLI_freelinkN(tracksbase, track);
					track = NULL;
				}

				/* happens when all tracking segments are not long enough */
				if (track && track->markersnr == 0) {
					if (track == act_track)
						clip->tracking.act_track = NULL;

					BKE_tracking_free_track(track);
					BLI_freelinkN(tracksbase, track);
				}
			}
		}

		track = next;
	}

	WM_event_add_notifier(C, NC_MOVIECLIP | ND_SELECT, clip);

	return OPERATOR_FINISHED;
}

static int clean_tracks_invoke(bContext *C, wmOperator *op, wmEvent *UNUSED(event))
{
	SpaceClip *sc = CTX_wm_space_clip(C);
	MovieClip *clip = ED_space_clip(sc);

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
	MovieClip *clip = ED_space_clip(sc);
	MovieTracking *tracking = &clip->tracking;

	BKE_tracking_new_object(tracking, "Object");

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
	MovieClip *clip = ED_space_clip(sc);
	MovieTracking *tracking = &clip->tracking;
	MovieTrackingObject *object;

	object = BKE_tracking_active_object(tracking);

	if (object->flag & TRACKING_OBJECT_CAMERA) {
		BKE_report(op->reports, RPT_WARNING, "Object used for camera tracking can't be deleted");
		return OPERATOR_CANCELLED;
	}

	BKE_tracking_remove_object(tracking, object);

	WM_event_add_notifier(C, NC_MOVIECLIP | NA_EDITED, clip);

	return OPERATOR_FINISHED;
}

void CLIP_OT_tracking_object_remove(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Movie Tracking Object";
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
	MovieClip *clip = ED_space_clip(sc);
	MovieTracking *tracking = &clip->tracking;
	MovieTrackingObject *object = BKE_tracking_active_object(tracking);

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
	MovieClip *clip = ED_space_clip(sc);
	MovieTracking *tracking = &clip->tracking;
	MovieTrackingObject *object = BKE_tracking_active_object(tracking);

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
