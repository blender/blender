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
 * Initialize Python thingies.
 */

#ifndef __KX_PYMATH_H__
#define __KX_PYMATH_H__

#include "MT_Point2.h"
#include "MT_Point3.h"
#include "MT_Vector2.h"
#include "MT_Vector3.h"
#include "MT_Vector4.h"
#include "MT_Matrix3x3.h"
#include "MT_Matrix4x4.h"

#include "KX_Python.h"
#include "PyObjectPlus.h"

#ifndef DISABLE_PYTHON
#ifdef USE_MATHUTILS
extern "C" {
#include "../../blender/python/generic/Mathutils.h" /* so we can have mathutils callbacks */
}
#endif

inline unsigned int Size(const MT_Matrix4x4&)          { return 4; }
inline unsigned int Size(const MT_Matrix3x3&)          { return 3; }
inline unsigned int Size(const MT_Tuple2&)                { return 2; }
inline unsigned int Size(const MT_Tuple3&)                { return 3; }
inline unsigned int Size(const MT_Tuple4&)                { return 4; }

/**
 *  Converts the given python matrix (column-major) to an MT class (row-major).
 */
template<class T>
bool PyMatTo(PyObject* pymat, T& mat)
{
	bool noerror = true;
	mat.setIdentity();
	if (PySequence_Check(pymat))
	{
		unsigned int cols = PySequence_Size(pymat);
		if (cols != Size(mat))
			return false;
			
		for (unsigned int x = 0; noerror && x < cols; x++)
		{
			PyObject *pycol = PySequence_GetItem(pymat, x); /* new ref */
			if (!PyErr_Occurred() && PySequence_Check(pycol))
			{
				unsigned int rows = PySequence_Size(pycol);
				if (rows != Size(mat))
					noerror = false;
				else
				{
					for( unsigned int y = 0; y < rows; y++)
					{
						PyObject *item = PySequence_GetItem(pycol, y); /* new ref */
						mat[y][x] = PyFloat_AsDouble(item);
						Py_DECREF(item);
					}
				}
			} else 
				noerror = false;
			Py_DECREF(pycol);
		}
	} else 
		noerror = false;
	
	if (noerror==false)
		PyErr_SetString(PyExc_TypeError, "could not be converted to a matrix (sequence of sequences)");
	
	return noerror;
}

/**
 *  Converts a python sequence to a MT class.
 */
template<class T>
bool PyVecTo(PyObject* pyval, T& vec)
{
#ifdef USE_MATHUTILS
	/* no need for BaseMath_ReadCallback() here, reading the sequences will do this */
	
	if(VectorObject_Check(pyval)) {
		VectorObject *pyvec= (VectorObject *)pyval;
		BaseMath_ReadCallback(pyvec);
		if (pyvec->size != Size(vec)) {
			PyErr_Format(PyExc_AttributeError, "error setting vector, %d args, should be %d", pyvec->size, Size(vec));
			return false;
		}
		vec.setValue((float *) pyvec->vec);
		return true;
	}
	else if(QuaternionObject_Check(pyval)) {
		QuaternionObject *pyquat= (QuaternionObject *)pyval;
		BaseMath_ReadCallback(pyquat);
		if (4 != Size(vec)) {
			PyErr_Format(PyExc_AttributeError, "error setting vector, %d args, should be %d", 4, Size(vec));
			return false;
		}
		/* xyzw -> wxyz reordering is done by PyQuatTo */
		vec.setValue((float *) pyquat->quat);
		return true;
	}
	else if(EulerObject_Check(pyval)) {
		EulerObject *pyeul= (EulerObject *)pyval;
		BaseMath_ReadCallback(pyeul);
		if (3 != Size(vec)) {
			PyErr_Format(PyExc_AttributeError, "error setting vector, %d args, should be %d", 3, Size(vec));
			return false;
		}
		vec.setValue((float *) pyeul->eul);
		return true;
	} else
#endif
	if(PyTuple_Check(pyval))
	{
		unsigned int numitems = PyTuple_GET_SIZE(pyval);
		if (numitems != Size(vec)) {
			PyErr_Format(PyExc_AttributeError, "error setting vector, %d args, should be %d", numitems, Size(vec));
			return false;
		}
		
		for (unsigned int x = 0; x < numitems; x++)
			vec[x] = PyFloat_AsDouble(PyTuple_GET_ITEM(pyval, x)); /* borrow ref */
		
		if (PyErr_Occurred()) {
			PyErr_SetString(PyExc_AttributeError, "one or more of the items in the sequence was not a float");
			return false;
		}
		
		return true;
	}
	else if (PyObject_TypeCheck(pyval, (PyTypeObject *)&PyObjectPlus::Type))
	{	/* note, include this check because PySequence_Check does too much introspection
		 * on the PyObject (like getting its __class__, on a BGE type this means searching up
		 * the parent list each time only to discover its not a sequence.
		 * GameObjects are often used as an alternative to vectors so this is a common case
		 * better to do a quick check for it, likely the error below will be ignored.
		 * 
		 * This is not 'correct' since we have proxy type CListValues's which could
		 * contain floats/ints but there no cases of CValueLists being this way
		 */
		PyErr_Format(PyExc_AttributeError, "expected a sequence type");
		return false;
	}
	else if (PySequence_Check(pyval))
	{
		unsigned int numitems = PySequence_Size(pyval);
		if (numitems != Size(vec)) {
			PyErr_Format(PyExc_AttributeError, "error setting vector, %d args, should be %d", numitems, Size(vec));
			return false;
		}
		
		for (unsigned int x = 0; x < numitems; x++)
		{
			PyObject *item = PySequence_GetItem(pyval, x); /* new ref */
			vec[x] = PyFloat_AsDouble(item);
			Py_DECREF(item);
		}
		
		if (PyErr_Occurred()) {
			PyErr_SetString(PyExc_AttributeError, "one or more of the items in the sequence was not a float");
			return false;
		}
		
		return true;
	} else
	{
		PyErr_Format(PyExc_AttributeError, "not a sequence type, expected a sequence of numbers size %d", Size(vec));
	}
	
	return false;
}


bool PyQuatTo(PyObject* pyval, MT_Quaternion &qrot);

bool PyOrientationTo(PyObject* pyval, MT_Matrix3x3 &mat, const char *error_prefix);

/**
 * Converts an MT_Matrix4x4 to a python object.
 */
PyObject* PyObjectFrom(const MT_Matrix4x4 &mat);

/**
 * Converts an MT_Matrix3x3 to a python object.
 */
PyObject* PyObjectFrom(const MT_Matrix3x3 &mat);

/**
 * Converts an MT_Tuple2 to a python object.
 */
PyObject* PyObjectFrom(const MT_Tuple2 &vec);

/**
 * Converts an MT_Tuple3 to a python object
 */
PyObject* PyObjectFrom(const MT_Tuple3 &vec);

#ifdef USE_MATHUTILS
/**
 * Converts an MT_Quaternion to a python object.
 */
PyObject* PyObjectFrom(const MT_Quaternion &qrot);
#endif

/**
 * Converts an MT_Tuple4 to a python object.
 */
PyObject* PyObjectFrom(const MT_Tuple4 &pos);

#endif

#endif // DISABLE_PYTHON
