/* SPDX-FileCopyrightText: 2008 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bke
 */

#include <optional>

#include "BLI_bounds_types.hh"
#include "BLI_math_vector_types.hh"

struct Depsgraph;
struct Main;
struct Object;
struct Scene;
struct bGPDcurve;
struct bGPDlayer;
struct bGPDframe;
struct bGPDspoint;
struct bGPDstroke;
struct bGPdata;

/* Object bound-box. */

/**
 * Get min/max bounds of all strokes in grease pencil data-block.
 * \param gpd: Grease pencil data-block
 * \param r_min: Result minimum coordinates
 * \param r_max: Result maximum coordinates
 * \return True if it was possible to calculate
 */
std::optional<blender::Bounds<blender::float3>> BKE_gpencil_data_minmax(const struct bGPdata *gpd);
/**
 * Get min/max coordinate bounds for single stroke.
 * \param gps: Grease pencil stroke
 * \param use_select: Include only selected points
 * \param r_min: Result minimum coordinates
 * \param r_max: Result maximum coordinates
 * \return True if it was possible to calculate
 */
bool BKE_gpencil_stroke_minmax(const struct bGPDstroke *gps,
                               bool use_select,
                               float r_min[3],
                               float r_max[3]);

/**
 * Compute center of bounding box.
 * \param gpd: Grease pencil data-block
 * \param r_centroid: Location of the center
 */
void BKE_gpencil_centroid_3d(struct bGPdata *gpd, float r_centroid[3]);
/**
 * Compute stroke bounding box.
 * \param gps: Grease pencil Stroke
 */
void BKE_gpencil_stroke_boundingbox_calc(struct bGPDstroke *gps);

/* Stroke geometry utilities. */

/**
 * Calculate stroke normals.
 * \param gps: Grease pencil stroke
 * \param r_normal: Return Normal vector normalized
 */
void BKE_gpencil_stroke_normal(const struct bGPDstroke *gps, float r_normal[3]);

/**
 * Trim stroke to the first intersection or loop.
 * \param gps: Stroke data
 */
bool BKE_gpencil_stroke_trim(struct bGPdata *gpd, struct bGPDstroke *gps);

/**
 * Get points of stroke always flat to view not affected
 * by camera view or view position.
 * \param points: Array of grease pencil points (3D)
 * \param totpoints: Total of points
 * \param points2d: Result array of 2D points
 * \param r_direction: Return Concave (-1), Convex (1), or Auto-detect (0)
 */
void BKE_gpencil_stroke_2d_flat(const struct bGPDspoint *points,
                                int totpoints,
                                float (*points2d)[2],
                                int *r_direction);
/**
 * Get points of stroke always flat to view not affected by camera view or view position
 * using another stroke as reference.
 * \param ref_points: Array of reference points (3D)
 * \param ref_totpoints: Total reference points
 * \param points: Array of points to flat (3D)
 * \param totpoints: Total points
 * \param points2d: Result array of 2D points
 * \param scale: Scale factor
 * \param r_direction: Return Concave (-1), Convex (1), or Auto-detect (0)
 */
void BKE_gpencil_stroke_2d_flat_ref(const struct bGPDspoint *ref_points,
                                    int ref_totpoints,
                                    const struct bGPDspoint *points,
                                    int totpoints,
                                    float (*points2d)[2],
                                    float scale,
                                    int *r_direction);
/**
 * Triangulate stroke to generate data for filling areas.
 * \param gps: Grease pencil stroke
 */
void BKE_gpencil_stroke_fill_triangulate(struct bGPDstroke *gps);
/**
 * Recalc all internal geometry data for the stroke
 * \param gpd: Grease pencil data-block
 * \param gps: Grease pencil stroke
 */
void BKE_gpencil_stroke_geometry_update(struct bGPdata *gpd, struct bGPDstroke *gps);
/**
 * Update Stroke UV data.
 * \param gps: Grease pencil stroke
 */
void BKE_gpencil_stroke_uv_update(struct bGPDstroke *gps);

/**
 * Apply grease pencil Transforms.
 * \param gpd: Grease pencil data-block
 * \param mat: Transformation matrix
 */
void BKE_gpencil_transform(struct bGPdata *gpd, const float mat[4][4]);

typedef struct GPencilPointCoordinates {
  /* This is used when doing "move only origin" in object_data_transform.cc.
   * pressure is needs to be stored here as it is tied to object scale. */
  float co[3];
  float pressure;
} GPencilPointCoordinates;

/**
 * \note Used for "move only origins" in object_data_transform.cc.
 */
int BKE_gpencil_stroke_point_count(const struct bGPdata *gpd);
/**
 * \note Used for "move only origins" in object_data_transform.cc.
 */
void BKE_gpencil_point_coords_get(struct bGPdata *gpd, GPencilPointCoordinates *elem_data);
/**
 * \note Used for "move only origins" in object_data_transform.cc.
 */
void BKE_gpencil_point_coords_apply(struct bGPdata *gpd, const GPencilPointCoordinates *elem_data);
/**
 * \note Used for "move only origins" in object_data_transform.cc.
 */
void BKE_gpencil_point_coords_apply_with_mat4(struct bGPdata *gpd,
                                              const GPencilPointCoordinates *elem_data,
                                              const float mat[4][4]);

/**
 * Apply smooth position to stroke point.
 * \param gps: Stroke to smooth
 * \param i: Point index
 * \param inf: Amount of smoothing to apply
 * \param iterations: Radius of points to consider, equivalent to iterations
 * \param smooth_caps: Apply smooth to stroke extremes
 * \param keep_shape: Smooth out fine details first
 * \param r_gps: Stroke to put the result into
 */
bool BKE_gpencil_stroke_smooth_point(struct bGPDstroke *gps,
                                     int point_index,
                                     float influence,
                                     int iterations,
                                     bool smooth_caps,
                                     bool keep_shape,
                                     struct bGPDstroke *r_gps);
/**
 * Apply smooth strength to stroke point.
 * \param gps: Stroke to smooth
 * \param point_index: Point index
 * \param influence: Amount of smoothing to apply
 * \param iterations: Radius of points to consider, equivalent to iterations
 * \param r_gps: Stroke to put the result into
 */
bool BKE_gpencil_stroke_smooth_strength(struct bGPDstroke *gps,
                                        int point_index,
                                        float influence,
                                        int iterations,
                                        struct bGPDstroke *r_gps);
/**
 * Apply smooth for thickness to stroke point (use pressure).
 * \param gps: Stroke to smooth
 * \param point_index: Point index
 * \param influence: Amount of smoothing to apply
 * \param iterations: Radius of points to consider, equivalent to iterations
 * \param r_gps: Stroke to put the result into
 */
bool BKE_gpencil_stroke_smooth_thickness(struct bGPDstroke *gps,
                                         int point_index,
                                         float influence,
                                         int iterations,
                                         struct bGPDstroke *r_gps);
/**
 * Apply smooth for UV rotation/factor to stroke point.
 * \param gps: Stroke to smooth
 * \param point_index: Point index
 * \param influence: Amount of smoothing to apply
 * \param iterations: Radius of points to consider, equivalent to iterations
 * \param r_gps: Stroke to put the result into
 */
bool BKE_gpencil_stroke_smooth_uv(struct bGPDstroke *gps,
                                  int point_index,
                                  float influence,
                                  int iterations,
                                  struct bGPDstroke *r_gps);
/**
 * Apply smooth operation to the stroke.
 * \param gps: Stroke to smooth
 * \param influence: The interpolation factor for the smooth and the original stroke
 * \param iterations: Radius of points to consider, equivalent to iterations
 * \param smooth_position: Smooth point locations
 * \param smooth_strength: Smooth point strength
 * \param smooth_thickness: Smooth point thickness
 * \param smooth_uv: Smooth uv rotation/factor
 * \param keep_shape: Use different distribution for smooth locations to keep the shape
 * \param weights: per point weights to multiply influence with (optional, can be null)
 */
void BKE_gpencil_stroke_smooth(struct bGPDstroke *gps,
                               const float influence,
                               const int iterations,
                               const bool smooth_position,
                               const bool smooth_strength,
                               const bool smooth_thickness,
                               const bool smooth_uv,
                               const bool keep_shape,
                               const float *weights);
/**
 * Close grease pencil stroke.
 * \param gps: Stroke to close
 */
bool BKE_gpencil_stroke_close(struct bGPDstroke *gps);

/**
 * Split the given stroke into several new strokes, partitioning
 * it based on whether the stroke points have a particular flag
 * is set (e.g. #GP_SPOINT_SELECT in most cases, but not always).
 */
struct bGPDstroke *BKE_gpencil_stroke_delete_tagged_points(struct bGPdata *gpd,
                                                           struct bGPDframe *gpf,
                                                           struct bGPDstroke *gps,
                                                           struct bGPDstroke *next_stroke,
                                                           int tag_flags,
                                                           bool select,
                                                           bool flat_cap,
                                                           int limit);

/**
 * Flip stroke.
 */
void BKE_gpencil_stroke_flip(struct bGPDstroke *gps);

/**
 * Calculate grease pencil stroke length.
 * \param gps: Grease pencil stroke.
 * \param use_3d: Set to true to use 3D points.
 * \return Length of the stroke.
 */
float BKE_gpencil_stroke_length(const struct bGPDstroke *gps, bool use_3d);
/** Calculate grease pencil stroke length between points. */
float BKE_gpencil_stroke_segment_length(const struct bGPDstroke *gps,
                                        int start_index,
                                        int end_index,
                                        bool use_3d);

/**
 * Set a random color to stroke using vertex color.
 * \param gps: Stroke
 */
void BKE_gpencil_stroke_set_random_color(struct bGPDstroke *gps);

/**
 * Join two strokes using the shortest distance (reorder stroke if necessary).
 * \param auto_flip: Flip the stroke if the join between two strokes is not end->start points.
 */
void BKE_gpencil_stroke_join(struct bGPDstroke *gps_a,
                             struct bGPDstroke *gps_b,
                             bool leave_gaps,
                             bool fit_thickness,
                             bool smooth,
                             bool auto_flip);

/**
 * Stroke to view space
 * Transforms a stroke to view space.
 * This allows for manipulations in 2D but also easy conversion back to 3D.
 * \note also takes care of parent space transform.
 */
void BKE_gpencil_stroke_to_view_space(struct bGPDstroke *gps,
                                      float viewmat[4][4],
                                      const float diff_mat[4][4]);
/**
 * Stroke from view space
 * Transforms a stroke from view space back to world space.
 * Inverse of #BKE_gpencil_stroke_to_view_space
 * \note also takes care of parent space transform.
 */
void BKE_gpencil_stroke_from_view_space(struct bGPDstroke *gps,
                                        float viewinv[4][4],
                                        const float diff_mat[4][4]);
/**
 * Get average pressure.
 */
float BKE_gpencil_stroke_average_pressure_get(struct bGPDstroke *gps);
/**
 * Check if the thickness of the stroke is constant.
 */
bool BKE_gpencil_stroke_is_pressure_constant(struct bGPDstroke *gps);
