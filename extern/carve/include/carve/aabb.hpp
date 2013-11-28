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

#include <carve/geom.hpp>

#include <vector>

namespace carve {
  namespace geom {



    // n-dimensional AABB
    template<unsigned ndim>
    struct aabb {
      typedef vector<ndim> vector_t;
      typedef aabb<ndim> aabb_t;

      vector_t pos;     // the centre of the AABB
      vector_t extent;  // the extent of the AABB - the vector from the centre to the maximal vertex.

      void empty();

      bool isEmpty() const;

      void fit(const vector_t &v1);
      void fit(const vector_t &v1, const vector_t &v2);
      void fit(const vector_t &v1, const vector_t &v2, const vector_t &v3);

      template<typename iter_t, typename value_type>
      void _fit(iter_t begin, iter_t end, value_type);

      template<typename iter_t>
      void _fit(iter_t begin, iter_t end, vector_t);

      template<typename iter_t>
      void _fit(iter_t begin, iter_t end, aabb_t);

      template<typename iter_t>
      void fit(iter_t begin, iter_t end);

      template<typename iter_t, typename adapt_t>
      void fit(iter_t begin, iter_t end, adapt_t adapt);

      void unionAABB(const aabb<ndim> &a);

      void expand(double pad);

      bool completelyContains(const aabb<ndim> &other) const;

      bool containsPoint(const vector_t &v) const;

      bool intersectsLineSegment(const vector_t &v1, const vector_t &v2) const;

      double axisSeparation(const aabb<ndim> &other, unsigned axis) const;

      double maxAxisSeparation(const aabb<ndim> &other) const;

      bool intersects(const aabb<ndim> &other) const;
      bool intersects(const sphere<ndim> &s) const;
      bool intersects(const plane<ndim> &plane) const;
      bool intersects(const ray<ndim> &ray) const;
      bool intersects(tri<ndim> tri) const;
      bool intersects(const linesegment<ndim> &ls) const;

      std::pair<double, double> rangeInDirection(const carve::geom::vector<ndim> &v) const;

      vector_t min() const;
      vector_t mid() const;
      vector_t max() const;

      double min(unsigned dim) const;
      double mid(unsigned dim) const;
      double max(unsigned dim) const;

      double volume() const;

      int compareAxis(const axis_pos &ap) const;

      void constrainMax(const axis_pos &ap);
      void constrainMin(const axis_pos &ap);

      aabb getAABB() const;

      aabb(const vector_t &_pos = vector_t::ZERO(),
           const vector_t &_extent = vector_t::ZERO());

      template<typename iter_t, typename adapt_t>
      aabb(iter_t begin, iter_t end, adapt_t adapt);

      template<typename iter_t>
      aabb(iter_t begin, iter_t end);

      aabb(const aabb<ndim> &a, const aabb<ndim> &b);
    };

    template<unsigned ndim>
    bool operator==(const aabb<ndim> &a, const aabb<ndim> &b);

    template<unsigned ndim>
    bool operator!=(const aabb<ndim> &a, const aabb<ndim> &b);

    template<unsigned ndim>
    std::ostream &operator<<(std::ostream &o, const aabb<ndim> &a);

    template<unsigned ndim>
    double distance2(const aabb<3> &a, const vector<ndim> &v);

    template<unsigned ndim>
    double distance(const aabb<3> &a, const vector<ndim> &v);



    template<unsigned ndim, typename obj_t>
    struct get_aabb {
      aabb<ndim> operator()(const obj_t &obj) const {
        return obj.getAABB();
      }
    };

    template<unsigned ndim, typename obj_t>
    struct get_aabb<ndim, obj_t *> {
      aabb<ndim> operator()(const obj_t *obj) const {
        return obj->getAABB();
      }
    };



  }
}

namespace carve {
  namespace geom3d {
    typedef carve::geom::aabb<3> AABB;
  }
}

#include <carve/aabb_impl.hpp>
