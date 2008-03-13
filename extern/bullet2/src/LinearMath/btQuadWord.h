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

#include "btScalar.h"




///btQuadWord is base-class for vectors, points
class	btQuadWord
{
	protected:
		btScalar	m_x;
		btScalar	m_y;
		btScalar	m_z;
		btScalar	m_unusedW;

	public:
	
//		SIMD_FORCE_INLINE btScalar&       operator[](int i)       { return (&m_x)[i];	}      
//		SIMD_FORCE_INLINE const btScalar& operator[](int i) const { return (&m_x)[i]; }

		SIMD_FORCE_INLINE const btScalar& getX() const { return m_x; }

		SIMD_FORCE_INLINE const btScalar& getY() const { return m_y; }

		SIMD_FORCE_INLINE const btScalar& getZ() const { return m_z; }

		SIMD_FORCE_INLINE void	setX(btScalar x) { m_x = x;};

		SIMD_FORCE_INLINE void	setY(btScalar y) { m_y = y;};

		SIMD_FORCE_INLINE void	setZ(btScalar z) { m_z = z;};

		SIMD_FORCE_INLINE void	setW(btScalar w) { m_unusedW = w;};

		SIMD_FORCE_INLINE const btScalar& x() const { return m_x; }

		SIMD_FORCE_INLINE const btScalar& y() const { return m_y; }

		SIMD_FORCE_INLINE const btScalar& z() const { return m_z; }

		SIMD_FORCE_INLINE const btScalar& w() const { return m_unusedW; }


		SIMD_FORCE_INLINE	operator       btScalar *()       { return &m_x; }
		SIMD_FORCE_INLINE	operator const btScalar *() const { return &m_x; }

		SIMD_FORCE_INLINE void 	setValue(const btScalar& x, const btScalar& y, const btScalar& z)
		{
			m_x=x;
			m_y=y;
			m_z=z;
			m_unusedW = 0.f;
		}

/*		void getValue(btScalar *m) const 
		{
			m[0] = m_x;
			m[1] = m_y;
			m[2] = m_z;
		}
*/
		SIMD_FORCE_INLINE void	setValue(const btScalar& x, const btScalar& y, const btScalar& z,const btScalar& w)
		{
			m_x=x;
			m_y=y;
			m_z=z;
			m_unusedW=w;
		}

		SIMD_FORCE_INLINE btQuadWord()
		//	:m_x(btScalar(0.)),m_y(btScalar(0.)),m_z(btScalar(0.)),m_unusedW(btScalar(0.))
		{
		}

		SIMD_FORCE_INLINE btQuadWord(const btScalar& x, const btScalar& y, const btScalar& z) 
		:m_x(x),m_y(y),m_z(z)
		//todo, remove this in release/simd ?
		,m_unusedW(btScalar(0.))
		{
		}

		SIMD_FORCE_INLINE btQuadWord(const btScalar& x, const btScalar& y, const btScalar& z,const btScalar& w) 
			:m_x(x),m_y(y),m_z(z),m_unusedW(w)
		{
		}


		SIMD_FORCE_INLINE void	setMax(const btQuadWord& other)
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

		SIMD_FORCE_INLINE void	setMin(const btQuadWord& other)
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
