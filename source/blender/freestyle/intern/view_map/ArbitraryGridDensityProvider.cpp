//
//  Filename         : ArbitraryGridDensityProvider.cpp
//  Author(s)        : Alexander Beels
//  Purpose          : Class to define a cell grid surrounding
//                     the projected image of a scene
//  Date of creation : 2011-2-5
//
///////////////////////////////////////////////////////////////////////////////


//
//  Copyright (C) : Please refer to the COPYRIGHT file distributed 
//   with this source distribution. 
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 2
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//
///////////////////////////////////////////////////////////////////////////////

#include "ArbitraryGridDensityProvider.h"

ArbitraryGridDensityProvider::ArbitraryGridDensityProvider(OccluderSource& source, const real proscenium[4], unsigned numCells) 
	: GridDensityProvider(source), numCells(numCells)
{
	initialize (proscenium);
}

ArbitraryGridDensityProvider::ArbitraryGridDensityProvider(OccluderSource& source, const BBox<Vec3r>& bbox, const GridHelpers::Transform& transform, unsigned numCells) 
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

ArbitraryGridDensityProvider::~ArbitraryGridDensityProvider () {}

void ArbitraryGridDensityProvider::initialize (const real proscenium[4]) 
{
	float prosceniumWidth = (proscenium[1] - proscenium[0]);
	float prosceniumHeight = (proscenium[3] - proscenium[2]);
	real cellArea = prosceniumWidth * prosceniumHeight / numCells;
	cout << prosceniumWidth << " x " << prosceniumHeight << " grid with cells of area " << cellArea << "." << endl;

	_cellSize = sqrt(cellArea);
	// Now we know how many cells make each side of our grid
	_cellsX = ceil(prosceniumWidth / _cellSize);
	_cellsY = ceil(prosceniumHeight / _cellSize);
	cout << _cellsX << "x" << _cellsY << " cells of size " << _cellSize << " square." << endl;

	// Make sure the grid exceeds the proscenium by a small amount
	float safetyZone = 0.1;
	if ( _cellsX * _cellSize < prosceniumWidth * (1.0 + safetyZone) ) {
		_cellsX = prosceniumWidth * (1.0 + safetyZone) / _cellSize;
	}
	if ( _cellsY * _cellSize < prosceniumHeight * (1.0 + safetyZone) ) {
		_cellsY = prosceniumHeight * (1.0 + safetyZone) / _cellSize;
	}
	cout << _cellsX << "x" << _cellsY << " cells of size " << _cellSize << " square." << endl;

	// Find grid origin
	_cellOrigin[0] = ((proscenium[0] + proscenium[1]) / 2.0) - (_cellsX / 2.0) * _cellSize;
	_cellOrigin[1] = ((proscenium[2] + proscenium[3]) / 2.0) - (_cellsY / 2.0) * _cellSize;
}

ArbitraryGridDensityProviderFactory::ArbitraryGridDensityProviderFactory(unsigned numCells)
	: numCells(numCells)
{
}

ArbitraryGridDensityProviderFactory::~ArbitraryGridDensityProviderFactory () {}

auto_ptr<GridDensityProvider> ArbitraryGridDensityProviderFactory::newGridDensityProvider(OccluderSource& source, const real proscenium[4]) 
{
	return auto_ptr<GridDensityProvider>(new ArbitraryGridDensityProvider(source, proscenium, numCells));
}

auto_ptr<GridDensityProvider> ArbitraryGridDensityProviderFactory::newGridDensityProvider(OccluderSource& source, const BBox<Vec3r>& bbox, const GridHelpers::Transform& transform) 
{
	return auto_ptr<GridDensityProvider>(new ArbitraryGridDensityProvider(source, bbox, transform, numCells));
}

auto_ptr<GridDensityProvider> ArbitraryGridDensityProviderFactory::newGridDensityProvider(OccluderSource& source) 
{
	return auto_ptr<GridDensityProvider>(new ArbitraryGridDensityProvider(source, numCells));
}

