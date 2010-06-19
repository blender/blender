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

/**

 * $Id$
 * Copyright (C) 2001 NaN Technologies B.V.
 * A 4x4 matrix compatible with other stuff.
 */

#ifndef MT_MATRIX4X4_H
#define MT_MATRIX4X4_H

#include <MT_assert.h>

#include "MT_Vector4.h"
#include "MT_Transform.h"

// Row-major 4x4 matrix

class MT_Matrix4x4 {
public:
	/**
	 * Empty contructor.
	 */
    MT_Matrix4x4() {}
	/**
	 * Initialize all fields with the values pointed at by m. A
	 * contigous block of 16 values is read.  */
    MT_Matrix4x4(const float *m) { setValue(m); }
	/**
	 * Initialize all fields with the values pointed at by m. A
	 * contigous block of 16 values is read.  */
    MT_Matrix4x4(const double *m) { setValue(m); }
    
	/**
	 * Initialise with these 16 explicit values.
	 */
    MT_Matrix4x4(MT_Scalar xx, MT_Scalar xy, MT_Scalar xz, MT_Scalar xw,
                 MT_Scalar yx, MT_Scalar yy, MT_Scalar yz, MT_Scalar yw,
                 MT_Scalar zx, MT_Scalar zy, MT_Scalar zz, MT_Scalar zw,
                 MT_Scalar wx, MT_Scalar wy, MT_Scalar wz, MT_Scalar ww) { 
        setValue(xx, xy, xz, xw, 
                 yx, yy, yz, yw,
                 zx, zy, zz, zw,
				 wx, wy, wz, ww);
    }
	
	/** 
	 * Initialize from an MT_Transform.
	 */ 
	MT_Matrix4x4(const MT_Transform &t) {

		const MT_Matrix3x3 &basis = t.getBasis();
		const MT_Vector3 &origin = t.getOrigin();  	

		setValue(
			basis[0][0],basis[0][1],basis[0][2],origin[0],
			basis[1][0],basis[1][1],basis[1][2],origin[1],
			basis[2][0],basis[2][1],basis[2][2],origin[2],
			MT_Scalar(0),MT_Scalar(0),MT_Scalar(0),MT_Scalar(1)
		);
	}
		
	/**
	 * Get the i-th row.
	 */
    MT_Vector4&       operator[](int i)       { return m_el[i]; }
	/**
	 * Get the i-th row.
	 */
    const MT_Vector4& operator[](int i) const { return m_el[i]; }

    /**
	 * Set the matrix to the values pointer at by m. A contiguous
	 * block of 16 values is copied.  */
    void setValue(const float *m) {
        m_el[0][0] = *m++; m_el[1][0] = *m++; m_el[2][0] = *m++; m_el[3][0] = *m++;
        m_el[0][1] = *m++; m_el[1][1] = *m++; m_el[2][1] = *m++; m_el[3][1] = *m++;
        m_el[0][2] = *m++; m_el[1][2] = *m++; m_el[2][2] = *m++; m_el[3][2] = *m++;
        m_el[0][3] = *m++; m_el[1][3] = *m++; m_el[2][3] = *m++; m_el[3][3] = *m;
    }

    /**
	 * Set the matrix to the values pointer at by m. A contiguous
	 * block of 16 values is copied.
	 */
    void setValue(const double *m) {
        m_el[0][0] = *m++; m_el[1][0] = *m++; m_el[2][0] = *m++; m_el[3][0] = *m++;
        m_el[0][1] = *m++; m_el[1][1] = *m++; m_el[2][1] = *m++; m_el[3][1] = *m++;
        m_el[0][2] = *m++; m_el[1][2] = *m++; m_el[2][2] = *m++; m_el[3][2] = *m++;
        m_el[0][3] = *m++; m_el[1][3] = *m++; m_el[2][3] = *m++; m_el[3][3] = *m;
    }

    /**
	 * Set the matrix to these 16 explicit values.
	 */
    void setValue(MT_Scalar xx, MT_Scalar xy, MT_Scalar xz, MT_Scalar xw,
                  MT_Scalar yx, MT_Scalar yy, MT_Scalar yz, MT_Scalar yw,
                  MT_Scalar zx, MT_Scalar zy, MT_Scalar zz, MT_Scalar zw,
                  MT_Scalar wx, MT_Scalar wy, MT_Scalar wz, MT_Scalar ww) {
        m_el[0][0] = xx; m_el[0][1] = xy; m_el[0][2] = xz; m_el[0][3] = xw;
        m_el[1][0] = yx; m_el[1][1] = yy; m_el[1][2] = yz; m_el[1][3] = yw;
        m_el[2][0] = zx; m_el[2][1] = zy; m_el[2][2] = zz; m_el[2][3] = zw;
        m_el[3][0] = wx; m_el[3][1] = wy; m_el[3][2] = wz; m_el[3][3] = ww;
    }
	
	/**
	 * Scale the columns of this matrix with x, y, z, w respectively. 
	 */
    void scale(MT_Scalar x, MT_Scalar y, MT_Scalar z, MT_Scalar w) {
        m_el[0][0] *= x; m_el[0][1] *= y; m_el[0][2] *= z; m_el[0][3] *= w;
        m_el[1][0] *= x; m_el[1][1] *= y; m_el[1][2] *= z; m_el[1][3] *= w;
        m_el[2][0] *= x; m_el[2][1] *= y; m_el[2][2] *= z; m_el[2][3] *= w;
        m_el[3][0] *= x; m_el[3][1] *= y; m_el[3][2] *= z; m_el[3][3] *= w;
    }

	/**
	 * Return a column-scaled version of this matrix.
	 */
    MT_Matrix4x4 scaled(MT_Scalar x, MT_Scalar y, MT_Scalar z, MT_Scalar w) const {
        return MT_Matrix4x4(m_el[0][0] * x, m_el[0][1] * y, m_el[0][2] * z, m_el[0][3] * w,
                            m_el[1][0] * x, m_el[1][1] * y, m_el[1][2] * z, m_el[1][3] * w,
                            m_el[2][0] * x, m_el[2][1] * y, m_el[2][2] * z, m_el[2][3] * w,
                            m_el[3][0] * x, m_el[3][1] * y, m_el[3][2] * z, m_el[3][3] * w);
    }

	/**
	 * Set this matrix to I.
	 */
    void setIdentity() { 
        setValue(MT_Scalar(1.0), MT_Scalar(0.0), MT_Scalar(0.0), MT_Scalar(0.0),
                 MT_Scalar(0.0), MT_Scalar(1.0), MT_Scalar(0.0), MT_Scalar(0.0),
                 MT_Scalar(0.0), MT_Scalar(0.0), MT_Scalar(1.0), MT_Scalar(0.0),
			     MT_Scalar(0.0), MT_Scalar(0.0), MT_Scalar(0.0), MT_Scalar(1.0)); 
    }

	/**
	 * Read the element from row i, column j.
	 */
	float getElement(int i, int j) {
		return (float) m_el[i][j];
	}
	
    /**
	 * Copy the contents to a contiguous block of 16 floats.
	 */
    void getValue(float *m) const {
        *m++ = (float) m_el[0][0]; *m++ = (float) m_el[1][0]; *m++ = (float) m_el[2][0]; *m++ = (float) m_el[3][0];
        *m++ = (float) m_el[0][1]; *m++ = (float) m_el[1][1]; *m++ = (float) m_el[2][1]; *m++ = (float) m_el[3][1];
        *m++ = (float) m_el[0][2]; *m++ = (float) m_el[1][2]; *m++ = (float) m_el[2][2]; *m++ = (float) m_el[3][2];
        *m++ = (float) m_el[0][3]; *m++ = (float) m_el[1][3]; *m++ = (float) m_el[2][3]; *m = (float) m_el[3][3];
    }

    /**
	 * Copy the contents to a contiguous block of 16 doubles.
	 */
    void getValue(double *m) const {
        *m++ = m_el[0][0]; *m++ = m_el[1][0]; *m++ = m_el[2][0]; *m++ = m_el[3][0];
        *m++ = m_el[0][1]; *m++ = m_el[1][1]; *m++ = m_el[2][1]; *m++ = m_el[3][1];
        *m++ = m_el[0][2]; *m++ = m_el[1][2]; *m++ = m_el[2][2]; *m++ = m_el[3][2];
        *m++ = m_el[0][3]; *m++ = m_el[1][3]; *m++ = m_el[2][3]; *m = m_el[3][3];
    }

	/** 
	 * Left-multiply this matrix with the argument.
	 */
    MT_Matrix4x4& operator*=(const MT_Matrix4x4& m); 

	/**
	 * Left-multiply column c with row vector c.
	 */
    MT_Scalar tdot(int c, const MT_Vector4& v) const {
        return m_el[0][c] * v[0]
			+ m_el[1][c] * v[1]
			+ m_el[2][c] * v[2]
			+ m_el[3][c] * v[3];
    }

	/* I'll postpone this for now... - nzc*/ 
/*      MT_Scalar    determinant() const; */
/*  	MT_Matrix4x4 adjoint() const; */
/*      MT_Matrix4x4 inverse() const;  */

	MT_Matrix4x4 absolute() const;

	MT_Matrix4x4 transposed() const; 
	void         transpose();

	MT_Matrix4x4 inverse() const;
	void         invert();
  
protected:
	/**
	 * Access with [row index][column index]
	 */
    MT_Vector4 m_el[4];
};

/* These multiplicators do exactly what you ask from them: they
 * multiply in the indicated order. */
MT_Vector4   operator*(const MT_Matrix4x4& m, const MT_Vector4& v);
MT_Vector4   operator*(const MT_Vector4& v, const MT_Matrix4x4& m);
MT_Matrix4x4 operator*(const MT_Matrix4x4& m1, const MT_Matrix4x4& m2);

/*  MT_Matrix4x4 MT_multTransposeLeft(const MT_Matrix4x4& m1, const MT_Matrix4x4& m2); */
/*  MT_Matrix4x4 MT_multTransposeRight(const MT_Matrix4x4& m1, const MT_Matrix4x4& m2); */

inline MT_OStream& operator<<(MT_OStream& os, const MT_Matrix4x4& m) {
    return os << m[0] << GEN_endl
			  << m[1] << GEN_endl
			  << m[2] << GEN_endl
			  << m[3] << GEN_endl;


	
}

#ifdef GEN_INLINED
#include "MT_Matrix4x4.inl"
#endif

#endif

