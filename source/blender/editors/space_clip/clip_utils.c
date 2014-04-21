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

/** \file blender/editors/space_clip/clip_utils.c
 *  \ingroup spclip
 */

#include "DNA_scene_types.h"
#include "DNA_object_types.h"   /* SELECT */

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"
#include "BLI_math.h"
#include "BLI_listbase.h"
#include "BLI_string.h"

#include "BKE_animsys.h"
#include "BKE_context.h"
#include "BKE_movieclip.h"
#include "BKE_tracking.h"
#include "BKE_depsgraph.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_screen.h"
#include "ED_clip.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "UI_interface.h"
#include "UI_resources.h"
#include "UI_view2d.h"

#include "clip_intern.h"    // own include

void clip_graph_tracking_values_iterate_track(
        SpaceClip *sc, MovieTrackingTrack *track, void *userdata,
        void (*func)(void *userdata, MovieTrackingTrack *track, MovieTrackingMarker *marker, int coord,
                     int scene_framenr, float val),
        void (*segment_start)(void *userdata, MovieTrackingTrack *track, int coord),
        void (*segment_end)(void *userdata, int coord))
{
	MovieClip *clip = ED_space_clip_get_clip(sc);
	int width, height, coord;

	BKE_movieclip_get_size(clip, &sc->user, &width, &height);

	for (coord = 0; coord < 2; coord++) {
		int i, prevfra = 0;
		bool open = false;
		float prevval = 0.0f;

		for (i = 0; i < track->markersnr; i++) {
			MovieTrackingMarker *marker = &track->markers[i];
			float val;

			if (marker->flag & MARKER_DISABLED) {
				if (open) {
					if (segment_end)
						segment_end(userdata, coord);

					open = false;
				}

				continue;
			}

			if (!open) {
				if (segment_start)
					segment_start(userdata, track, coord);

				open = true;
				prevval = marker->pos[coord];
			}

			/* value is a pixels per frame speed */
			val = (marker->pos[coord] - prevval) * ((coord == 0) ? (width) : (height));
			val /= marker->framenr - prevfra;

			if (func) {
				int scene_framenr = BKE_movieclip_remap_clip_to_scene_frame(clip, marker->framenr);

				func(userdata, track, marker, coord, scene_framenr, val);
			}

			prevval = marker->pos[coord];
			prevfra = marker->framenr;
		}

		if (open) {
			if (segment_end)
				segment_end(userdata, coord);
		}
	}
}

void clip_graph_tracking_values_iterate(
        SpaceClip *sc, bool selected_only, bool include_hidden, void *userdata,
        void (*func)(void *userdata, MovieTrackingTrack *track, MovieTrackingMarker *marker,
                     int coord, int scene_framenr, float val),
        void (*segment_start)(void *userdata, MovieTrackingTrack *track, int coord),
        void (*segment_end)(void *userdata, int coord))
{
	MovieClip *clip = ED_space_clip_get_clip(sc);
	MovieTracking *tracking = &clip->tracking;
	ListBase *tracksbase = BKE_tracking_get_active_tracks(tracking);
	MovieTrackingTrack *track;

	for (track = tracksbase->first; track; track = track->next) {
		if (!include_hidden && (track->flag & TRACK_HIDDEN) != 0)
			continue;

		if (selected_only && !TRACK_SELECTED(track))
			continue;

		clip_graph_tracking_values_iterate_track(sc, track, userdata, func, segment_start, segment_end);
	}
}

void clip_graph_tracking_iterate(SpaceClip *sc, bool selected_only, bool include_hidden, void *userdata,
                                 void (*func)(void *userdata, MovieTrackingMarker *marker))
{
	MovieClip *clip = ED_space_clip_get_clip(sc);
	MovieTracking *tracking = &clip->tracking;
	ListBase *tracksbase = BKE_tracking_get_active_tracks(tracking);
	MovieTrackingTrack *track;

	for (track = tracksbase->first; track; track = track->next) {
		int i;

		if (!include_hidden && (track->flag & TRACK_HIDDEN) != 0)
			continue;

		if (selected_only && !TRACK_SELECTED(track))
			continue;

		for (i = 0; i < track->markersnr; i++) {
			MovieTrackingMarker *marker = &track->markers[i];

			if (marker->flag & MARKER_DISABLED)
				continue;

			if (func)
				func(userdata, marker);
		}
	}
}

void clip_delete_track(bContext *C, MovieClip *clip, MovieTrackingTrack *track)
{
	MovieTracking *tracking = &clip->tracking;
	MovieTrackingStabilization *stab = &tracking->stabilization;
	MovieTrackingTrack *act_track = BKE_tracking_track_get_active(tracking);
	MovieTrackingPlaneTrack *plane_track, *next_plane_track;
	ListBase *tracksbase = BKE_tracking_get_active_tracks(tracking);
	ListBase *plane_tracks_base = BKE_tracking_get_active_plane_tracks(tracking);
	bool has_bundle = false, update_stab = false;
	char track_name_escaped[MAX_NAME], prefix[MAX_NAME * 2];

	if (track == act_track)
		tracking->act_track = NULL;

	if (track == stab->rot_track) {
		stab->rot_track = NULL;

		update_stab = true;
	}

	/* handle reconstruction display in 3d viewport */
	if (track->flag & TRACK_HAS_BUNDLE)
		has_bundle = true;

	/* Make sure no plane will use freed track */
	for (plane_track = plane_tracks_base->first;
	     plane_track;
	     plane_track = next_plane_track)
	{
		bool found = false;
		int i;

		next_plane_track = plane_track->next;

		for (i = 0; i < plane_track->point_tracksnr; i++) {
			if (plane_track->point_tracks[i] == track) {
				found = true;
				break;
			}
		}

		if (!found) {
			continue;
		}

		if (plane_track->point_tracksnr > 4) {
			int track_index;
			MovieTrackingTrack **new_point_tracks;

			new_point_tracks = MEM_mallocN(sizeof(*new_point_tracks) * plane_track->point_tracksnr,
			                               "new point tracks array");

			for (i = 0, track_index = 0; i < plane_track->point_tracksnr; i++) {
				if (plane_track->point_tracks[i] != track) {
					new_point_tracks[track_index++] = plane_track->point_tracks[i];
				}
			}

			MEM_freeN(plane_track->point_tracks);
			plane_track->point_tracks = new_point_tracks;
			plane_track->point_tracksnr--;
		}
		else {
			/* Delete planes with less than 3 point tracks in it. */
			BKE_tracking_plane_track_free(plane_track);
			BLI_freelinkN(plane_tracks_base, plane_track);
		}
	}

	/* Delete f-curves associated with the track (such as weight, i.e.) */
	BLI_strescape(track_name_escaped, track->name, sizeof(track_name_escaped));
	BLI_snprintf(prefix, sizeof(prefix), "tracks[\"%s\"]", track_name_escaped);
	BKE_animdata_fix_paths_remove(&clip->id, prefix);

	BKE_tracking_track_free(track);
	BLI_freelinkN(tracksbase, track);

	WM_event_add_notifier(C, NC_MOVIECLIP | NA_EDITED, clip);

	if (update_stab) {
		tracking->stabilization.ok = false;
		WM_event_add_notifier(C, NC_MOVIECLIP | ND_DISPLAY, clip);
	}

	DAG_id_tag_update(&clip->id, 0);

	if (has_bundle)
		WM_event_add_notifier(C, NC_SPACE | ND_SPACE_VIEW3D, NULL);
}

void clip_delete_marker(bContext *C, MovieClip *clip, MovieTrackingTrack *track,
                        MovieTrackingMarker *marker)
{
	if (track->markersnr == 1) {
		clip_delete_track(C, clip, track);
	}
	else {
		BKE_tracking_marker_delete(track, marker->framenr);

		WM_event_add_notifier(C, NC_MOVIECLIP | NA_EDITED, clip);
	}
}

void clip_view_center_to_point(SpaceClip *sc, float x, float y)
{
	int width, height;
	float aspx, aspy;

	ED_space_clip_get_size(sc, &width, &height);
	ED_space_clip_get_aspect(sc, &aspx, &aspy);

	sc->xof = (x - 0.5f) * width * aspx;
	sc->yof = (y - 0.5f) * height * aspy;
}

void clip_draw_cfra(SpaceClip *sc, ARegion *ar, Scene *scene)
{
	View2D *v2d = &ar->v2d;
	float xscale, yscale;
	float vec[2];

	/* Draw a light green line to indicate current frame */
	vec[0] = (float)(sc->user.framenr * scene->r.framelen);

	UI_ThemeColor(TH_CFRAME);
	glLineWidth(2.0);

	glBegin(GL_LINE_STRIP);
	vec[1] = v2d->cur.ymin;
	glVertex2fv(vec);

	vec[1] = v2d->cur.ymax;
	glVertex2fv(vec);
	glEnd();

	glLineWidth(1.0);

	UI_view2d_view_orthoSpecial(ar, v2d, 1);

	/* because the frame number text is subject to the same scaling as the contents of the view */
	UI_view2d_scale_get(v2d, &xscale, &yscale);
	glScalef(1.0f / xscale, 1.0f, 1.0f);

	ED_region_cache_draw_curfra_label(sc->user.framenr, (float)sc->user.framenr * xscale, 18);

	/* restore view transform */
	glScalef(xscale, 1.0, 1.0);
}

void clip_draw_sfra_efra(View2D *v2d, Scene *scene)
{
	UI_view2d_view_ortho(v2d);

	/* currently clip editor supposes that editing clip length is equal to scene frame range */
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_BLEND);
	glColor4f(0.0f, 0.0f, 0.0f, 0.4f);

	glRectf(v2d->cur.xmin, v2d->cur.ymin, (float)SFRA, v2d->cur.ymax);
	glRectf((float)EFRA, v2d->cur.ymin, v2d->cur.xmax, v2d->cur.ymax);
	glDisable(GL_BLEND);

	UI_ThemeColorShade(TH_BACK, -60);

	/* thin lines where the actual frames are */
	fdrawline((float)SFRA, v2d->cur.ymin, (float)SFRA, v2d->cur.ymax);
	fdrawline((float)EFRA, v2d->cur.ymin, (float)EFRA, v2d->cur.ymax);
}
