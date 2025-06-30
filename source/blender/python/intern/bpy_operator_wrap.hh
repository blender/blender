/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup pythonintern
 */

#pragma once

#include <Python.h>

struct wmOperatorType;

/**
 * These are used for operator methods, used by `bpy_operator.cc`.
 *
 * Accessed via sub-classes of `bpy.types.Macro` using the `define` method.
 */
[[nodiscard]] PyObject *PYOP_wrap_macro_define(PyObject *self, PyObject *args);

/* Exposed to RNA/WM API. */

/**
 * Generic function used by all Python defined operators
 * it's passed as an argument to #WM_operatortype_append_ptr in for operator registration.
 */
void BPY_RNA_operator_wrapper(wmOperatorType *ot, void *userdata);
/**
 * Generic function used by all Python defined macro-operators
 * it's passed as an argument to #WM_operatortype_append_ptr in for operator registration.
 */
void BPY_RNA_operator_macro_wrapper(wmOperatorType *ot, void *userdata);
