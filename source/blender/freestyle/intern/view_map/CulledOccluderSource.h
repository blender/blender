/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup freestyle
 * \brief Class to define a cell grid surrounding the projected image of a scene
 */

#include "OccluderSource.h"
#include "ViewMap.h"

namespace Freestyle {

class CulledOccluderSource : public OccluderSource {
  // Disallow copying and assignment
  CulledOccluderSource(const CulledOccluderSource &other);
  CulledOccluderSource &operator=(const CulledOccluderSource &other);

 public:
  CulledOccluderSource(const GridHelpers::Transform &transform,
                       WingedEdge &we,
                       ViewMap &viewMap,
                       bool extensiveFEdgeSearch = true);

  void cullViewEdges(ViewMap &viewMap, bool extensiveFEdgeSearch);

  bool next();

  void getOccluderProscenium(real proscenium[4]);

 private:
  bool testCurrent();
  void expandGridSpaceOccluderProscenium(FEdge *fe);

  real occluderProscenium[4];
  real gridSpaceOccluderProscenium[4];

  unsigned long rejected;
  bool gridSpaceOccluderProsceniumInitialized;
};

} /* namespace Freestyle */
