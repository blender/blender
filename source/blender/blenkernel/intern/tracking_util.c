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

/** \file blender/blenkernel/intern/tracking_util.c
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

#include "BKE_movieclip.h"
#include "BKE_tracking.h"

#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"
#include "IMB_moviecache.h"

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


/* Fill in Libmv C-API camera intrinsics options from tracking structure. */
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

/* Get previous keyframed marker. */
MovieTrackingMarker *tracking_get_keyframed_marker(MovieTrackingTrack *track,
                                                   int current_frame,
                                                   bool backwards)
{
	MovieTrackingMarker *marker_keyed = NULL;
	MovieTrackingMarker *marker_keyed_fallback = NULL;
	int a = BKE_tracking_marker_get(track, current_frame) - track->markers;

	while (a >= 0 && a < track->markersnr) {
		int next = backwards ? a + 1 : a - 1;
		bool is_keyframed = false;
		MovieTrackingMarker *cur_marker = &track->markers[a];
		MovieTrackingMarker *next_marker = NULL;

		if (next >= 0 && next < track->markersnr)
			next_marker = &track->markers[next];

		if ((cur_marker->flag & MARKER_DISABLED) == 0) {
			/* If it'll happen so we didn't find a real keyframe marker,
			 * fallback to the first marker in current tracked segment
			 * as a keyframe.
			 */
			if (next_marker && next_marker->flag & MARKER_DISABLED) {
				if (marker_keyed_fallback == NULL)
					marker_keyed_fallback = cur_marker;
			}

			is_keyframed |= (cur_marker->flag & MARKER_TRACKED) == 0;
		}

		if (is_keyframed) {
			marker_keyed = cur_marker;

			break;
		}

		a = next;
	}

	if (marker_keyed == NULL)
		marker_keyed = marker_keyed_fallback;

	return marker_keyed;
}

/*********************** Frame accessr *************************/

typedef struct AccessCacheKey {
	int clip_index;
	int frame;
	int downscale;
	libmv_InputMode input_mode;
	int64_t transform_key;
} AccessCacheKey;

static unsigned int accesscache_hashhash(const void *key_v)
{
	const AccessCacheKey *key = (const AccessCacheKey *) key_v;
	/* TODP(sergey): Need better hasing here for faster frame access. */
	return key->clip_index << 16 | key->frame;
}

static bool accesscache_hashcmp(const void *a_v, const void *b_v)
{
	const AccessCacheKey *a = (const AccessCacheKey *) a_v;
	const AccessCacheKey *b = (const AccessCacheKey *) b_v;

#define COMPARE_FIELD(field)
	{ \
		if (a->clip_index != b->clip_index) { \
			return false; \
		} \
	} (void) 0

	COMPARE_FIELD(clip_index);
	COMPARE_FIELD(frame);
	COMPARE_FIELD(downscale);
	COMPARE_FIELD(input_mode);
	COMPARE_FIELD(transform_key);

#undef COMPARE_FIELD

	return true;
}

static void accesscache_put(TrackingImageAccessor *accessor,
                            int clip_index,
                            int frame,
                            libmv_InputMode input_mode,
                            int downscale,
                            int64_t transform_key,
                            ImBuf *ibuf)
{
	AccessCacheKey key;
	key.clip_index = clip_index;
	key.frame = frame;
	key.input_mode = input_mode;
	key.downscale = downscale;
	key.transform_key = transform_key;
	IMB_moviecache_put(accessor->cache, &key, ibuf);
}

static ImBuf *accesscache_get(TrackingImageAccessor *accessor,
                              int clip_index,
                              int frame,
                              libmv_InputMode input_mode,
                              int downscale,
                              int64_t transform_key)
{
	AccessCacheKey key;
	key.clip_index = clip_index;
	key.frame = frame;
	key.input_mode = input_mode;
	key.downscale = downscale;
	key.transform_key = transform_key;
	return IMB_moviecache_get(accessor->cache, &key);
}

static ImBuf *accessor_get_preprocessed_ibuf(TrackingImageAccessor *accessor,
                                             int clip_index,
                                             int frame)
{
	MovieClip *clip;
	MovieClipUser user;
	ImBuf *ibuf;
	int scene_frame;

	BLI_assert(clip_index < accessor->num_clips);

	clip = accessor->clips[clip_index];
	scene_frame = BKE_movieclip_remap_clip_to_scene_frame(clip, frame);
	BKE_movieclip_user_set_frame(&user, scene_frame);
	user.render_size = MCLIP_PROXY_RENDER_SIZE_FULL;
	user.render_flag = 0;
	ibuf = BKE_movieclip_get_ibuf(clip, &user);

	return ibuf;
}

static ImBuf *make_grayscale_ibuf_copy(ImBuf *ibuf)
{
	ImBuf *grayscale = IMB_allocImBuf(ibuf->x, ibuf->y, 32, 0);
	size_t size;
	int i;

	BLI_assert(ibuf->channels == 3 || ibuf->channels == 4);

	/* TODO(sergey): Bummer, currently IMB API only allows to create 4 channels
	 * float buffer, so we do it manually here.
	 *
	 * Will generalize it later.
	 */
	size = (size_t)grayscale->x * (size_t)grayscale->y * sizeof(float);
	grayscale->channels = 1;
	if ((grayscale->rect_float = MEM_mapallocN(size, "tracking grayscale image"))) {
		grayscale->mall |= IB_rectfloat;
		grayscale->flags |= IB_rectfloat;
	}

	for (i = 0; i < grayscale->x * grayscale->y; ++i) {
		const float *pixel = ibuf->rect_float + ibuf->channels * i;

		grayscale->rect_float[i] = 0.2126f * pixel[0] +
		                           0.7152f * pixel[1] +
		                           0.0722f * pixel[2];
	}

	return grayscale;
}

static void ibuf_to_float_image(const ImBuf *ibuf, libmv_FloatImage *float_image)
{
	BLI_assert(ibuf->rect_float != NULL);
	float_image->buffer = ibuf->rect_float;
	float_image->width = ibuf->x;
	float_image->height = ibuf->y;
	float_image->channels = ibuf->channels;
}

static ImBuf *float_image_to_ibuf(libmv_FloatImage *float_image)
{
	ImBuf *ibuf = IMB_allocImBuf(float_image->width, float_image->height, 32, 0);
	size_t size = (size_t)ibuf->x * (size_t)ibuf->y *
	              float_image->channels * sizeof(float);
	ibuf->channels = float_image->channels;
	if ((ibuf->rect_float = MEM_mapallocN(size, "tracking grayscale image"))) {
		ibuf->mall |= IB_rectfloat;
		ibuf->flags |= IB_rectfloat;
	}
	memcpy(ibuf->rect_float, float_image->buffer, size);
	return ibuf;
}

static ImBuf *accessor_get_ibuf(TrackingImageAccessor *accessor,
                                int clip_index,
                                int frame,
                                libmv_InputMode input_mode,
                                int downscale,
                                const libmv_Region *region,
                                const libmv_FrameTransform *transform)
{
	ImBuf *ibuf, *orig_ibuf, *final_ibuf;
	int64_t transform_key = 0;

	if (transform != NULL) {
		transform_key = libmv_frameAccessorgetTransformKey(transform);
	}

	/* First try to get fully processed image from the cache. */
	ibuf = accesscache_get(accessor,
	                       clip_index,
	                       frame,
	                       input_mode,
	                       downscale,
	                       transform_key);
	if (ibuf != NULL) {
		return ibuf;
	}

	/* And now we do postprocessing of the original frame. */
	orig_ibuf = accessor_get_preprocessed_ibuf(accessor, clip_index, frame);

	if (orig_ibuf == NULL) {
		return NULL;
	}

	if (region != NULL) {
		int width = region->max[0] - region->min[0],
		    height = region->max[1] - region->min[1];

		/* If the requested region goes outside of the actual frame we still
		 * return the requested region size, but only fill it's partially with
		 * the data we can.
		 */
		int clamped_origin_x = max_ii((int)region->min[0], 0),
		    clamped_origin_y = max_ii((int)region->min[1], 0);
		int dst_offset_x = clamped_origin_x - (int)region->min[0],
		    dst_offset_y = clamped_origin_y - (int)region->min[1];
		int clamped_width = width - dst_offset_x,
		    clamped_height = height - dst_offset_y;
		clamped_width = min_ii(clamped_width, orig_ibuf->x - clamped_origin_x);
		clamped_height = min_ii(clamped_height, orig_ibuf->y - clamped_origin_y);

		final_ibuf = IMB_allocImBuf(width, height, 32, IB_rectfloat);

		if (orig_ibuf->rect_float != NULL) {
			IMB_rectcpy(final_ibuf, orig_ibuf,
			            dst_offset_x, dst_offset_y,
			            clamped_origin_x, clamped_origin_y,
			            clamped_width, clamped_height);
		}
		else {
			int y;
			/* TODO(sergey): We don't do any color space or alpha conversion
			 * here. Probably Libmv is better to work in the linear space,
			 * but keep sRGB space here for compatibility for now.
			 */
			for (y = 0; y < clamped_height; ++y) {
				int x;
				for (x = 0; x < clamped_width; ++x) {
					int src_x = x + clamped_origin_x,
					    src_y = y + clamped_origin_y;
					int dst_x = x + dst_offset_x,
					    dst_y = y + dst_offset_y;
					int dst_index = (dst_y * width + dst_x) * 4,
					    src_index = (src_y * orig_ibuf->x + src_x) * 4;
					rgba_uchar_to_float(final_ibuf->rect_float + dst_index,
					                    (unsigned char *)orig_ibuf->rect +
					                                     src_index);
				}
			}
		}
	}
	else {
		/* Libmv only works with float images,
		 *
		 * This would likely make it so loads of float buffers are being stored
		 * in the cache which is nice on the one hand (faster re-use of the
		 * frames) but on the other hand it bumps the memory usage up.
		 */
		BLI_lock_thread(LOCK_MOVIECLIP);
		IMB_float_from_rect(orig_ibuf);
		BLI_unlock_thread(LOCK_MOVIECLIP);
		final_ibuf = orig_ibuf;
	}

	if (downscale > 0) {
		if (final_ibuf == orig_ibuf) {
			final_ibuf = IMB_dupImBuf(orig_ibuf);
		}
		IMB_scaleImBuf(final_ibuf,
		               ibuf->x / (1 << downscale),
		               ibuf->y / (1 << downscale));
	}

	if (transform != NULL) {
		libmv_FloatImage input_image, output_image;
		ibuf_to_float_image(final_ibuf, &input_image);
		libmv_frameAccessorgetTransformRun(transform,
		                                   &input_image,
		                                   &output_image);
		if (final_ibuf != orig_ibuf) {
			IMB_freeImBuf(final_ibuf);
		}
		final_ibuf = float_image_to_ibuf(&output_image);
		libmv_floatImageDestroy(&output_image);
	}

	if (input_mode == LIBMV_IMAGE_MODE_RGBA) {
		BLI_assert(ibuf->channels == 3 || ibuf->channels == 4);
		/* pass */
	}
	else /* if (input_mode == LIBMV_IMAGE_MODE_MONO) */ {
		if (final_ibuf->channels != 1) {
			ImBuf *grayscale_ibuf = make_grayscale_ibuf_copy(final_ibuf);
			if (final_ibuf != orig_ibuf) {
				/* We dereference original frame later. */
				IMB_freeImBuf(final_ibuf);
			}
			final_ibuf = grayscale_ibuf;
		}
	}

	/* it's possible processing stil didn't happen at this point,
	 * but we really need a copy of the buffer to be transformed
	 * and to be put to the cache.
	 */
	if (final_ibuf == orig_ibuf) {
		final_ibuf = IMB_dupImBuf(orig_ibuf);
	}

	IMB_freeImBuf(orig_ibuf);

	/* We put postprocessed frame to the cache always for now,
	 * not the smartest thing in the world, but who cares at this point.
	 */

	/* TODO(sergey): Disable cache for now, because we don't store region
	 * in the cache key and can't check whether cached version is usable for
	 * us or not.
	 *
	 * Need to think better about what to cache and when.
	 */
	if (false) {
		accesscache_put(accessor,
		                clip_index,
		                frame,
		                input_mode,
		                downscale,
		                transform_key,
		                final_ibuf);
	}

	return final_ibuf;
}

static libmv_CacheKey accessor_get_image_callback(
		struct libmv_FrameAccessorUserData *user_data,
		int clip_index,
		int frame,
		libmv_InputMode input_mode,
		int downscale,
		const libmv_Region *region,
		const libmv_FrameTransform *transform,
		float **destination,
		int *width,
		int *height,
		int *channels)
{
	TrackingImageAccessor *accessor = (TrackingImageAccessor *) user_data;
	ImBuf *ibuf;

	BLI_assert(clip_index >= 0 && clip_index < accessor->num_clips);

	ibuf = accessor_get_ibuf(accessor,
	                         clip_index,
	                         frame,
	                         input_mode,
	                         downscale,
	                         region,
	                         transform);

	if (ibuf) {
		*destination = ibuf->rect_float;
		*width = ibuf->x;
		*height = ibuf->y;
		*channels = ibuf->channels;
	}
	else {
		*destination = NULL;
		*width = 0;
		*height = 0;
		*channels = 0;
	}

	return ibuf;
}

static void accessor_release_image_callback(libmv_CacheKey cache_key)
{
	ImBuf *ibuf = (ImBuf *) cache_key;
	IMB_freeImBuf(ibuf);
}

TrackingImageAccessor *tracking_image_accessor_new(MovieClip *clips[MAX_ACCESSOR_CLIP],
                                                   int num_clips,
                                                   int start_frame)
{
	TrackingImageAccessor *accessor =
		MEM_callocN(sizeof(TrackingImageAccessor), "tracking image accessor");

	BLI_assert(num_clips <= MAX_ACCESSOR_CLIP);

	accessor->cache = IMB_moviecache_create("frame access cache",
	                                        sizeof(AccessCacheKey),
	                                        accesscache_hashhash,
	                                        accesscache_hashcmp);

	memcpy(accessor->clips, clips, num_clips * sizeof(MovieClip*));
	accessor->num_clips = num_clips;
	accessor->start_frame = start_frame;

	accessor->libmv_accessor =
		libmv_FrameAccessorNew((libmv_FrameAccessorUserData *) accessor,
		                       accessor_get_image_callback,
		                       accessor_release_image_callback);

	return accessor;
}

void tracking_image_accessor_destroy(TrackingImageAccessor *accessor)
{
	IMB_moviecache_free(accessor->cache);
	libmv_FrameAccessorDestroy(accessor->libmv_accessor);
	MEM_freeN(accessor);
}
