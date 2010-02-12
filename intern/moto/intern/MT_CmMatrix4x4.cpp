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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "MT_CmMatrix4x4.h"
#include "MT_Vector3.h"
#include "MT_Point3.h"


MT_CmMatrix4x4::MT_CmMatrix4x4()
{
	Identity();
}



MT_CmMatrix4x4::MT_CmMatrix4x4(const float value[4][4])
{
	for (int i=0;i<4;i++)
	{
		for (int j=0;j<4;j++)
			m_V[i][j] = value[i][j];
	}
}



MT_CmMatrix4x4::MT_CmMatrix4x4(const double value[16])
{
	for (int i=0;i<16;i++)
		m_Vflat[i] = value[i];
}



MT_CmMatrix4x4::MT_CmMatrix4x4(const MT_CmMatrix4x4& other)
{
	SetMatrix(other);
}



MT_CmMatrix4x4::MT_CmMatrix4x4(const MT_Point3& orig,
							 const MT_Vector3& dir,
							 const MT_Vector3 up)
{
	MT_Vector3 z = -(dir.normalized());
	MT_Vector3 x = (up.cross(z)).normalized();
	MT_Vector3 y = (z.cross(x));
	
	m_V[0][0] = x.x();
	m_V[0][1] = y.x();
	m_V[0][2] = z.x();
	m_V[0][3] = 0.0f;
	
	m_V[1][0] = x.y();
	m_V[1][1] = y.y();
	m_V[1][2] = z.y();
	m_V[1][3] = 0.0f;
	
	m_V[2][0] = x.z();
	m_V[2][1] = y.z();
	m_V[2][2] = z.z();
	m_V[2][3] = 0.0f;
	
	m_V[3][0] = orig.x();//0.0f;
	m_V[3][1] = orig.y();//0.0f;
	m_V[3][2] = orig.z();//0.0f;
	m_V[3][3] = 1.0f;
	
	//Translate(-orig);
}



MT_Vector3 MT_CmMatrix4x4::GetRight() const
{
	return MT_Vector3(m_V[0][0], m_V[0][1], m_V[0][2]);
}



MT_Vector3 MT_CmMatrix4x4::GetUp() const
{
	return MT_Vector3(m_V[1][0], m_V[1][1], m_V[1][2]);
}



MT_Vector3 MT_CmMatrix4x4::GetDir() const
{
	return MT_Vector3(m_V[2][0], m_V[2][1], m_V[2][2]);
}



MT_Point3 MT_CmMatrix4x4::GetPos() const
{
	return MT_Point3(m_V[3][0], m_V[3][1], m_V[3][2]);
}



void MT_CmMatrix4x4::Identity()
{
	for (int i=0; i<4; i++)
	{
		for (int j=0; j<4; j++)
			m_V[i][j] = (i==j?1.0f:0.0f);
	} 
}



void MT_CmMatrix4x4::SetMatrix(const MT_CmMatrix4x4& other)
{
	for (int i=0; i<16; i++)
		m_Vflat[i] = other.m_Vflat[i];
}



double*	MT_CmMatrix4x4::getPointer()
{
	return &m_V[0][0];
}



const double* MT_CmMatrix4x4::getPointer() const
{
	return &m_V[0][0];
}	



void MT_CmMatrix4x4::setElem(int pos,double newvalue)
{
	m_Vflat[pos] = newvalue;
}	

MT_CmMatrix4x4 MT_CmMatrix4x4::Perspective(
	MT_Scalar inLeft,
	MT_Scalar inRight,
	MT_Scalar inBottom,
	MT_Scalar inTop,
	MT_Scalar inNear,
	MT_Scalar inFar
){

	MT_CmMatrix4x4 mat;
	
	// Column 0
	mat(0, 0) = -(2.0*inNear)			/ (inRight-inLeft);
	mat(1, 0) = 0;
	mat(2, 0) = 0;
	mat(3, 0) = 0;

	// Column 1
	mat(0, 1) = 0;
	mat(1, 1) = (2.0*inNear)			/ (inTop-inBottom);
	mat(2, 1) = 0;
	mat(3, 1) = 0;

	// Column 2
	mat(0, 2) =  (inRight+inLeft)		/ (inRight-inLeft);
	mat(1, 2) =  (inTop+inBottom)		/ (inTop-inBottom);
	mat(2, 2) = -(inFar+inNear)			/ (inFar-inNear);
	mat(3, 2) = -1;

	// Column 3
	mat(0, 3) = 0;
	mat(1, 3) = 0;
	mat(2, 3) = -(2.0*inFar*inNear)		/ (inFar-inNear);
	mat(3, 3) = 0;

	return mat;
}
