/*
 *
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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
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

/** \file boolop/intern/BOP_MathUtils.h
 *  \ingroup boolopintern
 */

 
#ifndef __BOP_MATHUTILS_H__
#define __BOP_MATHUTILS_H__

#include <math.h>
#include <float.h>
#include "MT_Point3.h"
#include "MT_Plane3.h"

/* define this to give better precision comparisons */
#define VAR_EPSILON

#ifndef VAR_EPSILON
const MT_Scalar BOP_EPSILON(1.0e-5);
#else
const MT_Scalar BOP_EPSILON(9.3132257461547852e-10);	/* ~= 2**-30 */
#endif

inline int BOP_sign(MT_Scalar x) {
    return x < 0.0 ? -1 : x > 0.0 ? 1 : 0;
}
inline MT_Scalar BOP_abs(MT_Scalar x) { return fabs(x); }
int BOP_comp(const MT_Scalar A, const MT_Scalar B);
int BOP_comp(const MT_Tuple3& A, const MT_Tuple3& B);
int BOP_comp0(const MT_Scalar A);
inline bool BOP_fuzzyZero(MT_Scalar x) { return BOP_comp0(x) == 0; }
int BOP_exactComp(const MT_Scalar A, const MT_Scalar B);
int BOP_exactComp(const MT_Tuple3& A, const MT_Tuple3& B);
bool BOP_between(const MT_Point3& p1, const MT_Point3& p2, const MT_Point3& p3);
bool BOP_collinear(const MT_Point3& p1, const MT_Point3& p2, const MT_Point3& p3);
bool BOP_convex(const MT_Point3& p1, const MT_Point3& p2, const MT_Point3& p3, 
				const MT_Point3& p4);
int BOP_concave(const MT_Point3& p1, const MT_Point3& p2, const MT_Point3& p3, const MT_Point3& p4);
bool BOP_intersect(const MT_Vector3& vL1, const MT_Point3& pL1, const MT_Vector3& vL2, 
				   const MT_Point3& pL2, MT_Point3& intersection);
bool BOP_getCircleCenter(const MT_Point3& p1, const MT_Point3& p2, const MT_Point3& p3, 
						 const MT_Point3& center);
bool BOP_isInsideCircle(const MT_Point3& p1, const MT_Point3& p2, const MT_Point3& p3, 
						const MT_Point3& p4, const MT_Point3& p5);
bool BOP_isInsideCircle(const MT_Point3& p1, const MT_Point3& p2, const MT_Point3& p3, 
						const MT_Point3& q);
MT_Scalar BOP_orientation(const MT_Plane3& p1, const MT_Plane3& p2);
int BOP_classify(const MT_Point3& p, const MT_Plane3& plane);
MT_Point3 BOP_intersectPlane(const MT_Plane3& plane, const MT_Point3& p1, const MT_Point3& p2);
bool BOP_containsPoint(const MT_Plane3& plane, const MT_Point3& point);
MT_Point3 BOP_4PointIntersect(const MT_Point3& p0, const MT_Point3& p1, const MT_Point3& p2, 
							  const MT_Point3& q);
MT_Scalar BOP_EpsilonDistance(const MT_Point3& p0, const MT_Point3& p1, const MT_Point3& q);

#endif
