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


#pragma once

#include <list>
#include <vector>
#include <algorithm>

#include <carve/carve.hpp>

#include <carve/geom2d.hpp>

namespace carve {
  namespace geom {
    std::vector<int> convexHull(const std::vector<carve::geom2d::P2> &points);

    template<typename project_t, typename polygon_container_t>
    std::vector<int> convexHull(const project_t &project, const polygon_container_t &points) {
      std::vector<carve::geom2d::P2> proj;
      proj.reserve(points.size());
      for (typename polygon_container_t::const_iterator i = points.begin(); i != points.end(); ++i) {
        proj.push_back(project(*i));
      }
      return convexHull(proj);
    }

    template<typename project_t, typename iter_t>
    std::vector<int> convexHull(const project_t &project, iter_t beg, iter_t end, size_t size_hint = 0) {
      std::vector<carve::geom2d::P2> proj;
      if (size_hint) proj.reserve(size_hint);
      for (; beg != end; ++beg) {
        proj.push_back(project(*beg));
      }
      return convexHull(proj);
    }
  }
}
