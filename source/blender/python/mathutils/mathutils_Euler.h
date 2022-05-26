/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup pymathutils
 */

extern PyTypeObject euler_Type;
#define EulerObject_Check(v) PyObject_TypeCheck((v), &euler_Type)
#define EulerObject_CheckExact(v) (Py_TYPE(v) == &euler_Type)

typedef struct {
  BASE_MATH_MEMBERS(eul);
  unsigned char order; /* rotation order */

} EulerObject;

/* struct data contains a pointer to the actual data that the
 * object uses. It can use either PyMem allocated data (which will
 * be stored in py_data) or be a wrapper for data allocated through
 * blender (stored in blend_data). This is an either/or struct not both */

/* prototypes */

PyObject *Euler_CreatePyObject(const float eul[3],
                               short order,
                               PyTypeObject *base_type) ATTR_WARN_UNUSED_RESULT;
PyObject *Euler_CreatePyObject_wrap(float eul[3],
                                    short order,
                                    PyTypeObject *base_type) ATTR_WARN_UNUSED_RESULT
    ATTR_NONNULL(1);
PyObject *Euler_CreatePyObject_cb(PyObject *cb_user,
                                  short order,
                                  unsigned char cb_type,
                                  unsigned char cb_subtype) ATTR_WARN_UNUSED_RESULT;

short euler_order_from_string(const char *str, const char *error_prefix);
