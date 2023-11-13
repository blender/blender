/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup pythonintern
 */

struct BPy_StructRNA;

#ifdef __cplusplus
extern "C" {
#endif

#if 0
PyObject *pyrna_callback_add(BPy_StructRNA *self, PyObject *args);
PyObject *pyrna_callback_remove(BPy_StructRNA *self, PyObject *args);
#endif

PyObject *pyrna_callback_classmethod_add(PyObject *self, PyObject *args);
PyObject *pyrna_callback_classmethod_remove(PyObject *self, PyObject *args);

#ifdef __cplusplus
}
#endif
