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

/** \file blender/blenkernel/tracking_util.c
 *  \ingroup bke
 *
 * This file contains implementation of function which are used
 * by multiple tracking files but which should not be public.
 */

#include <stddef.h>

#include "MEM_guardedalloc.h"

#include "DNA_movieclip_types.h"

#include "BLI_utildefines.h"
#include "BLI_math.h"
#include "BLI_listbase.h"
#include "BLI_ghash.h"
#include "BLI_path_util.h"
#include "BLI_string.h"

#include "BLF_translation.h"

#include "BKE_tracking.h"

#include "tracking_private.h"

#include "libmv-capi.h"

/*********************** Tracks map *************************/

TracksMap *tracks_map_new(const char *object_name, bool is_camera, int num_tracks, int customdata_size)
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

	BLI_spin_init(&map->spin_lock);

	return map;
}

int tracks_map_get_size(TracksMap *map)
{
	return map->num_tracks;
}

void tracks_map_get_indexed_element(TracksMap *map, int index, MovieTrackingTrack **track, void **customdata)
{
	*track = &map->tracks[index];

	if (map->customdata)
		*customdata = &map->customdata[index * map->customdata_size];
}

void tracks_map_insert(TracksMap *map, MovieTrackingTrack *track, void *customdata)
{
	MovieTrackingTrack new_track = *track;

	new_track.markers = MEM_dupallocN(new_track.markers);

	map->tracks[map->ptr] = new_track;

	if (customdata)
		memcpy(&map->customdata[map->ptr * map->customdata_size], customdata, map->customdata_size);

	BLI_ghash_insert(map->hash, &map->tracks[map->ptr], track);

	map->ptr++;
}

void tracks_map_merge(TracksMap *map, MovieTracking *tracking)
{
	MovieTrackingTrack *track;
	ListBase tracks = {NULL, NULL}, new_tracks = {NULL, NULL};
	ListBase *old_tracks;
	int a;

	if (map->is_camera) {
		old_tracks = &tracking->tracks;
	}
	else {
		MovieTrackingObject *object = BKE_tracking_object_get_named(tracking, map->object_name);

		if (!object) {
			/* object was deleted by user, create new one */
			object = BKE_tracking_object_add(tracking, map->object_name);
		}

		old_tracks = &object->tracks;
	}

	/* duplicate currently operating tracks to temporary list.
	 * this is needed to keep names in unique state and it's faster to change names
	 * of currently operating tracks (if needed)
	 */
	for (a = 0; a < map->num_tracks; a++) {
		MovieTrackingTrack *old_track;
		bool mapped_to_old = false;

		track = &map->tracks[a];

		/* find original of operating track in list of previously displayed tracks */
		old_track = BLI_ghash_lookup(map->hash, track);
		if (old_track) {
			if (BLI_findindex(old_tracks, old_track) != -1) {
				BLI_remlink(old_tracks, old_track);

				BLI_spin_lock(&map->spin_lock);

				/* Copy flags like selection back to the track map. */
				track->flag = old_track->flag;
				track->pat_flag = old_track->pat_flag;
				track->search_flag = old_track->search_flag;

				/* Copy all the rest settings back from the map to the actual tracks. */
				MEM_freeN(old_track->markers);
				*old_track = *track;
				old_track->markers = MEM_dupallocN(old_track->markers);

				BLI_spin_unlock(&map->spin_lock);

				BLI_addtail(&tracks, old_track);

				mapped_to_old = true;
			}
		}

		if (mapped_to_old == false) {
			MovieTrackingTrack *new_track = BKE_tracking_track_duplicate(track);

			/* Update old-new track mapping */
			BLI_ghash_remove(map->hash, track, NULL, NULL);
			BLI_ghash_insert(map->hash, track, new_track);

			BLI_addtail(&tracks, new_track);
		}
	}

	/* move all tracks, which aren't operating */
	track = old_tracks->first;
	while (track) {
		MovieTrackingTrack *next = track->next;
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

		BLI_uniquename(&new_tracks, track, CTX_DATA_(BLF_I18NCONTEXT_ID_MOVIECLIP, "Track"), '.',
		               offsetof(MovieTrackingTrack, name), sizeof(track->name));

		track = next;
	}

	*old_tracks = new_tracks;
}

void tracks_map_free(TracksMap *map, void (*customdata_free)(void *customdata))
{
	int i = 0;

	BLI_ghash_free(map->hash, NULL, NULL);

	for (i = 0; i < map->num_tracks; i++) {
		if (map->customdata && customdata_free)
			customdata_free(&map->customdata[i * map->customdata_size]);

		BKE_tracking_track_free(&map->tracks[i]);
	}

	if (map->customdata)
		MEM_freeN(map->customdata);

	MEM_freeN(map->tracks);

	BLI_spin_end(&map->spin_lock);

	MEM_freeN(map);
}

/*********************** Space transformation functions *************************/

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
                                                      const float marker_unified_coords[2],
                                                      float frame_pixel_coords[2])
{
	marker_to_frame_unified(marker, marker_unified_coords, frame_pixel_coords);
	unified_to_pixel(frame_width, frame_height, frame_pixel_coords, frame_pixel_coords);
}

void tracking_get_search_origin_frame_pixel(int frame_width, int frame_height,
                                            const MovieTrackingMarker *marker,
                                            float frame_pixel[2])
{
	/* Get the lower left coordinate of the search window and snap to pixel coordinates */
	marker_unified_to_frame_pixel_coordinates(frame_width, frame_height, marker, marker->search_min, frame_pixel);
	frame_pixel[0] = (int)frame_pixel[0];
	frame_pixel[1] = (int)frame_pixel[1];
}

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
	tracking_get_search_origin_frame_pixel(frame_width, frame_height, marker, search_origin_frame_pixel);
	sub_v2_v2v2(search_pixel, frame_pixel, search_origin_frame_pixel);
}

static void search_pixel_to_marker_unified(int frame_width, int frame_height,
                                           const MovieTrackingMarker *marker,
                                           const float search_pixel[2], float marker_unified[2])
{
	float frame_unified[2];
	float search_origin_frame_pixel[2];

	tracking_get_search_origin_frame_pixel(frame_width, frame_height, marker, search_origin_frame_pixel);
	add_v2_v2v2(frame_unified, search_pixel, search_origin_frame_pixel);
	pixel_to_unified(frame_width, frame_height, frame_unified, frame_unified);

	/* marker pos is in frame unified */
	sub_v2_v2v2(marker_unified, frame_unified, marker->pos);
}

/* Each marker has 5 coordinates associated with it that get warped with
 * tracking: the four corners ("pattern_corners"), and the center ("pos").
 * This function puts those 5 points into the appropriate frame for tracking
 * (the "search" coordinate frame).
 */
void tracking_get_marker_coords_for_tracking(int frame_width, int frame_height,
                                             const MovieTrackingMarker *marker,
                                             double search_pixel_x[5], double search_pixel_y[5])
{
	int i;
	float unified_coords[2];
	float pixel_coords[2];

	/* Convert the corners into search space coordinates. */
	for (i = 0; i < 4; i++) {
		marker_unified_to_search_pixel(frame_width, frame_height, marker, marker->pattern_corners[i], pixel_coords);
		search_pixel_x[i] = pixel_coords[0] - 0.5f;
		search_pixel_y[i] = pixel_coords[1] - 0.5f;
	}

	/* Convert the center position (aka "pos"); this is the origin */
	unified_coords[0] = 0.0f;
	unified_coords[1] = 0.0f;
	marker_unified_to_search_pixel(frame_width, frame_height, marker, unified_coords, pixel_coords);

	search_pixel_x[4] = pixel_coords[0] - 0.5f;
	search_pixel_y[4] = pixel_coords[1] - 0.5f;
}

/* Inverse of above. */
void tracking_set_marker_coords_from_tracking(int frame_width, int frame_height, MovieTrackingMarker *marker,
                                              const double search_pixel_x[5], const double search_pixel_y[5])
{
	int i;
	float marker_unified[2];
	float search_pixel[2];

	/* Convert the corners into search space coordinates. */
	for (i = 0; i < 4; i++) {
		search_pixel[0] = search_pixel_x[i] + 0.5;
		search_pixel[1] = search_pixel_y[i] + 0.5;
		search_pixel_to_marker_unified(frame_width, frame_height, marker, search_pixel, marker->pattern_corners[i]);
	}

	/* Convert the center position (aka "pos"); this is the origin */
	search_pixel[0] = search_pixel_x[4] + 0.5;
	search_pixel[1] = search_pixel_y[4] + 0.5;
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

/*********************** General purpose utility functions *************************/

/* Place a disabled marker before or after specified ref_marker.
 *
 * If before is truth, disabled marker is placed before reference
 * one, and it's placed after it otherwise.
 *
 * If there's already a marker at the frame where disabled one
 * is expected to be placed, nothing will happen if overwrite
 * is false.
 */
void tracking_marker_insert_disabled(MovieTrackingTrack *track, const MovieTrackingMarker *ref_marker,
                                     bool before, bool overwrite)
{
	MovieTrackingMarker marker_new;

	marker_new = *ref_marker;
	marker_new.flag &= ~MARKER_TRACKED;
	marker_new.flag |= MARKER_DISABLED;

	if (before)
		marker_new.framenr--;
	else
		marker_new.framenr++;

	if (overwrite || !BKE_tracking_track_has_marker_at_frame(track, marker_new.framenr))
		BKE_tracking_marker_insert(track, &marker_new);
}


/* Fill in Libmv C-API camera intrinsics options from tracking structure.
 */
void tracking_cameraIntrinscisOptionsFromTracking(MovieTracking *tracking,
                                                  int calibration_width, int calibration_height,
                                                  libmv_CameraIntrinsicsOptions *camera_intrinsics_options)
{
	MovieTrackingCamera *camera = &tracking->camera;
	float aspy = 1.0f / tracking->camera.pixel_aspect;

	camera_intrinsics_options->focal_length = camera->focal;

	camera_intrinsics_options->principal_point_x = camera->principal[0];
	camera_intrinsics_options->principal_point_y = camera->principal[1] * aspy;

	switch (camera->distortion_model) {
		case TRACKING_DISTORTION_MODEL_POLYNOMIAL:
			camera_intrinsics_options->distortion_model = LIBMV_DISTORTION_MODEL_POLYNOMIAL;
			camera_intrinsics_options->polynomial_k1 = camera->k1;
			camera_intrinsics_options->polynomial_k2 = camera->k2;
			camera_intrinsics_options->polynomial_k3 = camera->k3;
			camera_intrinsics_options->polynomial_p1 = 0.0;
			camera_intrinsics_options->polynomial_p2 = 0.0;
			break;
		case TRACKING_DISTORTION_MODEL_DIVISION:
			camera_intrinsics_options->distortion_model = LIBMV_DISTORTION_MODEL_DIVISION;
			camera_intrinsics_options->division_k1 = camera->division_k1;
			camera_intrinsics_options->division_k2 = camera->division_k2;
			break;
		default:
			BLI_assert(!"Unknown distortion model");
	}

	camera_intrinsics_options->image_width = calibration_width;
	camera_intrinsics_options->image_height = (int) (calibration_height * aspy);
}

void tracking_trackingCameraFromIntrinscisOptions(MovieTracking *tracking,
                                                  const libmv_CameraIntrinsicsOptions *camera_intrinsics_options)
{
	float aspy = 1.0f / tracking->camera.pixel_aspect;
	MovieTrackingCamera *camera = &tracking->camera;

	camera->focal = camera_intrinsics_options->focal_length;

	camera->principal[0] = camera_intrinsics_options->principal_point_x;
	camera->principal[1] = camera_intrinsics_options->principal_point_y / (double) aspy;

	switch (camera_intrinsics_options->distortion_model) {
		case LIBMV_DISTORTION_MODEL_POLYNOMIAL:
			camera->distortion_model = TRACKING_DISTORTION_MODEL_POLYNOMIAL;
			camera->k1 = camera_intrinsics_options->polynomial_k1;
			camera->k2 = camera_intrinsics_options->polynomial_k2;
			camera->k3 = camera_intrinsics_options->polynomial_k3;
			break;
		case LIBMV_DISTORTION_MODEL_DIVISION:
			camera->distortion_model = TRACKING_DISTORTION_MODEL_DIVISION;
			camera->division_k1 = camera_intrinsics_options->division_k1;
			camera->division_k2 = camera_intrinsics_options->division_k2;
			break;
		default:
			BLI_assert(!"Unknown distortion model");
	}
}
