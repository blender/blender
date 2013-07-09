#include "MT_Optimize.h"

GEN_INLINE MT_Vector3& MT_Vector3::operator+=(const MT_Vector3& v) {
    m_co[0] += v[0]; m_co[1] += v[1]; m_co[2] += v[2];
    return *this;
}

GEN_INLINE MT_Vector3& MT_Vector3::operator-=(const MT_Vector3& v) {
    m_co[0] -= v[0]; m_co[1] -= v[1]; m_co[2] -= v[2];
    return *this;
}
 
GEN_INLINE MT_Vector3& MT_Vector3::operator*=(MT_Scalar s) {
    m_co[0] *= s; m_co[1] *= s; m_co[2] *= s;
    return *this;
}

GEN_INLINE MT_Vector3& MT_Vector3::operator/=(MT_Scalar s) {
    MT_assert(!MT_fuzzyZero(s));
    return *this *= MT_Scalar(1.0) / s;
}

GEN_INLINE MT_Vector3 operator+(const MT_Vector3& v1, const MT_Vector3& v2) {
    return MT_Vector3(v1[0] + v2[0], v1[1] + v2[1], v1[2] + v2[2]);
}

GEN_INLINE MT_Vector3 operator-(const MT_Vector3& v1, const MT_Vector3& v2) {
    return MT_Vector3(v1[0] - v2[0], v1[1] - v2[1], v1[2] - v2[2]);
}

GEN_INLINE MT_Vector3 operator-(const MT_Vector3& v) {
    return MT_Vector3(-v[0], -v[1], -v[2]);
}

GEN_INLINE MT_Vector3 operator*(const MT_Vector3& v, MT_Scalar s) {
    return MT_Vector3(v[0] * s, v[1] * s, v[2] * s);
}

GEN_INLINE MT_Vector3 operator*(MT_Scalar s, const MT_Vector3& v) { return v * s; }

GEN_INLINE MT_Vector3 operator/(const MT_Vector3& v, MT_Scalar s) {
    MT_assert(!MT_fuzzyZero(s));
    return v * (MT_Scalar(1.0) / s);
}

GEN_INLINE MT_Vector3 operator*(const MT_Vector3& v1, const MT_Vector3& v2) {
    return MT_Vector3(v1[0] * v2[0], v1[1] * v2[1], v1[2] * v2[2]);
}

GEN_INLINE MT_Scalar MT_Vector3::dot(const MT_Vector3& v) const {
    return m_co[0] * v[0] + m_co[1] * v[1] + m_co[2] * v[2];
}

GEN_INLINE MT_Scalar MT_Vector3::length2() const { return dot(*this); }
GEN_INLINE MT_Scalar MT_Vector3::length() const { return sqrt(length2()); }

GEN_INLINE MT_Vector3 MT_Vector3::absolute() const {
    return MT_Vector3(MT_abs(m_co[0]), MT_abs(m_co[1]), MT_abs(m_co[2]));
}

GEN_INLINE bool MT_Vector3::fuzzyZero() const {
	return MT_fuzzyZero(length2());
}

GEN_INLINE void MT_Vector3::noiseGate(MT_Scalar threshold) {
    if (length2() < threshold) {
        setValue(MT_Scalar(0.0), MT_Scalar(0.0), MT_Scalar(0.0));
    }
}

GEN_INLINE void MT_Vector3::normalize() { *this /= length(); }
GEN_INLINE MT_Vector3 MT_Vector3::normalized() const { return *this / length(); }
GEN_INLINE MT_Vector3 MT_Vector3::safe_normalized() const { 
	MT_Scalar len = length();
	return MT_fuzzyZero(len) ? 
        MT_Vector3(MT_Scalar(1.0), MT_Scalar(0.0), MT_Scalar(0.0)) : 
        *this / len; 
}

GEN_INLINE void MT_Vector3::scale(MT_Scalar xx, MT_Scalar yy, MT_Scalar zz) {
    m_co[0] *= xx; m_co[1] *= yy; m_co[2] *= zz;
}

GEN_INLINE MT_Vector3 MT_Vector3::scaled(MT_Scalar xx, MT_Scalar yy, MT_Scalar zz) const {
    return MT_Vector3(m_co[0] * xx, m_co[1] * yy, m_co[2] * zz);
}

GEN_INLINE MT_Scalar MT_Vector3::angle(const MT_Vector3& v) const {
    MT_Scalar s = sqrt(length2() * v.length2());
    MT_assert(!MT_fuzzyZero(s));
    return acos(dot(v) / s);
}

GEN_INLINE MT_Vector3 MT_Vector3::cross(const MT_Vector3& v) const {
    return MT_Vector3(m_co[1] * v[2] - m_co[2] * v[1],
                      m_co[2] * v[0] - m_co[0] * v[2],
                      m_co[0] * v[1] - m_co[1] * v[0]);
}

GEN_INLINE MT_Scalar MT_Vector3::triple(const MT_Vector3& v1, const MT_Vector3& v2) const {
    return m_co[0] * (v1[1] * v2[2] - v1[2] * v2[1]) + 
           m_co[1] * (v1[2] * v2[0] - v1[0] * v2[2]) + 
           m_co[2] * (v1[0] * v2[1] - v1[1] * v2[0]);
}

GEN_INLINE int MT_Vector3::closestAxis() const {
    MT_Vector3 a = absolute();
    return a[0] < a[1] ? (a[1] < a[2] ? 2 : 1) : (a[0] < a[2] ? 2 : 0);
}

GEN_INLINE MT_Vector3 MT_Vector3::random() {
    MT_Scalar z = MT_Scalar(2.0) * MT_random() - MT_Scalar(1.0);
    MT_Scalar r = sqrt(MT_Scalar(1.0) - z * z);
    MT_Scalar t = MT_2_PI * MT_random();
    return MT_Vector3(r * cos(t), r * sin(t), z);
}

GEN_INLINE MT_Scalar  MT_dot(const MT_Vector3& v1, const MT_Vector3& v2) { 
    return v1.dot(v2);
}

GEN_INLINE MT_Scalar  MT_length2(const MT_Vector3& v) { return v.length2(); }
GEN_INLINE MT_Scalar  MT_length(const MT_Vector3& v) { return v.length(); }

GEN_INLINE bool       MT_fuzzyZero(const MT_Vector3& v) { return v.fuzzyZero(); }
GEN_INLINE bool       MT_fuzzyEqual(const MT_Vector3& v1, const MT_Vector3& v2) { 
    return MT_fuzzyZero(v1 - v2); 
}

GEN_INLINE MT_Scalar  MT_angle(const MT_Vector3& v1, const MT_Vector3& v2) { return v1.angle(v2); }
GEN_INLINE MT_Vector3 MT_cross(const MT_Vector3& v1, const MT_Vector3& v2) { return v1.cross(v2); }
GEN_INLINE MT_Scalar  MT_triple(const MT_Vector3& v1, const MT_Vector3& v2, const MT_Vector3& v3) {
    return v1.triple(v2, v3);
}
