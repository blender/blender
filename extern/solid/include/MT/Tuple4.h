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

#ifndef TUPLE4_H
#define TUPLE4_H

#if defined (__sgi)
#include <assert.h>
#else
#include <cassert>
#endif

#include <iostream>

namespace MT {

	template <typename Scalar>
	class Tuple4 {
	public:
		Tuple4() {}
		
		template <typename Scalar2>
		explicit Tuple4(const Scalar2 *v) 
		{ 
			setValue(v);
		}
		
		template <typename Scalar2>
		Tuple4(const Scalar2& x, const Scalar2& y, const Scalar2& z, const Scalar2& w) 
		{ 
			setValue(x, y, z, w); 
		}
		
		operator       Scalar *()       { return m_co; }
		operator const Scalar *() const { return m_co; }

		Scalar&       operator[](int i)       { return m_co[i]; }      
		const Scalar& operator[](int i) const {	return m_co[i];	}
		
		Scalar&       x()       { return m_co[0]; }
		const Scalar& x() const { return m_co[0]; }
		
		Scalar&       y()       { return m_co[1]; }
		const Scalar& y() const { return m_co[1]; }
		
		Scalar&       z()       { return m_co[2]; }
		const Scalar& z() const { return m_co[2]; }

		Scalar&       w()       { return m_co[3]; }
		const Scalar& w() const { return m_co[3]; }
    
		template <typename Scalar2>
		void setValue(const Scalar2 *v) 
		{
			m_co[0] = Scalar(v[0]); 
			m_co[1] = Scalar(v[1]); 
			m_co[2] = Scalar(v[2]);
			m_co[3] = Scalar(v[3]);
		}

		template <typename Scalar2>
		void setValue(const Scalar2& x, const Scalar2& y, const Scalar2& z, const Scalar2& w)
		{
			m_co[0] = Scalar(x); 
			m_co[1] = Scalar(y); 
			m_co[2] = Scalar(z);
			m_co[3] = Scalar(w);
		}

		template <typename Scalar2>
		void getValue(Scalar2 *v) const 
		{
			v[0] = Scalar2(m_co[0]);
			v[1] = Scalar2(m_co[1]);
			v[2] = Scalar2(m_co[2]);
			v[3] = Scalar2(m_co[3]);
		}

	protected:
		Scalar m_co[4];
	};

	template <typename Scalar>
	inline std::ostream&
	operator<<(std::ostream& os, const Tuple4<Scalar>& t)
	{
		return os << t[0] << ' ' << t[1] << ' ' << t[2] << ' ' << t[3];
	}

}

#endif
