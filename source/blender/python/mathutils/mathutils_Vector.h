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
 * Contributor(s): Willian P. Germano & Joseph Gilbert
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/python/mathutils/mathutils_Vector.h
 *  \ingroup pymathutils
 */


#ifndef __MATHUTILS_VECTOR_H__
#define __MATHUTILS_VECTOR_H__

extern PyTypeObject vector_Type;
#define VectorObject_Check(_v) PyObject_TypeCheck((_v), &vector_Type)

typedef struct {
	BASE_MATH_MEMBERS(vec);

	int size;  /* vec size 2 or more */
} VectorObject;

/*prototypes*/
PyObject *Vector_CreatePyObject(
        const float *vec, const int size,
        PyTypeObject *base_type
        ) ATTR_WARN_UNUSED_RESULT;
PyObject *Vector_CreatePyObject_wrap(
        float *vec, const int size,
        PyTypeObject *base_type
        ) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(1);
PyObject *Vector_CreatePyObject_cb(
        PyObject *user, int size,
        unsigned char cb_type, unsigned char subtype
        ) ATTR_WARN_UNUSED_RESULT;
PyObject *Vector_CreatePyObject_alloc(
        float *vec, const int size,
        PyTypeObject *base_type
        ) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(1);

#endif  /* __MATHUTILS_VECTOR_H__ */
