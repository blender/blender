/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup freestyle
 * \brief Class to define a cell grid surrounding the projected image of a scene
 */

//#include <memory> // provided by GridDensityProvider.h

#include "AverageAreaGridDensityProvider.h"
//#include "GridDensityProvider.h" // provided by *GridDensityProvider.h below
#include "Pow23GridDensityProvider.h"

namespace Freestyle {

class HeuristicGridDensityProviderFactory : public GridDensityProviderFactory {
 public:
  HeuristicGridDensityProviderFactory(real sizeFactor, uint numFaces);

  AutoPtr<GridDensityProvider> newGridDensityProvider(OccluderSource &source,
                                                      const real proscenium[4]);
  AutoPtr<GridDensityProvider> newGridDensityProvider(OccluderSource &source,
                                                      const BBox<Vec3r> &bbox,
                                                      const GridHelpers::Transform &transform);
  AutoPtr<GridDensityProvider> newGridDensityProvider(OccluderSource &source);

 protected:
  real sizeFactor;
  uint numFaces;
};

} /* namespace Freestyle */
