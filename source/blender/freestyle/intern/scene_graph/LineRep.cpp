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

/** \file blender/freestyle/intern/scene_graph/LineRep.cpp
 *  \ingroup freestyle
 *  \brief Class to define the representation of 3D Line.
 *  \author Stephane Grabli
 *  \date 26/03/2002
 */

#include "LineRep.h"

namespace Freestyle {

void LineRep::ComputeBBox()
{
	real XMax = _vertices.front()[0];
	real YMax = _vertices.front()[1];
	real ZMax = _vertices.front()[2];

	real XMin = _vertices.front()[0];
	real YMin = _vertices.front()[1];
	real ZMin = _vertices.front()[2];

	// parse all the coordinates to find 
	// the XMax, YMax, ZMax
	vector<Vec3r>::iterator v;
	for (v = _vertices.begin(); v != _vertices.end(); ++v) {
		// X
		if ((*v)[0] > XMax)
			XMax = (*v)[0];
		if ((*v)[0] < XMin)
			XMin = (*v)[0];

		// Y
		if ((*v)[1] > YMax)
			YMax = (*v)[1];
		if ((*v)[1] < YMin)
			YMin = (*v)[1];

		// Z
		if ((*v)[2] > ZMax)
			ZMax = (*v)[2];
		if ((*v)[2] < ZMin)
			ZMin = (*v)[2];
	}

	setBBox(BBox<Vec3r>(Vec3r(XMin, YMin, ZMin), Vec3r(XMax, YMax, ZMax)));
}

} /* namespace Freestyle */
