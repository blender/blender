#ifndef __CSG_MATH_H
#define __CSG_MATH_H
/*
  CSGLib - Software Library for Constructive Solid Geometry
  Copyright (C) 2003-2004  Laurence Bourn

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Library General Public
  License as published by the Free Software Foundation; either
  version 2 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Library General Public License for more details.

  You should have received a copy of the GNU Library General Public
  License along with this library; if not, write to the Free
  Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

  Please send remarks, questions and bug reports to laurencebourn@hotmail.com
*/

#include "MT_Plane3.h"
#include "MT_Matrix3x3.h"
#include "MT_Line3.h"
#include "CSG_BBox.h"

// useful geometry functions.
/////////////////////////////

const int cofacTable[3][2] = {{1,2},{0,2},{0,1}};

class CSG_Geometry
{
public :

	 static bool Intersect(
		const MT_Plane3& p1,
		const MT_Plane3& p2,
		MT_Line3& output
	);
 
	// Return intersection of line1 and line 2 in the plane formed by the given
	// standard axis. Intersection l1Param is the intersection parameter of line1.
	 static bool Intersect2D(
		const MT_Line3& l1,
		const MT_Line3& l2,
		int majAxis,
		MT_Scalar& l1Param
	);
		
	 static bool Intersect2DBoundsCheck(
		const MT_Line3& l1,
		const MT_Line3& l2,
		int majAxis,
		MT_Scalar& l1Param,
		MT_Scalar& l2Param
	);

	 static bool Intersect2DNoBoundsCheck(
		const MT_Line3& l1,
		const MT_Line3& l2,
		int majAxis,
		MT_Scalar& l1Param,
		MT_Scalar& l2Param
	);	

	 static int ComputeClassification(
		const MT_Scalar& distance,
		const MT_Scalar& epsilon
	); 	
};

// The template parameter TGBinder is a model of a Geometry Binder concept
// see CSG_GeometryBinder.h for an example.

template <typename TGBinder> class CSG_Math
{
public :

	 static bool IntersectPolyWithLine2D(
		const MT_Line3& l,
		const TGBinder& p1,
		const MT_Plane3& plane,
		MT_Scalar& a,
		MT_Scalar& b
	);
		
	 static bool InstersectPolyWithLine3D(
		const MT_Line3& l,
		const TGBinder& p1,
		const MT_Plane3& plane,
		MT_Scalar& a
	);

	 static bool PointInPolygonTest3D(
		const TGBinder& p1,
		const MT_Plane3& plane,
		const MT_Point3& origin,
		const MT_Point3& pointOnPlane
	);

	// Return the mid-point of the given polygon index.
	 static MT_Point3 PolygonMidPoint(
		const TGBinder& p1
	);

	 static MT_Line3 PolygonMidPointRay(
		const TGBinder& p1,
		const MT_Plane3& plane
	);

	 static MT_Plane3 ComputePlane(
		const TGBinder &p1
	);

	// Return a bounding box of the given polygon index.
	 static BBox FitBBox(
		const TGBinder& p1
	); 

	// Return which side of the polygon the plane is on.
	// 0 == 0n
	// 1 == in
	// 2 == out
	// 3 == Straddle
	static int WhichSide(
		const TGBinder& p1,
		const MT_Plane3& plane1
	);

};

template <class TGBinderA, class TGBinderB> class CSG_PolygonIntersector
{
public :

	static bool IntersectPolygons (
		const TGBinderA& p1,
		const TGBinderB& p2,
		const MT_Plane3& plane1,
		const MT_Plane3& plane2
	);

};


#include "CSG_Math.inl"

#endif

