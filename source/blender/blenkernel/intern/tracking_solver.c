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

/** \file blender/blenkernel/intern/tracking_solver.c
 *  \ingroup bke
 *
 * This file contains blender-side implementation of camera solver.
 */

#include <limits.h>

#include "MEM_guardedalloc.h"

#include "DNA_anim_types.h"
#include "DNA_movieclip_types.h"

#include "BLI_utildefines.h"
#include "BLI_math.h"
#include "BLI_listbase.h"
#include "BLI_string.h"

#include "BLF_translation.h"

#include "BKE_fcurve.h"
#include "BKE_tracking.h"
#include "BKE_movieclip.h"

#include "RNA_access.h"

#include "libmv-capi.h"
#include "tracking_private.h"

typedef struct MovieReconstructContext {
	struct libmv_Tracks *tracks;
	bool select_keyframes;
	int keyframe1, keyframe2;
	int refine_flags;

	struct libmv_Reconstruction *reconstruction;

	char object_name[MAX_NAME];
	bool is_camera;
	short motion_flag;

	libmv_CameraIntrinsicsOptions camera_intrinsics_options;

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

/* Create new libmv Tracks structure from blender's tracks list. */
static struct libmv_Tracks *libmv_tracks_new(MovieClip *clip, ListBase *tracksbase, int width, int height)
{
	int tracknr = 0;
	MovieTrackingTrack *track;
	struct libmv_Tracks *tracks = libmv_tracksNew();

	track = tracksbase->first;
	while (track) {
		FCurve *weight_fcurve;
		int a = 0;

		weight_fcurve = id_data_find_fcurve(&clip->id, track, &RNA_MovieTrackingTrack,
		                                    "weight", 0, NULL);

		for (a = 0; a < track->markersnr; a++) {
			MovieTrackingMarker *marker = &track->markers[a];

			if ((marker->flag & MARKER_DISABLED) == 0) {
				float weight = track->weight;

				if (weight_fcurve) {
					int scene_framenr =
						BKE_movieclip_remap_clip_to_scene_frame(clip, marker->framenr);
					weight = evaluate_fcurve(weight_fcurve, scene_framenr);
				}

				libmv_tracksInsert(tracks, marker->framenr, tracknr,
				                   (marker->pos[0] + track->offset[0]) * width,
				                   (marker->pos[1] + track->offset[1]) * height,
				                   weight);
			}
		}

		track = track->next;
		tracknr++;
	}

	return tracks;
}

/* Retrieve refined camera intrinsics from libmv to blender. */
static void reconstruct_retrieve_libmv_intrinsics(MovieReconstructContext *context, MovieTracking *tracking)
{
	struct libmv_Reconstruction *libmv_reconstruction = context->reconstruction;
	struct libmv_CameraIntrinsics *libmv_intrinsics = libmv_reconstructionExtractIntrinsics(libmv_reconstruction);

	libmv_CameraIntrinsicsOptions camera_intrinsics_options;
	libmv_cameraIntrinsicsExtractOptions(libmv_intrinsics, &camera_intrinsics_options);

	tracking_trackingCameraFromIntrinscisOptions(tracking,
	                                             &camera_intrinsics_options);
}

/* Retrieve reconstructed tracks from libmv to blender.
 * Actually, this also copies reconstructed cameras
 * from libmv to movie clip datablock.
 */
static bool reconstruct_retrieve_libmv_tracks(MovieReconstructContext *context, MovieTracking *tracking)
{
	struct libmv_Reconstruction *libmv_reconstruction = context->reconstruction;
	MovieTrackingReconstruction *reconstruction = NULL;
	MovieReconstructedCamera *reconstructed;
	MovieTrackingTrack *track;
	ListBase *tracksbase =  NULL;
	int tracknr = 0, a;
	bool ok = true;
	bool origin_set = false;
	int sfra = context->sfra, efra = context->efra;
	float imat[4][4];

	if (context->is_camera) {
		tracksbase = &tracking->tracks;
		reconstruction = &tracking->reconstruction;
	}
	else {
		MovieTrackingObject *object = BKE_tracking_object_get_named(tracking, context->object_name);

		tracksbase = &object->tracks;
		reconstruction = &object->reconstruction;
	}

	unit_m4(imat);

	track = tracksbase->first;
	while (track) {
		double pos[3];

		if (libmv_reprojectionPointForTrack(libmv_reconstruction, tracknr, pos)) {
			track->bundle_pos[0] = pos[0];
			track->bundle_pos[1] = pos[1];
			track->bundle_pos[2] = pos[2];

			track->flag |= TRACK_HAS_BUNDLE;
			track->error = libmv_reprojectionErrorForTrack(libmv_reconstruction, tracknr);
		}
		else {
			track->flag &= ~TRACK_HAS_BUNDLE;
			ok = false;

			printf("Unable to reconstruct position for track #%d '%s'\n", tracknr, track->name);
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

		if (libmv_reprojectionCameraForImage(libmv_reconstruction, a, matd)) {
			int i, j;
			float mat[4][4];
			float error = libmv_reprojectionErrorForImage(libmv_reconstruction, a);

			for (i = 0; i < 4; i++) {
				for (j = 0; j < 4; j++)
					mat[i][j] = matd[i][j];
			}

			/* Ensure first camera has got zero rotation and transform.
			 * This is essential for object tracking to work -- this way
			 * we'll always know object and environment are properly
			 * oriented.
			 *
			 * There's one weak part tho, which is requirement object
			 * motion starts at the same frame as camera motion does,
			 * otherwise that;' be a russian roulette whether object is
			 * aligned correct or not.
			 */
			if (!origin_set) {
				invert_m4_m4(imat, mat);
				unit_m4(mat);
				origin_set = true;
			}
			else {
				mul_m4_m4m4(mat, imat, mat);
			}

			copy_m4_m4(reconstructed[reconstruction->camnr].mat, mat);
			reconstructed[reconstruction->camnr].framenr = a;
			reconstructed[reconstruction->camnr].error = error;
			reconstruction->camnr++;
		}
		else {
			ok = false;
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

/* Retrieve all the libmv data from context to blender's side data blocks. */
static int reconstruct_retrieve_libmv(MovieReconstructContext *context, MovieTracking *tracking)
{
	/* take the intrinsics back from libmv */
	reconstruct_retrieve_libmv_intrinsics(context, tracking);

	return reconstruct_retrieve_libmv_tracks(context, tracking);
}

/* Convert blender's refinement flags to libmv's. */
static int reconstruct_refine_intrinsics_get_flags(MovieTracking *tracking, MovieTrackingObject *object)
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
		flags |= LIBMV_REFINE_RADIAL_DISTORTION_K1;

	if (refine & REFINE_RADIAL_DISTORTION_K2)
		flags |= LIBMV_REFINE_RADIAL_DISTORTION_K2;

	return flags;
}

/* Count tracks which has markers at both of keyframes. */
static int reconstruct_count_tracks_on_both_keyframes(MovieTracking *tracking, MovieTrackingObject *object)
{
	ListBase *tracksbase = BKE_tracking_object_get_tracks(tracking, object);
	int tot = 0;
	int frame1 = object->keyframe1, frame2 = object->keyframe2;
	MovieTrackingTrack *track;

	track = tracksbase->first;
	while (track) {
		if (BKE_tracking_track_has_enabled_marker_at_frame(track, frame1)) {
			if (BKE_tracking_track_has_enabled_marker_at_frame(track, frame2)) {
				tot++;
			}
		}

		track = track->next;
	}

	return tot;
}

/* Perform early check on whether everything is fine to start reconstruction. */
bool BKE_tracking_reconstruction_check(MovieTracking *tracking, MovieTrackingObject *object,
                                       char *error_msg, int error_size)
{
	if (tracking->settings.motion_flag & TRACKING_MOTION_MODAL) {
		/* TODO: check for number of tracks? */
		return true;
	}
	else if ((tracking->settings.reconstruction_flag & TRACKING_USE_KEYFRAME_SELECTION) == 0) {
		/* automatic keyframe selection does not require any pre-process checks */
		if (reconstruct_count_tracks_on_both_keyframes(tracking, object) < 8) {
			BLI_strncpy(error_msg,
			            N_("At least 8 common tracks on both keyframes are needed for reconstruction"),
			            error_size);

			return false;
		}
	}

#ifndef WITH_LIBMV
	BLI_strncpy(error_msg, N_("Blender is compiled without motion tracking library"), error_size);
	return false;
#endif

	return true;
}

/* Create context for camera/object motion reconstruction.
 * Copies all data needed for reconstruction from movie
 * clip datablock, so editing this clip is safe during
 * reconstruction job is in progress.
 */
MovieReconstructContext *BKE_tracking_reconstruction_context_new(MovieClip *clip, MovieTrackingObject *object,
                                                                 int keyframe1, int keyframe2, int width, int height)
{
	MovieTracking *tracking = &clip->tracking;
	MovieReconstructContext *context = MEM_callocN(sizeof(MovieReconstructContext), "MovieReconstructContext data");
	ListBase *tracksbase = BKE_tracking_object_get_tracks(tracking, object);
	float aspy = 1.0f / tracking->camera.pixel_aspect;
	int num_tracks = BLI_listbase_count(tracksbase);
	int sfra = INT_MAX, efra = INT_MIN;
	MovieTrackingTrack *track;

	BLI_strncpy(context->object_name, object->name, sizeof(context->object_name));
	context->is_camera = object->flag & TRACKING_OBJECT_CAMERA;
	context->motion_flag = tracking->settings.motion_flag;

	context->select_keyframes =
		(tracking->settings.reconstruction_flag & TRACKING_USE_KEYFRAME_SELECTION) != 0;

	tracking_cameraIntrinscisOptionsFromTracking(tracking,
                                                 width, height,
                                                 &context->camera_intrinsics_options);

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
			sfra = min_ii(sfra, first_marker->framenr);

		if (last >= 0)
			efra = max_ii(efra, last_marker->framenr);

		tracks_map_insert(context->tracks_map, track, NULL);

		track = track->next;
	}

	context->sfra = sfra;
	context->efra = efra;

	context->tracks = libmv_tracks_new(clip, tracksbase, width, height * aspy);
	context->keyframe1 = keyframe1;
	context->keyframe2 = keyframe2;
	context->refine_flags = reconstruct_refine_intrinsics_get_flags(tracking, object);

	return context;
}

/* Free memory used by a reconstruction process. */
void BKE_tracking_reconstruction_context_free(MovieReconstructContext *context)
{
	if (context->reconstruction)
		libmv_reconstructionDestroy(context->reconstruction);

	libmv_tracksDestroy(context->tracks);

	tracks_map_free(context->tracks_map, NULL);

	MEM_freeN(context);
}

/* Callback which is called from libmv side to update progress in the interface. */
static void reconstruct_update_solve_cb(void *customdata, double progress, const char *message)
{
	ReconstructProgressData *progressdata = customdata;

	if (progressdata->progress) {
		*progressdata->progress = progress;
		*progressdata->do_update = true;
	}

	BLI_snprintf(progressdata->stats_message, progressdata->message_size, "Solving camera | %s", message);
}

/* Fill in reconstruction options structure from reconstruction context. */
static void reconstructionOptionsFromContext(libmv_ReconstructionOptions *reconstruction_options,
                                             MovieReconstructContext *context)
{
	reconstruction_options->select_keyframes = context->select_keyframes;

	reconstruction_options->keyframe1 = context->keyframe1;
	reconstruction_options->keyframe2 = context->keyframe2;

	reconstruction_options->refine_intrinsics = context->refine_flags;
}

/* Solve camera/object motion and reconstruct 3D markers position
 * from a prepared reconstruction context.
 *
 * stop is not actually used at this moment, so reconstruction
 * job could not be stopped.
 *
 * do_update, progress and stat_message are set by reconstruction
 * callback in libmv side and passing to an interface.
 */
void BKE_tracking_reconstruction_solve(MovieReconstructContext *context, short *stop, short *do_update,
                                       float *progress, char *stats_message, int message_size)
{
	float error;

	ReconstructProgressData progressdata;

	libmv_ReconstructionOptions reconstruction_options;

	progressdata.stop = stop;
	progressdata.do_update = do_update;
	progressdata.progress = progress;
	progressdata.stats_message = stats_message;
	progressdata.message_size = message_size;

	reconstructionOptionsFromContext(&reconstruction_options, context);

	if (context->motion_flag & TRACKING_MOTION_MODAL) {
		context->reconstruction = libmv_solveModal(context->tracks,
		                                           &context->camera_intrinsics_options,
		                                           &reconstruction_options,
		                                           reconstruct_update_solve_cb, &progressdata);
	}
	else {
		context->reconstruction = libmv_solveReconstruction(context->tracks,
		                                                    &context->camera_intrinsics_options,
		                                                    &reconstruction_options,
		                                                    reconstruct_update_solve_cb, &progressdata);

		if (context->select_keyframes) {
			/* store actual keyframes used for reconstruction to update them in the interface later */
			context->keyframe1 = reconstruction_options.keyframe1;
			context->keyframe2 = reconstruction_options.keyframe2;
		}
	}

	error = libmv_reprojectionError(context->reconstruction);

	context->reprojection_error = error;
}

/* Finish reconstruction process by copying reconstructed data
 * to an actual movie clip datablock.
 */
bool BKE_tracking_reconstruction_finish(MovieReconstructContext *context, MovieTracking *tracking)
{
	MovieTrackingReconstruction *reconstruction;
	MovieTrackingObject *object;

	tracks_map_merge(context->tracks_map, tracking);
	BKE_tracking_dopesheet_tag_update(tracking);

	object = BKE_tracking_object_get_named(tracking, context->object_name);
	
	if (context->is_camera)
		reconstruction = &tracking->reconstruction;
	else
		reconstruction = &object->reconstruction;

	/* update keyframe in the interface */
	if (context->select_keyframes) {
		object->keyframe1 = context->keyframe1;
		object->keyframe2 = context->keyframe2;
	}

	reconstruction->error = context->reprojection_error;
	reconstruction->flag |= TRACKING_RECONSTRUCTED;

	if (!reconstruct_retrieve_libmv(context, tracking))
		return false;

	return true;
}

static void tracking_scale_reconstruction(ListBase *tracksbase, MovieTrackingReconstruction *reconstruction,
                                          const float scale[3])
{
	MovieTrackingTrack *track;
	int i;
	float first_camera_delta[3] = {0.0f, 0.0f, 0.0f};

	if (reconstruction->camnr > 0) {
		mul_v3_v3v3(first_camera_delta, reconstruction->cameras[0].mat[3], scale);
	}

	for (i = 0; i < reconstruction->camnr; i++) {
		MovieReconstructedCamera *camera = &reconstruction->cameras[i];
		mul_v3_v3(camera->mat[3], scale);
		sub_v3_v3(camera->mat[3], first_camera_delta);
	}

	for (track = tracksbase->first; track; track = track->next) {
		if (track->flag & TRACK_HAS_BUNDLE) {
			mul_v3_v3(track->bundle_pos, scale);
			sub_v3_v3(track->bundle_pos, first_camera_delta);
		}
	}
}

/* Apply scale on all reconstructed cameras and bundles,
 * used by camera scale apply operator.
 */
void BKE_tracking_reconstruction_scale(MovieTracking *tracking, float scale[3])
{
	MovieTrackingObject *object;

	for (object = tracking->objects.first; object; object = object->next) {
		ListBase *tracksbase;
		MovieTrackingReconstruction *reconstruction;

		tracksbase = BKE_tracking_object_get_tracks(tracking, object);
		reconstruction = BKE_tracking_object_get_reconstruction(tracking, object);

		tracking_scale_reconstruction(tracksbase, reconstruction, scale);
	}
}
