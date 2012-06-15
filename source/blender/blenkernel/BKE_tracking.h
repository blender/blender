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
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __BKE_TRACKING_H__
#define __BKE_TRACKING_H__

/** \file BKE_tracking.h
 *  \ingroup bke
 *  \author Sergey Sharybin
 */

struct bGPDlayer;
struct ImBuf;
struct ListBase;
struct MovieReconstructContext;
struct MovieTrackingTrack;
struct MovieTrackingMarker;
struct MovieTracking;
struct MovieTrackingContext;
struct MovieTrackingObject;
struct MovieClipUser;
struct MovieDistortion;
struct Camera;
struct Object;
struct Scene;

/* **** Common functions **** */

void BKE_tracking_free(struct MovieTracking *tracking);

void BKE_tracking_settings_init(struct MovieTracking *tracking);

struct ListBase *BKE_tracking_get_active_tracks(struct MovieTracking *tracking);
struct MovieTrackingReconstruction *BKE_tracking_get_active_reconstruction(struct MovieTracking *tracking);

/* matrices for constraints and drawing */
void BKE_tracking_get_camera_object_matrix(struct Scene *scene, struct Object *ob, float mat[4][4]);
void BKE_tracking_get_projection_matrix(struct MovieTracking *tracking, struct MovieTrackingObject *object,
                                        int framenr, int winx, int winy, float mat[4][4]);

/* **** Clipboard **** */
void BKE_tracking_clipboard_free(void);
void BKE_tracking_clipboard_copy_tracks(struct MovieTracking *tracking, struct MovieTrackingObject *object);
int BKE_tracking_clipboard_has_tracks(void);
void BKE_tracking_clipboard_paste_tracks(struct MovieTracking *tracking, struct MovieTrackingObject *object);

/* **** Track **** */
struct MovieTrackingTrack *BKE_tracking_track_add(struct MovieTracking *tracking, struct ListBase *tracksbase,
                                                  float x, float y, int framenr, int width, int height);
void BKE_tracking_track_unique_name(struct ListBase *tracksbase, struct MovieTrackingTrack *track);
void BKE_tracking_track_free(struct MovieTrackingTrack *track);

void BKE_tracking_track_flag_set(struct MovieTrackingTrack *track, int area, int flag);
void BKE_tracking_track_flag_clear(struct MovieTrackingTrack *track, int area, int flag);

int BKE_tracking_track_has_marker_at_frame(struct MovieTrackingTrack *track, int framenr);
int BKE_tracking_track_has_enabled_marker_at_frame(struct MovieTrackingTrack *track, int framenr);

void BKE_tracking_track_path_clear(struct MovieTrackingTrack *track, int ref_frame, int action);
void BKE_tracking_tracks_join(struct MovieTrackingTrack *dst_track, struct MovieTrackingTrack *src_track);

struct MovieTrackingTrack *BKE_tracking_track_get_named(struct MovieTracking *tracking,
                                                        struct MovieTrackingObject *object,
                                                        const char *name);
struct MovieTrackingTrack *BKE_tracking_track_get_indexed(struct MovieTracking *tracking, int tracknr,
                                                          struct ListBase **tracksbase_r);

struct MovieTrackingTrack *BKE_tracking_track_get_active(struct MovieTracking *tracking);

float *BKE_tracking_track_get_mask(int frame_width, int frame_height, struct MovieTrackingTrack *track,
                                   struct MovieTrackingMarker *marker);

/* selection */
void BKE_tracking_track_select(struct ListBase *tracksbase, struct MovieTrackingTrack *track, int area, int extend);
void BKE_tracking_track_deselect(struct MovieTrackingTrack *track, int area);

/* **** Marker **** */
struct MovieTrackingMarker *BKE_tracking_marker_insert(struct MovieTrackingTrack *track,
                                                       struct MovieTrackingMarker *marker);
void BKE_tracking_marker_delete(struct MovieTrackingTrack *track, int framenr);

void BKE_tracking_marker_clamp(struct MovieTrackingMarker *marker, int event);

struct MovieTrackingMarker *BKE_tracking_marker_get(struct MovieTrackingTrack *track, int framenr);
struct MovieTrackingMarker *BKE_tracking_marker_get_exact(struct MovieTrackingTrack *track, int framenr);
struct MovieTrackingMarker *BKE_tracking_marker_ensure(struct MovieTrackingTrack *track, int framenr);

void BKE_tracking_marker_pattern_minmax(const struct MovieTrackingMarker *marker, float min[2], float max[2]);

/* **** Object **** */
struct MovieTrackingObject *BKE_tracking_object_add(struct MovieTracking *tracking, const char *name);
void BKE_tracking_object_delete(struct MovieTracking *tracking, struct MovieTrackingObject *object);

void BKE_tracking_object_unique_name(struct MovieTracking *tracking, struct MovieTrackingObject *object);

struct MovieTrackingObject *BKE_tracking_object_get_named(struct MovieTracking *tracking, const char *name);

struct MovieTrackingObject *BKE_tracking_object_get_active(struct MovieTracking *tracking);
struct MovieTrackingObject *BKE_tracking_object_get_camera(struct MovieTracking *tracking);

struct ListBase *BKE_tracking_object_get_tracks(struct MovieTracking *tracking, struct MovieTrackingObject *object);
struct MovieTrackingReconstruction *BKE_tracking_object_get_reconstruction(struct MovieTracking *tracking,
                                                                           struct MovieTrackingObject *object);

/* **** Camera **** */
void BKE_tracking_camera_shift_get(struct MovieTracking *tracking, int winx, int winy, float *shiftx, float *shifty);
void BKE_tracking_camera_to_blender(struct MovieTracking *tracking, struct Scene *scene,
                                    struct Camera *camera, int width, int height);

struct MovieReconstructedCamera *BKE_tracking_camera_get_reconstructed(struct MovieTracking *tracking,
                                                                       struct MovieTrackingObject *object,
                                                                       int framenr);
void BKE_tracking_camera_get_reconstructed_interpolate(struct MovieTracking *tracking,
                                                       struct MovieTrackingObject *object,
                                                       int framenr, float mat[4][4]);

/* **** Distortion/Undistortion **** */
struct MovieDistortion *BKE_tracking_distortion_new(void);
void BKE_tracking_distortion_update(struct MovieDistortion *distortion, struct MovieTracking *tracking,
                                    int calibration_width, int calibration_height);
struct MovieDistortion *BKE_tracking_distortion_copy(struct MovieDistortion *distortion);
struct ImBuf *BKE_tracking_distortion_exec(struct MovieDistortion *distortion, struct MovieTracking *tracking,
                                           struct ImBuf *ibuf, int width, int height, float overscan, int undistort);
void BKE_tracking_distortion_free(struct MovieDistortion *distortion);

void BKE_tracking_distort_v2(struct MovieTracking *tracking, float co[2], float nco[2]);
void BKE_tracking_undistort_v2(struct MovieTracking *tracking, float co[2], float nco[2]);

struct ImBuf *BKE_tracking_undistort_frame(struct MovieTracking *tracking, struct ImBuf *ibuf,
                                           int calibration_width, int calibration_height, float overscan);
struct ImBuf *BKE_tracking_distort_frame(struct MovieTracking *tracking, struct ImBuf *ibuf,
                                         int calibration_width, int calibration_height, float overscan);

/* **** Image sampling **** */
struct ImBuf *BKE_tracking_sample_pattern(int frame_width, int frame_height,
                                          struct ImBuf *struct_ibuf, struct MovieTrackingTrack *track,
                                          struct MovieTrackingMarker *marker, int use_mask,
                                          int num_samples_x, int num_samples_y, float pos[2]);
struct ImBuf *BKE_tracking_get_pattern_imbuf(struct ImBuf *ibuf, struct MovieTrackingTrack *track,
                                             struct MovieTrackingMarker *marker, int anchored, int disable_channels);
struct ImBuf *BKE_tracking_get_search_imbuf(struct ImBuf *ibuf, struct MovieTrackingTrack *track,
                                            struct MovieTrackingMarker *marker, int anchored, int disable_channels);

void BKE_tracking_disable_channels(struct ImBuf *ibuf, int disable_red, int disable_green,
                                   int disable_blue, int grayscale);

/* **** 2D tracking **** */
struct MovieTrackingContext *BKE_tracking_context_new(struct MovieClip *clip, struct MovieClipUser *user,
                                                      short backwards, short sequence);
void BKE_tracking_context_free(struct MovieTrackingContext *context);
void BKE_tracking_context_sync(struct MovieTrackingContext *context);
void BKE_tracking_context_sync_user(const struct MovieTrackingContext *context, struct MovieClipUser *user);
int BKE_tracking_context_step(struct MovieTrackingContext *context);

/* **** Camera solving **** */
int BKE_tracking_reconstruction_check(struct MovieTracking *tracking, struct MovieTrackingObject *object,
                                      char *error_msg, int error_size);

struct MovieReconstructContext *BKE_tracking_reconstruction_context_new(struct MovieTracking *tracking,
                                                                        struct MovieTrackingObject *object,
                                                                        int keyframe1, int keyframe2,
                                                                        int width, int height);
void BKE_tracking_reconstruction_context_free(struct MovieReconstructContext *context);
void BKE_tracking_reconstruction_solve(struct MovieReconstructContext *context, short *stop, short *do_update,
                                       float *progress, char *stats_message, int message_size);
int BKE_tracking_reconstruction_finish(struct MovieReconstructContext *context, struct MovieTracking *tracking);

/* **** Feature detection **** */
void BKE_tracking_detect_fast(struct MovieTracking *tracking, struct ListBase *tracksbase, struct ImBuf *imbuf,
                              int framenr, int margin, int min_trackness, int min_distance, struct bGPDlayer *layer,
                              int place_outside_layer);

/* **** 2D stabilization **** */
void BKE_tracking_stabilization_data_get(struct MovieTracking *tracking, int framenr, int width, int height,
                                         float loc[2], float *scale, float *angle);
struct ImBuf *BKE_tracking_stabilize_frame(struct MovieTracking *tracking, int framenr, struct ImBuf *ibuf,
                                           float loc[2], float *scale, float *angle);
void BKE_tracking_stabilization_data_to_mat4(int width, int height, float aspect, float loc[2],
                                             float scale, float angle, float mat[4][4]);

/* Dopesheet */
void BKE_tracking_dopesheet_tag_update(struct MovieTracking *tracking);
void BKE_tracking_dopesheet_update(struct MovieTracking *tracking);

#define TRACK_SELECTED(track)               ((track)->flag & SELECT || (track)->pat_flag & SELECT || (track)->search_flag & SELECT)

#define TRACK_AREA_SELECTED(track, area)    ((area) == TRACK_AREA_POINT ? (track)->flag & SELECT : \
                                             ((area) == TRACK_AREA_PAT ? (track)->pat_flag & SELECT : \
                                              (track)->search_flag & SELECT))

#define TRACK_VIEW_SELECTED(sc, track)      ((((track)->flag & TRACK_HIDDEN) == 0) && \
                                             (TRACK_AREA_SELECTED(track, TRACK_AREA_POINT) || \
                                              (((sc)->flag & SC_SHOW_MARKER_PATTERN) && TRACK_AREA_SELECTED(track, TRACK_AREA_PAT)) || \
                                              (((sc)->flag & SC_SHOW_MARKER_SEARCH) && TRACK_AREA_SELECTED(track, TRACK_AREA_SEARCH))))

#define MARKER_VISIBLE(sc, track, marker)       (((marker)->flag & MARKER_DISABLED) == 0 || ((sc)->flag & SC_HIDE_DISABLED) == 0 || (sc->clip->tracking.act_track == track))

#define TRACK_CLEAR_UPTO        0
#define TRACK_CLEAR_REMAINED    1
#define TRACK_CLEAR_ALL         2

#define CLAMP_PAT_DIM       1
#define CLAMP_PAT_POS       2
#define CLAMP_SEARCH_DIM    3
#define CLAMP_SEARCH_POS    4

#define TRACK_AREA_NONE     -1
#define TRACK_AREA_POINT    1
#define TRACK_AREA_PAT      2
#define TRACK_AREA_SEARCH   4

#define TRACK_AREA_ALL      (TRACK_AREA_POINT | TRACK_AREA_PAT | TRACK_AREA_SEARCH)

#endif
