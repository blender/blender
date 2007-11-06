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

#ifndef DT_VERTEXBASE_H
#define DT_VERTEXBASE_H

#include "MT_Point3.h"

#include <vector>

class DT_Complex;

typedef std::vector<DT_Complex *>DT_ComplexList;

class DT_VertexBase {
public:
    explicit DT_VertexBase(const void *base = 0, DT_Size stride = 0, bool owner = false) : 
        m_base((char *)base),
		m_stride(stride ? stride : 3 * sizeof(DT_Scalar)),
		m_owner(owner)
	{}
	
	~DT_VertexBase()
	{
		if (m_owner)
		{
			delete [] m_base;
		}
	}
    
    MT_Point3 operator[](DT_Index i) const 
	{ 
        return MT_Point3(reinterpret_cast<DT_Scalar *>(m_base + i * m_stride));
    }
    
    void setPointer(const void *base, bool owner = false)
	{
		m_base = (char *)base; 
		m_owner = owner;
	} 
	
    const void *getPointer() const { return m_base; }	
	bool        isOwner() const { return m_owner; }
    
	void addComplex(DT_Complex *complex) const { m_complexList.push_back(complex); }
	void removeComplex(DT_Complex *complex) const
	{
		DT_ComplexList::iterator it = std::find(m_complexList.begin(), m_complexList.end(), complex); 
		if (it != m_complexList.end())
		{
			m_complexList.erase(it);
		}
	}
	
	const DT_ComplexList& getComplexList() const { return m_complexList; }
	
private:    
    char                  *m_base;
    DT_Size                m_stride;
	bool                   m_owner;
	mutable DT_ComplexList m_complexList;
};

#endif
