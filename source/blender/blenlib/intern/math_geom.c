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
#include "BLI_memarena.h"
#include "BLI_utildefines.h"

/********************************** Polygons *********************************/

void cent_tri_v3(float cent[3], const float v1[3], const float v2[3], const float v3[3])
{
	cent[0] = 0.33333f * (v1[0] + v2[0] + v3[0]);
	cent[1] = 0.33333f * (v1[1] + v2[1] + v3[1]);
	cent[2] = 0.33333f * (v1[2] + v2[2] + v3[2]);
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

float area_tri_v2(const float v1[2], const float v2[2], const float v3[2])
{
	return 0.5f * fabsf((v1[0] - v2[0]) * (v2[1] - v3[1]) + (v1[1] - v2[1]) * (v3[0] - v2[0]));
}

float area_tri_signed_v2(const float v1[2], const float v2[2], const float v3[2])
{
	return 0.5f * ((v1[0] - v2[0]) * (v2[1] - v3[1]) + (v1[1] - v2[1]) * (v3[0] - v2[0]));
}

/* only convex Quadrilaterals */
float area_quad_v3(const float v1[3], const float v2[3], const float v3[3], const float v4[3])
{
	float len, vec1[3], vec2[3], n[3];

	sub_v3_v3v3(vec1, v2, v1);
	sub_v3_v3v3(vec2, v4, v1);
	cross_v3_v3v3(n, vec1, vec2);
	len = normalize_v3(n);

	sub_v3_v3v3(vec1, v4, v3);
	sub_v3_v3v3(vec2, v2, v3);
	cross_v3_v3v3(n, vec1, vec2);
	len += normalize_v3(n);

	return (len / 2.0f);
}

/* Triangles */
float area_tri_v3(const float v1[3], const float v2[3], const float v3[3])
{
	float len, vec1[3], vec2[3], n[3];

	sub_v3_v3v3(vec1, v3, v2);
	sub_v3_v3v3(vec2, v1, v2);
	cross_v3_v3v3(n, vec1, vec2);
	len = normalize_v3(n);

	return (len / 2.0f);
}

float area_poly_v3(int nr, float verts[][3], const float normal[3])
{
	float x, y, z, area, max;
	float *cur, *prev;
	int a, px = 0, py = 1;

	/* first: find dominant axis: 0==X, 1==Y, 2==Z
	 * don't use 'axis_dominant_v3()' because we need max axis too */
	x = fabsf(normal[0]);
	y = fabsf(normal[1]);
	z = fabsf(normal[2]);
	max = MAX3(x, y, z);
	if (max == y) py = 2;
	else if (max == x) {
		px = 1;
		py = 2;
	}

	/* The Trapezium Area Rule */
	prev = verts[nr - 1];
	cur = verts[0];
	area = 0;
	for (a = 0; a < nr; a++) {
		area += (cur[px] - prev[px]) * (cur[py] + prev[py]);
		prev = verts[a];
		cur = verts[a + 1];
	}

	return fabsf(0.5f * area / max);
}

/********************************* Distance **********************************/

/* distance p to line v1-v2
 * using Hesse formula, NO LINE PIECE! */
float dist_to_line_v2(const float p[2], const float l1[2], const float l2[2])
{
	float a[2], deler;

	a[0] = l1[1] - l2[1];
	a[1] = l2[0] - l1[0];
	deler = (float)sqrt(a[0] * a[0] + a[1] * a[1]);
	if (deler == 0.0f) return 0;

	return fabsf((p[0] - l1[0]) * a[0] + (p[1] - l1[1]) * a[1]) / deler;

}

/* distance p to line-piece v1-v2 */
float dist_squared_to_line_segment_v2(const float p[2], const float l1[2], const float l2[2])
{
	float labda, rc[2], pt[2], len;

	rc[0] = l2[0] - l1[0];
	rc[1] = l2[1] - l1[1];
	len = rc[0] * rc[0] + rc[1] * rc[1];
	if (len == 0.0f) {
		rc[0] = p[0] - l1[0];
		rc[1] = p[1] - l1[1];
		return (float)(sqrt(rc[0] * rc[0] + rc[1] * rc[1]));
	}

	labda = (rc[0] * (p[0] - l1[0]) + rc[1] * (p[1] - l1[1])) / len;
	if (labda <= 0.0f) {
		pt[0] = l1[0];
		pt[1] = l1[1];
	}
	else if (labda >= 1.0f) {
		pt[0] = l2[0];
		pt[1] = l2[1];
	}
	else {
		pt[0] = labda * rc[0] + l1[0];
		pt[1] = labda * rc[1] + l1[1];
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
void closest_to_line_segment_v2(float close_r[2], const float p[2], const float l1[2], const float l2[2])
{
	float lambda, cp[2];

	lambda = closest_to_line_v2(cp, p, l1, l2);

	if (lambda <= 0.0f)
		copy_v2_v2(close_r, l1);
	else if (lambda >= 1.0f)
		copy_v2_v2(close_r, l2);
	else
		copy_v2_v2(close_r, cp);
}

/* point closest to v1 on line v2-v3 in 3D */
void closest_to_line_segment_v3(float close_r[3], const float v1[3], const float v2[3], const float v3[3])
{
	float lambda, cp[3];

	lambda = closest_to_line_v3(cp, v1, v2, v3);

	if (lambda <= 0.0f)
		copy_v3_v3(close_r, v2);
	else if (lambda >= 1.0f)
		copy_v3_v3(close_r, v3);
	else
		copy_v3_v3(close_r, cp);
}

/* find the closest point on a plane to another point and store it in close_r
 * close_r:       return coordinate
 * plane_co:      a point on the plane
 * plane_no_unit: the plane's normal, and d is the last number in the plane equation 0 = ax + by + cz + d
 * pt:            the point that you want the nearest of
 */

void closest_to_plane_v3(float close_r[3], const float plane_co[3], const float plane_no_unit[3], const float pt[3])
{
	float temp[3];
	float dotprod;

	sub_v3_v3v3(temp, pt, plane_co);
	dotprod = dot_v3v3(temp, plane_no_unit);

	close_r[0] = pt[0] - (plane_no_unit[0] * dotprod);
	close_r[1] = pt[1] - (plane_no_unit[1] * dotprod);
	close_r[2] = pt[2] - (plane_no_unit[2] * dotprod);
}

/* signed distance from the point to the plane in 3D */
float dist_to_plane_normalized_v3(const float p[3], const float plane_co[3], const float plane_no_unit[3])
{
	float plane_co_other[3];

	add_v3_v3v3(plane_co_other, plane_co, plane_no_unit);

	return line_point_factor_v3(p, plane_co, plane_co_other);
}

float dist_to_plane_v3(const float p[3], const float plane_co[3], const float plane_no[3])
{
	float plane_no_unit[3];
	float plane_co_other[3];

	normalize_v3_v3(plane_no_unit, plane_no);
	add_v3_v3v3(plane_co_other, plane_co, plane_no_unit);

	return line_point_factor_v3(p, plane_co, plane_co_other);
}

/* distance v1 to line-piece v2-v3 in 3D */
float dist_to_line_segment_v3(const float v1[3], const float v2[3], const float v3[3])
{
	float closest[3];

	closest_to_line_segment_v3(closest, v1, v2, v3);

	return len_v3v3(closest, v1);
}

/******************************* Intersection ********************************/

/* intersect Line-Line, shorts */
int isect_line_line_v2_int(const int v1[2], const int v2[2], const int v3[2], const int v4[2])
{
	float div, labda, mu;

	div = (float)((v2[0] - v1[0]) * (v4[1] - v3[1]) - (v2[1] - v1[1]) * (v4[0] - v3[0]));
	if (div == 0.0f) return ISECT_LINE_LINE_COLINEAR;

	labda = ((float)(v1[1] - v3[1]) * (v4[0] - v3[0]) - (v1[0] - v3[0]) * (v4[1] - v3[1])) / div;

	mu = ((float)(v1[1] - v3[1]) * (v2[0] - v1[0]) - (v1[0] - v3[0]) * (v2[1] - v1[1])) / div;

	if (labda >= 0.0f && labda <= 1.0f && mu >= 0.0f && mu <= 1.0f) {
		if (labda == 0.0f || labda == 1.0f || mu == 0.0f || mu == 1.0f) return ISECT_LINE_LINE_EXACT;
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
	float div, labda, mu;

	div = (v2[0] - v1[0]) * (v4[1] - v3[1]) - (v2[1] - v1[1]) * (v4[0] - v3[0]);
	if (div == 0.0f) return ISECT_LINE_LINE_COLINEAR;

	labda = ((float)(v1[1] - v3[1]) * (v4[0] - v3[0]) - (v1[0] - v3[0]) * (v4[1] - v3[1])) / div;

	mu = ((float)(v1[1] - v3[1]) * (v2[0] - v1[0]) - (v1[0] - v3[0]) * (v2[1] - v1[1])) / div;

	if (labda >= 0.0f && labda <= 1.0f && mu >= 0.0f && mu <= 1.0f) {
		if (labda == 0.0f || labda == 1.0f || mu == 0.0f || mu == 1.0f) return ISECT_LINE_LINE_EXACT;
		return ISECT_LINE_LINE_CROSS;
	}
	return ISECT_LINE_LINE_NONE;
}

/* get intersection point of two 2D segments and return intersection type:
 *  -1: colliniar
 *   1: intersection
 */
int isect_seg_seg_v2_point(const float v1[2], const float v2[2], const float v3[2], const float v4[2], float vi[2])
{
	float a1, a2, b1, b2, c1, c2, d;
	float u, v;
	const float eps = 0.000001f;

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

			if (len_v2v2(v1, v2) == 0.0f) {
				if (len_v2v2(v3, v4) > eps) {
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
			else if (maxf(0.0f, u) == minf(1.0f, u2)) { /* one common point: can return result */
				interp_v2_v2v2(vi, v1, v2, maxf(0, u));
				return 1;
			}
		}

		/* lines are colliniar */
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

	const float a = dot_v3v3(ldir, ldir);

	const float b = 2.0f *
	                (ldir[0] * (l1[0] - sp[0]) +
	                 ldir[1] * (l1[1] - sp[1]) +
	                 ldir[2] * (l1[2] - sp[2]));

	const float c =
	    dot_v3v3(sp, sp) +
	    dot_v3v3(l1, l1) -
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
		const float i_sqrt = sqrt(i); /* avoid calc twice */

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
		const float i_sqrt = sqrt(i); /* avoid calc twice */

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

/*
 * -1: colliniar
 *  1: intersection
 */
static short IsectLLPt2Df(const float x0, const float y0, const float x1, const float y1,
                          const float x2, const float y2, const float x3, const float y3, float *xi, float *yi)

{
	/*
	 * this function computes the intersection of the sent lines
	 * and returns the intersection point, note that the function assumes
	 * the lines intersect. the function can handle vertical as well
	 * as horizontal lines. note the function isn't very clever, it simply
	 * applies the math, but we don't need speed since this is a
	 * pre-processing step
	 */
	float c1, c2; /* constants of linear equations */
	float det_inv; /* the inverse of the determinant of the coefficient */
	float m1, m2; /* the slopes of each line */
	/*
	 * compute slopes, note the cludge for infinity, however, this will
	 * be close enough
	 */
	if (fabs(x1 - x0) > 0.000001f)
		m1 = (y1 - y0) / (x1 - x0);
	else
		return -1; /*m1 = (float)1e+10;*/ /* close enough to infinity */

	if (fabs(x3 - x2) > 0.000001f)
		m2 = (y3 - y2) / (x3 - x2);
	else
		return -1; /*m2 = (float)1e+10;*/ /* close enough to infinity */

	if (fabs(m1 - m2) < 0.000001f)
		return -1;  /* parallel lines */

	/* compute constants */

	c1 = (y0 - m1 * x0);
	c2 = (y2 - m2 * x2);

	/* compute the inverse of the determinate */

	det_inv = 1.0f / (-m1 + m2);

	/* use Kramers rule to compute xi and yi */

	*xi = ((-c2 + c1) * det_inv);
	*yi = ((m2 * c1 - m1 * c2) * det_inv);

	return 1;
}

/* point in tri */

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
int isect_line_tri_v3(const float p1[3], const float p2[3],
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

/* moved from effect.c
 * test if the ray starting at p1 going in d direction intersects the triangle v0..v2
 * return non zero if it does
 */
int isect_ray_tri_v3(const float p1[3], const float d[3],
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
	 * a human head (1BU==1m), subsurf level 2, this gave many errors - campbell */
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

int isect_ray_plane_v3(const float p1[3], const float d[3],
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
	 * a human head (1BU==1m), subsurf level 2, this gave many errors - campbell */
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

int isect_ray_tri_epsilon_v3(const float p1[3], const float d[3],
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

int isect_ray_tri_threshold_v3(const float p1[3], const float d[3],
                               const float v0[3], const float v1[3], const float v2[3],
                               float *r_lambda, float r_uv[2], const float threshold)
{
	float p[3], s[3], e1[3], e2[3], q[3];
	float a, f, u, v;
	float du = 0, dv = 0;

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

	if (u < 0) du = u;
	if (u > 1) du = u - 1;
	if (v < 0) dv = v;
	if (v > 1) dv = v - 1;
	if (u > 0 && v > 0 && u + v > 1) {
		float t = u + v - 1;
		du = u - t / 2;
		dv = v - t / 2;
	}

	mul_v3_fl(e1, du);
	mul_v3_fl(e2, dv);

	if (dot_v3v3(e1, e1) + dot_v3v3(e2, e2) > threshold * threshold) {
		return 0;
	}

	if (r_uv) {
		r_uv[0] = u;
		r_uv[1] = v;
	}

	return 1;
}

int isect_line_plane_v3(float out[3],
                        const float l1[3], const float l2[3],
                        const float plane_co[3], const float plane_no[3], const short no_flip)
{
	float l_vec[3]; /* l1 -> l2 normalized vector */
	float p_no[3]; /* 'plane_no' normalized */
	float dot;

	sub_v3_v3v3(l_vec, l2, l1);

	normalize_v3(l_vec);
	normalize_v3_v3(p_no, plane_no);

	dot = dot_v3v3(l_vec, p_no);
	if (dot == 0.0f) {
		return 0;
	}
	else {
		float l1_plane[3]; /* line point aligned with the plane */
		float dist; /* 'plane_no' aligned distance to the 'plane_co' */

		/* for predictable flipping since the plane is only used to
		 * define a direction, ignore its flipping and aligned with 'l_vec' */
		if (dot < 0.0f) {
			dot = -dot;
			negate_v3(p_no);
		}

		add_v3_v3v3(l1_plane, l1, p_no);

		dist = line_point_factor_v3(plane_co, l1, l1_plane);

		/* treat line like a ray, when 'no_flip' is set */
		if (no_flip && dist < 0.0f) {
			dist = -dist;
		}

		mul_v3_fl(l_vec, dist / dot);

		add_v3_v3v3(out, l1, l_vec);

		return 1;
	}
}

/* note: return normal isn't unit length */
void isect_plane_plane_v3(float r_isect_co[3], float r_isect_no[3],
                          const float plane_a_co[3], const float plane_a_no[3],
                          const float plane_b_co[3], const float plane_b_no[3])
{
	float plane_a_co_other[3];
	cross_v3_v3v3(r_isect_no, plane_a_no, plane_b_no); /* direction is simply the cross product */
	cross_v3_v3v3(plane_a_co_other, plane_a_no, r_isect_no);
	add_v3_v3(plane_a_co_other, plane_a_co);
	isect_line_plane_v3(r_isect_co, plane_a_co, plane_a_co_other, plane_b_co, plane_b_no, FALSE);
}


/* Adapted from the paper by Kasper Fauerby */

/* "Improved Collision detection and Response" */
static int getLowestRoot(const float a, const float b, const float c, const float maxR, float *root)
{
	// Check if a solution exists
	float determinant = b * b - 4.0f * a * c;

	// If determinant is negative it means no solutions.
	if (determinant >= 0.0f) {
		/* calculate the two roots: (if determinant == 0 then
		 * x1==x2 but lets disregard that slight optimization) */
		float sqrtD = (float)sqrt(determinant);
		float r1 = (-b - sqrtD) / (2.0f * a);
		float r2 = (-b + sqrtD) / (2.0f * a);

		// Sort so x1 <= x2
		if (r1 > r2)
			SWAP(float, r1, r2);

		// Get lowest root:
		if (r1 > 0.0f && r1 < maxR) {
			*root = r1;
			return 1;
		}

		// It is possible that we want x2 - this can happen
		// if x1 < 0
		if (r2 > 0.0f && r2 < maxR) {
			*root = r2;
			return 1;
		}
	}
	// No (valid) solutions
	return 0;
}

int isect_sweeping_sphere_tri_v3(const float p1[3], const float p2[3], const float radius,
                                 const float v0[3], const float v1[3], const float v2[3],
                                 float *r_lambda, float ipoint[3])
{
	float e1[3], e2[3], e3[3], point[3], vel[3], /*dist[3],*/ nor[3], temp[3], bv[3];
	float a, b, c, d, e, x, y, z, radius2 = radius * radius;
	float elen2, edotv, edotbv, nordotv;
	float newLambda;
	int found_by_sweep = 0;

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
			return 0;
		}
	}
	else {
		float t0 = (-a + radius) / nordotv;
		float t1 = (-a - radius) / nordotv;

		if (t0 > t1)
			SWAP(float, t0, t1);

		if (t0 > 1.0f || t1 < 0.0f) return 0;

		/* clamp to [0,1] */
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
			return 1;
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
		found_by_sweep = 1;
	}

	/*v1*/
	sub_v3_v3v3(temp, p1, v1);
	b = 2.0f * dot_v3v3(vel, temp);
	c = dot_v3v3(temp, temp) - radius2;

	if (getLowestRoot(a, b, c, *r_lambda, r_lambda)) {
		copy_v3_v3(ipoint, v1);
		found_by_sweep = 1;
	}

	/*v2*/
	sub_v3_v3v3(temp, p1, v2);
	b = 2.0f * dot_v3v3(vel, temp);
	c = dot_v3v3(temp, temp) - radius2;

	if (getLowestRoot(a, b, c, *r_lambda, r_lambda)) {
		copy_v3_v3(ipoint, v2);
		found_by_sweep = 1;
	}

	/*---test edges---*/
	sub_v3_v3v3(e3, v2, v1); //wasnt yet calculated


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
			found_by_sweep = 1;
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
			found_by_sweep = 1;
		}
	}

	/*e3*/
	/* sub_v3_v3v3(bv,v0,p1); */ /* UNUSED */
	/* elen2 = dot_v3v3(e1,e1); */ /* UNUSED */
	/* edotv = dot_v3v3(e1,vel); */ /* UNUSED */
	/* edotbv = dot_v3v3(e1,bv); */ /* UNUSED */

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
			found_by_sweep = 1;
		}
	}


	return found_by_sweep;
}

int isect_axial_line_tri_v3(const int axis, const float p1[3], const float p2[3],
                            const float v0[3], const float v1[3], const float v2[3], float *r_lambda)
{
	float p[3], e1[3], e2[3];
	float u, v, f;
	int a0 = axis, a1 = (axis + 1) % 3, a2 = (axis + 2) % 3;

#if 0
	return isect_line_tri_v3(p1,p2,v0,v1,v2,lambda);

	/* first a simple bounding box test */
	if (MIN3(v0[a1],v1[a1],v2[a1]) > p1[a1]) return 0;
	if (MIN3(v0[a2],v1[a2],v2[a2]) > p1[a2]) return 0;
	if (MAX3(v0[a1],v1[a1],v2[a1]) < p1[a1]) return 0;
	if (MAX3(v0[a2],v1[a2],v2[a2]) < p1[a2]) return 0;

	/* then a full intersection test */
#endif

	sub_v3_v3v3(e1, v1, v0);
	sub_v3_v3v3(e2, v2, v0);
	sub_v3_v3v3(p, v0, p1);

	f = (e2[a1] * e1[a2] - e2[a2] * e1[a1]);
	if ((f > -0.000001f) && (f < 0.000001f)) return 0;

	v = (p[a2] * e1[a1] - p[a1] * e1[a2]) / f;
	if ((v < 0.0f) || (v > 1.0f)) return 0;

	f = e1[a1];
	if ((f > -0.000001f) && (f < 0.000001f)) {
		f = e1[a2];
		if ((f > -0.000001f) && (f < 0.000001f)) return 0;
		u = (-p[a2] - v * e2[a2]) / f;
	}
	else
		u = (-p[a1] - v * e2[a1]) / f;

	if ((u < 0.0f) || ((u + v) > 1.0f)) return 0;

	*r_lambda = (p[a0] + u * e1[a0] + v * e2[a0]) / (p2[a0] - p1[a0]);

	if ((*r_lambda < 0.0f) || (*r_lambda > 1.0f)) return 0;

	return 1;
}

/* Returns the number of point of interests
 * 0 - lines are colinear
 * 1 - lines are coplanar, i1 is set to intersection
 * 2 - i1 and i2 are the nearest points on line 1 (v1, v2) and line 2 (v3, v4) respectively
 * */
int isect_line_line_v3(const float v1[3], const float v2[3], const float v3[3], const float v4[3], float i1[3], float i2[3])
{
	float a[3], b[3], c[3], ab[3], cb[3], dir1[3], dir2[3];
	float d;

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

	/* test if the two lines are coplanar */
	if (d > -0.000001f && d < 0.000001f) {
		cross_v3_v3v3(cb, c, b);

		mul_v3_fl(a, dot_v3v3(cb, ab) / dot_v3v3(ab, ab));
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

/* Intersection point strictly between the two lines
 * 0 when no intersection is found
 * */
int isect_line_line_strict_v3(const float v1[3], const float v2[3], const float v3[3], const float v4[3], float vi[3], float *r_lambda)
{
	float a[3], b[3], c[3], ab[3], cb[3], ca[3], dir1[3], dir2[3];
	float d;

	sub_v3_v3v3(c, v3, v1);
	sub_v3_v3v3(a, v2, v1);
	sub_v3_v3v3(b, v4, v3);

	normalize_v3_v3(dir1, a);
	normalize_v3_v3(dir2, b);
	d = dot_v3v3(dir1, dir2);
	if (d == 1.0f || d == -1.0f || d == 0) {
		/* colinear or one vector is zero-length*/
		return 0;
	}

	cross_v3_v3v3(ab, a, b);
	d = dot_v3v3(c, ab);

	/* test if the two lines are coplanar */
	if (d > -0.000001f && d < 0.000001f) {
		float f1, f2;
		cross_v3_v3v3(cb, c, b);
		cross_v3_v3v3(ca, c, a);

		f1 = dot_v3v3(cb, ab) / dot_v3v3(ab, ab);
		f2 = dot_v3v3(ca, ab) / dot_v3v3(ab, ab);

		if (f1 >= 0 && f1 <= 1 &&
		    f2 >= 0 && f2 <= 1)
		{
			mul_v3_fl(a, f1);
			add_v3_v3v3(vi, v1, a);

			if (r_lambda) *r_lambda = f1;

			return 1; /* intersection found */
		}
		else {
			return 0;
		}
	}
	else {
		return 0;
	}
}

int isect_aabb_aabb_v3(const float min1[3], const float max1[3], const float min2[3], const float max2[3])
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

	data->sign[0] = data->ray_inv_dir[0] < 0;
	data->sign[1] = data->ray_inv_dir[1] < 0;
	data->sign[2] = data->ray_inv_dir[2] < 0;
}

/* Adapted from http://www.gamedev.net/community/forums/topic.asp?topic_id=459973 */
int isect_ray_aabb(const IsectRayAABBData *data, const float bb_min[3],
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
		return FALSE;

	if (tymin > tmin)
		tmin = tymin;

	if (tymax < tmax)
		tmax = tymax;

	tzmin = (bbox[data->sign[2]][2] - data->ray_start[2]) * data->ray_inv_dir[2];
	tzmax = (bbox[1 - data->sign[2]][2] - data->ray_start[2]) * data->ray_inv_dir[2];

	if ((tmin > tzmax) || (tzmin > tmax))
		return FALSE;

	if (tzmin > tmin)
		tmin = tzmin;

	/* XXX jwilkins: tmax does not need to be updated since we don't use it
	 * keeping this here for future reference */
	//if (tzmax < tmax) tmax = tzmax;

	if (tmin_out)
		(*tmin_out) = tmin;

	return TRUE;
}

/* find closest point to p on line through l1,l2 and return lambda,
 * where (0 <= lambda <= 1) when cp is in the line segement l1,l2
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
	sub_v3_v3v3(u, l2, l1);
	sub_v3_v3v3(h, p, l1);
	return (dot_v3v3(u, h) / dot_v3v3(u, u));
}

float line_point_factor_v2(const float p[2], const float l1[2], const float l2[2])
{
	float h[2], u[2];
	sub_v2_v2v2(u, l2, l1);
	sub_v2_v2v2(h, p, l1);
	return (dot_v2v2(u, h) / dot_v2v2(u, u));
}

/* ensyre the distance between these points is no greater then 'dist'
 * if it is, scale then both into the center */
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

/* Similar to LineIntersectsTriangleUV, except it operates on a quad and in 2d, assumes point is in quad */
void isect_point_quad_uv_v2(const float v0[2], const float v1[2], const float v2[2], const float v3[2],
                            const float pt[2], float r_uv[2])
{
	float x0, y0, x1, y1, wtot, v2d[2], w1, w2;

	/* used for parallel lines */
	float pt3d[3], l1[3], l2[3], pt_on_line[3];

	/* compute 2 edges  of the quad  intersection point */
	if (IsectLLPt2Df(v0[0], v0[1], v1[0], v1[1], v2[0], v2[1], v3[0], v3[1], &x0, &y0) == 1) {
		/* the intersection point between the quad-edge intersection and the point in the quad we want the uv's for */
		/* should never be paralle !! */
		/*printf("\tnot parallel 1\n");*/
		IsectLLPt2Df(pt[0], pt[1], x0, y0, v0[0], v0[1], v3[0], v3[1], &x1, &y1);

		/* Get the weights from the new intersection point, to each edge */
		v2d[0] = x1 - v0[0];
		v2d[1] = y1 - v0[1];
		w1 = len_v2(v2d);

		v2d[0] = x1 - v3[0]; /* some but for the other vert */
		v2d[1] = y1 - v3[1];
		w2 = len_v2(v2d);
		wtot = w1 + w2;
		/*w1 = w1/wtot;*/
		/*w2 = w2/wtot;*/
		r_uv[0] = w1 / wtot;
	}
	else {
		/* lines are parallel, lambda_cp_line_ex is 3d grrr */
		/*printf("\tparallel1\n");*/
		pt3d[0] = pt[0];
		pt3d[1] = pt[1];
		pt3d[2] = l1[2] = l2[2] = 0.0f;

		l1[0] = v0[0];
		l1[1] = v0[1];
		l2[0] = v1[0];
		l2[1] = v1[1];
		closest_to_line_v3(pt_on_line, pt3d, l1, l2);
		v2d[0] = pt[0] - pt_on_line[0]; /* same, for the other vert */
		v2d[1] = pt[1] - pt_on_line[1];
		w1 = len_v2(v2d);

		l1[0] = v2[0];
		l1[1] = v2[1];
		l2[0] = v3[0];
		l2[1] = v3[1];
		closest_to_line_v3(pt_on_line, pt3d, l1, l2);
		v2d[0] = pt[0] - pt_on_line[0]; /* same, for the other vert */
		v2d[1] = pt[1] - pt_on_line[1];
		w2 = len_v2(v2d);
		wtot = w1 + w2;
		r_uv[0] = w1 / wtot;
	}

	/* Same as above to calc the uv[1] value, alternate calculation */

	if (IsectLLPt2Df(v0[0], v0[1], v3[0], v3[1], v1[0], v1[1], v2[0], v2[1], &x0, &y0) == 1) { /* was v0,v1  v2,v3  now v0,v3  v1,v2*/
		/* never paralle if above was not */
		/*printf("\tnot parallel2\n");*/
		IsectLLPt2Df(pt[0], pt[1], x0, y0, v0[0], v0[1], v1[0], v1[1], &x1, &y1); /* was v0,v3  now v0,v1*/

		v2d[0] = x1 - v0[0];
		v2d[1] = y1 - v0[1];
		w1 = len_v2(v2d);

		v2d[0] = x1 - v1[0];
		v2d[1] = y1 - v1[1];
		w2 = len_v2(v2d);
		wtot = w1 + w2;
		r_uv[1] = w1 / wtot;
	}
	else {
		/* lines are parallel, lambda_cp_line_ex is 3d grrr */
		/*printf("\tparallel2\n");*/
		pt3d[0] = pt[0];
		pt3d[1] = pt[1];
		pt3d[2] = l1[2] = l2[2] = 0.0f;


		l1[0] = v0[0];
		l1[1] = v0[1];
		l2[0] = v3[0];
		l2[1] = v3[1];
		closest_to_line_v3(pt_on_line, pt3d, l1, l2);
		v2d[0] = pt[0] - pt_on_line[0]; /* some but for the other vert */
		v2d[1] = pt[1] - pt_on_line[1];
		w1 = len_v2(v2d);

		l1[0] = v1[0];
		l1[1] = v1[1];
		l2[0] = v2[0];
		l2[1] = v2[1];
		closest_to_line_v3(pt_on_line, pt3d, l1, l2);
		v2d[0] = pt[0] - pt_on_line[0]; /* some but for the other vert */
		v2d[1] = pt[1] - pt_on_line[1];
		w2 = len_v2(v2d);
		wtot = w1 + w2;
		r_uv[1] = w1 / wtot;
	}
	/* may need to flip UV's here */
}

/* same as above but does tri's and quads, tri's are a bit of a hack */
void isect_point_face_uv_v2(const int isquad,
                            const float v0[2], const float v1[2], const float v2[2], const float v3[2],
                            const float pt[2], float r_uv[2])
{
	if (isquad) {
		isect_point_quad_uv_v2(v0, v1, v2, v3, pt, r_uv);
	}
	else {
		/* not for quads, use for our abuse of LineIntersectsTriangleUV */
		float p1_3d[3], p2_3d[3], v0_3d[3], v1_3d[3], v2_3d[3], lambda;

		p1_3d[0] = p2_3d[0] = r_uv[0];
		p1_3d[1] = p2_3d[1] = r_uv[1];
		p1_3d[2] = 1.0f;
		p2_3d[2] = -1.0f;
		v0_3d[2] = v1_3d[2] = v2_3d[2] = 0.0;

		/* generate a new fuv, (this is possibly a non optimal solution,
		 * since we only need 2d calculation but use 3d func's)
		 *
		 * this method makes an imaginary triangle in 2d space using the UV's from the derived mesh face
		 * Then find new uv coords using the fuv and this face with LineIntersectsTriangleUV.
		 * This means the new values will be correct in relation to the derived meshes face.
		 */
		copy_v2_v2(v0_3d, v0);
		copy_v2_v2(v1_3d, v1);
		copy_v2_v2(v2_3d, v2);

		/* Doing this in 3D is not nice */
		isect_line_tri_v3(p1_3d, p2_3d, v0_3d, v1_3d, v2_3d, &lambda, r_uv);
	}
}

#if 0 // XXX this version used to be used in isect_point_tri_v2_int() and was called IsPointInTri2D

int isect_point_tri_v2(float pt[2], float v1[2], float v2[2], float v3[2])
{
	float inp1, inp2, inp3;

	inp1 = (v2[0] - v1[0]) * (v1[1] - pt[1]) + (v1[1] - v2[1]) * (v1[0] - pt[0]);
	inp2 = (v3[0] - v2[0]) * (v2[1] - pt[1]) + (v2[1] - v3[1]) * (v2[0] - pt[0]);
	inp3 = (v1[0] - v3[0]) * (v3[1] - pt[1]) + (v3[1] - v1[1]) * (v3[0] - pt[0]);

	if (inp1 <= 0.0f && inp2 <= 0.0f && inp3 <= 0.0f) return 1;
	if (inp1 >= 0.0f && inp2 >= 0.0f && inp3 >= 0.0f) return 1;

	return 0;
}
#endif

#if 0

int isect_point_tri_v2(float v0[2], float v1[2], float v2[2], float pt[2])
{
	/* not for quads, use for our abuse of LineIntersectsTriangleUV */
	float p1_3d[3], p2_3d[3], v0_3d[3], v1_3d[3], v2_3d[3];
	/* not used */
	float lambda, uv[3];

	p1_3d[0] = p2_3d[0] = uv[0] = pt[0];
	p1_3d[1] = p2_3d[1] = uv[1] = uv[2] = pt[1];
	p1_3d[2] = 1.0f;
	p2_3d[2] = -1.0f;
	v0_3d[2] = v1_3d[2] = v2_3d[2] = 0.0;

	/* generate a new fuv, (this is possibly a non optimal solution,
	 * since we only need 2d calculation but use 3d func's)
	 *
	 * this method makes an imaginary triangle in 2d space using the UV's from the derived mesh face
	 * Then find new uv coords using the fuv and this face with LineIntersectsTriangleUV.
	 * This means the new values will be correct in relation to the derived meshes face.
	 */
	copy_v2_v2(v0_3d, v0);
	copy_v2_v2(v1_3d, v1);
	copy_v2_v2(v2_3d, v2);

	/* Doing this in 3D is not nice */
	return isect_line_tri_v3(p1_3d, p2_3d, v0_3d, v1_3d, v2_3d, &lambda, uv);
}
#endif

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

static int point_in_slice(const float p[3], const float v1[3], const float l1[3], const float l2[3])
{
	/*
	 * what is a slice ?
	 * some maths:
	 * a line including l1,l2 and a point not on the line
	 * define a subset of R3 delimited by planes parallel to the line and orthogonal
	 * to the (point --> line) distance vector,one plane on the line one on the point,
	 * the room inside usually is rather small compared to R3 though still infinte
	 * useful for restricting (speeding up) searches
	 * e.g. all points of triangular prism are within the intersection of 3 'slices'
	 * onother trivial case : cube
	 * but see a 'spat' which is a deformed cube with paired parallel planes needs only 3 slices too
	 */
	float h, rp[3], cp[3], q[3];

	closest_to_line_v3(cp, v1, l1, l2);
	sub_v3_v3v3(q, cp, v1);

	sub_v3_v3v3(rp, p, v1);
	h = dot_v3v3(q, rp) / dot_v3v3(q, q);
	if (h < 0.0f || h > 1.0f) return 0;
	return 1;
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

/*mama (knowing the squared length of the normal)*/
static int point_in_slice_m(float p[3], float origin[3], float normal[3], float lns)
{
	float h, rp[3];
	sub_v3_v3v3(rp, p, origin);
	h = dot_v3v3(normal, rp) / lns;
	if (h < 0.0f || h > 1.0f) return 0;
	return 1;
}
#endif

int isect_point_tri_prism_v3(const float p[3], const float v1[3], const float v2[3], const float v3[3])
{
	if (!point_in_slice(p, v1, v2, v3)) return 0;
	if (!point_in_slice(p, v2, v3, v1)) return 0;
	if (!point_in_slice(p, v3, v1, v2)) return 0;
	return 1;
}

int clip_line_plane(float p1[3], float p2[3], const float plane[4])
{
	float dp[3], n[3], div, t, pc[3];

	copy_v3_v3(n, plane);
	sub_v3_v3v3(dp, p2, p1);
	div = dot_v3v3(dp, n);

	if (div == 0.0f) /* parallel */
		return 1;

	t = -(dot_v3v3(p1, n) + plane[3]) / div;

	if (div > 0.0f) {
		/* behind plane, completely clipped */
		if (t >= 1.0f) {
			zero_v3(p1);
			zero_v3(p2);
			return 0;
		}

		/* intersect plane */
		if (t > 0.0f) {
			madd_v3_v3v3fl(pc, p1, dp, t);
			copy_v3_v3(p1, pc);
			return 1;
		}

		return 1;
	}
	else {
		/* behind plane, completely clipped */
		if (t <= 0.0f) {
			zero_v3(p1);
			zero_v3(p2);
			return 0;
		}

		/* intersect plane */
		if (t < 1.0f) {
			madd_v3_v3v3fl(pc, p1, dp, t);
			copy_v3_v3(p2, pc);
			return 1;
		}

		return 1;
	}
}

void plot_line_v2v2i(const int p1[2], const int p2[2], int (*callback)(int, int, void *), void *userData)
{
	int x1 = p1[0];
	int y1 = p1[1];
	int x2 = p2[0];
	int y2 = p2[1];

	signed char ix;
	signed char iy;

	// if x1 == x2 or y1 == y2, then it does not matter what we set here
	int delta_x = (x2 > x1 ? (ix = 1, x2 - x1) : (ix = -1, x1 - x2)) << 1;
	int delta_y = (y2 > y1 ? (iy = 1, y2 - y1) : (iy = -1, y1 - y2)) << 1;

	if (callback(x1, y1, userData) == 0) {
		return;
	}

	if (delta_x >= delta_y) {
		// error may go below zero
		int error = delta_y - (delta_x >> 1);

		while (x1 != x2) {
			if (error >= 0) {
				if (error || (ix > 0)) {
					y1 += iy;
					error -= delta_x;
				}
				// else do nothing
			}
			// else do nothing

			x1 += ix;
			error += delta_y;

			if (callback(x1, y1, userData) == 0) {
				return;
			}
		}
	}
	else {
		// error may go below zero
		int error = delta_x - (delta_y >> 1);

		while (y1 != y2) {
			if (error >= 0) {
				if (error || (iy > 0)) {
					x1 += ix;
					error -= delta_y;
				}
				// else do nothing
			}
			// else do nothing

			y1 += iy;
			error += delta_x;

			if (callback(x1, y1, userData) == 0) {
				return;
			}
		}
	}
}

/****************************** Interpolation ********************************/

/* get the 2 dominant axis values, 0==X, 1==Y, 2==Z */
void axis_dominant_v3(int *axis_a, int *axis_b, const float axis[3])
{
	const float xn = fabsf(axis[0]);
	const float yn = fabsf(axis[1]);
	const float zn = fabsf(axis[2]);

	if      (zn >= xn && zn >= yn) { *axis_a = 0; *axis_b = 1; }
	else if (yn >= xn && yn >= zn) { *axis_a = 0; *axis_b = 2; }
	else                           { *axis_a = 1; *axis_b = 2; }
}

static float tri_signed_area(const float v1[3], const float v2[3], const float v3[3], const int i, const int j)
{
	return 0.5f * ((v1[i] - v2[i]) * (v2[j] - v3[j]) + (v1[j] - v2[j]) * (v3[i] - v2[i]));
}

/* return 1 when degenerate */
static int barycentric_weights(const float v1[3], const float v2[3], const float v3[3], const float co[3], const float n[3], float w[3])
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
		return 0;
	}
	else {
		/* zero area triangle */
		copy_v3_fl(w, 1.0f / 3.0f);
		return 1;
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
		int degenerate;

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
		else
			barycentric_weights(v1, v2, v3, co, n, w);
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
int barycentric_coords_v2(const float v1[2], const float v2[2], const float v3[2], const float co[2], float w[3])
{
	float x = co[0], y = co[1];
	float x1 = v1[0], y1 = v1[1];
	float x2 = v2[0], y2 = v2[1];
	float x3 = v3[0], y3 = v3[1];
	float det = (y2 - y3) * (x1 - x3) + (x3 - x2) * (y1 - y3);

	if (fabsf(det) > FLT_EPSILON) {
		w[0] = ((y2 - y3) * (x - x3) + (x3 - x2) * (y - y3)) / det;
		w[1] = ((y3 - y1) * (x - x3) + (x1 - x3) * (y - y3)) / det;
		w[2] = 1.0f - w[0] - w[1];

	return 1;
	}

	return 0;
}

/* used by projection painting
 * note: using area_tri_signed_v2 means locations outside the triangle are correctly weighted */
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

/* same as #barycentric_weights_v2 but works with a quad,
 * note: untested for values outside the quad's bounds.
 * note: there may be a more efficient method to do this, just figured it out - campbell */
void barycentric_weights_v2_quad(const float v1[2], const float v2[2], const float v3[2], const float v4[2],
                                 const float co[2], float w[4])
{
	float wtot;

	const float areas_co[4] = {
	    area_tri_signed_v2(v1, v2, co),
	    area_tri_signed_v2(v2, v3, co),
	    area_tri_signed_v2(v3, v4, co),
	    area_tri_signed_v2(v4, v1, co),
	};

	const float areas_diag[4] = {
	    area_tri_signed_v2(v4, v1, v2),
	    area_tri_signed_v2(v1, v2, v3),
	    area_tri_signed_v2(v2, v3, v4),
	    area_tri_signed_v2(v3, v4, v1),
	};

	const float u = areas_co[3] / (areas_co[1] + areas_co[3]);
	const float v = areas_co[0] / (areas_co[0] + areas_co[2]);

	w[0] = ((1.0f - u) * (1.0f - v)) * sqrtf(areas_diag[0] / areas_diag[2]);
	w[1] = ((       u) * (1.0f - v)) * sqrtf(areas_diag[1] / areas_diag[3]);
	w[2] = ((       u) * (       v)) * sqrtf(areas_diag[2] / areas_diag[0]);
	w[3] = ((1.0f - u) * (       v)) * sqrtf(areas_diag[3] / areas_diag[1]);

	wtot = w[0] + w[1] + w[2] + w[3];

	if (wtot != 0.0f) {
		mul_v4_fl(w, 1.0f / wtot);
	}
	else { /* dummy values for zero area face */
		copy_v4_fl(w, 1.0f / 4.0f);
	}
}

/* given 2 triangles in 3D space, and a point in relation to the first triangle.
 * calculate the location of a point in relation to the second triangle.
 * Useful for finding relative positions with geometry */
void barycentric_transform(float pt_tar[3], float const pt_src[3],
                           const float tri_tar_p1[3], const float tri_tar_p2[3], const float tri_tar_p3[3],
                           const float tri_src_p1[3], const float tri_src_p2[3], const float tri_src_p3[3])
{
	/* this works by moving the source triangle so its normal is pointing on the Z
	 * axis where its barycentric wights can be calculated in 2D and its Z offset can
	 *  be re-applied. The weights are applied directly to the targets 3D points and the
	 *  z-depth is used to scale the targets normal as an offset.
	 * This saves transforming the target into its Z-Up orientation and back (which could also work) */
	const float z_up[3] = {0, 0, 1};
	float no_tar[3], no_src[3];
	float quat_src[4];
	float pt_src_xy[3];
	float tri_xy_src[3][3];
	float w_src[3];
	float area_tar, area_src;
	float z_ofs_src;

	normal_tri_v3(no_tar, tri_tar_p1, tri_tar_p2, tri_tar_p3);
	normal_tri_v3(no_src, tri_src_p1, tri_src_p2, tri_src_p3);

	rotation_between_vecs_to_quat(quat_src, no_src, z_up);
	normalize_qt(quat_src);

	copy_v3_v3(pt_src_xy, pt_src);
	copy_v3_v3(tri_xy_src[0], tri_src_p1);
	copy_v3_v3(tri_xy_src[1], tri_src_p2);
	copy_v3_v3(tri_xy_src[2], tri_src_p3);

	/* make the source tri xy space */
	mul_qt_v3(quat_src, pt_src_xy);
	mul_qt_v3(quat_src, tri_xy_src[0]);
	mul_qt_v3(quat_src, tri_xy_src[1]);
	mul_qt_v3(quat_src, tri_xy_src[2]);

	barycentric_weights_v2(tri_xy_src[0], tri_xy_src[1], tri_xy_src[2], pt_src_xy, w_src);
	interp_v3_v3v3v3(pt_tar, tri_tar_p1, tri_tar_p2, tri_tar_p3, w_src);

	area_tar = sqrtf(area_tri_v3(tri_tar_p1, tri_tar_p2, tri_tar_p3));
	area_src = sqrtf(area_tri_v2(tri_xy_src[0], tri_xy_src[1], tri_xy_src[2]));

	z_ofs_src = pt_src_xy[2] - tri_xy_src[0][2];
	madd_v3_v3v3fl(pt_tar, pt_tar, no_tar, (z_ofs_src / area_src) * area_tar);
}

/* given an array with some invalid values this function interpolates valid values
 * replacing the invalid ones */
int interp_sparse_array(float *array, int const list_size, const float skipval)
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

		float *array_up = MEM_callocN(sizeof(float) * list_size, "interp_sparse_array up");
		float *array_down = MEM_callocN(sizeof(float) * list_size, "interp_sparse_array up");

		int *ofs_tot_up = MEM_callocN(sizeof(int) * list_size, "interp_sparse_array tup");
		int *ofs_tot_down = MEM_callocN(sizeof(int) * list_size, "interp_sparse_array tdown");

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
					array[i] = ((array_up[i] * ofs_tot_down[i]) + (array_down[i] * ofs_tot_up[i])) / (float)(ofs_tot_down[i] + ofs_tot_up[i]);
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
static float mean_value_half_tan(const float v1[3], const float v2[3], const float v3[3])
{
	float d2[3], d3[3], cross[3], area, dot, len;

	sub_v3_v3v3(d2, v2, v1);
	sub_v3_v3v3(d3, v3, v1);
	cross_v3_v3v3(cross, d2, d3);

	area = len_v3(cross);
	dot = dot_v3v3(d2, d3);
	len = len_v3(d2) * len_v3(d3);

	if (area == 0.0f)
		return 0.0f;
	else
		return (len - dot) / area;
}

void interp_weights_poly_v3(float *w, float v[][3], const int n, const float co[3])
{
	float totweight, t1, t2, len, *vmid, *vprev, *vnext;
	int i;

	totweight = 0.0f;

	for (i = 0; i < n; i++) {
		vmid = v[i];
		vprev = (i == 0) ? v[n - 1] : v[i - 1];
		vnext = (i == n - 1) ? v[0] : v[i + 1];

		t1 = mean_value_half_tan(co, vprev, vmid);
		t2 = mean_value_half_tan(co, vmid, vnext);

		len = len_v3v3(co, vmid);
		w[i] = (t1 + t2) / len;
		totweight += w[i];
	}

	if (totweight != 0.0f)
		for (i = 0; i < n; i++)
			w[i] /= totweight;
}

/* (x1,v1)(t1=0)------(x2,v2)(t2=1), 0<t<1 --> (x,v)(t) */
void interp_cubic_v3(float x[3], float v[3], const float x1[3], const float v1[3], const float x2[3], const float v2[3], const float t)
{
	float a[3], b[3];
	float t2 = t * t;
	float t3 = t2 * t;

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

/* Barycentric reverse  */
void resolve_tri_uv(float r_uv[2], const float st[2], const float st0[2], const float st1[2], const float st2[2])
{
	/* find UV such that
	 * t = u * t0 + v * t1 + (1 - u - v) * t2
	 * u * (t0 - t2) + v * (t1 - t2) = t - t2 */
	const double a = st0[0] - st2[0], b = st1[0] - st2[0];
	const double c = st0[1] - st2[1], d = st1[1] - st2[1];
	const double det = a * d - c * b;

	if (IS_ZERO(det) == 0) { /* det should never be zero since the determinant is the signed ST area of the triangle. */
		const double x[] = {st[0] - st2[0], st[1] - st2[1]};

		r_uv[0] = (float)((d * x[0] - b * x[1]) / det);
		r_uv[1] = (float)(((-c) * x[0] + a * x[1]) / det);
	}
	else zero_v2(r_uv);
}

/* bilinear reverse */
void resolve_quad_uv(float r_uv[2], const float st[2], const float st0[2], const float st1[2], const float st2[2], const float st3[2])
{
	const double signed_area = (st0[0] * st1[1] - st0[1] * st1[0]) + (st1[0] * st2[1] - st1[1] * st2[0]) +
	                           (st2[0] * st3[1] - st2[1] * st3[0]) + (st3[0] * st0[1] - st3[1] * st0[0]);

	/* X is 2D cross product (determinant)
	 * A= (p0-p) X (p0-p3)*/
	const double a = (st0[0] - st[0]) * (st0[1] - st3[1]) - (st0[1] - st[1]) * (st0[0] - st3[0]);

	/* B= ( (p0-p) X (p1-p2) + (p1-p) X (p0-p3) ) / 2 */
	const double b = 0.5 * (((st0[0] - st[0]) * (st1[1] - st2[1]) - (st0[1] - st[1]) * (st1[0] - st2[0])) +
	                        ((st1[0] - st[0]) * (st0[1] - st3[1]) - (st1[1] - st[1]) * (st0[0] - st3[0])));

	/* C = (p1-p) X (p1-p2) */
	const double fC = (st1[0] - st[0]) * (st1[1] - st2[1]) - (st1[1] - st[1]) * (st1[0] - st2[0]);
	const double denom = a - 2 * b + fC;

	// clear outputs
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
		double denom = denom_s;

		if (fabs(denom_s) < fabs(denom_t)) {
			i = 1;
			denom = denom_t;
		}

		if (IS_ZERO(denom) == 0)
			r_uv[1] = (float)(((1.0f - r_uv[0]) * (st0[i] - st[i]) + r_uv[0] * (st1[i] - st[i])) / denom);
	}
}

#undef IS_ZERO

/***************************** View & Projection *****************************/

void orthographic_m4(float matrix[][4], const float left, const float right, const float bottom, const float top, const float nearClip, const float farClip)
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

void perspective_m4(float mat[4][4], const float left, const float right, const float bottom, const float top, const float nearClip, const float farClip)
{
	float Xdelta, Ydelta, Zdelta;

	Xdelta = right - left;
	Ydelta = top - bottom;
	Zdelta = farClip - nearClip;

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
	        mat[3][0] = mat[3][1] = mat[3][3] = 0.0;

}

/* translate a matrix created by orthographic_m4 or perspective_m4 in XY coords (used to jitter the view) */
void window_translate_m4(float winmat[][4], float perspmat[][4], const float x, const float y)
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

static void i_multmatrix(float icand[][4], float Vm[][4])
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

void polarview_m4(float Vm[][4], float dist, float azimuth, float incidence, float twist)
{

	unit_m4(Vm);

	translate_m4(Vm, 0.0, 0.0, -dist);
	rotate_m4(Vm, 'Z', -twist);
	rotate_m4(Vm, 'X', -incidence);
	rotate_m4(Vm, 'Z', -azimuth);
}

void lookat_m4(float mat[][4], float vx, float vy, float vz, float px, float py, float pz, float twist)
{
	float sine, cosine, hyp, hyp1, dx, dy, dz;
	float mat1[4][4] = MAT4_UNITY;

	unit_m4(mat);

	rotate_m4(mat, 'Z', -twist);

	dx = px - vx;
	dy = py - vy;
	dz = pz - vz;
	hyp = dx * dx + dz * dz; /* hyp squared	*/
	hyp1 = (float)sqrt(dy * dy + hyp);
	hyp = (float)sqrt(hyp); /* the real hyp	*/

	if (hyp1 != 0.0f) { /* rotate X	*/
		sine = -dy / hyp1;
		cosine = hyp / hyp1;
	}
	else {
		sine = 0;
		cosine = 1.0f;
	}
	mat1[1][1] = cosine;
	mat1[1][2] = sine;
	mat1[2][1] = -sine;
	mat1[2][2] = cosine;

	i_multmatrix(mat1, mat);

	mat1[1][1] = mat1[2][2] = 1.0f; /* be careful here to reinit	*/
	mat1[1][2] = mat1[2][1] = 0.0; /* those modified by the last	*/

	/* paragraph	*/
	if (hyp != 0.0f) { /* rotate Y	*/
		sine = dx / hyp;
		cosine = -dz / hyp;
	}
	else {
		sine = 0;
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

		*r_v = 1.0f - (float)saacos(z / len) / (float)M_PI;
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

			// accumulate
			madd_v3_v3fl(vn[i], f_no, fac);
			prev_edge = cur_edge;
		}
	}
}

/* Add weighted face normal component into normals of the face vertices.
 * Caller must pass pre-allocated vdiffs of nverts length. */
void accumulate_vertex_normals_poly(float **vertnos, float polyno[3],
                                    float **vertcos, float vdiffs[][3], int nverts)
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
		int i;

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

/* For normal map tangents we need to detect uv boundaries, and only average
 * tangents in case the uvs are connected. Alternative would be to store 1
 * tangent per face rather than 4 per face vertex, but that's not compatible
 * with games */


/* from BKE_mesh.h */
#define STD_UV_CONNECT_LIMIT  0.0001f

void sum_or_add_vertex_tangent(void *arena, VertexTangent **vtang, const float tang[3], const float uv[2])
{
	VertexTangent *vt;

	/* find a tangent with connected uvs */
	for (vt = *vtang; vt; vt = vt->next) {
		if (fabsf(uv[0] - vt->uv[0]) < STD_UV_CONNECT_LIMIT && fabsf(uv[1] - vt->uv[1]) < STD_UV_CONNECT_LIMIT) {
			add_v3_v3(vt->tang, tang);
			return;
		}
	}

	/* if not found, append a new one */
	vt = BLI_memarena_alloc((MemArena *) arena, sizeof(VertexTangent));
	copy_v3_v3(vt->tang, tang);
	vt->uv[0] = uv[0];
	vt->uv[1] = uv[1];

	if (*vtang)
		vt->next = *vtang;
	*vtang = vt;
}

float *find_vertex_tangent(VertexTangent *vtang, const float uv[2])
{
	VertexTangent *vt;
	static float nulltang[3] = {0.0f, 0.0f, 0.0f};

	for (vt = vtang; vt; vt = vt->next)
		if (fabsf(uv[0] - vt->uv[0]) < STD_UV_CONNECT_LIMIT && fabsf(uv[1] - vt->uv[1]) < STD_UV_CONNECT_LIMIT)
			return vt->tang;

	return nulltang; /* shouldn't happen, except for nan or so */
}

void tangent_from_uv(float uv1[2], float uv2[2], float uv3[3], float co1[3], float co2[3], float co3[3], float n[3], float tang[3])
{
	float s1 = uv2[0] - uv1[0];
	float s2 = uv3[0] - uv1[0];
	float t1 = uv2[1] - uv1[1];
	float t2 = uv3[1] - uv1[1];
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
		tang[0] = tang[1] = tang[2] = 0.0;
	}
}

/****************************** Vector Clouds ********************************/

/* vector clouds */
/* void vcloud_estimate_transform(int list_size, float (*pos)[3], float *weight,float (*rpos)[3], float *rweight,
 *                                float lloc[3],float rloc[3],float lrot[3][3],float lscale[3][3])
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

/* can't believe there is none in math utils */
static float _det_m3(float m2[3][3])
{
	float det = 0.f;
	if (m2) {
		det = (m2[0][0] * (m2[1][1] * m2[2][2] - m2[1][2] * m2[2][1]) -
		       m2[1][0] * (m2[0][1] * m2[2][2] - m2[0][2] * m2[2][1]) +
		       m2[2][0] * (m2[0][1] * m2[1][2] - m2[0][2] * m2[1][1]));
	}
	return det;
}

void vcloud_estimate_transform(int list_size, float (*pos)[3], float *weight, float (*rpos)[3], float *rweight,
                               float lloc[3], float rloc[3], float lrot[3][3], float lscale[3][3])
{
	float accu_com[3] = {0.0f, 0.0f, 0.0f}, accu_rcom[3] = {0.0f, 0.0f, 0.0f};
	float accu_weight = 0.0f, accu_rweight = 0.0f, eps = 0.000001f;

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
			else add_v3_v3(accu_com, pos[a]);

			if (rweight) {
				float v[3];
				copy_v3_v3(v, rpos[a]);
				mul_v3_fl(v, rweight[a]);
				add_v3_v3(accu_rcom, v);
				accu_rweight += rweight[a];
			}
			else add_v3_v3(accu_rcom, rpos[a]);

		}
		if (!weight || !rweight) {
			accu_weight = accu_rweight = list_size;
		}

		mul_v3_fl(accu_com, 1.0f / accu_weight);
		mul_v3_fl(accu_rcom, 1.0f / accu_rweight);
		if (lloc) copy_v3_v3(lloc, accu_com);
		if (rloc) copy_v3_v3(rloc, accu_rcom);
		if (lrot || lscale) { /* caller does not want rot nor scale, strange but legal */
			/*so now do some reverse engeneering and see if we can split rotation from scale ->Polardecompose*/
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
				/* mul_v3_fl(va,bp->mass);  mass needs renormalzation here ?? */
				sub_v3_v3v3(vb, pos[a], accu_com);
				/* mul_v3_fl(va,rp->mass); */
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
			odet = 0.f;
			ndet = _det_m3(q);
			while ((odet - ndet) * (odet - ndet) > eps && i < imax) {
				invert_m3_m3(qi, q);
				transpose_m3(qi);
				add_m3_m3m3(q, q, qi);
				mul_m3_fl(q, 0.5f);
				odet = ndet;
				ndet = _det_m3(q);
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

static int ff_visible_quad(const float p[3], const float n[3],
                           const float v0[3], const float v1[3], const float v2[3],
                           float q0[3], float q1[3], float q2[3], float q3[3])
{
	static const float epsilon = 1e-6f;
	float c, sd[3];

	c = dot_v3v3(n, p);

	/* signed distances from the vertices to the plane. */
	sd[0] = dot_v3v3(n, v0) - c;
	sd[1] = dot_v3v3(n, v1) - c;
	sd[2] = dot_v3v3(n, v2) - c;

	if (fabsf(sd[0]) < epsilon) sd[0] = 0.0f;
	if (fabsf(sd[1]) < epsilon) sd[1] = 0.0f;
	if (fabsf(sd[2]) < epsilon) sd[2] = 0.0f;

	if (sd[0] > 0) {
		if (sd[1] > 0) {
			if (sd[2] > 0) {
				/* +++ */
				copy_v3_v3(q0, v0);
				copy_v3_v3(q1, v1);
				copy_v3_v3(q2, v2);
				copy_v3_v3(q3, q2);
			}
			else if (sd[2] < 0) {
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
		else if (sd[1] < 0) {
			if (sd[2] > 0) {
				/* +-+ */
				copy_v3_v3(q0, v0);
				vec_add_dir(q1, v0, v1, (sd[0] / (sd[0] - sd[1])));
				vec_add_dir(q2, v1, v2, (sd[1] / (sd[1] - sd[2])));
				copy_v3_v3(q3, v2);
			}
			else if (sd[2] < 0) {
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
			if (sd[2] > 0) {
				/* +0+ */
				copy_v3_v3(q0, v0);
				copy_v3_v3(q1, v1);
				copy_v3_v3(q2, v2);
				copy_v3_v3(q3, q2);
			}
			else if (sd[2] < 0) {
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
	else if (sd[0] < 0) {
		if (sd[1] > 0) {
			if (sd[2] > 0) {
				/* -++ */
				vec_add_dir(q0, v0, v1, (sd[0] / (sd[0] - sd[1])));
				copy_v3_v3(q1, v1);
				copy_v3_v3(q2, v2);
				vec_add_dir(q3, v0, v2, (sd[0] / (sd[0] - sd[2])));
			}
			else if (sd[2] < 0) {
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
		else if (sd[1] < 0) {
			if (sd[2] > 0) {
				/* --+ */
				vec_add_dir(q0, v0, v2, (sd[0] / (sd[0] - sd[2])));
				vec_add_dir(q1, v1, v2, (sd[1] / (sd[1] - sd[2])));
				copy_v3_v3(q2, v2);
				copy_v3_v3(q3, q2);
			}
			else if (sd[2] < 0) {
				/* --- */
				return 0;
			}
			else {
				/* --0 */
				return 0;
			}
		}
		else {
			if (sd[2] > 0) {
				/* -0+ */
				vec_add_dir(q0, v0, v2, (sd[0] / (sd[0] - sd[2])));
				copy_v3_v3(q1, v1);
				copy_v3_v3(q2, v2);
				copy_v3_v3(q3, q2);
			}
			else if (sd[2] < 0) {
				/* -0- */
				return 0;
			}
			else {
				/* -00 */
				return 0;
			}
		}
	}
	else {
		if (sd[1] > 0) {
			if (sd[2] > 0) {
				/* 0++ */
				copy_v3_v3(q0, v0);
				copy_v3_v3(q1, v1);
				copy_v3_v3(q2, v2);
				copy_v3_v3(q3, q2);
			}
			else if (sd[2] < 0) {
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
		else if (sd[1] < 0) {
			if (sd[2] > 0) {
				/* 0-+ */
				copy_v3_v3(q0, v0);
				vec_add_dir(q1, v1, v2, (sd[1] / (sd[1] - sd[2])));
				copy_v3_v3(q2, v2);
				copy_v3_v3(q3, q2);
			}
			else if (sd[2] < 0) {
				/* 0-- */
				return 0;
			}
			else {
				/* 0-0 */
				return 0;
			}
		}
		else {
			if (sd[2] > 0) {
				/* 00+ */
				copy_v3_v3(q0, v0);
				copy_v3_v3(q1, v1);
				copy_v3_v3(q2, v2);
				copy_v3_v3(q3, q2);
			}
			else if (sd[2] < 0) {
				/* 00- */
				return 0;
			}
			else {
				/* 000 */
				return 0;
			}
		}
	}

	return 1;
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
	 * gives big erros for near 1.0 values, sqrt(2 * x) * acos(1 - x) should work
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

	if (d > 1.0e-35F) {
		d = 1.0f / sqrtf(d);

		n[0] *= d;
		n[1] *= d;
		n[2] *= d;
	}
}

static float ff_quad_form_factor(const float p[3], const float n[3],
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

	if (ff_visible_quad(p, n, v1, v2, v3, q0, q1, q2, q3))
		contrib += ff_quad_form_factor(p, n, q0, q1, q2, q3);

	if (v4 && ff_visible_quad(p, n, v1, v3, v4, q0, q1, q2, q3))
		contrib += ff_quad_form_factor(p, n, q0, q1, q2, q3);

	return contrib;
}

/* evaluate if entire quad is a proper convex quad */
int is_quad_convex_v3(const float v1[3], const float v2[3], const float v3[3], const float v4[3])
{
	float nor[3], nor1[3], nor2[3], vec[4][2];
	int axis_a, axis_b;

	/* define projection, do both trias apart, quad is undefined! */

	normal_tri_v3(nor1, v1, v2, v3);
	normal_tri_v3(nor2, v1, v3, v4);

	/* when the face is folded over as 2 tris we probably don't want to create
	 * a quad from it, but go ahead with the intersection test since this
	 * isn't a function for degenerate faces */
	if (UNLIKELY(dot_v3v3(nor1, nor2) < 0.0f)) {
		/* flip so adding normals in the opposite direction
		 * doesnt give a zero length vector */
		negate_v3(nor2);
	}

	add_v3_v3v3(nor, nor1, nor2);

	axis_dominant_v3(&axis_a, &axis_b, nor);

	vec[0][0] = v1[axis_a];
	vec[0][1] = v1[axis_b];
	vec[1][0] = v2[axis_a];
	vec[1][1] = v2[axis_b];

	vec[2][0] = v3[axis_a];
	vec[2][1] = v3[axis_b];
	vec[3][0] = v4[axis_a];
	vec[3][1] = v4[axis_b];

	/* linetests, the 2 diagonals have to instersect to be convex */
	return (isect_line_line_v2(vec[0], vec[2], vec[1], vec[3]) > 0) ? TRUE : FALSE;
}

int is_quad_convex_v2(const float v1[2], const float v2[2], const float v3[2], const float v4[2])
{
	/* linetests, the 2 diagonals have to instersect to be convex */
	return (isect_line_line_v2(v1, v3, v2, v4) > 0) ? TRUE : FALSE;
}

