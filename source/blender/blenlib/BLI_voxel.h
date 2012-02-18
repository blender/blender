/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
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
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Matt Ebb, Raul Fernandez Hernandez (Farsthary).
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __BLI_VOXEL_H__
#define __BLI_VOXEL_H__

/** \file BLI_voxel.h
 *  \ingroup bli
 */

/** find the index number of a voxel, given x/y/z integer coords and resolution vector */
#define V_I(x, y, z, res) ( (z)*(res)[1]*(res)[0] + (y)*(res)[0] + (x) )

/* all input coordinates must be in bounding box 0.0 - 1.0 */
float voxel_sample_nearest(float *data, const int res[3], const float co[3]);
float voxel_sample_trilinear(float *data, const int res[3], const float co[3]);
float voxel_sample_triquadratic(float *data, const int res[3], const float co[3]);
float voxel_sample_tricubic(float *data, const int res[3], const float co[3], int bspline);

#endif /* __BLI_VOXEL_H__ */
