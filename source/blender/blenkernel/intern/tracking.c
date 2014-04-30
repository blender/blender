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

#include "DNA_anim_types.h"
#include "DNA_gpencil_types.h"
#include "DNA_camera_types.h"
#include "DNA_movieclip_types.h"
#include "DNA_object_types.h"   /* SELECT */
#include "DNA_scene_types.h"

#include "BLI_utildefines.h"
#include "BLI_math.h"
#include "BLI_math_base.h"
#include "BLI_listbase.h"
#include "BLI_path_util.h"
#include "BLI_string.h"
#include "BLI_threads.h"

#include "BLF_translation.h"

#include "BKE_fcurve.h"
#include "BKE_tracking.h"
#include "BKE_movieclip.h"
#include "BKE_object.h"
#include "BKE_scene.h"

#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"

#include "RNA_access.h"

#include "raskter.h"

#include "libmv-capi.h"
#include "tracking_private.h"

typedef struct MovieDistortion {
	struct libmv_CameraIntrinsics *intrinsics;
} MovieDistortion;

static struct {
	ListBase tracks;
} tracking_clipboard;

/*********************** Common functions *************************/

/* Free the whole list of tracks, list's head and tail are set to NULL. */
static void tracking_tracks_free(ListBase *tracks)
{
	MovieTrackingTrack *track;

	for (track = tracks->first; track; track = track->next) {
		BKE_tracking_track_free(track);
	}

	BLI_freelistN(tracks);
}

/* Free the whole list of plane tracks, list's head and tail are set to NULL. */
static void tracking_plane_tracks_free(ListBase *plane_tracks)
{
	MovieTrackingPlaneTrack *plane_track;

	for (plane_track = plane_tracks->first; plane_track; plane_track = plane_track->next) {
		BKE_tracking_plane_track_free(plane_track);
	}

	BLI_freelistN(plane_tracks);
}

/* Free reconstruction structures, only frees contents of a structure,
 * (if structure is allocated in heap, it shall be handled outside).
 *
 * All the pointers inside structure becomes invalid after this call.
 */
static void tracking_reconstruction_free(MovieTrackingReconstruction *reconstruction)
{
	if (reconstruction->cameras)
		MEM_freeN(reconstruction->cameras);
}

/* Free memory used by tracking object, only frees contents of the structure,
 * (if structure is allocated in heap, it shall be handled outside).
 *
 * All the pointers inside structure becomes invalid after this call.
 */
static void tracking_object_free(MovieTrackingObject *object)
{
	tracking_tracks_free(&object->tracks);
	tracking_plane_tracks_free(&object->plane_tracks);
	tracking_reconstruction_free(&object->reconstruction);
}

/* Free list of tracking objects, list's head and tail is set to NULL. */
static void tracking_objects_free(ListBase *objects)
{
	MovieTrackingObject *object;

	/* Free objects contents. */
	for (object = objects->first; object; object = object->next)
		tracking_object_free(object);

	/* Free objects themselves. */
	BLI_freelistN(objects);
}

/* Free memory used by a dopesheet, only frees dopesheet contents.
 * leaving dopesheet crystal clean for further usage.
 */
static void tracking_dopesheet_free(MovieTrackingDopesheet *dopesheet)
{
	MovieTrackingDopesheetChannel *channel;

	/* Free channel's sergments. */
	channel = dopesheet->channels.first;
	while (channel) {
		if (channel->segments) {
			MEM_freeN(channel->segments);
		}

		channel = channel->next;
	}

	/* Free lists themselves. */
	BLI_freelistN(&dopesheet->channels);
	BLI_freelistN(&dopesheet->coverage_segments);

	/* Ensure lists are clean. */
	BLI_listbase_clear(&dopesheet->channels);
	BLI_listbase_clear(&dopesheet->coverage_segments);
	dopesheet->tot_channel = 0;
}

/* Free tracking structure, only frees structure contents
 * (if structure is allocated in heap, it shall be handled outside).
 *
 * All the pointers inside structure becomes invalid after this call.
 */
void BKE_tracking_free(MovieTracking *tracking)
{
	tracking_tracks_free(&tracking->tracks);
	tracking_plane_tracks_free(&tracking->plane_tracks);
	tracking_reconstruction_free(&tracking->reconstruction);
	tracking_objects_free(&tracking->objects);

	if (tracking->camera.intrinsics)
		BKE_tracking_distortion_free(tracking->camera.intrinsics);

	tracking_dopesheet_free(&tracking->dopesheet);
}

/* Initialize motion tracking settings to default values,
 * used when new movie clip datablock is creating.
 */
void BKE_tracking_settings_init(MovieTracking *tracking)
{
	tracking->camera.sensor_width = 35.0f;
	tracking->camera.pixel_aspect = 1.0f;
	tracking->camera.units = CAMERA_UNITS_MM;

	tracking->settings.default_motion_model = TRACK_MOTION_MODEL_TRANSLATION;
	tracking->settings.default_minimum_correlation = 0.75;
	tracking->settings.default_pattern_size = 21;
	tracking->settings.default_search_size = 71;
	tracking->settings.default_algorithm_flag |= TRACK_ALGORITHM_FLAG_USE_BRUTE;
	tracking->settings.default_weight = 1.0f;
	tracking->settings.dist = 1;
	tracking->settings.object_distance = 1;

	tracking->stabilization.scaleinf = 1.0f;
	tracking->stabilization.locinf = 1.0f;
	tracking->stabilization.rotinf = 1.0f;
	tracking->stabilization.maxscale = 2.0f;
	tracking->stabilization.filter = TRACKING_FILTER_BILINEAR;

	BKE_tracking_object_add(tracking, "Camera");
}

/* Get list base of active object's tracks. */
ListBase *BKE_tracking_get_active_tracks(MovieTracking *tracking)
{
	MovieTrackingObject *object = BKE_tracking_object_get_active(tracking);

	if (object && (object->flag & TRACKING_OBJECT_CAMERA) == 0) {
		return &object->tracks;
	}

	return &tracking->tracks;
}

/* Get list base of active object's plane tracks. */
ListBase *BKE_tracking_get_active_plane_tracks(MovieTracking *tracking)
{
	MovieTrackingObject *object = BKE_tracking_object_get_active(tracking);

	if (object && (object->flag & TRACKING_OBJECT_CAMERA) == 0) {
		return &object->plane_tracks;
	}

	return &tracking->plane_tracks;
}

/* Get reconstruction data of active object. */
MovieTrackingReconstruction *BKE_tracking_get_active_reconstruction(MovieTracking *tracking)
{
	MovieTrackingObject *object = BKE_tracking_object_get_active(tracking);

	return BKE_tracking_object_get_reconstruction(tracking, object);
}

/* Get transformation matrix for a given object which is used
 * for parenting motion tracker reconstruction to 3D world.
 */
void BKE_tracking_get_camera_object_matrix(Scene *scene, Object *ob, float mat[4][4])
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

/* Get projection matrix for camera specified by given tracking object
 * and frame number.
 *
 * NOTE: frame number should be in clip space, not scene space
 */
void BKE_tracking_get_projection_matrix(MovieTracking *tracking, MovieTrackingObject *object,
                                        int framenr, int winx, int winy, float mat[4][4])
{
	MovieReconstructedCamera *camera;
	float lens = tracking->camera.focal * tracking->camera.sensor_width / (float)winx;
	float viewfac, pixsize, left, right, bottom, top, clipsta, clipend;
	float winmat[4][4];
	float ycor =  1.0f / tracking->camera.pixel_aspect;
	float shiftx, shifty, winside = (float)min_ii(winx, winy);

	BKE_tracking_camera_shift_get(tracking, winx, winy, &shiftx, &shifty);

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

	camera = BKE_tracking_camera_get_reconstructed(tracking, object, framenr);

	if (camera) {
		float imat[4][4];

		invert_m4_m4(imat, camera->mat);
		mul_m4_m4m4(mat, winmat, imat);
	}
	else {
		copy_m4_m4(mat, winmat);
	}
}

/*********************** clipboard *************************/

/* Free clipboard by freeing memory used by all tracks in it. */
void BKE_tracking_clipboard_free(void)
{
	MovieTrackingTrack *track = tracking_clipboard.tracks.first, *next_track;

	while (track) {
		next_track = track->next;

		BKE_tracking_track_free(track);
		MEM_freeN(track);

		track = next_track;
	}

	BLI_listbase_clear(&tracking_clipboard.tracks);
}

/* Copy selected tracks from specified object to the clipboard. */
void BKE_tracking_clipboard_copy_tracks(MovieTracking *tracking, MovieTrackingObject *object)
{
	ListBase *tracksbase = BKE_tracking_object_get_tracks(tracking, object);
	MovieTrackingTrack *track = tracksbase->first;

	/* First drop all tracks from current clipboard. */
	BKE_tracking_clipboard_free();

	/* Then copy all selected visible tracks to it. */
	while (track) {
		if (TRACK_SELECTED(track) && (track->flag & TRACK_HIDDEN) == 0) {
			MovieTrackingTrack *new_track = BKE_tracking_track_duplicate(track);

			BLI_addtail(&tracking_clipboard.tracks, new_track);
		}

		track = track->next;
	}
}

/* Check whether there're any tracks in the clipboard. */
bool BKE_tracking_clipboard_has_tracks(void)
{
	return (BLI_listbase_is_empty(&tracking_clipboard.tracks) == false);
}

/* Paste tracks from clipboard to specified object.
 *
 * Names of new tracks in object are guaranteed to
 * be unique here.
 */
void BKE_tracking_clipboard_paste_tracks(MovieTracking *tracking, MovieTrackingObject *object)
{
	ListBase *tracksbase = BKE_tracking_object_get_tracks(tracking, object);
	MovieTrackingTrack *track = tracking_clipboard.tracks.first;

	while (track) {
		MovieTrackingTrack *new_track = BKE_tracking_track_duplicate(track);

		BLI_addtail(tracksbase, new_track);
		BKE_tracking_track_unique_name(tracksbase, new_track);

		track = track->next;
	}
}

/*********************** Tracks *************************/

/* Add new track to a specified tracks base.
 *
 * Coordinates are expected to be in normalized 0..1 space,
 * frame number is expected to be in clip space.
 *
 * Width and height are clip's dimension used to scale track's
 * pattern and search regions.
 */
MovieTrackingTrack *BKE_tracking_track_add(MovieTracking *tracking, ListBase *tracksbase, float x, float y,
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

	/* fill track's settings from default settings */
	track->motion_model = settings->default_motion_model;
	track->minimum_correlation = settings->default_minimum_correlation;
	track->margin = settings->default_margin;
	track->pattern_match = settings->default_pattern_match;
	track->frames_limit = settings->default_frames_limit;
	track->flag = settings->default_flag;
	track->algorithm_flag = settings->default_algorithm_flag;
	track->weight = settings->default_weight;

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

	BKE_tracking_marker_insert(track, &marker);

	BLI_addtail(tracksbase, track);
	BKE_tracking_track_unique_name(tracksbase, track);

	return track;
}

/* Duplicate the specified track, result will no belong to any list. */
MovieTrackingTrack *BKE_tracking_track_duplicate(MovieTrackingTrack *track)
{
	MovieTrackingTrack *new_track;

	new_track = MEM_callocN(sizeof(MovieTrackingTrack), "tracking_track_duplicate new_track");

	*new_track = *track;
	new_track->next = new_track->prev = NULL;

	new_track->markers = MEM_dupallocN(new_track->markers);

	return new_track;
}

/* Ensure specified track has got unique name,
 * if it's not name of specified track will be changed
 * keeping names of all other tracks unchanged.
 */
void BKE_tracking_track_unique_name(ListBase *tracksbase, MovieTrackingTrack *track)
{
	BLI_uniquename(tracksbase, track, CTX_DATA_(BLF_I18NCONTEXT_ID_MOVIECLIP, "Track"), '.',
	               offsetof(MovieTrackingTrack, name), sizeof(track->name));
}

/* Free specified track, only frees contents of a structure
 * (if track is allocated in heap, it shall be handled outside).
 *
 * All the pointers inside track becomes invalid after this call.
 */
void BKE_tracking_track_free(MovieTrackingTrack *track)
{
	if (track->markers)
		MEM_freeN(track->markers);
}

/* Set flag for all specified track's areas.
 *
 * area - which part of marker should be selected. see TRACK_AREA_* constants.
 * flag - flag to be set for areas.
 */
void BKE_tracking_track_flag_set(MovieTrackingTrack *track, int area, int flag)
{
	if (area == TRACK_AREA_NONE)
		return;

	if (area & TRACK_AREA_POINT)
		track->flag |= flag;
	if (area & TRACK_AREA_PAT)
		track->pat_flag |= flag;
	if (area & TRACK_AREA_SEARCH)
		track->search_flag |= flag;
}

/* Clear flag from all specified track's areas.
 *
 * area - which part of marker should be selected. see TRACK_AREA_* constants.
 * flag - flag to be cleared for areas.
 */
void BKE_tracking_track_flag_clear(MovieTrackingTrack *track, int area, int flag)
{
	if (area == TRACK_AREA_NONE)
		return;

	if (area & TRACK_AREA_POINT)
		track->flag &= ~flag;
	if (area & TRACK_AREA_PAT)
		track->pat_flag &= ~flag;
	if (area & TRACK_AREA_SEARCH)
		track->search_flag &= ~flag;
}

/* Check whether track has got marker at specified frame.
 *
 * NOTE: frame number should be in clip space, not scene space.
 */
bool BKE_tracking_track_has_marker_at_frame(MovieTrackingTrack *track, int framenr)
{
	return BKE_tracking_marker_get_exact(track, framenr) != NULL;
}

/* Check whether track has got enabled marker at specified frame.
 *
 * NOTE: frame number should be in clip space, not scene space.
 */
bool BKE_tracking_track_has_enabled_marker_at_frame(MovieTrackingTrack *track, int framenr)
{
	MovieTrackingMarker *marker = BKE_tracking_marker_get_exact(track, framenr);

	return marker && (marker->flag & MARKER_DISABLED) == 0;
}

/* Clear track's path:
 *
 * - If action is TRACK_CLEAR_REMAINED path from ref_frame+1 up to
 *   end will be clear.
 *
 * - If action is TRACK_CLEAR_UPTO path from the beginning up to
 *   ref_frame-1 will be clear.
 *
 * - If action is TRACK_CLEAR_ALL only mareker at frame ref_frame will remain.
 *
 * NOTE: frame number should be in clip space, not scene space
 */
void BKE_tracking_track_path_clear(MovieTrackingTrack *track, int ref_frame, int action)
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
			tracking_marker_insert_disabled(track, &track->markers[track->markersnr - 1], false, true);
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
			tracking_marker_insert_disabled(track, &track->markers[0], true, true);
	}
	else if (action == TRACK_CLEAR_ALL) {
		MovieTrackingMarker *marker, marker_new;

		marker = BKE_tracking_marker_get(track, ref_frame);
		marker_new = *marker;

		MEM_freeN(track->markers);
		track->markers = NULL;
		track->markersnr = 0;

		BKE_tracking_marker_insert(track, &marker_new);

		tracking_marker_insert_disabled(track, &marker_new, true, true);
		tracking_marker_insert_disabled(track, &marker_new, false, true);
	}
}

void BKE_tracking_tracks_join(MovieTracking *tracking, MovieTrackingTrack *dst_track, MovieTrackingTrack *src_track)
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
				else {
					markers[i] = src_track->markers[a];
				}
			}
			else {
				markers[i] = dst_track->markers[b];
			}

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

	BKE_tracking_dopesheet_tag_update(tracking);
}

MovieTrackingTrack *BKE_tracking_track_get_named(MovieTracking *tracking, MovieTrackingObject *object, const char *name)
{
	ListBase *tracksbase = BKE_tracking_object_get_tracks(tracking, object);
	MovieTrackingTrack *track = tracksbase->first;

	while (track) {
		if (!strcmp(track->name, name))
			return track;

		track = track->next;
	}

	return NULL;
}

MovieTrackingTrack *BKE_tracking_track_get_indexed(MovieTracking *tracking, int tracknr, ListBase **tracksbase_r)
{
	MovieTrackingObject *object;
	int cur = 1;

	object = tracking->objects.first;
	while (object) {
		ListBase *tracksbase = BKE_tracking_object_get_tracks(tracking, object);
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

MovieTrackingTrack *BKE_tracking_track_get_active(MovieTracking *tracking)
{
	ListBase *tracksbase;

	if (!tracking->act_track)
		return NULL;

	tracksbase = BKE_tracking_get_active_tracks(tracking);

	/* check that active track is in current tracks list */
	if (BLI_findindex(tracksbase, tracking->act_track) != -1)
		return tracking->act_track;

	return NULL;
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
			bool ok = false;

			while (frame) {
				if (frame->strokes.first) {
					ok = true;
					break;
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

				/* TODO: add an option to control whether AA is enabled or not */
				PLX_raskterize((float (*)[2])mask_points, stroke->totpoints, mask, mask_width, mask_height);

				MEM_freeN(mask_points);
			}

			stroke = stroke->next;
		}

		frame = frame->next;
	}
}

float *BKE_tracking_track_get_mask(int frame_width, int frame_height,
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

float BKE_tracking_track_get_weight_for_marker(MovieClip *clip, MovieTrackingTrack *track, MovieTrackingMarker *marker)
{
	FCurve *weight_fcurve;
	float weight = track->weight;

	weight_fcurve = id_data_find_fcurve(&clip->id, track, &RNA_MovieTrackingTrack,
	                                    "weight", 0, NULL);

	if (weight_fcurve) {
		int scene_framenr =
			BKE_movieclip_remap_clip_to_scene_frame(clip, marker->framenr);
		weight = evaluate_fcurve(weight_fcurve, scene_framenr);
	}

	return weight;
}

/* area - which part of marker should be selected. see TRACK_AREA_* constants */
void BKE_tracking_track_select(ListBase *tracksbase, MovieTrackingTrack *track, int area, bool extend)
{
	if (extend) {
		BKE_tracking_track_flag_set(track, area, SELECT);
	}
	else {
		MovieTrackingTrack *cur = tracksbase->first;

		while (cur) {
			if ((cur->flag & TRACK_HIDDEN) == 0) {
				if (cur == track) {
					BKE_tracking_track_flag_clear(cur, TRACK_AREA_ALL, SELECT);
					BKE_tracking_track_flag_set(cur, area, SELECT);
				}
				else {
					BKE_tracking_track_flag_clear(cur, TRACK_AREA_ALL, SELECT);
				}
			}

			cur = cur->next;
		}
	}
}

void BKE_tracking_track_deselect(MovieTrackingTrack *track, int area)
{
	BKE_tracking_track_flag_clear(track, area, SELECT);
}

void BKE_tracking_tracks_deselect_all(ListBase *tracksbase)
{
	MovieTrackingTrack *track;

	for (track = tracksbase->first; track; track = track->next) {
		if ((track->flag & TRACK_HIDDEN) == 0) {
			BKE_tracking_track_flag_clear(track, TRACK_AREA_ALL, SELECT);
		}
	}
}

/*********************** Marker *************************/

MovieTrackingMarker *BKE_tracking_marker_insert(MovieTrackingTrack *track, MovieTrackingMarker *marker)
{
	MovieTrackingMarker *old_marker = NULL;

	if (track->markersnr)
		old_marker = BKE_tracking_marker_get_exact(track, marker->framenr);

	if (old_marker) {
		/* simply replace settings for already allocated marker */
		*old_marker = *marker;

		return old_marker;
	}
	else {
		int a = track->markersnr;

		/* find position in array where to add new marker */
		while (a--) {
			if (track->markers[a].framenr < marker->framenr)
				break;
		}

		track->markersnr++;

		if (track->markers)
			track->markers = MEM_reallocN(track->markers, sizeof(MovieTrackingMarker) * track->markersnr);
		else
			track->markers = MEM_callocN(sizeof(MovieTrackingMarker), "MovieTracking markers");

		/* shift array to "free" space for new marker */
		memmove(track->markers + a + 2, track->markers + a + 1,
		        (track->markersnr - a - 2) * sizeof(MovieTrackingMarker));

		/* put new marker */
		track->markers[a + 1] = *marker;

		track->last_marker = a + 1;

		return &track->markers[a + 1];
	}
}

void BKE_tracking_marker_delete(MovieTrackingTrack *track, int framenr)
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

void BKE_tracking_marker_clamp(MovieTrackingMarker *marker, int event)
{
	int a;
	float pat_min[2], pat_max[2];

	BKE_tracking_marker_pattern_minmax(marker, pat_min, pat_max);

	if (event == CLAMP_PAT_DIM) {
		for (a = 0; a < 2; a++) {
			/* search shouldn't be resized smaller than pattern */
			marker->search_min[a] = min_ff(pat_min[a], marker->search_min[a]);
			marker->search_max[a] = max_ff(pat_max[a], marker->search_max[a]);
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
			marker->search_min[a] = min_ff(pat_min[a], marker->search_min[a]);
			marker->search_max[a] = max_ff(pat_max[a], marker->search_max[a]);
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
}

MovieTrackingMarker *BKE_tracking_marker_get(MovieTrackingTrack *track, int framenr)
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

MovieTrackingMarker *BKE_tracking_marker_get_exact(MovieTrackingTrack *track, int framenr)
{
	MovieTrackingMarker *marker = BKE_tracking_marker_get(track, framenr);

	if (marker->framenr != framenr)
		return NULL;

	return marker;
}

MovieTrackingMarker *BKE_tracking_marker_ensure(MovieTrackingTrack *track, int framenr)
{
	MovieTrackingMarker *marker = BKE_tracking_marker_get(track, framenr);

	if (marker->framenr != framenr) {
		MovieTrackingMarker marker_new;

		marker_new = *marker;
		marker_new.framenr = framenr;

		BKE_tracking_marker_insert(track, &marker_new);
		marker = BKE_tracking_marker_get(track, framenr);
	}

	return marker;
}

void BKE_tracking_marker_pattern_minmax(const MovieTrackingMarker *marker, float min[2], float max[2])
{
	INIT_MINMAX2(min, max);

	minmax_v2v2_v2(min, max, marker->pattern_corners[0]);
	minmax_v2v2_v2(min, max, marker->pattern_corners[1]);
	minmax_v2v2_v2(min, max, marker->pattern_corners[2]);
	minmax_v2v2_v2(min, max, marker->pattern_corners[3]);
}

void BKE_tracking_marker_get_subframe_position(MovieTrackingTrack *track, float framenr, float pos[2])
{
	MovieTrackingMarker *marker = BKE_tracking_marker_get(track, (int) framenr);
	MovieTrackingMarker *marker_last = track->markers + (track->markersnr - 1);

	if (marker != marker_last) {
		MovieTrackingMarker *marker_next = marker + 1;

		if (marker_next->framenr == marker->framenr + 1) {
			/* currently only do subframing inside tracked ranges, do not extrapolate tracked segments
			 * could be changed when / if mask parent would be interpolating position in-between
			 * tracked segments
			 */

			float fac = (framenr - (int) framenr) / (marker_next->framenr - marker->framenr);

			interp_v2_v2v2(pos, marker->pos, marker_next->pos, fac);
		}
		else {
			copy_v2_v2(pos, marker->pos);
		}
	}
	else {
		copy_v2_v2(pos, marker->pos);
	}

	/* currently track offset is always wanted to be applied here, could be made an option later */
	add_v2_v2(pos, track->offset);
}

/*********************** Plane Track *************************/

/* Creates new plane track out of selected point tracks */
MovieTrackingPlaneTrack *BKE_tracking_plane_track_add(MovieTracking *tracking, ListBase *plane_tracks_base,
                                                      ListBase *tracks, int framenr)
{
	MovieTrackingPlaneTrack *plane_track;
	MovieTrackingPlaneMarker plane_marker;
	MovieTrackingTrack *track;
	float tracks_min[2], tracks_max[2];
	int track_index, num_selected_tracks = 0;

	(void) tracking;  /* Ignored. */

	/* Use bounding box of selected markers as an initial size of plane. */
	INIT_MINMAX2(tracks_min, tracks_max);
	for (track = tracks->first; track; track = track->next) {
		if (TRACK_SELECTED(track)) {
			MovieTrackingMarker *marker = BKE_tracking_marker_get(track, framenr);
			float pattern_min[2], pattern_max[2];
			BKE_tracking_marker_pattern_minmax(marker, pattern_min, pattern_max);
			add_v2_v2(pattern_min, marker->pos);
			add_v2_v2(pattern_max, marker->pos);
			minmax_v2v2_v2(tracks_min, tracks_max, pattern_min);
			minmax_v2v2_v2(tracks_min, tracks_max, pattern_max);
			num_selected_tracks++;
		}
	}

	if (num_selected_tracks < 4) {
		return NULL;
	}

	/* Allocate new plane track. */
	plane_track = MEM_callocN(sizeof(MovieTrackingPlaneTrack), "new plane track");

	/* Use some default name. */
	strcpy(plane_track->name, "Plane Track");

	plane_track->image_opacity = 1.0f;

	/* Use selected tracks from given list as a plane. */
	plane_track->point_tracks =
		MEM_mallocN(sizeof(MovieTrackingTrack *) * num_selected_tracks, "new plane tracks array");
	for (track = tracks->first, track_index = 0; track; track = track->next) {
		if (TRACK_SELECTED(track)) {
			plane_track->point_tracks[track_index] = track;
			track_index++;
		}
	}
	plane_track->point_tracksnr = num_selected_tracks;

	/* Setup new plane marker and add it to the track. */
	plane_marker.framenr = framenr;
	plane_marker.flag = 0;

	copy_v2_v2(plane_marker.corners[0], tracks_min);
	copy_v2_v2(plane_marker.corners[2], tracks_max);

	plane_marker.corners[1][0] = tracks_max[0];
	plane_marker.corners[1][1] = tracks_min[1];
	plane_marker.corners[3][0] = tracks_min[0];
	plane_marker.corners[3][1] = tracks_max[1];

	BKE_tracking_plane_marker_insert(plane_track, &plane_marker);

	/* Put new plane track to the list, ensure it's name is unique. */
	BLI_addtail(plane_tracks_base, plane_track);
	BKE_tracking_plane_track_unique_name(plane_tracks_base, plane_track);

	return plane_track;
}

void BKE_tracking_plane_track_unique_name(ListBase *plane_tracks_base, MovieTrackingPlaneTrack *plane_track)
{
	BLI_uniquename(plane_tracks_base, plane_track, CTX_DATA_(BLF_I18NCONTEXT_ID_MOVIECLIP, "Plane Track"), '.',
	               offsetof(MovieTrackingPlaneTrack, name), sizeof(plane_track->name));
}

/* Free specified plane track, only frees contents of a structure
 * (if track is allocated in heap, it shall be handled outside).
 *
 * All the pointers inside track becomes invalid after this call.
 */
void BKE_tracking_plane_track_free(MovieTrackingPlaneTrack *plane_track)
{
	if (plane_track->markers) {
		MEM_freeN(plane_track->markers);
	}

	MEM_freeN(plane_track->point_tracks);
}

MovieTrackingPlaneTrack *BKE_tracking_plane_track_get_named(MovieTracking *tracking,
                                                            MovieTrackingObject *object,
                                                            const char *name)
{
	ListBase *plane_tracks_base = BKE_tracking_object_get_plane_tracks(tracking, object);
	MovieTrackingPlaneTrack *plane_track;

	for (plane_track = plane_tracks_base->first;
	     plane_track;
	     plane_track = plane_track->next)
	{
		if (!strcmp(plane_track->name, name)) {
			return plane_track;
		}
	}

	return NULL;
}

MovieTrackingPlaneTrack *BKE_tracking_plane_track_get_active(struct MovieTracking *tracking)
{
	ListBase *plane_tracks_base;

	if (tracking->act_plane_track == NULL) {
		return NULL;
	}

	plane_tracks_base = BKE_tracking_get_active_plane_tracks(tracking);

	/* Check that active track is in current plane tracks list */
	if (BLI_findindex(plane_tracks_base, tracking->act_plane_track) != -1) {
		return tracking->act_plane_track;
	}

	return NULL;
}

void BKE_tracking_plane_tracks_deselect_all(ListBase *plane_tracks_base)
{
	MovieTrackingPlaneTrack *plane_track;

	for (plane_track = plane_tracks_base->first; plane_track; plane_track = plane_track->next) {
		plane_track->flag &= ~SELECT;
	}
}

/*********************** Plane Marker *************************/

MovieTrackingPlaneMarker *BKE_tracking_plane_marker_insert(MovieTrackingPlaneTrack *plane_track,
                                                           MovieTrackingPlaneMarker *plane_marker)
{
	MovieTrackingPlaneMarker *old_plane_marker = NULL;

	if (plane_track->markersnr)
		old_plane_marker = BKE_tracking_plane_marker_get_exact(plane_track, plane_marker->framenr);

	if (old_plane_marker) {
		/* Simply replace settings in existing marker. */
		*old_plane_marker = *plane_marker;

		return old_plane_marker;
	}
	else {
		int a = plane_track->markersnr;

		/* Find position in array where to add new marker. */
		/* TODO(sergey): we coud use bisect to speed things up. */
		while (a--) {
			if (plane_track->markers[a].framenr < plane_marker->framenr) {
				break;
			}
		}

		plane_track->markersnr++;
		plane_track->markers = MEM_reallocN(plane_track->markers,
		                                    sizeof(MovieTrackingPlaneMarker) * plane_track->markersnr);

		/* Shift array to "free" space for new marker. */
		memmove(plane_track->markers + a + 2, plane_track->markers + a + 1,
		        (plane_track->markersnr - a - 2) * sizeof(MovieTrackingPlaneMarker));

		/* Put new marker to an array. */
		plane_track->markers[a + 1] = *plane_marker;
		plane_track->last_marker = a + 1;

		return &plane_track->markers[a + 1];
	}
}

void BKE_tracking_plane_marker_delete(MovieTrackingPlaneTrack *plane_track, int framenr)
{
	int a = 0;

	while (a < plane_track->markersnr) {
		if (plane_track->markers[a].framenr == framenr) {
			if (plane_track->markersnr > 1) {
				memmove(plane_track->markers + a, plane_track->markers + a + 1,
				        (plane_track->markersnr - a - 1) * sizeof(MovieTrackingPlaneMarker));
				plane_track->markersnr--;
				plane_track->markers = MEM_reallocN(plane_track->markers,
				                                    sizeof(MovieTrackingMarker) * plane_track->markersnr);
			}
			else {
				MEM_freeN(plane_track->markers);
				plane_track->markers = NULL;
				plane_track->markersnr = 0;
			}

			break;
		}

		a++;
	}
}

/* TODO(sergey): The next couple of functions are really quite the same as point marker version,
 *               would be nice to de-duplicate them somehow..
 */

/* Get a plane marker at given frame,
 * If there's no such marker, closest one from the left side will be returned.
 */
MovieTrackingPlaneMarker *BKE_tracking_plane_marker_get(MovieTrackingPlaneTrack *plane_track, int framenr)
{
	int a = plane_track->markersnr - 1;

	if (!plane_track->markersnr)
		return NULL;

	/* Approximate pre-first framenr marker with first marker. */
	if (framenr < plane_track->markers[0].framenr) {
		return &plane_track->markers[0];
	}

	if (plane_track->last_marker < plane_track->markersnr) {
		a = plane_track->last_marker;
	}

	if (plane_track->markers[a].framenr <= framenr) {
		while (a < plane_track->markersnr && plane_track->markers[a].framenr <= framenr) {
			if (plane_track->markers[a].framenr == framenr) {
				plane_track->last_marker = a;

				return &plane_track->markers[a];
			}
			a++;
		}

		/* If there's no marker for exact position, use nearest marker from left side. */
		return &plane_track->markers[a - 1];
	}
	else {
		while (a >= 0 && plane_track->markers[a].framenr >= framenr) {
			if (plane_track->markers[a].framenr == framenr) {
				plane_track->last_marker = a;

				return &plane_track->markers[a];
			}

			a--;
		}

		/* If there's no marker for exact position, use nearest marker from left side. */
		return &plane_track->markers[a];
	}

	return NULL;
}

/* Get a plane marker at exact given frame, if there's no marker at the frame,
 * NULL will be returned.
 */
MovieTrackingPlaneMarker *BKE_tracking_plane_marker_get_exact(MovieTrackingPlaneTrack *plane_track, int framenr)
{
	MovieTrackingPlaneMarker *plane_marker = BKE_tracking_plane_marker_get(plane_track, framenr);

	if (plane_marker->framenr != framenr) {
		return NULL;
	}

	return plane_marker;
}

/* Ensure there's a marker for the given frame. */
MovieTrackingPlaneMarker *BKE_tracking_plane_marker_ensure(MovieTrackingPlaneTrack *plane_track, int framenr)
{
	MovieTrackingPlaneMarker *plane_marker = BKE_tracking_plane_marker_get(plane_track, framenr);

	if (plane_marker->framenr != framenr) {
		MovieTrackingPlaneMarker plane_marker_new;

		plane_marker_new = *plane_marker;
		plane_marker_new.framenr = framenr;

		plane_marker = BKE_tracking_plane_marker_insert(plane_track, &plane_marker_new);
	}

	return plane_marker;
}

/*********************** Object *************************/

MovieTrackingObject *BKE_tracking_object_add(MovieTracking *tracking, const char *name)
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
	object->keyframe1 = 1;
	object->keyframe2 = 30;

	BKE_tracking_object_unique_name(tracking, object);
	BKE_tracking_dopesheet_tag_update(tracking);

	return object;
}

bool BKE_tracking_object_delete(MovieTracking *tracking, MovieTrackingObject *object)
{
	MovieTrackingTrack *track;
	int index = BLI_findindex(&tracking->objects, object);

	if (index == -1)
		return false;

	if (object->flag & TRACKING_OBJECT_CAMERA) {
		/* object used for camera solving can't be deleted */
		return false;
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

	if (index != 0)
		tracking->objectnr = index - 1;
	else
		tracking->objectnr = 0;

	BKE_tracking_dopesheet_tag_update(tracking);

	return true;
}

void BKE_tracking_object_unique_name(MovieTracking *tracking, MovieTrackingObject *object)
{
	BLI_uniquename(&tracking->objects, object, DATA_("Object"), '.',
	               offsetof(MovieTrackingObject, name), sizeof(object->name));
}

MovieTrackingObject *BKE_tracking_object_get_named(MovieTracking *tracking, const char *name)
{
	MovieTrackingObject *object = tracking->objects.first;

	while (object) {
		if (!strcmp(object->name, name))
			return object;

		object = object->next;
	}

	return NULL;
}

MovieTrackingObject *BKE_tracking_object_get_active(MovieTracking *tracking)
{
	return BLI_findlink(&tracking->objects, tracking->objectnr);
}

MovieTrackingObject *BKE_tracking_object_get_camera(MovieTracking *tracking)
{
	MovieTrackingObject *object = tracking->objects.first;

	while (object) {
		if (object->flag & TRACKING_OBJECT_CAMERA)
			return object;

		object = object->next;
	}

	return NULL;
}

ListBase *BKE_tracking_object_get_tracks(MovieTracking *tracking, MovieTrackingObject *object)
{
	if (object->flag & TRACKING_OBJECT_CAMERA) {
		return &tracking->tracks;
	}

	return &object->tracks;
}

ListBase *BKE_tracking_object_get_plane_tracks(MovieTracking *tracking, MovieTrackingObject *object)
{
	if (object->flag & TRACKING_OBJECT_CAMERA) {
		return &tracking->plane_tracks;
	}

	return &object->plane_tracks;
}

MovieTrackingReconstruction *BKE_tracking_object_get_reconstruction(MovieTracking *tracking,
                                                                    MovieTrackingObject *object)
{
	if (object->flag & TRACKING_OBJECT_CAMERA) {
		return &tracking->reconstruction;
	}

	return &object->reconstruction;
}

/*********************** Camera *************************/

static int reconstructed_camera_index_get(MovieTrackingReconstruction *reconstruction, int framenr, bool nearest)
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

static void reconstructed_camera_scale_set(MovieTrackingObject *object, float mat[4][4])
{
	if ((object->flag & TRACKING_OBJECT_CAMERA) == 0) {
		float smat[4][4];

		scale_m4_fl(smat, 1.0f / object->scale);
		mul_m4_m4m4(mat, mat, smat);
	}
}

/* converts principal offset from center to offset of blender's camera */
void BKE_tracking_camera_shift_get(MovieTracking *tracking, int winx, int winy, float *shiftx, float *shifty)
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

	BKE_tracking_camera_shift_get(tracking, width, height, &camera->shiftx, &camera->shifty);
}

MovieReconstructedCamera *BKE_tracking_camera_get_reconstructed(MovieTracking *tracking,
                                                                MovieTrackingObject *object, int framenr)
{
	MovieTrackingReconstruction *reconstruction;
	int a;

	reconstruction = BKE_tracking_object_get_reconstruction(tracking, object);
	a = reconstructed_camera_index_get(reconstruction, framenr, false);

	if (a == -1)
		return NULL;

	return &reconstruction->cameras[a];
}

void BKE_tracking_camera_get_reconstructed_interpolate(MovieTracking *tracking, MovieTrackingObject *object,
                                                       int framenr, float mat[4][4])
{
	MovieTrackingReconstruction *reconstruction;
	MovieReconstructedCamera *cameras;
	int a;

	reconstruction = BKE_tracking_object_get_reconstruction(tracking, object);
	cameras = reconstruction->cameras;
	a = reconstructed_camera_index_get(reconstruction, framenr, true);

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

	reconstructed_camera_scale_set(object, mat);
}

/*********************** Distortion/Undistortion *************************/

MovieDistortion *BKE_tracking_distortion_new(MovieTracking *tracking,
                                             int calibration_width, int calibration_height)
{
	MovieDistortion *distortion;
	libmv_CameraIntrinsicsOptions camera_intrinsics_options;

	tracking_cameraIntrinscisOptionsFromTracking(tracking,
	                                             calibration_width,
	                                             calibration_height,
	                                             &camera_intrinsics_options);

	distortion = MEM_callocN(sizeof(MovieDistortion), "BKE_tracking_distortion_create");
	distortion->intrinsics = libmv_cameraIntrinsicsNew(&camera_intrinsics_options);

	return distortion;
}

void BKE_tracking_distortion_update(MovieDistortion *distortion, MovieTracking *tracking,
                                    int calibration_width, int calibration_height)
{
	libmv_CameraIntrinsicsOptions camera_intrinsics_options;

	tracking_cameraIntrinscisOptionsFromTracking(tracking,
	                                             calibration_width,
	                                             calibration_height,
	                                             &camera_intrinsics_options);

	libmv_cameraIntrinsicsUpdate(&camera_intrinsics_options, distortion->intrinsics);
}

void BKE_tracking_distortion_set_threads(MovieDistortion *distortion, int threads)
{
	libmv_cameraIntrinsicsSetThreads(distortion->intrinsics, threads);
}

MovieDistortion *BKE_tracking_distortion_copy(MovieDistortion *distortion)
{
	MovieDistortion *new_distortion;

	new_distortion = MEM_callocN(sizeof(MovieDistortion), "BKE_tracking_distortion_create");

	new_distortion->intrinsics = libmv_cameraIntrinsicsCopy(distortion->intrinsics);

	return new_distortion;
}

ImBuf *BKE_tracking_distortion_exec(MovieDistortion *distortion, MovieTracking *tracking, ImBuf *ibuf,
                                    int calibration_width, int calibration_height, float overscan, bool undistort)
{
	ImBuf *resibuf;

	BKE_tracking_distortion_update(distortion, tracking, calibration_width, calibration_height);

	resibuf = IMB_dupImBuf(ibuf);

	if (ibuf->rect_float) {
		if (undistort) {
			libmv_cameraIntrinsicsUndistortFloat(distortion->intrinsics,
			                                     ibuf->rect_float, resibuf->rect_float,
			                                     ibuf->x, ibuf->y, overscan, ibuf->channels);
		}
		else {
			libmv_cameraIntrinsicsDistortFloat(distortion->intrinsics,
			                                   ibuf->rect_float, resibuf->rect_float,
			                                   ibuf->x, ibuf->y, overscan, ibuf->channels);
		}

		if (ibuf->rect)
			imb_freerectImBuf(ibuf);
	}
	else {
		if (undistort) {
			libmv_cameraIntrinsicsUndistortByte(distortion->intrinsics,
			                                    (unsigned char *)ibuf->rect, (unsigned char *)resibuf->rect,
			                                    ibuf->x, ibuf->y, overscan, ibuf->channels);
		}
		else {
			libmv_cameraIntrinsicsDistortByte(distortion->intrinsics,
			                                  (unsigned char *)ibuf->rect, (unsigned char *)resibuf->rect,
			                                  ibuf->x, ibuf->y, overscan, ibuf->channels);
		}
	}

	return resibuf;
}

void BKE_tracking_distortion_free(MovieDistortion *distortion)
{
	libmv_cameraIntrinsicsDestroy(distortion->intrinsics);

	MEM_freeN(distortion);
}

void BKE_tracking_distort_v2(MovieTracking *tracking, const float co[2], float r_co[2])
{
	MovieTrackingCamera *camera = &tracking->camera;

	libmv_CameraIntrinsicsOptions camera_intrinsics_options;
	double x, y;
	float aspy = 1.0f / tracking->camera.pixel_aspect;

	tracking_cameraIntrinscisOptionsFromTracking(tracking,
	                                             0, 0,
	                                             &camera_intrinsics_options);

	/* normalize coords */
	x = (co[0] - camera->principal[0]) / camera->focal;
	y = (co[1] - camera->principal[1] * aspy) / camera->focal;

	libmv_cameraIntrinsicsApply(&camera_intrinsics_options, x, y, &x, &y);

	/* result is in image coords already */
	r_co[0] = x;
	r_co[1] = y;
}

void BKE_tracking_undistort_v2(MovieTracking *tracking, const float co[2], float r_co[2])
{
	MovieTrackingCamera *camera = &tracking->camera;

	libmv_CameraIntrinsicsOptions camera_intrinsics_options;
	double x = co[0], y = co[1];
	float aspy = 1.0f / tracking->camera.pixel_aspect;

	tracking_cameraIntrinscisOptionsFromTracking(tracking,
	                                             0, 0,
	                                             &camera_intrinsics_options);

	libmv_cameraIntrinsicsInvert(&camera_intrinsics_options, x, y, &x, &y);

	r_co[0] = (float)x * camera->focal + camera->principal[0];
	r_co[1] = (float)y * camera->focal + camera->principal[1] * aspy;
}

ImBuf *BKE_tracking_undistort_frame(MovieTracking *tracking, ImBuf *ibuf, int calibration_width,
                                    int calibration_height, float overscan)
{
	MovieTrackingCamera *camera = &tracking->camera;

	if (camera->intrinsics == NULL) {
		camera->intrinsics = BKE_tracking_distortion_new(tracking, calibration_width, calibration_height);
	}

	return BKE_tracking_distortion_exec(camera->intrinsics, tracking, ibuf, calibration_width,
	                                    calibration_height, overscan, true);
}

ImBuf *BKE_tracking_distort_frame(MovieTracking *tracking, ImBuf *ibuf, int calibration_width,
                                  int calibration_height, float overscan)
{
	MovieTrackingCamera *camera = &tracking->camera;

	if (camera->intrinsics == NULL) {
		camera->intrinsics = BKE_tracking_distortion_new(tracking, calibration_width, calibration_height);
	}

	return BKE_tracking_distortion_exec(camera->intrinsics, tracking, ibuf, calibration_width,
	                                    calibration_height, overscan, false);
}

void BKE_tracking_max_undistortion_delta_across_bound(MovieTracking *tracking, rcti *rect, float delta[2])
{
	int a;
	float pos[2], warped_pos[2];
	const int coord_delta = 5;

	delta[0] = delta[1] = -FLT_MAX;

	for (a = rect->xmin; a <= rect->xmax + coord_delta; a += coord_delta) {
		if (a > rect->xmax)
			a = rect->xmax;

		/* bottom edge */
		pos[0] = a;
		pos[1] = rect->ymin;

		BKE_tracking_undistort_v2(tracking, pos, warped_pos);

		delta[0] = max_ff(delta[0], fabsf(pos[0] - warped_pos[0]));
		delta[1] = max_ff(delta[1], fabsf(pos[1] - warped_pos[1]));

		/* top edge */
		pos[0] = a;
		pos[1] = rect->ymax;

		BKE_tracking_undistort_v2(tracking, pos, warped_pos);

		delta[0] = max_ff(delta[0], fabsf(pos[0] - warped_pos[0]));
		delta[1] = max_ff(delta[1], fabsf(pos[1] - warped_pos[1]));

		if (a >= rect->xmax)
			break;
	}

	for (a = rect->ymin; a <= rect->ymax + coord_delta; a += coord_delta) {
		if (a > rect->ymax)
			a = rect->ymax;

		/* left edge */
		pos[0] = rect->xmin;
		pos[1] = a;

		BKE_tracking_undistort_v2(tracking, pos, warped_pos);

		delta[0] = max_ff(delta[0], fabsf(pos[0] - warped_pos[0]));
		delta[1] = max_ff(delta[1], fabsf(pos[1] - warped_pos[1]));

		/* right edge */
		pos[0] = rect->xmax;
		pos[1] = a;

		BKE_tracking_undistort_v2(tracking, pos, warped_pos);

		delta[0] = max_ff(delta[0], fabsf(pos[0] - warped_pos[0]));
		delta[1] = max_ff(delta[1], fabsf(pos[1] - warped_pos[1]));

		if (a >= rect->ymax)
			break;
	}
}

/*********************** Image sampling *************************/

static void disable_imbuf_channels(ImBuf *ibuf, MovieTrackingTrack *track, bool grayscale)
{
	BKE_tracking_disable_channels(ibuf, track->flag & TRACK_DISABLE_RED,
	                              track->flag & TRACK_DISABLE_GREEN,
	                              track->flag & TRACK_DISABLE_BLUE, grayscale);
}

ImBuf *BKE_tracking_sample_pattern(int frame_width, int frame_height, ImBuf *search_ibuf,
                                   MovieTrackingTrack *track, MovieTrackingMarker *marker,
                                   bool from_anchor, bool use_mask, int num_samples_x, int num_samples_y,
                                   float pos[2])
{
	ImBuf *pattern_ibuf;
	double src_pixel_x[5], src_pixel_y[5];
	double warped_position_x, warped_position_y;
	float *mask = NULL;

	if (num_samples_x <= 0 || num_samples_y <= 0)
		return NULL;

	pattern_ibuf = IMB_allocImBuf(num_samples_x, num_samples_y,
	                              32,
	                              search_ibuf->rect_float ? IB_rectfloat : IB_rect);

	tracking_get_marker_coords_for_tracking(frame_width, frame_height, marker, src_pixel_x, src_pixel_y);

	/* from_anchor means search buffer was obtained for an anchored position,
	 * which means applying track offset rounded to pixel space (we could not
	 * store search buffer with sub-pixel precision)
	 *
	 * in this case we need to alter coordinates a bit, to compensate rounded
	 * fractional part of offset
	 */
	if (from_anchor) {
		int a;

		for (a = 0; a < 5; a++) {
			src_pixel_x[a] += (double) ((track->offset[0] * frame_width) - ((int) (track->offset[0] * frame_width)));
			src_pixel_y[a] += (double) ((track->offset[1] * frame_height) - ((int) (track->offset[1] * frame_height)));

			/* when offset is negative, rounding happens in opposite direction */
			if (track->offset[0] < 0.0f)
				src_pixel_x[a] += 1.0;
			if (track->offset[1] < 0.0f)
				src_pixel_y[a] += 1.0;
		}
	}

	if (use_mask) {
		mask = BKE_tracking_track_get_mask(frame_width, frame_height, track, marker);
	}

	if (search_ibuf->rect_float) {
		libmv_samplePlanarPatch(search_ibuf->rect_float,
		                        search_ibuf->x, search_ibuf->y, 4,
		                        src_pixel_x, src_pixel_y,
		                        num_samples_x, num_samples_y,
		                        mask,
		                        pattern_ibuf->rect_float,
		                        &warped_position_x,
		                        &warped_position_y);
	}
	else {
		libmv_samplePlanarPatchByte((unsigned char *) search_ibuf->rect,
		                            search_ibuf->x, search_ibuf->y, 4,
		                            src_pixel_x, src_pixel_y,
		                            num_samples_x, num_samples_y,
		                            mask,
		                            (unsigned char *) pattern_ibuf->rect,
		                            &warped_position_x,
		                            &warped_position_y);
	}

	if (pos) {
		pos[0] = warped_position_x;
		pos[1] = warped_position_y;
	}

	if (mask) {
		MEM_freeN(mask);
	}

	return pattern_ibuf;
}

ImBuf *BKE_tracking_get_pattern_imbuf(ImBuf *ibuf, MovieTrackingTrack *track, MovieTrackingMarker *marker,
                                      bool anchored, bool disable_channels)
{
	ImBuf *pattern_ibuf, *search_ibuf;
	float pat_min[2], pat_max[2];
	int num_samples_x, num_samples_y;

	BKE_tracking_marker_pattern_minmax(marker, pat_min, pat_max);

	num_samples_x = (pat_max[0] - pat_min[0]) * ibuf->x;
	num_samples_y = (pat_max[1] - pat_min[1]) * ibuf->y;

	search_ibuf = BKE_tracking_get_search_imbuf(ibuf, track, marker, anchored, disable_channels);

	if (search_ibuf) {
		pattern_ibuf = BKE_tracking_sample_pattern(ibuf->x, ibuf->y, search_ibuf, track, marker,
		                                           anchored, false, num_samples_x, num_samples_y, NULL);

		IMB_freeImBuf(search_ibuf);
	}
	else {
		pattern_ibuf = NULL;
	}

	return pattern_ibuf;
}

ImBuf *BKE_tracking_get_search_imbuf(ImBuf *ibuf, MovieTrackingTrack *track, MovieTrackingMarker *marker,
                                     bool anchored, bool disable_channels)
{
	ImBuf *searchibuf;
	int x, y, w, h;
	float search_origin[2];

	tracking_get_search_origin_frame_pixel(ibuf->x, ibuf->y, marker, search_origin);

	x = search_origin[0];
	y = search_origin[1];

	if (anchored) {
		x += track->offset[0] * ibuf->x;
		y += track->offset[1] * ibuf->y;
	}

	w = (marker->search_max[0] - marker->search_min[0]) * ibuf->x;
	h = (marker->search_max[1] - marker->search_min[1]) * ibuf->y;

	if (w <= 0 || h <= 0)
		return NULL;

	searchibuf = IMB_allocImBuf(w, h, 32, ibuf->rect_float ? IB_rectfloat : IB_rect);

	IMB_rectcpy(searchibuf, ibuf, 0, 0, x, y, w, h);

	if (disable_channels) {
		if ((track->flag & TRACK_PREVIEW_GRAYSCALE) ||
		    (track->flag & TRACK_DISABLE_RED)       ||
		    (track->flag & TRACK_DISABLE_GREEN)     ||
		    (track->flag & TRACK_DISABLE_BLUE))
		{
			disable_imbuf_channels(searchibuf, track, true);
		}
	}

	return searchibuf;
}

/* zap channels from the imbuf that are disabled by the user. this can lead to
 * better tracks sometimes. however, instead of simply zeroing the channels
 * out, do a partial grayscale conversion so the display is better.
 */
void BKE_tracking_disable_channels(ImBuf *ibuf, bool disable_red, bool disable_green, bool disable_blue,
                                   bool grayscale)
{
	int x, y;
	float scale;

	if (!disable_red && !disable_green && !disable_blue && !grayscale)
		return;

	/* if only some components are selected, it's important to rescale the result
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

/*********************** Dopesheet functions *************************/

/* ** Channels sort comparators ** */

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

/* Calculate frames segments at which track is tracked continuously. */
static void tracking_dopesheet_channels_segments_calc(MovieTrackingDopesheetChannel *channel)
{
	MovieTrackingTrack *track = channel->track;
	int i, segment;

	channel->tot_segment = 0;
	channel->max_segment = 0;
	channel->total_frames = 0;

	/* TODO(sergey): looks a bit code-duplicated, need to look into
	 *               logic de-duplication here.
	 */

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

			channel->max_segment = max_ii(channel->max_segment, len);
			segment++;
		}

		i++;
	}
}

/* Create channels for tracks and calculate tracked segments for them. */
static void tracking_dopesheet_channels_calc(MovieTracking *tracking)
{
	MovieTrackingObject *object = BKE_tracking_object_get_active(tracking);
	MovieTrackingDopesheet *dopesheet = &tracking->dopesheet;
	MovieTrackingTrack *track;
	MovieTrackingReconstruction *reconstruction =
		BKE_tracking_object_get_reconstruction(tracking, object);
	ListBase *tracksbase = BKE_tracking_object_get_tracks(tracking, object);

	bool sel_only = (dopesheet->flag & TRACKING_DOPE_SELECTED_ONLY) != 0;
	bool show_hidden = (dopesheet->flag & TRACKING_DOPE_SHOW_HIDDEN) != 0;

	for (track = tracksbase->first; track; track = track->next) {
		MovieTrackingDopesheetChannel *channel;

		if (!show_hidden && (track->flag & TRACK_HIDDEN) != 0)
			continue;

		if (sel_only && !TRACK_SELECTED(track))
			continue;

		channel = MEM_callocN(sizeof(MovieTrackingDopesheetChannel), "tracking dopesheet channel");
		channel->track = track;

		if (reconstruction->flag & TRACKING_RECONSTRUCTED) {
			BLI_snprintf(channel->name, sizeof(channel->name), "%s (%.4f)", track->name, track->error);
		}
		else {
			BLI_strncpy(channel->name, track->name, sizeof(channel->name));
		}

		tracking_dopesheet_channels_segments_calc(channel);

		BLI_addtail(&dopesheet->channels, channel);
		dopesheet->tot_channel++;
	}
}

/* Sot dopesheet channels using given method (name, average error, total coverage,
 * longest tracked segment) and could also inverse the list if it's enabled.
 */
static void tracking_dopesheet_channels_sort(MovieTracking *tracking, int sort_method, bool inverse)
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

static int coverage_from_count(int count)
{
	/* Values are actually arbitrary here, probably need to be tweaked. */
	if (count < 8)
		return TRACKING_COVERAGE_BAD;
	else if (count < 16)
		return TRACKING_COVERAGE_ACCEPTABLE;
	return TRACKING_COVERAGE_OK;
}

/* Calculate coverage of frames with tracks, this information
 * is used to highlight dopesheet background depending on how
 * many tracks exists on the frame.
 */
static void tracking_dopesheet_calc_coverage(MovieTracking *tracking)
{
	MovieTrackingDopesheet *dopesheet = &tracking->dopesheet;
	MovieTrackingObject *object = BKE_tracking_object_get_active(tracking);
	ListBase *tracksbase = BKE_tracking_object_get_tracks(tracking, object);
	MovieTrackingTrack *track;
	int frames, start_frame = INT_MAX, end_frame = -INT_MAX;
	int *per_frame_counter;
	int prev_coverage, last_segment_frame;
	int i;

	/* find frame boundaries */
	for (track = tracksbase->first; track; track = track->next) {
		start_frame = min_ii(start_frame, track->markers[0].framenr);
		end_frame = max_ii(end_frame, track->markers[track->markersnr - 1].framenr);
	}

	frames = end_frame - start_frame + 1;

	/* this is a per-frame counter of markers (how many markers belongs to the same frame) */
	per_frame_counter = MEM_callocN(sizeof(int) * frames, "per frame track counter");

	/* find per-frame markers count */
	for (track = tracksbase->first; track; track = track->next) {
		int i;

		for (i = 0; i < track->markersnr; i++) {
			MovieTrackingMarker *marker = &track->markers[i];

			/* TODO: perhaps we need to add check for non-single-frame track here */
			if ((marker->flag & MARKER_DISABLED) == 0)
				per_frame_counter[marker->framenr - start_frame]++;
		}
	}

	/* convert markers count to coverage and detect segments with the same coverage */
	prev_coverage = coverage_from_count(per_frame_counter[0]);
	last_segment_frame = start_frame;

	/* means only disabled tracks in the beginning, could be ignored */
	if (!per_frame_counter[0])
		prev_coverage = TRACKING_COVERAGE_OK;

	for (i = 1; i < frames; i++) {
		int coverage = coverage_from_count(per_frame_counter[i]);

		/* means only disabled tracks in the end, could be ignored */
		if (i == frames - 1 && !per_frame_counter[i])
			coverage = TRACKING_COVERAGE_OK;

		if (coverage != prev_coverage || i == frames - 1) {
			MovieTrackingDopesheetCoverageSegment *coverage_segment;
			int end_segment_frame = i - 1 + start_frame;

			if (end_segment_frame == last_segment_frame)
				end_segment_frame++;

			coverage_segment = MEM_callocN(sizeof(MovieTrackingDopesheetCoverageSegment), "tracking coverage segment");
			coverage_segment->coverage = prev_coverage;
			coverage_segment->start_frame = last_segment_frame;
			coverage_segment->end_frame = end_segment_frame;

			BLI_addtail(&dopesheet->coverage_segments, coverage_segment);

			last_segment_frame = end_segment_frame;
		}

		prev_coverage = coverage;
	}

	MEM_freeN(per_frame_counter);
}

/* Tag dopesheet for update, actual update will happen later
 * when it'll be actually needed.
 */
void BKE_tracking_dopesheet_tag_update(MovieTracking *tracking)
{
	MovieTrackingDopesheet *dopesheet = &tracking->dopesheet;

	dopesheet->ok = false;
}

/* Do dopesheet update, if update is not needed nothing will happen. */
void BKE_tracking_dopesheet_update(MovieTracking *tracking)
{
	MovieTrackingDopesheet *dopesheet = &tracking->dopesheet;

	short sort_method = dopesheet->sort_method;
	bool inverse = (dopesheet->flag & TRACKING_DOPE_SORT_INVERSE) != 0;

	if (dopesheet->ok)
		return;

	tracking_dopesheet_free(dopesheet);

	/* channels */
	tracking_dopesheet_channels_calc(tracking);
	tracking_dopesheet_channels_sort(tracking, sort_method, inverse);

	/* frame coverage */
	tracking_dopesheet_calc_coverage(tracking);

	dopesheet->ok = true;
}
