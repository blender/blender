/* SPDX-FileCopyrightText: 2011 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 *
 * This file contains declarations of function which are used
 * by multiple tracking files but which should not be public.
 */

#pragma once

#include "BLI_threads.h"

#ifdef __cplusplus
extern "C" {
#endif

struct GHash;
struct MovieTracking;
struct MovieTrackingMarker;

struct libmv_CameraIntrinsicsOptions;

/*********************** Tracks map *************************/

typedef struct TracksMap {
  char object_name[MAX_NAME];

  int num_tracks;
  MovieTrackingTrack *tracks;

  struct GHash *hash;

  int ptr;

  /* Spin lock is used to sync context during tracking. */
  SpinLock spin_lock;
} TracksMap;

struct TracksMap *tracks_map_new(const char *object_name, int num_tracks);
int tracks_map_get_size(struct TracksMap *map);
void tracks_map_insert(struct TracksMap *map, struct MovieTrackingTrack *track);
void tracks_map_free(struct TracksMap *map);
void tracks_map_merge(struct TracksMap *map, struct MovieTracking *tracking);

/*********************** Space transformation functions *************************/

void tracking_get_search_origin_frame_pixel(int frame_width,
                                            int frame_height,
                                            const struct MovieTrackingMarker *marker,
                                            float frame_pixel[2]);

/**
 * Each marker has 5 coordinates associated with it that get warped with
 * tracking: the four corners ("pattern_corners"), and the center ("pos").
 * This function puts those 5 points into the appropriate frame for tracking
 * (the "search" coordinate frame).
 */
void tracking_get_marker_coords_for_tracking(int frame_width,
                                             int frame_height,
                                             const struct MovieTrackingMarker *marker,
                                             double search_pixel_x[5],
                                             double search_pixel_y[5]);

/**
 * Inverse of #tracking_get_marker_coords_for_tracking.
 */
void tracking_set_marker_coords_from_tracking(int frame_width,
                                              int frame_height,
                                              struct MovieTrackingMarker *marker,
                                              const double search_pixel_x[5],
                                              const double search_pixel_y[5]);

/**
 * Convert the lens principal point (optical center) between normalized and pixel spaces.
 *
 * The normalized space stores principal point relative to the frame center which has normalized
 * principal coordinate of (0, 0). The right top corner of the frame corresponds to a normalized
 * principal coordinate of (1, 1), and the left bottom corner corresponds to coordinate of
 * (-1, -1).
 *
 * The pixel space is measured in pixels, with the reference being the left bottom corner of
 * the frame.
 */
void tracking_principal_point_normalized_to_pixel(const float principal_point_normalized[2],
                                                  int frame_width,
                                                  int frame_height,
                                                  float r_principal_point_pixel[2]);
void tracking_principal_point_pixel_to_normalized(const float principal_point_pixel[2],
                                                  int frame_width,
                                                  int frame_height,
                                                  float r_principal_point_normalized[2]);

/*********************** General purpose utility functions *************************/

/**
 * Place a disabled marker before or after specified ref_marker.
 *
 * If before is truth, disabled marker is placed before reference
 * one, and it's placed after it otherwise.
 *
 * If there's already a marker at the frame where disabled one is expected to be placed,
 * nothing will happen if overwrite is false.
 */
void tracking_marker_insert_disabled(struct MovieTrackingTrack *track,
                                     const struct MovieTrackingMarker *ref_marker,
                                     bool before,
                                     bool overwrite);

/**
 * Fill in Libmv C-API camera intrinsics options from tracking structure.
 */
void tracking_cameraIntrinscisOptionsFromTracking(
    struct MovieTracking *tracking,
    int calibration_width,
    int calibration_height,
    struct libmv_CameraIntrinsicsOptions *camera_intrinsics_options);

void tracking_trackingCameraFromIntrinscisOptions(
    struct MovieTracking *tracking,
    const struct libmv_CameraIntrinsicsOptions *camera_intrinsics_options);

struct libmv_TrackRegionOptions;

/**
 * Fill in libmv tracker options structure with settings need to be used to perform track.
 */
void tracking_configure_tracker(const MovieTrackingTrack *track,
                                float *mask,
                                bool is_backwards,
                                struct libmv_TrackRegionOptions *options);

/**
 * Get previous keyframed marker.
 */
struct MovieTrackingMarker *tracking_get_keyframed_marker(struct MovieTrackingTrack *track,
                                                          int current_frame,
                                                          bool backwards);

/*********************** Masking *************************/

/**
 * Region is in pixel space, relative to marker's center.
 */
float *tracking_track_get_mask_for_region(const int frame_width,
                                          const int frame_height,
                                          const float region_min[2],
                                          const float region_max[2],
                                          const MovieTrackingTrack *track);

/*********************** Frame Accessor *************************/

struct libmv_FrameAccessor;

#define MAX_ACCESSOR_CLIP 64
typedef struct TrackingImageAccessor {
  struct MovieClip *clips[MAX_ACCESSOR_CLIP];
  int num_clips;

  /* Array of tracks which are being tracked.
   * Points to actual track from the `MovieClip` (or multiple of them).
   * This accessor owns the array, but not the tracks themselves. */
  struct MovieTrackingTrack **tracks;
  int num_tracks;

  struct libmv_FrameAccessor *libmv_accessor;
  SpinLock cache_lock;
} TrackingImageAccessor;

/**
 * Clips are used to access images of an actual footage.
 * Tracks are used to access masks associated with the tracks.
 *
 * \note Both clips and tracks arrays are copied into the image accessor. It means that the caller
 * is allowed to pass temporary arrays which are only valid during initialization.
 */
TrackingImageAccessor *tracking_image_accessor_new(MovieClip *clips[MAX_ACCESSOR_CLIP],
                                                   int num_clips,
                                                   MovieTrackingTrack **tracks,
                                                   int num_tracks);
void tracking_image_accessor_destroy(TrackingImageAccessor *accessor);

#ifdef __cplusplus
}
#endif
