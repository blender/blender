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
#include "CSG_Math.h"
#include "MT_minmax.h"
#include "MT_Point2.h"
#include "MT_Vector2.h"
#include "MT_Plane3.h"

#include <vector>

using namespace std;

inline bool CSG_Geometry::Intersect(
	const MT_Plane3& p1,
	const MT_Plane3& p2,
	MT_Line3& output
) {
	MT_Matrix3x3 mat;
	mat[0] = p1.Normal();
	mat[1] = p2.Normal();
	mat[2] = mat[0].cross(mat[1]);		
	
	if (mat[2].fuzzyZero()) 
	{
		return false;
	}

	MT_Vector3 aPoint(-p1.Scalar(),-p2.Scalar(),0);

	output = MT_Line3(MT_Point3(0,0,0) + mat.inverse()*aPoint ,mat[2]);

	assert(MT_fuzzyZero(p1.signedDistance(output.Origin())));
	assert(MT_fuzzyZero(p2.signedDistance(output.Origin())));


	return true;
}

inline bool CSG_Geometry::Intersect2DNoBoundsCheck(
	const MT_Line3& l1,
	const MT_Line3& l2,
	int majAxis,
	MT_Scalar& l1Param,
	MT_Scalar& l2Param
){
	int ind1 = cofacTable[majAxis][0];
	int ind2 = cofacTable[majAxis][1];

	MT_Scalar Zx = l2.Origin()[ind1] - l1.Origin()[ind1];
	MT_Scalar Zy = l2.Origin()[ind2] - l1.Origin()[ind2];
 
	MT_Scalar det =  l1.Direction()[ind1]*l2.Direction()[ind2] - 
					 l2.Direction()[ind1]*l1.Direction()[ind2];

	if (MT_fuzzyZero(det)) 
	{
		return false;
	}

	l1Param =   (l2.Direction()[ind2]*Zx - l2.Direction()[ind1]*Zy)/det;
	l2Param = -(-l1.Direction()[ind2]*Zx + l1.Direction()[ind1]*Zy)/det;
	
	return true;
}
	
	
inline bool CSG_Geometry::Intersect2DBoundsCheck(
	const MT_Line3& l1,
	const MT_Line3& l2,
	int majAxis,
	MT_Scalar& l1Param,
	MT_Scalar& l2Param
) {

	bool isect = Intersect2DNoBoundsCheck(l1,l2,majAxis,l1Param,l2Param);
	if (!isect) return false;

	return l1.IsParameterOnLine(l1Param) && l2.IsParameterOnLine(l2Param);	
}


//IMplementation of CSG_Math
////////////////////////////

template<typename TGBinder>
inline bool CSG_Math<TGBinder>::IntersectPolyWithLine2D(
	const MT_Line3& l,
	const TGBinder& p1,
	const MT_Plane3& plane,
	MT_Scalar& a,
	MT_Scalar& b
){
	int majAxis = plane.Normal().closestAxis();
	int lastInd = p1.Size()-1;

	b = (-MT_INFINITY);
	a = (MT_INFINITY);

	MT_Scalar isectParam(0);
	MT_Scalar isectParam2(0);

	int i;
	int j = lastInd;
	int isectsFound(0);
	for (i=0;i<=lastInd; j=i,i++ )
	{
		MT_Line3 testLine(p1[j],p1[i]);
		if ( CSG_Geometry::Intersect2DBoundsCheck(l,testLine,majAxis,isectParam,isectParam2))
		{
			++isectsFound;
			b = MT_max(isectParam,b);
			a = MT_min(isectParam,a);
		}
	}

	return (isectsFound > 0);
}
	
template<typename TGBinder>
inline bool CSG_Math<TGBinder>::InstersectPolyWithLine3D(
	const MT_Line3& l,
	const TGBinder& p1,
	const MT_Plane3& plane,
	MT_Scalar& a
){
	// First compute intersection parameter t
	MT_Scalar determinant = l.Direction().dot(plane.Normal());

	// they are coplanar but we're not interested in that right?
	if (MT_fuzzyZero(determinant)) return false;

	a = -plane.Scalar() - plane.Normal().dot(l.Origin());
	a /= determinant;

	// intersection point is behind the ray.
	if (a <= 0 ) return false;


	// check if line is bounded and if t lies in bounds.
	if (!l.IsParameterOnLine(a)) return false;

	// calculate the point on the plane 
	MT_Point3 pointOnPlane = l.Origin() + l.Direction()*a;

	assert(MT_fuzzyZero(plane.signedDistance(pointOnPlane)));

	// make sure the intersection point is within the polygon
	return PointInPolygonTest3D(p1,plane,l.Origin(),pointOnPlane);
}



template<typename TGBinder>
inline bool CSG_Math<TGBinder>::PointInPolygonTest3D(
	const TGBinder& p1,
	const MT_Plane3& plane,
	const MT_Point3& origin,
	const MT_Point3& pointOnPlane
) {
	// Form planes with the edges of the polygon and the origin.
	// make sure that origin is inside all these planes.

	// ONe small detail we have to first workout which side the origin
	// is wrt the plane of the polygon.

	bool discardSign = plane.signedDistance(origin) < 0 ? true : false;
	
	const int polySize = p1.Size();

	MT_Point3 lastPoint = p1[polySize-1];

	int i;
	for (i=0;i<polySize; ++i)
	{
		const MT_Point3& aPoint = p1[i];

		MT_Plane3 testPlane(origin,lastPoint,aPoint);
		if ((testPlane.signedDistance(pointOnPlane) <= 0) == discardSign) 
		{
			return false;
		}
		lastPoint = aPoint;
	}
	return true;
}		
		
// return 0 = on
// return 1 = in
// return 2 = out

inline int CSG_Geometry::ComputeClassification(
	const MT_Scalar& distance,
	const MT_Scalar& epsilon
) {
	if (MT_abs(distance) < epsilon)	
	{
		return 0;
	} else {
		return distance < 0 ? 1 : 2;
	}
}
	
// Return the mid-point of the given polygon index.
template <typename TGBinder>
inline MT_Point3 CSG_Math<TGBinder>::PolygonMidPoint(
	const TGBinder& p1
){

	MT_Point3 midPoint(0,0,0);

	int i;
	for (i=0; i < p1.Size(); i++)
	{
		midPoint += p1[i];
	}			 			

	return MT_Point3(midPoint[0]/i,midPoint[1]/i,midPoint[2]/i);
}

template <typename TGBinder>
int CSG_Math<TGBinder>::WhichSide(
	const TGBinder& p1,
	const MT_Plane3& plane1
){

	int output = 0;
	int i;
	for (i=0; i<p1.Size(); i++)
	{
		MT_Scalar signedDistance = plane1.signedDistance(p1[i]);
		if (!MT_fuzzyZero(signedDistance))
		{
			signedDistance < 0 ? (output |= 1) : (output |=2);
		}
	}
	return output;
	
}


template <typename TGBinder>
inline MT_Line3 CSG_Math<TGBinder>::PolygonMidPointRay(
	const TGBinder& p1,
	const MT_Plane3& plane
){
	return MT_Line3(PolygonMidPoint(p1),plane.Normal(),true,false);
}

template <typename TGBinder>
inline MT_Plane3 CSG_Math<TGBinder>::ComputePlane(
	const TGBinder &poly
){
	assert(poly.Size() >= 3);		
	MT_Point3 plast(poly[poly.Size()-1]);	
	MT_Point3 pivot;
	MT_Vector3 edge;
	int j;
	for (j=0; j < poly.Size(); j++) 
	{
		pivot = poly[j];
		edge =  pivot - plast;
		if (!edge.fuzzyZero()) break;
	}
	
	for (; j < poly.Size(); j++) 
	{		
		MT_Vector3 v2 = poly[j] - pivot;	
		MT_Vector3 v3 = edge.cross(v2);
		if (!v3.fuzzyZero())
		{
			return MT_Plane3(v3,pivot);
		}		
	}

	return MT_Plane3();

}

template <typename TGBinder>
inline BBox CSG_Math<TGBinder>::FitBBox(
	const TGBinder& p1
) {
	BBox bbox;
	bbox.SetEmpty();
	
	for (int i = 0; i < p1.Size(); ++i) {
		bbox.Include(p1[i]);
	}
	return bbox;
}

// we now have enough machinary to intersect 3d polygons
// Compute line of intersect
// Intersect line with each edge in PolygonB record min and max intersection parameters
// Do same for PolygonB. If range of intersections overlap then polys overlap.

// Does not yet deal with 2d case.

template<typename TGBinderA, typename TGBinderB>
inline bool CSG_PolygonIntersector<TGBinderA,TGBinderB>::IntersectPolygons (
	const TGBinderA& p1,
	const TGBinderB& p2,
	const MT_Plane3& plane1,
	const MT_Plane3& plane2
){
	MT_Line3 intersectLine;
	if (!CSG_Geometry::Intersect(plane1,plane2,intersectLine))
	{
		// parrallel planes
		return false;
	}
	// check intersection of polygons with intersectLine
	MT_Scalar p1A,p1B;
	MT_Scalar p2A,p2B;
	if ( 
		!CSG_Math<TGBinderA>::IntersectPolyWithLine2D(intersectLine,p1,plane1,p1A,p1B) ||
		!CSG_Math<TGBinderB>::IntersectPolyWithLine2D(intersectLine,p2,plane2,p2A,p2B)
	) {
		return false;
	}

	// see if intersections overlap.

	MT_Scalar maxOMin = MT_max(p1A,p2A);
	MT_Scalar minOMax = MT_min(p1B,p2B);

	return (maxOMin <= minOMax);
}
















