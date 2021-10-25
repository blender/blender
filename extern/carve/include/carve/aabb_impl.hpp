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

#include <carve/carve.hpp>

#include <carve/vector.hpp>
#include <carve/geom3d.hpp>

#include <carve/geom.hpp>

#include <vector>

namespace carve {
  namespace geom {

    template<unsigned ndim>
    void aabb<ndim>::empty() {
      pos.setZero();
      extent.setZero();
    }

    template<unsigned ndim>
    bool aabb<ndim>::isEmpty() const {
      return extent.exactlyZero();
    }

    template<unsigned ndim>
    template<typename iter_t, typename value_type>
    void aabb<ndim>::_fit(iter_t begin, iter_t end, value_type) {
      if (begin == end) {
        empty();
        return;
      }

      vector_t min, max;
      aabb<ndim> a = get_aabb<ndim, value_type>()(*begin); ++begin;
      min = a.min();
      max = a.max();
      while (begin != end) {
        aabb<ndim> a = get_aabb<ndim, value_type>()(*begin); ++begin;
        assign_op(min, min, a.min(), carve::util::min_functor());
        assign_op(max, max, a.max(), carve::util::max_functor());
      }

      pos = (min + max) / 2.0;
      assign_op(extent, max - pos, pos - min, carve::util::max_functor());
    }

    template<unsigned ndim>
    template<typename iter_t>
    void aabb<ndim>::_fit(iter_t begin, iter_t end, vector_t) {
      if (begin == end) {
        empty();
        return;
      }

      vector_t min, max;
      bounds(begin, end, min, max);
      pos = (min + max) / 2.0;
      assign_op(extent, max - pos, pos - min, carve::util::max_functor());
    }

    template<unsigned ndim>
    template<typename iter_t>
    void aabb<ndim>::_fit(iter_t begin, iter_t end, aabb_t) {
      if (begin == end) {
        empty();
        return;
      }

      vector_t min, max;
      aabb<ndim> a = *begin++;
      min = a.min();
      max = a.max();
      while (begin != end) {
        aabb<ndim> a = *begin; ++begin;
        assign_op(min, min, a.min(), carve::util::min_functor());
        assign_op(max, max, a.max(), carve::util::max_functor());
      }

      pos = (min + max) / 2.0;
      assign_op(extent, max - pos, pos - min, carve::util::max_functor());
    }

    template<unsigned ndim>
    void aabb<ndim>::fit(const vector_t &v1) {
      pos = v1;
      extent.setZero();
    }

    template<unsigned ndim>
    void aabb<ndim>::fit(const vector_t &v1, const vector_t &v2) {
      vector_t min, max;
      assign_op(min, v1, v2, carve::util::min_functor());
      assign_op(max, v1, v2, carve::util::max_functor());

      pos = (min + max) / 2.0;
      assign_op(extent, max - pos, pos - min, carve::util::max_functor());
    }

    template<unsigned ndim>
    void aabb<ndim>::fit(const vector_t &v1, const vector_t &v2, const vector_t &v3) {
      vector_t min, max;
      min = max = v1;

      assign_op(min, min, v2, carve::util::min_functor());
      assign_op(max, max, v2, carve::util::max_functor());
      assign_op(min, min, v3, carve::util::min_functor());
      assign_op(max, max, v3, carve::util::max_functor());

      pos = (min + max) / 2.0;
      assign_op(extent, max - pos, pos - min, carve::util::max_functor());
    }

    template<unsigned ndim>
    template<typename iter_t, typename adapt_t>
    void aabb<ndim>::fit(iter_t begin, iter_t end, adapt_t adapt) {
      vector_t min, max;

      bounds(begin, end, adapt, min, max);
      pos = (min + max) / 2.0;
      assign_op(extent, max - pos, pos - min, carve::util::max_functor());
    }

    template<unsigned ndim>
    template<typename iter_t>
    void aabb<ndim>::fit(iter_t begin, iter_t end) {
      _fit(begin, end, typename std::iterator_traits<iter_t>::value_type());
    }

    template<unsigned ndim>
    void aabb<ndim>::expand(double pad) {
      extent += pad;
    }

    template<unsigned ndim>
    void aabb<ndim>::unionAABB(const aabb<ndim> &a) {
      vector_t vmin, vmax;

      assign_op(vmin, min(), a.min(), carve::util::min_functor());
      assign_op(vmax, max(), a.max(), carve::util::max_functor());
      pos = (vmin + vmax) / 2.0;
      assign_op(extent, vmax - pos, pos - vmin, carve::util::max_functor());
    }

    template<unsigned ndim>
    bool aabb<ndim>::completelyContains(const aabb<ndim> &other) const {
      for (unsigned i = 0; i < ndim; ++i) {
        if (fabs(other.pos.v[i] - pos.v[i]) + other.extent.v[i] > extent.v[i]) return false;
      }
      return true;
    }

    template<unsigned ndim>
    bool aabb<ndim>::containsPoint(const vector_t &v) const {
      for (unsigned i = 0; i < ndim; ++i) {
        if (fabs(v.v[i] - pos.v[i]) > extent.v[i]) return false;
      }
      return true;
    }

    template<unsigned ndim>
    double aabb<ndim>::axisSeparation(const aabb<ndim> &other, unsigned axis) const {
      return fabs(other.pos.v[axis] - pos.v[axis]) - extent.v[axis] - other.extent.v[axis];
    }

    template<unsigned ndim>
    double aabb<ndim>::maxAxisSeparation(const aabb<ndim> &other) const {
      double m = axisSeparation(other, 0);
      for (unsigned i = 1; i < ndim; ++i) {
        m = std::max(m, axisSeparation(other, i));
      }
      return m;
    }

    template<unsigned ndim>
    bool aabb<ndim>::intersects(const aabb<ndim> &other) const {
      return maxAxisSeparation(other) <= 0.0;
    }
 
    template<unsigned ndim>
    bool aabb<ndim>::intersects(const sphere<ndim> &s) const {
      double r = 0.0;
      for (unsigned i = 0; i < ndim; ++i) {
        double t = fabs(s.C[i] - pos[i]) - extent[i]; if (t > 0.0) r += t*t;
      }
      return r <= s.r*s.r;
    }

    template<unsigned ndim>
    bool aabb<ndim>::intersects(const plane<ndim> &plane) const {
      double d1 = fabs(distance(plane, pos));
      double d2 = dot(abs(plane.N), extent);
      return d1 <= d2;
    }

    template<unsigned ndim>
    bool aabb<ndim>::intersects(const linesegment<ndim> &ls) const {
      return intersectsLineSegment(ls.v1, ls.v2);
    }

    template<unsigned ndim>
    std::pair<double, double> aabb<ndim>::rangeInDirection(const carve::geom::vector<ndim> &v) const {
      double d1 = dot(v, pos);
      double d2 = dot(abs(v), extent);

      return std::make_pair(d1 - d2, d1 + d2);
    }

    template<unsigned ndim>
    typename aabb<ndim>::vector_t aabb<ndim>::min() const { return pos - extent; }

    template<unsigned ndim>
    typename aabb<ndim>::vector_t aabb<ndim>::mid() const { return pos; }

    template<unsigned ndim>
    typename aabb<ndim>::vector_t aabb<ndim>::max() const { return pos + extent; }

    template<unsigned ndim>
    double aabb<ndim>::min(unsigned dim) const { return pos.v[dim] - extent.v[dim]; }

    template<unsigned ndim>
    double aabb<ndim>::mid(unsigned dim) const { return pos.v[dim]; }

    template<unsigned ndim>
    double aabb<ndim>::max(unsigned dim) const { return pos.v[dim] + extent.v[dim]; }

    template<unsigned ndim>
    double aabb<ndim>::volume() const {
      double v = 1.0;
      for (size_t dim = 0; dim < ndim; ++dim) { v *= 2.0 * extent.v[dim]; }
      return v;
    }

    template<unsigned ndim>
    int aabb<ndim>::compareAxis(const axis_pos &ap) const {
      double p = ap.pos - pos[ap.axis];
      if (p > extent[ap.axis]) return -1;
      if (p < -extent[ap.axis]) return +1;
      return 0;
    }

    template<unsigned ndim>
    void aabb<ndim>::constrainMax(const axis_pos &ap) {
      if (pos[ap.axis] + extent[ap.axis] > ap.pos) {
        double min = std::min(ap.pos, pos[ap.axis] - extent[ap.axis]);
        pos[ap.axis] = (min + ap.pos) / 2.0;
        extent[ap.axis] = ap.pos - pos[ap.axis];
      }
    }

    template<unsigned ndim>
    void aabb<ndim>::constrainMin(const axis_pos &ap) {
      if (pos[ap.axis] - extent[ap.axis] < ap.pos) {
        double max = std::max(ap.pos, pos[ap.axis] + extent[ap.axis]);
        pos[ap.axis] = (ap.pos + max) / 2.0;
        extent[ap.axis] = pos[ap.axis] - ap.pos;
      }
    }

    template<unsigned ndim>
    aabb<ndim> aabb<ndim>::getAABB() const {
      return *this;
    }

    template<unsigned ndim>
    aabb<ndim>::aabb(const vector_t &_pos,
                     const vector_t &_extent) : pos(_pos), extent(_extent) {
    }

    template<unsigned ndim>
    template<typename iter_t, typename adapt_t>
    aabb<ndim>::aabb(iter_t begin, iter_t end, adapt_t adapt) {
      fit(begin, end, adapt);
    }

    template<unsigned ndim>
    template<typename iter_t>
    aabb<ndim>::aabb(iter_t begin, iter_t end) {
      fit(begin, end);
    }

    template<unsigned ndim>
    aabb<ndim>::aabb(const aabb<ndim> &a, const aabb<ndim> &b) {
      fit(a, b);
    }

    template<unsigned ndim>
    bool operator==(const aabb<ndim> &a, const aabb<ndim> &b) {
      return a.pos == b.pos && a.extent == b.extent;
    }

    template<unsigned ndim>
    bool operator!=(const aabb<ndim> &a, const aabb<ndim> &b) {
      return a.pos != b.pos || a.extent != b.extent;
    }

    template<unsigned ndim>
    std::ostream &operator<<(std::ostream &o, const aabb<ndim> &a) {
      o << (a.pos - a.extent) << "--" << (a.pos + a.extent);
      return o;
    }

    template<unsigned ndim>
    double distance2(const aabb<3> &a, const vector<ndim> &v) {
      double d2 = 0.0;
      for (unsigned i = 0; i < ndim; ++i) {
        double d = ::fabs(v.v[i] - a.pos.v[i]) - a.extent.v[i];
        if (d > 0.0) {
          d2 += d * d;
        }
      }
      return d2;
    }

    template<unsigned ndim>
    double distance(const aabb<3> &a, const vector<ndim> &v) {
      return ::sqrt(distance2(a, v));
    }

    template<>
    inline bool aabb<3>::intersects(const ray<3> &ray) const {
      vector<3> t = pos - ray.v;
      double r;

      //l.cross(x-axis)?
      r = extent.y * fabs(ray.D.z) + extent.z * fabs(ray.D.y);
      if (fabs(t.y * ray.D.z - t.z * ray.D.y) > r) return false;

      //ray.D.cross(y-axis)?
      r = extent.x * fabs(ray.D.z) + extent.z * fabs(ray.D.x);
      if (fabs(t.z * ray.D.x - t.x * ray.D.z) > r) return false;

      //ray.D.cross(z-axis)?
      r = extent.x*fabs(ray.D.y) + extent.y*fabs(ray.D.x);
      if (fabs(t.x * ray.D.y - t.y * ray.D.x) > r) return false;

      return true;
    }

    template<>
    inline bool aabb<3>::intersectsLineSegment(const vector<3> &v1, const vector<3> &v2) const {
      vector<3> half_length = 0.5 * (v2 - v1);
      vector<3> t = pos - half_length - v1;
      double r;

      //do any of the principal axes form a separating axis?
      if(fabs(t.x) > extent.x + fabs(half_length.x)) return false;
      if(fabs(t.y) > extent.y + fabs(half_length.y)) return false;
      if(fabs(t.z) > extent.z + fabs(half_length.z)) return false;

      // NOTE: Since the separating axis is perpendicular to the line in
      // these last four cases, the line does not contribute to the
      // projection.

      //line.cross(x-axis)?
      r = extent.y * fabs(half_length.z) + extent.z * fabs(half_length.y);
      if (fabs(t.y * half_length.z - t.z * half_length.y) > r) return false;

      //half_length.cross(y-axis)?
      r = extent.x * fabs(half_length.z) + extent.z * fabs(half_length.x);
      if (fabs(t.z * half_length.x - t.x * half_length.z) > r) return false;

      //half_length.cross(z-axis)?
      r = extent.x*fabs(half_length.y) + extent.y*fabs(half_length.x);
      if (fabs(t.x * half_length.y - t.y * half_length.x) > r) return false;

      return true;
    }

    template<int Ax, int Ay, int Az, int c>
    static inline bool intersectsTriangle_axisTest_3(const aabb<3> &aabb, const tri<3> &tri) {
      const int d = (c+1) % 3, e = (c+2) % 3;
      const vector<3> a = cross(VECTOR(Ax, Ay, Az), tri.v[d] - tri.v[c]);
      double p1 = dot(a, tri.v[c]), p2 = dot(a, tri.v[e]);
      if (p1 > p2) std::swap(p1, p2);
      const double r = dot(abs(a), aabb.extent);
      return !(p1 > r || p2 < -r);
    }

    template<int c>
    static inline bool intersectsTriangle_axisTest_2(const aabb<3> &aabb, const tri<3> &tri) {
      double vmin = std::min(std::min(tri.v[0][c], tri.v[1][c]), tri.v[2][c]),
             vmax = std::max(std::max(tri.v[0][c], tri.v[1][c]), tri.v[2][c]);
      return !(vmin > aabb.extent[c] || vmax < -aabb.extent[c]);
    }

    static inline bool intersectsTriangle_axisTest_1(const aabb<3> &aabb, const tri<3> &tri) {
      vector<3> n = cross(tri.v[1] - tri.v[0], tri.v[2] - tri.v[0]);
      double d1 = fabs(dot(n, tri.v[0]));
      double d2 = dot(abs(n), aabb.extent);
      return d1 <= d2;
    }

    template<>
    inline bool aabb<3>::intersects(tri<3> tri) const {
      tri.v[0] -= pos;
      tri.v[1] -= pos;
      tri.v[2] -= pos;

      if (!intersectsTriangle_axisTest_2<0>(*this, tri)) return false;
      if (!intersectsTriangle_axisTest_2<1>(*this, tri)) return false;
      if (!intersectsTriangle_axisTest_2<2>(*this, tri)) return false;

      if (!intersectsTriangle_axisTest_3<1,0,0,0>(*this, tri)) return false;
      if (!intersectsTriangle_axisTest_3<1,0,0,1>(*this, tri)) return false;
      if (!intersectsTriangle_axisTest_3<1,0,0,2>(*this, tri)) return false;
                                                          
      if (!intersectsTriangle_axisTest_3<0,1,0,0>(*this, tri)) return false;
      if (!intersectsTriangle_axisTest_3<0,1,0,1>(*this, tri)) return false;
      if (!intersectsTriangle_axisTest_3<0,1,0,2>(*this, tri)) return false;
                                                          
      if (!intersectsTriangle_axisTest_3<0,0,1,0>(*this, tri)) return false;
      if (!intersectsTriangle_axisTest_3<0,0,1,1>(*this, tri)) return false;
      if (!intersectsTriangle_axisTest_3<0,0,1,2>(*this, tri)) return false;

      if (!intersectsTriangle_axisTest_1(*this, tri)) return false;

      return true;
    }



  }
}
