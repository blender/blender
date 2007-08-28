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

#ifndef MATRIX3X3_H
#define MATRIX3X3_H

#if defined (__sgi)
#include <assert.h>
#else
#include <cassert>
#endif

#include "Vector3.h"
#include "Quaternion.h"

namespace MT {

  // Row-major 3x3 matrix
  
	template <typename Scalar>
	class Matrix3x3 {
	public:
		Matrix3x3() {}
		
		template <typename Scalar2>
		explicit Matrix3x3(const Scalar2 *m) { setValue(m); }
		
		explicit Matrix3x3(const Quaternion<Scalar>& q) { setRotation(q); }
		
		template <typename Scalar2>
		Matrix3x3(const Scalar2& yaw, const Scalar2& pitch, const Scalar2& roll)
		{ 
			setEuler(yaw, pitch, roll);
		}
		
		template <typename Scalar2>
		Matrix3x3(const Scalar2& xx, const Scalar2& xy, const Scalar2& xz,
				  const Scalar2& yx, const Scalar2& yy, const Scalar2& yz,
				  const Scalar2& zx, const Scalar2& zy, const Scalar2& zz)
		{ 
			setValue(xx, xy, xz, 
					 yx, yy, yz, 
					 zx, zy, zz);
		}
		
		Vector3<Scalar>&  operator[](int i)
		{ 
			assert(0 <= i && i < 3);
			return m_el[i]; 
		}
		
		const Vector3<Scalar>& operator[](int i) const
		{
			assert(0 <= i && i < 3);
			return m_el[i]; 
		}
		
		Matrix3x3<Scalar>& operator*=(const Matrix3x3<Scalar>& m); 
		
		template <typename Scalar2>
		void setValue(const Scalar2 *m)
		{
			m_el[0][0] = Scalar(m[0]); 
			m_el[1][0] = Scalar(m[1]); 
			m_el[2][0] = Scalar(m[2]);
			m_el[0][1] = Scalar(m[4]); 
			m_el[1][1] = Scalar(m[5]); 
			m_el[2][1] = Scalar(m[6]);
			m_el[0][2] = Scalar(m[8]); 
			m_el[1][2] = Scalar(m[9]); 
			m_el[2][2] = Scalar(m[10]);
		}

		template <typename Scalar2>
		void setValue(const Scalar2& xx, const Scalar2& xy, const Scalar2& xz, 
					  const Scalar2& yx, const Scalar2& yy, const Scalar2& yz, 
					  const Scalar2& zx, const Scalar2& zy, const Scalar2& zz)
		{
			m_el[0][0] = Scalar(xx); 
			m_el[0][1] = Scalar(xy); 
			m_el[0][2] = Scalar(xz);
			m_el[1][0] = Scalar(yx); 
			m_el[1][1] = Scalar(yy); 
			m_el[1][2] = Scalar(yz);
			m_el[2][0] = Scalar(zx); 
			m_el[2][1] = Scalar(zy); 
			m_el[2][2] = Scalar(zz);
		}
  
		void setRotation(const Quaternion<Scalar>& q) 
		{
			Scalar d = q.length2();
			assert(d != Scalar(0.0));
			Scalar s = Scalar(2.0) / d;
			Scalar xs = q[0] * s,   ys = q[1] * s,   zs = q[2] * s;
			Scalar wx = q[3] * xs,  wy = q[3] * ys,  wz = q[3] * zs;
			Scalar xx = q[0] * xs,  xy = q[0] * ys,  xz = q[0] * zs;
			Scalar yy = q[1] * ys,  yz = q[1] * zs,  zz = q[2] * zs;
			setValue(Scalar(1.0) - (yy + zz), xy - wz, xz + wy,
					 xy + wz, Scalar(1.0) - (xx + zz), yz - wx,
					 xz - wy, yz + wx, Scalar(1.0) - (xx + yy));
		}
		
		template <typename Scalar2> 
		void setEuler(const Scalar2& yaw, const Scalar2& pitch, const Scalar2& roll) 
		{
			Scalar cy(Scalar_traits<Scalar>::cos(yaw)); 
			Scalar sy(Scalar_traits<Scalar>::sin(yaw)); 
			Scalar cp(Scalar_traits<Scalar>::cos(pitch)); 
			Scalar sp(Scalar_traits<Scalar>::sin(pitch)); 
			Scalar cr(Scalar_traits<Scalar>::cos(roll));
			Scalar sr(Scalar_traits<Scalar>::sin(roll));
			Scalar cc = cy * cr; 
			Scalar cs = cy * sr; 
			Scalar sc = sy * cr; 
			Scalar ss = sy * sr;
			setValue(cy * cp, -sc + sp * cs,  ss - sp * cc,
					 sy * cp,  cc + sp * ss, -cs + sp * sc,
					     -sp,       cp * sr,       cp * cr);
		}
		void setIdentity()
		{ 
			setValue(Scalar(1.0), Scalar(0.0), Scalar(0.0), 
					 Scalar(0.0), Scalar(1.0), Scalar(0.0), 
					 Scalar(0.0), Scalar(0.0), Scalar(1.0)); 
		}
    
		template <typename Scalar2>
		void getValue(Scalar2 *m) const 
		{
			m[0]  = Scalar2(m_el[0][0]); 
			m[1]  = Scalar2(m_el[1][0]);
			m[2]  = Scalar2(m_el[2][0]);
			m[3]  = Scalar2(0.0); 
			m[4]  = Scalar2(m_el[0][1]);
			m[5]  = Scalar2(m_el[1][1]);
			m[6]  = Scalar2(m_el[2][1]);
			m[7]  = Scalar2(0.0); 
			m[8]  = Scalar2(m_el[0][2]); 
			m[9]  = Scalar2(m_el[1][2]);
			m[10] = Scalar2(m_el[2][2]);
			m[11] = Scalar2(0.0); 
		}
		
		void getRotation(Quaternion<Scalar>& q) const
		{
			Scalar trace = m_el[0][0] + m_el[1][1] + m_el[2][2];
			
			if (trace > Scalar(0.0)) 
			{
				Scalar s = Scalar_traits<Scalar>::sqrt(trace + Scalar(1.0));
				q[3] = s * Scalar(0.5);
				s = Scalar(0.5) / s;
				
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
				
				Scalar s = Scalar_traits<Scalar>::sqrt(m_el[i][i] - m_el[j][j] - m_el[k][k] + Scalar(1.0));
				q[i] = s * Scalar(0.5);
				s = Scalar(0.5) / s;
				
				q[3] = (m_el[k][j] - m_el[j][k]) * s;
				q[j] = (m_el[j][i] + m_el[i][j]) * s;
				q[k] = (m_el[k][i] + m_el[i][k]) * s;
			}
		}


		
		template <typename Scalar2>
		void getEuler(Scalar2& yaw, Scalar2& pitch, Scalar2& roll) const
		{
			pitch = Scalar2(Scalar_traits<Scalar>::asin(-m_el[2][0]));
			if (pitch < Scalar_traits<Scalar2>::TwoTimesPi())
			{
				if (pitch > Scalar_traits<Scalar2>::TwoTimesPi())
				{
					yaw = Scalar2(Scalar_traits<Scalar>::atan2(m_el[1][0], m_el[0][0]));
					roll = Scalar2(Scalar_traits<Scalar>::atan2(m_el[2][1], m_el[2][2]));
				}
				else 
				{
					yaw = Scalar2(-Scalar_traits<Scalar>::atan2(-m_el[0][1], m_el[0][2]));
					roll = Scalar2(0.0);
				}
			}
			else
			{
				yaw = Scalar2(Scalar_traits<Scalar>::atan2(-m_el[0][1], m_el[0][2]));
				roll = Scalar2(0.0);
			}
		}

		Vector3<Scalar> getScaling() const
		{
			return Vector3<Scalar>(m_el[0][0] * m_el[0][0] + m_el[1][0] * m_el[1][0] + m_el[2][0] * m_el[2][0],
								   m_el[0][1] * m_el[0][1] + m_el[1][1] * m_el[1][1] + m_el[2][1] * m_el[2][1],
								   m_el[0][2] * m_el[0][2] + m_el[1][2] * m_el[1][2] + m_el[2][2] * m_el[2][2]);
		}
		
		
		Matrix3x3<Scalar> scaled(const Vector3<Scalar>& s) const
		{
			return Matrix3x3<Scalar>(m_el[0][0] * s[0], m_el[0][1] * s[1], m_el[0][2] * s[2],
									 m_el[1][0] * s[0], m_el[1][1] * s[1], m_el[1][2] * s[2],
									 m_el[2][0] * s[0], m_el[2][1] * s[1], m_el[2][2] * s[2]);
		}

		Scalar            determinant() const;
		Matrix3x3<Scalar> adjoint() const;
		Matrix3x3<Scalar> absolute() const;
		Matrix3x3<Scalar> transpose() const;
		Matrix3x3<Scalar> inverse() const; 
		
		Matrix3x3<Scalar> transposeTimes(const Matrix3x3<Scalar>& m) const;
		Matrix3x3<Scalar> timesTranspose(const Matrix3x3<Scalar>& m) const;
		
		Scalar tdot(int c, const Vector3<Scalar>& v) const 
		{
			return m_el[0][c] * v[0] + m_el[1][c] * v[1] + m_el[2][c] * v[2];
		}
		
	protected:
		Scalar cofac(int r1, int c1, int r2, int c2) const 
		{
			return m_el[r1][c1] * m_el[r2][c2] - m_el[r1][c2] * m_el[r2][c1];
		}

		Vector3<Scalar> m_el[3];
	};
	
	template <typename Scalar>
	inline std::ostream& 
	operator<<(std::ostream& os, const Matrix3x3<Scalar>& m)
	{
		return os << m[0] << std::endl << m[1] << std::endl << m[2] << std::endl;
	}
	
	template <typename Scalar>
	inline Matrix3x3<Scalar>& 
	Matrix3x3<Scalar>::operator*=(const Matrix3x3<Scalar>& m)
	{
		setValue(m.tdot(0, m_el[0]), m.tdot(1, m_el[0]), m.tdot(2, m_el[0]),
				 m.tdot(0, m_el[1]), m.tdot(1, m_el[1]), m.tdot(2, m_el[1]),
				 m.tdot(0, m_el[2]), m.tdot(1, m_el[2]), m.tdot(2, m_el[2]));
		return *this;
	}
	
	template <typename Scalar>
	inline Scalar 
	Matrix3x3<Scalar>::determinant() const
	{ 
		return triple((*this)[0], (*this)[1], (*this)[2]);
	}
	

	template <typename Scalar>
	inline Matrix3x3<Scalar> 
	Matrix3x3<Scalar>::absolute() const
	{
		return Matrix3x3<Scalar>(
			Scalar_traits<Scalar>::abs(m_el[0][0]), Scalar_traits<Scalar>::abs(m_el[0][1]), Scalar_traits<Scalar>::abs(m_el[0][2]),
			Scalar_traits<Scalar>::abs(m_el[1][0]), Scalar_traits<Scalar>::abs(m_el[1][1]), Scalar_traits<Scalar>::abs(m_el[1][2]),
			Scalar_traits<Scalar>::abs(m_el[2][0]), Scalar_traits<Scalar>::abs(m_el[2][1]), Scalar_traits<Scalar>::abs(m_el[2][2]));
	}

	template <typename Scalar>
	inline Matrix3x3<Scalar> 
	Matrix3x3<Scalar>::transpose() const 
	{
		return Matrix3x3<Scalar>(m_el[0][0], m_el[1][0], m_el[2][0],
								 m_el[0][1], m_el[1][1], m_el[2][1],
								 m_el[0][2], m_el[1][2], m_el[2][2]);
	}
	
	template <typename Scalar>
	inline Matrix3x3<Scalar> 
	Matrix3x3<Scalar>::adjoint() const 
	{
		return Matrix3x3<Scalar>(cofac(1, 1, 2, 2), cofac(0, 2, 2, 1), cofac(0, 1, 1, 2),
								 cofac(1, 2, 2, 0), cofac(0, 0, 2, 2), cofac(0, 2, 1, 0),
								 cofac(1, 0, 2, 1), cofac(0, 1, 2, 0), cofac(0, 0, 1, 1));
	}
	
	template <typename Scalar>
	inline Matrix3x3<Scalar> 
	Matrix3x3<Scalar>::inverse() const
	{
		Vector3<Scalar> co(cofac(1, 1, 2, 2), cofac(1, 2, 2, 0), cofac(1, 0, 2, 1));
		Scalar det = (*this)[0].dot(co);
		assert(det != Scalar(0.0));
		Scalar s = Scalar(1.0) / det;
		return Matrix3x3<Scalar>(co[0] * s, cofac(0, 2, 2, 1) * s, cofac(0, 1, 1, 2) * s,
								 co[1] * s, cofac(0, 0, 2, 2) * s, cofac(0, 2, 1, 0) * s,
								 co[2] * s, cofac(0, 1, 2, 0) * s, cofac(0, 0, 1, 1) * s);
	}
	
	template <typename Scalar>
	inline Matrix3x3<Scalar> 
	Matrix3x3<Scalar>::transposeTimes(const Matrix3x3<Scalar>& m) const
	{
		return Matrix3x3<Scalar>(
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
	
	template <typename Scalar>
	inline Matrix3x3<Scalar> 
	Matrix3x3<Scalar>::timesTranspose(const Matrix3x3<Scalar>& m) const
	{
		return Matrix3x3<Scalar>(
			m_el[0].dot(m[0]), m_el[0].dot(m[1]), m_el[0].dot(m[2]),
			m_el[1].dot(m[0]), m_el[1].dot(m[1]), m_el[1].dot(m[2]),
			m_el[2].dot(m[0]), m_el[2].dot(m[1]), m_el[2].dot(m[2]));
		
	}

	template <typename Scalar>
	inline Vector3<Scalar> 
	operator*(const Matrix3x3<Scalar>& m, const Vector3<Scalar>& v) 
	{
		return Vector3<Scalar>(m[0].dot(v), m[1].dot(v), m[2].dot(v));
	}
	

	template <typename Scalar>
	inline Vector3<Scalar>
	operator*(const Vector3<Scalar>& v, const Matrix3x3<Scalar>& m)
	{
		return Vector3<Scalar>(m.tdot(0, v), m.tdot(1, v), m.tdot(2, v));
	}

	template <typename Scalar>
	inline Matrix3x3<Scalar> 
	operator*(const Matrix3x3<Scalar>& m1, const Matrix3x3<Scalar>& m2)
	{
		return Matrix3x3<Scalar>(
			m2.tdot(0, m1[0]), m2.tdot(1, m1[0]), m2.tdot(2, m1[0]),
			m2.tdot(0, m1[1]), m2.tdot(1, m1[1]), m2.tdot(2, m1[1]),
			m2.tdot(0, m1[2]), m2.tdot(1, m1[2]), m2.tdot(2, m1[2]));
	}
}

#endif
