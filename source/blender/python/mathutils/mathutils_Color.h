/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup pymathutils
 */

#pragma once

extern PyTypeObject color_Type;
#define ColorObject_Check(v) PyObject_TypeCheck((v), &color_Type)
#define ColorObject_CheckExact(v) (Py_TYPE(v) == &color_Type)

typedef struct {
  BASE_MATH_MEMBERS(col);
} ColorObject;

/* struct data contains a pointer to the actual data that the
 * object uses. It can use either PyMem allocated data (which will
 * be stored in py_data) or be a wrapper for data allocated through
 * Blender (stored in blend_data). This is an either/or struct not both. */

/* Prototypes. */

PyObject *Color_CreatePyObject(const float col[3],
                               PyTypeObject *base_type) ATTR_WARN_UNUSED_RESULT;
PyObject *Color_CreatePyObject_wrap(float col[3], PyTypeObject *base_type) ATTR_WARN_UNUSED_RESULT
    ATTR_NONNULL(1);
PyObject *Color_CreatePyObject_cb(PyObject *cb_user,
                                  unsigned char cb_type,
                                  unsigned char cb_subtype) ATTR_WARN_UNUSED_RESULT;
