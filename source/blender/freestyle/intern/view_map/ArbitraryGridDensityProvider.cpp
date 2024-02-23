/* SPDX-FileCopyrightText: 2011-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 * \brief Class to define a cell grid surrounding the projected image of a scene
 */

#include "ArbitraryGridDensityProvider.h"

#include "BLI_sys_types.h"

#include "BKE_global.hh"

namespace Freestyle {

ArbitraryGridDensityProvider::ArbitraryGridDensityProvider(OccluderSource &source,
                                                           const real proscenium[4],
                                                           uint numCells)
    : GridDensityProvider(source), numCells(numCells)
{
  initialize(proscenium);
}

ArbitraryGridDensityProvider::ArbitraryGridDensityProvider(OccluderSource &source,
                                                           const BBox<Vec3r> &bbox,
                                                           const GridHelpers::Transform &transform,
                                                           uint numCells)
    : GridDensityProvider(source), numCells(numCells)
{
  real proscenium[4];
  calculateQuickProscenium(transform, bbox, proscenium);

  initialize(proscenium);
}

ArbitraryGridDensityProvider::ArbitraryGridDensityProvider(OccluderSource &source, uint numCells)
    : GridDensityProvider(source), numCells(numCells)
{
  real proscenium[4];
  calculateOptimalProscenium(source, proscenium);

  initialize(proscenium);
}

void ArbitraryGridDensityProvider::initialize(const real proscenium[4])
{
  float prosceniumWidth = (proscenium[1] - proscenium[0]);
  float prosceniumHeight = (proscenium[3] - proscenium[2]);
  real cellArea = prosceniumWidth * prosceniumHeight / numCells;
  if (G.debug & G_DEBUG_FREESTYLE) {
    cout << prosceniumWidth << " x " << prosceniumHeight << " grid with cells of area " << cellArea
         << "." << endl;
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

ArbitraryGridDensityProviderFactory::ArbitraryGridDensityProviderFactory(uint numCells)
    : numCells(numCells)
{
}

AutoPtr<GridDensityProvider> ArbitraryGridDensityProviderFactory::newGridDensityProvider(
    OccluderSource &source, const real proscenium[4])
{
  return AutoPtr<GridDensityProvider>(
      new ArbitraryGridDensityProvider(source, proscenium, numCells));
}

AutoPtr<GridDensityProvider> ArbitraryGridDensityProviderFactory::newGridDensityProvider(
    OccluderSource &source, const BBox<Vec3r> &bbox, const GridHelpers::Transform &transform)
{
  return AutoPtr<GridDensityProvider>(
      new ArbitraryGridDensityProvider(source, bbox, transform, numCells));
}

AutoPtr<GridDensityProvider> ArbitraryGridDensityProviderFactory::newGridDensityProvider(
    OccluderSource &source)
{
  return AutoPtr<GridDensityProvider>(new ArbitraryGridDensityProvider(source, numCells));
}

} /* namespace Freestyle */
