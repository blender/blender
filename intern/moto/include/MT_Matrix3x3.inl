#include "MT_Optimize.h"

GEN_INLINE MT_Quaternion MT_Matrix3x3::getRotation() const {
    static int 	next[3] = { 1, 2, 0 };

    MT_Quaternion result;
   
    MT_Scalar trace = m_el[0][0] + m_el[1][1] + m_el[2][2];
    
    if (trace > 0.0) 
    {
        MT_Scalar s = sqrt(trace + MT_Scalar(1.0));
        result[3] = s * MT_Scalar(0.5);
        s = MT_Scalar(0.5) / s;
        
        result[0] = (m_el[2][1] - m_el[1][2]) * s;
        result[1] = (m_el[0][2] - m_el[2][0]) * s;
        result[2] = (m_el[1][0] - m_el[0][1]) * s;
    } 
    else 
    {
        int i = 0;
        if (m_el[1][1] > m_el[0][0])
            i = 1;
        if (m_el[2][2] > m_el[i][i])
            i = 2;
        
        int j = next[i];  
        int k = next[j];
        
        MT_Scalar s = sqrt(m_el[i][i] - m_el[j][j] - m_el[k][k] + MT_Scalar(1.0));
        
        result[i] = s * MT_Scalar(0.5);
        
        s = MT_Scalar(0.5) / s;
        
        result[3] = (m_el[k][j] - m_el[j][k]) * s;
        result[j] = (m_el[j][i] + m_el[i][j]) * s;
        result[k] = (m_el[k][i] + m_el[i][k]) * s;
    }
    return result;
}

GEN_INLINE MT_Matrix3x3& MT_Matrix3x3::operator*=(const MT_Matrix3x3& m) {
    setValue(m.tdot(0, m_el[0]), m.tdot(1, m_el[0]), m.tdot(2, m_el[0]),
             m.tdot(0, m_el[1]), m.tdot(1, m_el[1]), m.tdot(2, m_el[1]),
             m.tdot(0, m_el[2]), m.tdot(1, m_el[2]), m.tdot(2, m_el[2]));
    return *this;
}

GEN_INLINE MT_Scalar MT_Matrix3x3::determinant() const { 
    return MT_triple((*this)[0], (*this)[1], (*this)[2]);
}

GEN_INLINE MT_Matrix3x3 MT_Matrix3x3::absolute() const {
    return 
        MT_Matrix3x3(MT_abs(m_el[0][0]), MT_abs(m_el[0][1]), MT_abs(m_el[0][2]),
                     MT_abs(m_el[1][0]), MT_abs(m_el[1][1]), MT_abs(m_el[1][2]),
                     MT_abs(m_el[2][0]), MT_abs(m_el[2][1]), MT_abs(m_el[2][2]));
}

GEN_INLINE MT_Matrix3x3 MT_Matrix3x3::transposed() const {
    return MT_Matrix3x3(m_el[0][0], m_el[1][0], m_el[2][0],
                        m_el[0][1], m_el[1][1], m_el[2][1],
                        m_el[0][2], m_el[1][2], m_el[2][2]);
}

GEN_INLINE void MT_Matrix3x3::transpose() {
	*this = transposed();
}

GEN_INLINE MT_Matrix3x3 MT_Matrix3x3::adjoint() const {
    return 
        MT_Matrix3x3(cofac(1, 1, 2, 2), cofac(0, 2, 2, 1), cofac(0, 1, 1, 2),
                     cofac(1, 2, 2, 0), cofac(0, 0, 2, 2), cofac(0, 2, 1, 0),
                     cofac(1, 0, 2, 1), cofac(0, 1, 2, 0), cofac(0, 0, 1, 1));
}

GEN_INLINE MT_Matrix3x3 MT_Matrix3x3::inverse() const {
    MT_Vector3 co(cofac(1, 1, 2, 2), cofac(1, 2, 2, 0), cofac(1, 0, 2, 1));
    MT_Scalar det = MT_dot((*this)[0], co);
    MT_assert(!MT_fuzzyZero2(det));
    MT_Scalar s = MT_Scalar(1.0) / det;
    return 
        MT_Matrix3x3(co[0] * s, cofac(0, 2, 2, 1) * s, cofac(0, 1, 1, 2) * s,
                     co[1] * s, cofac(0, 0, 2, 2) * s, cofac(0, 2, 1, 0) * s,
                     co[2] * s, cofac(0, 1, 2, 0) * s, cofac(0, 0, 1, 1) * s);
}

GEN_INLINE void MT_Matrix3x3::invert() {
	*this = inverse();
}

GEN_INLINE MT_Vector3 operator*(const MT_Matrix3x3& m, const MT_Vector3& v) {
    return MT_Vector3(MT_dot(m[0], v), MT_dot(m[1], v), MT_dot(m[2], v));
}

GEN_INLINE MT_Vector3 operator*(const MT_Vector3& v, const MT_Matrix3x3& m) {
    return MT_Vector3(m.tdot(0, v), m.tdot(1, v), m.tdot(2, v));
}

GEN_INLINE MT_Matrix3x3 operator*(const MT_Matrix3x3& m1, const MT_Matrix3x3& m2) {
    return 
        MT_Matrix3x3(m2.tdot(0, m1[0]), m2.tdot(1, m1[0]), m2.tdot(2, m1[0]),
                     m2.tdot(0, m1[1]), m2.tdot(1, m1[1]), m2.tdot(2, m1[1]),
                     m2.tdot(0, m1[2]), m2.tdot(1, m1[2]), m2.tdot(2, m1[2]));
}

GEN_INLINE MT_Matrix3x3 MT_multTransposeLeft(const MT_Matrix3x3& m1, const MT_Matrix3x3& m2) {
    return MT_Matrix3x3(
        m1[0][0] * m2[0][0] + m1[1][0] * m2[1][0] + m1[2][0] * m2[2][0],
        m1[0][0] * m2[0][1] + m1[1][0] * m2[1][1] + m1[2][0] * m2[2][1],
        m1[0][0] * m2[0][2] + m1[1][0] * m2[1][2] + m1[2][0] * m2[2][2],
        m1[0][1] * m2[0][0] + m1[1][1] * m2[1][0] + m1[2][1] * m2[2][0],
        m1[0][1] * m2[0][1] + m1[1][1] * m2[1][1] + m1[2][1] * m2[2][1],
        m1[0][1] * m2[0][2] + m1[1][1] * m2[1][2] + m1[2][1] * m2[2][2],
        m1[0][2] * m2[0][0] + m1[1][2] * m2[1][0] + m1[2][2] * m2[2][0],
        m1[0][2] * m2[0][1] + m1[1][2] * m2[1][1] + m1[2][2] * m2[2][1],
        m1[0][2] * m2[0][2] + m1[1][2] * m2[1][2] + m1[2][2] * m2[2][2]);
}

GEN_INLINE MT_Matrix3x3 MT_multTransposeRight(const MT_Matrix3x3& m1, const MT_Matrix3x3& m2) {
    return
        MT_Matrix3x3(m1[0].dot(m2[0]), m1[0].dot(m2[1]), m1[0].dot(m2[2]),
                     m1[1].dot(m2[0]), m1[1].dot(m2[1]), m1[1].dot(m2[2]),
                     m1[2].dot(m2[0]), m1[2].dot(m2[1]), m1[2].dot(m2[2]));
                     
}
