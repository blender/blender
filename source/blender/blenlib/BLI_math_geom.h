/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2001-2002 NaN Holding BV. All rights reserved. */

#pragma once

/** \file
 * \ingroup bli
 */

#include "BLI_compiler_attrs.h"
#include "BLI_math_inline.h"

#ifdef BLI_MATH_GCC_WARN_PRAGMA
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wredundant-decls"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------- */
/** \name Polygons
 * \{ */

float normal_tri_v3(float n[3], const float v1[3], const float v2[3], const float v3[3]);
float normal_quad_v3(
    float n[3], const float v1[3], const float v2[3], const float v3[3], const float v4[3]);
/**
 * Computes the normal of a planar polygon See Graphics Gems for computing newell normal.
 */
float normal_poly_v3(float n[3], const float verts[][3], unsigned int nr);

MINLINE float area_tri_v2(const float v1[2], const float v2[2], const float v3[2]);
MINLINE float area_squared_tri_v2(const float v1[2], const float v2[2], const float v3[2]);
MINLINE float area_tri_signed_v2(const float v1[2], const float v2[2], const float v3[2]);

/* Triangles */

float area_tri_v3(const float v1[3], const float v2[3], const float v3[3]);
float area_squared_tri_v3(const float v1[3], const float v2[3], const float v3[3]);
float area_tri_signed_v3(const float v1[3],
                         const float v2[3],
                         const float v3[3],
                         const float normal[3]);
float area_quad_v3(const float v1[3], const float v2[3], const float v3[3], const float v4[3]);
float area_squared_quad_v3(const float v1[3],
                           const float v2[3],
                           const float v3[3],
                           const float v4[3]);
float area_poly_v3(const float verts[][3], unsigned int nr);
float area_poly_v2(const float verts[][2], unsigned int nr);
float area_squared_poly_v3(const float verts[][3], unsigned int nr);
float area_squared_poly_v2(const float verts[][2], unsigned int nr);
float area_poly_signed_v2(const float verts[][2], unsigned int nr);
float cotangent_tri_weight_v3(const float v1[3], const float v2[3], const float v3[3]);

void cross_tri_v3(float n[3], const float v1[3], const float v2[3], const float v3[3]);
MINLINE float cross_tri_v2(const float v1[2], const float v2[2], const float v3[2]);
void cross_poly_v3(float n[3], const float verts[][3], unsigned int nr);
/**
 * Scalar cross product of a 2d polygon.
 *
 * - equivalent to `area * 2`
 * - useful for checking polygon winding (a positive value is clockwise).
 */
float cross_poly_v2(const float verts[][2], unsigned int nr);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Planes
 * \{ */

/**
 * Calculate a plane from a point and a direction,
 * \note \a point_no isn't required to be normalized.
 */
void plane_from_point_normal_v3(float r_plane[4],
                                const float plane_co[3],
                                const float plane_no[3]);
/**
 * Get a point and a direction from a plane.
 */
void plane_to_point_vector_v3(const float plane[4], float r_plane_co[3], float r_plane_no[3]);
/**
 * Version of #plane_to_point_vector_v3 that gets a unit length vector.
 */
void plane_to_point_vector_v3_normalized(const float plane[4],
                                         float r_plane_co[3],
                                         float r_plane_no[3]);

MINLINE float plane_point_side_v3(const float plane[4], const float co[3]);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Volume
 * \{ */

/**
 * The volume from a tetrahedron, points can be in any order
 */
float volume_tetrahedron_v3(const float v1[3],
                            const float v2[3],
                            const float v3[3],
                            const float v4[3]);
/**
 * The volume from a tetrahedron, normal pointing inside gives negative volume
 */
float volume_tetrahedron_signed_v3(const float v1[3],
                                   const float v2[3],
                                   const float v3[3],
                                   const float v4[3]);

/**
 * The volume from a triangle that is made into a tetrahedron.
 * This uses a simplified formula where the tip of the tetrahedron is in the world origin.
 * Using this method, the total volume of a closed triangle mesh can be calculated.
 * Note that you need to divide the result by 6 to get the actual volume.
 */
float volume_tri_tetrahedron_signed_v3_6x(const float v1[3], const float v2[3], const float v3[3]);
float volume_tri_tetrahedron_signed_v3(const float v1[3], const float v2[3], const float v3[3]);

/**
 * Check if the edge is convex or concave
 * (depends on face winding)
 * Copied from BM_edge_is_convex().
 */
bool is_edge_convex_v3(const float v1[3], const float v2[3], const float v3[3], const float v4[3]);
/**
 * Evaluate if entire quad is a proper convex quad
 */
bool is_quad_convex_v3(const float v1[3], const float v2[3], const float v3[3], const float v4[3]);
bool is_quad_convex_v2(const float v1[2], const float v2[2], const float v3[2], const float v4[2]);
bool is_poly_convex_v2(const float verts[][2], unsigned int nr);
/**
 * Check if either of the diagonals along this quad create flipped triangles
 * (normals pointing away from each other).
 * - (1 << 0): (v1-v3) is flipped.
 * - (1 << 1): (v2-v4) is flipped.
 */
int is_quad_flip_v3(const float v1[3], const float v2[3], const float v3[3], const float v4[3]);
bool is_quad_flip_v3_first_third_fast(const float v1[3],
                                      const float v2[3],
                                      const float v3[3],
                                      const float v4[3]);
bool is_quad_flip_v3_first_third_fast_with_normal(const float v1[3],
                                                  const float v2[3],
                                                  const float v3[3],
                                                  const float v4[3],
                                                  const float normal[3]);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Distance
 * \{ */

/**
 * Distance p to line v1-v2 using Hesse formula (NO LINE PIECE!)
 */
float dist_squared_to_line_v2(const float p[2], const float l1[2], const float l2[2]);
float dist_to_line_v2(const float p[2], const float l1[2], const float l2[2]);
/**
 * Distance p to line-piece v1-v2.
 */
float dist_squared_to_line_segment_v2(const float p[2], const float l1[2], const float l2[2]);
float dist_to_line_segment_v2(const float p[2], const float l1[2], const float l2[2]);

float dist_signed_squared_to_plane_v3(const float p[3], const float plane[4]);
float dist_squared_to_plane_v3(const float p[3], const float plane[4]);
/**
 * Return the signed distance from the point to the plane.
 */
float dist_signed_to_plane_v3(const float p[3], const float plane[4]);
float dist_to_plane_v3(const float p[3], const float plane[4]);

/* Plane3 versions. */

float dist_signed_squared_to_plane3_v3(const float p[3], const float plane[3]);
float dist_squared_to_plane3_v3(const float p[3], const float plane[3]);
float dist_signed_to_plane3_v3(const float p[3], const float plane[3]);
float dist_to_plane3_v3(const float p[3], const float plane[3]);

/**
 * Distance v1 to line-piece l1-l2 in 3D.
 */
float dist_squared_to_line_segment_v3(const float p[3], const float l1[3], const float l2[3]);
float dist_to_line_segment_v3(const float p[3], const float l1[3], const float l2[3]);
float dist_squared_to_line_v3(const float p[3], const float l1[3], const float l2[3]);
float dist_to_line_v3(const float p[3], const float l1[3], const float l2[3]);
/**
 * Check if \a p is inside the 2x planes defined by `(v1, v2, v3)`
 * where the 3x points define 2x planes.
 *
 * \param axis_ref: used when v1,v2,v3 form a line and to check if the corner is concave/convex.
 *
 * \note the distance from \a v1 & \a v3 to \a v2 doesn't matter
 * (it just defines the planes).
 *
 * \return the lowest squared distance to either of the planes.
 * where `(return < 0.0)` is outside.
 *
 * \code{.unparsed}
 *            v1
 *            +
 *           /
 * x - out  /  x - inside
 *         /
 *        +----+
 *        v2   v3
 *           x - also outside
 * \endcode
 */
float dist_signed_squared_to_corner_v3v3v3(const float p[3],
                                           const float v1[3],
                                           const float v2[3],
                                           const float v3[3],
                                           const float axis_ref[3]);
/**
 * Compute the squared distance of a point to a line (defined as ray).
 * \param ray_origin: A point on the line.
 * \param ray_direction: Normalized direction of the line.
 * \param co: Point to which the distance is to be calculated.
 */
float dist_squared_to_ray_v3_normalized(const float ray_origin[3],
                                        const float ray_direction[3],
                                        const float co[3]);
/**
 * Find the closest point in a seg to a ray and return the distance squared.
 * \param r_point: Is the point on segment closest to ray
 * (or to ray_origin if the ray and the segment are parallel).
 * \param r_depth: the distance of r_point projection on ray to the ray_origin.
 */
float dist_squared_ray_to_seg_v3(const float ray_origin[3],
                                 const float ray_direction[3],
                                 const float v0[3],
                                 const float v1[3],
                                 float r_point[3],
                                 float *r_depth);

/**
 * Returns the coordinates of the nearest vertex and the farthest vertex from a plane (or normal).
 */
void aabb_get_near_far_from_plane(const float plane_no[3],
                                  const float bbmin[3],
                                  const float bbmax[3],
                                  float bb_near[3],
                                  float bb_afar[3]);

struct DistRayAABB_Precalc {
  float ray_origin[3];
  float ray_direction[3];
  float ray_inv_dir[3];
};
void dist_squared_ray_to_aabb_v3_precalc(struct DistRayAABB_Precalc *neasrest_precalc,
                                         const float ray_origin[3],
                                         const float ray_direction[3]);
/**
 * Returns the distance from a ray to a bound-box (projected on ray)
 */
float dist_squared_ray_to_aabb_v3(const struct DistRayAABB_Precalc *data,
                                  const float bb_min[3],
                                  const float bb_max[3],
                                  float r_point[3],
                                  float *r_depth);
/**
 * Use when there is no advantage to pre-calculation.
 */
float dist_squared_ray_to_aabb_v3_simple(const float ray_origin[3],
                                         const float ray_direction[3],
                                         const float bb_min[3],
                                         const float bb_max[3],
                                         float r_point[3],
                                         float *r_depth);

struct DistProjectedAABBPrecalc {
  float ray_origin[3];
  float ray_direction[3];
  float ray_inv_dir[3];
  float pmat[4][4];
  float mval[2];
};
/**
 * \param projmat: Projection Matrix (usually perspective
 * matrix multiplied by object matrix).
 */
void dist_squared_to_projected_aabb_precalc(struct DistProjectedAABBPrecalc *precalc,
                                            const float projmat[4][4],
                                            const float winsize[2],
                                            const float mval[2]);
/**
 * Returns the distance from a 2D coordinate to a bound-box (projected).
 */
float dist_squared_to_projected_aabb(struct DistProjectedAABBPrecalc *data,
                                     const float bbmin[3],
                                     const float bbmax[3],
                                     bool r_axis_closest[3]);
float dist_squared_to_projected_aabb_simple(const float projmat[4][4],
                                            const float winsize[2],
                                            const float mval[2],
                                            const float bbmin[3],
                                            const float bbmax[3]);

/** Returns the distance between two 2D line segments. */
float dist_seg_seg_v2(const float a1[3], const float a2[3], const float b1[3], const float b2[3]);

float closest_to_ray_v3(float r_close[3],
                        const float p[3],
                        const float ray_orig[3],
                        const float ray_dir[3]);
float closest_to_line_v2(float r_close[2], const float p[2], const float l1[2], const float l2[2]);
double closest_to_line_v2_db(double r_close[2],
                             const double p[2],
                             const double l1[2],
                             const double l2[2]);
/**
 * Find closest point to p on line through (`l1`, `l2`) and return lambda,
 * where (0 <= lambda <= 1) when `p` is in the line segment (`l1`, `l2`).
 */
float closest_to_line_v3(float r_close[3], const float p[3], const float l1[3], const float l2[3]);
/**
 * Point closest to v1 on line v2-v3 in 2D.
 *
 * \return A value in [0, 1] that corresponds to the position of #r_close on the line segment.
 */
float closest_to_line_segment_v2(float r_close[2],
                                 const float p[2],
                                 const float l1[2],
                                 const float l2[2]);

/**
 * Finds the points where two line segments are closest to each other.
 *
 * `lambda_*` is a value between 0 and 1 for each segment that indicates where `r_closest_*` is on
 * the corresponding segment.
 *
 * \return Squared distance between both segments.
 */
float closest_seg_seg_v2(float r_closest_a[2],
                         float r_closest_b[2],
                         float *r_lambda_a,
                         float *r_lambda_b,
                         const float a1[2],
                         const float a2[2],
                         const float b1[2],
                         const float b2[2]);

/**
 * Point closest to v1 on line v2-v3 in 3D.
 *
 * \return A value in [0, 1] that corresponds to the position of #r_close on the line segment.
 */
float closest_to_line_segment_v3(float r_close[3],
                                 const float p[3],
                                 const float l1[3],
                                 const float l2[3]);
void closest_to_plane_normalized_v3(float r_close[3], const float plane[4], const float pt[3]);
/**
 * Find the closest point on a plane.
 *
 * \param r_close: Return coordinate
 * \param plane: The plane to test against.
 * \param pt: The point to find the nearest of
 *
 * \note non-unit-length planes are supported.
 */
void closest_to_plane_v3(float r_close[3], const float plane[4], const float pt[3]);
void closest_to_plane3_normalized_v3(float r_close[3], const float plane[3], const float pt[3]);
void closest_to_plane3_v3(float r_close[3], const float plane[3], const float pt[3]);

/**
 * Set 'r' to the point in triangle (v1, v2, v3) closest to point 'p'.
 */
void closest_on_tri_to_point_v3(
    float r[3], const float p[3], const float v1[3], const float v2[3], const float v3[3]);

float ray_point_factor_v3_ex(const float p[3],
                             const float ray_origin[3],
                             const float ray_direction[3],
                             float epsilon,
                             float fallback);
float ray_point_factor_v3(const float p[3],
                          const float ray_origin[3],
                          const float ray_direction[3]);

/**
 * A simplified version of #closest_to_line_v3
 * we only need to return the `lambda`
 *
 * \param epsilon: avoid approaching divide-by-zero.
 * Passing a zero will just check for nonzero division.
 */
float line_point_factor_v3_ex(
    const float p[3], const float l1[3], const float l2[3], float epsilon, float fallback);
float line_point_factor_v3(const float p[3], const float l1[3], const float l2[3]);

float line_point_factor_v2_ex(
    const float p[2], const float l1[2], const float l2[2], float epsilon, float fallback);
float line_point_factor_v2(const float p[2], const float l1[2], const float l2[2]);

/**
 * \note #isect_line_plane_v3() shares logic.
 */
float line_plane_factor_v3(const float plane_co[3],
                           const float plane_no[3],
                           const float l1[3],
                           const float l2[3]);

/**
 * Ensure the distance between these points is no greater than 'dist'.
 * If it is, scale them both into the center.
 */
void limit_dist_v3(float v1[3], float v2[3], float dist);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Intersection
 * \{ */

/* TODO: int return value consistency. */

/* line-line */
#define ISECT_LINE_LINE_COLINEAR -1
#define ISECT_LINE_LINE_NONE 0
#define ISECT_LINE_LINE_EXACT 1
#define ISECT_LINE_LINE_CROSS 2

/**
 * Intersect Line-Line, floats.
 */
int isect_seg_seg_v2(const float v1[2], const float v2[2], const float v3[2], const float v4[2]);
/**
 * Returns a point on each segment that is closest to the other.
 */
void isect_seg_seg_v3(const float a0[3],
                      const float a1[3],
                      const float b0[3],
                      const float b1[3],
                      float r_a[3],
                      float r_b[3]);

/**
 * Intersect Line-Line, integer.
 */
int isect_seg_seg_v2_int(const int v1[2], const int v2[2], const int v3[2], const int v4[2]);
/**
 * Get intersection point of two 2D segments.
 *
 * \param endpoint_bias: Bias to use when testing for end-point overlap.
 * A positive value considers intersections that extend past the endpoints,
 * negative values contract the endpoints.
 * Note the bias is applied to a 0-1 factor, not scaled to the length of segments.
 *
 * \returns intersection type:
 * - -1: collinear.
 * -  1: intersection.
 * -  0: no intersection.
 */
int isect_seg_seg_v2_point_ex(const float v0[2],
                              const float v1[2],
                              const float v2[2],
                              const float v3[2],
                              float endpoint_bias,
                              float vi[2]);
int isect_seg_seg_v2_point(
    const float v0[2], const float v1[2], const float v2[2], const float v3[2], float vi[2]);
bool isect_seg_seg_v2_simple(const float v1[2],
                             const float v2[2],
                             const float v3[2],
                             const float v4[2]);
/**
 * If intersection == ISECT_LINE_LINE_CROSS or ISECT_LINE_LINE_NONE:
 * <pre>
 * pt = v1 + lambda * (v2 - v1) = v3 + mu * (v4 - v3)
 * </pre>
 * \returns intersection type:
 * - ISECT_LINE_LINE_COLINEAR: collinear.
 * - ISECT_LINE_LINE_EXACT: intersection at an endpoint of either.
 * - ISECT_LINE_LINE_CROSS: interaction, not at an endpoint.
 * - ISECT_LINE_LINE_NONE: no intersection.
 * Also returns lambda and mu in r_lambda and r_mu.
 */
int isect_seg_seg_v2_lambda_mu_db(const double v1[2],
                                  const double v2[2],
                                  const double v3[2],
                                  const double v4[2],
                                  double *r_lambda,
                                  double *r_mu);
/**
 * \param l1, l2: Coordinates (point of line).
 * \param sp, r: Coordinate and radius (sphere).
 * \return r_p1, r_p2: Intersection coordinates.
 *
 * \note The order of assignment for intersection points (\a r_p1, \a r_p2) is predictable,
 * based on the direction defined by `l2 - l1`,
 * this direction compared with the normal of each point on the sphere:
 * \a r_p1 always has a >= 0.0 dot product.
 * \a r_p2 always has a <= 0.0 dot product.
 * For example, when \a l1 is inside the sphere and \a l2 is outside,
 * \a r_p1 will always be between \a l1 and \a l2.
 */
int isect_line_sphere_v3(const float l1[3],
                         const float l2[3],
                         const float sp[3],
                         float r,
                         float r_p1[3],
                         float r_p2[3]);
int isect_line_sphere_v2(const float l1[2],
                         const float l2[2],
                         const float sp[2],
                         float r,
                         float r_p1[2],
                         float r_p2[2]);

/**
 * Intersect Line-Line, floats - gives intersection point.
 */
int isect_line_line_v2_point(
    const float v0[2], const float v1[2], const float v2[2], const float v3[2], float r_vi[2]);
/**
 * \return The number of point of interests
 * 0 - lines are collinear
 * 1 - lines are coplanar, i1 is set to intersection
 * 2 - i1 and i2 are the nearest points on line 1 (v1, v2) and line 2 (v3, v4) respectively
 */
int isect_line_line_epsilon_v3(const float v1[3],
                               const float v2[3],
                               const float v3[3],
                               const float v4[3],
                               float i1[3],
                               float i2[3],
                               float epsilon);
int isect_line_line_v3(const float v1[3],
                       const float v2[3],
                       const float v3[3],
                       const float v4[3],
                       float r_i1[3],
                       float r_i2[3]);
/**
 * Intersection point strictly between the two lines
 * \return false when no intersection is found.
 */
bool isect_line_line_strict_v3(const float v1[3],
                               const float v2[3],
                               const float v3[3],
                               const float v4[3],
                               float vi[3],
                               float *r_lambda);
/**
 * Check if two rays are not parallel and returns a factor that indicates
 * the distance from \a ray_origin_b to the closest point on ray-a to ray-b.
 *
 * \note Neither directions need to be normalized.
 */
bool isect_ray_ray_epsilon_v3(const float ray_origin_a[3],
                              const float ray_direction_a[3],
                              const float ray_origin_b[3],
                              const float ray_direction_b[3],
                              float epsilon,
                              float *r_lambda_a,
                              float *r_lambda_b);
bool isect_ray_ray_v3(const float ray_origin_a[3],
                      const float ray_direction_a[3],
                      const float ray_origin_b[3],
                      const float ray_direction_b[3],
                      float *r_lambda_a,
                      float *r_lambda_b);

/**
 * if clip is nonzero, will only return true if lambda is >= 0.0
 * (i.e. intersection point is along positive \a ray_direction)
 *
 * \note #line_plane_factor_v3() shares logic.
 */
bool isect_ray_plane_v3(const float ray_origin[3],
                        const float ray_direction[3],
                        const float plane[4],
                        float *r_lambda,
                        bool clip);

/**
 * Check if a point is behind all planes.
 */
bool isect_point_planes_v3(float (*planes)[4], int totplane, const float p[3]);
/**
 * Check if a point is in front all planes.
 * Same as isect_point_planes_v3 but with planes facing the opposite direction.
 */
bool isect_point_planes_v3_negated(const float (*planes)[4], int totplane, const float p[3]);

/**
 * Intersect line/plane.
 *
 * \param r_isect_co: The intersection point.
 * \param l1: The first point of the line.
 * \param l2: The second point of the line.
 * \param plane_co: A point on the plane to intersect with.
 * \param plane_no: The direction of the plane (does not need to be normalized).
 *
 * \note #line_plane_factor_v3() shares logic.
 */
bool isect_line_plane_v3(float r_isect_co[3],
                         const float l1[3],
                         const float l2[3],
                         const float plane_co[3],
                         const float plane_no[3]) ATTR_WARN_UNUSED_RESULT;

/**
 * Intersect three planes, return the point where all 3 meet.
 * See Graphics Gems 1 pg 305
 *
 * \param plane_a, plane_b, plane_c: Planes.
 * \param r_isect_co: The resulting intersection point.
 */
bool isect_plane_plane_plane_v3(const float plane_a[4],
                                const float plane_b[4],
                                const float plane_c[4],
                                float r_isect_co[3]) ATTR_WARN_UNUSED_RESULT;
/**
 * Intersect two planes, return a point on the intersection and a vector
 * that runs on the direction of the intersection.
 * \note this is a slightly reduced version of #isect_plane_plane_plane_v3
 *
 * \param plane_a, plane_b: Planes.
 * \param r_isect_co: The resulting intersection point.
 * \param r_isect_no: The resulting vector of the intersection.
 *
 * \note \a r_isect_no isn't unit length.
 */
bool isect_plane_plane_v3(const float plane_a[4],
                          const float plane_b[4],
                          float r_isect_co[3],
                          float r_isect_no[3]) ATTR_WARN_UNUSED_RESULT;

/**
 * Intersect all planes, calling `callback_fn` for each point that intersects
 * 3 of the planes that isn't outside any of the other planes.
 *
 * This can be thought of as calculating a convex-hull from an array of planes.
 *
 * \param eps_coplanar: Epsilon for testing if two planes are aligned (co-planar).
 * \param eps_isect: Epsilon for testing of a point is behind any of the planes.
 *
 * \warning As complexity is a little under `O(N^3)`, this is only suitable for small arrays.
 *
 * \note This function could be optimized by some spatial structure.
 */
bool isect_planes_v3_fn(
    const float planes[][4],
    int planes_len,
    float eps_coplanar,
    float eps_isect,
    void (*callback_fn)(const float co[3], int i, int j, int k, void *user_data),
    void *user_data);

/* line/ray triangle */

/**
 * Test if the line starting at p1 ending at p2 intersects the triangle v0..v2
 * return non zero if it does.
 */
bool isect_line_segment_tri_v3(const float p1[3],
                               const float p2[3],
                               const float v0[3],
                               const float v1[3],
                               const float v2[3],
                               float *r_lambda,
                               float r_uv[2]);
/**
 * Like #isect_line_segment_tri_v3, but allows epsilon tolerance around triangle.
 */
bool isect_line_segment_tri_epsilon_v3(const float p1[3],
                                       const float p2[3],
                                       const float v0[3],
                                       const float v1[3],
                                       const float v2[3],
                                       float *r_lambda,
                                       float r_uv[2],
                                       float epsilon);
bool isect_axial_line_segment_tri_v3(int axis,
                                     const float p1[3],
                                     const float p2[3],
                                     const float v0[3],
                                     const float v1[3],
                                     const float v2[3],
                                     float *r_lambda);

/**
 * Test if the ray starting at p1 going in d direction intersects the triangle v0..v2
 * return non zero if it does.
 */
bool isect_ray_tri_v3(const float ray_origin[3],
                      const float ray_direction[3],
                      const float v0[3],
                      const float v1[3],
                      const float v2[3],
                      float *r_lambda,
                      float r_uv[2]);
bool isect_ray_tri_threshold_v3(const float ray_origin[3],
                                const float ray_direction[3],
                                const float v0[3],
                                const float v1[3],
                                const float v2[3],
                                float *r_lambda,
                                float r_uv[2],
                                float threshold);
bool isect_ray_tri_epsilon_v3(const float ray_origin[3],
                              const float ray_direction[3],
                              const float v0[3],
                              const float v1[3],
                              const float v2[3],
                              float *r_lambda,
                              float r_uv[2],
                              float epsilon);
/**
 * Intersect two triangles.
 *
 * \param r_i1, r_i2: Retrieve the overlapping edge between the 2 triangles.
 * \param r_tri_a_edge_isect_count: Indicates how many edges in the first triangle are intersected.
 * \return true when the triangles intersect.
 *
 * \note If it exists, \a r_i1 will be a point on the edge of the 1st triangle.
 * \note intersections between coplanar triangles are currently undetected.
 */
bool isect_tri_tri_v3_ex(const float tri_a[3][3],
                         const float tri_b[3][3],
                         float r_i1[3],
                         float r_i2[3],
                         int *r_tri_a_edge_isect_count);
bool isect_tri_tri_v3(const float t_a0[3],
                      const float t_a1[3],
                      const float t_a2[3],
                      const float t_b0[3],
                      const float t_b1[3],
                      const float t_b2[3],
                      float r_i1[3],
                      float r_i2[3]);

bool isect_tri_tri_v2(const float p1[2],
                      const float q1[2],
                      const float r1[2],
                      const float p2[2],
                      const float q2[2],
                      const float r2[2]);

/**
 * Water-tight ray-cast (requires pre-calculation).
 */
struct IsectRayPrecalc {
  /* Maximal dimension `kz`, and orthogonal dimensions. */
  int kx, ky, kz;

  /* Shear constants. */
  float sx, sy, sz;
};

void isect_ray_tri_watertight_v3_precalc(struct IsectRayPrecalc *isect_precalc,
                                         const float ray_direction[3]);
bool isect_ray_tri_watertight_v3(const float ray_origin[3],
                                 const struct IsectRayPrecalc *isect_precalc,
                                 const float v0[3],
                                 const float v1[3],
                                 const float v2[3],
                                 float *r_dist,
                                 float r_uv[2]);
/**
 * Slower version which calculates #IsectRayPrecalc each time.
 */
bool isect_ray_tri_watertight_v3_simple(const float ray_origin[3],
                                        const float ray_direction[3],
                                        const float v0[3],
                                        const float v1[3],
                                        const float v2[3],
                                        float *r_lambda,
                                        float r_uv[2]);

bool isect_ray_seg_v2(const float ray_origin[2],
                      const float ray_direction[2],
                      const float v0[2],
                      const float v1[2],
                      float *r_lambda,
                      float *r_u);

bool isect_ray_line_v3(const float ray_origin[3],
                       const float ray_direction[3],
                       const float v0[3],
                       const float v1[3],
                       float *r_lambda);

/* Point in polygon. */

bool isect_point_poly_v2(const float pt[2],
                         const float verts[][2],
                         unsigned int nr,
                         bool use_holes);
bool isect_point_poly_v2_int(const int pt[2],
                             const int verts[][2],
                             unsigned int nr,
                             bool use_holes);

/**
 * Point in quad - only convex quads.
 */
int isect_point_quad_v2(
    const float p[2], const float v1[2], const float v2[2], const float v3[2], const float v4[2]);

int isect_point_tri_v2(const float pt[2], const float v1[2], const float v2[2], const float v3[2]);
/**
 * Only single direction.
 */
bool isect_point_tri_v2_cw(const float pt[2],
                           const float v1[2],
                           const float v2[2],
                           const float v3[2]);
/**
 * \code{.unparsed}
 * x1,y2
 * |  \
 * |   \     .(a,b)
 * |    \
 * x1,y1-- x2,y1
 * \endcode
 */
int isect_point_tri_v2_int(int x1, int y1, int x2, int y2, int a, int b);
bool isect_point_tri_prism_v3(const float p[3],
                              const float v1[3],
                              const float v2[3],
                              const float v3[3]);
/**
 * \param r_isect_co: The point \a p projected onto the triangle.
 * \return True when \a p is inside the triangle.
 * \note Its up to the caller to check the distance between \a p and \a r_vi
 * against an error margin.
 */
bool isect_point_tri_v3(const float p[3],
                        const float v1[3],
                        const float v2[3],
                        const float v3[3],
                        float r_isect_co[3]);

/**
 * Axis-aligned bounding box.
 */
bool isect_aabb_aabb_v3(const float min1[3],
                        const float max1[3],
                        const float min2[3],
                        const float max2[3]);

struct IsectRayAABB_Precalc {
  float ray_origin[3];
  float ray_inv_dir[3];
  int sign[3];
};

void isect_ray_aabb_v3_precalc(struct IsectRayAABB_Precalc *data,
                               const float ray_origin[3],
                               const float ray_direction[3]);
bool isect_ray_aabb_v3(const struct IsectRayAABB_Precalc *data,
                       const float bb_min[3],
                       const float bb_max[3],
                       float *tmin);
/**
 * Test a bounding box (AABB) for ray intersection.
 * Assumes the ray is already local to the boundbox space.
 *
 * \note \a direction should be normalized
 * if you intend to use the \a tmin or \a tmax distance results!
 */
bool isect_ray_aabb_v3_simple(const float orig[3],
                              const float dir[3],
                              const float bb_min[3],
                              const float bb_max[3],
                              float *tmin,
                              float *tmax);

/* other */
#define ISECT_AABB_PLANE_BEHIND_ANY 0
#define ISECT_AABB_PLANE_CROSS_ANY 1
#define ISECT_AABB_PLANE_IN_FRONT_ALL 2

/**
 * Checks status of an AABB in relation to a list of planes.
 *
 * \returns intersection type:
 * - ISECT_AABB_PLANE_BEHIND_ONE   (0): AABB is completely behind at least 1 plane;
 * - ISECT_AABB_PLANE_CROSS_ANY    (1): AABB intersects at least 1 plane;
 * - ISECT_AABB_PLANE_IN_FRONT_ALL (2): AABB is completely in front of all planes;
 */
int isect_aabb_planes_v3(const float (*planes)[4],
                         int totplane,
                         const float bbmin[3],
                         const float bbmax[3]);

bool isect_sweeping_sphere_tri_v3(const float p1[3],
                                  const float p2[3],
                                  float radius,
                                  const float v0[3],
                                  const float v1[3],
                                  const float v2[3],
                                  float *r_lambda,
                                  float ipoint[3]);

bool clip_segment_v3_plane(
    const float p1[3], const float p2[3], const float plane[4], float r_p1[3], float r_p2[3]);
bool clip_segment_v3_plane_n(const float p1[3],
                             const float p2[3],
                             const float plane_array[][4],
                             int plane_num,
                             float r_p1[3],
                             float r_p2[3]);

bool point_in_slice_seg(float p[3], float l1[3], float l2[3]);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Interpolation
 * \{ */

void interp_weights_tri_v3(
    float w[3], const float v1[3], const float v2[3], const float v3[3], const float co[3]);
void interp_weights_quad_v3(float w[4],
                            const float v1[3],
                            const float v2[3],
                            const float v3[3],
                            const float v4[3],
                            const float co[3]);
void interp_weights_poly_v3(float w[], float v[][3], int n, const float co[3]);
void interp_weights_poly_v2(float w[], float v[][2], int n, const float co[2]);

/** `(x1, v1)(t1=0)------(x2, v2)(t2=1), 0<t<1 --> (x, v)(t)`. */
void interp_cubic_v3(float x[3],
                     float v[3],
                     const float x1[3],
                     const float v1[3],
                     const float x2[3],
                     const float v2[3],
                     float t);

/**
 * Given an array with some invalid values this function interpolates valid values
 * replacing the invalid ones.
 */
int interp_sparse_array(float *array, int list_size, float skipval);

/**
 * Given 2 triangles in 3D space, and a point in relation to the first triangle.
 * calculate the location of a point in relation to the second triangle.
 * Useful for finding relative positions with geometry.
 */
void transform_point_by_tri_v3(float pt_tar[3],
                               float const pt_src[3],
                               const float tri_tar_p1[3],
                               const float tri_tar_p2[3],
                               const float tri_tar_p3[3],
                               const float tri_src_p1[3],
                               const float tri_src_p2[3],
                               const float tri_src_p3[3]);
/**
 * Simply re-interpolates,
 * assumes p_src is between \a l_src_p1-l_src_p2
 */
void transform_point_by_seg_v3(float p_dst[3],
                               const float p_src[3],
                               const float l_dst_p1[3],
                               const float l_dst_p2[3],
                               const float l_src_p1[3],
                               const float l_src_p2[3]);

/**
 * \note Using #cross_tri_v2 means locations outside the triangle are correctly weighted.
 *
 * \note This is *exactly* the same calculation as #resolve_tri_uv_v2,
 * although it has double precision and is used for texture baking, so keep both.
 */
void barycentric_weights_v2(
    const float v1[2], const float v2[2], const float v3[2], const float co[2], float w[3]);
/**
 * A version of #barycentric_weights_v2 that doesn't allow negative weights.
 * Useful when negative values cause problems and points are only
 * ever slightly outside of the triangle.
 */
void barycentric_weights_v2_clamped(
    const float v1[2], const float v2[2], const float v3[2], const float co[2], float w[3]);
/**
 * still use 2D X,Y space but this works for verts transformed by a perspective matrix,
 * using their 4th component as a weight
 */
void barycentric_weights_v2_persp(
    const float v1[4], const float v2[4], const float v3[4], const float co[2], float w[3]);
/**
 * same as #barycentric_weights_v2 but works with a quad,
 * NOTE: untested for values outside the quad's bounds
 * this is #interp_weights_poly_v2 expanded for quads only
 */
void barycentric_weights_v2_quad(const float v1[2],
                                 const float v2[2],
                                 const float v3[2],
                                 const float v4[2],
                                 const float co[2],
                                 float w[4]);

/**
 * \return false for degenerated triangles.
 */
bool barycentric_coords_v2(
    const float v1[2], const float v2[2], const float v3[2], const float co[2], float w[3]);
/**
 * \return
 * - 0 if the point is outside of triangle.
 * - 1 if the point is inside triangle.
 * - 2 if it's on the edge.
 */
int barycentric_inside_triangle_v2(const float w[3]);

/**
 * Barycentric reverse
 *
 * Compute coordinates (u, v) for point \a st with respect to triangle (\a st0, \a st1, \a st2)
 *
 * \note same basic result as #barycentric_weights_v2, see its comment for details.
 */
void resolve_tri_uv_v2(
    float r_uv[2], const float st[2], const float st0[2], const float st1[2], const float st2[2]);
/**
 * Barycentric reverse 3d
 *
 * Compute coordinates (u, v) for point \a st with respect to triangle (\a st0, \a st1, \a st2)
 */
void resolve_tri_uv_v3(
    float r_uv[2], const float st[3], const float st0[3], const float st1[3], const float st2[3]);
/**
 * Bilinear reverse.
 */
void resolve_quad_uv_v2(float r_uv[2],
                        const float st[2],
                        const float st0[2],
                        const float st1[2],
                        const float st2[2],
                        const float st3[2]);
/**
 * Bilinear reverse with derivatives.
 */
void resolve_quad_uv_v2_deriv(float r_uv[2],
                              float r_deriv[2][2],
                              const float st[2],
                              const float st0[2],
                              const float st1[2],
                              const float st2[2],
                              const float st3[2]);
/**
 * A version of resolve_quad_uv_v2 that only calculates the 'u'.
 */
float resolve_quad_u_v2(const float st[2],
                        const float st0[2],
                        const float st1[2],
                        const float st2[2],
                        const float st3[2]);

/**
 * Use to find the point of a UV on a face.
 * Reverse of `resolve_*` functions.
 */
void interp_bilinear_quad_v3(float data[4][3], float u, float v, float res[3]);
void interp_barycentric_tri_v3(float data[3][3], float u, float v, float res[3]);

/** \} */

/* -------------------------------------------------------------------- */
/** \name View & Projection
 * \{ */

void lookat_m4(
    float mat[4][4], float vx, float vy, float vz, float px, float py, float pz, float twist);
void polarview_m4(float mat[4][4], float dist, float azimuth, float incidence, float twist);

/**
 * Matches `glFrustum` result.
 */
void perspective_m4(float mat[4][4],
                    float left,
                    float right,
                    float bottom,
                    float top,
                    float nearClip,
                    float farClip);
void perspective_m4_fov(float mat[4][4],
                        float angle_left,
                        float angle_right,
                        float angle_up,
                        float angle_down,
                        float nearClip,
                        float farClip);
/**
 * Matches `glOrtho` result.
 */
void orthographic_m4(float mat[4][4],
                     float left,
                     float right,
                     float bottom,
                     float top,
                     float nearClip,
                     float farClip);
/**
 * Translate a matrix created by orthographic_m4 or perspective_m4 in XY coords
 * (used to jitter the view).
 */
void window_translate_m4(float winmat[4][4], float perspmat[4][4], float x, float y);

/**
 * Frustum planes extraction from a projection matrix
 * (homogeneous 4d vector representations of planes).
 *
 * plane parameters can be NULL if you do not need them.
 */
void planes_from_projmat(const float mat[4][4],
                         float left[4],
                         float right[4],
                         float bottom[4],
                         float top[4],
                         float near[4],
                         float far[4]);

void projmat_dimensions(const float winmat[4][4],
                        float *r_left,
                        float *r_right,
                        float *r_bottom,
                        float *r_top,
                        float *r_near,
                        float *r_far);
void projmat_dimensions_db(const float winmat[4][4],
                           double *r_left,
                           double *r_right,
                           double *r_bottom,
                           double *r_top,
                           double *r_near,
                           double *r_far);

/**
 * Creates a projection matrix for a small region of the viewport.
 *
 * \param projmat: Projection Matrix.
 * \param win_size: Viewport Size.
 * \param x_min, x_max, y_min, y_max: Coordinates of the subregion.
 * \return r_projmat: Resulting Projection Matrix.
 */
void projmat_from_subregion(const float projmat[4][4],
                            const int win_size[2],
                            int x_min,
                            int x_max,
                            int y_min,
                            int y_max,
                            float r_projmat[4][4]);

int box_clip_bounds_m4(float boundbox[2][3], const float bounds[4], float winmat[4][4]);
void box_minmax_bounds_m4(float min[3], float max[3], float boundbox[2][3], float mat[4][4]);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Mapping
 * \{ */

void map_to_tube(float *r_u, float *r_v, float x, float y, float z);
void map_to_sphere(float *r_u, float *r_v, float x, float y, float z);
void map_to_plane_v2_v3v3(float r_co[2], const float co[3], const float no[3]);
void map_to_plane_axis_angle_v2_v3v3fl(float r_co[2],
                                       const float co[3],
                                       const float axis[3],
                                       float angle);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Normals
 * \{ */

void accumulate_vertex_normals_tri_v3(float n1[3],
                                      float n2[3],
                                      float n3[3],
                                      const float f_no[3],
                                      const float co1[3],
                                      const float co2[3],
                                      const float co3[3]);

void accumulate_vertex_normals_v3(float n1[3],
                                  float n2[3],
                                  float n3[3],
                                  float n4[3],
                                  const float f_no[3],
                                  const float co1[3],
                                  const float co2[3],
                                  const float co3[3],
                                  const float co4[3]);

/**
 * Add weighted face normal component into normals of the face vertices.
 * Caller must pass pre-allocated vdiffs of nverts length.
 */
void accumulate_vertex_normals_poly_v3(
    float **vertnos, const float polyno[3], const float **vertcos, float vdiffs[][3], int nverts);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Tangents
 * \{ */

void tangent_from_uv_v3(const float uv1[2],
                        const float uv2[2],
                        const float uv3[2],
                        const float co1[3],
                        const float co2[3],
                        const float co3[3],
                        const float n[3],
                        float r_tang[3]);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Vector Clouds
 * \{ */

/**
 * Input:
 *
 * \param list_size: 4 lists as pointer to array[list_size]
 * \param pos: current pos array of 'new' positions
 * \param weight: current weight array of 'new'weights (may be NULL pointer if you have no weights)
 * \param rpos: Reference rpos array of 'old' positions
 * \param rweight: Reference rweight array of 'old'weights
 * (may be NULL pointer if you have no weights).
 *
 * Output:
 *
 * \param lloc: Center of mass pos.
 * \param rloc: Center of mass rpos.
 * \param lrot: Rotation matrix.
 * \param lscale: Scale matrix.
 *
 * pointers may be NULL if not needed
 */
void vcloud_estimate_transform_v3(int list_size,
                                  const float (*pos)[3],
                                  const float *weight,
                                  const float (*rpos)[3],
                                  const float *rweight,
                                  float lloc[3],
                                  float rloc[3],
                                  float lrot[3][3],
                                  float lscale[3][3]);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Spherical Harmonics
 *
 * Uses 2nd order SH => 9 coefficients, stored in this order:
 * - 0 = `(0, 0)`
 * - 1 = `(1, -1), 2 = (1, 0), 3 = (1, 1)`
 * - 4 = `(2, -2), 5 = (2, -1), 6 = (2, 0), 7 = (2, 1), 8 = (2, 2)`
 * \{ */

MINLINE void zero_sh(float r[9]);
MINLINE void copy_sh_sh(float r[9], const float a[9]);
MINLINE void mul_sh_fl(float r[9], float f);
MINLINE void add_sh_shsh(float r[9], const float a[9], const float b[9]);
MINLINE float dot_shsh(const float a[9], const float b[9]);

MINLINE float eval_shv3(float sh[9], const float v[3]);
MINLINE float diffuse_shv3(const float sh[9], const float v[3]);
MINLINE void vec_fac_to_sh(float r[9], const float v[3], float f);
MINLINE void madd_sh_shfl(float r[9], const float sh[9], float f);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Form Factor
 * \{ */

float form_factor_quad(const float p[3],
                       const float n[3],
                       const float q0[3],
                       const float q1[3],
                       const float q2[3],
                       const float q3[3]);
bool form_factor_visible_quad(const float p[3],
                              const float n[3],
                              const float v0[3],
                              const float v1[3],
                              const float v2[3],
                              float q0[3],
                              float q1[3],
                              float q2[3],
                              float q3[3]);
float form_factor_hemi_poly(
    float p[3], float n[3], float v1[3], float v2[3], float v3[3], float v4[3]);

/**
 * Same as axis_dominant_v3_to_m3, but flips the normal
 */
void axis_dominant_v3_to_m3_negate(float r_mat[3][3], const float normal[3]);
/**
 * \brief Normal to x,y matrix
 *
 * Creates a 3x3 matrix from a normal.
 * This matrix can be applied to vectors so their 'z' axis runs along \a normal.
 * In practice it means you can use x,y as 2d coords. \see
 *
 * \param r_mat: The matrix to return.
 * \param normal: A unit length vector.
 */
void axis_dominant_v3_to_m3(float r_mat[3][3], const float normal[3]);

/**
 * Get the 2 dominant axis values, 0==X, 1==Y, 2==Z.
 */
MINLINE void axis_dominant_v3(int *r_axis_a, int *r_axis_b, const float axis[3]);
/**
 * Same as #axis_dominant_v3 but return the max value.
 */
MINLINE float axis_dominant_v3_max(int *r_axis_a,
                                   int *r_axis_b,
                                   const float axis[3]) ATTR_WARN_UNUSED_RESULT;
/**
 * Get the single dominant axis value, 0==X, 1==Y, 2==Z.
 */
MINLINE int axis_dominant_v3_single(const float vec[3]);
/**
 * The dominant axis of an orthogonal vector.
 */
MINLINE int axis_dominant_v3_ortho_single(const float vec[3]);

MINLINE int max_axis_v3(const float vec[3]);
MINLINE int min_axis_v3(const float vec[3]);

/**
 * Simple function to either:
 * - Calculate how many triangles needed from the total number of polygons + loops.
 * - Calculate the first triangle index from the polygon index & that polygons loop-start.
 *
 * \param poly_count: The number of polygons or polygon-index
 * (3+ sided faces, 1-2 sided give incorrect results).
 * \param corner_count: The number of corners (also called loop-index).
 */
MINLINE int poly_to_tri_count(int poly_count, int corner_count);

/**
 * Useful to calculate an even width shell, by taking the angle between 2 planes.
 * The return value is a scale on the offset.
 * no angle between planes is 1.0, as the angle between the 2 planes approaches 180d
 * the distance gets very high, 180d would be inf, but this case isn't valid.
 */
MINLINE float shell_angle_to_dist(float angle);
/**
 * Equivalent to `shell_angle_to_dist(angle_normalized_v3v3(a, b))`.
 */
MINLINE float shell_v3v3_normalized_to_dist(const float a[3], const float b[3]);
/**
 * Equivalent to `shell_angle_to_dist(angle_normalized_v2v2(a, b))`.
 */
MINLINE float shell_v2v2_normalized_to_dist(const float a[2], const float b[2]);
/**
 * Equivalent to `shell_angle_to_dist(angle_normalized_v3v3(a, b) / 2)`.
 */
MINLINE float shell_v3v3_mid_normalized_to_dist(const float a[3], const float b[3]);
/**
 * Equivalent to `shell_angle_to_dist(angle_normalized_v2v2(a, b) / 2)`.
 */
MINLINE float shell_v2v2_mid_normalized_to_dist(const float a[2], const float b[2]);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Cubic (Bezier)
 * \{ */

/**
 * Return the value which the distance between points will need to be scaled by,
 * to define a handle, given both points are on a perfect circle.
 *
 * Use when we want a bezier curve to match a circle as closely as possible.
 *
 * \note the return value will need to be divided by 0.75 for correct results.
 */
float cubic_tangent_factor_circle_v3(const float tan_l[3], const float tan_r[3]);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Geodesics
 * \{ */

/**
 * Utility for computing approximate geodesic distances on triangle meshes.
 *
 * Given triangle with vertex coordinates v0, v1, v2, and known geodesic distances
 * dist1 and dist2 at v1 and v2, estimate a geodesic distance at vertex v0.
 *
 * From "Dart Throwing on Surfaces", EGSR 2009. Section 7, Geodesic Dart Throwing.
 */
float geodesic_distance_propagate_across_triangle(
    const float v0[3], const float v1[3], const float v2[3], float dist1, float dist2);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Inline Definitions
 * \{ */

#if BLI_MATH_DO_INLINE
#  include "intern/math_geom_inline.c"
#endif

#ifdef BLI_MATH_GCC_WARN_PRAGMA
#  pragma GCC diagnostic pop
#endif

/** \} */

#ifdef __cplusplus
}
#endif
