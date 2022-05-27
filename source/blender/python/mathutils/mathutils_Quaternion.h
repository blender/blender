/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup pymathutils
 */

extern PyTypeObject quaternion_Type;

#define QuaternionObject_Check(v) PyObject_TypeCheck((v), &quaternion_Type)
#define QuaternionObject_CheckExact(v) (Py_TYPE(v) == &quaternion_Type)

typedef struct {
  BASE_MATH_MEMBERS(quat);
} QuaternionObject;

/* struct data contains a pointer to the actual data that the
 * object uses. It can use either PyMem allocated data (which will
 * be stored in py_data) or be a wrapper for data allocated through
 * blender (stored in blend_data). This is an either/or struct not both */

/* Prototypes. */

PyObject *Quaternion_CreatePyObject(const float quat[4],
                                    PyTypeObject *base_type) ATTR_WARN_UNUSED_RESULT;
PyObject *Quaternion_CreatePyObject_wrap(float quat[4],
                                         PyTypeObject *base_type) ATTR_WARN_UNUSED_RESULT
    ATTR_NONNULL(1);
PyObject *Quaternion_CreatePyObject_cb(PyObject *cb_user,
                                       unsigned char cb_type,
                                       unsigned char cb_subtype) ATTR_WARN_UNUSED_RESULT;
