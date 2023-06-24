/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup freestyle
 * \brief Class to define a cell grid surrounding the projected image of a scene
 */

#include "../geometry/GridHelpers.h"

#include "../winged_edge/WEdge.h"

#ifdef WITH_CXX_GUARDEDALLOC
#  include "MEM_guardedalloc.h"
#endif

namespace Freestyle {

class OccluderSource {
  // Disallow copying and assignment
  OccluderSource(const OccluderSource &other);
  OccluderSource &operator=(const OccluderSource &other);

 public:
  OccluderSource(const GridHelpers::Transform &transform, WingedEdge &we);
  virtual ~OccluderSource();

  void begin();
  virtual bool next();
  bool isValid();

  WFace *getWFace();
  Polygon3r getCameraSpacePolygon();
  Polygon3r &getGridSpacePolygon();

  virtual void getOccluderProscenium(real proscenium[4]);
  virtual real averageOccluderArea();

 protected:
  WingedEdge &wingedEdge;
  vector<WShape *>::const_iterator currentShape, shapesEnd;
  vector<WFace *>::const_iterator currentFace, facesEnd;

  bool valid;

  Polygon3r cachedPolygon;
  const GridHelpers::Transform &transform;

  void buildCachedPolygon();

#ifdef WITH_CXX_GUARDEDALLOC
  MEM_CXX_CLASS_ALLOC_FUNCS("Freestyle:OccluderSource")
#endif
};

} /* namespace Freestyle */
