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
 * ***** END GPL LICENSE BLOCK *****
 */


/** \file blender/python/mathutils/mathutils_bvhtree.h
 *  \ingroup mathutils
 */

#ifndef __MATHUTILS_BVHTREE_H__
#define __MATHUTILS_BVHTREE_H__

PyMODINIT_FUNC PyInit_mathutils_bvhtree(void);

extern PyTypeObject PyBVHTree_Type;

#define PyBVHTree_Check(_v)  PyObject_TypeCheck((_v), &PyBVHTree_Type)
#define PyBVHTree_CheckExact(v)  (Py_TYPE(v) == &PyBVHTree_Type)

#endif /* __MATHUTILS_BVHTREE_H__ */
