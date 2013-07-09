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

#include <vector>

namespace carve {
  namespace geom {

    template<unsigned ndim> struct aabb;

    // ========================================================================
    struct _uninitialized { };

    template<unsigned ndim>
    struct base {
      double v[ndim];
    };

    template<> struct base<2> {union { double v[2]; struct { double x, y; }; }; };
    template<> struct base<3> {union { double v[3]; struct { double x, y, z; }; }; };
    template<> struct base<4> {union { double v[4]; struct { double x, y, z, w; }; }; };

    template<unsigned ndim>
    struct vector : public base<ndim> {
      enum { __ndim = ndim };

      static vector ZERO();
      double length2() const;
      double length() const;
      vector<ndim> &normalize();
      vector<ndim> normalized() const;
      bool exactlyZero() const;
      bool isZero(double epsilon = EPSILON) const;
      void setZero();
      void fill(double val);
      vector<ndim> &scaleBy(double d);
      vector<ndim> &invscaleBy(double d);
      vector<ndim> scaled(double d) const;
      vector<ndim> invscaled(double d) const;
      vector<ndim> &negate();
      vector<ndim> negated() const;
      double &operator[](unsigned i);
      const double &operator[](unsigned i) const;
      template<typename assign_t>
      vector<ndim> &operator=(const assign_t &t);
      std::string asStr() const;

      aabb<ndim> getAABB() const;

      vector() { setZero(); }
      vector(noinit_t) { }
    };

    template<unsigned ndim>
    vector<ndim> vector<ndim>::ZERO() { vector<ndim> r; r.setZero(); return r; }

    static inline vector<2> VECTOR(double x, double y) { vector<2> r; r.x = x; r.y = y; return r; }
    static inline vector<3> VECTOR(double x, double y, double z) { vector<3> r; r.x = x; r.y = y; r.z = z; return r; }
    static inline vector<4> VECTOR(double x, double y, double z, double w) { vector<4> r; r.x = x; r.y = y; r.z = z; r.w = w; return r; }

    template<unsigned ndim> vector<ndim> operator+(const vector<ndim> &a, const vector<ndim> &b);
    template<unsigned ndim> vector<ndim> operator+(const vector<ndim> &a, double b);
    template<unsigned ndim, typename val_t> vector<ndim> operator+(const vector<ndim> &a, const val_t &b);
    template<unsigned ndim, typename val_t> vector<ndim> operator+(const val_t &a, const vector<ndim> &b);

    template<unsigned ndim> vector<ndim> &operator+=(vector<ndim> &a, const vector<ndim> &b);
    template<unsigned ndim> vector<ndim> &operator+=(vector<ndim> &a, double b);
    template<unsigned ndim, typename val_t> vector<ndim> &operator+=(vector<ndim> &a, const val_t &b);

    template<unsigned ndim> vector<ndim> operator-(const vector<ndim> &a);

    template<unsigned ndim> vector<ndim> operator-(const vector<ndim> &a, const vector<ndim> &b);
    template<unsigned ndim> vector<ndim> operator-(const vector<ndim> &a, double b);
    template<unsigned ndim, typename val_t> vector<ndim> operator-(const vector<ndim> &a, const val_t &b);
    template<unsigned ndim, typename val_t> vector<ndim> operator-(const val_t &a, const vector<ndim> &b);

    template<unsigned ndim> vector<ndim> &operator-=(vector<ndim> &a, const vector<ndim> &b);
    template<unsigned ndim> vector<ndim> &operator-=(vector<ndim> &a, double b);
    template<unsigned ndim, typename val_t> vector<ndim> &operator-=(vector<ndim> &a, const val_t &b);

    template<unsigned ndim> vector<ndim> operator*(const vector<ndim> &a, double s);
    template<unsigned ndim> vector<ndim> operator*(double s, const vector<ndim> &a);
    template<unsigned ndim> vector<ndim> &operator*=(vector<ndim> &a, double s);

    template<unsigned ndim> vector<ndim> operator/(const vector<ndim> &a, double s);
    template<unsigned ndim> vector<ndim> &operator/=(vector<ndim> &a, double s);

    template<unsigned ndim> bool operator==(const vector<ndim> &a, const vector<ndim> &b);
    template<unsigned ndim> bool operator!=(const vector<ndim> &a, const vector<ndim> &b);
    template<unsigned ndim> bool operator<(const vector<ndim> &a, const vector<ndim> &b);
    template<unsigned ndim> bool operator<=(const vector<ndim> &a, const vector<ndim> &b);
    template<unsigned ndim> bool operator>(const vector<ndim> &a, const vector<ndim> &b);
    template<unsigned ndim> bool operator>=(const vector<ndim> &a, const vector<ndim> &b);

    template<unsigned ndim> vector<ndim> abs(const vector<ndim> &a);

    template<unsigned ndim> double distance2(const vector<ndim> &a, const vector<ndim> &b);

    template<unsigned ndim> double distance(const vector<ndim> &a, const vector<ndim> &b);

    template<unsigned ndim> bool equal(const vector<ndim> &a, const vector<ndim> &b);

    template<unsigned ndim> int smallestAxis(const vector<ndim> &a);

    template<unsigned ndim> int largestAxis(const vector<ndim> &a);

    template<unsigned ndim> vector<2> select(const vector<ndim> &a, int a1, int a2);

    template<unsigned ndim> vector<3> select(const vector<ndim> &a, int a1, int a2, int a3);

    template<unsigned ndim, typename assign_t, typename oper_t>
    vector<ndim> &assign_op(vector<ndim> &a, const assign_t &t, oper_t op);

    template<unsigned ndim, typename assign1_t, typename assign2_t, typename oper_t>
    vector<ndim> &assign_op(vector<ndim> &a, const assign1_t &t1, const assign2_t &t2, oper_t op);

    template<unsigned ndim, typename iter_t>
    void bounds(iter_t begin, iter_t end, vector<ndim> &min, vector<ndim> &max);

    template<unsigned ndim, typename iter_t, typename adapt_t>
    void bounds(iter_t begin, iter_t end, adapt_t adapt, vector<ndim> &min, vector<ndim> &max);

    template<unsigned ndim, typename iter_t, typename adapt_t>
    void centroid(iter_t begin, iter_t end, adapt_t adapt, vector<ndim> &c);

    template<unsigned ndim, typename val_t> double dot(const vector<ndim> &a, const val_t &b);

    static inline vector<3> cross(const vector<3> &a, const vector<3> &b);

    static inline double cross(const vector<2> &a, const vector<2> &b);

    static inline double dotcross(const vector<3> &a, const vector<3> &b, const vector<3> &c);



    // ========================================================================
    struct axis_pos {
      int axis;
      double pos;

      axis_pos(int _axis, double _pos) : axis(_axis), pos(_pos) { }
    };

    template<unsigned ndim>
    double distance(const axis_pos &a, const vector<ndim> &b);

    template<unsigned ndim>
    double distance2(const axis_pos &a, const vector<ndim> &b);

    template<unsigned ndim> bool operator<(const axis_pos &a, const vector<ndim> &b);
    template<unsigned ndim> bool operator<(const vector<ndim> &a, const axis_pos &b);

    template<unsigned ndim> bool operator<=(const axis_pos &a, const vector<ndim> &b);
    template<unsigned ndim> bool operator<=(const vector<ndim> &a, const axis_pos &b);

    template<unsigned ndim> bool operator>(const axis_pos &a, const vector<ndim> &b);
    template<unsigned ndim> bool operator>(const vector<ndim> &a, const axis_pos &b);

    template<unsigned ndim> bool operator>=(const axis_pos &a, const vector<ndim> &b);
    template<unsigned ndim> bool operator>=(const vector<ndim> &a, const axis_pos &b);

    template<unsigned ndim> bool operator==(const axis_pos &a, const vector<ndim> &b);
    template<unsigned ndim> bool operator==(const vector<ndim> &a, const axis_pos &b);

    template<unsigned ndim> bool operator!=(const axis_pos &a, const vector<ndim> &b);
    template<unsigned ndim> bool operator!=(const vector<ndim> &a, const axis_pos &b);



    // ========================================================================
    template<unsigned ndim>
    struct ray {
      typedef vector<ndim> vector_t;

      vector_t D, v;

      bool OK() const;

      ray() { }
      ray(vector_t _D, vector_t _v) : D(_D), v(_v) { }
    };

    template<unsigned ndim>
    ray<ndim> rayThrough(const vector<ndim> &a, const vector<ndim> &b);

    static inline double distance2(const ray<3> &r, const vector<3> &v);

    static inline double distance(const ray<3> &r, const vector<3> &v);

    static inline double distance2(const ray<2> &r, const vector<2> &v);

    static inline double distance(const ray<2> &r, const vector<2> &v);



    // ========================================================================
    template<unsigned ndim>
    struct linesegment {
      typedef vector<ndim> vector_t;

      vector_t v1;
      vector_t v2;
      vector_t midpoint;
      vector_t half_length;

      void update();
      bool OK() const;
      void flip();

      aabb<ndim> getAABB() const;

      linesegment(const vector_t &_v1, const vector_t &_v2);
    };

    template<unsigned ndim>
    double distance2(const linesegment<ndim> &l, const vector<ndim> &v);

    template<unsigned ndim>
    double distance(const linesegment<ndim> &l, const vector<ndim> &v);



    // ========================================================================
    template<unsigned ndim>
    struct plane {
      typedef vector<ndim> vector_t;

      vector_t N;
      double d;

      void negate();

      plane();
      plane(const vector_t &_N, vector_t _p);
      plane(const vector_t &_N, double _d);
    };

    template<unsigned ndim>
    inline plane<ndim> operator-(const plane<ndim> &p);

    template<unsigned ndim, typename val_t>
    double distance(const plane<ndim> &plane, const val_t &point);

    template<unsigned ndim, typename val_t>
    double distance2(const plane<ndim> &plane, const val_t &point);

    template<unsigned ndim>
    static inline vector<ndim> closestPoint(const plane<ndim> &p, const vector<ndim> &v);



    // ========================================================================
    template<unsigned ndim>
    struct sphere {
      typedef vector<ndim> vector_t;

      vector_t C;
      double r;

      aabb<ndim> getAABB() const;

      sphere();
      sphere(const vector_t &_C, double _r);
    };

    template<unsigned ndim, typename val_t>
    double distance(const sphere<ndim> &sphere, const val_t &point);

    template<unsigned ndim, typename val_t>
    double distance2(const sphere<ndim> &sphere, const val_t &point);

    template<unsigned ndim>
    static inline vector<ndim> closestPoint(const sphere<ndim> &sphere, const vector<ndim> &point);


    // ========================================================================
    template<unsigned ndim>
    struct tri {
      typedef vector<ndim> vector_t;

      vector_t v[3];

      aabb<ndim> getAABB() const;

      tri(vector_t _v[3]);
      tri(const vector_t &a, const vector_t &b, const vector_t &c);

      vector_t normal() const {
        return cross(v[1] - v[0], v[2] - v[1]).normalized();
      }
    };



    template<unsigned ndim> std::ostream &operator<<(std::ostream &o, const vector<ndim> &v);
    template<unsigned ndim> std::ostream &operator<<(std::ostream &o, const carve::geom::plane<ndim> &p);
    template<unsigned ndim> std::ostream &operator<<(std::ostream &o, const carve::geom::sphere<ndim> &sphere);
    template<unsigned ndim> std::ostream &operator<<(std::ostream &o, const carve::geom::tri<ndim> &tri);



    template<unsigned ndim> vector<ndim> closestPoint(const tri<ndim> &tri, const vector<ndim> &pt);
    template<unsigned ndim> double distance(const tri<ndim> &tri, const vector<ndim> &pt);
    template<unsigned ndim> double distance2(const tri<ndim> &tri, const vector<ndim> &pt);



    // ========================================================================
    struct distance_functor {
      template<typename obj1_t, typename obj2_t>
      double operator()(const obj1_t &o1, const obj2_t &o2) {
        return distance(o1, o2);
      }
    };



    // ========================================================================
    template<int base, int power> struct __pow__          { enum { val = __pow__<base, (power >> 1)>::val * __pow__<base, power - (power >> 1)>::val }; };
    template<int base>            struct __pow__<base, 1> { enum { val = base }; };
    template<int base>            struct __pow__<base, 0> { enum { val = 1 }; };

    template<unsigned base, unsigned ndigits>
    struct quantize {
      typedef __pow__<base, ndigits> fac;

      double operator()(double in) {
        return round(in * fac::val) / fac::val;
      }

      template<unsigned ndim>
      vector<ndim> operator()(const vector<ndim> &in) {
        vector<ndim> r(NOINIT);
        assign_op(r, in, *this);
        return r;
      }
    };



  }
}


#include <carve/geom_impl.hpp>
