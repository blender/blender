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

#ifndef __BLI_MATH_GEOM_H__
#define __BLI_MATH_GEOM_H__

/** \file BLI_math_geom.h
 *  \ingroup bli
 */

#ifdef __cplusplus
extern "C" {
#endif

#include "BLI_math_inline.h"

#ifdef __BLI_MATH_INLINE_H__
#include "intern/math_geom_inline.c"
#endif

/********************************** Polygons *********************************/

void cent_tri_v3(float r[3], const float a[3], const float b[3], const float c[3]);
void cent_quad_v3(float r[3], const float a[3], const float b[3], const float c[3], const float d[3]);

float normal_tri_v3(float r[3], const float a[3], const float b[3], const float c[3]);
float normal_quad_v3(float r[3], const float a[3], const float b[3], const float c[3], const float d[3]);

float area_tri_v2(const float a[2], const float b[2], const float c[2]);
float area_tri_signed_v2(const float v1[2], const float v2[2], const float v3[2]);
float area_tri_v3(const float a[3], const float b[3], const float c[3]);
float area_quad_v3(const float a[3], const float b[3], const float c[3], const float d[3]);
float area_poly_v3(int nr, float verts[][3], const float normal[3]);

int is_quad_convex_v3(const float v1[3], const float v2[3], const float v3[3], const float v4[3]);
int is_quad_convex_v2(const float v1[2], const float v2[2], const float v3[2], const float v4[2]);

/********************************* Distance **********************************/

float dist_to_line_v2(const float p[2], const float l1[2], const float l2[2]);
float dist_squared_to_line_segment_v2(const float p[2], const float l1[2], const float l2[2]);
float         dist_to_line_segment_v2(const float p[2], const float l1[2], const float l2[2]);
void closest_to_line_segment_v2(float closest[2], const float p[2], const float l1[2], const float l2[2]);

float dist_to_plane_normalized_v3(const float p[3], const float plane_co[3], const float plane_no_unit[3]);
float dist_to_plane_v3(const float p[3], const float plane_co[3], const float plane_no[3]);
float dist_to_line_segment_v3(const float p[3], const float l1[3], const float l2[3]);
float closest_to_line_v3(float r[3], const float p[3], const float l1[3], const float l2[3]);
float closest_to_line_v2(float r[2], const float p[2], const float l1[2], const float l2[2]);
void closest_to_line_segment_v3(float r[3], const float p[3], const float l1[3], const float l2[3]);
void closest_to_plane_v3(float r[3], const float plane_co[3], const float plane_no_unit[3], const float pt[3]);


float line_point_factor_v3(const float p[3], const float l1[3], const float l2[3]);
float line_point_factor_v2(const float p[2], const float l1[2], const float l2[2]);
void limit_dist_v3(float v1[3], float v2[3], const float dist);

/******************************* Intersection ********************************/

/* TODO int return value consistency */

/* line-line */
#define ISECT_LINE_LINE_COLINEAR    -1
#define ISECT_LINE_LINE_NONE         0
#define ISECT_LINE_LINE_EXACT        1
#define ISECT_LINE_LINE_CROSS        2

int isect_line_line_v2_point(const float v1[2], const float v2[2], const float v3[2], const float v4[2], float vi[2]);
int isect_line_line_v2(const float a1[2], const float a2[2], const float b1[2], const float b2[2]);
int isect_line_line_v2_int(const int a1[2], const int a2[2], const int b1[2], const int b2[2]);
int isect_line_sphere_v3(const float l1[3], const float l2[3], const float sp[3], const float r, float r_p1[3], float r_p2[3]);
int isect_line_sphere_v2(const float l1[2], const float l2[2], const float sp[2], const float r, float r_p1[2], float r_p2[2]);
int isect_seg_seg_v2_point(const float v1[2], const float v2[2], const float v3[2], const float v4[2], float vi[2]);
int isect_seg_seg_v2(const float v1[2], const float v2[2], const float v3[2], const float v4[2]);

/* Returns the number of point of interests
 * 0 - lines are colinear
 * 1 - lines are coplanar, i1 is set to intersection
 * 2 - i1 and i2 are the nearest points on line 1 (v1, v2) and line 2 (v3, v4) respectively 
 * */

int isect_line_line_v3(const float v1[3], const float v2[3],
                       const float v3[3], const float v4[3],
                       float i1[3], float i2[3]);
int isect_line_line_strict_v3(const float v1[3], const float v2[3],
                              const float v3[3], const float v4[3],
                              float vi[3], float *r_lambda);

/* if clip is nonzero, will only return true if lambda is >= 0.0
 * (i.e. intersection point is along positive d)*/
int isect_ray_plane_v3(const float p1[3], const float d[3],
                       const float v0[3], const float v1[3], const float v2[3],
                       float *r_lambda, const int clip);

/**
 * Intersect line/plane, optionally treat line as directional (like a ray) with the no_flip argument.
 * \param out The intersection point.
 * \param l1 The first point of the line.
 * \param l2 The second point of the line.
 * \param plane_co A point on the plane to intersect with.
 * \param plane_no The direction of the plane (does not need to be normalized).
 * \param no_flip When true, the intersection point will always be from l1 to l2, even if this is not on the plane.
 */
int isect_line_plane_v3(float out[3], const float l1[3], const float l2[3],
                        const float plane_co[3], const float plane_no[3], const short no_flip);

/**
 * Intersect two planes, return a point on the intersection and a vector
 * that runs on the direction of the intersection.
 * Return error code is the same as 'isect_line_line_v3'.
 * \param r_isect_co The resulting intersection point.
 * \param r_isect_no The resulting vector of the intersection.
 * \param plane_a_co The point on the first plane.
 * \param plane_a_no The normal of the first plane.
 * \param plane_b_co The point on the second plane.
 * \param plane_b_no The normal of the second plane.
 */
void isect_plane_plane_v3(float r_isect_co[3], float r_isect_no[3],
                          const float plane_a_co[3], const float plane_a_no[3],
                          const float plane_b_co[3], const float plane_b_no[3]);

/* line/ray triangle */
int isect_line_tri_v3(const float p1[3], const float p2[3],
                      const float v0[3], const float v1[3], const float v2[3], float *r_lambda, float r_uv[2]);
int isect_ray_tri_v3(const float p1[3], const float d[3],
                     const float v0[3], const float v1[3], const float v2[3], float *r_lambda, float r_uv[2]);
int isect_ray_tri_threshold_v3(const float p1[3], const float d[3],
                               const float v0[3], const float v1[3], const float v2[3], float *r_lambda, float r_uv[2], const float threshold);
int isect_ray_tri_epsilon_v3(const float p1[3], const float d[3],
                             const float v0[3], const float v1[3], const float v2[3], float *r_lambda, float r_uv[2], const float epsilon);

/* point in polygon */
int isect_point_quad_v2(const float p[2], const float a[2], const float b[2], const float c[2], const float d[2]);

int isect_point_tri_v2(const float v1[2], const float v2[2], const float v3[2], const float pt[2]);
int isect_point_tri_v2_cw(const float pt[2], const float v1[2], const float v2[2], const float v3[2]);
int isect_point_tri_v2_int(const int x1, const int y1, const int x2, const int y2, const int a, const int b);
int isect_point_tri_prism_v3(const float p[3], const float v1[3], const float v2[3], const float v3[3]);

void isect_point_quad_uv_v2(const float v0[2], const float v1[2], const float v2[2], const float v3[2],
                            const float pt[2], float r_uv[2]);
void isect_point_face_uv_v2(const int isquad, const float v0[2], const float v1[2], const float v2[2],
                            const float v3[2], const float pt[2], float r_uv[2]);

/* axis-aligned bounding box */
int isect_aabb_aabb_v3(const float min1[3], const float max1[3], const float min2[3], const float max2[3]);

typedef struct {
	float ray_start[3];
	float ray_inv_dir[3];
	int sign[3];
} IsectRayAABBData;

void isect_ray_aabb_initialize(IsectRayAABBData *data, const float ray_start[3], const float ray_direction[3]);
int isect_ray_aabb(const IsectRayAABBData *data, const float bb_min[3], const float bb_max[3], float *tmin);

/* other */
int isect_sweeping_sphere_tri_v3(const float p1[3], const float p2[3], const float radius,
                                 const float v0[3], const float v1[3], const float v2[3], float *r_lambda, float ipoint[3]);

int isect_axial_line_tri_v3(const int axis, const float co1[3], const float co2[3],
                            const float v0[3], const float v1[3], const float v2[3], float *r_lambda);

int clip_line_plane(float p1[3], float p2[3], const float plane[4]);

void plot_line_v2v2i(const int p1[2], const int p2[2], int (*callback)(int, int, void *), void *userData);

/****************************** Interpolation ********************************/

/* tri or quad, d can be NULL */
void interp_weights_face_v3(float w[4],
                            const float a[3], const float b[3], const float c[3], const float d[3], const float p[3]);
void interp_weights_poly_v3(float w[], float v[][3], const int n, const float co[3]);
void interp_weights_poly_v2(float w[], float v[][2], const int n, const float co[2]);

void interp_cubic_v3(float x[3], float v[3],
                     const float x1[3], const float v1[3], const float x2[3], const float v2[3], const float t);

int interp_sparse_array(float *array, const int list_size, const float invalid);

void barycentric_transform(float pt_tar[3], float const pt_src[3],
                           const float tri_tar_p1[3], const float tri_tar_p2[3], const float tri_tar_p3[3],
                           const float tri_src_p1[3], const float tri_src_p2[3], const float tri_src_p3[3]);

void barycentric_weights_v2(const float v1[2], const float v2[2], const float v3[2],
                            const float co[2], float w[3]);
void barycentric_weights_v2_quad(const float v1[2], const float v2[2], const float v3[2], const float v4[2],
                                 const float co[2], float w[4]);

int barycentric_coords_v2(const float v1[2], const float v2[2], const float v3[2], const float co[2], float w[3]);
int barycentric_inside_triangle_v2(const float w[3]);

void resolve_tri_uv(float r_uv[2], const float st[2], const float st0[2], const float st1[2], const float st2[2]);
void resolve_quad_uv(float uv[2], const float st[2], const float st0[2], const float st1[2], const float st2[2], const float st3[2]);

/***************************** View & Projection *****************************/

void lookat_m4(float mat[4][4], float vx, float vy, 
               float vz, float px, float py, float pz, float twist);
void polarview_m4(float mat[4][4], float dist, float azimuth,
                  float incidence, float twist);

void perspective_m4(float mat[4][4], const float left, const float right,
                    const float bottom, const float top, const float nearClip, const float farClip);
void orthographic_m4(float mat[4][4], const float left, const float right,
                     const float bottom, const float top, const float nearClip, const float farClip);
void window_translate_m4(float winmat[][4], float perspmat[][4],
                         const float x, const float y);

int box_clip_bounds_m4(float boundbox[2][3],
                       const float bounds[4], float winmat[4][4]);
void box_minmax_bounds_m4(float min[3], float max[3],
                          float boundbox[2][3], float mat[4][4]);

/********************************** Mapping **********************************/

void map_to_tube(float *r_u, float *r_v, const float x, const float y, const float z);
void map_to_sphere(float *r_u, float *r_v, const float x, const float y, const float z);

/********************************** Normals **********************************/

void accumulate_vertex_normals(float n1[3], float n2[3], float n3[3],
                               float n4[3], const float f_no[3], const float co1[3], const float co2[3],
                               const float co3[3], const float co4[3]);

void accumulate_vertex_normals_poly(float **vertnos, float polyno[3],
                                    float **vertcos, float vdiffs[][3], int nverts);

/********************************* Tangents **********************************/

typedef struct VertexTangent {
	struct VertexTangent *next;
	float tang[3], uv[2];
} VertexTangent;

float *find_vertex_tangent(VertexTangent *vtang, const float uv[2]);
void sum_or_add_vertex_tangent(void *arena, VertexTangent **vtang,
                               const float tang[3], const float uv[2]);
void tangent_from_uv(float uv1[2], float uv2[2], float uv3[2],
                     float co1[3], float co2[3], float co3[3], float n[3], float tang[3]);

/******************************** Vector Clouds ******************************/

void vcloud_estimate_transform(int list_size, float (*pos)[3], float *weight,
                               float (*rpos)[3], float *rweight,
                               float lloc[3], float rloc[3], float lrot[3][3], float lscale[3][3]);

/****************************** Spherical Harmonics *************************/

/* Uses 2nd order SH => 9 coefficients, stored in this order:
 * 0 = (0, 0),
 * 1 = (1, -1), 2 = (1, 0), 3 = (1, 1),
 * 4 = (2, -2), 5 = (2, -1), 6 = (2, 0), 7 = (2, 1), 8 = (2, 2) */

MINLINE void zero_sh(float r[9]);
MINLINE void copy_sh_sh(float r[9], const float a[9]);
MINLINE void mul_sh_fl(float r[9], const float f);
MINLINE void add_sh_shsh(float r[9], const float a[9], const float b[9]);

MINLINE float eval_shv3(float r[9], const float v[3]);
MINLINE float diffuse_shv3(float r[9], const float v[3]);
MINLINE void vec_fac_to_sh(float r[9], const float v[3], const float f);
MINLINE void madd_sh_shfl(float r[9], const float sh[3], const float f);

/********************************* Form Factor *******************************/

float form_factor_hemi_poly(float p[3], float n[3],
                            float v1[3], float v2[3], float v3[3], float v4[3]);

void axis_dominant_v3(int *axis_a, int *axis_b, const float axis[3]);

#ifdef __cplusplus
}
#endif

#endif /* __BLI_MATH_GEOM_H__ */

