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
#include <carve/geom.hpp>

#include <math.h>
#include <carve/math_constants.hpp>

#include <vector>
#include <list>
#include <map>

#if defined(CARVE_DEBUG)
#  include <iostream>
#endif

#if defined CARVE_USE_EXACT_PREDICATES
#  include <carve/shewchuk_predicates.hpp>
#endif

namespace carve {
  namespace geom3d {

    typedef carve::geom::plane<3> Plane;
    typedef carve::geom::ray<3> Ray;
    typedef carve::geom::linesegment<3> LineSegment;
    typedef carve::geom::vector<3> Vector;

    template<typename iter_t, typename adapt_t>
    bool fitPlane(iter_t begin, iter_t end, adapt_t adapt, Plane &plane) {
      std::vector<Vector> p;
      for (; begin != end; ++begin) {
        p.push_back(adapt(*begin));
      }

      if (p.size() < 3) {
        return false;
      }

      Vector C;
      carve::geom::centroid(p.begin(), p.end(), C);

      Vector n;

      if (p.size() == 3) {
        n = cross(p[1] - p[0], p[2] - p[0]);
      } else {
        const size_t N = p.size();

        n = cross(p[N-1] - C, p[0] - C);
        if (n < Vector::ZERO()) n.negate();
        for (size_t i = 1; i < p.size(); ++i) {
          Vector v = cross(p[i] - C, p[i-1] - C);
          if (v < Vector::ZERO()) v.negate();
          n += v;
        }
      }

      double l = n.length();

      if (l == 0.0) {
        n.x = 1.0;
        n.y = 0.0;
        n.z = 0.0;
      } else {
        n.normalize();
      }

      plane.N = n;
      plane.d = -dot(n, C);

#if defined(CARVE_DEBUG)
      if (p.size() > 3) {
        std::cerr << "N-gon with " << p.size() << " vertices: fitted distance:";
        for (size_t i = 0; i < p.size(); ++i) {
          std::cerr << " {" << p[i] << "} " << distance(plane, p[i]);
        }
        std::cerr << std::endl;
      }
#endif

      return true;
    }

    bool planeIntersection(const Plane &a, const Plane &b, Ray &r);

    IntersectionClass rayPlaneIntersection(const Plane &p,
                                           const Vector &v1,
                                           const Vector &v2,
                                           Vector &v,
                                           double &t);

    IntersectionClass lineSegmentPlaneIntersection(const Plane &p,
                                                   const LineSegment &line,
                                                   Vector &v);

    RayIntersectionClass rayRayIntersection(const Ray &r1,
                                            const Ray &r2,
                                            Vector &v1,
                                            Vector &v2,
                                            double &mu1,
                                            double &mu2);



    // test whether point d is above, below or on the plane formed by the triangle a,b,c.
    // return: +ve = d is below a,b,c
    //         -ve = d is above a,b,c
    //           0 = d is on a,b,c
#if defined CARVE_USE_EXACT_PREDICATES
    inline double orient3d(const Vector &a,
                           const Vector &b,
                           const Vector &c,
                           const Vector &d) {
      return shewchuk::orient3d(a.v, b.v, c.v, d.v);
    }
#else
    inline double orient3d(const Vector &a,
                           const Vector &b,
                           const Vector &c,
                           const Vector &d) {
      return dotcross((a - d), (b - d), (c - d));
    }
#endif

    // Volume of a tetrahedron described by 4 points. Will be
    // positive if the anticlockwise normal of a,b,c is oriented out
    // of the tetrahedron.
    //
    // see: http://mathworld.wolfram.com/Tetrahedron.html
    inline double tetrahedronVolume(const Vector &a,
                                    const Vector &b,
                                    const Vector &c,
                                    const Vector &d) {
      return dotcross((a - d), (b - d), (c - d)) / 6.0;
    }

    /** 
     * \brief Determine whether p is internal to the wedge defined by
     *        the area between the planes defined by a,b,c and a,b,d
     *        angle abc, where ab is the apex of the angle.
     *
     * @param[in] a 
     * @param[in] b 
     * @param[in] c
     * @param[in] d
     * @param[in] p 
     * 
     * @return true, if p is contained in the wedge defined by the
     *               area between the planes defined by a,b,c and
     *               a,b,d. If the wedge is reflex, p is considered to
     *               be contained if it lies on either plane. Acute
     *               wdges do not contain p if p lies on either
     *               plane. This is so that internalToWedge(a,b,c,d,p) =
     *               !internalToWedge(a,b,d,c,p)
     */
    inline bool internalToWedge(const Vector &a,
                                const Vector &b,
                                const Vector &c,
                                const Vector &d,
                                const Vector &p) {
      bool reflex = (c < d) ?
        orient3d(a, b, c, d) >= 0.0 :
        orient3d(a, b, d, c) < 0.0;

      double d1 = orient3d(a, b, c, p);
      double d2 = orient3d(a, b, d, p);

      if (reflex) {
        // above a,b,c or below a,b,d (or coplanar with either)
        return d1 <= 0.0 || d2 >= 0.0;
      } else {
        // above a,b,c and below a,b,d
        return d1 < 0.0 && d2 > 0.0;
      }
    }

    /** 
     * \brief Determine the ordering relationship of a and b, when
     *        rotating around direction, starting from base.
     *
     * @param[in] adirection
     * @param[in] base
     * @param[in] a
     * @param[in] b
     * 
     * @return 
     *         * -1, if a is ordered before b around, rotating about direction.
     *         * 0, if a and b are equal in angle.
     *         * +1, if a is ordered after b around, rotating about direction.
     */
    inline int compareAngles(const Vector &direction, const Vector &base, const Vector &a, const Vector &b) {
      // double d1 = carve::geom3d::orient3d(carve::geom::VECTOR(0,0,0), direction, a, b);
      // double d2 = carve::geom3d::orient3d(carve::geom::VECTOR(0,0,0), direction, base, a);
      // double d3 = carve::geom3d::orient3d(carve::geom::VECTOR(0,0,0), direction, base, b);

#if defined(CARVE_USE_EXACT_PREDICATES)
      // which is equivalent to the following (which eliminates a
      // vector subtraction):
      double d1 = carve::geom3d::orient3d(direction, b, a,    carve::geom::VECTOR(0,0,0));
      double d2 = carve::geom3d::orient3d(direction, a, base, carve::geom::VECTOR(0,0,0));
      double d3 = carve::geom3d::orient3d(direction, b, base, carve::geom::VECTOR(0,0,0));
#else
      // dotcross = a . (b x c)
      double d1 = carve::geom::dotcross(direction, b, a   );
      double d2 = carve::geom::dotcross(direction, a, base);
      double d3 = carve::geom::dotcross(direction, b, base);
#endif

      // CASE: a and b are coplanar wrt. direction.
      if (d1 == 0.0) {
        // a and b point in the same direction.
        if (dot(a, b) > 0.0) {
          // Neither is less than the other.
          return 0;
        }

        // a and b point in opposite directions.
        // * if d2 < 0.0, a is above plane(direction, base) and is less
        //   than b.
        // * if d2 == 0.0 a is coplanar with plane(direction, base) and is
        //   less than b if it points in the same direction as base.
        // * if d2 > 0.0, a is below plane(direction, base) and is greater
        //   than b.

        if (d2 == 0.0) { return dot(a, base) > 0.0 ? -1 : +1; }
        if (d3 == 0.0) { return dot(b, base) > 0.0 ? +1 : -1; }
        if (d2 < 0.0 && d3 > 0.0) return -1;
        if (d2 > 0.0 && d3 < 0.0) return +1;

        // both a and b are to one side of plane(direction, base) -
        // rounding error (if a and b are truly coplanar with
        // direction, one should be above, and one should be below any
        // other plane that is not itself coplanar with
        // plane(direction, a|b) - which would imply d2 and d3 == 0.0).

        // If both are below plane(direction, base) then the one that
        // points in the same direction as base is greater.
        // If both are above plane(direction, base) then the one that
        // points in the same direction as base is lesser.
        if (d2 > 0.0) { return dot(a, base) > 0.0 ? +1 : -1; }
        else          { return dot(a, base) > 0.0 ? -1 : +1; }
      }

      // CASE: a and b are not coplanar wrt. direction

      if (d2 < 0.0) {
        // if a is above plane(direction,base), then a is less than b if
        // b is below plane(direction,base) or b is above plane(direction,a)
        return (d3 > 0.0 || d1 < 0.0) ? -1 : +1;
      } else if (d2 == 0.0) {
        // if a is on plane(direction,base) then a is less than b if a
        // points in the same direction as base, or b is below
        // plane(direction,base)
        return (dot(a, base) > 0.0 || d3 > 0.0) ? -1 : +1;
      } else {
        // if a is below plane(direction,base), then a is less than b if b
        // is below plane(direction,base) and b is above plane(direction,a)
        return (d3 > 0.0 && d1 < 0.0) ? -1 : +1;
      }
    }

    // The anticlockwise angle from vector "from" to vector "to", oriented around the vector "orient".
    static inline double antiClockwiseAngle(const Vector &from, const Vector &to, const Vector &orient) {
      double dp = dot(from, to);
      Vector cp = cross(from, to);
      if (cp.isZero()) {
        if (dp < 0) {
          return M_PI;
        } else {
          return 0.0;
        }
      } else {
        if (dot(cp, orient) > 0.0) {
          return acos(dp);
        } else {
          return M_TWOPI - acos(dp);
        }
      }
    }



    static inline double antiClockwiseOrdering(const Vector &from, const Vector &to, const Vector &orient) {
      double dp = dot(from, to);
      Vector cp = cross(from, to);
      if (cp.isZero()) {
        if (dp < 0) {
          return 2.0;
        } else {
          return 0.0;
        }
      } else {
        if (dot(cp, orient) > 0.0) {
          // 1..-1 -> 0..2
          return 1.0 - dp;
        } else {
          // -1..1 -> 2..4
          return dp + 1.0;
        }
      }
    }



  }
}
