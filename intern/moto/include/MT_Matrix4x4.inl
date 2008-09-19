#include "MT_Optimize.h"

/*
 * This is a supposedly faster inverter than the cofactor
 * computation. It uses an LU decomposition sort of thing.  */
GEN_INLINE void MT_Matrix4x4::invert()  {
	/* normalize row 0 */

	int i,j,k;

	for (i=1; i < 4; i++) m_el[0][i] /= m_el[0][0];
	for (i=1; i < 4; i++)  { 
		for (j=i; j < 4; j++)  { // do a column of L
			MT_Scalar sum = 0.0;
			for (k = 0; k < i; k++)  
				sum += m_el[j][k] * m_el[k][i];
			m_el[j][i] -= sum;
		}
		if (i == 3) continue;
		for (j=i+1; j < 4; j++)  {  // do a row of U
			MT_Scalar sum = 0.0;
			for (k = 0; k < i; k++)
				sum += m_el[i][k]*m_el[k][j];
			m_el[i][j] = 
				(m_el[i][j]-sum) / m_el[i][i];
		}
	}
	for (i = 0; i < 4; i++ )  // invert L
		for (j = i; j < 4; j++ )  {
			MT_Scalar x = 1.0;
			if ( i != j ) {
				x = 0.0;
				for (k = i; k < j; k++ ) 
					x -= m_el[j][k]*m_el[k][i];
			}
			m_el[j][i] = x / m_el[j][j];
		}
	for (i = 0; i < 4; i++ )   // invert U
		for (j = i; j < 4; j++ )  {
			if ( i == j ) continue;
			MT_Scalar sum = 0.0;
			for (k = i; k < j; k++ )
				sum += m_el[k][j]*( (i==k) ? 1.0 : m_el[i][k] );
			m_el[i][j] = -sum;
		}
	for (i = 0; i < 4; i++ )   // final inversion
		for (j = 0; j < 4; j++ )  {
			MT_Scalar sum = 0.0;
			for (k = ((i>j)?i:j); k < 4; k++ )  
				sum += ((j==k)?1.0:m_el[j][k])*m_el[k][i];
			m_el[j][i] = sum;
		}
}

GEN_INLINE MT_Matrix4x4 MT_Matrix4x4::inverse() const
{
	MT_Matrix4x4 invmat = *this;

	invmat.invert();

	return invmat;
}

GEN_INLINE MT_Matrix4x4& MT_Matrix4x4::operator*=(const MT_Matrix4x4& m)
{
	setValue(m.tdot(0, m_el[0]), m.tdot(1, m_el[0]), m.tdot(2, m_el[0]), m.tdot(3, m_el[0]),
             m.tdot(0, m_el[1]), m.tdot(1, m_el[1]), m.tdot(2, m_el[1]), m.tdot(3, m_el[1]),
             m.tdot(0, m_el[2]), m.tdot(1, m_el[2]), m.tdot(2, m_el[2]), m.tdot(3, m_el[2]),
             m.tdot(0, m_el[3]), m.tdot(1, m_el[3]), m.tdot(2, m_el[3]), m.tdot(3, m_el[3]));
    return *this;

}

GEN_INLINE MT_Vector4 operator*(const MT_Matrix4x4& m, const MT_Vector4& v) {
    return MT_Vector4(MT_dot(m[0], v), MT_dot(m[1], v), MT_dot(m[2], v), MT_dot(m[3], v));
}

GEN_INLINE MT_Vector4 operator*(const MT_Vector4& v, const MT_Matrix4x4& m) {
    return MT_Vector4(m.tdot(0, v), m.tdot(1, v), m.tdot(2, v), m.tdot(3, v));
}

GEN_INLINE MT_Matrix4x4 operator*(const MT_Matrix4x4& m1, const MT_Matrix4x4& m2) {
	return 
		MT_Matrix4x4(m2.tdot(0, m1[0]), m2.tdot(1, m1[0]), m2.tdot(2, m1[0]), m2.tdot(3, m1[0]),
                     m2.tdot(0, m1[1]), m2.tdot(1, m1[1]), m2.tdot(2, m1[1]), m2.tdot(3, m1[1]),
                     m2.tdot(0, m1[2]), m2.tdot(1, m1[2]), m2.tdot(2, m1[2]), m2.tdot(3, m1[2]),
                     m2.tdot(0, m1[3]), m2.tdot(1, m1[3]), m2.tdot(2, m1[3]), m2.tdot(3, m1[3]));
}


GEN_INLINE MT_Matrix4x4 MT_Matrix4x4::transposed() const {
    return MT_Matrix4x4(m_el[0][0], m_el[1][0], m_el[2][0], m_el[3][0],
                        m_el[0][1], m_el[1][1], m_el[2][1], m_el[3][1],
                        m_el[0][2], m_el[1][2], m_el[2][2], m_el[3][2],
                        m_el[0][3], m_el[1][3], m_el[2][3], m_el[3][3]);
}

GEN_INLINE void MT_Matrix4x4::transpose() {
	*this = transposed();
}

GEN_INLINE MT_Matrix4x4 MT_Matrix4x4::absolute() const {
    return 
        MT_Matrix4x4(MT_abs(m_el[0][0]), MT_abs(m_el[0][1]), MT_abs(m_el[0][2]), MT_abs(m_el[0][3]),
                     MT_abs(m_el[1][0]), MT_abs(m_el[1][1]), MT_abs(m_el[1][2]), MT_abs(m_el[1][3]),
                     MT_abs(m_el[2][0]), MT_abs(m_el[2][1]), MT_abs(m_el[2][2]), MT_abs(m_el[2][3]),
                     MT_abs(m_el[3][0]), MT_abs(m_el[3][1]), MT_abs(m_el[3][2]), MT_abs(m_el[3][3]));
}
