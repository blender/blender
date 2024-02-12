/* SPDX-FileCopyrightText: 2011-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 * \brief Class to define a cell grid surrounding the projected image of a scene
 */

#include "AverageAreaGridDensityProvider.h"

#include "BLI_sys_types.h"

#include "BKE_global.hh"

namespace Freestyle {

AverageAreaGridDensityProvider::AverageAreaGridDensityProvider(OccluderSource &source,
                                                               const real proscenium[4],
                                                               real sizeFactor)
    : GridDensityProvider(source)
{
  initialize(proscenium, sizeFactor);
}

AverageAreaGridDensityProvider::AverageAreaGridDensityProvider(
    OccluderSource &source,
    const BBox<Vec3r> &bbox,
    const GridHelpers::Transform &transform,
    real sizeFactor)
    : GridDensityProvider(source)
{
  real proscenium[4];
  calculateQuickProscenium(transform, bbox, proscenium);

  initialize(proscenium, sizeFactor);
}

AverageAreaGridDensityProvider::AverageAreaGridDensityProvider(OccluderSource &source,
                                                               real sizeFactor)
    : GridDensityProvider(source)
{
  real proscenium[4];
  calculateOptimalProscenium(source, proscenium);

  initialize(proscenium, sizeFactor);
}

void AverageAreaGridDensityProvider::initialize(const real proscenium[4], real sizeFactor)
{
  float prosceniumWidth = (proscenium[1] - proscenium[0]);
  float prosceniumHeight = (proscenium[3] - proscenium[2]);

  real cellArea = 0.0;
  uint numFaces = 0;
  for (source.begin(); source.isValid(); source.next()) {
    Polygon3r &poly(source.getGridSpacePolygon());
    Vec3r min, max;
    poly.getBBox(min, max);
    cellArea += (max[0] - min[0]) * (max[1] - min[1]);
    ++numFaces;
  }
  if (G.debug & G_DEBUG_FREESTYLE) {
    cout << "Total area: " << cellArea << ". Number of faces: " << numFaces << "." << endl;
  }
  cellArea /= numFaces;
  cellArea *= sizeFactor;
  if (G.debug & G_DEBUG_FREESTYLE) {
    cout << "Building grid with average area " << cellArea << endl;
  }

  _cellSize = sqrt(cellArea);
  uint maxCells = 931;  // * 1.1 = 1024
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

AutoPtr<GridDensityProvider> AverageAreaGridDensityProviderFactory::newGridDensityProvider(
    OccluderSource &source, const real proscenium[4])
{
  return AutoPtr<GridDensityProvider>(
      new AverageAreaGridDensityProvider(source, proscenium, sizeFactor));
}

AutoPtr<GridDensityProvider> AverageAreaGridDensityProviderFactory::newGridDensityProvider(
    OccluderSource &source, const BBox<Vec3r> &bbox, const GridHelpers::Transform &transform)
{
  return AutoPtr<GridDensityProvider>(
      new AverageAreaGridDensityProvider(source, bbox, transform, sizeFactor));
}

AutoPtr<GridDensityProvider> AverageAreaGridDensityProviderFactory::newGridDensityProvider(
    OccluderSource &source)
{
  return AutoPtr<GridDensityProvider>(new AverageAreaGridDensityProvider(source, sizeFactor));
}

} /* namespace Freestyle */
