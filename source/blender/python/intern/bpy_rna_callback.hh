/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup pythonintern
 */

#include <Python.h>

struct BPy_StructRNA;

#if 0
[[nodiscard]] PyObject *pyrna_callback_add(BPy_StructRNA *self, PyObject *args);
[[nodiscard]] PyObject *pyrna_callback_remove(BPy_StructRNA *self, PyObject *args);
#endif

[[nodiscard]] PyObject *pyrna_callback_classmethod_add(PyObject *self, PyObject *args);
[[nodiscard]] PyObject *pyrna_callback_classmethod_remove(PyObject *self, PyObject *args);
