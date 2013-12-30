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

#include "MEM_guardedalloc.h"

#include "DNA_gpencil_types.h"
#include "DNA_movieclip_types.h"
#include "DNA_object_types.h"   /* SELECT */

#include "BLI_utildefines.h"

#include "BKE_tracking.h"

#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"

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

		xu = x / width;
		yu = y / height;

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

/* Get a gray-scale unsigned char buffer from given image buffer
 * wich will be used for feature detection.
 */
static unsigned char *detect_get_frame_ucharbuf(ImBuf *ibuf)
{
	int x, y;
	unsigned char *pixels, *cp;

	cp = pixels = MEM_callocN(ibuf->x * ibuf->y * sizeof(unsigned char), "tracking ucharBuf");
	for (y = 0; y < ibuf->y; y++) {
		for (x = 0; x < ibuf->x; x++) {
			int pixel = ibuf->x * y + x;

			if (ibuf->rect_float) {
				const float *rrgbf = ibuf->rect_float + pixel * 4;
				const float gray_f = 0.2126f * rrgbf[0] + 0.7152f * rrgbf[1] + 0.0722f * rrgbf[2];

				*cp = FTOCHAR(gray_f);
			}
			else {
				const unsigned char *rrgb = (unsigned char *)ibuf->rect + pixel * 4;

				*cp = 0.2126f * rrgb[0] + 0.7152f * rrgb[1] + 0.0722f * rrgb[2];
			}

			cp++;
		}
	}

	return pixels;
}

/* Detect features using FAST detector */
void BKE_tracking_detect_fast(MovieTracking *tracking, ListBase *tracksbase, ImBuf *ibuf,
                              int framenr, int margin, int min_trackness, int min_distance, bGPDlayer *layer,
                              bool place_outside_layer)
{
	struct libmv_Features *features;
	unsigned char *pixels = detect_get_frame_ucharbuf(ibuf);

	features = libmv_detectFeaturesFAST(pixels, ibuf->x, ibuf->y, ibuf->x,
	                                    margin, min_trackness, min_distance);

	MEM_freeN(pixels);

	detect_retrieve_libmv_features(tracking, tracksbase, features,
	                               framenr, ibuf->x, ibuf->y, layer,
	                               place_outside_layer);

	libmv_featuresDestroy(features);
}
