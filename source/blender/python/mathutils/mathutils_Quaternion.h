/*
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
 * Contributor(s): Joseph Gilbert
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __MATHUTILS_QUATERNION_H__
#define __MATHUTILS_QUATERNION_H__

/** \file blender/python/mathutils/mathutils_Quaternion.h
 *  \ingroup pymathutils
 */

extern PyTypeObject quaternion_Type;
#define QuaternionObject_Check(_v) PyObject_TypeCheck((_v), &quaternion_Type)

typedef struct {
	BASE_MATH_MEMBERS(quat);
} QuaternionObject;

/* struct data contains a pointer to the actual data that the
 * object uses. It can use either PyMem allocated data (which will
 * be stored in py_data) or be a wrapper for data allocated through
 * blender (stored in blend_data). This is an either/or struct not both */

/* prototypes */
PyObject *Quaternion_CreatePyObject(float quat[4], int type, PyTypeObject *base_type);
PyObject *Quaternion_CreatePyObject_cb(PyObject *cb_user,
                                       unsigned char cb_type, unsigned char cb_subtype);

#endif /* __MATHUTILS_QUATERNION_H__ */
