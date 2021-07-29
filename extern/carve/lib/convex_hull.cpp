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

#include <carve/csg.hpp>
#include <carve/convex_hull.hpp>

#include <algorithm>

namespace {

  bool grahamScan(const std::vector<carve::geom2d::P2> &points,
                  int vpp, int vp,
                  const std::vector<int> &ordered,
                  int start,
                  std::vector<int> &result, int _i = 0) {
    carve::geom2d::P2 v1 = points[vp] - points[vpp];
    if (start == (int)ordered.size()) return true;

    for (int i = start; i < (int)ordered.size(); ++i) {
      int v = ordered[i];
      carve::geom2d::P2 v2 = points[v] - points[vp];

      double cp = v1.x * v2.y - v2.x * v1.y;
      if (cp < 0) return false;

      int j = i + 1;
      while (j < (int)ordered.size() && points[ordered[j]] == points[v]) j++;

      result.push_back(v);
      if (grahamScan(points, vp, v, ordered, j, result, _i + 1)) return true;
      result.pop_back();
    }

    return false;
  }

}

namespace carve {
  namespace geom {

    std::vector<int> convexHull(const std::vector<carve::geom2d::P2> &points) {
      double max_x = points[0].x;
      unsigned max_v = 0;

      for (unsigned i = 1; i < points.size(); ++i) {
        if (points[i].x > max_x) {
          max_x = points[i].x;
          max_v = i;
        }
      }

      std::vector<std::pair<double, double> > angle_dist;
      std::vector<int> ordered;
      angle_dist.reserve(points.size());
      ordered.reserve(points.size() - 1);
      for (unsigned i = 0; i < points.size(); ++i) {
        if (i == max_v) continue;
        angle_dist[i] = std::make_pair(carve::math::ANG(carve::geom2d::atan2(points[i] - points[max_v])), distance2(points[i], points[max_v]));
        ordered.push_back(i);
      }
  
      std::sort(ordered.begin(),
                ordered.end(),
                make_index_sort(angle_dist.begin()));

      std::vector<int> result;
      result.push_back(max_v);
      result.push_back(ordered[0]);
  
      if (!grahamScan(points, max_v, ordered[0], ordered, 1, result)) {
        result.clear();
        throw carve::exception("convex hull failed!");
      }

      return result;
    }

  }
}


