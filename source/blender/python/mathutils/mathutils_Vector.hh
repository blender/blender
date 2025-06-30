/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup pymathutils
 */

#pragma once

#include <Python.h>

#include "mathutils.hh"

extern PyTypeObject vector_Type;

#define VectorObject_Check(v) PyObject_TypeCheck((v), &vector_Type)
#define VectorObject_CheckExact(v) (Py_TYPE(v) == &vector_Type)

struct VectorObject {
  BASE_MATH_MEMBERS(vec);

  /** Number of items in this vector (2 or more). */
  int vec_num;
};

/* Prototypes. */

[[nodiscard]] PyObject *Vector_CreatePyObject(const float *vec,
                                              int vec_num,
                                              PyTypeObject *base_type);
/**
 * Create a vector that wraps existing memory.
 *
 * \param vec: Use this vector in-place.
 */
[[nodiscard]] PyObject *Vector_CreatePyObject_wrap(float *vec,
                                                   int vec_num,
                                                   PyTypeObject *base_type) ATTR_NONNULL(1);
/**
 * Create a vector where the value is defined by registered callbacks,
 * see: #Mathutils_RegisterCallback
 */
[[nodiscard]] PyObject *Vector_CreatePyObject_cb(PyObject *cb_user,
                                                 int vec_num,
                                                 unsigned char cb_type,
                                                 unsigned char cb_subtype);
/**
 * \param vec: Initialized vector value to use in-place, allocated with #PyMem_Malloc
 */
[[nodiscard]] PyObject *Vector_CreatePyObject_alloc(float *vec,
                                                    int vec_num,
                                                    PyTypeObject *base_type) ATTR_NONNULL(1);
