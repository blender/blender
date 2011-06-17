/*
 * $Id$
 *
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
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 *
 *
 * Contributor(s): Blender Foundation,
 *                 Sergey Sharybin
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include "MEM_guardedalloc.h"

#include "DNA_movieclip_types.h"
#include "DNA_object_types.h"	/* SELECT */
#include "DNA_scene_types.h"

#include "BLI_utildefines.h"
#include "BLI_math.h"
#include "BLI_listbase.h"
#include "BLI_rect.h"
#include "BLI_blenlib.h"

#include "BKE_context.h"
#include "BKE_movieclip.h"
#include "BKE_tracking.h"
#include "BKE_global.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_screen.h"
#include "ED_clip.h"

#include "IMB_imbuf_types.h"

#include "UI_interface.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "UI_view2d.h"

#include "clip_intern.h"	// own include

/** \file blender/editors/space_clip/tracking_ops.c
 *  \ingroup spclip
 */

static int space_clip_tracking_poll(bContext *C)
{
	SpaceClip *sc= CTX_wm_space_clip(C);

	if(sc && sc->clip)
		return 1;

	return 0;
}

static int space_clip_frame_poll(bContext *C)
{
	SpaceClip *sc= CTX_wm_space_clip(C);

	if(sc) {
		MovieClip *clip= ED_space_clip(sc);

		if(clip)
			return BKE_movieclip_has_frame(clip, &sc->user);
	}

	return 0;
}

/********************** add marker operator *********************/

static void add_marker(SpaceClip *sc, float x, float y)
{
	MovieClip *clip= ED_space_clip(sc);
	MovieTrackingTrack *track;
	MovieTrackingMarker marker;
	int width, height;
	float pat[2]= {15.0f, 15.0f}, search[2]= {30.0f, 30.0f}; /* TODO: move to default setting? */

	ED_space_clip_size(sc, &width, &height);

	pat[0] /= (float)width;
	pat[1] /= (float)height;

	search[0] /= (float)width;
	search[1] /= (float)height;

	track= MEM_callocN(sizeof(MovieTrackingTrack), "add_marker_exec track");

	marker.pos[0]= x;
	marker.pos[1]= y;
	marker.framenr= sc->user.framenr;

	copy_v2_v2(track->pat_max, pat);
	negate_v2_v2(track->pat_min, pat);

	copy_v2_v2(track->search_max, search);
	negate_v2_v2(track->search_min, search);

	BKE_tracking_insert_marker(track, &marker);

	BLI_addtail(&clip->tracking.tracks, track);

	BKE_movieclip_select_track(clip, track, TRACK_AREA_ALL, 0);
	BKE_movieclip_set_selection(clip, MCLIP_SEL_TRACK, track);
}

static int add_marker_exec(bContext *C, wmOperator *op)
{
	SpaceClip *sc= CTX_wm_space_clip(C);
	MovieClip *clip= ED_space_clip(sc);
	float pos[2];
	int width, height;

	ED_space_clip_size(sc, &width, &height);
	if(!width || !height)
		return OPERATOR_CANCELLED;

	RNA_float_get_array(op->ptr, "location", pos);

	add_marker(sc, pos[0], pos[1]);

	WM_event_add_notifier(C, NC_MOVIECLIP|NA_EDITED, clip);

	return OPERATOR_FINISHED;
}

static void mouse_pos(bContext *C, wmEvent *event, float co[2])
{
	ARegion *ar= CTX_wm_region(C);
	SpaceClip *sc= CTX_wm_space_clip(C);
	int sx, sy;

	UI_view2d_to_region_no_clip(&ar->v2d, 0.0f, 0.0f, &sx, &sy);
	co[0]= ((float)event->mval[0]-sx)/sc->zoom;
	co[1]= ((float)event->mval[1]-sy)/sc->zoom;
}

static int add_marker_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	SpaceClip *sc= CTX_wm_space_clip(C);
	int width, height;
	float co[2];

	ED_space_clip_size(sc, &width, &height);
	if(!width || !height)
		return OPERATOR_CANCELLED;

	mouse_pos(C, event, co);
	co[0]/= width;
	co[1]/= height;

	RNA_float_set_array(op->ptr, "location", co);

	return add_marker_exec(C, op);
}

void CLIP_OT_add_marker(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Add Marker";
	ot->idname= "CLIP_OT_add_marker";
	ot->description= "Place new marker at specified location";

	/* api callbacks */
	ot->invoke= add_marker_invoke;
	ot->exec= add_marker_exec;
	ot->poll= space_clip_tracking_poll;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;

	/* properties */
	RNA_def_float_vector(ot->srna, "location", 2, NULL, -FLT_MIN, FLT_MAX,
		"Location", "Location of marker on frame.", -1.f, 1.f);
}

/********************** delete operator *********************/

static int delete_exec(bContext *C, wmOperator *UNUSED(op))
{
	SpaceClip *sc= CTX_wm_space_clip(C);
	MovieClip *clip= ED_space_clip(sc);
	MovieTrackingTrack *track= clip->tracking.tracks.first, *next;

	while(track) {
		next= track->next;

		if(track->flag&SELECT) {
			BKE_tracking_free_track(track);
			BLI_freelinkN(&clip->tracking.tracks, track);
		}

		track= next;
	}

	BKE_movieclip_set_selection(clip, MCLIP_SEL_NONE, NULL);
	WM_event_add_notifier(C, NC_MOVIECLIP|NA_EDITED, clip);

	return OPERATOR_FINISHED;
}

void CLIP_OT_delete(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Delete";
	ot->idname= "CLIP_OT_delete";
	ot->description= "Delete selected tracks";

	/* api callbacks */
	ot->invoke= WM_operator_confirm;
	ot->exec= delete_exec;
	ot->poll= space_clip_tracking_poll;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

/********************** mouse select operator *********************/

static int mouse_on_size(float co[2], float x1, float y1, float x2, float y2, float epsx, float epsy)
{
	if(x1>x2) SWAP(float, x1, x2);
	if(y1>y2) SWAP(float, y1, y2);

	return (co[0]>=x1-epsx && co[0]<=x2+epsx) && (co[1]>=y1-epsy && co[1]<=y2+epsy);
}

static int mouse_on_rect(float co[2], float pos[2], float min[2], float max[2], float epsx, float epsy)
{
	return mouse_on_size(co, pos[0]+min[0], pos[1]+min[1], pos[0]+max[0], pos[1]+min[1], epsx, epsy) ||
	       mouse_on_size(co, pos[0]+min[0], pos[1]+min[1], pos[0]+min[0], pos[1]+max[1], epsx, epsy) ||
	       mouse_on_size(co, pos[0]+min[0], pos[1]+max[1], pos[0]+max[0], pos[1]+max[1], epsx, epsy) ||
	       mouse_on_size(co, pos[0]+max[0], pos[1]+min[1], pos[0]+max[0], pos[1]+max[1], epsx, epsy);
}

static int track_mouse_area(SpaceClip *sc, float co[2], MovieTrackingTrack *track)
{
	MovieTrackingMarker *marker= BKE_tracking_get_marker(track, sc->user.framenr);
	float epsx, epsy;
	int width, height;

	ED_space_clip_size(sc, &width, &height);

	epsx= MIN4(track->pat_min[0]-track->search_min[0], track->search_max[0]-track->pat_max[0],
	           fabsf(track->pat_min[0]), fabsf(track->pat_max[0])) / 2;
	epsy= MIN4(track->pat_min[1]-track->search_min[1], track->search_max[1]-track->pat_max[1],
	           fabsf(track->pat_min[1]), fabsf(track->pat_max[1])) / 2;

	epsx= MAX2(epsy, 2.0 / width);
	epsy= MAX2(epsy, 2.0 / height);

	if(fabsf(co[0]-marker->pos[0])< epsx && fabsf(co[1]-marker->pos[1])<=epsy)
		return TRACK_AREA_POINT;

	if(sc->flag&SC_SHOW_MARKER_PATTERN)
		if(mouse_on_rect(co, marker->pos, track->pat_min, track->pat_max, epsx, epsy))
			return TRACK_AREA_PAT;

	if(sc->flag&SC_SHOW_MARKER_SEARCH)
		if(mouse_on_rect(co, marker->pos, track->search_min, track->search_max, epsx, epsy))
			return TRACK_AREA_SEARCH;

	return TRACK_AREA_NONE;
}

static float dist_to_rect(float co[2], float pos[2], float min[2], float max[2])
{
	float d1, d2, d3, d4;
	float p[2]= {co[0]-pos[0], co[1]-pos[1]};
	float v1[2]= {min[0], min[1]}, v2[2]= {max[0], min[1]},
	      v3[2]= {max[0], max[1]}, v4[2]= {min[0], max[1]};

	d1= dist_to_line_segment_v2(p, v1, v2);
	d2= dist_to_line_segment_v2(p, v2, v3);
	d3= dist_to_line_segment_v2(p, v3, v4);
	d4= dist_to_line_segment_v2(p, v4, v1);

	return MIN4(d1, d2, d3, d4);
}

static MovieTrackingTrack *find_nearest_track(SpaceClip *sc, MovieClip *clip, float co[2])
{
	MovieTrackingTrack *track= NULL, *cur;
	float mindist= 0.0f;

	cur= clip->tracking.tracks.first;
	while(cur) {
		MovieTrackingMarker *marker= BKE_tracking_get_marker(cur, sc->user.framenr);
		float dist, d1, d2, d3;

		if(marker) {
			d1= sqrtf((co[0]-marker->pos[0])*(co[0]-marker->pos[0])+
					  (co[1]-marker->pos[1])*(co[1]-marker->pos[1])); /* distance to marker point */
			d2= dist_to_rect(co, marker->pos, cur->pat_min, cur->pat_max); /* distance to search boundbox */
			d3= dist_to_rect(co, marker->pos, cur->search_min, cur->search_max); /* distance to search boundbox */

			/* choose minimal distance. useful for cases of overlapped markers. */
			dist= MIN3(d1, d2, d3);

			if(track==NULL || dist<mindist) {
				track= cur;
				mindist= dist;
			}
		}

		cur= cur->next;
	}

	return track;
}

static int mouse_select(bContext *C, float co[2], int extend)
{
	SpaceClip *sc= CTX_wm_space_clip(C);
	MovieClip *clip= ED_space_clip(sc);
	MovieTrackingTrack *track= NULL;	/* selected marker */

	track= find_nearest_track(sc, clip, co);

	if(track) {
		int area= track_mouse_area(sc, co, track);

		if(!extend || !TRACK_SELECTED(track))
			area= TRACK_AREA_ALL;

		if(extend && TRACK_AREA_SELECTED(track, area)) {
			BKE_movieclip_deselect_track(clip, track, area);
		} else {
			if(area==TRACK_AREA_POINT) area= TRACK_AREA_ALL;

			BKE_movieclip_select_track(clip, track, area, extend);
			BKE_movieclip_set_selection(clip, MCLIP_SEL_TRACK, track);
		}
	}

	WM_event_add_notifier(C, NC_GEOM|ND_SELECT, NULL);

	return OPERATOR_FINISHED;
}

static int select_exec(bContext *C, wmOperator *op)
{
	float co[2];
	int extend;

	RNA_float_get_array(op->ptr, "location", co);
	extend= RNA_boolean_get(op->ptr, "extend");

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

void CLIP_OT_select(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Select";
	ot->description= "Select tracking markers";
	ot->idname= "CLIP_OT_select";

	/* api callbacks */
	ot->exec= select_exec;
	ot->invoke= select_invoke;
	ot->poll= space_clip_frame_poll;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;

	/* properties */
	RNA_def_boolean(ot->srna, "extend", 0,
		"Extend", "Extend selection rather than clearing the existing selection.");
	RNA_def_float_vector(ot->srna, "location", 2, NULL, -FLT_MAX, FLT_MAX,
		"Location", "Mouse location in normalized coordinates, 0.0 to 1.0 is within the image bounds.", -100.0f, 100.0f);
}

/********************** border select operator *********************/

static int border_select_exec(bContext *C, wmOperator *op)
{
	SpaceClip *sc= CTX_wm_space_clip(C);
	MovieClip *clip= ED_space_clip(sc);
	MovieTrackingTrack *track;
	ARegion *ar= CTX_wm_region(C);
	rcti rect;
	rctf rectf;
	int change= 0, mode;

	/* get rectangle from operator */
	rect.xmin= RNA_int_get(op->ptr, "xmin");
	rect.ymin= RNA_int_get(op->ptr, "ymin");
	rect.xmax= RNA_int_get(op->ptr, "xmax");
	rect.ymax= RNA_int_get(op->ptr, "ymax");

	UI_view2d_region_to_view(&ar->v2d, rect.xmin, rect.ymin, &rectf.xmin, &rectf.ymin);
	UI_view2d_region_to_view(&ar->v2d, rect.xmax, rect.ymax, &rectf.xmax, &rectf.ymax);

	mode= RNA_int_get(op->ptr, "gesture_mode");

	/* do actual selection */
	track= clip->tracking.tracks.first;
	while(track) {
		MovieTrackingMarker *marker= BKE_tracking_get_marker(track, sc->user.framenr);

		if(marker) {
			if(BLI_in_rctf(&rectf, marker->pos[0], marker->pos[1])) {
				BKE_tracking_track_flag(track, TRACK_AREA_ALL, SELECT, mode!=GESTURE_MODAL_SELECT);

				change= 1;
			}
		}

		track= track->next;
	}

	BKE_movieclip_set_selection(clip, MCLIP_SEL_NONE, NULL);

	if(change) {
		WM_event_add_notifier(C, NC_GEOM|ND_SELECT, NULL);

		return OPERATOR_FINISHED;
	}

	return OPERATOR_CANCELLED;
}

void CLIP_OT_select_border(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Border Select";
	ot->description= "Select markers using border selection";
	ot->idname= "CLIP_OT_select_border";

	/* api callbacks */
	ot->invoke= WM_border_select_invoke;
	ot->exec= border_select_exec;
	ot->modal= WM_border_select_modal;
	ot->poll= space_clip_frame_poll;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;

	/* properties */
	WM_operator_properties_gesture_border(ot, FALSE);
}

/********************** circle select operator *********************/

static int marker_inside_ellipse(MovieTrackingMarker *marker, float offset[2], float ellipse[2])
{
	/* normalized ellipse: ell[0] = scaleX, ell[1] = scaleY */
	float x, y;

	x= (marker->pos[0] - offset[0])*ellipse[0];
	y= (marker->pos[1] - offset[1])*ellipse[1];

	return x*x + y*y < 1.0f;
}

static int circle_select_exec(bContext *C, wmOperator *op)
{
	SpaceClip *sc= CTX_wm_space_clip(C);
	MovieClip *clip= ED_space_clip(sc);
	ARegion *ar= CTX_wm_region(C);
	MovieTrackingTrack *track;
	int x, y, radius, width, height, mode, change= 0;
	float zoomx, zoomy, offset[2], ellipse[2];

	/* get operator properties */
	x= RNA_int_get(op->ptr, "x");
	y= RNA_int_get(op->ptr, "y");
	radius= RNA_int_get(op->ptr, "radius");

	mode= RNA_int_get(op->ptr, "gesture_mode");

	/* compute ellipse and position in unified coordinates */
	ED_space_clip_size(sc, &width, &height);
	ED_space_clip_zoom(sc, ar, &zoomx, &zoomy);

	ellipse[0]= width*zoomx/radius;
	ellipse[1]= height*zoomy/radius;

	UI_view2d_region_to_view(&ar->v2d, x, y, &offset[0], &offset[1]);

	/* do selection */
	track= clip->tracking.tracks.first;
	while(track) {
		MovieTrackingMarker *marker= BKE_tracking_get_marker(track, sc->user.framenr);

		if(marker) {
			if(marker_inside_ellipse(marker, offset, ellipse)) {
				BKE_tracking_track_flag(track, TRACK_AREA_ALL, SELECT, mode!=GESTURE_MODAL_SELECT);

				change= 1;
			}
		}

		track= track->next;
	}

	BKE_movieclip_set_selection(clip, MCLIP_SEL_NONE, NULL);

	if(change) {
		WM_event_add_notifier(C, NC_GEOM|ND_SELECT, NULL);

		return OPERATOR_FINISHED;
	}

	return OPERATOR_CANCELLED;
}

void CLIP_OT_select_circle(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Circle Select";
	ot->description= "Select markers using circle selection";
	ot->idname= "CLIP_OT_select_circle";

	/* api callbacks */
	ot->invoke= WM_gesture_circle_invoke;
	ot->modal= WM_gesture_circle_modal;
	ot->exec= circle_select_exec;
	ot->poll= space_clip_frame_poll;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;

	/* properties */
	RNA_def_int(ot->srna, "x", 0, INT_MIN, INT_MAX, "X", "", INT_MIN, INT_MAX);
	RNA_def_int(ot->srna, "y", 0, INT_MIN, INT_MAX, "Y", "", INT_MIN, INT_MAX);
	RNA_def_int(ot->srna, "radius", 0, INT_MIN, INT_MAX, "Radius", "", INT_MIN, INT_MAX);
	RNA_def_int(ot->srna, "gesture_mode", 0, INT_MIN, INT_MAX, "Gesture Mode", "", INT_MIN, INT_MAX);
}

/********************** select all operator *********************/

static int select_all_exec(bContext *C, wmOperator *op)
{
	SpaceClip *sc= CTX_wm_space_clip(C);
	MovieClip *clip= ED_space_clip(sc);
	MovieTrackingTrack *track= NULL;	/* selected track */
	int action= RNA_enum_get(op->ptr, "action");
	int sel_type, framenr= sc->user.framenr;
	void *sel;

	if(action == SEL_TOGGLE){
		action= SEL_SELECT;
		track= clip->tracking.tracks.first;
		while(track) {
			if(BKE_tracking_has_marker(track, framenr)) {
				if(TRACK_SELECTED(track)) {
					action= SEL_DESELECT;
					break;
				}
			}

			track= track->next;
		}
	}

	track= clip->tracking.tracks.first;
	while(track) {
		if(BKE_tracking_has_marker(track, framenr)) {
			switch (action) {
				case SEL_SELECT:
					if(track->bundle) track->bundle->flag|= SELECT;
					track->flag|= SELECT;
					track->pat_flag|= SELECT;
					track->search_flag|= SELECT;
					break;
				case SEL_DESELECT:
					if(track->bundle) track->bundle->flag&= ~SELECT;
					track->flag&= ~SELECT;
					track->pat_flag&= ~SELECT;
					track->search_flag&= ~SELECT;
					break;
				case SEL_INVERT:
					if(track->bundle) track->bundle->flag^= SELECT;
					track->flag^= SELECT;
					track->pat_flag^= SELECT;
					track->search_flag^= SELECT;
					break;
			}
		}

		track= track->next;
	}

	BKE_movieclip_last_selection(clip, &sel_type, &sel);
	if(sel_type==MCLIP_SEL_TRACK)
		if(!TRACK_SELECTED(((MovieTrackingTrack*)sel)))
			BKE_movieclip_set_selection(clip, MCLIP_SEL_NONE, NULL);

	WM_event_add_notifier(C, NC_GEOM|ND_SELECT, NULL);

	return OPERATOR_FINISHED;
}

void CLIP_OT_select_all(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Select or Deselect All";
	ot->description= "Change selection of all tracking markers";
	ot->idname= "CLIP_OT_select_all";

	/* api callbacks */
	ot->exec= select_all_exec;
	ot->poll= space_clip_frame_poll;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;

	WM_operator_properties_select_all(ot);
}

/********************** track operator *********************/

typedef struct TrackMarkersJob {
	struct MovieTrackingContext *context;
	int sfra, efra, backwards;
	MovieClip *clip;
	MovieClipUser *user;
} TrackMarkersJob;

static int track_markers_testbreak(void)
{
	return G.afbreek;
}

static void track_markers_initjob(bContext *C, TrackMarkersJob *tmj, int backwards)
{
	SpaceClip *sc= CTX_wm_space_clip(C);
	MovieClip *clip= ED_space_clip(sc);
	Scene *scene= CTX_data_scene(C);

	tmj->sfra= sc->user.framenr;
	tmj->clip= clip;
	tmj->user= &sc->user;
	tmj->backwards= backwards;

	if(backwards) tmj->efra= SFRA;
	else tmj->efra= EFRA;

	tmj->context= BKE_tracking_context_new(clip, &sc->user, backwards);
}

static void track_markers_startjob(void *tmv, short *UNUSED(stop), short *do_update, float *progress)
{
	TrackMarkersJob *tmj= (TrackMarkersJob *)tmv;
	int framenr= tmj->sfra;

	while(framenr != tmj->efra) {
		if(!BKE_tracking_next(tmj->context))
			break;

		*do_update= 1;
		*progress=(float)(framenr-tmj->sfra) / (tmj->efra-tmj->sfra);

		if(track_markers_testbreak())
			break;

		if(tmj->backwards) framenr--;
		else framenr++;
	}
}

static void track_markers_updatejob(void *tmv)
{
	TrackMarkersJob *tmj= (TrackMarkersJob *)tmv;

	BKE_tracking_sync(tmj->context);
}

static void track_markers_freejob(void *tmv)
{
	TrackMarkersJob *tmj= (TrackMarkersJob *)tmv;

	BKE_tracking_sync(tmj->context);
	BKE_tracking_context_free(tmj->context);

	/* movie clip's user was used during tracking,
	   need to restore current frame number */
	tmj->user->framenr= tmj->sfra;

	MEM_freeN(tmj);
}

static int track_markers_invoke(bContext *C, wmOperator *op, wmEvent *UNUSED(event))
{
	TrackMarkersJob *tmj;
	wmJob *steve;
	Scene *scene= CTX_data_scene(C);
	int backwards= RNA_boolean_get(op->ptr, "backwards");

	tmj= MEM_callocN(sizeof(TrackMarkersJob), "TrackMarkersJob data");
	track_markers_initjob(C, tmj, backwards);

	/* setup job */
	steve= WM_jobs_get(CTX_wm_manager(C), CTX_wm_window(C), scene, "Track Markers", WM_JOB_EXCL_RENDER|WM_JOB_PRIORITY|WM_JOB_PROGRESS);
	WM_jobs_customdata(steve, tmj, track_markers_freejob);
	WM_jobs_timer(steve, 0.2, NC_MOVIECLIP|NA_EVALUATED, 0);
	WM_jobs_callbacks(steve, track_markers_startjob, NULL, track_markers_updatejob, NULL);

	G.afbreek= 0;

	WM_jobs_start(CTX_wm_manager(C), steve);
	WM_cursor_wait(0);

	/* add modal handler for ESC */
	WM_event_add_modal_handler(C, op);

	return OPERATOR_RUNNING_MODAL;
}

static int track_markers_exec(bContext *C, wmOperator *op)
{
	SpaceClip *sc= CTX_wm_space_clip(C);
	MovieClip *clip= ED_space_clip(sc);
	Scene *scene= CTX_data_scene(C);
	struct MovieTrackingContext *context;
	int framenr= sc->user.framenr;
	int sfra= framenr;
	int backwards= RNA_boolean_get(op->ptr, "backwards");

	context= BKE_tracking_context_new(clip, &sc->user, backwards);

	while(framenr != EFRA) {
		if(!BKE_tracking_next(context))
			break;

		if(backwards) framenr--;
		else framenr++;
	}

	BKE_tracking_sync(context);
	BKE_tracking_context_free(context);

	/* movie clip's user was used during tracking,
	   need to restore current frame number */
	sc->user.framenr= sfra;

	WM_event_add_notifier(C, NC_MOVIECLIP|NA_EVALUATED, clip);

	return OPERATOR_FINISHED;
}

static int track_marekrs_modal(bContext *C, wmOperator *UNUSED(op), wmEvent *event)
{
	/* no running blender, remove handler and pass through */
	if(0==WM_jobs_test(CTX_wm_manager(C), CTX_data_scene(C)))
		return OPERATOR_FINISHED|OPERATOR_PASS_THROUGH;

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
	ot->name= "Track Markers";
	ot->description= "Track sleected markers";
	ot->idname= "CLIP_OT_track_markers";

	/* api callbacks */
	ot->exec= track_markers_exec;
	ot->invoke= track_markers_invoke;
	ot->poll= space_clip_frame_poll;
	ot->modal= track_marekrs_modal;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;

	/* properties */
	RNA_def_boolean(ot->srna, "backwards", 0, "Backwards", "Do backwards tarcking");
}

/********************** reset tracking settings operator *********************/

static int reset_tracking_settings_exec(bContext *C, wmOperator *UNUSED(op))
{
	SpaceClip *sc= CTX_wm_space_clip(C);
	MovieClip *clip= ED_space_clip(sc);

	BKE_tracking_reset_settings(&clip->tracking);

	return OPERATOR_FINISHED;
}

void CLIP_OT_reset_tracking_settings(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Reset Tracking Settings";
	ot->description= "Reset tracking settings to default values";
	ot->idname= "CLIP_OT_reset_tracking_settings";

	/* api callbacks */
	ot->exec= reset_tracking_settings_exec;
	ot->poll= space_clip_frame_poll;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

/********************** clear track operator *********************/

static int clear_track_path_poll(bContext *C)
{
	SpaceClip *sc= CTX_wm_space_clip(C);

	if(sc) {
		MovieClip *clip= ED_space_clip(sc);

		if(clip && BKE_movieclip_has_frame(clip, &sc->user)) {
			int sel_type;
			void *sel;

			BKE_movieclip_last_selection(clip, &sel_type, &sel);

			return sel_type == MCLIP_SEL_TRACK;
		}
	}

	return 0;
}

static int clear_track_path_exec(bContext *C, wmOperator *UNUSED(op))
{
	SpaceClip *sc= CTX_wm_space_clip(C);
	MovieClip *clip= ED_space_clip(sc);
	int sel_type;
	MovieTrackingTrack *track;

	BKE_movieclip_last_selection(clip, &sel_type, (void**)&track);

	BKE_tracking_clear_path(track, sc->user.framenr);

	WM_event_add_notifier(C, NC_MOVIECLIP|NA_EVALUATED, clip);

	return OPERATOR_FINISHED;
}

void CLIP_OT_clear_track_path(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Clear Track Path";
	ot->description= "Clear path of active track";
	ot->idname= "CLIP_OT_clear_track_path";

	/* api callbacks */
	ot->exec= clear_track_path_exec;
	ot->poll= clear_track_path_poll;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}