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

#include "MT_Vector3.h"
#include "MT_Point3.h"
#include "MT_Vector4.h"
#include "MT_Matrix4x4.h"
#include "MT_Matrix3x3.h"

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

#endif
