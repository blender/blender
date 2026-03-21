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

namespace blender {

/**
 * Create a new BPyOpFunction object for the given operator module and function.
 *
 * \param self: Unused (required by Python C API).
 * \param args: Python tuple containing module and function name strings.
 * \return A new #BPyOpFunction object or NULL on error.
 */
PyObject *pyop_create_function(PyObject *self, PyObject *args);

/**
 * Get RNA type for an operator by name (Python module method wrapper).
 */
PyObject *pyop_getrna_type(PyObject *self, PyObject *value);

/**
 * Initialize the BPyOpFunction type.
 * This must be called before using any BPyOpFunction functions.
 *
 * \return 0 on success, -1 on failure
 */
int BPyOpFunction_InitTypes();

}  // namespace blender
