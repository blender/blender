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

#include "SimdQuadWord.h"


///SimdVector3 is 16byte aligned, and has an extra unused component m_w
///this extra component can be used by derived classes (Quaternion?) or by user
class SimdVector3 : public SimdQuadWord {


public:
	SIMD_FORCE_INLINE SimdVector3() {}

	

	SIMD_FORCE_INLINE SimdVector3(const SimdScalar& x, const SimdScalar& y, const SimdScalar& z) 
		:SimdQuadWord(x,y,z,0.f)
	{
	}

//	SIMD_FORCE_INLINE SimdVector3(const SimdScalar& x, const SimdScalar& y, const SimdScalar& z,const SimdScalar& w) 
//		: SimdQuadWord(x,y,z,w)
//	{
//	}

	

	SIMD_FORCE_INLINE SimdVector3& operator+=(const SimdVector3& v)
	{
		m_x += v.x(); m_y += v.y(); m_z += v.z();
		return *this;
	}



	SIMD_FORCE_INLINE SimdVector3& operator-=(const SimdVector3& v) 
	{
		m_x -= v.x(); m_y -= v.y(); m_z -= v.z();
		return *this;
	}

	SIMD_FORCE_INLINE SimdVector3& operator*=(const SimdScalar& s)
	{
		m_x *= s; m_y *= s; m_z *= s;
		return *this;
	}

	SIMD_FORCE_INLINE SimdVector3& operator/=(const SimdScalar& s) 
	{
		assert(s != SimdScalar(0.0));
		return *this *= SimdScalar(1.0) / s;
	}

	SIMD_FORCE_INLINE SimdScalar dot(const SimdVector3& v) const
	{
		return m_x * v.x() + m_y * v.y() + m_z * v.z();
	}

	SIMD_FORCE_INLINE SimdScalar length2() const
	{
		return dot(*this);
	}

	SIMD_FORCE_INLINE SimdScalar length() const
	{
		return SimdSqrt(length2());
	}

	SIMD_FORCE_INLINE SimdScalar distance2(const SimdVector3& v) const;

	SIMD_FORCE_INLINE SimdScalar distance(const SimdVector3& v) const;

	SIMD_FORCE_INLINE SimdVector3& normalize() 
	{
		return *this /= length();
	}

	SIMD_FORCE_INLINE SimdVector3 normalized() const;

	SIMD_FORCE_INLINE SimdVector3 rotate( const SimdVector3& wAxis, const SimdScalar angle );

	SIMD_FORCE_INLINE SimdScalar angle(const SimdVector3& v) const 
	{
		SimdScalar s = SimdSqrt(length2() * v.length2());
		assert(s != SimdScalar(0.0));
		return SimdAcos(dot(v) / s);
	}

	SIMD_FORCE_INLINE SimdVector3 absolute() const 
	{
		return SimdVector3(
			SimdFabs(m_x), 
			SimdFabs(m_y), 
			SimdFabs(m_z));
	}

	SIMD_FORCE_INLINE SimdVector3 cross(const SimdVector3& v) const
	{
		return SimdVector3(
			m_y * v.z() - m_z * v.y(),
			m_z * v.x() - m_x * v.z(),
			m_x * v.y() - m_y * v.x());
	}

	SIMD_FORCE_INLINE SimdScalar triple(const SimdVector3& v1, const SimdVector3& v2) const
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

	SIMD_FORCE_INLINE void setInterpolate3(const SimdVector3& v0, const SimdVector3& v1, SimdScalar rt)
	{
		SimdScalar s = 1.0f - rt;
		m_x = s * v0[0] + rt * v1.x();
		m_y = s * v0[1] + rt * v1.y();
		m_z = s * v0[2] + rt * v1.z();
		//don't do the unused w component
		//		m_co[3] = s * v0[3] + rt * v1[3];
	}

	SIMD_FORCE_INLINE SimdVector3 lerp(const SimdVector3& v, const SimdScalar& t) const 
	{
		return SimdVector3(m_x + (v.x() - m_x) * t,
			m_y + (v.y() - m_y) * t,
			m_z + (v.z() - m_z) * t);
	}


	SIMD_FORCE_INLINE SimdVector3& operator*=(const SimdVector3& v)
	{
		m_x *= v.x(); m_y *= v.y(); m_z *= v.z();
		return *this;
	}

	

};

SIMD_FORCE_INLINE SimdVector3 
operator+(const SimdVector3& v1, const SimdVector3& v2) 
{
	return SimdVector3(v1.x() + v2.x(), v1.y() + v2.y(), v1.z() + v2.z());
}

SIMD_FORCE_INLINE SimdVector3 
operator*(const SimdVector3& v1, const SimdVector3& v2) 
{
	return SimdVector3(v1.x() * v2.x(), v1.y() * v2.y(), v1.z() *+ v2.z());
}

SIMD_FORCE_INLINE SimdVector3 
operator-(const SimdVector3& v1, const SimdVector3& v2)
{
	return SimdVector3(v1.x() - v2.x(), v1.y() - v2.y(), v1.z() - v2.z());
}

SIMD_FORCE_INLINE SimdVector3 
operator-(const SimdVector3& v)
{
	return SimdVector3(-v.x(), -v.y(), -v.z());
}

SIMD_FORCE_INLINE SimdVector3 
operator*(const SimdVector3& v, const SimdScalar& s)
{
	return SimdVector3(v.x() * s, v.y() * s, v.z() * s);
}

SIMD_FORCE_INLINE SimdVector3 
operator*(const SimdScalar& s, const SimdVector3& v)
{ 
	return v * s; 
}

SIMD_FORCE_INLINE SimdVector3
operator/(const SimdVector3& v, const SimdScalar& s)
{
	assert(s != SimdScalar(0.0));
	return v * (SimdScalar(1.0) / s);
}

SIMD_FORCE_INLINE SimdVector3
operator/(const SimdVector3& v1, const SimdVector3& v2)
{
	return SimdVector3(v1.x() / v2.x(),v1.y() / v2.y(),v1.z() / v2.z());
}

SIMD_FORCE_INLINE SimdScalar 
dot(const SimdVector3& v1, const SimdVector3& v2) 
{ 
	return v1.dot(v2); 
}



SIMD_FORCE_INLINE SimdScalar
distance2(const SimdVector3& v1, const SimdVector3& v2) 
{ 
	return v1.distance2(v2); 
}


SIMD_FORCE_INLINE SimdScalar
distance(const SimdVector3& v1, const SimdVector3& v2) 
{ 
	return v1.distance(v2); 
}

SIMD_FORCE_INLINE SimdScalar
angle(const SimdVector3& v1, const SimdVector3& v2) 
{ 
	return v1.angle(v2); 
}

SIMD_FORCE_INLINE SimdVector3 
cross(const SimdVector3& v1, const SimdVector3& v2) 
{ 
	return v1.cross(v2); 
}

SIMD_FORCE_INLINE SimdScalar
triple(const SimdVector3& v1, const SimdVector3& v2, const SimdVector3& v3)
{
	return v1.triple(v2, v3);
}

SIMD_FORCE_INLINE SimdVector3 
lerp(const SimdVector3& v1, const SimdVector3& v2, const SimdScalar& t)
{
	return v1.lerp(v2, t);
}


SIMD_FORCE_INLINE bool operator==(const SimdVector3& p1, const SimdVector3& p2) 
{
	return p1[0] == p2[0] && p1[1] == p2[1] && p1[2] == p2[2];
}

SIMD_FORCE_INLINE SimdScalar SimdVector3::distance2(const SimdVector3& v) const
{
	return (v - *this).length2();
}

SIMD_FORCE_INLINE SimdScalar SimdVector3::distance(const SimdVector3& v) const
{
	return (v - *this).length();
}

SIMD_FORCE_INLINE SimdVector3 SimdVector3::normalized() const
{
	return *this / length();
} 

SIMD_FORCE_INLINE SimdVector3 SimdVector3::rotate( const SimdVector3& wAxis, const SimdScalar angle )
{
	// wAxis must be a unit lenght vector

	SimdVector3 o = wAxis * wAxis.dot( *this );
	SimdVector3 x = *this - o;
	SimdVector3 y;

	y = wAxis.cross( *this );

	return ( o + x * SimdCos( angle ) + y * SimdSin( angle ) );
}

class SimdVector4 : public SimdVector3
{
public:

	SIMD_FORCE_INLINE SimdVector4() {}


	SIMD_FORCE_INLINE SimdVector4(const SimdScalar& x, const SimdScalar& y, const SimdScalar& z,const SimdScalar& w) 
		: SimdVector3(x,y,z)
	{
		m_unusedW = w;
	}


	SIMD_FORCE_INLINE SimdVector4 absolute4() const 
	{
		return SimdVector4(
			SimdFabs(m_x), 
			SimdFabs(m_y), 
			SimdFabs(m_z),
			SimdFabs(m_unusedW));
	}



	float	getW() const { return m_unusedW;}


		SIMD_FORCE_INLINE int maxAxis4() const
	{
		int maxIndex = -1;
		float maxVal = -1e30f;
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
		float minVal = 1e30f;
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
