/*
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
 */

/** \file
 * \ingroup pymathutils
 */

#pragma once

extern PyTypeObject vector_Type;

#define VectorObject_Check(v) PyObject_TypeCheck((v), &vector_Type)
#define VectorObject_CheckExact(v) (Py_TYPE(v) == &vector_Type)

typedef struct {
  BASE_MATH_MEMBERS(vec);

  int size; /* vec size 2 or more */
} VectorObject;

/*prototypes*/
PyObject *Vector_CreatePyObject(const float *vec,
                                int size,
                                PyTypeObject *base_type) ATTR_WARN_UNUSED_RESULT;
/**
 * Create a vector that wraps existing memory.
 *
 * \param vec: Use this vector in-place.
 */
PyObject *Vector_CreatePyObject_wrap(float *vec,
                                     int size,
                                     PyTypeObject *base_type) ATTR_WARN_UNUSED_RESULT
    ATTR_NONNULL(1);
/**
 * Create a vector where the value is defined by registered callbacks,
 * see: #Mathutils_RegisterCallback
 */
PyObject *Vector_CreatePyObject_cb(PyObject *user,
                                   int size,
                                   unsigned char cb_type,
                                   unsigned char subtype) ATTR_WARN_UNUSED_RESULT;
/**
 * \param vec: Initialized vector value to use in-place, allocated with #PyMem_Malloc
 */
PyObject *Vector_CreatePyObject_alloc(float *vec,
                                      int size,
                                      PyTypeObject *base_type) ATTR_WARN_UNUSED_RESULT
    ATTR_NONNULL(1);
