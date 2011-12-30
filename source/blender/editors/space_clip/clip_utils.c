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

#include "DNA_object_types.h"	/* SELECT */

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"
#include "BLI_math.h"
#include "BLI_listbase.h"

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

void clip_graph_tracking_values_iterate_track(SpaceClip *sc, MovieTrackingTrack *track, void *userdata,
			void (*func) (void *userdata, MovieTrackingTrack *track, MovieTrackingMarker *marker, int coord, float val),
			void (*segment_start) (void *userdata, MovieTrackingTrack *track, int coord),
			void (*segment_end) (void *userdata))
{
	MovieClip *clip= ED_space_clip(sc);
	int width, height, coord;

	BKE_movieclip_get_size(clip, &sc->user, &width, &height);

	for(coord= 0; coord<2; coord++) {
		int i, open= 0, prevfra= 0;
		float prevval= 0.0f;

		for(i= 0; i<track->markersnr; i++) {
			MovieTrackingMarker *marker= &track->markers[i];
			float val;

			if(marker->flag&MARKER_DISABLED) {
				if(open) {
					if(segment_end)
						segment_end(userdata);

					open= 0;
				}

				continue;
			}

			if(!open) {
				if(segment_start)
					segment_start(userdata, track, coord);

				open= 1;
				prevval= marker->pos[coord];
			}

			/* value is a pixels per frame speed */
			val= (marker->pos[coord] - prevval) * ((i==0) ? (width) : (height));
			val/= marker->framenr-prevfra;

			if(func)
				func(userdata, track, marker, coord, val);

			prevval= marker->pos[coord];
			prevfra= marker->framenr;
		}

		if(open) {
			if(segment_end)
				segment_end(userdata);
		}
	}
}

void clip_graph_tracking_values_iterate(SpaceClip *sc, void *userdata,
			void (*func) (void *userdata, MovieTrackingTrack *track, MovieTrackingMarker *marker, int coord, float val),
			void (*segment_start) (void *userdata, MovieTrackingTrack *track, int coord),
			void (*segment_end) (void *userdata))
{
	MovieClip *clip= ED_space_clip(sc);
	MovieTracking *tracking= &clip->tracking;
	ListBase *tracksbase= BKE_tracking_get_tracks(tracking);
	MovieTrackingTrack *track;

	track= tracksbase->first;
	while(track) {
		if(TRACK_VIEW_SELECTED(sc, track)) {
			clip_graph_tracking_values_iterate_track(sc, track, userdata, func, segment_start, segment_end);
		}

		track= track->next;
	}
}

void clip_graph_tracking_iterate(SpaceClip *sc, void *userdata,
			void (*func) (void *userdata, MovieTrackingMarker *marker))
{
	MovieClip *clip= ED_space_clip(sc);
	MovieTracking *tracking= &clip->tracking;
	ListBase *tracksbase= BKE_tracking_get_tracks(tracking);
	MovieTrackingTrack *track;

	track= tracksbase->first;
	while(track) {
		if(TRACK_VIEW_SELECTED(sc, track)) {
			int i;

			for(i= 0; i<track->markersnr; i++) {
				MovieTrackingMarker *marker= &track->markers[i];

				if(marker->flag&MARKER_DISABLED)
					continue;

				if(func)
					func(userdata, marker);
			}
		}

		track= track->next;
	}
}

void clip_delete_track(bContext *C, MovieClip *clip, ListBase *tracksbase, MovieTrackingTrack *track)
{
	MovieTracking *tracking= &clip->tracking;
	MovieTrackingStabilization *stab= &tracking->stabilization;
	MovieTrackingTrack *act_track= BKE_tracking_active_track(tracking);

	int has_bundle= 0, update_stab= 0;

	if(track==act_track)
		tracking->act_track= NULL;

	if(track==stab->rot_track) {
		stab->rot_track= NULL;

		update_stab= 1;
	}

	/* handle reconstruction display in 3d viewport */
	if(track->flag&TRACK_HAS_BUNDLE)
		has_bundle= 1;

	BKE_tracking_free_track(track);
	BLI_freelinkN(tracksbase, track);

	WM_event_add_notifier(C, NC_MOVIECLIP|NA_EDITED, clip);

	if(update_stab) {
		tracking->stabilization.ok= 0;

		DAG_id_tag_update(&clip->id, 0);
		WM_event_add_notifier(C, NC_MOVIECLIP|ND_DISPLAY, clip);
	}

	if(has_bundle)
		WM_event_add_notifier(C, NC_SPACE|ND_SPACE_VIEW3D, NULL);
}

void clip_delete_marker(bContext *C, MovieClip *clip, ListBase *tracksbase, MovieTrackingTrack *track, MovieTrackingMarker *marker)
{
	if(track->markersnr==1) {
		clip_delete_track(C, clip, tracksbase, track);
	}
	else {
		BKE_tracking_delete_marker(track, marker->framenr);

		WM_event_add_notifier(C, NC_MOVIECLIP|NA_EDITED, clip);
	}
}

void clip_view_center_to_point(SpaceClip *sc, float x, float y)
{
	int width, height;
	float aspx, aspy;

	ED_space_clip_size(sc, &width, &height);
	ED_space_clip_aspect(sc, &aspx, &aspy);

	sc->xof= (x-0.5f)*width*aspx;
	sc->yof= (y-0.5f)*height*aspy;
}
