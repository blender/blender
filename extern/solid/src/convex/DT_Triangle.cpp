/*
 * SOLID - Software Library for Interference Detection
 * 
 * Copyright (C) 2001-2003  Dtecta.  All rights reserved.
 *
 * This library may be distributed under the terms of the Q Public License
 * (QPL) as defined by Trolltech AS of Norway and appearing in the file
 * LICENSE.QPL included in the packaging of this file.
 *
 * This library may be distributed and/or modified under the terms of the
 * GNU General Public License (GPL) version 2 as published by the Free Software
 * Foundation and appearing in the file LICENSE.GPL included in the
 * packaging of this file.
 *
 * This library is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
 * WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 * Commercial use or any other use of this library not covered by either 
 * the QPL or the GPL requires an additional license from Dtecta. 
 * Please contact info@dtecta.com for enquiries about the terms of commercial
 * use of this library.
 */

//#define BACKFACE_CULLING

#include "DT_Triangle.h"

MT_BBox DT_Triangle::bbox() const 
{
	return MT_BBox((*this)[0]).hull((*this)[1]).hull((*this)[2]);
}

MT_Scalar DT_Triangle::supportH(const MT_Vector3& v) const
{
    return GEN_max(GEN_max(v.dot((*this)[0]), v.dot((*this)[1])), v.dot((*this)[2]));
}

MT_Point3 DT_Triangle::support(const MT_Vector3& v) const
{
    MT_Vector3 dots(v.dot((*this)[0]), v.dot((*this)[1]), v.dot((*this)[2]));

	return (*this)[dots.maxAxis()];
}

bool DT_Triangle::ray_cast(const MT_Point3& source, const MT_Point3& target, 
						   MT_Scalar& param, MT_Vector3& normal) const 
{
	MT_Vector3 d1 = (*this)[1] - (*this)[0];
	MT_Vector3 d2 = (*this)[2] - (*this)[0];
	MT_Vector3 n = d1.cross(d2);
	MT_Vector3 r = target - source;
	MT_Scalar delta = -r.dot(n);

   MT_Scalar rounding_error = GEN_max(GEN_max(MT_abs(n[0]), MT_abs(n[1])), MT_abs(n[2])) * MT_EPSILON; 

#ifdef BACKFACE_CULLING	
   if (delta > rounding_error)
#else
	if (MT_abs(delta) > rounding_error)
#endif      
		// The ray is not parallel to the triangle's plane. 
		// (Coplanar rays are ignored.)
	{
		MT_Vector3 b = source - (*this)[0];
		MT_Scalar lambda = b.dot(n) / delta;

		if (MT_Scalar(0.0) <= lambda && lambda <= param)
			// The ray intersects the triangle's plane.
		{
			MT_Vector3 u = b.cross(r);
			MT_Scalar mu1 = d2.dot(u) / delta;

			if (MT_Scalar(0.0) <= mu1 && mu1 <= MT_Scalar(1.0)) 
			{
				MT_Scalar mu2 = -d1.dot(u) / delta;

				if (MT_Scalar(0.0) <= mu2 && mu1 + mu2 <= MT_Scalar(1.0)) 
					// The ray intersects the triangle.
				{
					param = lambda;
					// Return a normal that points at the source.
#ifdef BACKFACE_CULLING
               normal = n;
#else
					normal = delta > MT_Scalar(0.0) ? n : -n;
#endif
					return true;
				}
			}
		}
	}

	return false;
}


