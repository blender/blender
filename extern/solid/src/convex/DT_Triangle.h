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

#ifndef DT_TRIANGLE_H
#define DT_TRIANGLE_H

#include "SOLID_types.h"

#include "DT_Convex.h"
#include "DT_IndexArray.h"
#include "DT_VertexBase.h"

class DT_Triangle : public DT_Convex {
public:
    DT_Triangle(const DT_VertexBase *base, DT_Index i0, DT_Index i1, DT_Index i2) : 
        m_base(base)
	{
		m_index[0] = i0;
		m_index[1] = i1;
		m_index[2] = i2;
	}

    DT_Triangle(const DT_VertexBase *base, const DT_Index *index) : 
        m_base(base)
	{
		m_index[0] = index[0];
		m_index[1] = index[1];
		m_index[2] = index[2];
	}

	virtual MT_BBox bbox() const;
    virtual MT_Scalar supportH(const MT_Vector3& v) const;
    virtual MT_Point3 support(const MT_Vector3& v) const;
	virtual bool ray_cast(const MT_Point3& source, const MT_Point3& target, MT_Scalar& lambda, MT_Vector3& normal) const;

    MT_Point3 operator[](int i) const { return (*m_base)[m_index[i]]; }

private:
    const DT_VertexBase *m_base;
    DT_Index             m_index[3];
};

#endif
