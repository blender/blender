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

#ifndef BKE_TRACKING_H
#define BKE_TRACKING_H

/** \file BKE_trackingp.h
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

void BKE_tracking_init_settings(struct MovieTracking *tracking);
void BKE_tracking_clamp_track(struct MovieTrackingTrack *track, int event);
void BKE_tracking_track_flag(struct MovieTrackingTrack *track, int area, int flag, int clear);

struct MovieTrackingTrack *BKE_tracking_add_track(struct MovieTracking *tracking, struct ListBase *tracksbase,
			float x, float y, int framenr, int width, int height);
void BKE_tracking_insert_marker(struct MovieTrackingTrack *track, struct MovieTrackingMarker *marker);
void BKE_tracking_delete_marker(struct MovieTrackingTrack *track, int framenr);

struct MovieTrackingMarker *BKE_tracking_get_marker(struct MovieTrackingTrack *track, int framenr);
struct MovieTrackingMarker *BKE_tracking_ensure_marker(struct MovieTrackingTrack *track, int framenr);
struct MovieTrackingMarker *BKE_tracking_exact_marker(struct MovieTrackingTrack *track, int framenr);
int BKE_tracking_has_marker(struct MovieTrackingTrack *track, int framenr);

void BKE_tracking_free_track(struct MovieTrackingTrack *track);

void BKE_tracking_clear_path(struct MovieTrackingTrack *track, int ref_frame, int action);

int BKE_tracking_test_join_tracks(struct MovieTrackingTrack *dst_track, struct MovieTrackingTrack *src_track);
void BKE_tracking_join_tracks(struct MovieTrackingTrack *dst_track, struct MovieTrackingTrack *src_track);
void BKE_tracking_free(struct MovieTracking *tracking);

struct ImBuf *BKE_tracking_get_pattern_imbuf(struct ImBuf *ibuf, struct MovieTrackingTrack *track,
			struct MovieTrackingMarker *marker, int margin, int anchored, float pos[2], int origin[2]);
struct ImBuf *BKE_tracking_get_search_imbuf(struct ImBuf *ibuf, struct MovieTrackingTrack *track,
			struct MovieTrackingMarker *marker, int margin, int anchored, float pos[2], int origin[2]);

void BKE_track_unique_name(struct ListBase *tracksbase, struct MovieTrackingTrack *track);

struct MovieTrackingTrack *BKE_tracking_named_track(struct MovieTracking *tracking, struct MovieTrackingObject *object, const char *name);
struct MovieTrackingTrack *BKE_tracking_indexed_track(struct MovieTracking *tracking, int tracknr, struct ListBase **tracksbase_r);

void BKE_tracking_camera_shift(struct MovieTracking *tracking, int winx, int winy, float *shiftx, float *shifty);
void BKE_tracking_camera_to_blender(struct MovieTracking *tracking, struct Scene *scene, struct Camera *camera, int width, int height);

void BKE_get_tracking_mat(struct Scene *scene, struct Object *ob, float mat[4][4]);
void BKE_tracking_projection_matrix(struct MovieTracking *tracking, struct MovieTrackingObject *object,
			int framenr, int winx, int winy, float mat[4][4]);

struct ListBase *BKE_tracking_get_tracks(struct MovieTracking *tracking);
struct MovieTrackingReconstruction *BKE_tracking_get_reconstruction(struct MovieTracking *tracking);

struct MovieTrackingTrack *BKE_tracking_active_track(struct MovieTracking *tracking);
struct MovieTrackingObject *BKE_tracking_active_object(struct MovieTracking *tracking);
struct MovieTrackingObject *BKE_tracking_get_camera_object(struct MovieTracking *tracking);
struct ListBase *BKE_tracking_object_tracks(struct MovieTracking *tracking, struct MovieTrackingObject *object);
struct MovieTrackingReconstruction *BKE_tracking_object_reconstruction(struct MovieTracking *tracking,
			struct MovieTrackingObject *object);

/* 2D tracking */
struct MovieTrackingContext *BKE_tracking_context_new(struct MovieClip *clip, struct MovieClipUser *user,
			short backwards, short disable_failed, short sequence);
void BKE_tracking_context_free(struct MovieTrackingContext *context);
void BKE_tracking_sync(struct MovieTrackingContext *context);
void BKE_tracking_sync_user(struct MovieClipUser *user, struct MovieTrackingContext *context);
int BKE_tracking_next(struct MovieTrackingContext *context);

/* Camera solving */
int BKE_tracking_can_reconstruct(struct MovieTracking *tracking, struct MovieTrackingObject *object,
			char *error_msg, int error_size);

struct MovieReconstructContext* BKE_tracking_reconstruction_context_new(struct MovieTracking *tracking,
			struct MovieTrackingObject *object, int keyframe1, int keyframe2, int width, int height);
void BKE_tracking_reconstruction_context_free(struct MovieReconstructContext *context);
void BKE_tracking_solve_reconstruction(struct MovieReconstructContext *context,
			short *stop, short *do_update, float *progress, char *stats_message, int message_size);
int BKE_tracking_finish_reconstruction(struct MovieReconstructContext *context, struct MovieTracking *tracking);

struct MovieReconstructedCamera *BKE_tracking_get_reconstructed_camera(struct MovieTracking *tracking,
			struct MovieTrackingObject *object, int framenr);
void BKE_tracking_get_interpolated_camera(struct MovieTracking *tracking,
			struct MovieTrackingObject *object, int framenr, float mat[4][4]);

/* Feature detection */
void BKE_tracking_detect_fast(struct MovieTracking *tracking, struct ListBase *tracksbase, struct ImBuf *imbuf,
			int framenr, int margin, int min_trackness, int min_distance, struct bGPDlayer *layer,
			int place_outside_layer);

/* 2D stabilization */
void BKE_tracking_stabilization_data(struct MovieTracking *tracking, int framenr, int width, int height, float loc[2], float *scale, float *angle);
struct ImBuf *BKE_tracking_stabilize(struct MovieTracking *tracking, int framenr, struct ImBuf *ibuf, float loc[2], float *scale, float *angle);
void BKE_tracking_stabdata_to_mat4(int width, int height, float loc[2], float scale, float angle, float mat[4][4]);

/* Distortion/Undistortion */
void BKE_tracking_apply_intrinsics(struct MovieTracking *tracking, float co[2], float nco[2]);
void BKE_tracking_invert_intrinsics(struct MovieTracking *tracking, float co[2], float nco[2]);

struct MovieDistortion *BKE_tracking_distortion_create(void);
struct MovieDistortion *BKE_tracking_distortion_copy(struct MovieDistortion *distortion);
struct ImBuf *BKE_tracking_distortion_exec(struct MovieDistortion *distortion, struct MovieTracking *tracking,
			struct ImBuf *ibuf, int width, int height, float overscan, int undistort);
void BKE_tracking_distortion_destroy(struct MovieDistortion *distortion);

struct ImBuf *BKE_tracking_undistort(struct MovieTracking *tracking, struct ImBuf *ibuf, int width, int height, float overscan);
struct ImBuf *BKE_tracking_distort(struct MovieTracking *tracking, struct ImBuf *ibuf, int width, int height, float overscan);

/* Object tracking */
struct MovieTrackingObject *BKE_tracking_new_object(struct MovieTracking *tracking, const char *name);
void BKE_tracking_remove_object(struct MovieTracking *tracking, struct MovieTrackingObject *object);
void BKE_tracking_object_unique_name(struct MovieTracking *tracking, struct MovieTrackingObject *object);
struct MovieTrackingObject *BKE_tracking_named_object(struct MovieTracking *tracking, const char *name);

/* Select */
void BKE_tracking_select_track(struct ListBase *tracksbase, struct MovieTrackingTrack *track, int area, int extend);
void BKE_tracking_deselect_track(struct MovieTrackingTrack *track, int area);

#define TRACK_SELECTED(track)				((((track)->flag&TRACK_HIDDEN)==0) && ((track)->flag&SELECT || (track)->pat_flag&SELECT || (track)->search_flag&SELECT))
#define TRACK_AREA_SELECTED(track, area)	((((track)->flag&TRACK_HIDDEN)==0) && ((area)==TRACK_AREA_POINT?(track)->flag&SELECT : ((area)==TRACK_AREA_PAT?(track)->pat_flag&SELECT:(track)->search_flag&SELECT)))
#define TRACK_VIEW_SELECTED(sc, track)		((TRACK_AREA_SELECTED(track, TRACK_AREA_POINT) || (((sc)->flag&SC_SHOW_MARKER_PATTERN && TRACK_AREA_SELECTED(track, TRACK_AREA_PAT))) || (((sc)->flag&SC_SHOW_MARKER_SEARCH && TRACK_AREA_SELECTED(track, TRACK_AREA_SEARCH)))))

#define MARKER_VISIBLE(sc, marker)			(((marker)->flag&MARKER_DISABLED)==0 || ((sc)->flag&SC_HIDE_DISABLED)==0)

#define CLAMP_PAT_DIM		1
#define CLAMP_PAT_POS		2
#define CLAMP_SEARCH_DIM	3
#define CLAMP_SEARCH_POS	4
#define CLAMP_PYRAMID_LEVELS	5

#define TRACK_AREA_NONE		-1
#define TRACK_AREA_POINT	1
#define TRACK_AREA_PAT		2
#define TRACK_AREA_SEARCH	4

#define TRACK_AREA_ALL		(TRACK_AREA_POINT|TRACK_AREA_PAT|TRACK_AREA_SEARCH)

#endif
