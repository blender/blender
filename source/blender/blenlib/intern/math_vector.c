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
 
 * The Original Code is: some of this file.
 *
 * ***** END GPL LICENSE BLOCK *****
 * */

/** \file blender/blenlib/intern/math_vector.c
 *  \ingroup bli
 */



#include "BLI_math.h"

//******************************* Interpolation *******************************/

void interp_v2_v2v2(float target[2], const float a[2], const float b[2], const float t)
{
	float s = 1.0f-t;

	target[0]= s*a[0] + t*b[0];
	target[1]= s*a[1] + t*b[1];
}

/* weight 3 2D vectors,
 * 'w' must be unit length but is not a vector, just 3 weights */
void interp_v2_v2v2v2(float p[2], const float v1[2], const float v2[2], const float v3[2], const float w[3])
{
	p[0] = v1[0]*w[0] + v2[0]*w[1] + v3[0]*w[2];
	p[1] = v1[1]*w[0] + v2[1]*w[1] + v3[1]*w[2];
}

void interp_v3_v3v3(float target[3], const float a[3], const float b[3], const float t)
{
	float s = 1.0f-t;

	target[0]= s*a[0] + t*b[0];
	target[1]= s*a[1] + t*b[1];
	target[2]= s*a[2] + t*b[2];
}

void interp_v4_v4v4(float target[4], const float a[4], const float b[4], const float t)
{
	float s = 1.0f-t;

	target[0]= s*a[0] + t*b[0];
	target[1]= s*a[1] + t*b[1];
	target[2]= s*a[2] + t*b[2];
	target[3]= s*a[3] + t*b[3];
}

/* weight 3 vectors,
 * 'w' must be unit length but is not a vector, just 3 weights */
void interp_v3_v3v3v3(float p[3], const float v1[3], const float v2[3], const float v3[3], const float w[3])
{
	p[0] = v1[0]*w[0] + v2[0]*w[1] + v3[0]*w[2];
	p[1] = v1[1]*w[0] + v2[1]*w[1] + v3[1]*w[2];
	p[2] = v1[2]*w[0] + v2[2]*w[1] + v3[2]*w[2];
}

/* weight 3 vectors,
 * 'w' must be unit length but is not a vector, just 4 weights */
void interp_v3_v3v3v3v3(float p[3], const float v1[3], const float v2[3], const float v3[3], const float v4[3], const float w[4])
{
	p[0] = v1[0]*w[0] + v2[0]*w[1] + v3[0]*w[2] + v4[0]*w[3];
	p[1] = v1[1]*w[0] + v2[1]*w[1] + v3[1]*w[2] + v4[1]*w[3];
	p[2] = v1[2]*w[0] + v2[2]*w[1] + v3[2]*w[2] + v4[2]*w[3];
}

void interp_v4_v4v4v4(float p[4], const float v1[4], const float v2[4], const float v3[4], const float w[3])
{
	p[0] = v1[0]*w[0] + v2[0]*w[1] + v3[0]*w[2];
	p[1] = v1[1]*w[0] + v2[1]*w[1] + v3[1]*w[2];
	p[2] = v1[2]*w[0] + v2[2]*w[1] + v3[2]*w[2];
	p[3] = v1[3]*w[0] + v2[3]*w[1] + v3[3]*w[2];
}

void interp_v4_v4v4v4v4(float p[4], const float v1[4], const float v2[4], const float v3[4], const float v4[4], const float w[4])
{
	p[0] = v1[0]*w[0] + v2[0]*w[1] + v3[0]*w[2] + v4[0]*w[3];
	p[1] = v1[1]*w[0] + v2[1]*w[1] + v3[1]*w[2] + v4[1]*w[3];
	p[2] = v1[2]*w[0] + v2[2]*w[1] + v3[2]*w[2] + v4[2]*w[3];
	p[3] = v1[3]*w[0] + v2[3]*w[1] + v3[3]*w[2] + v4[3]*w[3];
}

void mid_v3_v3v3(float v[3], const float v1[3], const float v2[3])
{
	v[0]= 0.5f*(v1[0] + v2[0]);
	v[1]= 0.5f*(v1[1] + v2[1]);
	v[2]= 0.5f*(v1[2] + v2[2]);
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

	vec1[0] = v2[0]-v1[0];
	vec1[1] = v2[1]-v1[1];
	
	vec2[0] = v2[0]-v3[0];
	vec2[1] = v2[1]-v3[1];
	
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

float angle_normalized_v3v3(const float v1[3], const float v2[3])
{
	/* this is the same as acos(dot_v3v3(v1, v2)), but more accurate */
	if (dot_v3v3(v1, v2) < 0.0f) {
		float vec[3];
		
		vec[0]= -v2[0];
		vec[1]= -v2[1];
		vec[2]= -v2[2];
		
		return (float)M_PI - 2.0f*(float)saasin(len_v3v3(vec, v1)/2.0f);
	}
	else
		return 2.0f*(float)saasin(len_v3v3(v2, v1)/2.0f);
}

float angle_normalized_v2v2(const float v1[2], const float v2[2])
{
	/* this is the same as acos(dot_v3v3(v1, v2)), but more accurate */
	if (dot_v2v2(v1, v2) < 0.0f) {
		float vec[2];
		
		vec[0]= -v2[0];
		vec[1]= -v2[1];
		
		return (float)M_PI - 2.0f*saasin(len_v2v2(vec, v1)/2.0f);
	}
	else
		return 2.0f*(float)saasin(len_v2v2(v2, v1)/2.0f);
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

	angles[0]= (float)M_PI - angle_normalized_v3v3(ed1, ed2);
	angles[1]= (float)M_PI - angle_normalized_v3v3(ed2, ed3);
	// face_angles[2] = M_PI - angle_normalized_v3v3(ed3, ed1);
	angles[2]= (float)M_PI - (angles[0] + angles[1]);
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

	angles[0]= (float)M_PI - angle_normalized_v3v3(ed1, ed2);
	angles[1]= (float)M_PI - angle_normalized_v3v3(ed2, ed3);
	angles[2]= (float)M_PI - angle_normalized_v3v3(ed3, ed4);
	angles[3]= (float)M_PI - angle_normalized_v3v3(ed4, ed1);
}

void angle_poly_v3(float *angles, const float *verts[3], int len)
{
	int i;
	float vec[3][3];

	sub_v3_v3v3(vec[2], verts[len-1], verts[0]);
	normalize_v3(vec[2]);
	for (i = 0; i < len; i++) {
		sub_v3_v3v3(vec[i%3], verts[i%len], verts[(i+1)%len]);
		normalize_v3(vec[i%3]);
		angles[i] = (float)M_PI - angle_normalized_v3v3(vec[(i+2)%3], vec[i%3]);
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

/* Returns a reflection vector from a vector and a normal vector
reflect = vec - ((2 * DotVecs(vec, mirror)) * mirror)
*/
void reflect_v3_v3v3(float out[3], const float v1[3], const float v2[3])
{
	float vec[3], normal[3];
	float reflect[3] = {0.0f, 0.0f, 0.0f};
	float dot2;

	copy_v3_v3(vec, v1);
	copy_v3_v3(normal, v2);

	dot2 = 2 * dot_v3v3(vec, normal);

	reflect[0] = vec[0] - (dot2 * normal[0]);
	reflect[1] = vec[1] - (dot2 * normal[1]);
	reflect[2] = vec[2] - (dot2 * normal[2]);

	copy_v3_v3(out, reflect);
}

void ortho_basis_v3v3_v3(float v1[3], float v2[3], const float v[3])
{
	const float f = (float)sqrt(v[0]*v[0] + v[1]*v[1]);

	if (f < 1e-35f) {
		// degenerate case
		v1[0] = (v[2] < 0.0f) ? -1.0f : 1.0f;
		v1[1] = v1[2] = v2[0] = v2[2] = 0.0f;
		v2[1] = 1.0f;
	}
	else  {
		const float d= 1.0f/f;

		v1[0] =  v[1]*d;
		v1[1] = -v[0]*d;
		v1[2] = 0.0f;
		v2[0] = -v[2]*v1[1];
		v2[1] = v[2]*v1[0];
		v2[2] = v[0]*v1[1] - v[1]*v1[0];
	}
}

/* Rotate a point p by angle theta around an arbitrary axis r
   http://local.wasp.uwa.edu.au/~pbourke/geometry/
*/
void rotate_normalized_v3_v3v3fl(float r[3], const float p[3], const float axis[3], const float angle)
{
	const float costheta= cos(angle);
	const float sintheta= sin(angle);

	r[0]=	((costheta + (1 - costheta) * axis[0] * axis[0]) * p[0]) +
			(((1 - costheta) * axis[0] * axis[1] - axis[2] * sintheta) * p[1]) +
			(((1 - costheta) * axis[0] * axis[2] + axis[1] * sintheta) * p[2]);

	r[1]=	(((1 - costheta) * axis[0] * axis[1] + axis[2] * sintheta) * p[0]) +
			((costheta + (1 - costheta) * axis[1] * axis[1]) * p[1]) +
			(((1 - costheta) * axis[1] * axis[2] - axis[0] * sintheta) * p[2]);

	r[2]=	(((1 - costheta) * axis[0] * axis[2] - axis[1] * sintheta) * p[0]) +
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

void minmax_v3v3_v3(float min[3], float max[3], const float vec[3])
{
	if(min[0]>vec[0]) min[0]= vec[0];
	if(min[1]>vec[1]) min[1]= vec[1];
	if(min[2]>vec[2]) min[2]= vec[2];

	if(max[0]<vec[0]) max[0]= vec[0];
	if(max[1]<vec[1]) max[1]= vec[1];
	if(max[2]<vec[2]) max[2]= vec[2];
}


/***************************** Array Functions *******************************/

double dot_vn_vn(const float *array_src_a, const float *array_src_b, const int size)
{
	double d= 0.0f;
	const float *array_pt_a= array_src_a + (size-1);
	const float *array_pt_b= array_src_b + (size-1);
	int i= size;
	while(i--) { d += *(array_pt_a--) * *(array_pt_b--); }
	return d;
}

float normalize_vn_vn(float *array_tar, const float *array_src, const int size)
{
	double d= dot_vn_vn(array_tar, array_src, size);
	float d_sqrt;
	if (d > 1.0e-35) {
		d_sqrt= (float)sqrt(d);
		mul_vn_vn_fl(array_tar, array_src, size, 1.0f/d_sqrt);
	}
	else {
		fill_vn_fl(array_tar, size, 0.0f);
		d_sqrt= 0.0f;
	}
	return d_sqrt;
}

float normalize_vn(float *array_tar, const int size)
{
	return normalize_vn_vn(array_tar, array_tar, size);
}

void range_vn_i(int *array_tar, const int size, const int start)
{
	int *array_pt= array_tar + (size-1);
	int j= start + (size-1);
	int i= size;
	while(i--) { *(array_pt--) = j--; }
}

void range_vn_fl(float *array_tar, const int size, const float start, const float step)
{
	float *array_pt= array_tar + (size-1);
	int i= size;
	while(i--) {
		*(array_pt--) = start + step * (float)(i);
	}
}

void negate_vn(float *array_tar, const int size)
{
	float *array_pt= array_tar + (size-1);
	int i= size;
	while(i--) { *(array_pt--) *= -1.0f; }
}

void negate_vn_vn(float *array_tar, const float *array_src, const int size)
{
	float *tar= array_tar + (size-1);
	const float *src= array_src + (size-1);
	int i= size;
	while(i--) { *(tar--) = - *(src--); }
}

void mul_vn_fl(float *array_tar, const int size, const float f)
{
	float *array_pt= array_tar + (size-1);
	int i= size;
	while(i--) { *(array_pt--) *= f; }
}

void mul_vn_vn_fl(float *array_tar, const float *array_src, const int size, const float f)
{
	float *tar= array_tar + (size-1);
	const float *src= array_src + (size-1);
	int i= size;
	while(i--) { *(tar--) = *(src--) * f; }
}

void add_vn_vn(float *array_tar, const float *array_src, const int size)
{
	float *tar= array_tar + (size-1);
	const float *src= array_src + (size-1);
	int i= size;
	while(i--) { *(tar--) += *(src--); }
}

void add_vn_vnvn(float *array_tar, const float *array_src_a, const float *array_src_b, const int size)
{
	float *tar= array_tar + (size-1);
	const float *src_a= array_src_a + (size-1);
	const float *src_b= array_src_b + (size-1);
	int i= size;
	while(i--) { *(tar--) = *(src_a--) + *(src_b--); }
}

void sub_vn_vn(float *array_tar, const float *array_src, const int size)
{
	float *tar= array_tar + (size-1);
	const float *src= array_src + (size-1);
	int i= size;
	while(i--) { *(tar--) -= *(src--); }
}

void sub_vn_vnvn(float *array_tar, const float *array_src_a, const float *array_src_b, const int size)
{
	float *tar= array_tar + (size-1);
	const float *src_a= array_src_a + (size-1);
	const float *src_b= array_src_b + (size-1);
	int i= size;
	while(i--) { *(tar--) = *(src_a--) - *(src_b--); }
}

void fill_vn_i(int *array_tar, const int size, const int val)
{
	int *tar= array_tar + (size-1);
	int i= size;
	while(i--) { *(tar--) = val; }
}

void fill_vn_fl(float *array_tar, const int size, const float val)
{
	float *tar= array_tar + (size-1);
	int i= size;
	while(i--) { *(tar--) = val; }
}
