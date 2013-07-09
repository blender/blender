/*
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

/** \file moto/include/MT_Quaternion.h
 *  \ingroup moto
 */


/*

 * Copyright (c) 2000 Gino van den Bergen <gino@acm.org>
 *
 * Permission to use, copy, modify, distribute and sell this software
 * and its documentation for any purpose is hereby granted without fee,
 * provided that the above copyright notice appear in all copies and
 * that both that copyright notice and this permission notice appear
 * in supporting documentation.  Gino van den Bergen makes no
 * representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied warranty.
 *
 */

#ifndef MT_QUATERNION_H
#define MT_QUATERNION_H

#include <MT_assert.h>

#include "MT_Vector3.h"
#include "MT_Vector4.h"

class MT_Quaternion : public MT_Vector4 {
public:
    MT_Quaternion() {}
    MT_Quaternion(const MT_Vector4& v) : MT_Vector4(v) {}
    MT_Quaternion(const float v[4]) : MT_Vector4(v) {}
    MT_Quaternion(const double v[4]) : MT_Vector4(v) {}
    MT_Quaternion(MT_Scalar xx, MT_Scalar yy, MT_Scalar zz, MT_Scalar ww) :
        MT_Vector4(xx, yy, zz, ww) {}
    MT_Quaternion(const MT_Vector3& axis, MT_Scalar mt_angle) { 
        setRotation(axis, mt_angle); 
    }
    MT_Quaternion(MT_Scalar yaw, MT_Scalar pitch, MT_Scalar roll) { 
        setEuler(yaw, pitch, roll); 
    }

    void setRotation(const MT_Vector3& axis, MT_Scalar mt_angle) {
        MT_Scalar d = axis.length();
        MT_assert(!MT_fuzzyZero(d));
        MT_Scalar s = sin(mt_angle * MT_Scalar(0.5)) / d;
        setValue(axis[0] * s, axis[1] * s, axis[2] * s, 
                 cos(mt_angle * MT_Scalar(0.5)));
    }

    void setEuler(MT_Scalar yaw, MT_Scalar pitch, MT_Scalar roll) {
        MT_Scalar cosYaw = cos(yaw * MT_Scalar(0.5));
        MT_Scalar sinYaw = sin(yaw * MT_Scalar(0.5));
        MT_Scalar cosPitch = cos(pitch * MT_Scalar(0.5));
        MT_Scalar sinPitch = sin(pitch * MT_Scalar(0.5));
        MT_Scalar cosRoll = cos(roll * MT_Scalar(0.5));
        MT_Scalar sinRoll = sin(roll * MT_Scalar(0.5));
        setValue(cosRoll * sinPitch * cosYaw + sinRoll * cosPitch * sinYaw,
                 cosRoll * cosPitch * sinYaw - sinRoll * sinPitch * cosYaw,
                 sinRoll * cosPitch * cosYaw - cosRoll * sinPitch * sinYaw,
                 cosRoll * cosPitch * cosYaw + sinRoll * sinPitch * sinYaw);
    }
  
    MT_Quaternion& operator*=(const MT_Quaternion& q);
  
    void conjugate();
    MT_Quaternion conjugate() const;

    void invert();
    MT_Quaternion inverse() const;

    MT_Scalar angle(const MT_Quaternion& q) const;
    MT_Quaternion slerp(const MT_Quaternion& q, const MT_Scalar& t) const;
    
    static MT_Quaternion random();
};

MT_Quaternion operator*(const MT_Quaternion& q1, const MT_Quaternion& q2);
MT_Quaternion operator*(const MT_Quaternion& q, const MT_Vector3& w);
MT_Quaternion operator*(const MT_Vector3& w, const MT_Quaternion& q);

#ifdef GEN_INLINED
#include "MT_Quaternion.inl"
#endif

#endif

