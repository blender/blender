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

/** \file blender/freestyle/intern/geometry/FastGrid.cpp
 *  \ingroup freestyle
 *  \brief Class to define a cell grid surrounding the bounding box of the scene
 *  \author Stephane Grabli
 *  \date 30/07/2002
 */

#include "FastGrid.h"

#include "BKE_global.h"

namespace Freestyle {

void FastGrid::clear()
{
	if (!_cells)
		return;

	for (unsigned int i = 0; i < _cells_size; i++) {
		if (_cells[i])
			delete _cells[i];
	}
	delete[] _cells;
	_cells = NULL;
	_cells_size = 0;

	Grid::clear();
}

void FastGrid::configure(const Vec3r& orig, const Vec3r& size, unsigned nb)
{
	Grid::configure(orig, size, nb);
	_cells_size = _cells_nb[0] * _cells_nb[1] * _cells_nb[2];
	_cells = new Cell *[_cells_size];
	memset(_cells, 0, _cells_size * sizeof(*_cells));
}

Cell *FastGrid::getCell(const Vec3u& p)
{
#if 0
	if (G.debug & G_DEBUG_FREESTYLE) {
		cout << _cells << " " << p << " " << _cells_nb[0] << "-" << _cells_nb[1] << "-" << _cells_nb[2]
		     << " " << _cells_size << endl;
	}
#endif
	assert(_cells || ("_cells is a null pointer"));
	assert((_cells_nb[0] * (p[2] * _cells_nb[1] + p[1]) + p[0]) < _cells_size);
	assert(p[0] < _cells_nb[0]);
	assert(p[1] < _cells_nb[1]);
	assert(p[2] < _cells_nb[2]);
	return _cells[_cells_nb[0] * (p[2] * _cells_nb[1] + p[1]) + p[0]];
}

void FastGrid::fillCell(const Vec3u& p, Cell& cell)
{
	assert(_cells || ("_cells is a null pointer"));
	assert((_cells_nb[0] * (p[2] * _cells_nb[1] + p[1]) + p[0]) < _cells_size);
	assert(p[0] < _cells_nb[0]);
	assert(p[1] < _cells_nb[1]);
	assert(p[2] < _cells_nb[2]);
	_cells[_cells_nb[0] * (p[2] * _cells_nb[1] + p[1]) + p[0]] = &cell;
}

} /* namespace Freestyle */
