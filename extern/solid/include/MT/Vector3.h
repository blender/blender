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

#ifndef VECTOR3_H
#define VECTOR3_H

#if defined (__sgi)
#include <assert.h>
#else
#include <cassert>
#endif

#include "Tuple3.h"

namespace MT {

	template <typename Scalar>	
	class Vector3 : public Tuple3<Scalar> {
	public:
		Vector3() {}
	
		template <typename Scalar2>
		explicit Vector3(const Scalar2 *v) : Tuple3<Scalar>(v) {}
		
		template <typename Scalar2>
		Vector3(const Scalar2& x, const Scalar2& y, const Scalar2& z) 
			: Tuple3<Scalar>(x, y, z) 
		{}
		
		Vector3<Scalar>& operator+=(const Vector3<Scalar>& v)
		{
			this->m_co[0] += v[0]; this->m_co[1] += v[1]; this->m_co[2] += v[2];
			return *this;
		}
		
		Vector3<Scalar>& operator-=(const Vector3<Scalar>& v) 
		{
			this->m_co[0] -= v[0]; this->m_co[1] -= v[1]; this->m_co[2] -= v[2];
			return *this;
		}

		Vector3<Scalar>& operator*=(const Scalar& s)
		{
			this->m_co[0] *= s; this->m_co[1] *= s; this->m_co[2] *= s;
			return *this;
		}
		
		Vector3<Scalar>& operator/=(const Scalar& s) 
		{
			assert(s != Scalar(0.0));
			return *this *= Scalar(1.0) / s;
		}
  
		Scalar dot(const Vector3<Scalar>& v) const
		{
			return this->m_co[0] * v[0] + this->m_co[1] * v[1] + this->m_co[2] * v[2];
		}

		Scalar length2() const
		{
			return dot(*this);
		}

		Scalar length() const
		{
			return Scalar_traits<Scalar>::sqrt(length2());
		}

		Scalar distance2(const Vector3<Scalar>& v) const 
		{
			return (v - *this).length2();
		}

		Scalar distance(const Vector3<Scalar>& v) const 
		{
			return (v - *this).length();
		}
		
		Vector3<Scalar>& normalize() 
		{
			return *this /= length();
		}
		
		Vector3<Scalar> normalized() const 
		{
			return *this / length();
		} 

		Scalar angle(const Vector3<Scalar>& v) const 
		{
			Scalar s = Scalar_traits<Scalar>::sqrt(length2() * v.length2());
			assert(s != Scalar(0.0));
			return Scalar_traits<Scalar>::acos(dot(v) / s);
		}
   
		Vector3<Scalar> absolute() const 
		{
			return Vector3<Scalar>(Scalar_traits<Scalar>::abs(this->m_co[0]), 
								   Scalar_traits<Scalar>::abs(this->m_co[1]), 
								   Scalar_traits<Scalar>::abs(this->m_co[2]));
		}

		Vector3<Scalar> cross(const Vector3<Scalar>& v) const
		{
			return Vector3<Scalar>(this->m_co[1] * v[2] - this->m_co[2] * v[1],
								   this->m_co[2] * v[0] - this->m_co[0] * v[2],
								   this->m_co[0] * v[1] - this->m_co[1] * v[0]);
		}
		
		Scalar triple(const Vector3<Scalar>& v1, const Vector3<Scalar>& v2) const
		{
			return this->m_co[0] * (v1[1] * v2[2] - v1[2] * v2[1]) + 
				   this->m_co[1] * (v1[2] * v2[0] - v1[0] * v2[2]) + 
				   this->m_co[2] * (v1[0] * v2[1] - v1[1] * v2[0]);
		}

		int minAxis() const
		{
			return this->m_co[0] < this->m_co[1] ? (this->m_co[0] < this->m_co[2] ? 0 : 2) : (this->m_co[1] < this->m_co[2] ? 1 : 2);
		}

		int maxAxis() const 
		{
			return this->m_co[0] < this->m_co[1] ? (this->m_co[1] < this->m_co[2] ? 2 : 1) : (this->m_co[0] < this->m_co[2] ? 2 : 0);
		}

		int furthestAxis() const
		{
			return absolute().minAxis();
		}

		int closestAxis() const 
		{
			return absolute().maxAxis();
		}

		Vector3<Scalar> lerp(const Vector3<Scalar>& v, const Scalar& t) const 
		{
			return Vector3<Scalar>(this->m_co[0] + (v[0] - this->m_co[0]) * t,
								   this->m_co[1] + (v[1] - this->m_co[1]) * t,
								   this->m_co[2] + (v[2] - this->m_co[2]) * t);
		}
    
		static Vector3<Scalar> random() 
		{
			Scalar z = Scalar(2.0) * Scalar_traits<Scalar>::random() - Scalar(1.0);
			Scalar r = Scalar_traits<Scalar>::sqrt(Scalar(1.0) - z * z);
			Scalar t = Scalar_traits<Scalar>::TwoTimesPi() * Scalar_traits<Scalar>::random();
			return Vector3<Scalar>(r * Scalar_traits<Scalar>::cos(t), 
								   r * Scalar_traits<Scalar>::sin(t), 
								   z);
		}
	};

	template <typename Scalar>
	inline Vector3<Scalar> 
	operator+(const Vector3<Scalar>& v1, const Vector3<Scalar>& v2) 
	{
		return Vector3<Scalar>(v1[0] + v2[0], v1[1] + v2[1], v1[2] + v2[2]);
	}

	template <typename Scalar>
	inline Vector3<Scalar> 
	operator-(const Vector3<Scalar>& v1, const Vector3<Scalar>& v2)
	{
		return Vector3<Scalar>(v1[0] - v2[0], v1[1] - v2[1], v1[2] - v2[2]);
	}
	
	template <typename Scalar>
	inline Vector3<Scalar> 
	operator-(const Vector3<Scalar>& v)
	{
		return Vector3<Scalar>(-v[0], -v[1], -v[2]);
	}
	
	template <typename Scalar>
	inline Vector3<Scalar> 
	operator*(const Vector3<Scalar>& v, const Scalar& s)
	{
		return Vector3<Scalar>(v[0] * s, v[1] * s, v[2] * s);
	}
	
	template <typename Scalar>
	inline Vector3<Scalar> 
	operator*(const Scalar& s, const Vector3<Scalar>& v)
	{ 
		return v * s; 
	}
	
	template <typename Scalar>
	inline Vector3<Scalar>
	operator/(const Vector3<Scalar>& v, const Scalar& s)
	{
		assert(s != Scalar(0.0));
		return v * (Scalar(1.0) / s);
	}
	
	template <typename Scalar>
	inline Scalar 
	dot(const Vector3<Scalar>& v1, const Vector3<Scalar>& v2) 
	{ 
		return v1.dot(v2); 
	}
	
	template <typename Scalar>
	inline Scalar
	length2(const Vector3<Scalar>& v) 
	{ 
		return v.length2(); 
	}

	template <typename Scalar>
	inline Scalar
	length(const Vector3<Scalar>& v) 
	{ 
		return v.length(); 
	}

	template <typename Scalar>
	inline Scalar
	distance2(const Vector3<Scalar>& v1, const Vector3<Scalar>& v2) 
	{ 
		return v1.distance2(v2); 
	}

	template <typename Scalar>
	inline Scalar
	distance(const Vector3<Scalar>& v1, const Vector3<Scalar>& v2) 
	{ 
		return v1.distance(v2); 
	}

	template <typename Scalar>
	inline Scalar
	angle(const Vector3<Scalar>& v1, const Vector3<Scalar>& v2) 
	{ 
		return v1.angle(v2); 
	}

	template <typename Scalar>
	inline Vector3<Scalar> 
	cross(const Vector3<Scalar>& v1, const Vector3<Scalar>& v2) 
	{ 
		return v1.cross(v2); 
	}

	template <typename Scalar>
	inline Scalar
	triple(const Vector3<Scalar>& v1, const Vector3<Scalar>& v2, const Vector3<Scalar>& v3)
	{
		return v1.triple(v2, v3);
	}

	template <typename Scalar>
	inline Vector3<Scalar> 
	lerp(const Vector3<Scalar>& v1, const Vector3<Scalar>& v2, const Scalar& t)
	{
		return v1.lerp(v2, t);
	}

}

#endif
