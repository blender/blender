/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bli
 */

#include "BLI_math_vector_types.hh"
#include "BLI_span.hh"

struct rcti;

void BLI_lasso_boundbox(rcti *rect, blender::Span<blender::int2> mcoords);
bool BLI_lasso_is_point_inside(blender::Span<blender::int2> mcoords,
                               int sx,
                               int sy,
                               int error_value);
/**
 * Edge version for lasso select. We assume bound-box check was done.
 */
bool BLI_lasso_is_edge_inside(
    blender::Span<blender::int2> mcoords, int x0, int y0, int x1, int y1, int error_value);
