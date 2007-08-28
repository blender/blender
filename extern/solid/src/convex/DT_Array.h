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

#ifndef DT_ARRAY_H
#define DT_ARRAY_H

#if defined (__sgi)
#include <assert.h>
#else
#include <cassert>
#endif

template <typename Data, typename Size = size_t>
class DT_Array {
public:
	DT_Array() 
      :	m_count(0), 
		m_data(0) 
	{}

	explicit DT_Array(Size count)
	  :	m_count(count),
		m_data(new Data[count]) 
	{
		assert(m_data);
	}
	
	DT_Array(Size count, const Data *data) 
	  :	m_count(count),
		m_data(new Data[count]) 
	{
		assert(m_data);		
		std::copy(&data[0], &data[count], m_data);
	}
	
	~DT_Array() 
	{ 
		delete [] m_data; 
	}
	
	const Data& operator[](int i) const { return m_data[i]; }
	Data&       operator[](int i)       { return m_data[i]; }

	Size size() const { return m_count; }
	
private:
	DT_Array(const DT_Array&);
	DT_Array& operator=(const DT_Array&);

	Size  m_count;
	Data *m_data;
};
  
#endif

