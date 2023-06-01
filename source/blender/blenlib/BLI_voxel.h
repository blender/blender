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

/** Calculate the index number of a voxel, given x/y/z integer coords and resolution vector. */
#define BLI_VOXEL_INDEX(x, y, z, res) \
  ((int64_t)(x) + (int64_t)(y) * (int64_t)(res)[0] + \
   (int64_t)(z) * (int64_t)(res)[0] * (int64_t)(res)[1])

/* All input coordinates must be in bounding box 0.0 - 1.0. */

float BLI_voxel_sample_nearest(const float *data, const int res[3], const float co[3]);
float BLI_voxel_sample_trilinear(const float *data, const int res[3], const float co[3]);
float BLI_voxel_sample_triquadratic(const float *data, const int res[3], const float co[3]);
float BLI_voxel_sample_tricubic(const float *data,
                                const int res[3],
                                const float co[3],
                                int bspline);

#ifdef __cplusplus
}
#endif
