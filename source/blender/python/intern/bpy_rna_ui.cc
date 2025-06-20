/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup pythonintern
 *
 * This adds helpers to #uiLayout which can't be added easily to RNA itself.
 */

#include <Python.h>

#include "MEM_guardedalloc.h"

#include "../generic/py_capi_utils.hh"

#include "UI_interface_layout.hh"

#include "bpy_rna.hh"
#include "bpy_rna_ui.hh" /* Declare #BPY_rna_uilayout_introspect_method_def. */

PyDoc_STRVAR(
    /* Wrap. */
    bpy_rna_uilayout_introspect_doc,
    ".. method:: introspect()\n"
    "\n"
    "   Return a dictionary containing a textual representation of the UI layout.\n");
static PyObject *bpy_rna_uilayout_introspect(PyObject *self)
{
  BPy_StructRNA *pyrna = (BPy_StructRNA *)self;
  uiLayout *layout = static_cast<uiLayout *>(pyrna->ptr->data);

  const char *expr = UI_layout_introspect(layout);
  PyObject *main_mod = PyC_MainModule_Backup();
  PyObject *py_dict = PyC_DefaultNameSpace("<introspect>");
  PyObject *result = PyRun_String(expr, Py_eval_input, py_dict, py_dict);
  MEM_freeN(expr);
  Py_DECREF(py_dict);
  PyC_MainModule_Restore(main_mod);
  return result;
}

#ifdef __GNUC__
#  ifdef __clang__
#    pragma clang diagnostic push
#    pragma clang diagnostic ignored "-Wcast-function-type"
#  else
#    pragma GCC diagnostic push
#    pragma GCC diagnostic ignored "-Wcast-function-type"
#  endif
#endif

PyMethodDef BPY_rna_uilayout_introspect_method_def = {
    "introspect",
    (PyCFunction)bpy_rna_uilayout_introspect,
    METH_NOARGS,
    bpy_rna_uilayout_introspect_doc,
};

#ifdef __GNUC__
#  ifdef __clang__
#    pragma clang diagnostic pop
#  else
#    pragma GCC diagnostic pop
#  endif
#endif
