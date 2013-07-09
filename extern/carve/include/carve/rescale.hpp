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
#include <carve/aabb.hpp>
#include <carve/matrix.hpp>

#include <limits>

namespace carve {
  namespace rescale {

    template<typename T>
    T calc_scale(T max) {
      const int radix = std::numeric_limits<T>::radix;

      T div = T(1);
      T m = fabs(max);
      while (div < m) div *= radix;
      m *= radix;
      while (div > m) div /= radix;
      return div;
    }

    template<typename T>
    T calc_delta(T min, T max) {
      const int radix = std::numeric_limits<T>::radix;

      if (min >= T(0) || max <= T(0)) {
        bool neg = false;
        if (max <= T(0)) {
          min = -min;
          max = -max;
          std::swap(min, max);
          neg = true;
        }
        T t = T(1);
        while (t > max) t /= radix;
        while (t <= max/radix) t *= radix;
        volatile T temp = t + min;
        temp -= t;
        if (neg) temp = -temp;
        return temp;
      } else {
        return T(0);
      }
    }

    struct rescale {
      double dx, dy, dz, scale;

      void init(double minx, double miny, double minz, double maxx, double maxy, double maxz) {
        dx = calc_delta(minx, maxx); minx -= dx; maxx -= dx;
        dy = calc_delta(miny, maxy); miny -= dy; maxy -= dy;
        dz = calc_delta(minz, maxz); minz -= dz; maxz -= dz;
        scale = calc_scale(std::max(std::max(fabs(minz), fabs(maxz)),
                                    std::max(std::max(fabs(minx), fabs(maxx)),
                                             std::max(fabs(miny), fabs(maxy)))));
      }

      rescale(double minx, double miny, double minz, double maxx, double maxy, double maxz) {
        init(minx, miny, minz, maxx, maxy, maxz);
      }
      rescale(const carve::geom3d::Vector &min, const carve::geom3d::Vector &max) {
        init(min.x, min.y, min.z, max.x, max.y, max.z);
      }
    };

    struct fwd {
      rescale r;
      fwd(const rescale &_r) : r(_r) { }
      carve::geom3d::Vector operator()(const carve::geom3d::Vector &v) const { return carve::geom::VECTOR((v.x - r.dx) / r.scale, (v.y - r.dy) / r.scale, (v.z - r.dz) / r.scale); }
    };

    struct rev {
      rescale r;
      rev(const rescale &_r) : r(_r) { }
      carve::geom3d::Vector operator()(const carve::geom3d::Vector &v) const { return carve::geom::VECTOR((v.x * r.scale) + r.dx, (v.y * r.scale) + r.dy, (v.z * r.scale) + r.dz); }
    };

  }
}
