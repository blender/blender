/*
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
 */

/** \file
 * \ingroup DNA
 */

#pragma once

#include "DNA_ID.h"
#include "DNA_customdata_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * A reusable data structure for geometry consisting of many curves. All control point data is
 * stored contiguously for better efficiency. Data for each curve is stored as a slice of the
 * main #point_data array.
 *
 * The data structure is meant to be embedded in other data-blocks to allow reusing
 * curve-processing algorithms for multiple Blender data-block types.
 */
typedef struct CurvesGeometry {
  /**
   * A runtime pointer to the "position" attribute data.
   * \note This data is owned by #point_data.
   */
  float (*position)[3];
  /**
   * A runtime pointer to the "radius" attribute data.
   * \note This data is owned by #point_data.
   */
  float *radius;

  /**
   * The start index of each curve in the point data. The size of each curve can be calculated by
   * subtracting the offset from the next offset. That is valid even for the last curve because
   * this array is allocated with a length one larger than the number of splines.
   *
   * \note This is *not* stored in #CustomData because its size is one larger than #curve_data.
   */
  int *offsets;

  /**
   * All attributes stored on control points (#ATTR_DOMAIN_POINT).
   */
  CustomData point_data;

  /**
   * All attributes stored on curves (#ATTR_DOMAIN_CURVE).
   */
  CustomData curve_data;

  /**
   * The total number of control points in all curves.
   */
  int point_size;
  /**
   * The number of curves in the data-block.
   */
  int curve_size;
} CurvesGeometry;

typedef struct Curves {
  ID id;
  /* Animation data (must be immediately after id). */
  struct AnimData *adt;

  CurvesGeometry geometry;

  int flag;
  int attributes_active_index;

  /* Materials. */
  struct Material **mat;
  short totcol;
  short _pad2[3];

  /* Draw Cache. */
  void *batch_cache;
} Curves;

/* Curves.flag */
enum {
  HA_DS_EXPAND = (1 << 0),
};

/* Only one material supported currently. */
#define CURVES_MATERIAL_NR 1

#ifdef __cplusplus
}
#endif
