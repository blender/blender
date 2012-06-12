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
 * Contributor(s): Blender Foundation,
 *                 Sergey Sharybin
 *                 Keir Mierle
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/blenkernel/intern/tracking.c
 *  \ingroup bke
 */

#include <stddef.h>
#include <limits.h>
#include <math.h>
#include <memory.h>

#include "MEM_guardedalloc.h"

#include "DNA_gpencil_types.h"
#include "DNA_camera_types.h"
#include "DNA_movieclip_types.h"
#include "DNA_object_types.h"   /* SELECT */
#include "DNA_scene_types.h"

#include "BLI_utildefines.h"
#include "BLI_math.h"
#include "BLI_math_base.h"
#include "BLI_listbase.h"
#include "BLI_ghash.h"
#include "BLI_path_util.h"
#include "BLI_string.h"
#include "BLI_threads.h"

#include "BKE_global.h"
#include "BKE_tracking.h"
#include "BKE_movieclip.h"
#include "BKE_object.h"
#include "BKE_scene.h"

#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"

#include "raskter.h"

#ifdef WITH_LIBMV
#  include "libmv-capi.h"
#else
struct libmv_Features;
#endif

typedef struct MovieDistortion {
	struct libmv_CameraIntrinsics *intrinsics;
} MovieDistortion;

static struct {
	ListBase tracks;
} tracking_clipboard;

/*********************** space transformation functions  *************************/

/* Three coordinate frames: Frame, Search, and Marker
 * Two units: Pixels, Unified
 * Notation: {coordinate frame}_{unit}; for example, "search_pixel" are search
 * window relative coordinates in pixels, and "frame_unified" are unified 0..1
 * coordinates relative to the entire frame.
 */
static void unified_to_pixel(int frame_width, int frame_height,
                             const float unified_coords[2], float pixel_coords[2])
{
	pixel_coords[0] = unified_coords[0] * frame_width;
	pixel_coords[1] = unified_coords[1] * frame_height;
}

static void marker_to_frame_unified(const MovieTrackingMarker *marker, const float marker_unified_coords[2],
                                    float frame_unified_coords[2])
{
	frame_unified_coords[0] = marker_unified_coords[0] + marker->pos[0];
	frame_unified_coords[1] = marker_unified_coords[1] + marker->pos[1];
}

static void marker_unified_to_frame_pixel_coordinates(int frame_width, int frame_height,
                                                      const MovieTrackingMarker *marker,
                                                      const float marker_unified_coords[2], float frame_pixel_coords[2])
{
	marker_to_frame_unified(marker, marker_unified_coords, frame_pixel_coords);
	unified_to_pixel(frame_width, frame_height, frame_pixel_coords, frame_pixel_coords);
}

static void get_search_origin_frame_pixel(int frame_width, int frame_height,
                                          const MovieTrackingMarker *marker, float frame_pixel[2])
{
	/* Get the lower left coordinate of the search window and snap to pixel coordinates */
	marker_unified_to_frame_pixel_coordinates(frame_width, frame_height, marker, marker->search_min, frame_pixel);
	frame_pixel[0] = (int)frame_pixel[0];
	frame_pixel[1] = (int)frame_pixel[1];
}

#ifdef WITH_LIBMV
static void pixel_to_unified(int frame_width, int frame_height, const float pixel_coords[2], float unified_coords[2])
{
	unified_coords[0] = pixel_coords[0] / frame_width;
	unified_coords[1] = pixel_coords[1] / frame_height;
}

static void marker_unified_to_search_pixel(int frame_width, int frame_height,
                                           const MovieTrackingMarker *marker,
                                           const float marker_unified[2], float search_pixel[2])
{
	float frame_pixel[2];
	float search_origin_frame_pixel[2];

	marker_unified_to_frame_pixel_coordinates(frame_width, frame_height, marker, marker_unified, frame_pixel);
	get_search_origin_frame_pixel(frame_width, frame_height, marker, search_origin_frame_pixel);
	sub_v2_v2v2(search_pixel, frame_pixel, search_origin_frame_pixel);
}

static void search_pixel_to_marker_unified(int frame_width, int frame_height,
                                           const MovieTrackingMarker *marker,
                                           const float search_pixel[2], float marker_unified[2])
{
	float frame_unified[2];
	float search_origin_frame_pixel[2];

	get_search_origin_frame_pixel(frame_width, frame_height, marker, search_origin_frame_pixel);
	add_v2_v2v2(frame_unified, search_pixel, search_origin_frame_pixel);
	pixel_to_unified(frame_width, frame_height, frame_unified, frame_unified);

	/* marker pos is in frame unified */
	sub_v2_v2v2(marker_unified, frame_unified, marker->pos);
}

/* Each marker has 5 coordinates associated with it that get warped with
 * tracking: the four corners ("pattern_corners"), and the cernter ("pos").
 * This function puts those 5 points into the appropriate frame for tracking
 * (the "search" coordinate frame).
 */
static void get_marker_coords_for_tracking(int frame_width, int frame_height,
                                           const MovieTrackingMarker *marker,
                                           double search_pixel_x[5], double search_pixel_y[5])
{
	int i;
	float unified_coords[2];
	float pixel_coords[2];

	/* Convert the corners into search space coordinates. */
	for (i = 0; i < 4; i++) {
		marker_unified_to_search_pixel(frame_width, frame_height, marker, marker->pattern_corners[i], pixel_coords);
		search_pixel_x[i] = pixel_coords[0];
		search_pixel_y[i] = pixel_coords[1];
	}
	/* Convert the center position (aka "pos"); this is the origin */
	unified_coords[0] = 0.0;
	unified_coords[1] = 0.0;
	marker_unified_to_search_pixel(frame_width, frame_height, marker, unified_coords, pixel_coords);

	search_pixel_x[4] = pixel_coords[0];
	search_pixel_y[4] = pixel_coords[1];
}

/* Inverse of above. */
static void set_marker_coords_from_tracking(int frame_width, int frame_height, MovieTrackingMarker *marker,
                                            const double search_pixel_x[5], const double search_pixel_y[5])
{
	int i;
	float marker_unified[2];
	float search_pixel[2];

	/* Convert the corners into search space coordinates. */
	for (i = 0; i < 4; i++) {
		search_pixel[0] = search_pixel_x[i];
		search_pixel[1] = search_pixel_y[i];
		search_pixel_to_marker_unified(frame_width, frame_height, marker, search_pixel, marker->pattern_corners[i]);
	}

	/* Convert the center position (aka "pos"); this is the origin */
	search_pixel[0] = search_pixel_x[4];
	search_pixel[1] = search_pixel_y[4];
	search_pixel_to_marker_unified(frame_width, frame_height, marker, search_pixel, marker_unified);

	/* If the tracker tracked nothing, then "marker_unified" would be zero.
	 * Otherwise, the entire patch shifted, and that delta should be applied to
	 * all the coordinates.
	 */
	for (i = 0; i < 4; i++) {
		marker->pattern_corners[i][0] -= marker_unified[0];
		marker->pattern_corners[i][1] -= marker_unified[1];
	}

	marker->pos[0] += marker_unified[0];
	marker->pos[1] += marker_unified[1];
}
#endif

/*********************** common functions *************************/

void BKE_tracking_init_settings(MovieTracking *tracking)
{
	tracking->camera.sensor_width = 35.0f;
	tracking->camera.pixel_aspect = 1.0f;
	tracking->camera.units = CAMERA_UNITS_MM;

	tracking->settings.default_motion_model = TRACK_MOTION_MODEL_TRANSLATION;
	tracking->settings.default_minimum_correlation = 0.75;
	tracking->settings.default_pattern_size = 11;
	tracking->settings.default_search_size = 61;
	tracking->settings.keyframe1 = 1;
	tracking->settings.keyframe2 = 30;
	tracking->settings.dist = 1;
	tracking->settings.object_distance = 1;

	tracking->stabilization.scaleinf = 1.0f;
	tracking->stabilization.locinf = 1.0f;
	tracking->stabilization.rotinf = 1.0f;
	tracking->stabilization.maxscale = 2.0f;

	BKE_tracking_new_object(tracking, "Camera");
}

void BKE_tracking_clamp_marker(MovieTrackingMarker *marker, int event)
{
	int a;
	float pat_min[2], pat_max[2];

	BKE_tracking_marker_pattern_minmax(marker, pat_min, pat_max);

	if (event == CLAMP_PAT_DIM) {
		for (a = 0; a < 2; a++) {
			/* search shouldn't be resized smaller than pattern */
			marker->search_min[a] = MIN2(pat_min[a], marker->search_min[a]);
			marker->search_max[a] = MAX2(pat_max[a], marker->search_max[a]);
		}
	}
	else if (event == CLAMP_PAT_POS) {
		float dim[2];

		sub_v2_v2v2(dim, pat_max, pat_min);

		for (a = 0; a < 2; a++) {
			int b;
			/* pattern shouldn't be moved outside of search */
			if (pat_min[a] < marker->search_min[a]) {
				for (b = 0; b < 4; b++)
					marker->pattern_corners[b][a] += marker->search_min[a] - pat_min[a];
			}
			if (pat_max[a] > marker->search_max[a]) {
				for (b = 0; b < 4; b++)
					marker->pattern_corners[b][a] -= pat_max[a] - marker->search_max[a];
			}
		}
	}
	else if (event == CLAMP_SEARCH_DIM) {
		for (a = 0; a < 2; a++) {
			/* search shouldn't be resized smaller than pattern */
			marker->search_min[a] = MIN2(pat_min[a], marker->search_min[a]);
			marker->search_max[a] = MAX2(pat_max[a], marker->search_max[a]);
		}
	}
	else if (event == CLAMP_SEARCH_POS) {
		float dim[2];

		sub_v2_v2v2(dim, marker->search_max, marker->search_min);

		for (a = 0; a < 2; a++) {
			/* search shouldn't be moved inside pattern */
			if (marker->search_min[a] > pat_min[a]) {
				marker->search_min[a] = pat_min[a];
				marker->search_max[a] = marker->search_min[a] + dim[a];
			}
			if (marker->search_max[a] < pat_max[a]) {
				marker->search_max[a] = pat_max[a];
				marker->search_min[a] = marker->search_max[a] - dim[a];
			}
		}
	}
	else if (event == CLAMP_SEARCH_DIM) {
		float dim[2];
		sub_v2_v2v2(dim, pat_max, pat_min);
		for (a = 0; a < 2; a++) {
			marker->search_min[a] = pat_min[a];
			marker->search_max[a] = pat_max[a];
		}
	}
}

void BKE_tracking_track_flag(MovieTrackingTrack *track, int area, int flag, int clear)
{
	if (area == TRACK_AREA_NONE)
		return;

	if (clear) {
		if (area & TRACK_AREA_POINT)
			track->flag &= ~flag;
		if (area & TRACK_AREA_PAT)
			track->pat_flag &= ~flag;
		if (area & TRACK_AREA_SEARCH)
			track->search_flag &= ~flag;
	}
	else {
		if (area & TRACK_AREA_POINT)
			track->flag |= flag;
		if (area & TRACK_AREA_PAT)
			track->pat_flag |= flag;
		if (area & TRACK_AREA_SEARCH)
			track->search_flag |= flag;
	}
}

MovieTrackingTrack *BKE_tracking_add_track(MovieTracking *tracking, ListBase *tracksbase, float x, float y,
                                           int framenr, int width, int height)
{
	MovieTrackingTrack *track;
	MovieTrackingMarker marker;
	MovieTrackingSettings *settings = &tracking->settings;

	float half_pattern = (float)settings->default_pattern_size / 2.0f;
	float half_search = (float)settings->default_search_size / 2.0f;
	float pat[2], search[2];

	pat[0] = half_pattern / (float)width;
	pat[1] = half_pattern / (float)height;

	search[0] = half_search / (float)width;
	search[1] = half_search / (float)height;

	track = MEM_callocN(sizeof(MovieTrackingTrack), "add_marker_exec track");
	strcpy(track->name, "Track");

	track->motion_model = settings->default_motion_model;
	track->minimum_correlation = settings->default_minimum_correlation;
	track->margin = settings->default_margin;
	track->pattern_match = settings->default_pattern_match;
	track->frames_limit = settings->default_frames_limit;
	track->flag = settings->default_flag;
	track->algorithm_flag = settings->default_algorithm_flag;

	memset(&marker, 0, sizeof(marker));
	marker.pos[0] = x;
	marker.pos[1] = y;
	marker.framenr = framenr;

	marker.pattern_corners[0][0] = -pat[0];
	marker.pattern_corners[0][1] = -pat[1];

	marker.pattern_corners[1][0] = pat[0];
	marker.pattern_corners[1][1] = -pat[1];

	negate_v2_v2(marker.pattern_corners[2], marker.pattern_corners[0]);
	negate_v2_v2(marker.pattern_corners[3], marker.pattern_corners[1]);

	copy_v2_v2(marker.search_max, search);
	negate_v2_v2(marker.search_min, search);

	BKE_tracking_insert_marker(track, &marker);

	BLI_addtail(tracksbase, track);
	BKE_track_unique_name(tracksbase, track);

	return track;
}

MovieTrackingMarker *BKE_tracking_insert_marker(MovieTrackingTrack *track, MovieTrackingMarker *marker)
{
	MovieTrackingMarker *old_marker = NULL;

	if (track->markersnr)
		old_marker = BKE_tracking_exact_marker(track, marker->framenr);

	if (old_marker) {
		*old_marker = *marker;

		return old_marker;
	}
	else {
		int a = track->markersnr;

		while (a--) {
			if (track->markers[a].framenr < marker->framenr)
				break;
		}

		track->markersnr++;

		if (track->markers)
			track->markers = MEM_reallocN(track->markers, sizeof(MovieTrackingMarker) * track->markersnr);
		else
			track->markers = MEM_callocN(sizeof(MovieTrackingMarker), "MovieTracking markers");

		memmove(track->markers + a + 2, track->markers + a + 1,
		        (track->markersnr - a - 2) * sizeof(MovieTrackingMarker));
		track->markers[a + 1] = *marker;

		track->last_marker = a + 1;

		return &track->markers[a + 1];
	}
}

void BKE_tracking_delete_marker(MovieTrackingTrack *track, int framenr)
{
	int a = 0;

	while (a < track->markersnr) {
		if (track->markers[a].framenr == framenr) {
			if (track->markersnr > 1) {
				memmove(track->markers + a, track->markers + a + 1,
				        (track->markersnr - a - 1) * sizeof(MovieTrackingMarker));
				track->markersnr--;
				track->markers = MEM_reallocN(track->markers, sizeof(MovieTrackingMarker) * track->markersnr);
			}
			else {
				MEM_freeN(track->markers);
				track->markers = NULL;
				track->markersnr = 0;
			}

			break;
		}

		a++;
	}
}

void BKE_tracking_marker_pattern_minmax(const MovieTrackingMarker *marker, float min[2], float max[2])
{
	INIT_MINMAX2(min, max);

	DO_MINMAX2(marker->pattern_corners[0], min, max);
	DO_MINMAX2(marker->pattern_corners[1], min, max);
	DO_MINMAX2(marker->pattern_corners[2], min, max);
	DO_MINMAX2(marker->pattern_corners[3], min, max);
}

MovieTrackingMarker *BKE_tracking_get_marker(MovieTrackingTrack *track, int framenr)
{
	int a = track->markersnr - 1;

	if (!track->markersnr)
		return NULL;

	/* approximate pre-first framenr marker with first marker */
	if (framenr < track->markers[0].framenr)
		return &track->markers[0];

	if (track->last_marker < track->markersnr)
		a = track->last_marker;

	if (track->markers[a].framenr <= framenr) {
		while (a < track->markersnr && track->markers[a].framenr <= framenr) {
			if (track->markers[a].framenr == framenr) {
				track->last_marker = a;

				return &track->markers[a];
			}
			a++;
		}

		/* if there's no marker for exact position, use nearest marker from left side */
		return &track->markers[a - 1];
	}
	else {
		while (a >= 0 && track->markers[a].framenr >= framenr) {
			if (track->markers[a].framenr == framenr) {
				track->last_marker = a;

				return &track->markers[a];
			}

			a--;
		}

		/* if there's no marker for exact position, use nearest marker from left side */
		return &track->markers[a];
	}

	return NULL;
}

MovieTrackingMarker *BKE_tracking_ensure_marker(MovieTrackingTrack *track, int framenr)
{
	MovieTrackingMarker *marker = BKE_tracking_get_marker(track, framenr);

	if (marker->framenr != framenr) {
		MovieTrackingMarker marker_new;

		marker_new = *marker;
		marker_new.framenr = framenr;

		BKE_tracking_insert_marker(track, &marker_new);
		marker = BKE_tracking_get_marker(track, framenr);
	}

	return marker;
}

MovieTrackingMarker *BKE_tracking_exact_marker(MovieTrackingTrack *track, int framenr)
{
	MovieTrackingMarker *marker = BKE_tracking_get_marker(track, framenr);

	if (marker->framenr != framenr)
		return NULL;

	return marker;
}

int BKE_tracking_has_marker(MovieTrackingTrack *track, int framenr)
{
	return BKE_tracking_exact_marker(track, framenr) != 0;
}

int BKE_tracking_has_enabled_marker(MovieTrackingTrack *track, int framenr)
{
	MovieTrackingMarker *marker = BKE_tracking_exact_marker(track, framenr);

	return marker && (marker->flag & MARKER_DISABLED) == 0;
}

void BKE_tracking_free_track(MovieTrackingTrack *track)
{
	if (track->markers)
		MEM_freeN(track->markers);
}

static void put_disabled_marker(MovieTrackingTrack *track, MovieTrackingMarker *ref_marker, int before, int overwrite)
{
	MovieTrackingMarker marker_new;

	marker_new = *ref_marker;
	marker_new.flag &= ~MARKER_TRACKED;
	marker_new.flag |= MARKER_DISABLED;

	if (before)
		marker_new.framenr--;
	else
		marker_new.framenr++;

	if (!BKE_tracking_has_marker(track, marker_new.framenr) || overwrite)
		BKE_tracking_insert_marker(track, &marker_new);
}

void BKE_tracking_clear_path(MovieTrackingTrack *track, int ref_frame, int action)
{
	int a;

	if (action == TRACK_CLEAR_REMAINED) {
		a = 1;

		while (a < track->markersnr) {
			if (track->markers[a].framenr > ref_frame) {
				track->markersnr = a;
				track->markers = MEM_reallocN(track->markers, sizeof(MovieTrackingMarker) * track->markersnr);

				break;
			}

			a++;
		}

		if (track->markersnr)
			put_disabled_marker(track, &track->markers[track->markersnr - 1], 0, 1);
	}
	else if (action == TRACK_CLEAR_UPTO) {
		a = track->markersnr - 1;

		while (a >= 0) {
			if (track->markers[a].framenr <= ref_frame) {
				memmove(track->markers, track->markers + a, (track->markersnr - a) * sizeof(MovieTrackingMarker));

				track->markersnr = track->markersnr - a;
				track->markers = MEM_reallocN(track->markers, sizeof(MovieTrackingMarker) * track->markersnr);

				break;
			}

			a--;
		}

		if (track->markersnr)
			put_disabled_marker(track, &track->markers[0], 1, 1);
	}
	else if (action == TRACK_CLEAR_ALL) {
		MovieTrackingMarker *marker, marker_new;

		marker = BKE_tracking_get_marker(track, ref_frame);
		marker_new = *marker;

		MEM_freeN(track->markers);
		track->markers = NULL;
		track->markersnr = 0;

		BKE_tracking_insert_marker(track, &marker_new);

		put_disabled_marker(track, &marker_new, 1, 1);
		put_disabled_marker(track, &marker_new, 0, 1);
	}
}

void BKE_tracking_join_tracks(MovieTrackingTrack *dst_track, MovieTrackingTrack *src_track)
{
	int i = 0, a = 0, b = 0, tot;
	MovieTrackingMarker *markers;

	tot = dst_track->markersnr + src_track->markersnr;
	markers = MEM_callocN(tot * sizeof(MovieTrackingMarker), "tmp tracking joined tracks");

	while (a < src_track->markersnr || b < dst_track->markersnr) {
		if (b >= dst_track->markersnr) {
			markers[i] = src_track->markers[a++];
		}
		else if (a >= src_track->markersnr) {
			markers[i] = dst_track->markers[b++];
		}
		else if (src_track->markers[a].framenr < dst_track->markers[b].framenr) {
			markers[i] = src_track->markers[a++];
		}
		else if (src_track->markers[a].framenr > dst_track->markers[b].framenr) {
			markers[i] = dst_track->markers[b++];
		}
		else {
			if ((src_track->markers[a].flag & MARKER_DISABLED) == 0) {
				if ((dst_track->markers[b].flag & MARKER_DISABLED) == 0) {
					/* both tracks are enabled on this frame, so find the whole segment
					 * on which tracks are intersecting and blend tracks using linear
					 * interpolation to prevent jumps
					 */

					MovieTrackingMarker *marker_a, *marker_b;
					int start_a = a, start_b = b, len = 0, frame = src_track->markers[a].framenr;
					int j, inverse = 0;

					inverse = (b == 0) ||
					          (dst_track->markers[b - 1].flag & MARKER_DISABLED) ||
					          (dst_track->markers[b - 1].framenr != frame - 1);

					/* find length of intersection */
					while (a < src_track->markersnr && b < dst_track->markersnr) {
						marker_a = &src_track->markers[a];
						marker_b = &dst_track->markers[b];

						if (marker_a->flag & MARKER_DISABLED || marker_b->flag & MARKER_DISABLED)
							break;

						if (marker_a->framenr != frame || marker_b->framenr != frame)
							break;

						frame++;
						len++;
						a++;
						b++;
					}

					a = start_a;
					b = start_b;

					/* linear interpolation for intersecting frames */
					for (j = 0; j < len; j++) {
						float fac = 0.5f;

						if (len > 1)
							fac = 1.0f / (len - 1) * j;

						if (inverse)
							fac = 1.0f - fac;

						marker_a = &src_track->markers[a];
						marker_b = &dst_track->markers[b];

						markers[i] = dst_track->markers[b];
						interp_v2_v2v2(markers[i].pos, marker_b->pos, marker_a->pos, fac);
						a++;
						b++;
						i++;
					}

					/* this values will be incremented at the end of the loop cycle */
					a--; b--; i--;
				}
				else markers[i] = src_track->markers[a];
			}
			else markers[i] = dst_track->markers[b];

			a++;
			b++;
		}

		i++;
	}

	MEM_freeN(dst_track->markers);

	dst_track->markers = MEM_callocN(i * sizeof(MovieTrackingMarker), "tracking joined tracks");
	memcpy(dst_track->markers, markers, i * sizeof(MovieTrackingMarker));

	dst_track->markersnr = i;

	MEM_freeN(markers);
}

static void tracking_tracks_free(ListBase *tracks)
{
	MovieTrackingTrack *track;

	for (track = tracks->first; track; track = track->next) {
		BKE_tracking_free_track(track);
	}

	BLI_freelistN(tracks);
}

static void tracking_reconstruction_free(MovieTrackingReconstruction *reconstruction)
{
	if (reconstruction->cameras)
		MEM_freeN(reconstruction->cameras);
}

static void tracking_object_free(MovieTrackingObject *object)
{
	tracking_tracks_free(&object->tracks);
	tracking_reconstruction_free(&object->reconstruction);
}

static void tracking_objects_free(ListBase *objects)
{
	MovieTrackingObject *object;

	for (object = objects->first; object; object = object->next)
		tracking_object_free(object);

	BLI_freelistN(objects);
}

static void tracking_dopesheet_free(MovieTrackingDopesheet *dopesheet)
{
	MovieTrackingDopesheetChannel *channel;

	channel = dopesheet->channels.first;
	while (channel) {
		if (channel->segments) {
			MEM_freeN(channel->segments);
		}

		channel = channel->next;
	}

	BLI_freelistN(&dopesheet->channels);

	dopesheet->channels.first = dopesheet->channels.last = NULL;
	dopesheet->tot_channel = 0;
}

void BKE_tracking_free(MovieTracking *tracking)
{
	tracking_tracks_free(&tracking->tracks);
	tracking_reconstruction_free(&tracking->reconstruction);
	tracking_objects_free(&tracking->objects);

	if (tracking->stabilization.scaleibuf)
		IMB_freeImBuf(tracking->stabilization.scaleibuf);

	if (tracking->camera.intrinsics)
		BKE_tracking_distortion_destroy(tracking->camera.intrinsics);

	tracking_dopesheet_free(&tracking->dopesheet);
}

static MovieTrackingTrack *duplicate_track(MovieTrackingTrack *track)
{
	MovieTrackingTrack *new_track;

	new_track = MEM_callocN(sizeof(MovieTrackingTrack), "tracksMapMerge new_track");

	*new_track = *track;
	new_track->next = new_track->prev = NULL;

	new_track->markers = MEM_dupallocN(new_track->markers);

	return new_track;
}

/*********************** clipboard *************************/

void BKE_tracking_free_clipboard(void)
{
	MovieTrackingTrack *track = tracking_clipboard.tracks.first, *next_track;

	while (track) {
		next_track = track->next;

		BKE_tracking_free_track(track);
		MEM_freeN(track);

		track = next_track;
	}
}

void BKE_tracking_clipboard_copy_tracks(MovieTracking *tracking, MovieTrackingObject *object)
{
	ListBase *tracksbase = BKE_tracking_object_tracks(tracking, object);
	MovieTrackingTrack *track = tracksbase->first;

	BKE_tracking_free_clipboard();

	while (track) {
		if (TRACK_SELECTED(track) && (track->flag & TRACK_HIDDEN) == 0) {
			MovieTrackingTrack *new_track = duplicate_track(track);

			BLI_addtail(&tracking_clipboard.tracks, new_track);
		}

		track = track->next;
	}
}

int BKE_tracking_clipboard_has_tracks(void)
{
	return tracking_clipboard.tracks.first != NULL;
}

void BKE_tracking_clipboard_paste_tracks(MovieTracking *tracking, MovieTrackingObject *object)
{
	ListBase *tracksbase = BKE_tracking_object_tracks(tracking, object);
	MovieTrackingTrack *track = tracking_clipboard.tracks.first;

	while (track) {
		MovieTrackingTrack *new_track = duplicate_track(track);

		BLI_addtail(tracksbase, new_track);
		BKE_track_unique_name(tracksbase, new_track);

		track = track->next;
	}
}

/*********************** tracks map *************************/

typedef struct TracksMap {
	char object_name[MAX_NAME];
	int is_camera;

	int num_tracks;
	int customdata_size;

	char *customdata;
	MovieTrackingTrack *tracks;

	GHash *hash;

	int ptr;
} TracksMap;

static TracksMap *tracks_map_new(const char *object_name, int is_camera, int num_tracks, int customdata_size)
{
	TracksMap *map = MEM_callocN(sizeof(TracksMap), "TrackingsMap");

	BLI_strncpy(map->object_name, object_name, sizeof(map->object_name));
	map->is_camera = is_camera;

	map->num_tracks = num_tracks;
	map->customdata_size = customdata_size;

	map->tracks = MEM_callocN(sizeof(MovieTrackingTrack) * num_tracks, "TrackingsMap tracks");

	if (customdata_size)
		map->customdata = MEM_callocN(customdata_size * num_tracks, "TracksMap customdata");

	map->hash = BLI_ghash_ptr_new("TracksMap hash");

	return map;
}

static int tracks_map_size(TracksMap *map)
{
	return map->num_tracks;
}

static void tracks_map_get(TracksMap *map, int index, MovieTrackingTrack **track, void **customdata)
{
	*track = &map->tracks[index];

	if (map->customdata)
		*customdata = &map->customdata[index * map->customdata_size];
}

static void tracks_map_insert(TracksMap *map, MovieTrackingTrack *track, void *customdata)
{
	MovieTrackingTrack new_track = *track;

	new_track.markers = MEM_dupallocN(new_track.markers);

	map->tracks[map->ptr] = new_track;

	if (customdata)
		memcpy(&map->customdata[map->ptr * map->customdata_size], customdata, map->customdata_size);

	BLI_ghash_insert(map->hash, &map->tracks[map->ptr], track);

	map->ptr++;
}

static void tracks_map_merge(TracksMap *map, MovieTracking *tracking)
{
	MovieTrackingTrack *track;
	MovieTrackingTrack *act_track = BKE_tracking_active_track(tracking);
	MovieTrackingTrack *rot_track = tracking->stabilization.rot_track;
	ListBase tracks = {NULL, NULL}, new_tracks = {NULL, NULL};
	ListBase *old_tracks;
	int a;

	if (map->is_camera) {
		old_tracks = &tracking->tracks;
	}
	else {
		MovieTrackingObject *object = BKE_tracking_named_object(tracking, map->object_name);

		if (!object) {
			/* object was deleted by user, create new one */
			object = BKE_tracking_new_object(tracking, map->object_name);
		}

		old_tracks = &object->tracks;
	}

	/* duplicate currently operating tracks to temporary list.
	 * this is needed to keep names in unique state and it's faster to change names
	 * of currently operating tracks (if needed)
	 */
	for (a = 0; a < map->num_tracks; a++) {
		int replace_sel = 0, replace_rot = 0;
		MovieTrackingTrack *new_track, *old;

		track = &map->tracks[a];

		/* find original of operating track in list of previously displayed tracks */
		old = BLI_ghash_lookup(map->hash, track);
		if (old) {
			MovieTrackingTrack *cur = old_tracks->first;

			while (cur) {
				if (cur == old)
					break;

				cur = cur->next;
			}

			/* original track was found, re-use flags and remove this track */
			if (cur) {
				if (cur == act_track)
					replace_sel = 1;
				if (cur == rot_track)
					replace_rot = 1;

				track->flag = cur->flag;
				track->pat_flag = cur->pat_flag;
				track->search_flag = cur->search_flag;

				BKE_tracking_free_track(cur);
				BLI_freelinkN(old_tracks, cur);
			}
		}

		new_track = duplicate_track(track);

		BLI_ghash_remove(map->hash, track, NULL, NULL); /* XXX: are we actually need this */
		BLI_ghash_insert(map->hash, track, new_track);

		if (replace_sel)  /* update current selection in clip */
			tracking->act_track = new_track;

		if (replace_rot)  /* update track used for rotation stabilization */
			tracking->stabilization.rot_track = new_track;

		BLI_addtail(&tracks, new_track);
	}

	/* move all tracks, which aren't operating */
	track = old_tracks->first;
	while (track) {
		MovieTrackingTrack *next = track->next;

		track->next = track->prev = NULL;
		BLI_addtail(&new_tracks, track);

		track = next;
	}

	/* now move all tracks which are currently operating and keep their names unique */
	track = tracks.first;
	while (track) {
		MovieTrackingTrack *next = track->next;

		BLI_remlink(&tracks, track);

		track->next = track->prev = NULL;
		BLI_addtail(&new_tracks, track);

		BLI_uniquename(&new_tracks, track, "Track", '.', offsetof(MovieTrackingTrack, name), sizeof(track->name));

		track = next;
	}

	*old_tracks = new_tracks;
}

static void tracks_map_free(TracksMap *map, void (*customdata_free)(void *customdata))
{
	int i = 0;

	BLI_ghash_free(map->hash, NULL, NULL);

	for (i = 0; i < map->num_tracks; i++) {
		if (map->customdata && customdata_free)
			customdata_free(&map->customdata[i * map->customdata_size]);

		BKE_tracking_free_track(&map->tracks[i]);
	}

	if (map->customdata)
		MEM_freeN(map->customdata);

	MEM_freeN(map->tracks);
	MEM_freeN(map);
}

/*********************** tracking *************************/

typedef struct TrackContext {
#ifdef WITH_LIBMV
	/* the reference marker and cutout search area */
	MovieTrackingMarker marker;

	/* keyframed patch. This is the search area */
	float *search_area;
	int search_area_height;
	int search_area_width;
	int framenr;

	float *mask;
#else
	int pad;
#endif
} TrackContext;

typedef struct MovieTrackingContext {
	MovieClipUser user;
	MovieClip *clip;
	int clip_flag;

	int first_time, frames;

	MovieTrackingSettings settings;
	TracksMap *tracks_map;

	short backwards, sequence;
	int sync_frame;
} MovieTrackingContext;

MovieTrackingContext *BKE_tracking_context_new(MovieClip *clip, MovieClipUser *user, short backwards, short sequence)
{
	MovieTrackingContext *context = MEM_callocN(sizeof(MovieTrackingContext), "trackingContext");
	MovieTracking *tracking = &clip->tracking;
	MovieTrackingSettings *settings = &tracking->settings;
	ListBase *tracksbase = BKE_tracking_get_tracks(tracking);
	MovieTrackingTrack *track;
	MovieTrackingObject *object = BKE_tracking_active_object(tracking);
	int num_tracks = 0;

	context->settings = *settings;
	context->backwards = backwards;
	context->sync_frame = user->framenr;
	context->first_time = TRUE;
	context->sequence = sequence;

	/* count */
	track = tracksbase->first;
	while (track) {
		if (TRACK_SELECTED(track) && (track->flag & (TRACK_LOCKED | TRACK_HIDDEN)) == 0) {
			int framenr = BKE_movieclip_remap_scene_to_clip_frame(clip, user->framenr);
			MovieTrackingMarker *marker = BKE_tracking_get_marker(track, framenr);

			if ((marker->flag & MARKER_DISABLED) == 0)
				num_tracks++;
		}

		track = track->next;
	}

	if (num_tracks) {
		int width, height;

		context->tracks_map = tracks_map_new(object->name, object->flag & TRACKING_OBJECT_CAMERA,
		                                     num_tracks, sizeof(TrackContext));

		BKE_movieclip_get_size(clip, user, &width, &height);

		/* create tracking data */
		track = tracksbase->first;
		while (track) {
			if (TRACK_SELECTED(track) && (track->flag & (TRACK_HIDDEN | TRACK_LOCKED)) == 0) {
				int framenr = BKE_movieclip_remap_scene_to_clip_frame(clip, user->framenr);
				MovieTrackingMarker *marker = BKE_tracking_get_marker(track, framenr);

				if ((marker->flag & MARKER_DISABLED) == 0) {
					TrackContext track_context;
					memset(&track_context, 0, sizeof(TrackContext));
					tracks_map_insert(context->tracks_map, track, &track_context);
				}
			}

			track = track->next;
		}
	}

	context->clip = clip;

	/* store needed clip flags passing to get_buffer functions
	 * - MCLIP_USE_PROXY is needed to because timecode affects on movie clip
	 *   only in case Proxy/Timecode flag is set, so store this flag to use
	 *   timecodes properly but reset render size to SIZE_FULL so correct resolution
	 *   would be used for images
	 * - MCLIP_USE_PROXY_CUSTOM_DIR is needed because proxy/timecode files might
	 *   be stored in a different location
	 * ignore all the rest possible flags for now
	 */
	context->clip_flag = clip->flag & MCLIP_TIMECODE_FLAGS;

	context->user = *user;
	context->user.render_size = MCLIP_PROXY_RENDER_SIZE_FULL;
	context->user.render_flag = 0;

	if (!sequence)
		BLI_begin_threaded_malloc();

	return context;
}

static void track_context_free(void *customdata)
{
	TrackContext *track_context = (TrackContext *)customdata;

#if WITH_LIBMV
	if (track_context->search_area)
		MEM_freeN(track_context->search_area);

	if (track_context->mask)
		MEM_freeN(track_context->mask);

#else
	(void)track_context;
#endif
}

void BKE_tracking_context_free(MovieTrackingContext *context)
{
	if (!context->sequence)
		BLI_end_threaded_malloc();

	tracks_map_free(context->tracks_map, track_context_free);

	MEM_freeN(context);
}

/* zap channels from the imbuf that are disabled by the user. this can lead to
 * better tracks sometimes. however, instead of simply zeroing the channels
 * out, do a partial grayscale conversion so the display is better.
 */
void BKE_tracking_disable_imbuf_channels(ImBuf *ibuf, int disable_red, int disable_green, int disable_blue,
                                         int grayscale)
{
	int x, y;
	float scale;

	if (!disable_red && !disable_green && !disable_blue && !grayscale)
		return;

	/* If only some components are selected, it's important to rescale the result
	 * appropriately so that e.g. if only blue is selected, it's not zeroed out.
	 */
	scale = (disable_red   ? 0.0f : 0.2126f) +
	        (disable_green ? 0.0f : 0.7152f) +
	        (disable_blue  ? 0.0f : 0.0722f);

	for (y = 0; y < ibuf->y; y++) {
		for (x = 0; x < ibuf->x; x++) {
			int pixel = ibuf->x * y + x;

			if (ibuf->rect_float) {
				float *rrgbf = ibuf->rect_float + pixel * 4;
				float r = disable_red   ? 0.0f : rrgbf[0];
				float g = disable_green ? 0.0f : rrgbf[1];
				float b = disable_blue  ? 0.0f : rrgbf[2];

				if (grayscale) {
					float gray = (0.2126f * r + 0.7152f * g + 0.0722f * b) / scale;

					rrgbf[0] = rrgbf[1] = rrgbf[2] = gray;
				}
				else {
					rrgbf[0] = r;
					rrgbf[1] = g;
					rrgbf[2] = b;
				}
			}
			else {
				char *rrgb = (char *)ibuf->rect + pixel * 4;
				char r = disable_red   ? 0 : rrgb[0];
				char g = disable_green ? 0 : rrgb[1];
				char b = disable_blue  ? 0 : rrgb[2];

				if (grayscale) {
					float gray = (0.2126f * r + 0.7152f * g + 0.0722f * b) / scale;

					rrgb[0] = rrgb[1] = rrgb[2] = gray;
				}
				else {
					rrgb[0] = r;
					rrgb[1] = g;
					rrgb[2] = b;
				}
			}
		}
	}

	if (ibuf->rect_float)
		ibuf->userflags |= IB_RECT_INVALID;
}

static void disable_imbuf_channels(ImBuf *ibuf, MovieTrackingTrack *track, int grayscale)
{
	BKE_tracking_disable_imbuf_channels(ibuf, track->flag & TRACK_DISABLE_RED,
	                                    track->flag & TRACK_DISABLE_GREEN, track->flag & TRACK_DISABLE_BLUE, grayscale);
}

ImBuf *BKE_tracking_sample_pattern_imbuf(int frame_width, int frame_height, ImBuf *search_ibuf,
                                         MovieTrackingTrack *track, MovieTrackingMarker *marker,
                                         int use_mask, int num_samples_x, int num_samples_y,
                                         float pos[2])
{
#ifdef WITH_LIBMV
	ImBuf *pattern_ibuf;
	double src_pixel_x[5], src_pixel_y[5];
	double warped_position_x, warped_position_y;
	float *mask = NULL;

	pattern_ibuf = IMB_allocImBuf(num_samples_x, num_samples_y, 32, IB_rectfloat);
	pattern_ibuf->profile = IB_PROFILE_LINEAR_RGB;

	if (!search_ibuf->rect_float) {
		IMB_float_from_rect(search_ibuf);
	}

	get_marker_coords_for_tracking(frame_width, frame_height, marker, src_pixel_x, src_pixel_y);

	if (use_mask) {
		mask = BKE_tracking_track_mask_get(frame_width, frame_height, track, marker);
	}

	libmv_samplePlanarPatch(search_ibuf->rect_float, search_ibuf->x, search_ibuf->y, 4,
	                        src_pixel_x, src_pixel_y, num_samples_x,
	                        num_samples_y, mask, pattern_ibuf->rect_float,
	                        &warped_position_x, &warped_position_y);

	if (pos) {
		pos[0] = warped_position_x;
		pos[1] = warped_position_y;
	}

	if (mask) {
		MEM_freeN(mask);
	}

	return pattern_ibuf;
#else
	ImBuf *pattern_ibuf;

	/* real sampling requires libmv, but areas are supposing pattern would be
	 * sampled if search area does exists, so we'll need to create empty
	 * pattern area here to prevent adding NULL-checks all over just to deal
	 * with situation when lubmv is disabled
	 */

	(void) frame_width;
	(void) frame_height;
	(void) search_ibuf;
	(void) marker;

	pattern_ibuf = IMB_allocImBuf(num_samples_x, num_samples_y, 32, IB_rectfloat);

	pos[0] = num_samples_x / 2.0f;
	pos[1] = num_samples_y / 2.0f;

	return pattern_ibuf;
#endif
}

ImBuf *BKE_tracking_get_pattern_imbuf(ImBuf *ibuf, MovieTrackingTrack *track, MovieTrackingMarker *marker,
                                      int anchored, int disable_channels)
{
	ImBuf *pattern_ibuf, *search_ibuf;
	float pat_min[2], pat_max[2];
	int num_samples_x, num_samples_y;

	BKE_tracking_marker_pattern_minmax(marker, pat_min, pat_max);

	num_samples_x = (pat_max[0] - pat_min[0]) * ibuf->x;
	num_samples_y = (pat_max[1] - pat_min[1]) * ibuf->y;

	search_ibuf = BKE_tracking_get_search_imbuf(ibuf, track, marker, anchored, disable_channels);

	pattern_ibuf = BKE_tracking_sample_pattern_imbuf(ibuf->x, ibuf->y, search_ibuf, track, marker,
	                                                 FALSE, num_samples_x, num_samples_y, NULL);

	IMB_freeImBuf(search_ibuf);

	return pattern_ibuf;
}

ImBuf *BKE_tracking_get_search_imbuf(ImBuf *ibuf, MovieTrackingTrack *track, MovieTrackingMarker *marker,
                                     int anchored, int disable_channels)
{
	ImBuf *searchibuf;
	int x, y, w, h;
	float search_origin[2];

	get_search_origin_frame_pixel(ibuf->x, ibuf->y, marker, search_origin);

	x = search_origin[0];
	y = search_origin[1];

	if (anchored) {
		x += track->offset[0] * ibuf->x;
		y += track->offset[1] * ibuf->y;
	}

	w = (marker->search_max[0] - marker->search_min[0]) * ibuf->x;
	h = (marker->search_max[1] - marker->search_min[1]) * ibuf->y;

	searchibuf = IMB_allocImBuf(w, h, 32, ibuf->rect_float ? IB_rectfloat : IB_rect);
	searchibuf->profile = ibuf->profile;

	IMB_rectcpy(searchibuf, ibuf, 0, 0, x, y, w, h);

	if (disable_channels) {
		if ((track->flag & TRACK_PREVIEW_GRAYSCALE) ||
		    (track->flag & TRACK_DISABLE_RED)       ||
		    (track->flag & TRACK_DISABLE_GREEN)     ||
		    (track->flag & TRACK_DISABLE_BLUE))
		{
			disable_imbuf_channels(searchibuf, track, TRUE);
		}
	}

	return searchibuf;
}

static bGPDlayer *track_mask_gpencil_layer_get(MovieTrackingTrack *track)
{
	bGPDlayer *layer;

	if (!track->gpd)
		return NULL;

	layer = track->gpd->layers.first;

	while (layer) {
		if (layer->flag & GP_LAYER_ACTIVE) {
			bGPDframe *frame = layer->frames.first;
			int ok = FALSE;

			while (frame) {
				if (frame->strokes.first) {
					ok = TRUE;
				}

				frame = frame->next;
			}

			if (ok)
				return layer;
		}

		layer = layer->next;
	}

	return NULL;
}

static void track_mask_gpencil_layer_rasterize(int frame_width, int frame_height,
											   MovieTrackingMarker *marker, bGPDlayer *layer,
                                               float *mask, int mask_width, int mask_height)
{
	bGPDframe *frame = layer->frames.first;

	while (frame) {
		bGPDstroke *stroke = frame->strokes.first;

		while (stroke) {
			bGPDspoint *stroke_points = stroke->points;
			float *mask_points, *fp;
			int i;

			if (stroke->flag & GP_STROKE_2DSPACE) {
				fp = mask_points = MEM_callocN(2 * stroke->totpoints * sizeof(float),
				                               "track mask rasterization points");

				for (i = 0; i < stroke->totpoints; i++, fp += 2) {
					fp[0] = (stroke_points[i].x - marker->search_min[0]) * frame_width / mask_width;
					fp[1] = (stroke_points[i].y - marker->search_min[1]) * frame_height / mask_height;
				}

				PLX_raskterize((float (*)[2])mask_points, stroke->totpoints, mask, mask_width, mask_height);

				MEM_freeN(mask_points);
			}

			stroke = stroke->next;
		}

		frame = frame->next;
	}
}

float *BKE_tracking_track_mask_get(int frame_width, int frame_height,
                                   MovieTrackingTrack *track, MovieTrackingMarker *marker)
{
	float *mask = NULL;
	bGPDlayer *layer = track_mask_gpencil_layer_get(track);
	int mask_width, mask_height;

	mask_width = (marker->search_max[0] - marker->search_min[0]) * frame_width;
	mask_height = (marker->search_max[1] - marker->search_min[1]) * frame_height;

	if (layer) {
		mask = MEM_callocN(mask_width * mask_height * sizeof(float), "track mask");

		track_mask_gpencil_layer_rasterize(frame_width, frame_height, marker, layer,
		                                   mask, mask_width, mask_height);
	}

	return mask;
}

#ifdef WITH_LIBMV

/* Convert from float and byte RGBA to grayscale. Supports different coefficients for RGB. */
static void float_rgba_to_gray(const float *rgba, float *gray, int num_pixels,
                               float weight_red, float weight_green, float weight_blue)
{
	int i;

	for (i = 0; i < num_pixels; i++) {
		const float *pixel = rgba + 4 * i;

		gray[i] = weight_red * pixel[0] + weight_green * pixel[1] + weight_blue * pixel[2];
	}
}

static void uint8_rgba_to_float_gray(const unsigned char *rgba, float *gray, int num_pixels,
                                     float weight_red, float weight_green, float weight_blue)
{
	int i;

	for (i = 0; i < num_pixels; i++) {
		const unsigned char *pixel = rgba + i * 4;

		*gray++ = (weight_red * pixel[0] + weight_green * pixel[1] + weight_blue * pixel[2]) / 255.0f;
	}
}

static float *get_search_floatbuf(ImBuf *ibuf, MovieTrackingTrack *track, MovieTrackingMarker *marker,
                                  int *width_r, int *height_r)
{
	ImBuf *searchibuf;
	float *gray_pixels;
	int width, height;

	searchibuf = BKE_tracking_get_search_imbuf(ibuf, track, marker, FALSE, TRUE);

	width = searchibuf->x;
	height = searchibuf->y;

	*width_r = searchibuf->x;
	*height_r = searchibuf->y;

	gray_pixels = MEM_callocN(width * height * sizeof(float), "tracking floatBuf");

	if (searchibuf->rect_float) {
		float_rgba_to_gray(searchibuf->rect_float, gray_pixels, width * height,
		                   0.2126f, 0.7152f, 0.0722f);
	}
	else {
		uint8_rgba_to_float_gray((unsigned char *)searchibuf->rect, gray_pixels, width * height,
		                         0.2126f, 0.7152f, 0.0722f);
	}

	IMB_freeImBuf(searchibuf);

	return gray_pixels;
}

static unsigned char *get_ucharbuf(ImBuf *ibuf)
{
	int x, y;
	unsigned char *pixels, *cp;

	cp = pixels = MEM_callocN(ibuf->x * ibuf->y * sizeof(unsigned char), "tracking ucharBuf");
	for (y = 0; y < ibuf->y; y++) {
		for (x = 0; x < ibuf->x; x++) {
			int pixel = ibuf->x * y + x;

			if (ibuf->rect_float) {
				const float *rrgbf = ibuf->rect_float + pixel * 4;
				const float grey_f = 0.2126f * rrgbf[0] + 0.7152f * rrgbf[1] + 0.0722f * rrgbf[2];

				*cp = FTOCHAR(grey_f);
			}
			else {
				const unsigned char *rrgb = (unsigned char *)ibuf->rect + pixel * 4;

				*cp = 0.2126f * rrgb[0] + 0.7152f * rrgb[1] + 0.0722f * rrgb[2];
			}

			cp++;
		}
	}

	return pixels;
}

static ImBuf *get_frame_ibuf(MovieTrackingContext *context, int framenr)
{
	ImBuf *ibuf;
	MovieClipUser user = context->user;

	user.framenr = BKE_movieclip_remap_clip_to_scene_frame(context->clip, framenr);

	ibuf = BKE_movieclip_get_ibuf_flag(context->clip, &user, context->clip_flag, MOVIECLIP_CACHE_SKIP);

	return ibuf;
}

static ImBuf *get_keyframed_ibuf(MovieTrackingContext *context, MovieTrackingTrack *track,
                                 MovieTrackingMarker *marker, MovieTrackingMarker **marker_keyed)
{
	int framenr = marker->framenr;
	int a = marker - track->markers;

	*marker_keyed = marker;

	while (a >= 0 && a < track->markersnr) {
		int next = (context->backwards) ? a + 1 : a - 1;
		int is_keyframed = FALSE;
		MovieTrackingMarker *cur_marker = &track->markers[a];
		MovieTrackingMarker *next_marker = NULL;

		if (next >= 0 && next < track->markersnr)
			next_marker = &track->markers[next];

		/* if next mrker is disabled, stop searching keyframe and use current frame as keyframe */
		if (next_marker && next_marker->flag & MARKER_DISABLED)
			is_keyframed = TRUE;

		is_keyframed |= (cur_marker->flag & MARKER_TRACKED) == 0;

		if (is_keyframed) {
			framenr = cur_marker->framenr;
			*marker_keyed = cur_marker;

			break;
		}

		a = next;
	}

	return get_frame_ibuf(context, framenr);
}

static ImBuf *get_adjust_ibuf(MovieTrackingContext *context, MovieTrackingTrack *track, MovieTrackingMarker *marker,
                              int curfra, MovieTrackingMarker **marker_keyed)
{
	ImBuf *ibuf = NULL;

	if (track->pattern_match == TRACK_MATCH_KEYFRAME) {
		ibuf = get_keyframed_ibuf(context, track, marker, marker_keyed);
	}
	else {
		ibuf = get_frame_ibuf(context, curfra);

		/* use current marker as keyframed position */
		*marker_keyed = marker;
	}

	return ibuf;
}

static void marker_search_scale_after_tracking(const MovieTrackingMarker *old_marker, MovieTrackingMarker *new_marker)
{
	float old_pat_min[2], old_pat_max[2];
	float new_pat_min[2], new_pat_max[2];
	float scale_x, scale_y;

	BKE_tracking_marker_pattern_minmax(old_marker, old_pat_min, old_pat_max);
	BKE_tracking_marker_pattern_minmax(new_marker, new_pat_min, new_pat_max);

	scale_x = (new_pat_max[0] - new_pat_min[0]) / (old_pat_max[0] - old_pat_min[0]);
	scale_y = (new_pat_max[1] - new_pat_min[1]) / (old_pat_max[1] - old_pat_min[1]);

	new_marker->search_min[0] *= scale_x;
	new_marker->search_min[1] *= scale_y;

	new_marker->search_max[0] *= scale_x;
	new_marker->search_max[1] *= scale_y;
}

#endif

void BKE_tracking_sync(MovieTrackingContext *context)
{
	MovieTracking *tracking = &context->clip->tracking;
	int newframe;

	tracks_map_merge(context->tracks_map, tracking);

	if (context->backwards)
		newframe = context->user.framenr + 1;
	else
		newframe = context->user.framenr - 1;

	context->sync_frame = newframe;

	tracking->dopesheet.ok = FALSE;
}

void BKE_tracking_sync_user(MovieClipUser *user, MovieTrackingContext *context)
{
	user->framenr = context->sync_frame;
}

int BKE_tracking_next(MovieTrackingContext *context)
{
	ImBuf *destination_ibuf;
	int curfra =  BKE_movieclip_remap_scene_to_clip_frame(context->clip, context->user.framenr);
	int a, ok = FALSE, map_size;

	int frame_width, frame_height;

	map_size = tracks_map_size(context->tracks_map);

	/* nothing to track, avoid unneeded frames reading to save time and memory */
	if (!map_size)
		return FALSE;

	if (context->backwards)
		context->user.framenr--;
	else
		context->user.framenr++;

	destination_ibuf = BKE_movieclip_get_ibuf_flag(context->clip, &context->user,
	                                               context->clip_flag, MOVIECLIP_CACHE_SKIP);
	if (!destination_ibuf)
		return FALSE;

	frame_width = destination_ibuf->x;
	frame_height = destination_ibuf->y;

	//#pragma omp parallel for private(a) shared(destination_ibuf, ok) if (map_size>1)
	for (a = 0; a < map_size; a++) {
		TrackContext *track_context = NULL;
		MovieTrackingTrack *track;
		MovieTrackingMarker *marker;

		tracks_map_get(context->tracks_map, a, &track, (void **)&track_context);

		marker = BKE_tracking_exact_marker(track, curfra);

		if (marker && (marker->flag & MARKER_DISABLED) == 0) {
#ifdef WITH_LIBMV
			int width, height, tracked = 0, need_readjust = 0;
			float margin[2], dim[2], pat_min[2], pat_max[2];
			MovieTrackingMarker marker_new, *marker_keyed;
			int onbound = FALSE, nextfra;
			double dst_pixel_x[5], dst_pixel_y[5];

			if (track->pattern_match == TRACK_MATCH_KEYFRAME)
				need_readjust = context->first_time;
			else
				need_readjust = TRUE;

			if (context->backwards)
				nextfra = curfra - 1;
			else
				nextfra = curfra + 1;

			/* margin from frame boundaries */
			BKE_tracking_marker_pattern_minmax(marker, pat_min, pat_max);
			sub_v2_v2v2(dim, pat_max, pat_min);
			margin[0] = margin[1] = MAX2(dim[0], dim[1]) / 2.0f;

			margin[0] = MAX2(margin[0], (float)track->margin / destination_ibuf->x);
			margin[1] = MAX2(margin[1], (float)track->margin / destination_ibuf->y);

			/* do not track markers which are too close to boundary */
			if (marker->pos[0] < margin[0] || marker->pos[0] > 1.0f - margin[0] ||
			    marker->pos[1] < margin[1] || marker->pos[1] > 1.0f - margin[1])
			{
				onbound = TRUE;
			}
			else {
				/* to convert to the x/y split array format for libmv. */
				double src_pixel_x[5];
				double src_pixel_y[5];

				/* settings for the tracker */
				struct libmv_trackRegionOptions options = {0};
				struct libmv_trackRegionResult result;

				float *patch_new;

				if (need_readjust) {
					ImBuf *reference_ibuf = NULL;
					/* calculate patch for keyframed position */
					reference_ibuf = get_adjust_ibuf(context, track, marker, curfra, &marker_keyed);
					track_context->marker = *marker_keyed;

					if (track_context->search_area)
						MEM_freeN(track_context->search_area);

					track_context->search_area = get_search_floatbuf(reference_ibuf, track,
					                                                 marker_keyed, &width, &height);
					track_context->search_area_height = height;
					track_context->search_area_width = width;

					IMB_freeImBuf(reference_ibuf);

					if ((track->algorithm_flag & TRACK_ALGORITHM_FLAG_USE_MASK) != 0) {
						if (track_context->mask)
							MEM_freeN(track_context->mask);

						track_context->mask = BKE_tracking_track_mask_get(frame_width, frame_height,
						                                                  track, marker);
					}
				}

				/* for now track to the same search area dimension as marker has got for current frame
				 * will make all tracked markers in currently tracked segment have the same search area
				 * size, but it's quite close to what is actually needed
				 */
				patch_new = get_search_floatbuf(destination_ibuf, track, marker, &width, &height);

				/* Configure the tracker */
				options.motion_model = track->motion_model;

				options.use_brute =
				    ((track->algorithm_flag & TRACK_ALGORITHM_FLAG_USE_BRUTE) != 0);

				options.use_normalization =
				    ((track->algorithm_flag & TRACK_ALGORITHM_FLAG_USE_NORMALIZATION) != 0);

				options.num_iterations = 50;
				options.minimum_correlation = track->minimum_correlation;
				options.sigma = 0.9;

				if ((track->algorithm_flag & TRACK_ALGORITHM_FLAG_USE_MASK) != 0)
					options.image1_mask = track_context->mask;

				/* Convert the marker corners and center into pixel coordinates in the search/destination images. */
				get_marker_coords_for_tracking(frame_width, frame_height, &track_context->marker, src_pixel_x, src_pixel_y);
				get_marker_coords_for_tracking(frame_width, frame_height, marker, dst_pixel_x, dst_pixel_y);

				/* Run the tracker! */
				tracked = libmv_trackRegion(&options,
				                            track_context->search_area,
											track_context->search_area_width,
											track_context->search_area_height,
											patch_new, width, height,
				                            src_pixel_x, src_pixel_y,
				                            &result,
				                            dst_pixel_x, dst_pixel_y);
				MEM_freeN(patch_new);
			}

			if (tracked && !onbound) {
				memset(&marker_new, 0, sizeof(marker_new));
				marker_new = *marker;
				set_marker_coords_from_tracking(frame_width, frame_height, &marker_new, dst_pixel_x, dst_pixel_y);
				marker_new.flag |= MARKER_TRACKED;
				marker_new.framenr = nextfra;

				marker_search_scale_after_tracking(marker, &marker_new);

				//#pragma omp critical
				{
					if (context->first_time) {
						/* check if there's no keyframe/tracked markers before tracking marker.
						 * if so -- create disabled marker before currently tracking "segment"
						 */

						put_disabled_marker(track, &marker_new, !context->backwards, 0);
					}

					/* insert currently tracked marker */
					BKE_tracking_insert_marker(track, &marker_new);

					/* make currently tracked segment be finished with disabled marker */
					put_disabled_marker(track, &marker_new, context->backwards, 0);
				}
			}
			else {
				marker_new = *marker;

				marker_new.framenr = nextfra;
				marker_new.flag |= MARKER_DISABLED;

				//#pragma omp critical
				{
					BKE_tracking_insert_marker(track, &marker_new);
				}
			}

			ok = TRUE;
#else
			(void)frame_height;
			(void)frame_width;
#endif
		}
	}

	IMB_freeImBuf(destination_ibuf);

	context->first_time = FALSE;
	context->frames++;

	return ok;
}

/*********************** camera solving *************************/

typedef struct MovieReconstructContext {
#ifdef WITH_LIBMV
	struct libmv_Tracks *tracks;
	int keyframe1, keyframe2;
	short refine_flags;

	struct libmv_Reconstruction *reconstruction;
#endif
	char object_name[MAX_NAME];
	int is_camera;
	short motion_flag;

	float focal_length;
	float principal_point[2];
	float k1, k2, k3;

	float reprojection_error;

	TracksMap *tracks_map;

	int sfra, efra;
} MovieReconstructContext;

typedef struct ReconstructProgressData {
	short *stop;
	short *do_update;
	float *progress;
	char *stats_message;
	int message_size;
} ReconstructProgressData;

#if WITH_LIBMV
static struct libmv_Tracks *create_libmv_tracks(ListBase *tracksbase, int width, int height)
{
	int tracknr = 0;
	MovieTrackingTrack *track;
	struct libmv_Tracks *tracks = libmv_tracksNew();

	track = tracksbase->first;
	while (track) {
		int a = 0;

		for (a = 0; a < track->markersnr; a++) {
			MovieTrackingMarker *marker = &track->markers[a];

			if ((marker->flag & MARKER_DISABLED) == 0) {
				libmv_tracksInsert(tracks, marker->framenr, tracknr,
				                   marker->pos[0] * width, marker->pos[1] * height);
			}
		}

		track = track->next;
		tracknr++;
	}

	return tracks;
}

static void retrieve_libmv_reconstruct_intrinscis(MovieReconstructContext *context, MovieTracking *tracking)
{
	struct libmv_Reconstruction *libmv_reconstruction = context->reconstruction;
	struct libmv_CameraIntrinsics *libmv_intrinsics = libmv_ReconstructionExtractIntrinsics(libmv_reconstruction);

	float aspy = 1.0f / tracking->camera.pixel_aspect;

	double focal_length, principal_x, principal_y, k1, k2, k3;
	int width, height;

	libmv_CameraIntrinsicsExtract(libmv_intrinsics, &focal_length, &principal_x, &principal_y,
	                              &k1, &k2, &k3, &width, &height);

	tracking->camera.focal = focal_length;
	tracking->camera.principal[0] = principal_x;

	tracking->camera.principal[1] = principal_y / aspy;
	tracking->camera.k1 = k1;
	tracking->camera.k2 = k2;
}

static int retrieve_libmv_reconstruct_tracks(MovieReconstructContext *context, MovieTracking *tracking)
{
	struct libmv_Reconstruction *libmv_reconstruction = context->reconstruction;
	MovieTrackingReconstruction *reconstruction = NULL;
	MovieReconstructedCamera *reconstructed;
	MovieTrackingTrack *track;
	ListBase *tracksbase =  NULL;
	int ok = TRUE, tracknr = 0, a, origin_set = FALSE;
	int sfra = context->sfra, efra = context->efra;
	float imat[4][4];

	if (context->is_camera) {
		tracksbase = &tracking->tracks;
		reconstruction = &tracking->reconstruction;
	}
	else {
		MovieTrackingObject *object = BKE_tracking_named_object(tracking, context->object_name);

		tracksbase = &object->tracks;
		reconstruction = &object->reconstruction;
	}

	unit_m4(imat);

	track = tracksbase->first;
	while (track) {
		double pos[3];

		if (libmv_reporojectionPointForTrack(libmv_reconstruction, tracknr, pos)) {
			track->bundle_pos[0] = pos[0];
			track->bundle_pos[1] = pos[1];
			track->bundle_pos[2] = pos[2];

			track->flag |= TRACK_HAS_BUNDLE;
			track->error = libmv_reporojectionErrorForTrack(libmv_reconstruction, tracknr);
		}
		else {
			track->flag &= ~TRACK_HAS_BUNDLE;
			ok = FALSE;

			printf("No bundle for track #%d '%s'\n", tracknr, track->name);
		}

		track = track->next;
		tracknr++;
	}

	if (reconstruction->cameras)
		MEM_freeN(reconstruction->cameras);

	reconstruction->camnr = 0;
	reconstruction->cameras = NULL;
	reconstructed = MEM_callocN((efra - sfra + 1) * sizeof(MovieReconstructedCamera),
	                            "temp reconstructed camera");

	for (a = sfra; a <= efra; a++) {
		double matd[4][4];

		if (libmv_reporojectionCameraForImage(libmv_reconstruction, a, matd)) {
			int i, j;
			float mat[4][4];
			float error = libmv_reporojectionErrorForImage(libmv_reconstruction, a);

			for (i = 0; i < 4; i++)
				for (j = 0; j < 4; j++)
					mat[i][j] = matd[i][j];

			if (!origin_set) {
				copy_m4_m4(imat, mat);
				invert_m4(imat);
				origin_set = TRUE;
			}

			if (origin_set)
				mult_m4_m4m4(mat, imat, mat);

			copy_m4_m4(reconstructed[reconstruction->camnr].mat, mat);
			reconstructed[reconstruction->camnr].framenr = a;
			reconstructed[reconstruction->camnr].error = error;
			reconstruction->camnr++;
		}
		else {
			ok = FALSE;
			printf("No camera for frame %d\n", a);
		}
	}

	if (reconstruction->camnr) {
		int size = reconstruction->camnr * sizeof(MovieReconstructedCamera);
		reconstruction->cameras = MEM_callocN(size, "reconstructed camera");
		memcpy(reconstruction->cameras, reconstructed, size);
	}

	if (origin_set) {
		track = tracksbase->first;
		while (track) {
			if (track->flag & TRACK_HAS_BUNDLE)
				mul_v3_m4v3(track->bundle_pos, imat, track->bundle_pos);

			track = track->next;
		}
	}

	MEM_freeN(reconstructed);

	return ok;
}

static int retrieve_libmv_reconstruct(MovieReconstructContext *context, MovieTracking *tracking)
{
	/* take the intrinscis back from libmv */
	retrieve_libmv_reconstruct_intrinscis(context, tracking);

	return retrieve_libmv_reconstruct_tracks(context, tracking);
}

static int get_refine_intrinsics_flags(MovieTracking *tracking, MovieTrackingObject *object)
{
	int refine = tracking->settings.refine_camera_intrinsics;
	int flags = 0;

	if ((object->flag & TRACKING_OBJECT_CAMERA) == 0)
		return 0;

	if (refine & REFINE_FOCAL_LENGTH)
		flags |= LIBMV_REFINE_FOCAL_LENGTH;

	if (refine & REFINE_PRINCIPAL_POINT)
		flags |= LIBMV_REFINE_PRINCIPAL_POINT;

	if (refine & REFINE_RADIAL_DISTORTION_K1)
		flags |= REFINE_RADIAL_DISTORTION_K1;

	if (refine & REFINE_RADIAL_DISTORTION_K2)
		flags |= REFINE_RADIAL_DISTORTION_K2;

	return flags;
}

static int count_tracks_on_both_keyframes(MovieTracking *tracking, ListBase *tracksbase)
{
	int tot = 0;
	int frame1 = tracking->settings.keyframe1, frame2 = tracking->settings.keyframe2;
	MovieTrackingTrack *track;

	track = tracksbase->first;
	while (track) {
		if (BKE_tracking_has_enabled_marker(track, frame1))
			if (BKE_tracking_has_enabled_marker(track, frame2))
				tot++;

		track = track->next;
	}

	return tot;
}
#endif

int BKE_tracking_can_reconstruct(MovieTracking *tracking, MovieTrackingObject *object, char *error_msg, int error_size)
{
#if WITH_LIBMV
	ListBase *tracksbase = BKE_tracking_object_tracks(tracking, object);

	if (tracking->settings.motion_flag & TRACKING_MOTION_MODAL) {
		/* TODO: check for number of tracks? */
		return TRUE;
	}
	else if (count_tracks_on_both_keyframes(tracking, tracksbase) < 8) {
		BLI_strncpy(error_msg, "At least 8 common tracks on both of keyframes are needed for reconstruction",
		            error_size);

		return FALSE;
	}

	return TRUE;
#else
	BLI_strncpy(error_msg, "Blender is compiled without motion tracking library", error_size);

	(void) tracking;
	(void) object;

	return 0;
#endif
}

MovieReconstructContext *BKE_tracking_reconstruction_context_new(MovieTracking *tracking, MovieTrackingObject *object,
                                                                 int keyframe1, int keyframe2, int width, int height)
{
	MovieReconstructContext *context = MEM_callocN(sizeof(MovieReconstructContext), "MovieReconstructContext data");
	MovieTrackingCamera *camera = &tracking->camera;
	ListBase *tracksbase = BKE_tracking_object_tracks(tracking, object);
	float aspy = 1.0f / tracking->camera.pixel_aspect;
	int num_tracks = BLI_countlist(tracksbase);
	int sfra = INT_MAX, efra = INT_MIN;
	MovieTrackingTrack *track;

	BLI_strncpy(context->object_name, object->name, sizeof(context->object_name));
	context->is_camera = object->flag & TRACKING_OBJECT_CAMERA;
	context->motion_flag = tracking->settings.motion_flag;

	context->tracks_map = tracks_map_new(context->object_name, context->is_camera, num_tracks, 0);

	track = tracksbase->first;
	while (track) {
		int first = 0, last = track->markersnr - 1;
		MovieTrackingMarker *first_marker = &track->markers[0];
		MovieTrackingMarker *last_marker = &track->markers[track->markersnr - 1];

		/* find first not-disabled marker */
		while (first <= track->markersnr - 1 && first_marker->flag & MARKER_DISABLED) {
			first++;
			first_marker++;
		}

		/* find last not-disabled marker */
		while (last >= 0 && last_marker->flag & MARKER_DISABLED) {
			last--;
			last_marker--;
		}

		if (first < track->markersnr - 1)
			sfra = MIN2(sfra, first_marker->framenr);

		if (last >= 0)
			efra = MAX2(efra, last_marker->framenr);

		tracks_map_insert(context->tracks_map, track, NULL);

		track = track->next;
	}

	context->sfra = sfra;
	context->efra = efra;

#ifdef WITH_LIBMV
	context->tracks = create_libmv_tracks(tracksbase, width, height * aspy);
	context->keyframe1 = keyframe1;
	context->keyframe2 = keyframe2;
	context->refine_flags = get_refine_intrinsics_flags(tracking, object);
#else
	(void) width;
	(void) height;
	(void) keyframe1;
	(void) keyframe2;
#endif

	context->focal_length = camera->focal;
	context->principal_point[0] = camera->principal[0];
	context->principal_point[1] = camera->principal[1] * aspy;

	context->k1 = camera->k1;
	context->k2 = camera->k2;
	context->k3 = camera->k3;

	return context;
}

void BKE_tracking_reconstruction_context_free(MovieReconstructContext *context)
{
#ifdef WITH_LIBMV
	if (context->reconstruction)
		libmv_destroyReconstruction(context->reconstruction);

	libmv_tracksDestroy(context->tracks);
#endif

	tracks_map_free(context->tracks_map, NULL);

	MEM_freeN(context);
}

#ifdef WITH_LIBMV
static void solve_reconstruction_update_cb(void *customdata, double progress, const char *message)
{
	ReconstructProgressData *progressdata = customdata;

	if (progressdata->progress) {
		*progressdata->progress = progress;
		*progressdata->do_update = TRUE;
	}

	BLI_snprintf(progressdata->stats_message, progressdata->message_size, "Solving camera | %s", message);
}
#endif

#if 0
static int solve_reconstruction_testbreak_cb(void *customdata)
{
	ReconstructProgressData *progressdata = customdata;

	if (progressdata->stop && *progressdata->stop)
		return TRUE;

	return G.afbreek;
}
#endif

void BKE_tracking_solve_reconstruction(MovieReconstructContext *context, short *stop, short *do_update,
                                       float *progress, char *stats_message, int message_size)
{
#ifdef WITH_LIBMV
	float error;

	ReconstructProgressData progressdata;

	progressdata.stop = stop;
	progressdata.do_update = do_update;
	progressdata.progress = progress;
	progressdata.stats_message = stats_message;
	progressdata.message_size = message_size;

	if (context->motion_flag & TRACKING_MOTION_MODAL) {
		context->reconstruction = libmv_solveModal(context->tracks,
		                                           context->focal_length,
		                                           context->principal_point[0], context->principal_point[1],
		                                           context->k1, context->k2, context->k3,
		                                           solve_reconstruction_update_cb, &progressdata);
	}
	else {
		context->reconstruction = libmv_solveReconstruction(context->tracks,
		                                                    context->keyframe1, context->keyframe2,
		                                                    context->refine_flags,
		                                                    context->focal_length,
		                                                    context->principal_point[0], context->principal_point[1],
		                                                    context->k1, context->k2, context->k3,
		                                                    solve_reconstruction_update_cb, &progressdata);
	}

	error = libmv_reprojectionError(context->reconstruction);

	context->reprojection_error = error;
#else
	(void) context;
	(void) stop;
	(void) do_update;
	(void) progress;
	(void) stats_message;
	(void) message_size;
#endif
}

int BKE_tracking_finish_reconstruction(MovieReconstructContext *context, MovieTracking *tracking)
{
	MovieTrackingReconstruction *reconstruction;

	tracks_map_merge(context->tracks_map, tracking);

	if (context->is_camera) {
		reconstruction = &tracking->reconstruction;
	}
	else {
		MovieTrackingObject *object;

		object = BKE_tracking_named_object(tracking, context->object_name);
		reconstruction = &object->reconstruction;
	}

	reconstruction->error = context->reprojection_error;
	reconstruction->flag |= TRACKING_RECONSTRUCTED;

#ifdef WITH_LIBMV
	if (!retrieve_libmv_reconstruct(context, tracking))
		return FALSE;
#endif

	return TRUE;
}

void BKE_track_unique_name(ListBase *tracksbase, MovieTrackingTrack *track)
{
	BLI_uniquename(tracksbase, track, "Track", '.', offsetof(MovieTrackingTrack, name), sizeof(track->name));
}

MovieTrackingTrack *BKE_tracking_named_track(MovieTracking *tracking, MovieTrackingObject *object, const char *name)
{
	ListBase *tracksbase = BKE_tracking_object_tracks(tracking, object);
	MovieTrackingTrack *track = tracksbase->first;

	while (track) {
		if (!strcmp(track->name, name))
			return track;

		track = track->next;
	}

	return NULL;
}

static int reconstruction_camera_index(MovieTrackingReconstruction *reconstruction, int framenr, int nearest)
{
	MovieReconstructedCamera *cameras = reconstruction->cameras;
	int a = 0, d = 1;

	if (!reconstruction->camnr)
		return -1;

	if (framenr < cameras[0].framenr) {
		if (nearest)
			return 0;
		else
			return -1;
	}

	if (framenr > cameras[reconstruction->camnr - 1].framenr) {
		if (nearest)
			return reconstruction->camnr - 1;
		else
			return -1;
	}

	if (reconstruction->last_camera < reconstruction->camnr)
		a = reconstruction->last_camera;

	if (cameras[a].framenr >= framenr)
		d = -1;

	while (a >= 0 && a < reconstruction->camnr) {
		int cfra = cameras[a].framenr;

		/* check if needed framenr was "skipped" -- no data for requested frame */

		if (d > 0 && cfra > framenr) {
			/* interpolate with previous position */
			if (nearest)
				return a - 1;
			else
				break;
		}

		if (d < 0 && cfra < framenr) {
			/* interpolate with next position */
			if (nearest)
				return a;
			else
				break;
		}

		if (cfra == framenr) {
			reconstruction->last_camera = a;

			return a;
		}

		a += d;
	}

	return -1;
}

static void scale_reconstructed_camera(MovieTrackingObject *object, float mat[4][4])
{
	if ((object->flag & TRACKING_OBJECT_CAMERA) == 0) {
		float smat[4][4];

		scale_m4_fl(smat, 1.0f / object->scale);
		mult_m4_m4m4(mat, mat, smat);
	}
}

MovieReconstructedCamera *BKE_tracking_get_reconstructed_camera(MovieTracking *tracking,
                                                                MovieTrackingObject *object, int framenr)
{
	MovieTrackingReconstruction *reconstruction;
	int a;

	reconstruction = BKE_tracking_object_reconstruction(tracking, object);
	a = reconstruction_camera_index(reconstruction, framenr, FALSE);

	if (a == -1)
		return NULL;

	return &reconstruction->cameras[a];
}

void BKE_tracking_get_interpolated_camera(MovieTracking *tracking, MovieTrackingObject *object,
                                          int framenr, float mat[4][4])
{
	MovieTrackingReconstruction *reconstruction;
	MovieReconstructedCamera *cameras;
	int a;

	reconstruction = BKE_tracking_object_reconstruction(tracking, object);
	cameras = reconstruction->cameras;
	a = reconstruction_camera_index(reconstruction, framenr, 1);

	if (a == -1) {
		unit_m4(mat);

		return;
	}

	if (cameras[a].framenr != framenr && a > 0 && a < reconstruction->camnr - 1) {
		float t = ((float)framenr - cameras[a].framenr) / (cameras[a + 1].framenr - cameras[a].framenr);

		blend_m4_m4m4(mat, cameras[a].mat, cameras[a + 1].mat, t);
	}
	else {
		copy_m4_m4(mat, cameras[a].mat);
	}

	scale_reconstructed_camera(object, mat);
}

void BKE_get_tracking_mat(Scene *scene, Object *ob, float mat[4][4])
{
	if (!ob) {
		if (scene->camera)
			ob = scene->camera;
		else
			ob = BKE_scene_camera_find(scene);
	}

	if (ob)
		BKE_object_where_is_calc_mat4(scene, ob, mat);
	else
		unit_m4(mat);
}

void BKE_tracking_camera_shift(MovieTracking *tracking, int winx, int winy, float *shiftx, float *shifty)
{
	/* indeed in both of cases it should be winx -- it's just how camera shift works for blender's camera */
	*shiftx = (0.5f * winx - tracking->camera.principal[0]) / winx;
	*shifty = (0.5f * winy - tracking->camera.principal[1]) / winx;
}

void BKE_tracking_camera_to_blender(MovieTracking *tracking, Scene *scene, Camera *camera, int width, int height)
{
	float focal = tracking->camera.focal;

	camera->sensor_x = tracking->camera.sensor_width;
	camera->sensor_fit = CAMERA_SENSOR_FIT_AUTO;
	camera->lens = focal * camera->sensor_x / width;

	scene->r.xsch = width * tracking->camera.pixel_aspect;
	scene->r.ysch = height;

	scene->r.xasp = 1.0f;
	scene->r.yasp = 1.0f;

	BKE_tracking_camera_shift(tracking, width, height, &camera->shiftx, &camera->shifty);
}

void BKE_tracking_projection_matrix(MovieTracking *tracking, MovieTrackingObject *object,
                                    int framenr, int winx, int winy, float mat[4][4])
{
	MovieReconstructedCamera *camera;
	float lens = tracking->camera.focal * tracking->camera.sensor_width / (float)winx;
	float viewfac, pixsize, left, right, bottom, top, clipsta, clipend;
	float winmat[4][4];
	float ycor =  1.0f / tracking->camera.pixel_aspect;
	float shiftx, shifty, winside = MAX2(winx, winy);

	BKE_tracking_camera_shift(tracking, winx, winy, &shiftx, &shifty);

	clipsta = 0.1f;
	clipend = 1000.0f;

	if (winx >= winy)
		viewfac = (lens * winx) / tracking->camera.sensor_width;
	else
		viewfac = (ycor * lens * winy) / tracking->camera.sensor_width;

	pixsize = clipsta / viewfac;

	left = -0.5f * (float)winx + shiftx * winside;
	bottom = -0.5f * (ycor) * (float)winy + shifty * winside;
	right =  0.5f * (float)winx + shiftx * winside;
	top =  0.5f * (ycor) * (float)winy + shifty * winside;

	left *= pixsize;
	right *= pixsize;
	bottom *= pixsize;
	top *= pixsize;

	perspective_m4(winmat, left, right, bottom, top, clipsta, clipend);

	camera = BKE_tracking_get_reconstructed_camera(tracking, object, framenr);

	if (camera) {
		float imat[4][4];

		invert_m4_m4(imat, camera->mat);
		mult_m4_m4m4(mat, winmat, imat);
	}
	else copy_m4_m4(mat, winmat);
}

ListBase *BKE_tracking_get_tracks(MovieTracking *tracking)
{
	MovieTrackingObject *object = BKE_tracking_active_object(tracking);

	if (object && (object->flag & TRACKING_OBJECT_CAMERA) == 0) {
		return &object->tracks;
	}

	return &tracking->tracks;
}

MovieTrackingTrack *BKE_tracking_active_track(MovieTracking *tracking)
{
	ListBase *tracksbase;

	if (!tracking->act_track)
		return NULL;

	tracksbase = BKE_tracking_get_tracks(tracking);

	/* check that active track is in current tracks list */
	if (BLI_findindex(tracksbase, tracking->act_track) >= 0)
		return tracking->act_track;

	return NULL;
}

MovieTrackingObject *BKE_tracking_active_object(MovieTracking *tracking)
{
	return BLI_findlink(&tracking->objects, tracking->objectnr);
}

MovieTrackingObject *BKE_tracking_get_camera_object(MovieTracking *tracking)
{
	MovieTrackingObject *object = tracking->objects.first;

	while (object) {
		if (object->flag & TRACKING_OBJECT_CAMERA)
			return object;

		object = object->next;
	}

	return NULL;
}

ListBase *BKE_tracking_object_tracks(MovieTracking *tracking, MovieTrackingObject *object)
{
	if (object->flag & TRACKING_OBJECT_CAMERA) {
		return &tracking->tracks;
	}

	return &object->tracks;
}

MovieTrackingReconstruction *BKE_tracking_object_reconstruction(MovieTracking *tracking, MovieTrackingObject *object)
{
	if (object->flag & TRACKING_OBJECT_CAMERA) {
		return &tracking->reconstruction;
	}

	return &object->reconstruction;
}

MovieTrackingReconstruction *BKE_tracking_get_reconstruction(MovieTracking *tracking)
{
	MovieTrackingObject *object = BKE_tracking_active_object(tracking);

	return BKE_tracking_object_reconstruction(tracking, object);
}

void BKE_tracking_apply_intrinsics(MovieTracking *tracking, float co[2], float nco[2])
{
	MovieTrackingCamera *camera = &tracking->camera;

#ifdef WITH_LIBMV
	double x, y;
	float aspy = 1.0f / tracking->camera.pixel_aspect;

	/* normalize coords */
	x = (co[0] - camera->principal[0]) / camera->focal;
	y = (co[1] - camera->principal[1] * aspy) / camera->focal;

	libmv_applyCameraIntrinsics(camera->focal, camera->principal[0], camera->principal[1] * aspy,
	                            camera->k1, camera->k2, camera->k3, x, y, &x, &y);

	/* result is in image coords already */
	nco[0] = x;
	nco[1] = y;
#else
	(void) camera;
	(void) co;
	(void) nco;
#endif
}

void BKE_tracking_invert_intrinsics(MovieTracking *tracking, float co[2], float nco[2])
{
	MovieTrackingCamera *camera = &tracking->camera;

#ifdef WITH_LIBMV
	double x = co[0], y = co[1];
	float aspy = 1.0f / tracking->camera.pixel_aspect;

	libmv_InvertIntrinsics(camera->focal, camera->principal[0], camera->principal[1] * aspy,
	                       camera->k1, camera->k2, camera->k3, x, y, &x, &y);

	nco[0] = x * camera->focal + camera->principal[0];
	nco[1] = y * camera->focal + camera->principal[1] * aspy;
#else
	(void) camera;
	(void) co;
	(void) nco;
#endif
}

#ifdef WITH_LIBMV
static int point_in_stroke(bGPDstroke *stroke, float x, float y)
{
	int i, prev;
	int count = 0;
	bGPDspoint *points = stroke->points;

	prev = stroke->totpoints - 1;

	for (i = 0; i < stroke->totpoints; i++) {
		if ((points[i].y < y && points[prev].y >= y) || (points[prev].y < y && points[i].y >= y)) {
			float fac = (y - points[i].y) / (points[prev].y - points[i].y);

			if (points[i].x + fac * (points[prev].x - points[i].x) < x)
				count++;
		}

		prev = i;
	}

	return count % 2;
}

static int point_in_layer(bGPDlayer *layer, float x, float y)
{
	bGPDframe *frame = layer->frames.first;

	while (frame) {
		bGPDstroke *stroke = frame->strokes.first;

		while (stroke) {
			if (point_in_stroke(stroke, x, y))
				return TRUE;

			stroke = stroke->next;
		}
		frame = frame->next;
	}

	return FALSE;
}

static void retrieve_libmv_features(MovieTracking *tracking, ListBase *tracksbase,
                                    struct libmv_Features *features, int framenr, int width, int height,
                                    bGPDlayer *layer, int place_outside_layer)
{
	int a;

	a = libmv_countFeatures(features);
	while (a--) {
		MovieTrackingTrack *track;
		double x, y, size, score;
		int ok = TRUE;
		float xu, yu;

		libmv_getFeature(features, a, &x, &y, &score, &size);

		xu = x / width;
		yu = y / height;

		if (layer)
			ok = point_in_layer(layer, xu, yu) != place_outside_layer;

		if (ok) {
			track = BKE_tracking_add_track(tracking, tracksbase, xu, yu, framenr, width, height);
			track->flag |= SELECT;
			track->pat_flag |= SELECT;
			track->search_flag |= SELECT;
		}
	}
}
#endif

void BKE_tracking_detect_fast(MovieTracking *tracking, ListBase *tracksbase, ImBuf *ibuf,
                              int framenr, int margin, int min_trackness, int min_distance, bGPDlayer *layer,
                              int place_outside_layer)
{
#ifdef WITH_LIBMV
	struct libmv_Features *features;
	unsigned char *pixels = get_ucharbuf(ibuf);

	features = libmv_detectFeaturesFAST(pixels, ibuf->x, ibuf->y, ibuf->x,
	                                    margin, min_trackness, min_distance);

	MEM_freeN(pixels);

	retrieve_libmv_features(tracking, tracksbase, features, framenr,
	                        ibuf->x, ibuf->y, layer, place_outside_layer);

	libmv_destroyFeatures(features);
#else
	(void) tracking;
	(void) tracksbase;
	(void) ibuf;
	(void) framenr;
	(void) margin;
	(void) min_trackness;
	(void) min_distance;
	(void) layer;
	(void) place_outside_layer;
#endif
}

MovieTrackingTrack *BKE_tracking_indexed_track(MovieTracking *tracking, int tracknr, ListBase **tracksbase_r)
{
	MovieTrackingObject *object;
	int cur = 1;

	object = tracking->objects.first;
	while (object) {
		ListBase *tracksbase = BKE_tracking_object_tracks(tracking, object);
		MovieTrackingTrack *track = tracksbase->first;

		while (track) {
			if (track->flag & TRACK_HAS_BUNDLE) {
				if (cur == tracknr) {
					*tracksbase_r = tracksbase;
					return track;
				}

				cur++;
			}

			track = track->next;
		}

		object = object->next;
	}

	*tracksbase_r = NULL;

	return NULL;
}

static int stabilization_median_point(MovieTracking *tracking, int framenr, float median[2])
{
	int ok = FALSE;
	float min[2], max[2];
	MovieTrackingTrack *track;

	INIT_MINMAX2(min, max);

	track = tracking->tracks.first;
	while (track) {
		if (track->flag & TRACK_USE_2D_STAB) {
			MovieTrackingMarker *marker = BKE_tracking_get_marker(track, framenr);

			DO_MINMAX2(marker->pos, min, max);

			ok = TRUE;
		}

		track = track->next;
	}

	median[0] = (max[0] + min[0]) / 2.0f;
	median[1] = (max[1] + min[1]) / 2.0f;

	return ok;
}

static void calculate_stabdata(MovieTracking *tracking, int framenr, float width, float height,
                               float firstmedian[2], float median[2], float loc[2], float *scale, float *angle)
{
	MovieTrackingStabilization *stab = &tracking->stabilization;

	*scale = (stab->scale - 1.0f) * stab->scaleinf + 1.0f;
	*angle = 0.0f;

	loc[0] = (firstmedian[0] - median[0]) * width * (*scale);
	loc[1] = (firstmedian[1] - median[1]) * height * (*scale);

	mul_v2_fl(loc, stab->locinf);

	if ((stab->flag & TRACKING_STABILIZE_ROTATION) && stab->rot_track && stab->rotinf) {
		MovieTrackingMarker *marker;
		float a[2], b[2];
		float x0 = (float)width / 2.0f, y0 = (float)height / 2.0f;
		float x = median[0] * width, y = median[1] * height;

		marker = BKE_tracking_get_marker(stab->rot_track, 1);
		sub_v2_v2v2(a, marker->pos, firstmedian);
		a[0] *= width;
		a[1] *= height;

		marker = BKE_tracking_get_marker(stab->rot_track, framenr);
		sub_v2_v2v2(b, marker->pos, median);
		b[0] *= width;
		b[1] *= height;

		*angle = -atan2(a[0] * b[1] - a[1] * b[0], a[0] * b[0] + a[1] * b[1]);
		*angle *= stab->rotinf;

		/* convert to rotation around image center */
		loc[0] -= (x0 + (x - x0) * cosf(*angle) - (y - y0) * sinf(*angle) - x) * (*scale);
		loc[1] -= (y0 + (x - x0) * sinf(*angle) + (y - y0) * cosf(*angle) - y) * (*scale);
	}
}

static float stabilization_auto_scale_factor(MovieTracking *tracking, int width, int height)
{
	float firstmedian[2];
	MovieTrackingStabilization *stab = &tracking->stabilization;
	float aspect = tracking->camera.pixel_aspect;

	if (stab->ok)
		return stab->scale;

	if (stabilization_median_point(tracking, 1, firstmedian)) {
		int sfra = INT_MAX, efra = INT_MIN, cfra;
		float scale = 1.0f;
		MovieTrackingTrack *track;

		stab->scale = 1.0f;

		track = tracking->tracks.first;
		while (track) {
			if (track->flag & TRACK_USE_2D_STAB ||
			    ((stab->flag & TRACKING_STABILIZE_ROTATION) && track == stab->rot_track))
			{
				sfra = MIN2(sfra, track->markers[0].framenr);
				efra = MAX2(efra, track->markers[track->markersnr - 1].framenr);
			}

			track = track->next;
		}

		for (cfra = sfra; cfra <= efra; cfra++) {
			float median[2];
			float loc[2], angle, tmp_scale;
			int i;
			float mat[4][4];
			float points[4][2] = {{0.0f, 0.0f}, {0.0f, height}, {width, height}, {width, 0.0f}};
			float si, co;

			stabilization_median_point(tracking, cfra, median);

			calculate_stabdata(tracking, cfra, width, height, firstmedian, median, loc, &tmp_scale, &angle);

			BKE_tracking_stabdata_to_mat4(width, height, aspect, loc, 1.0f, angle, mat);

			si = sin(angle);
			co = cos(angle);

			for (i = 0; i < 4; i++) {
				int j;
				float a[3] = {0.0f, 0.0f, 0.0f}, b[3] = {0.0f, 0.0f, 0.0f};

				copy_v3_v3(a, points[i]);
				copy_v3_v3(b, points[(i + 1) % 4]);

				mul_m4_v3(mat, a);
				mul_m4_v3(mat, b);

				for (j = 0; j < 4; j++) {
					float point[3] = {points[j][0], points[j][1], 0.0f};
					float v1[3], v2[3];

					sub_v3_v3v3(v1, b, a);
					sub_v3_v3v3(v2, point, a);

					if (cross_v2v2(v1, v2) >= 0.0f) {
						const float rotDx[4][2] = {{1.0f, 0.0f}, {0.0f, -1.0f}, {-1.0f, 0.0f}, {0.0f, 1.0f}};
						const float rotDy[4][2] = {{0.0f, 1.0f}, {1.0f, 0.0f}, {0.0f, -1.0f}, {-1.0f, 0.0f}};

						float dx = loc[0] * rotDx[j][0] + loc[1] * rotDx[j][1],
						      dy = loc[0] * rotDy[j][0] + loc[1] * rotDy[j][1];

						float w, h, E, F, G, H, I, J, K, S;

						if (j % 2) {
							w = (float)height / 2.0f;
							h = (float)width / 2.0f;
						}
						else {
							w = (float)width / 2.0f;
							h = (float)height / 2.0f;
						}

						E = -w * co + h * si;
						F = -h * co - w * si;

						if ((i % 2) == (j % 2)) {
							G = -w * co - h * si;
							H = h * co - w * si;
						}
						else {
							G = w * co + h * si;
							H = -h * co + w * si;
						}

						I = F - H;
						J = G - E;
						K = G * F - E * H;

						S = (-w * I - h * J) / (dx * I + dy * J + K);

						scale = MAX2(scale, S);
					}
				}
			}
		}

		stab->scale = scale;

		if (stab->maxscale > 0.0f)
			stab->scale = MIN2(stab->scale, stab->maxscale);
	}
	else {
		stab->scale = 1.0f;
	}

	stab->ok = TRUE;

	return stab->scale;
}

static ImBuf *stabilize_alloc_ibuf(ImBuf *cacheibuf, ImBuf *srcibuf, int fill)
{
	int flags;

	if (cacheibuf && (cacheibuf->x != srcibuf->x || cacheibuf->y != srcibuf->y)) {
		IMB_freeImBuf(cacheibuf);
		cacheibuf = NULL;
	}

	flags = IB_rect;

	if (srcibuf->rect_float)
		flags |= IB_rectfloat;

	if (cacheibuf) {
		if (fill) {
			float col[4] = {0.0f, 0.0f, 0.0f, 0.0f};

			IMB_rectfill(cacheibuf, col);
		}
	}
	else {
		cacheibuf = IMB_allocImBuf(srcibuf->x, srcibuf->y, srcibuf->planes, flags);
		cacheibuf->profile = srcibuf->profile;
	}

	return cacheibuf;
}

void BKE_tracking_stabilization_data(MovieTracking *tracking, int framenr, int width, int height,
                                     float loc[2], float *scale, float *angle)
{
	float firstmedian[2], median[2];
	MovieTrackingStabilization *stab = &tracking->stabilization;

	if ((stab->flag & TRACKING_2D_STABILIZATION) == 0) {
		zero_v2(loc);
		*scale = 1.0f;
		*angle = 0.0f;

		return;
	}

	if (stabilization_median_point(tracking, 1, firstmedian)) {
		stabilization_median_point(tracking, framenr, median);

		if ((stab->flag & TRACKING_AUTOSCALE) == 0)
			stab->scale = 1.0f;

		if (!stab->ok) {
			if (stab->flag & TRACKING_AUTOSCALE)
				stabilization_auto_scale_factor(tracking, width, height);

			calculate_stabdata(tracking, framenr, width, height, firstmedian, median, loc, scale, angle);

			stab->ok = TRUE;
		}
		else {
			calculate_stabdata(tracking, framenr, width, height, firstmedian, median, loc, scale, angle);
		}
	}
	else {
		zero_v2(loc);
		*scale = 1.0f;
		*angle = 0.0f;
	}
}

ImBuf *BKE_tracking_stabilize(MovieTracking *tracking, int framenr, ImBuf *ibuf,
                              float loc[2], float *scale, float *angle)
{
	float tloc[2], tscale, tangle;
	MovieTrackingStabilization *stab = &tracking->stabilization;
	ImBuf *tmpibuf;
	float width = ibuf->x, height = ibuf->y;
	float aspect = tracking->camera.pixel_aspect;

	if (loc)
		copy_v2_v2(tloc, loc);

	if (scale)
		tscale = *scale;

	if ((stab->flag & TRACKING_2D_STABILIZATION) == 0) {
		if (loc)
			zero_v2(loc);

		if (scale)
			*scale = 1.0f;

		return ibuf;
	}

	BKE_tracking_stabilization_data(tracking, framenr, width, height, tloc, &tscale, &tangle);

	tmpibuf = stabilize_alloc_ibuf(NULL, ibuf, TRUE);

	/* scale would be handled by matrix transformation when angle is non-zero */
	if (tscale != 1.0f && tangle == 0.0f) {
		ImBuf *scaleibuf;

		stabilization_auto_scale_factor(tracking, width, height);

		scaleibuf = stabilize_alloc_ibuf(stab->scaleibuf, ibuf, 0);
		stab->scaleibuf = scaleibuf;

		IMB_rectcpy(scaleibuf, ibuf, 0, 0, 0, 0, ibuf->x, ibuf->y);
		IMB_scalefastImBuf(scaleibuf, ibuf->x * tscale, ibuf->y * tscale);

		ibuf = scaleibuf;
	}

	if (tangle == 0.0f) {
		/* if angle is zero, then it's much faster to use rect copy
		 * but could be issues with subpixel precisions
		 */
		IMB_rectcpy(tmpibuf, ibuf,
		            tloc[0] - (tscale - 1.0f) * width / 2.0f,
		            tloc[1] - (tscale - 1.0f) * height / 2.0f,
		            0, 0, ibuf->x, ibuf->y);
	}
	else {
		float mat[4][4];
		int i, j, filter = tracking->stabilization.filter;
		void (*interpolation)(struct ImBuf *, struct ImBuf *, float, float, int, int) = NULL;

		BKE_tracking_stabdata_to_mat4(ibuf->x, ibuf->y, aspect, tloc, tscale, tangle, mat);
		invert_m4(mat);

		if (filter == TRACKING_FILTER_NEAREAST)
			interpolation = neareast_interpolation;
		else if (filter == TRACKING_FILTER_BILINEAR)
			interpolation = bilinear_interpolation;
		else if (filter == TRACKING_FILTER_BICUBIC)
			interpolation = bicubic_interpolation;
		else
			/* fallback to default interpolation method */
			interpolation = neareast_interpolation;

		for (j = 0; j < tmpibuf->y; j++) {
			for (i = 0; i < tmpibuf->x; i++) {
				float vec[3] = {i, j, 0};

				mul_v3_m4v3(vec, mat, vec);

				interpolation(ibuf, tmpibuf, vec[0], vec[1], i, j);
			}
		}
	}

	tmpibuf->userflags |= IB_MIPMAP_INVALID;

	if (tmpibuf->rect_float)
		tmpibuf->userflags |= IB_RECT_INVALID;

	if (loc)
		copy_v2_v2(loc, tloc);

	if (scale)
		*scale = tscale;

	if (angle)
		*angle = tangle;

	return tmpibuf;
}

void BKE_tracking_stabdata_to_mat4(int width, int height, float aspect,
                                   float loc[2], float scale, float angle, float mat[4][4])
{
	float lmat[4][4], rmat[4][4], smat[4][4], cmat[4][4], icmat[4][4], amat[4][4], iamat[4][4];
	float svec[3] = {scale, scale, scale};

	unit_m4(rmat);
	unit_m4(lmat);
	unit_m4(smat);
	unit_m4(cmat);
	unit_m4(amat);

	/* aspect ratio correction matrix */
	amat[0][0] = 1.0f / aspect;
	invert_m4_m4(iamat, amat);

	/* image center as rotation center */
	cmat[3][0] = (float)width / 2.0f;
	cmat[3][1] = (float)height / 2.0f;
	invert_m4_m4(icmat, cmat);

	size_to_mat4(smat, svec);       /* scale matrix */
	add_v2_v2(lmat[3], loc);        /* translation matrix */
	rotate_m4(rmat, 'Z', angle);    /* rotation matrix */

	/* compose transformation matrix */
	mul_serie_m4(mat, lmat, cmat, amat, rmat, iamat, smat, icmat, NULL);
}

MovieDistortion *BKE_tracking_distortion_create(void)
{
	MovieDistortion *distortion;

	distortion = MEM_callocN(sizeof(MovieDistortion), "BKE_tracking_distortion_create");

	return distortion;
}

MovieDistortion *BKE_tracking_distortion_copy(MovieDistortion *distortion)
{
	MovieDistortion *new_distortion;

	new_distortion = MEM_callocN(sizeof(MovieDistortion), "BKE_tracking_distortion_create");

#ifdef WITH_LIBMV
	new_distortion->intrinsics = libmv_CameraIntrinsicsCopy(distortion->intrinsics);
#else
	(void) distortion;
#endif

	return new_distortion;
}

void BKE_tracking_distortion_update(MovieDistortion *distortion, MovieTracking *tracking, int width, int height)
{
	MovieTrackingCamera *camera = &tracking->camera;
	float aspy = 1.0f / tracking->camera.pixel_aspect;

#ifdef WITH_LIBMV
	if (!distortion->intrinsics) {
		distortion->intrinsics = libmv_CameraIntrinsicsNew(camera->focal,
		                                                   camera->principal[0], camera->principal[1] * aspy,
		                                                   camera->k1, camera->k2, camera->k3, width, height * aspy);
	}
	else {
		libmv_CameraIntrinsicsUpdate(distortion->intrinsics, camera->focal,
		                             camera->principal[0], camera->principal[1] * aspy,
		                             camera->k1, camera->k2, camera->k3, width, height * aspy);
	}
#else
	(void) distortion;
	(void) width;
	(void) height;
	(void) camera;
	(void) aspy;
#endif
}

ImBuf *BKE_tracking_distortion_exec(MovieDistortion *distortion, MovieTracking *tracking,
                                    ImBuf *ibuf, int width, int height, float overscan, int undistort)
{
	ImBuf *resibuf;

	BKE_tracking_distortion_update(distortion, tracking, width, height);

	resibuf = IMB_dupImBuf(ibuf);

	if (ibuf->rect_float) {
#ifdef WITH_LIBMV
		if (undistort) {
			libmv_CameraIntrinsicsUndistortFloat(distortion->intrinsics,
			                                     ibuf->rect_float, resibuf->rect_float,
			                                     ibuf->x, ibuf->y, overscan, ibuf->channels);
		}
		else {
			libmv_CameraIntrinsicsDistortFloat(distortion->intrinsics,
			                                   ibuf->rect_float, resibuf->rect_float,
			                                   ibuf->x, ibuf->y, overscan, ibuf->channels);
		}
#endif

		resibuf->userflags |= IB_RECT_INVALID;
	}
	else {
#ifdef WITH_LIBMV
		if (undistort) {
			libmv_CameraIntrinsicsUndistortByte(distortion->intrinsics,
			                                    (unsigned char *)ibuf->rect, (unsigned char *)resibuf->rect,
			                                    ibuf->x, ibuf->y, overscan, ibuf->channels);
		}
		else {
			libmv_CameraIntrinsicsDistortByte(distortion->intrinsics,
			                                  (unsigned char *)ibuf->rect, (unsigned char *)resibuf->rect,
			                                  ibuf->x, ibuf->y, overscan, ibuf->channels);
		}
#endif
	}

#ifndef WITH_LIBMV
	(void) overscan;
	(void) undistort;
#endif

	return resibuf;
}

void BKE_tracking_distortion_destroy(MovieDistortion *distortion)
{
#ifdef WITH_LIBMV
	libmv_CameraIntrinsicsDestroy(distortion->intrinsics);
#endif

	MEM_freeN(distortion);
}

ImBuf *BKE_tracking_undistort(MovieTracking *tracking, ImBuf *ibuf, int width, int height, float overscan)
{
	MovieTrackingCamera *camera = &tracking->camera;

	if (camera->intrinsics == NULL)
		camera->intrinsics = BKE_tracking_distortion_create();

	return BKE_tracking_distortion_exec(camera->intrinsics, tracking, ibuf, width, height, overscan, 1);
}

ImBuf *BKE_tracking_distort(MovieTracking *tracking, ImBuf *ibuf, int width, int height, float overscan)
{
	MovieTrackingCamera *camera = &tracking->camera;

	if (camera->intrinsics == NULL)
		camera->intrinsics = BKE_tracking_distortion_create();

	return BKE_tracking_distortion_exec(camera->intrinsics, tracking, ibuf, width, height, overscan, 0);
}

/* area - which part of marker should be selected. see TRACK_AREA_* constants */
void BKE_tracking_select_track(ListBase *tracksbase, MovieTrackingTrack *track, int area, int extend)
{
	if (extend) {
		BKE_tracking_track_flag(track, area, SELECT, 0);
	}
	else {
		MovieTrackingTrack *cur = tracksbase->first;

		while (cur) {
			if ((cur->flag & TRACK_HIDDEN) == 0) {
				if (cur == track) {
					BKE_tracking_track_flag(cur, TRACK_AREA_ALL, SELECT, 1);
					BKE_tracking_track_flag(cur, area, SELECT, 0);
				}
				else {
					BKE_tracking_track_flag(cur, TRACK_AREA_ALL, SELECT, 1);
				}
			}

			cur = cur->next;
		}
	}
}

void BKE_tracking_deselect_track(MovieTrackingTrack *track, int area)
{
	BKE_tracking_track_flag(track, area, SELECT, 1);
}

MovieTrackingObject *BKE_tracking_new_object(MovieTracking *tracking, const char *name)
{
	MovieTrackingObject *object = MEM_callocN(sizeof(MovieTrackingObject), "tracking object");

	if (tracking->tot_object == 0) {
		/* first object is always camera */
		BLI_strncpy(object->name, "Camera", sizeof(object->name));

		object->flag |= TRACKING_OBJECT_CAMERA;
	}
	else {
		BLI_strncpy(object->name, name, sizeof(object->name));
	}

	BLI_addtail(&tracking->objects, object);

	tracking->tot_object++;
	tracking->objectnr = BLI_countlist(&tracking->objects) - 1;

	object->scale = 1.0f;

	BKE_tracking_object_unique_name(tracking, object);

	return object;
}

void BKE_tracking_remove_object(MovieTracking *tracking, MovieTrackingObject *object)
{
	MovieTrackingTrack *track;
	int index = BLI_findindex(&tracking->objects, object);

	if (index < 0)
		return;

	if (object->flag & TRACKING_OBJECT_CAMERA) {
		/* object used for camera solving can't be deleted */
		return;
	}

	track = object->tracks.first;
	while (track) {
		if (track == tracking->act_track)
			tracking->act_track = NULL;

		track = track->next;
	}

	tracking_object_free(object);
	BLI_freelinkN(&tracking->objects, object);

	tracking->tot_object--;

	if (index > 0)
		tracking->objectnr = index - 1;
	else
		tracking->objectnr = 0;
}

void BKE_tracking_object_unique_name(MovieTracking *tracking, MovieTrackingObject *object)
{
	BLI_uniquename(&tracking->objects, object, "Object", '.',
	               offsetof(MovieTrackingObject, name), sizeof(object->name));
}

MovieTrackingObject *BKE_tracking_named_object(MovieTracking *tracking, const char *name)
{
	MovieTrackingObject *object = tracking->objects.first;

	while (object) {
		if (!strcmp(object->name, name))
			return object;

		object = object->next;
	}

	return NULL;
}

/*********************** dopesheet functions *************************/

static int channels_alpha_sort(void *a, void *b)
{
	MovieTrackingDopesheetChannel *channel_a = a;
	MovieTrackingDopesheetChannel *channel_b = b;

	if (BLI_strcasecmp(channel_a->track->name, channel_b->track->name) > 0)
		return 1;
	else
		return 0;
}

static int channels_total_track_sort(void *a, void *b)
{
	MovieTrackingDopesheetChannel *channel_a = a;
	MovieTrackingDopesheetChannel *channel_b = b;

	if (channel_a->total_frames > channel_b->total_frames)
		return 1;
	else
		return 0;
}

static int channels_longest_segment_sort(void *a, void *b)
{
	MovieTrackingDopesheetChannel *channel_a = a;
	MovieTrackingDopesheetChannel *channel_b = b;

	if (channel_a->max_segment > channel_b->max_segment)
		return 1;
	else
		return 0;
}

static int channels_average_error_sort(void *a, void *b)
{
	MovieTrackingDopesheetChannel *channel_a = a;
	MovieTrackingDopesheetChannel *channel_b = b;

	if (channel_a->track->error > channel_b->track->error)
		return 1;
	else
		return 0;
}

static int channels_alpha_inverse_sort(void *a, void *b)
{
	if (channels_alpha_sort(a, b))
		return 0;
	else
		return 1;
}

static int channels_total_track_inverse_sort(void *a, void *b)
{
	if (channels_total_track_sort(a, b))
		return 0;
	else
		return 1;
}

static int channels_longest_segment_inverse_sort(void *a, void *b)
{
	if (channels_longest_segment_sort(a, b))
		return 0;
	else
		return 1;
}

static int channels_average_error_inverse_sort(void *a, void *b)
{
	MovieTrackingDopesheetChannel *channel_a = a;
	MovieTrackingDopesheetChannel *channel_b = b;

	if (channel_a->track->error < channel_b->track->error)
		return 1;
	else
		return 0;
}

static void channels_segments_calc(MovieTrackingDopesheetChannel *channel)
{
	MovieTrackingTrack *track = channel->track;
	int i, segment;

	channel->tot_segment = 0;
	channel->max_segment = 0;
	channel->total_frames = 0;

	/* count */
	i = 0;
	while (i < track->markersnr) {
		MovieTrackingMarker *marker = &track->markers[i];

		if ((marker->flag & MARKER_DISABLED) == 0) {
			int prev_fra = marker->framenr, len = 0;

			i++;
			while (i < track->markersnr) {
				marker = &track->markers[i];

				if (marker->framenr != prev_fra + 1)
					break;
				if (marker->flag & MARKER_DISABLED)
					break;

				prev_fra = marker->framenr;
				len++;
				i++;
			}

			channel->tot_segment++;
		}

		i++;
	}

	if (!channel->tot_segment)
		return;

	channel->segments = MEM_callocN(2 * sizeof(int) * channel->tot_segment, "tracking channel segments");

	/* create segments */
	i = 0;
	segment = 0;
	while (i < track->markersnr) {
		MovieTrackingMarker *marker = &track->markers[i];

		if ((marker->flag & MARKER_DISABLED) == 0) {
			MovieTrackingMarker *start_marker = marker;
			int prev_fra = marker->framenr, len = 0;

			i++;
			while (i < track->markersnr) {
				marker = &track->markers[i];

				if (marker->framenr != prev_fra + 1)
					break;
				if (marker->flag & MARKER_DISABLED)
					break;

				prev_fra = marker->framenr;
				channel->total_frames++;
				len++;
				i++;
			}

			channel->segments[2 * segment] = start_marker->framenr;
			channel->segments[2 * segment + 1] = start_marker->framenr + len;

			channel->max_segment =  MAX2(channel->max_segment, len);
			segment++;
		}

		i++;
	}
}

static void  tracking_dopesheet_sort(MovieTracking *tracking, int sort_method, int inverse)
{
	MovieTrackingDopesheet *dopesheet = &tracking->dopesheet;

	if (inverse) {
		if (sort_method == TRACKING_DOPE_SORT_NAME) {
			BLI_sortlist(&dopesheet->channels, channels_alpha_inverse_sort);
		}
		else if (sort_method == TRACKING_DOPE_SORT_LONGEST) {
			BLI_sortlist(&dopesheet->channels, channels_longest_segment_inverse_sort);
		}
		else if (sort_method == TRACKING_DOPE_SORT_TOTAL) {
			BLI_sortlist(&dopesheet->channels, channels_total_track_inverse_sort);
		}
		else if (sort_method == TRACKING_DOPE_SORT_AVERAGE_ERROR) {
			BLI_sortlist(&dopesheet->channels, channels_average_error_inverse_sort);
		}
	}
	else {
		if (sort_method == TRACKING_DOPE_SORT_NAME) {
			BLI_sortlist(&dopesheet->channels, channels_alpha_sort);
		}
		else if (sort_method == TRACKING_DOPE_SORT_LONGEST) {
			BLI_sortlist(&dopesheet->channels, channels_longest_segment_sort);
		}
		else if (sort_method == TRACKING_DOPE_SORT_TOTAL) {
			BLI_sortlist(&dopesheet->channels, channels_total_track_sort);
		}
		else if (sort_method == TRACKING_DOPE_SORT_AVERAGE_ERROR) {
			BLI_sortlist(&dopesheet->channels, channels_average_error_sort);
		}
	}
}

void BKE_tracking_dopesheet_tag_update(MovieTracking *tracking)
{
	MovieTrackingDopesheet *dopesheet = &tracking->dopesheet;

	dopesheet->ok = FALSE;
}

void BKE_tracking_dopesheet_update(MovieTracking *tracking)
{
	MovieTrackingObject *object = BKE_tracking_active_object(tracking);
	MovieTrackingDopesheet *dopesheet = &tracking->dopesheet;
	MovieTrackingTrack *track;
	ListBase *tracksbase = BKE_tracking_object_tracks(tracking, object);
	short sort_method = dopesheet->sort_method;
	short inverse = dopesheet->flag & TRACKING_DOPE_SORT_INVERSE;

	if (dopesheet->ok)
		return;

	tracking_dopesheet_free(dopesheet);

	for (track = tracksbase->first; track; track = track->next) {
		if (TRACK_SELECTED(track) && (track->flag & TRACK_HIDDEN) == 0) {
			MovieTrackingDopesheetChannel *channel;

			channel = MEM_callocN(sizeof(MovieTrackingDopesheetChannel), "tracking dopesheet channel");
			channel->track = track;

			channels_segments_calc(channel);

			BLI_addtail(&dopesheet->channels, channel);
			dopesheet->tot_channel++;
		}
	}

	tracking_dopesheet_sort(tracking, sort_method, inverse);

	dopesheet->ok = TRUE;
}
