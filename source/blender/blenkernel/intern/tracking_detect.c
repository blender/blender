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

/** \file blender/blenkernel/intern/tracking_detect.c
 *  \ingroup bke
 *
 * This file contains blender-side implementation of feature detection.
 */

#include "DNA_gpencil_types.h"
#include "DNA_movieclip_types.h"
#include "DNA_object_types.h"   /* SELECT */

#include "BLI_utildefines.h"

#include "BKE_tracking.h"

#include "IMB_imbuf_types.h"

#include "libmv-capi.h"

/* Check whether point is inside grease pencil stroke. */
static bool check_point_in_stroke(bGPDstroke *stroke, float x, float y)
{
	int i, prev;
	int count = 0;
	bGPDspoint *points = stroke->points;

	/* Count intersections of horizontal ray coming from the point.
	 * Point will be inside layer if and only if number of intersection
	 * is uneven.
	 *
	 * Well, if layer has got self-intersections, this logic wouldn't
	 * work, but such situation is crappy anyway.
	 */

	prev = stroke->totpoints - 1;

	for (i = 0; i < stroke->totpoints; i++) {
		if ((points[i].y < y && points[prev].y >= y) || (points[prev].y < y && points[i].y >= y)) {
			float fac = (y - points[i].y) / (points[prev].y - points[i].y);

			if (points[i].x + fac * (points[prev].x - points[i].x) < x)
				count++;
		}

		prev = i;
	}

	return (count % 2) ? true : false;
}

/* Check whether point is inside any stroke of grease pencil layer. */
static bool check_point_in_layer(bGPDlayer *layer, float x, float y)
{
	bGPDframe *frame = layer->frames.first;

	while (frame) {
		bGPDstroke *stroke = frame->strokes.first;

		while (stroke) {
			if (check_point_in_stroke(stroke, x, y))
				return true;

			stroke = stroke->next;
		}
		frame = frame->next;
	}

	return false;
}

/* Get features detected by libmv and create tracks on the clip for them. */
static void detect_retrieve_libmv_features(MovieTracking *tracking, ListBase *tracksbase,
                                           struct libmv_Features *features, int framenr, int width, int height,
                                           bGPDlayer *layer, bool place_outside_layer)
{
	int a;

	a = libmv_countFeatures(features);
	while (a--) {
		MovieTrackingTrack *track;
		double x, y, size, score;
		bool ok = true;
		float xu, yu;

		libmv_getFeature(features, a, &x, &y, &score, &size);

		/* In Libmv integer coordinate points to pixel center, in blender
		 * it's not. Need to add 0.5px offset to center.
		 */
		xu = (x + 0.5) / width;
		yu = (y + 0.5) / height;

		if (layer)
			ok = check_point_in_layer(layer, xu, yu) != place_outside_layer;

		if (ok) {
			track = BKE_tracking_track_add(tracking, tracksbase, xu, yu, framenr, width, height);
			track->flag |= SELECT;
			track->pat_flag |= SELECT;
			track->search_flag |= SELECT;
		}
	}
}

static void run_configured_detector(MovieTracking *tracking, ListBase *tracksbase,
                                    ImBuf *ibuf, int framenr, bGPDlayer *layer, bool place_outside_layer,
                                    libmv_DetectOptions *options)
{
	struct libmv_Features *features = NULL;

	if (ibuf->rect_float) {
		features = libmv_detectFeaturesFloat(ibuf->rect_float, ibuf->x, ibuf->y, 4, options);
	}
	else if (ibuf->rect) {
		features = libmv_detectFeaturesByte((unsigned char *) ibuf->rect, ibuf->x, ibuf->y, 4, options);
	}

	if (features != NULL) {
		detect_retrieve_libmv_features(tracking, tracksbase, features,
		                               framenr, ibuf->x, ibuf->y, layer,
		                               place_outside_layer);

		libmv_featuresDestroy(features);
	}
}

/* Detect features using FAST detector */
void BKE_tracking_detect_fast(MovieTracking *tracking, ListBase *tracksbase, ImBuf *ibuf,
                              int framenr, int margin, int min_trackness, int min_distance, bGPDlayer *layer,
                              bool place_outside_layer)
{
	libmv_DetectOptions options = {0};

	options.detector = LIBMV_DETECTOR_FAST;
	options.margin = margin;
	options.min_distance = min_distance;
	options.fast_min_trackness = min_trackness;

	run_configured_detector(tracking, tracksbase, ibuf, framenr, layer,
	                        place_outside_layer, &options);
}

/* Detect features using Harris detector */
void BKE_tracking_detect_harris(MovieTracking *tracking, ListBase *tracksbase, ImBuf *ibuf,
                                int framenr, int margin, float threshold, int min_distance, bGPDlayer *layer,
                                bool place_outside_layer)
{
	libmv_DetectOptions options = {0};

	options.detector = LIBMV_DETECTOR_HARRIS;
	options.margin = margin;
	options.min_distance = min_distance;
	options.harris_threshold = threshold;

	run_configured_detector(tracking, tracksbase, ibuf, framenr, layer,
	                        place_outside_layer, &options);
}
