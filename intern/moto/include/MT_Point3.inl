#include "MT_Optimize.h"

GEN_INLINE MT_Point3& MT_Point3::operator+=(const MT_Vector3& v) {
    m_co[0] += v[0]; m_co[1] += v[1]; m_co[2] += v[2];
    return *this;
}

GEN_INLINE MT_Point3& MT_Point3::operator-=(const MT_Vector3& v) {
    m_co[0] -= v[0]; m_co[1] -= v[1]; m_co[2] -= v[2];
    return *this;
}

GEN_INLINE MT_Point3& MT_Point3::operator=(const MT_Vector3& v) {
    m_co[0] = v[0]; m_co[1] = v[1]; m_co[2] = v[2];
    return *this;
}

GEN_INLINE MT_Point3& MT_Point3::operator=(const MT_Point3& v) {
    m_co[0] = v[0]; m_co[1] = v[1]; m_co[2] = v[2];
    return *this;
}

GEN_INLINE MT_Scalar MT_Point3::distance(const MT_Point3& p) const {
    return (p - *this).length();
}

GEN_INLINE MT_Scalar MT_Point3::distance2(const MT_Point3& p) const {
    return (p - *this).length2();
}

GEN_INLINE MT_Point3 MT_Point3::lerp(const MT_Point3& p, MT_Scalar t) const {
    return MT_Point3(m_co[0] + (p[0] - m_co[0]) * t,
                     m_co[1] + (p[1] - m_co[1]) * t,
                     m_co[2] + (p[2] - m_co[2]) * t);
}

GEN_INLINE MT_Point3  operator+(const MT_Point3& p, const MT_Vector3& v) {
    return MT_Point3(p[0] + v[0], p[1] + v[1], p[2] + v[2]);
}

GEN_INLINE MT_Point3  operator-(const MT_Point3& p, const MT_Vector3& v) {
    return MT_Point3(p[0] - v[0], p[1] - v[1], p[2] - v[2]);
}

GEN_INLINE MT_Vector3 operator-(const MT_Point3& p1, const MT_Point3& p2) {
    return MT_Vector3(p1[0] - p2[0], p1[1] - p2[1], p1[2] - p2[2]);
}

GEN_INLINE MT_Scalar MT_distance(const MT_Point3& p1, const MT_Point3& p2) { 
    return p1.distance(p2); 
}

GEN_INLINE MT_Scalar MT_distance2(const MT_Point3& p1, const MT_Point3& p2) { 
    return p1.distance2(p2); 
}

GEN_INLINE MT_Point3 MT_lerp(const MT_Point3& p1, const MT_Point3& p2, MT_Scalar t) {
    return p1.lerp(p2, t);
}
