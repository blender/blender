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

/** \file blender/freestyle/intern/geometry/HashGrid.cpp
 *  \ingroup freestyle
 *  \brief Class to define a cell grid surrounding the bounding box of the scene
 *  \author Stephane Grabli
 *  \date 30/07/2002
 */

#include "HashGrid.h"

namespace Freestyle {

void HashGrid::clear()
{
	if (!_cells.empty()) {
		for (GridHashTable::iterator it = _cells.begin(); it != _cells.end(); it++) {
			Cell *cell = (*it).second;
			delete cell;
		}
		_cells.clear();
	}

	Grid::clear();
}

void HashGrid::configure(const Vec3r& orig, const Vec3r& size, unsigned nb)
{
	Grid::configure(orig, size, nb);
}

} /* namespace Freestyle */
