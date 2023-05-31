/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bli
 */

#ifdef __cplusplus
extern "C" {
#endif

struct rcti;

void BLI_lasso_boundbox(struct rcti *rect, const int mcoords[][2], unsigned int mcoords_len);
bool BLI_lasso_is_point_inside(
    const int mcoords[][2], unsigned int mcoords_len, int sx, int sy, int error_value);
/**
 * Edge version for lasso select. We assume bound-box check was done.
 */
bool BLI_lasso_is_edge_inside(const int mcoords[][2],
                              unsigned int mcoords_len,
                              int x0,
                              int y0,
                              int x1,
                              int y1,
                              int error_value);

#ifdef __cplusplus
}
#endif
