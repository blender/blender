/* SPDX-FileCopyrightText: 2011-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 * \brief Class to define a cell grid surrounding the projected image of a scene
 */

#include "Pow23GridDensityProvider.h"

#include "BLI_sys_types.h"

#include "BKE_global.h"

namespace Freestyle {

Pow23GridDensityProvider::Pow23GridDensityProvider(OccluderSource &source,
                                                   const real proscenium[4],
                                                   uint numFaces)
    : GridDensityProvider(source), numFaces(numFaces)
{
  initialize(proscenium);
}

Pow23GridDensityProvider::Pow23GridDensityProvider(OccluderSource &source,
                                                   const BBox<Vec3r> &bbox,
                                                   const GridHelpers::Transform &transform,
                                                   uint numFaces)
    : GridDensityProvider(source), numFaces(numFaces)
{
  real proscenium[4];
  calculateQuickProscenium(transform, bbox, proscenium);

  initialize(proscenium);
}

Pow23GridDensityProvider::Pow23GridDensityProvider(OccluderSource &source, uint numFaces)
    : GridDensityProvider(source), numFaces(numFaces)
{
  real proscenium[4];
  calculateOptimalProscenium(source, proscenium);

  initialize(proscenium);
}

void Pow23GridDensityProvider::initialize(const real proscenium[4])
{
  float prosceniumWidth = (proscenium[1] - proscenium[0]);
  float prosceniumHeight = (proscenium[3] - proscenium[2]);
  real cellArea = prosceniumWidth * prosceniumHeight / pow(numFaces, 2.0f / 3.0f);
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
  float safetyZone = 0.1;
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

Pow23GridDensityProviderFactory::Pow23GridDensityProviderFactory(uint numFaces)
    : numFaces(numFaces)
{
}

AutoPtr<GridDensityProvider> Pow23GridDensityProviderFactory::newGridDensityProvider(
    OccluderSource &source, const real proscenium[4])
{
  return AutoPtr<GridDensityProvider>(new Pow23GridDensityProvider(source, proscenium, numFaces));
}

AutoPtr<GridDensityProvider> Pow23GridDensityProviderFactory::newGridDensityProvider(
    OccluderSource &source, const BBox<Vec3r> &bbox, const GridHelpers::Transform &transform)
{
  return AutoPtr<GridDensityProvider>(
      new Pow23GridDensityProvider(source, bbox, transform, numFaces));
}

AutoPtr<GridDensityProvider> Pow23GridDensityProviderFactory::newGridDensityProvider(
    OccluderSource &source)
{
  return AutoPtr<GridDensityProvider>(new Pow23GridDensityProvider(source, numFaces));
}

} /* namespace Freestyle */
