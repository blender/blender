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


/** \file curve_fit_inline.h
 *  \ingroup curve_fit
 */

/** \name Simple Vector Math Lib
 * \{ */

#ifdef _MSC_VER
#  define MINLINE static __forceinline
#else
#  define MINLINE static inline
#endif

MINLINE double sq(const double d)
{
	return d * d;
}

#ifndef _MSC_VER
MINLINE double min(const double a, const double b)
{
	return b < a ? b : a;
}

MINLINE double max(const double a, const double b)
{
	return a < b ? b : a;
}
#endif

MINLINE void zero_vn(
        double v0[], const uint dims)
{
	for (uint j = 0; j < dims; j++) {
		v0[j] = 0.0;
	}
}

MINLINE void flip_vn_vnvn(
        double v_out[], const double v0[], const double v1[], const uint dims)
{
	for (uint j = 0; j < dims; j++) {
		v_out[j] = v0[j] + (v0[j] - v1[j]);
	}
}

MINLINE void copy_vnvn(
        double v0[], const double v1[], const uint dims)
{
	for (uint j = 0; j < dims; j++) {
		v0[j] = v1[j];
	}
}

MINLINE void copy_vnfl_vndb(
        float v0[], const double v1[], const uint dims)
{
	for (uint j = 0; j < dims; j++) {
		v0[j] = (float)v1[j];
	}
}

MINLINE void copy_vndb_vnfl(
        double v0[], const float v1[], const uint dims)
{
	for (uint j = 0; j < dims; j++) {
		v0[j] = (double)v1[j];
	}
}

MINLINE double dot_vnvn(
        const double v0[], const double v1[], const uint dims)
{
	double d = 0.0;
	for (uint j = 0; j < dims; j++) {
		d += v0[j] * v1[j];
	}
	return d;
}

MINLINE void add_vn_vnvn(
        double v_out[], const double v0[], const double v1[], const uint dims)
{
	for (uint j = 0; j < dims; j++) {
		v_out[j] = v0[j] + v1[j];
	}
}

MINLINE void sub_vn_vnvn(
        double v_out[], const double v0[], const double v1[], const uint dims)
{
	for (uint j = 0; j < dims; j++) {
		v_out[j] = v0[j] - v1[j];
	}
}

MINLINE void iadd_vnvn(
        double v0[], const double v1[], const uint dims)
{
	for (uint j = 0; j < dims; j++) {
		v0[j] += v1[j];
	}
}

MINLINE void isub_vnvn(
        double v0[], const double v1[], const uint dims)
{
	for (uint j = 0; j < dims; j++) {
		v0[j] -= v1[j];
	}
}

MINLINE void madd_vn_vnvn_fl(
        double v_out[],
        const double v0[], const double v1[],
        const double f, const uint dims)
{
	for (uint j = 0; j < dims; j++) {
		v_out[j] = v0[j] + v1[j] * f;
	}
}

MINLINE void msub_vn_vnvn_fl(
        double v_out[],
        const double v0[], const double v1[],
        const double f, const uint dims)
{
	for (uint j = 0; j < dims; j++) {
		v_out[j] = v0[j] - v1[j] * f;
	}
}

MINLINE void miadd_vn_vn_fl(
        double v_out[], const double v0[], double f, const uint dims)
{
	for (uint j = 0; j < dims; j++) {
		v_out[j] += v0[j] * f;
	}
}

#if 0
MINLINE void misub_vn_vn_fl(
        double v_out[], const double v0[], double f, const uint dims)
{
	for (uint j = 0; j < dims; j++) {
		v_out[j] -= v0[j] * f;
	}
}
#endif

MINLINE void mul_vnvn_fl(
        double v_out[],
        const double v0[], const double f, const uint dims)
{
	for (uint j = 0; j < dims; j++) {
		v_out[j] = v0[j] * f;
	}
}

MINLINE void imul_vn_fl(double v0[], const double f, const uint dims)
{
	for (uint j = 0; j < dims; j++) {
		v0[j] *= f;
	}
}


MINLINE double len_squared_vnvn(
        const double v0[], const double v1[], const uint dims)
{
	double d = 0.0;
	for (uint j = 0; j < dims; j++) {
		d += sq(v0[j] - v1[j]);
	}
	return d;
}

MINLINE double len_squared_vn(
        const double v0[], const uint dims)
{
	double d = 0.0;
	for (uint j = 0; j < dims; j++) {
		d += sq(v0[j]);
	}
	return d;
}

MINLINE double len_vnvn(
        const double v0[], const double v1[], const uint dims)
{
	return sqrt(len_squared_vnvn(v0, v1, dims));
}

MINLINE double len_vn(
        const double v0[], const uint dims)
{
	return sqrt(len_squared_vn(v0, dims));
}

/* special case, save us negating a copy, then getting the length */
MINLINE double len_squared_negated_vnvn(
        const double v0[], const double v1[], const uint dims)
{
	double d = 0.0;
	for (uint j = 0; j < dims; j++) {
		d += sq(v0[j] + v1[j]);
	}
	return d;
}

MINLINE double len_negated_vnvn(
        const double v0[], const double v1[], const uint dims)
{
	return sqrt(len_squared_negated_vnvn(v0, v1, dims));
}

MINLINE double normalize_vn(
        double v0[], const uint dims)
{
	double d = len_squared_vn(v0, dims);
	if (d != 0.0 && ((d = sqrt(d)) != 0.0)) {
		imul_vn_fl(v0, 1.0 / d, dims);
	}
	return d;
}

/* v_out = (v0 - v1).normalized() */
MINLINE double normalize_vn_vnvn(
        double v_out[],
        const double v0[], const double v1[], const uint dims)
{
	double d = 0.0;
	for (uint j = 0; j < dims; j++) {
		double a = v0[j] - v1[j];
		d += sq(a);
		v_out[j] = a;
	}
	if (d != 0.0 && ((d = sqrt(d)) != 0.0)) {
		imul_vn_fl(v_out, 1.0 / d, dims);
	}
	return d;
}

MINLINE bool is_almost_zero_ex(double val, double eps)
{
	return (-eps < val) && (val < eps);
}

MINLINE bool is_almost_zero(double val)
{
	return is_almost_zero_ex(val, 1e-8);
}

MINLINE bool equals_vnvn(
		const double v0[], const double v1[], const uint dims)
{
	for (uint j = 0; j < dims; j++) {
		if (v0[j] != v1[j]) {
			return false;
		}
	}
	return true;
}

MINLINE void project_vn_vnvn(
        double v_out[], const double p[], const double v_proj[], const uint dims)
{
	const double mul = dot_vnvn(p, v_proj, dims) / dot_vnvn(v_proj, v_proj, dims);
	mul_vnvn_fl(v_out, v_proj, mul, dims);
}

MINLINE void project_vn_vnvn_normalized(
        double v_out[], const double p[], const double v_proj[], const uint dims)
{
	const double mul = dot_vnvn(p, v_proj, dims);
	mul_vnvn_fl(v_out, v_proj, mul, dims);
}

MINLINE void project_plane_vn_vnvn_normalized(
        double v_out[], const double v[], const double v_plane[], const uint dims)
{
	assert(v != v_out);
	project_vn_vnvn_normalized(v_out, v, v_plane, dims);
	sub_vn_vnvn(v_out, v, v_out, dims);
}

/** \} */
