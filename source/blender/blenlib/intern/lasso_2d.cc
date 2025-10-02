/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bli
 */

#include "DNA_vec_types.h"

#include "BLI_math_base.h"
#include "BLI_math_geom.h"
#include "BLI_sys_types.h"

#include "BLI_lasso_2d.hh" /* own include */

#include "BLI_strict_flags.h" /* IWYU pragma: keep. Keep last. */

using blender::int2;
using blender::Span;

void BLI_lasso_boundbox(rcti *rect, const Span<int2> mcoords)
{
  rect->xmin = rect->xmax = mcoords[0][0];
  rect->ymin = rect->ymax = mcoords[0][1];

  for (const int64_t a : mcoords.index_range().drop_front(1)) {
    if (mcoords[a][0] < rect->xmin) {
      rect->xmin = mcoords[a][0];
    }
    else if (mcoords[a][0] > rect->xmax) {
      rect->xmax = mcoords[a][0];
    }
    if (mcoords[a][1] < rect->ymin) {
      rect->ymin = mcoords[a][1];
    }
    else if (mcoords[a][1] > rect->ymax) {
      rect->ymax = mcoords[a][1];
    }
  }
}

bool BLI_lasso_is_point_inside(const Span<int2> mcoords,
                               const int sx,
                               const int sy,
                               const int error_value)
{
  if (sx == error_value || mcoords.is_empty()) {
    return false;
  }

  const int pt[2] = {sx, sy};
  return isect_point_poly_v2_int(
      pt, reinterpret_cast<const int (*)[2]>(mcoords.data()), uint(mcoords.size()));
}

bool BLI_lasso_is_edge_inside(
    const Span<int2> mcoords, int x0, int y0, int x1, int y1, const int error_value)
{

  if (x0 == error_value || x1 == error_value || mcoords.is_empty()) {
    return false;
  }

  const int v1[2] = {x0, y0}, v2[2] = {x1, y1};

  /* check points in lasso */
  if (BLI_lasso_is_point_inside(mcoords, v1[0], v1[1], error_value)) {
    return true;
  }
  if (BLI_lasso_is_point_inside(mcoords, v2[0], v2[1], error_value)) {
    return true;
  }

  /* no points in lasso, so we have to intersect with lasso edge */

  if (isect_seg_seg_v2_int(mcoords[0], mcoords.last(), v1, v2) > 0) {
    return true;
  }
  for (const int64_t i : mcoords.index_range().drop_back(1)) {
    if (isect_seg_seg_v2_int(mcoords[i], mcoords[i + 1], v1, v2) > 0) {
      return true;
    }
  }

  return false;
}
