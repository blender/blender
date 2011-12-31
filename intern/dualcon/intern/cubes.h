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

#ifndef CUBES_H
#define CUBES_H

#include "marching_cubes_table.h"

/* simple wrapper for auto-generated marching cubes data */
class Cubes
{
public:
	/// Get number of triangles
	int getNumTriangle(int mask)
	{
		return marching_cubes_numtri[mask];
	}

	/// Get a triangle
	void getTriangle(int mask, int index, int indices[3] )
	{
		for(int i = 0; i < 3; i++)
			indices[i] = marching_cubes_tris[mask][index][i];
	}
};

#endif
