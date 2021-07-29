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

#include <carve/math.hpp>
#include <carve/math_constants.hpp>

#include <carve/geom.hpp>

#include <vector>
#include <algorithm>

#include <math.h>

#if defined(CARVE_DEBUG)
#  include <iostream>
#endif

#if defined CARVE_USE_EXACT_PREDICATES
#  include <carve/shewchuk_predicates.hpp>
#endif

namespace carve {
  namespace geom2d {

    typedef carve::geom::vector<2> P2;
    typedef carve::geom::ray<2> Ray2;
    typedef carve::geom::linesegment<2> LineSegment2;



    struct p2_adapt_ident {
      P2 &operator()(P2 &p) const { return p; }
      const P2 &operator()(const P2 &p) const { return p; }
    };



    typedef std::vector<P2> P2Vector;

    /** 
     * \brief Return the orientation of c with respect to the ray defined by a->b.
     *
     * (Can be implemented exactly)
     * 
     * @param[in] a 
     * @param[in] b 
     * @param[in] c 
     * 
     * @return positive, if c to the left of a->b.
     *         zero, if c is colinear with a->b.
     *         negative, if c to the right of a->b.
     */
#if defined CARVE_USE_EXACT_PREDICATES
    inline double orient2d(const P2 &a, const P2 &b, const P2 &c) {
      return shewchuk::orient2d(a.v, b.v, c.v);
    }
#else
    inline double orient2d(const P2 &a, const P2 &b, const P2 &c) {
      double acx = a.x - c.x;
      double bcx = b.x - c.x;
      double acy = a.y - c.y;
      double bcy = b.y - c.y;
      return acx * bcy - acy * bcx;
    }
#endif

    /** 
     * \brief Determine whether p is internal to the anticlockwise
     *        angle abc, where b is the apex of the angle.
     *
     * @param[in] a 
     * @param[in] b 
     * @param[in] c 
     * @param[in] p 
     * 
     * @return true, if p is contained in the anticlockwise angle from
     *               b->a to b->c. Reflex angles contain p if p lies
     *               on b->a or on b->c. Acute angles do not contain p
     *               if p lies on b->a or on b->c. This is so that
     *               internalToAngle(a,b,c,p) = !internalToAngle(c,b,a,p)
     */
    inline bool internalToAngle(const P2 &a,
                                const P2 &b,
                                const P2 &c,
                                const P2 &p) {
      bool reflex = (a < c) ?  orient2d(b, a, c) <= 0.0 : orient2d(b, c, a) > 0.0;
      double d1 = orient2d(b, a, p);
      double d2 = orient2d(b, c, p);
      if (reflex) {
        return d1 >= 0.0 || d2 <= 0.0;
      } else {
        return d1 > 0.0 && d2 < 0.0;
      }
    }

    /** 
     * \brief Determine whether p is internal to the anticlockwise
     *        angle ac, with apex at (0,0).
     *
     * @param[in] a 
     * @param[in] c 
     * @param[in] p 
     * 
     * @return true, if p is contained in a0c.
     */
    inline bool internalToAngle(const P2 &a,
                                const P2 &c,
                                const P2 &p) {
      return internalToAngle(a, P2::ZERO(), c, p);
    }

    template<typename P2vec>
    bool isAnticlockwise(const P2vec &tri) {
      return orient2d(tri[0], tri[1], tri[2]) > 0.0;
    }

    template<typename P2vec>
    bool pointIntersectsTriangle(const P2 &p, const P2vec &tri) {
      int orient = isAnticlockwise(tri) ? +1 : -1;
      if (orient2d(tri[0], tri[1], p) * orient < 0) return false;
      if (orient2d(tri[1], tri[2], p) * orient < 0) return false;
      if (orient2d(tri[2], tri[0], p) * orient < 0) return false;
      return true;
    }

    template<typename P2vec>
    bool lineIntersectsTriangle(const P2 &p1, const P2 &p2, const P2vec &tri) {
      double s[3];
      // does tri lie on one side or the other of p1-p2?
      s[0] = orient2d(p1, p2, tri[0]);
      s[1] = orient2d(p1, p2, tri[1]);
      s[2] = orient2d(p1, p2, tri[2]);
      if (*std::max_element(s, s+3) < 0) return false;
      if (*std::min_element(s, s+3) > 0) return false;

      // does line lie entirely to the right of a triangle edge?
      int orient = isAnticlockwise(tri) ? +1 : -1;
      if (orient2d(tri[0], tri[1], p1) * orient < 0 && orient2d(tri[0], tri[1], p2) * orient < 0) return false;
      if (orient2d(tri[1], tri[2], p1) * orient < 0 && orient2d(tri[1], tri[2], p2) * orient < 0) return false;
      if (orient2d(tri[2], tri[0], p1) * orient < 0 && orient2d(tri[2], tri[0], p2) * orient < 0) return false;
      return true;
    }

    template<typename P2vec>
    int triangleLineOrientation(const P2 &p1, const P2 &p2, const P2vec &tri) {
      double lo, hi, tmp;
      lo = hi = orient2d(p1, p2, tri[0]);
      tmp = orient2d(p1, p2, tri[1]); lo = std::min(lo, tmp); hi = std::max(hi, tmp);
      tmp = orient2d(p1, p2, tri[2]); lo = std::min(lo, tmp); hi = std::max(hi, tmp);
      if (hi < 0.0) return -1;
      if (lo > 0.0) return +1;
      return 0;
    }

    template<typename P2vec>
    bool triangleIntersectsTriangle(const P2vec &tri_b, const P2vec &tri_a) {
      int orient_a = isAnticlockwise(tri_a) ? +1 : -1;
      if (triangleLineOrientation(tri_a[0], tri_a[1], tri_b) * orient_a < 0) return false;
      if (triangleLineOrientation(tri_a[1], tri_a[2], tri_b) * orient_a < 0) return false;
      if (triangleLineOrientation(tri_a[2], tri_a[0], tri_b) * orient_a < 0) return false;

      int orient_b = isAnticlockwise(tri_b) ? +1 : -1;
      if (triangleLineOrientation(tri_b[0], tri_b[1], tri_a) * orient_b < 0) return false;
      if (triangleLineOrientation(tri_b[1], tri_b[2], tri_a) * orient_b < 0) return false;
      if (triangleLineOrientation(tri_b[2], tri_b[0], tri_a) * orient_b < 0) return false;

      return true;
    }



    static inline double atan2(const P2 &p) {
      return ::atan2(p.y, p.x);
    }



    struct LineIntersectionInfo {
      LineIntersectionClass iclass;
      P2 ipoint;
      int p1, p2;

      LineIntersectionInfo(LineIntersectionClass _iclass,
                           P2 _ipoint = P2::ZERO(),
                           int _p1 = -1,
                           int _p2 = -1) :
        iclass(_iclass), ipoint(_ipoint), p1(_p1), p2(_p2) {
      }
    };

    struct PolyInclusionInfo {
      PointClass iclass;
      int iobjnum;

      PolyInclusionInfo(PointClass _iclass,
                        int _iobjnum = -1) :
        iclass(_iclass), iobjnum(_iobjnum) {
      }
    };

    struct PolyIntersectionInfo {
      IntersectionClass iclass;
      P2 ipoint;
      size_t iobjnum;

      PolyIntersectionInfo(IntersectionClass _iclass,
                           const P2 &_ipoint,
                           size_t _iobjnum) :
        iclass(_iclass), ipoint(_ipoint), iobjnum(_iobjnum) {
      }
    };

    bool lineSegmentIntersection_simple(const P2 &l1v1, const P2 &l1v2,
                                        const P2 &l2v1, const P2 &l2v2);
    bool lineSegmentIntersection_simple(const LineSegment2 &l1,
                                        const LineSegment2 &l2);

    LineIntersectionInfo lineSegmentIntersection(const P2 &l1v1, const P2 &l1v2,
                                                 const P2 &l2v1, const P2 &l2v2);
    LineIntersectionInfo lineSegmentIntersection(const LineSegment2 &l1,
                                                 const LineSegment2 &l2);

    int lineSegmentPolyIntersections(const std::vector<P2> &points,
                                     LineSegment2 line,
                                     std::vector<PolyInclusionInfo> &out);

    int sortedLineSegmentPolyIntersections(const std::vector<P2> &points,
                                           LineSegment2 line,
                                           std::vector<PolyInclusionInfo> &out);



    static inline bool quadIsConvex(const P2 &a, const P2 &b, const P2 &c, const P2 &d) {
      double s_1, s_2;

      s_1 = carve::geom2d::orient2d(a, c, b);
      s_2 = carve::geom2d::orient2d(a, c, d);
      if ((s_1 < 0.0 && s_2 < 0.0) || (s_1 > 0.0 && s_2 > 0.0)) return false;

      s_1 = carve::geom2d::orient2d(b, d, a);
      s_2 = carve::geom2d::orient2d(b, d, c);
      if ((s_1 < 0.0 && s_2 < 0.0) || (s_1 > 0.0 && s_2 > 0.0)) return false;

      return true;
    }

    template<typename T, typename adapt_t>
    inline bool quadIsConvex(const T &a, const T &b, const T &c, const T &d, adapt_t adapt) {
      return quadIsConvex(adapt(a), adapt(b), adapt(c), adapt(d));
    }



    double signedArea(const std::vector<P2> &points);

    static inline double signedArea(const P2 &a, const P2 &b, const P2 &c) {
      return ((b.y + a.y) * (b.x - a.x) + (c.y + b.y) * (c.x - b.x) + (a.y + c.y) * (a.x - c.x)) / 2.0;
    }

    template<typename T, typename adapt_t>
    double signedArea(const std::vector<T> &points, adapt_t adapt) {
      P2Vector::size_type l = points.size();
      double A = 0.0;

      for (P2Vector::size_type i = 0; i < l - 1; i++) {
        A += (adapt(points[i + 1]).y + adapt(points[i]).y) * (adapt(points[i + 1]).x - adapt(points[i]).x);
      }
      A += (adapt(points[0]).y + adapt(points[l - 1]).y) * (adapt(points[0]).x - adapt(points[l - 1]).x);

      return A / 2.0;
    }



    template<typename iter_t, typename adapt_t>
    double signedArea(iter_t begin, iter_t end, adapt_t adapt) {
      double A = 0.0;
      P2 p, n;

      if (begin == end) return 0.0;

      p = adapt(*begin);
      for (iter_t c = begin; ++c != end; ) {
        P2 n = adapt(*c);
        A += (n.y + p.y) * (n.x - p.x);
        p = n;
      }
      n = adapt(*begin);
      A += (n.y + p.y) * (n.x - p.x);

      return A / 2.0;
    }



    bool pointInPolySimple(const std::vector<P2> &points, const P2 &p);

    template<typename T, typename adapt_t>
    bool pointInPolySimple(const std::vector<T> &points, adapt_t adapt, const P2 &p) {
      CARVE_ASSERT(points.size() > 0);
      P2Vector::size_type l = points.size();
      double s = 0.0;
      double rp, r0, d;

      rp = r0 = atan2(adapt(points[0]) - p);

      for (P2Vector::size_type i = 1; i < l; i++) {
        double r = atan2(adapt(points[i]) - p);
        d = r - rp;
        if (d > M_PI) d -= M_TWOPI;
        if (d < -M_PI) d += M_TWOPI;
        s = s + d;
        rp = r;
      }

      d = r0 - rp;
      if (d > M_PI) d -= M_TWOPI;
      if (d < -M_PI) d += M_TWOPI;
      s = s + d;

      return !carve::math::ZERO(s);
    }



    PolyInclusionInfo pointInPoly(const std::vector<P2> &points, const P2 &p);

    template<typename T, typename adapt_t>
    PolyInclusionInfo pointInPoly(const std::vector<T> &points, adapt_t adapt, const P2 &p) {
      P2Vector::size_type l = points.size();
      for (unsigned i = 0; i < l; i++) {
        if (equal(adapt(points[i]), p)) return PolyInclusionInfo(POINT_VERTEX, (int)i);
      }

      for (unsigned i = 0; i < l; i++) {
        unsigned j = (i + 1) % l;

        if (std::min(adapt(points[i]).x, adapt(points[j]).x) - EPSILON < p.x &&
            std::max(adapt(points[i]).x, adapt(points[j]).x) + EPSILON > p.x &&
            std::min(adapt(points[i]).y, adapt(points[j]).y) - EPSILON < p.y &&
            std::max(adapt(points[i]).y, adapt(points[j]).y) + EPSILON > p.y &&
            distance2(carve::geom::rayThrough(adapt(points[i]), adapt(points[j])), p) < EPSILON2) {
          return PolyInclusionInfo(POINT_EDGE, (int)i);
        }
      }

      if (pointInPolySimple(points, adapt, p)) {
        return PolyInclusionInfo(POINT_IN);
      }

      return PolyInclusionInfo(POINT_OUT);
    }



    bool pickContainedPoint(const std::vector<P2> &poly, P2 &result);

    template<typename T, typename adapt_t>
    bool pickContainedPoint(const std::vector<T> &poly, adapt_t adapt, P2 &result) {
#if defined(CARVE_DEBUG)
      std::cerr << "pickContainedPoint ";
      for (unsigned i = 0; i < poly.size(); ++i) std::cerr << " " << adapt(poly[i]);
      std::cerr << std::endl;
#endif

      const size_t S = poly.size();
      P2 a, b, c;
      for (unsigned i = 0; i < S; ++i) {
        a = adapt(poly[i]);
        b = adapt(poly[(i + 1) % S]);
        c = adapt(poly[(i + 2) % S]);

        if (cross(a - b, c - b) < 0) {
          P2 p = (a + b + c) / 3;
          if (pointInPolySimple(poly, adapt, p)) {
            result = p;
            return true;
          }
        }
      }
      return false;
    }

  }
}
