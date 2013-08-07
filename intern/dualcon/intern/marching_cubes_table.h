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
 * Contributor(s): Tao Ju
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __MARCHING_CUBES_TABLE_H__
#define __MARCHING_CUBES_TABLE_H__

/* number of configurations */
#define TOTCONF 256

/* maximum number of triangles per configuration */
#define MAX_TRIS 10

/* number of triangles in each configuration */
extern const int marching_cubes_numtri[TOTCONF];

/* table of triangles in each configuration */
extern const int marching_cubes_tris[TOTCONF][MAX_TRIS][3];

#endif
