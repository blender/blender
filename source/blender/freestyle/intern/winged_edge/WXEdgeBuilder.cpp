/* SPDX-FileCopyrightText: 2008-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 * \brief Class inherited from WingedEdgeBuilder and designed to build a WX (WingedEdge + extended
 * info (silhouette etc...)) structure from a polygonal model
 */

#include "WXEdgeBuilder.h"
#include "WXEdge.h"

#include "BLI_sys_types.h"

namespace Freestyle {

void WXEdgeBuilder::visitIndexedFaceSet(IndexedFaceSet &ifs)
{
  if (_pRenderMonitor && _pRenderMonitor->testBreak()) {
    return;
  }
  WXShape *shape = new WXShape;
  if (!buildWShape(*shape, ifs)) {
    delete shape;
    return;
  }
  shape->setId(ifs.getId().getFirst());
  shape->setName(ifs.getName());
  shape->setLibraryPath(ifs.getLibraryPath());
  // ifs.setId(shape->GetId());
}

void WXEdgeBuilder::buildWVertices(WShape &shape, const float *vertices, uint vsize)
{
  WXVertex *vertex;
  for (uint i = 0; i < vsize; i += 3) {
    vertex = new WXVertex(Vec3f(vertices[i], vertices[i + 1], vertices[i + 2]));
    vertex->setId(i / 3);
    shape.AddVertex(vertex);
  }
}

} /* namespace Freestyle */
