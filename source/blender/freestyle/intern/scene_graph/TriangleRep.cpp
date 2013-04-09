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

/** \file blender/freestyle/intern/scene_graph/TriangleRep.cpp
 *  \ingroup freestyle
 *  \brief Class to define the represenation of a triangle
 *  \author Stephane Grabli
 *  \date 16/12/2002
 */

#include "TriangleRep.h"

namespace Freestyle {

void TriangleRep::ComputeBBox()
{
	real XMax = _vertices[0][0];
	real YMax = _vertices[0][1];
	real ZMax = _vertices[0][2];

	real XMin = _vertices[0][0];
	real YMin = _vertices[0][1];
	real ZMin = _vertices[0][2];

	// parse all the coordinates to find the XMax, YMax, ZMax
	for (int i = 0; i < 3; ++i) {
		// X
		if (_vertices[i][0] > XMax)
			XMax = _vertices[i][0];
		if (_vertices[i][0] < XMin)
			XMin = _vertices[i][0];

		// Y
		if (_vertices[i][1] > YMax)
			YMax = _vertices[i][1];
		if (_vertices[i][1] < YMin)
			YMin = _vertices[i][1];

		// Z
		if (_vertices[i][2] > ZMax)
			ZMax = _vertices[i][2];
		if (_vertices[i][2] < ZMin)
			ZMin = _vertices[i][2];
	}

	setBBox(BBox<Vec3r>(Vec3r(XMin, YMin, ZMin), Vec3r(XMax, YMax, ZMax)));
}

} /* namespace Freestyle */
