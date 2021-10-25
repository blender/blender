/*
 * Copyright (c) 2016, DWANGO Co., Ltd.
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

/** \file curve_fit_cubic.c
 *  \ingroup curve_fit
 */

#ifdef _MSC_VER
#  define _USE_MATH_DEFINES
#endif

#include <math.h>
#include <float.h>
#include <stdbool.h>
#include <assert.h>

#include <string.h>
#include <stdlib.h>

#include "../curve_fit_nd.h"

/* Take curvature into account when calculating the least square solution isn't usable. */
#define USE_CIRCULAR_FALLBACK

/* Use the maximum distance of any points from the direct line between 2 points
 * to calculate how long the handles need to be.
 * Can do a 'perfect' reversal of subdivision when for curve has symmetrical handles and doesn't change direction
 * (as with an 'S' shape). */
#define USE_OFFSET_FALLBACK

/* avoid re-calculating lengths multiple times */
#define USE_LENGTH_CACHE

/* store the indices in the cubic data so we can return the original indices,
 * useful when the caller has data associated with the curve. */
#define USE_ORIG_INDEX_DATA

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

#define SWAP(type, a, b)  {    \
	type sw_ap;                \
	sw_ap = (a);               \
	(a) = (b);                 \
	(b) = sw_ap;               \
} (void)0


/* -------------------------------------------------------------------- */

/** \name Cubic Type & Functions
 * \{ */

typedef struct Cubic {
	/* single linked lists */
	struct Cubic *next;
#ifdef USE_ORIG_INDEX_DATA
	uint orig_span;
#endif
	/* 0: point_0, 1: handle_0, 2: handle_1, 3: point_1,
	 * each one is offset by 'dims' */
	double pt_data[0];
} Cubic;

#define CUBIC_PT(cubic, index, dims) \
	(&(cubic)->pt_data[(index) * (dims)])

#define CUBIC_VARS(c, dims, _p0, _p1, _p2, _p3) \
	double \
	*_p0 = (c)->pt_data, \
	*_p1 = _p0 + (dims), \
	*_p2 = _p1 + (dims), \
	*_p3 = _p2 + (dims); ((void)0)
#define CUBIC_VARS_CONST(c, dims, _p0, _p1, _p2, _p3) \
	const double \
	*_p0 = (c)->pt_data, \
	*_p1 = _p0 + (dims), \
	*_p2 = _p1 + (dims), \
	*_p3 = _p2 + (dims); ((void)0)


static size_t cubic_alloc_size(const uint dims)
{
	return sizeof(Cubic) + (sizeof(double) * 4 * dims);
}

static Cubic *cubic_alloc(const uint dims)
{
	return malloc(cubic_alloc_size(dims));
}

static void cubic_copy(Cubic *cubic_dst, const Cubic *cubic_src, const uint dims)
{
	memcpy(cubic_dst, cubic_src, cubic_alloc_size(dims));
}

static void cubic_init(
        Cubic *cubic,
        const double p0[], const double p1[], const double p2[], const double p3[],
        const uint dims)
{
	copy_vnvn(CUBIC_PT(cubic, 0, dims), p0, dims);
	copy_vnvn(CUBIC_PT(cubic, 1, dims), p1, dims);
	copy_vnvn(CUBIC_PT(cubic, 2, dims), p2, dims);
	copy_vnvn(CUBIC_PT(cubic, 3, dims), p3, dims);
}

static void cubic_free(Cubic *cubic)
{
	free(cubic);
}

/** \} */


/* -------------------------------------------------------------------- */

/** \name CubicList Type & Functions
 * \{ */

typedef struct CubicList {
	struct Cubic *items;
	uint          len;
	uint          dims;
} CubicList;

static void cubic_list_prepend(CubicList *clist, Cubic *cubic)
{
	cubic->next = clist->items;
	clist->items = cubic;
	clist->len++;
}

static double *cubic_list_as_array(
        const CubicList *clist
#ifdef USE_ORIG_INDEX_DATA
        ,
        const uint index_last,
        uint *r_orig_index
#endif
        )
{
	const uint dims = clist->dims;
	const uint array_flat_len = (clist->len + 1) * 3 * dims;

	double *array = malloc(sizeof(double) * array_flat_len);
	const double *handle_prev = &((Cubic *)clist->items)->pt_data[dims];

#ifdef USE_ORIG_INDEX_DATA
	uint orig_index_value = index_last;
	uint orig_index_index = clist->len;
	bool use_orig_index = (r_orig_index != NULL);
#endif

	/* fill the array backwards */
	const size_t array_chunk = 3 * dims;
	double *array_iter = array + array_flat_len;
	for (Cubic *citer = clist->items; citer; citer = citer->next) {
		array_iter -= array_chunk;
		memcpy(array_iter, &citer->pt_data[2 * dims], sizeof(double) * 2 * dims);
		memcpy(&array_iter[2 * dims], &handle_prev[dims], sizeof(double) * dims);
		handle_prev = citer->pt_data;

#ifdef USE_ORIG_INDEX_DATA
		if (use_orig_index) {
			r_orig_index[orig_index_index--] = orig_index_value;
			orig_index_value -= citer->orig_span;
		}
#endif
	}

#ifdef USE_ORIG_INDEX_DATA
	if (use_orig_index) {
		assert(orig_index_index == 0);
		assert(orig_index_value == 0 || index_last == 0);
		r_orig_index[orig_index_index] = index_last ? orig_index_value : 0;

	}
#endif

	/* flip tangent for first and last (we could leave at zero, but set to something useful) */

	/* first */
	array_iter -= array_chunk;
	memcpy(&array_iter[dims], handle_prev, sizeof(double) * 2 * dims);
	flip_vn_vnvn(&array_iter[0 * dims], &array_iter[1 * dims], &array_iter[2 * dims], dims);
	assert(array == array_iter);

	/* last */
	array_iter += array_flat_len - (3 * dims);
	flip_vn_vnvn(&array_iter[2 * dims], &array_iter[1 * dims], &array_iter[0 * dims], dims);

	return array;
}

static void cubic_list_clear(CubicList *clist)
{
	Cubic *cubic_next;
	for (Cubic *citer = clist->items; citer; citer = cubic_next) {
		cubic_next = citer->next;
		cubic_free(citer);
	}
	clist->items = NULL;
	clist->len  = 0;
}

/** \} */


/* -------------------------------------------------------------------- */

/** \name Cubic Evaluation
 * \{ */

static void cubic_calc_point(
        const Cubic *cubic, const double t, const uint dims,
        double r_v[])
{
	CUBIC_VARS_CONST(cubic, dims, p0, p1, p2, p3);
	const double s = 1.0 - t;

	for (uint j = 0; j < dims; j++) {
		const double p01 = (p0[j] * s) + (p1[j] * t);
		const double p12 = (p1[j] * s) + (p2[j] * t);
		const double p23 = (p2[j] * s) + (p3[j] * t);
		r_v[j] = ((((p01 * s) + (p12 * t))) * s) +
		         ((((p12 * s) + (p23 * t))) * t);
	}
}

static void cubic_calc_speed(
        const Cubic *cubic, const double t, const uint dims,
        double r_v[])
{
	CUBIC_VARS_CONST(cubic, dims, p0, p1, p2, p3);
	const double s = 1.0 - t;
	for (uint j = 0; j < dims; j++) {
		r_v[j] =  3.0 * ((p1[j] - p0[j]) * s * s + 2.0 *
		                 (p2[j] - p0[j]) * s * t +
		                 (p3[j] - p2[j]) * t * t);
	}
}

static void cubic_calc_acceleration(
        const Cubic *cubic, const double t, const uint dims,
        double r_v[])
{
	CUBIC_VARS_CONST(cubic, dims, p0, p1, p2, p3);
	const double s = 1.0 - t;
	for (uint j = 0; j < dims; j++) {
		r_v[j] = 6.0 * ((p2[j] - 2.0 * p1[j] + p0[j]) * s +
		                (p3[j] - 2.0 * p2[j] + p1[j]) * t);
	}
}

/**
 * Returns a 'measure' of the maximum distance (squared) of the points specified
 * by points_offset from the corresponding cubic(u[]) points.
 */
static double cubic_calc_error(
        const Cubic *cubic,
        const double *points_offset,
        const uint points_offset_len,
        const double *u,
        const uint dims,

        uint *r_error_index)
{
	double error_max_sq = 0.0;
	uint   error_index = 0;

	const double *pt_real = points_offset + dims;
#ifdef USE_VLA
	double        pt_eval[dims];
#else
	double       *pt_eval = alloca(sizeof(double) * dims);
#endif

	for (uint i = 1; i < points_offset_len - 1; i++, pt_real += dims) {
		cubic_calc_point(cubic, u[i], dims, pt_eval);

		const double err_sq = len_squared_vnvn(pt_real, pt_eval, dims);
		if (err_sq >= error_max_sq) {
			error_max_sq = err_sq;
			error_index = i;
		}
	}

	*r_error_index = error_index;
	return error_max_sq;
}

#ifdef USE_OFFSET_FALLBACK
/**
 * A version #cubic_calc_error where we don't need the split-index and can exit early when over the limit.
 */
static double cubic_calc_error_simple(
        const Cubic *cubic,
        const double *points_offset,
        const uint points_offset_len,
        const double *u,
        const double error_threshold_sq,
        const uint dims)

{
	double error_max_sq = 0.0;

	const double *pt_real = points_offset + dims;
#ifdef USE_VLA
	double        pt_eval[dims];
#else
	double       *pt_eval = alloca(sizeof(double) * dims);
#endif

	for (uint i = 1; i < points_offset_len - 1; i++, pt_real += dims) {
		cubic_calc_point(cubic, u[i], dims, pt_eval);

		const double err_sq = len_squared_vnvn(pt_real, pt_eval, dims);
		if (err_sq >= error_threshold_sq) {
			return error_threshold_sq;
		}
		else if (err_sq >= error_max_sq) {
			error_max_sq = err_sq;
		}
	}

	return error_max_sq;
}
#endif

/**
 * Bezier multipliers
 */

static double B1(double u)
{
	double tmp = 1.0 - u;
	return 3.0 * u * tmp * tmp;
}

static double B2(double u)
{
	return 3.0 * u * u * (1.0 - u);
}

static double B0plusB1(double u)
{
    double tmp = 1.0 - u;
    return tmp * tmp * (1.0 + 2.0 * u);
}

static double B2plusB3(double u)
{
    return u * u * (3.0 - 2.0 * u);
}

static void points_calc_center_weighted(
        const double *points_offset,
        const uint    points_offset_len,
        const uint    dims,

        double r_center[])
{
	/*
	 * Calculate a center that compensates for point spacing.
	 */

	const double *pt_prev = &points_offset[(points_offset_len - 2) * dims];
	const double *pt_curr = pt_prev + dims;
	const double *pt_next = points_offset;

	double w_prev = len_vnvn(pt_prev, pt_curr, dims);

	zero_vn(r_center, dims);
	double w_tot = 0.0;

	for (uint i_next = 0; i_next < points_offset_len; i_next++) {
		const double w_next = len_vnvn(pt_curr, pt_next, dims);
		const double w = w_prev + w_next;
		w_tot += w;

		miadd_vn_vn_fl(r_center, pt_curr, w, dims);

		w_prev = w_next;

		pt_prev = pt_curr;
		pt_curr = pt_next;
		pt_next += dims;
	}

	if (w_tot != 0.0) {
		imul_vn_fl(r_center, 1.0 / w_tot, dims);
	}
}

#ifdef USE_CIRCULAR_FALLBACK

/**
 * Return a scale value, used to calculate how much the curve handles should be increased,
 *
 * This works by placing each end-point on an imaginary circle,
 * the placement on the circle is based on the tangent vectors,
 * where larger differences in tangent angle cover a larger part of the circle.
 *
 * Return the scale representing how much larger the distance around the circle is.
 */
static double points_calc_circumference_factor(
        const double  tan_l[],
        const double  tan_r[],
        const uint dims)
{
	const double dot = dot_vnvn(tan_l, tan_r, dims);
	const double len_tangent = dot < 0.0 ? len_vnvn(tan_l, tan_r, dims) : len_negated_vnvn(tan_l, tan_r, dims);
	if (len_tangent > DBL_EPSILON) {
		/* only clamp to avoid precision error */
		double angle = acos(max(-fabs(dot), -1.0));
		/* Angle may be less than the length when the tangents define >180 degrees of the circle,
		 * (tangents that point away from each other).
		 * We could try support this but will likely cause extreme >1 scales which could cause other issues. */
		// assert(angle >= len_tangent);
		double factor = (angle / len_tangent);
		assert(factor < (M_PI / 2) + (DBL_EPSILON * 10));
		return factor;
	}
	else {
		/* tangents are exactly aligned (think two opposite sides of a circle). */
		return (M_PI / 2);
	}
}

/**
 * Return the value which the distance between points will need to be scaled by,
 * to define a handle, given both points are on a perfect circle.
 *
 * \note the return value will need to be multiplied by 1.3... for correct results.
 */
static double points_calc_circle_tangent_factor(
        const double  tan_l[],
        const double  tan_r[],
        const uint dims)
{
	const double eps = 1e-8;
	const double tan_dot = dot_vnvn(tan_l, tan_r, dims);
	if (tan_dot > 1.0 - eps) {
		/* no angle difference (use fallback, length wont make any difference) */
		return (1.0 / 3.0) * 0.75;
	}
	else if (tan_dot < -1.0 + eps) {
		/* parallel tangents (half-circle) */
		return (1.0 / 2.0);
	}
	else {
		/* non-aligned tangents, calculate handle length */
		const double angle = acos(tan_dot) / 2.0;

		/* could also use 'angle_sin = len_vnvn(tan_l, tan_r, dims) / 2.0' */
		const double angle_sin = sin(angle);
		const double angle_cos = cos(angle);
		return ((1.0 - angle_cos) / (angle_sin * 2.0)) / angle_sin;
	}
}

/**
 * Calculate the scale the handles, which serves as a best-guess
 * used as a fallback when the least-square solution fails.
 */
static double points_calc_cubic_scale(
        const double v_l[], const double v_r[],
        const double  tan_l[],
        const double  tan_r[],
        const double coords_length, uint dims)
{
	const double len_direct = len_vnvn(v_l, v_r, dims);
	const double len_circle_factor = points_calc_circle_tangent_factor(tan_l, tan_r, dims);

	/* if this curve is a circle, this value doesn't need modification */
	const double len_circle_handle = (len_direct * (len_circle_factor / 0.75));

	/* scale by the difference from the circumference distance */
	const double len_circle = len_direct * points_calc_circumference_factor(tan_l, tan_r, dims);
	double scale_handle = (coords_length / len_circle);

	/* Could investigate an accurate calculation here,
	 * though this gives close results */
	scale_handle = ((scale_handle - 1.0) * 1.75) + 1.0;

	return len_circle_handle * scale_handle;
}

static void cubic_from_points_fallback(
        const double *points_offset,
        const uint    points_offset_len,
        const double  tan_l[],
        const double  tan_r[],
        const uint dims,

        Cubic *r_cubic)
{
	const double *p0 = &points_offset[0];
	const double *p3 = &points_offset[(points_offset_len - 1) * dims];

	double alpha = len_vnvn(p0, p3, dims) / 3.0;

	double *p1 = CUBIC_PT(r_cubic, 1, dims);
	double *p2 = CUBIC_PT(r_cubic, 2, dims);

	copy_vnvn(CUBIC_PT(r_cubic, 0, dims), p0, dims);
	copy_vnvn(CUBIC_PT(r_cubic, 3, dims), p3, dims);

#ifdef USE_ORIG_INDEX_DATA
	r_cubic->orig_span = (points_offset_len - 1);
#endif

	/* p1 = p0 - (tan_l * alpha);
	 * p2 = p3 + (tan_r * alpha);
	 */
	msub_vn_vnvn_fl(p1, p0, tan_l, alpha, dims);
	madd_vn_vnvn_fl(p2, p3, tan_r, alpha, dims);
}
#endif  /* USE_CIRCULAR_FALLBACK */


#ifdef USE_OFFSET_FALLBACK

static void cubic_from_points_offset_fallback(
        const double *points_offset,
        const uint    points_offset_len,
        const double  tan_l[],
        const double  tan_r[],
        const uint dims,

        Cubic *r_cubic)
{
	const double *p0 = &points_offset[0];
	const double *p3 = &points_offset[(points_offset_len - 1) * dims];

#ifdef USE_VLA
	double dir_unit[dims];
	double a[2][dims];
	double tmp[dims];
#else
	double *dir_unit = alloca(sizeof(double) * dims);
	double *a[2] = {
	    alloca(sizeof(double) * dims),
	    alloca(sizeof(double) * dims),
	};
	double *tmp = alloca(sizeof(double) * dims);
#endif

	const double dir_dist = normalize_vn_vnvn(dir_unit, p3, p0, dims);
	project_plane_vn_vnvn_normalized(a[0], tan_l, dir_unit, dims);
	project_plane_vn_vnvn_normalized(a[1], tan_r, dir_unit, dims);

	/* only for better accuracy, not essential */
	normalize_vn(a[0], dims);
	normalize_vn(a[1], dims);

	mul_vnvn_fl(a[1], a[1], -1, dims);

	double dists[2] = {0, 0};

	const double *pt = &points_offset[dims];
	for (uint i = 1; i < points_offset_len - 1; i++, pt += dims) {
		for (uint k = 0; k < 2; k++) {
			sub_vn_vnvn(tmp, p0, pt, dims);
			project_vn_vnvn_normalized(tmp, tmp, a[k], dims);
			dists[k] = max(dists[k], dot_vnvn(tmp, a[k], dims));
		}
	}

	double alpha_l = (dists[0] / 0.75) / fabs(dot_vnvn(tan_l, a[0], dims));
	double alpha_r = (dists[1] / 0.75) / fabs(dot_vnvn(tan_r, a[1], dims));

	if (!(alpha_l > 0.0)) {
		alpha_l = dir_dist / 3.0;
	}
	if (!(alpha_r > 0.0)) {
		alpha_r = dir_dist / 3.0;
	}

	double *p1 = CUBIC_PT(r_cubic, 1, dims);
	double *p2 = CUBIC_PT(r_cubic, 2, dims);

	copy_vnvn(CUBIC_PT(r_cubic, 0, dims), p0, dims);
	copy_vnvn(CUBIC_PT(r_cubic, 3, dims), p3, dims);

#ifdef USE_ORIG_INDEX_DATA
	r_cubic->orig_span = (points_offset_len - 1);
#endif

	/* p1 = p0 - (tan_l * alpha_l);
	 * p2 = p3 + (tan_r * alpha_r);
	 */
	msub_vn_vnvn_fl(p1, p0, tan_l, alpha_l, dims);
	madd_vn_vnvn_fl(p2, p3, tan_r, alpha_r, dims);
}

#endif  /* USE_OFFSET_FALLBACK */


/**
 * Use least-squares method to find Bezier control points for region.
 */
static void cubic_from_points(
        const double *points_offset,
        const uint    points_offset_len,
#ifdef USE_CIRCULAR_FALLBACK
        const double  points_offset_coords_length,
#endif
        const double *u_prime,
        const double  tan_l[],
        const double  tan_r[],
        const uint dims,

        Cubic *r_cubic)
{

	const double *p0 = &points_offset[0];
	const double *p3 = &points_offset[(points_offset_len - 1) * dims];

	/* Point Pairs */
	double alpha_l, alpha_r;
#ifdef USE_VLA
	double a[2][dims];
#else
	double *a[2] = {
	    alloca(sizeof(double) * dims),
	    alloca(sizeof(double) * dims),
	};
#endif

	{
		double x[2] = {0.0}, c[2][2] = {{0.0}};
		const double *pt = points_offset;

		for (uint i = 0; i < points_offset_len; i++, pt += dims) {
			mul_vnvn_fl(a[0], tan_l, B1(u_prime[i]), dims);
			mul_vnvn_fl(a[1], tan_r, B2(u_prime[i]), dims);

			const double b0_plus_b1 = B0plusB1(u_prime[i]);
			const double b2_plus_b3 = B2plusB3(u_prime[i]);

			/* inline dot product */
			for (uint j = 0; j < dims; j++) {
				const double tmp = (pt[j] - (p0[j] * b0_plus_b1)) + (p3[j] * b2_plus_b3);

				x[0] += a[0][j] * tmp;
				x[1] += a[1][j] * tmp;

				c[0][0] += a[0][j] * a[0][j];
				c[0][1] += a[0][j] * a[1][j];
				c[1][1] += a[1][j] * a[1][j];
			}

			c[1][0] = c[0][1];
		}

		double det_C0_C1 = c[0][0] * c[1][1] - c[0][1] * c[1][0];
		double det_C_0X  = x[1]    * c[0][0] - x[0]    * c[0][1];
		double det_X_C1  = x[0]    * c[1][1] - x[1]    * c[0][1];

		if (is_almost_zero(det_C0_C1)) {
			det_C0_C1 = c[0][0] * c[1][1] * 10e-12;
		}

		/* may still divide-by-zero, check below will catch nan values */
		alpha_l = det_X_C1 / det_C0_C1;
		alpha_r = det_C_0X / det_C0_C1;
	}

	/*
	 * The problem that the stupid values for alpha dare not put
	 * only when we realize that the sign and wrong,
	 * but even if the values are too high.
	 * But how do you evaluate it?
	 *
	 * Meanwhile, we should ensure that these values are sometimes
	 * so only problems absurd of approximation and not for bugs in the code.
	 */

	bool use_clamp = true;

	/* flip check to catch nan values */
	if (!(alpha_l >= 0.0) ||
	    !(alpha_r >= 0.0))
	{
#ifdef USE_CIRCULAR_FALLBACK
		double alpha_test = points_calc_cubic_scale(p0, p3, tan_l, tan_r, points_offset_coords_length, dims);
		if (!isfinite(alpha_test)) {
			alpha_test = len_vnvn(p0, p3, dims) / 3.0;
		}
		alpha_l = alpha_r = alpha_test;
#else
		alpha_l = alpha_r = len_vnvn(p0, p3, dims) / 3.0;
#endif

		/* skip clamping when we're using default handles */
		use_clamp = false;
	}

	double *p1 = CUBIC_PT(r_cubic, 1, dims);
	double *p2 = CUBIC_PT(r_cubic, 2, dims);

	copy_vnvn(CUBIC_PT(r_cubic, 0, dims), p0, dims);
	copy_vnvn(CUBIC_PT(r_cubic, 3, dims), p3, dims);

#ifdef USE_ORIG_INDEX_DATA
	r_cubic->orig_span = (points_offset_len - 1);
#endif

	/* p1 = p0 - (tan_l * alpha_l);
	 * p2 = p3 + (tan_r * alpha_r);
	 */
	msub_vn_vnvn_fl(p1, p0, tan_l, alpha_l, dims);
	madd_vn_vnvn_fl(p2, p3, tan_r, alpha_r, dims);

	/* ------------------------------------
	 * Clamping (we could make it optional)
	 */
	if (use_clamp) {
#ifdef USE_VLA
		double center[dims];
#else
		double *center = alloca(sizeof(double) * dims);
#endif
		points_calc_center_weighted(points_offset, points_offset_len, dims, center);

		const double clamp_scale = 3.0;  /* clamp to 3x */
		double dist_sq_max = 0.0;

		{
			const double *pt = points_offset;
			for (uint i = 0; i < points_offset_len; i++, pt += dims) {
#if 0
				double dist_sq_test = sq(len_vnvn(center, pt, dims) * clamp_scale);
#else
				/* do inline */
				double dist_sq_test = 0.0;
				for (uint j = 0; j < dims; j++) {
					dist_sq_test += sq((pt[j] - center[j]) * clamp_scale);
				}
#endif
				dist_sq_max = max(dist_sq_max, dist_sq_test);
			}
		}

		double p1_dist_sq = len_squared_vnvn(center, p1, dims);
		double p2_dist_sq = len_squared_vnvn(center, p2, dims);

		if (p1_dist_sq > dist_sq_max ||
		    p2_dist_sq > dist_sq_max)
		{
#ifdef USE_CIRCULAR_FALLBACK
			double alpha_test = points_calc_cubic_scale(p0, p3, tan_l, tan_r, points_offset_coords_length, dims);
			if (!isfinite(alpha_test)) {
				alpha_test = len_vnvn(p0, p3, dims) / 3.0;
			}
			alpha_l = alpha_r = alpha_test;
#else
			alpha_l = alpha_r = len_vnvn(p0, p3, dims) / 3.0;
#endif

			/*
			 * p1 = p0 - (tan_l * alpha_l);
			 * p2 = p3 + (tan_r * alpha_r);
			 */
			for (uint j = 0; j < dims; j++) {
				p1[j] = p0[j] - (tan_l[j] * alpha_l);
				p2[j] = p3[j] + (tan_r[j] * alpha_r);
			}

			p1_dist_sq = len_squared_vnvn(center, p1, dims);
			p2_dist_sq = len_squared_vnvn(center, p2, dims);
		}

		/* clamp within the 3x radius */
		if (p1_dist_sq > dist_sq_max) {
			isub_vnvn(p1, center, dims);
			imul_vn_fl(p1, sqrt(dist_sq_max) / sqrt(p1_dist_sq), dims);
			iadd_vnvn(p1, center, dims);
		}
		if (p2_dist_sq > dist_sq_max) {
			isub_vnvn(p2, center, dims);
			imul_vn_fl(p2, sqrt(dist_sq_max) / sqrt(p2_dist_sq), dims);
			iadd_vnvn(p2, center, dims);
		}
	}
	/* end clamping */
}

#ifdef USE_LENGTH_CACHE
static void points_calc_coord_length_cache(
        const double *points_offset,
        const uint    points_offset_len,
        const uint    dims,

        double     *r_points_length_cache)
{
	const double *pt_prev = points_offset;
	const double *pt = pt_prev + dims;
	r_points_length_cache[0] = 0.0;
	for (uint i = 1; i < points_offset_len; i++) {
		r_points_length_cache[i] = len_vnvn(pt, pt_prev, dims);
		pt_prev = pt;
		pt += dims;
	}
}
#endif  /* USE_LENGTH_CACHE */

/**
 * \return the accumulated length of \a points_offset.
 */
static double points_calc_coord_length(
        const double *points_offset,
        const uint    points_offset_len,
        const uint    dims,
#ifdef USE_LENGTH_CACHE
        const double *points_length_cache,
#endif
        double *r_u)
{
	const double *pt_prev = points_offset;
	const double *pt = pt_prev + dims;
	r_u[0] = 0.0;
	for (uint i = 1, i_prev = 0; i < points_offset_len; i++) {
		double length;

#ifdef USE_LENGTH_CACHE
		length = points_length_cache[i];
		assert(len_vnvn(pt, pt_prev, dims) == points_length_cache[i]);
#else
		length = len_vnvn(pt, pt_prev, dims);
#endif

		r_u[i] = r_u[i_prev] + length;
		i_prev = i;
		pt_prev = pt;
		pt += dims;
	}
	assert(!is_almost_zero(r_u[points_offset_len - 1]));
	const double w = r_u[points_offset_len - 1];
	for (uint i = 1; i < points_offset_len; i++) {
		r_u[i] /= w;
	}
	return w;
}

/**
 * Use Newton-Raphson iteration to find better root.
 *
 * \param cubic: Current fitted curve.
 * \param p: Point to test against.
 * \param u: Parameter value for \a p.
 *
 * \note Return value may be `nan` caller must check for this.
 */
static double cubic_find_root(
        const Cubic *cubic,
        const double p[],
        const double u,
        const uint dims)
{
	/* Newton-Raphson Method. */
	/* all vectors */
#ifdef USE_VLA
	double q0_u[dims];
	double q1_u[dims];
	double q2_u[dims];
#else
	double *q0_u = alloca(sizeof(double) * dims);
	double *q1_u = alloca(sizeof(double) * dims);
	double *q2_u = alloca(sizeof(double) * dims);
#endif

	cubic_calc_point(cubic, u, dims, q0_u);
	cubic_calc_speed(cubic, u, dims, q1_u);
	cubic_calc_acceleration(cubic, u, dims, q2_u);

	/* may divide-by-zero, caller must check for that case */
	/* u - ((q0_u - p) * q1_u) / (q1_u.length_squared() + (q0_u - p) * q2_u) */
	isub_vnvn(q0_u, p, dims);
	return u - dot_vnvn(q0_u, q1_u, dims) /
	       (len_squared_vn(q1_u, dims) + dot_vnvn(q0_u, q2_u, dims));
}

static int compare_double_fn(const void *a_, const void *b_)
{
	const double *a = a_;
	const double *b = b_;
	if      (*a > *b) return  1;
	else if (*a < *b) return -1;
	else              return  0;
}

/**
 * Given set of points and their parameterization, try to find a better parameterization.
 */
static bool cubic_reparameterize(
        const Cubic *cubic,
        const double *points_offset,
        const uint    points_offset_len,
        const double *u,
        const uint    dims,

        double       *r_u_prime)
{
	/*
	 * Recalculate the values of u[] based on the Newton Raphson method
	 */

	const double *pt = points_offset;
	for (uint i = 0; i < points_offset_len; i++, pt += dims) {
		r_u_prime[i] = cubic_find_root(cubic, pt, u[i], dims);
		if (!isfinite(r_u_prime[i])) {
			return false;
		}
	}

	qsort(r_u_prime, points_offset_len, sizeof(double), compare_double_fn);

	if ((r_u_prime[0] < 0.0) ||
	    (r_u_prime[points_offset_len - 1] > 1.0))
	{
		return false;
	}

	assert(r_u_prime[0] >= 0.0);
	assert(r_u_prime[points_offset_len - 1] <= 1.0);
	return true;
}


static bool fit_cubic_to_points(
        const double *points_offset,
        const uint    points_offset_len,
#ifdef USE_LENGTH_CACHE
        const double *points_length_cache,
#endif
        const double  tan_l[],
        const double  tan_r[],
        const double  error_threshold_sq,
        const uint    dims,

        Cubic *r_cubic, double *r_error_max_sq, uint *r_split_index)
{
	const uint iteration_max = 4;

	if (points_offset_len == 2) {
		CUBIC_VARS(r_cubic, dims, p0, p1, p2, p3);

		copy_vnvn(p0, &points_offset[0 * dims], dims);
		copy_vnvn(p3, &points_offset[1 * dims], dims);

		const double dist = len_vnvn(p0, p3, dims) / 3.0;
		msub_vn_vnvn_fl(p1, p0, tan_l, dist, dims);
		madd_vn_vnvn_fl(p2, p3, tan_r, dist, dims);

#ifdef USE_ORIG_INDEX_DATA
		r_cubic->orig_span = 1;
#endif
		return true;
	}

	double *u = malloc(sizeof(double) * points_offset_len);

#ifdef USE_CIRCULAR_FALLBACK
	const double points_offset_coords_length  =
#endif
	points_calc_coord_length(
	        points_offset, points_offset_len, dims,
#ifdef USE_LENGTH_CACHE
	        points_length_cache,
#endif
	        u);

	double error_max_sq;
	uint split_index;

	/* Parameterize points, and attempt to fit curve */
	cubic_from_points(
	        points_offset, points_offset_len,
#ifdef USE_CIRCULAR_FALLBACK
	        points_offset_coords_length,
#endif
	        u, tan_l, tan_r, dims, r_cubic);

	/* Find max deviation of points to fitted curve */
	error_max_sq = cubic_calc_error(
	        r_cubic, points_offset, points_offset_len, u, dims,
	        &split_index);

	Cubic *cubic_test = alloca(cubic_alloc_size(dims));

	/* Run this so we use the non-circular calculation when the circular-fallback
	 * in 'cubic_from_points' failed to give a close enough result. */
#ifdef USE_CIRCULAR_FALLBACK
	if (!(error_max_sq < error_threshold_sq)) {
		/* Don't use the cubic calculated above, instead calculate a new fallback cubic,
		 * since this tends to give more balanced split_index along the curve.
		 * This is because the attempt to calcualte the cubic may contain spikes
		 * along the curve which may give a lop-sided maximum distance. */
		cubic_from_points_fallback(
		        points_offset, points_offset_len,
		        tan_l, tan_r, dims, cubic_test);
		const double error_max_sq_test = cubic_calc_error(
		        cubic_test, points_offset, points_offset_len, u, dims,
		        &split_index);

		/* intentionally use the newly calculated 'split_index',
		 * even if the 'error_max_sq_test' is worse. */
		if (error_max_sq > error_max_sq_test) {
			error_max_sq = error_max_sq_test;
			cubic_copy(r_cubic, cubic_test, dims);
		}
	}
#endif

	/* Test the offset fallback */
#ifdef USE_OFFSET_FALLBACK
	if (!(error_max_sq < error_threshold_sq)) {
		/* Using the offset from the curve to calculate cubic handle length may give better results
		 * try this as a second fallback. */
		cubic_from_points_offset_fallback(
		        points_offset, points_offset_len,
		        tan_l, tan_r, dims, cubic_test);
		const double error_max_sq_test = cubic_calc_error_simple(
		        cubic_test, points_offset, points_offset_len, u, error_max_sq, dims);

		if (error_max_sq > error_max_sq_test) {
			error_max_sq = error_max_sq_test;
			cubic_copy(r_cubic, cubic_test, dims);
		}
	}
#endif

	*r_error_max_sq = error_max_sq;
	*r_split_index  = split_index;

	if (!(error_max_sq < error_threshold_sq)) {
		cubic_copy(cubic_test, r_cubic, dims);

		/* If error not too large, try some reparameterization and iteration */
		double *u_prime = malloc(sizeof(double) * points_offset_len);
		for (uint iter = 0; iter < iteration_max; iter++) {
			if (!cubic_reparameterize(
			        cubic_test, points_offset, points_offset_len, u, dims, u_prime))
			{
				break;
			}

			cubic_from_points(
			        points_offset, points_offset_len,
#ifdef USE_CIRCULAR_FALLBACK
			        points_offset_coords_length,
#endif
			        u_prime, tan_l, tan_r, dims, cubic_test);

			const double error_max_sq_test = cubic_calc_error(
			        cubic_test, points_offset, points_offset_len, u_prime, dims,
			        &split_index);

			if (error_max_sq > error_max_sq_test) {
				error_max_sq = error_max_sq_test;
				cubic_copy(r_cubic, cubic_test, dims);
				*r_error_max_sq = error_max_sq;
				*r_split_index = split_index;
			}

			if (!(error_max_sq < error_threshold_sq)) {
				/* continue */
			}
			else {
				assert((error_max_sq < error_threshold_sq));
				free(u_prime);
				free(u);
				return true;
			}

			SWAP(double *, u, u_prime);
		}
		free(u_prime);
		free(u);

		return false;
	}
	else {
		free(u);
		return true;
	}
}

static void fit_cubic_to_points_recursive(
        const double *points_offset,
        const uint    points_offset_len,
#ifdef USE_LENGTH_CACHE
        const double *points_length_cache,
#endif
        const double  tan_l[],
        const double  tan_r[],
        const double  error_threshold_sq,
        const uint    calc_flag,
        const uint    dims,
        /* fill in the list */
        CubicList *clist)
{
	Cubic *cubic = cubic_alloc(dims);
	uint split_index;
	double error_max_sq;

	if (fit_cubic_to_points(
	        points_offset, points_offset_len,
#ifdef USE_LENGTH_CACHE
	        points_length_cache,
#endif
	        tan_l, tan_r,
	        (calc_flag & CURVE_FIT_CALC_HIGH_QUALIY) ? DBL_EPSILON : error_threshold_sq,
	        dims,
	        cubic, &error_max_sq, &split_index) ||
	    (error_max_sq < error_threshold_sq))
	{
		cubic_list_prepend(clist, cubic);
		return;
	}
	cubic_free(cubic);


	/* Fitting failed -- split at max error point and fit recursively */

	/* Check splinePoint is not an endpoint?
	 *
	 * This assert happens sometimes...
	 * Look into it but disable for now. Campbell! */

	// assert(split_index > 1)
#ifdef USE_VLA
	double tan_center[dims];
#else
	double *tan_center = alloca(sizeof(double) * dims);
#endif

	const double *pt_a = &points_offset[(split_index - 1) * dims];
	const double *pt_b = &points_offset[(split_index + 1) * dims];

	assert(split_index < points_offset_len);
	if (equals_vnvn(pt_a, pt_b, dims)) {
		pt_a += dims;
	}

	{
#ifdef USE_VLA
		double tan_center_a[dims];
		double tan_center_b[dims];
#else
		double *tan_center_a = alloca(sizeof(double) * dims);
		double *tan_center_b = alloca(sizeof(double) * dims);
#endif
		const double *pt   = &points_offset[split_index * dims];

		/* tan_center = ((pt_a - pt).normalized() + (pt - pt_b).normalized()).normalized() */
		normalize_vn_vnvn(tan_center_a, pt_a, pt, dims);
		normalize_vn_vnvn(tan_center_b, pt, pt_b, dims);
		add_vn_vnvn(tan_center, tan_center_a, tan_center_b, dims);
		normalize_vn(tan_center, dims);
	}

	fit_cubic_to_points_recursive(
	        points_offset, split_index + 1,
#ifdef USE_LENGTH_CACHE
	        points_length_cache,
#endif
	        tan_l, tan_center, error_threshold_sq, calc_flag, dims, clist);
	fit_cubic_to_points_recursive(
	        &points_offset[split_index * dims], points_offset_len - split_index,
#ifdef USE_LENGTH_CACHE
	        points_length_cache + split_index,
#endif
	        tan_center, tan_r, error_threshold_sq, calc_flag, dims, clist);

}

/** \} */


/* -------------------------------------------------------------------- */

/** \name External API for Curve-Fitting
 * \{ */

/**
 * Main function:
 *
 * Take an array of 3d points.
 * return the cubic splines
 */
int curve_fit_cubic_to_points_db(
        const double *points,
        const uint    points_len,
        const uint    dims,
        const double  error_threshold,
        const uint    calc_flag,
        const uint   *corners,
        uint          corners_len,

        double **r_cubic_array, uint *r_cubic_array_len,
        uint **r_cubic_orig_index,
        uint **r_corner_index_array, uint *r_corner_index_len)
{
	uint corners_buf[2];
	if (corners == NULL) {
		assert(corners_len == 0);
		corners_buf[0] = 0;
		corners_buf[1] = points_len - 1;
		corners = corners_buf;
		corners_len = 2;
	}

	CubicList clist = {0};
	clist.dims = dims;

#ifdef USE_VLA
	double tan_l[dims];
	double tan_r[dims];
#else
	double *tan_l = alloca(sizeof(double) * dims);
	double *tan_r = alloca(sizeof(double) * dims);
#endif

#ifdef USE_LENGTH_CACHE
	double *points_length_cache = NULL;
	uint    points_length_cache_len_alloc = 0;
#endif

	uint *corner_index_array = NULL;
	uint  corner_index = 0;
	if (r_corner_index_array && (corners != corners_buf)) {
		corner_index_array = malloc(sizeof(uint) * corners_len);
		corner_index_array[corner_index++] = corners[0];
	}

	const double error_threshold_sq = sq(error_threshold);

	for (uint i = 1; i < corners_len; i++) {
		const uint points_offset_len = corners[i] - corners[i - 1] + 1;
		const uint first_point = corners[i - 1];

		assert(points_offset_len >= 1);
		if (points_offset_len > 1) {
			const double *pt_l = &points[first_point * dims];
			const double *pt_r = &points[(first_point + points_offset_len - 1) * dims];
			const double *pt_l_next = pt_l + dims;
			const double *pt_r_prev = pt_r - dims;

			/* tan_l = (pt_l - pt_l_next).normalized()
			 * tan_r = (pt_r_prev - pt_r).normalized()
			 */
			normalize_vn_vnvn(tan_l, pt_l, pt_l_next, dims);
			normalize_vn_vnvn(tan_r, pt_r_prev, pt_r, dims);

#ifdef USE_LENGTH_CACHE
			if (points_length_cache_len_alloc < points_offset_len) {
				if (points_length_cache) {
					free(points_length_cache);
				}
				points_length_cache = malloc(sizeof(double) * points_offset_len);
			}
			points_calc_coord_length_cache(
			        &points[first_point * dims], points_offset_len, dims,
			        points_length_cache);
#endif

			fit_cubic_to_points_recursive(
			        &points[first_point * dims], points_offset_len,
#ifdef USE_LENGTH_CACHE
			        points_length_cache,
#endif
			        tan_l, tan_r, error_threshold_sq, calc_flag, dims, &clist);
		}
		else if (points_len == 1) {
			assert(points_offset_len == 1);
			assert(corners_len == 2);
			assert(corners[0] == 0);
			assert(corners[1] == 0);
			const double *pt = &points[0];
			Cubic *cubic = cubic_alloc(dims);
			cubic_init(cubic, pt, pt, pt, pt, dims);
			cubic_list_prepend(&clist, cubic);
		}

		if (corner_index_array) {
			corner_index_array[corner_index++] = clist.len;
		}
	}

#ifdef USE_LENGTH_CACHE
	if (points_length_cache) {
		free(points_length_cache);
	}
#endif

#ifdef USE_ORIG_INDEX_DATA
	uint *cubic_orig_index = NULL;
	if (r_cubic_orig_index) {
		cubic_orig_index = malloc(sizeof(uint) * (clist.len + 1));
	}
#else
	*r_cubic_orig_index = NULL;
#endif

	/* allocate a contiguous array and free the linked list */
	*r_cubic_array = cubic_list_as_array(
	        &clist
#ifdef USE_ORIG_INDEX_DATA
	        , corners[corners_len - 1], cubic_orig_index
#endif
	        );
	*r_cubic_array_len = clist.len + 1;

	cubic_list_clear(&clist);

#ifdef USE_ORIG_INDEX_DATA
	if (cubic_orig_index) {
		*r_cubic_orig_index = cubic_orig_index;
	}
#endif

	if (corner_index_array) {
		assert(corner_index == corners_len);
		*r_corner_index_array = corner_index_array;
		*r_corner_index_len = corner_index;
	}

	return 0;
}

/**
 * A version of #curve_fit_cubic_to_points_db to handle floats
 */
int curve_fit_cubic_to_points_fl(
        const float  *points,
        const uint    points_len,
        const uint    dims,
        const float   error_threshold,
        const uint    calc_flag,
        const uint   *corners,
        const uint    corners_len,

        float **r_cubic_array, uint *r_cubic_array_len,
        uint **r_cubic_orig_index,
        uint **r_corner_index_array, uint *r_corner_index_len)
{
	const uint points_flat_len = points_len * dims;
	double *points_db = malloc(sizeof(double) * points_flat_len);

	copy_vndb_vnfl(points_db, points, points_flat_len);

	double *cubic_array_db = NULL;
	float  *cubic_array_fl = NULL;
	uint    cubic_array_len = 0;

	int result = curve_fit_cubic_to_points_db(
	        points_db, points_len, dims, error_threshold, calc_flag, corners, corners_len,
	        &cubic_array_db, &cubic_array_len,
	        r_cubic_orig_index,
	        r_corner_index_array, r_corner_index_len);
	free(points_db);

	if (!result) {
		uint cubic_array_flat_len = cubic_array_len * 3 * dims;
		cubic_array_fl = malloc(sizeof(float) * cubic_array_flat_len);
		for (uint i = 0; i < cubic_array_flat_len; i++) {
			cubic_array_fl[i] = (float)cubic_array_db[i];
		}
		free(cubic_array_db);
	}

	*r_cubic_array = cubic_array_fl;
	*r_cubic_array_len = cubic_array_len;

	return result;
}

/**
 * Fit a single cubic to points.
 */
int curve_fit_cubic_to_points_single_db(
        const double *points,
        const uint    points_len,
        const double *points_length_cache,
        const uint    dims,
        const double  error_threshold,
        const double tan_l[],
        const double tan_r[],

        double  r_handle_l[],
        double  r_handle_r[],
        double *r_error_max_sq,
        uint   *r_error_index)
{
	Cubic *cubic = alloca(cubic_alloc_size(dims));

	/* in this instance theres no advantage in using length cache,
	 * since we're not recursively calculating values. */
#ifdef USE_LENGTH_CACHE
	double *points_length_cache_alloc = NULL;
	if (points_length_cache == NULL) {
		points_length_cache_alloc = malloc(sizeof(double) * points_len);
		points_calc_coord_length_cache(
		        points, points_len, dims,
		        points_length_cache_alloc);
		points_length_cache = points_length_cache_alloc;
	}
#endif

	fit_cubic_to_points(
	        points, points_len,
#ifdef USE_LENGTH_CACHE
	        points_length_cache,
#endif
	        tan_l, tan_r, error_threshold, dims,

	        cubic, r_error_max_sq, r_error_index);

#ifdef USE_LENGTH_CACHE
	if (points_length_cache_alloc) {
		free(points_length_cache_alloc);
	}
#endif

	copy_vnvn(r_handle_l, CUBIC_PT(cubic, 1, dims), dims);
	copy_vnvn(r_handle_r, CUBIC_PT(cubic, 2, dims), dims);

	return 0;
}

int curve_fit_cubic_to_points_single_fl(
        const float  *points,
        const uint    points_len,
        const float  *points_length_cache,
        const uint    dims,
        const float   error_threshold,
        const float   tan_l[],
        const float   tan_r[],

        float   r_handle_l[],
        float   r_handle_r[],
        float  *r_error_sq,
        uint   *r_error_index)
{
	const uint points_flat_len = points_len * dims;
	double *points_db = malloc(sizeof(double) * points_flat_len);
	double *points_length_cache_db = NULL;

	copy_vndb_vnfl(points_db, points, points_flat_len);

	if (points_length_cache) {
		points_length_cache_db = malloc(sizeof(double) * points_len);
		copy_vndb_vnfl(points_length_cache_db, points_length_cache, points_len);
	}

#ifdef USE_VLA
	double tan_l_db[dims];
	double tan_r_db[dims];
	double r_handle_l_db[dims];
	double r_handle_r_db[dims];
#else
	double *tan_l_db = alloca(sizeof(double) * dims);
	double *tan_r_db = alloca(sizeof(double) * dims);
	double *r_handle_l_db = alloca(sizeof(double) * dims);
	double *r_handle_r_db = alloca(sizeof(double) * dims);
#endif
	double r_error_sq_db;

	copy_vndb_vnfl(tan_l_db, tan_l, dims);
	copy_vndb_vnfl(tan_r_db, tan_r, dims);

	int result = curve_fit_cubic_to_points_single_db(
	        points_db, points_len, points_length_cache_db, dims,
	        (double)error_threshold,
	        tan_l_db, tan_r_db,
	        r_handle_l_db, r_handle_r_db,
	        &r_error_sq_db,
	        r_error_index);

	free(points_db);

	if (points_length_cache_db) {
		free(points_length_cache_db);
	}

	copy_vnfl_vndb(r_handle_l, r_handle_l_db, dims);
	copy_vnfl_vndb(r_handle_r, r_handle_r_db, dims);
	*r_error_sq = (float)r_error_sq_db;

	return result;
}

/** \} */
