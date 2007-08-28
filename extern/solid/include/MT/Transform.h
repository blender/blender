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

#ifndef TRANSFORM_H
#define TRANSFORM_H

#include "Vector3.h"
#include "Matrix3x3.h"

namespace MT {

	template <typename Scalar>
	class Transform {
		enum { 
			TRANSLATION = 0x01,
			ROTATION    = 0x02,
			RIGID       = TRANSLATION | ROTATION,  
			SCALING     = 0x04,
			LINEAR      = ROTATION | SCALING,
			AFFINE      = TRANSLATION | LINEAR
		};
    
	public:
		Transform() {}
		
		template <typename Scalar2>
		explicit Transform(const Scalar2 *m) { setValue(m); }

		explicit Transform(const Quaternion<Scalar>& q, 
						   const Vector3<Scalar>& c = Vector3<Scalar>(Scalar(0), Scalar(0), Scalar(0))) 
			: m_basis(q),
			  m_origin(c),
			  m_type(RIGID)
		{}

		explicit Transform(const Matrix3x3<Scalar>& b, 
						   const Vector3<Scalar>& c = Vector3<Scalar>(Scalar(0), Scalar(0), Scalar(0)), 
						   unsigned int type = AFFINE)
			: m_basis(b),
			  m_origin(c),
			  m_type(type)
		{}

		Vector3<Scalar> operator()(const Vector3<Scalar>& x) const
		{
			return Vector3<Scalar>(m_basis[0].dot(x) + m_origin[0], 
								   m_basis[1].dot(x) + m_origin[1], 
								   m_basis[2].dot(x) + m_origin[2]);
		}
    
		Vector3<Scalar> operator*(const Vector3<Scalar>& x) const
		{
			return (*this)(x);
		}

		Matrix3x3<Scalar>&       getBasis()          { return m_basis; }
		const Matrix3x3<Scalar>& getBasis()    const { return m_basis; }

		Vector3<Scalar>&         getOrigin()         { return m_origin; }
		const Vector3<Scalar>&   getOrigin()   const { return m_origin; }

		Quaternion<Scalar> getRotation() const { return m_basis.getRotation(); }
		template <typename Scalar2>
		void setValue(const Scalar2 *m) 
		{
			m_basis.setValue(m);
			m_origin.setValue(&m[12]);
			m_type = AFFINE;
		}

		template <typename Scalar2>
		void getValue(Scalar2 *m) const 
		{
			m_basis.getValue(m);
			m_origin.getValue(&m[12]);
			m[15] = Scalar2(1.0);
		}

		void setOrigin(const Vector3<Scalar>& origin) 
		{ 
			m_origin = origin;
			m_type |= TRANSLATION;
		}

		void setBasis(const Matrix3x3<Scalar>& basis)
		{ 
			m_basis = basis;
			m_type |= LINEAR;
		}

		void setRotation(const Quaternion<Scalar>& q)
		{
			m_basis.setRotation(q);
			m_type = (m_type & ~LINEAR) | ROTATION;
		}

    	void scale(const Vector3<Scalar>& scaling)
		{
			m_basis = m_basis.scaled(scaling);
			m_type |= SCALING;
		}
    
		void setIdentity()
		{
			m_basis.setIdentity();
			m_origin.setValue(Scalar(0.0), Scalar(0.0), Scalar(0.0));
			m_type = 0x0;
		}
		
		bool isIdentity() const { return m_type == 0x0; }
    
		Transform<Scalar>& operator*=(const Transform<Scalar>& t) 
		{
			m_origin += m_basis * t.m_origin;
			m_basis *= t.m_basis;
			m_type |= t.m_type; 
			return *this;
		}

		Transform<Scalar> inverse() const
		{ 
			Matrix3x3<Scalar> inv = (m_type & SCALING) ? 
				                    m_basis.inverse() : 
				                    m_basis.transpose();
			
			return Transform<Scalar>(inv, inv * -m_origin, m_type);
		}

		Transform<Scalar> inverseTimes(const Transform<Scalar>& t) const;  

		Transform<Scalar> operator*(const Transform<Scalar>& t) const;

	private:
		
		Matrix3x3<Scalar> m_basis;
		Vector3<Scalar>   m_origin;
		unsigned int      m_type;
	};


	template <typename Scalar>
	inline Transform<Scalar> 
	Transform<Scalar>::inverseTimes(const Transform<Scalar>& t) const  
	{
		Vector3<Scalar> v = t.getOrigin() - m_origin;
		if (m_type & SCALING) 
		{
			Matrix3x3<Scalar> inv = m_basis.inverse();
			return Transform<Scalar>(inv * t.getBasis(), inv * v, 
									 m_type | t.m_type);
		}
		else 
		{
			return Transform<Scalar>(m_basis.transposeTimes(t.m_basis),
									 v * m_basis, m_type | t.m_type);
		}
	}

	template <typename Scalar>
	inline Transform<Scalar> 
	Transform<Scalar>::operator*(const Transform<Scalar>& t) const
	{
		return Transform<Scalar>(m_basis * t.m_basis, 
								 (*this)(t.m_origin), 
								 m_type | t.m_type);
	}	
}

#endif
