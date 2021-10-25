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

/** \file blender/blenkernel/tracking_private.h
 *  \ingroup bke
 *
 * This file contains declarations of function which are used
 * by multiple tracking files but which should not be public.
 */

#ifndef __TRACKING_PRIVATE_H__
#define __TRACKING_PRIVATE_H__

#include "BLI_threads.h"

struct GHash;
struct MovieTracking;
struct MovieTrackingMarker;

struct libmv_CameraIntrinsicsOptions;

/*********************** Tracks map *************************/

typedef struct TracksMap {
	char object_name[MAX_NAME];
	bool is_camera;

	int num_tracks;
	int customdata_size;

	char *customdata;
	MovieTrackingTrack *tracks;

	struct GHash *hash;

	int ptr;

	/* Spin lock is used to sync context during tracking. */
	SpinLock spin_lock;
} TracksMap;

struct TracksMap *tracks_map_new(const char *object_name, bool is_camera, int num_tracks, int customdata_size);
int tracks_map_get_size(struct TracksMap *map);
void tracks_map_get_indexed_element(struct TracksMap *map, int index, struct MovieTrackingTrack **track, void **customdata);
void tracks_map_insert(struct TracksMap *map, struct MovieTrackingTrack *track, void *customdata);
void tracks_map_free(struct TracksMap *map, void (*customdata_free)(void *customdata));
void tracks_map_merge(struct TracksMap *map, struct MovieTracking *tracking);

/*********************** Space transformation functions *************************/

void tracking_get_search_origin_frame_pixel(int frame_width, int frame_height,
                                            const struct MovieTrackingMarker *marker,
                                            float frame_pixel[2]);

void tracking_get_marker_coords_for_tracking(int frame_width, int frame_height,
                                             const struct MovieTrackingMarker *marker,
                                             double search_pixel_x[5], double search_pixel_y[5]);

void tracking_set_marker_coords_from_tracking(int frame_width, int frame_height, struct MovieTrackingMarker *marker,
                                              const double search_pixel_x[5], const double search_pixel_y[5]);

/*********************** General purpose utility functions *************************/

void tracking_marker_insert_disabled(struct MovieTrackingTrack *track, const struct MovieTrackingMarker *ref_marker,
                                     bool before, bool overwrite);

void tracking_cameraIntrinscisOptionsFromTracking(struct MovieTracking *tracking,
                                                  int calibration_width, int calibration_height,
                                                  struct libmv_CameraIntrinsicsOptions *camera_intrinsics_options);

void tracking_trackingCameraFromIntrinscisOptions(struct MovieTracking *tracking,
                                                  const struct libmv_CameraIntrinsicsOptions *camera_intrinsics_options);

struct libmv_TrackRegionOptions;

void tracking_configure_tracker(const MovieTrackingTrack *track, float *mask,
                                struct libmv_TrackRegionOptions *options);

struct MovieTrackingMarker *tracking_get_keyframed_marker(
	struct MovieTrackingTrack *track,
	int current_frame,
	bool backwards);

/*********************** Masking *************************/

float *tracking_track_get_mask_for_region(int frame_width, int frame_height,
                                          const float region_min[2],
                                          const float region_max[2],
                                          MovieTrackingTrack *track);

/*********************** Frame accessr *************************/

struct libmv_FrameAccessor;

#define MAX_ACCESSOR_CLIP 64
typedef struct TrackingImageAccessor {
	struct MovieCache *cache;
	struct MovieClip *clips[MAX_ACCESSOR_CLIP];
	int num_clips;
	struct MovieTrackingTrack **tracks;
	int num_tracks;
	int start_frame;
	struct libmv_FrameAccessor *libmv_accessor;
} TrackingImageAccessor;

TrackingImageAccessor *tracking_image_accessor_new(MovieClip *clips[MAX_ACCESSOR_CLIP],
                                                   int num_clips,
                                                   MovieTrackingTrack **tracks,
                                                   int num_tracks,
                                                   int start_frame);
void tracking_image_accessor_destroy(TrackingImageAccessor *accessor);

#endif  /* __TRACKING_PRIVATE_H__ */
