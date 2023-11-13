/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup freestyle
 * \brief Class inherited from WingedEdgeBuilder and designed to build a WX (WingedEdge + extended
 * info (silhouette etc...)) structure from a polygonal model
 */

#include "WingedEdgeBuilder.h"

#include "../scene_graph/IndexedFaceSet.h"

namespace Freestyle {

class WXEdgeBuilder : public WingedEdgeBuilder {
 public:
  WXEdgeBuilder() : WingedEdgeBuilder() {}
  virtual ~WXEdgeBuilder() {}
  VISIT_DECL(IndexedFaceSet);

 protected:
  virtual void buildWVertices(WShape &shape, const float *vertices, uint vsize);

#ifdef WITH_CXX_GUARDEDALLOC
  MEM_CXX_CLASS_ALLOC_FUNCS("Freestyle:WXEdgeBuilder")
#endif
};

} /* namespace Freestyle */
