/* -*- mode: C++; indent-tabs-mode: nil; -*-
 *
 * This file is a part of LEMON, a generic C++ optimization library.
 *
 * Copyright (C) 2003-2013
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

#ifndef LEMON_BEZIER_H
#define LEMON_BEZIER_H

//\ingroup misc
//\file
//\brief Classes to compute with Bezier curves.
//
//Up to now this file is used internally by \ref graph_to_eps.h

#include<lemon/dim2.h>

namespace lemon {
  namespace dim2 {

class BezierBase {
public:
  typedef lemon::dim2::Point<double> Point;
protected:
  static Point conv(Point x,Point y,double t) {return (1-t)*x+t*y;}
};

class Bezier1 : public BezierBase
{
public:
  Point p1,p2;

  Bezier1() {}
  Bezier1(Point _p1, Point _p2) :p1(_p1), p2(_p2) {}

  Point operator()(double t) const
  {
    //    return conv(conv(p1,p2,t),conv(p2,p3,t),t);
    return conv(p1,p2,t);
  }
  Bezier1 before(double t) const
  {
    return Bezier1(p1,conv(p1,p2,t));
  }

  Bezier1 after(double t) const
  {
    return Bezier1(conv(p1,p2,t),p2);
  }

  Bezier1 revert() const { return Bezier1(p2,p1);}
  Bezier1 operator()(double a,double b) const { return before(b).after(a/b); }
  Point grad() const { return p2-p1; }
  Point norm() const { return rot90(p2-p1); }
  Point grad(double) const { return grad(); }
  Point norm(double t) const { return rot90(grad(t)); }
};

class Bezier2 : public BezierBase
{
public:
  Point p1,p2,p3;

  Bezier2() {}
  Bezier2(Point _p1, Point _p2, Point _p3) :p1(_p1), p2(_p2), p3(_p3) {}
  Bezier2(const Bezier1 &b) : p1(b.p1), p2(conv(b.p1,b.p2,.5)), p3(b.p2) {}
  Point operator()(double t) const
  {
    //    return conv(conv(p1,p2,t),conv(p2,p3,t),t);
    return ((1-t)*(1-t))*p1+(2*(1-t)*t)*p2+(t*t)*p3;
  }
  Bezier2 before(double t) const
  {
    Point q(conv(p1,p2,t));
    Point r(conv(p2,p3,t));
    return Bezier2(p1,q,conv(q,r,t));
  }

  Bezier2 after(double t) const
  {
    Point q(conv(p1,p2,t));
    Point r(conv(p2,p3,t));
    return Bezier2(conv(q,r,t),r,p3);
  }
  Bezier2 revert() const { return Bezier2(p3,p2,p1);}
  Bezier2 operator()(double a,double b) const { return before(b).after(a/b); }
  Bezier1 grad() const { return Bezier1(2.0*(p2-p1),2.0*(p3-p2)); }
  Bezier1 norm() const { return Bezier1(2.0*rot90(p2-p1),2.0*rot90(p3-p2)); }
  Point grad(double t) const { return grad()(t); }
  Point norm(double t) const { return rot90(grad(t)); }
};

class Bezier3 : public BezierBase
{
public:
  Point p1,p2,p3,p4;

  Bezier3() {}
  Bezier3(Point _p1, Point _p2, Point _p3, Point _p4)
    : p1(_p1), p2(_p2), p3(_p3), p4(_p4) {}
  Bezier3(const Bezier1 &b) : p1(b.p1), p2(conv(b.p1,b.p2,1.0/3.0)),
                              p3(conv(b.p1,b.p2,2.0/3.0)), p4(b.p2) {}
  Bezier3(const Bezier2 &b) : p1(b.p1), p2(conv(b.p1,b.p2,2.0/3.0)),
                              p3(conv(b.p2,b.p3,1.0/3.0)), p4(b.p3) {}

  Point operator()(double t) const
    {
      //    return Bezier2(conv(p1,p2,t),conv(p2,p3,t),conv(p3,p4,t))(t);
      return ((1-t)*(1-t)*(1-t))*p1+(3*t*(1-t)*(1-t))*p2+
        (3*t*t*(1-t))*p3+(t*t*t)*p4;
    }
  Bezier3 before(double t) const
    {
      Point p(conv(p1,p2,t));
      Point q(conv(p2,p3,t));
      Point r(conv(p3,p4,t));
      Point a(conv(p,q,t));
      Point b(conv(q,r,t));
      Point c(conv(a,b,t));
      return Bezier3(p1,p,a,c);
    }

  Bezier3 after(double t) const
    {
      Point p(conv(p1,p2,t));
      Point q(conv(p2,p3,t));
      Point r(conv(p3,p4,t));
      Point a(conv(p,q,t));
      Point b(conv(q,r,t));
      Point c(conv(a,b,t));
      return Bezier3(c,b,r,p4);
    }
  Bezier3 revert() const { return Bezier3(p4,p3,p2,p1);}
  Bezier3 operator()(double a,double b) const { return before(b).after(a/b); }
  Bezier2 grad() const { return Bezier2(3.0*(p2-p1),3.0*(p3-p2),3.0*(p4-p3)); }
  Bezier2 norm() const { return Bezier2(3.0*rot90(p2-p1),
                                  3.0*rot90(p3-p2),
                                  3.0*rot90(p4-p3)); }
  Point grad(double t) const { return grad()(t); }
  Point norm(double t) const { return rot90(grad(t)); }

  template<class R,class F,class S,class D>
  R recSplit(F &_f,const S &_s,D _d) const
  {
    const Point a=(p1+p2)/2;
    const Point b=(p2+p3)/2;
    const Point c=(p3+p4)/2;
    const Point d=(a+b)/2;
    const Point e=(b+c)/2;
    // const Point f=(d+e)/2;
    R f1=_f(Bezier3(p1,a,d,e),_d);
    R f2=_f(Bezier3(e,d,c,p4),_d);
    return _s(f1,f2);
  }

};


} //END OF NAMESPACE dim2
} //END OF NAMESPACE lemon

#endif // LEMON_BEZIER_H
