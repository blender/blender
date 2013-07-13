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

/** \file blender/freestyle/intern/view_map/ArbitraryGridDensityProvider.cpp
 *  \ingroup freestyle
 *  \brief Class to define a cell grid surrounding the projected image of a scene
 *  \author Alexander Beels
 *  \date 2011-2-5
 */

#include "ArbitraryGridDensityProvider.h"

#include "BKE_global.h"

namespace Freestyle {

ArbitraryGridDensityProvider::ArbitraryGridDensityProvider(OccluderSource& source, const real proscenium[4],
                                                           unsigned numCells)
: GridDensityProvider(source), numCells(numCells)
{
	initialize (proscenium);
}

ArbitraryGridDensityProvider::ArbitraryGridDensityProvider(OccluderSource& source, const BBox<Vec3r>& bbox,
                                                           const GridHelpers::Transform& transform, unsigned numCells)
: GridDensityProvider(source), numCells(numCells)
{
	real proscenium[4];
	calculateQuickProscenium(transform, bbox, proscenium);

	initialize (proscenium);
}

ArbitraryGridDensityProvider::ArbitraryGridDensityProvider(OccluderSource& source, unsigned numCells)
: GridDensityProvider(source), numCells(numCells) 
{
	real proscenium[4];
	calculateOptimalProscenium(source, proscenium);

	initialize (proscenium);
}

ArbitraryGridDensityProvider::~ArbitraryGridDensityProvider() {}

void ArbitraryGridDensityProvider::initialize(const real proscenium[4])
{
	float prosceniumWidth = (proscenium[1] - proscenium[0]);
	float prosceniumHeight = (proscenium[3] - proscenium[2]);
	real cellArea = prosceniumWidth * prosceniumHeight / numCells;
	if (G.debug & G_DEBUG_FREESTYLE) {
		cout << prosceniumWidth << " x " << prosceniumHeight << " grid with cells of area " << cellArea << "." << endl;
	}

	_cellSize = sqrt(cellArea);
	// Now we know how many cells make each side of our grid
	_cellsX = ceil(prosceniumWidth / _cellSize);
	_cellsY = ceil(prosceniumHeight / _cellSize);
	if (G.debug & G_DEBUG_FREESTYLE) {
		cout << _cellsX << "x" << _cellsY << " cells of size " << _cellSize << " square." << endl;
	}

	// Make sure the grid exceeds the proscenium by a small amount
	float safetyZone = 0.1f;
	if (_cellsX * _cellSize < prosceniumWidth * (1.0 + safetyZone)) {
		_cellsX = ceil(prosceniumWidth * (1.0 + safetyZone) / _cellSize);
	}
	if (_cellsY * _cellSize < prosceniumHeight * (1.0 + safetyZone)) {
		_cellsY = ceil(prosceniumHeight * (1.0 + safetyZone) / _cellSize);
	}
	if (G.debug & G_DEBUG_FREESTYLE) {
		cout << _cellsX << "x" << _cellsY << " cells of size " << _cellSize << " square." << endl;
	}

	// Find grid origin
	_cellOrigin[0] = ((proscenium[0] + proscenium[1]) / 2.0) - (_cellsX / 2.0) * _cellSize;
	_cellOrigin[1] = ((proscenium[2] + proscenium[3]) / 2.0) - (_cellsY / 2.0) * _cellSize;
}

ArbitraryGridDensityProviderFactory::ArbitraryGridDensityProviderFactory(unsigned numCells)
: numCells(numCells)
{
}

ArbitraryGridDensityProviderFactory::~ArbitraryGridDensityProviderFactory() {}

auto_ptr<GridDensityProvider> ArbitraryGridDensityProviderFactory::newGridDensityProvider(OccluderSource& source,
                                                                                          const real proscenium[4])
{
	return auto_ptr<GridDensityProvider>(new ArbitraryGridDensityProvider(source, proscenium, numCells));
}

auto_ptr<GridDensityProvider>
ArbitraryGridDensityProviderFactory::newGridDensityProvider(OccluderSource& source, const BBox<Vec3r>& bbox,
                                                            const GridHelpers::Transform& transform)
{
	return auto_ptr<GridDensityProvider>(new ArbitraryGridDensityProvider(source, bbox, transform, numCells));
}

auto_ptr<GridDensityProvider> ArbitraryGridDensityProviderFactory::newGridDensityProvider(OccluderSource& source)
{
	return auto_ptr<GridDensityProvider>(new ArbitraryGridDensityProvider(source, numCells));
}

} /* namespace Freestyle */
