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


#ifndef btMatrix3x3_H
#define btMatrix3x3_H

#include "btScalar.h"

#include "btVector3.h"
#include "btQuaternion.h"


class btMatrix3x3 {
	public:
		btMatrix3x3 () {}
		
//		explicit btMatrix3x3(const btScalar *m) { setFromOpenGLSubMatrix(m); }
		
		explicit btMatrix3x3(const btQuaternion& q) { setRotation(q); }
		/*
		template <typename btScalar>
		Matrix3x3(const btScalar& yaw, const btScalar& pitch, const btScalar& roll)
		{ 
			setEulerYPR(yaw, pitch, roll);
		}
		*/
		btMatrix3x3(const btScalar& xx, const btScalar& xy, const btScalar& xz,
				  const btScalar& yx, const btScalar& yy, const btScalar& yz,
				  const btScalar& zx, const btScalar& zy, const btScalar& zz)
		{ 
			setValue(xx, xy, xz, 
					 yx, yy, yz, 
					 zx, zy, zz);
		}
		
		SIMD_FORCE_INLINE btVector3 getColumn(int i) const
		{
			return btVector3(m_el[0][i],m_el[1][i],m_el[2][i]);
		}
		


		SIMD_FORCE_INLINE const btVector3& getRow(int i) const
		{
			return m_el[i];
		}


		SIMD_FORCE_INLINE btVector3&  operator[](int i)
		{ 
			btFullAssert(0 <= i && i < 3);
			return m_el[i]; 
		}
		
		SIMD_FORCE_INLINE const btVector3& operator[](int i) const
		{
			btFullAssert(0 <= i && i < 3);
			return m_el[i]; 
		}
		
		btMatrix3x3& operator*=(const btMatrix3x3& m); 
		
	
	void setFromOpenGLSubMatrix(const btScalar *m)
		{
			m_el[0].setValue(m[0],m[4],m[8]);
			m_el[1].setValue(m[1],m[5],m[9]);
			m_el[2].setValue(m[2],m[6],m[10]);

		}

		void setValue(const btScalar& xx, const btScalar& xy, const btScalar& xz, 
					  const btScalar& yx, const btScalar& yy, const btScalar& yz, 
					  const btScalar& zx, const btScalar& zy, const btScalar& zz)
		{
			m_el[0].setValue(xx,xy,xz);
			m_el[1].setValue(yx,yy,yz);
			m_el[2].setValue(zx,zy,zz);
		}
  
		void setRotation(const btQuaternion& q) 
		{
			btScalar d = q.length2();
			btFullAssert(d != btScalar(0.0));
			btScalar s = btScalar(2.0) / d;
			btScalar xs = q.x() * s,   ys = q.y() * s,   zs = q.z() * s;
			btScalar wx = q.w() * xs,  wy = q.w() * ys,  wz = q.w() * zs;
			btScalar xx = q.x() * xs,  xy = q.x() * ys,  xz = q.x() * zs;
			btScalar yy = q.y() * ys,  yz = q.y() * zs,  zz = q.z() * zs;
			setValue(btScalar(1.0) - (yy + zz), xy - wz, xz + wy,
					 xy + wz, btScalar(1.0) - (xx + zz), yz - wx,
					 xz - wy, yz + wx, btScalar(1.0) - (xx + yy));
		}
		


		void setEulerYPR(const btScalar& yaw, const btScalar& pitch, const btScalar& roll) 
		{

			btScalar cy(btCos(yaw)); 
			btScalar  sy(btSin(yaw)); 
			btScalar  cp(btCos(pitch)); 
			btScalar  sp(btSin(pitch)); 
			btScalar  cr(btCos(roll));
			btScalar  sr(btSin(roll));
			btScalar  cc = cy * cr; 
			btScalar  cs = cy * sr; 
			btScalar  sc = sy * cr; 
			btScalar  ss = sy * sr;
			setValue(cc - sp * ss, -cs - sp * sc, -sy * cp,
                     cp * sr,       cp * cr,      -sp,
					 sc + sp * cs, -ss + sp * cc,  cy * cp);
		
		}

	/**
	 * setEulerZYX
	 * @param euler a const reference to a btVector3 of euler angles
	 * These angles are used to produce a rotation matrix. The euler
	 * angles are applied in ZYX order. I.e a vector is first rotated 
	 * about X then Y and then Z
	 **/
	
	void setEulerZYX(btScalar eulerX,btScalar eulerY,btScalar eulerZ) {
		btScalar ci ( btCos(eulerX)); 
		btScalar cj ( btCos(eulerY)); 
		btScalar ch ( btCos(eulerZ)); 
		btScalar si ( btSin(eulerX)); 
		btScalar sj ( btSin(eulerY)); 
		btScalar sh ( btSin(eulerZ)); 
		btScalar cc = ci * ch; 
		btScalar cs = ci * sh; 
		btScalar sc = si * ch; 
		btScalar ss = si * sh;
		
		setValue(cj * ch, sj * sc - cs, sj * cc + ss,
				 cj * sh, sj * ss + cc, sj * cs - sc, 
	       			 -sj,      cj * si,      cj * ci);
	}

		void setIdentity()
		{ 
			setValue(btScalar(1.0), btScalar(0.0), btScalar(0.0), 
					 btScalar(0.0), btScalar(1.0), btScalar(0.0), 
					 btScalar(0.0), btScalar(0.0), btScalar(1.0)); 
		}
    
		void getOpenGLSubMatrix(btScalar *m) const 
		{
			m[0]  = btScalar(m_el[0].x()); 
			m[1]  = btScalar(m_el[1].x());
			m[2]  = btScalar(m_el[2].x());
			m[3]  = btScalar(0.0); 
			m[4]  = btScalar(m_el[0].y());
			m[5]  = btScalar(m_el[1].y());
			m[6]  = btScalar(m_el[2].y());
			m[7]  = btScalar(0.0); 
			m[8]  = btScalar(m_el[0].z()); 
			m[9]  = btScalar(m_el[1].z());
			m[10] = btScalar(m_el[2].z());
			m[11] = btScalar(0.0); 
		}

		void getRotation(btQuaternion& q) const
		{
			btScalar trace = m_el[0].x() + m_el[1].y() + m_el[2].z();
			btScalar temp[4];
			
			if (trace > btScalar(0.0)) 
			{
				btScalar s = btSqrt(trace + btScalar(1.0));
				temp[3]=(s * btScalar(0.5));
				s = btScalar(0.5) / s;
				
				temp[0]=((m_el[2].y() - m_el[1].z()) * s);
				temp[1]=((m_el[0].z() - m_el[2].x()) * s);
				temp[2]=((m_el[1].x() - m_el[0].y()) * s);
			} 
			else 
			{
				int i = m_el[0].x() < m_el[1].y() ? 
					(m_el[1].y() < m_el[2].z() ? 2 : 1) :
					(m_el[0].x() < m_el[2].z() ? 2 : 0); 
				int j = (i + 1) % 3;  
				int k = (i + 2) % 3;
				
				btScalar s = btSqrt(m_el[i][i] - m_el[j][j] - m_el[k][k] + btScalar(1.0));
				temp[i] = s * btScalar(0.5);
				s = btScalar(0.5) / s;
				
				temp[3] = (m_el[k][j] - m_el[j][k]) * s;
				temp[j] = (m_el[j][i] + m_el[i][j]) * s;
				temp[k] = (m_el[k][i] + m_el[i][k]) * s;
			}
			q.setValue(temp[0],temp[1],temp[2],temp[3]);
		}
	
		void getEuler(btScalar& yaw, btScalar& pitch, btScalar& roll) const
		{
			
			if (btScalar(m_el[1].z()) < btScalar(1))
			{
				if (btScalar(m_el[1].z()) > -btScalar(1))
				{
					yaw = btScalar(btAtan2(m_el[1].x(), m_el[0].x()));
					pitch = btScalar(btAsin(-m_el[1].y()));
					roll = btScalar(btAtan2(m_el[2].y(), m_el[2].z()));
				}
				else 
				{
					yaw = btScalar(-btAtan2(-m_el[0].y(), m_el[0].z()));
					pitch = SIMD_HALF_PI;
					roll = btScalar(0.0);
				}
			}
			else
			{
				yaw = btScalar(btAtan2(-m_el[0].y(), m_el[0].z()));
				pitch = -SIMD_HALF_PI;
				roll = btScalar(0.0);
			}
		}
		

	
		
		btMatrix3x3 scaled(const btVector3& s) const
		{
			return btMatrix3x3(m_el[0].x() * s.x(), m_el[0].y() * s.y(), m_el[0].z() * s.z(),
									 m_el[1].x() * s.x(), m_el[1].y() * s.y(), m_el[1].z() * s.z(),
									 m_el[2].x() * s.x(), m_el[2].y() * s.y(), m_el[2].z() * s.z());
		}

		btScalar            determinant() const;
		btMatrix3x3 adjoint() const;
		btMatrix3x3 absolute() const;
		btMatrix3x3 transpose() const;
		btMatrix3x3 inverse() const; 
		
		btMatrix3x3 transposeTimes(const btMatrix3x3& m) const;
		btMatrix3x3 timesTranspose(const btMatrix3x3& m) const;
		
		SIMD_FORCE_INLINE btScalar tdotx(const btVector3& v) const 
		{
			return m_el[0].x() * v.x() + m_el[1].x() * v.y() + m_el[2].x() * v.z();
		}
		SIMD_FORCE_INLINE btScalar tdoty(const btVector3& v) const 
		{
			return m_el[0].y() * v.x() + m_el[1].y() * v.y() + m_el[2].y() * v.z();
		}
		SIMD_FORCE_INLINE btScalar tdotz(const btVector3& v) const 
		{
			return m_el[0].z() * v.x() + m_el[1].z() * v.y() + m_el[2].z() * v.z();
		}
		

		
	protected:
		btScalar cofac(int r1, int c1, int r2, int c2) const 
		{
			return m_el[r1][c1] * m_el[r2][c2] - m_el[r1][c2] * m_el[r2][c1];
		}

		btVector3 m_el[3];
	};
	
	SIMD_FORCE_INLINE btMatrix3x3& 
	btMatrix3x3::operator*=(const btMatrix3x3& m)
	{
		setValue(m.tdotx(m_el[0]), m.tdoty(m_el[0]), m.tdotz(m_el[0]),
				 m.tdotx(m_el[1]), m.tdoty(m_el[1]), m.tdotz(m_el[1]),
				 m.tdotx(m_el[2]), m.tdoty(m_el[2]), m.tdotz(m_el[2]));
		return *this;
	}
	
	SIMD_FORCE_INLINE btScalar 
	btMatrix3x3::determinant() const
	{ 
		return triple((*this)[0], (*this)[1], (*this)[2]);
	}
	

	SIMD_FORCE_INLINE btMatrix3x3 
	btMatrix3x3::absolute() const
	{
		return btMatrix3x3(
			btFabs(m_el[0].x()), btFabs(m_el[0].y()), btFabs(m_el[0].z()),
			btFabs(m_el[1].x()), btFabs(m_el[1].y()), btFabs(m_el[1].z()),
			btFabs(m_el[2].x()), btFabs(m_el[2].y()), btFabs(m_el[2].z()));
	}

	SIMD_FORCE_INLINE btMatrix3x3 
	btMatrix3x3::transpose() const 
	{
		return btMatrix3x3(m_el[0].x(), m_el[1].x(), m_el[2].x(),
								 m_el[0].y(), m_el[1].y(), m_el[2].y(),
								 m_el[0].z(), m_el[1].z(), m_el[2].z());
	}
	
	SIMD_FORCE_INLINE btMatrix3x3 
	btMatrix3x3::adjoint() const 
	{
		return btMatrix3x3(cofac(1, 1, 2, 2), cofac(0, 2, 2, 1), cofac(0, 1, 1, 2),
								 cofac(1, 2, 2, 0), cofac(0, 0, 2, 2), cofac(0, 2, 1, 0),
								 cofac(1, 0, 2, 1), cofac(0, 1, 2, 0), cofac(0, 0, 1, 1));
	}
	
	SIMD_FORCE_INLINE btMatrix3x3 
	btMatrix3x3::inverse() const
	{
		btVector3 co(cofac(1, 1, 2, 2), cofac(1, 2, 2, 0), cofac(1, 0, 2, 1));
		btScalar det = (*this)[0].dot(co);
		btFullAssert(det != btScalar(0.0));
		btScalar s = btScalar(1.0) / det;
		return btMatrix3x3(co.x() * s, cofac(0, 2, 2, 1) * s, cofac(0, 1, 1, 2) * s,
								 co.y() * s, cofac(0, 0, 2, 2) * s, cofac(0, 2, 1, 0) * s,
								 co.z() * s, cofac(0, 1, 2, 0) * s, cofac(0, 0, 1, 1) * s);
	}
	
	SIMD_FORCE_INLINE btMatrix3x3 
	btMatrix3x3::transposeTimes(const btMatrix3x3& m) const
	{
		return btMatrix3x3(
			m_el[0].x() * m[0].x() + m_el[1].x() * m[1].x() + m_el[2].x() * m[2].x(),
			m_el[0].x() * m[0].y() + m_el[1].x() * m[1].y() + m_el[2].x() * m[2].y(),
			m_el[0].x() * m[0].z() + m_el[1].x() * m[1].z() + m_el[2].x() * m[2].z(),
			m_el[0].y() * m[0].x() + m_el[1].y() * m[1].x() + m_el[2].y() * m[2].x(),
			m_el[0].y() * m[0].y() + m_el[1].y() * m[1].y() + m_el[2].y() * m[2].y(),
			m_el[0].y() * m[0].z() + m_el[1].y() * m[1].z() + m_el[2].y() * m[2].z(),
			m_el[0].z() * m[0].x() + m_el[1].z() * m[1].x() + m_el[2].z() * m[2].x(),
			m_el[0].z() * m[0].y() + m_el[1].z() * m[1].y() + m_el[2].z() * m[2].y(),
			m_el[0].z() * m[0].z() + m_el[1].z() * m[1].z() + m_el[2].z() * m[2].x());
	}
	
	SIMD_FORCE_INLINE btMatrix3x3 
	btMatrix3x3::timesTranspose(const btMatrix3x3& m) const
	{
		return btMatrix3x3(
			m_el[0].dot(m[0]), m_el[0].dot(m[1]), m_el[0].dot(m[2]),
			m_el[1].dot(m[0]), m_el[1].dot(m[1]), m_el[1].dot(m[2]),
			m_el[2].dot(m[0]), m_el[2].dot(m[1]), m_el[2].dot(m[2]));
		
	}

	SIMD_FORCE_INLINE btVector3 
	operator*(const btMatrix3x3& m, const btVector3& v) 
	{
		return btVector3(m[0].dot(v), m[1].dot(v), m[2].dot(v));
	}
	

	SIMD_FORCE_INLINE btVector3
	operator*(const btVector3& v, const btMatrix3x3& m)
	{
		return btVector3(m.tdotx(v), m.tdoty(v), m.tdotz(v));
	}

	SIMD_FORCE_INLINE btMatrix3x3 
	operator*(const btMatrix3x3& m1, const btMatrix3x3& m2)
	{
		return btMatrix3x3(
			m2.tdotx( m1[0]), m2.tdoty( m1[0]), m2.tdotz( m1[0]),
			m2.tdotx( m1[1]), m2.tdoty( m1[1]), m2.tdotz( m1[1]),
			m2.tdotx( m1[2]), m2.tdoty( m1[2]), m2.tdotz( m1[2]));
	}

/*
	SIMD_FORCE_INLINE btMatrix3x3 btMultTransposeLeft(const btMatrix3x3& m1, const btMatrix3x3& m2) {
    return btMatrix3x3(
        m1[0][0] * m2[0][0] + m1[1][0] * m2[1][0] + m1[2][0] * m2[2][0],
        m1[0][0] * m2[0][1] + m1[1][0] * m2[1][1] + m1[2][0] * m2[2][1],
        m1[0][0] * m2[0][2] + m1[1][0] * m2[1][2] + m1[2][0] * m2[2][2],
        m1[0][1] * m2[0][0] + m1[1][1] * m2[1][0] + m1[2][1] * m2[2][0],
        m1[0][1] * m2[0][1] + m1[1][1] * m2[1][1] + m1[2][1] * m2[2][1],
        m1[0][1] * m2[0][2] + m1[1][1] * m2[1][2] + m1[2][1] * m2[2][2],
        m1[0][2] * m2[0][0] + m1[1][2] * m2[1][0] + m1[2][2] * m2[2][0],
        m1[0][2] * m2[0][1] + m1[1][2] * m2[1][1] + m1[2][2] * m2[2][1],
        m1[0][2] * m2[0][2] + m1[1][2] * m2[1][2] + m1[2][2] * m2[2][2]);
}
*/


#endif
