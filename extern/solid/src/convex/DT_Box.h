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

#ifndef DT_BOX_H
#define DT_BOX_H

#include "DT_Convex.h"

class DT_Box : public DT_Convex {
public:
    DT_Box(MT_Scalar x, MT_Scalar y, MT_Scalar z) : 
        m_extent(x, y, z) 
	{}

    DT_Box(const MT_Vector3& e) : 
		m_extent(e) 
	{}

    virtual MT_Scalar supportH(const MT_Vector3& v) const;
    virtual MT_Point3 support(const MT_Vector3& v) const;
	virtual bool ray_cast(const MT_Point3& source, const MT_Point3& target,
						  MT_Scalar& param, MT_Vector3& normal) const;
    
    const MT_Vector3& getExtent() const { return m_extent; }
	
protected:
	typedef unsigned int T_Outcode;
	
	T_Outcode outcode(const MT_Point3& p) const
	{
		return (p[0] < -m_extent[0] ? 0x01 : 0x0) |    
			   (p[0] >  m_extent[0] ? 0x02 : 0x0) |
			   (p[1] < -m_extent[1] ? 0x04 : 0x0) |    
			   (p[1] >  m_extent[1] ? 0x08 : 0x0) |
			   (p[2] < -m_extent[2] ? 0x10 : 0x0) |    
			   (p[2] >  m_extent[2] ? 0x20 : 0x0);
	}
    
    MT_Vector3 m_extent;
};

#endif
