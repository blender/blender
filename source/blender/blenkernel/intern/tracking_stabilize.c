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

/** \file blender/blenkernel/intern/tracking_stabilize.c
 *  \ingroup bke
 *
 * This file contains implementation of 2D frame stabilization.
 */

#include <limits.h>

#include "DNA_movieclip_types.h"

#include "BLI_utildefines.h"
#include "BLI_math.h"

#include "BKE_tracking.h"

#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"

/* Calculate median point of markers of tracks marked as used for
 * 2D stabilization.
 *
 * NOTE: frame number should be in clip space, not scene space
 */
static bool stabilization_median_point_get(MovieTracking *tracking, int framenr, float median[2])
{
	bool ok = false;
	float min[2], max[2];
	MovieTrackingTrack *track;

	INIT_MINMAX2(min, max);

	track = tracking->tracks.first;
	while (track) {
		if (track->flag & TRACK_USE_2D_STAB) {
			MovieTrackingMarker *marker = BKE_tracking_marker_get(track, framenr);

			minmax_v2v2_v2(min, max, marker->pos);

			ok = true;
		}

		track = track->next;
	}

	median[0] = (max[0] + min[0]) / 2.0f;
	median[1] = (max[1] + min[1]) / 2.0f;

	return ok;
}

/* Calculate stabilization data (translation, scale and rotation) from
 * given median of first and current frame medians, tracking data and
 * frame number.
 *
 * NOTE: frame number should be in clip space, not scene space
 */
static void stabilization_calculate_data(MovieTracking *tracking, int framenr, int width, int height,
                                         const float firstmedian[2], const float median[2],
                                         float translation[2], float *scale, float *angle)
{
	MovieTrackingStabilization *stab = &tracking->stabilization;

	*scale = (stab->scale - 1.0f) * stab->scaleinf + 1.0f;
	*angle = 0.0f;

	translation[0] = (firstmedian[0] - median[0]) * width * (*scale);
	translation[1] = (firstmedian[1] - median[1]) * height * (*scale);

	mul_v2_fl(translation, stab->locinf);

	if ((stab->flag & TRACKING_STABILIZE_ROTATION) && stab->rot_track && stab->rotinf) {
		MovieTrackingMarker *marker;
		float a[2], b[2];
		float x0 = (float)width / 2.0f, y0 = (float)height / 2.0f;
		float x = median[0] * width, y = median[1] * height;

		marker = BKE_tracking_marker_get(stab->rot_track, 1);
		sub_v2_v2v2(a, marker->pos, firstmedian);
		a[0] *= width;
		a[1] *= height;

		marker = BKE_tracking_marker_get(stab->rot_track, framenr);
		sub_v2_v2v2(b, marker->pos, median);
		b[0] *= width;
		b[1] *= height;

		*angle = -atan2f(a[0] * b[1] - a[1] * b[0], a[0] * b[0] + a[1] * b[1]);
		*angle *= stab->rotinf;

		/* convert to rotation around image center */
		translation[0] -= (x0 + (x - x0) * cosf(*angle) - (y - y0) * sinf(*angle) - x) * (*scale);
		translation[1] -= (y0 + (x - x0) * sinf(*angle) + (y - y0) * cosf(*angle) - y) * (*scale);
	}
}

/* Calculate factor of a scale, which will eliminate black areas
 * appearing on the frame caused by frame translation.
 */
static float stabilization_calculate_autoscale_factor(MovieTracking *tracking, int width, int height)
{
	float firstmedian[2];
	MovieTrackingStabilization *stab = &tracking->stabilization;
	float aspect = tracking->camera.pixel_aspect;

	/* Early output if stabilization data is already up-to-date. */
	if (stab->ok)
		return stab->scale;

	/* See comment in BKE_tracking_stabilization_data_get about first frame. */
	if (stabilization_median_point_get(tracking, 1, firstmedian)) {
		int sfra = INT_MAX, efra = INT_MIN, cfra;
		float scale = 1.0f;
		MovieTrackingTrack *track;

		stab->scale = 1.0f;

		/* Calculate frame range of tracks used for stabilization. */
		track = tracking->tracks.first;
		while (track) {
			if (track->flag & TRACK_USE_2D_STAB ||
			    ((stab->flag & TRACKING_STABILIZE_ROTATION) && track == stab->rot_track))
			{
				sfra = min_ii(sfra, track->markers[0].framenr);
				efra = max_ii(efra, track->markers[track->markersnr - 1].framenr);
			}

			track = track->next;
		}

		/* For every frame we calculate scale factor needed to eliminate black
		 * area and choose largest scale factor as final one.
		 */
		for (cfra = sfra; cfra <= efra; cfra++) {
			float median[2];
			float translation[2], angle, tmp_scale;
			int i;
			float mat[4][4];
			float points[4][2] = {{0.0f, 0.0f}, {0.0f, height}, {width, height}, {width, 0.0f}};
			float si, co;

			stabilization_median_point_get(tracking, cfra, median);

			stabilization_calculate_data(tracking, cfra, width, height, firstmedian, median, translation,
			                             &tmp_scale, &angle);

			BKE_tracking_stabilization_data_to_mat4(width, height, aspect, translation, 1.0f, angle, mat);

			si = sinf(angle);
			co = cosf(angle);

			for (i = 0; i < 4; i++) {
				int j;
				float a[3] = {0.0f, 0.0f, 0.0f}, b[3] = {0.0f, 0.0f, 0.0f};

				copy_v3_v3(a, points[i]);
				copy_v3_v3(b, points[(i + 1) % 4]);

				mul_m4_v3(mat, a);
				mul_m4_v3(mat, b);

				for (j = 0; j < 4; j++) {
					float point[3] = {points[j][0], points[j][1], 0.0f};
					float v1[3], v2[3];

					sub_v3_v3v3(v1, b, a);
					sub_v3_v3v3(v2, point, a);

					if (cross_v2v2(v1, v2) >= 0.0f) {
						const float rotDx[4][2] = {{1.0f, 0.0f}, {0.0f, -1.0f}, {-1.0f, 0.0f}, {0.0f, 1.0f}};
						const float rotDy[4][2] = {{0.0f, 1.0f}, {1.0f, 0.0f}, {0.0f, -1.0f}, {-1.0f, 0.0f}};

						float dx = translation[0] * rotDx[j][0] + translation[1] * rotDx[j][1],
						      dy = translation[0] * rotDy[j][0] + translation[1] * rotDy[j][1];

						float w, h, E, F, G, H, I, J, K, S;

						if (j % 2) {
							w = (float)height / 2.0f;
							h = (float)width / 2.0f;
						}
						else {
							w = (float)width / 2.0f;
							h = (float)height / 2.0f;
						}

						E = -w * co + h * si;
						F = -h * co - w * si;

						if ((i % 2) == (j % 2)) {
							G = -w * co - h * si;
							H = h * co - w * si;
						}
						else {
							G = w * co + h * si;
							H = -h * co + w * si;
						}

						I = F - H;
						J = G - E;
						K = G * F - E * H;

						S = (-w * I - h * J) / (dx * I + dy * J + K);

						scale = max_ff(scale, S);
					}
				}
			}
		}

		stab->scale = scale;

		if (stab->maxscale > 0.0f)
			stab->scale = min_ff(stab->scale, stab->maxscale);
	}
	else {
		stab->scale = 1.0f;
	}

	stab->ok = true;

	return stab->scale;
}

/* Get stabilization data (translation, scaling and angle) for a given frame.
 *
 * NOTE: frame number should be in clip space, not scene space
 */
void BKE_tracking_stabilization_data_get(MovieTracking *tracking, int framenr, int width, int height,
                                         float translation[2], float *scale, float *angle)
{
	float firstmedian[2], median[2];
	MovieTrackingStabilization *stab = &tracking->stabilization;

	/* Early output if stabilization is disabled. */
	if ((stab->flag & TRACKING_2D_STABILIZATION) == 0) {
		zero_v2(translation);
		*scale = 1.0f;
		*angle = 0.0f;

		return;
	}

	/* Even if tracks does not start at frame 1, their position will
	 * be estimated at this frame, which will give reasonable result
	 * in most of cases.
	 *
	 * However, it's still better to replace this with real first
	 * frame number at which tracks are appearing.
	 */
	if (stabilization_median_point_get(tracking, 1, firstmedian)) {
		stabilization_median_point_get(tracking, framenr, median);

		if ((stab->flag & TRACKING_AUTOSCALE) == 0)
			stab->scale = 1.0f;

		if (!stab->ok) {
			if (stab->flag & TRACKING_AUTOSCALE)
				stabilization_calculate_autoscale_factor(tracking, width, height);

			stabilization_calculate_data(tracking, framenr, width, height, firstmedian, median,
			                             translation, scale, angle);

			stab->ok = true;
		}
		else {
			stabilization_calculate_data(tracking, framenr, width, height, firstmedian, median,
			                             translation, scale, angle);
		}
	}
	else {
		zero_v2(translation);
		*scale = 1.0f;
		*angle = 0.0f;
	}
}

/* Stabilize given image buffer using stabilization data for
 * a specified frame number.
 *
 * NOTE: frame number should be in clip space, not scene space
 */
ImBuf *BKE_tracking_stabilize_frame(MovieTracking *tracking, int framenr, ImBuf *ibuf,
                                    float translation[2], float *scale, float *angle)
{
	float tloc[2], tscale, tangle;
	MovieTrackingStabilization *stab = &tracking->stabilization;
	ImBuf *tmpibuf;
	int width = ibuf->x, height = ibuf->y;
	float aspect = tracking->camera.pixel_aspect;
	float mat[4][4];
	int j, filter = tracking->stabilization.filter;
	void (*interpolation)(struct ImBuf *, struct ImBuf *, float, float, int, int) = NULL;
	int ibuf_flags;

	if (translation)
		copy_v2_v2(tloc, translation);

	if (scale)
		tscale = *scale;

	/* Perform early output if no stabilization is used. */
	if ((stab->flag & TRACKING_2D_STABILIZATION) == 0) {
		if (translation)
			zero_v2(translation);

		if (scale)
			*scale = 1.0f;

		if (angle)
			*angle = 0.0f;

		return ibuf;
	}

	/* Allocate frame for stabilization result. */
	ibuf_flags = 0;
	if (ibuf->rect)
		ibuf_flags |= IB_rect;
	if (ibuf->rect_float)
		ibuf_flags |= IB_rectfloat;

	tmpibuf = IMB_allocImBuf(ibuf->x, ibuf->y, ibuf->planes, ibuf_flags);

	/* Calculate stabilization matrix. */
	BKE_tracking_stabilization_data_get(tracking, framenr, width, height, tloc, &tscale, &tangle);
	BKE_tracking_stabilization_data_to_mat4(ibuf->x, ibuf->y, aspect, tloc, tscale, tangle, mat);
	invert_m4(mat);

	if (filter == TRACKING_FILTER_NEAREST)
		interpolation = nearest_interpolation;
	else if (filter == TRACKING_FILTER_BILINEAR)
		interpolation = bilinear_interpolation;
	else if (filter == TRACKING_FILTER_BICUBIC)
		interpolation = bicubic_interpolation;
	else
		/* fallback to default interpolation method */
		interpolation = nearest_interpolation;

	/* This function is only used for display in clip editor and
	 * sequencer only, which would only benefit of using threads
	 * here.
	 *
	 * But need to keep an eye on this if the function will be
	 * used in other cases.
	 */
#pragma omp parallel for if (tmpibuf->y > 128)
	for (j = 0; j < tmpibuf->y; j++) {
		int i;
		for (i = 0; i < tmpibuf->x; i++) {
			float vec[3] = {i, j, 0.0f};

			mul_v3_m4v3(vec, mat, vec);

			interpolation(ibuf, tmpibuf, vec[0], vec[1], i, j);
		}
	}

	if (tmpibuf->rect_float)
		tmpibuf->userflags |= IB_RECT_INVALID;

	if (translation)
		copy_v2_v2(translation, tloc);

	if (scale)
		*scale = tscale;

	if (angle)
		*angle = tangle;

	return tmpibuf;
}

/* Get 4x4 transformation matrix which corresponds to
 * stabilization data and used for easy coordinate
 * transformation.
 *
 * NOTE: The reason it is 4x4 matrix is because it's
 *       used for OpenGL drawing directly.
 */
void BKE_tracking_stabilization_data_to_mat4(int width, int height, float aspect,
                                             float translation[2], float scale, float angle,
                                             float mat[4][4])
{
	float translation_mat[4][4], rotation_mat[4][4], scale_mat[4][4],
	      center_mat[4][4], inv_center_mat[4][4],
	      aspect_mat[4][4], inv_aspect_mat[4][4];
	float scale_vector[3] = {scale, scale, scale};

	unit_m4(translation_mat);
	unit_m4(rotation_mat);
	unit_m4(scale_mat);
	unit_m4(center_mat);
	unit_m4(aspect_mat);

	/* aspect ratio correction matrix */
	aspect_mat[0][0] = 1.0f / aspect;
	invert_m4_m4(inv_aspect_mat, aspect_mat);

	/* image center as rotation center
	 *
	 * Rotation matrix is constructing in a way rotation happens around image center,
	 * and it's matter of calculating translation in a way, that applying translation
	 * after rotation would make it so rotation happens around median point of tracks
	 * used for translation stabilization.
	 */
	center_mat[3][0] = (float)width / 2.0f;
	center_mat[3][1] = (float)height / 2.0f;
	invert_m4_m4(inv_center_mat, center_mat);

	size_to_mat4(scale_mat, scale_vector);       /* scale matrix */
	add_v2_v2(translation_mat[3], translation);  /* translation matrix */
	rotate_m4(rotation_mat, 'Z', angle);         /* rotation matrix */

	/* compose transformation matrix */
	mul_m4_series(mat, translation_mat, center_mat, aspect_mat, rotation_mat, inv_aspect_mat,
	             scale_mat, inv_center_mat);
}
