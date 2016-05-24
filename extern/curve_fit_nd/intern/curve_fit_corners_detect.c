/*
 * Copyright (c) 2016, Blender Foundation.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the <organization> nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/** \file curve_fit_corners_detect.c
 *  \ingroup curve_fit
 */

#include <math.h>
#include <float.h>
#include <stdbool.h>
#include <assert.h>

#include <string.h>
#include <stdlib.h>

#include "../curve_fit_nd.h"

typedef unsigned int uint;

#include "curve_fit_inline.h"

#ifdef _MSC_VER
#  define alloca(size) _alloca(size)
#endif

#if !defined(_MSC_VER)
#  define USE_VLA
#endif

#ifdef USE_VLA
#  ifdef __GNUC__
#    pragma GCC diagnostic ignored "-Wvla"
#  endif
#else
#  ifdef __GNUC__
#    pragma GCC diagnostic error "-Wvla"
#  endif
#endif

/* -------------------------------------------------------------------- */

/** \name Simple Vector Math Lib
 * \{ */

static double cos_vnvnvn(
        const double v0[], const double v1[], const double v2[],
        const uint dims)
{
#ifdef USE_VLA
	double dvec0[dims];
	double dvec1[dims];
#else
	double *dvec0 = alloca(sizeof(double) * dims);
	double *dvec1 = alloca(sizeof(double) * dims);
#endif
	normalize_vn_vnvn(dvec0, v0, v1, dims);
	normalize_vn_vnvn(dvec1, v1, v2, dims);
	double d = dot_vnvn(dvec0, dvec1, dims);
	/* sanity check */
	d = max(-1.0, min(1.0, d));
	return d;
}

static double angle_vnvnvn(
        const double v0[], const double v1[], const double v2[],
        const uint dims)
{
	return acos(cos_vnvnvn(v0, v1, v2, dims));
}


static bool isect_line_sphere_vn(
        const double l1[],
        const double l2[],
        const double sp[],
        const double r,
        uint dims,

        double r_p1[]
#if 0   /* UNUSED */
        double r_p2[]
#endif
        )
{
#ifdef USE_VLA
	double ldir[dims];
	double tvec[dims];
#else
	double *ldir = alloca(sizeof(double) * dims);
	double *tvec = alloca(sizeof(double) * dims);
#endif

	sub_vn_vnvn(ldir, l2, l1, dims);

	sub_vn_vnvn(tvec, l1, sp, dims);
	const double a = len_squared_vn(ldir, dims);
	const double b = 2.0 * dot_vnvn(ldir, tvec, dims);
	const double c = len_squared_vn(sp, dims) + len_squared_vn(l1, dims) - (2.0 * dot_vnvn(sp, l1, dims)) - sq(r);

	const double i = b * b - 4.0 * a * c;

	if ((i < 0.0) || (a == 0.0)) {
		return false;
	}
	else if (i == 0.0) {
		/* one intersection */
		const double mu = -b / (2.0 * a);
		mul_vnvn_fl(r_p1, ldir, mu, dims);
		iadd_vnvn(r_p1, l1, dims);
		return true;
	}
	else if (i > 0.0) {
		/* # avoid calc twice */
		const double i_sqrt = sqrt(i);
		double mu;

		/* Note: when l1 is inside the sphere and l2 is outside.
		 * the first intersection point will always be between the pair. */

		/* first intersection */
		mu = (-b + i_sqrt) / (2.0 * a);
		mul_vnvn_fl(r_p1, ldir, mu, dims);
		iadd_vnvn(r_p1, l1, dims);
#if 0
		/* second intersection */
		mu = (-b - i_sqrt) / (2.0 * a);
		mul_vnvn_fl(r_p2, ldir, mu, dims);
		iadd_vnvn(r_p2, l1, dims);
#endif
		return true;
	}
	else {
		return false;
	}
}

/** \} */


/* -------------------------------------------------------------------- */


static bool point_corner_measure(
        const double *points,
        const uint    points_len,
        const uint i,
        const uint i_prev_init,
        const uint i_next_init,
        const double radius,
        const uint samples_max,
        const uint dims,

        double r_p_prev[], uint *r_i_prev_next,
        double r_p_next[], uint *r_i_next_prev)
{
	const double *p = &points[i * dims];
	uint sample;


	uint i_prev = i_prev_init;
	uint i_prev_next = i_prev + 1;
	sample = 0;
	while (true) {
		if ((i_prev == -1) || (sample++ > samples_max)) {
			return false;
		}
		else if (len_squared_vnvn(p, &points[i_prev * dims], dims) < radius) {
			i_prev -= 1;
		}
		else {
			break;
		}
	}

	uint i_next = i_next_init;
	uint i_next_prev = i_next - 1;
	sample = 0;
	while (true) {
		if ((i_next == points_len) || (sample++ > samples_max)) {
			return false;
		}
		else if (len_squared_vnvn(p, &points[i_next * dims], dims) < radius) {
			i_next += 1;
		}
		else {
			break;
		}
	}

	/* find points on the sphere */
	if (!isect_line_sphere_vn(
	        &points[i_prev * dims], &points[i_prev_next * dims], p, radius, dims,
	        r_p_prev))
	{
		return false;
	}

	if (!isect_line_sphere_vn(
	        &points[i_next * dims], &points[i_next_prev * dims], p, radius, dims,
	        r_p_next))
	{
		return false;
	}

	*r_i_prev_next = i_prev_next;
	*r_i_next_prev = i_next_prev;

	return true;
}


static double point_corner_angle(
        const double *points,
        const uint    points_len,
        const uint i,
        const double radius_mid,
        const double radius_max,
        const double angle_threshold,
        const double angle_threshold_cos,
        /* prevent locking up when for example `radius_min` is very large
         * (possibly larger then the curve).
         * In this case we would end up checking every point from every other point,
         * never reaching one that was outside the `radius_min`. */

        /* prevent locking up when for e */
        const uint samples_max,

        const uint dims)
{
	assert(angle_threshold_cos == cos(angle_threshold));

	if (i == 0 || i == points_len - 1) {
		return 0.0;
	}

	const double *p = &points[i * dims];

	/* initial test */
	if (cos_vnvnvn(&points[(i - 1) * dims], p, &points[(i + 1) * dims], dims) > angle_threshold_cos) {
		return 0.0;
	}

#ifdef USE_VLA
	double p_mid_prev[dims];
	double p_mid_next[dims];
#else
	double *p_mid_prev = alloca(sizeof(double) * dims);
	double *p_mid_next = alloca(sizeof(double) * dims);
#endif

	uint i_mid_prev_next, i_mid_next_prev;
	if (point_corner_measure(
	        points, points_len,
	        i, i - 1, i + 1,
	        radius_mid,
	        samples_max,
	        dims,

	        p_mid_prev, &i_mid_prev_next,
	        p_mid_next, &i_mid_next_prev))
	{
		const double angle_mid_cos = cos_vnvnvn(p_mid_prev, p, p_mid_next, dims);

		/* compare as cos and flip direction */

		/* if (angle_mid > angle_threshold) { */
		if (angle_mid_cos < angle_threshold_cos) {
#ifdef USE_VLA
			double p_max_prev[dims];
			double p_max_next[dims];
#else
			double *p_max_prev = alloca(sizeof(double) * dims);
			double *p_max_next = alloca(sizeof(double) * dims);
#endif

			uint i_max_prev_next, i_max_next_prev;
			if (point_corner_measure(
			        points, points_len,
			        i, i - 1, i + 1,
			        radius_max,
			        samples_max,
			        dims,

			        p_max_prev, &i_max_prev_next,
			        p_max_next, &i_max_next_prev))
			{
				const double angle_mid = acos(angle_mid_cos);
				const double angle_max = angle_vnvnvn(p_max_prev, p, p_max_next, dims) / 2.0;
				const double angle_diff = angle_mid - angle_max;
				if (angle_diff > angle_threshold) {
					return angle_diff;
				}
			}
		}
	}

	return 0.0;
}


int curve_fit_corners_detect_db(
        const double *points,
        const uint    points_len,
        const uint dims,
        const double radius_min,  /* ignore values below this */
        const double radius_max,  /* ignore values above this */
        const uint samples_max,
        const double angle_threshold,

        uint **r_corners,
        uint  *r_corners_len)
{
	const double angle_threshold_cos = cos(angle_threshold);
	uint corners_len = 0;

	/* Use the difference in angle between the mid-max radii
	 * to detect the difference between a corner and a sharp turn. */
	const double radius_mid = (radius_min + radius_max) / 2.0;

	/* we could ignore first/last- but simple to keep aligned with the point array */
	double *points_angle = malloc(sizeof(double) * points_len);
	points_angle[0] = 0.0;

	*r_corners = NULL;
	*r_corners_len = 0;

	for (uint i = 0; i < points_len; i++) {
		points_angle[i] =  point_corner_angle(
		        points, points_len, i,
		        radius_mid, radius_max,
		        angle_threshold, angle_threshold_cos,
		        samples_max,
		        dims);

		if (points_angle[i] != 0.0) {
			corners_len++;
		}
	}

	if (corners_len == 0) {
		free(points_angle);
		return 0;
	}

	/* Clean angle limits!
	 *
	 * How this works:
	 * - Find contiguous 'corners' (where the distance is less or equal to the error threshold).
	 * - Keep track of the corner with the highest angle
	 * - Clear every other angle (so they're ignored when setting corners). */
	{
		const double radius_min_sq = sq(radius_min);
		uint i_span_start = 0;
		while (i_span_start < points_len) {
			uint i_span_end = i_span_start;
			if (points_angle[i_span_start] != 0.0) {
				uint i_next = i_span_start + 1;
				uint i_best = i_span_start;
				while (i_next < points_len) {
					if ((points_angle[i_next] == 0.0) ||
					    (len_squared_vnvn(
					         &points[(i_next - 1) * dims],
					         &points[i_next * dims], dims) > radius_min_sq))
					{
						break;
					}
					else {
						if (points_angle[i_best] < points_angle[i_next]) {
							i_best = i_next;
						}
						i_span_end = i_next;
						i_next += 1;
					}
				}

				if (i_span_start != i_span_end) {
					uint i = i_span_start;
					while (i <= i_span_end) {
						if (i != i_best) {
							/* we could use some other error code */
							assert(points_angle[i] != 0.0);
							points_angle[i] = 0.0;
							corners_len--;
						}
						i += 1;
					}
				}
			}
			i_span_start = i_span_end + 1;
		}
	}
	/* End angle limit cleaning! */

	corners_len += 2;  /* first and last */
	uint *corners = malloc(sizeof(uint) * corners_len);
	uint i_corner = 0;
	corners[i_corner++] = 0;
	for (uint i = 0; i < points_len; i++) {
		if (points_angle[i] != 0.0) {
			corners[i_corner++] = i;
		}
	}
	corners[i_corner++] = points_len - 1;
	assert(i_corner == corners_len);

	free(points_angle);

	*r_corners = corners;
	*r_corners_len = corners_len;

	return 0;
}

int curve_fit_corners_detect_fl(
        const float *points,
        const uint   points_len,
        const uint dims,
        const float radius_min,  /* ignore values below this */
        const float radius_max,  /* ignore values above this */
        const uint samples_max,
        const float angle_threshold,

        uint **r_corners,
        uint  *r_corners_len)
{
	const uint points_flat_len = points_len * dims;
	double *points_db = malloc(sizeof(double) * points_flat_len);

	for (uint i = 0; i < points_flat_len; i++) {
		points_db[i] = (double)points[i];
	}

	int result = curve_fit_corners_detect_db(
	        points_db, points_len,
	        dims,
	        radius_min, radius_max,
	        samples_max,
	        angle_threshold,
	        r_corners, r_corners_len);

	free(points_db);

	return result;
}
