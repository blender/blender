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

struct Object;
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
