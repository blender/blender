#include "MT_Optimize.h"

GEN_INLINE MT_Vector4& MT_Vector4::operator+=(const MT_Vector4& v) {
    m_co[0] += v[0]; m_co[1] += v[1]; m_co[2] += v[2]; m_co[3] += v[3];
    return *this;
}

GEN_INLINE MT_Vector4& MT_Vector4::operator-=(const MT_Vector4& v) {
    m_co[0] -= v[0]; m_co[1] -= v[1]; m_co[2] -= v[2]; m_co[3] -= v[3];
    return *this;
}
 
GEN_INLINE MT_Vector4& MT_Vector4::operator*=(MT_Scalar s) {
    m_co[0] *= s; m_co[1] *= s; m_co[2] *= s; m_co[3] *= s;
    return *this;
}

GEN_INLINE MT_Vector4& MT_Vector4::operator/=(MT_Scalar s) {
    MT_assert(!MT_fuzzyZero(s));
    return *this *= MT_Scalar(1.0) / s;
}

GEN_INLINE MT_Vector4 operator+(const MT_Vector4& v1, const MT_Vector4& v2) {
    return MT_Vector4(v1[0] + v2[0], v1[1] + v2[1], v1[2] + v2[2], v1[3] + v2[3]);
}

GEN_INLINE MT_Vector4 operator-(const MT_Vector4& v1, const MT_Vector4& v2) {
    return MT_Vector4(v1[0] - v2[0], v1[1] - v2[1], v1[2] - v2[2], v1[3] - v2[3]);
}

GEN_INLINE MT_Vector4 operator-(const MT_Vector4& v) {
    return MT_Vector4(-v[0], -v[1], -v[2], -v[3]);
}

GEN_INLINE MT_Vector4 operator*(const MT_Vector4& v, MT_Scalar s) {
    return MT_Vector4(v[0] * s, v[1] * s, v[2] * s, v[3] * s);
}

GEN_INLINE MT_Vector4 operator*(MT_Scalar s, const MT_Vector4& v) { return v * s; }

GEN_INLINE MT_Vector4 operator/(const MT_Vector4& v, MT_Scalar s) {
    MT_assert(!MT_fuzzyZero(s));
    return v * (MT_Scalar(1.0) / s);
}

GEN_INLINE MT_Scalar MT_Vector4::dot(const MT_Vector4& v) const {
    return m_co[0] * v[0] + m_co[1] * v[1] + m_co[2] * v[2] + m_co[3] * v[3];
}

GEN_INLINE MT_Scalar MT_Vector4::length2() const { return MT_dot(*this, *this); }
GEN_INLINE MT_Scalar MT_Vector4::length() const { return sqrt(length2()); }

GEN_INLINE MT_Vector4 MT_Vector4::absolute() const {
    return MT_Vector4(MT_abs(m_co[0]), MT_abs(m_co[1]), MT_abs(m_co[2]), MT_abs(m_co[3]));
}

GEN_INLINE void MT_Vector4::scale(MT_Scalar xx, MT_Scalar yy, MT_Scalar zz, MT_Scalar ww) {
    m_co[0] *= xx; m_co[1] *= yy; m_co[2] *= zz; m_co[3] *= ww;
}

GEN_INLINE MT_Vector4 MT_Vector4::scaled(MT_Scalar xx, MT_Scalar yy, MT_Scalar zz, MT_Scalar ww) const {
    return MT_Vector4(m_co[0] * xx, m_co[1] * yy, m_co[2] * zz, m_co[3] * ww);
}

GEN_INLINE bool MT_Vector4::fuzzyZero() const { return MT_fuzzyZero2(length2()); }

GEN_INLINE void MT_Vector4::normalize() { *this /= length(); }
GEN_INLINE MT_Vector4 MT_Vector4::normalized() const { return *this / length(); }

GEN_INLINE MT_Scalar  MT_dot(const MT_Vector4& v1, const MT_Vector4& v2) { 
    return v1.dot(v2);
}

GEN_INLINE MT_Scalar  MT_length2(const MT_Vector4& v) { return v.length2(); }
GEN_INLINE MT_Scalar  MT_length(const MT_Vector4& v) { return v.length(); }

GEN_INLINE bool MT_fuzzyZero(const MT_Vector4& v) { return v.fuzzyZero(); }
GEN_INLINE bool MT_fuzzyEqual(const MT_Vector4& v1, const MT_Vector4& v2) { 
    return MT_fuzzyZero(v1 - v2); 
}
