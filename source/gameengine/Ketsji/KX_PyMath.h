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

#ifndef __KX_PYMATH_H__
#define __KX_PYMATH_H__

#include "MT_Point2.h"
#include "MT_Point3.h"
#include "MT_Vector3.h"
#include "MT_Vector4.h"
#include "MT_Matrix3x3.h"
#include "MT_Matrix4x4.h"

#include "KX_Python.h"

/**
 * Converts a python list to an MT_Vector3
 */
MT_Vector3 MT_Vector3FromPyList(PyObject* pylist);

/**
 * Converts a python list to an MT_Point3
 */
MT_Point3 MT_Point3FromPyList(PyObject* pylist);

/**
 * Converts a python list to an MT_Vector4
 */
MT_Vector4 MT_Vector4FromPyList(PyObject* pylist);

/**
 * Converts a python list to an MT_Vector2
 */
MT_Point2 MT_Point2FromPyList(PyObject* pylist);

/**
 * Converts a python list to an MT_Quaternion
 */
MT_Quaternion MT_QuaternionFromPyList(PyObject* pylist);

/**
 * Converts a python list of lists to an MT_Matrix4x4.
 * Any object that supports the sequence protocol will work.
 * Only the first four rows and first four columns in each row will be converted.
 * @example The python object [[1.0, 0.0, 0.0, 0.0], [0.0, 1.0, 0.0, 0.0], [0.0, 0.0, 1.0, 0.0], [0.0, 0.0, 0.0, 1.0]]
 */
MT_Matrix4x4 MT_Matrix4x4FromPyObject(PyObject *pymat);
/**
 * Converts a python list of lists to an MT_Matrix3x3
 * Any object that supports the sequence protocol will work.
 * Only the first three rows and first three columns in each row will be converted.
 * @example The python object [[1.0, 0.0, 0.0], [0.0, 1.0, 0.0], [0.0, 0.0, 1.0]]
 */
MT_Matrix3x3 MT_Matrix3x3FromPyObject(PyObject *pymat);

/**
 * Converts an MT_Matrix4x4 to a python object.
 */
PyObject* PyObjectFromMT_Matrix4x4(const MT_Matrix4x4 &mat);

/**
 * Converts an MT_Matrix3x3 to a python object.
 */
PyObject* PyObjectFromMT_Matrix3x3(const MT_Matrix3x3 &mat);

/**
 * Converts an MT_Vector3 to a python object.
 */
PyObject* PyObjectFromMT_Vector3(const MT_Vector3 &vec);

/**
 * Converts an MT_Vector4 to a python object
 */
PyObject* PyObjectFromMT_Vector4(const MT_Vector4 &vec);

/**
 * Converts an MT_Vector3 to a python object.
 */
PyObject* PyObjectFromMT_Point3(const MT_Point3 &pos);

/**
 * Converts an MT_Point2 to a python object.
 */
PyObject* PyObjectFromMT_Point2(const MT_Point2 &vec);
 
/**
 * True if the given PyObject can be converted to an MT_Matrix
 * @param rank = 3 (for MT_Matrix3x3) or 4 (for MT_Matrix4x4)
 */
bool PyObject_IsMT_Matrix(PyObject *pymat, unsigned int rank);

#endif
