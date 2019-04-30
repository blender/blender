/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

/** \file
 * \ingroup freestyle
 * \brief Class inherited from WingedEdgeBuilder and designed to build a WX (WingedEdge + extended
 * info (silhouette etc...)) structure from a polygonal model
 */

#include "WXEdge.h"
#include "WXEdgeBuilder.h"

namespace Freestyle {

void WXEdgeBuilder::visitIndexedFaceSet(IndexedFaceSet &ifs)
{
  if (_pRenderMonitor && _pRenderMonitor->testBreak())
    return;
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

void WXEdgeBuilder::buildWVertices(WShape &shape, const float *vertices, unsigned vsize)
{
  WXVertex *vertex;
  for (unsigned int i = 0; i < vsize; i += 3) {
    vertex = new WXVertex(Vec3f(vertices[i], vertices[i + 1], vertices[i + 2]));
    vertex->setId(i / 3);
    shape.AddVertex(vertex);
  }
}

} /* namespace Freestyle */
