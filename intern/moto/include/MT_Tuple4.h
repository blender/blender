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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
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

#ifndef MT_TUPLE4_H
#define MT_TUPLE4_H

#include "MT_Stream.h"
#include "MT_Scalar.h"

class MT_Tuple4 {
public:
    MT_Tuple4() {}
    MT_Tuple4(const float *v) { setValue(v); }
    MT_Tuple4(const double *v) { setValue(v); }
    MT_Tuple4(MT_Scalar xx, MT_Scalar yy, MT_Scalar zz, MT_Scalar ww) {
        setValue(xx, yy, zz, ww);
    }
    
    MT_Scalar&       operator[](int i)       { return m_co[i]; }
    const MT_Scalar& operator[](int i) const { return m_co[i]; }
    
    MT_Scalar&       x()       { return m_co[0]; } 
    const MT_Scalar& x() const { return m_co[0]; } 

    MT_Scalar&       y()       { return m_co[1]; }
    const MT_Scalar& y() const { return m_co[1]; } 

    MT_Scalar&       z()       { return m_co[2]; } 
    const MT_Scalar& z() const { return m_co[2]; } 

    MT_Scalar&       w()       { return m_co[3]; } 
    const MT_Scalar& w() const { return m_co[3]; } 

    MT_Scalar       *getValue()       { return m_co; }
    const MT_Scalar *getValue() const { return m_co; }
    

    void getValue(float *v) const { 
        v[0] = float(m_co[0]);
		v[1] = float(m_co[1]); 
		v[2] = float(m_co[2]); 
		v[3] = float(m_co[3]);
    }
    
    void getValue(double *v) const { 
        v[0] = double(m_co[0]); 
		v[1] = double(m_co[1]); 
		v[2] = double(m_co[2]); 
		v[3] = double(m_co[3]);
    }
    
    void setValue(const float *v) {
        m_co[0] = MT_Scalar(v[0]); 
        m_co[1] = MT_Scalar(v[1]); 
        m_co[2] = MT_Scalar(v[2]); 
        m_co[3] = MT_Scalar(v[3]);
    }
    
    void setValue(const double *v) {
        m_co[0] = MT_Scalar(v[0]); 
        m_co[1] = MT_Scalar(v[1]); 
        m_co[2] = MT_Scalar(v[2]); 
        m_co[3] = MT_Scalar(v[3]);
    }
    
    void setValue(MT_Scalar xx, MT_Scalar yy, MT_Scalar zz, MT_Scalar ww) {
        m_co[0] = xx; m_co[1] = yy; m_co[2] = zz; m_co[3] = ww;
    }
    
protected:
    MT_Scalar m_co[4];                            
};

inline bool operator==(const MT_Tuple4& t1, const MT_Tuple4& t2) {
    return t1[0] == t2[0] && t1[1] == t2[1] && t1[2] == t2[2] && t1[3] == t2[3];
}

inline MT_OStream& operator<<(MT_OStream& os, const MT_Tuple4& t) {
    return os << t[0] << ' ' << t[1] << ' ' << t[2] << ' ' << t[3];
}

#endif

