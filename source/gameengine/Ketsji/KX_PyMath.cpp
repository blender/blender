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
 * Initialize Python thingies.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef WIN32
#pragma warning (disable : 4786)
#endif //WIN32

#include "MT_Vector3.h"
#include "MT_Vector4.h"
#include "MT_Matrix4x4.h"
#include "MT_Point2.h"

#include "ListValue.h"

#include "KX_Python.h"

bool PyObject_IsMT_Matrix(PyObject *pymat, unsigned int rank)
{
	if (!pymat)
		return false;
		
	unsigned int y;
	if (PySequence_Check(pymat))
	{
		unsigned int rows = PySequence_Size(pymat);
		if (rows != rank)
			return false;
		
		bool ismatrix = true;
		for (y = 0; y < rank && ismatrix; y++)
		{
			PyObject *pyrow = PySequence_GetItem(pymat, y); /* new ref */
			if (PySequence_Check(pyrow))
			{
				if (((unsigned int)PySequence_Size(pyrow)) != rank)
					ismatrix = false;
			} else 
				ismatrix = false;
			Py_DECREF(pyrow);
		}
		return ismatrix;
	}
	return false;
}


PyObject* PyObjectFrom(const MT_Matrix4x4 &mat)
{
	return Py_BuildValue("[[ffff][ffff][ffff][ffff]]",
		mat[0][0], mat[0][1], mat[0][2], mat[0][3], 
		mat[1][0], mat[1][1], mat[1][2], mat[1][3], 
		mat[2][0], mat[2][1], mat[2][2], mat[2][3], 
		mat[3][0], mat[3][1], mat[3][2], mat[3][3]);
}

PyObject* PyObjectFrom(const MT_Matrix3x3 &mat)
{
	return Py_BuildValue("[[fff][fff][fff]]",
		mat[0][0], mat[0][1], mat[0][2], 
		mat[1][0], mat[1][1], mat[1][2], 
		mat[2][0], mat[2][1], mat[2][2]);
}

PyObject* PyObjectFrom(const MT_Tuple4 &vec)
{
	return Py_BuildValue("[ffff]", 
		vec[0], vec[1], vec[2], vec[3]);
}

PyObject* PyObjectFrom(const MT_Tuple3 &vec)
{
	return Py_BuildValue("[fff]", 
		vec[0], vec[1], vec[2]);
}

PyObject* PyObjectFrom(const MT_Tuple2 &vec)
{
	return Py_BuildValue("[ff]",
		vec[0], vec[1]);
}
