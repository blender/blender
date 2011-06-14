/*
 * $Id$
 *
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
 * Contributor(s): Blender Foundation,
 *                 Sergey Sharybin
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef LIBMV_C_API_H
#define LIBMV_C_API_H

#ifdef __cplusplus
extern "C" {
#endif

typedef void *libmv_regionTrackerHandle;

/* Regiontracker */
libmv_regionTrackerHandle libmv_regionTrackerNew(int max_iterations, int pyramid_level, double tolerance);
int libmv_regionTrackerTrack(libmv_regionTrackerHandle tracker, const float *ima1, const float *ima2,
			int width, int height, int half_window_size,
			double  x1, double  y1, double *x2, double *y2);
void libmv_regionTrackerDestroy(libmv_regionTrackerHandle tracker);

#ifdef __cplusplus
}
#endif

#endif // LIBMV_C_API_H
