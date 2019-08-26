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

#include <lemon/dim2.h>
#include <iostream>
#include "test_tools.h"

using namespace std;
using namespace lemon;

int main()
{
  typedef dim2::Point<int> Point;

  Point p;
  check(p.size()==2, "Wrong dim2::Point initialization.");

  Point a(1,2);
  Point b(3,4);
  check(a[0]==1 && a[1]==2, "Wrong dim2::Point initialization.");

  p = a+b;
  check(p.x==4 && p.y==6, "Wrong dim2::Point addition.");

  p = a-b;
  check(p.x==-2 && p.y==-2, "Wrong dim2::Point subtraction.");

  check(a.normSquare()==5,"Wrong dim2::Point norm calculation.");
  check(a*b==11, "Wrong dim2::Point scalar product.");

  int l=2;
  p = a*l;
  check(p.x==2 && p.y==4, "Wrong dim2::Point multiplication by a scalar.");

  p = b/l;
  check(p.x==1 && p.y==2, "Wrong dim2::Point division by a scalar.");

  typedef dim2::Box<int> Box;
  Box box1;
  check(box1.empty(), "Wrong empty() in dim2::Box.");

  box1.add(a);
  check(!box1.empty(), "Wrong empty() in dim2::Box.");
  box1.add(b);

  check(box1.left()==1 && box1.bottom()==2 &&
        box1.right()==3 && box1.top()==4,
        "Wrong addition of points to dim2::Box.");

  check(box1.inside(Point(2,3)), "Wrong inside() in dim2::Box.");
  check(box1.inside(Point(1,3)), "Wrong inside() in dim2::Box.");
  check(!box1.inside(Point(0,3)), "Wrong inside() in dim2::Box.");

  Box box2(Point(2,2));
  check(!box2.empty(), "Wrong empty() in dim2::Box.");

  box2.bottomLeft(Point(2,0));
  box2.topRight(Point(5,3));
  Box box3 = box1 & box2;
  check(!box3.empty() &&
        box3.left()==2 && box3.bottom()==2 &&
        box3.right()==3 && box3.top()==3,
        "Wrong intersection of two dim2::Box objects.");

  box1.add(box2);
  check(!box1.empty() &&
        box1.left()==1 && box1.bottom()==0 &&
        box1.right()==5 && box1.top()==4,
        "Wrong addition of two dim2::Box objects.");

  return 0;
}
