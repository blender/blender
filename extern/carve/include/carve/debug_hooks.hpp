// Begin License:
// Copyright (C) 2006-2011 Tobias Sargeant (tobias.sargeant@gmail.com).
// All rights reserved.
//
// This file is part of the Carve CSG Library (http://carve-csg.com/)
//
// This file may be used under the terms of the GNU General Public
// License version 2.0 as published by the Free Software Foundation
// and appearing in the file LICENSE.GPL2 included in the packaging of
// this file.
//
// This file is provided "AS IS" with NO WARRANTY OF ANY KIND,
// INCLUDING THE WARRANTIES OF DESIGN, MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE.
// End:


#pragma once

#include <carve/carve.hpp>

#include <carve/vector.hpp>
#include <carve/geom3d.hpp>
#include <carve/csg.hpp>

#include <iomanip>

template<typename MAP>
void map_histogram(std::ostream &out, const MAP &map) {
  std::vector<int> hist;
  for (typename MAP::const_iterator i = map.begin(); i != map.end(); ++i) {
    size_t n = (*i).second.size();
    if (hist.size() <= n) {
      hist.resize(n + 1);
    }
    hist[n]++;
  }
  int total = map.size();
  std::string bar(50, '*');
  for (size_t i = 0; i < hist.size(); i++) {
    if (hist[i] > 0) {
      out << std::setw(5) << i << " : " << std::setw(5) << hist[i] << " " << bar.substr(50 - hist[i] * 50 / total) << std::endl;
    }
  }
}

namespace carve {
  namespace csg {
    class IntersectDebugHooks {
    public:
      virtual void drawIntersections(const VertexIntersections & /* vint */) {
      }

      virtual void drawPoint(const carve::mesh::MeshSet<3>::vertex_t * /* v */,
                             float /* r */,
                             float /* g */,
                             float /* b */,
                             float /* a */,
                             float /* rad */) {
      }
      virtual void drawEdge(const carve::mesh::MeshSet<3>::vertex_t * /* v1 */,
                            const carve::mesh::MeshSet<3>::vertex_t * /* v2 */,
                            float /* rA */, float /* gA */, float /* bA */, float /* aA */,
                            float /* rB */, float /* gB */, float /* bB */, float /* aB */,
                            float /* thickness */ = 1.0) {
      }

      virtual void drawFaceLoopWireframe(const std::vector<carve::mesh::MeshSet<3>::vertex_t *> & /* face_loop */,
                                         const carve::mesh::MeshSet<3>::vertex_t & /* normal */,
                                         float /* r */, float /* g */, float /* b */, float /* a */,
                                         bool /* inset */ = true) {
      }

      virtual void drawFaceLoop(const std::vector<carve::mesh::MeshSet<3>::vertex_t *> & /* face_loop */,
                                const carve::mesh::MeshSet<3>::vertex_t & /* normal */,
                                float /* r */, float /* g */, float /* b */, float /* a */,
                                bool /* offset */ = true,
                                bool /* lit */ = true) {
      }

      virtual void drawFaceLoop2(const std::vector<carve::mesh::MeshSet<3>::vertex_t *> & /* face_loop */,
                                 const carve::mesh::MeshSet<3>::vertex_t & /* normal */,
                                 float /* rF */, float /* gF */, float /* bF */, float /* aF */,
                                 float /* rB */, float /* gB */, float /* bB */, float /* aB */,
                                 bool /* offset */ = true,
                                 bool /* lit */ = true) {
      }

      virtual ~IntersectDebugHooks() {
      }
    };

    IntersectDebugHooks *intersect_installDebugHooks(IntersectDebugHooks *hooks);
    bool intersect_debugEnabled();

  }
}
