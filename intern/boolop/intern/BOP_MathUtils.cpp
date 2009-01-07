/**
 *
 * $Id$
 *
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Marc Freixas, Ken Hughes
 *
 * ***** END GPL LICENSE BLOCK *****
 */
 
#include "BOP_MathUtils.h"
#include <iostream>
using namespace std;

/**
 * Compares two scalars with EPSILON accuracy.
 * @param A scalar
 * @param B scalar
 * @return 1 if A > B, -1 if A < B, 0 otherwise
 */
int BOP_comp(const MT_Scalar A, const MT_Scalar B)
{
#ifndef VAR_EPSILON
	if (A >= B + BOP_EPSILON) return 1;
	else if (B >= A + BOP_EPSILON) return -1;
	else return 0; 
#else
	int expA, expB;
	float mant;
	frexp(A, &expA);	/* get exponents of each number */
	frexp(B, &expB);

	if(expA < expB)		/* find the larger exponent */
		expA = expB;
	mant = frexp((A-B), &expB);	/* get exponent of the difference */
	/* mantissa will only be zero is (A-B) is really zero; otherwise, also
	 * also allow a "reasonably" small exponent or "reasonably large"
	 * difference in exponents to be considers "close to zero" */
	if( mant == 0 || expB < -30 || expA - expB > 31) return 0;
	else if( mant > 0) return 1;
	else return -1;
#endif
}

/**
 * Compares a scalar with EPSILON accuracy.
 * @param A scalar
 * @return 1 if A > 0, -1 if A < 0, 0 otherwise
 */
int BOP_comp0(const MT_Scalar A)
{
	if (A >= BOP_EPSILON) return 1;
	else if (0 >= A + BOP_EPSILON) return -1;
	else return 0; 
}

/**
 * Compares two scalar triplets with EPSILON accuracy.
 * @param A scalar triplet
 * @param B scalar triplet
 * @return 1 if A > B, -1 if A < B, 0 otherwise
 */
int BOP_comp(const MT_Tuple3& A, const MT_Tuple3& B)
{
#ifndef VAR_EPSILON
	if (A.x() >= (B.x() + BOP_EPSILON)) return 1;
	else if (B.x() >= (A.x() + BOP_EPSILON)) return -1;
	else if (A.y() >= (B.y() + BOP_EPSILON)) return 1;
	else if (B.y() >= (A.y() + BOP_EPSILON)) return -1;
	else if (A.z() >= (B.z() + BOP_EPSILON)) return 1;
	else if (B.z() >= (A.z() + BOP_EPSILON)) return -1;
	else return 0;
#else
	int result = BOP_comp(A.x(), B.x());
	if (result != 0) return result;
	result = BOP_comp(A.y(), B.y());
	if (result != 0) return result;
	return BOP_comp(A.z(), B.z());
#endif
}

/**
 * Compares two scalars strictly.
 * @param A scalar
 * @param B scalar
 * @return 1 if A > B, -1 if A < B, 0 otherwise
 */
int BOP_exactComp(const MT_Scalar A, const MT_Scalar B)
{
	if (A > B) return 1;
	else if (B > A) return -1;
	else return 0; 
}
/**
 * Compares two scalar strictly.
 * @param A scalar triplet
 * @param B scalar triplet
 * @return 1 if A > B, -1 if A < B, 0 otherwise
 */
int BOP_exactComp(const MT_Tuple3& A, const MT_Tuple3& B)
{
	if (A.x() > B.x()) return 1;
	else if (B.x() > A.x()) return -1;
	else if (A.y() > B.y()) return 1;
	else if (B.y() > A.y()) return -1;
	else if (A.z() > B.z()) return 1;
	else if (B.z() > A.z()) return -1;
	else return 0;
}

/**
 * Returns if p1 is between p2 and p3 and lay on the same line (are collinears).
 * @param p1 point
 * @param p2 point
 * @param p3 point
 * @return true if p1 is between p2 and p3 and lay on the same line, false otherwise
 */
bool BOP_between(const MT_Point3& p1, const MT_Point3& p2, const MT_Point3& p3)
{
	MT_Scalar distance = p2.distance(p3);
	return (p1.distance(p2) < distance && p1.distance(p3) < distance) && BOP_collinear(p1,p2,p3);
}

/**
 * Returns if three points lay on the same line (are collinears).
 * @param p1 point
 * @param p2 point
 * @param p3 point
 * @return true if the three points lay on the same line, false otherwise
 */
bool BOP_collinear(const MT_Point3& p1, const MT_Point3& p2, const MT_Point3& p3)
{
	if( BOP_comp(p1,p2) == 0 || BOP_comp(p2,p3) == 0 ) return true;

	MT_Vector3 v1 = p2 - p1;
	MT_Vector3 v2 = p3 - p2;

	/* normalize vectors before taking their cross product, so its length 
     * has some actual meaning */
	// if(MT_fuzzyZero(v1.length()) || MT_fuzzyZero(v2.length())) return true;
	v1.normalize();	
	v2.normalize();

	MT_Vector3 w = v1.cross(v2);
	
	return (BOP_fuzzyZero(w.x()) && BOP_fuzzyZero(w.y()) && BOP_fuzzyZero(w.z()));
}

/**
 * Returns if a quad (coplanar) is convex.
 * @return true if the quad is convex, false otherwise
 */
bool BOP_convex(const MT_Point3& p1, const MT_Point3& p2, const MT_Point3& p3, const MT_Point3& p4)
{
	MT_Vector3 v1 = p3 - p1;
	MT_Vector3 v2 = p4 - p2;
	MT_Vector3 quadPlane = v1.cross(v2);
	// plane1 is the perpendicular plane that contains the quad diagonal (p2,p4)
	MT_Plane3 plane1(quadPlane.cross(v2),p2);
	// if p1 and p3 are classified in the same region, the quad is not convex 
	if (BOP_classify(p1,plane1) == BOP_classify(p3,plane1)) return false;
	else {
		// Test the other quad diagonal (p1,p3) and perpendicular plane
		MT_Plane3 plane2(quadPlane.cross(v1),p1);
		// if p2 and p4 are classified in the same region, the quad is not convex
		return (BOP_classify(p2,plane2) != BOP_classify(p4,plane2));
	}
}

/**
 * Returns if a quad (coplanar) is concave and where is the split edge.
 * @return 0 if is convex, 1 if is concave and split edge is p1-p3 and -1 if is
 * cancave and split edge is p2-p4.
 */
int BOP_concave(const MT_Point3& p1, const MT_Point3& p2, const MT_Point3& p3, const MT_Point3& p4)
{
	MT_Vector3 v1 = p3 - p1;
	MT_Vector3 v2 = p4 - p2;
	MT_Vector3 quadPlane = v1.cross(v2);
	// plane1 is the perpendicular plane that contains the quad diagonal (p2,p4)
	MT_Plane3 plane1(quadPlane.cross(v2),p2);
	// if p1 and p3 are classified in the same region, the quad is not convex 
	if (BOP_classify(p1,plane1) == BOP_classify(p3,plane1)) return 1;
	else {
		// Test the other quad diagonal (p1,p3) and perpendicular plane
		MT_Plane3 plane2(quadPlane.cross(v1),p1);
		// if p2 and p4 are classified in the same region, the quad is not convex
		if (BOP_classify(p2,plane2) == BOP_classify(p4,plane2)) return -1;
		else return 0;
	}
}

/**
 * Computes the intersection between two lines (on the same plane).
 * @param vL1 first line vector
 * @param pL1 first line point
 * @param vL2 second line vector
 * @param pL2 second line point
 * @param intersection intersection point (if exists)
 * @return false if lines are parallels, true otherwise
 */
bool BOP_intersect(const MT_Vector3& vL1, const MT_Point3& pL1, const MT_Vector3& vL2, 
				   const MT_Point3& pL2, MT_Point3 &intersection)
{
	// NOTE: 
    // If the lines aren't on the same plane, the intersection point will not be valid. 
	// So be careful !!

	MT_Scalar t = -1;
	MT_Scalar den = (vL1.y()*vL2.x() - vL1.x() * vL2.y());
	
	if (!BOP_fuzzyZero(den)) {
		t =  (pL2.y()*vL1.x() - vL1.y()*pL2.x() + pL1.x()*vL1.y() - pL1.y()*vL1.x()) / den ;
	}
	else {
		den = (vL1.y()*vL2.z() - vL1.z() * vL2.y());
		if (!BOP_fuzzyZero(den)) {
			t =  (pL2.y()*vL1.z() - vL1.y()*pL2.z() + pL1.z()*vL1.y() - pL1.y()*vL1.z()) / den ;
		}
		else {
			den = (vL1.x()*vL2.z() - vL1.z() * vL2.x());
			if (!BOP_fuzzyZero(den)) {
				t =  (pL2.x()*vL1.z() - vL1.x()*pL2.z() + pL1.z()*vL1.x() - pL1.x()*vL1.z()) / den ;
			}
			else {
				return false;
			}
		}
	}
	
	intersection.setValue(vL2.x()*t + pL2.x(), vL2.y()*t + pL2.y(), vL2.z()*t + pL2.z());
	return true;
}

/**
 * Returns the center of the circle defined by three points.
 * @param p1 point
 * @param p2 point
 * @param p3 point
 * @param center circle center
 * @return false if points are collinears, true otherwise
 */
bool BOP_getCircleCenter(const MT_Point3& p1, const MT_Point3& p2, const MT_Point3& p3, 
						 MT_Point3& center)
{
	// Compute quad plane
	MT_Vector3 p1p2 = p2-p1;
	MT_Vector3 p1p3 = p3-p1;
	MT_Plane3 plane1(p1,p2,p3);
	MT_Vector3 plane = plane1.Normal();
	
	// Compute first line vector, perpendicular to plane vector and edge (p1,p2)
	MT_Vector3 vL1 = p1p2.cross(plane);
	if( MT_fuzzyZero(vL1.length() ) )
			return false;
	vL1.normalize();
	
	// Compute first line point, middle point of edge (p1,p2)
	MT_Point3 pL1 = p1.lerp(p2, 0.5);

	// Compute second line vector, perpendicular to plane vector and edge (p1,p3)
	MT_Vector3 vL2 = p1p3.cross(plane);
	if( MT_fuzzyZero(vL2.length() ) )
			return false;
	vL2.normalize();
	
	// Compute second line point, middle point of edge (p1,p3)
	MT_Point3 pL2 = p1.lerp(p3, 0.5);

	// Compute intersection (the lines lay on the same plane, so the intersection exists
    // only if they are not parallel!!)
	return BOP_intersect(vL1,pL1,vL2,pL2,center);
}

/**
 * Returns if points q is inside the circle defined by p1, p2 and p3.
 * @param p1 point
 * @param p2 point
 * @param p3 point
 * @param q point
 * @return true if p4 or p5 are inside the circle, false otherwise. If 
 * the circle does not exist (p1, p2 and p3 are collinears) returns true
 */
bool BOP_isInsideCircle(const MT_Point3& p1, const MT_Point3& p2, const MT_Point3& p3, 
						const MT_Point3& q)
{
	MT_Point3 center;

	// Compute circle center
	bool ok = BOP_getCircleCenter(p1,p2,p3,center);
	
	if (!ok) return true; // p1,p2 and p3 are collinears

	// Check if q is inside the circle
	MT_Scalar r = p1.distance(center);
	MT_Scalar d = q.distance(center);    
	return (BOP_comp(d,r) <= 0);
}

/**
 * Returns if points p4 or p5 is inside the circle defined by p1, p2 and p3.
 * @param p1 point
 * @param p2 point
 * @param p3 point
 * @param p4 point
 * @param p5 point
 * @return true if p4 or p5 is inside the circle, false otherwise. If 
 * the circle does not exist (p1, p2 and p3 are collinears) returns true
 */
bool BOP_isInsideCircle(const MT_Point3& p1, const MT_Point3& p2, const MT_Point3& p3, 
						const MT_Point3& p4, const MT_Point3& p5)
{
	MT_Point3 center;
	bool ok = BOP_getCircleCenter(p1,p2,p3,center);

	if (!ok) return true; // Collinear points!

	// Check if p4 or p5 is inside the circle
	MT_Scalar r = p1.distance(center);
	MT_Scalar d1 = p4.distance(center);
	MT_Scalar d2 = p5.distance(center);
	return (BOP_comp(d1,r) <= 0 || BOP_comp(d2,r) <= 0);
}

/**
 * Returns if two planes share the same orientation.
 * @return >0 if planes share the same orientation
 */
MT_Scalar BOP_orientation(const MT_Plane3& p1, const MT_Plane3& p2)
{
	// Dot product between plane normals
	return (p1.x()*p2.x() + p1.y()*p2.y() + p1.z()*p2.z());
}

/**
 * Classifies a point according to the specified plane with EPSILON accuracy.
 * @param p point
 * @param plane plane
 * @return >0 if the point is above (OUT), 
 *         =0 if the point is on (ON), 
 *         <0 if the point is below (IN)
 */
int BOP_classify(const MT_Point3& p, const MT_Plane3& plane)
{
	// Compare plane - point distance with zero
	return BOP_comp0(plane.signedDistance(p));
}

/**
 * Intersects a plane with the line that contains the specified points.
 * @param plane split plane
 * @param p1 first line point
 * @param p2 second line point
 * @return intersection between plane and line that contains p1 and p2
 */
MT_Point3 BOP_intersectPlane(const MT_Plane3& plane, const MT_Point3& p1, const MT_Point3& p2)
{
	// Compute intersection between plane and line ...
    //
	//       L: (p2-p1)lambda + p1
	//
	// supposes resolve equation ...
	//
	//       coefA*((p2.x - p1.y)*lambda + p1.x) + ... + coefD = 0
	
    MT_Point3 intersection = MT_Point3(0,0,0); //never ever return anything undefined! 
    MT_Scalar den = plane.x()*(p2.x()-p1.x()) + 
					plane.y()*(p2.y()-p1.y()) + 
					plane.z()*(p2.z()-p1.z());
	if (den != 0) {
		MT_Scalar lambda = (-plane.x()*p1.x()-plane.y()*p1.y()-plane.z()*p1.z()-plane.w()) / den;
		intersection.setValue(p1.x() + (p2.x()-p1.x())*lambda, 
						  p1.y() + (p2.y()-p1.y())*lambda, 
						  p1.z() + (p2.z()-p1.z())*lambda);
		return intersection;
	}
	return intersection;
}

/**
 * Returns if a plane contains a point with EPSILON accuracy.
 * @param plane plane
 * @param point point
 * @return true if the point is on the plane, false otherwise
 */
bool BOP_containsPoint(const MT_Plane3& plane, const MT_Point3& point)
{
	return BOP_fuzzyZero(plane.signedDistance(point));
}

/**
 * Pre: p0, p1 and p2 is a triangle and q is an interior point.
 * @param p0 point
 * @param p1 point
 * @param p2 point
 * @param q point
 * @return intersection point I
 *                v 
 *  (p0)-----(I)----->(p1)
 *    \       ^        /
 *     \      |w      /
 *      \     |      /
 *       \   (q)    /
 *        \   |    /
 *         \  |   /
 *          \ |  /
 *           (p2)
 *
 * v = P1-P2
 * w = P3-Q
 * r0(t) = v*t+P1
 * r1(t) = w*t+P3
 * I = r0^r1
 */
MT_Point3 BOP_4PointIntersect(const MT_Point3& p0, const MT_Point3& p1, const MT_Point3& p2, 
							  const MT_Point3& q)
{
	MT_Vector3 v(p0.x()-p1.x(), p0.y()-p1.y(), p0.z()-p1.z());
	MT_Vector3 w(p2.x()-q.x(), p2.y()-q.y(), p2.z()-q.z());
	MT_Point3 I;
	
	BOP_intersect(v,p0,w,p2,I);
	return I;
}

/**
 * Pre: p0, p1 and q are collinears.
 * @param p0 point
 * @param p1 point
 * @param q point
 * @return 0 if q == p0, 1 if q == p1, or a value between 0 and 1 otherwise
 *
 * (p0)-----(q)------------(p1)
 *   |<-d1-->|               |
 *   |<---------d0---------->|
 * 
 */
MT_Scalar BOP_EpsilonDistance(const MT_Point3& p0, const MT_Point3& p1, const MT_Point3& q)
{
	MT_Scalar d0 = p0.distance(p1);
	MT_Scalar d1 = p0.distance(q);
	MT_Scalar d;
	
	if (BOP_fuzzyZero(d0)) d = 1.0;
	else if (BOP_fuzzyZero(d1)) d = 0.0;
	else d = d1 / d0;
	return d;
}
