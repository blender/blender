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

class ArbitraryGridDensityProvider : public GridDensityProvider {
  // Disallow copying and assignment
  ArbitraryGridDensityProvider(const ArbitraryGridDensityProvider &other);
  ArbitraryGridDensityProvider &operator=(const ArbitraryGridDensityProvider &other);

 public:
  ArbitraryGridDensityProvider(OccluderSource &source, const real proscenium[4], uint numCells);
  ArbitraryGridDensityProvider(OccluderSource &source,
                               const BBox<Vec3r> &bbox,
                               const GridHelpers::Transform &transform,
                               uint numCells);
  ArbitraryGridDensityProvider(OccluderSource &source, uint numCells);

 protected:
  uint numCells;

 private:
  void initialize(const real proscenium[4]);
};

class ArbitraryGridDensityProviderFactory : public GridDensityProviderFactory {
 public:
  ArbitraryGridDensityProviderFactory(uint numCells);

  AutoPtr<GridDensityProvider> newGridDensityProvider(OccluderSource &source,
                                                      const real proscenium[4]);
  AutoPtr<GridDensityProvider> newGridDensityProvider(OccluderSource &source,
                                                      const BBox<Vec3r> &bbox,
                                                      const GridHelpers::Transform &transform);
  AutoPtr<GridDensityProvider> newGridDensityProvider(OccluderSource &source);

 protected:
  uint numCells;
};

} /* namespace Freestyle */
