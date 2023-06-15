/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 * \brief Class to define a cell grid surrounding the projected image of a scene
 */

#include "HeuristicGridDensityProviderFactory.h"

#include "BLI_sys_types.h"

namespace Freestyle {

HeuristicGridDensityProviderFactory::HeuristicGridDensityProviderFactory(real sizeFactor,
                                                                         uint numFaces)
    : sizeFactor(sizeFactor), numFaces(numFaces)
{
}

AutoPtr<GridDensityProvider> HeuristicGridDensityProviderFactory::newGridDensityProvider(
    OccluderSource &source, const real proscenium[4])
{
  AutoPtr<AverageAreaGridDensityProvider> avg(
      new AverageAreaGridDensityProvider(source, proscenium, sizeFactor));
  AutoPtr<Pow23GridDensityProvider> p23(
      new Pow23GridDensityProvider(source, proscenium, numFaces));
  if (avg->cellSize() > p23->cellSize()) {
    return (AutoPtr<GridDensityProvider>)p23;
  }

  return (AutoPtr<GridDensityProvider>)avg;
}

AutoPtr<GridDensityProvider> HeuristicGridDensityProviderFactory::newGridDensityProvider(
    OccluderSource &source, const BBox<Vec3r> &bbox, const GridHelpers::Transform &transform)
{
  AutoPtr<AverageAreaGridDensityProvider> avg(
      new AverageAreaGridDensityProvider(source, bbox, transform, sizeFactor));
  AutoPtr<Pow23GridDensityProvider> p23(
      new Pow23GridDensityProvider(source, bbox, transform, numFaces));
  if (avg->cellSize() > p23->cellSize()) {
    return (AutoPtr<GridDensityProvider>)p23;
  }

  return (AutoPtr<GridDensityProvider>)avg;
}

AutoPtr<GridDensityProvider> HeuristicGridDensityProviderFactory::newGridDensityProvider(
    OccluderSource &source)
{
  real proscenium[4];
  GridDensityProvider::calculateOptimalProscenium(source, proscenium);
  AutoPtr<AverageAreaGridDensityProvider> avg(
      new AverageAreaGridDensityProvider(source, proscenium, sizeFactor));
  AutoPtr<Pow23GridDensityProvider> p23(
      new Pow23GridDensityProvider(source, proscenium, numFaces));
  if (avg->cellSize() > p23->cellSize()) {
    return (AutoPtr<GridDensityProvider>)p23;
  }

  return (AutoPtr<GridDensityProvider>)avg;
}

} /* namespace Freestyle */
