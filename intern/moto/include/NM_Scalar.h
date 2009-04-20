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

#include <math.h>
#include <iostream>

template <class T> 
class NM_Scalar {
public:    
    NM_Scalar() {}
    explicit NM_Scalar(T value, T error = 0.0) : 
        m_value(value), m_error(error) {}

    T getValue() const { return m_value; }
    T getError() const { return m_error; }

    operator T() const { return m_value; }

    NM_Scalar operator-() const {
        return NM_Scalar<T>(-m_value, m_error);
    }

    NM_Scalar& operator=(T value) {
        m_value = value;
        m_error = 0.0;
        return *this;
    }

    NM_Scalar& operator+=(const NM_Scalar& x) {
        m_value += x.m_value;
        m_error = (fabs(m_value) * (m_error + 1.0) + 
                   fabs(x.m_value) * (x.m_error + 1.0)) /
            fabs(m_value + x.m_value);
        return *this;
    }

    NM_Scalar& operator-=(const NM_Scalar& x) {
        m_value -= x.m_value;
        m_error = (fabs(m_value) * (m_error + 1.0) + 
                   fabs(x.m_value) * (x.m_error + 1.0)) /
            fabs(m_value - x.m_value);
        return *this;
    }

    NM_Scalar& operator*=(const NM_Scalar& x) {
        m_value *= x.m_value;
        m_error += x.m_error + 1.0;
        return *this;
    }

    NM_Scalar& operator/=(const NM_Scalar& x) {
        m_value /= x.m_value;
        m_error += x.m_error + 1.0;
        return *this;
    }

private:
    T m_value;
    T m_error;
};

template <class T>
inline NM_Scalar<T> operator+(const NM_Scalar<T>& x, const NM_Scalar<T>& y) {
    return x.getValue() == 0.0 && y.getValue() == 0.0 ?
        NM_Scalar<T>(0.0, 0.0) :
        NM_Scalar<T>(x.getValue() + y.getValue(), 
                     (fabs(x.getValue()) * (x.getError() + 1.0) + 
                      fabs(y.getValue()) * (y.getError() + 1.0)) /
                     fabs(x.getValue() + y.getValue()));
}

template <class T>
inline NM_Scalar<T> operator-(const NM_Scalar<T>& x, const NM_Scalar<T>& y) {
    return x.getValue() == 0.0 && y.getValue() == 0.0 ?
        NM_Scalar<T>(0.0, 0.0) :
        NM_Scalar<T>(x.getValue() - y.getValue(), 
                     (fabs(x.getValue()) * (x.getError() + 1.0) + 
                      fabs(y.getValue()) * (y.getError() + 1.0)) /
                     fabs(x.getValue() - y.getValue()));
}

template <class T>
inline NM_Scalar<T> operator*(const NM_Scalar<T>& x, const NM_Scalar<T>& y) {
    return NM_Scalar<T>(x.getValue() * y.getValue(), 
                        x.getError() + y.getError() + 1.0);
}

template <class T>
inline NM_Scalar<T> operator/(const NM_Scalar<T>& x, const NM_Scalar<T>& y) {
    return NM_Scalar<T>(x.getValue() / y.getValue(), 
                        x.getError() + y.getError() + 1.0);
}

template <class T>
inline std::ostream& operator<<(std::ostream& os, const NM_Scalar<T>& x) {
    return os << x.getValue() << '[' << x.getError() << ']';
}

template <class T>
inline NM_Scalar<T> sqrt(const NM_Scalar<T>& x) {
    return NM_Scalar<T>(sqrt(x.getValue()),
                        0.5 * x.getError() + 1.0);
}

template <class T>
inline NM_Scalar<T> acos(const NM_Scalar<T>& x) {
    return NM_Scalar<T>(acos(x.getValue()), x.getError() + 1.0);
}

template <class T>
inline NM_Scalar<T> cos(const NM_Scalar<T>& x) {
    return NM_Scalar<T>(cos(x.getValue()), x.getError() + 1.0);
}

template <class T>
inline NM_Scalar<T> sin(const NM_Scalar<T>& x) {
    return NM_Scalar<T>(sin(x.getValue()), x.getError() + 1.0);
}

template <class T>
inline NM_Scalar<T> fabs(const NM_Scalar<T>& x) {
    return NM_Scalar<T>(fabs(x.getValue()), x.getError());
}

template <class T>
inline NM_Scalar<T> pow(const NM_Scalar<T>& x, const NM_Scalar<T>& y) {
    return NM_Scalar<T>(pow(x.getValue(), y.getValue()), 
                        fabs(y.getValue()) * x.getError() + 1.0);
}

