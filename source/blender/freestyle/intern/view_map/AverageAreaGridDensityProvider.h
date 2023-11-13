/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup freestyle
 * \brief Class to define a cell grid surrounding the projected image of a scene
 */

#include "GridDensityProvider.h"

namespace Freestyle {

class AverageAreaGridDensityProvider : public GridDensityProvider {
  // Disallow copying and assignment
  AverageAreaGridDensityProvider(const AverageAreaGridDensityProvider &other);
  AverageAreaGridDensityProvider &operator=(const AverageAreaGridDensityProvider &other);

 public:
  AverageAreaGridDensityProvider(OccluderSource &source,
                                 const real proscenium[4],
                                 real sizeFactor);
  AverageAreaGridDensityProvider(OccluderSource &source,
                                 const BBox<Vec3r> &bbox,
                                 const GridHelpers::Transform &transform,
                                 real sizeFactor);
  AverageAreaGridDensityProvider(OccluderSource &source, real sizeFactor);

 private:
  void initialize(const real proscenium[4], real sizeFactor);
};

class AverageAreaGridDensityProviderFactory : public GridDensityProviderFactory {
 public:
  AverageAreaGridDensityProviderFactory(real sizeFactor);

  AutoPtr<GridDensityProvider> newGridDensityProvider(OccluderSource &source,
                                                      const real proscenium[4]);
  AutoPtr<GridDensityProvider> newGridDensityProvider(OccluderSource &source,
                                                      const BBox<Vec3r> &bbox,
                                                      const GridHelpers::Transform &transform);
  AutoPtr<GridDensityProvider> newGridDensityProvider(OccluderSource &source);

 protected:
  real sizeFactor;
};

} /* namespace Freestyle */
