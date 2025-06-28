/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup pythonintern
 */

#pragma once

#include <Python.h>

extern PyTypeObject pyop_base_Type;

#define BPy_OperatorBase_Check(v) (PyObject_TypeCheck(v, &pyop_base_Type))

struct BPy_OperatorBase {
  PyObject_HEAD /* Required Python macro. */
};

[[nodiscard]] PyObject *BPY_operator_module();
