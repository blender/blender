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
