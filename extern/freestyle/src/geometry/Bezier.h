//
//  Filename         : Bezier.h
//  Author(s)        : Stephane Grabli
//  Purpose          : Class to define a Bezier curve of order 4.
//  Date of creation : 04/06/2003
//
///////////////////////////////////////////////////////////////////////////////


//
//  Copyright (C) : Please refer to the COPYRIGHT file distributed 
//   with this source distribution. 
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 2
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//
///////////////////////////////////////////////////////////////////////////////

#ifndef  BEZIER_H
# define BEZIER_H

#include <vector>
#include "../system/FreestyleConfig.h"
#include "Geom.h"

using namespace Geometry;

class LIB_GEOMETRY_EXPORT BezierCurveSegment 
{
private:
  std::vector<Vec2d> _ControlPolygon;
  std::vector<Vec2d> _Vertices;

public:
  BezierCurveSegment();
  virtual ~BezierCurveSegment();

  void AddControlPoint(const Vec2d& iPoint);
  void Build();
  inline int size() const {return _ControlPolygon.size();}
  inline std::vector<Vec2d>& vertices() {return _Vertices;}
};


class LIB_GEOMETRY_EXPORT BezierCurve 
{
private:
  std::vector<Vec2d> _ControlPolygon;
  std::vector<BezierCurveSegment*> _Segments;
  BezierCurveSegment *_currentSegment;

public:
  BezierCurve();
  BezierCurve(std::vector<Vec2d>& iPoints, double error=4.0);
  virtual ~BezierCurve();

  void AddControlPoint(const Vec2d& iPoint);
  std::vector<Vec2d>& controlPolygon() {return _ControlPolygon;}
  std::vector<BezierCurveSegment*>& segments() {return _Segments;}
};

#endif // BEZIER_H
