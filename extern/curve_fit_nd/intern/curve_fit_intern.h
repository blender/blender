/*
 * Copyright (c) 2016, Campbell Barton.
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

#ifndef __CURVE_FIT_INTERN_H__
#define __CURVE_FIT_INTERN_H__

#ifndef __CURVE_FIT_UINT_DEFINED__
#define __CURVE_FIT_UINT_DEFINED__
typedef unsigned int uint;
#endif

/**
 * Internal header for shared declarations between curve_fit_cubic.c
 * and curve_fit_cubic_refit.c.
 */

/* Split point calculation functions (implemented in curve_fit_cubic.c) */

#define SPLIT_POINT_INVALID ((uint)-1)

uint split_point_find_sign_change(
        const double *points,
        const uint points_len,
        const uint index_l, const uint index_r,
        const uint dims);

uint split_point_find_max_distance(
        const double *points,
        const uint points_len,
        const uint index_l, const uint index_r,
        const uint dims);

uint split_point_find_max_on_axis(
        const double *points,
        const uint points_len,
        const uint index_l, const uint index_r,
        const double *axis,
        const uint dims);

uint split_point_find_inflection(
        const double *points,
        const uint points_len,
        const uint index_l, const uint index_r,
        const uint dims);

#endif  /* __CURVE_FIT_INTERN_H__ */
