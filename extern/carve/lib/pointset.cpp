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


#if defined(HAVE_CONFIG_H)
#  include <carve_config.h>
#endif

#include <carve/geom.hpp>
#include <carve/pointset.hpp>

namespace carve {
  namespace point {

    PointSet::PointSet(const std::vector<carve::geom3d::Vector> &points) {
      vertices.resize(points.size());
      for (size_t i = 0; i < points.size(); ++i) {
        vertices[i].v = points[i];
      }
      aabb.fit(points.begin(), points.end());
    }

    void PointSet::sortVertices(const carve::geom3d::Vector &axis) {
      std::vector<std::pair<double, size_t> > temp;
      temp.reserve(vertices.size());
      for (size_t i = 0; i < vertices.size(); ++i) {
        temp.push_back(std::make_pair(dot(axis, vertices[i].v), i));
      }
      std::sort(temp.begin(), temp.end());

      std::vector<Vertex> vnew;
      vnew.reserve(vertices.size());

      // std::vector<int> revmap;
      // revmap.resize(vertices.size());

      for (size_t i = 0; i < vertices.size(); ++i) {
        vnew.push_back(vertices[temp[i].second]);
        // revmap[temp[i].second] = i;
      }

      vertices.swap(vnew);
   }

  }
}
