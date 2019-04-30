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

#ifndef __FREESTYLE_WX_EDGE_BUILDER_H__
#define __FREESTYLE_WX_EDGE_BUILDER_H__

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
  WXEdgeBuilder() : WingedEdgeBuilder()
  {
  }
  virtual ~WXEdgeBuilder()
  {
  }
  VISIT_DECL(IndexedFaceSet)

 protected:
  virtual void buildWVertices(WShape &shape, const float *vertices, unsigned vsize);

#ifdef WITH_CXX_GUARDEDALLOC
  MEM_CXX_CLASS_ALLOC_FUNCS("Freestyle:WXEdgeBuilder")
#endif
};

} /* namespace Freestyle */

#endif  // __FREESTYLE_WX_EDGE_BUILDER_H__
