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
#include "KX_PyMath.h"

bool PyOrientationTo(PyObject* pyval, MT_Matrix3x3 &rot, const char *error_prefix)
{
	int size= PySequence_Size(pyval);
	
	if (size == 4)
	{
		MT_Quaternion qrot;
		if (PyQuatTo(pyval, qrot))
		{
			rot.setRotation(qrot);
			return true;
		}
	}
	else if (size == 3) {
		/* 3x3 matrix or euler */
		MT_Vector3 erot;
		if (PyVecTo(pyval, erot))
		{
			rot.setEuler(erot);
			return true;
		}
		PyErr_Clear();
		
		if (PyMatTo(pyval, rot))
		{
			return true;
		}
	}
	
	PyErr_Format(PyExc_TypeError, "%s, could not set the orientation from a 3x3 matrix, quaternion or euler sequence", error_prefix);
	return false;
}

bool PyQuatTo(PyObject* pyval, MT_Quaternion &qrot)
{
	if(!PyVecTo(pyval, qrot))
		return false;

	/* annoying!, Blender/Mathutils have the W axis first! */
	MT_Scalar w= qrot[0]; /* from python, this is actually the W */
	qrot[0]= qrot[1];
	qrot[1]= qrot[2];
	qrot[2]= qrot[3];
	qrot[3]= w;

	return true;
}

PyObject* PyObjectFrom(const MT_Matrix4x4 &mat)
{
#ifdef USE_MATHUTILS
	float fmat[16];
	mat.getValue(fmat);
	return newMatrixObject(fmat, 4, 4, Py_NEW, NULL);
#else
	PyObject *list = PyList_New(4);
	PyObject *sublist;
	int i;
	
	for(i=0; i < 4; i++) {
		sublist = PyList_New(4);
		PyList_SET_ITEM(sublist, 0, PyFloat_FromDouble(mat[i][0]));
		PyList_SET_ITEM(sublist, 1, PyFloat_FromDouble(mat[i][1]));
		PyList_SET_ITEM(sublist, 2, PyFloat_FromDouble(mat[i][2]));
		PyList_SET_ITEM(sublist, 3, PyFloat_FromDouble(mat[i][3]));
		PyList_SET_ITEM(list, i, sublist);
	}
	
	return list;
#endif
}

PyObject* PyObjectFrom(const MT_Matrix3x3 &mat)
{
#ifdef USE_MATHUTILS
	float fmat[9];
	mat.getValue3x3(fmat);
	return newMatrixObject(fmat, 3, 3, Py_NEW, NULL);
#else
	PyObject *list = PyList_New(3);
	PyObject *sublist;
	int i;
	
	for(i=0; i < 3; i++) {
		sublist = PyList_New(3);
		PyList_SET_ITEM(sublist, 0, PyFloat_FromDouble(mat[i][0]));
		PyList_SET_ITEM(sublist, 1, PyFloat_FromDouble(mat[i][1]));
		PyList_SET_ITEM(sublist, 2, PyFloat_FromDouble(mat[i][2]));
		PyList_SET_ITEM(list, i, sublist);
	}
	
	return list;
#endif
}

#ifdef USE_MATHUTILS
PyObject* PyObjectFrom(const MT_Quaternion &qrot)
{
	/* NOTE, were re-ordering here for Mathutils compat */
	float fvec[4]= {qrot[3], qrot[0], qrot[1], qrot[2]};
	return newQuaternionObject(fvec, Py_NEW, NULL);
}
#endif

PyObject* PyObjectFrom(const MT_Tuple4 &vec)
{
#ifdef USE_MATHUTILS
	float fvec[4]= {vec[0], vec[1], vec[2], vec[3]};
	return newVectorObject(fvec, 4, Py_NEW, NULL);
#else
	PyObject *list = PyList_New(4);
	PyList_SET_ITEM(list, 0, PyFloat_FromDouble(vec[0]));
	PyList_SET_ITEM(list, 1, PyFloat_FromDouble(vec[1]));
	PyList_SET_ITEM(list, 2, PyFloat_FromDouble(vec[2]));
	PyList_SET_ITEM(list, 3, PyFloat_FromDouble(vec[3]));
	return list;
#endif
}

PyObject* PyObjectFrom(const MT_Tuple3 &vec)
{
#ifdef USE_MATHUTILS
	float fvec[3]= {vec[0], vec[1], vec[2]};
	return newVectorObject(fvec, 3, Py_NEW, NULL);
#else
	PyObject *list = PyList_New(3);
	PyList_SET_ITEM(list, 0, PyFloat_FromDouble(vec[0]));
	PyList_SET_ITEM(list, 1, PyFloat_FromDouble(vec[1]));
	PyList_SET_ITEM(list, 2, PyFloat_FromDouble(vec[2]));
	return list;
#endif	
}

PyObject* PyObjectFrom(const MT_Tuple2 &vec)
{
#ifdef USE_MATHUTILS
	float fvec[2]= {vec[0], vec[1]};
	return newVectorObject(fvec, 2, Py_NEW, NULL);
#else
	PyObject *list = PyList_New(2);
	PyList_SET_ITEM(list, 0, PyFloat_FromDouble(vec[0]));
	PyList_SET_ITEM(list, 1, PyFloat_FromDouble(vec[1]));
	return list;
#endif
}
