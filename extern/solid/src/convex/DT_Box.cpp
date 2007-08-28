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

#include "DT_Box.h"

MT_Scalar DT_Box::supportH(const MT_Vector3& v) const 
{
    return v.absolute().dot(m_extent);
}

MT_Point3 DT_Box::support(const MT_Vector3& v) const 
{
    return MT_Point3(v[0] < MT_Scalar(0.0) ? -m_extent[0] : m_extent[0],
                     v[1] < MT_Scalar(0.0) ? -m_extent[1] : m_extent[1],
                     v[2] < MT_Scalar(0.0) ? -m_extent[2] : m_extent[2]); 
    
}


bool DT_Box::ray_cast(const MT_Point3& source, const MT_Point3& target,
					  MT_Scalar& param, MT_Vector3& normal) const 
{
	T_Outcode source_bits = outcode(source);
	T_Outcode target_bits = outcode(target);

	if ((source_bits & target_bits) == 0x0)
		// None of the side planes separate the ray from the box.
	{
		MT_Scalar lambda_enter = MT_Scalar(0.0);
		MT_Scalar lambda_exit  = param;
		MT_Vector3 r = target - source;
		T_Outcode normal_bit = 0x0; // Indicates the axis that is returned as normal.
		T_Outcode bit = 0x01;
		int i;
		for (i = 0; i != 3; ++i)
		{
			if (source_bits & bit)
				// Point of intersection is entering
			{
				MT_Scalar lambda = (-source[i] - m_extent[i]) / r[i];
				if (lambda_enter < lambda)
				{
					lambda_enter = lambda;
					normal_bit = bit;
				}
			}
			else if (target_bits & bit) 
				// Point of intersection is exiting
			{
				MT_Scalar lambda = (-source[i] - m_extent[i]) / r[i];
				GEN_set_min(lambda_exit, lambda);
			}
			bit <<=1;
			if (source_bits & bit)
				// Point of intersection is entering
			{
				MT_Scalar lambda =  (-source[i] + m_extent[i]) / r[i];
				if (lambda_enter < lambda)
				{
					lambda_enter = lambda;
					normal_bit = bit;
				}
			}
			else if (target_bits & bit) 
				// Point of intersection is exiting
			{
				MT_Scalar lambda =  (-source[i] + m_extent[i]) / r[i];
				GEN_set_min(lambda_exit, lambda);
			}
			bit <<=1;
		}
		if (lambda_enter <= lambda_exit)
			// The ray intersects the box
		{
			param = lambda_enter;
			normal.setValue(normal_bit == 0x01 ? -MT_Scalar(1.0) : 
							normal_bit == 0x02 ?  MT_Scalar(1.0) : 
							MT_Scalar(0.0),
							normal_bit == 0x04 ? -MT_Scalar(1.0) : 
							normal_bit == 0x08 ?  MT_Scalar(1.0) : 
							MT_Scalar(0.0),
							normal_bit == 0x10 ? -MT_Scalar(1.0) : 
							normal_bit == 0x20 ?  MT_Scalar(1.0) : 
							MT_Scalar(0.0));
			return true;
		}
	}

	return false;
}


