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
 *
 * The Original Code is: some of this file.
 *
 * ***** END GPL LICENSE BLOCK *****
 * */

#ifndef BLI_MATH_GEOM
#define BLI_MATH_GEOM

#ifdef __cplusplus
extern "C" {
#endif

/********************************** Polygons *********************************/

void cent_tri_v3(float r[3], float a[3], float b[3], float c[3]);
void cent_quad_v3(float r[3], float a[3], float b[3], float c[3], float d[3]);

float normal_tri_v3(float r[3], const float a[3], const float b[3], const float c[3]);
float normal_quad_v3(float r[3], const float a[3], const float b[3], const float c[3], const float d[3]);

float area_tri_v2(const float a[2], const float b[2], const float c[2]);
float area_tri_signed_v2(const float v1[2], const float v2[2], const float v3[2]);
float area_tri_v3(const float a[3], const float b[3], const float c[3]);
float area_quad_v3(const float a[3], const float b[3], const float c[3], const float d[3]);
float area_poly_v3(int nr, float verts[][3], float normal[3]);

/********************************* Distance **********************************/

float dist_to_line_v2(float p[2], float l1[2], float l2[2]);
float dist_to_line_segment_v2(float p[2], float l1[2], float l2[2]);

float dist_to_line_segment_v3(float p[3], float l1[3], float l2[3]);
float closest_to_line_v3(float r[3], float p[3], float l1[3], float l2[3]);
void closest_to_line_segment_v3(float r[3], float p[3], float l1[3], float l2[3]);

/******************************* Intersection ********************************/

/* TODO int return value consistency */

/* line-line */
#define ISECT_LINE_LINE_COLINEAR	-1
#define ISECT_LINE_LINE_NONE		 0
#define ISECT_LINE_LINE_EXACT		 1
#define ISECT_LINE_LINE_CROSS		 2

int isect_line_line_v2(float a1[2], float a2[2], float b1[2], float b2[2]);
int isect_line_line_v2_short(short a1[2], short a2[2], short b1[2], short b2[2]);

/* Returns the number of point of interests
 * 0 - lines are colinear
 * 1 - lines are coplanar, i1 is set to intersection
 * 2 - i1 and i2 are the nearest points on line 1 (v1, v2) and line 2 (v3, v4) respectively 
 * */

int isect_line_line_v3(float v1[3], float v2[3],
	float v3[3], float v4[3], float i1[3], float i2[3]);
int isect_line_line_strict_v3(float v1[3], float v2[3],
	float v3[3], float v4[3], float vi[3], float *lambda);

/* line/ray triangle */
int isect_line_tri_v3(float p1[3], float p2[3],
	float v0[3], float v1[3], float v2[3], float *lambda, float *uv);
int isect_ray_tri_v3(float p1[3], float d[3],
	float v0[3], float v1[3], float v2[3], float *lambda, float *uv);
int isect_ray_tri_threshold_v3(float p1[3], float d[3],
	float v0[3], float v1[3], float v2[3], float *lambda, float *uv, float threshold);
int isect_ray_tri_epsilon_v3(float p1[3], float d[3],
	float v0[3], float v1[3], float v2[3], float *lambda, float *uv, float epsilon);

/* point in polygon */
int isect_point_tri_v2(float p[2], float a[2], float b[2], float c[2]);
int isect_point_quad_v2(float p[2], float a[2], float b[2], float c[2], float d[2]);

int isect_point_tri_v2(float v1[2], float v2[2], float v3[2], float pt[2]);
int isect_point_tri_v2_int(int x1, int y1, int x2, int y2, int a, int b);
int isect_point_tri_prism_v3(float p[3], float v1[3], float v2[3], float v3[3]);

void isect_point_quad_uv_v2(float v0[2], float v1[2], float v2[2], float v3[2],
	float pt[2], float *uv);
void isect_point_face_uv_v2(int isquad, float v0[2], float v1[2], float v2[2],
	float v3[2], float pt[2], float *uv);

/* other */
int isect_sweeping_sphere_tri_v3(float p1[3], float p2[3], float radius,
	float v0[3], float v1[3], float v2[3], float *lambda, float *ipoint);

int isect_axial_line_tri_v3(int axis, float co1[3], float co2[3],
	float v0[3], float v1[3], float v2[3], float *lambda);

int isect_aabb_aabb_v3(float min1[3], float max1[3], float min2[3], float max2[3]);

/****************************** Interpolation ********************************/

/* tri or quad, d can be NULL */
void interp_weights_face_v3(float w[4],
	float a[3], float b[3], float c[3], float d[3], float p[3]);
void interp_weights_poly_v3(float w[], float v[][3], int n, float p[3]);

void interp_cubic_v3(float x[3], float v[3],
	float x1[3], float v1[3], float x2[3], float v2[3], float t);

int interp_sparse_array(float *array, int list_size, float invalid);

void barycentric_transform(float pt_tar[3], float const pt_src[3],
	const float tri_tar_p1[3], const float tri_tar_p2[3], const float tri_tar_p3[3],
	const float tri_src_p1[3], const float tri_src_p2[3], const float tri_src_p3[3]);

void barycentric_weights_v2(const float v1[2], const float v2[2], const float v3[2],
	const float co[2], float w[3]);

/***************************** View & Projection *****************************/

void lookat_m4(float mat[4][4], float vx, float vy, 
	float vz, float px, float py, float pz, float twist);
void polarview_m4(float mat[4][4], float dist, float azimuth,
	float incidence, float twist);

void perspective_m4(float mat[4][4], float left, float right,
	float bottom, float top, float nearClip, float farClip);
void orthographic_m4(float mat[4][4], float left, float right,
	float bottom, float top, float nearClip, float farClip);

/********************************** Mapping **********************************/

void map_to_tube(float *u, float *v, float x, float y, float z);
void map_to_sphere(float *u, float *v, float x, float y, float z);

/********************************* Tangents **********************************/

typedef struct VertexTangent {
	float tang[3], uv[2];
	struct VertexTangent *next;
} VertexTangent;

float *find_vertex_tangent(VertexTangent *vtang, float *uv);
void sum_or_add_vertex_tangent(void *arena, VertexTangent **vtang,
	float *tang, float *uv);
void tangent_from_uv(float *uv1, float *uv2, float *uv3,
	float *co1, float *co2, float *co3, float *n, float *tang);

/********************************* vector clouds******************************/


void vcloud_estimate_transform(int list_size, float (*pos)[3], float *weight,float (*rpos)[3], float *rweight,
							float lloc[3],float rloc[3],float lrot[3][3],float lscale[3][3]);

#ifdef __cplusplus
}
#endif

#endif /* BLI_MATH_GEOM */

