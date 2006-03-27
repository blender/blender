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


#ifndef SIMD_QUADWORD_H
#define SIMD_QUADWORD_H

#include "SimdScalar.h"





class	SimdQuadWord
{
	protected:
		ATTRIBUTE_ALIGNED16	(SimdScalar	m_x);
		SimdScalar	m_y;
		SimdScalar	m_z;
		SimdScalar	m_unusedW;

	public:
	
		SIMD_FORCE_INLINE SimdScalar&       operator[](int i)       { return (&m_x)[i];	}      
		SIMD_FORCE_INLINE const SimdScalar& operator[](int i) const { return (&m_x)[i]; }

		SIMD_FORCE_INLINE const SimdScalar& getX() const { return m_x; }

		SIMD_FORCE_INLINE const SimdScalar& getY() const { return m_y; }

		SIMD_FORCE_INLINE const SimdScalar& getZ() const { return m_z; }

		SIMD_FORCE_INLINE void	setX(float x) { m_x = x;};

		SIMD_FORCE_INLINE void	setY(float y) { m_y = y;};

		SIMD_FORCE_INLINE void	setZ(float z) { m_z = z;};

		SIMD_FORCE_INLINE const SimdScalar& x() const { return m_x; }

		SIMD_FORCE_INLINE const SimdScalar& y() const { return m_y; }

		SIMD_FORCE_INLINE const SimdScalar& z() const { return m_z; }


		operator       SimdScalar *()       { return &m_x; }
		operator const SimdScalar *() const { return &m_x; }

		SIMD_FORCE_INLINE void 	setValue(const SimdScalar& x, const SimdScalar& y, const SimdScalar& z)
		{
			m_x=x;
			m_y=y;
			m_z=z;
		}

/*		void getValue(SimdScalar *m) const 
		{
			m[0] = m_x;
			m[1] = m_y;
			m[2] = m_z;
		}
*/
		SIMD_FORCE_INLINE void	setValue(const SimdScalar& x, const SimdScalar& y, const SimdScalar& z,const SimdScalar& w)
		{
			m_x=x;
			m_y=y;
			m_z=z;
			m_unusedW=w;
		}

		SIMD_FORCE_INLINE SimdQuadWord() :
		m_x(0.f),m_y(0.f),m_z(0.f),m_unusedW(0.f)
		{
		}

		SIMD_FORCE_INLINE SimdQuadWord(const SimdScalar& x, const SimdScalar& y, const SimdScalar& z) 
		:m_x(x),m_y(y),m_z(z)
		//todo, remove this in release/simd ?
		,m_unusedW(0.f)
		{
		}

		SIMD_FORCE_INLINE SimdQuadWord(const SimdScalar& x, const SimdScalar& y, const SimdScalar& z,const SimdScalar& w) 
			:m_x(x),m_y(y),m_z(z),m_unusedW(w)
		{
		}


		SIMD_FORCE_INLINE void	setMax(const SimdQuadWord& other)
		{
			if (other.m_x > m_x)
				m_x = other.m_x;

			if (other.m_y > m_y)
				m_y = other.m_y;

			if (other.m_z > m_z)
				m_z = other.m_z;

			if (other.m_unusedW > m_unusedW)
				m_unusedW = other.m_unusedW;
		}

		SIMD_FORCE_INLINE void	setMin(const SimdQuadWord& other)
		{
			if (other.m_x < m_x)
				m_x = other.m_x;

			if (other.m_y < m_y)
				m_y = other.m_y;

			if (other.m_z < m_z)
				m_z = other.m_z;

			if (other.m_unusedW < m_unusedW)
				m_unusedW = other.m_unusedW;
		}



};

#endif //SIMD_QUADWORD_H
