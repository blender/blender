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
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 */

#ifndef __BLI_VOXEL_H__
#define __BLI_VOXEL_H__

/** \file \ingroup bli
 */

/** find the index number of a voxel, given x/y/z integer coords and resolution vector */

#define BLI_VOXEL_INDEX(x, y, z, res) \
	((int64_t)(x) + \
	 (int64_t)(y) * (int64_t)(res)[0] + \
	 (int64_t)(z) * (int64_t)(res)[0] * (int64_t)(res)[1])

/* all input coordinates must be in bounding box 0.0 - 1.0 */
float BLI_voxel_sample_nearest(float *data, const int res[3], const float co[3]);
float BLI_voxel_sample_trilinear(float *data, const int res[3], const float co[3]);
float BLI_voxel_sample_triquadratic(float *data, const int res[3], const float co[3]);
float BLI_voxel_sample_tricubic(float *data, const int res[3], const float co[3], int bspline);

#endif /* __BLI_VOXEL_H__ */
