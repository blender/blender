// Begin License:
// Copyright (C) 2006-2014 Tobias Sargeant (tobias.sargeant@gmail.com).
// All rights reserved.
//
// This file is part of the Carve CSG Library (http://carve-csg.com/)
//
// This file may be used under the terms of either the GNU General
// Public License version 2 or 3 (at your option) as published by the
// Free Software Foundation and appearing in the files LICENSE.GPL2
// and LICENSE.GPL3 included in the packaging of this file.
//
// This file is provided "AS IS" with NO WARRANTY OF ANY KIND,
// INCLUDING THE WARRANTIES OF DESIGN, MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE.
// End:


#if defined(HAVE_CONFIG_H)
#  include <carve_config.h>
#endif

#include <carve/geom.hpp>
#include <carve/vector.hpp>
#include <carve/polyline.hpp>

namespace carve {
  namespace line {
    carve::geom3d::AABB Polyline::aabb() const {
      return carve::geom3d::AABB(vbegin(), vend(), vec_adapt_vertex_ptr());
    }

    PolylineSet::PolylineSet(const std::vector<carve::geom3d::Vector> &points) {
      vertices.resize(points.size());
      for (size_t i = 0; i < points.size(); ++i) vertices[i].v = points[i];
      aabb.fit(points.begin(), points.end(), carve::geom3d::vec_adapt_ident());
    }

    void PolylineSet::sortVertices(const carve::geom3d::Vector &axis) {
      std::vector<std::pair<double, size_t> > temp;
      temp.reserve(vertices.size());
      for (size_t i = 0; i < vertices.size(); ++i) {
        temp.push_back(std::make_pair(dot(axis, vertices[i].v), i));
      }
      std::sort(temp.begin(), temp.end());
      std::vector<Vertex> vnew;
      std::vector<int> revmap;
      vnew.reserve(vertices.size());
      revmap.resize(vertices.size());

      for (size_t i = 0; i < vertices.size(); ++i) {
        vnew.push_back(vertices[temp[i].second]);
        revmap[temp[i].second] = i;
      }

      for (line_iter i = lines.begin(); i != lines.end(); ++i) {
        Polyline &l = *(*i);
        for (size_t j = 0; j < l.edges.size(); ++j) {
          PolylineEdge &e = *l.edges[j];
          if (e.v1) e.v1 = &vnew[revmap[vertexToIndex_fast(e.v1)]];
          if (e.v2) e.v2 = &vnew[revmap[vertexToIndex_fast(e.v2)]];
        }
      }
      vertices.swap(vnew);
    }

  }
}
