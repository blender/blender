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

/** \file blender/freestyle/intern/view_map/AverageAreaGridDensityProvider.cpp
 *  \ingroup freestyle
 *  \brief Class to define a cell grid surrounding the projected image of a scene
 *  \author Alexander Beels
 *  \date 2011-2-9
 */

#include "AverageAreaGridDensityProvider.h"

#include "BKE_global.h"

namespace Freestyle {

AverageAreaGridDensityProvider::AverageAreaGridDensityProvider(OccluderSource& source, const real proscenium[4],
                                                               real sizeFactor)
: GridDensityProvider(source)
{
	initialize (proscenium, sizeFactor);
}

AverageAreaGridDensityProvider::AverageAreaGridDensityProvider(OccluderSource& source, const BBox<Vec3r>& bbox,
                                                               const GridHelpers::Transform& transform, real sizeFactor)
: GridDensityProvider(source)
{
	real proscenium[4];
	calculateQuickProscenium(transform, bbox, proscenium);

	initialize(proscenium, sizeFactor);
}

AverageAreaGridDensityProvider::AverageAreaGridDensityProvider(OccluderSource& source, real sizeFactor)
: GridDensityProvider(source)
{
	real proscenium[4];
	calculateOptimalProscenium(source, proscenium);

	initialize(proscenium, sizeFactor);
}

AverageAreaGridDensityProvider::~AverageAreaGridDensityProvider() {}

void AverageAreaGridDensityProvider::initialize(const real proscenium[4], real sizeFactor)
{
	float prosceniumWidth = (proscenium[1] - proscenium[0]);
	float prosceniumHeight = (proscenium[3] - proscenium[2]);

	real cellArea = 0.0;
	unsigned numFaces = 0;
	for (source.begin(); source.isValid(); source.next()) {
		Polygon3r& poly(source.getGridSpacePolygon());
		Vec3r min, max;
		poly.getBBox(min, max);
		cellArea += (max[0] - min[0]) * (max[1] - min[1]);
		++numFaces;
	}
	if (G.debug & G_DEBUG_FREESTYLE) {
		cout << "Total area: " << cellArea << ".  Number of faces: " << numFaces << "." << endl;
	}
	cellArea /= numFaces;
	cellArea *= sizeFactor;
	if (G.debug & G_DEBUG_FREESTYLE) {
		cout << "Building grid with average area " << cellArea << endl;
	}

	_cellSize = sqrt(cellArea);
	unsigned maxCells = 931; // * 1.1 = 1024
	if (std::max(prosceniumWidth, prosceniumHeight) / _cellSize > maxCells) {
		if (G.debug & G_DEBUG_FREESTYLE) {
			cout << "Scene-dependent cell size (" << _cellSize << " square) is too small." << endl;
		}
		_cellSize = std::max(prosceniumWidth, prosceniumHeight) / maxCells;
	}
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

AverageAreaGridDensityProviderFactory::AverageAreaGridDensityProviderFactory(real sizeFactor)
: sizeFactor(sizeFactor)
{
}

AverageAreaGridDensityProviderFactory::~AverageAreaGridDensityProviderFactory() {}

auto_ptr<GridDensityProvider>
AverageAreaGridDensityProviderFactory::newGridDensityProvider(OccluderSource& source, const real proscenium[4])
{
	return auto_ptr<GridDensityProvider>(new AverageAreaGridDensityProvider(source, proscenium, sizeFactor));
}

auto_ptr<GridDensityProvider>
AverageAreaGridDensityProviderFactory::newGridDensityProvider(OccluderSource& source, const BBox<Vec3r>& bbox,
                                                              const GridHelpers::Transform& transform)
{
	return auto_ptr<GridDensityProvider>(new AverageAreaGridDensityProvider(source, bbox, transform, sizeFactor));
}

auto_ptr<GridDensityProvider> AverageAreaGridDensityProviderFactory::newGridDensityProvider(OccluderSource& source)
{
	return auto_ptr<GridDensityProvider>(new AverageAreaGridDensityProvider(source, sizeFactor));
}

} /* namespace Freestyle */
