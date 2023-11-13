/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup pythonintern
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

PyObject *BPY_rna_props(void);
/**
 * Run this on exit, clearing all Python callback users and disable the RNA callback,
 * as it would be called after Python has already finished.
 */
void BPY_rna_props_clear_all(void);

PyObject *BPy_PointerProperty(PyObject *self, PyObject *args, PyObject *kw);
PyObject *BPy_CollectionProperty(PyObject *self, PyObject *args, PyObject *kw);
StructRNA *pointer_type_from_py(PyObject *value, const char *error_prefix);

typedef struct {
  PyObject_HEAD
  /**
   * Internally a #PyCFunctionObject type.
   * \note This isn't GC tracked, it's a function from `bpy.props` so it's not going away.
   */
  void *fn;
  PyObject *kw;
} BPy_PropDeferred;

extern PyTypeObject bpy_prop_deferred_Type;
#define BPy_PropDeferred_CheckTypeExact(v) (Py_TYPE(v) == &bpy_prop_deferred_Type)

#define PYRNA_STACK_ARRAY RNA_STACK_ARRAY

#ifdef __cplusplus
}
#endif
