/**
 * $Id$
 *
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
#ifndef GEN_MATRIX4X4
#define GEN_MATRIX4X4

#include "MT_Point3.h"

class GEN_Matrix4x4
{
public:
	// creators.
	GEN_Matrix4x4();
	GEN_Matrix4x4(const float value[4][4]);
	GEN_Matrix4x4(const double value[16]);
	GEN_Matrix4x4(const GEN_Matrix4x4 & other);
	GEN_Matrix4x4(const MT_Point3& orig,
				  const MT_Vector3& dir,
				  const MT_Vector3 up);

	void Identity();
	void SetMatrix(const GEN_Matrix4x4 & other);
	double*	getPointer();
	const double* getPointer() const;
	void setElem(int pos,double newvalue);

	
	MT_Vector3	GetRight() const;
	MT_Vector3	GetUp() const;
	MT_Vector3	GetDir() const;
	MT_Point3	GetPos() const;
	void		SetPos(const MT_Vector3 & v);

	double& operator () (int row,int col)	{ return m_V[col][row]; }
	
	static GEN_Matrix4x4 Perspective(MT_Scalar inLeft,
									 MT_Scalar inRight,
									 MT_Scalar inBottom,
									 MT_Scalar inTop,
									 MT_Scalar inNear,
									 MT_Scalar inFar);
protected:
	union
	{
		double m_V[4][4];
		double m_Vflat[16];
	};
};

#endif //GEN_MATRIX4X4

