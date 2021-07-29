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
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __BLI_CONVEXHULL2D_H__
#define __BLI_CONVEXHULL2D_H__

/** \file BLI_convexhull2d.h
 *  \ingroup bli
 */

int BLI_convexhull_2d_sorted(const float (*points)[2], const int n, int r_points[]);
int BLI_convexhull_2d(const float (*points)[2], const int n, int r_points[]);

float BLI_convexhull_aabb_fit_hull_2d(const float (*points_hull)[2], unsigned int n);
float BLI_convexhull_aabb_fit_points_2d(const float (*points)[2], unsigned int n);

#endif  /* __BLI_CONVEXHULL2D_H__ */
