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

#include "DT_Cone.h"

MT_Point3 DT_Cone::support(const MT_Vector3& v) const 
{
    MT_Scalar v_len = v.length();

    if (v[1] > v_len * sinAngle)
	{
		return MT_Point3(MT_Scalar(0.0), halfHeight, MT_Scalar(0.0));
	}
    else
	{
        MT_Scalar s = MT_sqrt(v[0] * v[0] + v[2] * v[2]);
        if (s != MT_Scalar(0.0))
		{
            MT_Scalar d = bottomRadius / s;  
            return MT_Point3(v[0] * d, -halfHeight, v[2] * d);
        }
        else
		{
			return MT_Point3(bottomRadius, -halfHeight, MT_Scalar(0.0));
		}
    }
}

