/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup pythonintern
 *
 * This file implements the #BPyOpFunction type,
 * a Python C-API implementation of a callable Blender operator.
 */

#pragma once

#include <Python.h>

#include "DNA_windowmanager_types.h"

/**
 * A callable operator.
 *
 * Exposed by `bpy.ops.{module}.{operator}()` to allow Blender operators to be called from Python.
 */
typedef struct {
  PyObject_HEAD
  /** Operator ID name (e.g., `OBJECT_OT_select_all`). */
  char idname[OP_MAX_TYPENAME];
} BPyOpFunction;

extern PyTypeObject BPyOpFunctionType;

#define BPyOpFunction_Check(v) (PyObject_TypeCheck(v, &BPyOpFunctionType))
#define BPyOpFunction_CheckExact(v) (Py_TYPE(v) == &BPyOpFunctionType)

/* Forward declarations for external functions from `bpy_operator.cc`. */

PyObject *pyop_poll(PyObject *self, PyObject *args);
PyObject *pyop_call(PyObject *self, PyObject *args);
PyObject *pyop_as_string(PyObject *self, PyObject *args);
PyObject *pyop_getrna_type(PyObject *self, PyObject *value);
PyObject *pyop_get_bl_options(PyObject *self, PyObject *value);

/**
 * Create a new BPyOpFunction object for the given operator module and function.
 *
 * \param self: Unused (required by Python C API).
 * \param args: Python tuple containing module and function name strings.
 * \return A new #BPyOpFunction object or NULL on error.
 */
PyObject *pyop_create_function(PyObject *self, PyObject *args);

/**
 * Initialize the BPyOpFunction type.
 * This must be called before using any BPyOpFunction functions.
 *
 * \return 0 on success, -1 on failure
 */
int BPyOpFunction_InitTypes();
