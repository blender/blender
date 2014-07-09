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

/** \file blender/blenlib/intern/math_vector.c
 *  \ingroup bli
 */

#include "BLI_math.h"

#include "BLI_strict_flags.h"

//******************************* Interpolation *******************************/

void interp_v2_v2v2(float target[2], const float a[2], const float b[2], const float t)
{
	float s = 1.0f - t;

	target[0] = s * a[0] + t * b[0];
	target[1] = s * a[1] + t * b[1];
}

/* weight 3 2D vectors,
 * 'w' must be unit length but is not a vector, just 3 weights */
void interp_v2_v2v2v2(float p[2], const float v1[2], const float v2[2], const float v3[2], const float w[3])
{
	p[0] = v1[0] * w[0] + v2[0] * w[1] + v3[0] * w[2];
	p[1] = v1[1] * w[0] + v2[1] * w[1] + v3[1] * w[2];
}

void interp_v3_v3v3(float target[3], const float a[3], const float b[3], const float t)
{
	float s = 1.0f - t;

	target[0] = s * a[0] + t * b[0];
	target[1] = s * a[1] + t * b[1];
	target[2] = s * a[2] + t * b[2];
}

void interp_v4_v4v4(float target[4], const float a[4], const float b[4], const float t)
{
	float s = 1.0f - t;

	target[0] = s * a[0] + t * b[0];
	target[1] = s * a[1] + t * b[1];
	target[2] = s * a[2] + t * b[2];
	target[3] = s * a[3] + t * b[3];
}

/**
 * slerp, treat vectors as spherical coordinates
 * \see #interp_qt_qtqt
 *
 * \return success
 */
bool interp_v3_v3v3_slerp(float target[3], const float a[3], const float b[3], const float t)
{
	float cosom, w[2];

	BLI_ASSERT_UNIT_V3(a);
	BLI_ASSERT_UNIT_V3(b);

	cosom = dot_v3v3(a, b);

	/* direct opposites */
	if (UNLIKELY(cosom < (-1.0f + FLT_EPSILON))) {
		return false;
	}

	interp_dot_slerp(t, cosom, w);

	target[0] = w[0] * a[0] + w[1] * b[0];
	target[1] = w[0] * a[1] + w[1] * b[1];
	target[2] = w[0] * a[2] + w[1] * b[2];

	return true;
}
bool interp_v2_v2v2_slerp(float target[2], const float a[2], const float b[2], const float t)
{
	float cosom, w[2];

	BLI_ASSERT_UNIT_V2(a);
	BLI_ASSERT_UNIT_V2(b);

	cosom = dot_v2v2(a, b);

	/* direct opposites */
	if (UNLIKELY(cosom < (1.0f + FLT_EPSILON))) {
		return false;
	}

	interp_dot_slerp(t, cosom, w);

	target[0] = w[0] * a[0] + w[1] * b[0];
	target[1] = w[0] * a[1] + w[1] * b[1];

	return true;
}

/**
 * Same as #interp_v3_v3v3_slerp buy uses fallback values
 * for opposite vectors.
 */
void interp_v3_v3v3_slerp_safe(float target[3], const float a[3], const float b[3], const float t)
{
	if (UNLIKELY(!interp_v3_v3v3_slerp(target, a, b, t))) {
		/* axis are aligned so any otho vector is acceptable */
		float ab_ortho[3];
		ortho_v3_v3(ab_ortho, a);
		normalize_v3(ab_ortho);
		if (t < 0.5f) {
			if (UNLIKELY(!interp_v3_v3v3_slerp(target, a, ab_ortho, t * 2.0f))) {
				BLI_assert(0);
				copy_v3_v3(target, a);
			}
		}
		else {
			if (UNLIKELY(!interp_v3_v3v3_slerp(target, ab_ortho, b, (t - 0.5f) * 2.0f))) {
				BLI_assert(0);
				copy_v3_v3(target, b);
			}
		}
	}
}
void interp_v2_v2v2_slerp_safe(float target[2], const float a[2], const float b[2], const float t)
{
	if (UNLIKELY(!interp_v2_v2v2_slerp(target, a, b, t))) {
		/* axis are aligned so any otho vector is acceptable */
		float ab_ortho[2];
		ortho_v2_v2(ab_ortho, a);
		// normalize_v2(ab_ortho);
		if (t < 0.5f) {
			if (UNLIKELY(!interp_v2_v2v2_slerp(target, a, ab_ortho, t * 2.0f))) {
				BLI_assert(0);
				copy_v2_v2(target, a);
			}
		}
		else {
			if (UNLIKELY(!interp_v2_v2v2_slerp(target, ab_ortho, b, (t - 0.5f) * 2.0f))) {
				BLI_assert(0);
				copy_v2_v2(target, b);
			}
		}
	}
}

/* weight 3 vectors,
 * 'w' must be unit length but is not a vector, just 3 weights */
void interp_v3_v3v3v3(float p[3], const float v1[3], const float v2[3], const float v3[3], const float w[3])
{
	p[0] = v1[0] * w[0] + v2[0] * w[1] + v3[0] * w[2];
	p[1] = v1[1] * w[0] + v2[1] * w[1] + v3[1] * w[2];
	p[2] = v1[2] * w[0] + v2[2] * w[1] + v3[2] * w[2];
}

/* weight 3 vectors,
 * 'w' must be unit length but is not a vector, just 4 weights */
void interp_v3_v3v3v3v3(float p[3], const float v1[3], const float v2[3], const float v3[3], const float v4[3], const float w[4])
{
	p[0] = v1[0] * w[0] + v2[0] * w[1] + v3[0] * w[2] + v4[0] * w[3];
	p[1] = v1[1] * w[0] + v2[1] * w[1] + v3[1] * w[2] + v4[1] * w[3];
	p[2] = v1[2] * w[0] + v2[2] * w[1] + v3[2] * w[2] + v4[2] * w[3];
}

void interp_v4_v4v4v4(float p[4], const float v1[4], const float v2[4], const float v3[4], const float w[3])
{
	p[0] = v1[0] * w[0] + v2[0] * w[1] + v3[0] * w[2];
	p[1] = v1[1] * w[0] + v2[1] * w[1] + v3[1] * w[2];
	p[2] = v1[2] * w[0] + v2[2] * w[1] + v3[2] * w[2];
	p[3] = v1[3] * w[0] + v2[3] * w[1] + v3[3] * w[2];
}

void interp_v4_v4v4v4v4(float p[4], const float v1[4], const float v2[4], const float v3[4], const float v4[4], const float w[4])
{
	p[0] = v1[0] * w[0] + v2[0] * w[1] + v3[0] * w[2] + v4[0] * w[3];
	p[1] = v1[1] * w[0] + v2[1] * w[1] + v3[1] * w[2] + v4[1] * w[3];
	p[2] = v1[2] * w[0] + v2[2] * w[1] + v3[2] * w[2] + v4[2] * w[3];
	p[3] = v1[3] * w[0] + v2[3] * w[1] + v3[3] * w[2] + v4[3] * w[3];
}

void interp_v3_v3v3v3_uv(float p[3], const float v1[3], const float v2[3], const float v3[3], const float uv[2])
{
	p[0] = v1[0] + ((v2[0] - v1[0]) * uv[0]) + ((v3[0] - v1[0]) * uv[1]);
	p[1] = v1[1] + ((v2[1] - v1[1]) * uv[0]) + ((v3[1] - v1[1]) * uv[1]);
	p[2] = v1[2] + ((v2[2] - v1[2]) * uv[0]) + ((v3[2] - v1[2]) * uv[1]);
}

void interp_v3_v3v3_uchar(char unsigned target[3], const unsigned char a[3], const unsigned char b[3], const float t)
{
	float s = 1.0f - t;

	target[0] = (char)floorf(s * a[0] + t * b[0]);
	target[1] = (char)floorf(s * a[1] + t * b[1]);
	target[2] = (char)floorf(s * a[2] + t * b[2]);
}
void interp_v3_v3v3_char(char target[3], const char a[3], const char b[3], const float t)
{
	interp_v3_v3v3_uchar((unsigned char *)target, (const unsigned char *)a, (const unsigned char *)b, t);
}

void interp_v4_v4v4_uchar(char unsigned target[4], const unsigned char a[4], const unsigned char b[4], const float t)
{
	float s = 1.0f - t;

	target[0] = (char)floorf(s * a[0] + t * b[0]);
	target[1] = (char)floorf(s * a[1] + t * b[1]);
	target[2] = (char)floorf(s * a[2] + t * b[2]);
	target[3] = (char)floorf(s * a[3] + t * b[3]);
}
void interp_v4_v4v4_char(char target[4], const char a[4], const char b[4], const float t)
{
	interp_v4_v4v4_uchar((unsigned char *)target, (const unsigned char *)a, (const unsigned char *)b, t);
}

void mid_v3_v3v3(float v[3], const float v1[3], const float v2[3])
{
	v[0] = 0.5f * (v1[0] + v2[0]);
	v[1] = 0.5f * (v1[1] + v2[1]);
	v[2] = 0.5f * (v1[2] + v2[2]);
}

void mid_v2_v2v2(float v[2], const float v1[2], const float v2[2])
{
	v[0] = 0.5f * (v1[0] + v2[0]);
	v[1] = 0.5f * (v1[1] + v2[1]);
}

void mid_v3_v3v3v3(float v[3], const float v1[3], const float v2[3], const float v3[3])
{
	v[0] = (v1[0] + v2[0] + v3[0]) / 3.0f;
	v[1] = (v1[1] + v2[1] + v3[1]) / 3.0f;
	v[2] = (v1[2] + v2[2] + v3[2]) / 3.0f;
}

/**
 * Specialized function for calculating normals.
 * fastpath for:
 *
 * \code{.c}
 * add_v3_v3v3(r, a, b);
 * normalize_v3(r)
 * mul_v3_fl(r, angle_normalized_v3v3(a, b) / M_PI_2);
 * \endcode
 *
 * We can use the length of (a + b) to calculate the angle.
 */
void mid_v3_v3v3_angle_weighted(float r[3], const float a[3], const float b[3])
{
	/* trick, we want the middle of 2 normals as well as the angle between them
	 * avoid multiple calculations by */
	float angle;

	/* double check they are normalized */
	BLI_ASSERT_UNIT_V3(a);
	BLI_ASSERT_UNIT_V3(b);

	add_v3_v3v3(r, a, b);
	angle = ((float)(1.0 / (M_PI / 2.0)) *
	         /* normally we would only multiply by 2,
	          * but instead of an angle make this 0-1 factor */
	         2.0f) *
	        acosf(normalize_v3(r) / 2.0f);
	mul_v3_fl(r, angle);
}
/**
 * Same as mid_v3_v3v3_angle_weighted
 * but \a r is assumed to be accumulated normals, divided by their total.
 */
void mid_v3_angle_weighted(float r[3])
{
	/* trick, we want the middle of 2 normals as well as the angle between them
	 * avoid multiple calculations by */
	float angle;

	/* double check they are normalized */
	BLI_assert(len_squared_v3(r) <= 1.0f + FLT_EPSILON);

	angle = ((float)(1.0 / (M_PI / 2.0)) *
	         /* normally we would only multiply by 2,
	          * but instead of an angle make this 0-1 factor */
	         2.0f) *
	        acosf(normalize_v3(r));
	mul_v3_fl(r, angle);
}

/**
 * Equivalent to:
 * interp_v3_v3v3(v, v1, v2, -1.0f);
 */

void flip_v4_v4v4(float v[4], const float v1[4], const float v2[4])
{
	v[0] = v1[0] + (v1[0] - v2[0]);
	v[1] = v1[1] + (v1[1] - v2[1]);
	v[2] = v1[2] + (v1[2] - v2[2]);
	v[3] = v1[3] + (v1[3] - v2[3]);
}

void flip_v3_v3v3(float v[3], const float v1[3], const float v2[3])
{
	v[0] = v1[0] + (v1[0] - v2[0]);
	v[1] = v1[1] + (v1[1] - v2[1]);
	v[2] = v1[2] + (v1[2] - v2[2]);
}

void flip_v2_v2v2(float v[2], const float v1[2], const float v2[2])
{
	v[0] = v1[0] + (v1[0] - v2[0]);
	v[1] = v1[1] + (v1[1] - v2[1]);
}

/********************************** Angles ***********************************/

/* Return the angle in radians between vecs 1-2 and 2-3 in radians
 * If v1 is a shoulder, v2 is the elbow and v3 is the hand,
 * this would return the angle at the elbow.
 *
 * note that when v1/v2/v3 represent 3 points along a straight line
 * that the angle returned will be pi (180deg), rather then 0.0
 */
float angle_v3v3v3(const float v1[3], const float v2[3], const float v3[3])
{
	float vec1[3], vec2[3];

	sub_v3_v3v3(vec1, v2, v1);
	sub_v3_v3v3(vec2, v2, v3);
	normalize_v3(vec1);
	normalize_v3(vec2);

	return angle_normalized_v3v3(vec1, vec2);
}

/* Quicker than full angle computation */
float cos_v3v3v3(const float p1[3], const float p2[3], const float p3[3])
{
	float vec1[3], vec2[3];

	sub_v3_v3v3(vec1, p2, p1);
	sub_v3_v3v3(vec2, p2, p3);
	normalize_v3(vec1);
	normalize_v3(vec2);

	return dot_v3v3(vec1, vec2);
}

/* Return the shortest angle in radians between the 2 vectors */
float angle_v3v3(const float v1[3], const float v2[3])
{
	float vec1[3], vec2[3];

	normalize_v3_v3(vec1, v1);
	normalize_v3_v3(vec2, v2);

	return angle_normalized_v3v3(vec1, vec2);
}

float angle_v2v2v2(const float v1[2], const float v2[2], const float v3[2])
{
	float vec1[2], vec2[2];

	vec1[0] = v2[0] - v1[0];
	vec1[1] = v2[1] - v1[1];

	vec2[0] = v2[0] - v3[0];
	vec2[1] = v2[1] - v3[1];

	normalize_v2(vec1);
	normalize_v2(vec2);

	return angle_normalized_v2v2(vec1, vec2);
}

/* Return the shortest angle in radians between the 2 vectors */
float angle_v2v2(const float v1[2], const float v2[2])
{
	float vec1[2], vec2[2];

	vec1[0] = v1[0];
	vec1[1] = v1[1];

	vec2[0] = v2[0];
	vec2[1] = v2[1];

	normalize_v2(vec1);
	normalize_v2(vec2);

	return angle_normalized_v2v2(vec1, vec2);
}

float angle_signed_v2v2(const float v1[2], const float v2[2])
{
	const float perp_dot = (v1[1] * v2[0]) - (v1[0] * v2[1]);
	return atan2f(perp_dot, dot_v2v2(v1, v2));
}

float angle_normalized_v3v3(const float v1[3], const float v2[3])
{
	/* double check they are normalized */
	BLI_ASSERT_UNIT_V3(v1);
	BLI_ASSERT_UNIT_V3(v2);

	/* this is the same as acos(dot_v3v3(v1, v2)), but more accurate */
	if (dot_v3v3(v1, v2) >= 0.0f) {
		return 2.0f * saasin(len_v3v3(v1, v2) / 2.0f);
	}
	else {
		float v2_n[3];
		negate_v3_v3(v2_n, v2);
		return (float)M_PI - 2.0f * saasin(len_v3v3(v1, v2_n) / 2.0f);
	}
}

float angle_normalized_v2v2(const float v1[2], const float v2[2])
{
	/* double check they are normalized */
	BLI_ASSERT_UNIT_V2(v1);
	BLI_ASSERT_UNIT_V2(v2);

	/* this is the same as acos(dot_v3v3(v1, v2)), but more accurate */
	if (dot_v2v2(v1, v2) >= 0.0f) {
		return 2.0f * saasin(len_v2v2(v1, v2) / 2.0f);
	}
	else {
		float v2_n[2];
		negate_v2_v2(v2_n, v2);
		return (float)M_PI - 2.0f * saasin(len_v2v2(v1, v2_n) / 2.0f);
	}
}

/**
 * angle between 2 vectors defined by 3 coords, about an axis. */
float angle_on_axis_v3v3v3_v3(const float v1[3], const float v2[3], const float v3[3], const float axis[3])
{
	float v1_proj[3], v2_proj[3], tproj[3];

	sub_v3_v3v3(v1_proj, v1, v2);
	sub_v3_v3v3(v2_proj, v3, v2);

	/* project the vectors onto the axis */
	project_v3_v3v3(tproj, v1_proj, axis);
	sub_v3_v3(v1_proj, tproj);

	project_v3_v3v3(tproj, v2_proj, axis);
	sub_v3_v3(v2_proj, tproj);

	return angle_v3v3(v1_proj, v2_proj);
}

float angle_signed_on_axis_v3v3v3_v3(const float v1[3], const float v2[3], const float v3[3], const float axis[3])
{
	float v1_proj[3], v2_proj[3], tproj[3];
	float angle;

	sub_v3_v3v3(v1_proj, v1, v2);
	sub_v3_v3v3(v2_proj, v3, v2);

	/* project the vectors onto the axis */
	project_v3_v3v3(tproj, v1_proj, axis);
	sub_v3_v3(v1_proj, tproj);

	project_v3_v3v3(tproj, v2_proj, axis);
	sub_v3_v3(v2_proj, tproj);

	angle = angle_v3v3(v1_proj, v2_proj);

	/* calculate the sign (reuse 'tproj') */
	cross_v3_v3v3(tproj, v2_proj, v1_proj);
	if (dot_v3v3(tproj, axis) < 0.0f) {
		angle = ((float)(M_PI * 2.0)) - angle;
	}

	return angle;
}

void angle_tri_v3(float angles[3], const float v1[3], const float v2[3], const float v3[3])
{
	float ed1[3], ed2[3], ed3[3];

	sub_v3_v3v3(ed1, v3, v1);
	sub_v3_v3v3(ed2, v1, v2);
	sub_v3_v3v3(ed3, v2, v3);

	normalize_v3(ed1);
	normalize_v3(ed2);
	normalize_v3(ed3);

	angles[0] = (float)M_PI - angle_normalized_v3v3(ed1, ed2);
	angles[1] = (float)M_PI - angle_normalized_v3v3(ed2, ed3);
	// face_angles[2] = M_PI - angle_normalized_v3v3(ed3, ed1);
	angles[2] = (float)M_PI - (angles[0] + angles[1]);
}

void angle_quad_v3(float angles[4], const float v1[3], const float v2[3], const float v3[3], const float v4[3])
{
	float ed1[3], ed2[3], ed3[3], ed4[3];

	sub_v3_v3v3(ed1, v4, v1);
	sub_v3_v3v3(ed2, v1, v2);
	sub_v3_v3v3(ed3, v2, v3);
	sub_v3_v3v3(ed4, v3, v4);

	normalize_v3(ed1);
	normalize_v3(ed2);
	normalize_v3(ed3);
	normalize_v3(ed4);

	angles[0] = (float)M_PI - angle_normalized_v3v3(ed1, ed2);
	angles[1] = (float)M_PI - angle_normalized_v3v3(ed2, ed3);
	angles[2] = (float)M_PI - angle_normalized_v3v3(ed3, ed4);
	angles[3] = (float)M_PI - angle_normalized_v3v3(ed4, ed1);
}

void angle_poly_v3(float *angles, const float *verts[3], int len)
{
	int i;
	float vec[3][3];

	sub_v3_v3v3(vec[2], verts[len - 1], verts[0]);
	normalize_v3(vec[2]);
	for (i = 0; i < len; i++) {
		sub_v3_v3v3(vec[i % 3], verts[i % len], verts[(i + 1) % len]);
		normalize_v3(vec[i % 3]);
		angles[i] = (float)M_PI - angle_normalized_v3v3(vec[(i + 2) % 3], vec[i % 3]);
	}
}

/********************************* Geometry **********************************/

/* Project v1 on v2 */
void project_v2_v2v2(float c[2], const float v1[2], const float v2[2])
{
	float mul;
	mul = dot_v2v2(v1, v2) / dot_v2v2(v2, v2);

	c[0] = mul * v2[0];
	c[1] = mul * v2[1];
}

/* Project v1 on v2 */
void project_v3_v3v3(float c[3], const float v1[3], const float v2[3])
{
	float mul;
	mul = dot_v3v3(v1, v2) / dot_v3v3(v2, v2);

	c[0] = mul * v2[0];
	c[1] = mul * v2[1];
	c[2] = mul * v2[2];
}

/* project a vector on a plane defined by normal and a plane point p */
void project_v3_plane(float v[3], const float n[3], const float p[3])
{
	float vector[3];
	float mul;

	sub_v3_v3v3(vector, v, p);
	mul = dot_v3v3(vector, n) / len_squared_v3(n);

	mul_v3_v3fl(vector, n, mul);

	sub_v3_v3(v, vector);
}

/* Returns a vector bisecting the angle at v2 formed by v1, v2 and v3 */
void bisect_v3_v3v3v3(float out[3], const float v1[3], const float v2[3], const float v3[3])
{
	float d_12[3], d_23[3];
	sub_v3_v3v3(d_12, v2, v1);
	sub_v3_v3v3(d_23, v3, v2);
	normalize_v3(d_12);
	normalize_v3(d_23);
	add_v3_v3v3(out, d_12, d_23);
	normalize_v3(out);
}

/**
 * Returns a reflection vector from a vector and a normal vector
 * reflect = vec - ((2 * DotVecs(vec, mirror)) * mirror)
 */
void reflect_v3_v3v3(float out[3], const float vec[3], const float normal[3])
{
	const float dot2 = 2.0f * dot_v3v3(vec, normal);

	BLI_ASSERT_UNIT_V3(normal);

	out[0] = vec[0] - (dot2 * normal[0]);
	out[1] = vec[1] - (dot2 * normal[1]);
	out[2] = vec[2] - (dot2 * normal[2]);
}

/**
 * Takes a vector and computes 2 orthogonal directions.
 *
 * \note if \a n is n unit length, computed values will be too.
 */
void ortho_basis_v3v3_v3(float r_n1[3], float r_n2[3], const float n[3])
{
	const float eps = FLT_EPSILON;
	const float f = len_squared_v2(n);

	if (f > eps) {
		const float d = 1.0f / sqrtf(f);

		BLI_assert(finite(d));

		r_n1[0] =  n[1] * d;
		r_n1[1] = -n[0] * d;
		r_n1[2] =  0.0f;
		r_n2[0] = -n[2] * r_n1[1];
		r_n2[1] =  n[2] * r_n1[0];
		r_n2[2] =  n[0] * r_n1[1] - n[1] * r_n1[0];
	}
	else {
		/* degenerate case */
		r_n1[0] = (n[2] < 0.0f) ? -1.0f : 1.0f;
		r_n1[1] = r_n1[2] = r_n2[0] = r_n2[2] = 0.0f;
		r_n2[1] = 1.0f;
	}
}

/**
 * Calculates \a p - a perpendicular vector to \a v
 *
 * \note return vector won't maintain same length.
 */
void ortho_v3_v3(float p[3], const float v[3])
{
	const int axis = axis_dominant_v3_single(v);

	BLI_assert(p != v);

	switch (axis) {
		case 0:
			p[0] = -v[1] - v[2];
			p[1] =  v[0];
			p[2] =  v[0];
			break;
		case 1:
			p[0] =  v[1];
			p[1] = -v[0] - v[2];
			p[2] =  v[1];
			break;
		case 2:
			p[0] =  v[2];
			p[1] =  v[2];
			p[2] = -v[0] - v[1];
			break;
	}
}

/**
 * no brainer compared to v3, just have for consistency.
 */
void ortho_v2_v2(float p[2], const float v[2])
{
	BLI_assert(p != v);

	p[0] = -v[1];
	p[1] =  v[0];
}

/* Rotate a point p by angle theta around an arbitrary axis r
 * http://local.wasp.uwa.edu.au/~pbourke/geometry/
 */
void rotate_normalized_v3_v3v3fl(float r[3], const float p[3], const float axis[3], const float angle)
{
	const float costheta = cosf(angle);
	const float sintheta = sinf(angle);

	/* double check they are normalized */
	BLI_ASSERT_UNIT_V3(axis);

	r[0] = ((costheta + (1 - costheta) * axis[0] * axis[0]) * p[0]) +
	       (((1 - costheta) * axis[0] * axis[1] - axis[2] * sintheta) * p[1]) +
	       (((1 - costheta) * axis[0] * axis[2] + axis[1] * sintheta) * p[2]);

	r[1] = (((1 - costheta) * axis[0] * axis[1] + axis[2] * sintheta) * p[0]) +
	       ((costheta + (1 - costheta) * axis[1] * axis[1]) * p[1]) +
	       (((1 - costheta) * axis[1] * axis[2] - axis[0] * sintheta) * p[2]);

	r[2] = (((1 - costheta) * axis[0] * axis[2] - axis[1] * sintheta) * p[0]) +
	       (((1 - costheta) * axis[1] * axis[2] + axis[0] * sintheta) * p[1]) +
	       ((costheta + (1 - costheta) * axis[2] * axis[2]) * p[2]);
}

void rotate_v3_v3v3fl(float r[3], const float p[3], const float axis[3], const float angle)
{
	float axis_n[3];

	normalize_v3_v3(axis_n, axis);

	rotate_normalized_v3_v3v3fl(r, p, axis_n, angle);
}

/*********************************** Other ***********************************/

void print_v2(const char *str, const float v[2])
{
	printf("%s: %.3f %.3f\n", str, v[0], v[1]);
}

void print_v3(const char *str, const float v[3])
{
	printf("%s: %.3f %.3f %.3f\n", str, v[0], v[1], v[2]);
}

void print_v4(const char *str, const float v[4])
{
	printf("%s: %.3f %.3f %.3f %.3f\n", str, v[0], v[1], v[2], v[3]);
}

void print_vn(const char *str, const float v[], const int n)
{
	int i = 0;
	printf("%s[%d]:", str, n);
	while (i < n) {
		printf(" %.3f", v[i++]);
	}
	printf("\n");
}

void minmax_v3v3_v3(float min[3], float max[3], const float vec[3])
{
	if (min[0] > vec[0]) min[0] = vec[0];
	if (min[1] > vec[1]) min[1] = vec[1];
	if (min[2] > vec[2]) min[2] = vec[2];

	if (max[0] < vec[0]) max[0] = vec[0];
	if (max[1] < vec[1]) max[1] = vec[1];
	if (max[2] < vec[2]) max[2] = vec[2];
}

void minmax_v2v2_v2(float min[2], float max[2], const float vec[2])
{
	if (min[0] > vec[0]) min[0] = vec[0];
	if (min[1] > vec[1]) min[1] = vec[1];

	if (max[0] < vec[0]) max[0] = vec[0];
	if (max[1] < vec[1]) max[1] = vec[1];
}

/** ensure \a v1 is \a dist from \a v2 */
void dist_ensure_v3_v3fl(float v1[3], const float v2[3], const float dist)
{
	if (!equals_v3v3(v2, v1)) {
		float nor[3];

		sub_v3_v3v3(nor, v1, v2);
		normalize_v3(nor);
		madd_v3_v3v3fl(v1, v2, nor, dist);
	}
}

void dist_ensure_v2_v2fl(float v1[2], const float v2[2], const float dist)
{
	if (!equals_v2v2(v2, v1)) {
		float nor[2];

		sub_v2_v2v2(nor, v1, v2);
		normalize_v2(nor);
		madd_v2_v2v2fl(v1, v2, nor, dist);
	}
}

void axis_sort_v3(const float axis_values[3], int r_axis_order[3])
{
	float v[3];
	copy_v3_v3(v, axis_values);

#define SWAP_AXIS(a, b) { \
	SWAP(float, v[a],            v[b]); \
	SWAP(int,   r_axis_order[a], r_axis_order[b]); \
} (void)0

	if (v[0] < v[1]) {
		if (v[2] < v[0]) {  SWAP_AXIS(0, 2); }
	}
	else {
		if (v[1] < v[2]) { SWAP_AXIS(0, 1); }
		else             { SWAP_AXIS(0, 2); }
	}
	if (v[2] < v[1])     { SWAP_AXIS(1, 2); }

#undef SWAP_AXIS
}

/***************************** Array Functions *******************************/

MINLINE double sqr_db(double f)
{
	return f * f;
}

double dot_vn_vn(const float *array_src_a, const float *array_src_b, const int size)
{
	double d = 0.0f;
	const float *array_pt_a = array_src_a + (size - 1);
	const float *array_pt_b = array_src_b + (size - 1);
	int i = size;
	while (i--) {
		d += (double)(*(array_pt_a--) * *(array_pt_b--));
	}
	return d;
}

double len_squared_vn(const float *array, const int size)
{
	double d = 0.0f;
	const float *array_pt = array + (size - 1);
	int i = size;
	while (i--) {
		d += sqr_db((double)(*(array_pt--)));
	}
	return d;
}

float normalize_vn_vn(float *array_tar, const float *array_src, const int size)
{
	double d = len_squared_vn(array_src, size);
	float d_sqrt;
	if (d > 1.0e-35) {
		d_sqrt = (float)sqrt(d);
		mul_vn_vn_fl(array_tar, array_src, size, 1.0f / d_sqrt);
	}
	else {
		fill_vn_fl(array_tar, size, 0.0f);
		d_sqrt = 0.0f;
	}
	return d_sqrt;
}

float normalize_vn(float *array_tar, const int size)
{
	return normalize_vn_vn(array_tar, array_tar, size);
}

void range_vn_i(int *array_tar, const int size, const int start)
{
	int *array_pt = array_tar + (size - 1);
	int j = start + (size - 1);
	int i = size;
	while (i--) {
		*(array_pt--) = j--;
	}
}

void range_vn_fl(float *array_tar, const int size, const float start, const float step)
{
	float *array_pt = array_tar + (size - 1);
	int i = size;
	while (i--) {
		*(array_pt--) = start + step * (float)(i);
	}
}

void negate_vn(float *array_tar, const int size)
{
	float *array_pt = array_tar + (size - 1);
	int i = size;
	while (i--) {
		*(array_pt--) *= -1.0f;
	}
}

void negate_vn_vn(float *array_tar, const float *array_src, const int size)
{
	float *tar = array_tar + (size - 1);
	const float *src = array_src + (size - 1);
	int i = size;
	while (i--) {
		*(tar--) = -*(src--);
	}
}

void mul_vn_fl(float *array_tar, const int size, const float f)
{
	float *array_pt = array_tar + (size - 1);
	int i = size;
	while (i--) {
		*(array_pt--) *= f;
	}
}

void mul_vn_vn_fl(float *array_tar, const float *array_src, const int size, const float f)
{
	float *tar = array_tar + (size - 1);
	const float *src = array_src + (size - 1);
	int i = size;
	while (i--) {
		*(tar--) = *(src--) * f;
	}
}

void add_vn_vn(float *array_tar, const float *array_src, const int size)
{
	float *tar = array_tar + (size - 1);
	const float *src = array_src + (size - 1);
	int i = size;
	while (i--) {
		*(tar--) += *(src--);
	}
}

void add_vn_vnvn(float *array_tar, const float *array_src_a, const float *array_src_b, const int size)
{
	float *tar = array_tar + (size - 1);
	const float *src_a = array_src_a + (size - 1);
	const float *src_b = array_src_b + (size - 1);
	int i = size;
	while (i--) {
		*(tar--) = *(src_a--) + *(src_b--);
	}
}

void madd_vn_vn(float *array_tar, const float *array_src, const float f, const int size)
{
	float *tar = array_tar + (size - 1);
	const float *src = array_src + (size - 1);
	int i = size;
	while (i--) {
		*(tar--) += *(src--) * f;
	}
}

void madd_vn_vnvn(float *array_tar, const float *array_src_a, const float *array_src_b, const float f, const int size)
{
	float *tar = array_tar + (size - 1);
	const float *src_a = array_src_a + (size - 1);
	const float *src_b = array_src_b + (size - 1);
	int i = size;
	while (i--) {
		*(tar--) = *(src_a--) + (*(src_b--) * f);
	}
}

void sub_vn_vn(float *array_tar, const float *array_src, const int size)
{
	float *tar = array_tar + (size - 1);
	const float *src = array_src + (size - 1);
	int i = size;
	while (i--) {
		*(tar--) -= *(src--);
	}
}

void sub_vn_vnvn(float *array_tar, const float *array_src_a, const float *array_src_b, const int size)
{
	float *tar = array_tar + (size - 1);
	const float *src_a = array_src_a + (size - 1);
	const float *src_b = array_src_b + (size - 1);
	int i = size;
	while (i--) {
		*(tar--) = *(src_a--) - *(src_b--);
	}
}

void msub_vn_vn(float *array_tar, const float *array_src, const float f, const int size)
{
	float *tar = array_tar + (size - 1);
	const float *src = array_src + (size - 1);
	int i = size;
	while (i--) {
		*(tar--) -= *(src--) * f;
	}
}

void msub_vn_vnvn(float *array_tar, const float *array_src_a, const float *array_src_b, const float f, const int size)
{
	float *tar = array_tar + (size - 1);
	const float *src_a = array_src_a + (size - 1);
	const float *src_b = array_src_b + (size - 1);
	int i = size;
	while (i--) {
		*(tar--) = *(src_a--) - (*(src_b--) * f);
	}
}

void interp_vn_vn(float *array_tar, const float *array_src, const float t, const int size)
{
	const float s = 1.0f - t;
	float *tar = array_tar + (size - 1);
	const float *src = array_src + (size - 1);
	int i = size;
	while (i--) {
		*(tar) = (s * *(tar)) + (t * *(src));
		tar--;
		src--;
	}
}

void fill_vn_i(int *array_tar, const int size, const int val)
{
	int *tar = array_tar + (size - 1);
	int i = size;
	while (i--) {
		*(tar--) = val;
	}
}

void fill_vn_short(short *array_tar, const int size, const short val)
{
	short *tar = array_tar + (size - 1);
	int i = size;
	while (i--) {
		*(tar--) = val;
	}
}

void fill_vn_ushort(unsigned short *array_tar, const int size, const unsigned short val)
{
	unsigned short *tar = array_tar + (size - 1);
	int i = size;
	while (i--) {
		*(tar--) = val;
	}
}

void fill_vn_fl(float *array_tar, const int size, const float val)
{
	float *tar = array_tar + (size - 1);
	int i = size;
	while (i--) {
		*(tar--) = val;
	}
}
