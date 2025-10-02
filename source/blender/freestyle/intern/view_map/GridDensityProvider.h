/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup freestyle
 * \brief Class to define a cell grid surrounding the projected image of a scene
 */

#include <algorithm>
#include <memory>
#include <stdexcept>

#include "AutoPtrHelper.h"
#include "OccluderSource.h"

#include "../geometry/BBox.h"

#include "BKE_global.hh"

#include "MEM_guardedalloc.h"

namespace Freestyle {

class GridDensityProvider {
  // Disallow copying and assignment
  GridDensityProvider(const GridDensityProvider &other);
  GridDensityProvider &operator=(const GridDensityProvider &other);

 public:
  GridDensityProvider(OccluderSource &source) : source(source) {}

  virtual ~GridDensityProvider() {};

  float cellSize()
  {
    return _cellSize;
  }

  uint cellsX()
  {
    return _cellsX;
  }

  uint cellsY()
  {
    return _cellsY;
  }

  float cellOrigin(int index)
  {
    if (index < 2) {
      return _cellOrigin[index];
    }
    else {
      throw new out_of_range("GridDensityProvider::cellOrigin can take only indexes of 0 or 1.");
    }
  }

  static void calculateOptimalProscenium(OccluderSource &source, real proscenium[4])
  {
    source.begin();
    if (source.isValid()) {
      const Vec3r &initialPoint = source.getGridSpacePolygon().getVertices()[0];
      proscenium[0] = proscenium[1] = initialPoint[0];
      proscenium[2] = proscenium[3] = initialPoint[1];
      while (source.isValid()) {
        GridHelpers::expandProscenium(proscenium, source.getGridSpacePolygon());
        source.next();
      }
    }
    if (G.debug & G_DEBUG_FREESTYLE) {
      cout << "Proscenium: (" << proscenium[0] << ", " << proscenium[1] << ", " << proscenium[2]
           << ", " << proscenium[3] << ")" << endl;
    }
  }

  static void calculateQuickProscenium(const GridHelpers::Transform &transform,
                                       const BBox<Vec3r> &bbox,
                                       real proscenium[4])
  {
    // Transform the coordinates of the 8 corners of the 3D bounding box
    real xm = bbox.getMin()[0], xM = bbox.getMax()[0];
    real ym = bbox.getMin()[1], yM = bbox.getMax()[1];
    real zm = bbox.getMin()[2], zM = bbox.getMax()[2];
    Vec3r p1 = transform(Vec3r(xm, ym, zm));
    Vec3r p2 = transform(Vec3r(xm, ym, zM));
    Vec3r p3 = transform(Vec3r(xm, yM, zm));
    Vec3r p4 = transform(Vec3r(xm, yM, zM));
    Vec3r p5 = transform(Vec3r(xM, ym, zm));
    Vec3r p6 = transform(Vec3r(xM, ym, zM));
    Vec3r p7 = transform(Vec3r(xM, yM, zm));
    Vec3r p8 = transform(Vec3r(xM, yM, zM));
    // Determine the proscenium face according to the min and max values of the transformed x and y
    // coordinates
    proscenium[0] = std::min(std::min(std::min(p1.x(), p2.x()), std::min(p3.x(), p4.x())),
                             std::min(std::min(p5.x(), p6.x()), std::min(p7.x(), p8.x())));
    proscenium[1] = std::max(std::max(std::max(p1.x(), p2.x()), std::max(p3.x(), p4.x())),
                             std::max(std::max(p5.x(), p6.x()), std::max(p7.x(), p8.x())));
    proscenium[2] = std::min(std::min(std::min(p1.y(), p2.y()), std::min(p3.y(), p4.y())),
                             std::min(std::min(p5.y(), p6.y()), std::min(p7.y(), p8.y())));
    proscenium[3] = std::max(std::max(std::max(p1.y(), p2.y()), std::max(p3.y(), p4.y())),
                             std::max(std::max(p5.y(), p6.y()), std::max(p7.y(), p8.y())));
    if (G.debug & G_DEBUG_FREESTYLE) {
      cout << "Proscenium: " << proscenium[0] << ", " << proscenium[1] << ", " << proscenium[2]
           << ", " << proscenium[3] << endl;
    }
  }

 protected:
  OccluderSource &source;
  uint _cellsX, _cellsY;
  float _cellSize;
  float _cellOrigin[2];

  MEM_CXX_CLASS_ALLOC_FUNCS("Freestyle:GridDensityProvider")
};

class GridDensityProviderFactory {
  // Disallow copying and assignment
  GridDensityProviderFactory(const GridDensityProviderFactory &other);
  GridDensityProviderFactory &operator=(const GridDensityProviderFactory &other);

 public:
  GridDensityProviderFactory() {}

  virtual AutoPtr<GridDensityProvider> newGridDensityProvider(OccluderSource &source,
                                                              const real proscenium[4]) = 0;

  virtual AutoPtr<GridDensityProvider> newGridDensityProvider(
      OccluderSource &source,
      const BBox<Vec3r> &bbox,
      const GridHelpers::Transform &transform) = 0;

  virtual AutoPtr<GridDensityProvider> newGridDensityProvider(OccluderSource &source) = 0;

  virtual ~GridDensityProviderFactory() {}

  MEM_CXX_CLASS_ALLOC_FUNCS("Freestyle:GridDensityProviderFactory")
};

} /* namespace Freestyle */
