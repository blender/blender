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

#include "DT_Polytope.h"

MT_BBox DT_Polytope::bbox() const 
{
	MT_BBox bbox = (*this)[0];
	DT_Index i;
    for (i = 1; i < numVerts(); ++i) 
	{
        bbox = bbox.hull((*this)[i]);
    }
    return bbox;
}

MT_Scalar DT_Polytope::supportH(const MT_Vector3& v) const 
{
    int c = 0;
    MT_Scalar h = (*this)[0].dot(v), d;
	DT_Index i;
    for (i = 1; i < numVerts(); ++i) 
	{
        if ((d = (*this)[i].dot(v)) > h) 
		{ 
			c = i; 
			h = d; 
		}
    }
    return h;
}

MT_Point3 DT_Polytope::support(const MT_Vector3& v) const 
{
    int c = 0;
    MT_Scalar h = (*this)[0].dot(v), d;
	DT_Index i;
    for (i = 1; i < numVerts(); ++i)
	{
        if ((d = (*this)[i].dot(v)) > h)
		{ 
			c = i;
			h = d; 
		}
    }
    return (*this)[c];
}


