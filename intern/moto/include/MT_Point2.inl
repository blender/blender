#include "MT_Optimize.h"

GEN_INLINE MT_Point2& MT_Point2::operator+=(const MT_Vector2& v) {
    m_co[0] += v[0]; m_co[1] += v[1]; 
    return *this;
}

GEN_INLINE MT_Point2& MT_Point2::operator-=(const MT_Vector2& v) {
    m_co[0] -= v[0]; m_co[1] -= v[1]; 
    return *this;
}

GEN_INLINE MT_Point2& MT_Point2::operator=(const MT_Vector2& v) {
    m_co[0] = v[0]; m_co[1] = v[1]; 
    return *this;
}

GEN_INLINE MT_Scalar MT_Point2::distance(const MT_Point2& p) const {
    return (p - *this).length();
}

GEN_INLINE MT_Scalar MT_Point2::distance2(const MT_Point2& p) const {
    return (p - *this).length2();
}

GEN_INLINE MT_Point2 MT_Point2::lerp(const MT_Point2& p, MT_Scalar t) const {
    return MT_Point2(m_co[0] + (p[0] - m_co[0]) * t,
                     m_co[1] + (p[1] - m_co[1]) * t);
}

GEN_INLINE MT_Point2  operator+(const MT_Point2& p, const MT_Vector2& v) {
    return MT_Point2(p[0] + v[0], p[1] + v[1]);
}

GEN_INLINE MT_Point2  operator-(const MT_Point2& p, const MT_Vector2& v) {
    return MT_Point2(p[0] - v[0], p[1] - v[1]);
}

GEN_INLINE MT_Vector2 operator-(const MT_Point2& p1, const MT_Point2& p2) {
    return MT_Vector2(p1[0] - p2[0], p1[1] - p2[1]);
}

GEN_INLINE MT_Scalar MT_distance(const MT_Point2& p1, const MT_Point2& p2) { 
    return p1.distance(p2); 
}

GEN_INLINE MT_Scalar MT_distance2(const MT_Point2& p1, const MT_Point2& p2) { 
    return p1.distance2(p2); 
}

GEN_INLINE MT_Point2 MT_lerp(const MT_Point2& p1, const MT_Point2& p2, MT_Scalar t) {
    return p1.lerp(p2, t);
}

