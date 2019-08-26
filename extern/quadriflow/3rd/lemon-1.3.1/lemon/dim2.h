/* -*- mode: C++; indent-tabs-mode: nil; -*-
 *
 * This file is a part of LEMON, a generic C++ optimization library.
 *
 * Copyright (C) 2003-2009
 * Egervary Jeno Kombinatorikus Optimalizalasi Kutatocsoport
 * (Egervary Research Group on Combinatorial Optimization, EGRES).
 *
 * Permission to use, modify and distribute this software is granted
 * provided that this copyright notice appears in all copies. For
 * precise terms see the accompanying LICENSE file.
 *
 * This software is provided "AS IS" with no warranty of any kind,
 * express or implied, and with no claim as to its suitability for any
 * purpose.
 *
 */

#ifndef LEMON_DIM2_H
#define LEMON_DIM2_H

#include <iostream>
#include <algorithm>

///\ingroup geomdat
///\file
///\brief A simple two dimensional vector and a bounding box implementation

namespace lemon {

  ///Tools for handling two dimensional coordinates

  ///This namespace is a storage of several
  ///tools for handling two dimensional coordinates
  namespace dim2 {

  /// \addtogroup geomdat
  /// @{

  /// Two dimensional vector (plain vector)

  /// A simple two dimensional vector (plain vector) implementation
  /// with the usual vector operations.
  template<typename T>
    class Point {

    public:

      typedef T Value;

      ///First coordinate
      T x;
      ///Second coordinate
      T y;

      ///Default constructor
      Point() {}

      ///Construct an instance from coordinates
      Point(T a, T b) : x(a), y(b) { }

      ///Returns the dimension of the vector (i.e. returns 2).

      ///The dimension of the vector.
      ///This function always returns 2.
      int size() const { return 2; }

      ///Subscripting operator

      ///\c p[0] is \c p.x and \c p[1] is \c p.y
      ///
      T& operator[](int idx) { return idx == 0 ? x : y; }

      ///Const subscripting operator

      ///\c p[0] is \c p.x and \c p[1] is \c p.y
      ///
      const T& operator[](int idx) const { return idx == 0 ? x : y; }

      ///Conversion constructor
      template<class TT> Point(const Point<TT> &p) : x(p.x), y(p.y) {}

      ///Give back the square of the norm of the vector
      T normSquare() const {
        return x*x+y*y;
      }

      ///Increment the left hand side by \c u
      Point<T>& operator +=(const Point<T>& u) {
        x += u.x;
        y += u.y;
        return *this;
      }

      ///Decrement the left hand side by \c u
      Point<T>& operator -=(const Point<T>& u) {
        x -= u.x;
        y -= u.y;
        return *this;
      }

      ///Multiply the left hand side with a scalar
      Point<T>& operator *=(const T &u) {
        x *= u;
        y *= u;
        return *this;
      }

      ///Divide the left hand side by a scalar
      Point<T>& operator /=(const T &u) {
        x /= u;
        y /= u;
        return *this;
      }

      ///Return the scalar product of two vectors
      T operator *(const Point<T>& u) const {
        return x*u.x+y*u.y;
      }

      ///Return the sum of two vectors
      Point<T> operator+(const Point<T> &u) const {
        Point<T> b=*this;
        return b+=u;
      }

      ///Return the negative of the vector
      Point<T> operator-() const {
        Point<T> b=*this;
        b.x=-b.x; b.y=-b.y;
        return b;
      }

      ///Return the difference of two vectors
      Point<T> operator-(const Point<T> &u) const {
        Point<T> b=*this;
        return b-=u;
      }

      ///Return a vector multiplied by a scalar
      Point<T> operator*(const T &u) const {
        Point<T> b=*this;
        return b*=u;
      }

      ///Return a vector divided by a scalar
      Point<T> operator/(const T &u) const {
        Point<T> b=*this;
        return b/=u;
      }

      ///Test equality
      bool operator==(const Point<T> &u) const {
        return (x==u.x) && (y==u.y);
      }

      ///Test inequality
      bool operator!=(Point u) const {
        return  (x!=u.x) || (y!=u.y);
      }

    };

  ///Return a Point

  ///Return a Point.
  ///\relates Point
  template <typename T>
  inline Point<T> makePoint(const T& x, const T& y) {
    return Point<T>(x, y);
  }

  ///Return a vector multiplied by a scalar

  ///Return a vector multiplied by a scalar.
  ///\relates Point
  template<typename T> Point<T> operator*(const T &u,const Point<T> &x) {
    return x*u;
  }

  ///Read a plain vector from a stream

  ///Read a plain vector from a stream.
  ///\relates Point
  ///
  template<typename T>
  inline std::istream& operator>>(std::istream &is, Point<T> &z) {
    char c;
    if (is >> c) {
      if (c != '(') is.putback(c);
    } else {
      is.clear();
    }
    if (!(is >> z.x)) return is;
    if (is >> c) {
      if (c != ',') is.putback(c);
    } else {
      is.clear();
    }
    if (!(is >> z.y)) return is;
    if (is >> c) {
      if (c != ')') is.putback(c);
    } else {
      is.clear();
    }
    return is;
  }

  ///Write a plain vector to a stream

  ///Write a plain vector to a stream.
  ///\relates Point
  ///
  template<typename T>
  inline std::ostream& operator<<(std::ostream &os, const Point<T>& z)
  {
    os << "(" << z.x << "," << z.y << ")";
    return os;
  }

  ///Rotate by 90 degrees

  ///Returns the parameter rotated by 90 degrees in positive direction.
  ///\relates Point
  ///
  template<typename T>
  inline Point<T> rot90(const Point<T> &z)
  {
    return Point<T>(-z.y,z.x);
  }

  ///Rotate by 180 degrees

  ///Returns the parameter rotated by 180 degrees.
  ///\relates Point
  ///
  template<typename T>
  inline Point<T> rot180(const Point<T> &z)
  {
    return Point<T>(-z.x,-z.y);
  }

  ///Rotate by 270 degrees

  ///Returns the parameter rotated by 90 degrees in negative direction.
  ///\relates Point
  ///
  template<typename T>
  inline Point<T> rot270(const Point<T> &z)
  {
    return Point<T>(z.y,-z.x);
  }



  /// Bounding box of plain vectors (points).

  /// A class to calculate or store the bounding box of plain vectors
  /// (\ref Point "points").
  template<typename T>
  class Box {
      Point<T> _bottom_left, _top_right;
      bool _empty;
    public:

      ///Default constructor: creates an empty box
      Box() { _empty = true; }

      ///Construct a box from one point
      Box(Point<T> a) {
        _bottom_left = _top_right = a;
        _empty = false;
      }

      ///Construct a box from two points

      ///Construct a box from two points.
      ///\param a The bottom left corner.
      ///\param b The top right corner.
      ///\warning The coordinates of the bottom left corner must be no more
      ///than those of the top right one.
      Box(Point<T> a,Point<T> b)
      {
        _bottom_left = a;
        _top_right = b;
        _empty = false;
      }

      ///Construct a box from four numbers

      ///Construct a box from four numbers.
      ///\param l The left side of the box.
      ///\param b The bottom of the box.
      ///\param r The right side of the box.
      ///\param t The top of the box.
      ///\warning The left side must be no more than the right side and
      ///bottom must be no more than the top.
      Box(T l,T b,T r,T t)
      {
        _bottom_left=Point<T>(l,b);
        _top_right=Point<T>(r,t);
        _empty = false;
      }

      ///Return \c true if the box is empty.

      ///Return \c true if the box is empty (i.e. return \c false
      ///if at least one point was added to the box or the coordinates of
      ///the box were set).
      ///
      ///The coordinates of an empty box are not defined.
      bool empty() const {
        return _empty;
      }

      ///Make the box empty
      void clear() {
        _empty = true;
      }

      ///Give back the bottom left corner of the box

      ///Give back the bottom left corner of the box.
      ///If the box is empty, then the return value is not defined.
      Point<T> bottomLeft() const {
        return _bottom_left;
      }

      ///Set the bottom left corner of the box

      ///Set the bottom left corner of the box.
      ///\pre The box must not be empty.
      void bottomLeft(Point<T> p) {
        _bottom_left = p;
      }

      ///Give back the top right corner of the box

      ///Give back the top right corner of the box.
      ///If the box is empty, then the return value is not defined.
      Point<T> topRight() const {
        return _top_right;
      }

      ///Set the top right corner of the box

      ///Set the top right corner of the box.
      ///\pre The box must not be empty.
      void topRight(Point<T> p) {
        _top_right = p;
      }

      ///Give back the bottom right corner of the box

      ///Give back the bottom right corner of the box.
      ///If the box is empty, then the return value is not defined.
      Point<T> bottomRight() const {
        return Point<T>(_top_right.x,_bottom_left.y);
      }

      ///Set the bottom right corner of the box

      ///Set the bottom right corner of the box.
      ///\pre The box must not be empty.
      void bottomRight(Point<T> p) {
        _top_right.x = p.x;
        _bottom_left.y = p.y;
      }

      ///Give back the top left corner of the box

      ///Give back the top left corner of the box.
      ///If the box is empty, then the return value is not defined.
      Point<T> topLeft() const {
        return Point<T>(_bottom_left.x,_top_right.y);
      }

      ///Set the top left corner of the box

      ///Set the top left corner of the box.
      ///\pre The box must not be empty.
      void topLeft(Point<T> p) {
        _top_right.y = p.y;
        _bottom_left.x = p.x;
      }

      ///Give back the bottom of the box

      ///Give back the bottom of the box.
      ///If the box is empty, then the return value is not defined.
      T bottom() const {
        return _bottom_left.y;
      }

      ///Set the bottom of the box

      ///Set the bottom of the box.
      ///\pre The box must not be empty.
      void bottom(T t) {
        _bottom_left.y = t;
      }

      ///Give back the top of the box

      ///Give back the top of the box.
      ///If the box is empty, then the return value is not defined.
      T top() const {
        return _top_right.y;
      }

      ///Set the top of the box

      ///Set the top of the box.
      ///\pre The box must not be empty.
      void top(T t) {
        _top_right.y = t;
      }

      ///Give back the left side of the box

      ///Give back the left side of the box.
      ///If the box is empty, then the return value is not defined.
      T left() const {
        return _bottom_left.x;
      }

      ///Set the left side of the box

      ///Set the left side of the box.
      ///\pre The box must not be empty.
      void left(T t) {
        _bottom_left.x = t;
      }

      /// Give back the right side of the box

      /// Give back the right side of the box.
      ///If the box is empty, then the return value is not defined.
      T right() const {
        return _top_right.x;
      }

      ///Set the right side of the box

      ///Set the right side of the box.
      ///\pre The box must not be empty.
      void right(T t) {
        _top_right.x = t;
      }

      ///Give back the height of the box

      ///Give back the height of the box.
      ///If the box is empty, then the return value is not defined.
      T height() const {
        return _top_right.y-_bottom_left.y;
      }

      ///Give back the width of the box

      ///Give back the width of the box.
      ///If the box is empty, then the return value is not defined.
      T width() const {
        return _top_right.x-_bottom_left.x;
      }

      ///Checks whether a point is inside the box
      bool inside(const Point<T>& u) const {
        if (_empty)
          return false;
        else {
          return ( (u.x-_bottom_left.x)*(_top_right.x-u.x) >= 0 &&
                   (u.y-_bottom_left.y)*(_top_right.y-u.y) >= 0 );
        }
      }

      ///Increments the box with a point

      ///Increments the box with a point.
      ///
      Box& add(const Point<T>& u){
        if (_empty) {
          _bottom_left = _top_right = u;
          _empty = false;
        }
        else {
          if (_bottom_left.x > u.x) _bottom_left.x = u.x;
          if (_bottom_left.y > u.y) _bottom_left.y = u.y;
          if (_top_right.x < u.x) _top_right.x = u.x;
          if (_top_right.y < u.y) _top_right.y = u.y;
        }
        return *this;
      }

      ///Increments the box to contain another box

      ///Increments the box to contain another box.
      ///
      Box& add(const Box &u){
        if ( !u.empty() ){
          add(u._bottom_left);
          add(u._top_right);
        }
        return *this;
      }

      ///Intersection of two boxes

      ///Intersection of two boxes.
      ///
      Box operator&(const Box& u) const {
        Box b;
        if (_empty || u._empty) {
          b._empty = true;
        } else {
          b._bottom_left.x = std::max(_bottom_left.x, u._bottom_left.x);
          b._bottom_left.y = std::max(_bottom_left.y, u._bottom_left.y);
          b._top_right.x = std::min(_top_right.x, u._top_right.x);
          b._top_right.y = std::min(_top_right.y, u._top_right.y);
          b._empty = b._bottom_left.x > b._top_right.x ||
                     b._bottom_left.y > b._top_right.y;
        }
        return b;
      }

  };//class Box


  ///Read a box from a stream

  ///Read a box from a stream.
  ///\relates Box
  template<typename T>
  inline std::istream& operator>>(std::istream &is, Box<T>& b) {
    char c;
    Point<T> p;
    if (is >> c) {
      if (c != '(') is.putback(c);
    } else {
      is.clear();
    }
    if (!(is >> p)) return is;
    b.bottomLeft(p);
    if (is >> c) {
      if (c != ',') is.putback(c);
    } else {
      is.clear();
    }
    if (!(is >> p)) return is;
    b.topRight(p);
    if (is >> c) {
      if (c != ')') is.putback(c);
    } else {
      is.clear();
    }
    return is;
  }

  ///Write a box to a stream

  ///Write a box to a stream.
  ///\relates Box
  template<typename T>
  inline std::ostream& operator<<(std::ostream &os, const Box<T>& b)
  {
    os << "(" << b.bottomLeft() << "," << b.topRight() << ")";
    return os;
  }

  ///Map of x-coordinates of a <tt>Point</tt>-map

  ///Map of x-coordinates of a \ref Point "Point"-map.
  ///
  template<class M>
  class XMap
  {
    M& _map;
  public:

    typedef typename M::Value::Value Value;
    typedef typename M::Key Key;
    ///\e
    XMap(M& map) : _map(map) {}
    Value operator[](Key k) const {return _map[k].x;}
    void set(Key k,Value v) {_map.set(k,typename M::Value(v,_map[k].y));}
  };

  ///Returns an XMap class

  ///This function just returns an XMap class.
  ///\relates XMap
  template<class M>
  inline XMap<M> xMap(M &m)
  {
    return XMap<M>(m);
  }

  template<class M>
  inline XMap<M> xMap(const M &m)
  {
    return XMap<M>(m);
  }

  ///Constant (read only) version of XMap

  ///Constant (read only) version of XMap.
  ///
  template<class M>
  class ConstXMap
  {
    const M& _map;
  public:

    typedef typename M::Value::Value Value;
    typedef typename M::Key Key;
    ///\e
    ConstXMap(const M &map) : _map(map) {}
    Value operator[](Key k) const {return _map[k].x;}
  };

  ///Returns a ConstXMap class

  ///This function just returns a ConstXMap class.
  ///\relates ConstXMap
  template<class M>
  inline ConstXMap<M> xMap(const M &m)
  {
    return ConstXMap<M>(m);
  }

  ///Map of y-coordinates of a <tt>Point</tt>-map

  ///Map of y-coordinates of a \ref Point "Point"-map.
  ///
  template<class M>
  class YMap
  {
    M& _map;
  public:

    typedef typename M::Value::Value Value;
    typedef typename M::Key Key;
    ///\e
    YMap(M& map) : _map(map) {}
    Value operator[](Key k) const {return _map[k].y;}
    void set(Key k,Value v) {_map.set(k,typename M::Value(_map[k].x,v));}
  };

  ///Returns a YMap class

  ///This function just returns a YMap class.
  ///\relates YMap
  template<class M>
  inline YMap<M> yMap(M &m)
  {
    return YMap<M>(m);
  }

  template<class M>
  inline YMap<M> yMap(const M &m)
  {
    return YMap<M>(m);
  }

  ///Constant (read only) version of YMap

  ///Constant (read only) version of YMap.
  ///
  template<class M>
  class ConstYMap
  {
    const M& _map;
  public:

    typedef typename M::Value::Value Value;
    typedef typename M::Key Key;
    ///\e
    ConstYMap(const M &map) : _map(map) {}
    Value operator[](Key k) const {return _map[k].y;}
  };

  ///Returns a ConstYMap class

  ///This function just returns a ConstYMap class.
  ///\relates ConstYMap
  template<class M>
  inline ConstYMap<M> yMap(const M &m)
  {
    return ConstYMap<M>(m);
  }


  ///\brief Map of the normSquare() of a <tt>Point</tt>-map
  ///
  ///Map of the \ref Point::normSquare() "normSquare()"
  ///of a \ref Point "Point"-map.
  template<class M>
  class NormSquareMap
  {
    const M& _map;
  public:

    typedef typename M::Value::Value Value;
    typedef typename M::Key Key;
    ///\e
    NormSquareMap(const M &map) : _map(map) {}
    Value operator[](Key k) const {return _map[k].normSquare();}
  };

  ///Returns a NormSquareMap class

  ///This function just returns a NormSquareMap class.
  ///\relates NormSquareMap
  template<class M>
  inline NormSquareMap<M> normSquareMap(const M &m)
  {
    return NormSquareMap<M>(m);
  }

  /// @}

  } //namespce dim2

} //namespace lemon

#endif //LEMON_DIM2_H
