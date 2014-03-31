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

/** \file blender/blenkernel/intern/tracking_region_tracker.c
 *  \ingroup bke
 *
 * This file contains implementation of blender-side region tracker
 * which is used for 2D feature tracking.
 */

#include "MEM_guardedalloc.h"

#include "DNA_movieclip_types.h"
#include "DNA_object_types.h"   /* SELECT */

#include "BLI_utildefines.h"
#include "BLI_math.h"
#include "BLI_ghash.h"
#include "BLI_threads.h"

#include "BKE_tracking.h"
#include "BKE_movieclip.h"

#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"

#include "libmv-capi.h"
#include "tracking_private.h"

typedef struct TrackContext {
	/* the reference marker and cutout search area */
	MovieTrackingMarker reference_marker;

	/* keyframed patch. This is the search area */
	float *search_area;
	int search_area_height;
	int search_area_width;
	int framenr;

	float *mask;
} TrackContext;

typedef struct MovieTrackingContext {
	MovieClipUser user;
	MovieClip *clip;
	int clip_flag;

	int frames, first_frame;
	bool first_time;

	MovieTrackingSettings settings;
	TracksMap *tracks_map;

	bool backwards, sequence;
	int sync_frame;
} MovieTrackingContext;

static void track_context_free(void *customdata)
{
	TrackContext *track_context = (TrackContext *)customdata;

	if (track_context->search_area)
		MEM_freeN(track_context->search_area);

	if (track_context->mask)
		MEM_freeN(track_context->mask);
}

/* Create context for motion 2D tracking, copies all data needed
 * for thread-safe tracking, allowing clip modifications during
 * tracking.
 */
MovieTrackingContext *BKE_tracking_context_new(MovieClip *clip, MovieClipUser *user,
                                               const bool backwards, const bool sequence)
{
	MovieTrackingContext *context = MEM_callocN(sizeof(MovieTrackingContext), "trackingContext");
	MovieTracking *tracking = &clip->tracking;
	MovieTrackingSettings *settings = &tracking->settings;
	ListBase *tracksbase = BKE_tracking_get_active_tracks(tracking);
	MovieTrackingTrack *track;
	MovieTrackingObject *object = BKE_tracking_object_get_active(tracking);
	int num_tracks = 0;

	context->clip = clip;
	context->settings = *settings;
	context->backwards = backwards;
	context->sync_frame = user->framenr;
	context->first_time = true;
	context->first_frame = user->framenr;
	context->sequence = sequence;

	/* count */
	track = tracksbase->first;
	while (track) {
		if (TRACK_SELECTED(track) && (track->flag & (TRACK_LOCKED | TRACK_HIDDEN)) == 0) {
			int framenr = BKE_movieclip_remap_scene_to_clip_frame(clip, user->framenr);
			MovieTrackingMarker *marker = BKE_tracking_marker_get(track, framenr);

			if ((marker->flag & MARKER_DISABLED) == 0)
				num_tracks++;
		}

		track = track->next;
	}

	/* create tracking contextx for all tracks which would be tracked */
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
				MovieTrackingMarker *marker = BKE_tracking_marker_get(track, framenr);

				if ((marker->flag & MARKER_DISABLED) == 0) {
					TrackContext track_context;
					memset(&track_context, 0, sizeof(TrackContext));
					tracks_map_insert(context->tracks_map, track, &track_context);
				}
			}

			track = track->next;
		}
	}

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

/* Free context used for tracking. */
void BKE_tracking_context_free(MovieTrackingContext *context)
{
	if (!context->sequence)
		BLI_end_threaded_malloc();

	tracks_map_free(context->tracks_map, track_context_free);

	MEM_freeN(context);
}

/* Synchronize tracks between clip editor and tracking context,
 * by merging them together so all new created tracks and tracked
 * ones presents in the movie clip.
 */
void BKE_tracking_context_sync(MovieTrackingContext *context)
{
	MovieTracking *tracking = &context->clip->tracking;
	int newframe;

	tracks_map_merge(context->tracks_map, tracking);

	if (context->backwards)
		newframe = context->user.framenr + 1;
	else
		newframe = context->user.framenr - 1;

	context->sync_frame = newframe;

	BKE_tracking_dopesheet_tag_update(tracking);
}

/* Synchronize clip user's frame number with a frame number from tracking context,
 * used to update current frame displayed in the clip editor while tracking.
 */
void BKE_tracking_context_sync_user(const MovieTrackingContext *context, MovieClipUser *user)
{
	user->framenr = context->sync_frame;
}

/* **** utility functions for tracking **** */

/* convert from float and byte RGBA to grayscale. Supports different coefficients for RGB. */
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

		gray[i] = (weight_red * pixel[0] + weight_green * pixel[1] + weight_blue * pixel[2]) / 255.0f;
	}
}

/* Get grayscale float search buffer for given marker and frame. */
static float *track_get_search_floatbuf(ImBuf *ibuf, MovieTrackingTrack *track, MovieTrackingMarker *marker,
                                        int *width_r, int *height_r)
{
	ImBuf *searchibuf;
	float *gray_pixels;
	int width, height;

	searchibuf = BKE_tracking_get_search_imbuf(ibuf, track, marker, false, true);

	if (!searchibuf) {
		*width_r = 0;
		*height_r = 0;
		return NULL;
	}

	width = searchibuf->x;
	height = searchibuf->y;

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

	*width_r = width;
	*height_r = height;

	return gray_pixels;
}

/* Get image boffer for a given frame
 *
 * Frame is in clip space.
 */
static ImBuf *tracking_context_get_frame_ibuf(MovieClip *clip, MovieClipUser *user, int clip_flag, int framenr)
{
	ImBuf *ibuf;
	MovieClipUser new_user = *user;

	new_user.framenr = BKE_movieclip_remap_clip_to_scene_frame(clip, framenr);

	ibuf = BKE_movieclip_get_ibuf_flag(clip, &new_user, clip_flag, MOVIECLIP_CACHE_SKIP);

	return ibuf;
}

/* Get previous keyframed marker. */
static MovieTrackingMarker *tracking_context_get_keyframed_marker(MovieTrackingTrack *track,
                                                                  int curfra, bool backwards)
{
	MovieTrackingMarker *marker_keyed = NULL;
	MovieTrackingMarker *marker_keyed_fallback = NULL;
	int a = BKE_tracking_marker_get(track, curfra) - track->markers;

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

/* Get image buffer for previous marker's keyframe. */
static ImBuf *tracking_context_get_keyframed_ibuf(MovieClip *clip, MovieClipUser *user, int clip_flag,
                                                  MovieTrackingTrack *track, int curfra, bool backwards,
                                                  MovieTrackingMarker **marker_keyed_r)
{
	MovieTrackingMarker *marker_keyed;
	int keyed_framenr;

	marker_keyed = tracking_context_get_keyframed_marker(track, curfra, backwards);
	if (marker_keyed == NULL) {
		return NULL;
	}

	keyed_framenr = marker_keyed->framenr;

	*marker_keyed_r = marker_keyed;

	return tracking_context_get_frame_ibuf(clip, user, clip_flag, keyed_framenr);
}

/* Get image buffer which si used as referece for track. */
static ImBuf *tracking_context_get_reference_ibuf(MovieClip *clip, MovieClipUser *user, int clip_flag,
                                                  MovieTrackingTrack *track, int curfra, bool backwards,
                                                  MovieTrackingMarker **reference_marker)
{
	ImBuf *ibuf = NULL;

	if (track->pattern_match == TRACK_MATCH_KEYFRAME) {
		ibuf = tracking_context_get_keyframed_ibuf(clip, user, clip_flag, track, curfra, backwards, reference_marker);
	}
	else {
		ibuf = tracking_context_get_frame_ibuf(clip, user, clip_flag, curfra);

		/* use current marker as keyframed position */
		*reference_marker = BKE_tracking_marker_get(track, curfra);
	}

	return ibuf;
}

/* Update track's reference patch (patch from which track is tracking from)
 *
 * Returns false if reference image buffer failed to load.
 */
static bool track_context_update_reference(MovieTrackingContext *context, TrackContext *track_context,
                                           MovieTrackingTrack *track, MovieTrackingMarker *marker, int curfra,
                                           int frame_width, int frame_height)
{
	MovieTrackingMarker *reference_marker = NULL;
	ImBuf *reference_ibuf = NULL;
	int width, height;

	/* calculate patch for keyframed position */
	reference_ibuf = tracking_context_get_reference_ibuf(context->clip, &context->user, context->clip_flag,
	                                                     track, curfra, context->backwards, &reference_marker);

	if (!reference_ibuf)
		return false;

	track_context->reference_marker = *reference_marker;

	if (track_context->search_area) {
		MEM_freeN(track_context->search_area);
	}

	track_context->search_area = track_get_search_floatbuf(reference_ibuf, track, reference_marker, &width, &height);
	track_context->search_area_height = height;
	track_context->search_area_width = width;

	if ((track->algorithm_flag & TRACK_ALGORITHM_FLAG_USE_MASK) != 0) {
		if (track_context->mask)
			MEM_freeN(track_context->mask);

		track_context->mask = BKE_tracking_track_get_mask(frame_width, frame_height, track, marker);
	}

	IMB_freeImBuf(reference_ibuf);

	return true;
}

/* Fill in libmv tracker options structure with settings need to be used to perform track. */
static void tracking_configure_tracker(const MovieTrackingTrack *track, float *mask,
                                       libmv_TrackRegionOptions *options)
{
	options->motion_model = track->motion_model;

	options->use_brute = ((track->algorithm_flag & TRACK_ALGORITHM_FLAG_USE_BRUTE) != 0);

	options->use_normalization = ((track->algorithm_flag & TRACK_ALGORITHM_FLAG_USE_NORMALIZATION) != 0);

	options->num_iterations = 50;
	options->minimum_correlation = track->minimum_correlation;
	options->sigma = 0.9;

	if ((track->algorithm_flag & TRACK_ALGORITHM_FLAG_USE_MASK) != 0)
		options->image1_mask = mask;
	else
		options->image1_mask = NULL;
}

/* returns false if marker crossed margin area from frame bounds */
static bool tracking_check_marker_margin(MovieTrackingTrack *track, MovieTrackingMarker *marker,
                                         int frame_width, int frame_height)
{
	float pat_min[2], pat_max[2];
	float margin_left, margin_top, margin_right, margin_bottom;
	float normalized_track_margin[2];

	/* margin from frame boundaries */
	BKE_tracking_marker_pattern_minmax(marker, pat_min, pat_max);

	normalized_track_margin[0] = (float)track->margin / frame_width;
	normalized_track_margin[1] = (float)track->margin / frame_height;

	margin_left   = max_ff(-pat_min[0], normalized_track_margin[0]);
	margin_top    = max_ff( pat_max[1], normalized_track_margin[1]);
	margin_right  = max_ff( pat_max[0], normalized_track_margin[0]);
	margin_bottom = max_ff(-pat_min[1], normalized_track_margin[1]);

	/* do not track markers which are too close to boundary */
	if (marker->pos[0] < margin_left || marker->pos[0] > 1.0f - margin_right ||
	    marker->pos[1] < margin_bottom || marker->pos[1] > 1.0f - margin_top)
	{
		return false;
	}

	return true;
}

/* Scale search area of marker based on scale changes of pattern area,
 *
 * TODO(sergey): currently based on pattern bounding box scale change,
 *               smarter approach here is welcome.
 */
static void tracking_scale_marker_search(const MovieTrackingMarker *old_marker, MovieTrackingMarker *new_marker)
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

/* Insert new marker which was tracked from old_marker to a new image,
 * will also ensure tracked segment is surrounded by disabled markers.
 */
static void tracking_insert_new_marker(MovieTrackingContext *context, MovieTrackingTrack *track,
                                       const MovieTrackingMarker *old_marker, int curfra, bool tracked,
                                       int frame_width, int frame_height,
                                       double dst_pixel_x[5], double dst_pixel_y[5])
{
	MovieTrackingMarker new_marker;
	int frame_delta = context->backwards ? -1 : 1;
	int nextfra = curfra + frame_delta;

	new_marker = *old_marker;

	if (tracked) {
		tracking_set_marker_coords_from_tracking(frame_width, frame_height, &new_marker, dst_pixel_x, dst_pixel_y);
		new_marker.flag |= MARKER_TRACKED;
		new_marker.framenr = nextfra;

		tracking_scale_marker_search(old_marker, &new_marker);

		if (context->first_time) {
			/* check if there's no keyframe/tracked markers before tracking marker.
			 * if so -- create disabled marker before currently tracking "segment"
			 */

			tracking_marker_insert_disabled(track, old_marker, !context->backwards, false);
		}

		/* insert currently tracked marker */
		BKE_tracking_marker_insert(track, &new_marker);

		/* make currently tracked segment be finished with disabled marker */
		tracking_marker_insert_disabled(track, &new_marker, context->backwards, false);
	}
	else {
		new_marker.framenr = nextfra;
		new_marker.flag |= MARKER_DISABLED;

		BKE_tracking_marker_insert(track, &new_marker);
	}
}

/* Peform tracking from a reference_marker to destination_ibuf.
 * Uses marker as an initial position guess.
 *
 * Returns truth if tracker returned success, puts result
 * to dst_pixel_x and dst_pixel_y.
 */
static bool configure_and_run_tracker(ImBuf *destination_ibuf, MovieTrackingTrack *track,
                                      MovieTrackingMarker *reference_marker, MovieTrackingMarker *marker,
                                      float *reference_search_area, int reference_search_area_width,
                                      int reference_search_area_height, float *mask,
                                      double dst_pixel_x[5], double dst_pixel_y[5])
{
	/* To convert to the x/y split array format for libmv. */
	double src_pixel_x[5], src_pixel_y[5];

	/* Settings for the tracker */
	libmv_TrackRegionOptions options = {0};
	libmv_TrackRegionResult result;

	float *patch_new;

	int new_search_area_width, new_search_area_height;
	int frame_width, frame_height;

	bool tracked;

	frame_width = destination_ibuf->x;
	frame_height = destination_ibuf->y;

	/* for now track to the same search area dimension as marker has got for current frame
	 * will make all tracked markers in currently tracked segment have the same search area
	 * size, but it's quite close to what is actually needed
	 */
	patch_new = track_get_search_floatbuf(destination_ibuf, track, marker,
	                                      &new_search_area_width, &new_search_area_height);

	/* configure the tracker */
	tracking_configure_tracker(track, mask, &options);

	/* convert the marker corners and center into pixel coordinates in the search/destination images. */
	tracking_get_marker_coords_for_tracking(frame_width, frame_height, reference_marker, src_pixel_x, src_pixel_y);
	tracking_get_marker_coords_for_tracking(frame_width, frame_height, marker, dst_pixel_x, dst_pixel_y);

	if (patch_new == NULL || reference_search_area == NULL)
		return false;

	/* run the tracker! */
	tracked = libmv_trackRegion(&options,
	                            reference_search_area,
	                            reference_search_area_width,
	                            reference_search_area_height,
	                            patch_new,
	                            new_search_area_width,
	                            new_search_area_height,
	                            src_pixel_x, src_pixel_y,
	                            &result,
	                            dst_pixel_x, dst_pixel_y);

	MEM_freeN(patch_new);

	return tracked;
}

/* Track all the tracks from context one more frame,
 * returns FALSe if nothing was tracked.
 */
bool BKE_tracking_context_step(MovieTrackingContext *context)
{
	ImBuf *destination_ibuf;
	int frame_delta = context->backwards ? -1 : 1;
	int curfra =  BKE_movieclip_remap_scene_to_clip_frame(context->clip, context->user.framenr);
	int a, map_size;
	bool ok = false;

	int frame_width, frame_height;

	map_size = tracks_map_get_size(context->tracks_map);

	/* Nothing to track, avoid unneeded frames reading to save time and memory. */
	if (!map_size)
		return false;

	/* Get an image buffer for frame we're tracking to. */
	context->user.framenr += frame_delta;

	destination_ibuf = BKE_movieclip_get_ibuf_flag(context->clip, &context->user,
	                                               context->clip_flag, MOVIECLIP_CACHE_SKIP);
	if (!destination_ibuf)
		return false;

	frame_width = destination_ibuf->x;
	frame_height = destination_ibuf->y;

#pragma omp parallel for private(a) shared(destination_ibuf, ok) if (map_size > 1)
	for (a = 0; a < map_size; a++) {
		TrackContext *track_context = NULL;
		MovieTrackingTrack *track;
		MovieTrackingMarker *marker;

		tracks_map_get_indexed_element(context->tracks_map, a, &track, (void **)&track_context);

		marker = BKE_tracking_marker_get_exact(track, curfra);

		if (marker && (marker->flag & MARKER_DISABLED) == 0) {
			bool tracked = false, need_readjust;
			double dst_pixel_x[5], dst_pixel_y[5];

			if (track->pattern_match == TRACK_MATCH_KEYFRAME)
				need_readjust = context->first_time;
			else
				need_readjust = true;

			/* do not track markers which are too close to boundary */
			if (tracking_check_marker_margin(track, marker, frame_width, frame_height)) {
				if (need_readjust) {
					if (track_context_update_reference(context, track_context, track, marker,
					                                   curfra, frame_width, frame_height) == false)
					{
						/* happens when reference frame fails to be loaded */
						continue;
					}
				}

				tracked = configure_and_run_tracker(destination_ibuf, track,
				                                    &track_context->reference_marker, marker,
				                                    track_context->search_area,
				                                    track_context->search_area_width,
				                                    track_context->search_area_height,
				                                    track_context->mask,
				                                    dst_pixel_x, dst_pixel_y);
			}

			BLI_spin_lock(&context->tracks_map->spin_lock);
			tracking_insert_new_marker(context, track, marker, curfra, tracked,
			                           frame_width, frame_height, dst_pixel_x, dst_pixel_y);
			BLI_spin_unlock(&context->tracks_map->spin_lock);

			ok = true;
		}
	}

	IMB_freeImBuf(destination_ibuf);

	context->first_time = false;
	context->frames++;

	return ok;
}

void BKE_tracking_context_finish(MovieTrackingContext *context)
{
	MovieClip *clip = context->clip;
	ListBase *plane_tracks_base = BKE_tracking_get_active_plane_tracks(&clip->tracking);
	MovieTrackingPlaneTrack *plane_track;
	int map_size = tracks_map_get_size(context->tracks_map);

	for (plane_track = plane_tracks_base->first;
	     plane_track;
	     plane_track = plane_track->next)
	{
		if ((plane_track->flag & PLANE_TRACK_AUTOKEY) == 0) {
			int i;
			for (i = 0; i < map_size; i++) {
				TrackContext *track_context = NULL;
				MovieTrackingTrack *track, *old_track;
				bool do_update = false;
				int j;

				tracks_map_get_indexed_element(context->tracks_map, i, &track, (void **)&track_context);

				old_track = BLI_ghash_lookup(context->tracks_map->hash, track);
				for (j = 0; j < plane_track->point_tracksnr; j++) {
					if (plane_track->point_tracks[j] == old_track) {
						do_update = true;
						break;
					}
				}

				if (do_update) {
					BKE_tracking_track_plane_from_existing_motion(plane_track, context->first_frame);
					break;
				}
			}
		}
	}
}

/* Refine marker's position using previously known keyframe.
 * Direction of searching for a keyframe depends on backwards flag,
 * which means if backwards is false, previous keyframe will be as
 * reference.
 */
void BKE_tracking_refine_marker(MovieClip *clip, MovieTrackingTrack *track, MovieTrackingMarker *marker, bool backwards)
{
	MovieTrackingMarker *reference_marker = NULL;
	ImBuf *reference_ibuf, *destination_ibuf;
	float *search_area, *mask = NULL;
	int frame_width, frame_height;
	int search_area_height, search_area_width;
	int clip_flag = clip->flag & MCLIP_TIMECODE_FLAGS;
	int reference_framenr;
	MovieClipUser user = {0};
	double dst_pixel_x[5], dst_pixel_y[5];
	bool tracked;

	/* Construct a temporary clip used, used to acquire image buffers. */
	user.framenr = BKE_movieclip_remap_clip_to_scene_frame(clip, marker->framenr);

	BKE_movieclip_get_size(clip, &user, &frame_width, &frame_height);

	/* Get an image buffer for reference frame, also gets reference marker.
	 *
	 * Usually tracking_context_get_reference_ibuf will return current frame
	 * if marker is keyframed, which is correct for normal tracking. But here
	 * we'll want to have next/previous frame in such cases. So let's use small
	 * magic with original frame number used to get reference frame for.
	 */
	reference_framenr = backwards ? marker->framenr + 1 : marker->framenr - 1;
	reference_ibuf = tracking_context_get_reference_ibuf(clip, &user, clip_flag, track, reference_framenr,
	                                                     backwards, &reference_marker);
	if (reference_ibuf == NULL) {
		return;
	}

	/* Could not refine with self. */
	if (reference_marker == marker) {
		return;
	}

	/* Destination image buffer has got frame number corresponding to refining marker. */
	destination_ibuf = BKE_movieclip_get_ibuf_flag(clip, &user, clip_flag, MOVIECLIP_CACHE_SKIP);
	if (destination_ibuf == NULL) {
		IMB_freeImBuf(reference_ibuf);
		return;
	}

	/* Get search area from reference image. */
	search_area = track_get_search_floatbuf(reference_ibuf, track, reference_marker,
	                                        &search_area_width, &search_area_height);

	/* If needed, compute track's mask. */
	if ((track->algorithm_flag & TRACK_ALGORITHM_FLAG_USE_MASK) != 0)
		mask = BKE_tracking_track_get_mask(frame_width, frame_height, track, marker);

	/* Run the tracker from reference frame to current one. */
	tracked = configure_and_run_tracker(destination_ibuf, track, reference_marker, marker,
	                                    search_area, search_area_width, search_area_height,
	                                    mask, dst_pixel_x, dst_pixel_y);

	/* Refine current marker's position if track was successful. */
	if (tracked) {
		tracking_set_marker_coords_from_tracking(frame_width, frame_height, marker, dst_pixel_x, dst_pixel_y);
		marker->flag |= MARKER_TRACKED;
	}

	/* Free memory used for refining */
	MEM_freeN(search_area);
	if (mask)
		MEM_freeN(mask);
	IMB_freeImBuf(reference_ibuf);
	IMB_freeImBuf(destination_ibuf);
}
