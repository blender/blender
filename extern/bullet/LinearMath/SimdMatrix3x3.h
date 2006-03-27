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


#ifndef SimdMatrix3x3_H
#define SimdMatrix3x3_H

#include "SimdScalar.h"

#include "SimdVector3.h"
#include "SimdQuaternion.h"


class SimdMatrix3x3 {
	public:
		SimdMatrix3x3 () {}
		
//		explicit SimdMatrix3x3(const SimdScalar *m) { setFromOpenGLSubMatrix(m); }
		
		explicit SimdMatrix3x3(const SimdQuaternion& q) { setRotation(q); }
		/*
		template <typename SimdScalar>
		Matrix3x3(const SimdScalar& yaw, const SimdScalar& pitch, const SimdScalar& roll)
		{ 
			setEulerYPR(yaw, pitch, roll);
		}
		*/
		SimdMatrix3x3(const SimdScalar& xx, const SimdScalar& xy, const SimdScalar& xz,
				  const SimdScalar& yx, const SimdScalar& yy, const SimdScalar& yz,
				  const SimdScalar& zx, const SimdScalar& zy, const SimdScalar& zz)
		{ 
			setValue(xx, xy, xz, 
					 yx, yy, yz, 
					 zx, zy, zz);
		}
		
		SimdVector3 getColumn(int i) const
		{
			return SimdVector3(m_el[0][i],m_el[1][i],m_el[2][i]);
		}

		const SimdVector3& getRow(int i) const
		{
			return m_el[i];
		}


		SIMD_FORCE_INLINE SimdVector3&  operator[](int i)
		{ 
			assert(0 <= i && i < 3);
			return m_el[i]; 
		}
		
		const SimdVector3& operator[](int i) const
		{
			assert(0 <= i && i < 3);
			return m_el[i]; 
		}
		
		SimdMatrix3x3& operator*=(const SimdMatrix3x3& m); 
		
	
	void setFromOpenGLSubMatrix(const SimdScalar *m)
		{
			m_el[0][0] = (m[0]); 
			m_el[1][0] = (m[1]); 
			m_el[2][0] = (m[2]);
			m_el[0][1] = (m[4]); 
			m_el[1][1] = (m[5]); 
			m_el[2][1] = (m[6]);
			m_el[0][2] = (m[8]); 
			m_el[1][2] = (m[9]); 
			m_el[2][2] = (m[10]);
		}

		void setValue(const SimdScalar& xx, const SimdScalar& xy, const SimdScalar& xz, 
					  const SimdScalar& yx, const SimdScalar& yy, const SimdScalar& yz, 
					  const SimdScalar& zx, const SimdScalar& zy, const SimdScalar& zz)
		{
			m_el[0][0] = SimdScalar(xx); 
			m_el[0][1] = SimdScalar(xy); 
			m_el[0][2] = SimdScalar(xz);
			m_el[1][0] = SimdScalar(yx); 
			m_el[1][1] = SimdScalar(yy); 
			m_el[1][2] = SimdScalar(yz);
			m_el[2][0] = SimdScalar(zx); 
			m_el[2][1] = SimdScalar(zy); 
			m_el[2][2] = SimdScalar(zz);
		}
  
		void setRotation(const SimdQuaternion& q) 
		{
			SimdScalar d = q.length2();
			assert(d != SimdScalar(0.0));
			SimdScalar s = SimdScalar(2.0) / d;
			SimdScalar xs = q[0] * s,   ys = q[1] * s,   zs = q[2] * s;
			SimdScalar wx = q[3] * xs,  wy = q[3] * ys,  wz = q[3] * zs;
			SimdScalar xx = q[0] * xs,  xy = q[0] * ys,  xz = q[0] * zs;
			SimdScalar yy = q[1] * ys,  yz = q[1] * zs,  zz = q[2] * zs;
			setValue(SimdScalar(1.0) - (yy + zz), xy - wz, xz + wy,
					 xy + wz, SimdScalar(1.0) - (xx + zz), yz - wx,
					 xz - wy, yz + wx, SimdScalar(1.0) - (xx + yy));
		}
		


		void setEulerYPR(const SimdScalar& yaw, const SimdScalar& pitch, const SimdScalar& roll) 
		{

			SimdScalar cy(SimdCos(yaw)); 
			SimdScalar  sy(SimdSin(yaw)); 
			SimdScalar  cp(SimdCos(pitch)); 
			SimdScalar  sp(SimdSin(pitch)); 
			SimdScalar  cr(SimdCos(roll));
			SimdScalar  sr(SimdSin(roll));
			SimdScalar  cc = cy * cr; 
			SimdScalar  cs = cy * sr; 
			SimdScalar  sc = sy * cr; 
			SimdScalar  ss = sy * sr;
			setValue(cc - sp * ss, -cs - sp * sc, -sy * cp,
                     cp * sr,       cp * cr,      -sp,
					 sc + sp * cs, -ss + sp * cc,  cy * cp);
		
		}

	/**
	 * setEulerZYX
	 * @param euler a const reference to a SimdVector3 of euler angles
	 * These angles are used to produce a rotation matrix. The euler
	 * angles are applied in ZYX order. I.e a vector is first rotated 
	 * about X then Y and then Z
	 **/
	
	void setEulerZYX(SimdScalar eulerX,SimdScalar eulerY,SimdScalar eulerZ) {
		SimdScalar ci ( SimdCos(eulerX)); 
		SimdScalar cj ( SimdCos(eulerY)); 
		SimdScalar ch ( SimdCos(eulerZ)); 
		SimdScalar si ( SimdSin(eulerX)); 
		SimdScalar sj ( SimdSin(eulerY)); 
		SimdScalar sh ( SimdSin(eulerZ)); 
		SimdScalar cc = ci * ch; 
		SimdScalar cs = ci * sh; 
		SimdScalar sc = si * ch; 
		SimdScalar ss = si * sh;
		
		setValue(cj * ch, sj * sc - cs, sj * cc + ss,
				 cj * sh, sj * ss + cc, sj * cs - sc, 
	       			 -sj,      cj * si,      cj * ci);
	}

		void setIdentity()
		{ 
			setValue(SimdScalar(1.0), SimdScalar(0.0), SimdScalar(0.0), 
					 SimdScalar(0.0), SimdScalar(1.0), SimdScalar(0.0), 
					 SimdScalar(0.0), SimdScalar(0.0), SimdScalar(1.0)); 
		}
    
		void getOpenGLSubMatrix(SimdScalar *m) const 
		{
			m[0]  = SimdScalar(m_el[0][0]); 
			m[1]  = SimdScalar(m_el[1][0]);
			m[2]  = SimdScalar(m_el[2][0]);
			m[3]  = SimdScalar(0.0); 
			m[4]  = SimdScalar(m_el[0][1]);
			m[5]  = SimdScalar(m_el[1][1]);
			m[6]  = SimdScalar(m_el[2][1]);
			m[7]  = SimdScalar(0.0); 
			m[8]  = SimdScalar(m_el[0][2]); 
			m[9]  = SimdScalar(m_el[1][2]);
			m[10] = SimdScalar(m_el[2][2]);
			m[11] = SimdScalar(0.0); 
		}

		void getRotation(SimdQuaternion& q) const
		{
			SimdScalar trace = m_el[0][0] + m_el[1][1] + m_el[2][2];
			
			if (trace > SimdScalar(0.0)) 
			{
				SimdScalar s = SimdSqrt(trace + SimdScalar(1.0));
				q[3] = s * SimdScalar(0.5);
				s = SimdScalar(0.5) / s;
				
				q[0] = (m_el[2][1] - m_el[1][2]) * s;
				q[1] = (m_el[0][2] - m_el[2][0]) * s;
				q[2] = (m_el[1][0] - m_el[0][1]) * s;
			} 
			else 
			{
				int i = m_el[0][0] < m_el[1][1] ? 
					(m_el[1][1] < m_el[2][2] ? 2 : 1) :
					(m_el[0][0] < m_el[2][2] ? 2 : 0); 
				int j = (i + 1) % 3;  
				int k = (i + 2) % 3;
				
				SimdScalar s = SimdSqrt(m_el[i][i] - m_el[j][j] - m_el[k][k] + SimdScalar(1.0));
				q[i] = s * SimdScalar(0.5);
				s = SimdScalar(0.5) / s;
				
				q[3] = (m_el[k][j] - m_el[j][k]) * s;
				q[j] = (m_el[j][i] + m_el[i][j]) * s;
				q[k] = (m_el[k][i] + m_el[i][k]) * s;
			}
		}
	

		
		void getEuler(SimdScalar& yaw, SimdScalar& pitch, SimdScalar& roll) const
		{
			pitch = SimdScalar(SimdAsin(-m_el[2][0]));
			if (pitch < SIMD_2_PI)
			{
				if (pitch > SIMD_2_PI)
				{
					yaw = SimdScalar(SimdAtan2(m_el[1][0], m_el[0][0]));
					roll = SimdScalar(SimdAtan2(m_el[2][1], m_el[2][2]));
				}
				else 
				{
					yaw = SimdScalar(-SimdAtan2(-m_el[0][1], m_el[0][2]));
					roll = SimdScalar(0.0);
				}
			}
			else
			{
				yaw = SimdScalar(SimdAtan2(-m_el[0][1], m_el[0][2]));
				roll = SimdScalar(0.0);
			}
		}

		SimdVector3 getScaling() const
		{
			return SimdVector3(m_el[0][0] * m_el[0][0] + m_el[1][0] * m_el[1][0] + m_el[2][0] * m_el[2][0],
								   m_el[0][1] * m_el[0][1] + m_el[1][1] * m_el[1][1] + m_el[2][1] * m_el[2][1],
								   m_el[0][2] * m_el[0][2] + m_el[1][2] * m_el[1][2] + m_el[2][2] * m_el[2][2]);
		}
		
		
		SimdMatrix3x3 scaled(const SimdVector3& s) const
		{
			return SimdMatrix3x3(m_el[0][0] * s[0], m_el[0][1] * s[1], m_el[0][2] * s[2],
									 m_el[1][0] * s[0], m_el[1][1] * s[1], m_el[1][2] * s[2],
									 m_el[2][0] * s[0], m_el[2][1] * s[1], m_el[2][2] * s[2]);
		}

		SimdScalar            determinant() const;
		SimdMatrix3x3 adjoint() const;
		SimdMatrix3x3 absolute() const;
		SimdMatrix3x3 transpose() const;
		SimdMatrix3x3 inverse() const; 
		
		SimdMatrix3x3 transposeTimes(const SimdMatrix3x3& m) const;
		SimdMatrix3x3 timesTranspose(const SimdMatrix3x3& m) const;
		
		SimdScalar tdot(int c, const SimdVector3& v) const 
		{
			return m_el[0][c] * v[0] + m_el[1][c] * v[1] + m_el[2][c] * v[2];
		}
		
	protected:
		SimdScalar cofac(int r1, int c1, int r2, int c2) const 
		{
			return m_el[r1][c1] * m_el[r2][c2] - m_el[r1][c2] * m_el[r2][c1];
		}

		SimdVector3 m_el[3];
	};
	
	SIMD_FORCE_INLINE SimdMatrix3x3& 
	SimdMatrix3x3::operator*=(const SimdMatrix3x3& m)
	{
		setValue(m.tdot(0, m_el[0]), m.tdot(1, m_el[0]), m.tdot(2, m_el[0]),
				 m.tdot(0, m_el[1]), m.tdot(1, m_el[1]), m.tdot(2, m_el[1]),
				 m.tdot(0, m_el[2]), m.tdot(1, m_el[2]), m.tdot(2, m_el[2]));
		return *this;
	}
	
	SIMD_FORCE_INLINE SimdScalar 
	SimdMatrix3x3::determinant() const
	{ 
		return triple((*this)[0], (*this)[1], (*this)[2]);
	}
	

	SIMD_FORCE_INLINE SimdMatrix3x3 
	SimdMatrix3x3::absolute() const
	{
		return SimdMatrix3x3(
			SimdFabs(m_el[0][0]), SimdFabs(m_el[0][1]), SimdFabs(m_el[0][2]),
			SimdFabs(m_el[1][0]), SimdFabs(m_el[1][1]), SimdFabs(m_el[1][2]),
			SimdFabs(m_el[2][0]), SimdFabs(m_el[2][1]), SimdFabs(m_el[2][2]));
	}

	SIMD_FORCE_INLINE SimdMatrix3x3 
	SimdMatrix3x3::transpose() const 
	{
		return SimdMatrix3x3(m_el[0][0], m_el[1][0], m_el[2][0],
								 m_el[0][1], m_el[1][1], m_el[2][1],
								 m_el[0][2], m_el[1][2], m_el[2][2]);
	}
	
	SIMD_FORCE_INLINE SimdMatrix3x3 
	SimdMatrix3x3::adjoint() const 
	{
		return SimdMatrix3x3(cofac(1, 1, 2, 2), cofac(0, 2, 2, 1), cofac(0, 1, 1, 2),
								 cofac(1, 2, 2, 0), cofac(0, 0, 2, 2), cofac(0, 2, 1, 0),
								 cofac(1, 0, 2, 1), cofac(0, 1, 2, 0), cofac(0, 0, 1, 1));
	}
	
	SIMD_FORCE_INLINE SimdMatrix3x3 
	SimdMatrix3x3::inverse() const
	{
		SimdVector3 co(cofac(1, 1, 2, 2), cofac(1, 2, 2, 0), cofac(1, 0, 2, 1));
		SimdScalar det = (*this)[0].dot(co);
		assert(det != SimdScalar(0.0f));
		SimdScalar s = SimdScalar(1.0f) / det;
		return SimdMatrix3x3(co[0] * s, cofac(0, 2, 2, 1) * s, cofac(0, 1, 1, 2) * s,
								 co[1] * s, cofac(0, 0, 2, 2) * s, cofac(0, 2, 1, 0) * s,
								 co[2] * s, cofac(0, 1, 2, 0) * s, cofac(0, 0, 1, 1) * s);
	}
	
	SIMD_FORCE_INLINE SimdMatrix3x3 
	SimdMatrix3x3::transposeTimes(const SimdMatrix3x3& m) const
	{
		return SimdMatrix3x3(
			m_el[0][0] * m[0][0] + m_el[1][0] * m[1][0] + m_el[2][0] * m[2][0],
			m_el[0][0] * m[0][1] + m_el[1][0] * m[1][1] + m_el[2][0] * m[2][1],
			m_el[0][0] * m[0][2] + m_el[1][0] * m[1][2] + m_el[2][0] * m[2][2],
			m_el[0][1] * m[0][0] + m_el[1][1] * m[1][0] + m_el[2][1] * m[2][0],
			m_el[0][1] * m[0][1] + m_el[1][1] * m[1][1] + m_el[2][1] * m[2][1],
			m_el[0][1] * m[0][2] + m_el[1][1] * m[1][2] + m_el[2][1] * m[2][2],
			m_el[0][2] * m[0][0] + m_el[1][2] * m[1][0] + m_el[2][2] * m[2][0],
			m_el[0][2] * m[0][1] + m_el[1][2] * m[1][1] + m_el[2][2] * m[2][1],
			m_el[0][2] * m[0][2] + m_el[1][2] * m[1][2] + m_el[2][2] * m[2][2]);
	}
	
	SIMD_FORCE_INLINE SimdMatrix3x3 
	SimdMatrix3x3::timesTranspose(const SimdMatrix3x3& m) const
	{
		return SimdMatrix3x3(
			m_el[0].dot(m[0]), m_el[0].dot(m[1]), m_el[0].dot(m[2]),
			m_el[1].dot(m[0]), m_el[1].dot(m[1]), m_el[1].dot(m[2]),
			m_el[2].dot(m[0]), m_el[2].dot(m[1]), m_el[2].dot(m[2]));
		
	}

	SIMD_FORCE_INLINE SimdVector3 
	operator*(const SimdMatrix3x3& m, const SimdVector3& v) 
	{
		return SimdVector3(m[0].dot(v), m[1].dot(v), m[2].dot(v));
	}
	

	SIMD_FORCE_INLINE SimdVector3
	operator*(const SimdVector3& v, const SimdMatrix3x3& m)
	{
		return SimdVector3(m.tdot(0, v), m.tdot(1, v), m.tdot(2, v));
	}

	SIMD_FORCE_INLINE SimdMatrix3x3 
	operator*(const SimdMatrix3x3& m1, const SimdMatrix3x3& m2)
	{
		return SimdMatrix3x3(
			m2.tdot(0, m1[0]), m2.tdot(1, m1[0]), m2.tdot(2, m1[0]),
			m2.tdot(0, m1[1]), m2.tdot(1, m1[1]), m2.tdot(2, m1[1]),
			m2.tdot(0, m1[2]), m2.tdot(1, m1[2]), m2.tdot(2, m1[2]));
	}


	SIMD_FORCE_INLINE SimdMatrix3x3 SimdMultTransposeLeft(const SimdMatrix3x3& m1, const SimdMatrix3x3& m2) {
    return SimdMatrix3x3(
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


#endif
