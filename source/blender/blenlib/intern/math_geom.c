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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: some of this file.
 *
 * ***** END GPL LICENSE BLOCK *****
 * */

/** \file blender/blenlib/intern/math_geom.c
 *  \ingroup bli
 */

#include "MEM_guardedalloc.h"

#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "BLI_strict_flags.h"

/********************************** Polygons *********************************/

void cent_tri_v3(float cent[3], const float v1[3], const float v2[3], const float v3[3])
{
	cent[0] = (v1[0] + v2[0] + v3[0]) / 3.0f;
	cent[1] = (v1[1] + v2[1] + v3[1]) / 3.0f;
	cent[2] = (v1[2] + v2[2] + v3[2]) / 3.0f;
}

void cent_quad_v3(float cent[3], const float v1[3], const float v2[3], const float v3[3], const float v4[3])
{
	cent[0] = 0.25f * (v1[0] + v2[0] + v3[0] + v4[0]);
	cent[1] = 0.25f * (v1[1] + v2[1] + v3[1] + v4[1]);
	cent[2] = 0.25f * (v1[2] + v2[2] + v3[2] + v4[2]);
}

float normal_tri_v3(float n[3], const float v1[3], const float v2[3], const float v3[3])
{
	float n1[3], n2[3];

	n1[0] = v1[0] - v2[0];
	n2[0] = v2[0] - v3[0];
	n1[1] = v1[1] - v2[1];
	n2[1] = v2[1] - v3[1];
	n1[2] = v1[2] - v2[2];
	n2[2] = v2[2] - v3[2];
	n[0] = n1[1] * n2[2] - n1[2] * n2[1];
	n[1] = n1[2] * n2[0] - n1[0] * n2[2];
	n[2] = n1[0] * n2[1] - n1[1] * n2[0];

	return normalize_v3(n);
}

float normal_quad_v3(float n[3], const float v1[3], const float v2[3], const float v3[3], const float v4[3])
{
	/* real cross! */
	float n1[3], n2[3];

	n1[0] = v1[0] - v3[0];
	n1[1] = v1[1] - v3[1];
	n1[2] = v1[2] - v3[2];

	n2[0] = v2[0] - v4[0];
	n2[1] = v2[1] - v4[1];
	n2[2] = v2[2] - v4[2];

	n[0] = n1[1] * n2[2] - n1[2] * n2[1];
	n[1] = n1[2] * n2[0] - n1[0] * n2[2];
	n[2] = n1[0] * n2[1] - n1[1] * n2[0];

	return normalize_v3(n);
}

/**
 * Computes the normal of a planar
 * polygon See Graphics Gems for
 * computing newell normal.
 */
float normal_poly_v3(float n[3], const float verts[][3], unsigned int nr)
{
	const float *v_prev = verts[nr - 1];
	const float *v_curr = verts[0];
	unsigned int i;

	zero_v3(n);

	/* Newell's Method */
	for (i = 0; i < nr; v_prev = v_curr, v_curr = verts[++i]) {
		add_newell_cross_v3_v3v3(n, v_prev, v_curr);
	}

	return normalize_v3(n);
}

/* only convex Quadrilaterals */
float area_quad_v3(const float v1[3], const float v2[3], const float v3[3], const float v4[3])
{
	float len, vec1[3], vec2[3], n[3];

	sub_v3_v3v3(vec1, v2, v1);
	sub_v3_v3v3(vec2, v4, v1);
	cross_v3_v3v3(n, vec1, vec2);
	len = len_v3(n);

	sub_v3_v3v3(vec1, v4, v3);
	sub_v3_v3v3(vec2, v2, v3);
	cross_v3_v3v3(n, vec1, vec2);
	len += len_v3(n);

	return (len / 2.0f);
}

/* Triangles */
float area_tri_v3(const float v1[3], const float v2[3], const float v3[3])
{
	float vec1[3], vec2[3], n[3];

	sub_v3_v3v3(vec1, v3, v2);
	sub_v3_v3v3(vec2, v1, v2);
	cross_v3_v3v3(n, vec1, vec2);

	return len_v3(n) / 2.0f;
}

float area_tri_signed_v3(const float v1[3], const float v2[3], const float v3[3], const float normal[3])
{
	float area, vec1[3], vec2[3], n[3];

	sub_v3_v3v3(vec1, v3, v2);
	sub_v3_v3v3(vec2, v1, v2);
	cross_v3_v3v3(n, vec1, vec2);
	area = len_v3(n) / 2.0f;

	/* negate area for flipped triangles */
	if (dot_v3v3(n, normal) < 0.0f)
		area = -area;

	return area;
}

float area_poly_v3(const float verts[][3], unsigned int nr)
{
	float n[3];
	return normal_poly_v3(n, verts, nr) * 0.5f;
}

float cross_poly_v2(const float verts[][2], unsigned int nr)
{
	unsigned int a;
	float cross;
	const float *co_curr, *co_prev;

	/* The Trapezium Area Rule */
	co_prev = verts[nr - 1];
	co_curr = verts[0];
	cross = 0.0f;
	for (a = 0; a < nr; a++) {
		cross += (co_curr[0] - co_prev[0]) * (co_curr[1] + co_prev[1]);
		co_prev = co_curr;
		co_curr += 2;
	}

	return cross;
}

float area_poly_v2(const float verts[][2], unsigned int nr)
{
	return fabsf(0.5f * cross_poly_v2(verts, nr));
}

float cotangent_tri_weight_v3(const float v1[3], const float v2[3], const float v3[3])
{
	float a[3], b[3], c[3], c_len;

	sub_v3_v3v3(a, v2, v1);
	sub_v3_v3v3(b, v3, v1);
	cross_v3_v3v3(c, a, b);

	c_len = len_v3(c);

	if (c_len > FLT_EPSILON) {
		return dot_v3v3(a, b) / c_len;
	}
	else {
		return 0.0f;
	}
}

/********************************* Planes **********************************/

/**
 * Calculate a plane from a point and a direction,
 * \note \a point_no isn't required to be normalized.
 */
void plane_from_point_normal_v3(float r_plane[4], const float plane_co[3], const float plane_no[3])
{
	copy_v3_v3(r_plane, plane_no);
	r_plane[3] = -dot_v3v3(r_plane, plane_co);
}

/**
 * Get a point and a normal from a plane.
 */
void plane_to_point_normal_v3(const float plane[4], float r_plane_co[3], float r_plane_no[3])
{
	const float length = normalize_v3_v3(r_plane_no, plane);
	madd_v3_v3v3fl(r_plane_co, r_plane_no, r_plane_no, (-plane[3] / length) - 1.0f);
}


/********************************* Volume **********************************/

/**
 * The volume from a tetrahedron, points can be in any order
 */
float volume_tetrahedron_v3(const float v1[3], const float v2[3], const float v3[3], const float v4[3])
{
	float m[3][3];
	sub_v3_v3v3(m[0], v1, v2);
	sub_v3_v3v3(m[1], v2, v3);
	sub_v3_v3v3(m[2], v3, v4);
	return fabsf(determinant_m3_array(m)) / 6.0f;
}

/**
 * The volume from a tetrahedron, normal pointing inside gives negative volume
 */
float volume_tetrahedron_signed_v3(const float v1[3], const float v2[3], const float v3[3], const float v4[3])
{
	float m[3][3];
	sub_v3_v3v3(m[0], v1, v2);
	sub_v3_v3v3(m[1], v2, v3);
	sub_v3_v3v3(m[2], v3, v4);
	return determinant_m3_array(m) / 6.0f;
}


/********************************* Distance **********************************/

/* distance p to line v1-v2
 * using Hesse formula, NO LINE PIECE! */
float dist_squared_to_line_v2(const float p[2], const float l1[2], const float l2[2])
{
	float a[2], deler;

	a[0] = l1[1] - l2[1];
	a[1] = l2[0] - l1[0];

	deler = len_squared_v2(a);

	if (deler != 0.0f) {
		float f = ((p[0] - l1[0]) * a[0] +
		           (p[1] - l1[1]) * a[1]);
		return (f * f) / deler;
	}
	else {
		return 0.0f;
	}
}
float dist_to_line_v2(const float p[2], const float l1[2], const float l2[2])
{
	float a[2], deler;

	a[0] = l1[1] - l2[1];
	a[1] = l2[0] - l1[0];

	deler = len_squared_v2(a);

	if (deler != 0.0f) {
		float f = ((p[0] - l1[0]) * a[0] +
		           (p[1] - l1[1]) * a[1]);
		return fabsf(f) / sqrtf(deler);
	}
	else {
		return 0.0f;
	}
}

/* distance p to line-piece v1-v2 */
float dist_squared_to_line_segment_v2(const float p[2], const float l1[2], const float l2[2])
{
	float lambda, rc[2], pt[2], len;

	rc[0] = l2[0] - l1[0];
	rc[1] = l2[1] - l1[1];
	len = rc[0] * rc[0] + rc[1] * rc[1];
	if (len == 0.0f) {
		rc[0] = p[0] - l1[0];
		rc[1] = p[1] - l1[1];
		return (rc[0] * rc[0] + rc[1] * rc[1]);
	}

	lambda = (rc[0] * (p[0] - l1[0]) + rc[1] * (p[1] - l1[1])) / len;
	if (lambda <= 0.0f) {
		pt[0] = l1[0];
		pt[1] = l1[1];
	}
	else if (lambda >= 1.0f) {
		pt[0] = l2[0];
		pt[1] = l2[1];
	}
	else {
		pt[0] = lambda * rc[0] + l1[0];
		pt[1] = lambda * rc[1] + l1[1];
	}

	rc[0] = pt[0] - p[0];
	rc[1] = pt[1] - p[1];
	return (rc[0] * rc[0] + rc[1] * rc[1]);
}

float dist_to_line_segment_v2(const float p[2], const float l1[2], const float l2[2])
{
	return sqrtf(dist_squared_to_line_segment_v2(p, l1, l2));
}

/* point closest to v1 on line v2-v3 in 2D */
void closest_to_line_segment_v2(float r_close[2], const float p[2], const float l1[2], const float l2[2])
{
	float lambda, cp[2];

	lambda = closest_to_line_v2(cp, p, l1, l2);

	if (lambda <= 0.0f)
		copy_v2_v2(r_close, l1);
	else if (lambda >= 1.0f)
		copy_v2_v2(r_close, l2);
	else
		copy_v2_v2(r_close, cp);
}

/* point closest to v1 on line v2-v3 in 3D */
void closest_to_line_segment_v3(float r_close[3], const float v1[3], const float v2[3], const float v3[3])
{
	float lambda, cp[3];

	lambda = closest_to_line_v3(cp, v1, v2, v3);

	if (lambda <= 0.0f)
		copy_v3_v3(r_close, v2);
	else if (lambda >= 1.0f)
		copy_v3_v3(r_close, v3);
	else
		copy_v3_v3(r_close, cp);
}

/**
 * Find the closest point on a plane.
 *
 * \param r_close  Return coordinate
 * \param plane  The plane to test against.
 * \param pt  The point to find the nearest of
 *
 * \note non-unit-length planes are supported.
 */
void closest_to_plane_v3(float r_close[3], const float plane[4], const float pt[3])
{
	const float len_sq = len_squared_v3(plane);
	const float side = plane_point_side_v3(plane, pt);
	madd_v3_v3v3fl(r_close, pt, plane, -side / len_sq);
}

float dist_signed_squared_to_plane_v3(const float pt[3], const float plane[4])
{
	const float len_sq = len_squared_v3(plane);
	const float side = plane_point_side_v3(plane, pt);
	const float fac = side / len_sq;
	return copysignf(len_sq * (fac * fac), side);
}
float dist_squared_to_plane_v3(const float pt[3], const float plane[4])
{
	const float len_sq = len_squared_v3(plane);
	const float side = plane_point_side_v3(plane, pt);
	const float fac = side / len_sq;
	/* only difference to code above - no 'copysignf' */
	return len_sq * (fac * fac);
}

/**
 * Return the signed distance from the point to the plane.
 */
float dist_signed_to_plane_v3(const float pt[3], const float plane[4])
{
	const float len_sq = len_squared_v3(plane);
	const float side = plane_point_side_v3(plane, pt);
	const float fac = side / len_sq;
	return sqrtf(len_sq) * fac;
}
float dist_to_plane_v3(const float pt[3], const float plane[4])
{
	return fabsf(dist_signed_to_plane_v3(pt, plane));
}

/* distance v1 to line-piece l1-l2 in 3D */
float dist_squared_to_line_segment_v3(const float p[3], const float l1[3], const float l2[3])
{
	float closest[3];

	closest_to_line_segment_v3(closest, p, l1, l2);

	return len_squared_v3v3(closest, p);
}

float dist_to_line_segment_v3(const float p[3], const float l1[3], const float l2[3])
{
	return sqrtf(dist_squared_to_line_segment_v3(p, l1, l2));
}

float dist_squared_to_line_v3(const float v1[3], const float l1[3], const float l2[3])
{
	float closest[3];

	closest_to_line_v3(closest, v1, l1, l2);

	return len_squared_v3v3(closest, v1);
}
float dist_to_line_v3(const float v1[3], const float l1[3], const float l2[3])
{
	return sqrtf(dist_squared_to_line_v3(v1, l1, l2));
}

/* Adapted from "Real-Time Collision Detection" by Christer Ericson,
 * published by Morgan Kaufmann Publishers, copyright 2005 Elsevier Inc.
 * 
 * Set 'r' to the point in triangle (a, b, c) closest to point 'p' */
void closest_on_tri_to_point_v3(float r[3], const float p[3],
                                const float a[3], const float b[3], const float c[3])
{
	float ab[3], ac[3], ap[3], d1, d2;
	float bp[3], d3, d4, vc, cp[3], d5, d6, vb, va;
	float denom, v, w;

	/* Check if P in vertex region outside A */
	sub_v3_v3v3(ab, b, a);
	sub_v3_v3v3(ac, c, a);
	sub_v3_v3v3(ap, p, a);
	d1 = dot_v3v3(ab, ap);
	d2 = dot_v3v3(ac, ap);
	if (d1 <= 0.0f && d2 <= 0.0f) {
		/* barycentric coordinates (1,0,0) */
		copy_v3_v3(r, a);
		return;
	}

	/* Check if P in vertex region outside B */
	sub_v3_v3v3(bp, p, b);
	d3 = dot_v3v3(ab, bp);
	d4 = dot_v3v3(ac, bp);
	if (d3 >= 0.0f && d4 <= d3) {
		/* barycentric coordinates (0,1,0) */
		copy_v3_v3(r, b);
		return;
	}
	/* Check if P in edge region of AB, if so return projection of P onto AB */
	vc = d1 * d4 - d3 * d2;
	if (vc <= 0.0f && d1 >= 0.0f && d3 <= 0.0f) {
		v = d1 / (d1 - d3);
		/* barycentric coordinates (1-v,v,0) */
		madd_v3_v3v3fl(r, a, ab, v);
		return;
	}
	/* Check if P in vertex region outside C */
	sub_v3_v3v3(cp, p, c);
	d5 = dot_v3v3(ab, cp);
	d6 = dot_v3v3(ac, cp);
	if (d6 >= 0.0f && d5 <= d6) {
		/* barycentric coordinates (0,0,1) */
		copy_v3_v3(r, c);
		return;
	}
	/* Check if P in edge region of AC, if so return projection of P onto AC */
	vb = d5 * d2 - d1 * d6;
	if (vb <= 0.0f && d2 >= 0.0f && d6 <= 0.0f) {
		w = d2 / (d2 - d6);
		/* barycentric coordinates (1-w,0,w) */
		madd_v3_v3v3fl(r, a, ac, w);
		return;
	}
	/* Check if P in edge region of BC, if so return projection of P onto BC */
	va = d3 * d6 - d5 * d4;
	if (va <= 0.0f && (d4 - d3) >= 0.0f && (d5 - d6) >= 0.0f) {
		w = (d4 - d3) / ((d4 - d3) + (d5 - d6));
		/* barycentric coordinates (0,1-w,w) */
		sub_v3_v3v3(r, c, b);
		mul_v3_fl(r, w);
		add_v3_v3(r, b);
		return;
	}

	/* P inside face region. Compute Q through its barycentric coordinates (u,v,w) */
	denom = 1.0f / (va + vb + vc);
	v = vb * denom;
	w = vc * denom;

	/* = u*a + v*b + w*c, u = va * denom = 1.0f - v - w */
	/* ac * w */
	mul_v3_fl(ac, w);
	/* a + ab * v */
	madd_v3_v3v3fl(r, a, ab, v);
	/* a + ab * v + ac * w */
	add_v3_v3(r, ac);
}

/******************************* Intersection ********************************/

/* intersect Line-Line, shorts */
int isect_line_line_v2_int(const int v1[2], const int v2[2], const int v3[2], const int v4[2])
{
	float div, lambda, mu;

	div = (float)((v2[0] - v1[0]) * (v4[1] - v3[1]) - (v2[1] - v1[1]) * (v4[0] - v3[0]));
	if (div == 0.0f) return ISECT_LINE_LINE_COLINEAR;

	lambda = (float)((v1[1] - v3[1]) * (v4[0] - v3[0]) - (v1[0] - v3[0]) * (v4[1] - v3[1])) / div;

	mu = (float)((v1[1] - v3[1]) * (v2[0] - v1[0]) - (v1[0] - v3[0]) * (v2[1] - v1[1])) / div;

	if (lambda >= 0.0f && lambda <= 1.0f && mu >= 0.0f && mu <= 1.0f) {
		if (lambda == 0.0f || lambda == 1.0f || mu == 0.0f || mu == 1.0f) return ISECT_LINE_LINE_EXACT;
		return ISECT_LINE_LINE_CROSS;
	}
	return ISECT_LINE_LINE_NONE;
}

/* intersect Line-Line, floats - gives intersection point */
int isect_line_line_v2_point(const float v1[2], const float v2[2], const float v3[2], const float v4[2], float vi[2])
{
	float div;

	div = (v2[0] - v1[0]) * (v4[1] - v3[1]) - (v2[1] - v1[1]) * (v4[0] - v3[0]);
	if (div == 0.0f) return ISECT_LINE_LINE_COLINEAR;

	vi[0] = ((v3[0] - v4[0]) * (v1[0] * v2[1] - v1[1] * v2[0]) - (v1[0] - v2[0]) * (v3[0] * v4[1] - v3[1] * v4[0])) / div;
	vi[1] = ((v3[1] - v4[1]) * (v1[0] * v2[1] - v1[1] * v2[0]) - (v1[1] - v2[1]) * (v3[0] * v4[1] - v3[1] * v4[0])) / div;

	return ISECT_LINE_LINE_CROSS;
}


/* intersect Line-Line, floats */
int isect_line_line_v2(const float v1[2], const float v2[2], const float v3[2], const float v4[2])
{
	float div, lambda, mu;

	div = (v2[0] - v1[0]) * (v4[1] - v3[1]) - (v2[1] - v1[1]) * (v4[0] - v3[0]);
	if (div == 0.0f) return ISECT_LINE_LINE_COLINEAR;

	lambda = ((float)(v1[1] - v3[1]) * (v4[0] - v3[0]) - (v1[0] - v3[0]) * (v4[1] - v3[1])) / div;

	mu = ((float)(v1[1] - v3[1]) * (v2[0] - v1[0]) - (v1[0] - v3[0]) * (v2[1] - v1[1])) / div;

	if (lambda >= 0.0f && lambda <= 1.0f && mu >= 0.0f && mu <= 1.0f) {
		if (lambda == 0.0f || lambda == 1.0f || mu == 0.0f || mu == 1.0f) return ISECT_LINE_LINE_EXACT;
		return ISECT_LINE_LINE_CROSS;
	}
	return ISECT_LINE_LINE_NONE;
}

/* get intersection point of two 2D segments and return intersection type:
 *  -1: collinear
 *   1: intersection
 */
int isect_seg_seg_v2_point(const float v1[2], const float v2[2], const float v3[2], const float v4[2], float vi[2])
{
	float a1, a2, b1, b2, c1, c2, d;
	float u, v;
	const float eps = 1e-6f;
	const float eps_sq = eps * eps;

	a1 = v2[0] - v1[0];
	b1 = v4[0] - v3[0];
	c1 = v1[0] - v4[0];

	a2 = v2[1] - v1[1];
	b2 = v4[1] - v3[1];
	c2 = v1[1] - v4[1];

	d = a1 * b2 - a2 * b1;

	if (d == 0) {
		if (a1 * c2 - a2 * c1 == 0.0f && b1 * c2 - b2 * c1 == 0.0f) { /* equal lines */
			float a[2], b[2], c[2];
			float u2;

			if (equals_v2v2(v1, v2)) {
				if (len_squared_v2v2(v3, v4) > eps_sq) {
					/* use non-point segment as basis */
					SWAP(const float *, v1, v3);
					SWAP(const float *, v2, v4);
				}
				else { /* both of segments are points */
					if (equals_v2v2(v1, v3)) { /* points are equal */
						copy_v2_v2(vi, v1);
						return 1;
					}

					/* two different points */
					return -1;
				}
			}

			sub_v2_v2v2(a, v3, v1);
			sub_v2_v2v2(b, v2, v1);
			sub_v2_v2v2(c, v2, v1);
			u = dot_v2v2(a, b) / dot_v2v2(c, c);

			sub_v2_v2v2(a, v4, v1);
			u2 = dot_v2v2(a, b) / dot_v2v2(c, c);

			if (u > u2) SWAP(float, u, u2);

			if (u > 1.0f + eps || u2 < -eps) return -1;  /* non-ovlerlapping segments */
			else if (max_ff(0.0f, u) == min_ff(1.0f, u2)) { /* one common point: can return result */
				interp_v2_v2v2(vi, v1, v2, max_ff(0, u));
				return 1;
			}
		}

		/* lines are collinear */
		return -1;
	}

	u = (c2 * b1 - b2 * c1) / d;
	v = (c1 * a2 - a1 * c2) / d;

	if (u >= -eps && u <= 1.0f + eps && v >= -eps && v <= 1.0f + eps) { /* intersection */
		interp_v2_v2v2(vi, v1, v2, u);
		return 1;
	}

	/* out of segment intersection */
	return -1;
}

bool isect_seg_seg_v2(const float v1[2], const float v2[2], const float v3[2], const float v4[2])
{
#define CCW(A, B, C) ((C[1] - A[1]) * (B[0] - A[0]) > (B[1]-A[1]) * (C[0]-A[0]))

	return CCW(v1, v3, v4) != CCW(v2, v3, v4) && CCW(v1, v2, v3) != CCW(v1, v2, v4);

#undef CCW
}

int isect_line_sphere_v3(const float l1[3], const float l2[3],
                         const float sp[3], const float r,
                         float r_p1[3], float r_p2[3])
{
	/* l1:         coordinates (point of line)
	 * l2:         coordinates (point of line)
	 * sp, r:      coordinates and radius (sphere)
	 * r_p1, r_p2: return intersection coordinates
	 */


	/* adapted for use in blender by Campbell Barton - 2011
	 *
	 * atelier iebele abel - 2001
	 * atelier@iebele.nl
	 * http://www.iebele.nl
	 *
	 * sphere_line_intersection function adapted from:
	 * http://astronomy.swin.edu.au/pbourke/geometry/sphereline
	 * Paul Bourke pbourke@swin.edu.au
	 */

	const float ldir[3] = {
		l2[0] - l1[0],
		l2[1] - l1[1],
		l2[2] - l1[2]
	};

	const float a = len_squared_v3(ldir);

	const float b = 2.0f *
	                (ldir[0] * (l1[0] - sp[0]) +
	                 ldir[1] * (l1[1] - sp[1]) +
	                 ldir[2] * (l1[2] - sp[2]));

	const float c =
	    len_squared_v3(sp) +
	    len_squared_v3(l1) -
	    (2.0f * dot_v3v3(sp, l1)) -
	    (r * r);

	const float i = b * b - 4.0f * a * c;

	float mu;

	if (i < 0.0f) {
		/* no intersections */
		return 0;
	}
	else if (i == 0.0f) {
		/* one intersection */
		mu = -b / (2.0f * a);
		madd_v3_v3v3fl(r_p1, l1, ldir, mu);
		return 1;
	}
	else if (i > 0.0f) {
		const float i_sqrt = sqrtf(i);  /* avoid calc twice */

		/* first intersection */
		mu = (-b + i_sqrt) / (2.0f * a);
		madd_v3_v3v3fl(r_p1, l1, ldir, mu);

		/* second intersection */
		mu = (-b - i_sqrt) / (2.0f * a);
		madd_v3_v3v3fl(r_p2, l1, ldir, mu);
		return 2;
	}
	else {
		/* math domain error - nan */
		return -1;
	}
}

/* keep in sync with isect_line_sphere_v3 */
int isect_line_sphere_v2(const float l1[2], const float l2[2],
                         const float sp[2], const float r,
                         float r_p1[2], float r_p2[2])
{
	const float ldir[2] = {l2[0] - l1[0],
	                       l2[1] - l1[1]};

	const float a = dot_v2v2(ldir, ldir);

	const float b = 2.0f *
	                (ldir[0] * (l1[0] - sp[0]) +
	                 ldir[1] * (l1[1] - sp[1]));

	const float c =
	    dot_v2v2(sp, sp) +
	    dot_v2v2(l1, l1) -
	    (2.0f * dot_v2v2(sp, l1)) -
	    (r * r);

	const float i = b * b - 4.0f * a * c;

	float mu;

	if (i < 0.0f) {
		/* no intersections */
		return 0;
	}
	else if (i == 0.0f) {
		/* one intersection */
		mu = -b / (2.0f * a);
		madd_v2_v2v2fl(r_p1, l1, ldir, mu);
		return 1;
	}
	else if (i > 0.0f) {
		const float i_sqrt = sqrtf(i);  /* avoid calc twice */

		/* first intersection */
		mu = (-b + i_sqrt) / (2.0f * a);
		madd_v2_v2v2fl(r_p1, l1, ldir, mu);

		/* second intersection */
		mu = (-b - i_sqrt) / (2.0f * a);
		madd_v2_v2v2fl(r_p2, l1, ldir, mu);
		return 2;
	}
	else {
		/* math domain error - nan */
		return -1;
	}
}

/* point in polygon (keep float and int versions in sync) */
#if 0
bool isect_point_poly_v2(const float pt[2], const float verts[][2], const unsigned int nr,
                         const bool use_holes)
{
	/* we do the angle rule, define that all added angles should be about zero or (2 * PI) */
	float angletot = 0.0;
	float fp1[2], fp2[2];
	unsigned int i;
	const float *p1, *p2;

	p1 = verts[nr - 1];

	/* first vector */
	fp1[0] = (float)(p1[0] - pt[0]);
	fp1[1] = (float)(p1[1] - pt[1]);

	for (i = 0; i < nr; i++) {
		p2 = verts[i];

		/* second vector */
		fp2[0] = (float)(p2[0] - pt[0]);
		fp2[1] = (float)(p2[1] - pt[1]);

		/* dot and angle and cross */
		angletot += angle_signed_v2v2(fp1, fp2);

		/* circulate */
		copy_v2_v2(fp1, fp2);
		p1 = p2;
	}

	angletot = fabsf(angletot);
	if (use_holes) {
		const float nested = floorf((angletot / (float)(M_PI * 2.0)) + 0.00001f);
		angletot -= nested * (float)(M_PI * 2.0);
		return (angletot > 4.0f) != ((int)nested % 2);
	}
	else {
		return (angletot > 4.0f);
	}
}
bool isect_point_poly_v2_int(const int pt[2], const int verts[][2], const unsigned int nr,
                             const bool use_holes)
{
	/* we do the angle rule, define that all added angles should be about zero or (2 * PI) */
	float angletot = 0.0;
	float fp1[2], fp2[2];
	unsigned int i;
	const int *p1, *p2;

	p1 = verts[nr - 1];

	/* first vector */
	fp1[0] = (float)(p1[0] - pt[0]);
	fp1[1] = (float)(p1[1] - pt[1]);

	for (i = 0; i < nr; i++) {
		p2 = verts[i];

		/* second vector */
		fp2[0] = (float)(p2[0] - pt[0]);
		fp2[1] = (float)(p2[1] - pt[1]);

		/* dot and angle and cross */
		angletot += angle_signed_v2v2(fp1, fp2);

		/* circulate */
		copy_v2_v2(fp1, fp2);
		p1 = p2;
	}

	angletot = fabsf(angletot);
	if (use_holes) {
		const float nested = floorf((angletot / (float)(M_PI * 2.0)) + 0.00001f);
		angletot -= nested * (float)(M_PI * 2.0);
		return (angletot > 4.0f) != ((int)nested % 2);
	}
	else {
		return (angletot > 4.0f);
	}
}

#else

bool isect_point_poly_v2(const float pt[2], const float verts[][2], const unsigned int nr,
                         const bool UNUSED(use_holes))
{
	unsigned int i, j;
	bool isect = false;
	for (i = 0, j = nr - 1; i < nr; j = i++) {
		if (((verts[i][1] > pt[1]) != (verts[j][1] > pt[1])) &&
		    (pt[0] < (verts[j][0] - verts[i][0]) * (pt[1] - verts[i][1]) / (verts[j][1] - verts[i][1]) + verts[i][0]))
		{
			isect = !isect;
		}
	}
	return isect;
}
bool isect_point_poly_v2_int(const int pt[2], const int verts[][2], const unsigned int nr,
                             const bool UNUSED(use_holes))
{
	unsigned int i, j;
	bool isect = false;
	for (i = 0, j = nr - 1; i < nr; j = i++) {
		if (((verts[i][1] > pt[1]) != (verts[j][1] > pt[1])) &&
		    (pt[0] < (verts[j][0] - verts[i][0]) * (pt[1] - verts[i][1]) / (verts[j][1] - verts[i][1]) + verts[i][0]))
		{
			isect = !isect;
		}
	}
	return isect;
}

#endif

/* point in tri */

/* only single direction */
bool isect_point_tri_v2_cw(const float pt[2], const float v1[2], const float v2[2], const float v3[2])
{
	if (line_point_side_v2(v1, v2, pt) >= 0.0f) {
		if (line_point_side_v2(v2, v3, pt) >= 0.0f) {
			if (line_point_side_v2(v3, v1, pt) >= 0.0f) {
				return 1;
			}
		}
	}

	return 0;
}

int isect_point_tri_v2(const float pt[2], const float v1[2], const float v2[2], const float v3[2])
{
	if (line_point_side_v2(v1, v2, pt) >= 0.0f) {
		if (line_point_side_v2(v2, v3, pt) >= 0.0f) {
			if (line_point_side_v2(v3, v1, pt) >= 0.0f) {
				return 1;
			}
		}
	}
	else {
		if (!(line_point_side_v2(v2, v3, pt) >= 0.0f)) {
			if (!(line_point_side_v2(v3, v1, pt) >= 0.0f)) {
				return -1;
			}
		}
	}

	return 0;
}

/* point in quad - only convex quads */
int isect_point_quad_v2(const float pt[2], const float v1[2], const float v2[2], const float v3[2], const float v4[2])
{
	if (line_point_side_v2(v1, v2, pt) >= 0.0f) {
		if (line_point_side_v2(v2, v3, pt) >= 0.0f) {
			if (line_point_side_v2(v3, v4, pt) >= 0.0f) {
				if (line_point_side_v2(v4, v1, pt) >= 0.0f) {
					return 1;
				}
			}
		}
	}
	else {
		if (!(line_point_side_v2(v2, v3, pt) >= 0.0f)) {
			if (!(line_point_side_v2(v3, v4, pt) >= 0.0f)) {
				if (!(line_point_side_v2(v4, v1, pt) >= 0.0f)) {
					return -1;
				}
			}
		}
	}

	return 0;
}

/* moved from effect.c
 * test if the line starting at p1 ending at p2 intersects the triangle v0..v2
 * return non zero if it does
 */
bool isect_line_tri_v3(const float p1[3], const float p2[3],
                       const float v0[3], const float v1[3], const float v2[3],
                       float *r_lambda, float r_uv[2])
{

	float p[3], s[3], d[3], e1[3], e2[3], q[3];
	float a, f, u, v;

	sub_v3_v3v3(e1, v1, v0);
	sub_v3_v3v3(e2, v2, v0);
	sub_v3_v3v3(d, p2, p1);

	cross_v3_v3v3(p, d, e2);
	a = dot_v3v3(e1, p);
	if ((a > -0.000001f) && (a < 0.000001f)) return 0;
	f = 1.0f / a;

	sub_v3_v3v3(s, p1, v0);

	u = f * dot_v3v3(s, p);
	if ((u < 0.0f) || (u > 1.0f)) return 0;

	cross_v3_v3v3(q, s, e1);

	v = f * dot_v3v3(d, q);
	if ((v < 0.0f) || ((u + v) > 1.0f)) return 0;

	*r_lambda = f * dot_v3v3(e2, q);
	if ((*r_lambda < 0.0f) || (*r_lambda > 1.0f)) return 0;

	if (r_uv) {
		r_uv[0] = u;
		r_uv[1] = v;
	}

	return 1;
}

/* like isect_line_tri_v3, but allows epsilon tolerance around triangle */
bool isect_line_tri_epsilon_v3(const float p1[3], const float p2[3],
                       const float v0[3], const float v1[3], const float v2[3],
                       float *r_lambda, float r_uv[2], const float epsilon)
{

	float p[3], s[3], d[3], e1[3], e2[3], q[3];
	float a, f, u, v;

	sub_v3_v3v3(e1, v1, v0);
	sub_v3_v3v3(e2, v2, v0);
	sub_v3_v3v3(d, p2, p1);

	cross_v3_v3v3(p, d, e2);
	a = dot_v3v3(e1, p);
	if ((a > -0.000001f) && (a < 0.000001f)) return 0;
	f = 1.0f / a;

	sub_v3_v3v3(s, p1, v0);

	u = f * dot_v3v3(s, p);
	if ((u < -epsilon) || (u > 1.0f + epsilon)) return 0;

	cross_v3_v3v3(q, s, e1);

	v = f * dot_v3v3(d, q);
	if ((v < -epsilon) || ((u + v) > 1.0f + epsilon)) return 0;

	*r_lambda = f * dot_v3v3(e2, q);
	if ((*r_lambda < 0.0f) || (*r_lambda > 1.0f)) return 0;

	if (r_uv) {
		r_uv[0] = u;
		r_uv[1] = v;
	}

	return 1;
}

/* moved from effect.c
 * test if the ray starting at p1 going in d direction intersects the triangle v0..v2
 * return non zero if it does
 */
bool isect_ray_tri_v3(const float p1[3], const float d[3],
                      const float v0[3], const float v1[3], const float v2[3],
                      float *r_lambda, float r_uv[2])
{
	float p[3], s[3], e1[3], e2[3], q[3];
	float a, f, u, v;

	sub_v3_v3v3(e1, v1, v0);
	sub_v3_v3v3(e2, v2, v0);

	cross_v3_v3v3(p, d, e2);
	a = dot_v3v3(e1, p);
	/* note: these values were 0.000001 in 2.4x but for projection snapping on
	 * a human head (1BU == 1m), subsurf level 2, this gave many errors - campbell */
	if ((a > -0.00000001f) && (a < 0.00000001f)) return 0;
	f = 1.0f / a;

	sub_v3_v3v3(s, p1, v0);

	u = f * dot_v3v3(s, p);
	if ((u < 0.0f) || (u > 1.0f)) return 0;

	cross_v3_v3v3(q, s, e1);

	v = f * dot_v3v3(d, q);
	if ((v < 0.0f) || ((u + v) > 1.0f)) return 0;

	*r_lambda = f * dot_v3v3(e2, q);
	if ((*r_lambda < 0.0f)) return 0;

	if (r_uv) {
		r_uv[0] = u;
		r_uv[1] = v;
	}

	return 1;
}

/**
 * if clip is nonzero, will only return true if lambda is >= 0.0
 * (i.e. intersection point is along positive d)
 */
bool isect_ray_plane_v3(const float p1[3], const float d[3],
                        const float v0[3], const float v1[3], const float v2[3],
                        float *r_lambda, const int clip)
{
	float p[3], s[3], e1[3], e2[3], q[3];
	float a, f;
	/* float  u, v; */ /*UNUSED*/

	sub_v3_v3v3(e1, v1, v0);
	sub_v3_v3v3(e2, v2, v0);

	cross_v3_v3v3(p, d, e2);
	a = dot_v3v3(e1, p);
	/* note: these values were 0.000001 in 2.4x but for projection snapping on
	 * a human head (1BU == 1m), subsurf level 2, this gave many errors - campbell */
	if ((a > -0.00000001f) && (a < 0.00000001f)) return 0;
	f = 1.0f / a;

	sub_v3_v3v3(s, p1, v0);

	/* u = f * dot_v3v3(s, p); */ /*UNUSED*/

	cross_v3_v3v3(q, s, e1);

	/* v = f * dot_v3v3(d, q); */ /*UNUSED*/

	*r_lambda = f * dot_v3v3(e2, q);
	if (clip && (*r_lambda < 0.0f)) return 0;

	return 1;
}

bool isect_ray_tri_epsilon_v3(const float p1[3], const float d[3],
                              const float v0[3], const float v1[3], const float v2[3],
                              float *r_lambda, float uv[2], const float epsilon)
{
	float p[3], s[3], e1[3], e2[3], q[3];
	float a, f, u, v;

	sub_v3_v3v3(e1, v1, v0);
	sub_v3_v3v3(e2, v2, v0);

	cross_v3_v3v3(p, d, e2);
	a = dot_v3v3(e1, p);
	if (a == 0.0f) return 0;
	f = 1.0f / a;

	sub_v3_v3v3(s, p1, v0);

	u = f * dot_v3v3(s, p);
	if ((u < -epsilon) || (u > 1.0f + epsilon)) return 0;

	cross_v3_v3v3(q, s, e1);

	v = f * dot_v3v3(d, q);
	if ((v < -epsilon) || ((u + v) > 1.0f + epsilon)) return 0;

	*r_lambda = f * dot_v3v3(e2, q);
	if ((*r_lambda < 0.0f)) return 0;

	if (uv) {
		uv[0] = u;
		uv[1] = v;
	}

	return 1;
}

bool isect_ray_tri_threshold_v3(const float p1[3], const float d[3],
                                const float v0[3], const float v1[3], const float v2[3],
                                float *r_lambda, float r_uv[2], const float threshold)
{
	float p[3], s[3], e1[3], e2[3], q[3];
	float a, f, u, v;
	float du, dv;

	sub_v3_v3v3(e1, v1, v0);
	sub_v3_v3v3(e2, v2, v0);

	cross_v3_v3v3(p, d, e2);
	a = dot_v3v3(e1, p);
	if ((a > -0.000001f) && (a < 0.000001f)) return 0;
	f = 1.0f / a;

	sub_v3_v3v3(s, p1, v0);

	cross_v3_v3v3(q, s, e1);
	*r_lambda = f * dot_v3v3(e2, q);
	if ((*r_lambda < 0.0f)) return 0;

	u = f * dot_v3v3(s, p);
	v = f * dot_v3v3(d, q);

	if (u > 0 && v > 0 && u + v > 1) {
		float t = (u + v - 1) / 2;
		du = u - t;
		dv = v - t;
	}
	else {
		if      (u < 0) du = u;
		else if (u > 1) du = u - 1;
		else            du = 0.0f;

		if      (v < 0) dv = v;
		else if (v > 1) dv = v - 1;
		else            dv = 0.0f;
	}

	mul_v3_fl(e1, du);
	mul_v3_fl(e2, dv);

	if (len_squared_v3(e1) + len_squared_v3(e2) > threshold * threshold) {
		return 0;
	}

	if (r_uv) {
		r_uv[0] = u;
		r_uv[1] = v;
	}

	return 1;
}

/**
 * Check if a point is behind all planes.
 */
bool isect_point_planes_v3(float (*planes)[4], int totplane, const float p[3])
{
	int i;

	for (i = 0; i < totplane; i++) {
		if (plane_point_side_v3(planes[i], p) > 0.0f) {
			return false;
		}
	}

	return true;
}

/**
 * Intersect line/plane.
 *
 * \param out The intersection point.
 * \param l1 The first point of the line.
 * \param l2 The second point of the line.
 * \param plane_co A point on the plane to intersect with.
 * \param plane_no The direction of the plane (does not need to be normalized).
 *
 * \note #line_plane_factor_v3() shares logic.
 */
bool isect_line_plane_v3(float out[3],
                         const float l1[3], const float l2[3],
                         const float plane_co[3], const float plane_no[3])
{
	float u[3], h[3];
	float dot;

	sub_v3_v3v3(u, l2, l1);
	sub_v3_v3v3(h, l1, plane_co);
	dot = dot_v3v3(plane_no, u);

	if (fabsf(dot) > FLT_EPSILON) {
		float lambda = -dot_v3v3(plane_no, h) / dot;
		madd_v3_v3v3fl(out, l1, u, lambda);
		return true;
	}
	else {
		/* The segment is parallel to plane */
		return false;
	}
}

/**
 * Intersect two planes, return a point on the intersection and a vector
 * that runs on the direction of the intersection.
 * Return error code is the same as 'isect_line_line_v3'.
 *
 * \param r_isect_co The resulting intersection point.
 * \param r_isect_no The resulting vector of the intersection.
 * \param plane_a_co The point on the first plane.
 * \param plane_a_no The normal of the first plane.
 * \param plane_b_co The point on the second plane.
 * \param plane_b_no The normal of the second plane.
 *
 * \note return normal isn't unit length
 */
bool isect_plane_plane_v3(float r_isect_co[3], float r_isect_no[3],
                          const float plane_a_co[3], const float plane_a_no[3],
                          const float plane_b_co[3], const float plane_b_no[3])
{
	float plane_a_co_other[3];
	cross_v3_v3v3(r_isect_no, plane_a_no, plane_b_no); /* direction is simply the cross product */
	cross_v3_v3v3(plane_a_co_other, plane_a_no, r_isect_no);
	add_v3_v3(plane_a_co_other, plane_a_co);
	return isect_line_plane_v3(r_isect_co, plane_a_co, plane_a_co_other, plane_b_co, plane_b_no);
}


/* Adapted from the paper by Kasper Fauerby */

/* "Improved Collision detection and Response" */
static bool getLowestRoot(const float a, const float b, const float c, const float maxR, float *root)
{
	/* Check if a solution exists */
	const float determinant = b * b - 4.0f * a * c;

	/* If determinant is negative it means no solutions. */
	if (determinant >= 0.0f) {
		/* calculate the two roots: (if determinant == 0 then
		 * x1==x2 but lets disregard that slight optimization) */
		const float sqrtD = sqrtf(determinant);
		float r1 = (-b - sqrtD) / (2.0f * a);
		float r2 = (-b + sqrtD) / (2.0f * a);

		/* Sort so x1 <= x2 */
		if (r1 > r2)
			SWAP(float, r1, r2);

		/* Get lowest root: */
		if (r1 > 0.0f && r1 < maxR) {
			*root = r1;
			return true;
		}

		/* It is possible that we want x2 - this can happen */
		/* if x1 < 0 */
		if (r2 > 0.0f && r2 < maxR) {
			*root = r2;
			return true;
		}
	}
	/* No (valid) solutions */
	return false;
}

bool isect_sweeping_sphere_tri_v3(const float p1[3], const float p2[3], const float radius,
                                  const float v0[3], const float v1[3], const float v2[3],
                                  float *r_lambda, float ipoint[3])
{
	float e1[3], e2[3], e3[3], point[3], vel[3], /*dist[3],*/ nor[3], temp[3], bv[3];
	float a, b, c, d, e, x, y, z, radius2 = radius * radius;
	float elen2, edotv, edotbv, nordotv;
	float newLambda;
	bool found_by_sweep = false;

	sub_v3_v3v3(e1, v1, v0);
	sub_v3_v3v3(e2, v2, v0);
	sub_v3_v3v3(vel, p2, p1);

	/*---test plane of tri---*/
	cross_v3_v3v3(nor, e1, e2);
	normalize_v3(nor);

	/* flip normal */
	if (dot_v3v3(nor, vel) > 0.0f) negate_v3(nor);

	a = dot_v3v3(p1, nor) - dot_v3v3(v0, nor);
	nordotv = dot_v3v3(nor, vel);

	if (fabsf(nordotv) < 0.000001f) {
		if (fabsf(a) >= radius) {
			return false;
		}
	}
	else {
		float t0 = (-a + radius) / nordotv;
		float t1 = (-a - radius) / nordotv;

		if (t0 > t1)
			SWAP(float, t0, t1);

		if (t0 > 1.0f || t1 < 0.0f) return 0;

		/* clamp to [0, 1] */
		CLAMP(t0, 0.0f, 1.0f);
		CLAMP(t1, 0.0f, 1.0f);

		/*---test inside of tri---*/
		/* plane intersection point */

		point[0] = p1[0] + vel[0] * t0 - nor[0] * radius;
		point[1] = p1[1] + vel[1] * t0 - nor[1] * radius;
		point[2] = p1[2] + vel[2] * t0 - nor[2] * radius;


		/* is the point in the tri? */
		a = dot_v3v3(e1, e1);
		b = dot_v3v3(e1, e2);
		c = dot_v3v3(e2, e2);

		sub_v3_v3v3(temp, point, v0);
		d = dot_v3v3(temp, e1);
		e = dot_v3v3(temp, e2);

		x = d * c - e * b;
		y = e * a - d * b;
		z = x + y - (a * c - b * b);


		if (z <= 0.0f && (x >= 0.0f && y >= 0.0f)) {
			//(((unsigned int)z)& ~(((unsigned int)x)|((unsigned int)y))) & 0x80000000) {
			*r_lambda = t0;
			copy_v3_v3(ipoint, point);
			return true;
		}
	}


	*r_lambda = 1.0f;

	/*---test points---*/
	a = dot_v3v3(vel, vel);

	/*v0*/
	sub_v3_v3v3(temp, p1, v0);
	b = 2.0f * dot_v3v3(vel, temp);
	c = dot_v3v3(temp, temp) - radius2;

	if (getLowestRoot(a, b, c, *r_lambda, r_lambda)) {
		copy_v3_v3(ipoint, v0);
		found_by_sweep = true;
	}

	/*v1*/
	sub_v3_v3v3(temp, p1, v1);
	b = 2.0f * dot_v3v3(vel, temp);
	c = dot_v3v3(temp, temp) - radius2;

	if (getLowestRoot(a, b, c, *r_lambda, r_lambda)) {
		copy_v3_v3(ipoint, v1);
		found_by_sweep = true;
	}

	/*v2*/
	sub_v3_v3v3(temp, p1, v2);
	b = 2.0f * dot_v3v3(vel, temp);
	c = dot_v3v3(temp, temp) - radius2;

	if (getLowestRoot(a, b, c, *r_lambda, r_lambda)) {
		copy_v3_v3(ipoint, v2);
		found_by_sweep = true;
	}

	/*---test edges---*/
	sub_v3_v3v3(e3, v2, v1);  /* wasnt yet calculated */


	/*e1*/
	sub_v3_v3v3(bv, v0, p1);

	elen2 = dot_v3v3(e1, e1);
	edotv = dot_v3v3(e1, vel);
	edotbv = dot_v3v3(e1, bv);

	a = elen2 * (-dot_v3v3(vel, vel)) + edotv * edotv;
	b = 2.0f * (elen2 * dot_v3v3(vel, bv) - edotv * edotbv);
	c = elen2 * (radius2 - dot_v3v3(bv, bv)) + edotbv * edotbv;

	if (getLowestRoot(a, b, c, *r_lambda, &newLambda)) {
		e = (edotv * newLambda - edotbv) / elen2;

		if (e >= 0.0f && e <= 1.0f) {
			*r_lambda = newLambda;
			copy_v3_v3(ipoint, e1);
			mul_v3_fl(ipoint, e);
			add_v3_v3(ipoint, v0);
			found_by_sweep = true;
		}
	}

	/*e2*/
	/*bv is same*/
	elen2 = dot_v3v3(e2, e2);
	edotv = dot_v3v3(e2, vel);
	edotbv = dot_v3v3(e2, bv);

	a = elen2 * (-dot_v3v3(vel, vel)) + edotv * edotv;
	b = 2.0f * (elen2 * dot_v3v3(vel, bv) - edotv * edotbv);
	c = elen2 * (radius2 - dot_v3v3(bv, bv)) + edotbv * edotbv;

	if (getLowestRoot(a, b, c, *r_lambda, &newLambda)) {
		e = (edotv * newLambda - edotbv) / elen2;

		if (e >= 0.0f && e <= 1.0f) {
			*r_lambda = newLambda;
			copy_v3_v3(ipoint, e2);
			mul_v3_fl(ipoint, e);
			add_v3_v3(ipoint, v0);
			found_by_sweep = true;
		}
	}

	/*e3*/
	/* sub_v3_v3v3(bv, v0, p1); */ /* UNUSED */
	/* elen2 = dot_v3v3(e1, e1); */ /* UNUSED */
	/* edotv = dot_v3v3(e1, vel); */ /* UNUSED */
	/* edotbv = dot_v3v3(e1, bv); */ /* UNUSED */

	sub_v3_v3v3(bv, v1, p1);
	elen2 = dot_v3v3(e3, e3);
	edotv = dot_v3v3(e3, vel);
	edotbv = dot_v3v3(e3, bv);

	a = elen2 * (-dot_v3v3(vel, vel)) + edotv * edotv;
	b = 2.0f * (elen2 * dot_v3v3(vel, bv) - edotv * edotbv);
	c = elen2 * (radius2 - dot_v3v3(bv, bv)) + edotbv * edotbv;

	if (getLowestRoot(a, b, c, *r_lambda, &newLambda)) {
		e = (edotv * newLambda - edotbv) / elen2;

		if (e >= 0.0f && e <= 1.0f) {
			*r_lambda = newLambda;
			copy_v3_v3(ipoint, e3);
			mul_v3_fl(ipoint, e);
			add_v3_v3(ipoint, v1);
			found_by_sweep = true;
		}
	}


	return found_by_sweep;
}

bool isect_axial_line_tri_v3(const int axis, const float p1[3], const float p2[3],
                             const float v0[3], const float v1[3], const float v2[3], float *r_lambda)
{
	float p[3], e1[3], e2[3];
	float u, v, f;
	int a0 = axis, a1 = (axis + 1) % 3, a2 = (axis + 2) % 3;

#if 0
	return isect_line_tri_v3(p1, p2, v0, v1, v2, lambda);

	/* first a simple bounding box test */
	if (min_fff(v0[a1], v1[a1], v2[a1]) > p1[a1]) return false;
	if (min_fff(v0[a2], v1[a2], v2[a2]) > p1[a2]) return false;
	if (max_fff(v0[a1], v1[a1], v2[a1]) < p1[a1]) return false;
	if (max_fff(v0[a2], v1[a2], v2[a2]) < p1[a2]) return false;

	/* then a full intersection test */
#endif

	sub_v3_v3v3(e1, v1, v0);
	sub_v3_v3v3(e2, v2, v0);
	sub_v3_v3v3(p, v0, p1);

	f = (e2[a1] * e1[a2] - e2[a2] * e1[a1]);
	if ((f > -0.000001f) && (f < 0.000001f)) return false;

	v = (p[a2] * e1[a1] - p[a1] * e1[a2]) / f;
	if ((v < 0.0f) || (v > 1.0f)) return 0;

	f = e1[a1];
	if ((f > -0.000001f) && (f < 0.000001f)) {
		f = e1[a2];
		if ((f > -0.000001f) && (f < 0.000001f)) return false;
		u = (-p[a2] - v * e2[a2]) / f;
	}
	else
		u = (-p[a1] - v * e2[a1]) / f;

	if ((u < 0.0f) || ((u + v) > 1.0f)) return 0;

	*r_lambda = (p[a0] + u * e1[a0] + v * e2[a0]) / (p2[a0] - p1[a0]);

	if ((*r_lambda < 0.0f) || (*r_lambda > 1.0f)) return false;

	return true;
}

/**
 * \return The number of point of interests
 * 0 - lines are colinear
 * 1 - lines are coplanar, i1 is set to intersection
 * 2 - i1 and i2 are the nearest points on line 1 (v1, v2) and line 2 (v3, v4) respectively
 */
int isect_line_line_epsilon_v3(
        const float v1[3], const float v2[3],
        const float v3[3], const float v4[3], float i1[3], float i2[3],
        const float epsilon)
{
	float a[3], b[3], c[3], ab[3], cb[3], dir1[3], dir2[3];
	float d, div;

	sub_v3_v3v3(c, v3, v1);
	sub_v3_v3v3(a, v2, v1);
	sub_v3_v3v3(b, v4, v3);

	normalize_v3_v3(dir1, a);
	normalize_v3_v3(dir2, b);
	d = dot_v3v3(dir1, dir2);
	if (d == 1.0f || d == -1.0f) {
		/* colinear */
		return 0;
	}

	cross_v3_v3v3(ab, a, b);
	d = dot_v3v3(c, ab);
	div = dot_v3v3(ab, ab);

	/* test zero length line */
	if (UNLIKELY(div == 0.0f)) {
		return 0;
	}
	/* test if the two lines are coplanar */
	else if (UNLIKELY(fabsf(d) <= epsilon)) {
		cross_v3_v3v3(cb, c, b);

		mul_v3_fl(a, dot_v3v3(cb, ab) / div);
		add_v3_v3v3(i1, v1, a);
		copy_v3_v3(i2, i1);

		return 1; /* one intersection only */
	}
	/* if not */
	else {
		float n[3], t[3];
		float v3t[3], v4t[3];
		sub_v3_v3v3(t, v1, v3);

		/* offset between both plane where the lines lies */
		cross_v3_v3v3(n, a, b);
		project_v3_v3v3(t, t, n);

		/* for the first line, offset the second line until it is coplanar */
		add_v3_v3v3(v3t, v3, t);
		add_v3_v3v3(v4t, v4, t);

		sub_v3_v3v3(c, v3t, v1);
		sub_v3_v3v3(a, v2, v1);
		sub_v3_v3v3(b, v4t, v3t);

		cross_v3_v3v3(ab, a, b);
		cross_v3_v3v3(cb, c, b);

		mul_v3_fl(a, dot_v3v3(cb, ab) / dot_v3v3(ab, ab));
		add_v3_v3v3(i1, v1, a);

		/* for the second line, just substract the offset from the first intersection point */
		sub_v3_v3v3(i2, i1, t);

		return 2; /* two nearest points */
	}
}

int isect_line_line_v3(
        const float v1[3], const float v2[3],
        const float v3[3], const float v4[3], float i1[3], float i2[3])
{
	const float epsilon = 0.000001f;
	return isect_line_line_epsilon_v3(v1, v2, v3, v4, i1, i2, epsilon);
}

/** Intersection point strictly between the two lines
 * \return false when no intersection is found
 */
bool isect_line_line_strict_v3(const float v1[3], const float v2[3],
                               const float v3[3], const float v4[3],
                               float vi[3], float *r_lambda)
{
	float a[3], b[3], c[3], ab[3], cb[3], ca[3], dir1[3], dir2[3];
	float d, div;

	sub_v3_v3v3(c, v3, v1);
	sub_v3_v3v3(a, v2, v1);
	sub_v3_v3v3(b, v4, v3);

	normalize_v3_v3(dir1, a);
	normalize_v3_v3(dir2, b);
	d = dot_v3v3(dir1, dir2);
	if (d == 1.0f || d == -1.0f || d == 0) {
		/* colinear or one vector is zero-length*/
		return false;
	}

	cross_v3_v3v3(ab, a, b);
	d = dot_v3v3(c, ab);
	div = dot_v3v3(ab, ab);

	/* test zero length line */
	if (UNLIKELY(div == 0.0f)) {
		return false;
	}
	/* test if the two lines are coplanar */
	else if (d > -0.000001f && d < 0.000001f) {
		float f1, f2;
		cross_v3_v3v3(cb, c, b);
		cross_v3_v3v3(ca, c, a);

		f1 = dot_v3v3(cb, ab) / div;
		f2 = dot_v3v3(ca, ab) / div;

		if (f1 >= 0 && f1 <= 1 &&
		    f2 >= 0 && f2 <= 1)
		{
			mul_v3_fl(a, f1);
			add_v3_v3v3(vi, v1, a);

			if (r_lambda) *r_lambda = f1;

			return true; /* intersection found */
		}
		else {
			return false;
		}
	}
	else {
		return false;
	}
}

bool isect_aabb_aabb_v3(const float min1[3], const float max1[3], const float min2[3], const float max2[3])
{
	return (min1[0] < max2[0] && min1[1] < max2[1] && min1[2] < max2[2] &&
	        min2[0] < max1[0] && min2[1] < max1[1] && min2[2] < max1[2]);
}

void isect_ray_aabb_initialize(IsectRayAABBData *data, const float ray_start[3], const float ray_direction[3])
{
	copy_v3_v3(data->ray_start, ray_start);

	data->ray_inv_dir[0] = 1.0f / ray_direction[0];
	data->ray_inv_dir[1] = 1.0f / ray_direction[1];
	data->ray_inv_dir[2] = 1.0f / ray_direction[2];

	data->sign[0] = data->ray_inv_dir[0] < 0.0f;
	data->sign[1] = data->ray_inv_dir[1] < 0.0f;
	data->sign[2] = data->ray_inv_dir[2] < 0.0f;
}

/* Adapted from http://www.gamedev.net/community/forums/topic.asp?topic_id=459973 */
bool isect_ray_aabb(const IsectRayAABBData *data, const float bb_min[3],
                    const float bb_max[3], float *tmin_out)
{
	float bbox[2][3];
	float tmin, tmax, tymin, tymax, tzmin, tzmax;

	copy_v3_v3(bbox[0], bb_min);
	copy_v3_v3(bbox[1], bb_max);

	tmin = (bbox[data->sign[0]][0] - data->ray_start[0]) * data->ray_inv_dir[0];
	tmax = (bbox[1 - data->sign[0]][0] - data->ray_start[0]) * data->ray_inv_dir[0];

	tymin = (bbox[data->sign[1]][1] - data->ray_start[1]) * data->ray_inv_dir[1];
	tymax = (bbox[1 - data->sign[1]][1] - data->ray_start[1]) * data->ray_inv_dir[1];

	if ((tmin > tymax) || (tymin > tmax))
		return false;

	if (tymin > tmin)
		tmin = tymin;

	if (tymax < tmax)
		tmax = tymax;

	tzmin = (bbox[data->sign[2]][2] - data->ray_start[2]) * data->ray_inv_dir[2];
	tzmax = (bbox[1 - data->sign[2]][2] - data->ray_start[2]) * data->ray_inv_dir[2];

	if ((tmin > tzmax) || (tzmin > tmax))
		return false;

	if (tzmin > tmin)
		tmin = tzmin;

	/* XXX jwilkins: tmax does not need to be updated since we don't use it
	 * keeping this here for future reference */
	//if (tzmax < tmax) tmax = tzmax;

	if (tmin_out)
		(*tmin_out) = tmin;

	return true;
}

/* find closest point to p on line through (l1, l2) and return lambda,
 * where (0 <= lambda <= 1) when cp is in the line segment (l1, l2)
 */
float closest_to_line_v3(float cp[3], const float p[3], const float l1[3], const float l2[3])
{
	float h[3], u[3], lambda;
	sub_v3_v3v3(u, l2, l1);
	sub_v3_v3v3(h, p, l1);
	lambda = dot_v3v3(u, h) / dot_v3v3(u, u);
	cp[0] = l1[0] + u[0] * lambda;
	cp[1] = l1[1] + u[1] * lambda;
	cp[2] = l1[2] + u[2] * lambda;
	return lambda;
}

float closest_to_line_v2(float cp[2], const float p[2], const float l1[2], const float l2[2])
{
	float h[2], u[2], lambda;
	sub_v2_v2v2(u, l2, l1);
	sub_v2_v2v2(h, p, l1);
	lambda = dot_v2v2(u, h) / dot_v2v2(u, u);
	cp[0] = l1[0] + u[0] * lambda;
	cp[1] = l1[1] + u[1] * lambda;
	return lambda;
}

/* little sister we only need to know lambda */
float line_point_factor_v3(const float p[3], const float l1[3], const float l2[3])
{
	float h[3], u[3];
	float dot;
	sub_v3_v3v3(u, l2, l1);
	sub_v3_v3v3(h, p, l1);
#if 0
	return (dot_v3v3(u, h) / dot_v3v3(u, u));
#else
	/* better check for zero */
	dot = dot_v3v3(u, u);
	return (dot != 0.0f) ? (dot_v3v3(u, h) / dot) : 0.0f;
#endif
}

float line_point_factor_v2(const float p[2], const float l1[2], const float l2[2])
{
	float h[2], u[2];
	float dot;
	sub_v2_v2v2(u, l2, l1);
	sub_v2_v2v2(h, p, l1);
#if 0
	return (dot_v2v2(u, h) / dot_v2v2(u, u));
#else
	/* better check for zero */
	dot = dot_v2v2(u, u);
	return (dot != 0.0f) ? (dot_v2v2(u, h) / dot) : 0.0f;
#endif
}

/**
 * \note #isect_line_plane_v3() shares logic
 */
float line_plane_factor_v3(const float plane_co[3], const float plane_no[3],
                           const float l1[3], const float l2[3])
{
	float u[3], h[3];
	float dot;
	sub_v3_v3v3(u, l2, l1);
	sub_v3_v3v3(h, l1, plane_co);
	dot = dot_v3v3(plane_no, u);
	return (dot != 0.0f) ? -dot_v3v3(plane_no, h) / dot : 0.0f;
}

/** Ensure the distance between these points is no greater then 'dist'.
 *  If it is, scale then both into the center.
 */
void limit_dist_v3(float v1[3], float v2[3], const float dist)
{
	const float dist_old = len_v3v3(v1, v2);

	if (dist_old > dist) {
		float v1_old[3];
		float v2_old[3];
		float fac = (dist / dist_old) * 0.5f;

		copy_v3_v3(v1_old, v1);
		copy_v3_v3(v2_old, v2);

		interp_v3_v3v3(v1, v1_old, v2_old, 0.5f - fac);
		interp_v3_v3v3(v2, v1_old, v2_old, 0.5f + fac);
	}
}

/*
 *     x1,y2
 *     |  \
 *     |   \     .(a,b)
 *     |    \
 *     x1,y1-- x2,y1
 */
int isect_point_tri_v2_int(const int x1, const int y1, const int x2, const int y2, const int a, const int b)
{
	float v1[2], v2[2], v3[2], p[2];

	v1[0] = (float)x1;
	v1[1] = (float)y1;

	v2[0] = (float)x1;
	v2[1] = (float)y2;

	v3[0] = (float)x2;
	v3[1] = (float)y1;

	p[0] = (float)a;
	p[1] = (float)b;

	return isect_point_tri_v2(p, v1, v2, v3);
}

static bool point_in_slice(const float p[3], const float v1[3], const float l1[3], const float l2[3])
{
	/*
	 * what is a slice ?
	 * some maths:
	 * a line including (l1, l2) and a point not on the line
	 * define a subset of R3 delimited by planes parallel to the line and orthogonal
	 * to the (point --> line) distance vector, one plane on the line one on the point,
	 * the room inside usually is rather small compared to R3 though still infinite
	 * useful for restricting (speeding up) searches
	 * e.g. all points of triangular prism are within the intersection of 3 'slices'
	 * another trivial case : cube
	 * but see a 'spat' which is a deformed cube with paired parallel planes needs only 3 slices too
	 */
	float h, rp[3], cp[3], q[3];

	closest_to_line_v3(cp, v1, l1, l2);
	sub_v3_v3v3(q, cp, v1);

	sub_v3_v3v3(rp, p, v1);
	h = dot_v3v3(q, rp) / dot_v3v3(q, q);
	return (h < 0.0f || h > 1.0f) ? false : true;
}

#if 0

/* adult sister defining the slice planes by the origin and the normal
 * NOTE |normal| may not be 1 but defining the thickness of the slice */
static int point_in_slice_as(float p[3], float origin[3], float normal[3])
{
	float h, rp[3];
	sub_v3_v3v3(rp, p, origin);
	h = dot_v3v3(normal, rp) / dot_v3v3(normal, normal);
	if (h < 0.0f || h > 1.0f) return 0;
	return 1;
}

/*mama (knowing the squared length of the normal) */
static int point_in_slice_m(float p[3], float origin[3], float normal[3], float lns)
{
	float h, rp[3];
	sub_v3_v3v3(rp, p, origin);
	h = dot_v3v3(normal, rp) / lns;
	if (h < 0.0f || h > 1.0f) return 0;
	return 1;
}
#endif

bool isect_point_tri_prism_v3(const float p[3], const float v1[3], const float v2[3], const float v3[3])
{
	if (!point_in_slice(p, v1, v2, v3)) return false;
	if (!point_in_slice(p, v2, v3, v1)) return false;
	if (!point_in_slice(p, v3, v1, v2)) return false;
	return true;
}

/**
 * \param r_vi The point \a p projected onto the triangle.
 * \return True when \a p is inside the triangle.
 * \note Its up to the caller to check the distance between \a p and \a r_vi against an error margin.
 */
bool isect_point_tri_v3(const float p[3], const float v1[3], const float v2[3], const float v3[3],
                        float r_vi[3])
{
	if (isect_point_tri_prism_v3(p, v1, v2, v3)) {
		float no[3], n1[3], n2[3];

		/* Could use normal_tri_v3, but doesn't have to be unit-length */
		sub_v3_v3v3(n1, v1, v2);
		sub_v3_v3v3(n2, v2, v3);
		cross_v3_v3v3(no, n1, n2);

		if (LIKELY(len_squared_v3(no) != 0.0f)) {
			float plane[4];
			plane_from_point_normal_v3(plane, v1, no);
			closest_to_plane_v3(r_vi, plane, p);
		}
		else {
			/* degenerate */
			copy_v3_v3(r_vi, p);
		}

		return true;
	}
	else {
		return false;
	}
}

bool clip_segment_v3_plane(float p1[3], float p2[3], const float plane[4])
{
	float dp[3], div, t, pc[3];

	sub_v3_v3v3(dp, p2, p1);
	div = dot_v3v3(dp, plane);

	if (div == 0.0f) /* parallel */
		return true;

	t = -plane_point_side_v3(plane, p1) / div;

	if (div > 0.0f) {
		/* behind plane, completely clipped */
		if (t >= 1.0f) {
			zero_v3(p1);
			zero_v3(p2);
			return false;
		}

		/* intersect plane */
		if (t > 0.0f) {
			madd_v3_v3v3fl(pc, p1, dp, t);
			copy_v3_v3(p1, pc);
			return true;
		}

		return true;
	}
	else {
		/* behind plane, completely clipped */
		if (t <= 0.0f) {
			zero_v3(p1);
			zero_v3(p2);
			return false;
		}

		/* intersect plane */
		if (t < 1.0f) {
			madd_v3_v3v3fl(pc, p1, dp, t);
			copy_v3_v3(p2, pc);
			return true;
		}

		return true;
	}
}

bool clip_segment_v3_plane_n(float r_p1[3], float r_p2[3], float plane_array[][4], const int plane_tot)
{
	/* intersect from both directions */
	float p1[3], p2[3], dp[3], dp_orig[3];
	int i;
	copy_v3_v3(p1, r_p1);
	copy_v3_v3(p2, r_p2);

	sub_v3_v3v3(dp, p2, p1);
	copy_v3_v3(dp_orig, dp);

	for (i = 0; i < plane_tot; i++) {
		const float *plane = plane_array[i];
		const float div = dot_v3v3(dp, plane);

		if (div != 0.0f) {
			const float t = -plane_point_side_v3(plane, p1) / div;
			if (div > 0.0f) {
				/* clip a */
				if (t >= 1.0f) {
					return false;
				}

				/* intersect plane */
				if (t > 0.0f) {
					madd_v3_v3v3fl(p1, p1, dp, t);
					/* recalc direction and test for flipping */
					sub_v3_v3v3(dp, p2, p1);
					if (dot_v3v3(dp, dp_orig) < 0.0f) {
						return false;
					}
				}
			}
			else if (div < 0.0f) {
				/* clip b */
				if (t <= 0.0f) {
					return false;
				}

				/* intersect plane */
				if (t < 1.0f) {
					madd_v3_v3v3fl(p2, p1, dp, t);
					/* recalc direction and test for flipping */
					sub_v3_v3v3(dp, p2, p1);
					if (dot_v3v3(dp, dp_orig) < 0.0f) {
						return false;
					}
				}
			}
		}
	}

	copy_v3_v3(r_p1, p1);
	copy_v3_v3(r_p2, p2);
	return true;
}

void plot_line_v2v2i(const int p1[2], const int p2[2], bool (*callback)(int, int, void *), void *userData)
{
	int x1 = p1[0];
	int y1 = p1[1];
	int x2 = p2[0];
	int y2 = p2[1];

	signed char ix;
	signed char iy;

	/* if x1 == x2 or y1 == y2, then it does not matter what we set here */
	int delta_x = (x2 > x1 ? (ix = 1, x2 - x1) : (ix = -1, x1 - x2)) << 1;
	int delta_y = (y2 > y1 ? (iy = 1, y2 - y1) : (iy = -1, y1 - y2)) << 1;

	if (callback(x1, y1, userData) == 0) {
		return;
	}

	if (delta_x >= delta_y) {
		/* error may go below zero */
		int error = delta_y - (delta_x >> 1);

		while (x1 != x2) {
			if (error >= 0) {
				if (error || (ix > 0)) {
					y1 += iy;
					error -= delta_x;
				}
				/* else do nothing */
			}
			/* else do nothing */

			x1 += ix;
			error += delta_y;

			if (callback(x1, y1, userData) == 0) {
				return;
			}
		}
	}
	else {
		/* error may go below zero */
		int error = delta_x - (delta_y >> 1);

		while (y1 != y2) {
			if (error >= 0) {
				if (error || (iy > 0)) {
					x1 += ix;
					error -= delta_y;
				}
				/* else do nothing */
			}
			/* else do nothing */

			y1 += iy;
			error += delta_x;

			if (callback(x1, y1, userData) == 0) {
				return;
			}
		}
	}
}

void fill_poly_v2i_n(
        const int xmin, const int ymin, const int xmax, const int ymax,
        const int verts[][2], const int nr,
        void (*callback)(int, int, void *), void *userData)
{
	/* originally by Darel Rex Finley, 2007 */

	int  nodes, pixel_y, i, j, swap;
	int *node_x = MEM_mallocN(sizeof(*node_x) * (size_t)(nr + 1), __func__);

	/* Loop through the rows of the image. */
	for (pixel_y = ymin; pixel_y < ymax; pixel_y++) {

		/* Build a list of nodes. */
		nodes = 0; j = nr - 1;
		for (i = 0; i < nr; i++) {
			if ((verts[i][1] < pixel_y && verts[j][1] >= pixel_y) ||
			    (verts[j][1] < pixel_y && verts[i][1] >= pixel_y))
			{
				node_x[nodes++] = (int)(verts[i][0] +
				                        ((double)(pixel_y - verts[i][1]) / (verts[j][1] - verts[i][1])) *
				                        (verts[j][0] - verts[i][0]));
			}
			j = i;
		}

		/* Sort the nodes, via a simple "Bubble" sort. */
		i = 0;
		while (i < nodes - 1) {
			if (node_x[i] > node_x[i + 1]) {
				SWAP_TVAL(swap, node_x[i], node_x[i + 1]);
				if (i) i--;
			}
			else {
				i++;
			}
		}

		/* Fill the pixels between node pairs. */
		for (i = 0; i < nodes; i += 2) {
			if (node_x[i] >= xmax) break;
			if (node_x[i + 1] >  xmin) {
				if (node_x[i    ] < xmin) node_x[i    ] = xmin;
				if (node_x[i + 1] > xmax) node_x[i + 1] = xmax;
				for (j = node_x[i]; j < node_x[i + 1]; j++) {
					callback(j - xmin, pixel_y - ymin, userData);
				}
			}
		}
	}
	MEM_freeN(node_x);
}

/****************************** Axis Utils ********************************/

/**
 * \brief Normal to x,y matrix
 *
 * Creates a 3x3 matrix from a normal.
 * This matrix can be applied to vectors so their 'z' axis runs along \a normal.
 * In practice it means you can use x,y as 2d coords. \see
 *
 * \param r_mat The matrix to return.
 * \param normal A unit length vector.
 */
void axis_dominant_v3_to_m3(float r_mat[3][3], const float normal[3])
{
	BLI_ASSERT_UNIT_V3(normal);

	copy_v3_v3(r_mat[2], normal);
	ortho_basis_v3v3_v3(r_mat[0], r_mat[1], r_mat[2]);

	BLI_ASSERT_UNIT_V3(r_mat[0]);
	BLI_ASSERT_UNIT_V3(r_mat[1]);

	transpose_m3(r_mat);

	BLI_assert(!is_negative_m3(r_mat));
	BLI_assert(fabsf(dot_m3_v3_row_z(r_mat, normal) - 1.0f) < BLI_ASSERT_UNIT_EPSILON);
}

/****************************** Interpolation ********************************/

static float tri_signed_area(const float v1[3], const float v2[3], const float v3[3], const int i, const int j)
{
	return 0.5f * ((v1[i] - v2[i]) * (v2[j] - v3[j]) + (v1[j] - v2[j]) * (v3[i] - v2[i]));
}

/* return 1 when degenerate */
static bool barycentric_weights(const float v1[3], const float v2[3], const float v3[3], const float co[3], const float n[3], float w[3])
{
	float wtot;
	int i, j;

	axis_dominant_v3(&i, &j, n);

	w[0] = tri_signed_area(v2, v3, co, i, j);
	w[1] = tri_signed_area(v3, v1, co, i, j);
	w[2] = tri_signed_area(v1, v2, co, i, j);

	wtot = w[0] + w[1] + w[2];

	if (fabsf(wtot) > FLT_EPSILON) {
		mul_v3_fl(w, 1.0f / wtot);
		return false;
	}
	else {
		/* zero area triangle */
		copy_v3_fl(w, 1.0f / 3.0f);
		return true;
	}
}

void interp_weights_face_v3(float w[4], const float v1[3], const float v2[3], const float v3[3], const float v4[3], const float co[3])
{
	float w2[3];

	w[0] = w[1] = w[2] = w[3] = 0.0f;

	/* first check for exact match */
	if (equals_v3v3(co, v1))
		w[0] = 1.0f;
	else if (equals_v3v3(co, v2))
		w[1] = 1.0f;
	else if (equals_v3v3(co, v3))
		w[2] = 1.0f;
	else if (v4 && equals_v3v3(co, v4))
		w[3] = 1.0f;
	else {
		/* otherwise compute barycentric interpolation weights */
		float n1[3], n2[3], n[3];
		bool degenerate;

		sub_v3_v3v3(n1, v1, v3);
		if (v4) {
			sub_v3_v3v3(n2, v2, v4);
		}
		else {
			sub_v3_v3v3(n2, v2, v3);
		}
		cross_v3_v3v3(n, n1, n2);

		/* OpenGL seems to split this way, so we do too */
		if (v4) {
			degenerate = barycentric_weights(v1, v2, v4, co, n, w);
			SWAP(float, w[2], w[3]);

			if (degenerate || (w[0] < 0.0f)) {
				/* if w[1] is negative, co is on the other side of the v1-v3 edge,
				 * so we interpolate using the other triangle */
				degenerate = barycentric_weights(v2, v3, v4, co, n, w2);

				if (!degenerate) {
					w[0] = 0.0f;
					w[1] = w2[0];
					w[2] = w2[1];
					w[3] = w2[2];
				}
			}
		}
		else {
			barycentric_weights(v1, v2, v3, co, n, w);
		}
	}
}

/* return 1 of point is inside triangle, 2 if it's on the edge, 0 if point is outside of triangle */
int barycentric_inside_triangle_v2(const float w[3])
{
	if (IN_RANGE(w[0], 0.0f, 1.0f) &&
	    IN_RANGE(w[1], 0.0f, 1.0f) &&
	    IN_RANGE(w[2], 0.0f, 1.0f))
	{
		return 1;
	}
	else if (IN_RANGE_INCL(w[0], 0.0f, 1.0f) &&
	         IN_RANGE_INCL(w[1], 0.0f, 1.0f) &&
	         IN_RANGE_INCL(w[2], 0.0f, 1.0f))
	{
		return 2;
	}

	return 0;
}

/* returns 0 for degenerated triangles */
bool barycentric_coords_v2(const float v1[2], const float v2[2], const float v3[2], const float co[2], float w[3])
{
	const float x = co[0], y = co[1];
	const float x1 = v1[0], y1 = v1[1];
	const float x2 = v2[0], y2 = v2[1];
	const float x3 = v3[0], y3 = v3[1];
	const float det = (y2 - y3) * (x1 - x3) + (x3 - x2) * (y1 - y3);

	if (fabsf(det) > FLT_EPSILON) {
		w[0] = ((y2 - y3) * (x - x3) + (x3 - x2) * (y - y3)) / det;
		w[1] = ((y3 - y1) * (x - x3) + (x1 - x3) * (y - y3)) / det;
		w[2] = 1.0f - w[0] - w[1];

		return true;
	}

	return false;
}

/**
 * \note: using #area_tri_signed_v2 means locations outside the triangle are correctly weighted
 */
void barycentric_weights_v2(const float v1[2], const float v2[2], const float v3[2], const float co[2], float w[3])
{
	float wtot;

	w[0] = area_tri_signed_v2(v2, v3, co);
	w[1] = area_tri_signed_v2(v3, v1, co);
	w[2] = area_tri_signed_v2(v1, v2, co);
	wtot = w[0] + w[1] + w[2];

	if (wtot != 0.0f) {
		mul_v3_fl(w, 1.0f / wtot);
	}
	else { /* dummy values for zero area face */
		copy_v3_fl(w, 1.0f / 3.0f);
	}
}

/**
 * still use 2D X,Y space but this works for verts transformed by a perspective matrix,
 * using their 4th component as a weight
 */
void barycentric_weights_v2_persp(const float v1[4], const float v2[4], const float v3[4], const float co[2], float w[3])
{
	float wtot;

	w[0] = area_tri_signed_v2(v2, v3, co) / v1[3];
	w[1] = area_tri_signed_v2(v3, v1, co) / v2[3];
	w[2] = area_tri_signed_v2(v1, v2, co) / v3[3];
	wtot = w[0] + w[1] + w[2];

	if (wtot != 0.0f) {
		mul_v3_fl(w, 1.0f / wtot);
	}
	else { /* dummy values for zero area face */
		w[0] = w[1] = w[2] = 1.0f / 3.0f;
	}
}

/* same as #barycentric_weights_v2 but works with a quad,
 * note: untested for values outside the quad's bounds
 * this is #interp_weights_poly_v2 expanded for quads only */
void barycentric_weights_v2_quad(const float v1[2], const float v2[2], const float v3[2], const float v4[2],
                                 const float co[2], float w[4])
{
	/* note: fabsf() here is not needed for convex quads (and not used in interp_weights_poly_v2).
	 *       but in the case of concave/bow-tie quads for the mask rasterizer it gives unreliable results
	 *       without adding absf(). If this becomes an issue for more general usage we could have
	 *       this optional or use a different function - Campbell */
#define MEAN_VALUE_HALF_TAN_V2(_area, i1, i2) \
	        ((_area = cross_v2v2(dirs[i1], dirs[i2])) != 0.0f ? \
	         fabsf(((lens[i1] * lens[i2]) - dot_v2v2(dirs[i1], dirs[i2])) / _area) : 0.0f)

	const float dirs[4][2] = {
	    {v1[0] - co[0], v1[1] - co[1]},
	    {v2[0] - co[0], v2[1] - co[1]},
	    {v3[0] - co[0], v3[1] - co[1]},
	    {v4[0] - co[0], v4[1] - co[1]},
	};

	const float lens[4] = {
	    len_v2(dirs[0]),
	    len_v2(dirs[1]),
	    len_v2(dirs[2]),
	    len_v2(dirs[3]),
	};

	/* avoid divide by zero */
	if      (UNLIKELY(lens[0] < FLT_EPSILON)) { w[0] = 1.0f; w[1] = w[2] = w[3] = 0.0f; }
	else if (UNLIKELY(lens[1] < FLT_EPSILON)) { w[1] = 1.0f; w[0] = w[2] = w[3] = 0.0f; }
	else if (UNLIKELY(lens[2] < FLT_EPSILON)) { w[2] = 1.0f; w[0] = w[1] = w[3] = 0.0f; }
	else if (UNLIKELY(lens[3] < FLT_EPSILON)) { w[3] = 1.0f; w[0] = w[1] = w[2] = 0.0f; }
	else {
		float wtot, area;

		/* variable 'area' is just for storage,
		 * the order its initialized doesn't matter */
#ifdef __clang__
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Wunsequenced"
#endif

		/* inline mean_value_half_tan four times here */
		const float t[4] = {
			MEAN_VALUE_HALF_TAN_V2(area, 0, 1),
			MEAN_VALUE_HALF_TAN_V2(area, 1, 2),
			MEAN_VALUE_HALF_TAN_V2(area, 2, 3),
			MEAN_VALUE_HALF_TAN_V2(area, 3, 0),
		};

#ifdef __clang__
#  pragma clang diagnostic pop
#endif

#undef MEAN_VALUE_HALF_TAN_V2

		w[0] = (t[3] + t[0]) / lens[0];
		w[1] = (t[0] + t[1]) / lens[1];
		w[2] = (t[1] + t[2]) / lens[2];
		w[3] = (t[2] + t[3]) / lens[3];

		wtot = w[0] + w[1] + w[2] + w[3];

		if (wtot != 0.0f) {
			mul_v4_fl(w, 1.0f / wtot);
		}
		else { /* dummy values for zero area face */
			copy_v4_fl(w, 1.0f / 4.0f);
		}
	}
}

/* given 2 triangles in 3D space, and a point in relation to the first triangle.
 * calculate the location of a point in relation to the second triangle.
 * Useful for finding relative positions with geometry */
void transform_point_by_tri_v3(
        float pt_tar[3], float const pt_src[3],
        const float tri_tar_p1[3], const float tri_tar_p2[3], const float tri_tar_p3[3],
        const float tri_src_p1[3], const float tri_src_p2[3], const float tri_src_p3[3])
{
	/* this works by moving the source triangle so its normal is pointing on the Z
	 * axis where its barycentric weights can be calculated in 2D and its Z offset can
	 *  be re-applied. The weights are applied directly to the targets 3D points and the
	 *  z-depth is used to scale the targets normal as an offset.
	 * This saves transforming the target into its Z-Up orientation and back (which could also work) */
	float no_tar[3], no_src[3];
	float mat_src[3][3];
	float pt_src_xy[3];
	float tri_xy_src[3][3];
	float w_src[3];
	float area_tar, area_src;
	float z_ofs_src;

	normal_tri_v3(no_tar, tri_tar_p1, tri_tar_p2, tri_tar_p3);
	normal_tri_v3(no_src, tri_src_p1, tri_src_p2, tri_src_p3);

	axis_dominant_v3_to_m3(mat_src, no_src);

	/* make the source tri xy space */
	mul_v3_m3v3(pt_src_xy,     mat_src, pt_src);
	mul_v3_m3v3(tri_xy_src[0], mat_src, tri_src_p1);
	mul_v3_m3v3(tri_xy_src[1], mat_src, tri_src_p2);
	mul_v3_m3v3(tri_xy_src[2], mat_src, tri_src_p3);


	barycentric_weights_v2(tri_xy_src[0], tri_xy_src[1], tri_xy_src[2], pt_src_xy, w_src);
	interp_v3_v3v3v3(pt_tar, tri_tar_p1, tri_tar_p2, tri_tar_p3, w_src);

	area_tar = sqrtf(area_tri_v3(tri_tar_p1, tri_tar_p2, tri_tar_p3));
	area_src = sqrtf(area_tri_v2(tri_xy_src[0], tri_xy_src[1], tri_xy_src[2]));

	z_ofs_src = pt_src_xy[2] - tri_xy_src[0][2];
	madd_v3_v3v3fl(pt_tar, pt_tar, no_tar, (z_ofs_src / area_src) * area_tar);
}

/**
 * Simply re-interpolates,
 * assumes p_src is between \a l_src_p1-l_src_p2
 */
void transform_point_by_seg_v3(
        float p_dst[3], const float p_src[3],
        const float l_dst_p1[3], const float l_dst_p2[3],
        const float l_src_p1[3], const float l_src_p2[3])
{
	float t = line_point_factor_v3(p_src, l_src_p1, l_src_p2);
	interp_v3_v3v3(p_dst, l_dst_p1, l_dst_p2, t);
}

/* given an array with some invalid values this function interpolates valid values
 * replacing the invalid ones */
int interp_sparse_array(float *array, const int list_size, const float skipval)
{
	int found_invalid = 0;
	int found_valid = 0;
	int i;

	for (i = 0; i < list_size; i++) {
		if (array[i] == skipval)
			found_invalid = 1;
		else
			found_valid = 1;
	}

	if (found_valid == 0) {
		return -1;
	}
	else if (found_invalid == 0) {
		return 0;
	}
	else {
		/* found invalid depths, interpolate */
		float valid_last = skipval;
		int valid_ofs = 0;

		float *array_up = MEM_callocN(sizeof(float) * (size_t)list_size, "interp_sparse_array up");
		float *array_down = MEM_callocN(sizeof(float) * (size_t)list_size, "interp_sparse_array up");

		int *ofs_tot_up = MEM_callocN(sizeof(int) * (size_t)list_size, "interp_sparse_array tup");
		int *ofs_tot_down = MEM_callocN(sizeof(int) * (size_t)list_size, "interp_sparse_array tdown");

		for (i = 0; i < list_size; i++) {
			if (array[i] == skipval) {
				array_up[i] = valid_last;
				ofs_tot_up[i] = ++valid_ofs;
			}
			else {
				valid_last = array[i];
				valid_ofs = 0;
			}
		}

		valid_last = skipval;
		valid_ofs = 0;

		for (i = list_size - 1; i >= 0; i--) {
			if (array[i] == skipval) {
				array_down[i] = valid_last;
				ofs_tot_down[i] = ++valid_ofs;
			}
			else {
				valid_last = array[i];
				valid_ofs = 0;
			}
		}

		/* now blend */
		for (i = 0; i < list_size; i++) {
			if (array[i] == skipval) {
				if (array_up[i] != skipval && array_down[i] != skipval) {
					array[i] = ((array_up[i]   * (float)ofs_tot_down[i]) +
					            (array_down[i] * (float)ofs_tot_up[i])) / (float)(ofs_tot_down[i] + ofs_tot_up[i]);
				}
				else if (array_up[i] != skipval) {
					array[i] = array_up[i];
				}
				else if (array_down[i] != skipval) {
					array[i] = array_down[i];
				}
			}
		}

		MEM_freeN(array_up);
		MEM_freeN(array_down);

		MEM_freeN(ofs_tot_up);
		MEM_freeN(ofs_tot_down);
	}

	return 1;
}

/* Mean value weights - smooth interpolation weights for polygons with
 * more than 3 vertices */
static float mean_value_half_tan_v3(const float v1[3], const float v2[3], const float v3[3])
{
	float d2[3], d3[3], cross[3], area;

	sub_v3_v3v3(d2, v2, v1);
	sub_v3_v3v3(d3, v3, v1);
	cross_v3_v3v3(cross, d2, d3);

	area = len_v3(cross);
	if (LIKELY(area != 0.0f)) {
		const float dot = dot_v3v3(d2, d3);
		const float len = len_v3(d2) * len_v3(d3);
		return (len - dot) / area;
	}
	else {
		return 0.0f;
	}
}
static float mean_value_half_tan_v2(const float v1[2], const float v2[2], const float v3[2])
{
	float d2[2], d3[2], area;

	sub_v2_v2v2(d2, v2, v1);
	sub_v2_v2v2(d3, v3, v1);

	/* different from the 3d version but still correct */
	area = cross_v2v2(d2, d3);
	if (LIKELY(area != 0.0f)) {
		const float dot = dot_v2v2(d2, d3);
		const float len = len_v2(d2) * len_v2(d3);
		return (len - dot) / area;
	}
	else {
		return 0.0f;
	}
}

void interp_weights_poly_v3(float *w, float v[][3], const int n, const float co[3])
{
	const float eps = 1e-5f;  /* take care, low values cause [#36105] */
	const float eps_sq = eps * eps;
	const float *v_curr, *v_next;
	float ht_prev, ht;  /* half tangents */
	float totweight = 0.0f;
	int i = 0;
	bool vert_interp = false;
	bool edge_interp = false;

	v_curr = v[0];
	v_next = v[1];

	ht_prev = mean_value_half_tan_v3(co, v[n - 1], v_curr);

	while (i < n) {
		const float len_sq = len_squared_v3v3(co, v_curr);

		/* Mark Mayer et al algorithm that is used here does not operate well if vertex is close
		 * to borders of face. In that case, do simple linear interpolation between the two edge vertices */
		if (len_sq < eps_sq) {
			vert_interp = true;
			break;
		}
		else if (dist_squared_to_line_segment_v3(co, v_curr, v_next) < eps_sq) {
			edge_interp = true;
			break;
		}

		ht = mean_value_half_tan_v3(co, v_curr, v_next);
		w[i] = (ht_prev + ht) / sqrtf(len_sq);
		totweight += w[i];

		/* step */
		i++;
		v_curr = v_next;
		v_next = v[(i + 1) % n];

		ht_prev = ht;
	}

	if (vert_interp) {
		const int i_curr = i;
		for (i = 0; i < n; i++)
			w[i] = 0.0;
		w[i_curr] = 1.0f;
	}
	else if (edge_interp) {
		const int i_curr = i;
		float len_curr = len_v3v3(co, v_curr);
		float len_next = len_v3v3(co, v_next);
		float edge_len = len_curr + len_next;
		for (i = 0; i < n; i++)
			w[i] = 0.0;

		w[i_curr] = len_next / edge_len;
		w[(i_curr + 1) % n] = len_curr / edge_len;
	}
	else {
		if (totweight != 0.0f) {
			for (i = 0; i < n; i++) {
				w[i] /= totweight;
			}
		}
	}
}


void interp_weights_poly_v2(float *w, float v[][2], const int n, const float co[2])
{
	const float eps = 1e-5f;  /* take care, low values cause [#36105] */
	const float eps_sq = eps * eps;
	const float *v_curr, *v_next;
	float ht_prev, ht;  /* half tangents */
	float totweight = 0.0f;
	int i = 0;
	bool vert_interp = false;
	bool edge_interp = false;

	v_curr = v[0];
	v_next = v[1];

	ht_prev = mean_value_half_tan_v2(co, v[n - 1], v_curr);

	while (i < n) {
		const float len_sq = len_squared_v2v2(co, v_curr);

		/* Mark Mayer et al algorithm that is used here does not operate well if vertex is close
		 * to borders of face. In that case, do simple linear interpolation between the two edge vertices */
		if (len_sq < eps_sq) {
			vert_interp = true;
			break;
		}
		else if (dist_squared_to_line_segment_v2(co, v_curr, v_next) < eps_sq) {
			edge_interp = true;
			break;
		}

		ht = mean_value_half_tan_v2(co, v_curr, v_next);
		w[i] = (ht_prev + ht) / sqrtf(len_sq);
		totweight += w[i];

		/* step */
		i++;
		v_curr = v_next;
		v_next = v[(i + 1) % n];

		ht_prev = ht;
	}

	if (vert_interp) {
		const int i_curr = i;
		for (i = 0; i < n; i++)
			w[i] = 0.0;
		w[i_curr] = 1.0f;
	}
	else if (edge_interp) {
		const int i_curr = i;
		float len_curr = len_v2v2(co, v_curr);
		float len_next = len_v2v2(co, v_next);
		float edge_len = len_curr + len_next;
		for (i = 0; i < n; i++)
			w[i] = 0.0;

		w[i_curr] = len_next / edge_len;
		w[(i_curr + 1) % n] = len_curr / edge_len;
	}
	else {
		if (totweight != 0.0f) {
			for (i = 0; i < n; i++) {
				w[i] /= totweight;
			}
		}
	}
}

/* (x1, v1)(t1=0)------(x2, v2)(t2=1), 0<t<1 --> (x, v)(t) */
void interp_cubic_v3(float x[3], float v[3], const float x1[3], const float v1[3], const float x2[3], const float v2[3], const float t)
{
	float a[3], b[3];
	const float t2 = t * t;
	const float t3 = t2 * t;

	/* cubic interpolation */
	a[0] = v1[0] + v2[0] + 2 * (x1[0] - x2[0]);
	a[1] = v1[1] + v2[1] + 2 * (x1[1] - x2[1]);
	a[2] = v1[2] + v2[2] + 2 * (x1[2] - x2[2]);

	b[0] = -2 * v1[0] - v2[0] - 3 * (x1[0] - x2[0]);
	b[1] = -2 * v1[1] - v2[1] - 3 * (x1[1] - x2[1]);
	b[2] = -2 * v1[2] - v2[2] - 3 * (x1[2] - x2[2]);

	x[0] = a[0] * t3 + b[0] * t2 + v1[0] * t + x1[0];
	x[1] = a[1] * t3 + b[1] * t2 + v1[1] * t + x1[1];
	x[2] = a[2] * t3 + b[2] * t2 + v1[2] * t + x1[2];

	v[0] = 3 * a[0] * t2 + 2 * b[0] * t + v1[0];
	v[1] = 3 * a[1] * t2 + 2 * b[1] * t + v1[1];
	v[2] = 3 * a[2] * t2 + 2 * b[2] * t + v1[2];
}

/* unfortunately internal calculations have to be done at double precision to achieve correct/stable results. */

#define IS_ZERO(x) ((x > (-DBL_EPSILON) && x < DBL_EPSILON) ? 1 : 0)

/**
 * Barycentric reverse
 *
 * Compute coordinates (u, v) for point \a st with respect to triangle (\a st0, \a st1, \a st2)
 */
void resolve_tri_uv_v2(float r_uv[2], const float st[2],
                       const float st0[2], const float st1[2], const float st2[2])
{
	/* find UV such that
	 * t = u * t0 + v * t1 + (1 - u - v) * t2
	 * u * (t0 - t2) + v * (t1 - t2) = t - t2 */
	const double a = st0[0] - st2[0], b = st1[0] - st2[0];
	const double c = st0[1] - st2[1], d = st1[1] - st2[1];
	const double det = a * d - c * b;

	/* det should never be zero since the determinant is the signed ST area of the triangle. */
	if (IS_ZERO(det) == 0) {
		const double x[2] = {st[0] - st2[0], st[1] - st2[1]};

		r_uv[0] = (float)((d * x[0] - b * x[1]) / det);
		r_uv[1] = (float)(((-c) * x[0] + a * x[1]) / det);
	}
	else {
		zero_v2(r_uv);
	}
}

/**
 * Barycentric reverse 3d
 *
 * Compute coordinates (u, v) for point \a st with respect to triangle (\a st0, \a st1, \a st2)
 */
void resolve_tri_uv_v3(float r_uv[2], const float st[3], const float st0[3], const float st1[3], const float st2[3])
{
	float v0[3], v1[3], v2[3];
	double d00, d01, d11, d20, d21, det;

	sub_v3_v3v3(v0, st1, st0);
	sub_v3_v3v3(v1, st2, st0);
	sub_v3_v3v3(v2, st, st0);

	d00 = dot_v3v3(v0, v0);
	d01 = dot_v3v3(v0, v1);
	d11 = dot_v3v3(v1, v1);
	d20 = dot_v3v3(v2, v0);
	d21 = dot_v3v3(v2, v1);

	det = d00 * d11 - d01 * d01;

	/* det should never be zero since the determinant is the signed ST area of the triangle. */
	if (IS_ZERO(det) == 0) {
		float w;

		w =       (float)((d00 * d21 - d01 * d20) / det);
		r_uv[1] = (float)((d11 * d20 - d01 * d21) / det);
		r_uv[0] = 1.0f - r_uv[1] - w;
	}
	else {
		zero_v2(r_uv);
	}
}

/* bilinear reverse */
void resolve_quad_uv_v2(float r_uv[2], const float st[2],
                        const float st0[2], const float st1[2], const float st2[2], const float st3[2])
{
	resolve_quad_uv_v2_deriv(r_uv, NULL, st, st0, st1, st2, st3);
}

/* bilinear reverse with derivatives */
void resolve_quad_uv_v2_deriv(float r_uv[2], float r_deriv[2][2],
                              const float st[2], const float st0[2], const float st1[2], const float st2[2], const float st3[2])
{
	const double signed_area = (st0[0] * st1[1] - st0[1] * st1[0]) + (st1[0] * st2[1] - st1[1] * st2[0]) +
	                           (st2[0] * st3[1] - st2[1] * st3[0]) + (st3[0] * st0[1] - st3[1] * st0[0]);

	/* X is 2D cross product (determinant)
	 * A = (p0 - p) X (p0 - p3)*/
	const double a = (st0[0] - st[0]) * (st0[1] - st3[1]) - (st0[1] - st[1]) * (st0[0] - st3[0]);

	/* B = ( (p0 - p) X (p1 - p2) + (p1 - p) X (p0 - p3) ) / 2 */
	const double b = 0.5 * (double)(((st0[0] - st[0]) * (st1[1] - st2[1]) - (st0[1] - st[1]) * (st1[0] - st2[0])) +
	                                ((st1[0] - st[0]) * (st0[1] - st3[1]) - (st1[1] - st[1]) * (st0[0] - st3[0])));

	/* C = (p1-p) X (p1-p2) */
	const double fC = (st1[0] - st[0]) * (st1[1] - st2[1]) - (st1[1] - st[1]) * (st1[0] - st2[0]);
	double denom = a - 2 * b + fC;

	/* clear outputs */
	zero_v2(r_uv);

	if (IS_ZERO(denom) != 0) {
		const double fDen = a - fC;
		if (IS_ZERO(fDen) == 0)
			r_uv[0] = (float)(a / fDen);
	}
	else {
		const double desc_sq = b * b - a * fC;
		const double desc = sqrt(desc_sq < 0.0 ? 0.0 : desc_sq);
		const double s = signed_area > 0 ? (-1.0) : 1.0;

		r_uv[0] = (float)(((a - b) + s * desc) / denom);
	}

	/* find UV such that
	 * fST = (1-u)(1-v) * ST0 + u * (1-v) * ST1 + u * v * ST2 + (1-u) * v * ST3 */
	{
		const double denom_s = (1 - r_uv[0]) * (st0[0] - st3[0]) + r_uv[0] * (st1[0] - st2[0]);
		const double denom_t = (1 - r_uv[0]) * (st0[1] - st3[1]) + r_uv[0] * (st1[1] - st2[1]);
		int i = 0;
		denom = denom_s;

		if (fabs(denom_s) < fabs(denom_t)) {
			i = 1;
			denom = denom_t;
		}

		if (IS_ZERO(denom) == 0)
			r_uv[1] = (float)((double)((1.0f - r_uv[0]) * (st0[i] - st[i]) + r_uv[0] * (st1[i] - st[i])) / denom);
	}

	if (r_deriv) {
		float tmp1[2], tmp2[2], s[2], t[2];
		
		/* clear outputs */
		zero_v2(r_deriv[0]);
		zero_v2(r_deriv[1]);
		
		sub_v2_v2v2(tmp1, st1, st0);
		sub_v2_v2v2(tmp2, st2, st3);
		interp_v2_v2v2(s, tmp1, tmp2, r_uv[1]);
		sub_v2_v2v2(tmp1, st3, st0);
		sub_v2_v2v2(tmp2, st2, st1);
		interp_v2_v2v2(t, tmp1, tmp2, r_uv[0]);

		denom = t[0] * s[1] - t[1] * s[0];

		if (!IS_ZERO(denom)) {
			double inv_denom = 1.0 / denom;
			r_deriv[0][0] = (float)((double)-t[1] * inv_denom);
			r_deriv[0][1] = (float)((double) t[0] * inv_denom);
			r_deriv[1][0] = (float)((double) s[1] * inv_denom);
			r_deriv[1][1] = (float)((double)-s[0] * inv_denom);
		}
	}
}

#undef IS_ZERO

/* reverse of the functions above */
void interp_bilinear_quad_v3(float data[4][3], float u, float v, float res[3])
{
	float vec[3];

	copy_v3_v3(res, data[0]);
	mul_v3_fl(res, (1 - u) * (1 - v));
	copy_v3_v3(vec, data[1]);
	mul_v3_fl(vec, u * (1 - v)); add_v3_v3(res, vec);
	copy_v3_v3(vec, data[2]);
	mul_v3_fl(vec, u * v); add_v3_v3(res, vec);
	copy_v3_v3(vec, data[3]);
	mul_v3_fl(vec, (1 - u) * v); add_v3_v3(res, vec);
}

void interp_barycentric_tri_v3(float data[3][3], float u, float v, float res[3])
{
	float vec[3];

	copy_v3_v3(res, data[0]);
	mul_v3_fl(res, u);
	copy_v3_v3(vec, data[1]);
	mul_v3_fl(vec, v); add_v3_v3(res, vec);
	copy_v3_v3(vec, data[2]);
	mul_v3_fl(vec, 1.0f - u - v); add_v3_v3(res, vec);
}

/***************************** View & Projection *****************************/

void orthographic_m4(float matrix[4][4], const float left, const float right, const float bottom, const float top,
                     const float nearClip, const float farClip)
{
	float Xdelta, Ydelta, Zdelta;

	Xdelta = right - left;
	Ydelta = top - bottom;
	Zdelta = farClip - nearClip;
	if (Xdelta == 0.0f || Ydelta == 0.0f || Zdelta == 0.0f) {
		return;
	}
	unit_m4(matrix);
	matrix[0][0] = 2.0f / Xdelta;
	matrix[3][0] = -(right + left) / Xdelta;
	matrix[1][1] = 2.0f / Ydelta;
	matrix[3][1] = -(top + bottom) / Ydelta;
	matrix[2][2] = -2.0f / Zdelta; /* note: negate Z	*/
	matrix[3][2] = -(farClip + nearClip) / Zdelta;
}

void perspective_m4(float mat[4][4], const float left, const float right, const float bottom, const float top,
                    const float nearClip, const float farClip)
{
	const float Xdelta = right - left;
	const float Ydelta = top - bottom;
	const float Zdelta = farClip - nearClip;

	if (Xdelta == 0.0f || Ydelta == 0.0f || Zdelta == 0.0f) {
		return;
	}
	mat[0][0] = nearClip * 2.0f / Xdelta;
	mat[1][1] = nearClip * 2.0f / Ydelta;
	mat[2][0] = (right + left) / Xdelta; /* note: negate Z	*/
	mat[2][1] = (top + bottom) / Ydelta;
	mat[2][2] = -(farClip + nearClip) / Zdelta;
	mat[2][3] = -1.0f;
	mat[3][2] = (-2.0f * nearClip * farClip) / Zdelta;
	mat[0][1] = mat[0][2] = mat[0][3] =
	        mat[1][0] = mat[1][2] = mat[1][3] =
	        mat[3][0] = mat[3][1] = mat[3][3] = 0.0f;

}

/* translate a matrix created by orthographic_m4 or perspective_m4 in XY coords (used to jitter the view) */
void window_translate_m4(float winmat[4][4], float perspmat[4][4], const float x, const float y)
{
	if (winmat[2][3] == -1.0f) {
		/* in the case of a win-matrix, this means perspective always */
		float v1[3];
		float v2[3];
		float len1, len2;

		v1[0] = perspmat[0][0];
		v1[1] = perspmat[1][0];
		v1[2] = perspmat[2][0];

		v2[0] = perspmat[0][1];
		v2[1] = perspmat[1][1];
		v2[2] = perspmat[2][1];

		len1 = (1.0f / len_v3(v1));
		len2 = (1.0f / len_v3(v2));

		winmat[2][0] += len1 * winmat[0][0] * x;
		winmat[2][1] += len2 * winmat[1][1] * y;
	}
	else {
		winmat[3][0] += x;
		winmat[3][1] += y;
	}
}

static void i_multmatrix(float icand[4][4], float Vm[4][4])
{
	int row, col;
	float temp[4][4];

	for (row = 0; row < 4; row++)
		for (col = 0; col < 4; col++)
			temp[row][col] = (icand[row][0] * Vm[0][col] +
			                  icand[row][1] * Vm[1][col] +
			                  icand[row][2] * Vm[2][col] +
			                  icand[row][3] * Vm[3][col]);
	copy_m4_m4(Vm, temp);
}

void polarview_m4(float Vm[4][4], float dist, float azimuth, float incidence, float twist)
{
	unit_m4(Vm);

	translate_m4(Vm, 0.0, 0.0, -dist);
	rotate_m4(Vm, 'Z', -twist);
	rotate_m4(Vm, 'X', -incidence);
	rotate_m4(Vm, 'Z', -azimuth);
}

void lookat_m4(float mat[4][4], float vx, float vy, float vz, float px, float py, float pz, float twist)
{
	float sine, cosine, hyp, hyp1, dx, dy, dz;
	float mat1[4][4];

	unit_m4(mat);
	unit_m4(mat1);

	rotate_m4(mat, 'Z', -twist);

	dx = px - vx;
	dy = py - vy;
	dz = pz - vz;
	hyp = dx * dx + dz * dz; /* hyp squared */
	hyp1 = sqrtf(dy * dy + hyp);
	hyp = sqrtf(hyp); /* the real hyp */

	if (hyp1 != 0.0f) { /* rotate X */
		sine = -dy / hyp1;
		cosine = hyp / hyp1;
	}
	else {
		sine = 0.0f;
		cosine = 1.0f;
	}
	mat1[1][1] = cosine;
	mat1[1][2] = sine;
	mat1[2][1] = -sine;
	mat1[2][2] = cosine;

	i_multmatrix(mat1, mat);

	mat1[1][1] = mat1[2][2] = 1.0f; /* be careful here to reinit */
	mat1[1][2] = mat1[2][1] = 0.0f; /* those modified by the last */

	/* paragraph */
	if (hyp != 0.0f) { /* rotate Y */
		sine = dx / hyp;
		cosine = -dz / hyp;
	}
	else {
		sine = 0.0f;
		cosine = 1.0f;
	}
	mat1[0][0] = cosine;
	mat1[0][2] = -sine;
	mat1[2][0] = sine;
	mat1[2][2] = cosine;

	i_multmatrix(mat1, mat);
	translate_m4(mat, -vx, -vy, -vz); /* translate viewpoint to origin */
}

int box_clip_bounds_m4(float boundbox[2][3], const float bounds[4], float winmat[4][4])
{
	float mat[4][4], vec[4];
	int a, fl, flag = -1;

	copy_m4_m4(mat, winmat);

	for (a = 0; a < 8; a++) {
		vec[0] = (a & 1) ? boundbox[0][0] : boundbox[1][0];
		vec[1] = (a & 2) ? boundbox[0][1] : boundbox[1][1];
		vec[2] = (a & 4) ? boundbox[0][2] : boundbox[1][2];
		vec[3] = 1.0;
		mul_m4_v4(mat, vec);

		fl = 0;
		if (bounds) {
			if (vec[0] > bounds[1] * vec[3]) fl |= 1;
			if (vec[0] < bounds[0] * vec[3]) fl |= 2;
			if (vec[1] > bounds[3] * vec[3]) fl |= 4;
			if (vec[1] < bounds[2] * vec[3]) fl |= 8;
		}
		else {
			if (vec[0] < -vec[3]) fl |= 1;
			if (vec[0] > vec[3]) fl |= 2;
			if (vec[1] < -vec[3]) fl |= 4;
			if (vec[1] > vec[3]) fl |= 8;
		}
		if (vec[2] < -vec[3]) fl |= 16;
		if (vec[2] > vec[3]) fl |= 32;

		flag &= fl;
		if (flag == 0) return 0;
	}

	return flag;
}

void box_minmax_bounds_m4(float min[3], float max[3], float boundbox[2][3], float mat[4][4])
{
	float mn[3], mx[3], vec[3];
	int a;

	copy_v3_v3(mn, min);
	copy_v3_v3(mx, max);

	for (a = 0; a < 8; a++) {
		vec[0] = (a & 1) ? boundbox[0][0] : boundbox[1][0];
		vec[1] = (a & 2) ? boundbox[0][1] : boundbox[1][1];
		vec[2] = (a & 4) ? boundbox[0][2] : boundbox[1][2];

		mul_m4_v3(mat, vec);
		minmax_v3v3_v3(mn, mx, vec);
	}

	copy_v3_v3(min, mn);
	copy_v3_v3(max, mx);
}

/********************************** Mapping **********************************/

void map_to_tube(float *r_u, float *r_v, const float x, const float y, const float z)
{
	float len;

	*r_v = (z + 1.0f) / 2.0f;

	len = sqrtf(x * x + y * y);
	if (len > 0.0f) {
		*r_u = (float)((1.0 - (atan2(x / len, y / len) / M_PI)) / 2.0);
	}
	else {
		*r_v = *r_u = 0.0f; /* to avoid un-initialized variables */
	}
}

void map_to_sphere(float *r_u, float *r_v, const float x, const float y, const float z)
{
	float len;

	len = sqrtf(x * x + y * y + z * z);
	if (len > 0.0f) {
		if (x == 0.0f && y == 0.0f) *r_u = 0.0f;  /* othwise domain error */
		else *r_u = (1.0f - atan2f(x, y) / (float)M_PI) / 2.0f;

		*r_v = 1.0f - saacos(z / len) / (float)M_PI;
	}
	else {
		*r_v = *r_u = 0.0f; /* to avoid un-initialized variables */
	}
}

/********************************* Normals **********************************/

void accumulate_vertex_normals(float n1[3], float n2[3], float n3[3],
                               float n4[3], const float f_no[3], const float co1[3], const float co2[3],
                               const float co3[3], const float co4[3])
{
	float vdiffs[4][3];
	const int nverts = (n4 != NULL && co4 != NULL) ? 4 : 3;

	/* compute normalized edge vectors */
	sub_v3_v3v3(vdiffs[0], co2, co1);
	sub_v3_v3v3(vdiffs[1], co3, co2);

	if (nverts == 3) {
		sub_v3_v3v3(vdiffs[2], co1, co3);
	}
	else {
		sub_v3_v3v3(vdiffs[2], co4, co3);
		sub_v3_v3v3(vdiffs[3], co1, co4);
		normalize_v3(vdiffs[3]);
	}

	normalize_v3(vdiffs[0]);
	normalize_v3(vdiffs[1]);
	normalize_v3(vdiffs[2]);

	/* accumulate angle weighted face normal */
	{
		float *vn[] = {n1, n2, n3, n4};
		const float *prev_edge = vdiffs[nverts - 1];
		int i;

		for (i = 0; i < nverts; i++) {
			const float *cur_edge = vdiffs[i];
			const float fac = saacos(-dot_v3v3(cur_edge, prev_edge));

			/* accumulate */
			madd_v3_v3fl(vn[i], f_no, fac);
			prev_edge = cur_edge;
		}
	}
}

/* Add weighted face normal component into normals of the face vertices.
 * Caller must pass pre-allocated vdiffs of nverts length. */
void accumulate_vertex_normals_poly(float **vertnos, const float polyno[3],
                                    const float **vertcos, float vdiffs[][3], const int nverts)
{
	int i;

	/* calculate normalized edge directions for each edge in the poly */
	for (i = 0; i < nverts; i++) {
		sub_v3_v3v3(vdiffs[i], vertcos[(i + 1) % nverts], vertcos[i]);
		normalize_v3(vdiffs[i]);
	}

	/* accumulate angle weighted face normal */
	{
		const float *prev_edge = vdiffs[nverts - 1];

		for (i = 0; i < nverts; i++) {
			const float *cur_edge = vdiffs[i];

			/* calculate angle between the two poly edges incident on
			 * this vertex */
			const float fac = saacos(-dot_v3v3(cur_edge, prev_edge));

			/* accumulate */
			madd_v3_v3fl(vertnos[i], polyno, fac);
			prev_edge = cur_edge;
		}
	}
}

/********************************* Tangents **********************************/

void tangent_from_uv(float uv1[2], float uv2[2], float uv3[3], float co1[3], float co2[3], float co3[3], float n[3], float tang[3])
{
	const float s1 = uv2[0] - uv1[0];
	const float s2 = uv3[0] - uv1[0];
	const float t1 = uv2[1] - uv1[1];
	const float t2 = uv3[1] - uv1[1];
	float det = (s1 * t2 - s2 * t1);

	if (det != 0.0f) { /* otherwise 'tang' becomes nan */
		float tangv[3], ct[3], e1[3], e2[3];

		det = 1.0f / det;

		/* normals in render are inversed... */
		sub_v3_v3v3(e1, co1, co2);
		sub_v3_v3v3(e2, co1, co3);
		tang[0] = (t2 * e1[0] - t1 * e2[0]) * det;
		tang[1] = (t2 * e1[1] - t1 * e2[1]) * det;
		tang[2] = (t2 * e1[2] - t1 * e2[2]) * det;
		tangv[0] = (s1 * e2[0] - s2 * e1[0]) * det;
		tangv[1] = (s1 * e2[1] - s2 * e1[1]) * det;
		tangv[2] = (s1 * e2[2] - s2 * e1[2]) * det;
		cross_v3_v3v3(ct, tang, tangv);

		/* check flip */
		if (dot_v3v3(ct, n) < 0.0f) {
			negate_v3(tang);
		}
	}
	else {
		tang[0] = tang[1] = tang[2] = 0.0f;
	}
}

/****************************** Vector Clouds ********************************/

/* vector clouds */
/* void vcloud_estimate_transform(int list_size, float (*pos)[3], float *weight, float (*rpos)[3], float *rweight,
 *                                float lloc[3], float rloc[3], float lrot[3][3], float lscale[3][3])
 *
 * input
 * (
 * int list_size
 * 4 lists as pointer to array[list_size]
 * 1. current pos array of 'new' positions
 * 2. current weight array of 'new'weights (may be NULL pointer if you have no weights )
 * 3. reference rpos array of 'old' positions
 * 4. reference rweight array of 'old'weights (may be NULL pointer if you have no weights )
 * )
 * output
 * (
 * float lloc[3] center of mass pos
 * float rloc[3] center of mass rpos
 * float lrot[3][3] rotation matrix
 * float lscale[3][3] scale matrix
 * pointers may be NULL if not needed
 * )
 */

void vcloud_estimate_transform(int list_size, float (*pos)[3], float *weight, float (*rpos)[3], float *rweight,
                               float lloc[3], float rloc[3], float lrot[3][3], float lscale[3][3])
{
	float accu_com[3] = {0.0f, 0.0f, 0.0f}, accu_rcom[3] = {0.0f, 0.0f, 0.0f};
	float accu_weight = 0.0f, accu_rweight = 0.0f;
	const float eps = 1e-6f;

	int a;
	/* first set up a nice default response */
	if (lloc) zero_v3(lloc);
	if (rloc) zero_v3(rloc);
	if (lrot) unit_m3(lrot);
	if (lscale) unit_m3(lscale);
	/* do com for both clouds */
	if (pos && rpos && (list_size > 0)) { /* paranoya check */
		/* do com for both clouds */
		for (a = 0; a < list_size; a++) {
			if (weight) {
				float v[3];
				copy_v3_v3(v, pos[a]);
				mul_v3_fl(v, weight[a]);
				add_v3_v3(accu_com, v);
				accu_weight += weight[a];
			}
			else {
				add_v3_v3(accu_com, pos[a]);
			}

			if (rweight) {
				float v[3];
				copy_v3_v3(v, rpos[a]);
				mul_v3_fl(v, rweight[a]);
				add_v3_v3(accu_rcom, v);
				accu_rweight += rweight[a];
			}
			else {
				add_v3_v3(accu_rcom, rpos[a]);
			}
		}
		if (!weight || !rweight) {
			accu_weight = accu_rweight = (float)list_size;
		}

		mul_v3_fl(accu_com, 1.0f / accu_weight);
		mul_v3_fl(accu_rcom, 1.0f / accu_rweight);
		if (lloc) copy_v3_v3(lloc, accu_com);
		if (rloc) copy_v3_v3(rloc, accu_rcom);
		if (lrot || lscale) { /* caller does not want rot nor scale, strange but legal */
			/*so now do some reverse engineering and see if we can split rotation from scale ->Polardecompose*/
			/* build 'projection' matrix */
			float m[3][3], mr[3][3], q[3][3], qi[3][3];
			float va[3], vb[3], stunt[3];
			float odet, ndet;
			int i = 0, imax = 15;
			zero_m3(m);
			zero_m3(mr);

			/* build 'projection' matrix */
			for (a = 0; a < list_size; a++) {
				sub_v3_v3v3(va, rpos[a], accu_rcom);
				/* mul_v3_fl(va, bp->mass);  mass needs renormalzation here ?? */
				sub_v3_v3v3(vb, pos[a], accu_com);
				/* mul_v3_fl(va, rp->mass); */
				m[0][0] += va[0] * vb[0];
				m[0][1] += va[0] * vb[1];
				m[0][2] += va[0] * vb[2];

				m[1][0] += va[1] * vb[0];
				m[1][1] += va[1] * vb[1];
				m[1][2] += va[1] * vb[2];

				m[2][0] += va[2] * vb[0];
				m[2][1] += va[2] * vb[1];
				m[2][2] += va[2] * vb[2];

				/* building the reference matrix on the fly
				 * needed to scale properly later */

				mr[0][0] += va[0] * va[0];
				mr[0][1] += va[0] * va[1];
				mr[0][2] += va[0] * va[2];

				mr[1][0] += va[1] * va[0];
				mr[1][1] += va[1] * va[1];
				mr[1][2] += va[1] * va[2];

				mr[2][0] += va[2] * va[0];
				mr[2][1] += va[2] * va[1];
				mr[2][2] += va[2] * va[2];
			}
			copy_m3_m3(q, m);
			stunt[0] = q[0][0];
			stunt[1] = q[1][1];
			stunt[2] = q[2][2];
			/* renormalizing for numeric stability */
			mul_m3_fl(q, 1.f / len_v3(stunt));

			/* this is pretty much Polardecompose 'inline' the algo based on Higham's thesis */
			/* without the far case ... but seems to work here pretty neat                   */
			odet = 0.0f;
			ndet = determinant_m3_array(q);
			while ((odet - ndet) * (odet - ndet) > eps && i < imax) {
				invert_m3_m3(qi, q);
				transpose_m3(qi);
				add_m3_m3m3(q, q, qi);
				mul_m3_fl(q, 0.5f);
				odet = ndet;
				ndet = determinant_m3_array(q);
				i++;
			}

			if (i) {
				float scale[3][3];
				float irot[3][3];
				if (lrot) copy_m3_m3(lrot, q);
				invert_m3_m3(irot, q);
				invert_m3_m3(qi, mr);
				mul_m3_m3m3(q, m, qi);
				mul_m3_m3m3(scale, irot, q);
				if (lscale) copy_m3_m3(lscale, scale);

			}
		}
	}
}

/******************************* Form Factor *********************************/

static void vec_add_dir(float r[3], const float v1[3], const float v2[3], const float fac)
{
	r[0] = v1[0] + fac * (v2[0] - v1[0]);
	r[1] = v1[1] + fac * (v2[1] - v1[1]);
	r[2] = v1[2] + fac * (v2[2] - v1[2]);
}

bool form_factor_visible_quad(const float p[3], const float n[3],
                              const float v0[3], const float v1[3], const float v2[3],
                              float q0[3], float q1[3], float q2[3], float q3[3])
{
	static const float epsilon = 1e-6f;
	float sd[3];
	const float c = dot_v3v3(n, p);

	/* signed distances from the vertices to the plane. */
	sd[0] = dot_v3v3(n, v0) - c;
	sd[1] = dot_v3v3(n, v1) - c;
	sd[2] = dot_v3v3(n, v2) - c;

	if (fabsf(sd[0]) < epsilon) sd[0] = 0.0f;
	if (fabsf(sd[1]) < epsilon) sd[1] = 0.0f;
	if (fabsf(sd[2]) < epsilon) sd[2] = 0.0f;

	if (sd[0] > 0.0f) {
		if (sd[1] > 0.0f) {
			if (sd[2] > 0.0f) {
				/* +++ */
				copy_v3_v3(q0, v0);
				copy_v3_v3(q1, v1);
				copy_v3_v3(q2, v2);
				copy_v3_v3(q3, q2);
			}
			else if (sd[2] < 0.0f) {
				/* ++- */
				copy_v3_v3(q0, v0);
				copy_v3_v3(q1, v1);
				vec_add_dir(q2, v1, v2, (sd[1] / (sd[1] - sd[2])));
				vec_add_dir(q3, v0, v2, (sd[0] / (sd[0] - sd[2])));
			}
			else {
				/* ++0 */
				copy_v3_v3(q0, v0);
				copy_v3_v3(q1, v1);
				copy_v3_v3(q2, v2);
				copy_v3_v3(q3, q2);
			}
		}
		else if (sd[1] < 0.0f) {
			if (sd[2] > 0.0f) {
				/* +-+ */
				copy_v3_v3(q0, v0);
				vec_add_dir(q1, v0, v1, (sd[0] / (sd[0] - sd[1])));
				vec_add_dir(q2, v1, v2, (sd[1] / (sd[1] - sd[2])));
				copy_v3_v3(q3, v2);
			}
			else if (sd[2] < 0.0f) {
				/* +-- */
				copy_v3_v3(q0, v0);
				vec_add_dir(q1, v0, v1, (sd[0] / (sd[0] - sd[1])));
				vec_add_dir(q2, v0, v2, (sd[0] / (sd[0] - sd[2])));
				copy_v3_v3(q3, q2);
			}
			else {
				/* +-0 */
				copy_v3_v3(q0, v0);
				vec_add_dir(q1, v0, v1, (sd[0] / (sd[0] - sd[1])));
				copy_v3_v3(q2, v2);
				copy_v3_v3(q3, q2);
			}
		}
		else {
			if (sd[2] > 0.0f) {
				/* +0+ */
				copy_v3_v3(q0, v0);
				copy_v3_v3(q1, v1);
				copy_v3_v3(q2, v2);
				copy_v3_v3(q3, q2);
			}
			else if (sd[2] < 0.0f) {
				/* +0- */
				copy_v3_v3(q0, v0);
				copy_v3_v3(q1, v1);
				vec_add_dir(q2, v0, v2, (sd[0] / (sd[0] - sd[2])));
				copy_v3_v3(q3, q2);
			}
			else {
				/* +00 */
				copy_v3_v3(q0, v0);
				copy_v3_v3(q1, v1);
				copy_v3_v3(q2, v2);
				copy_v3_v3(q3, q2);
			}
		}
	}
	else if (sd[0] < 0.0f) {
		if (sd[1] > 0.0f) {
			if (sd[2] > 0.0f) {
				/* -++ */
				vec_add_dir(q0, v0, v1, (sd[0] / (sd[0] - sd[1])));
				copy_v3_v3(q1, v1);
				copy_v3_v3(q2, v2);
				vec_add_dir(q3, v0, v2, (sd[0] / (sd[0] - sd[2])));
			}
			else if (sd[2] < 0.0f) {
				/* -+- */
				vec_add_dir(q0, v0, v1, (sd[0] / (sd[0] - sd[1])));
				copy_v3_v3(q1, v1);
				vec_add_dir(q2, v1, v2, (sd[1] / (sd[1] - sd[2])));
				copy_v3_v3(q3, q2);
			}
			else {
				/* -+0 */
				vec_add_dir(q0, v0, v1, (sd[0] / (sd[0] - sd[1])));
				copy_v3_v3(q1, v1);
				copy_v3_v3(q2, v2);
				copy_v3_v3(q3, q2);
			}
		}
		else if (sd[1] < 0.0f) {
			if (sd[2] > 0.0f) {
				/* --+ */
				vec_add_dir(q0, v0, v2, (sd[0] / (sd[0] - sd[2])));
				vec_add_dir(q1, v1, v2, (sd[1] / (sd[1] - sd[2])));
				copy_v3_v3(q2, v2);
				copy_v3_v3(q3, q2);
			}
			else if (sd[2] < 0.0f) {
				/* --- */
				return false;
			}
			else {
				/* --0 */
				return false;
			}
		}
		else {
			if (sd[2] > 0.0f) {
				/* -0+ */
				vec_add_dir(q0, v0, v2, (sd[0] / (sd[0] - sd[2])));
				copy_v3_v3(q1, v1);
				copy_v3_v3(q2, v2);
				copy_v3_v3(q3, q2);
			}
			else if (sd[2] < 0.0f) {
				/* -0- */
				return false;
			}
			else {
				/* -00 */
				return false;
			}
		}
	}
	else {
		if (sd[1] > 0.0f) {
			if (sd[2] > 0.0f) {
				/* 0++ */
				copy_v3_v3(q0, v0);
				copy_v3_v3(q1, v1);
				copy_v3_v3(q2, v2);
				copy_v3_v3(q3, q2);
			}
			else if (sd[2] < 0.0f) {
				/* 0+- */
				copy_v3_v3(q0, v0);
				copy_v3_v3(q1, v1);
				vec_add_dir(q2, v1, v2, (sd[1] / (sd[1] - sd[2])));
				copy_v3_v3(q3, q2);
			}
			else {
				/* 0+0 */
				copy_v3_v3(q0, v0);
				copy_v3_v3(q1, v1);
				copy_v3_v3(q2, v2);
				copy_v3_v3(q3, q2);
			}
		}
		else if (sd[1] < 0.0f) {
			if (sd[2] > 0.0f) {
				/* 0-+ */
				copy_v3_v3(q0, v0);
				vec_add_dir(q1, v1, v2, (sd[1] / (sd[1] - sd[2])));
				copy_v3_v3(q2, v2);
				copy_v3_v3(q3, q2);
			}
			else if (sd[2] < 0.0f) {
				/* 0-- */
				return false;
			}
			else {
				/* 0-0 */
				return false;
			}
		}
		else {
			if (sd[2] > 0.0f) {
				/* 00+ */
				copy_v3_v3(q0, v0);
				copy_v3_v3(q1, v1);
				copy_v3_v3(q2, v2);
				copy_v3_v3(q3, q2);
			}
			else if (sd[2] < 0.0f) {
				/* 00- */
				return false;
			}
			else {
				/* 000 */
				return false;
			}
		}
	}

	return true;
}

/* altivec optimization, this works, but is unused */

#if 0
#include <Accelerate/Accelerate.h>

typedef union {
	vFloat v;
	float f[4];
} vFloatResult;

static vFloat vec_splat_float(float val)
{
	return (vFloat) {val, val, val, val};
}

static float ff_quad_form_factor(float *p, float *n, float *q0, float *q1, float *q2, float *q3)
{
	vFloat vcos, rlen, vrx, vry, vrz, vsrx, vsry, vsrz, gx, gy, gz, vangle;
	vUInt8 rotate = (vUInt8) {4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 0, 1, 2, 3};
	vFloatResult vresult;
	float result;

	/* compute r* */
	vrx = (vFloat) {q0[0], q1[0], q2[0], q3[0]} -vec_splat_float(p[0]);
	vry = (vFloat) {q0[1], q1[1], q2[1], q3[1]} -vec_splat_float(p[1]);
	vrz = (vFloat) {q0[2], q1[2], q2[2], q3[2]} -vec_splat_float(p[2]);

	/* normalize r* */
	rlen = vec_rsqrte(vrx * vrx + vry * vry + vrz * vrz + vec_splat_float(1e-16f));
	vrx = vrx * rlen;
	vry = vry * rlen;
	vrz = vrz * rlen;

	/* rotate r* for cross and dot */
	vsrx = vec_perm(vrx, vrx, rotate);
	vsry = vec_perm(vry, vry, rotate);
	vsrz = vec_perm(vrz, vrz, rotate);

	/* cross product */
	gx = vsry * vrz - vsrz * vry;
	gy = vsrz * vrx - vsrx * vrz;
	gz = vsrx * vry - vsry * vrx;

	/* normalize */
	rlen = vec_rsqrte(gx * gx + gy * gy + gz * gz + vec_splat_float(1e-16f));
	gx = gx * rlen;
	gy = gy * rlen;
	gz = gz * rlen;

	/* angle */
	vcos = vrx * vsrx + vry * vsry + vrz * vsrz;
	vcos = vec_max(vec_min(vcos, vec_splat_float(1.0f)), vec_splat_float(-1.0f));
	vangle = vacosf(vcos);

	/* dot */
	vresult.v = (vec_splat_float(n[0]) * gx +
	             vec_splat_float(n[1]) * gy +
	             vec_splat_float(n[2]) * gz) * vangle;

	result = (vresult.f[0] + vresult.f[1] + vresult.f[2] + vresult.f[3]) * (0.5f / (float)M_PI);
	result = MAX2(result, 0.0f);

	return result;
}

#endif

/* SSE optimization, acos code doesn't work */

#if 0

#include <xmmintrin.h>

static __m128 sse_approx_acos(__m128 x)
{
	/* needs a better approximation than taylor expansion of acos, since that
	 * gives big errors for near 1.0 values, sqrt(2 * x) * acos(1 - x) should work
	 * better, see http://www.tom.womack.net/projects/sse-fast-arctrig.html */

	return _mm_set_ps1(1.0f);
}

static float ff_quad_form_factor(float *p, float *n, float *q0, float *q1, float *q2, float *q3)
{
	float r0[3], r1[3], r2[3], r3[3], g0[3], g1[3], g2[3], g3[3];
	float a1, a2, a3, a4, dot1, dot2, dot3, dot4, result;
	float fresult[4] __attribute__((aligned(16)));
	__m128 qx, qy, qz, rx, ry, rz, rlen, srx, sry, srz, gx, gy, gz, glen, rcos, angle, aresult;

	/* compute r */
	qx = _mm_set_ps(q3[0], q2[0], q1[0], q0[0]);
	qy = _mm_set_ps(q3[1], q2[1], q1[1], q0[1]);
	qz = _mm_set_ps(q3[2], q2[2], q1[2], q0[2]);

	rx = qx - _mm_set_ps1(p[0]);
	ry = qy - _mm_set_ps1(p[1]);
	rz = qz - _mm_set_ps1(p[2]);

	/* normalize r */
	rlen = _mm_rsqrt_ps(rx * rx + ry * ry + rz * rz + _mm_set_ps1(1e-16f));
	rx = rx * rlen;
	ry = ry * rlen;
	rz = rz * rlen;

	/* cross product */
	srx = _mm_shuffle_ps(rx, rx, _MM_SHUFFLE(0, 3, 2, 1));
	sry = _mm_shuffle_ps(ry, ry, _MM_SHUFFLE(0, 3, 2, 1));
	srz = _mm_shuffle_ps(rz, rz, _MM_SHUFFLE(0, 3, 2, 1));

	gx = sry * rz - srz * ry;
	gy = srz * rx - srx * rz;
	gz = srx * ry - sry * rx;

	/* normalize g */
	glen = _mm_rsqrt_ps(gx * gx + gy * gy + gz * gz + _mm_set_ps1(1e-16f));
	gx = gx * glen;
	gy = gy * glen;
	gz = gz * glen;

	/* compute angle */
	rcos = rx * srx + ry * sry + rz * srz;
	rcos = _mm_max_ps(_mm_min_ps(rcos, _mm_set_ps1(1.0f)), _mm_set_ps1(-1.0f));

	angle = sse_approx_cos(rcos);
	aresult = (_mm_set_ps1(n[0]) * gx + _mm_set_ps1(n[1]) * gy + _mm_set_ps1(n[2]) * gz) * angle;

	/* sum together */
	result = (fresult[0] + fresult[1] + fresult[2] + fresult[3]) * (0.5f / (float)M_PI);
	result = MAX2(result, 0.0f);

	return result;
}

#endif

static void ff_normalize(float n[3])
{
	float d;

	d = dot_v3v3(n, n);

	if (d > 1.0e-35f) {
		d = 1.0f / sqrtf(d);

		n[0] *= d;
		n[1] *= d;
		n[2] *= d;
	}
}

float form_factor_quad(const float p[3], const float n[3],
                       const float q0[3], const float q1[3], const float q2[3], const float q3[3])
{
	float r0[3], r1[3], r2[3], r3[3], g0[3], g1[3], g2[3], g3[3];
	float a1, a2, a3, a4, dot1, dot2, dot3, dot4, result;

	sub_v3_v3v3(r0, q0, p);
	sub_v3_v3v3(r1, q1, p);
	sub_v3_v3v3(r2, q2, p);
	sub_v3_v3v3(r3, q3, p);

	ff_normalize(r0);
	ff_normalize(r1);
	ff_normalize(r2);
	ff_normalize(r3);

	cross_v3_v3v3(g0, r1, r0);
	ff_normalize(g0);
	cross_v3_v3v3(g1, r2, r1);
	ff_normalize(g1);
	cross_v3_v3v3(g2, r3, r2);
	ff_normalize(g2);
	cross_v3_v3v3(g3, r0, r3);
	ff_normalize(g3);

	a1 = saacosf(dot_v3v3(r0, r1));
	a2 = saacosf(dot_v3v3(r1, r2));
	a3 = saacosf(dot_v3v3(r2, r3));
	a4 = saacosf(dot_v3v3(r3, r0));

	dot1 = dot_v3v3(n, g0);
	dot2 = dot_v3v3(n, g1);
	dot3 = dot_v3v3(n, g2);
	dot4 = dot_v3v3(n, g3);

	result = (a1 * dot1 + a2 * dot2 + a3 * dot3 + a4 * dot4) * 0.5f / (float)M_PI;
	result = MAX2(result, 0.0f);

	return result;
}

float form_factor_hemi_poly(float p[3], float n[3], float v1[3], float v2[3], float v3[3], float v4[3])
{
	/* computes how much hemisphere defined by point and normal is
	 * covered by a quad or triangle, cosine weighted */
	float q0[3], q1[3], q2[3], q3[3], contrib = 0.0f;

	if (form_factor_visible_quad(p, n, v1, v2, v3, q0, q1, q2, q3))
		contrib += form_factor_quad(p, n, q0, q1, q2, q3);

	if (v4 && form_factor_visible_quad(p, n, v1, v3, v4, q0, q1, q2, q3))
		contrib += form_factor_quad(p, n, q0, q1, q2, q3);

	return contrib;
}

/* evaluate if entire quad is a proper convex quad */
bool is_quad_convex_v3(const float v1[3], const float v2[3], const float v3[3], const float v4[3])
{
	float nor[3], nor_a[3], nor_b[3], vec[4][2];
	float mat[3][3];
	const bool is_ok_a = (normal_tri_v3(nor_a, v1, v2, v3) > FLT_EPSILON);
	const bool is_ok_b = (normal_tri_v3(nor_b, v1, v3, v4) > FLT_EPSILON);

	/* define projection, do both trias apart, quad is undefined! */

	/* check normal length incase one size is zero area */
	if (is_ok_a) {
		if (is_ok_b) {
			/* use both, most common outcome */

			/* when the face is folded over as 2 tris we probably don't want to create
			 * a quad from it, but go ahead with the intersection test since this
			 * isn't a function for degenerate faces */
			if (UNLIKELY(dot_v3v3(nor_a, nor_b) < 0.0f)) {
				/* flip so adding normals in the opposite direction
				 * doesn't give a zero length vector */
				negate_v3(nor_b);
			}

			add_v3_v3v3(nor, nor_a, nor_b);
			normalize_v3(nor);
		}
		else {
			copy_v3_v3(nor, nor_a);  /* only 'a' */
		}
	}
	else {
		if (is_ok_b) {
			copy_v3_v3(nor, nor_b);  /* only 'b' */
		}
		else {
			return false;  /* both zero, we can't do anything useful here */
		}
	}

	axis_dominant_v3_to_m3(mat, nor);

	mul_v2_m3v3(vec[0], mat, v1);
	mul_v2_m3v3(vec[1], mat, v2);
	mul_v2_m3v3(vec[2], mat, v3);
	mul_v2_m3v3(vec[3], mat, v4);

	/* linetests, the 2 diagonals have to instersect to be convex */
	return (isect_line_line_v2(vec[0], vec[2], vec[1], vec[3]) > 0);
}

bool is_quad_convex_v2(const float v1[2], const float v2[2], const float v3[2], const float v4[2])
{
	/* linetests, the 2 diagonals have to instersect to be convex */
	return (isect_line_line_v2(v1, v3, v2, v4) > 0);
}

bool is_poly_convex_v2(const float verts[][2], unsigned int nr)
{
	unsigned int sign_flag = 0;
	unsigned int a;
	const float *co_curr, *co_prev;
	float dir_curr[2], dir_prev[2];

	co_prev = verts[nr - 1];
	co_curr = verts[0];

	sub_v2_v2v2(dir_prev, verts[nr - 2], co_prev);

	for (a = 0; a < nr; a++) {
		float cross;

		sub_v2_v2v2(dir_curr, co_prev, co_curr);

		cross = cross_v2v2(dir_prev, dir_curr);

		if (cross < 0.0f) {
			sign_flag |= 1;
		}
		else if (cross > 0.0f) {
			sign_flag |= 2;
		}

		if (sign_flag == (1 | 2)) {
			return false;
		}

		copy_v2_v2(dir_prev, dir_curr);

		co_prev = co_curr;
		co_curr += 2;
	}

	return true;
}
