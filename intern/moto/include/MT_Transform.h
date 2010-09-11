/**
 * $Id$
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/*

  MoTo - 3D Motion Toolkit 
  Copyright (C) 2000  Gino van den Bergen <gino@acm.org>

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Library General Public
  License as published by the Free Software Foundation; either
  version 2 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Library General Public License for more details.

  You should have received a copy of the GNU Library General Public
  License along with this library; if not, write to the Free
  Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#ifndef MT_TRANSFORM_H
#define MT_TRANSFORM_H

#include "MT_Point3.h"
#include "MT_Matrix3x3.h"

class MT_Transform {
public:
    MT_Transform() {}
    MT_Transform(const float *m) { setValue(m); }
    MT_Transform(const double *m) { setValue(m); }
    MT_Transform(const MT_Point3& p, const MT_Quaternion& q)
    	: m_type(IDENTITY)
	{ 
		setOrigin(p);
		setRotation(q);
	}

    MT_Transform(const MT_Point3& p, const MT_Matrix3x3& m) 
    	: m_type(IDENTITY)
	{ 
		setOrigin(p);
		setBasis(m);
	}

	static MT_Transform Identity()
	{
		MT_Transform t;
		t.setIdentity();
		return t;
	}


    MT_Point3 operator()(const MT_Point3& p) const {
        return MT_Point3(MT_dot(m_basis[0], p) + m_origin[0], 
                         MT_dot(m_basis[1], p) + m_origin[1], 
                         MT_dot(m_basis[2], p) + m_origin[2]);
    }

    MT_Vector3 operator()(const MT_Vector3& p) const {
        return MT_Vector3(MT_dot(m_basis[0], p) + m_origin[0], 
                         MT_dot(m_basis[1], p) + m_origin[1], 
                         MT_dot(m_basis[2], p) + m_origin[2]);
    }
    
    MT_Point3 operator*(const MT_Point3& p) const {
        return (*this)(p);
    }
 
    MT_Vector3 operator*(const MT_Vector3& p) const {
        return (*this)(p);
    }


    MT_Matrix3x3&         getBasis()          { return m_basis; }
    const MT_Matrix3x3&   getBasis()    const { return m_basis; }
    MT_Point3&            getOrigin()         { return m_origin; }
    const MT_Point3&      getOrigin()   const { return m_origin; }
    MT_Quaternion         getRotation() const { return m_basis.getRotation(); }
    
    void setValue(const float *m);
    void setValue(const double *m);

    void setOrigin(const MT_Point3& origin) { 
        m_origin = origin;
		m_type |= TRANSLATION;
    }

    void setBasis(const MT_Matrix3x3& basis) { 
        m_basis = basis;
		m_type |= LINEAR;
    }

    void setRotation(const MT_Quaternion& q) {
        m_basis.setRotation(q);
		m_type &= ~SCALING;
		m_type |= ROTATION;
    }
    
    void getValue(float *m) const;
    void getValue(double *m) const;

    void setIdentity();
    
    MT_Transform& operator*=(const MT_Transform& t);

	/**
	 * Translate the origin of the transform according to the vector.
	 * @param v The vector to translate over. The vector is specified
	 *          in the coordinate system of the transform itself.
	 */
    void translate(const MT_Vector3& v);
    void rotate(const MT_Quaternion& q);
    void scale(MT_Scalar x, MT_Scalar y, MT_Scalar z);
    
    void invert(const MT_Transform& t);
    void mult(const MT_Transform& t1, const MT_Transform& t2);
    void multInverseLeft(const MT_Transform& t1, const MT_Transform& t2); 
    
private:
    enum { 
        IDENTITY    = 0x00, 
        TRANSLATION = 0x01,
        ROTATION    = 0x02,
        RIGID       = TRANSLATION | ROTATION,  
        SCALING     = 0x04,
        LINEAR      = ROTATION | SCALING,
        AFFINE      = TRANSLATION | LINEAR
    };
    
    MT_Transform(const MT_Matrix3x3& basis, const MT_Point3& origin,
                 unsigned int type) {
        setValue(basis, origin, type);
    }
    
    void setValue(const MT_Matrix3x3& basis, const MT_Point3& origin,
                  unsigned int type) {
        m_basis  = basis;
        m_origin = origin;
        m_type   = type;
    }
    
    friend MT_Transform operator*(const MT_Transform& t1, const MT_Transform& t2);

    MT_Matrix3x3 m_basis;
    MT_Point3    m_origin;
    unsigned int m_type;
};

inline MT_Transform operator*(const MT_Transform& t1, const MT_Transform& t2) {
    return MT_Transform(t1.m_basis * t2.m_basis, 
                        t1(t2.m_origin), 
                        t1.m_type | t2.m_type);
}

#endif

