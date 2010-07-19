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

  MOTTO - 3D Motion Toolkit 
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

#include "MT_Transform.h"

void MT_Transform::setValue(const float *m) {
    m_basis.setValue(m);
    m_origin.setValue(&m[12]);
    m_type = AFFINE;
}

void MT_Transform::setValue(const double *m) {
    m_basis.setValue(m);
    m_origin.setValue(&m[12]);
    m_type = AFFINE;
}

void MT_Transform::getValue(float *m) const {
    m_basis.getValue(m);
    m_origin.getValue(&m[12]);
    m[15] = 1.0;
}

void MT_Transform::getValue(double *m) const {
    m_basis.getValue(m);
    m_origin.getValue(&m[12]);
    m[15] = 1.0;
}

MT_Transform& MT_Transform::operator*=(const MT_Transform& t) {
    m_origin += m_basis * t.m_origin;
    m_basis *= t.m_basis;
    m_type |= t.m_type; 
    return *this;
}

void MT_Transform::translate(const MT_Vector3& v) { 
    m_origin += m_basis * v; 
    m_type |= TRANSLATION;
}

void MT_Transform::rotate(const MT_Quaternion& q) { 
    m_basis *= MT_Matrix3x3(q); 
    m_type |= ROTATION; 
}

void MT_Transform::scale(MT_Scalar x, MT_Scalar y, MT_Scalar z) { 
    m_basis.scale(x, y, z);  
    m_type |= SCALING;
}

void MT_Transform::setIdentity() {
    m_basis.setIdentity();
    m_origin.setValue(MT_Scalar(0.0), MT_Scalar(0.0), MT_Scalar(0.0));
    m_type = IDENTITY;
}

void MT_Transform::invert(const MT_Transform& t) {
    m_basis = t.m_type & SCALING ? 
		t.m_basis.inverse() : 
		t.m_basis.transposed();
    m_origin.setValue(-MT_dot(m_basis[0], t.m_origin), 
                      -MT_dot(m_basis[1], t.m_origin), 
                      -MT_dot(m_basis[2], t.m_origin));  
    m_type = t.m_type;
}

void MT_Transform::mult(const MT_Transform& t1, const MT_Transform& t2) {
    m_basis = t1.m_basis * t2.m_basis;
    m_origin = t1(t2.m_origin);
    m_type = t1.m_type | t2.m_type;
}

void MT_Transform::multInverseLeft(const MT_Transform& t1, const MT_Transform& t2) {
    MT_Vector3 v = t2.m_origin - t1.m_origin;
    if (t1.m_type & SCALING) {
        MT_Matrix3x3 inv = t1.m_basis.inverse();
        m_basis = inv * t2.m_basis;
        m_origin = inv * v;
    }
    else {
        m_basis = MT_multTransposeLeft(t1.m_basis, t2.m_basis);
        m_origin = v * t1.m_basis;
    }
    m_type = t1.m_type | t2.m_type;
}



