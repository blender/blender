/**
 * $Id$
 *
 * ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.
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
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
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

#include "ListValue.h"

#include "KX_Python.h"

MT_Vector3 MT_Vector3FromPyList(PyObject* pylist)
{
	MT_Vector3 vec(0., 0., 0.);
	bool error=false;
	if (pylist->ob_type == &CListValue::Type)
	{
		CListValue* listval = (CListValue*) pylist;
		unsigned int numitems = listval->GetCount();
		if (numitems <= 3)
		{
			for (unsigned int index=0;index<numitems;index++)
			{
				vec[index] = listval->GetValue(index)->GetNumber();
			}
		}	else
		{
			error = true;
		}
		
	} else
	{
		// assert the list is long enough...
		unsigned int numitems = PySequence_Size(pylist);
		if (numitems <= 3)
		{
			for (unsigned int index=0;index<numitems;index++)
			{
				PyObject *item = PySequence_GetItem(pylist,index); /* new ref */
				vec[index] = PyFloat_AsDouble(item);
				Py_DECREF(item);
			}
		}
		else
		{
			error = true;
		}

	}
	if (error)
		PyErr_SetString(PyExc_TypeError, "Expected list of three items for vector argument.");

	return vec;
}

MT_Point3 MT_Point3FromPyList(PyObject* pylist)
{
	MT_Point3 point(0., 0., 0.);
	bool error=false;
	if (pylist->ob_type == &CListValue::Type)
	{
		CListValue* listval = (CListValue*) pylist;
		unsigned int numitems = listval->GetCount();
		if (numitems <= 3)
		{
			for (unsigned int index=0;index<numitems;index++)
			{
				point[index] = listval->GetValue(index)->GetNumber();
			}
		}	else
		{
			error = true;
		}
		
	} else
	{
		// assert the list is long enough...
		unsigned int numitems = PySequence_Size(pylist);
		if (numitems <= 3)
		{
			for (unsigned int index=0;index<numitems;index++)
			{
				PyObject *item = PySequence_GetItem(pylist,index); /* new ref */
				point[index] = PyFloat_AsDouble(item);
				Py_DECREF(item);
			}
		}
		else
		{
			error = true;
		}

	}
	if (error)
		PyErr_SetString(PyExc_TypeError, "Expected list of three items for point argument.");

	return point;
}

MT_Vector4 MT_Vector4FromPyList(PyObject* pylist)
{
	MT_Vector4 vec(0., 0., 0., 1.);
	bool error=false;
	if (pylist->ob_type == &CListValue::Type)
	{
		CListValue* listval = (CListValue*) pylist;
		unsigned int numitems = listval->GetCount();
		if (numitems <= 4)
		{
			for (unsigned index=0;index<numitems;index++)
			{
				vec[index] = listval->GetValue(index)->GetNumber();
			}
		} else
		{
			error = true;
		}
		
	} else
	{
		// assert the list is long enough...
		unsigned int numitems = PySequence_Size(pylist);
		if (numitems <= 4)
		{
			for (unsigned index=0;index<numitems;index++)
			{
				PyObject *item = PySequence_GetItem(pylist,index); /* new ref */
				vec[index] = PyFloat_AsDouble(item);
				Py_DECREF(item);
			}
		}
		else
		{
			error = true;
		}
	}
	if (error)
		PyErr_SetString(PyExc_TypeError, "Expected list of four items for Vector argument.");
	return vec;
}

MT_Quaternion MT_QuaternionFromPyList(PyObject* pylist)
{
	MT_Quaternion vec(0., 0., 0., 1.);
	bool error=false;
	if (pylist->ob_type == &CListValue::Type)
	{
		CListValue* listval = (CListValue*) pylist;
		unsigned int numitems = listval->GetCount();
		if (numitems <= 4)
		{
			for (unsigned index=0;index<numitems;index++)
			{
				vec[index] = listval->GetValue(index)->GetNumber();
			}
		} else
		{
			error = true;
		}
		
	} else
	{
		// assert the list is long enough...
		unsigned int numitems = PySequence_Size(pylist);
		if (numitems <= 4)
		{
			for (unsigned index=0;index<numitems;index++)
			{
				PyObject *item = PySequence_GetItem(pylist,index); /* new ref */
				vec[index] = PyFloat_AsDouble(item);
				Py_DECREF(item);
			}
		}
		else
		{
			error = true;
		}
	}
	if (error)
		PyErr_SetString(PyExc_TypeError, "Expected list of four items for Quaternion argument.");
	return vec;
}


MT_Matrix4x4 MT_Matrix4x4FromPyObject(PyObject *pymat)
{
	MT_Matrix4x4 mat;
	bool error = false;
	mat.setIdentity();
	if (PySequence_Check(pymat))
	{
		unsigned int rows = PySequence_Size(pymat);
		for (unsigned int y = 0; y < rows && y < 4; y++)
		{
			PyObject *pyrow = PySequence_GetItem(pymat, y); /* new ref */
			if (PySequence_Check(pyrow))
			{
				unsigned int cols = PySequence_Size(pyrow);
				for( unsigned int x = 0; x < cols && x < 4; x++)
				{
					PyObject *item = PySequence_GetItem(pyrow, x); /* new ref */
					mat[y][x] = PyFloat_AsDouble(item);
					Py_DECREF(item);
				}
			}
			Py_DECREF(pyrow);
		}
	}
	 
	return mat;
}

bool PyObject_IsMT_Matrix(PyObject *pymat, unsigned int rank)
{
	if (!pymat)
		return false;
		
	unsigned int x, y;
	if (pymat->ob_type == &CListValue::Type)
	{
		CListValue* listval = (CListValue*) pymat;
		if (listval->GetCount() == rank)
		{
			for (y = 0; y < rank; y++)
			{
				CListValue* vecval = (CListValue*)listval->GetValue(y);
				if (vecval->GetCount() != rank)
					return false;
			}
			return true;
		}
		return false;
	} else if (PySequence_Check(pymat))
	{
		unsigned int rows = PySequence_Size(pymat);
		if (rows != rank)
			return false;
		
		for (y = 0; y < rank; y++)
		{
			PyObject *pyrow = PySequence_GetItem(pymat, y); /* new ref */
			if (PySequence_Check(pyrow))
			{
				if (PySequence_Size(pyrow) != rank)
				{
					Py_DECREF(pyrow);
					return false;
				}
			}
			Py_DECREF(pyrow);
		}
		return true;
	}
	return false;
}

MT_Matrix3x3 MT_Matrix3x3FromPyObject(PyObject *pymat)
{
	MT_Matrix3x3 mat;
	bool error = false;
	mat.setIdentity();
	if (pymat->ob_type == &CListValue::Type)
	{
		unsigned int row, col;
		CListValue* listval = (CListValue*) pymat;
		if (listval->GetCount() == 3)
		{
			for (row=0;row<3;row++) // each row has a 3-vector [x,y,z]
			{
				CListValue* vecval = (CListValue*)listval->GetValue(row);
				for (col=0;col<3 && col < vecval->GetCount();col++)
				{
					mat[row][col] = vecval->GetValue(col)->GetNumber();
					
				}
			}
		}
		else
		{
			error = true;
			if (error)
				PyErr_SetString(PyExc_TypeError, "Expected list of three items for 3x3 matrix argument.");

		}
	} else if (PySequence_Check(pymat))
	{
		unsigned int rows = PySequence_Size(pymat);
		for (unsigned int y = 0; y < rows && y < 3; y++)
		{
			PyObject *pyrow = PySequence_GetItem(pymat, y); /* new ref */
			if (PySequence_Check(pyrow))
			{
				unsigned int cols = PySequence_Size(pyrow);
				for( unsigned int x = 0; x < cols && x < 3; x++)
				{
					PyObject *pyitem = PySequence_GetItem(pyrow, x); /* new ref */
					mat[y][x] = PyFloat_AsDouble(pyitem);
					Py_DECREF(pyitem);
				}
			}
			Py_DECREF(pyrow);
		}
	}
	 
	return mat;
}

PyObject* PyObjectFromMT_Matrix4x4(const MT_Matrix4x4 &mat)
{
	return Py_BuildValue("[[ffff][ffff][ffff][ffff]]",
		mat[0][0], mat[0][1], mat[0][2], mat[0][3], 
		mat[1][0], mat[1][1], mat[1][2], mat[1][3], 
		mat[2][0], mat[2][1], mat[2][2], mat[2][3], 
		mat[3][0], mat[3][1], mat[3][2], mat[3][3]);
}

PyObject* PyObjectFromMT_Matrix3x3(const MT_Matrix3x3 &mat)
{
	return Py_BuildValue("[[fff][fff][fff]]",
		mat[0][0], mat[0][1], mat[0][2], 
		mat[1][0], mat[1][1], mat[1][2], 
		mat[2][0], mat[2][1], mat[2][2]);
}

PyObject* PyObjectFromMT_Vector3(const MT_Vector3 &vec)
{
	return Py_BuildValue("[fff]", 
		vec[0], vec[1], vec[2]);
}

PyObject* PyObjectFromMT_Point3(const MT_Point3 &pos)
{
	return Py_BuildValue("[fff]",
		pos[0], pos[1], pos[2]);
}
