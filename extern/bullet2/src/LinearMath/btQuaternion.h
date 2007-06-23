/*
Copyright (c) 2003-2006 Gino van den Bergen / Erwin Coumans  http://continuousphysics.com/Bullet/

This software is provided 'as-is', without any express or implied warranty.
In no event will the authors be held liable for any damages arising from the use of this software.
Permission is granted to anyone to use this software for any purpose, 
including commercial applications, and to alter it and redistribute it freely, 
subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not claim that you wrote the original software. If you use this software in a product, an acknowledgment in the product documentation would be appreciated but is not required.
2. Altered source versions must be plainly marked as such, and must not be misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.
*/



#ifndef SIMD__QUATERNION_H_
#define SIMD__QUATERNION_H_

#include "btVector3.h"

class btQuaternion : public btQuadWord {
public:
	btQuaternion() {}

	//		template <typename btScalar>
	//		explicit Quaternion(const btScalar *v) : Tuple4<btScalar>(v) {}

	btQuaternion(const btScalar& x, const btScalar& y, const btScalar& z, const btScalar& w) 
		: btQuadWord(x, y, z, w) 
	{}

	btQuaternion(const btVector3& axis, const btScalar& angle) 
	{ 
		setRotation(axis, angle); 
	}

	btQuaternion(const btScalar& yaw, const btScalar& pitch, const btScalar& roll)
	{ 
		setEuler(yaw, pitch, roll); 
	}

	void setRotation(const btVector3& axis, const btScalar& angle)
	{
		btScalar d = axis.length();
		assert(d != btScalar(0.0));
		btScalar s = btSin(angle * btScalar(0.5)) / d;
		setValue(axis.x() * s, axis.y() * s, axis.z() * s, 
			btCos(angle * btScalar(0.5)));
	}

	void setEuler(const btScalar& yaw, const btScalar& pitch, const btScalar& roll)
	{
		btScalar halfYaw = btScalar(yaw) * btScalar(0.5);  
		btScalar halfPitch = btScalar(pitch) * btScalar(0.5);  
		btScalar halfRoll = btScalar(roll) * btScalar(0.5);  
		btScalar cosYaw = btCos(halfYaw);
		btScalar sinYaw = btSin(halfYaw);
		btScalar cosPitch = btCos(halfPitch);
		btScalar sinPitch = btSin(halfPitch);
		btScalar cosRoll = btCos(halfRoll);
		btScalar sinRoll = btSin(halfRoll);
		setValue(cosRoll * sinPitch * cosYaw + sinRoll * cosPitch * sinYaw,
			cosRoll * cosPitch * sinYaw - sinRoll * sinPitch * cosYaw,
			sinRoll * cosPitch * cosYaw - cosRoll * sinPitch * sinYaw,
			cosRoll * cosPitch * cosYaw + sinRoll * sinPitch * sinYaw);
	}

	btQuaternion& operator+=(const btQuaternion& q)
	{
		m_x += q.x(); m_y += q.y(); m_z += q.z(); m_unusedW += q.m_unusedW;
		return *this;
	}

	btQuaternion& operator-=(const btQuaternion& q) 
	{
		m_x -= q.x(); m_y -= q.y(); m_z -= q.z(); m_unusedW -= q.m_unusedW;
		return *this;
	}

	btQuaternion& operator*=(const btScalar& s)
	{
		m_x *= s; m_y *= s; m_z *= s; m_unusedW *= s;
		return *this;
	}


	btQuaternion& operator*=(const btQuaternion& q)
	{
		setValue(m_unusedW * q.x() + m_x * q.m_unusedW + m_y * q.z() - m_z * q.y(),
			m_unusedW * q.y() + m_y * q.m_unusedW + m_z * q.x() - m_x * q.z(),
			m_unusedW * q.z() + m_z * q.m_unusedW + m_x * q.y() - m_y * q.x(),
			m_unusedW * q.m_unusedW - m_x * q.x() - m_y * q.y() - m_z * q.z());
		return *this;
	}

	btScalar dot(const btQuaternion& q) const
	{
		return m_x * q.x() + m_y * q.y() + m_z * q.z() + m_unusedW * q.m_unusedW;
	}

	btScalar length2() const
	{
		return dot(*this);
	}

	btScalar length() const
	{
		return btSqrt(length2());
	}

	btQuaternion& normalize() 
	{
		return *this /= length();
	}

	SIMD_FORCE_INLINE btQuaternion
	operator*(const btScalar& s) const
	{
		return btQuaternion(x() * s, y() * s, z() * s, m_unusedW * s);
	}



	btQuaternion operator/(const btScalar& s) const
	{
		assert(s != btScalar(0.0));
		return *this * (btScalar(1.0) / s);
	}


	btQuaternion& operator/=(const btScalar& s) 
	{
		assert(s != btScalar(0.0));
		return *this *= btScalar(1.0) / s;
	}


	btQuaternion normalized() const 
	{
		return *this / length();
	} 

	btScalar angle(const btQuaternion& q) const 
	{
		btScalar s = btSqrt(length2() * q.length2());
		assert(s != btScalar(0.0));
		return btAcos(dot(q) / s);
	}

	btScalar getAngle() const 
	{
		btScalar s = btScalar(2.) * btAcos(m_unusedW);
		return s;
	}



	btQuaternion inverse() const
	{
		return btQuaternion(m_x, m_y, m_z, -m_unusedW);
	}

	SIMD_FORCE_INLINE btQuaternion
	operator+(const btQuaternion& q2) const
	{
		const btQuaternion& q1 = *this;
		return btQuaternion(q1.x() + q2.x(), q1.y() + q2.y(), q1.z() + q2.z(), q1.m_unusedW + q2.m_unusedW);
	}

	SIMD_FORCE_INLINE btQuaternion
	operator-(const btQuaternion& q2) const
	{
		const btQuaternion& q1 = *this;
		return btQuaternion(q1.x() - q2.x(), q1.y() - q2.y(), q1.z() - q2.z(), q1.m_unusedW - q2.m_unusedW);
	}

	SIMD_FORCE_INLINE btQuaternion operator-() const
	{
		const btQuaternion& q2 = *this;
		return btQuaternion( - q2.x(), - q2.y(),  - q2.z(),  - q2.m_unusedW);
	}

	SIMD_FORCE_INLINE btQuaternion farthest( const btQuaternion& qd) const 
	{
		btQuaternion diff,sum;
		diff = *this - qd;
		sum = *this + qd;
		if( diff.dot(diff) > sum.dot(sum) )
			return qd;
		return (-qd);
	}

	btQuaternion slerp(const btQuaternion& q, const btScalar& t) const
	{
		btScalar theta = angle(q);
		if (theta != btScalar(0.0))
		{
			btScalar d = btScalar(1.0) / btSin(theta);
			btScalar s0 = btSin((btScalar(1.0) - t) * theta);
			btScalar s1 = btSin(t * theta);   
			return btQuaternion((m_x * s0 + q.x() * s1) * d,
				(m_y * s0 + q.y() * s1) * d,
				(m_z * s0 + q.z() * s1) * d,
				(m_unusedW * s0 + q.m_unusedW * s1) * d);
		}
		else
		{
			return *this;
		}
	}

	SIMD_FORCE_INLINE const btScalar& getW() const { return m_unusedW; }

};



SIMD_FORCE_INLINE btQuaternion
operator-(const btQuaternion& q)
{
	return btQuaternion(-q.x(), -q.y(), -q.z(), -q.w());
}




SIMD_FORCE_INLINE btQuaternion
operator*(const btQuaternion& q1, const btQuaternion& q2) {
	return btQuaternion(q1.w() * q2.x() + q1.x() * q2.w() + q1.y() * q2.z() - q1.z() * q2.y(),
		q1.w() * q2.y() + q1.y() * q2.w() + q1.z() * q2.x() - q1.x() * q2.z(),
		q1.w() * q2.z() + q1.z() * q2.w() + q1.x() * q2.y() - q1.y() * q2.x(),
		q1.w() * q2.w() - q1.x() * q2.x() - q1.y() * q2.y() - q1.z() * q2.z()); 
}

SIMD_FORCE_INLINE btQuaternion
operator*(const btQuaternion& q, const btVector3& w)
{
	return btQuaternion( q.w() * w.x() + q.y() * w.z() - q.z() * w.y(),
		q.w() * w.y() + q.z() * w.x() - q.x() * w.z(),
		q.w() * w.z() + q.x() * w.y() - q.y() * w.x(),
		-q.x() * w.x() - q.y() * w.y() - q.z() * w.z()); 
}

SIMD_FORCE_INLINE btQuaternion
operator*(const btVector3& w, const btQuaternion& q)
{
	return btQuaternion( w.x() * q.w() + w.y() * q.z() - w.z() * q.y(),
		w.y() * q.w() + w.z() * q.x() - w.x() * q.z(),
		w.z() * q.w() + w.x() * q.y() - w.y() * q.x(),
		-w.x() * q.x() - w.y() * q.y() - w.z() * q.z()); 
}

SIMD_FORCE_INLINE btScalar 
dot(const btQuaternion& q1, const btQuaternion& q2) 
{ 
	return q1.dot(q2); 
}


SIMD_FORCE_INLINE btScalar
length(const btQuaternion& q) 
{ 
	return q.length(); 
}

SIMD_FORCE_INLINE btScalar
angle(const btQuaternion& q1, const btQuaternion& q2) 
{ 
	return q1.angle(q2); 
}


SIMD_FORCE_INLINE btQuaternion
inverse(const btQuaternion& q) 
{
	return q.inverse();
}

SIMD_FORCE_INLINE btQuaternion
slerp(const btQuaternion& q1, const btQuaternion& q2, const btScalar& t) 
{
	return q1.slerp(q2, t);
}


#endif



