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

#ifndef BP_ENDPOINT_H
#define BP_ENDPOINT_H

#include "SOLID_types.h"

class BP_Proxy;

class BP_Link {
public:
	BP_Link() {}
	explicit BP_Link(BP_Proxy *proxy) :
		m_proxy(proxy)
	{}

	DT_Index  m_index;
	DT_Count  m_count;
	BP_Proxy *m_proxy;
};

typedef unsigned int Uint32;

class BP_Endpoint {
public:
    enum { 
		MINIMUM = 0x00000000, 
		MAXIMUM = 0x80000000,	
		TYPEBIT = 0x00000001
	};

    BP_Endpoint() {}
    BP_Endpoint(DT_Scalar pos, Uint32 type, BP_Link *link) 
	  :	m_link(link)
	{
		setPos(pos, type);
	}
 
	DT_Scalar getPos()   const { return m_pos; }
	DT_Index&   getIndex() const { return m_link->m_index; }
	DT_Count&   getCount() const { return m_link->m_count; }
	BP_Proxy *getProxy() const { return m_link->m_proxy; }
	
	DT_Index   getEndIndex()   const { return (m_link + 1)->m_index; }
	
	Uint32 getType() const 
	{ 
		return (m_bits & TYPEBIT) ? (~m_bits & MAXIMUM) : (m_bits & MAXIMUM);
	}

	void setPos(DT_Scalar pos, Uint32 type) 
	{
		m_pos = pos; 
		if ((m_bits & MAXIMUM) == type) 
		{
			m_bits &= ~TYPEBIT;
		}
		else 
		{
			m_bits |= TYPEBIT;
		}
	}

private:
	union {
		DT_Scalar    m_pos;
		Uint32       m_bits;
	};
	BP_Link        *m_link;
};

inline bool operator<(const BP_Endpoint& a, const BP_Endpoint& b) 
{
    return a.getPos() < b.getPos(); 
}

#endif










