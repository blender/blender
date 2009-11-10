/**
 * $Id$
 *
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 
 * The Original Code is: some of this file.
 *
 * ***** END GPL LICENSE BLOCK *****
 * */

#include <float.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "BLI_math.h"

/********************************** Init *************************************/

void zero_v2(float r[2])
{
	r[0]= 0.0f;
	r[1]= 0.0f;
}

void zero_v3(float r[3])
{
	r[0]= 0.0f;
	r[1]= 0.0f;
	r[2]= 0.0f;
}

void copy_v2_v2(float r[2], float a[2])
{
	r[0]= a[0];
	r[1]= a[1];
}

void copy_v3_v3(float r[3], float a[3])
{
	r[0]= a[0];
	r[1]= a[1];
	r[2]= a[2];
}

/********************************* Arithmetic ********************************/

void add_v2_v2(float *r, float *a)
{
	r[0] += a[0];
	r[1] += a[1];
}

void add_v2_v2v2(float *r, float *a, float *b)
{
	r[0]= a[0] + b[0];
	r[1]= a[1] + b[1];
}

void add_v3_v3(float *r, float *a)
{
	r[0] += a[0];
	r[1] += a[1];
	r[1] += a[1];
}

void add_v3_v3v3(float *r, float *a, float *b)
{
	r[0]= a[0] + b[0];
	r[1]= a[1] + b[1];
	r[2]= a[2] + b[2];
}

void sub_v2_v2(float *r, float *a)
{
	r[0] -= a[0];
	r[1] -= a[1];
}

void sub_v2_v2v2(float *r, float *a, float *b)
{
	r[0]= a[0] - b[0];
	r[1]= a[1] - b[1];
}

void sub_v3_v3(float *r, float *a)
{
	r[0] -= a[0];
	r[1] -= a[1];
	r[1] -= a[1];
}

void sub_v3_v3v3(float *r, float *a, float *b)
{
	r[0]= a[0] - b[0];
	r[1]= a[1] - b[1];
	r[2]= a[2] - b[2];
}

void mul_v2_fl(float *v1, float f)
{
	v1[0]*= f;
	v1[1]*= f;
}

void mul_v3_fl(float r[3], float f)
{
	r[0] *= f;
	r[1] *= f;
	r[2] *= f;
}

void mul_v3_v3fl(float r[3], float a[3], float f)
{
	r[0]= a[0]*f;
	r[1]= a[1]*f;
	r[2]= a[2]*f;
}

void mul_v3_v3(float r[3], float a[3])
{
	r[0] *= a[0];
	r[1] *= a[1];
	r[2] *= a[2];
}

void mul_v3_v3v3(float *v, float *v1, float *v2)
{
	v[0] = v1[0] * v2[0];
	v[1] = v1[1] * v2[1];
	v[2] = v1[2] * v2[2];
}

void negate_v3(float r[3])
{
	r[0]= -r[0];
	r[1]= -r[1];
	r[2]= -r[2];
}

void negate_v3_v3(float r[3], float a[3])
{
	r[0]= -a[0];
	r[1]= -a[1];
	r[2]= -a[2];
}

float dot_v2v2(float *a, float *b)
{
	return a[0]*b[0] + a[1]*b[1];
}

float dot_v3v3(float a[3], float b[3])
{
	return a[0]*b[0] + a[1]*b[1] + a[2]*b[2];
}

void cross_v3_v3v3(float r[3], float a[3], float b[3])
{
	r[0]= a[1]*b[2] - a[2]*b[1];
	r[1]= a[2]*b[0] - a[0]*b[2];
	r[2]= a[0]*b[1] - a[1]*b[0];
}

void star_m3_v3(float mat[][3], float *vec)
{
	mat[0][0]= mat[1][1]= mat[2][2]= 0.0;
	mat[0][1]= -vec[2];	
	mat[0][2]= vec[1];
	mat[1][0]= vec[2];	
	mat[1][2]= -vec[0];
	mat[2][0]= -vec[1];	
	mat[2][1]= vec[0];
	
}

/*********************************** Length **********************************/

float len_v2(float *v)
{
	return (float)sqrt(v[0]*v[0] + v[1]*v[1]);
}

float len_v2v2(float *v1, float *v2)
{
	float x, y;

	x = v1[0]-v2[0];
	y = v1[1]-v2[1];
	return (float)sqrt(x*x+y*y);
}

float len_v3(float a[3])
{
	return sqrtf(dot_v3v3(a, a));
}

float len_v3v3(float a[3], float b[3])
{
	float d[3];

	sub_v3_v3v3(d, b, a);
	return len_v3(d);
}

float normalize_v2(float n[2])
{
	float d;
	
	d= n[0]*n[0]+n[1]*n[1];

	if(d>1.0e-35f) {
		d= (float)sqrt(d);
		n[0]/=d; 
		n[1]/=d; 
	} else {
		n[0]=n[1]= 0.0f;
		d= 0.0f;
	}
	return d;
}

float normalize_v3(float n[3])
{
	float d= dot_v3v3(n, n);

	/* a larger value causes normalize errors in a
	   scaled down models with camera xtreme close */
	if(d > 1.0e-35f) {
		d= sqrtf(d);
		mul_v3_fl(n, 1.0f/d);
	}
	else {
		zero_v3(n);
		d= 0.0f;
	}

	return d;
}

/******************************* Interpolation *******************************/

void interp_v2_v2v2(float *target, const float *a, const float *b, const float t)
{
	const float s = 1.0f-t;

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



void interp_v3_v3v3(float *target, const float *a, const float *b, const float t)
{
	const float s = 1.0f-t;

	target[0]= s*a[0] + t*b[0];
	target[1]= s*a[1] + t*b[1];
	target[2]= s*a[2] + t*b[2];
}

/* weight 3 vectors,
 * 'w' must be unit length but is not a vector, just 3 weights */
void interp_v3_v3v3v3(float p[3], const float v1[3], const float v2[3], const float v3[3], const float w[3])
{
	p[0] = v1[0]*w[0] + v2[0]*w[1] + v3[0]*w[2];
	p[1] = v1[1]*w[0] + v2[1]*w[1] + v3[1]*w[2];
	p[2] = v1[2]*w[0] + v2[2]*w[1] + v3[2]*w[2];
}

void mid_v3_v3v3(float *v, float *v1, float *v2)
{
	v[0]= 0.5f*(v1[0]+ v2[0]);
	v[1]= 0.5f*(v1[1]+ v2[1]);
	v[2]= 0.5f*(v1[2]+ v2[2]);
}

/********************************* Comparison ********************************/

int is_zero_v3(float *v)
{
	return (v[0] == 0 && v[1] == 0 && v[2] == 0);
}

int equals_v3v3(float *v1, float *v2)
{
	return ((v1[0]==v2[0]) && (v1[1]==v2[1]) && (v1[2]==v2[2]));
}

int compare_v3v3(float *v1, float *v2, float limit)
{
	if(fabs(v1[0]-v2[0])<limit)
		if(fabs(v1[1]-v2[1])<limit)
			if(fabs(v1[2]-v2[2])<limit)
				return 1;

	return 0;
}

int compare_len_v3v3(float *v1, float *v2, float limit)
{
    float x,y,z;

	x=v1[0]-v2[0];
	y=v1[1]-v2[1];
	z=v1[2]-v2[2];

	return ((x*x + y*y + z*z) < (limit*limit));
}

int compare_v4v4(float *v1, float *v2, float limit)
{
	if(fabs(v1[0]-v2[0])<limit)
		if(fabs(v1[1]-v2[1])<limit)
			if(fabs(v1[2]-v2[2])<limit)
				if(fabs(v1[3]-v2[3])<limit)
					return 1;

	return 0;
}

/********************************** Angles ***********************************/

/* Return the angle in radians between vecs 1-2 and 2-3 in radians
   If v1 is a shoulder, v2 is the elbow and v3 is the hand,
   this would return the angle at the elbow */
float angle_v3v3v3(float *v1, float *v2, float *v3)
{
	float vec1[3], vec2[3];

	sub_v3_v3v3(vec1, v2, v1);
	sub_v3_v3v3(vec2, v2, v3);
	normalize_v3(vec1);
	normalize_v3(vec2);

	return angle_normalized_v3v3(vec1, vec2);
}

float angle_v2v2v2(float *v1, float *v2, float *v3)
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
float angle_v2v2(float *v1, float *v2)
{
	float vec1[3], vec2[3];

	copy_v3_v3(vec1, v1);
	copy_v3_v3(vec2, v2);
	normalize_v3(vec1);
	normalize_v3(vec2);

	return angle_normalized_v3v3(vec1, vec2);
}

float angle_normalized_v3v3(float *v1, float *v2)
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

float angle_normalized_v2v2(float *v1, float *v2)
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

/********************************* Geometry **********************************/

/* Project v1 on v2 */
void project_v3_v3v3(float *c, float *v1, float *v2)
{
	float mul;
	mul = dot_v3v3(v1, v2) / dot_v3v3(v2, v2);
	
	c[0] = mul * v2[0];
	c[1] = mul * v2[1];
	c[2] = mul * v2[2];
}

/* Returns a vector bisecting the angle at v2 formed by v1, v2 and v3 */
void bisect_v3_v3v3v3(float *out, float *v1, float *v2, float *v3)
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
void reflect_v3_v3v3(float *out, float *v1, float *v2)
{
	float vec[3], normal[3];
	float reflect[3] = {0.0f, 0.0f, 0.0f};
	float dot2;

	copy_v3_v3(vec, v1);
	copy_v3_v3(normal, v2);

	normalize_v3(normal);

	dot2 = 2 * dot_v3v3(vec, normal);

	reflect[0] = vec[0] - (dot2 * normal[0]);
	reflect[1] = vec[1] - (dot2 * normal[1]);
	reflect[2] = vec[2] - (dot2 * normal[2]);

	copy_v3_v3(out, reflect);
}

void ortho_basis_v3v3_v3(float *v1, float *v2, float *v)
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

/*********************************** Other ***********************************/

void print_v2(char *str, float v[2])
{
	printf("%s: %.3f %.3f\n", str, v[0], v[1]);
}

void print_v3(char *str, float v[3])
{
	printf("%s: %.3f %.3f %.3f\n", str, v[0], v[1], v[2]);
}

void print_v4(char *str, float v[4])
{
	printf("%s: %.3f %.3f %.3f %.3f\n", str, v[0], v[1], v[2], v[3]);
}

void normal_short_to_float_v3(float *out, short *in)
{
	out[0] = in[0]*(1.0f/32767.0f);
	out[1] = in[1]*(1.0f/32767.0f);
	out[2] = in[2]*(1.0f/32767.0f);
}

void normal_float_to_short_v3(short *out, float *in)
{
	out[0] = (short)(in[0]*32767.0f);
	out[1] = (short)(in[1]*32767.0f);
	out[2] = (short)(in[2]*32767.0f);
}

void minmax_v3_v3v3(float *min, float *max, float *vec)
{
	if(min[0]>vec[0]) min[0]= vec[0];
	if(min[1]>vec[1]) min[1]= vec[1];
	if(min[2]>vec[2]) min[2]= vec[2];

	if(max[0]<vec[0]) max[0]= vec[0];
	if(max[1]<vec[1]) max[1]= vec[1];
	if(max[2]<vec[2]) max[2]= vec[2];
}

