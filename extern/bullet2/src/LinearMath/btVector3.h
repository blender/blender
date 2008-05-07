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



#ifndef SIMD__VECTOR3_H
#define SIMD__VECTOR3_H

#include "btQuadWord.h"

///btVector3 can be used to represent 3D points and vectors.
///It has an un-used w component to suit 16-byte alignment when btVector3 is stored in containers. This extra component can be used by derived classes (Quaternion?) or by user
///Ideally, this class should be replaced by a platform optimized SIMD version that keeps the data in registers
class	btVector3 : public btQuadWord {

public:
	SIMD_FORCE_INLINE btVector3() {}

	

	SIMD_FORCE_INLINE btVector3(const btScalar& x, const btScalar& y, const btScalar& z) 
		:btQuadWord(x,y,z,btScalar(0.))
	{
	}

//	SIMD_FORCE_INLINE btVector3(const btScalar& x, const btScalar& y, const btScalar& z,const btScalar& w) 
//		: btQuadWord(x,y,z,w)
//	{
//	}

	

	SIMD_FORCE_INLINE btVector3& operator+=(const btVector3& v)
	{
		m_x += v.x(); m_y += v.y(); m_z += v.z();
		return *this;
	}



	SIMD_FORCE_INLINE btVector3& operator-=(const btVector3& v) 
	{
		m_x -= v.x(); m_y -= v.y(); m_z -= v.z();
		return *this;
	}

	SIMD_FORCE_INLINE btVector3& operator*=(const btScalar& s)
	{
		m_x *= s; m_y *= s; m_z *= s;
		return *this;
	}

	SIMD_FORCE_INLINE btVector3& operator/=(const btScalar& s) 
	{
		btFullAssert(s != btScalar(0.0));
		return *this *= btScalar(1.0) / s;
	}

	SIMD_FORCE_INLINE btScalar dot(const btVector3& v) const
	{
		return m_x * v.x() + m_y * v.y() + m_z * v.z();
	}

	SIMD_FORCE_INLINE btScalar length2() const
	{
		return dot(*this);
	}

	SIMD_FORCE_INLINE btScalar length() const
	{
		return btSqrt(length2());
	}

	SIMD_FORCE_INLINE btScalar distance2(const btVector3& v) const;

	SIMD_FORCE_INLINE btScalar distance(const btVector3& v) const;

	SIMD_FORCE_INLINE btVector3& normalize() 
	{
		return *this /= length();
	}

	SIMD_FORCE_INLINE btVector3 normalized() const;

	SIMD_FORCE_INLINE btVector3 rotate( const btVector3& wAxis, const btScalar angle );

	SIMD_FORCE_INLINE btScalar angle(const btVector3& v) const 
	{
		btScalar s = btSqrt(length2() * v.length2());
		btFullAssert(s != btScalar(0.0));
		return btAcos(dot(v) / s);
	}

	SIMD_FORCE_INLINE btVector3 absolute() const 
	{
		return btVector3(
			btFabs(m_x), 
			btFabs(m_y), 
			btFabs(m_z));
	}

	SIMD_FORCE_INLINE btVector3 cross(const btVector3& v) const
	{
		return btVector3(
			m_y * v.z() - m_z * v.y(),
			m_z * v.x() - m_x * v.z(),
			m_x * v.y() - m_y * v.x());
	}

	SIMD_FORCE_INLINE btScalar triple(const btVector3& v1, const btVector3& v2) const
	{
		return m_x * (v1.y() * v2.z() - v1.z() * v2.y()) + 
			m_y * (v1.z() * v2.x() - v1.x() * v2.z()) + 
			m_z * (v1.x() * v2.y() - v1.y() * v2.x());
	}

	SIMD_FORCE_INLINE int minAxis() const
	{
		return m_x < m_y ? (m_x < m_z ? 0 : 2) : (m_y < m_z ? 1 : 2);
	}

	SIMD_FORCE_INLINE int maxAxis() const 
	{
		return m_x < m_y ? (m_y < m_z ? 2 : 1) : (m_x < m_z ? 2 : 0);
	}

	SIMD_FORCE_INLINE int furthestAxis() const
	{
		return absolute().minAxis();
	}

	SIMD_FORCE_INLINE int closestAxis() const 
	{
		return absolute().maxAxis();
	}

	SIMD_FORCE_INLINE void setInterpolate3(const btVector3& v0, const btVector3& v1, btScalar rt)
	{
		btScalar s = btScalar(1.0) - rt;
		m_x = s * v0.x() + rt * v1.x();
		m_y = s * v0.y() + rt * v1.y();
		m_z = s * v0.z() + rt * v1.z();
		//don't do the unused w component
		//		m_co[3] = s * v0[3] + rt * v1[3];
	}

	SIMD_FORCE_INLINE btVector3 lerp(const btVector3& v, const btScalar& t) const 
	{
		return btVector3(m_x + (v.x() - m_x) * t,
			m_y + (v.y() - m_y) * t,
			m_z + (v.z() - m_z) * t);
	}


	SIMD_FORCE_INLINE btVector3& operator*=(const btVector3& v)
	{
		m_x *= v.x(); m_y *= v.y(); m_z *= v.z();
		return *this;
	}

	

};

SIMD_FORCE_INLINE btVector3 
operator+(const btVector3& v1, const btVector3& v2) 
{
	return btVector3(v1.x() + v2.x(), v1.y() + v2.y(), v1.z() + v2.z());
}

SIMD_FORCE_INLINE btVector3 
operator*(const btVector3& v1, const btVector3& v2) 
{
	return btVector3(v1.x() * v2.x(), v1.y() * v2.y(), v1.z() * v2.z());
}

SIMD_FORCE_INLINE btVector3 
operator-(const btVector3& v1, const btVector3& v2)
{
	return btVector3(v1.x() - v2.x(), v1.y() - v2.y(), v1.z() - v2.z());
}

SIMD_FORCE_INLINE btVector3 
operator-(const btVector3& v)
{
	return btVector3(-v.x(), -v.y(), -v.z());
}

SIMD_FORCE_INLINE btVector3 
operator*(const btVector3& v, const btScalar& s)
{
	return btVector3(v.x() * s, v.y() * s, v.z() * s);
}

SIMD_FORCE_INLINE btVector3 
operator*(const btScalar& s, const btVector3& v)
{ 
	return v * s; 
}

SIMD_FORCE_INLINE btVector3
operator/(const btVector3& v, const btScalar& s)
{
	btFullAssert(s != btScalar(0.0));
	return v * (btScalar(1.0) / s);
}

SIMD_FORCE_INLINE btVector3
operator/(const btVector3& v1, const btVector3& v2)
{
	return btVector3(v1.x() / v2.x(),v1.y() / v2.y(),v1.z() / v2.z());
}

SIMD_FORCE_INLINE btScalar 
dot(const btVector3& v1, const btVector3& v2) 
{ 
	return v1.dot(v2); 
}



SIMD_FORCE_INLINE btScalar
distance2(const btVector3& v1, const btVector3& v2) 
{ 
	return v1.distance2(v2); 
}


SIMD_FORCE_INLINE btScalar
distance(const btVector3& v1, const btVector3& v2) 
{ 
	return v1.distance(v2); 
}

SIMD_FORCE_INLINE btScalar
angle(const btVector3& v1, const btVector3& v2) 
{ 
	return v1.angle(v2); 
}

SIMD_FORCE_INLINE btVector3 
cross(const btVector3& v1, const btVector3& v2) 
{ 
	return v1.cross(v2); 
}

SIMD_FORCE_INLINE btScalar
triple(const btVector3& v1, const btVector3& v2, const btVector3& v3)
{
	return v1.triple(v2, v3);
}

SIMD_FORCE_INLINE btVector3 
lerp(const btVector3& v1, const btVector3& v2, const btScalar& t)
{
	return v1.lerp(v2, t);
}


SIMD_FORCE_INLINE bool operator==(const btVector3& p1, const btVector3& p2) 
{
	return p1.x() == p2.x() && p1.y() == p2.y() && p1.z() == p2.z();
}

SIMD_FORCE_INLINE btScalar btVector3::distance2(const btVector3& v) const
{
	return (v - *this).length2();
}

SIMD_FORCE_INLINE btScalar btVector3::distance(const btVector3& v) const
{
	return (v - *this).length();
}

SIMD_FORCE_INLINE btVector3 btVector3::normalized() const
{
	return *this / length();
} 

SIMD_FORCE_INLINE btVector3 btVector3::rotate( const btVector3& wAxis, const btScalar angle )
{
	// wAxis must be a unit lenght vector

	btVector3 o = wAxis * wAxis.dot( *this );
	btVector3 x = *this - o;
	btVector3 y;

	y = wAxis.cross( *this );

	return ( o + x * btCos( angle ) + y * btSin( angle ) );
}

class btVector4 : public btVector3
{
public:

	SIMD_FORCE_INLINE btVector4() {}


	SIMD_FORCE_INLINE btVector4(const btScalar& x, const btScalar& y, const btScalar& z,const btScalar& w) 
		: btVector3(x,y,z)
	{
		m_unusedW = w;
	}


	SIMD_FORCE_INLINE btVector4 absolute4() const 
	{
		return btVector4(
			btFabs(m_x), 
			btFabs(m_y), 
			btFabs(m_z),
			btFabs(m_unusedW));
	}



	btScalar	getW() const { return m_unusedW;}


		SIMD_FORCE_INLINE int maxAxis4() const
	{
		int maxIndex = -1;
		btScalar maxVal = btScalar(-1e30);
		if (m_x > maxVal)
		{
			maxIndex = 0;
			maxVal = m_x;
		}
		if (m_y > maxVal)
		{
			maxIndex = 1;
			maxVal = m_y;
		}
		if (m_z > maxVal)
		{
			maxIndex = 2;
			maxVal = m_z;
		}
		if (m_unusedW > maxVal)
		{
			maxIndex = 3;
			maxVal = m_unusedW;
		}
		
		
		

		return maxIndex;

	}


	SIMD_FORCE_INLINE int minAxis4() const
	{
		int minIndex = -1;
		btScalar minVal = btScalar(1e30);
		if (m_x < minVal)
		{
			minIndex = 0;
			minVal = m_x;
		}
		if (m_y < minVal)
		{
			minIndex = 1;
			minVal = m_y;
		}
		if (m_z < minVal)
		{
			minIndex = 2;
			minVal = m_z;
		}
		if (m_unusedW < minVal)
		{
			minIndex = 3;
			minVal = m_unusedW;
		}
		
		return minIndex;

	}


	SIMD_FORCE_INLINE int closestAxis4() const 
	{
		return absolute4().maxAxis4();
	}

};

#endif //SIMD__VECTOR3_H
