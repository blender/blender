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

#ifndef __CURVE_FIT_ND_H__
#define __CURVE_FIT_ND_H__

/** \file curve_fit_nd.h
 *  \ingroup curve_fit
 */


/* curve_fit_cubic.c */

/**
 * Takes a flat array of points and evaluates that to calculate a bezier spline.
 *
 * \param points, points_len: The array of points to calculate a cubics from.
 * \param dims: The number of dimensions for for each element in \a points.
 * \param error_threshold: the error threshold to allow for,
 * the curve will be within this distance from \a points.
 * \param corners, corners_len: indices for points which will not have aligned tangents (optional).
 * This can use the output of #curve_fit_corners_detect_db which has been included
 * to evaluate a line to detect corner indices.
 *
 * \param r_cubic_array, r_cubic_array_len: Resulting array of tangents and knots, formatted as follows:
 * ``r_cubic_array[r_cubic_array_len][3][dims]``,
 * where each point has 0 and 2 for the tangents and the middle index 1 for the knot.
 * The size of the *flat* array will be ``r_cubic_array_len * 3 * dims``.
 * \param r_corner_index_array, r_corner_index_len: Corner indices in in \a r_cubic_array (optional).
 * This allows you to access corners on the resulting curve.
 *
 * \returns zero on success, nonzero is reserved for error values.
 */
int curve_fit_cubic_to_points_db(
        const double       *points,
        const unsigned int  points_len,
        const unsigned int  dims,
        const double        error_threshold,
        const unsigned int  calc_flag,
        const unsigned int *corners,
        unsigned int        corners_len,

        double **r_cubic_array, unsigned int *r_cubic_array_len,
        unsigned int **r_cubic_orig_index,
        unsigned int **r_corner_index_array, unsigned int *r_corner_index_len);

int curve_fit_cubic_to_points_fl(
        const float        *points,
        const unsigned int  points_len,
        const unsigned int  dims,
        const float         error_threshold,
        const unsigned int  calc_flag,
        const unsigned int *corners,
        const unsigned int  corners_len,

        float **r_cubic_array, unsigned int *r_cubic_array_len,
        unsigned int **r_cubic_orig_index,
        unsigned int **r_corners_index_array, unsigned int *r_corners_index_len);

/**
 * Takes a flat array of points and evaluates that to calculate handle lengths.
 *
 * \param points, points_len: The array of points to calculate a cubics from.
 * \param dims: The number of dimensions for for each element in \a points.
 * \param points_length_cache: Optional pre-calculated lengths between points.
 * \param error_threshold: the error threshold to allow for,
 * \param tan_l, tan_r: Normalized tangents the handles will be aligned to.
 * Note that tangents must both point along the direction of the \a points,
 * so \a tan_l points in the same direction of the resulting handle,
 * where \a tan_r will point the opposite direction of its handle.
 *
 * \param r_handle_l, r_handle_r: Resulting calculated handles.
 * \param r_error_sq: The maximum distance  (squared) this curve diverges from \a points.
 */
int curve_fit_cubic_to_points_single_db(
        const double      *points,
        const unsigned int points_len,
        const double      *points_length_cache,
        const unsigned int dims,
        const double       error_threshold,
        const double       tan_l[],
        const double       tan_r[],

        double  r_handle_l[],
        double  r_handle_r[],
        double *r_error_sq,
        unsigned int *r_error_index);

int curve_fit_cubic_to_points_single_fl(
        const float       *points,
        const unsigned int points_len,
        const float       *points_length_cache,
        const unsigned int dims,
        const float        error_threshold,
        const float        tan_l[],
        const float        tan_r[],

        float   r_handle_l[],
        float   r_handle_r[],
        float  *r_error_sq,
        unsigned int *r_error_index);

enum {
	CURVE_FIT_CALC_HIGH_QUALIY          = (1 << 0),
	CURVE_FIT_CALC_CYCLIC               = (1 << 1),
};


/* curve_fit_cubic_refit.c */

int curve_fit_cubic_to_points_refit_db(
        const double         *points,
        const unsigned int    points_len,
        const unsigned int    dims,
        const double          error_threshold,
        const unsigned int    calc_flag,
        const unsigned int   *corners,
        const unsigned int    corners_len,
        const double          corner_angle,

        double **r_cubic_array, unsigned int *r_cubic_array_len,
        unsigned int   **r_cubic_orig_index,
        unsigned int   **r_corner_index_array, unsigned int *r_corner_index_len);

int curve_fit_cubic_to_points_refit_fl(
        const float          *points,
        const unsigned int    points_len,
        const unsigned int    dims,
        const float           error_threshold,
        const unsigned int    calc_flag,
        const unsigned int   *corners,
        unsigned int          corners_len,
        const float           corner_angle,

        float **r_cubic_array, unsigned int *r_cubic_array_len,
        unsigned int   **r_cubic_orig_index,
        unsigned int   **r_corner_index_array, unsigned int *r_corner_index_len);

/* curve_fit_corners_detect.c */

/**
 * A helper function that takes a line and outputs its corner indices.
 *
 * \param points, points_len: Curve to evaluate.
 * \param dims: The number of dimensions for for each element in \a points.
 * \param radius_min: Corners on the curve between points below this radius are ignored.
 * \param radius_max: Corners on the curve above this radius are ignored.
 * \param samples_max: Prevent testing corners beyond this many points
 * (prevents a large radius taking excessive time to compute).
 * \param angle_threshold: Angles above this value are considered corners
 * (higher value for fewer corners).
 *
 * \param r_corners, r_corners_len: Resulting array of corners.
 *
 * \returns zero on success, nonzero is reserved for error values.
 */
int curve_fit_corners_detect_db(
        const double      *points,
        const unsigned int points_len,
        const unsigned int dims,
        const double       radius_min,
        const double       radius_max,
        const unsigned int samples_max,
        const double       angle_threshold,

        unsigned int **r_corners,
        unsigned int  *r_corners_len);

int curve_fit_corners_detect_fl(
        const float       *points,
        const unsigned int points_len,
        const unsigned int dims,
        const float        radius_min,
        const float        radius_max,
        const unsigned int samples_max,
        const float        angle_threshold,

        unsigned int **r_corners,
        unsigned int  *r_corners_len);

#endif  /* __CURVE_FIT_ND_H__ */
