/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup pymathutils
 */

#pragma once

extern PyTypeObject vector_Type;

#define VectorObject_Check(v) PyObject_TypeCheck((v), &vector_Type)
#define VectorObject_CheckExact(v) (Py_TYPE(v) == &vector_Type)

typedef struct {
  BASE_MATH_MEMBERS(vec);

  /** Number of items in this vector (2 or more). */
  int vec_num;
} VectorObject;

/* Prototypes. */

PyObject *Vector_CreatePyObject(const float *vec,
                                int vec_num,
                                PyTypeObject *base_type) ATTR_WARN_UNUSED_RESULT;
/**
 * Create a vector that wraps existing memory.
 *
 * \param vec: Use this vector in-place.
 */
PyObject *Vector_CreatePyObject_wrap(float *vec,
                                     int vec_num,
                                     PyTypeObject *base_type) ATTR_WARN_UNUSED_RESULT
    ATTR_NONNULL(1);
/**
 * Create a vector where the value is defined by registered callbacks,
 * see: #Mathutils_RegisterCallback
 */
PyObject *Vector_CreatePyObject_cb(PyObject *user,
                                   int vec_num,
                                   unsigned char cb_type,
                                   unsigned char subtype) ATTR_WARN_UNUSED_RESULT;
/**
 * \param vec: Initialized vector value to use in-place, allocated with #PyMem_Malloc
 */
PyObject *Vector_CreatePyObject_alloc(float *vec,
                                      int vec_num,
                                      PyTypeObject *base_type) ATTR_WARN_UNUSED_RESULT
    ATTR_NONNULL(1);
