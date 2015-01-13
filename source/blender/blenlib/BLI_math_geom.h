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

#include "BLI_compiler_attrs.h"
#include "BLI_math_inline.h"

#ifdef BLI_MATH_GCC_WARN_PRAGMA
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wredundant-decls"
#endif

/********************************** Polygons *********************************/

void cent_tri_v3(float r[3], const float a[3], const float b[3], const float c[3]);
void cent_quad_v3(float r[3], const float a[3], const float b[3], const float c[3], const float d[3]);

float normal_tri_v3(float r[3], const float a[3], const float b[3], const float c[3]);
float normal_quad_v3(float r[3], const float a[3], const float b[3], const float c[3], const float d[3]);
float normal_poly_v3(float r[3], const float verts[][3], unsigned int nr);

MINLINE float area_tri_v2(const float a[2], const float b[2], const float c[2]);
MINLINE float area_squared_tri_v2(const float a[2], const float b[2], const float c[2]);
MINLINE float area_tri_signed_v2(const float v1[2], const float v2[2], const float v3[2]);
float area_tri_v3(const float a[3], const float b[3], const float c[3]);
float area_squared_tri_v3(const float a[3], const float b[3], const float c[3]);
float area_tri_signed_v3(const float v1[3], const float v2[3], const float v3[3], const float normal[3]);
float area_quad_v3(const float a[3], const float b[3], const float c[3], const float d[3]);
float area_squared_quad_v3(const float a[3], const float b[3], const float c[3], const float d[3]);
float area_poly_v3(const float verts[][3], unsigned int nr);
float area_poly_v2(const float verts[][2], unsigned int nr);
float area_squared_poly_v3(const float verts[][3], unsigned int nr);
float area_squared_poly_v2(const float verts[][2], unsigned int nr);
float area_poly_signed_v2(const float verts[][2], unsigned int nr);
float cotangent_tri_weight_v3(const float v1[3], const float v2[3], const float v3[3]);

void          cross_tri_v3(float n[3], const float v1[3], const float v2[3], const float v3[3]);
MINLINE float cross_tri_v2(const float v1[2], const float v2[2], const float v3[2]);
void cross_poly_v3(float n[3], const float verts[][3], unsigned int nr);
float cross_poly_v2(const float verts[][2], unsigned int nr);

/********************************* Planes **********************************/

void  plane_from_point_normal_v3(float r_plane[4], const float plane_co[3], const float plane_no[3]);
void  plane_to_point_normal_v3(const float plane[4], float r_plane_co[3], float r_plane_no[3]);
MINLINE float plane_point_side_v3(const float plane[4], const float co[3]);

/********************************* Volume **********************************/

float volume_tetrahedron_v3(const float v1[3], const float v2[3], const float v3[3], const float v4[3]);
float volume_tetrahedron_signed_v3(const float v1[3], const float v2[3], const float v3[3], const float v4[3]);

bool is_quad_convex_v3(const float v1[3], const float v2[3], const float v3[3], const float v4[3]);
bool is_quad_convex_v2(const float v1[2], const float v2[2], const float v3[2], const float v4[2]);
bool is_poly_convex_v2(const float verts[][2], unsigned int nr);
int  is_quad_flip_v3(const float v1[3], const float v2[3], const float v3[3], const float v4[3]);

/********************************* Distance **********************************/

float dist_squared_to_line_v2(const float p[2], const float l1[2], const float l2[2]);
float         dist_to_line_v2(const float p[2], const float l1[2], const float l2[2]);
float dist_squared_to_line_segment_v2(const float p[2], const float l1[2], const float l2[2]);
float         dist_to_line_segment_v2(const float p[2], const float l1[2], const float l2[2]);
void closest_to_line_segment_v2(float r_close[2], const float p[2], const float l1[2], const float l2[2]);

float dist_signed_squared_to_plane_v3(const float p[3], const float plane[4]);
float        dist_squared_to_plane_v3(const float p[3], const float plane[4]);
float dist_signed_to_plane_v3(const float p[3], const float plane[4]);
float        dist_to_plane_v3(const float p[3], const float plane[4]);

float dist_squared_to_line_segment_v3(const float p[3], const float l1[3], const float l2[3]);
float         dist_to_line_segment_v3(const float p[3], const float l1[3], const float l2[3]);
float dist_squared_to_line_v3(const float p[3], const float l1[3], const float l2[3]);
float         dist_to_line_v3(const float p[3], const float l1[3], const float l2[3]);
float closest_to_line_v3(float r[3], const float p[3], const float l1[3], const float l2[3]);
float closest_to_line_v2(float r[2], const float p[2], const float l1[2], const float l2[2]);
void closest_to_line_segment_v3(float r_close[3], const float p[3], const float l1[3], const float l2[3]);
void closest_to_plane_v3(float r_close[3], const float plane[4], const float pt[3]);

/* Set 'r' to the point in triangle (t1, t2, t3) closest to point 'p' */
void closest_on_tri_to_point_v3(float r[3], const float p[3], const float t1[3], const float t2[3], const float t3[3]);


float line_point_factor_v3(const float p[3], const float l1[3], const float l2[3]);
float line_point_factor_v2(const float p[2], const float l1[2], const float l2[2]);

float line_plane_factor_v3(const float plane_co[3], const float plane_no[3],
                           const float l1[3], const float l2[3]);

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
bool isect_seg_seg_v2(const float v1[2], const float v2[2], const float v3[2], const float v4[2]);

int isect_line_line_epsilon_v3(
        const float v1[3], const float v2[3],
        const float v3[3], const float v4[3], float i1[3], float i2[3],
        const float epsilon);
int isect_line_line_v3(
        const float v1[3], const float v2[3],
        const float v3[3], const float v4[3],
        float i1[3], float i2[3]);
bool isect_line_line_strict_v3(const float v1[3], const float v2[3],
                               const float v3[3], const float v4[3],
                               float vi[3], float *r_lambda);

bool isect_ray_plane_v3(const float p1[3], const float d[3],
                        const float v0[3], const float v1[3], const float v2[3],
                        float *r_lambda, const int clip);

bool isect_point_planes_v3(float (*planes)[4], int totplane, const float p[3]);
bool isect_line_plane_v3(float out[3], const float l1[3], const float l2[3],
                         const float plane_co[3], const float plane_no[3]) ATTR_WARN_UNUSED_RESULT;

bool isect_plane_plane_v3(float r_isect_co[3], float r_isect_no[3],
                          const float plane_a_co[3], const float plane_a_no[3],
                          const float plane_b_co[3], const float plane_b_no[3]) ATTR_WARN_UNUSED_RESULT;

/* line/ray triangle */
bool isect_line_tri_v3(const float p1[3], const float p2[3],
                       const float v0[3], const float v1[3], const float v2[3], float *r_lambda, float r_uv[2]);
bool isect_line_tri_epsilon_v3(const float p1[3], const float p2[3],
                       const float v0[3], const float v1[3], const float v2[3],
                       float *r_lambda, float r_uv[2], const float epsilon);
bool isect_ray_tri_v3(const float p1[3], const float d[3],
                      const float v0[3], const float v1[3], const float v2[3], float *r_lambda, float r_uv[2]);
bool isect_ray_tri_threshold_v3(const float p1[3], const float d[3],
                                const float v0[3], const float v1[3], const float v2[3], float *r_lambda, float r_uv[2], const float threshold);
bool isect_ray_tri_epsilon_v3(const float p1[3], const float d[3],
                              const float v0[3], const float v1[3], const float v2[3], float *r_lambda, float r_uv[2], const float epsilon);

/* point in polygon */
bool isect_point_poly_v2(const float pt[2], const float verts[][2], const unsigned int nr, const bool use_holes);
bool isect_point_poly_v2_int(const int pt[2], const int verts[][2], const unsigned int nr, const bool use_holes);

int isect_point_quad_v2(const float p[2], const float a[2], const float b[2], const float c[2], const float d[2]);

int  isect_point_tri_v2(const float pt[2], const float v1[2], const float v2[2], const float v3[2]);
bool isect_point_tri_v2_cw(const float pt[2], const float v1[2], const float v2[2], const float v3[2]);
int  isect_point_tri_v2_int(const int x1, const int y1, const int x2, const int y2, const int a, const int b);
bool isect_point_tri_prism_v3(const float p[3], const float v1[3], const float v2[3], const float v3[3]);
bool isect_point_tri_v3(const float p[3], const float v1[3], const float v2[3], const float v3[3],
                        float r_vi[3]);

/* axis-aligned bounding box */
bool isect_aabb_aabb_v3(const float min1[3], const float max1[3], const float min2[3], const float max2[3]);

typedef struct {
	float ray_start[3];
	float ray_inv_dir[3];
	int sign[3];
} IsectRayAABBData;

void isect_ray_aabb_initialize(IsectRayAABBData *data, const float ray_start[3], const float ray_direction[3]);
bool isect_ray_aabb(const IsectRayAABBData *data, const float bb_min[3], const float bb_max[3], float *tmin);

/* other */
bool isect_sweeping_sphere_tri_v3(const float p1[3], const float p2[3], const float radius,
                                  const float v0[3], const float v1[3], const float v2[3], float *r_lambda, float ipoint[3]);

bool isect_axial_line_tri_v3(const int axis, const float co1[3], const float co2[3],
                             const float v0[3], const float v1[3], const float v2[3], float *r_lambda);

bool clip_segment_v3_plane(float p1[3], float p2[3], const float plane[4]);
bool clip_segment_v3_plane_n(float p1[3], float p2[3], float plane_array[][4], const int plane_tot);

void plot_line_v2v2i(const int p1[2], const int p2[2], bool (*callback)(int, int, void *), void *userData);
void fill_poly_v2i_n(
        const int xmin, const int ymin, const int xmax, const int ymax,
        const int polyXY[][2], const int polyCorners,
        void (*callback)(int, int, void *), void *userData);
/****************************** Interpolation ********************************/

/* tri or quad, d can be NULL */
void interp_weights_face_v3(float w[4],
                            const float a[3], const float b[3], const float c[3], const float d[3], const float p[3]);
void interp_weights_poly_v3(float w[], float v[][3], const int n, const float co[3]);
void interp_weights_poly_v2(float w[], float v[][2], const int n, const float co[2]);

void interp_cubic_v3(float x[3], float v[3],
                     const float x1[3], const float v1[3], const float x2[3], const float v2[3], const float t);

int interp_sparse_array(float *array, const int list_size, const float invalid);

void transform_point_by_tri_v3(
        float pt_tar[3], float const pt_src[3],
        const float tri_tar_p1[3], const float tri_tar_p2[3], const float tri_tar_p3[3],
        const float tri_src_p1[3], const float tri_src_p2[3], const float tri_src_p3[3]);
void transform_point_by_seg_v3(
        float p_dst[3], const float p_src[3],
        const float l_dst_p1[3], const float l_dst_p2[3],
        const float l_src_p1[3], const float l_src_p2[3]);

void barycentric_weights_v2(const float v1[2], const float v2[2], const float v3[2],
                            const float co[2], float w[3]);
void barycentric_weights_v2_persp(const float v1[4], const float v2[4], const float v3[4],
                                  const float co[2], float w[3]);
void barycentric_weights_v2_quad(const float v1[2], const float v2[2], const float v3[2], const float v4[2],
                                 const float co[2], float w[4]);

bool barycentric_coords_v2(const float v1[2], const float v2[2], const float v3[2], const float co[2], float w[3]);
int barycentric_inside_triangle_v2(const float w[3]);

void resolve_tri_uv_v2(float r_uv[2], const float st[2], const float st0[2], const float st1[2], const float st2[2]);
void resolve_tri_uv_v3(float r_uv[2], const float st[3], const float st0[3], const float st1[3], const float st2[3]);
void resolve_quad_uv_v2(float r_uv[2], const float st[2], const float st0[2], const float st1[2], const float st2[2], const float st3[2]);
void resolve_quad_uv_v2_deriv(float r_uv[2], float r_deriv[2][2],
                              const float st[2], const float st0[2], const float st1[2], const float st2[2], const float st3[2]);

/* use to find the point of a UV on a face */
void interp_bilinear_quad_v3(float data[4][3], float u, float v, float res[3]);
void interp_barycentric_tri_v3(float data[3][3], float u, float v, float res[3]);

/***************************** View & Projection *****************************/

void lookat_m4(float mat[4][4], float vx, float vy, 
               float vz, float px, float py, float pz, float twist);
void polarview_m4(float mat[4][4], float dist, float azimuth,
                  float incidence, float twist);

void perspective_m4(float mat[4][4], const float left, const float right,
                    const float bottom, const float top, const float nearClip, const float farClip);
void orthographic_m4(float mat[4][4], const float left, const float right,
                     const float bottom, const float top, const float nearClip, const float farClip);
void window_translate_m4(float winmat[4][4], float perspmat[4][4],
                         const float x, const float y);

void planes_from_projmat(float mat[4][4], float left[4], float right[4], float top[4], float bottom[4],
                         float front[4], float back[4]);

int box_clip_bounds_m4(float boundbox[2][3],
                       const float bounds[4], float winmat[4][4]);
void box_minmax_bounds_m4(float min[3], float max[3],
                          float boundbox[2][3], float mat[4][4]);

/********************************** Mapping **********************************/

void map_to_tube(float *r_u, float *r_v, const float x, const float y, const float z);
void map_to_sphere(float *r_u, float *r_v, const float x, const float y, const float z);

/********************************** Normals **********************************/

void accumulate_vertex_normals(
        float n1[3], float n2[3], float n3[3], float n4[3],
        const float f_no[3],
        const float co1[3], const float co2[3], const float co3[3], const float co4[3]);

void accumulate_vertex_normals_poly(
        float **vertnos, const float polyno[3],
        const float **vertcos, float vdiffs[][3], const int nverts);

/********************************* Tangents **********************************/

void tangent_from_uv(
        const float uv1[2], const float uv2[2], const float uv3[2],
        const float co1[3], const float co2[3], const float co3[3],
        const float n[3],
        float r_tang[3]);

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
MINLINE float dot_shsh(const float a[9], const float b[9]);

MINLINE float eval_shv3(float r[9], const float v[3]);
MINLINE float diffuse_shv3(float r[9], const float v[3]);
MINLINE void vec_fac_to_sh(float r[9], const float v[3], const float f);
MINLINE void madd_sh_shfl(float r[9], const float sh[3], const float f);

/********************************* Form Factor *******************************/

float form_factor_quad(const float p[3], const float n[3],
                       const float q0[3], const float q1[3], const float q2[3], const float q3[3]);
bool form_factor_visible_quad(const float p[3], const float n[3],
                              const float v0[3], const float v1[3], const float v2[3],
                              float q0[3], float q1[3], float q2[3], float q3[3]);
float form_factor_hemi_poly(float p[3], float n[3],
                            float v1[3], float v2[3], float v3[3], float v4[3]);

void  axis_dominant_v3_to_m3(float r_mat[3][3], const float normal[3]);

MINLINE void  axis_dominant_v3(int *r_axis_a, int *r_axis_b, const float axis[3]);
MINLINE float axis_dominant_v3_max(int *r_axis_a, int *r_axis_b, const float axis[3]) ATTR_WARN_UNUSED_RESULT;
MINLINE int axis_dominant_v3_single(const float vec[3]);

MINLINE int max_axis_v3(const float vec[3]);
MINLINE int min_axis_v3(const float vec[3]);

MINLINE int poly_to_tri_count(const int poly_count, const int corner_count);

MINLINE float shell_angle_to_dist(const float angle);
MINLINE float shell_v3v3_normalized_to_dist(const float a[3], const float b[3]);
MINLINE float shell_v2v2_normalized_to_dist(const float a[2], const float b[2]);
MINLINE float shell_v3v3_mid_normalized_to_dist(const float a[3], const float b[3]);
MINLINE float shell_v2v2_mid_normalized_to_dist(const float a[2], const float b[2]);

/**************************** Inline Definitions ******************************/

#if BLI_MATH_DO_INLINE
#include "intern/math_geom_inline.c"
#endif

#ifdef BLI_MATH_GCC_WARN_PRAGMA
#  pragma GCC diagnostic pop
#endif

#ifdef __cplusplus
}
#endif

#endif /* __BLI_MATH_GEOM_H__ */

