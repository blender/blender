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

struct ImBuf;
struct MovieTrackingTrack;
struct MovieTrackingMarker;
struct MovieTracking;
struct MovieTrackingContext;
struct MovieClipUser;
struct Scene;

void BKE_tracking_clamp_track(struct MovieTrackingTrack *track, int event);
void BKE_tracking_track_flag(struct MovieTrackingTrack *track, int area, int flag, int clear);
void BKE_tracking_insert_marker(struct MovieTrackingTrack *track, struct MovieTrackingMarker *marker);
void BKE_tracking_delete_marker(struct MovieTrackingTrack *track, int framenr);
struct MovieTrackingMarker *BKE_tracking_get_marker(struct MovieTrackingTrack *track, int framenr);
struct MovieTrackingMarker *BKE_tracking_ensure_marker(struct MovieTrackingTrack *track, int framenr);
struct MovieTrackingMarker *BKE_tracking_exact_marker(struct MovieTrackingTrack *track, int framenr);
int BKE_tracking_has_marker(struct MovieTrackingTrack *track, int framenr);

void BKE_tracking_free_track(struct MovieTrackingTrack *track);
struct MovieTrackingTrack *BKE_tracking_copy_track(struct MovieTrackingTrack *track);
void BKE_tracking_clear_path(struct MovieTrackingTrack *track, int ref_frame, int action);
void BKE_tracking_free(struct MovieTracking *tracking);

struct ImBuf *BKE_tracking_acquire_pattern_imbuf(struct ImBuf *ibuf, struct MovieTrackingTrack *track,
			struct MovieTrackingMarker *marker, int margin, float pos[2], int origin[2]);
struct ImBuf *BKE_tracking_acquire_search_imbuf(struct ImBuf *ibuf, struct MovieTrackingTrack *track,
			struct MovieTrackingMarker *marker, int margin, float pos[2], int origin[2]);

struct MovieTrackingContext *BKE_tracking_context_new(struct MovieClip *clip, struct MovieClipUser *user, int backwards);
void BKE_tracking_context_free(struct MovieTrackingContext *context);
void BKE_tracking_sync(struct MovieTrackingContext *context);
void BKE_tracking_sync_user(struct MovieClipUser *user, struct MovieTrackingContext *context);
int BKE_tracking_next(struct MovieTrackingContext *context);

void BKE_tracking_solve_reconstruction(struct MovieClip *clip);

void BKE_track_unique_name(struct MovieTracking *tracking, struct MovieTrackingTrack *track);
struct MovieTrackingTrack *BKE_find_track_by_name(struct MovieTracking *tracking, const char *name);

struct MovieReconstructedCamera *BKE_tracking_get_reconstructed_camera(struct MovieTracking *tracking, int framenr);

void BKE_get_tracking_mat(struct Scene *scene, float mat[4][4]);
void BKE_tracking_projection_matrix(struct MovieTracking *tracking, int framenr, int winx, int winy, float mat[4][4]);
void BKE_tracking_apply_intrinsics(struct MovieTracking *tracking, float co[2], float width, float height, float nco[2]);

#define TRACK_SELECTED(track) ((track)->flag&SELECT || (track)->pat_flag&SELECT || (track)->search_flag&SELECT)
#define TRACK_AREA_SELECTED(track, area) ((area)==TRACK_AREA_POINT?(track)->flag&SELECT : ((area)==TRACK_AREA_PAT?(track)->pat_flag&SELECT:(track)->search_flag&SELECT))
#define TRACK_VIEW_SELECTED(track) ((track->flag&TRACK_HIDDEN)==0 && TRACK_SELECTED(track))

#define CLAMP_PAT_DIM		1
#define CLAMP_PAT_POS		2
#define CLAMP_SEARCH_DIM	3
#define CLAMP_SEARCH_POS	4

#define TRACK_AREA_NONE		-1
#define TRACK_AREA_POINT	1
#define TRACK_AREA_PAT		2
#define TRACK_AREA_SEARCH	4

#define TRACK_AREA_ALL		(TRACK_AREA_POINT|TRACK_AREA_PAT|TRACK_AREA_SEARCH)

#endif
