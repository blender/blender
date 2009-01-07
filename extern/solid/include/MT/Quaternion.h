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

#ifndef QUATERNION_H
#define QUATERNION_H

#if defined (__sgi)
#include <assert.h>
#else
#include <cassert>
#endif

#include "Tuple4.h"
#include "Vector3.h"

namespace MT {

	template <typename Scalar>	
	class Quaternion : public Tuple4<Scalar> {
	public:
		Quaternion() {}
		
		template <typename Scalar2>
		explicit Quaternion(const Scalar2 *v) : Tuple4<Scalar>(v) {}
		
		template <typename Scalar2>
		Quaternion(const Scalar2& x, const Scalar2& y, const Scalar2& z, const Scalar2& w) 
			: Tuple4<Scalar>(x, y, z, w) 
		{}
		
		Quaternion(const Vector3<Scalar>& axis, const Scalar& angle) 
		{ 
			setRotation(axis, angle); 
		}

		template <typename Scalar2>
		Quaternion(const Scalar2& yaw, const Scalar2& pitch, const Scalar2& roll)
		{ 
			setEuler(yaw, pitch, roll); 
		}

		void setRotation(const Vector3<Scalar>& axis, const Scalar& angle)
		{
			Scalar d = axis.length();
			assert(d != Scalar(0.0));
			Scalar s = Scalar_traits<Scalar>::sin(angle * Scalar(0.5)) / d;
			setValue(axis[0] * s, axis[1] * s, axis[2] * s, 
					 Scalar_traits<Scalar>::cos(angle * Scalar(0.5)));
		}

		template <typename Scalar2>
		void setEuler(const Scalar2& yaw, const Scalar2& pitch, const Scalar2& roll)
		{
			Scalar halfYaw = Scalar(yaw) * Scalar(0.5);  
			Scalar halfPitch = Scalar(pitch) * Scalar(0.5);  
			Scalar halfRoll = Scalar(roll) * Scalar(0.5);  
			Scalar cosYaw = Scalar_traits<Scalar>::cos(halfYaw);
			Scalar sinYaw = Scalar_traits<Scalar>::sin(halfYaw);
			Scalar cosPitch = Scalar_traits<Scalar>::cos(halfPitch);
			Scalar sinPitch = Scalar_traits<Scalar>::sin(halfPitch);
			Scalar cosRoll = Scalar_traits<Scalar>::cos(halfRoll);
			Scalar sinRoll = Scalar_traits<Scalar>::sin(halfRoll);
			setValue(cosRoll * sinPitch * cosYaw + sinRoll * cosPitch * sinYaw,
					 cosRoll * cosPitch * sinYaw - sinRoll * sinPitch * cosYaw,
					 sinRoll * cosPitch * cosYaw - cosRoll * sinPitch * sinYaw,
					 cosRoll * cosPitch * cosYaw + sinRoll * sinPitch * sinYaw);
		}
  
		Quaternion<Scalar>& operator+=(const Quaternion<Scalar>& q)
		{
			this->m_co[0] += q[0]; this->m_co[1] += q[1]; this->m_co[2] += q[2]; this->m_co[3] += q[3];
			return *this;
		}
		
		Quaternion<Scalar>& operator-=(const Quaternion<Scalar>& q) 
		{
			this->m_co[0] -= q[0]; this->m_co[1] -= q[1]; this->m_co[2] -= q[2]; this->m_co[3] -= q[3];
			return *this;
		}

		Quaternion<Scalar>& operator*=(const Scalar& s)
		{
			this->m_co[0] *= s; this->m_co[1] *= s; this->m_co[2] *= s; this->m_co[3] *= s;
			return *this;
		}
		
		Quaternion<Scalar>& operator/=(const Scalar& s) 
		{
			assert(s != Scalar(0.0));
			return *this *= Scalar(1.0) / s;
		}
  
		Quaternion<Scalar>& operator*=(const Quaternion<Scalar>& q)
		{
			setValue(this->m_co[3] * q[0] + this->m_co[0] * q[3] + this->m_co[1] * q[2] - this->m_co[2] * q[1],
					 this->m_co[3] * q[1] + this->m_co[1] * q[3] + this->m_co[2] * q[0] - this->m_co[0] * q[2],
					 this->m_co[3] * q[2] + this->m_co[2] * q[3] + this->m_co[0] * q[1] - this->m_co[1] * q[0],
					 this->m_co[3] * q[3] - this->m_co[0] * q[0] - this->m_co[1] * q[1] - this->m_co[2] * q[2]);
			return *this;
		}
	
		Scalar dot(const Quaternion<Scalar>& q) const
		{
			return this->m_co[0] * q[0] + this->m_co[1] * q[1] + this->m_co[2] * q[2] + this->m_co[3] * q[3];
		}

		Scalar length2() const
		{
			return dot(*this);
		}

		Scalar length() const
		{
			return Scalar_traits<Scalar>::sqrt(length2());
		}

		Quaternion<Scalar>& normalize() 
		{
			return *this /= length();
		}
		
		Quaternion<Scalar> normalized() const 
		{
			return *this / length();
		} 

		Scalar angle(const Quaternion<Scalar>& q) const 
		{
			Scalar s = Scalar_traits<Scalar>::sqrt(length2() * q.length2());
			assert(s != Scalar(0.0));
			return Scalar_traits<Scalar>::acos(dot(q) / s);
		}
   
		Quaternion<Scalar> conjugate() const 
		{
			return Quaternion<Scalar>(-this->m_co[0], -this->m_co[1], -this->m_co[2], this->m_co[3]);
		}

		Quaternion<Scalar> inverse() const
		{
			return conjugate / length2();
		}
		
		Quaternion<Scalar> slerp(const Quaternion<Scalar>& q, const Scalar& t) const
		{
			Scalar theta = angle(q);
			if (theta != Scalar(0.0))
			{
				Scalar d = Scalar(1.0) / Scalar_traits<Scalar>::sin(theta);
				Scalar s0 = Scalar_traits<Scalar>::sin((Scalar(1.0) - t) * theta);
				Scalar s1 = Scalar_traits<Scalar>::sin(t * theta);   
				return Quaternion<Scalar>((this->m_co[0] * s0 + q[0] * s1) * d,
										  (this->m_co[1] * s0 + q[1] * s1) * d,
										  (this->m_co[2] * s0 + q[2] * s1) * d,
										  (this->m_co[3] * s0 + q[3] * s1) * d);
			}
			else
			{
				return *this;
			}
		}

		static Quaternion<Scalar> random() 
		{
			// From: "Uniform Random Rotations", Ken Shoemake, Graphics Gems III, 
            //       pg. 124-132
			Scalar x0 = Scalar_traits<Scalar>::random();
			Scalar r1 = Scalar_traits<Scalar>::sqrt(Scalar(1.0) - x0);
			Scalar r2 = Scalar_traits<Scalar>::sqrt(x0);
			Scalar t1 = Scalar_traits<Scalar>::TwoTimesPi() * Scalar_traits<Scalar>::random();
			Scalar t2 = Scalar_traits<Scalar>::TwoTimesPi() * Scalar_traits<Scalar>::random();
			Scalar c1 = Scalar_traits<Scalar>::cos(t1);
			Scalar s1 = Scalar_traits<Scalar>::sin(t1);
			Scalar c2 = Scalar_traits<Scalar>::cos(t2);
			Scalar s2 = Scalar_traits<Scalar>::sin(t2);
			return Quaternion<Scalar>(s1 * r1, c1 * r1, s2 * r2, c2 * r2);
		}

	};

	template <typename Scalar>
	inline Quaternion<Scalar>
	operator+(const Quaternion<Scalar>& q1, const Quaternion<Scalar>& q2)
	{
		return Quaternion<Scalar>(q1[0] + q2[0], q1[1] + q2[1], q1[2] + q2[2], q1[3] + q2[3]);
	}
	
	template <typename Scalar>
	inline Quaternion<Scalar>
	operator-(const Quaternion<Scalar>& q1, const Quaternion<Scalar>& q2)
	{
		return Quaternion<Scalar>(q1[0] - q2[0], q1[1] - q2[1], q1[2] - q2[2], q1[3] - q2[3]);
	}
	
	template <typename Scalar>
	inline Quaternion<Scalar>
	operator-(const Quaternion<Scalar>& q)
	{
		return Quaternion<Scalar>(-q[0], -q[1], -q[2], -q[3]);
	}

	template <typename Scalar>
	inline Quaternion<Scalar>
	operator*(const Quaternion<Scalar>& q, const Scalar& s)
	{
		return Quaternion<Scalar>(q[0] * s, q[1] * s, q[2] * s, q[3] * s);
	}
	
	template <typename Scalar>
	inline Quaternion<Scalar>
	operator*(const Scalar& s, const Quaternion<Scalar>& q)
	{
		return q * s;
	}
	
	template <typename Scalar>
	inline Quaternion<Scalar>
	operator*(const Quaternion<Scalar>& q1, const Quaternion<Scalar>& q2) {
		return Quaternion<Scalar>(q1[3] * q2[0] + q1[0] * q2[3] + q1[1] * q2[2] - q1[2] * q2[1],
								  q1[3] * q2[1] + q1[1] * q2[3] + q1[2] * q2[0] - q1[0] * q2[2],
								  q1[3] * q2[2] + q1[2] * q2[3] + q1[0] * q2[1] - q1[1] * q2[0],
								  q1[3] * q2[3] - q1[0] * q2[0] - q1[1] * q2[1] - q1[2] * q2[2]); 
	}
	
	template <typename Scalar>
	inline Quaternion<Scalar>
	operator*(const Quaternion<Scalar>& q, const Vector3<Scalar>& w)
	{
		return Quaternion<Scalar>( q[3] * w[0] + q[1] * w[2] - q[2] * w[1],
								   q[3] * w[1] + q[2] * w[0] - q[0] * w[2],
								   q[3] * w[2] + q[0] * w[1] - q[1] * w[0],
								  -q[0] * w[0] - q[1] * w[1] - q[2] * w[2]); 
	}
	
	template <typename Scalar>
	inline Quaternion<Scalar>
	operator*(const Vector3<Scalar>& w, const Quaternion<Scalar>& q)
	{
		return Quaternion<Scalar>( w[0] * q[3] + w[1] * q[2] - w[2] * q[1],
								   w[1] * q[3] + w[2] * q[0] - w[0] * q[2],
								   w[2] * q[3] + w[0] * q[1] - w[1] * q[0],
								  -w[0] * q[0] - w[1] * q[1] - w[2] * q[2]); 
	}
	
	template <typename Scalar>
	inline Scalar 
	dot(const Quaternion<Scalar>& q1, const Quaternion<Scalar>& q2) 
	{ 
		return q1.dot(q2); 
	}

	template <typename Scalar>
	inline Scalar
	length2(const Quaternion<Scalar>& q) 
	{ 
		return q.length2(); 
	}

	template <typename Scalar>
	inline Scalar
	length(const Quaternion<Scalar>& q) 
	{ 
		return q.length(); 
	}

	template <typename Scalar>
	inline Scalar
	angle(const Quaternion<Scalar>& q1, const Quaternion<Scalar>& q2) 
	{ 
		return q1.angle(q2); 
	}

	template <typename Scalar>
	inline Quaternion<Scalar>
	conjugate(const Quaternion<Scalar>& q) 
	{
		return q.conjugate();
	}

	template <typename Scalar>
	inline Quaternion<Scalar>
	inverse(const Quaternion<Scalar>& q) 
	{
		return q.inverse();
	}

	template <typename Scalar>
	inline Quaternion<Scalar>
	slerp(const Quaternion<Scalar>& q1, const Quaternion<Scalar>& q2, const Scalar& t) 
	{
		return q1.slerp(q2, t);
	}
	
}

#endif
