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

#include "DT_Sphere.h"
#include "GEN_MinMax.h"

MT_Scalar DT_Sphere::supportH(const MT_Vector3& v) const 
{
	return m_radius * v.length();
}

MT_Point3 DT_Sphere::support(const MT_Vector3& v) const 
{
   MT_Scalar s = v.length();
	
	if (s > MT_Scalar(0.0))
	{
		s = m_radius / s;
		return MT_Point3(v[0] * s, v[1] * s, v[2] * s);
	}
	else
	{
		return MT_Point3(m_radius, MT_Scalar(0.0), MT_Scalar(0.0));
	}
}

bool DT_Sphere::ray_cast(const MT_Point3& source, const MT_Point3& target,
						 MT_Scalar& param, MT_Vector3& normal) const 
{
	MT_Vector3 r = target - source;
	MT_Scalar  delta = -source.dot(r);  
	MT_Scalar  r_length2 = r.length2();
	MT_Scalar  sigma = delta * delta - r_length2 * (source.length2() - m_radius * m_radius);

	if (sigma >= MT_Scalar(0.0))
		// The line trough source and target intersects the sphere.
	{
		MT_Scalar sqrt_sigma = MT_sqrt(sigma);
		// We need only the sign of lambda2, so the division by the positive 
		// r_length2 can be left out.
		MT_Scalar lambda2 = (delta + sqrt_sigma) /* / r_length2 */ ;
		if (lambda2 >= MT_Scalar(0.0))
			// The ray points at the sphere
		{
			MT_Scalar lambda1 = (delta - sqrt_sigma) / r_length2;
			if (lambda1 <= param)
				// The ray hits the sphere, since 
				// [lambda1, lambda2] overlaps [0, param]. 
			{
				if (lambda1 > MT_Scalar(0.0))
				{
					param = lambda1;
					normal = (source + r * lambda1) / m_radius;
					// NB: division by m_radius to normalize the normal.
				}
				else
				{
					param = MT_Scalar(0.0);
					normal.setValue(MT_Scalar(0.0), MT_Scalar(0.0), MT_Scalar(0.0));
				}
						
				return true;
			}
		}
	}

	return false;
}


